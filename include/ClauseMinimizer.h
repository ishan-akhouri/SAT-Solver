#ifndef CLAUSE_MINIMIZER_H
#define CLAUSE_MINIMIZER_H

#include "SATInstance.h"
#include "ClauseDatabase.h"
#include "CDCLSolverIncremental.h" // Include this to get ImplicationNodeIncremental definition
#include <vector>
#include <unordered_map>
#include <unordered_set>

// Class for clause minimization techniques
class ClauseMinimizer {
private:
    // Reference to trail and assignments (from solver)
    const std::vector<ImplicationNodeIncremental>& trail;
    const std::unordered_map<int, bool>& assignments;
    const std::unordered_map<int, size_t>& var_to_trail;
    const std::vector<int>& decision_levels;
    
    // Reference to clause database
    ClauseDatabase& db;
    
    // Working data structures
    std::unordered_set<int> seen;
    std::vector<int> stack;
    
    // Settings
    bool use_binary_resolution;
    bool use_vivification;
    bool debug_output;
    
public:
    ClauseMinimizer(
        const std::vector<ImplicationNodeIncremental>& trail_ref,
        const std::unordered_map<int, bool>& assignments_ref,
        const std::unordered_map<int, size_t>& var_to_trail_ref,
        const std::vector<int>& decision_levels_ref,
        ClauseDatabase& db_ref,
        bool debug = false);
    
    // Main minimization methods
    void minimizeConflictClause(Clause& clause);
    void minimizeLearnedClauses();
    
    // Configuration
    void setUseBinaryResolution(bool use) { use_binary_resolution = use; }
    void setUseVivification(bool use) { use_vivification = use; }
    
private:
    // Minimization techniques
    bool recursiveMinimize(int lit, std::unordered_set<int>& seen, int depth = 0);
    void selfSubsumption(Clause& clause);
    void binaryResolution(Clause& clause);
    void vivification(Clause& clause);
    
    // Utility methods
    bool isRedundant(int lit);
    bool isImplied(int lit, const Clause& by_clause);
    void printClause(const Clause& clause) const;
};

#endif // CLAUSE_MINIMIZER_H