#ifndef CDCL_SOLVER_INCREMENTAL_H
#define CDCL_SOLVER_INCREMENTAL_H

#include "SATInstance.h"
#include "ClauseDatabase.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <chrono>

// Structure to represent a node in the implication graph for incremental CDCL
struct ImplicationNodeIncremental
{
    int literal;          // The literal that was assigned
    int decision_level;   // Decision level when this assignment was made
    size_t antecedent_id; // ID of the clause that caused this implication
    bool is_decision;     // Whether this was a decision variable

    ImplicationNodeIncremental(int lit, int level, size_t ante_id, bool decision = false)
        : literal(lit), decision_level(level), antecedent_id(ante_id), is_decision(decision) {}
};

// Forward declare the ClauseMinimizer class to avoid circular dependencies
class ClauseMinimizer;

// Forward declaration
class PortfolioManager;

// Class for incremental CDCL solving with clause database management
class CDCLSolverIncremental
{
private:
    // Core components
    std::unique_ptr<ClauseDatabase> db;            // Clause database
    std::unordered_map<int, bool> assignments;     // Variable assignments
    std::vector<ImplicationNodeIncremental> trail; // Decision/propagation trail
    std::unordered_map<int, size_t> var_to_trail;  // Maps variables to positions in the trail

    // Current state
    int decision_level;
    std::vector<int> decision_levels; // Maps variables to their decision levels

    // VSIDS variable selection
    std::unordered_map<int, double> activity; // Activity score for each variable
    double var_inc;                           // Value to increase activity by
    double var_decay;                         // Decay factor for VSIDS

    // Restart strategy
    int conflicts_since_restart;
    int restart_threshold;
    double restart_multiplier;
    int luby_index;
    bool use_luby_restarts;

    // Incremental solving
    std::vector<int> assumptions; // Current assumptions
    std::vector<int> core;        // Unsatisfiable core
    int last_solved_until;        // Index up to which the formula was solved

    // Statistics
    int conflicts;
    int decisions;
    int propagations;
    int restarts;
    int max_decision_level;

    // Settings
    bool use_lbd;          // Use LBD for clause quality assessment
    bool use_phase_saving; // Use phase saving for decisions
    bool debug_output;

    ClauseID conflict_clause_id; // ID of the last conflicting clause

    // Optional clause minimizer
    std::unique_ptr<ClauseMinimizer> minimizer;

    // Timeout related members
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::milliseconds timeout_duration;

    int stuck_counter; // Counter for detecting when solver is stuck

    // Pointer to portfolio manager
    PortfolioManager *portfolio_manager;

public:
    // Make ClauseMinimizer a friend to access private members
    friend class ClauseMinimizer;

    // Constructor from a CNF formula
    CDCLSolverIncremental(const CNF &formula, bool debug = false, PortfolioManager *portfolio = nullptr);

    // Destructor
    ~CDCLSolverIncremental();

    // Core solving methods
    bool solve();                                    // Solve the current formula
    bool solve(const std::vector<int> &assumptions); // Solve with assumptions

    // Incremental interface
    void addClause(const Clause &clause);                     // Add a permanent clause
    void addTemporaryClause(const Clause &clause);            // Add a clause valid only for next solve
    void setAssumptions(const std::vector<int> &assumptions); // Set assumptions for next solve
    void addAssumption(int literal);                          // Add a single assumption
    void clearAssumptions();                                  // Clear all assumptions
    std::vector<int> getUnsatCore() const;                    // Get the UNSAT core from the last solve

    // Variable management
    void setDecisionPolarity(int var, bool phase);           // Force initial polarity
    void setRandomizedPolarities(double random_freq = 0.02); // Randomize some polarities

    // Control methods
    void setMaxLearnts(size_t max_learnts);                     // Set maximum learned clauses
    void setVarDecay(double decay);                             // Set VSIDS decay factor
    void setRestartStrategy(bool use_luby, int init_threshold); // Configure restarts

    // Accessors
    const std::unordered_map<int, bool> &getAssignments() const { return assignments; }
    int getConflicts() const { return conflicts; }
    int getDecisions() const { return decisions; }
    int getPropagations() const { return propagations; }
    int getRestarts() const { return restarts; }
    int getMaxDecisionLevel() const { return max_decision_level; }
    int getNumVars() const;
    int getNumClauses() const;
    int getNumLearnts() const;

    // New variable, used for incremental solving specifically graph coloring
    int newVariable();

    // Timeout related methods
    bool checkTimeout();

private:
    // Internal solving methods
    bool unitPropagate();                                              // Propagate unit clauses
    int analyzeConflict(ClauseID conflict_id, Clause &learned_clause); // Analyze conflict
    void backtrack(int level);                                         // Backtrack to a specific decision level
    bool makeDecision();                                               // Make a new decision
    bool isSatisfied() const;                                          // Check if formula is satisfied

    // VSIDS helpers
    void initializeVSIDS();        // Initialize VSIDS scores
    int selectVarVSIDS();          // Select next variable by VSIDS
    void bumpVarActivity(int var); // Bump variable activity
    void decayVarActivities();     // Decay variable activities

    // Clause database helpers
    void minimizeClause(Clause &clause); // Minimize learned clause
    bool isRedundant(int lit, std::unordered_set<int> &seen, int &timeout_check_counter,
        const std::chrono::time_point<std::chrono::high_resolution_clock>& start_time,
        const std::chrono::milliseconds& max_time);

    // Restart strategy
    bool shouldRestart();    // Check if we should restart
    void restart();          // Perform a restart
    int lubySequence(int i); // Compute the Luby sequence

    // Debug helpers
    void printTrail() const;                      // Print current trail
    void printClause(const Clause &clause) const; // Print a clause
    void printStatistics() const;                 // Print solver statistics

   
};

#endif