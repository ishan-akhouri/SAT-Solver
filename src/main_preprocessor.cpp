#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <random>
#include <fstream>
#include <set>
#include "../include/SATInstance.h"
#include "../include/CDCL.h"
#include "../include/CDCLSolverIncremental.h"
#include "../include/DPLL.h"
#include "../include/Preprocessor.h"

enum class RedundancyLevel
{
    LOW,
    MODERATE,
    HIGH
};

// Count how many variables in a formula
int countVariables(const CNF &formula)
{
    std::set<int> variables;
    for (const auto &clause : formula)
    {
        for (int literal : clause)
        {
            variables.insert(std::abs(literal));
        }
    }
    return variables.size();
}

// Verify a solution on the original formula
bool verifySolution(const CNF &formula, const std::unordered_map<int, bool> &solution)
{
    for (const auto &clause : formula)
    {
        bool clause_satisfied = false;

        for (int literal : clause)
        {
            int var = std::abs(literal);
            auto it = solution.find(var);

            if (it != solution.end())
            {
                bool value = it->second;
                if ((literal > 0 && value) || (literal < 0 && !value))
                {
                    clause_satisfied = true;
                    break;
                }
            }
        }

        if (!clause_satisfied)
        {
            return false; // Found an unsatisfied clause
        }
    }

    return true; // All clauses satisfied
}

// Run a solver with preprocessing
bool runWithPreprocessing(const std::string &problem_name, const CNF &formula,
                          bool use_preprocessing = true, ProblemType detected_type = ProblemType::GENERIC)
{
    std::cout << "\n===== Testing " << problem_name << " =====\n";
    std::cout << "Variables: " << countVariables(formula) << ", Clauses: " << formula.size() << "\n";

    CNF working_formula = formula;
    std::chrono::microseconds preprocess_time(0);
    std::chrono::microseconds solve_time(0);

    // Preprocessing step
    if (use_preprocessing)
    {
        std::cout << "Applying preprocessing...\n";

        auto preprocess_start = std::chrono::high_resolution_clock::now();

        // Create preprocessor with appropriate configuration
        PreprocessorConfig config;
        config.use_unit_propagation = true;
        config.use_pure_literal_elimination = true;
        config.use_subsumption = true;
        config.enable_initial_phase = true;
        config.enable_final_phase = true;

        Preprocessor preprocessor(config);

        // Use the provided problem type if specified
        if (detected_type != ProblemType::GENERIC)
        {
            preprocessor.setProblemType(detected_type);
        }

        // Apply preprocessing
        working_formula = preprocessor.preprocess(formula);

        auto preprocess_end = std::chrono::high_resolution_clock::now();
        preprocess_time = std::chrono::duration_cast<std::chrono::microseconds>(
            preprocess_end - preprocess_start);

        // Display preprocessing statistics
        preprocessor.printStats();
        std::cout << "Preprocessing time: " << preprocess_time.count() / 1000 << " ms\n";
    }
    else
    {
        std::cout << "Skipping preprocessing\n";
    }

    // Solving step
    std::cout << "Solving with CDCLSolverIncremental...\n";

    auto solve_start = std::chrono::high_resolution_clock::now();

    // Create solver and solve
    CDCLSolverIncremental solver(working_formula);
    bool result = solver.solve();

    auto solve_end = std::chrono::high_resolution_clock::now();
    solve_time = std::chrono::duration_cast<std::chrono::microseconds>(
        solve_end - solve_start);

    // Display results
    std::cout << "Result: " << (result ? "SATISFIABLE" : "UNSATISFIABLE") << "\n";
    std::cout << "Solving time: " << solve_time.count() << " μs\n";
    std::cout << "Conflicts: " << solver.getConflicts() << "\n";
    std::cout << "Decisions: " << solver.getDecisions() << "\n";
    std::cout << "Propagations: " << solver.getPropagations() << "\n";

    // If satisfiable, verify solution on original formula
    if (result)
    {
        std::unordered_map<int, bool> solution = solver.getAssignments();
        bool verified = verifySolution(formula, solution);
        std::cout << "Solution verification on original formula: "
                  << (verified ? "VALID" : "INVALID") << "\n";
    }

    return result;
}

// Add duplicate clauses to the formula
void addDuplicateClauses(CNF &formula, double percentage)
{
    size_t numToAdd = static_cast<size_t>(formula.size() * percentage);

    // Use a random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, formula.size() - 1);

    // Add copies of randomly selected clauses
    for (size_t i = 0; i < numToAdd; i++)
    {
        int idx = distrib(gen);
        formula.push_back(formula[idx]);
    }
}

// Add subsumed clauses - clauses that are supersets of existing ones
void addSubsumedClauses(CNF &formula, double percentage)
{
    size_t numToAdd = static_cast<size_t>(formula.size() * percentage);

    // Use a random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> clauseDistrib(0, formula.size() - 1);

    // Get set of all variables for adding extra literals
    std::set<int> allVars;
    for (const auto &clause : formula)
    {
        for (int lit : clause)
        {
            allVars.insert(std::abs(lit));
        }
    }

    std::vector<int> variables(allVars.begin(), allVars.end());
    std::uniform_int_distribution<> varDistrib(0, variables.size() - 1);
    std::uniform_int_distribution<> signDistrib(0, 1); // For positive/negative literals

    // Add subsumed clauses
    for (size_t i = 0; i < numToAdd; i++)
    {
        int idx = clauseDistrib(gen);
        Clause newClause = formula[idx];

        // Add 1-3 extra literals
        int extraLits = 1 + signDistrib(gen) + signDistrib(gen);
        for (int j = 0; j < extraLits; j++)
        {
            int varIdx = varDistrib(gen);
            int var = variables[varIdx];
            int lit = signDistrib(gen) ? var : -var;

            // Make sure this literal isn't already in the clause (prevents tautologies)
            if (std::find(newClause.begin(), newClause.end(), lit) == newClause.end() &&
                std::find(newClause.begin(), newClause.end(), -lit) == newClause.end())
            {
                newClause.push_back(lit);
            }
        }

        formula.push_back(newClause);
    }
}

// Add tautological clauses - always satisfied
void addTautologicalClauses(CNF &formula, double percentage)
{
    size_t numToAdd = static_cast<size_t>(formula.size() * percentage);

    // Use a random number generator
    std::random_device rd;
    std::mt19937 gen(rd());

    // Get set of all variables
    std::set<int> allVars;
    for (const auto &clause : formula)
    {
        for (int lit : clause)
        {
            allVars.insert(std::abs(lit));
        }
    }

    std::vector<int> variables(allVars.begin(), allVars.end());
    std::uniform_int_distribution<> varDistrib(0, variables.size() - 1);
    std::uniform_int_distribution<> sizeDistrib(2, 5); // Random clause size

    // Add tautological clauses
    for (size_t i = 0; i < numToAdd; i++)
    {
        Clause tautClause;

        // Pick a random variable to make tautological
        int varIdx = varDistrib(gen);
        int var = variables[varIdx];

        // Add both the variable and its negation
        tautClause.push_back(var);
        tautClause.push_back(-var);

        // Add some random literals for variety
        int extraLits = sizeDistrib(gen);
        for (int j = 0; j < extraLits; j++)
        {
            int randVarIdx = varDistrib(gen);
            int randVar = variables[randVarIdx];

            // Skip if we already used this variable for tautology
            if (randVar == var)
                continue;

            int lit = (gen() % 2) ? randVar : -randVar;
            tautClause.push_back(lit);
        }

        formula.push_back(tautClause);
    }
}

// Add transitive clauses - if (A -> B) and (B -> C) then add (A -> C)
void addTransitiveClauses(CNF &formula, double percentage)
{
    // Build implication graph
    std::map<int, std::set<int>> implications; // lit -> set of implied literals

    // Process binary clauses to find implications
    for (const auto &clause : formula)
    {
        if (clause.size() == 2)
        {
            // Convert (a v b) to (-a -> b) and (-b -> a)
            int lit1 = clause[0];
            int lit2 = clause[1];

            implications[-lit1].insert(lit2);
            implications[-lit2].insert(lit1);
        }
    }

    // Find transitive relations
    std::vector<Clause> transitiveClauses;
    for (const auto &[lit, implied] : implications)
    {
        for (int middle : implied)
        {
            // For each B that A implies
            if (implications.find(middle) != implications.end())
            {
                // For each C that B implies
                for (int target : implications[middle])
                {
                    // Create A -> C
                    // Convert implication to disjunction: (A -> C) becomes (~A v C)
                    Clause transitiveClause = {-lit, target};

                    // Check if this transitive clause already exists
                    bool exists = false;
                    for (const auto &clause : formula)
                    {
                        if (clause.size() == 2 &&
                            ((clause[0] == -lit && clause[1] == target) ||
                             (clause[0] == target && clause[1] == -lit)))
                        {
                            exists = true;
                            break;
                        }
                    }

                    if (!exists)
                    {
                        transitiveClauses.push_back(transitiveClause);
                    }
                }
            }
        }
    }

    // Add a percentage of the found transitive clauses
    size_t numToAdd = std::min(
        static_cast<size_t>(formula.size() * percentage),
        transitiveClauses.size());

    // Use a random number generator to select which ones to add
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(transitiveClauses.begin(), transitiveClauses.end(), gen);

    for (size_t i = 0; i < numToAdd; i++)
    {
        formula.push_back(transitiveClauses[i]);
    }
}

// Add resolution-based clauses - if (a v b) and (~a v c), add (b v c)
void addResolutionClauses(CNF &formula, double percentage)
{
    std::vector<Clause> resolvents;

    // Group clauses by literals to find potential resolution pairs
    std::map<int, std::vector<size_t>> literalToClauses;
    for (size_t i = 0; i < formula.size(); i++)
    {
        for (int lit : formula[i])
        {
            literalToClauses[lit].push_back(i);
        }
    }

    // Find resolution opportunities
    for (const auto &[lit, clauses] : literalToClauses)
    {
        int negLit = -lit;
        if (literalToClauses.find(negLit) != literalToClauses.end())
        {
            // We have clauses with lit and clauses with ~lit
            // For each pair, create a resolvent
            for (size_t idx1 : literalToClauses[lit])
            {
                for (size_t idx2 : literalToClauses[negLit])
                {
                    Clause resolvent;

                    // Add all literals from clause1 except lit
                    for (int l : formula[idx1])
                    {
                        if (l != lit)
                        {
                            resolvent.push_back(l);
                        }
                    }

                    // Add all literals from clause2 except ~lit
                    for (int l : formula[idx2])
                    {
                        if (l != negLit)
                        {
                            resolvent.push_back(l);
                        }
                    }

                    // Remove duplicates
                    std::sort(resolvent.begin(), resolvent.end());
                    resolvent.erase(std::unique(resolvent.begin(), resolvent.end()), resolvent.end());

                    // Check if resolvent contains a variable and its negation (tautology)
                    bool isTautology = false;
                    for (size_t i = 1; i < resolvent.size(); i++)
                    {
                        if (resolvent[i - 1] == -resolvent[i])
                        {
                            isTautology = true;
                            break;
                        }
                    }

                    if (!isTautology && !resolvent.empty())
                    {
                        resolvents.push_back(resolvent);
                    }
                }
            }
        }
    }

    // Add a percentage of the found resolvents
    size_t numToAdd = std::min(
        static_cast<size_t>(formula.size() * percentage),
        resolvents.size());

    // Use a random number generator to select which ones to add
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(resolvents.begin(), resolvents.end(), gen);

    for (size_t i = 0; i < numToAdd; i++)
    {
        formula.push_back(resolvents[i]);
    }
}

// Add redundancy to an existing CNF formula
CNF addRedundancy(const CNF &formula, RedundancyLevel level = RedundancyLevel::MODERATE)
{
    CNF result = formula;

    // Based on the redundancy level, add different types of redundant clauses
    switch (level)
    {
    case RedundancyLevel::LOW:
        // Add a few duplicate clauses
        addDuplicateClauses(result, 0.1); // Add 10% duplicates
        break;

    case RedundancyLevel::MODERATE:
        // Add duplicates and subsumed clauses
        addDuplicateClauses(result, 0.2); // Add 20% duplicates
        addSubsumedClauses(result, 0.15); // Add 15% subsumed clauses
        break;

    case RedundancyLevel::HIGH:
        // Add several types of redundancy
        addDuplicateClauses(result, 0.3);    // Add 30% duplicates
        addSubsumedClauses(result, 0.25);    // Add 25% subsumed clauses
        addTautologicalClauses(result, 0.1); // Add 10% tautologies
        addTransitiveClauses(result, 0.2);   // Add 20% transitive clauses
        break;
    }

    return result;
}

void testWithRedundancy(const std::string &problem_name, const CNF &formula)
{
    std::cout << "\n===== Testing " << problem_name << " with Redundancy =====\n";

    // Regular formula with preprocessing
    std::cout << "\n--- Original Formula with Preprocessing ---\n";
    bool result1 = runWithPreprocessing(problem_name + " (Original)", formula, true);

    // Add redundancy
    CNF redundantFormula = addRedundancy(formula, RedundancyLevel::MODERATE);
    std::cout << "\n--- Redundant Formula without Preprocessing ---\n";
    bool result2 = runWithPreprocessing(problem_name + " (Redundant)", redundantFormula, false);

    // Redundant formula with preprocessing
    std::cout << "\n--- Redundant Formula with Preprocessing ---\n";
    bool result3 = runWithPreprocessing(problem_name + " (Redundant)", redundantFormula, true);

    // Compare results
    std::cout << "\n--- COMPARISON ---\n";
    std::cout << "Results match across all tests: "
              << (result1 == result2 && result2 == result3 ? "YES" : "NO") << "\n";
}

// Generate N-Queens Problem CNF
CNF generateNQueensCNF(int N, bool debug = false)
{
    CNF queensCNF;
    const int base = 1; // Starting variable index

    if (debug)
    {
        std::cout << "Generating " << N << "-Queens CNF...\n";
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
        queensCNF.push_back(atLeastOneInRow);
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
                queensCNF.push_back({-var1, -var2});
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
        queensCNF.push_back(atLeastOneInCol);
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
                queensCNF.push_back({-var1, -var2});
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
                queensCNF.push_back({-vars_on_diagonal[i], -vars_on_diagonal[j]});
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
                queensCNF.push_back({-vars_on_diagonal[i], -vars_on_diagonal[j]});
            }
        }
    }

    if (debug)
    {
        std::cout << "Total clauses generated: " << queensCNF.size() << "\n\n";
    }
    return queensCNF;
}

// Generate Hamiltonian Path CNF
CNF generateHamiltonianPath(int numVertices, bool cyclic = false)
{
    CNF formula;
    int baseVar = 1;

    // Use variables x_i,j to represent "vertex i is at position j in the path"
    // Variable mapping: var = baseVar + i * numVertices + j

    // Each position in the path must have exactly one vertex
    for (int pos = 0; pos < numVertices; pos++)
    {
        // At least one vertex in this position
        Clause atLeastOne;
        for (int v = 0; v < numVertices; v++)
        {
            int var = baseVar + v * numVertices + pos;
            atLeastOne.push_back(var);
        }
        formula.push_back(atLeastOne);

        // At most one vertex in this position (pairwise constraints)
        for (int v1 = 0; v1 < numVertices; v1++)
        {
            for (int v2 = v1 + 1; v2 < numVertices; v2++)
            {
                int var1 = baseVar + v1 * numVertices + pos;
                int var2 = baseVar + v2 * numVertices + pos;
                formula.push_back({-var1, -var2});
            }
        }
    }

    // Each vertex must appear exactly once in the path
    for (int v = 0; v < numVertices; v++)
    {
        // At least one position for this vertex
        Clause atLeastOne;
        for (int pos = 0; pos < numVertices; pos++)
        {
            int var = baseVar + v * numVertices + pos;
            atLeastOne.push_back(var);
        }
        formula.push_back(atLeastOne);

        // At most one position for this vertex (pairwise constraints)
        for (int pos1 = 0; pos1 < numVertices; pos1++)
        {
            for (int pos2 = pos1 + 1; pos2 < numVertices; pos2++)
            {
                int var1 = baseVar + v * numVertices + pos1;
                int var2 = baseVar + v * numVertices + pos2;
                formula.push_back({-var1, -var2});
            }
        }
    }

    // Adjacency constraints - only adjacent vertices can be consecutive in path
    // Assume a complete graph for simplicity, but mark some edges as non-existent
    std::vector<std::vector<bool>> adjacency(numVertices, std::vector<bool>(numVertices, true));

    // Remove some edges to make the problem interesting
    // For a simple pattern, we'll remove edges between vertices whose indices differ by more than numVertices/2
    for (int v1 = 0; v1 < numVertices; v1++)
    {
        for (int v2 = 0; v2 < numVertices; v2++)
        {
            if (v1 == v2)
            {
                adjacency[v1][v2] = false; // No self-loops
            }
            else if (std::abs(v1 - v2) > numVertices / 2)
            {
                adjacency[v1][v2] = false; // Remove some edges
            }
        }
    }

    // Add constraints for adjacent positions in the path
    for (int pos = 0; pos < numVertices - 1; pos++)
    {
        for (int v1 = 0; v1 < numVertices; v1++)
        {
            for (int v2 = 0; v2 < numVertices; v2++)
            {
                if (!adjacency[v1][v2])
                {
                    // If vertices v1 and v2 are not adjacent in the graph,
                    // they cannot be adjacent in the path
                    int var1 = baseVar + v1 * numVertices + pos;
                    int var2 = baseVar + v2 * numVertices + (pos + 1);
                    formula.push_back({-var1, -var2});
                }
            }
        }
    }

    // For cyclic Hamiltonian (Hamiltonian cycle), add constraints between last and first position
    if (cyclic)
    {
        for (int v1 = 0; v1 < numVertices; v1++)
        {
            for (int v2 = 0; v2 < numVertices; v2++)
            {
                if (!adjacency[v1][v2])
                {
                    // If vertices v1 and v2 are not adjacent in the graph,
                    // they cannot be at the last and first positions
                    int var1 = baseVar + v1 * numVertices + (numVertices - 1);
                    int var2 = baseVar + v2 * numVertices + 0;
                    formula.push_back({-var1, -var2});
                }
            }
        }
    }

    return formula;
}

// Generate Graph Coloring CNF
CNF generateGraphColoring(int numVertices, int numColors, double edgeDensity = 0.5, int seed = 42)
{
    CNF formula;

    // Initialize random generator for edge creation
    std::mt19937 gen(seed);
    std::uniform_real_distribution<> edge_dist(0.0, 1.0);

    // Create adjacency matrix for the graph
    std::vector<std::vector<bool>> adjacency(numVertices, std::vector<bool>(numVertices, false));

    // Generate random edges based on edge density
    for (int v1 = 0; v1 < numVertices; v1++)
    {
        for (int v2 = v1 + 1; v2 < numVertices; v2++)
        {
            if (edge_dist(gen) < edgeDensity)
            {
                adjacency[v1][v2] = true;
                adjacency[v2][v1] = true;
            }
        }
    }

    // Variable mapping: Use consecutive numbers
    // var(v,c) = v * numColors + c + 1

    // Each vertex must have at least one color
    for (int v = 0; v < numVertices; v++)
    {
        Clause atLeastOneColor;
        for (int c = 0; c < numColors; c++)
        {
            int var = v * numColors + c + 1;
            atLeastOneColor.push_back(var);
        }
        formula.push_back(atLeastOneColor);
    }

    // Each vertex must have at most one color (pairwise constraints)
    for (int v = 0; v < numVertices; v++)
    {
        for (int c1 = 0; c1 < numColors; c1++)
        {
            for (int c2 = c1 + 1; c2 < numColors; c2++)
            {
                int var1 = v * numColors + c1 + 1;
                int var2 = v * numColors + c2 + 1;
                formula.push_back({-var1, -var2});
            }
        }
    }

    // Adjacent vertices must have different colors
    for (int v1 = 0; v1 < numVertices; v1++)
    {
        for (int v2 = v1 + 1; v2 < numVertices; v2++)
        { // Only consider v2 > v1
            if (adjacency[v1][v2])
            {
                // For each color, both vertices cannot have the same color
                for (int c = 0; c < numColors; c++)
                {
                    int var1 = v1 * numColors + c + 1;
                    int var2 = v2 * numColors + c + 1;
                    formula.push_back({-var1, -var2});
                }
            }
        }
    }

    return formula;
}

// Generate harder Pigeonhole instance - m pigeons, n holes (UNSAT if m > n)
CNF generateHardPigeonholeCNF(int numPigeons, int numHoles)
{
    CNF pigeonholeCNF;
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

// Compare with and without preprocessing
void compareWithAndWithoutPreprocessing(const std::string &problem_name, const CNF &formula)
{
    std::cout << "\n===== Comparing With and Without Preprocessing: " << problem_name << " =====\n";

    // Run with preprocessing
    std::cout << "\n--- WITH PREPROCESSING ---\n";
    auto start_with = std::chrono::high_resolution_clock::now();
    bool result_with = runWithPreprocessing(problem_name, formula, true);
    auto end_with = std::chrono::high_resolution_clock::now();
    auto time_with = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_with - start_with);

    // Run without preprocessing
    std::cout << "\n--- WITHOUT PREPROCESSING ---\n";
    auto start_without = std::chrono::high_resolution_clock::now();
    bool result_without = runWithPreprocessing(problem_name, formula, false);
    auto end_without = std::chrono::high_resolution_clock::now();
    auto time_without = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_without - start_without);

    // Compare results
    std::cout << "\n--- COMPARISON ---\n";
    std::cout << "Results match: " << (result_with == result_without ? "YES" : "NO") << "\n";
    std::cout << "Total time with preprocessing: " << time_with.count() << " ms\n";
    std::cout << "Total time without preprocessing: " << time_without.count() << " ms\n";

    if (time_without.count() > 0)
    {
        double speedup = static_cast<double>(time_without.count()) / time_with.count();
        std::cout << "Speedup with preprocessing: " << std::fixed << std::setprecision(2) << speedup << "x\n";
    }
}

int main(int argc, char *argv[])
{
    std::cout << "SAT Solver Preprocessor Test Harness\n";
    std::cout << "===================================\n\n";

    // Process command line arguments
    bool run_nqueens = true;
    bool run_pigeonhole = true;
    bool run_hamiltonian = true;
    int queens_size = 8;              // Reduced size for faster testing with redundancy
    bool test_with_redundancy = true; // New flag

    if (argc > 1)
    {
        std::string arg = argv[1];
        if (arg == "nqueens")
        {
            run_pigeonhole = false;
            run_hamiltonian = false;
            if (argc > 2)
            {
                queens_size = std::stoi(argv[2]);
            }
        }
        else if (arg == "pigeonhole")
        {
            run_nqueens = false;
            run_hamiltonian = false;
        }
        else if (arg == "hamiltonian")
        {
            run_nqueens = false;
            run_pigeonhole = false;
        }
        else if (arg == "noredundancy")
        {
            test_with_redundancy = false;
        }
    }

    // Test 1: N-Queens Problems
    if (run_nqueens)
    {
        std::cout << "\n----- Testing N-Queens Problem -----\n";

        // Generate N-Queens CNF
        CNF queensCNF = generateNQueensCNF(queens_size);

        // First detect the problem type with original formula
        PreprocessorConfig config;
        Preprocessor detector(config);
        ProblemType queens_type = detector.detectProblemType(queensCNF);

        // Compare with and without preprocessing
        compareWithAndWithoutPreprocessing(std::to_string(queens_size) + "-Queens Problem", queensCNF);

        // Test with redundancy if enabled
        if (test_with_redundancy)
        {
            // Create redundant version with moderate redundancy
            CNF redundantQueensCNF = addRedundancy(queensCNF, RedundancyLevel::MODERATE);

            std::cout << "\n===== Testing " << queens_size << "-Queens Problem with Redundancy =====\n";
            std::cout << "Original clauses: " << queensCNF.size()
                      << ", Redundant clauses: " << redundantQueensCNF.size() << "\n";

            // Test redundant formula with preprocessing - passing the original problem type
            std::cout << "\n--- Redundant Formula with Preprocessing ---\n";
            bool result_with = runWithPreprocessing("Redundant " + std::to_string(queens_size) +
                                                        "-Queens Problem",
                                                    redundantQueensCNF, true, queens_type);

            // Test redundant formula without preprocessing
            std::cout << "\n--- Redundant Formula without Preprocessing ---\n";
            bool result_without = runWithPreprocessing("Redundant " + std::to_string(queens_size) +
                                                           "-Queens Problem",
                                                       redundantQueensCNF, false);

            // Compare the results
            std::cout << "\n--- COMPARISON ---\n";
            std::cout << "Results match: " << (result_with == result_without ? "YES" : "NO") << "\n";

            // Calculate preprocessing effect
            auto start_with = std::chrono::high_resolution_clock::now();
            bool _ = runWithPreprocessing("", redundantQueensCNF, true, queens_type);
            auto end_with = std::chrono::high_resolution_clock::now();
            auto time_with = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_with - start_with);

            auto start_without = std::chrono::high_resolution_clock::now();
            _ = runWithPreprocessing("", redundantQueensCNF, false);
            auto end_without = std::chrono::high_resolution_clock::now();
            auto time_without = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_without - start_without);

            std::cout << "Total time with preprocessing: " << time_with.count() << " ms\n";
            std::cout << "Total time without preprocessing: " << time_without.count() << " ms\n";

            if (time_without.count() > 0)
            {
                double speedup = static_cast<double>(time_without.count()) / time_with.count();
                std::cout << "Speedup with preprocessing: " << std::fixed << std::setprecision(2)
                          << speedup << "x\n";
            }
        }
    }

    // Test 2: Pigeonhole Problems (m pigeons, n holes)
    if (run_pigeonhole)
    {
        std::cout << "\n----- Testing Pigeonhole Problems -----\n";

        int m = 6;
        // UNSAT: m+1 pigeons, m holes
        CNF unsatPigeonholeCNF = generateHardPigeonholeCNF(m + 1, m);

        // First detect the problem type with original formula
        PreprocessorConfig config;
        Preprocessor detector(config);
        ProblemType pigeon_type = detector.detectProblemType(unsatPigeonholeCNF);

        compareWithAndWithoutPreprocessing("Pigeonhole Problem (UNSAT: " + std::to_string(m + 1) +
                                               " pigeons, " + std::to_string(m) + " holes)",
                                           unsatPigeonholeCNF);

        // Test with redundancy if enabled
        if (test_with_redundancy)
        {
            // Create redundant version with high redundancy
            CNF redundantPigeonholeCNF = addRedundancy(unsatPigeonholeCNF, RedundancyLevel::HIGH);

            std::cout << "\n===== Testing Pigeonhole Problem with Redundancy =====\n";
            std::cout << "Original clauses: " << unsatPigeonholeCNF.size()
                      << ", Redundant clauses: " << redundantPigeonholeCNF.size() << "\n";

            // Test redundant formula with preprocessing - passing the original problem type
            std::cout << "\n--- Redundant Formula with Preprocessing ---\n";
            bool result_with = runWithPreprocessing("Redundant Pigeonhole Problem",
                                                    redundantPigeonholeCNF, true, pigeon_type);

            // Test redundant formula without preprocessing
            std::cout << "\n--- Redundant Formula without Preprocessing ---\n";
            bool result_without = runWithPreprocessing("Redundant Pigeonhole Problem",
                                                       redundantPigeonholeCNF, false);

            // Compare the results
            std::cout << "\n--- COMPARISON ---\n";
            std::cout << "Results match: " << (result_with == result_without ? "YES" : "NO") << "\n";

            // Calculate preprocessing effect
            auto start_with = std::chrono::high_resolution_clock::now();
            bool _ = runWithPreprocessing("", redundantPigeonholeCNF, true, pigeon_type);
            auto end_with = std::chrono::high_resolution_clock::now();
            auto time_with = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_with - start_with);

            auto start_without = std::chrono::high_resolution_clock::now();
            _ = runWithPreprocessing("", redundantPigeonholeCNF, false);
            auto end_without = std::chrono::high_resolution_clock::now();
            auto time_without = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_without - start_without);

            std::cout << "Total time with preprocessing: " << time_with.count() << " ms\n";
            std::cout << "Total time without preprocessing: " << time_without.count() << " ms\n";

            if (time_without.count() > 0)
            {
                double speedup = static_cast<double>(time_without.count()) / time_with.count();
                std::cout << "Speedup with preprocessing: " << std::fixed << std::setprecision(2)
                          << speedup << "x\n";
            }
        }
    }

    // Test 3: Hamiltonian Path Problems
    if (run_hamiltonian)
    {
        std::cout << "\n----- Testing Hamiltonian Path Problems -----\n";

        int numVertices = 10;
        // Generate Hamiltonian Path problem
        CNF hamiltonianCNF = generateHamiltonianPath(numVertices);

        // First detect the problem type with original formula
        PreprocessorConfig config;
        Preprocessor detector(config);
        ProblemType hamilton_type = detector.detectProblemType(hamiltonianCNF);

        compareWithAndWithoutPreprocessing("Hamiltonian Path Problem (" +
                                               std::to_string(numVertices) + " vertices)",
                                           hamiltonianCNF);

        // Generate Hamiltonian Cycle problem
        CNF hamiltonianCycleCNF = generateHamiltonianPath(numVertices, true);
        compareWithAndWithoutPreprocessing("Hamiltonian Cycle Problem (" +
                                               std::to_string(numVertices) + " vertices)",
                                           hamiltonianCycleCNF);

        // Test with redundancy if enabled
        if (test_with_redundancy)
        {
            // Create redundant version with moderate redundancy
            CNF redundantHamiltonianCNF = addRedundancy(hamiltonianCNF, RedundancyLevel::MODERATE);

            std::cout << "\n===== Testing Hamiltonian Path Problem with Redundancy =====\n";
            std::cout << "Original clauses: " << hamiltonianCNF.size()
                      << ", Redundant clauses: " << redundantHamiltonianCNF.size() << "\n";

            // Test redundant formula with preprocessing - passing the original problem type
            std::cout << "\n--- Redundant Formula with Preprocessing ---\n";
            bool result_with = runWithPreprocessing("Redundant Hamiltonian Path Problem",
                                                    redundantHamiltonianCNF, true, hamilton_type);

            // Test redundant formula without preprocessing
            std::cout << "\n--- Redundant Formula without Preprocessing ---\n";
            bool result_without = runWithPreprocessing("Redundant Hamiltonian Path Problem",
                                                       redundantHamiltonianCNF, false);

            // Compare the results
            std::cout << "\n--- COMPARISON ---\n";
            std::cout << "Results match: " << (result_with == result_without ? "YES" : "NO") << "\n";

            // Calculate preprocessing effect
            auto start_with = std::chrono::high_resolution_clock::now();
            bool _ = runWithPreprocessing("", redundantHamiltonianCNF, true, hamilton_type);
            auto end_with = std::chrono::high_resolution_clock::now();
            auto time_with = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_with - start_with);

            auto start_without = std::chrono::high_resolution_clock::now();
            _ = runWithPreprocessing("", redundantHamiltonianCNF, false);
            auto end_without = std::chrono::high_resolution_clock::now();
            auto time_without = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_without - start_without);

            std::cout << "Total time with preprocessing: " << time_with.count() << " ms\n";
            std::cout << "Total time without preprocessing: " << time_without.count() << " ms\n";

            if (time_without.count() > 0)
            {
                double speedup = static_cast<double>(time_without.count()) / time_with.count();
                std::cout << "Speedup with preprocessing: " << std::fixed << std::setprecision(2)
                          << speedup << "x\n";
            }
        }
    }

    return 0;
}