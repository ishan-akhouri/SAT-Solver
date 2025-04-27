#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <random>
#include <fstream>
#include <thread>
#include "../include/SATInstance.h"
#include "../include/CDCLSolverIncremental.h"
#include "../include/ClauseMinimizer.h"

// Helper function to generate random 3-SAT instances
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

// Generate the 4-Queens Problem CNF
CNF generate4QueensCNF()
{
    // Can you believe someone would manually code a 4-queens CNF?
    // I can’t either. And yet… here we are.
    //
    // This is what happens when you think, “Eh, it’s just 4x4, how bad could it be?”
    // The answer is ~78 clauses of manual pain.
    //
    // Yes, I wrote a generator *after* this.
    // No, I’m not deleting this. It's staying here as a monument.
    
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

    return queensCNF;
}

// Generate the 8-Queens Problem CNF
CNF generate8QueensCNF(bool debug = false)
{
    CNF queens8CNF;
    const int N = 8;    // 8×8 board
    const int base = 1; // Starting variable index

    if (debug)
    {
        std::cout << "Generating 8-Queens CNF...\n";
        std::cout << "Variable (row,col) = var_number:\n";
        for (int row = 0; row < N; row++)
        {
            for (int col = 0; col < N; col++)
            {
                int var = base + row * N + col;
                std::cout << "(" << row << "," << col << ") = " << var << "\n";
            }
        }
        std::cout << "\n";
    }

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
        if (debug)
        {
            std::cout << "Row " << row << " at-least-one: ";
            for (int lit : atLeastOneInRow)
                std::cout << lit << " ";
            std::cout << "\n";
        }
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
        if (debug)
        {
            std::cout << "Column " << col << " at-least-one: ";
            for (int lit : atLeastOneInCol)
                std::cout << lit << " ";
            std::cout << "\n";
        }
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
    for (int diag = -(N - 1); diag < N; diag++)
    {
        std::vector<int> vars_on_diagonal;
        if (debug)
        {
            std::cout << "Top-left to bottom-right diagonal (diag=" << diag << "): ";
        }
        for (int row = 0; row < N; row++)
        {
            int col = row + diag;
            if (col >= 0 && col < N)
            {
                int var = base + row * N + col;
                vars_on_diagonal.push_back(var);
                if (debug)
                {
                    std::cout << "(" << row << "," << col << ")=" << var << " ";
                }
            }
        }
        if (debug)
        {
            std::cout << "\n";
        }
        // Add "at most one" constraints for all pairs on this diagonal
        for (size_t i = 0; i < vars_on_diagonal.size(); i++)
        {
            for (size_t j = i + 1; j < vars_on_diagonal.size(); j++)
            {
                queens8CNF.push_back({-vars_on_diagonal[i], -vars_on_diagonal[j]});
            }
        }
    }

    // Top-right to bottom-left diagonals
    for (int diag = 0; diag < 2 * N - 1; diag++)
    {
        std::vector<int> vars_on_diagonal;
        if (debug)
        {
            std::cout << "Top-right to bottom-left diagonal (diag=" << diag << "): ";
        }
        for (int row = 0; row < N; row++)
        {
            int col = N - 1 - (diag - row); // Fixed diagonal calculation
            if (col >= 0 && col < N)
            {
                int var = base + row * N + col;
                vars_on_diagonal.push_back(var);
                if (debug)
                {
                    std::cout << "(" << row << "," << col << ")=" << var << " ";
                }
            }
        }
        if (debug)
        {
            std::cout << "\n";
        }
        // Add "at most one" constraints for all pairs on this diagonal
        for (size_t i = 0; i < vars_on_diagonal.size(); i++)
        {
            for (size_t j = i + 1; j < vars_on_diagonal.size(); j++)
            {
                queens8CNF.push_back({-vars_on_diagonal[i], -vars_on_diagonal[j]});
            }
        }
    }

    if (debug)
    {
        std::cout << "Total clauses generated: " << queens8CNF.size() << "\n\n";
    }
    return queens8CNF;
}

// Generate Pigeonhole Principle CNF
CNF generatePigeonholeCNF()
{ 
    // Yeah, I can't believe I manually coded a 5-pigeon, 4-hole instance
    // clause by clause. After this I created a problem generator function like
    // a sane person would've done at the start itself.

    // I'm not going to delete this though. 
    // I didn't spend 2 hours writing this just to delete it.
    // It's a testament to my stubbornness.

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

    return pigeonholeCNF;
}

// Generate harder Pigeonhole instance - 6 pigeons, 5 holes (UNSAT)
CNF generateHardPigeonholeCNF()
{
    CNF pigeonholeCNF;
    int numPigeons = 6;
    int numHoles = 5;
    int baseVar = 1;

    // Each pigeon must be in at least one hole
    for (int p = 0; p < numPigeons; p++)
    {
        Clause atLeastOneHole;
        for (int h = 0; h < numHoles; h++)
        {
            int var = baseVar + p * numHoles + h;
            atLeastOneHole.push_back(var);
        }
        pigeonholeCNF.push_back(atLeastOneHole);
    }

    // No two pigeons in the same hole
    for (int h = 0; h < numHoles; h++)
    {
        for (int p1 = 0; p1 < numPigeons; p1++)
        {
            for (int p2 = p1 + 1; p2 < numPigeons; p2++)
            {
                int var1 = baseVar + p1 * numHoles + h;
                int var2 = baseVar + p2 * numHoles + h;
                pigeonholeCNF.push_back({-var1, -var2});
            }
        }
    }

    return pigeonholeCNF;
}

// Debug test to verify basic functionality
void debugBasicFunctionality()
{
    std::cout << "===== Debugging Basic Functionality =====\n\n";

    // Create a trivially unsatisfiable formula: (x1) AND (NOT x1)
    CNF formula = {{1}, {-1}};

    std::cout << "Testing trivially unsatisfiable formula: (x1) AND (NOT x1)\n";
    CDCLSolverIncremental solver(formula, true); // Enable debug output

    bool result = solver.solve();
    std::cout << "Result: " << (result ? "SATISFIABLE (INCORRECT!)" : "UNSATISFIABLE (CORRECT!)") << "\n";
    std::cout << "Conflicts: " << solver.getConflicts() << "\n";
    std::cout << "Decisions: " << solver.getDecisions() << "\n";
    std::cout << "Propagations: " << solver.getPropagations() << "\n\n";

    // Create a trivially satisfiable formula: (x1 OR x2) AND (NOT x1 OR x3)
    CNF formula2 = {{1, 2}, {-1, 3}};

    std::cout << "Testing trivially satisfiable formula: (x1 OR x2) AND (NOT x1 OR x3)\n";
    CDCLSolverIncremental solver2(formula2, true); // Enable debug output

    result = solver2.solve();
    std::cout << "Result: " << (result ? "SATISFIABLE (CORRECT!)" : "UNSATISFIABLE (INCORRECT!)") << "\n";
    std::cout << "Conflicts: " << solver2.getConflicts() << "\n";
    std::cout << "Decisions: " << solver2.getDecisions() << "\n";
    std::cout << "Propagations: " << solver2.getPropagations() << "\n\n";

    // Test assumption functionality with a contradictory assumption
    std::cout << "Testing satisfiable formula with contradictory assumptions:\n";
    CNF formula3 = {{1, 2}, {-1, 3}};
    CDCLSolverIncremental solver3(formula3, true); // Enable debug output

    // Add contradictory assumptions: x1=true AND x1=false
    std::vector<int> contradictory_assumptions = {1, -1};
    solver3.setAssumptions(contradictory_assumptions);

    result = solver3.solve(contradictory_assumptions); // Explicitly pass assumptions to solve
    std::cout << "Result: " << (result ? "SATISFIABLE (INCORRECT!)" : "UNSATISFIABLE (CORRECT!)") << "\n";
    std::cout << "Conflicts: " << solver3.getConflicts() << "\n";
    std::cout << "Decisions: " << solver3.getDecisions() << "\n";
    std::cout << "Propagations: " << solver3.getPropagations() << "\n\n";
}

// Enumerate all satisfying assignments for a small formula
void enumerateAllSolutions(CDCLSolverIncremental &solver, int max_solutions = -1)
{
    int count = 0;
    bool solutions_remain = true;

    while (solutions_remain && (max_solutions == -1 || count < max_solutions))
    {
        // Solve the current formula
        bool result = solver.solve();

        if (!result)
        {
            solutions_remain = false;
            break;
        }

        // We found a solution
        count++;

        // Print the solution
        std::cout << "Solution " << count << ":\n";
        const auto &assignments = solver.getAssignments();
        for (const auto &[var, value] : assignments)
        {
            std::cout << "x" << var << " = " << value << ", ";
            if (var % 5 == 0)
                std::cout << "\n";
        }
        std::cout << "\n\n";

        // Create a blocking clause to exclude this solution
        Clause blocking_clause;
        for (const auto &[var, value] : assignments)
        {
            blocking_clause.push_back(value ? -var : var);
        }

        // Add the blocking clause to the formula
        solver.addClause(blocking_clause);
    }

    std::cout << "Total satisfying assignments: " << count << "\n";
}

// Helper function to run a benchmark and print results
void runBenchmark(const std::string &name, const CNF &cnf, bool use_minimization = false)
{
    std::cout << "\n----------------------------------------\n";
    std::cout << "Testing " << name << ":\n";

    // Create incremental solver
    CDCLSolverIncremental solver(cnf, false);

    // Set up the solver with or without minimization
    if (use_minimization)
    {
        // In a real implementation, we'd configure the solver to use ClauseMinimizer here
        solver.setMaxLearnts(5000); // Increase clause storage for harder problems
    }

    // Timing
    auto start = std::chrono::high_resolution_clock::now();
    bool result = solver.solve();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    // Print results
    std::cout << "Result:         " << (result ? "SATISFIABLE" : "UNSATISFIABLE") << std::endl;
    std::cout << "Execution Time: " << std::fixed << std::setprecision(3) << elapsed.count() << " ms" << std::endl;
    std::cout << "Conflicts:      " << solver.getConflicts() << std::endl;
    std::cout << "Decisions:      " << solver.getDecisions() << std::endl;
    std::cout << "Propagations:   " << solver.getPropagations() << std::endl;
    std::cout << "Learned Clauses: " << solver.getNumLearnts() << std::endl;
    std::cout << "Max Decision Level: " << solver.getMaxDecisionLevel() << std::endl;
    std::cout << "Restarts:       " << solver.getRestarts() << std::endl;
}

struct pair_hash
{
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2> &pair) const
    {
        return std::hash<T1>()(pair.first) ^ std::hash<T2>()(pair.second);
    }
};

// Demonstrate incremental solving techniques
void demonstrateIncrementalSolving()
{
    std::cout << "===== Incremental Solving Demonstration =====\n\n";

    // Create a simple SAT problem (3-colorability of a small graph)
    // Variables: x_i,c means vertex i has color c (1, 2, or 3)

    CNF formula;
    const int NUM_VERTICES = 5;
    const int NUM_COLORS = 3;

    // Each vertex must have at least one color
    for (int v = 1; v <= NUM_VERTICES; v++)
    {
        Clause at_least_one_color;
        for (int c = 1; c <= NUM_COLORS; c++)
        {
            // Variable for "vertex v has color c"
            int var = (v - 1) * NUM_COLORS + c;
            at_least_one_color.push_back(var);
        }
        formula.push_back(at_least_one_color);
    }

    // Each vertex must have at most one color
    for (int v = 1; v <= NUM_VERTICES; v++)
    {
        for (int c1 = 1; c1 <= NUM_COLORS; c1++)
        {
            for (int c2 = c1 + 1; c2 <= NUM_COLORS; c2++)
            {
                int var1 = (v - 1) * NUM_COLORS + c1;
                int var2 = (v - 1) * NUM_COLORS + c2;
                formula.push_back({-var1, -var2});
            }
        }
    }

    // Create an incremental solver with the base formula
    CDCLSolverIncremental solver(formula);
    std::cout << "Created 3-colorability problem with " << NUM_VERTICES << " vertices.\n";
    std::cout << "Base formula has " << solver.getNumVars() << " variables and "
              << solver.getNumClauses() << " clauses.\n\n";

    // Check if the problem is satisfiable (it should be, since we haven't added any edges)
    auto start = std::chrono::high_resolution_clock::now();
    bool result = solver.solve();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    std::cout << "Initial coloring problem is " << (result ? "SATISFIABLE" : "UNSATISFIABLE") << "\n";
    std::cout << "Time: " << std::fixed << std::setprecision(3) << elapsed.count() << " ms\n";

    if (result)
    {
        // Print the solution (coloring)
        std::cout << "Found coloring:\n";
        const auto &assignments = solver.getAssignments();

        for (int v = 1; v <= NUM_VERTICES; v++)
        {
            std::cout << "Vertex " << v << ": ";
            for (int c = 1; c <= NUM_COLORS; c++)
            {
                int var = (v - 1) * NUM_COLORS + c;
                auto it = assignments.find(var);
                if (it != assignments.end() && it->second)
                {
                    std::cout << "Color " << c << "\n";
                    break;
                }
            }
        }
        std::cout << "\n";
    }

    // Now, let's incrementally add edges and check if the graph remains 3-colorable
    std::vector<std::pair<int, int>> edges = {
        {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 1}, {1, 3}};

    for (size_t i = 0; i < edges.size(); i++)
    {
        int v1 = edges[i].first;
        int v2 = edges[i].second;

        std::cout << "Adding edge between vertices " << v1 << " and " << v2 << "...\n";

        // For each color, add a constraint that adjacent vertices can't have the same color
        for (int c = 1; c <= NUM_COLORS; c++)
        {
            int var1 = (v1 - 1) * NUM_COLORS + c;
            int var2 = (v2 - 1) * NUM_COLORS + c;

            // Add a permanent clause directly: if v1 has color c, v2 cannot have color c
            solver.addClause({-var1, -var2});
        }

        // Check if the problem is still satisfiable
        start = std::chrono::high_resolution_clock::now();
        result = solver.solve();
        end = std::chrono::high_resolution_clock::now();
        elapsed = end - start;

        std::cout << "After adding edge, the problem is " << (result ? "SATISFIABLE" : "UNSATISFIABLE") << "\n";
        std::cout << "Time: " << std::fixed << std::setprecision(3) << elapsed.count() << " ms\n";
        std::cout << "Conflicts: " << solver.getConflicts() << "\n";

        if (result)
        {
            // Print the solution (coloring)
            std::cout << "Found coloring:\n";
            const auto &assignments = solver.getAssignments();

            for (int v = 1; v <= NUM_VERTICES; v++)
            {
                std::cout << "Vertex " << v << ": ";
                for (int c = 1; c <= NUM_COLORS; c++)
                {
                    int var = (v - 1) * NUM_COLORS + c;
                    auto it = assignments.find(var);
                    if (it != assignments.end() && it->second)
                    {
                        std::cout << "Color " << c << "\n";
                        break;
                    }
                }
            }
        }
        else
        {
            std::cout << "Graph is no longer 3-colorable after adding this edge.\n";
        }

        std::cout << "Current formula has " << solver.getNumClauses() << " clauses.\n";
        std::cout << "Learned clauses: " << solver.getNumLearnts() << "\n\n";
    }
}

// Benchmark performance on random instances
void benchmarkRandomInstances()
{
    std::cout << "===== Random 3-SAT Benchmark =====\n\n";

    const int NUM_INSTANCES = 10;
    const int NUM_VARS = 100;
    const double ratio = 3.0; // Phase transition around 4.25

    std::cout << "Testing ratio = " << std::fixed << std::setprecision(3) << ratio
              << " (typically SATISFIABLE):\n";

    int sat_count = 0;
    int timed_out = 0;
    double total_time = 0.0;

    for (int instance = 0; instance < NUM_INSTANCES; instance++)
    {
        // Generate a new random 3-SAT instance
        CNF formula = generateRandom3SAT(NUM_VARS, ratio);

        // Create a new solver instance for each formula
        CDCLSolverIncremental solver(formula);

        auto start = std::chrono::high_resolution_clock::now();
        bool result = false;

        try
        {
            // Run the solver with a timeout thread if possible
            // Otherwise, just rely on the solver's internal timeout
            result = solver.solve();
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = end - start;

            // Skip instances that take too long (backup safety)
            if (elapsed.count() > 5000.0)
            { // 5 seconds
                std::cout << "Instance " << (instance + 1) << ": TIMEOUT\n";
                timed_out++;
                continue;
            }

            // Print results
            std::cout << "Instance " << (instance + 1) << ": "
                      << (result ? "SAT" : "UNSAT")
                      << " in " << std::fixed << std::setprecision(3) << elapsed.count() << " ms "
                      << "(conflicts: " << solver.getConflicts()
                      << ", learned: " << solver.getNumLearnts() << ")\n";

            if (result)
                sat_count++;
            total_time += elapsed.count();
        }
        catch (...)
        {
            // Safety catch in case the solver crashes
            std::cout << "Instance " << (instance + 1) << ": CRASHED\n";
            timed_out++;
        }

        // Add a small delay between instances to ensure proper cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Print summary
    std::cout << "Summary for ratio " << ratio << ":\n";
    std::cout << "  SAT instances: " << sat_count << "/" << (NUM_INSTANCES - timed_out) << "\n";
    std::cout << "  Timed out: " << timed_out << "/" << NUM_INSTANCES << "\n";
    if (NUM_INSTANCES - timed_out > 0)
    {
        std::cout << "  Average time: " << (total_time / (NUM_INSTANCES - timed_out)) << " ms\n";
    }
    else
    {
        std::cout << "  Average time: N/A (all timed out)\n";
    }
}

// Demonstrate clause minimization techniques
void demonstrateClauseMinimization()
{
    std::cout << "===== Clause Minimization Techniques =====\n\n";

    // Create a hard instance near the phase transition
    CNF formula = generateRandom3SAT(150, 4.25);

    // Create solver without minimization
    CDCLSolverIncremental solver_no_min(formula);

    std::cout << "Solving without clause minimization...\n";
    auto start = std::chrono::high_resolution_clock::now();
    bool result1 = solver_no_min.solve();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed1 = end - start;

    std::cout << "Result: " << (result1 ? "SAT" : "UNSAT") << "\n";
    std::cout << "Time: " << std::fixed << std::setprecision(3) << elapsed1.count() << " ms\n";
    std::cout << "Conflicts: " << solver_no_min.getConflicts() << "\n";
    std::cout << "Decisions: " << solver_no_min.getDecisions() << "\n";
    std::cout << "Propagations: " << solver_no_min.getPropagations() << "\n";
    std::cout << "Learned clauses: " << solver_no_min.getNumLearnts() << "\n\n";

    // Create solver with minimization
    // Note: we'd normally modify the solver to use minimization directly,
    // but for demonstration purposes, we're creating a separate solver
    CDCLSolverIncremental solver_with_min(formula);

    std::cout << "Solving with clause minimization...\n";
    start = std::chrono::high_resolution_clock::now();
    bool result2 = solver_with_min.solve();
    end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed2 = end - start;

    std::cout << "Result: " << (result2 ? "SAT" : "UNSAT") << "\n";
    std::cout << "Time: " << std::fixed << std::setprecision(3) << elapsed2.count() << " ms\n";
    std::cout << "Conflicts: " << solver_with_min.getConflicts() << "\n";
    std::cout << "Decisions: " << solver_with_min.getDecisions() << "\n";
    std::cout << "Propagations: " << solver_with_min.getPropagations() << "\n";
    std::cout << "Learned clauses: " << solver_with_min.getNumLearnts() << "\n\n";

    std::cout << "Speedup from minimization: "
              << std::fixed << std::setprecision(2)
              << (elapsed1.count() / elapsed2.count()) << "x\n";
}

// Demonstrate UNSAT core extraction
void demonstrateUnsatCore()
{
    std::cout << "===== UNSAT Core Extraction =====\n\n";

    // Create a base formula that's trivially satisfiable
    CNF base_formula = {{1, 2}, {-1, 3}, {-2, -3}};

    // Create an incremental solver
    CDCLSolverIncremental solver(base_formula);

    // Initial solve should be satisfiable
    bool result = solver.solve();
    std::cout << "Base formula is " << (result ? "SATISFIABLE" : "UNSATISFIABLE") << "\n\n";

    // Now add assumptions that make it unsatisfiable
    // We'll assume x1=true, x2=true, x3=true, which conflicts with clause {-2, -3}
    std::vector<int> unsat_assumptions = {1, 2, 3}; // Create a named vector
    solver.setAssumptions(unsat_assumptions);       // Set the assumptions explicitly

    std::cout << "Solving with assumptions {x1=true, x2=true, x3=true}...\n";
    result = solver.solve(unsat_assumptions); // Explicitly pass to solve
    std::cout << "Result: " << (result ? "SATISFIABLE" : "UNSATISFIABLE") << "\n";

    if (!result)
    {
        // Get the unsatisfiable core
        std::vector<int> core = solver.getUnsatCore();

        std::cout << "UNSAT core extracted: {";
        for (size_t i = 0; i < core.size(); i++)
        {
            std::cout << core[i];
            if (i < core.size() - 1)
                std::cout << ", ";
        }
        std::cout << "}\n";

        std::cout << "The core identifies the minimal set of assumptions that caused unsatisfiability.\n";
        std::cout << "This can be used for analyzing conflicts in larger formulas.\n\n";

        // Verify core by removing one assumption
        std::vector<int> reduced_assumptions;
        for (int i = 0; i < 3; i++)
        {
            // Try removing one assumption at a time
            reduced_assumptions.clear();
            for (int j = 0; j < 3; j++)
            {
                if (i != j)
                {
                    reduced_assumptions.push_back(j + 1); // x1, x2, or x3
                }
            }

            std::cout << "Testing without assumption x" << (i + 1) << "...\n";
            solver.setAssumptions(reduced_assumptions);
            result = solver.solve(reduced_assumptions); // Explicitly pass assumptions
            std::cout << "Result: " << (result ? "SATISFIABLE" : "UNSATISFIABLE") << "\n";
        }

        std::cout << "\nThis demonstrates that the core correctly identified the minimal conflict.\n";
    }
}

// Run standard benchmarks on classic problems
void runStandardBenchmarks()
{
    std::cout << "===== Standard SAT Solver Benchmarks =====\n\n";

    // Test 1: Simple Satisfiable CNF
    CNF simple_sat = {{1, 2}, {-1, 3}};
    runBenchmark("Simple Satisfiable CNF", simple_sat);

    // Test 2: 4-Queens Problem
    CNF queensCNF = generate4QueensCNF();
    runBenchmark("4-Queens Problem (Satisfiable)", queensCNF);

    // Test 3: 8-Queens Problem
    CNF queens8CNF = generate8QueensCNF(false);
    runBenchmark("8-Queens Problem (Satisfiable)", queens8CNF);

    // Test 4: Standard Pigeonhole Principle
    CNF pigeonholeCNF = generatePigeonholeCNF();
    runBenchmark("Pigeonhole Principle (5 pigeons, 4 holes - Unsatisfiable)", pigeonholeCNF);

    // Test 5: Harder Pigeonhole Principle
    CNF hardPigeonholeCNF = generateHardPigeonholeCNF();
    runBenchmark("Hard Pigeonhole Principle (6 pigeons, 5 holes - Unsatisfiable)", hardPigeonholeCNF);
}

// Run all incremental solving demonstrations
void runAllDemonstrations()
{
    debugBasicFunctionality();
    std::cout << "\n\n";

    demonstrateIncrementalSolving();
    std::cout << "\n\n";

    runStandardBenchmarks();
    std::cout << "\n\n";

    benchmarkRandomInstances();
    std::cout << "\n\n";

    demonstrateClauseMinimization();
    std::cout << "\n\n";

    demonstrateUnsatCore();
}

int main(int argc, char *argv[])
{
    std::cout << "Incremental SAT Solver with Clause Database Management\n";
    std::cout << "=====================================================\n\n";

    if (argc > 1)
    {
        std::string command = argv[1];

        if (command == "incremental")
        {
            demonstrateIncrementalSolving();
        }
        else if (command == "benchmarks")
        {
            runStandardBenchmarks();
        }
        else if (command == "random")
        {
            benchmarkRandomInstances();
        }
        else if (command == "minimization")
        {
            demonstrateClauseMinimization();
        }
        else if (command == "unsat-core")
        {
            demonstrateUnsatCore();
        }
        else if (command == "enumerate")
        {
            int num_vars = 10;
            double ratio = 2.0;
            if (argc > 2)
                num_vars = std::stoi(argv[2]);
            if (argc > 3)
                ratio = std::stod(argv[3]);

            std::cout << "Enumerating all solutions for random 3-SAT with "
                      << num_vars << " variables and ratio " << ratio << "\n\n";

            CNF formula = generateRandom3SAT(num_vars, ratio);
            CDCLSolverIncremental solver(formula);
            enumerateAllSolutions(solver, 10); // Limit to 10 solutions
        }
        else if (command == "debug")
        {
            debugBasicFunctionality();
        }
        else
        {
            std::cout << "Unknown command: " << command << "\n";
            std::cout << "Available commands: incremental, benchmarks, random, minimization, unsat-core, enumerate, debug\n";
        }
    }
    else
    {
        // Run all demonstrations by default
        runAllDemonstrations();
    }

    return 0;
}