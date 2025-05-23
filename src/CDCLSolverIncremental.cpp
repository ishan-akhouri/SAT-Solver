#include "../include/CDCLSolverIncremental.h"
#include "../include/ClauseMinimizer.h"
#include "../include/PortfolioManager.h"
#include <iostream>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <ctime>
#include <limits>
#include <random>

// Constructor from a CNF formula
CDCLSolverIncremental::CDCLSolverIncremental(const CNF &formula, bool debug, PortfolioManager *portfolio_manager)
    : decision_level(0),
      var_inc(1.0),
      var_decay(0.95),
      conflicts_since_restart(0),
      restart_threshold(100),
      restart_multiplier(1.5),
      luby_index(1),
      use_luby_restarts(true),
      last_solved_until(-1),
      conflicts(0),
      decisions(0),
      propagations(0),
      restarts(0),
      max_decision_level(0),
      use_lbd(true),
      use_phase_saving(true),
      debug_output(false),
      timeout_duration(std::chrono::milliseconds(30000)), // 30 second timeout
      stuck_counter(0),
      conflict_clause_id(0),
      portfolio_manager(portfolio_manager)
{ // Initialize stuck counter

    // Find the number of variables in the formula
    size_t num_vars = 0;
    for (const auto &clause : formula)
    {
        for (int literal : clause)
        {
            num_vars = std::max(num_vars, static_cast<size_t>(std::abs(literal)));
        }
    }

    // Initialize clause database
    db = std::make_unique<ClauseDatabase>(num_vars, debug);

    // Initialize decision level array (1-indexed for variables)
    decision_levels.resize(num_vars + 1, 0);

    // Add all clauses to the database
    for (const auto &clause : formula)
    {
        db->addClause(clause);
    }

    // Initialize VSIDS scores
    initializeVSIDS();

    if (debug_output)
    {
        std::cout << "CDCLSolverIncremental initialized with " << num_vars << " variables and "
                  << formula.size() << " clauses.\n";
    }
}

CDCLSolverIncremental::~CDCLSolverIncremental()
{
    // Smart pointer handles cleanup of clause database
}

// Main solving method
bool CDCLSolverIncremental::solve()
{
    // Clear any assumptions and solve
    clearAssumptions();
    return solve(assumptions);
}

// Solve with assumptions
bool CDCLSolverIncremental::solve(const std::vector<int> &assume)
{
    // Store start time for timeout
    start_time = std::chrono::high_resolution_clock::now();

    // Store the assumptions
    assumptions = assume;

    // Check for contradictory assumptions
    for (size_t i = 0; i < assumptions.size(); i++)
    {
        for (size_t j = i + 1; j < assumptions.size(); j++)
        {
            int lit1 = assumptions[i];
            int lit2 = assumptions[j];
            if (lit1 == -lit2)
            {
                if (debug_output)
                {
                    std::cout << "Contradictory assumptions: " << lit1 << " and " << lit2 << "\n";
                }

                // Create a core with these contradictory assumptions
                core = {lit1, lit2};
                return false;
            }
        }
    }

    // Clear trail and variable states for a fresh start
    trail.clear();
    var_to_trail.clear();
    assignments.clear();
    for (size_t i = 0; i < decision_levels.size(); i++)
    {
        decision_levels[i] = 0;
    }
    decision_level = 0;
    conflicts_since_restart = 0;

    // Initialize watches if this is a fresh solve
    if (last_solved_until == -1)
    {
        db->initWatches();
    }

    // Apply assumptions as unit clauses
    for (int lit : assumptions)
    {
        int var = std::abs(lit);
        bool value = (lit > 0);

        // Check for contradictory assumptions
        auto it = assignments.find(var);
        if (it != assignments.end() && it->second != value)
        {
            if (debug_output)
            {
                std::cout << "Contradictory assumptions, formula is UNSAT\n";
            }

            // Create a core with just this contradictory assumption
            core = {lit};
            return false;
        }

        // Add to trail
        trail.emplace_back(lit, 0, std::numeric_limits<size_t>::max(), true);
        var_to_trail[var] = trail.size() - 1;

        // Update assignment
        assignments[var] = value;
        decision_levels[var] = 0; // Assumptions are at level 0
    }

    // Check for immediate unit propagation conflicts
    if (!unitPropagate())
    {
        if (debug_output)
        {
            std::cout << "Conflict during initial unit propagation, formula is UNSAT\n";
        }

        conflicts++; // Increment conflict counter

        // Extract UNSAT core from the conflict
        Clause learned_clause;
        int backtrack_level = analyzeConflict(conflict_clause_id, learned_clause);

        // Calculate LBD if enabled
        int lbd = use_lbd ? db->computeLBD(learned_clause, decision_levels) : learned_clause.size();

        // Add the learned clause to the database
        db->addLearnedClause(learned_clause, lbd);

        // Bump VSIDS scores for variables in the learned clause
        for (int lit : learned_clause)
        {
            bumpVarActivity(std::abs(lit));
        }

        // Decay VSIDS scores
        decayVarActivities();

        // Extract the core assumptions
        core.clear();
        for (int assumption : assumptions)
        {
            int var = std::abs(assumption);

            // Check if this variable appears in the learned clause with correct polarity
            bool var_in_learned_clause = false;
            for (int learned_lit : learned_clause)
            {
                // Check for exact literal match to respect polarity
                if (learned_lit == -assumption)
                {
                    var_in_learned_clause = true;
                    break;
                }
            }

            if (!var_in_learned_clause)
            {
                continue; // Skip this assumption if not in learned clause
            }

            // Now check if it was assigned as an assumption at level 0
            auto trail_it = var_to_trail.find(var);
            if (trail_it != var_to_trail.end())
            {
                const auto &node = trail[trail_it->second];

                // Include this assumption if it was assigned at level 0
                if (node.decision_level == 0 && node.is_decision)
                {
                    core.push_back(assumption);
                }
            }
        }

        return false;
    }

    // Main CDCL loop
    const int MAX_ITERATIONS = 1000000; // Increased from 100000
    int iterations = 0;
    int last_conflict_count = 0;
    int last_decision_count = 0;
    int last_propagation_count = 0;
    stuck_counter = 0; // Reset stuck counter at start of solve
    int last_restart_count = 0;
    int no_progress_count = 0;
    int last_learned_clause_size = 0;
    int consecutive_restarts = 0;
    int last_decision_level = 0;
    int stuck_at_level_count = 0;

    while (iterations < MAX_ITERATIONS)
    {
        iterations++;

        // Check for timeout at the start of each iteration
        if (checkTimeout())
        {
            if (debug_output)
            {
                std::cout << "Timeout reached after " << iterations << " iterations.\n";
                printStatistics();
            }
            return false;
        }

        // Enhanced stuck detection
        bool progress = false;

        // Check for progress in conflicts
        if (conflicts > last_conflict_count)
        {
            progress = true;
            stuck_counter = 0;
            no_progress_count = 0;
            consecutive_restarts = 0;
            stuck_at_level_count = 0;
        }

        // Check for progress in decisions
        if (decisions > last_decision_count)
        {
            progress = true;
            stuck_counter = 0;
            no_progress_count = 0;
            consecutive_restarts = 0;
            stuck_at_level_count = 0;
        }

        // Check for progress in propagations
        if (propagations > last_propagation_count)
        {
            progress = true;
            stuck_counter = 0;
            no_progress_count = 0;
            consecutive_restarts = 0;
            stuck_at_level_count = 0;
        }

        // Check for progress in learned clause sizes
        if (db->getNumLearnedClauses() > last_learned_clause_size)
        {
            progress = true;
            stuck_counter = 0;
            no_progress_count = 0;
            consecutive_restarts = 0;
            stuck_at_level_count = 0;
        }

        // Check for progress in decision level
        if (decision_level > last_decision_level)
        {
            progress = true;
            stuck_counter = 0;
            no_progress_count = 0;
            consecutive_restarts = 0;
            stuck_at_level_count = 0;
        }
        else if (decision_level == last_decision_level)
        {
            stuck_at_level_count++;
        }

        // Check for restarts
        if (restarts > last_restart_count)
        {
            progress = true;
            stuck_counter = 0;
            no_progress_count = 0;
            consecutive_restarts++;
            stuck_at_level_count = 0;
        }

        // Update last counts
        last_conflict_count = conflicts;
        last_decision_count = decisions;
        last_propagation_count = propagations;
        last_restart_count = restarts;
        last_learned_clause_size = db->getNumLearnedClauses();
        last_decision_level = decision_level;

        // If no progress, increment counters
        if (!progress)
        {
            stuck_counter++;
            no_progress_count++;

            // If stuck for too long, try a restart
            if (stuck_counter > 50)
            { // Changed back to 50 from 200
                if (debug_output)
                {
                    std::cout << "No progress for " << stuck_counter << " iterations, forcing restart.\n";
                }

                // If we've restarted too many times consecutively, try a more aggressive restart
                if (consecutive_restarts > 10)
                { // Increased from 3
                    if (debug_output)
                    {
                        std::cout << "Too many consecutive restarts, clearing learned clauses.\n";
                    }
                    // Clear learned clauses and reset VSIDS scores
                    db->clearLearnedClauses();
                    initializeVSIDS();
                    consecutive_restarts = 0;
                }
                else
                {
                    restart();
                }
                stuck_counter = 0;
            }

            // If stuck at the same decision level for too long, force backtrack
            if (stuck_at_level_count > 400)
            { // Increased from 100
                if (debug_output)
                {
                    std::cout << "Stuck at decision level " << decision_level << " for too long, forcing backtrack.\n";
                }
                // Backtrack to a lower level
                backtrack(std::max(0, decision_level - 1));
                stuck_at_level_count = 0;
            }

            // If still stuck after multiple restarts, return UNSAT
            if (no_progress_count > 2000)
            { // Increased from 500
                if (debug_output)
                {
                    std::cout << "Solver appears to be stuck after " << iterations << " iterations.\n";
                    std::cout << "Last progress: " << no_progress_count << " iterations ago.\n";
                    printStatistics();
                }
                return false;
            }
        }

        // Check if we should restart
        if (shouldRestart())
        {
            restart();
        }

        // Perform unit propagation with timeout check
        bool conflict = !unitPropagate();

        if (conflict)
        {
            conflicts++;
            conflicts_since_restart++;

            // Handle conflict at decision level 0
            if (decision_level == 0)
            {
                if (debug_output)
                {
                    std::cout << "Conflict at decision level 0. Formula is UNSATISFIABLE.\n";
                }

                // Extract UNSAT core from the conflict
                Clause learned_clause;
                analyzeConflict(conflict_clause_id, learned_clause);

                // Extract the core assumptions
                core.clear();
                for (int assumption : assumptions)
                {
                    int var = std::abs(assumption);

                    // Check if this variable appears in the learned clause with correct polarity
                    bool var_in_learned_clause = false;
                    for (int learned_lit : learned_clause)
                    {
                        // Check for exact literal match to respect polarity
                        if (learned_lit == -assumption)
                        {
                            var_in_learned_clause = true;
                            break;
                        }
                    }

                    if (!var_in_learned_clause)
                    {
                        continue; // Skip this assumption if not in learned clause
                    }

                    // Now check if it was assigned as an assumption at level 0
                    auto trail_it = var_to_trail.find(var);
                    if (trail_it != var_to_trail.end())
                    {
                        const auto &node = trail[trail_it->second];

                        // Include this assumption if it was assigned at level 0
                        if (node.decision_level == 0 && node.is_decision)
                        {
                            core.push_back(assumption);
                        }
                    }
                }

                return false;
            }

            // Analyze conflict and learn a new clause
            Clause learned_clause;
            int backtrack_level = analyzeConflict(conflict_clause_id, learned_clause);

            // Check timeout after conflict analysis
            if (checkTimeout())
            {
                return false;
            }

            // Minimize the learned clause
            minimizeClause(learned_clause);

            // Calculate LBD if enabled
            int lbd = use_lbd ? db->computeLBD(learned_clause, decision_levels) : learned_clause.size();

            // Add the learned clause to the database
            db->addLearnedClause(learned_clause, lbd);

            // Backtrack to the computed level
            backtrack(backtrack_level);

            // Bump VSIDS scores for variables in the learned clause
            for (int lit : learned_clause)
            {
                bumpVarActivity(std::abs(lit));
            }

            // Decay VSIDS scores periodically
            decayVarActivities();

            // Decay clause activities
            db->decayClauseActivities();
        }
        else
        {
            // No conflict, make a new decision
            if (!makeDecision())
            {
                if (debug_output)
                {
                    std::cout << "All variables assigned without conflict. Formula is SATISFIABLE.\n";
                }
                return true;
            }
        }
    }

    // If we get here, the formula is satisfied or too complex
    if (debug_output)
    {
        std::cout << "Reached maximum iterations. Cannot determine satisfiability.\n";
        printStatistics();
    }

    // Return UNSAT as a conservative approach
    return false;
}

// Add a permanent clause to the formula
void CDCLSolverIncremental::addClause(const Clause &clause)
{
    // Add clause to the database
    db->addClause(clause);

    // Update VSIDS for the new clause
    for (int lit : clause)
    {
        int var = std::abs(lit);

        // Initialize activity if this is a new variable
        if (activity.find(var) == activity.end())
        {
            activity[var] = 0.0;

            // Resize decision levels array if needed
            if (var >= static_cast<int>(decision_levels.size()))
            {
                decision_levels.resize(var + 1, 0);
            }
        }
    }

    // Mark that the solver state is no longer valid
    if (last_solved_until != -1)
    {
        last_solved_until = -1;
    }
}

// Add a temporary clause valid only for the next solve
void CDCLSolverIncremental::addTemporaryClause(const Clause &clause)
{
    // Add clause to the database as a non-core clause
    ClauseID id = db->addClause(clause);
    db->clauses[id]->is_core = false;

    // Update VSIDS for the new clause
    for (int lit : clause)
    {
        int var = std::abs(lit);

        // Initialize activity if this is a new variable
        if (activity.find(var) == activity.end())
        {
            activity[var] = 0.0;

            // Resize decision levels array if needed
            if (var >= static_cast<int>(decision_levels.size()))
            {
                decision_levels.resize(var + 1, 0);
            }
        }
    }
}

// Set assumptions for the next solve
void CDCLSolverIncremental::setAssumptions(const std::vector<int> &assume)
{
    assumptions = assume;
}

// Add a single assumption
void CDCLSolverIncremental::addAssumption(int literal)
{
    assumptions.push_back(literal);
}

// Clear all assumptions
void CDCLSolverIncremental::clearAssumptions()
{
    assumptions.clear();
}

// Get the UNSAT core from the last solve
std::vector<int> CDCLSolverIncremental::getUnsatCore() const
{
    return core;
}

// Force initial polarity for a variable
void CDCLSolverIncremental::setDecisionPolarity(int var, bool phase)
{
    if (use_phase_saving)
    {
        // Store the preferred phase
        activity[var] = phase ? 1.0 : -1.0;
    }
}

// Randomize some polarities for diversification
void CDCLSolverIncremental::setRandomizedPolarities(double random_freq)
{
    if (!use_phase_saving)
        return;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);

    for (auto &[var, act] : activity)
    {
        if (dis(gen) < random_freq)
        {
            // Randomize the phase
            act = (dis(gen) < 0.5) ? 1.0 : -1.0;
        }
    }
}

// Set maximum number of learned clauses
void CDCLSolverIncremental::setMaxLearnts(size_t max_learnts)
{
    db->max_learnt_clauses = max_learnts;
}

// Set VSIDS decay factor
void CDCLSolverIncremental::setVarDecay(double decay)
{
    var_decay = decay;
}

// Configure restart strategy
void CDCLSolverIncremental::setRestartStrategy(bool use_luby, int init_threshold)
{
    use_luby_restarts = use_luby;
    restart_threshold = init_threshold;
    luby_index = 1;
}

// Get number of variables
int CDCLSolverIncremental::getNumVars() const
{
    return db->getNumVariables();
}

// Get number of clauses
int CDCLSolverIncremental::getNumClauses() const
{
    return db->getNumClauses();
}

// Get number of learned clauses
int CDCLSolverIncremental::getNumLearnts() const
{
    return db->getNumLearnedClauses();
}

// Propagate unit clauses
bool CDCLSolverIncremental::unitPropagate()
{
    size_t trail_index = 0;

    // Process trail until no more propagations
    while (trail_index < trail.size())
    {
        // Check for timeout periodically during propagation
        if (trail_index % 1000 == 0)
        { // Check every 1000 propagations
            if (checkTimeout())
            {
                return false;
            }
        }

        // Get the assigned literal
        int lit = trail[trail_index].literal;
        int neg_lit = -lit;

        // Check all clauses watching the negation of this literal
        const auto &watch_list = db->getWatches(neg_lit);

        // Create a copy of the watch list that we can modify
        std::vector<ClauseID> watch_list_copy = watch_list;

        for (auto it = watch_list_copy.begin(); it != watch_list_copy.end();)
        {
            ClauseID clause_id = *it;

            // Skip if clause has been deleted
            if (clause_id >= db->clauses.size() || !db->clauses[clause_id])
            {
                ++it;
                continue;
            }

            const auto &clause = db->clauses[clause_id];

            // Make sure the falsified literal is at index 0
            if (clause->watched_lits.second == neg_lit)
            {
                // Swap watched literals
                std::swap(clause->watched_lits.first, clause->watched_lits.second);
            }

            // Cache the other watched literal
            int other_lit = clause->watched_lits.second;

            // Check if the other watched literal is true
            int other_var = std::abs(other_lit);
            auto other_it = assignments.find(other_var);
            if (other_it != assignments.end() &&
                ((other_lit > 0 && other_it->second) || (other_lit < 0 && !other_it->second)))
            {
                // Clause is satisfied, continue
                ++it;
                continue;
            }

            // Try to find a new literal to watch
            bool found_new_watch = false;

            for (size_t i = 0; i < clause->literals.size(); i++)
            {
                int l = clause->literals[i];

                // Skip current watched literals
                if (l == clause->watched_lits.first || l == clause->watched_lits.second)
                {
                    continue;
                }

                // Check if this literal is unassigned or true
                int var = std::abs(l);
                auto var_it = assignments.find(var);
                if (var_it == assignments.end() ||
                    (l > 0 && var_it->second) || (l < 0 && !var_it->second))
                {
                    // Found a new watch
                    db->updateWatches(clause_id, neg_lit, l);
                    clause->watched_lits.first = l;
                    found_new_watch = true;
                    break;
                }
            }

            if (found_new_watch)
            {
                // We don't need to remove from watch_list_copy since we're just iterating through it
                ++it;
                continue;
            }

            // If we get here, no new watch was found

            // Check if the other watched literal is unassigned
            if (other_it == assignments.end())
            {
                // Unit clause, propagate it
                int var = std::abs(other_lit);
                bool value = (other_lit > 0);

                // Check for direct contradiction with existing assignment
                auto existing_assignment = assignments.find(var);
                if (existing_assignment != assignments.end() && existing_assignment->second != value)
                {
                    // We have a contradiction! The variable is already assigned the opposite value
                    if (debug_output)
                    {
                        std::cout << "Contradiction detected: x" << var << " would be assigned both "
                                  << existing_assignment->second << " and " << value << "\n";
                    }

                    // Find the clause that assigned the variable to its current value
                    ClauseID conflicting_clause_id = std::numeric_limits<ClauseID>::max();
                    for (size_t i = 0; i < trail.size(); i++)
                    {
                        const auto &node = trail[i];
                        if (std::abs(node.literal) == var)
                        {
                            conflicting_clause_id = node.antecedent_id;
                            break;
                        }
                    }

                    // If we couldn't find the conflicting clause, use the current clause
                    if (conflicting_clause_id == std::numeric_limits<ClauseID>::max())
                    {
                        conflicting_clause_id = clause_id; // Current clause ID
                    }

                    // Set the conflict clause ID
                    conflict_clause_id = conflicting_clause_id;
                    return false;
                }

                // Add to trail
                trail.emplace_back(other_lit, decision_level, clause_id);
                var_to_trail[var] = trail.size() - 1;

                // Update assignment
                assignments[var] = value;
                decision_levels[var] = decision_level;

                propagations++;

                if (debug_output)
                {
                    std::cout << "Unit propagation: x" << var << " = " << value
                              << " at level " << decision_level << "\n";
                }

                // Keep this clause in the watch list
                ++it;
            }
            else
            {
                // Both watched literals are assigned and false, this is a conflict
                if (debug_output)
                {
                    std::cout << "Conflict detected in clause: ";
                    printClause(clause->literals);
                    std::cout << "\n";
                }

                // Store conflicting clause ID for conflict analysis
                conflict_clause_id = clause_id;
                return false;
            }
        }

        // Move to next literal in the trail
        trail_index++;
    }

    // Also check for any unit clauses in the database
    // This is important for formulas like (x1) AND (NOT x1)
    for (size_t i = 0; i < db->clauses.size(); i++)
    {
        if (!db->clauses[i] || db->clauses[i]->literals.empty())
            continue;

        const auto &clause = db->clauses[i]->literals;

        // Count satisfied and unassigned literals
        int satisfied = 0;
        int unassigned = 0;
        int last_unassigned_lit = 0;

        for (int lit : clause)
        {
            int var = std::abs(lit);
            auto it = assignments.find(var);

            if (it == assignments.end())
            {
                // Unassigned
                unassigned++;
                last_unassigned_lit = lit;
            }
            else
            {
                // Assigned - check if it satisfies the clause
                if ((lit > 0 && it->second) || (lit < 0 && !it->second))
                {
                    satisfied++;
                    break;
                }
            }
        }

        // If clause is satisfied, skip it
        if (satisfied > 0)
        {
            continue;
        }

        // If all literals are assigned and none satisfied, we have a conflict
        if (unassigned == 0)
        {
            if (debug_output)
            {
                std::cout << "Conflict detected: clause is unsatisfied: ";
                printClause(clause);
                std::cout << "\n";
            }

            // Store conflicting clause ID for conflict analysis
            conflict_clause_id = i;
            return false;
        }

        // If there's exactly one unassigned literal, it's a unit clause
        if (unassigned == 1)
        {
            int var = std::abs(last_unassigned_lit);
            bool value = (last_unassigned_lit > 0);

            // Check for direct contradiction with existing assignment
            auto existing_assignment = assignments.find(var);
            if (existing_assignment != assignments.end() && existing_assignment->second != value)
            {
                // We have a contradiction! The variable is already assigned the opposite value
                if (debug_output)
                {
                    std::cout << "Contradiction detected: x" << var << " would be assigned both "
                              << existing_assignment->second << " and " << value << "\n";
                }

                // Find the clause that assigned the variable to its current value
                ClauseID conflicting_clause_id = std::numeric_limits<ClauseID>::max();
                for (size_t j = 0; j < trail.size(); j++)
                {
                    const auto &node = trail[j];
                    if (std::abs(node.literal) == var)
                    {
                        conflicting_clause_id = node.antecedent_id;
                        break;
                    }
                }

                // If we couldn't find the conflicting clause, use the current clause
                if (conflicting_clause_id == std::numeric_limits<ClauseID>::max())
                {
                    conflicting_clause_id = i; // Current clause ID
                }

                // Set the conflict clause ID
                conflict_clause_id = conflicting_clause_id;
                return false;
            }

            // Add to trail
            trail.emplace_back(last_unassigned_lit, decision_level, i);
            var_to_trail[var] = trail.size() - 1;

            // Update assignment
            assignments[var] = value;
            decision_levels[var] = decision_level;

            propagations++;

            if (debug_output)
            {
                std::cout << "Unit propagation from clause scan: x" << var << " = " << value
                          << " at level " << decision_level << "\n";
            }

            // Start over since this new assignment might trigger more propagations
            return unitPropagate();
        }
    }

    return true;
}

// Analyze conflict and learn a new clause
int CDCLSolverIncremental::analyzeConflict(ClauseID conflict_id, Clause &learned_clause)
{
    if (debug_output)
    {
        std::cout << "Analyzing conflict in clause: ";
        printClause(db->clauses[conflict_id]->literals);
        std::cout << "\n";
    }

    // Initialize the learned clause with the conflict clause
    learned_clause = db->clauses[conflict_id]->literals;

    // Count literals from the current decision level
    std::unordered_set<int> current_level_vars;
    for (int lit : learned_clause)
    {
        int var = std::abs(lit);
        auto it = var_to_trail.find(var);
        if (it != var_to_trail.end())
        {
            const auto &node = trail[it->second];
            if (node.decision_level == decision_level)
            {
                current_level_vars.insert(var);
            }
        }
    }

    if (debug_output)
    {
        std::cout << "Current level variables in conflict: " << current_level_vars.size() << "\n";
    }

    // Find the second highest decision level in the clause
    int backtrack_level = 0;

    // Create a vector of trail indices for variables at the current decision level
    // for deterministic processing
    std::vector<size_t> current_level_indices;
    for (int var : current_level_vars)
    {
        auto it = var_to_trail.find(var);
        if (it != var_to_trail.end())
        {
            current_level_indices.push_back(it->second);
        }
    }

    // Sort indices in reverse order for deterministic processing
    std::sort(current_level_indices.begin(), current_level_indices.end(),
              [](size_t a, size_t b)
              { return a > b; });

    // Resolve the conflict by combining with antecedent clauses
    size_t trail_index_pos = 0;
    while (current_level_vars.size() > 1 && trail_index_pos < current_level_indices.size())
    {
        if (trail_index_pos % 100 == 0)
        {
            if (checkTimeout())
            {
                return 0;
            }
        }

        // Keep one literal from current level for UIP
        size_t trail_index = current_level_indices[trail_index_pos];
        const auto &node = trail[trail_index];
        int var = std::abs(node.literal);

        // Skip if this variable is not in current_level_vars or is a decision
        if (current_level_vars.find(var) == current_level_vars.end() ||
            node.is_decision ||
            node.antecedent_id == std::numeric_limits<size_t>::max())
        {
            trail_index_pos++;
            continue;
        }

        // Get the antecedent clause
        const auto &antecedent = db->clauses[node.antecedent_id]->literals;

        if (debug_output)
        {
            std::cout << "Resolving with antecedent of x" << var << ": ";
            printClause(antecedent);
            std::cout << "\n";
        }

        // Resolve the learned clause with the antecedent
        // Remove the current variable from the learned clause
        learned_clause.erase(
            std::remove_if(learned_clause.begin(), learned_clause.end(),
                           [var](int lit)
                           { return std::abs(lit) == var; }),
            learned_clause.end());

        // Add literals from the antecedent (except the current variable)
        for (int lit : antecedent)
        {
            if (std::abs(lit) != var &&
                std::find(learned_clause.begin(), learned_clause.end(), lit) == learned_clause.end())
            {
                learned_clause.push_back(lit);

                // Check if this is a variable from the current decision level
                int lit_var = std::abs(lit);
                auto it = var_to_trail.find(lit_var);
                if (it != var_to_trail.end())
                {
                    const auto &lit_node = trail[it->second];
                    if (lit_node.decision_level == decision_level)
                    {
                        if (current_level_vars.find(lit_var) == current_level_vars.end())
                        {
                            current_level_vars.insert(lit_var);
                            current_level_indices.push_back(it->second);
                        }
                    }
                    else if (lit_node.decision_level > backtrack_level &&
                             lit_node.decision_level != decision_level)
                    {
                        // Update the second highest decision level
                        backtrack_level = lit_node.decision_level;
                    }
                }
            }
        }

        // Remove the variable from the current level set
        current_level_vars.erase(var);

        // Resort the indices after adding new variables
        std::sort(current_level_indices.begin(), current_level_indices.end(),
                  [](size_t a, size_t b)
                  { return a > b; });

        if (debug_output)
        {
            std::cout << "After resolution, learned clause: ";
            printClause(learned_clause);
            std::cout << "Current level variables remaining: " << current_level_vars.size() << "\n";
        }

        // Move to the next position
        trail_index_pos = 0; // Restart with the sorted indices
    }

    // Remove duplicates from the learned clause
    std::sort(learned_clause.begin(), learned_clause.end());
    learned_clause.erase(std::unique(learned_clause.begin(), learned_clause.end()), learned_clause.end());

    // Preserve all literals from assumptions
    for (int assumption : assumptions)
    {
        int var = std::abs(assumption);
        bool has_var = false;
        for (int lit : learned_clause)
        {
            if (std::abs(lit) == var)
            {
                has_var = true;
                break;
            }
        }

        // If the assumption variable is not in the learned clause,
        // check if it was involved in the conflict path
        if (!has_var)
        {
            auto it = var_to_trail.find(var);
            if (it != var_to_trail.end() && decision_levels[var] == 0)
            {
                // Add this assumption to ensure it's captured for UNSAT core
                learned_clause.push_back(assumption);
            }
        }
    }

    if (debug_output)
    {
        std::cout << "Final learned clause: ";
        printClause(learned_clause);
        std::cout << "Backtrack level: " << backtrack_level << "\n";
    }

    return backtrack_level;
}

// Backtrack to a specific decision level
void CDCLSolverIncremental::backtrack(int level)
{
    if (debug_output)
    {
        std::cout << "Backtracking from level " << decision_level << " to level " << level << "\n";
    }

    // Remove assignments made after the backtrack level
    while (!trail.empty())
    {
        const auto &node = trail.back();
        if (node.decision_level <= level)
        {
            break;
        }

        // Remove the variable from assignments
        int var = std::abs(node.literal);
        assignments.erase(var);
        var_to_trail.erase(var);
        decision_levels[var] = 0;

        // Remove from trail
        trail.pop_back();
    }

    // Update the decision level
    decision_level = level;

    if (debug_output)
    {
        std::cout << "After backtracking, trail size: " << trail.size() << "\n";
        printTrail();
    }
}

// Make a new decision
bool CDCLSolverIncremental::makeDecision()
{
    // Use VSIDS to select the next variable
    int var = selectVarVSIDS();

    if (var == 0)
    {
        if (debug_output)
        {
            std::cout << "No unassigned variables left for decisions.\n";
        }
        return false;
    }

    // Increment decision level
    decision_level++;
    max_decision_level = std::max(max_decision_level, decision_level);
    decisions++;

    // Get the current ratio (clauses/variables)
    double ratio = static_cast<double>(db->getNumClauses()) / db->getNumVariables();

    // Phase selection strategy optimized for random 3-SAT
    bool value;

    // Initialize random number generator
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.0, 1.0);

    // 1. Count positive and negative occurrences for balanced selection
    int pos_count = 0, neg_count = 0;
    for (size_t i = 0; i < db->clauses.size(); i++)
    {
        if (!db->clauses[i])
            continue;
        for (int lit : db->clauses[i]->literals)
        {
            if (std::abs(lit) == var)
            {
                if (lit > 0)
                    pos_count++;
                else
                    neg_count++;
            }
        }
    }

    // 2. Calculate activity-based bias
    double activity_bias = 0.0;
    auto it = activity.find(var);
    if (it != activity.end())
    {
        activity_bias = it->second;
    }

    // 3. Phase transition-aware decision making
    if (ratio >= 4.0 && ratio <= 4.5)
    {
        // In phase transition region
        // Calculate randomization probability based on distance from critical ratio and solver progress
        double dist_from_critical = std::abs(ratio - 4.25);
        double progress_factor = (stuck_counter > 0) ? 0.2 : 0.0;
        double rand_prob = 0.2 + (0.3 * (1.0 - dist_from_critical / 0.25)) + progress_factor; // 0.2-0.7 range

        if (dis(gen) < rand_prob)
        {
            // Random decision with slight bias towards activity
            value = (dis(gen) < 0.5 + (activity_bias * 0.1));
        }
        else
        {
            // Activity-based decision with occurrence count influence
            if (std::abs(activity_bias) > 0.1)
            {
                // Strong activity bias
                value = (activity_bias > 0);
            }
            else
            {
                // Balanced selection based on occurrence counts
                value = (pos_count >= neg_count);
            }
        }
    }
    else if (ratio > 4.5)
    {
        // Above phase transition - more aggressive randomization
        double progress_factor = (stuck_counter > 0) ? 0.15 : 0.0;
        if (dis(gen) < 0.4 + progress_factor)
        {
            // 40-55% chance for random decision with activity bias
            value = (dis(gen) < 0.5 + (activity_bias * 0.15));
        }
        else
        {
            // Activity-based with occurrence count influence
            value = (activity_bias > 0) == (pos_count >= neg_count);
        }
    }
    else
    {
        // Below phase transition - more deterministic
        if (stuck_counter > 0)
        {
            // When stuck, use more randomization
            value = (dis(gen) < 0.5 + (activity_bias * 0.05));
        }
        else
        {
            // Normal deterministic selection
            if (std::abs(activity_bias) > 0.1)
            {
                value = (activity_bias > 0);
            }
            else
            {
                value = (pos_count >= neg_count);
            }
        }
    }

    // Add the decision to the trail
    int lit = value ? var : -var;
    trail.emplace_back(lit, decision_level, std::numeric_limits<size_t>::max(), true);
    var_to_trail[var] = trail.size() - 1;

    // Update assignment
    assignments[var] = value;
    decision_levels[var] = decision_level;

    if (debug_output)
    {
        std::cout << "Decision: x" << var << " = " << value
                  << " at level " << decision_level
                  << " (ratio: " << ratio << ", activity: " << activity_bias
                  << ", pos/neg: " << pos_count << "/" << neg_count << ")\n";
    }

    return true;
}

// Check if formula is satisfied
bool CDCLSolverIncremental::isSatisfied() const
{
    // First check if all variables are assigned
    if (assignments.size() != db->getNumVariables())
    {
        return false;
    }

    // Then check if all clauses are satisfied
    for (size_t i = 0; i < db->clauses.size(); i++)
    {
        if (!db->clauses[i])
            continue;
        bool clause_satisfied = false;
        for (int literal : db->clauses[i]->literals)
        {
            auto it = assignments.find(std::abs(literal));
            if (it != assignments.end() &&
                (literal > 0 ? it->second : !it->second))
            {
                clause_satisfied = true;
                break;
            }
        }
        if (!clause_satisfied)
        {
            return false;
        }
    }
    return true;
}

// Initialize VSIDS scores
void CDCLSolverIncremental::initializeVSIDS()
{
    // Count occurrences of each literal in the initial formula
    for (size_t i = 0; i < db->clauses.size(); i++)
    {
        if (!db->clauses[i])
            continue; // Skip deleted clauses

        const auto &clause = db->clauses[i];
        for (int lit : clause->literals)
        {
            int var = std::abs(lit);
            activity[var] += 1.0;
        }
    }

    if (debug_output)
    {
        std::cout << "Initialized VSIDS activities:\n";
        for (const auto &[var, score] : activity)
        {
            std::cout << "Var " << var << ": " << score << "\n";
        }
    }
}

// Select variable by VSIDS
int CDCLSolverIncremental::selectVarVSIDS()
{
    int best_var = 0;
    double best_score = -1.0;

    // Get current ratio
    double ratio = static_cast<double>(db->getNumClauses()) / db->getNumVariables();

    // Initialize random number generator
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.0, 1.0);

    // Calculate random selection probability based on ratio and solver progress
    double random_prob = 0.0;
    if (ratio >= 4.0 && ratio <= 4.5)
    {
        // In phase transition region
        double dist_from_critical = std::abs(ratio - 4.25);
        // More aggressive randomization when stuck
        double progress_factor = (stuck_counter > 0) ? 0.2 : 0.0;
        random_prob = 0.15 + (0.35 * (1.0 - dist_from_critical / 0.25)) + progress_factor; // 0.15-0.7 range
    }
    else if (ratio > 4.5)
    {
        // Above phase transition - more random selection
        // Increase randomization when stuck
        double progress_factor = (stuck_counter > 0) ? 0.15 : 0.0;
        random_prob = 0.25 + (0.25 * (ratio - 4.5) / 0.5) + progress_factor; // 0.25-0.65 range
    }
    else
    {
        // Below phase transition - less random selection
        // Only use randomization when stuck
        random_prob = (stuck_counter > 0) ? 0.1 : 0.02; // 0.02-0.1 range
    }

    // Try random selection first with calculated probability
    if (dis(gen) < random_prob)
    {
        // Collect all unassigned variables
        std::vector<int> unassigned;
        for (size_t var = 1; var <= db->getNumVariables(); var++)
        {
            if (assignments.find(var) == assignments.end())
            {
                unassigned.push_back(var);
            }
        }

        // Randomly select one
        if (!unassigned.empty())
        {
            std::uniform_int_distribution<> var_dis(0, unassigned.size() - 1);
            best_var = unassigned[var_dis(gen)];

            if (debug_output)
            {
                std::cout << "Randomly selected var " << best_var << " (ratio: " << ratio
                          << ", random_prob: " << random_prob << ")\n";
            }
            return best_var;
        }
    }

    // If random selection didn't happen or failed, use VSIDS
    for (const auto &[var, score] : activity)
    {
        // Skip already assigned variables
        if (assignments.find(var) != assignments.end())
        {
            continue;
        }

        // Use absolute value of score for comparison
        double abs_score = std::abs(score);
        if (abs_score > best_score)
        {
            best_score = abs_score;
            best_var = var;
        }
    }

    // If VSIDS selection failed, try any unassigned variable
    if (best_var == 0)
    {
        for (size_t var = 1; var <= db->getNumVariables(); var++)
        {
            if (assignments.find(var) == assignments.end())
            {
                best_var = var;
                break;
            }
        }
    }

    if (debug_output && best_var != 0)
    {
        std::cout << "VSIDS selected var " << best_var << " with score "
                  << (activity.find(best_var) != activity.end() ? activity[best_var] : 0.0)
                  << " (ratio: " << ratio << ", random_prob: " << random_prob << ")\n";
    }

    return best_var;
}

// Bump variable activity
void CDCLSolverIncremental::bumpVarActivity(int var)
{
    activity[var] += var_inc;

    // Handle potential numerical overflow
    if (std::abs(activity[var]) > 1e100)
    {
        // Rescale all activity values
        for (auto &[v, act] : activity)
        {
            act *= 1e-100;
        }
        var_inc *= 1e-100;
    }
}

// Decay variable activities
void CDCLSolverIncremental::decayVarActivities()
{
    var_inc /= var_decay;
}

// Minimize learned clause using self-subsumption
void CDCLSolverIncremental::minimizeClause(Clause &clause)
{
    // Skip minimization for very large clauses
    if (clause.size() > 100)
    {
        return;
    }

    // Set a local timer for this specific operation
    auto minimize_start = std::chrono::high_resolution_clock::now();
    const auto max_minimize_time = std::chrono::milliseconds(100); // 100ms max

    // Add timeout check at the start
    if (checkTimeout())
    {
        return; // Skip minimization if timeout or solution found
    }

    if (clause.size() <= 1)
        return;

    std::unordered_set<int> seen;
    std::vector<int> minimized;

    // Counter for periodic timeout checks
    int timeout_check_counter = 0;

    // Mark all literals in the clause
    for (int lit : clause)
    {
        seen.insert(lit);
    }

    // Try to remove each literal
    for (int lit : clause)
    {
        // Check timeout periodically
        if ((timeout_check_counter++ % 50) == 0)
        {
            // Check local timeout
            auto current_time = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - minimize_start);
            if (elapsed > max_minimize_time || checkTimeout())
            {
                // If timeout detected, just return the original clause
                return;
            }
        }

        int var = std::abs(lit);

        // Never remove literals at decision level 0
        auto it = var_to_trail.find(var);
        if (it != var_to_trail.end() && decision_levels[var] == 0)
        {
            minimized.push_back(lit);
            continue;
        }

        // Never remove literals that correspond to assumptions
        bool is_assumption = false;
        for (int assumption : assumptions)
        {
            if (lit == assumption)
            { // Check exact literal match, not just variable
                is_assumption = true;
                break;
            }
        }

        if (is_assumption)
        {
            minimized.push_back(lit);
            continue;
        }

        // Check if this literal is redundant
        if (!isRedundant(lit, seen, timeout_check_counter, minimize_start, max_minimize_time))
        {
            minimized.push_back(lit);
        }
    }

    // Update the clause if we removed any literals
    if (minimized.size() < clause.size())
    {
        if (debug_output)
        {
            std::cout << "Minimized clause from " << clause.size() << " to " << minimized.size() << " literals\n";
        }
        clause = minimized;
    }

    // Clear the seen set to prevent tracking issues between different calls
    seen.clear();
}

// Check if a literal is redundant in the learned clause
// Modified isRedundant to include timeout checks
bool CDCLSolverIncremental::isRedundant(int lit, std::unordered_set<int> &seen, int &timeout_check_counter,
                                        const std::chrono::time_point<std::chrono::high_resolution_clock> &start_time,
                                        const std::chrono::milliseconds &max_time)
{
    // Check timeout periodically
    if ((timeout_check_counter++ % 50) == 0)
    {
        // Check local timeout
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - start_time);
        if (elapsed > max_time || checkTimeout())
        {
            return false; // Conservative approach: if timeout, don't consider redundant
        }
    }

    int var = std::abs(lit);
    auto it = var_to_trail.find(var);
    if (it == var_to_trail.end())
        return false;

    const auto &node = trail[it->second];

    // Decision variables or assumptions are never redundant
    if (node.is_decision || node.antecedent_id == std::numeric_limits<size_t>::max())
    {
        return false;
    }

    // Literals from assumptions should never be considered redundant
    for (int assumption : assumptions)
    {
        if (lit == assumption)
        { // Exact literal match to preserve polarity
            return false;
        }
    }

    // Check if the reason clause allows us to remove this literal
    if (node.antecedent_id >= db->clauses.size() || !db->clauses[node.antecedent_id])
    {
        return false; // The antecedent clause has been deleted
    }

    const auto &reason = db->clauses[node.antecedent_id]->literals;

    // Check each literal in the reason clause
    for (int reason_lit : reason)
    {
        // Periodic timeout check
        if ((timeout_check_counter++ % 50) == 0)
        {
            // Check local timeout
            auto current_time = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - start_time);
            if (elapsed > max_time || checkTimeout())
            {
                return false; // Conservative approach
            }
        }

        if (std::abs(reason_lit) == var)
            continue; // Skip the literal itself

        // Calculate the negated reason literal (for seen check)
        int neg_reason_lit = -reason_lit;

        // If the negation is not in seen, this literal is not redundant
        if (seen.find(neg_reason_lit) == seen.end())
        {
            int reason_var = std::abs(reason_lit);
            auto reason_it = var_to_trail.find(reason_var);

            // If the reason variable is not in the trail or is from a deeper level,
            // this literal is not redundant
            if (reason_it == var_to_trail.end() ||
                decision_levels[reason_var] > decision_levels[var])
            {
                return false;
            }

            // Check if this reason literal is derived from an assumption
            // If it is, we should not consider the current literal redundant
            const auto &reason_node = trail[reason_it->second];
            if (reason_node.decision_level == 0)
            {
                return false;
            }
        }
    }

    // If we got here, the literal is redundant
    return true;
}

// Check if we should restart
bool CDCLSolverIncremental::shouldRestart()
{
    if (use_luby_restarts)
    {
        // Luby sequence restart strategy
        return conflicts_since_restart >= restart_threshold * lubySequence(luby_index);
    }
    else
    {
        // Geometric restart strategy
        return conflicts_since_restart >= restart_threshold;
    }
}

// Perform a restart
void CDCLSolverIncremental::restart()
{
    if (debug_output)
    {
        std::cout << "Restarting after " << conflicts_since_restart << " conflicts\n";
    }

    // Backtrack to decision level 0 (keep assumptions)
    backtrack(0);

    // Update restart parameters
    if (use_luby_restarts)
    {
        luby_index++;
    }
    else
    {
        restart_threshold = static_cast<int>(restart_threshold * restart_multiplier);
    }

    conflicts_since_restart = 0;
    restarts++;
}

// Compute the Luby sequence
int CDCLSolverIncremental::lubySequence(int i)
{
    // Compute the largest power of 2 that divides i
    int power = 0;
    int temp = i;
    while (temp % 2 == 0)
    {
        temp /= 2;
        power++;
    }

    // Check if i is one less than a power of 2
    if (temp == 1)
    {
        return 1 << power;
    }
    else
    {
        // Recursively compute luby(i - 2^k + 1)
        return lubySequence(i - (1 << power) + 1);
    }
}

// Print the current trail
void CDCLSolverIncremental::printTrail() const
{
    if (!debug_output)
        return;

    std::cout << "Trail (decision level, literal, is_decision):\n";
    for (size_t i = 0; i < trail.size(); i++)
    {
        const auto &node = trail[i];
        int var = std::abs(node.literal);
        bool value = node.literal > 0;

        std::cout << "[" << i << "] Level " << node.decision_level << ": x" << var << " = " << value;
        if (node.is_decision)
        {
            std::cout << " (decision)";
        }
        else if (node.antecedent_id != std::numeric_limits<size_t>::max())
        {
            std::cout << " (propagation from: ";
            if (node.antecedent_id < db->clauses.size() && db->clauses[node.antecedent_id])
            {
                printClause(db->clauses[node.antecedent_id]->literals);
            }
            else
            {
                std::cout << "deleted clause";
            }
            std::cout << ")";
        }
        else
        {
            std::cout << " (assumption)";
        }
        std::cout << "\n";
    }
}

// Print a clause
void CDCLSolverIncremental::printClause(const Clause &clause) const
{
    if (!debug_output)
        return;

    std::cout << "(";
    for (size_t i = 0; i < clause.size(); i++)
    {
        int lit = clause[i];
        if (lit > 0)
        {
            std::cout << "x" << lit;
        }
        else
        {
            std::cout << "~x" << -lit;
        }

        if (i < clause.size() - 1)
        {
            std::cout << " ∨ ";
        }
    }
    std::cout << ")";
}

// Print solver statistics
void CDCLSolverIncremental::printStatistics() const
{
    std::cout << "Solver Statistics:\n";
    std::cout << "  Variables: " << db->getNumVariables() << "\n";
    std::cout << "  Clauses: " << db->getNumClauses() << "\n";
    std::cout << "  Learned Clauses: " << db->getNumLearnedClauses() << "\n";
    std::cout << "  Conflicts: " << conflicts << "\n";
    std::cout << "  Decisions: " << decisions << "\n";
    std::cout << "  Propagations: " << propagations << "\n";
    std::cout << "  Restarts: " << restarts << "\n";
    std::cout << "  Max Decision Level: " << max_decision_level << "\n";

    // Print clause database statistics
    db->printStatistics();
}

// Check if timeout has been reached
bool CDCLSolverIncremental::checkTimeout()
{
    auto current_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        current_time - start_time);

    // Check both timeout and solution found status from portfolio
    if (elapsed > timeout_duration ||
        (portfolio_manager != nullptr && portfolio_manager->isSolutionFound()))
    {
        if (debug_output)
        {
            std::cout << "Timeout reached or solution already found. Stopping search.\n";
        }
        return true;
    }
    return false;
}

int CDCLSolverIncremental::newVariable()
{
    int new_var = db->getNumVariables();

    // Initialize activity for the new variable
    activity[new_var] = 0.0;

    // Resize decision levels array if needed
    if (new_var >= static_cast<int>(decision_levels.size()))
    {
        decision_levels.resize(new_var + 1, 0);
    }

    // Update the number of variables in the database
    db->addVariable();

    return new_var;
}