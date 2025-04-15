#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <random>
#include <thread>
#include <unordered_map>
#include <map>
#include "../include/SATInstance.h"
#include "../include/PortfolioManager.h"

// Helper function to generate random 3-SAT instances (taken from main_incremental.cpp)
CNF generateRandom3SAT(int num_vars, double clause_ratio, int seed = 42)
{
    CNF formula;
    int num_clauses = static_cast<int>(num_vars * clause_ratio);

    // Initialize random seed
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> var_dist(1, num_vars);
    std::uniform_int_distribution<> sign_dist(0, 1);

    // Generate random clauses
    for (int i = 0; i < num_clauses; i++)
    {
        Clause clause;

        // Generate 3 distinct literals for the clause
        while (clause.size() < 3)
        {
            int var = var_dist(gen);
            int lit = sign_dist(gen) == 0 ? var : -var;

            // Ensure no duplicate literals in the clause
            if (std::find(clause.begin(), clause.end(), lit) == clause.end() &&
                std::find(clause.begin(), clause.end(), -lit) == clause.end())
            {
                clause.push_back(lit);
            }
        }

        formula.push_back(clause);
    }

    return formula;
}

// Helper to print a nicely formatted table row
void printTableRow(const std::vector<std::string> &cells, const std::vector<int> &widths)
{
    std::cout << "| ";
    for (size_t i = 0; i < cells.size(); ++i)
    {
        std::cout << std::left << std::setw(widths[i]) << cells[i] << " | ";
    }
    std::cout << std::endl;
}

// Helper to print a separator row
void printTableSeparator(const std::vector<int> &widths)
{
    std::cout << "+";
    for (int width : widths)
    {
        std::cout << std::string(width + 2, '-') << "+";
    }
    std::cout << std::endl;
}

// Test a range of clause ratios with the portfolio solver
void testClauseRatios(int num_vars, const std::vector<double> &ratios, int instances_per_ratio,
                      std::chrono::seconds timeout_per_instance)
{
    // Setup table for console output
    std::vector<int> column_widths = {8, 10, 12, 12, 12, 15}; // Increased width for time column

    // Print header
    std::cout << "\nRandom 3-SAT Portfolio Solver Benchmark\n";
    std::cout << "Variables: " << num_vars << ", Instances per ratio: " << instances_per_ratio << "\n";
    std::cout << "Timeout per instance: " << timeout_per_instance.count() << " seconds\n\n";

    printTableSeparator(column_widths);
    printTableRow({"Ratio", "SAT/UNSAT", "Time (µs)", "Conflicts", "Decisions", "Winning Config"}, column_widths);
    printTableSeparator(column_widths);

    // Test each ratio
    for (double ratio : ratios)
    {
        int sat_count = 0;
        int total_instances = 0;

        // Summary statistics
        double total_time = 0;
        long long total_conflicts = 0;
        long long total_decisions = 0;
        std::map<int, int> winning_configs;

        // Generate and solve multiple instances for this ratio
        for (int instance = 0; instance < instances_per_ratio; ++instance)
        {
            // Use a truly random seed each time
            std::random_device rd;
            int seed = rd();
            CNF formula = generateRandom3SAT(num_vars, ratio, seed);

            std::cout << "Testing ratio " << std::fixed << std::setprecision(2) << ratio
                      << ", instance " << (instance + 1) << "/" << instances_per_ratio << "..." << std::endl;

            try
            {
                // Create portfolio solver with timeout
                PortfolioManager portfolio(formula, std::chrono::duration_cast<std::chrono::milliseconds>(timeout_per_instance));

                // Solve and measure time
                auto start = std::chrono::high_resolution_clock::now();
                bool result = portfolio.solve(formula);
                auto end = std::chrono::high_resolution_clock::now();
                auto solve_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

                // Get statistics from the portfolio solver
                int winning_config = -1;
                int conflicts = 0;
                int decisions = 0;

                // Calculate total statistics and find fastest solver
                long long total_solver_time = 0;
                long long total_solver_conflicts = 0;
                long long total_solver_decisions = 0;
                int fastest_solver = -1;
                long long fastest_time = std::numeric_limits<long long>::max();
                int fastest_conflicts = 0;
                int fastest_decisions = 0;
                int num_solvers = 0;

                for (const auto &stats : portfolio.getSolverStatistics())
                {
                    if (stats.solve_time.count() > 0)
                    {
                        total_solver_time += stats.solve_time.count();
                        total_solver_conflicts += stats.conflicts;
                        total_solver_decisions += stats.decisions;
                        num_solvers++;

                        // Track fastest solver for UNSAT cases
                        if (!result && stats.solve_time.count() < fastest_time)
                        {
                            fastest_time = stats.solve_time.count();
                            fastest_conflicts = stats.conflicts;
                            fastest_decisions = stats.decisions;
                            fastest_solver = &stats - &portfolio.getSolverStatistics()[0];
                        }
                    }
                }

                // Calculate averages for SAT cases
                long long avg_solver_time = num_solvers > 0 ? total_solver_time / num_solvers : 0;
                long long avg_conflicts = num_solvers > 0 ? total_solver_conflicts / num_solvers : 0;
                long long avg_decisions = num_solvers > 0 ? total_solver_decisions / num_solvers : 0;

                // Update statistics
                if (result)
                {
                    sat_count++;
                    winning_config = portfolio.getWinningSolverId();
                    winning_configs[winning_config]++;
                }
                else if (fastest_solver >= 0)
                {
                    // For UNSAT cases, track the fastest solver as the winner
                    winning_config = fastest_solver;
                    winning_configs[winning_config]++;
                }

                total_time += avg_solver_time;
                total_conflicts += avg_conflicts;
                total_decisions += avg_decisions;
                total_instances++;

                // Print individual instance results
                if (result == true)
                {
                    printTableRow({std::to_string(ratio),
                                   "SAT",
                                   std::to_string(static_cast<long long>(avg_solver_time)),
                                   std::to_string(static_cast<long long>(avg_conflicts)),
                                   std::to_string(static_cast<long long>(avg_decisions)),
                                   std::to_string(winning_config)},
                                  column_widths);
                }
                else
                {
                    // For UNSAT, show the fastest solver's stats
                    if (fastest_solver >= 0)
                    {
                        printTableRow({std::to_string(ratio),
                                       "UNSAT",
                                       std::to_string(fastest_time),
                                       std::to_string(fastest_conflicts),
                                       std::to_string(fastest_decisions),
                                       std::to_string(fastest_solver)},
                                      column_widths);
                    }
                    else
                    {
                        printTableRow({std::to_string(ratio),
                                       "UNSAT",
                                       "0",
                                       "0",
                                       "0",
                                       "-1"},
                                      column_widths);
                    }
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Portfolio solver error: " << e.what() << std::endl;
                std::vector<std::string> row = {
                    std::to_string(ratio),
                    "ERROR",
                    "N/A",
                    "N/A",
                    "N/A",
                    "N/A"};
                printTableRow(row, column_widths);
                total_instances++;
            }
            catch (...)
            {
                std::cerr << "Unknown error in portfolio solver" << std::endl;
                std::vector<std::string> row = {
                    std::to_string(ratio),
                    "ERROR",
                    "N/A",
                    "N/A",
                    "N/A",
                    "N/A"};
                printTableRow(row, column_widths);
                total_instances++;
            }

            // Force a small delay to allow system resources to settle
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Skip summary if no instances completed
        if (total_instances == 0)
        {
            continue;
        }

        // Print ratio summary
        std::cout << "+----------+------------+--------------+--------------+--------------+-----------------+" << std::endl;
        std::cout << "Ratio " << std::fixed << std::setprecision(2) << ratio << " summary:" << std::endl;
        double sat_ratio = (static_cast<double>(sat_count) / total_instances) * 100.0;
        std::cout << "  SAT ratio: " << std::fixed << std::setprecision(2) << sat_ratio << "%" << std::endl;
        std::cout << "  Avg time: " << std::fixed << std::setprecision(2) << (total_time / total_instances) << " µs" << std::endl;

        // Print winning configurations
        if (!winning_configs.empty())
        {
            std::cout << "  Winning configurations:" << std::endl;
            for (const auto &pair : winning_configs)
            {
                std::cout << "    Config " << pair.first << ": " << pair.second << " instances" << std::endl;
            }
        }
        else
        {
            std::cout << "  No winning configurations recorded" << std::endl;
        }
        std::cout << "+----------+------------+--------------+--------------+--------------+-----------------+" << std::endl;
    }
}

// Function to run comprehensive benchmarks around the phase transition
void runPhaseTransitionBenchmark()
{
    // Benchmark settings
    int num_vars = 100;
    int instances_per_ratio = 10;
    auto timeout = std::chrono::seconds(300); // 5 minutes per instance

    // Generate ratios centered around the phase transition point (approx. 4.25 for 3-SAT)
    std::vector<double> ratios = {
        3.00, 3.50, 3.80, 4.00, 4.20, 4.25, 4.30, 4.40, 4.50, 5.00};

    std::cout << "Running Phase Transition Benchmark\n";
    std::cout << "=================================\n";
    std::cout << "Testing " << ratios.size() << " clause ratios around the phase transition\n";
    std::cout << "Each ratio tested with " << instances_per_ratio << " instances\n";
    std::cout << "Variables per instance: " << num_vars << "\n";
    std::cout << "Timeout per instance: " << timeout.count() << " seconds\n\n";

    testClauseRatios(num_vars, ratios, instances_per_ratio, timeout);
}

// Function to run scaling benchmark (fixed ratio, increasing variables)
void runScalingBenchmark()
{
    // Settings for scaling benchmark
    std::vector<int> variable_counts = {50, 75, 100, 125, 150, 200};
    double ratio = 4.25; // At phase transition for maximum difficulty
    int instances_per_size = 5;
    auto timeout = std::chrono::seconds(300); // 5 minutes per instance

    std::cout << "Running Scaling Benchmark\n";
    std::cout << "========================\n";
    std::cout << "Testing " << variable_counts.size() << " different problem sizes\n";
    std::cout << "Fixed clause ratio: " << ratio << " (phase transition)\n";
    std::cout << "Each size tested with " << instances_per_size << " instances\n";
    std::cout << "Timeout per instance: " << timeout.count() << " seconds\n\n";

    for (int num_vars : variable_counts)
    {
        std::vector<double> ratios = {ratio}; // Just one ratio
        testClauseRatios(num_vars, ratios, instances_per_size, timeout);
    }
}

// Function to test the portfolio's performance against different configuration types
void runConfigurationBenchmark()
{
    // Settings
    int num_vars = 100;
    double ratio = 4.25; // Phase transition
    int instances = 10;
    auto timeout = std::chrono::seconds(180); // 3 minutes per instance

    std::cout << "Running Configuration Effectiveness Benchmark\n";
    std::cout << "===========================================\n";
    std::cout << "This benchmark shows which configurations are most effective at the phase transition\n";
    std::cout << "Variables: " << num_vars << ", Clause ratio: " << ratio << "\n";
    std::cout << "Testing " << instances << " instances\n\n";

    std::vector<double> ratios = {ratio}; // Just the phase transition ratio
    testClauseRatios(num_vars, ratios, instances, timeout);
}

// Main function with command line processing
int main(int argc, char *argv[])
{
    std::cout << "Portfolio SAT Solver Benchmark\n";
    std::cout << "==============================\n\n";

    if (argc > 1)
    {
        std::string command = argv[1];

        if (command == "phase")
        {
            runPhaseTransitionBenchmark();
        }
        else if (command == "scale")
        {
            runScalingBenchmark();
        }
        else if (command == "config")
        {
            runConfigurationBenchmark();
        }
        else if (command == "custom")
        {
            // Parse custom benchmark parameters
            int num_vars = 100;
            int instances = 5;
            int timeout_seconds = 120;

            if (argc > 2)
                num_vars = std::stoi(argv[2]);
            if (argc > 3)
                instances = std::stoi(argv[3]);
            if (argc > 4)
                timeout_seconds = std::stoi(argv[4]);

            // Generate ratios from 3.0 to 5.0 in steps of 0.2
            std::vector<double> ratios;
            for (double r = 3.0; r <= 5.0; r += 0.2)
            {
                ratios.push_back(r);
            }

            std::cout << "Running Custom Benchmark\n";
            std::cout << "======================\n";
            std::cout << "Variables: " << num_vars << "\n";
            std::cout << "Instances per ratio: " << instances << "\n";
            std::cout << "Timeout: " << timeout_seconds << " seconds\n\n";

            testClauseRatios(num_vars, ratios, instances, std::chrono::seconds(timeout_seconds));
        }
        else if (command == "help")
        {
            std::cout << "Usage:\n";
            std::cout << "  ./portfolio_solver phase   - Run phase transition benchmark\n";
            std::cout << "  ./portfolio_solver scale   - Run scaling benchmark\n";
            std::cout << "  ./portfolio_solver config  - Run configuration effectiveness benchmark\n";
            std::cout << "  ./portfolio_solver custom [vars] [instances] [timeout] - Run custom benchmark\n";
            std::cout << "  ./portfolio_solver help    - Show this help\n";
        }
        else
        {
            std::cout << "Unknown command: " << command << "\n";
            std::cout << "Try 'help' for usage information.\n";
        }
    }
    else
    {
        // Default: run a small phase transition benchmark
        std::cout << "Running default benchmark (quick phase transition test)\n\n";

        int num_vars = 75;
        int instances_per_ratio = 3;
        auto timeout = std::chrono::seconds(60);

        // A few points around phase transition for a quick test
        std::vector<double> ratios = {3.5, 4.0, 4.25, 4.5, 5.0};

        testClauseRatios(num_vars, ratios, instances_per_ratio, timeout);
    }

    return 0;
}