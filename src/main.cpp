#include <iostream>
#include <chrono>
#include <iomanip>
#include <string>
#include "../include/SATInstance.h"
#include "../include/DPLL.h"
#include "../include/CDCL.h"

enum SolverType
{
    DPLL_SOLVER,
    CDCL_SOLVER
};

int main(int argc, char *argv[])
{
    bool debug_mode = false;
    SolverType solver_type = CDCL_SOLVER; // Default to CDCL

    // Process command line arguments
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--debug" || arg == "-d")
        {
            debug_mode = true;
        }
        else if (arg == "--dpll")
        {
            solver_type = DPLL_SOLVER;
        }
        else if (arg == "--cdcl")
        {
            solver_type = CDCL_SOLVER;
        }
    }

    std::cout << "SAT Solver Benchmarks" << std::endl;
    std::cout << "Algorithm: " << (solver_type == DPLL_SOLVER ? "DPLL with VSIDS" : "CDCL with Non-Chronological Backtracking") << std::endl;
    std::cout << (debug_mode ? "Debug mode: ON\n" : "Debug mode: OFF\n");

    // Define a helper function for running benchmarks with either solver
    auto runBenchmark = [&](const std::string &name, const CNF &cnf)
    {
        std::cout << "\n----------------------------------------\n";
        std::cout << "Testing " << name << ":\n";

        bool result = false;
        double elapsed_time = 0.0;

        if (solver_type == DPLL_SOLVER)
        {
            // Reset performance counters
            dpll_calls = 0;
            backtracks = 0;

            // Create instance
            SATInstance instance(cnf, debug_mode);
            if (debug_mode)
                instance.print();
            instance.initializeVSIDS();

            // Timing
            auto start = std::chrono::high_resolution_clock::now();
            result = DPLL(instance);
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = end - start;
            elapsed_time = elapsed.count();

            // Print results in a formatted table
            std::cout << "Result:         " << (result ? "SATISFIABLE" : "UNSATISFIABLE") << std::endl;
            std::cout << "Execution Time: " << std::fixed << std::setprecision(3) << elapsed_time << " ms" << std::endl;
            std::cout << "Recursive Calls: " << dpll_calls << std::endl;
            std::cout << "Backtracks:     " << backtracks << std::endl;

            // Print variable assignments if debug mode and satisfiable
            if (debug_mode && result)
            {
                std::cout << "\nVariable Assignments:\n";
                for (const auto &[var, val] : instance.assignments)
                {
                    std::cout << "x" << var << " = " << val << "\n";
                }
            }
        }
        else
        { // CDCL_SOLVER
            // Create CDCL solver
            CDCLSolver solver(cnf, debug_mode);

            // Timing
            auto start = std::chrono::high_resolution_clock::now();
            result = solver.solve();
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = end - start;
            elapsed_time = elapsed.count();

            // Print results
            std::cout << "Result:         " << (result ? "SATISFIABLE" : "UNSATISFIABLE") << std::endl;
            std::cout << "Execution Time: " << std::fixed << std::setprecision(3) << elapsed_time << " ms" << std::endl;
            std::cout << "Conflicts:      " << solver.getConflicts() << std::endl;
            std::cout << "Decisions:      " << solver.getDecisions() << std::endl;
            std::cout << "Propagations:   " << solver.getPropagations() << std::endl;
            std::cout << "Learned Clauses: " << solver.getLearnedClauses() << std::endl;
            std::cout << "Max Decision Level: " << solver.getMaxDecisionLevel() << std::endl;
            std::cout << "Restarts:       " << solver.getRestarts() << std::endl;

            // Print variable assignments if debug mode and satisfiable
            if (debug_mode && result)
            {
                std::cout << "\nVariable Assignments:\n";
                auto assignments = solver.getAssignments();
                for (const auto &[var, val] : assignments)
                {
                    std::cout << "x" << var << " = " << val << "\n";
                }
            }
        }

        return elapsed_time;
    };

    // I'm writing this a month after I first wrote this code.
    // Writing these CNFs by hand was one of the most pointless things I've done.
    // Please do not fall for "it's only a few clauses."

    // Test 1: Simple Satisfiable Formula
    CNF exampleCNF = {{1, 2}, {-1, 3}, {-2, -3}};
    double simpleCNFTime = runBenchmark("Simple Satisfiable CNF", exampleCNF);

    // Test 2: 4-Queens Problem
    CNF queensCNF = {
        // At least one queen in each row
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
        {13, 14, 15, 16},

        // At most one queen in each row
        {-1, -2},
        {-1, -3},
        {-1, -4},
        {-2, -3},
        {-2, -4},
        {-3, -4},
        {-5, -6},
        {-5, -7},
        {-5, -8},
        {-6, -7},
        {-6, -8},
        {-7, -8},
        {-9, -10},
        {-9, -11},
        {-9, -12},
        {-10, -11},
        {-10, -12},
        {-11, -12},
        {-13, -14},
        {-13, -15},
        {-13, -16},
        {-14, -15},
        {-14, -16},
        {-15, -16},

        // At least one queen in each column
        {1, 5, 9, 13},
        {2, 6, 10, 14},
        {3, 7, 11, 15},
        {4, 8, 12, 16},

        // At most one queen in each column
        {-1, -5},
        {-1, -9},
        {-1, -13},
        {-5, -9},
        {-5, -13},
        {-9, -13},
        {-2, -6},
        {-2, -10},
        {-2, -14},
        {-6, -10},
        {-6, -14},
        {-10, -14},
        {-3, -7},
        {-3, -11},
        {-3, -15},
        {-7, -11},
        {-7, -15},
        {-11, -15},
        {-4, -8},
        {-4, -12},
        {-4, -16},
        {-8, -12},
        {-8, -16},
        {-12, -16},

        // No queens on same diagonal (top-left to bottom-right)
        {-1, -6},
        {-1, -11},
        {-1, -16},
        {-2, -7},
        {-2, -12},
        {-3, -8},
        {-5, -10},
        {-5, -15},
        {-6, -11},
        {-6, -16},
        {-7, -12},
        {-9, -14},

        // No queens on same diagonal (top-right to bottom-left)
        {-4, -7},
        {-4, -10},
        {-4, -13},
        {-3, -6},
        {-3, -9},
        {-2, -5},
        {-8, -11},
        {-8, -14},
        {-7, -10},
        {-7, -13},
        {-6, -9},
        {-12, -15}};
    double queensTime = runBenchmark("4-Queens Problem (Satisfiable)", queensCNF);

    // Test 3: Pigeonhole Principle
    CNF pigeonholeCNF = {
        // Each pigeon must be in at least one hole
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
        {13, 14, 15, 16},
        {17, 18, 19, 20},

        // Each pigeon can be in at most one hole
        {-1, -2},
        {-1, -3},
        {-1, -4},
        {-2, -3},
        {-2, -4},
        {-3, -4},
        {-5, -6},
        {-5, -7},
        {-5, -8},
        {-6, -7},
        {-6, -8},
        {-7, -8},
        {-9, -10},
        {-9, -11},
        {-9, -12},
        {-10, -11},
        {-10, -12},
        {-11, -12},
        {-13, -14},
        {-13, -15},
        {-13, -16},
        {-14, -15},
        {-14, -16},
        {-15, -16},
        {-17, -18},
        {-17, -19},
        {-17, -20},
        {-18, -19},
        {-18, -20},
        {-19, -20},

        // No two pigeons in the same hole
        {-1, -5},
        {-1, -9},
        {-1, -13},
        {-1, -17},
        {-5, -9},
        {-5, -13},
        {-5, -17},
        {-9, -13},
        {-9, -17},
        {-13, -17},

        {-2, -6},
        {-2, -10},
        {-2, -14},
        {-2, -18},
        {-6, -10},
        {-6, -14},
        {-6, -18},
        {-10, -14},
        {-10, -18},
        {-14, -18},

        {-3, -7},
        {-3, -11},
        {-3, -15},
        {-3, -19},
        {-7, -11},
        {-7, -15},
        {-7, -19},
        {-11, -15},
        {-11, -19},
        {-15, -19},

        {-4, -8},
        {-4, -12},
        {-4, -16},
        {-4, -20},
        {-8, -12},
        {-8, -16},
        {-8, -20},
        {-12, -16},
        {-12, -20},
        {-16, -20}};
    double pigeonholeTime = runBenchmark("Pigeonhole Principle (Unsatisfiable)", pigeonholeCNF);

    // Generate 8-Queens problem for a more challenging benchmark
    if (!debug_mode)
    { // Only run this larger benchmark in non-debug mode
        std::cout << "\n----------------------------------------\n";
        std::cout << "Generating 8-Queens Problem...\n";

        CNF queens8CNF;
        const int N = 8;    // 8×8 board
        const int base = 1; // Starting variable index

        // At least one queen in each row
        for (int row = 0; row < N; row++)
        {
            Clause atLeastOneInRow;
            for (int col = 0; col < N; col++)
            {
                int var = base + row * N + col;
                atLeastOneInRow.push_back(var);
            }
            queens8CNF.push_back(atLeastOneInRow);
        }

        // At most one queen in each row
        for (int row = 0; row < N; row++)
        {
            for (int col1 = 0; col1 < N; col1++)
            {
                for (int col2 = col1 + 1; col2 < N; col2++)
                {
                    int var1 = base + row * N + col1;
                    int var2 = base + row * N + col2;
                    queens8CNF.push_back({-var1, -var2});
                }
            }
        }

        // At least one queen in each column
        for (int col = 0; col < N; col++)
        {
            Clause atLeastOneInCol;
            for (int row = 0; row < N; row++)
            {
                int var = base + row * N + col;
                atLeastOneInCol.push_back(var);
            }
            queens8CNF.push_back(atLeastOneInCol);
        }

        // At most one queen in each column
        for (int col = 0; col < N; col++)
        {
            for (int row1 = 0; row1 < N; row1++)
            {
                for (int row2 = row1 + 1; row2 < N; row2++)
                {
                    int var1 = base + row1 * N + col;
                    int var2 = base + row2 * N + col;
                    queens8CNF.push_back({-var1, -var2});
                }
            }
        }

        // No two queens on the same diagonal
        // Top-left to bottom-right diagonals
        for (int d = 0; d < 2 * N - 1; d++)
        {
            for (int i = 0; i < N; i++)
            {
                int row = i;
                int col = d - i;
                if (col >= 0 && col < N)
                {
                    for (int j = i + 1; j < N; j++)
                    {
                        int row2 = j;
                        int col2 = d - j;
                        if (col2 >= 0 && col2 < N)
                        {
                            int var1 = base + row * N + col;
                            int var2 = base + row2 * N + col2;
                            queens8CNF.push_back({-var1, -var2});
                        }
                    }
                }
            }
        }

        // Top-right to bottom-left diagonals
        for (int d = 0; d < 2 * N - 1; d++)
        {
            for (int i = 0; i < N; i++)
            {
                int row = i;
                int col = i - d + N - 1;
                if (col >= 0 && col < N)
                {
                    for (int j = i + 1; j < N; j++)
                    {
                        int row2 = j;
                        int col2 = j - d + N - 1;
                        if (col2 >= 0 && col2 < N)
                        {
                            int var1 = base + row * N + col;
                            int var2 = base + row2 * N + col2;
                            queens8CNF.push_back({-var1, -var2});
                        }
                    }
                }
            }
        }

        // Run 8-Queens benchmark
        double queens8Time = runBenchmark("8-Queens Problem (Satisfiable)", queens8CNF);

        // Print summary comparison
        std::cout << "\n----------------------------------------\n";
        std::cout << "Performance Summary:\n";
        std::cout << "Simple CNF Time: " << std::fixed << std::setprecision(3) << simpleCNFTime << " ms\n";
        std::cout << "4-Queens Time:  " << std::fixed << std::setprecision(3) << queensTime << " ms\n";
        std::cout << "8-Queens Time:  " << std::fixed << std::setprecision(3) << queens8Time << " ms\n";
        std::cout << "Time Ratio (8-Queens/4-Queens): " << std::fixed << std::setprecision(2)
                  << queens8Time / queensTime << "x\n";
        std::cout << "Pigeonhole Time: " << std::fixed << std::setprecision(3) << pigeonholeTime << " ms\n";

        // Add more challenging benchmarks for CDCL if needed
        if (solver_type == CDCL_SOLVER)
        {
            // Random 3-SAT benchmarks with varying clause-to-variable ratios
            std::cout << "\n----------------------------------------\n";
            std::cout << "Running Random 3-SAT benchmarks...\n";

            auto generateRandom3SAT = [](int num_vars, double clause_ratio) -> CNF
            {
                CNF formula;
                int num_clauses = static_cast<int>(num_vars * clause_ratio);

                // Initialize random seed
                std::srand(static_cast<unsigned int>(std::time(nullptr)));

                for (int i = 0; i < num_clauses; i++)
                {
                    Clause clause;
                    // Generate 3 distinct literals for the clause
                    while (clause.size() < 3)
                    {
                        int var = (std::rand() % num_vars) + 1;        // Variables start from 1
                        int lit = (std::rand() % 2) == 0 ? var : -var; // Randomly negate

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
            };

            // Benchmark with different clause-to-variable ratios
            const int num_vars = 100;
            const std::vector<double> ratios = {3.0, 4.0, 4.25, 4.5, 5.0}; // Phase transition around 4.25

            for (double ratio : ratios)
            {
                CNF random_formula = generateRandom3SAT(num_vars, ratio);
                std::string benchmark_name = "Random 3-SAT (n=" + std::to_string(num_vars) +
                                             ", ratio=" + std::to_string(ratio) + ")";
                runBenchmark(benchmark_name, random_formula);
            }
        }
    }

    return 0;
}