#include "../include/ClauseMinimizer.h"
#include <iostream>
#include <algorithm>
#include <limits>

ClauseMinimizer::ClauseMinimizer(
    const std::vector<ImplicationNodeIncremental> &trail_ref,
    const std::unordered_map<int, bool> &assignments_ref,
    const std::unordered_map<int, size_t> &var_to_trail_ref,
    const std::vector<int> &decision_levels_ref,
    ClauseDatabase &db_ref,
    bool debug)
    : trail(trail_ref),
      assignments(assignments_ref),
      var_to_trail(var_to_trail_ref),
      decision_levels(decision_levels_ref),
      db(db_ref),
      use_binary_resolution(true),
      use_vivification(false),
      debug_output(debug)
{
}

// Main method to minimize a newly learned conflict clause
void ClauseMinimizer::minimizeConflictClause(Clause &clause)
{
    if (clause.size() <= 1)
        return;

    if (debug_output)
    {
        std::cout << "Before minimization: ";
        printClause(clause);
        std::cout << " (" << clause.size() << " literals)\n";
    }

    // Recursive minimization
    seen.clear();

    // Mark literals in the learned clause
    for (int lit : clause)
    {
        seen.insert(lit);
    }

    std::vector<int> minimized;

    // Try to remove each literal
    for (int lit : clause)
    {
        // Don't remove literals of decision level 0
        int var = std::abs(lit);
        auto it = var_to_trail.find(var);
        if (it != var_to_trail.end() && decision_levels[var] == 0)
        {
            minimized.push_back(lit);
            continue;
        }

        // Check if this literal is redundant
        if (!isRedundant(lit))
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

    seen.clear();

    // Apply binary resolution if enabled
    if (use_binary_resolution)
    {
        binaryResolution(clause);
    }

    // Apply vivification if enabled
    if (use_vivification)
    {
        vivification(clause);
    }

    if (debug_output)
    {
        std::cout << "After minimization: ";
        printClause(clause);
        std::cout << " (" << clause.size() << " literals)\n";
    }
}

// Minimize all learned clauses
void ClauseMinimizer::minimizeLearnedClauses()
{
    if (debug_output)
    {
        std::cout << "Minimizing all learned clauses...\n";
    }

    // Count of clauses and literals before minimization
    size_t clauses_before = 0;
    size_t literals_before = 0;

    // Count of clauses and literals after minimization
    size_t clauses_after = 0;
    size_t literals_after = 0;

    // Process all learned clauses
    for (size_t i = 0; i < db.clauses.size(); i++)
    {
        if (!db.clauses[i] || !db.clauses[i]->is_learned)
            continue;

        auto &clause = db.clauses[i]->literals;

        clauses_before++;
        literals_before += clause.size();

        // Skip unit clauses
        if (clause.size() <= 1)
        {
            clauses_after++;
            literals_after += clause.size();
            continue;
        }

        // Minimize this clause
        size_t before_size = clause.size();

        // Self-subsumption
        selfSubsumption(clause);

        // Binary resolution
        if (use_binary_resolution)
        {
            binaryResolution(clause);
        }

        if (debug_output && clause.size() < before_size)
        {
            std::cout << "Minimized clause " << i << " from " << before_size
                      << " to " << clause.size() << " literals\n";
        }

        clauses_after++;
        literals_after += clause.size();
    }

    if (debug_output)
    {
        std::cout << "Minimization results:\n";
        std::cout << "  Clauses: " << clauses_before << " -> " << clauses_after << "\n";
        std::cout << "  Literals: " << literals_before << " -> " << literals_after << "\n";
        std::cout << "  Reduction: " << (literals_before - literals_after) << " literals ("
                  << (100.0 * (literals_before - literals_after) / literals_before) << "%)\n";
    }
}

// Check if a literal is redundant in a conflict clause
bool ClauseMinimizer::isRedundant(int lit)
{
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

    // Check if the reason clause allows us to remove this literal
    if (node.antecedent_id >= db.clauses.size() || !db.clauses[node.antecedent_id])
    {
        return false; // The antecedent clause has been deleted
    }

    const auto &reason = db.clauses[node.antecedent_id]->literals;

    // Check each literal in the reason clause
    for (int reason_lit : reason)
    {
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

            // Try recursive minimization
            if (!recursiveMinimize(reason_lit, seen))
            {
                return false;
            }
        }
    }

    // If we got here, the literal is redundant
    return true;
}

// Recursive minimization of learned clauses
bool ClauseMinimizer::recursiveMinimize(int lit, std::unordered_set<int> &seen, int depth)
{
    // Avoid deep recursion
    if (depth > 100)
        return false;

    int var = std::abs(lit);
    auto it = var_to_trail.find(var);
    if (it == var_to_trail.end())
        return false;

    const auto &node = trail[it->second];

    // Cannot minimize using decisions or assumptions
    if (node.is_decision || node.antecedent_id == std::numeric_limits<size_t>::max())
    {
        return false;
    }

    // Decision level 0 literals are always safe to remove
    if (decision_levels[var] == 0)
    {
        return true;
    }

    // Check if the reason allows recursive minimization
    if (node.antecedent_id >= db.clauses.size() || !db.clauses[node.antecedent_id])
    {
        return false; // Antecedent not found
    }

    const auto &reason = db.clauses[node.antecedent_id]->literals;

    for (int reason_lit : reason)
    {
        if (std::abs(reason_lit) == var)
            continue; // Skip the literal itself

        // Check if this reason literal is already in the seen set
        int neg_reason_lit = -reason_lit;
        if (seen.find(neg_reason_lit) == seen.end())
        {
            // Not in seen, check recursively
            int reason_var = std::abs(reason_lit);
            auto reason_it = var_to_trail.find(reason_var);

            // If not assigned or at a deeper level, cannot minimize
            if (reason_it == var_to_trail.end() ||
                decision_levels[reason_var] > decision_levels[var])
            {
                return false;
            }

            // Recurse
            if (!recursiveMinimize(reason_lit, seen, depth + 1))
            {
                return false;
            }
        }
    }

    return true;
}

// Self-subsumption minimization
void ClauseMinimizer::selfSubsumption(Clause &clause)
{
    if (clause.size() <= 1)
        return;

    std::vector<int> minimized;

    // Try each literal in the clause
    for (int lit : clause)
    {
        int neg_lit = -lit;
        bool is_redundant = false;

        // Check if this literal is redundant by self-subsumption
        for (size_t i = 0; i < db.clauses.size(); i++)
        {
            if (!db.clauses[i])
                continue;

            const auto &other_clause = db.clauses[i]->literals;

            // Skip the clause itself
            if (&other_clause == &clause)
                continue;

            // Check if this clause subsumes our clause without lit
            if (isImplied(neg_lit, other_clause))
            {
                is_redundant = true;
                break;
            }
        }

        if (!is_redundant)
        {
            minimized.push_back(lit);
        }
        else if (debug_output)
        {
            std::cout << "Removed literal " << lit << " by self-subsumption\n";
        }
    }

    if (minimized.size() < clause.size())
    {
        clause = minimized;
    }
}

// Binary resolution minimization
void ClauseMinimizer::binaryResolution(Clause &clause)
{
    if (clause.size() <= 1)
        return;

    std::unordered_set<int> clause_lits(clause.begin(), clause.end());
    bool changed = false;

    // Try each binary clause in the database
    for (size_t i = 0; i < db.clauses.size(); i++)
    {
        if (!db.clauses[i] || db.clauses[i]->size() != 2)
            continue;

        const auto &binary = db.clauses[i]->literals;
        int lit1 = binary[0];
        int lit2 = binary[1];

        // Check if the first literal is in our clause
        if (clause_lits.find(-lit1) != clause_lits.end())
        {
            // Add the second literal if not already present
            if (clause_lits.find(lit2) == clause_lits.end())
            {
                clause_lits.insert(lit2);
                changed = true;

                if (debug_output)
                {
                    std::cout << "Added literal " << lit2 << " by binary resolution\n";
                }
            }
        }

        // Check if the second literal is in our clause
        if (clause_lits.find(-lit2) != clause_lits.end())
        {
            // Add the first literal if not already present
            if (clause_lits.find(lit1) == clause_lits.end())
            {
                clause_lits.insert(lit1);
                changed = true;

                if (debug_output)
                {
                    std::cout << "Added literal " << lit1 << " by binary resolution\n";
                }
            }
        }
    }

    if (changed)
    {
        // Rebuild the clause
        clause.clear();
        for (int lit : clause_lits)
        {
            clause.push_back(lit);
        }

        // Ensure deterministic order
        std::sort(clause.begin(), clause.end());
    }
}

// Vivification minimization
void ClauseMinimizer::vivification(Clause &clause)
{
    if (clause.size() <= 1)
        return;

    // Save current assignments
    auto saved_assignments = assignments;

    // Sort clause literals by decision level (lowest first)
    std::vector<std::pair<int, int>> sorted_lits;
    for (int lit : clause)
    {
        int var = std::abs(lit);
        int level = var_to_trail.find(var) != var_to_trail.end() ? decision_levels[var] : std::numeric_limits<int>::max();
        sorted_lits.push_back({lit, level});
    }

    std::sort(sorted_lits.begin(), sorted_lits.end(),
              [](const auto &a, const auto &b)
              { return a.second < b.second; });

    // Try to falsify each literal and detect forced assignments
    std::vector<int> vivified;
    std::unordered_set<int> forced_true;

    for (const auto &[lit, _] : sorted_lits)
    {
        int var = std::abs(lit);
        bool value_to_set = (lit < 0); // Falsify the literal

        // If this literal is already forced true, keep it
        if (forced_true.find(lit) != forced_true.end())
        {
            vivified.push_back(lit);
            continue;
        }

        // Tentatively assign the opposite value to falsify this literal
        auto &temp_assignments = const_cast<std::unordered_map<int, bool> &>(assignments);
        temp_assignments[var] = value_to_set;

        // Check if this forces any other literals to be true
        bool conflict = false;
        for (size_t i = 0; i < db.clauses.size() && !conflict; i++)
        {
            if (!db.clauses[i])
                continue;

            const auto &c = db.clauses[i]->literals;

            // Count false and unassigned literals
            int false_count = 0;
            int unassigned_count = 0;
            int last_unassigned = 0;

            for (int l : c)
            {
                int v = std::abs(l);
                auto it = temp_assignments.find(v);

                if (it == temp_assignments.end())
                {
                    unassigned_count++;
                    last_unassigned = l;
                }
                else if ((l > 0 && !it->second) || (l < 0 && it->second))
                {
                    false_count++;
                }
            }

            // If all literals are false, we have a conflict
            if (false_count == c.size())
            {
                conflict = true;
                break;
            }

            // If all but one are false, the last one is forced true
            if (false_count == c.size() - 1 && unassigned_count == 1)
            {
                forced_true.insert(last_unassigned);
                temp_assignments[std::abs(last_unassigned)] = (last_unassigned > 0);

                // If this forces one of our literals true, mark it
                for (int l : clause)
                {
                    if (l == last_unassigned)
                    {
                        vivified.push_back(l);
                        break;
                    }
                }
            }
        }

        // If we got a conflict, this literal must stay in the clause
        if (conflict)
        {
            vivified.push_back(lit);
        }
    }

    // Restore original assignments
    const_cast<std::unordered_map<int, bool> &>(assignments) = saved_assignments;

    // Update the clause if we removed any literals
    if (vivified.size() < clause.size())
    {
        if (debug_output)
        {
            std::cout << "Vivification reduced clause from " << clause.size()
                      << " to " << vivified.size() << " literals\n";
        }
        clause = vivified;
    }
}

// Check if a literal is implied by a clause
bool ClauseMinimizer::isImplied(int lit, const Clause &by_clause)
{
    // Check if the clause contains the negation of any literal in our clause
    for (int clause_lit : by_clause)
    {
        if (clause_lit == lit)
        {
            return true;
        }
    }

    return false;
}

// Print a clause
void ClauseMinimizer::printClause(const Clause &clause) const
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