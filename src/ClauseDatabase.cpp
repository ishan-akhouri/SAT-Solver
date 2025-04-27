#include "../include/ClauseDatabase.h"
#include <iostream>
#include <algorithm>
#include <cassert>
#include <limits>
#include <unordered_set>

// ClauseInfo implementation
ClauseInfo::ClauseInfo(const Clause &lits, bool learned, bool core)
    : literals(lits), is_learned(learned), is_core(core), activity(0), lbd(0)
{
    // Initialize watched literals to first two if possible
    if (literals.size() >= 2)
    {
        watched_lits = {literals[0], literals[1]};
    }
    else if (literals.size() == 1)
    {
        watched_lits = {literals[0], 0}; // Unit clause
    }
    else
    {
        watched_lits = {0, 0}; // Empty clause (conflict)
    }
}

// ClauseDatabase implementation
ClauseDatabase::ClauseDatabase(size_t num_vars, bool debug)
    : clause_activity_inc(1),
      clause_decay_factor(0.999),
      num_variables(num_vars),
      original_clauses(0),
      total_learned(0),
      active_learned(0),
      deleted_learned(0),
      max_learnt_clauses(num_vars * 4), // Increased from 2x to 4x
      clause_deletion_threshold(2.5),   // Lowered from 3.0 to be more aggressive
      allow_clause_deletion(true),
      debug_output(debug),
      current_memory_usage(0)
{

    // Initialize watches array with pre-allocated space
    watches.resize(2 * num_vars + 1);
    for (auto &watch_list : watches)
    {
        watch_list.reserve(num_vars); // Pre-allocate space
    }

    if (debug_output)
    {
        std::cout << "ClauseDatabase initialized with " << num_vars << " variables\n";
        std::cout << "Max learned clauses: " << max_learnt_clauses << "\n";
        std::cout << "Memory limit: " << MAX_MEMORY_MB << "MB\n";
    }
}

ClauseID ClauseDatabase::addClause(const Clause &clause, bool is_learned)
{
    // Create a new clause with the given literals
    auto clause_ref = std::make_shared<ClauseInfo>(clause, is_learned, !is_learned);
    ClauseID id = clauses.size();

    // Add to the clause vector
    clauses.push_back(clause_ref);

    // Update statistics
    if (is_learned)
    {
        learned_clauses.push_back(clause_ref);
        total_learned++;
        active_learned++;
    }
    else
    {
        original_clauses++;
    }

    // Set up watches if clause has at least 2 literals
    if (clause.size() >= 2)
    {
        int lit1 = clause[0];
        int lit2 = clause[1];

        size_t watch_idx1 = lit1 > 0 ? lit1 : num_variables + std::abs(lit1);
        size_t watch_idx2 = lit2 > 0 ? lit2 : num_variables + std::abs(lit2);

        watches[watch_idx1].push_back(id);
        watches[watch_idx2].push_back(id);
    }
    else if (clause.size() == 1)
    {
        // Unit clause - watch the only literal
        int lit = clause[0];
        size_t watch_idx = lit > 0 ? lit : num_variables + std::abs(lit);
        watches[watch_idx].push_back(id);
    }

    if (debug_output)
    {
        std::cout << "Added " << (is_learned ? "learned" : "original") << " clause: ";
        for (int lit : clause)
        {
            std::cout << lit << " ";
        }
        std::cout << " (ID: " << id << ")\n";
    }

    return id;
}

ClauseID ClauseDatabase::addLearnedClause(const Clause &clause, int lbd)
{
    // Create a new clause with the given literals
    auto clause_ref = std::make_shared<ClauseInfo>(clause, true, false);
    clause_ref->lbd = lbd;

    ClauseID id = clauses.size();

    // Add to the clause vector
    clauses.push_back(clause_ref);
    learned_clauses.push_back(clause_ref);

    // Update statistics
    total_learned++;
    active_learned++;

    // Set up watches if clause has at least 2 literals
    if (clause.size() >= 2)
    {
        int lit1 = clause[0];
        int lit2 = clause[1];

        size_t watch_idx1 = lit1 > 0 ? lit1 : num_variables + std::abs(lit1);
        size_t watch_idx2 = lit2 > 0 ? lit2 : num_variables + std::abs(lit2);

        watches[watch_idx1].push_back(id);
        watches[watch_idx2].push_back(id);

        // Store watched literals in the clause
        clause_ref->watched_lits = {lit1, lit2};
    }
    else if (clause.size() == 1)
    {
        // Unit clause - watch the only literal
        int lit = clause[0];
        size_t watch_idx = lit > 0 ? lit : num_variables + std::abs(lit);
        watches[watch_idx].push_back(id);

        // Store watched literal
        clause_ref->watched_lits = {lit, 0};
    }

    // Update memory usage
    updateMemoryUsage();

    // Trigger garbage collection if we exceed the limit
    if (allow_clause_deletion && active_learned > max_learnt_clauses)
    {
        // Create empty assignments map since we don't have current assignments
        std::unordered_map<int, bool> empty_assignments;
        reduceLearnedClauses(empty_assignments);
    }

    if (debug_output)
    {
        std::cout << "Added learned clause with LBD " << lbd << ": ";
        for (int lit : clause)
        {
            std::cout << lit << " ";
        }
        std::cout << " (ID: " << id << ")\n";
        std::cout << "Current memory usage: " << current_memory_usage / (1024 * 1024) << "MB\n";
    }

    return id;
}

void ClauseDatabase::removeClause(ClauseID id)
{
    if (id >= clauses.size() || !clauses[id])
    {
        // Clause already removed or invalid ID
        return;
    }

    auto &clause = clauses[id];

    // Remove watches
    if (clause->size() >= 2)
    {
        int lit1 = clause->watched_lits.first;
        int lit2 = clause->watched_lits.second;

        size_t watch_idx1 = lit1 > 0 ? lit1 : num_variables + std::abs(lit1);
        size_t watch_idx2 = lit2 > 0 ? lit2 : num_variables + std::abs(lit2);

        auto &watch_list1 = watches[watch_idx1];
        auto &watch_list2 = watches[watch_idx2];

        watch_list1.erase(std::remove(watch_list1.begin(), watch_list1.end(), id), watch_list1.end());
        watch_list2.erase(std::remove(watch_list2.begin(), watch_list2.end(), id), watch_list2.end());
    }
    else if (clause->size() == 1)
    {
        int lit = clause->literals[0];
        size_t watch_idx = lit > 0 ? lit : num_variables + std::abs(lit);

        auto &watch_list = watches[watch_idx];
        watch_list.erase(std::remove(watch_list.begin(), watch_list.end(), id), watch_list.end());
    }

    // Update statistics
    if (clause->is_learned)
    {
        active_learned--;
        deleted_learned++;
    }

    // Mark the clause as deleted by setting its pointer to nullptr
    // We don't actually remove it from the vector to keep IDs stable
    clauses[id] = nullptr;

    if (debug_output)
    {
        std::cout << "Removed clause with ID: " << id << "\n";
    }
}

void ClauseDatabase::initWatches()
{
    // Clear existing watches
    for (auto &watch_list : watches)
    {
        watch_list.clear();
    }

    // Set up watches for all clauses
    for (size_t id = 0; id < clauses.size(); id++)
    {
        if (!clauses[id])
            continue; // Skip deleted clauses

        const auto &clause = clauses[id];

        if (clause->size() >= 2)
        {
            int lit1 = clause->literals[0];
            int lit2 = clause->literals[1];

            size_t watch_idx1 = lit1 > 0 ? lit1 : num_variables + std::abs(lit1);
            size_t watch_idx2 = lit2 > 0 ? lit2 : num_variables + std::abs(lit2);

            watches[watch_idx1].push_back(id);
            watches[watch_idx2].push_back(id);

            // Store watched literals
            clause->watched_lits = {lit1, lit2};
        }
        else if (clause->size() == 1)
        {
            int lit = clause->literals[0];
            size_t watch_idx = lit > 0 ? lit : num_variables + std::abs(lit);

            watches[watch_idx].push_back(id);

            // Store watched literal
            clause->watched_lits = {lit, 0};
        }
    }

    if (debug_output)
    {
        std::cout << "Watched literals initialized\n";
        printWatches();
    }
}

void ClauseDatabase::updateWatches(ClauseID id, int old_lit, int new_lit)
{
    if (id >= clauses.size() || !clauses[id])
    {
        // Clause already removed or invalid ID
        return;
    }

    // Remove the old watch
    size_t old_watch_idx = old_lit > 0 ? old_lit : num_variables + std::abs(old_lit);
    auto &old_watch_list = watches[old_watch_idx];
    old_watch_list.erase(std::remove(old_watch_list.begin(), old_watch_list.end(), id), old_watch_list.end());

    // Add the new watch
    size_t new_watch_idx = new_lit > 0 ? new_lit : num_variables + std::abs(new_lit);
    watches[new_watch_idx].push_back(id);

    // Update the watched literals in the clause
    auto &clause = clauses[id];
    if (clause->watched_lits.first == old_lit)
    {
        clause->watched_lits.first = new_lit;
    }
    else if (clause->watched_lits.second == old_lit)
    {
        clause->watched_lits.second = new_lit;
    }
    else
    {
        if (debug_output)
        {
            std::cout << "Warning: Updating watch for literal that is not watched\n";
        }
    }
}

const std::vector<ClauseID> &ClauseDatabase::getWatches(int literal) const
{
    size_t watch_idx = literal > 0 ? literal : num_variables + std::abs(literal);
    return watches[watch_idx];
}

void ClauseDatabase::bumpClauseActivity(ClauseID id)
{
    if (id >= clauses.size() || !clauses[id])
    {
        return;
    }

    auto &clause = clauses[id];
    if (!clause->is_learned)
        return; // Only bump learned clauses

    clause->activity += clause_activity_inc;

    // Check for activity overflow
    if (clause->activity > 1e20)
    {
        // Rescale all activities
        for (auto &c : clauses)
        {
            if (c && c->is_learned)
            {
                c->activity *= 1e-20;
            }
        }
        clause_activity_inc *= 1e-20;
    }
}

void ClauseDatabase::decayClauseActivities()
{
    clause_activity_inc /= clause_decay_factor;
}

void ClauseDatabase::garbageCollect(const std::unordered_map<int, bool> &assignments)
{
    // Count satisfied clauses
    size_t satisfied = 0;

    for (size_t id = 0; id < clauses.size(); id++)
    {
        if (!clauses[id])
            continue; // Skip already deleted clauses

        const auto &clause = clauses[id];

        // Check if clause is satisfied
        bool is_satisfied = false;
        for (int lit : clause->literals)
        {
            int var = std::abs(lit);
            auto it = assignments.find(var);
            if (it != assignments.end())
            {
                bool value = it->second;
                if ((lit > 0 && value) || (lit < 0 && !value))
                {
                    is_satisfied = true;
                    break;
                }
            }
        }

        // Remove satisfied learned clauses
        if (is_satisfied && clause->is_learned && !clause->is_core)
        {
            removeClause(id);
            satisfied++;
        }
    }

    if (debug_output)
    {
        std::cout << "Garbage collection removed " << satisfied << " satisfied learned clauses\n";
    }
}

size_t ClauseDatabase::reduceLearnedClauses(const std::unordered_map<int, bool> &assignments)
{
    if (!allow_clause_deletion || active_learned <= max_learnt_clauses)
    {
        return 0;
    }

    // First, run garbage collection to remove satisfied clauses
    garbageCollect(assignments);

    // If we're still over the limit, remove the least active clauses
    if (active_learned > max_learnt_clauses)
    {
        // Sort learned clauses by activity and LBD
        std::vector<std::pair<ClauseID, double>> clause_scores;
        for (size_t id = 0; id < clauses.size(); id++)
        {
            if (!clauses[id] || !clauses[id]->is_learned || clauses[id]->is_core)
            {
                continue;
            }

            const auto &clause = clauses[id];

            // Score is a combination of activity and LBD (lower LBD is better)
            double score = clause->activity / (clause->lbd > 0 ? clause->lbd : 1.0);
            clause_scores.push_back({id, score});
        }

        // Sort by score (ascending)
        std::sort(clause_scores.begin(), clause_scores.end(),
                  [](const auto &a, const auto &b)
                  { return a.second < b.second; });

        // Target: remove clauses to get to about 3/4 of the max
        size_t target = max_learnt_clauses * 3 / 4;
        size_t to_remove = active_learned - target;
        size_t removed = 0;

        for (const auto &[id, score] : clause_scores)
        {
            if (removed >= to_remove)
                break;

            const auto &clause = clauses[id];

            // Don't remove clauses with low LBD (they are high quality)
            if (clause->lbd <= clause_deletion_threshold)
            {
                continue;
            }

            removeClause(id);
            removed++;
        }

        if (debug_output)
        {
            std::cout << "Clause reduction removed " << removed << " low-quality learned clauses\n";
            std::cout << "Active learned clauses: " << active_learned << "/" << max_learnt_clauses << "\n";
        }

        return removed;
    }

    return 0;
}

int ClauseDatabase::computeLBD(const Clause &clause, const std::vector<int> &levels)
{
    std::unordered_set<int> distinct_levels;

    for (int lit : clause)
    {
        int var = std::abs(lit);
        if (var < levels.size() && levels[var] > 0)
        {
            distinct_levels.insert(levels[var]);
        }
    }

    return distinct_levels.size();
}

void ClauseDatabase::addAssumption(int literal)
{
    // Add a unit clause with the assumption
    Clause assumption = {literal};
    addClause(assumption, false);
}

void ClauseDatabase::clearAssumptions()
{
    // The implementation for this is in CDCLSolverIncremental::clearAssumptions().
}

std::vector<int> ClauseDatabase::extractCoreAssumptions(const Clause &conflict)
{
    // The implementation for this is in CDCLSolverIncremental::analyzeConflict().
    return conflict;
}

// So why do we have those two functions?
// Definitely not because I've been debugging for 13 hours straight and I forgot to remove it.
// It's okay to have a little redundancy in the codebase. It's natural. It happens to everyone.
// Plus, what if I need it later for some obscure reason that will never happen?
// It's better to be safe than sorry, right?

const ClauseRef &ClauseDatabase::getClause(ClauseID id) const
{
    assert(id < clauses.size() && clauses[id]);
    return clauses[id];
}

size_t ClauseDatabase::getNumClauses() const
{
    return original_clauses + active_learned;
}

size_t ClauseDatabase::getNumLearnedClauses() const
{
    return active_learned;
}

size_t ClauseDatabase::getNumVariables() const
{
    return num_variables;
}

void ClauseDatabase::printStatistics() const
{
    std::cout << "Clause Database Statistics:\n";
    std::cout << "  Original clauses: " << original_clauses << "\n";
    std::cout << "  Learned clauses (total): " << total_learned << "\n";
    std::cout << "  Learned clauses (active): " << active_learned << "\n";
    std::cout << "  Learned clauses (deleted): " << deleted_learned << "\n";
    std::cout << "  Max learned clauses: " << max_learnt_clauses << "\n";
    std::cout << "  Deletion threshold (LBD): " << clause_deletion_threshold << "\n";
}

void ClauseDatabase::printWatches() const
{
    std::cout << "Watched literals:\n";
    for (size_t i = 1; i < watches.size(); i++)
    {
        if (!watches[i].empty())
        {
            int lit;
            if (i <= num_variables)
            {
                lit = i; // Positive literal
            }
            else
            {
                lit = -(i - num_variables); // Negative literal
            }

            std::cout << "  Literal " << lit << " is watched by clauses: ";
            for (ClauseID id : watches[i])
            {
                std::cout << id << " ";
            }
            std::cout << "\n";
        }
    }
}

bool ClauseDatabase::checkWatchesConsistency() const
{
    bool consistent = true;

    for (size_t id = 0; id < clauses.size(); id++)
    {
        if (!clauses[id])
            continue;

        const auto &clause = clauses[id];

        if (clause->size() >= 2)
        {
            int lit1 = clause->watched_lits.first;
            int lit2 = clause->watched_lits.second;

            size_t watch_idx1 = lit1 > 0 ? lit1 : num_variables + std::abs(lit1);
            size_t watch_idx2 = lit2 > 0 ? lit2 : num_variables + std::abs(lit2);

            bool found1 = false, found2 = false;

            for (ClauseID w_id : watches[watch_idx1])
            {
                if (w_id == id)
                {
                    found1 = true;
                    break;
                }
            }

            for (ClauseID w_id : watches[watch_idx2])
            {
                if (w_id == id)
                {
                    found2 = true;
                    break;
                }
            }

            if (!found1 || !found2)
            {
                std::cout << "Watch inconsistency for clause " << id << "\n";
                consistent = false;
            }
        }
        else if (clause->size() == 1)
        {
            int lit = clause->literals[0];
            size_t watch_idx = lit > 0 ? lit : num_variables + std::abs(lit);

            bool found = false;
            for (ClauseID w_id : watches[watch_idx])
            {
                if (w_id == id)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                std::cout << "Watch inconsistency for unit clause " << id << "\n";
                consistent = false;
            }
        }
    }

    return consistent;
}

// Add memory usage tracking
size_t ClauseDatabase::calculateMemoryUsage() const
{
    size_t total = 0;

    // Count memory used by clauses
    for (const auto &clause : clauses)
    {
        if (clause)
        {
            total += sizeof(ClauseInfo) +
                     clause->literals.size() * sizeof(int) +
                     sizeof(std::shared_ptr<ClauseInfo>);
        }
    }

    // Count memory used by watch lists
    for (const auto &watch_list : watches)
    {
        total += watch_list.size() * sizeof(ClauseID) +
                 sizeof(std::vector<ClauseID>);
    }

    return total;
}

void ClauseDatabase::updateMemoryUsage()
{
    current_memory_usage = calculateMemoryUsage();

    // If memory usage exceeds limit, force clause deletion
    if (current_memory_usage > MAX_MEMORY_MB * 1024 * 1024)
    {
        if (debug_output)
        {
            std::cout << "Memory usage (" << current_memory_usage / (1024 * 1024)
                      << "MB) exceeds limit, forcing clause deletion.\n";
        }
        // Use empty assignments map since we're just cleaning up
        std::unordered_map<int, bool> dummy_assignments;
        reduceLearnedClauses(dummy_assignments);
    }
}

int ClauseDatabase::addVariable()
{
    num_variables++;

    // Ensure the watches array is large enough
    if (watches.size() <= 2 * num_variables)
    {
        watches.resize(2 * num_variables + 1);
    }

    return num_variables;
}