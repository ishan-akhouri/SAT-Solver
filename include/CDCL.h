#ifndef CDCL_H
#define CDCL_H

#include "SATInstance.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

// Represents a node in the implication graph
struct ImplicationNode {
    int literal;                // The literal that was assigned
    int decision_level;         // Decision level when this assignment was made
    std::vector<int> antecedent; // Clause that caused this implication (empty for decisions)
    bool is_decision;           // Whether this was a decision variable
    
    ImplicationNode(int lit, int level, const std::vector<int>& ante, bool decision = false) 
        : literal(lit), decision_level(level), antecedent(ante), is_decision(decision) {}
};

// This class extends SATInstance with CDCL-specific functionality
class CDCLSolver {
private:
    SATInstance instance;                 // The underlying SAT instance
    std::vector<ImplicationNode> trail;   // Decision/propagation trail (ordered)
    int decision_level;                   // Current decision level
    
    // Maps variables to their positions in the trail
    std::unordered_map<int, size_t> var_to_trail;
    
    // Watched literals data structures
    std::vector<std::vector<size_t>> watches; // Maps literals to clauses that watch them
    size_t num_variables;                     // Number of variables in the formula
    
    // Statistics
    int conflicts;
    int decisions;
    int propagations;
    int learned_clauses;
    int max_decision_level;
    int restarts;

    // Restart strategy parameters
    int restart_threshold;
    double restart_multiplier;
    
    // Settings
    bool debug_output;
    
public:
    CDCLSolver(const CNF& formula, bool debug = false);
    
    // Main solving function
    bool solve();
    
    // Get the final variable assignments
    std::unordered_map<int, bool> getAssignments() const { return instance.assignments; }
    
    // Get solver statistics
    int getConflicts() const { return conflicts; }
    int getDecisions() const { return decisions; }
    int getPropagations() const { return propagations; }
    int getLearnedClauses() const { return learned_clauses; }
    int getMaxDecisionLevel() const { return max_decision_level; }
    int getRestarts() const { return restarts; }
    
private:
    // Initialize watched literals
    void initWatchedLiterals();
    
    // Unit propagation with watched literals
    bool unitPropagate();
    
    // Analyze conflict and learn a new clause
    int analyzeConflict(const Clause& conflict_clause, Clause& learned_clause);
    
    // Backtrack to a specific decision level
    void backtrack(int level);
    
    // Make a new decision
    bool makeDecision();
    
    // Add a learned clause to the formula
    void addLearnedClause(const Clause& clause);
    
    // Check if the formula is satisfied
    bool isSatisfied() const;
    
    // Debug functions
    void printTrail() const;
    void printWatches() const;
    void printClause(const Clause& clause) const;
};

#endif