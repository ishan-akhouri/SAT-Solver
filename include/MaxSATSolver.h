#ifndef MAXSAT_SOLVER_H
#define MAXSAT_SOLVER_H

#include "CDCLSolverIncremental.h"
#include <vector>
#include <unordered_map>

// Type definitions from original code
typedef std::vector<int> Clause;
typedef std::vector<Clause> CNF;

class MaxSATSolver
{
public:
    MaxSATSolver(const CNF &hard_clauses, bool debug = false);

    void addSoftClause(const Clause &soft_clause, int weight = 1);
    void addSoftClauses(const CNF &soft_clauses, int weight = 1);

    int solve();
    int solveBinarySearch();

    std::unordered_map<int, bool> getAssignment() const;

    int getNumHardClauses() const;
    int getNumSoftClauses() const;
    int getNumVariables() const;
    int getNumSolverCalls() const;

    void setPreviousSolution(const std::unordered_map<int, bool> &solution);

private:
    std::vector<int> createAssumptions(int k);
    bool solveWithKRelaxed(int k, std::vector<int> &assumptions);

    CDCLSolverIncremental solver;
    std::vector<int> relaxation_vars;
    std::vector<int> weights;
    int next_var;
    bool debug_output;
    int solver_calls;

    // New variables for warm starting
    std::unordered_map<int, bool> last_solution;
    bool has_previous_solution;
};

#endif // MAXSAT_SOLVER_H