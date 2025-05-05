#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <random>
#include <fstream>
#include <unordered_set>
#include "MaxSATSolver.h"
#include "WeightedMaxSATSolver.h"
#include "HybridMaxSATSolver.h"
#include <set>

// Generate a minimum vertex cover problem
// Returns hard clauses, soft clauses, and weights
std::tuple<CNF, CNF, std::vector<int>> generateVertexCoverProblem(int num_vertices, int num_edges, int seed = 42)
{
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> vertex_dist(1, num_vertices);

    // Create a random graph
    std::vector<std::pair<int, int>> edges;

    // Generate random edges
    int edges_added = 0;
    while (edges_added < num_edges)
    {
        int v1 = vertex_dist(gen);
        int v2 = vertex_dist(gen);

        // Avoid self-loops and duplicate edges
        if (v1 != v2)
        {
            // Ensure v1 < v2 for canonical representation
            if (v1 > v2)
                std::swap(v1, v2);

            // Check if edge already exists
            bool duplicate = false;
            for (const auto &edge : edges)
            {
                if (edge.first == v1 && edge.second == v2)
                {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate)
            {
                edges.push_back({v1, v2});
                edges_added++;
            }
        }
    }

    // Create CNF encoding
    CNF hard_clauses;
    CNF soft_clauses;
    std::vector<int> weights;

    // Hard constraints: Each edge must be covered (at least one endpoint in the cover)
    for (const auto &edge : edges)
    {
        hard_clauses.push_back({edge.first, edge.second});
    }

    // Soft constraints: Each vertex should not be in the cover if possible
    std::uniform_int_distribution<> weight_dist(1, 10);
    for (int v = 1; v <= num_vertices; v++)
    {
        soft_clauses.push_back({-v});
        weights.push_back(weight_dist(gen)); // Random weight
    }

    return {hard_clauses, soft_clauses, weights};
}

// Generate a maximum independent set problem
std::tuple<CNF, CNF, std::vector<int>> generateIndependentSetProblem(int num_vertices, int num_edges, int seed = 42)
{
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> vertex_dist(1, num_vertices);

    // Create a random graph
    std::vector<std::pair<int, int>> edges;

    // Generate random edges
    int edges_added = 0;
    while (edges_added < num_edges)
    {
        int v1 = vertex_dist(gen);
        int v2 = vertex_dist(gen);

        // Avoid self-loops and duplicate edges
        if (v1 != v2)
        {
            // Ensure v1 < v2 for canonical representation
            if (v1 > v2)
                std::swap(v1, v2);

            // Check if edge already exists
            bool duplicate = false;
            for (const auto &edge : edges)
            {
                if (edge.first == v1 && edge.second == v2)
                {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate)
            {
                edges.push_back({v1, v2});
                edges_added++;
            }
        }
    }

    // Create CNF encoding
    CNF hard_clauses;
    CNF soft_clauses;
    std::vector<int> weights;

    // Hard constraints: Adjacent vertices cannot both be in the independent set
    for (const auto &edge : edges)
    {
        hard_clauses.push_back({-edge.first, -edge.second});
    }

    // Soft constraints: Each vertex should be in the independent set if possible
    std::uniform_int_distribution<> weight_dist(1, 10);
    for (int v = 1; v <= num_vertices; v++)
    {
        soft_clauses.push_back({v});
        weights.push_back(weight_dist(gen)); // Random weight
    }

    return {hard_clauses, soft_clauses, weights};
}

// Generate a graph coloring problem with a given number of colors
std::tuple<CNF, CNF, std::vector<int>> generateGraphColoringProblem(int num_vertices, int num_edges, int num_colors, int seed = 42)
{
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> vertex_dist(1, num_vertices);

    // Create a random graph
    std::vector<std::pair<int, int>> edges;

    // Generate random edges
    int edges_added = 0;
    while (edges_added < num_edges)
    {
        int v1 = vertex_dist(gen);
        int v2 = vertex_dist(gen);

        // Avoid self-loops and duplicate edges
        if (v1 != v2)
        {
            // Ensure v1 < v2 for canonical representation
            if (v1 > v2)
                std::swap(v1, v2);

            // Check if edge already exists
            bool duplicate = false;
            for (const auto &edge : edges)
            {
                if (edge.first == v1 && edge.second == v2)
                {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate)
            {
                edges.push_back({v1, v2});
                edges_added++;
            }
        }
    }

    // Create CNF encoding
    CNF hard_clauses;
    CNF soft_clauses;
    std::vector<int> weights;

    // Encode vertex colors as variables
    // Variable (v-1)*num_colors + c represents "vertex v has color c"

    // Hard constraints: Each vertex must have at least one color
    for (int v = 1; v <= num_vertices; v++)
    {
        Clause at_least_one_color;
        for (int c = 1; c <= num_colors; c++)
        {
            int var = (v - 1) * num_colors + c;
            at_least_one_color.push_back(var);
        }
        hard_clauses.push_back(at_least_one_color);

        // Each vertex must have at most one color
        for (int c1 = 1; c1 <= num_colors; c1++)
        {
            for (int c2 = c1 + 1; c2 <= num_colors; c2++)
            {
                int var1 = (v - 1) * num_colors + c1;
                int var2 = (v - 1) * num_colors + c2;
                hard_clauses.push_back({-var1, -var2});
            }
        }
    }

    // Hard constraints: Adjacent vertices must have different colors
    for (const auto &edge : edges)
    {
        int v1 = edge.first;
        int v2 = edge.second;

        for (int c = 1; c <= num_colors; c++)
        {
            int var1 = (v1 - 1) * num_colors + c;
            int var2 = (v2 - 1) * num_colors + c;
            hard_clauses.push_back({-var1, -var2});
        }
    }

    // Soft constraints: Prefer certain colors for certain vertices (just for creating a weighted problem)
    std::uniform_int_distribution<> weight_dist(1, 10);
    for (int v = 1; v <= num_vertices; v++)
    {
        // Randomly prefer a particular color for this vertex
        int preferred_color = (gen() % num_colors) + 1;
        int var = (v - 1) * num_colors + preferred_color;
        soft_clauses.push_back({var});
        weights.push_back(weight_dist(gen));
    }

    return {hard_clauses, soft_clauses, weights};
}

// Generate a scheduling problem
std::tuple<CNF, CNF, std::vector<int>> generateSchedulingProblem(int num_tasks, int num_timeslots, int num_conflicts, int seed = 42)
{
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> task_dist(1, num_tasks);
    std::uniform_int_distribution<> timeslot_dist(1, num_timeslots);

    // Create random conflicts between tasks
    std::vector<std::pair<int, int>> conflicts;

    // Generate random conflicts
    int conflicts_added = 0;
    while (conflicts_added < num_conflicts)
    {
        int t1 = task_dist(gen);
        int t2 = task_dist(gen);

        // Avoid self-conflicts and duplicate conflicts
        if (t1 != t2)
        {
            // Ensure t1 < t2 for canonical representation
            if (t1 > t2)
                std::swap(t1, t2);

            // Check if conflict already exists
            bool duplicate = false;
            for (const auto &conflict : conflicts)
            {
                if (conflict.first == t1 && conflict.second == t2)
                {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate)
            {
                conflicts.push_back({t1, t2});
                conflicts_added++;
            }
        }
    }

    // Create CNF encoding
    CNF hard_clauses;
    CNF soft_clauses;
    std::vector<int> weights;

    // Encode task scheduling as variables
    // Variable (t-1)*num_timeslots + s represents "task t is scheduled at timeslot s"

    // Hard constraints: Each task must be scheduled in at least one timeslot
    for (int t = 1; t <= num_tasks; t++)
    {
        Clause at_least_one_timeslot;
        for (int s = 1; s <= num_timeslots; s++)
        {
            int var = (t - 1) * num_timeslots + s;
            at_least_one_timeslot.push_back(var);
        }
        hard_clauses.push_back(at_least_one_timeslot);

        // Each task must be scheduled in at most one timeslot
        for (int s1 = 1; s1 <= num_timeslots; s1++)
        {
            for (int s2 = s1 + 1; s2 <= num_timeslots; s2++)
            {
                int var1 = (t - 1) * num_timeslots + s1;
                int var2 = (t - 1) * num_timeslots + s2;
                hard_clauses.push_back({-var1, -var2});
            }
        }
    }

    // Hard constraints: Conflicting tasks cannot be scheduled in the same timeslot
    for (const auto &conflict : conflicts)
    {
        int t1 = conflict.first;
        int t2 = conflict.second;

        for (int s = 1; s <= num_timeslots; s++)
        {
            int var1 = (t1 - 1) * num_timeslots + s;
            int var2 = (t2 - 1) * num_timeslots + s;
            hard_clauses.push_back({-var1, -var2});
        }
    }

    // Soft constraints: Prefer certain timeslots for certain tasks
    std::uniform_int_distribution<> weight_dist(1, 20);
    for (int t = 1; t <= num_tasks; t++)
    {
        // Each task has a preferred timeslot
        int preferred_timeslot = timeslot_dist(gen);
        int var = (t - 1) * num_timeslots + preferred_timeslot;
        soft_clauses.push_back({var});
        weights.push_back(weight_dist(gen));
    }

    return {hard_clauses, soft_clauses, weights};
}

void testSmallExample()
{
    std::cout << "===== Small MaxSAT Example =====" << std::endl;

    // Create a simple MaxSAT problem
    // Hard clauses: (x1 OR x2) AND (NOT x1 OR x3)
    CNF hard_clauses = {{1, 2}, {-1, 3}};

    // Soft clauses: (NOT x2) AND (NOT x3)
    CNF soft_clauses = {{-2}, {-3}};

    // Create MaxSAT solver
    MaxSATSolver solver(hard_clauses, true); // Enable debug output

    // Add soft clauses
    solver.addSoftClauses(soft_clauses);

    std::cout << "Solving with linear search:" << std::endl;
    int linear_result = solver.solve();

    std::cout << "Result: " << linear_result << " violated soft clauses" << std::endl;

    // Print the satisfying assignment
    std::cout << "Satisfying assignment:" << std::endl;
    const auto &assignment = solver.getAssignment();
    for (const auto &[var, value] : assignment)
    {
        std::cout << "x" << var << " = " << (value ? "true" : "false") << std::endl;
    }

    std::cout << "\nSolving with binary search:" << std::endl;

    // Create a new solver for binary search
    MaxSATSolver binary_solver(hard_clauses, true);
    binary_solver.addSoftClauses(soft_clauses);

    int binary_result = binary_solver.solveBinarySearch();

    std::cout << "Result: " << binary_result << " violated soft clauses" << std::endl;
    std::cout << std::endl;

    // Test with hybrid solver
    std::cout << "\nSolving with hybrid solver:" << std::endl;
    HybridMaxSATSolver hybrid_solver(hard_clauses, true);
    hybrid_solver.addSoftClauses(soft_clauses);

    int hybrid_result = hybrid_solver.solve();

    std::cout << "Result: " << hybrid_result << " violated soft clauses" << std::endl;
    std::cout << std::endl;
}

void testSmallWeightedExample()
{
    std::cout << "===== Small Weighted MaxSAT Example =====" << std::endl;

    // Create a simple MaxSAT problem
    // Hard clauses: (x1 OR x2) AND (NOT x1 OR x3)
    CNF hard_clauses = {{1, 2}, {-1, 3}};

    // Soft clauses: (NOT x2) with weight 3 AND (NOT x3) with weight 1
    CNF soft_clauses = {{-2}, {-3}};
    std::vector<int> weights = {3, 1};

    // Create Weighted MaxSAT solver
    WeightedMaxSATSolver solver(hard_clauses, true); // Enable debug output

    // Add soft clauses with weights
    for (size_t i = 0; i < soft_clauses.size(); i++)
    {
        solver.addSoftClause(soft_clauses[i], weights[i]);
    }

    std::cout << "Solving with stratified approach:" << std::endl;
    int stratified_result = solver.solveStratified();

    std::cout << "Result: " << stratified_result << " total weight of violated soft clauses" << std::endl;

    std::cout << "\nSolving with binary search:" << std::endl;

    // Create a new solver for binary search
    WeightedMaxSATSolver binary_solver(hard_clauses, true);
    for (size_t i = 0; i < soft_clauses.size(); i++)
    {
        binary_solver.addSoftClause(soft_clauses[i], weights[i]);
    }

    int binary_result = binary_solver.solveBinarySearch();

    std::cout << "Result: " << binary_result << " total weight of violated soft clauses" << std::endl;

    // Test with hybrid solver
    std::cout << "\nSolving with hybrid solver:" << std::endl;
    HybridMaxSATSolver hybrid_solver(hard_clauses, true);

    // Add weighted soft clauses
    for (size_t i = 0; i < soft_clauses.size(); i++)
    {
        hybrid_solver.addSoftClause(soft_clauses[i], weights[i]);
    }

    int hybrid_result = hybrid_solver.solve();

    std::cout << "Result: " << hybrid_result << " total weight of violated soft clauses" << std::endl;
    std::cout << std::endl;
}

void benchmarkVertexCover()
{
    std::cout << "===== Vertex Cover Benchmarks =====" << std::endl;

    // Define benchmarks of increasing difficulty
    struct BenchmarkConfig
    {
        std::string name;
        int num_vertices;
        int num_edges;
        int seed;
        int timeout_ms; // Add a timeout for each benchmark
    };

    std::vector<BenchmarkConfig> benchmarks = {
        {"Small", 20, 40, 42, 5000},
        {"Medium", 40, 100, 43, 10000},
        {"Large", 60, 200, 44, 20000},
        {"Dense", 30, 150, 45, 10000} // Denser graph
    };

    // Results tracking
    for (const auto &config : benchmarks)
    {
        std::cout << "Running " << config.name << " benchmark ("
                  << config.num_vertices << " vertices, "
                  << config.num_edges << " edges):" << std::endl;

        // Generate vertex cover problem
        auto [hard_clauses, soft_clauses, weights] =
            generateVertexCoverProblem(config.num_vertices, config.num_edges, config.seed);

        std::cout << "  Problem size: " << hard_clauses.size() << " hard clauses, "
                  << soft_clauses.size() << " soft clauses" << std::endl;

        double stratified_time = 0.0;
        double binary_time = 0.0;
        double hybrid_time = 0.0;
        int stratified_calls = 0;
        int binary_calls = 0;
        int hybrid_calls = 0;
        int stratified_result = 0;
        int binary_result = 0;
        int hybrid_result = 0;
        bool stratified_timeout = false;
        bool binary_timeout = false;
        bool hybrid_timeout = false;

        // Unweighted test (all weights = 1)
        std::cout << "  Testing unweighted version:" << std::endl;

        // Linear search
        MaxSATSolver linear_solver(hard_clauses);
        linear_solver.addSoftClauses(soft_clauses);

        auto linear_start = std::chrono::high_resolution_clock::now();
        int linear_result = linear_solver.solve();
        auto linear_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> linear_elapsed = linear_end - linear_start;

        std::cout << "    Linear search: " << linear_result << " violated clauses, "
                  << std::fixed << std::setprecision(2) << linear_elapsed.count()
                  << "ms, " << linear_solver.getNumSolverCalls() << " solver calls" << std::endl;

        // Binary search
        MaxSATSolver binary_solver(hard_clauses);
        binary_solver.addSoftClauses(soft_clauses);

        auto binary_start = std::chrono::high_resolution_clock::now();
        int binary_result_unweighted = binary_solver.solveBinarySearch();
        auto binary_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> binary_elapsed = binary_end - binary_start;

        std::cout << "    Binary search: " << binary_result_unweighted << " violated clauses, "
                  << std::fixed << std::setprecision(2) << binary_elapsed.count()
                  << "ms, " << binary_solver.getNumSolverCalls() << " solver calls" << std::endl;

        if (linear_result != binary_result_unweighted && linear_result >= 0 && binary_result_unweighted >= 0)
        {
            std::cout << "    WARNING: Results don't match!" << std::endl;
        }

        // Calculate speedup
        double binary_speedup = linear_elapsed.count() / binary_elapsed.count();
        std::cout << "    Binary search speedup: " << std::fixed << std::setprecision(2)
                  << binary_speedup << "x" << std::endl;

        // Weighted test (with provided weights)
        std::cout << "  Testing weighted version:" << std::endl;

        // Stratified approach
        WeightedMaxSATSolver stratified_solver(hard_clauses);
        for (size_t j = 0; j < soft_clauses.size(); j++)
        {
            stratified_solver.addSoftClause(soft_clauses[j], weights[j]);
        }

        auto stratified_start = std::chrono::high_resolution_clock::now();

        // Add timeout check
        auto timeout = std::chrono::milliseconds(config.timeout_ms);
        auto start_time = std::chrono::high_resolution_clock::now();

        stratified_result = stratified_solver.solveStratified();

        auto stratified_end = std::chrono::high_resolution_clock::now();
        stratified_time = std::chrono::duration<double, std::milli>(stratified_end - stratified_start).count();
        stratified_calls = stratified_solver.getNumSolverCalls();

        stratified_timeout = (stratified_end - start_time) > timeout;

        // Binary search
        WeightedMaxSATSolver binary_solver_weighted(hard_clauses);
        for (size_t j = 0; j < soft_clauses.size(); j++)
        {
            binary_solver_weighted.addSoftClause(soft_clauses[j], weights[j]);
        }

        auto binary_start_weighted = std::chrono::high_resolution_clock::now();

        binary_result = binary_solver_weighted.solveBinarySearch();

        auto binary_end_weighted = std::chrono::high_resolution_clock::now();
        binary_time = std::chrono::duration<double, std::milli>(binary_end_weighted - binary_start_weighted).count();
        binary_calls = binary_solver_weighted.getNumSolverCalls();

        binary_timeout = (binary_end_weighted - start_time) > timeout;

        // Hybrid solver
        HybridMaxSATSolver hybrid_solver(hard_clauses);
        for (size_t j = 0; j < soft_clauses.size(); j++)
        {
            hybrid_solver.addSoftClause(soft_clauses[j], weights[j]);
        }

        auto hybrid_start = std::chrono::high_resolution_clock::now();

        hybrid_result = hybrid_solver.solve();

        auto hybrid_end = std::chrono::high_resolution_clock::now();
        hybrid_time = std::chrono::duration<double, std::milli>(hybrid_end - hybrid_start).count();
        hybrid_calls = hybrid_solver.getNumSolverCalls();

        hybrid_timeout = (hybrid_end - start_time) > timeout;

        // Print results
        std::cout << "    Stratified: " << (stratified_timeout ? "TIMEOUT" : std::to_string(stratified_result))
                  << " total weight violated, "
                  << std::fixed << std::setprecision(2) << stratified_time
                  << "ms, " << stratified_calls << " solver calls" << std::endl;

        std::cout << "    Binary search: " << (binary_timeout ? "TIMEOUT" : std::to_string(binary_result))
                  << " total weight violated, "
                  << std::fixed << std::setprecision(2) << binary_time
                  << "ms, " << binary_calls << " solver calls" << std::endl;

        std::cout << "    Hybrid solver: " << (hybrid_timeout ? "TIMEOUT" : std::to_string(hybrid_result))
                  << " total weight violated, "
                  << std::fixed << std::setprecision(2) << hybrid_time
                  << "ms, " << hybrid_calls << " solver calls" << std::endl;

        // Verify results match or check for anomalies
        if (!stratified_timeout && !binary_timeout && stratified_result != -1 && binary_result != -1)
        {
            if (binary_result < stratified_result)
            {
                std::cout << "    NOTE: Binary search found better solution than stratified!" << std::endl;
            }
            else if (binary_result > stratified_result)
            {
                std::cout << "    WARNING: Binary search found worse solution!" << std::endl;
            }

            if (!hybrid_timeout && hybrid_result != stratified_result)
            {
                if (hybrid_result < stratified_result)
                {
                    std::cout << "    NOTE: Hybrid solver found better solution than stratified!" << std::endl;
                }
                else
                {
                    std::cout << "    WARNING: Hybrid solution differs from stratified!" << std::endl;
                }
            }
        }

        // Calculate speedups
        if (!stratified_timeout && !binary_timeout)
        {
            double binary_speedup_weighted = stratified_time / binary_time;
            std::cout << "    Binary search speedup: " << std::fixed << std::setprecision(2)
                      << binary_speedup_weighted << "x" << std::endl;
        }

        if (!stratified_timeout && !hybrid_timeout)
        {
            double hybrid_speedup = stratified_time / hybrid_time;
            std::cout << "    Hybrid solver speedup: " << std::fixed << std::setprecision(2)
                      << hybrid_speedup << "x" << std::endl;
        }

        std::cout << std::endl;
    }
}

void benchmarkIndependentSet()
{
    std::cout << "===== Maximum Independent Set Benchmarks =====" << std::endl;

    // Define benchmarks of increasing difficulty
    struct BenchmarkConfig
    {
        std::string name;
        int num_vertices;
        int num_edges;
        int seed;
        int timeout_ms;
    };

    std::vector<BenchmarkConfig> benchmarks = {
        {"Small", 20, 30, 42, 5000},
        {"Medium", 40, 80, 43, 10000},
        {"Large", 60, 150, 44, 20000}};

    // Results tracking
    for (const auto &config : benchmarks)
    {
        std::cout << "Running " << config.name << " benchmark ("
                  << config.num_vertices << " vertices, "
                  << config.num_edges << " edges):" << std::endl;

        // Generate independent set problem
        auto [hard_clauses, soft_clauses, weights] =
            generateIndependentSetProblem(config.num_vertices, config.num_edges, config.seed);

        std::cout << "  Problem size: " << hard_clauses.size() << " hard clauses, "
                  << soft_clauses.size() << " soft clauses" << std::endl;

        // Weighted test with stratified vs binary
        std::cout << "  Testing weighted version:" << std::endl;

        // Stratified approach
        WeightedMaxSATSolver stratified_solver(hard_clauses);
        for (size_t j = 0; j < soft_clauses.size(); j++)
        {
            stratified_solver.addSoftClause(soft_clauses[j], weights[j]);
        }

        auto stratified_start = std::chrono::high_resolution_clock::now();

        // Add timeout check
        auto timeout = std::chrono::milliseconds(config.timeout_ms);
        auto start_time = std::chrono::high_resolution_clock::now();

        int stratified_result = stratified_solver.solveStratified();

        auto stratified_end = std::chrono::high_resolution_clock::now();
        double stratified_time = std::chrono::duration<double, std::milli>(stratified_end - stratified_start).count();
        int stratified_calls = stratified_solver.getNumSolverCalls();

        bool stratified_timeout = (stratified_end - start_time) > timeout;

        // Binary search
        WeightedMaxSATSolver binary_solver(hard_clauses);
        for (size_t j = 0; j < soft_clauses.size(); j++)
        {
            binary_solver.addSoftClause(soft_clauses[j], weights[j]);
        }

        auto binary_start = std::chrono::high_resolution_clock::now();

        int binary_result = binary_solver.solveBinarySearch();

        auto binary_end = std::chrono::high_resolution_clock::now();
        double binary_time = std::chrono::duration<double, std::milli>(binary_end - binary_start).count();
        int binary_calls = binary_solver.getNumSolverCalls();

        bool binary_timeout = (binary_end - start_time) > timeout;

        // Hybrid solver
        HybridMaxSATSolver hybrid_solver(hard_clauses);
        for (size_t j = 0; j < soft_clauses.size(); j++)
        {
            hybrid_solver.addSoftClause(soft_clauses[j], weights[j]);
        }

        auto hybrid_start = std::chrono::high_resolution_clock::now();

        int hybrid_result = hybrid_solver.solve();

        auto hybrid_end = std::chrono::high_resolution_clock::now();
        double hybrid_time = std::chrono::duration<double, std::milli>(hybrid_end - hybrid_start).count();
        int hybrid_calls = hybrid_solver.getNumSolverCalls();

        bool hybrid_timeout = (hybrid_end - start_time) > timeout;

        // Print results
        std::cout << "    Stratified: " << (stratified_timeout ? "TIMEOUT" : std::to_string(stratified_result))
                  << " total weight violated, "
                  << std::fixed << std::setprecision(2) << stratified_time
                  << "ms, " << stratified_calls << " solver calls" << std::endl;

        std::cout << "    Binary search: " << (binary_timeout ? "TIMEOUT" : std::to_string(binary_result))
                  << " total weight violated, "
                  << std::fixed << std::setprecision(2) << binary_time
                  << "ms, " << binary_calls << " solver calls" << std::endl;

        std::cout << "    Hybrid solver: " << (hybrid_timeout ? "TIMEOUT" : std::to_string(hybrid_result))
                  << " total weight violated, "
                  << std::fixed << std::setprecision(2) << hybrid_time
                  << "ms, " << hybrid_calls << " solver calls" << std::endl;

        // Calculate speedups if no timeouts
        if (!stratified_timeout && !binary_timeout)
        {
            double binary_speedup = stratified_time / binary_time;
            std::cout << "    Binary search speedup: " << std::fixed << std::setprecision(2)
                      << binary_speedup << "x" << std::endl;
        }

        if (!stratified_timeout && !hybrid_timeout)
        {
            double hybrid_speedup = stratified_time / hybrid_time;
            std::cout << "    Hybrid solver speedup: " << std::fixed << std::setprecision(2)
                      << hybrid_speedup << "x" << std::endl;
        }

        std::cout << std::endl;
    }
}

void benchmarkGraphColoring()
{
    std::cout << "===== Graph Coloring Benchmarks =====" << std::endl;

    // Define benchmarks of increasing difficulty
    struct BenchmarkConfig
    {
        std::string name;
        int num_vertices;
        int num_edges;
        int num_colors;
        int seed;
        int timeout_ms;
    };

    std::vector<BenchmarkConfig> benchmarks = {
        {"Small", 10, 20, 3, 42, 5000},
        {"Medium", 15, 40, 4, 43, 10000}};

    // Results tracking
    for (const auto &config : benchmarks)
    {
        std::cout << "Running " << config.name << " benchmark ("
                  << config.num_vertices << " vertices, "
                  << config.num_edges << " edges, "
                  << config.num_colors << " colors):" << std::endl;

        // Generate graph coloring problem
        auto [hard_clauses, soft_clauses, weights] =
            generateGraphColoringProblem(config.num_vertices, config.num_edges, config.num_colors, config.seed);

        std::cout << "  Problem size: " << hard_clauses.size() << " hard clauses, "
                  << soft_clauses.size() << " soft clauses" << std::endl;

        // Weighted test with stratified vs binary
        std::cout << "  Testing weighted version:" << std::endl;

        // Stratified approach
        WeightedMaxSATSolver stratified_solver(hard_clauses);
        for (size_t j = 0; j < soft_clauses.size(); j++)
        {
            stratified_solver.addSoftClause(soft_clauses[j], weights[j]);
        }

        auto stratified_start = std::chrono::high_resolution_clock::now();

        // Add timeout check
        auto timeout = std::chrono::milliseconds(config.timeout_ms);
        auto start_time = std::chrono::high_resolution_clock::now();

        int stratified_result = stratified_solver.solveStratified();

        auto stratified_end = std::chrono::high_resolution_clock::now();
        double stratified_time = std::chrono::duration<double, std::milli>(stratified_end - stratified_start).count();
        int stratified_calls = stratified_solver.getNumSolverCalls();

        bool stratified_timeout = (stratified_end - start_time) > timeout;

        // Binary search
        WeightedMaxSATSolver binary_solver(hard_clauses);
        for (size_t j = 0; j < soft_clauses.size(); j++)
        {
            binary_solver.addSoftClause(soft_clauses[j], weights[j]);
        }

        auto binary_start = std::chrono::high_resolution_clock::now();

        int binary_result = binary_solver.solveBinarySearch();

        auto binary_end = std::chrono::high_resolution_clock::now();
        double binary_time = std::chrono::duration<double, std::milli>(binary_end - binary_start).count();
        int binary_calls = binary_solver.getNumSolverCalls();

        bool binary_timeout = (binary_end - start_time) > timeout;

        // Hybrid solver
        HybridMaxSATSolver hybrid_solver(hard_clauses);
        for (size_t j = 0; j < soft_clauses.size(); j++)
        {
            hybrid_solver.addSoftClause(soft_clauses[j], weights[j]);
        }

        auto hybrid_start = std::chrono::high_resolution_clock::now();

        int hybrid_result = hybrid_solver.solve();

        auto hybrid_end = std::chrono::high_resolution_clock::now();
        double hybrid_time = std::chrono::duration<double, std::milli>(hybrid_end - hybrid_start).count();
        int hybrid_calls = hybrid_solver.getNumSolverCalls();

        bool hybrid_timeout = (hybrid_end - start_time) > timeout;

        // Print results
        std::cout << "    Stratified: " << (stratified_timeout ? "TIMEOUT" : std::to_string(stratified_result))
                  << " total weight violated, "
                  << std::fixed << std::setprecision(2) << stratified_time
                  << "ms, " << stratified_calls << " solver calls" << std::endl;

        std::cout << "    Binary search: " << (binary_timeout ? "TIMEOUT" : std::to_string(binary_result))
                  << " total weight violated, "
                  << std::fixed << std::setprecision(2) << binary_time
                  << "ms, " << binary_calls << " solver calls" << std::endl;

        std::cout << "    Hybrid solver: " << (hybrid_timeout ? "TIMEOUT" : std::to_string(hybrid_result))
                  << " total weight violated, "
                  << std::fixed << std::setprecision(2) << hybrid_time
                  << "ms, " << hybrid_calls << " solver calls" << std::endl;

        // Calculate speedups if no timeouts
        if (!stratified_timeout && !binary_timeout)
        {
            double binary_speedup = stratified_time / binary_time;
            std::cout << "    Binary search speedup: " << std::fixed << std::setprecision(2)
                      << binary_speedup << "x" << std::endl;
        }

        if (!stratified_timeout && !hybrid_timeout)
        {
            double hybrid_speedup = stratified_time / hybrid_time;
            std::cout << "    Hybrid solver speedup: " << std::fixed << std::setprecision(2)
                      << hybrid_speedup << "x" << std::endl;
        }

        std::cout << std::endl;
    }
}

// Test the effect of warm starting on incremental problems
void testWarmStartingIncremental()
{
    std::cout << "===== Testing Warm Starting on Incremental Vertex Cover =====" << std::endl;

    // Create a baseline graph
    const int base_vertices = 30;
    const int base_edges = 50;
    const int num_iterations = 5; // Number of incremental changes

    auto [hard_clauses, soft_clauses, weights] =
        generateVertexCoverProblem(base_vertices, base_edges, 42);

    std::cout << "Base problem: " << base_vertices << " vertices, "
              << base_edges << " edges, "
              << hard_clauses.size() << " hard clauses, "
              << soft_clauses.size() << " soft clauses" << std::endl;

    // Test with warm starting enabled
    std::cout << "\nTesting with warm starting enabled:" << std::endl;
    double total_warm_time = 0.0;
    int total_warm_calls = 0;

    // Keep the same solver instance across iterations to test warm starting
    WeightedMaxSATSolver warm_solver(hard_clauses);
    for (size_t i = 0; i < soft_clauses.size(); i++)
    {
        warm_solver.addSoftClause(soft_clauses[i], weights[i]);
    }

    // Initial solve
    auto warm_start = std::chrono::high_resolution_clock::now();
    int warm_result = warm_solver.solveStratified();
    auto warm_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> warm_elapsed = warm_end - warm_start;

    std::cout << "Initial solve: " << warm_result << " weight violated, "
              << std::fixed << std::setprecision(2) << warm_elapsed.count()
              << "ms, " << warm_solver.getNumSolverCalls() << " solver calls" << std::endl;

    total_warm_time += warm_elapsed.count();
    total_warm_calls += warm_solver.getNumSolverCalls();

    // Add additional edges incrementally
    std::mt19937 gen(43); // Different seed for incremental changes
    std::uniform_int_distribution<> vertex_dist(1, base_vertices);

    // Keep track of existing edges to avoid duplicates
    std::set<std::pair<int, int>> existing_edges;
    for (const auto &clause : hard_clauses)
    {
        if (clause.size() == 2)
        {
            int v1 = clause[0];
            int v2 = clause[1];
            if (v1 > v2)
                std::swap(v1, v2);
            existing_edges.insert({v1, v2});
        }
    }

    CNF current_hard = hard_clauses;

    for (int i = 1; i <= num_iterations; i++)
    {
        // Add 5 new edges
        CNF new_edges;
        for (int j = 0; j < 5; j++)
        {
            int v1, v2;
            bool duplicate;

            // Generate a new non-duplicate edge
            do
            {
                v1 = vertex_dist(gen);
                v2 = vertex_dist(gen);
                if (v1 > v2)
                    std::swap(v1, v2);
                duplicate = (v1 == v2) || existing_edges.count({v1, v2}) > 0;
            } while (duplicate);

            // Add the new edge
            existing_edges.insert({v1, v2});
            new_edges.push_back({v1, v2});
        }

        // Add to current hard clauses
        current_hard.insert(current_hard.end(), new_edges.begin(), new_edges.end());

        // Create a new solver with the updated problem (but using warm starting)
        WeightedMaxSATSolver incremental_solver(current_hard);
        for (size_t j = 0; j < soft_clauses.size(); j++)
        {
            incremental_solver.addSoftClause(soft_clauses[j], weights[j]);
        }

        // Solve with warm starting (implicit)
        warm_start = std::chrono::high_resolution_clock::now();
        warm_result = incremental_solver.solveStratified();
        warm_end = std::chrono::high_resolution_clock::now();
        warm_elapsed = warm_end - warm_start;

        int solver_calls = incremental_solver.getNumSolverCalls();

        std::cout << "Iteration " << i << " (+" << new_edges.size() << " edges): "
                  << warm_result << " weight violated, "
                  << std::fixed << std::setprecision(2) << warm_elapsed.count()
                  << "ms, " << solver_calls << " solver calls" << std::endl;

        total_warm_time += warm_elapsed.count();
        total_warm_calls += solver_calls;
    }

    std::cout << "Total time with warm starting: " << total_warm_time << "ms, "
              << total_warm_calls << " solver calls" << std::endl;

    // Test without warm starting
    std::cout << "\nTesting without warm starting:" << std::endl;

    // Reset the problem
    auto [reset_hard, reset_soft, reset_weights] =
        generateVertexCoverProblem(base_vertices, base_edges, 42);

    double total_cold_time = 0.0;
    int total_cold_calls = 0;

    // Initial solve
    WeightedMaxSATSolver initial_solver(reset_hard);
    for (size_t i = 0; i < reset_soft.size(); i++)
    {
        initial_solver.addSoftClause(reset_soft[i], reset_weights[i]);
    }

    auto cold_start = std::chrono::high_resolution_clock::now();
    int cold_result = initial_solver.solveStratified();
    auto cold_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> cold_elapsed = cold_end - cold_start;

    std::cout << "Initial solve: " << cold_result << " weight violated, "
              << std::fixed << std::setprecision(2) << cold_elapsed.count()
              << "ms, " << initial_solver.getNumSolverCalls() << " solver calls" << std::endl;

    total_cold_time += cold_elapsed.count();
    total_cold_calls += initial_solver.getNumSolverCalls();

    // Add the same additional edges but create new solver each time (no warm starting)
    gen.seed(43); // Use same seed as before for fair comparison

    // Reset existing edges tracking
    existing_edges.clear();
    for (const auto &clause : reset_hard)
    {
        if (clause.size() == 2)
        {
            int v1 = clause[0];
            int v2 = clause[1];
            if (v1 > v2)
                std::swap(v1, v2);
            existing_edges.insert({v1, v2});
        }
    }

    CNF current_cold_hard = reset_hard;

    for (int i = 1; i <= num_iterations; i++)
    {
        // Add 5 new edges (same as with warm starting)
        CNF new_edges;
        for (int j = 0; j < 5; j++)
        {
            int v1, v2;
            bool duplicate;

            // Generate a new non-duplicate edge
            do
            {
                v1 = vertex_dist(gen);
                v2 = vertex_dist(gen);
                if (v1 > v2)
                    std::swap(v1, v2);
                duplicate = (v1 == v2) || existing_edges.count({v1, v2}) > 0;
            } while (duplicate);

            // Add the new edge
            existing_edges.insert({v1, v2});
            new_edges.push_back({v1, v2});
        }

        // Add to current hard clauses
        current_cold_hard.insert(current_cold_hard.end(), new_edges.begin(), new_edges.end());

        // Create a new solver (no warm starting)
        WeightedMaxSATSolver cold_solver(current_cold_hard);
        for (size_t j = 0; j < reset_soft.size(); j++)
        {
            cold_solver.addSoftClause(reset_soft[j], reset_weights[j]);
        }

        cold_start = std::chrono::high_resolution_clock::now();
        cold_result = cold_solver.solveStratified();
        cold_end = std::chrono::high_resolution_clock::now();
        cold_elapsed = cold_end - cold_start;

        std::cout << "Iteration " << i << " (+" << new_edges.size() << " edges): "
                  << cold_result << " weight violated, "
                  << std::fixed << std::setprecision(2) << cold_elapsed.count()
                  << "ms, " << cold_solver.getNumSolverCalls() << " solver calls" << std::endl;

        total_cold_time += cold_elapsed.count();
        total_cold_calls += cold_solver.getNumSolverCalls();
    }

    std::cout << "Total time without warm starting: " << total_cold_time << "ms, "
              << total_cold_calls << " solver calls" << std::endl;

    // Calculate speedup
    double speedup = total_cold_time / total_warm_time;
    double calls_ratio = static_cast<double>(total_cold_calls) / total_warm_calls;

    std::cout << "\nWarm starting speedup: " << std::fixed << std::setprecision(2)
              << speedup << "x" << std::endl;
    std::cout << "Solver calls ratio: " << std::fixed << std::setprecision(2)
              << calls_ratio << "x" << std::endl;
}

int main(int argc, char *argv[])
{
    std::cout << "MaxSAT Solver based on Incremental SAT" << std::endl;
    std::cout << "===============================================" << std::endl
              << std::endl;

    // Run small example tests to verify functionality
    testSmallExample();
    std::cout << std::endl;

    testSmallWeightedExample();
    std::cout << std::endl;

    // Run structured benchmarks
    benchmarkVertexCover();
    std::cout << std::endl;

    benchmarkIndependentSet();
    std::cout << std::endl;

    //  benchmarkGraphColoring();
    std::cout << std::endl;

    // Test warm starting effects on incremental problems
    testWarmStartingIncremental();
    std::cout << std::endl;

    return 0;
}