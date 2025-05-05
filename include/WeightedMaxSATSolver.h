#ifndef WEIGHTED_MAXSAT_SOLVER_H
#define WEIGHTED_MAXSAT_SOLVER_H

#include "MaxSATSolver.h"
#include <vector>
#include <unordered_map>

class WeightedMaxSATSolver
{
public:
    WeightedMaxSATSolver(const CNF &hard_clauses, bool debug = false);

    void addSoftClause(const Clause &soft_clause, int weight);
    void addSoftClauses(const CNF &clauses, int weight);

    int solveStratified();
    int solveBinarySearch();

    int getNumSolverCalls() const;

private:
    bool checkWeightLimit(int weight_limit);

    CNF hard_clauses;
    CNF soft_clauses;
    std::vector<int> weights;
    bool debug_output;
    int solver_calls;

    // New variables for warm starting
    std::unordered_map<int, bool> last_solution;
    bool has_previous_solution;
};

#endif // WEIGHTED_MAXSAT_SOLVER_H