#ifndef HYBRID_MAXSAT_SOLVER_H
#define HYBRID_MAXSAT_SOLVER_H

#include "MaxSATSolver.h"
#include "WeightedMaxSATSolver.h"

class HybridMaxSATSolver
{
public:
    // Configuration options
    struct Config
    {
        bool use_warm_start = true;        // Whether to use warm starting
        bool use_exponential_probe = true; // Whether to use exponential probing for binary search
        int prob_size_threshold = 100;     // Problem size threshold for algorithm selection
        bool force_stratified = false;     // Force stratified approach for all weighted problems
        bool force_binary = false;         // Force binary search for all problems
    };

    HybridMaxSATSolver(const CNF &hard_clauses, bool debug = false);

    // Set configuration
    void setConfig(const Config &config);

    // Add clauses
    void addSoftClause(const Clause &soft_clause, int weight = 1);
    void addSoftClauses(const CNF &soft_clauses, int weight = 1);

    // Solve the problem - automatically selects the best approach
    int solve();

    // Explicitly use specific algorithms
    int solveLinear();
    int solveBinary();
    int solveStratified();

    // Get the solution
    std::unordered_map<int, bool> getAssignment() const;

    // Statistics
    int getNumHardClauses() const;
    int getNumSoftClauses() const;
    int getNumVariables() const;
    int getNumSolverCalls() const;

private:
    // Determine if the problem is weighted or unweighted
    bool isWeightedProblem() const;

    // Select the optimal algorithm based on problem characteristics
    enum class Algorithm
    {
        LINEAR,
        BINARY_SEARCH,
        STRATIFIED
    };

    Algorithm selectBestAlgorithm() const;

    CNF hard_clauses;
    CNF soft_clauses;
    std::vector<int> weights;
    bool debug_output;
    Config config;

    // Result tracking
    std::unordered_map<int, bool> last_assignment;
    int solver_calls;
};

#endif // HYBRID_MAXSAT_SOLVER_H