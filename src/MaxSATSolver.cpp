#include "MaxSATSolver.h"
#include <algorithm>
#include <iostream>
#include <chrono>

MaxSATSolver::MaxSATSolver(const CNF &hard_clauses, bool debug)
    : solver(hard_clauses, debug),
      next_var(solver.getNumVars() + 1),
      debug_output(debug),
      solver_calls(0),
      has_previous_solution(false) // Initialize warm start flag
{
}

void MaxSATSolver::addSoftClause(const Clause &soft_clause, int weight)
{
    if (soft_clause.empty())
        return;

    // Create a relaxation variable
    int relax_var = next_var++;
    solver.newVariable();

    // Add the soft clause with a relaxation variable
    Clause augmented_clause = soft_clause;
    augmented_clause.push_back(relax_var);
    solver.addClause(augmented_clause);

    // Store the relaxation variable and its weight
    relaxation_vars.push_back(relax_var);
    weights.push_back(weight);

    if (debug_output)
    {
        std::cout << "Added soft clause (weight " << weight << "): ";
        for (int lit : soft_clause)
        {
            std::cout << lit << " ";
        }
        std::cout << "with relaxation variable " << relax_var << std::endl;
    }
}

void MaxSATSolver::addSoftClauses(const CNF &soft_clauses, int weight)
{
    for (const auto &clause : soft_clauses)
    {
        addSoftClause(clause, weight);
    }
}

std::vector<int> MaxSATSolver::createAssumptions(int k)
{
    std::vector<int> assumptions;

    // Ensure relaxation variables for the first k soft clauses are false
    for (size_t i = 0; i < relaxation_vars.size() && i < k; i++)
    {
        assumptions.push_back(-relaxation_vars[i]);
    }

    return assumptions;
}

bool MaxSATSolver::solveWithKRelaxed(int k, std::vector<int> &assumptions)
{
    assumptions = createAssumptions(relaxation_vars.size() - k);

    // Apply warm starting if we have a previous solution
    if (has_previous_solution)
    {
        for (const auto &[var, value] : last_solution)
        {
            // Set preferred polarity based on previous solution
            solver.setDecisionPolarity(var, value);
        }

        if (debug_output)
        {
            std::cout << "Applied warm starting with " << last_solution.size() << " variables" << std::endl;
        }
    }

    solver_calls++;

    if (debug_output)
    {
        std::cout << "Solving with " << k << " relaxed clauses..."
                  << (has_previous_solution ? " (warm start)" : "") << std::endl;
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    bool result = solver.solve(assumptions);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    // Store solution if SAT
    if (result)
    {
        last_solution = solver.getAssignments();
        has_previous_solution = true;

        if (debug_output && has_previous_solution)
        {
            std::cout << "Updated warm start solution with " << last_solution.size() << " variables" << std::endl;
        }
    }

    if (debug_output)
    {
        std::cout << "  Result: " << (result ? "SAT" : "UNSAT") << " (time: " << duration << "ms)" << std::endl;
    }

    return result;
}

int MaxSATSolver::solve()
{
    // Reset warm start data at beginning of new solve
    has_previous_solution = false;
    last_solution.clear();

    if (relaxation_vars.empty())
    {
        // No soft clauses, just solve the hard clauses
        solver_calls++;
        bool result = solver.solve();
        return result ? 0 : -1; // -1 indicates unsatisfiable hard clauses
    }

    if (debug_output)
    {
        std::cout << "Starting linear search MaxSAT solver" << std::endl;
        std::cout << "Hard clauses: " << solver.getNumClauses() - relaxation_vars.size() << std::endl;
        std::cout << "Soft clauses: " << relaxation_vars.size() << std::endl;
    }

    std::vector<int> assumptions;

    // Try to satisfy all soft clauses first
    if (solveWithKRelaxed(0, assumptions))
    {
        return 0; // All clauses satisfied
    }

    // Linear search from 1 to number of soft clauses
    for (size_t k = 1; k <= relaxation_vars.size(); k++)
    {
        if (solveWithKRelaxed(k, assumptions))
        {
            // Found a solution with k relaxed clauses
            if (debug_output)
            {
                std::cout << "Found solution with " << k << " violated soft clauses" << std::endl;
                std::cout << "Total solver calls: " << solver_calls << std::endl;
            }
            return k;
        }
    }

    // If we get here, even relaxing all soft clauses didn't help
    if (debug_output)
    {
        std::cout << "Hard clauses are unsatisfiable!" << std::endl;
    }

    return -1; // Indicates unsatisfiable hard clauses
}

int MaxSATSolver::solveBinarySearch()
{
    // Reset warm start data at beginning of new solve
    has_previous_solution = false;
    last_solution.clear();

    if (relaxation_vars.empty())
    {
        // No soft clauses, just solve the hard clauses
        solver_calls++;
        bool result = solver.solve();
        return result ? 0 : -1; // -1 indicates unsatisfiable hard clauses
    }

    if (debug_output)
    {
        std::cout << "Starting binary search with improved exponential probing MaxSAT solver" << std::endl;
        std::cout << "Hard clauses: " << solver.getNumClauses() - relaxation_vars.size() << std::endl;
        std::cout << "Soft clauses: " << relaxation_vars.size() << std::endl;
    }

    std::vector<int> assumptions;

    // Try to satisfy all soft clauses first
    if (solveWithKRelaxed(0, assumptions))
    {
        return 0; // All clauses satisfied
    }

    // Improved exponential probing to find initial upper bound
    int lower_bound = 1; // We know 0 is unsatisfiable
    int upper_bound = 1; // Start with 1

    // Use a smarter initial step based on problem size
    int step_size = std::max(1, static_cast<int>(relaxation_vars.size() / 10));

    // Early estimation - solve with a small percentage of relaxed clauses
    int early_estimate = relaxation_vars.size() / 4; // Try 25% relaxed
    if (early_estimate > 1 && solveWithKRelaxed(early_estimate, assumptions))
    {
        // Found a satisfiable point, use it as upper bound
        upper_bound = early_estimate;

        if (debug_output)
        {
            std::cout << "Early estimation successful at k = " << early_estimate << std::endl;
        }
    }
    else
    {
        // Use exponential probing with adaptive step sizes
        while (upper_bound < relaxation_vars.size())
        {
            if (debug_output)
            {
                std::cout << "Probing at k = " << upper_bound << std::endl;
            }

            if (solveWithKRelaxed(upper_bound, assumptions))
            {
                // Found a satisfiable point, use this as upper bound
                break;
            }

            // Update bounds and increase step size
            lower_bound = upper_bound + 1;
            step_size = std::min(step_size * 2, static_cast<int>(relaxation_vars.size() / 2));
            upper_bound = std::min(upper_bound + step_size, static_cast<int>(relaxation_vars.size()));
        }
    }

    // If we've reached the maximum and it's still UNSAT, try with all variables relaxed
    if (upper_bound == relaxation_vars.size() && !solveWithKRelaxed(upper_bound, assumptions))
    {
        return -1; // Formula is UNSAT even with all soft clauses relaxed
    }

    if (debug_output)
    {
        std::cout << "Binary search range after probing: " << lower_bound << " to " << upper_bound << std::endl;
    }

    // Binary search within the identified range
    while (lower_bound < upper_bound)
    {
        int mid = lower_bound + (upper_bound - lower_bound) / 2;

        if (solveWithKRelaxed(mid, assumptions))
        {
            // We can satisfy with mid relaxed clauses, try fewer
            upper_bound = mid;
        }
        else
        {
            // Need more relaxed clauses
            lower_bound = mid + 1;
        }
    }

    // Verify the solution
    if (solveWithKRelaxed(lower_bound, assumptions))
    {
        if (debug_output)
        {
            std::cout << "Found solution with " << lower_bound << " violated soft clauses" << std::endl;
            std::cout << "Total solver calls: " << solver_calls << std::endl;
        }
        return lower_bound;
    }

    return -1; // Indicates unsatisfiable hard clauses
}

std::unordered_map<int, bool> MaxSATSolver::getAssignment() const
{
    return solver.getAssignments();
}

int MaxSATSolver::getNumHardClauses() const
{
    return solver.getNumClauses() - relaxation_vars.size();
}

int MaxSATSolver::getNumSoftClauses() const
{
    return relaxation_vars.size();
}

int MaxSATSolver::getNumVariables() const
{
    return solver.getNumVars();
}

int MaxSATSolver::getNumSolverCalls() const
{
    return solver_calls;
}