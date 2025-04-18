#ifndef CLAUSE_DATABASE_H
#define CLAUSE_DATABASE_H

#include "SATInstance.h"
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <memory>

// Forward declaration
class CDCLSolverIncremental;

// Enhanced representation of a clause with activity and other metadata
class ClauseInfo
{
public:
    Clause literals; // The literals in the clause
    bool is_learned; // Whether this is a learned clause
    bool is_core;    // Whether this is part of the original problem (core)
    size_t activity; // Activity counter for clause deletion heuristics
    int lbd;         // Literal Block Distance (for clause quality assessment)

    // Optional watched literals (stored here for cache efficiency)
    std::pair<int, int> watched_lits;

    ClauseInfo(const Clause &lits, bool learned = false, bool core = true);

    // Utility functions
    size_t size() const { return literals.size(); }
    bool empty() const { return literals.empty(); }
    int &operator[](size_t idx) { return literals[idx]; }
    const int &operator[](size_t idx) const { return literals[idx]; }
};

using ClauseID = size_t;                       // ID for a clause in the database
using ClauseRef = std::shared_ptr<ClauseInfo>; // Reference-counted clause

// Manages the clauses in a SAT solver efficiently
class ClauseDatabase
{
private:
    friend class CDCLSolverIncremental; // Allow the solver direct access
    friend class ClauseMinimizer;       // Allow the minimizer direct access

    std::vector<ClauseRef> clauses;             // All clauses
    std::vector<ClauseRef> learned_clauses;     // Learned clauses for more efficient access
    std::vector<std::vector<ClauseID>> watches; // Watched literals data structure

    // Activity management
    size_t clause_activity_inc;
    double clause_decay_factor;

    // Statistics
    size_t num_variables;    // Number of variables
    size_t original_clauses; // Number of original clauses
    size_t total_learned;    // Total number of learned clauses over time
    size_t active_learned;   // Currently active learned clauses
    size_t deleted_learned;  // Number of deleted learned clauses

    // Settings
    size_t max_learnt_clauses;        // Maximum number of learned clauses before deletion
    double clause_deletion_threshold; // LBD threshold for deletion
    bool allow_clause_deletion;       // Whether to allow clause deletion (disable for debugging)

    // Debug control
    bool debug_output;

    size_t current_memory_usage;
    static constexpr size_t MAX_MEMORY_MB = 1024; // 1GB limit

    size_t calculateMemoryUsage() const;
    void updateMemoryUsage();

public:
    // Constructor
    ClauseDatabase(size_t num_vars, bool debug = false);

    // Core database operations
    ClauseID addClause(const Clause &clause, bool is_learned = false);
    ClauseID addLearnedClause(const Clause &clause, int lbd);
    void removeClause(ClauseID id);

    // Watches management
    void initWatches();
    void updateWatches(ClauseID id, int old_lit, int new_lit);
    const std::vector<ClauseID> &getWatches(int literal) const;

    // Clause management
    void bumpClauseActivity(ClauseID id);
    void decayClauseActivities();
    void garbageCollect(const std::unordered_map<int, bool> &assignments);
    size_t reduceLearnedClauses(const std::unordered_map<int, bool> &assignments);

    // LBD computation
    int computeLBD(const Clause &clause, const std::vector<int> &levels);

    // Incremental solving support
    void addAssumption(int literal);
    void clearAssumptions();
    std::vector<int> extractCoreAssumptions(const Clause &conflict);

    // Accessors
    const ClauseRef &getClause(ClauseID id) const;
    size_t getNumClauses() const;
    size_t getNumLearnedClauses() const;
    size_t getNumVariables() const;

    // Add variable
    int addVariable();

    // Utilities
    void printStatistics() const;
    void printWatches() const;
    bool checkWatchesConsistency() const;

    // Clear all learned clauses and reset the database
    void clearLearnedClauses()
    {
        // Keep only original clauses
        size_t num_original = 0;
        for (size_t i = 0; i < clauses.size(); i++)
        {
            if (clauses[i] && clauses[i]->is_core)
            {
                num_original++;
            }
        }

        // Resize to keep only original clauses
        clauses.resize(num_original);

        // Clear learned clauses vector
        learned_clauses.clear();

        // Reset watches
        watches.clear();
        watches.resize(2 * num_variables + 1); // +1 for index 0, *2 for positive and negative literals

        // Reset statistics
        total_learned = 0;
        active_learned = 0;
        deleted_learned = 0;
        original_clauses = num_original;

        // Reset activity management
        clause_activity_inc = 1;

        if (debug_output)
        {
            std::cout << "Cleared learned clauses. Database now has " << num_original << " original clauses.\n";
        }
    }
};

#endif // CLAUSE_DATABASE_H