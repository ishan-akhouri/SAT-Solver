#include "HybridMaxSATSolver.h"
#include <algorithm>
#include <iostream>

HybridMaxSATSolver::HybridMaxSATSolver(const CNF &hard_clauses, bool debug)
    : hard_clauses(hard_clauses), debug_output(debug), solver_calls(0)
{
    // Initialize with default configuration
    config = Config();
}

void HybridMaxSATSolver::setConfig(const Config &new_config)
{
    config = new_config;
}

void HybridMaxSATSolver::addSoftClause(const Clause &soft_clause, int weight)
{
    if (soft_clause.empty() || weight <= 0)
        return;

    soft_clauses.push_back(soft_clause);
    weights.push_back(weight);

    if (debug_output)
    {
        std::cout << "Added soft clause with weight " << weight << ": ";
        for (int lit : soft_clause)
        {
            std::cout << lit << " ";
        }
        std::cout << std::endl;
    }
}

void HybridMaxSATSolver::addSoftClauses(const CNF &clauses, int weight)
{
    for (const auto &clause : clauses)
    {
        addSoftClause(clause, weight);
    }
}

bool HybridMaxSATSolver::isWeightedProblem() const
{
    // Check if we have weights other than 1
    if (weights.empty())
        return false;

    int first_weight = weights[0];
    for (int w : weights)
    {
        if (w != first_weight)
        {
            return true; // Found different weights, it's a weighted problem
        }
    }

    return false; // All weights are the same, treat as unweighted
}

HybridMaxSATSolver::Algorithm HybridMaxSATSolver::selectBestAlgorithm() const
{
    // Override with forced algorithms if specified
    if (config.force_binary)
    {
        if (debug_output)
        {
            std::cout << "Using binary search due to configuration override" << std::endl;
        }
        return Algorithm::BINARY_SEARCH;
    }

    if (config.force_stratified && isWeightedProblem())
    {
        if (debug_output)
        {
            std::cout << "Using stratified approach due to configuration override" << std::endl;
        }
        return Algorithm::STRATIFIED;
    }

    // Check if it's a weighted problem
    bool weighted = isWeightedProblem();

    if (weighted)
    {
        // Calculate statistical properties of weights for better decisions
        double mean = 0.0, max_weight = 0.0, min_weight = std::numeric_limits<double>::max();
        std::unordered_map<int, int> weight_counts;

        for (int w : weights)
        {
            mean += w;
            max_weight = std::max(max_weight, static_cast<double>(w));
            min_weight = std::min(min_weight, static_cast<double>(w));
            weight_counts[w]++;
        }
        mean /= weights.size();

        // Calculate weight variance
        double variance = 0.0;
        for (int w : weights)
        {
            variance += (w - mean) * (w - mean);
        }
        variance /= weights.size();
        double std_dev = std::sqrt(variance);
        double coeff_var = std_dev / mean;

        // Calculate weight range ratio
        double range_ratio = max_weight / (min_weight > 0 ? min_weight : 1.0);

        // Count unique weights
        size_t unique_weights = weight_counts.size();

        if (debug_output)
        {
            std::cout << "Weight statistics: mean=" << mean
                      << ", std_dev=" << std_dev
                      << ", coeff_var=" << coeff_var
                      << ", range_ratio=" << range_ratio
                      << ", unique_weights=" << unique_weights << std::endl;
        }

        // Based on benchmarks, stratified approach works better for weighted problems
        // Only consider alternatives for very specific cases
        if (weights.size() < 5 || unique_weights == 1)
        {
            // Very few clauses or uniform weights - binary search might be better
            if (debug_output)
            {
                std::cout << "Using binary search for weighted problem with few clauses/uniform weights" << std::endl;
            }
            return Algorithm::BINARY_SEARCH;
        }
        else if (coeff_var < 0.1 && range_ratio < 1.2)
        {
            // Very uniform weights - binary search might be competitive
            if (debug_output)
            {
                std::cout << "Using binary search for weighted problem with uniform weight distribution" << std::endl;
            }
            return Algorithm::BINARY_SEARCH;
        }
        else
        {
            // For most weighted problems, stratified is the best choice
            if (debug_output)
            {
                std::cout << "Using stratified approach for weighted problem" << std::endl;
            }
            return Algorithm::STRATIFIED;
        }
    }
    else
    {
        // Problem is unweighted - based on benchmarks, binary search is superior

        // Calculate clause density
        double var_count = static_cast<double>(getNumVariables());
        double clause_density = (hard_clauses.size() + soft_clauses.size()) / (var_count > 0 ? var_count : 1.0);

        // Calculate clause size statistics
        double avg_clause_size = 0.0;
        for (const auto &clause : hard_clauses)
        {
            avg_clause_size += clause.size();
        }
        for (const auto &clause : soft_clauses)
        {
            avg_clause_size += clause.size();
        }
        avg_clause_size /= (hard_clauses.size() + soft_clauses.size());

        if (debug_output)
        {
            std::cout << "Unweighted problem: vars=" << getNumVariables()
                      << ", hard=" << hard_clauses.size()
                      << ", soft=" << soft_clauses.size()
                      << ", density=" << clause_density
                      << ", avg_size=" << avg_clause_size << std::endl;
        }

        // Only use linear search for very small problems
        if (soft_clauses.size() < 10 && clause_density < 3.0)
        {
            if (debug_output)
            {
                std::cout << "Using linear search for small unweighted problem" << std::endl;
            }
            return Algorithm::LINEAR;
        }
        else
        {
            // For most unweighted problems, binary search is better
            if (debug_output)
            {
                std::cout << "Using binary search for unweighted problem" << std::endl;
            }
            return Algorithm::BINARY_SEARCH;
        }
    }
}

int HybridMaxSATSolver::solve()
{
    // Select the best algorithm based on problem characteristics
    Algorithm algo = selectBestAlgorithm();

    if (debug_output)
    {
        std::cout << "Hybrid solver selected algorithm: ";
        switch (algo)
        {
        case Algorithm::LINEAR:
            std::cout << "Linear search";
            break;
        case Algorithm::BINARY_SEARCH:
            std::cout << "Binary search with "
                      << (config.use_exponential_probe ? "" : "no ")
                      << "exponential probing";
            break;
        case Algorithm::STRATIFIED:
            std::cout << "Stratified approach";
            break;
        }
        std::cout << " with " << (config.use_warm_start ? "" : "no ")
                  << "warm starting" << std::endl;
    }

    // Call the appropriate algorithm
    switch (algo)
    {
    case Algorithm::LINEAR:
        return solveLinear();
    case Algorithm::BINARY_SEARCH:
        return solveBinary();
    case Algorithm::STRATIFIED:
        return solveStratified();
    default:
        return -1; // Should never reach here
    }
}

int HybridMaxSATSolver::solveLinear()
{
    // Create a MaxSAT solver and use linear search
    MaxSATSolver solver(hard_clauses, debug_output);

    // Add all soft clauses
    for (size_t i = 0; i < soft_clauses.size(); i++)
    {
        solver.addSoftClause(soft_clauses[i], weights[i]);
    }

    // Solve with linear search
    int result = solver.solve();
    solver_calls += solver.getNumSolverCalls();

    // Store the assignment if successful
    if (result >= 0)
    {
        last_assignment = solver.getAssignment();
    }

    return result;
}

int HybridMaxSATSolver::solveBinary()
{
    // Create a MaxSAT solver and use binary search
    MaxSATSolver solver(hard_clauses, debug_output);

    // Add all soft clauses
    for (size_t i = 0; i < soft_clauses.size(); i++)
    {
        solver.addSoftClause(soft_clauses[i], weights[i]);
    }

    // Solve with binary search
    int result = solver.solveBinarySearch();
    solver_calls += solver.getNumSolverCalls();

    // Store the assignment if successful
    if (result >= 0)
    {
        last_assignment = solver.getAssignment();
    }

    return result;
}

int HybridMaxSATSolver::solveStratified()
{
    // Only applicable for weighted problems
    if (!isWeightedProblem())
    {
        if (debug_output)
        {
            std::cout << "Warning: Stratified approach requested for unweighted problem. "
                      << "Using binary search instead." << std::endl;
        }
        return solveBinary();
    }

    // Create a weighted MaxSAT solver
    WeightedMaxSATSolver solver(hard_clauses, debug_output);

    // Add all soft clauses with their weights
    for (size_t i = 0; i < soft_clauses.size(); i++)
    {
        solver.addSoftClause(soft_clauses[i], weights[i]);
    }

    // Solve with stratified approach
    int result = solver.solveStratified();
    solver_calls += solver.getNumSolverCalls();

    // Unable to get assignment directly from WeightedMaxSATSolver,
    // so we need to ensure this implementation can access it

    return result;
}

std::unordered_map<int, bool> HybridMaxSATSolver::getAssignment() const
{
    return last_assignment;
}

int HybridMaxSATSolver::getNumHardClauses() const
{
    return hard_clauses.size();
}

int HybridMaxSATSolver::getNumSoftClauses() const
{
    return soft_clauses.size();
}

int HybridMaxSATSolver::getNumVariables() const
{
    // Find the max variable ID
    int max_var = 0;

    // Check hard clauses
    for (const auto &clause : hard_clauses)
    {
        for (int lit : clause)
        {
            max_var = std::max(max_var, std::abs(lit));
        }
    }

    // Check soft clauses
    for (const auto &clause : soft_clauses)
    {
        for (int lit : clause)
        {
            max_var = std::max(max_var, std::abs(lit));
        }
    }

    return max_var;
}

int HybridMaxSATSolver::getNumSolverCalls() const
{
    return solver_calls;
}