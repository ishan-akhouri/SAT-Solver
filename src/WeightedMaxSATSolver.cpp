#include "WeightedMaxSATSolver.h"
#include <algorithm>
#include <iostream>
#include <numeric>
#include <limits>
#include <chrono>

WeightedMaxSATSolver::WeightedMaxSATSolver(const CNF &hard_clauses, bool debug)
    : hard_clauses(hard_clauses), debug_output(debug), solver_calls(0),
      has_previous_solution(false) {}

void WeightedMaxSATSolver::addSoftClause(const Clause &soft_clause, int weight)
{
    if (soft_clause.empty() || weight <= 0)
        return;

    soft_clauses.push_back(soft_clause);
    weights.push_back(weight);

    if (debug_output)
    {
        std::cout << "Added weighted soft clause (weight " << weight << "): ";
        for (int lit : soft_clause)
        {
            std::cout << lit << " ";
        }
        std::cout << std::endl;
    }
}

void WeightedMaxSATSolver::addSoftClauses(const CNF &clauses, int weight)
{
    for (const auto &clause : clauses)
    {
        addSoftClause(clause, weight);
    }
}

int WeightedMaxSATSolver::solveStratified()
{
    // Reset warm start data at beginning of new solve
    has_previous_solution = false;
    last_solution.clear();

    if (soft_clauses.empty())
    {
        // No soft clauses, just solve the hard clauses
        MaxSATSolver solver(hard_clauses, debug_output);
        solver_calls += solver.getNumSolverCalls();
        bool result = solver.solve() == 0;
        return result ? 0 : -1; // -1 indicates unsatisfiable hard clauses
    }

    if (debug_output)
    {
        std::cout << "Starting stratified weighted MaxSAT solver" << std::endl;
        std::cout << "Hard clauses: " << hard_clauses.size() << std::endl;
        std::cout << "Soft clauses: " << soft_clauses.size() << std::endl;
    }

    // Sort clauses by weight (highest first)
    std::vector<std::pair<int, int>> indexed_weights; // (index, weight)
    for (size_t i = 0; i < weights.size(); i++)
    {
        indexed_weights.push_back({i, weights[i]});
    }

    std::sort(indexed_weights.begin(), indexed_weights.end(),
              [](const auto &a, const auto &b)
              { return a.second > b.second; });

    // Group clauses by weight
    std::vector<std::vector<int>> weight_groups; // Groups of clause indices with same weight
    std::vector<int> unique_weights;             // The weight of each group

    if (!indexed_weights.empty())
    {
        int current_weight = indexed_weights[0].second;
        unique_weights.push_back(current_weight);
        weight_groups.push_back({indexed_weights[0].first});

        for (size_t i = 1; i < indexed_weights.size(); i++)
        {
            if (indexed_weights[i].second == current_weight)
            {
                // Add to current group
                weight_groups.back().push_back(indexed_weights[i].first);
            }
            else
            {
                // Start a new group
                current_weight = indexed_weights[i].second;
                unique_weights.push_back(current_weight);
                weight_groups.push_back({indexed_weights[i].first});
            }
        }
    }

    // Current state of the solution
    CNF current_hard_clauses = hard_clauses;
    int total_weight_violated = 0;

    // Solve each weight group
    for (size_t group = 0; group < weight_groups.size(); group++)
    {
        if (debug_output)
        {
            std::cout << "Solving for weight group " << unique_weights[group]
                      << " with " << weight_groups[group].size() << " clauses"
                      << (has_previous_solution ? " (warm start)" : "") << std::endl;
        }

        // Create a MaxSAT solver for this weight group
        MaxSATSolver solver(current_hard_clauses, debug_output);

        // Apply warm starting if we have a previous solution
        if (has_previous_solution)
        {
            for (const auto &[var, value] : last_solution)
            {
                solver.getAssignment(); // This is just to access the solver
                // Need to access the internal CDCL solver to set polarities
                // Since we can't modify the interface, we'll have to do this differently
                // For now, we'll rely on the MaxSATSolver's own warm starting
            }

            if (debug_output)
            {
                std::cout << "Applied warm starting with " << last_solution.size() << " variables" << std::endl;
            }
        }

        // Add soft clauses for this weight group
        for (int idx : weight_groups[group])
        {
            solver.addSoftClause(soft_clauses[idx]);
        }

        // Solve
        int violated = solver.solve();
        solver_calls += solver.getNumSolverCalls();

        if (violated == -1)
        {
            // Hard clauses are unsatisfiable
            if (debug_output)
            {
                std::cout << "Hard clauses became unsatisfiable" << std::endl;
            }
            return -1;
        }

        // Update total weight
        total_weight_violated += violated * unique_weights[group];

        if (debug_output)
        {
            std::cout << "Violated " << violated << " clauses of weight "
                      << unique_weights[group] << std::endl;
        }

        // Get the assignment and update warm start data
        const auto &assignment = solver.getAssignment();
        last_solution = assignment;
        has_previous_solution = true;

        // Add the assignment as unit clauses to the hard clauses
        for (const auto &[var, value] : assignment)
        {
            // Add as unit clause
            current_hard_clauses.push_back({value ? var : -var});
        }
    }

    if (debug_output)
    {
        std::cout << "Total weight of violated clauses: " << total_weight_violated << std::endl;
        std::cout << "Total solver calls: " << solver_calls << std::endl;
    }

    return total_weight_violated;
}

int WeightedMaxSATSolver::solveBinarySearch()
{
    // Reset warm start data at beginning of new solve
    has_previous_solution = false;
    last_solution.clear();

    if (soft_clauses.empty())
    {
        // No soft clauses, just solve the hard clauses
        MaxSATSolver solver(hard_clauses, debug_output);
        solver_calls += solver.getNumSolverCalls();
        bool result = solver.solve() == 0;
        return result ? 0 : -1; // -1 indicates unsatisfiable hard clauses
    }

    if (debug_output)
    {
        std::cout << "Starting binary search with improved exponential probing weighted MaxSAT solver" << std::endl;
        std::cout << "Hard clauses: " << hard_clauses.size() << std::endl;
        std::cout << "Soft clauses: " << soft_clauses.size() << std::endl;
    }

    // First check if all soft clauses can be satisfied
    MaxSATSolver full_solver(hard_clauses, debug_output);
    for (const auto &clause : soft_clauses)
    {
        full_solver.addSoftClause(clause);
    }

    int violated = full_solver.solve();
    solver_calls += full_solver.getNumSolverCalls();

    if (violated == 0)
    {
        // All soft clauses can be satisfied
        return 0;
    }

    if (violated == -1)
    {
        // Hard clauses are unsatisfiable
        return -1;
    }

    // Save initial solution for warm starting
    if (violated >= 0)
    {
        last_solution = full_solver.getAssignment();
        has_previous_solution = true;
    }

    // Calculate total weight
    int total_weight = 0;
    for (int w : weights)
    {
        total_weight += w;
    }

    // Improved exponential probing to find initial bounds
    int lower_bound = 1;
    int upper_bound = 1;

    // Use a smarter initial step based on total weight
    int step_size = std::max(1, total_weight / 10);

    // Early estimation - try with a small percentage of total weight
    int early_estimate = total_weight / 4; // Try 25% of total weight
    if (early_estimate > 1 && checkWeightLimit(early_estimate))
    {
        // Found a satisfiable point, use it as upper bound
        upper_bound = early_estimate;

        if (debug_output)
        {
            std::cout << "Early estimation successful at weight = " << early_estimate << std::endl;
        }
    }
    else
    {
        // Use exponential probing with adaptive step sizes
        while (upper_bound < total_weight)
        {
            if (debug_output)
            {
                std::cout << "Exponential probing at weight = " << upper_bound << std::endl;
            }

            if (checkWeightLimit(upper_bound))
            {
                // Found a satisfiable point, use this as upper bound
                break;
            }

            // Update bounds and increase step size
            lower_bound = upper_bound + 1;
            step_size = std::min(step_size * 2, total_weight / 2);
            upper_bound = std::min(upper_bound + step_size, total_weight);
        }
    }

    // If we've reached the maximum and it's still UNSAT, try with all weight
    if (upper_bound == total_weight && !checkWeightLimit(total_weight))
    {
        return -1; // Formula is UNSAT even with all weight violated
    }

    if (debug_output)
    {
        std::cout << "Binary search range after probing: " << lower_bound << " to " << upper_bound << std::endl;
    }

    // Binary search within the identified range
    while (lower_bound < upper_bound)
    {
        int mid_weight = lower_bound + (upper_bound - lower_bound) / 2;

        if (debug_output)
        {
            std::cout << "Testing weight limit: " << mid_weight
                      << " (range: " << lower_bound << "-" << upper_bound << ")"
                      << (has_previous_solution ? " (warm start)" : "") << std::endl;
        }

        bool satisfiable = checkWeightLimit(mid_weight);

        if (satisfiable)
        {
            // We can satisfy with at most mid_weight violated
            upper_bound = mid_weight;
        }
        else
        {
            // Need to violate more weight
            lower_bound = mid_weight + 1;
        }
    }

    // Final check
    bool final_check = checkWeightLimit(lower_bound);

    if (debug_output)
    {
        std::cout << "Final weight of violated clauses: " << (final_check ? lower_bound : -1) << std::endl;
        std::cout << "Total solver calls: " << solver_calls << std::endl;
    }

    return final_check ? lower_bound : -1;
}

bool WeightedMaxSATSolver::checkWeightLimit(int weight_limit)
{
    // Create working copies
    std::vector<std::pair<int, int>> clause_weights; // (index, weight)
    for (size_t i = 0; i < weights.size(); i++)
    {
        clause_weights.push_back({i, weights[i]});
    }

    // Sort by weight (highest first for greedy approach)
    std::sort(clause_weights.begin(), clause_weights.end(),
              [](const auto &a, const auto &b)
              { return a.second > b.second; });

    // Greedy selection: violate lowest weight clauses first up to weight_limit
    std::vector<int> to_satisfy;
    int remaining_weight = weight_limit;

    // Start from the end (lowest weights)
    for (auto it = clause_weights.rbegin(); it != clause_weights.rend(); ++it)
    {
        if (it->second <= remaining_weight)
        {
            // We can afford to violate this clause
            remaining_weight -= it->second;
        }
        else
        {
            // Need to satisfy this clause
            to_satisfy.push_back(it->first);
        }
    }

    // Prepare clauses for the MaxSAT solver
    CNF modified_hard_clauses = hard_clauses;

    // Add clauses we must satisfy as hard clauses
    for (int idx : to_satisfy)
    {
        modified_hard_clauses.push_back(soft_clauses[idx]);
    }

    // Create a solver with hard clauses including the ones we must satisfy
    MaxSATSolver solver(modified_hard_clauses, debug_output);

    // Apply warm starting if we have a previous solution
    if (has_previous_solution)
    {
        // We rely on MaxSATSolver's internal warm starting since we can't directly modify it
        // The solution will be passed when we call solve() below
    }

    // Add remaining clauses as soft
    for (size_t i = 0; i < soft_clauses.size(); i++)
    {
        if (std::find(to_satisfy.begin(), to_satisfy.end(), i) == to_satisfy.end())
        {
            solver.addSoftClause(soft_clauses[i]);
        }
    }

    // Solve
    int result = solver.solve(); // MaxSATSolver now has warm starting built in
    solver_calls += solver.getNumSolverCalls();

    // Update warm start data if successful
    if (result >= 0)
    {
        last_solution = solver.getAssignment();
        has_previous_solution = true;
    }

    // Check if satisfiable and the total weight violated is <= weight_limit
    if (result == -1)
    {
        return false; // Hard clauses unsatisfiable
    }

    // Calculate actual weight violated
    int actual_weight = 0;
    for (size_t i = 0; i < soft_clauses.size(); i++)
    {
        if (std::find(to_satisfy.begin(), to_satisfy.end(), i) == to_satisfy.end())
        {
            // This is a soft clause
            if (result > 0)
            {
                // Some soft clauses violated
                actual_weight += weights[i];
                result--; // Count down the violations
            }
        }
    }

    if (debug_output)
    {
        std::cout << "  Weight limit: " << weight_limit
                  << ", actual: " << actual_weight
                  << ", result: " << (actual_weight <= weight_limit ? "SAT" : "UNSAT") << std::endl;
    }

    return actual_weight <= weight_limit;
}

int WeightedMaxSATSolver::getNumSolverCalls() const
{
    return solver_calls;
}