#ifndef MAXSAT_SOLVER_H
#define MAXSAT_SOLVER_H

#include <vector>
#include <unordered_map>
#include "CDCLSolverIncremental.h"
#include "SATInstance.h"

class MaxSATSolver {
private:
    CDCLSolverIncremental solver;
    std::vector<int> relaxation_vars;
    std::vector<int> weights;  // For weighted MaxSAT
    int next_var;
    bool debug_output;

public:
    MaxSATSolver(const CNF& hard_clauses, bool debug = false);
    
    // Add a soft clause with optional weight
    void addSoftClause(const Clause& soft_clause, int weight = 1);
    
    // Add multiple soft clauses
    void addSoftClauses(const CNF& soft_clauses, int weight = 1);
    
    // Linear search algorithm for MaxSAT
    int solve();
    
    // Binary search algorithm for MaxSAT (more efficient)
    int solveBinarySearch();
    
    // Get the satisfying assignment
    std::unordered_map<int, bool> getAssignment() const;
    
    // Get statistics
    int getNumHardClauses() const;
    int getNumSoftClauses() const;
    int getNumVariables() const;
    int getNumSolverCalls() const;
    
private:
    int solver_calls;  // Counter for solver invocations
    
    // Helper method to create assumptions
    std::vector<int> createAssumptions(int k);
    
    // Helper method to solve with k relaxed clauses
    bool solveWithKRelaxed(int k, std::vector<int>& assumptions);
};

#endif