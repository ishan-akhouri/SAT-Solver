// WeightedMaxSATSolver.h
#ifndef WEIGHTED_MAXSAT_SOLVER_H
#define WEIGHTED_MAXSAT_SOLVER_H

#include <vector>
#include <unordered_map>
#include <utility>
#include "MaxSATSolver.h"

class WeightedMaxSATSolver {
private:
    CNF hard_clauses;
    std::vector<Clause> soft_clauses;
    std::vector<int> weights;
    bool debug_output;
    int solver_calls;

    // Check if formula is satisfiable with at most weight_limit total weight violated
    bool checkWeightLimit(int weight_limit);

public:
    WeightedMaxSATSolver(const CNF& hard_clauses, bool debug = false);
    
    // Add a soft clause with a weight
    void addSoftClause(const Clause& soft_clause, int weight = 1);
    
    // Add multiple soft clauses with the same weight
    void addSoftClauses(const CNF& clauses, int weight = 1);
    
    // Stratified approach: solve by groups of clauses with same weight
    int solveStratified();
    
    // Binary search over weight space
    int solveBinarySearch();
    
    // Get number of solver calls
    int getNumSolverCalls() const;
};

#endif // WEIGHTED_MAXSAT_SOLVER_H