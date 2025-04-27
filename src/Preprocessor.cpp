#include "../include/Preprocessor.h"
#include <algorithm>
#include <iostream>
#include <set>

// PreprocessorConfig implementation
void PreprocessorConfig::adaptToType(ProblemType type)
{
    switch (type)
    {
    case ProblemType::NQUEENS:
        // For N-Queens, preserve structure but allow limited optimizations
        technique_enablement[type]["unit_propagation"] = true;
        technique_enablement[type]["pure_literal_elimination"] = true;
        technique_enablement[type]["subsumption"] = true;
        technique_enablement[type]["failed_literal"] = true;
        technique_enablement[type]["variable_elimination"] = false; // Don't eliminate variables for N-Queens
        technique_enablement[type]["blocked_clause"] = false;
        technique_enablement[type]["self_subsumption"] = true;

        // Enable all phases but with appropriate techniques
        enable_initial_phase = true;
        enable_structural_phase = true;
        enable_aggressive_phase = true;
        enable_final_phase = true;
        break;

    case ProblemType::PIGEONHOLE:
        // For Pigeonhole, allow more aggressive preprocessing
        technique_enablement[type]["unit_propagation"] = true;
        technique_enablement[type]["pure_literal_elimination"] = true;
        technique_enablement[type]["subsumption"] = true;
        technique_enablement[type]["failed_literal"] = true;
        technique_enablement[type]["variable_elimination"] = true; // Variable elimination can help pigeonhole
        technique_enablement[type]["blocked_clause"] = false;
        technique_enablement[type]["self_subsumption"] = true;

        // Enable all phases
        enable_initial_phase = true;
        enable_structural_phase = true;
        enable_aggressive_phase = true;
        enable_final_phase = true;
        break;

    default:
        // For generic problems, use all techniques
        technique_enablement[type]["unit_propagation"] = true;
        technique_enablement[type]["pure_literal_elimination"] = true;
        technique_enablement[type]["subsumption"] = true;
        technique_enablement[type]["failed_literal"] = true;
        technique_enablement[type]["variable_elimination"] = true;
        technique_enablement[type]["blocked_clause"] = true;
        technique_enablement[type]["self_subsumption"] = true;

        // Enable all phases
        enable_initial_phase = true;
        enable_structural_phase = true;
        enable_aggressive_phase = true;
        enable_final_phase = true;
        break;
    }
}

// PreprocessingStats implementation
void PreprocessingStats::updateTechniqueTiming(const std::string &technique,
                                               std::chrono::microseconds elapsed)
{
    technique_times[technique] = elapsed;
}

void PreprocessingStats::updatePhaseTiming(PreprocessingPhase phase,
                                           std::chrono::microseconds elapsed)
{
    phase_times[phase] = elapsed;
}

void PreprocessingStats::calculateReductions()
{
    if (original_variables > 0)
    {
        variables_reduction_percent = 100.0 * (original_variables - simplified_variables) / original_variables;
    }

    if (original_clauses > 0)
    {
        clauses_reduction_percent = 100.0 * (original_clauses - simplified_clauses) / original_clauses;
    }
}

// Preprocessor implementation
Preprocessor::Preprocessor(const PreprocessorConfig &conf)
    : problem_type(ProblemType::GENERIC), config(conf)
{
    // Initialize any additional structures if needed
    variable_map.clear();
}

CNF Preprocessor::preprocess(const CNF &formula)
{
    // Start timing
    auto start_time = std::chrono::high_resolution_clock::now();

    // Store original formula metrics
    stats.original_variables = countVariables(formula);
    stats.original_clauses = formula.size();

    // Check assumptions for contradictions
    std::unordered_map<int, bool> assumption_values;
    bool has_contradiction = false;

    for (int lit : assumption_literals)
    {
        int var = std::abs(lit);
        bool value = (lit > 0);

        auto it = assumption_values.find(var);
        if (it != assumption_values.end() && it->second != value)
        {
            has_contradiction = true;
            break;
        }

        assumption_values[var] = value;
    }

    // If assumptions are contradictory, formula is UNSAT
    if (has_contradiction)
    {
        CNF unsat_result;
        unsat_result.push_back(Clause{}); // Empty clause indicates UNSAT
        return unsat_result;
    }

    // Detect problem type
    problem_type = detectProblemType(formula);

    // Adapt configuration to problem type
    config.adaptToType(problem_type);

    // Initialize result with original formula
    CNF result = formula;

    // Initialize clause metadata
    initializeClauseMeta(result);

    // Execute each phase conditionally
    if (config.enable_initial_phase)
    {
        executePhase(PreprocessingPhase::INITIAL, result);
    }

    if (config.enable_structural_phase)
    {
        executePhase(PreprocessingPhase::STRUCTURAL_PRESERVE, result);
    }

    if (config.enable_aggressive_phase)
    {
        executePhase(PreprocessingPhase::AGGRESSIVE, result);
    }

    if (config.enable_final_phase)
    {
        executePhase(PreprocessingPhase::FINAL, result);
    }

    // Update final statistics
    stats.simplified_variables = countVariables(result);
    stats.simplified_clauses = result.size();
    stats.calculateReductions();

    // Record total time
    auto end_time = std::chrono::high_resolution_clock::now();
    stats.total_time = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);

    return result;
}

ProblemType Preprocessor::detectProblemType(const CNF &formula)
{
    // Count variables and gather structural information
    int num_variables = countVariables(formula);
    int num_clauses = formula.size();

    // Track clause lengths and special patterns
    std::map<int, int> clause_length_counts;
    int binary_negative_clauses = 0;
    int at_least_one_clauses = 0;
    int diagonal_conflicts = 0;

    // Variables for more detailed pattern detection
    std::set<int> row_constraints_seen;
    std::set<int> col_constraints_seen;

    // Analyze clauses
    for (const auto &clause : formula)
    {
        // Count by length
        clause_length_counts[clause.size()]++;

        // Check for at-least-one constraints (all positive literals)
        bool all_positive = true;
        for (int lit : clause)
        {
            if (lit < 0)
            {
                all_positive = false;
                break;
            }
        }

        if (all_positive && clause.size() > 1)
        {
            at_least_one_clauses++;

            // Additional pattern detection for N-Queens
            if (isPerfectSquare(num_variables))
            {
                int n = static_cast<int>(std::sqrt(num_variables));

                // Check if this looks like a row or column constraint in N-Queens
                if (clause.size() == n)
                {
                    // See if all variables are in the same row or column
                    int row = -1, col = -1;
                    bool same_row = true, same_col = true;

                    for (int lit : clause)
                    {
                        int var = abs(lit);
                        int curr_row = (var - 1) / n;
                        int curr_col = (var - 1) % n;

                        if (row == -1)
                            row = curr_row;
                        else if (row != curr_row)
                            same_row = false;

                        if (col == -1)
                            col = curr_col;
                        else if (col != curr_col)
                            same_col = false;
                    }

                    if (same_row)
                        row_constraints_seen.insert(row);
                    if (same_col)
                        col_constraints_seen.insert(col);
                }
            }
        }

        // Check for at-most-one constraints (all negative binary clauses)
        if (clause.size() == 2 && clause[0] < 0 && clause[1] < 0)
        {
            binary_negative_clauses++;

            // Fast-check for diagonal conflicts in N-Queens
            if (isPerfectSquare(num_variables))
            {
                int n = static_cast<int>(std::sqrt(num_variables));
                int var1 = abs(clause[0]);
                int var2 = abs(clause[1]);

                // Compute positions for a standard N-Queens encoding
                int row1 = (var1 - 1) / n;
                int col1 = (var1 - 1) % n;
                int row2 = (var2 - 1) / n;
                int col2 = (var2 - 1) % n;

                // Check if these positions are on the same diagonal
                if (abs(row1 - row2) == abs(col1 - col2))
                {
                    diagonal_conflicts++;
                }
            }
        }
    }

    // 1. N-Queens detection with fast-pass check
    bool perfect_square = isPerfectSquare(num_variables);
    if (perfect_square)
    {
        int n = static_cast<int>(std::sqrt(num_variables));

        // Expected counts for N-Queens
        int expected_row_col_constraints = 2 * n;      // Both row and column constraints
        int expected_diagonal_conflicts = n * (n - 1); // Minimum expected diagonal conflicts

        // Fast-pass check for N-Queens pattern:
        // 1. Do we have close to the right number of row/column constraints?
        // 2. Do we have a significant number of diagonal conflicts?
        // 3. Do we have the expected row and column constraint coverage?
        bool has_enough_row_col = row_constraints_seen.size() + col_constraints_seen.size() >= n * 1.5;
        bool has_enough_diagonals = diagonal_conflicts >= expected_diagonal_conflicts * 0.7;

        if (has_enough_row_col && has_enough_diagonals)
        {
            // Detailed validation of N-Queens structure
            bool structure_valid = validateNQueensStructure(formula, n);

            if (structure_valid)
            {
                std::cout << "Detected N-Queens problem of size " << n << "x" << n << std::endl;
                return ProblemType::NQUEENS;
            }
        }
    }

    // 2. Pigeonhole detection (strict structure checking)
    for (int m = 2; m <= 20; m++)
    { // Try reasonable pigeon counts
        for (int n = 2; n <= 20; n++)
        {
            if (m * n == num_variables)
            {
                // Calculate exact expected counts based on Pigeonhole structure
                int expected_pigeon_clauses = m;                   // Each pigeon in at least one hole
                int expected_hole_clauses = n * (m * (m - 1) / 2); // No two pigeons in same hole
                int expected_total_clauses = expected_pigeon_clauses + expected_hole_clauses;

                // Check for exact match of total clauses
                if (num_clauses == expected_total_clauses)
                {
                    // Check for exact clause length distribution
                    bool has_exact_pigeon_clauses = (clause_length_counts[n] == expected_pigeon_clauses);
                    bool has_exact_hole_clauses = (binary_negative_clauses == expected_hole_clauses);

                    // Only do full validation if the clause counts exactly match
                    if (has_exact_pigeon_clauses && has_exact_hole_clauses)
                    {
                        bool structure_valid = validatePigeonholeStructure(formula, m, n);

                        if (structure_valid)
                        {
                            std::cout << "Detected Pigeonhole problem with "
                                      << m << " pigeons and " << n << " holes" << std::endl;
                            return ProblemType::PIGEONHOLE;
                        }
                    }
                }
            }
        }
    }

    // 3. Graph coloring detection
    if (binary_negative_clauses > 0 && at_least_one_clauses > 0)
    {
        for (int v = 3; v <= 100; v++)
        {
            for (int c = 2; c <= 10; c++)
            {
                if (v * c == num_variables)
                {
                    // Expected constraint counts
                    int expected_vertex_constraints = v;                  // Each vertex has at least one color
                    int expected_different_color = (v * (v - 1) / 2) * c; // Adjacent vertices, different colors

                    // Fast-pass check for Graph Coloring
                    bool vertex_constraint_match = abs(at_least_one_clauses - expected_vertex_constraints) <= 1;
                    bool edge_constraint_match = binary_negative_clauses >= expected_different_color * 0.5;

                    if (vertex_constraint_match && edge_constraint_match)
                    {
                        // Verify structure
                        bool structure_valid = validateGraphColoringStructure(formula, v, c);

                        if (structure_valid)
                        {
                            std::cout << "Detected Graph Coloring problem with "
                                      << v << " vertices and " << c << " colors" << std::endl;
                            return ProblemType::GRAPH_COLORING;
                        }
                    }
                }
            }
        }
    }

    // 4. Hamiltonian cycle detection
    if (perfect_square)
    {
        int n = static_cast<int>(std::sqrt(num_variables));

        // Expected constraints for position-vertex encoding
        int expected_position_constraints = n; // Each position visited once
        int expected_vertex_constraints = n;   // Each vertex visited once

        // Fast-pass check for Hamiltonian Cycle
        bool position_vertex_match = abs(at_least_one_clauses - (expected_position_constraints + expected_vertex_constraints)) <= 2;
        bool has_large_clauses = clause_length_counts[n] >= n * 0.9;

        if (position_vertex_match && has_large_clauses)
        {
            // Validate structure
            bool structure_valid = validateHamiltonianStructure(formula, n);

            if (structure_valid)
            {
                std::cout << "Detected Hamiltonian problem with "
                          << n << " vertices" << std::endl;
                return ProblemType::HAMILTONIAN;
            }
        }
    }

    // If no specific problem detected, return GENERIC
    return ProblemType::GENERIC;
}

void Preprocessor::executePhase(PreprocessingPhase phase, CNF &formula)
{
    // Start phase timing
    auto phase_start = std::chrono::high_resolution_clock::now();

    // Take snapshot for later comparison
    snapshotFormula(phase, formula);

    // Apply appropriate techniques based on problem type and phase
    switch (phase)
    {
    case PreprocessingPhase::INITIAL:
        // Initial basic preprocessing is good for all problem types
        if (shouldApplyTechnique("unit_propagation"))
            formula = unitPropagation(formula);
        if (shouldApplyTechnique("pure_literal_elimination"))
            formula = pureLiteralElimination(formula);
        if (shouldApplyTechnique("subsumption"))
            formula = performBasicSubsumption(formula);
        break;

    case PreprocessingPhase::STRUCTURAL_PRESERVE:
        // For structured problems, be careful with techniques that change structure
        if (problem_type == ProblemType::NQUEENS || problem_type == ProblemType::PIGEONHOLE)
        {
            // Use lighter techniques for structured problems
            if (shouldApplyTechnique("subsumption"))
                formula = performBasicSubsumption(formula);
            if (shouldApplyTechnique("self_subsumption"))
                formula = performSelfSubsumption(formula);
        }
        else
        {
            // For general problems, use more aggressive techniques
            if (shouldApplyTechnique("subsumption"))
                formula = performBasicSubsumption(formula);
            if (shouldApplyTechnique("self_subsumption"))
                formula = performSelfSubsumption(formula);
            if (shouldApplyTechnique("variable_elimination") && problem_type == ProblemType::GENERIC)
                formula = eliminateVariables(formula);
        }
        break;

    case PreprocessingPhase::AGGRESSIVE:
        // Apply aggressive techniques selectively based on problem type
        if (problem_type == ProblemType::GENERIC)
        {
            // Full aggressive preprocessing for generic problems
            if (shouldApplyTechnique("failed_literal"))
                formula = detectFailedLiterals(formula);
            if (shouldApplyTechnique("variable_elimination"))
                formula = eliminateVariables(formula);
            if (shouldApplyTechnique("blocked_clause"))
                formula = eliminateBlockedClauses(formula);
        }
        else if (problem_type == ProblemType::PIGEONHOLE)
        {
            // For Pigeonhole, variable elimination can help if done carefully
            if (shouldApplyTechnique("variable_elimination"))
                formula = eliminateVariables(formula);
            if (shouldApplyTechnique("failed_literal"))
                formula = detectFailedLiterals(formula);
        }
        else if (problem_type == ProblemType::NQUEENS)
        {
            // For N-Queens, be more conservative with aggressive techniques
            if (shouldApplyTechnique("failed_literal"))
                formula = detectFailedLiterals(formula);
            // Add self-subsumption for N-Queens too
            if (shouldApplyTechnique("self_subsumption"))
                formula = performSelfSubsumption(formula);
        }
        break;

    case PreprocessingPhase::FINAL:
        // Final unit propagation is useful for all problem types
        if (shouldApplyTechnique("unit_propagation"))
            formula = finalUnitPropagation(formula);
        break;
    }

    // Record phase timing
    auto phase_end = std::chrono::high_resolution_clock::now();
    stats.updatePhaseTiming(phase, std::chrono::duration_cast<std::chrono::microseconds>(
                                       phase_end - phase_start));
}

void Preprocessor::snapshotFormula(PreprocessingPhase phase, const CNF &formula)
{
    phase_snapshots[phase] = formula;
}

bool Preprocessor::shouldApplyTechnique(const std::string &technique)
{
    // Check basic enablement in config
    bool enabled = false;

    if (technique == "unit_propagation")
        enabled = config.use_unit_propagation;
    else if (technique == "pure_literal_elimination")
        enabled = config.use_pure_literal_elimination;
    else if (technique == "subsumption")
        enabled = config.use_subsumption;
    else if (technique == "failed_literal")
        enabled = config.use_failed_literal;
    else if (technique == "variable_elimination")
        enabled = config.use_variable_elimination;
    else if (technique == "blocked_clause")
        enabled = config.use_blocked_clause;
    else if (technique == "self_subsumption")
        enabled = config.use_self_subsumption;
    else
        return false; // Unknown technique

    // If not enabled in base config, return false
    if (!enabled)
    {
        return false;
    }

    // Check problem-specific enablement
    if (config.technique_enablement.find(problem_type) != config.technique_enablement.end())
    {
        auto &problem_config = config.technique_enablement[problem_type];
        if (problem_config.find(technique) != problem_config.end())
        {
            return problem_config[technique];
        }
    }

    // Default to enabled if no specific override
    return true;
}

int Preprocessor::countVariables(const CNF &formula)
{
    std::set<int> variables;
    for (const auto &clause : formula)
    {
        for (int literal : clause)
        {
            variables.insert(std::abs(literal));
        }
    }
    return variables.size();
}

std::unordered_map<int, bool> Preprocessor::mapSolutionToOriginal(
    const std::unordered_map<int, bool> &solution)
{

    std::unordered_map<int, bool> original_solution;

    // First, add all fixed variables
    original_solution = fixed_variables;

    // Handle eliminated variables
    for (const auto &[var, mapped_var] : variable_map)
    {
        if (mapped_var == -1)
        {
            // This was an eliminated variable
            // We need to infer its value from the remaining formula
            // For now, assign a default value (false)
            // A more accurate approach would be to check if the variable's assignment
            // is forced by the remaining formula and fixed variables
            original_solution[var] = false;
        }
        else if (mapped_var > 0)
        {
            // This variable was mapped to another
            auto it = solution.find(mapped_var);
            if (it != solution.end())
            {
                original_solution[var] = it->second;
            }
        }
    }

    // Then map variables from the simplified solution
    for (const auto &[var, value] : solution)
    {
        // If this variable wasn't eliminated or mapped, copy it directly
        if (variable_map.find(var) == variable_map.end())
        {
            original_solution[var] = value;
        }
    }

    return original_solution;
}

PreprocessingStats Preprocessor::getStats() const
{
    return stats;
}

void Preprocessor::printStats() const
{
    std::cout << "Preprocessing Statistics:\n";
    std::cout << "  Original variables: " << stats.original_variables << "\n";
    std::cout << "  Original clauses: " << stats.original_clauses << "\n";
    std::cout << "  Simplified variables: " << stats.simplified_variables << "\n";
    std::cout << "  Simplified clauses: " << stats.simplified_clauses << "\n";
    std::cout << "  Variable reduction: " << stats.variables_reduction_percent << "%\n";
    std::cout << "  Clause reduction: " << stats.clauses_reduction_percent << "%\n";
    std::cout << "  Total time: " << stats.total_time.count() << " μs\n";

    std::cout << "  Technique timings:\n";
    for (const auto &[technique, time] : stats.technique_times)
    {
        std::cout << "    " << technique << ": " << time.count() << " μs\n";
    }

    std::cout << "  Phase timings:\n";
    for (const auto &[phase, time] : stats.phase_times)
    {
        std::cout << "    Phase " << static_cast<int>(phase) << ": " << time.count() << " μs\n";
    }
}

// Core preprocessing techniques implementation
CNF Preprocessor::unitPropagation(CNF &formula)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    std::unordered_map<int, bool> assignments;

    // Initialize assignments with assumptions
    for (int lit : assumption_literals)
    {
        int var = std::abs(lit);
        bool value = (lit > 0);
        assignments[var] = value;
    }

    bool changes = true;

    while (changes)
    {
        changes = false;

        for (auto it = formula.begin(); it != formula.end();)
        {
            // Check for tautologies using a set for O(1) lookups
            bool is_tautology = false;
            std::unordered_set<int> lit_set(it->begin(), it->end());

            for (int lit : lit_set)
            {
                if (lit_set.count(-lit))
                {
                    is_tautology = true;
                    break;
                }
            }

            if (is_tautology)
            {
                // Tautology is always satisfied, remove it
                it = formula.erase(it);
                stats.clauses_removed++;
                changes = true;
                continue;
            }

            // Check for unit clauses
            if (it->size() == 1)
            {
                int literal = (*it)[0];
                int var = std::abs(literal);
                bool value = (literal > 0);

                // Check if this is an assumption variable
                bool is_assumption = std::find(assumption_literals.begin(),
                                               assumption_literals.end(),
                                               literal) != assumption_literals.end();

                // Check for conflicts
                auto existing = assignments.find(var);
                if (existing != assignments.end() && existing->second != value)
                {
                    // Contradiction found - formula is UNSAT
                    formula.clear();
                    formula.push_back(Clause{}); // Empty clause indicates UNSAT

                    auto end_time = std::chrono::high_resolution_clock::now();
                    stats.updateTechniqueTiming("unit_propagation",
                                                std::chrono::duration_cast<std::chrono::microseconds>(
                                                    end_time - start_time));
                    return formula;
                }

                // Record assignment
                assignments[var] = value;

                // Only add to fixed variables if it's not an assumption
                // (assumptions are already in fixed_variables)
                if (!is_assumption)
                {
                    fixed_variables[var] = value;
                    stats.variables_fixed++;
                }

                // Remove this unit clause
                it = formula.erase(it);
                stats.clauses_removed++;
                changes = true;

                // Now apply this assignment to all other clauses
                for (auto clauses_it = formula.begin(); clauses_it != formula.end();)
                {
                    bool clause_satisfied = false;

                    for (auto lit_it = clauses_it->begin(); lit_it != clauses_it->end();)
                    {
                        int clause_lit = *lit_it;
                        int clause_var = std::abs(clause_lit);

                        if (clause_var == var)
                        {
                            // This literal contains our assigned variable
                            if ((clause_lit > 0 && value) || (clause_lit < 0 && !value))
                            {
                                // Clause is satisfied
                                clause_satisfied = true;
                                break;
                            }
                            else
                            {
                                // This literal is false, remove it
                                lit_it = clauses_it->erase(lit_it);
                                continue;
                            }
                        }
                        ++lit_it;
                    }

                    if (clause_satisfied)
                    {
                        // Remove satisfied clause
                        clauses_it = formula.erase(clauses_it);
                        stats.clauses_removed++;
                    }
                    else if (clauses_it->empty())
                    {
                        // Empty clause - formula is UNSAT
                        formula.clear();
                        formula.push_back(Clause{}); // Empty clause indicates UNSAT

                        auto end_time = std::chrono::high_resolution_clock::now();
                        stats.updateTechniqueTiming("unit_propagation",
                                                    std::chrono::duration_cast<std::chrono::microseconds>(
                                                        end_time - start_time));
                        return formula;
                    }
                    else
                    {
                        ++clauses_it;
                    }
                }

                // Restart from the beginning
                break;
            }
            else
            {
                ++it;
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    stats.updateTechniqueTiming("unit_propagation",
                                std::chrono::duration_cast<std::chrono::microseconds>(
                                    end_time - start_time));

    return formula;
}

CNF Preprocessor::pureLiteralElimination(CNF &formula)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    bool changes = true;

    while (changes)
    {
        changes = false;

        // First, remove tautologies
        auto it = formula.begin();
        while (it != formula.end())
        {
            bool is_tautology = false;
            std::unordered_set<int> lit_set(it->begin(), it->end());

            for (int lit : lit_set)
            {
                if (lit_set.count(-lit))
                {
                    is_tautology = true;
                    break;
                }
            }

            if (is_tautology)
            {
                it = formula.erase(it);
                stats.clauses_removed++;
                changes = true;
            }
            else
            {
                ++it;
            }
        }

        // Count occurrences of each literal
        std::unordered_map<int, int> literal_count;

        for (const auto &clause : formula)
        {
            for (int literal : clause)
            {
                literal_count[literal]++;
            }
        }

        // Find pure literals (those whose negation doesn't appear)
        std::vector<int> pure_literals;

        for (const auto &[literal, count] : literal_count)
        {
            if (literal_count.find(-literal) == literal_count.end())
            {
                // Check if this conflicts with an assumption
                int var = std::abs(literal);
                bool value = (literal > 0);

                // Get assumption value if any
                bool has_assumption = false;
                bool assumption_value = false;

                for (int assumption : assumption_literals)
                {
                    if (std::abs(assumption) == var)
                    {
                        has_assumption = true;
                        assumption_value = (assumption > 0);
                        break;
                    }
                }

                // If this is an assumption variable and the pure literal
                // conflicts with the assumption, skip it
                if (has_assumption && assumption_value != value)
                {
                    continue;
                }

                pure_literals.push_back(literal);

                // Record this assignment if it's not already an assumption
                if (!has_assumption)
                {
                    fixed_variables[var] = value;
                    stats.variables_fixed++;
                }

                changes = true;
            }
        }

        // Remove clauses containing pure literals
        if (!pure_literals.empty())
        {
            auto it = formula.begin();
            while (it != formula.end())
            {
                bool remove_clause = false;

                for (int pure_lit : pure_literals)
                {
                    if (std::find(it->begin(), it->end(), pure_lit) != it->end())
                    {
                        remove_clause = true;
                        break;
                    }
                }

                if (remove_clause)
                {
                    it = formula.erase(it);
                    stats.clauses_removed++;
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    stats.updateTechniqueTiming("pure_literal_elimination",
                                std::chrono::duration_cast<std::chrono::microseconds>(
                                    end_time - start_time));

    return formula;
}

CNF Preprocessor::performBasicSubsumption(CNF &formula)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    // Make a copy of the formula to work with
    CNF result;

    // Sort each clause internally for faster subsumption checking
    for (auto &clause : formula)
    {
        // Remove duplicate literals
        std::sort(clause.begin(), clause.end());
        clause.erase(std::unique(clause.begin(), clause.end()), clause.end());

        // Check for tautologies (clause contains both p and ¬p)
        bool is_tautology = false;
        for (size_t i = 1; i < clause.size(); ++i)
        {
            if (clause[i - 1] == -clause[i])
            {
                is_tautology = true;
                break;
            }
        }

        if (!is_tautology)
        {
            result.push_back(clause);
        }
        else
        {
            stats.clauses_removed++;
        }
    }

    // Sort clauses by size for more efficient subsumption checks
    std::sort(result.begin(), result.end(),
              [](const Clause &a, const Clause &b)
              {
                  return a.size() < b.size();
              });

    // Use a hashmap to group clauses by first literal for faster filtering
    std::unordered_map<int, std::vector<size_t>> clauses_by_first_lit;

    for (size_t i = 0; i < result.size(); i++)
    {
        if (!result[i].empty())
        {
            clauses_by_first_lit[result[i][0]].push_back(i);
        }
    }

    // Mark subsumed clauses
    std::vector<bool> subsumed(result.size(), false);

    for (size_t i = 0; i < result.size(); i++)
    {
        if (subsumed[i])
            continue; // Skip already subsumed clauses

        const Clause &c1 = result[i];

        // Skip empty clauses
        if (c1.empty())
            continue;

        // Get metadata for c1
        bool c1_is_structural = false;
        if (i < clauses_meta.size())
        {
            c1_is_structural = clauses_meta[i].is_structural;
        }

        // Only consider clauses that start with the same literal
        // or clauses that have all the literals in c1
        std::vector<size_t> candidate_indices;

        // Add clauses that start with the first literal of c1
        if (!c1.empty())
        {
            auto it = clauses_by_first_lit.find(c1[0]);
            if (it != clauses_by_first_lit.end())
            {
                candidate_indices = it->second;
            }
        }

        // Filter candidates by size
        for (auto j_it = candidate_indices.begin(); j_it != candidate_indices.end();)
        {
            size_t j = *j_it;
            // Clauses earlier than i have already been processed
            // Clauses of the same size or smaller cannot be subsumed by c1
            if (j <= i || result[j].size() <= c1.size())
            {
                j_it = candidate_indices.erase(j_it);
            }
            else
            {
                ++j_it;
            }
        }

        for (size_t j : candidate_indices)
        {
            if (subsumed[j])
                continue; // Skip already subsumed clauses

            const Clause &c2 = result[j];

            // Get metadata for c2
            bool c2_is_structural = false;
            if (j < clauses_meta.size())
            {
                c2_is_structural = clauses_meta[j].is_structural;
            }

            // Don't remove structural clauses in basic subsumption
            if (c2_is_structural)
                continue;

            // Check if c1 subsumes c2 using std::includes
            // This works because we've sorted the clauses
            if (std::includes(c2.begin(), c2.end(), c1.begin(), c1.end()))
            {
                // c1 subsumes c2, mark c2 as subsumed
                subsumed[j] = true;
                stats.clauses_removed++;
            }
        }
    }

    // Build the result formula without subsumed clauses
    CNF final_result;
    for (size_t i = 0; i < result.size(); i++)
    {
        if (!subsumed[i])
        {
            final_result.push_back(result[i]);
        }
    }

    // Update metadata for the result
    updateClauseMeta(final_result);

    auto end_time = std::chrono::high_resolution_clock::now();
    stats.updateTechniqueTiming("subsumption",
                                std::chrono::duration_cast<std::chrono::microseconds>(
                                    end_time - start_time));

    return final_result;
}

CNF Preprocessor::finalUnitPropagation(CNF &formula)
{
    // Final unit propagation is the same as regular unit propagation
    return unitPropagation(formula);
}

CNF Preprocessor::detectFailedLiterals(CNF &formula)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    // Collect all variables in the formula
    std::set<int> variables;
    for (const auto &clause : formula)
    {
        for (int literal : clause)
        {
            variables.insert(std::abs(literal));
        }
    }

    bool changed = true;
    int iterations = 0;
    int failed_literals_found = 0;

    // Continue until no more failed literals are found
    while (changed && iterations < 3)
    { // Limit iterations to avoid excessive time
        changed = false;
        iterations++;

        for (int var : variables)
        {
            // Skip already assigned variables
            if (fixed_variables.find(var) != fixed_variables.end())
            {
                continue;
            }

            // Try assigning var = true
            std::unordered_map<int, bool> implications;
            int result_pos = testLiteralAssignment(formula, var, true, implications);

            if (result_pos == -1)
            {
                // Contradiction found, var must be false
                fixed_variables[var] = false;

                // Add a unit clause for this assignment
                Clause unit_clause = {-var};
                formula.push_back(unit_clause);

                failed_literals_found++;
                changed = true;
                stats.variables_fixed++;

                // Re-propagate with this new information
                CNF simplified = unitPropagation(formula);
                if (simplified.empty() || (simplified.size() == 1 && simplified[0].empty()))
                {
                    // Formula is unsatisfiable
                    formula = simplified;
                    return formula;
                }
                formula = simplified;
                break; // Break to restart with the modified formula
            }
            else if (result_pos > 0)
            {
                // Found some implied assignments, add them to the formula
                for (const auto &[implied_var, implied_val] : implications)
                {
                    // Skip the test variable itself
                    if (implied_var == var)
                        continue;

                    if (fixed_variables.find(implied_var) == fixed_variables.end())
                    {
                        int lit = implied_val ? implied_var : -implied_var;
                        fixed_variables[implied_var] = implied_val;
                        Clause unit_clause = {lit};
                        formula.push_back(unit_clause);
                        stats.variables_fixed++;

                        changed = true;
                    }
                }

                if (changed)
                {
                    // Re-propagate with this new information
                    CNF simplified = unitPropagation(formula);
                    if (simplified.empty() || (simplified.size() == 1 && simplified[0].empty()))
                    {
                        // Formula is unsatisfiable
                        formula = simplified;
                        return formula;
                    }
                    formula = simplified;
                    break; // Break to restart with the modified formula
                }
            }

            // Try assigning var = false
            implications.clear();
            int result_neg = testLiteralAssignment(formula, var, false, implications);

            if (result_neg == -1)
            {
                // Contradiction found, var must be true
                fixed_variables[var] = true;

                // Add a unit clause for this assignment
                Clause unit_clause = {var};
                formula.push_back(unit_clause);

                failed_literals_found++;
                changed = true;
                stats.variables_fixed++;

                // Re-propagate with this new information
                CNF simplified = unitPropagation(formula);
                if (simplified.empty() || (simplified.size() == 1 && simplified[0].empty()))
                {
                    // Formula is unsatisfiable
                    formula = simplified;
                    return formula;
                }
                formula = simplified;
                break; // Break to restart with the modified formula
            }
            else if (result_neg > 0)
            {
                // Found some implied assignments, add them to the formula
                for (const auto &[implied_var, implied_val] : implications)
                {
                    // Skip the test variable itself
                    if (implied_var == var)
                        continue;

                    if (fixed_variables.find(implied_var) == fixed_variables.end())
                    {
                        int lit = implied_val ? implied_var : -implied_var;
                        fixed_variables[implied_var] = implied_val;
                        Clause unit_clause = {lit};
                        formula.push_back(unit_clause);
                        stats.variables_fixed++;

                        changed = true;
                    }
                }

                if (changed)
                {
                    // Re-propagate with this new information
                    CNF simplified = unitPropagation(formula);
                    if (simplified.empty() || (simplified.size() == 1 && simplified[0].empty()))
                    {
                        // Formula is unsatisfiable
                        formula = simplified;
                        return formula;
                    }
                    formula = simplified;
                    break; // Break to restart with the modified formula
                }
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    stats.updateTechniqueTiming("failed_literal",
                                std::chrono::duration_cast<std::chrono::microseconds>(
                                    end_time - start_time));

    return formula;
}

CNF Preprocessor::eliminateBlockedClauses(CNF &formula)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    // This is a stub implementation
    // Blocked clause elimination will be implemented later

    auto end_time = std::chrono::high_resolution_clock::now();
    stats.updateTechniqueTiming("blocked_clause",
                                std::chrono::duration_cast<std::chrono::microseconds>(
                                    end_time - start_time));

    return formula;
}

void Preprocessor::initializeClauseMeta(const CNF &formula)
{
    clauses_meta.clear();

    // Initialize metadata for each clause
    for (const auto &clause : formula)
    {
        // Detect if clause appears to be part of problem structure
        bool is_structural = isStructuralClause(clause);

        // Check if it's an assumption
        bool is_assumption = false;

        if (clause.size() == 1)
        {
            int lit = clause[0];
            is_assumption = std::find(assumption_literals.begin(),
                                      assumption_literals.end(),
                                      lit) != assumption_literals.end();
        }

        clauses_meta.emplace_back(clause, PreprocessingPhase::INITIAL,
                                  is_structural, is_assumption);
    }
}

bool Preprocessor::isStructuralClause(const Clause &clause)
{
    // For now, use simple heuristics based on problem type
    switch (problem_type)
    {
    case ProblemType::NQUEENS:
        // For N-Queens, clauses with exactly N literals might be row/column constraints
        if (clause.size() > 3)
        {
            // Check if all variables are in the same row or column
            bool same_row = true;
            bool same_col = true;

            for (size_t i = 1; i < clause.size(); i++)
            {
                int var1 = std::abs(clause[i - 1]);
                int var2 = std::abs(clause[i]);

                // These are simplistic checks - proper detection would need knowledge
                // of how the N-Queens problem is encoded
                if (var1 / 10 != var2 / 10)
                { // Assuming tens digit is row
                    same_row = false;
                }
                if (var1 % 10 != var2 % 10)
                { // Assuming ones digit is column
                    same_col = false;
                }
            }

            return same_row || same_col;
        }
        return false;

    case ProblemType::PIGEONHOLE:
        // For Pigeonhole, recognize "at most one pigeon per hole" constraints
        if (clause.size() == 2 && clause[0] < 0 && clause[1] < 0)
        {
            return true;
        }
        return false;

    default:
        // For generic problems, no special structural detection
        return false;
    }
}

void Preprocessor::updateClauseMeta(CNF &formula)
{
    // Create a mapping from clauses to their metadata
    std::vector<ClauseMeta> new_meta;

    for (const auto &clause : formula)
    {
        // Create a normalized version of the clause for comparison
        Clause normalized_clause = clause;
        std::sort(normalized_clause.begin(), normalized_clause.end());

        // Try to find matching metadata
        bool found = false;

        for (const auto &meta : clauses_meta)
        {
            // Create a normalized version of the metadata clause
            Clause normalized_meta_clause = meta.clause;
            std::sort(normalized_meta_clause.begin(), normalized_meta_clause.end());

            if (normalized_meta_clause == normalized_clause)
            {
                new_meta.push_back(meta);
                found = true;
                break;
            }
        }

        // If no matching metadata found, create new
        if (!found)
        {
            bool is_structural = isStructuralClause(clause);
            new_meta.emplace_back(clause, PreprocessingPhase::INITIAL, is_structural);
        }
    }

    // Update with the new metadata
    clauses_meta = std::move(new_meta);
}

void Preprocessor::setAssumptions(const std::vector<int> &assumptions)
{
    assumption_literals = assumptions;

    // Update fixed variables with assumptions
    for (int lit : assumptions)
    {
        int var = std::abs(lit);
        bool value = (lit > 0);
        fixed_variables[var] = value;

        // Mark this as an assumption in our tracking
        stats.variables_fixed++;
    }

    // Log for debugging
    if (!assumptions.empty())
    {
        std::cout << "Set " << assumptions.size() << " assumptions:" << std::endl;
        for (int lit : assumptions)
        {
            std::cout << "  " << lit << std::endl;
        }
    }
}

// Helper function to check if a number is a perfect square
bool Preprocessor::isPerfectSquare(int n)
{
    int sqrt_n = static_cast<int>(std::sqrt(n));
    return sqrt_n * sqrt_n == n;
}

bool Preprocessor::validatePigeonholeStructure(const CNF &formula, int m, int n)
{
    // Verify exact clause distribution
    int pigeon_constraint_count = 0; // Clauses with m positive literals
    int hole_constraint_count = 0;   // Binary clauses with negative literals

    // Track hole constraints to ensure proper structure
    std::vector<std::set<std::pair<int, int>>> pigeon_pairs_seen_per_hole(n);
    std::vector<std::set<int>> pigeons_per_hole(n);

    // Map variables to pigeon-hole pairs
    std::unordered_map<int, std::pair<int, int>> var_map;
    const int baseVar = 1;

    // Set up expected variable mapping
    for (int p = 0; p < m; p++)
    {
        for (int h = 0; h < n; h++)
        {
            int var = baseVar + p * n + h;
            var_map[var] = {p, h};
        }
    }

    // Check each clause for exact Pigeonhole structure
    for (const auto &clause : formula)
    {
        if (clause.size() == n && std::all_of(clause.begin(), clause.end(), [](int lit)
                                              { return lit > 0; }))
        {
            // This should be a pigeon constraint (pigeon p must be in some hole)
            int pigeon = -1;
            std::set<int> holes;
            bool is_valid_pigeon_constraint = true;

            for (int lit : clause)
            {
                auto it = var_map.find(lit);
                if (it == var_map.end())
                {
                    is_valid_pigeon_constraint = false;
                    break;
                }

                int cur_pigeon = it->second.first;
                int cur_hole = it->second.second;

                if (pigeon == -1)
                {
                    pigeon = cur_pigeon;
                }
                else if (pigeon != cur_pigeon)
                {
                    is_valid_pigeon_constraint = false;
                    break;
                }

                holes.insert(cur_hole);
            }

            // A valid pigeon constraint must have:
            // 1. All literals from the same pigeon
            // 2. Exactly n different holes
            if (is_valid_pigeon_constraint && pigeon >= 0 && pigeon < m && holes.size() == n)
            {
                pigeon_constraint_count++;
            }
        }
        else if (clause.size() == 2 && clause[0] < 0 && clause[1] < 0)
        {
            // This should be a hole constraint (no two pigeons in the same hole)
            int var1 = std::abs(clause[0]);
            int var2 = std::abs(clause[1]);

            auto it1 = var_map.find(var1);
            auto it2 = var_map.find(var2);

            if (it1 != var_map.end() && it2 != var_map.end())
            {
                int p1 = it1->second.first;
                int h1 = it1->second.second;
                int p2 = it2->second.first;
                int h2 = it2->second.second;

                // A valid hole constraint must have:
                // 1. Different pigeons (p1 != p2)
                // 2. Same hole (h1 == h2)
                if (p1 != p2 && h1 == h2 && h1 >= 0 && h1 < n)
                {
                    // Track this pair to ensure we don't have duplicates
                    if (p1 > p2)
                        std::swap(p1, p2);

                    // Get the set for this specific hole
                    auto &seen_pairs = pigeon_pairs_seen_per_hole[h1];
                    std::pair<int, int> pigeon_pair = {p1, p2};

                    if (seen_pairs.find(pigeon_pair) == seen_pairs.end())
                    {
                        seen_pairs.insert(pigeon_pair);
                        pigeons_per_hole[h1].insert(p1);
                        pigeons_per_hole[h1].insert(p2);
                        hole_constraint_count++;
                    }
                }
            }
        }
    }

    // Calculate expected counts
    int expected_pigeon_constraints = m;
    int expected_hole_constraints = n * (m * (m - 1) / 2);

    // Verify that all holes have the correct number of constraints
    bool all_holes_correct = true;
    for (int h = 0; h < n; h++)
    {
        // Each hole should have exactly m pigeons mentioned in constraints
        if (pigeons_per_hole[h].size() != m)
        {
            all_holes_correct = false;
            break;
        }
    }

    // For a strict Pigeonhole problem, we must have:
    // 1. Exactly m pigeon constraints
    // 2. Exactly n*(m*(m-1)/2) hole constraints
    // 3. Every hole must have exactly m pigeons in its constraints
    return (pigeon_constraint_count == expected_pigeon_constraints &&
            hole_constraint_count == expected_hole_constraints &&
            all_holes_correct);
}

bool Preprocessor::validateNQueensStructure(const CNF &formula, int n)
{
    // Map variables to board positions
    std::map<int, std::pair<int, int>> var_to_position;
    const int base = 1;

    for (int row = 0; row < n; row++)
    {
        for (int col = 0; col < n; col++)
        {
            int var = base + row * n + col;
            var_to_position[var] = {row, col};
        }
    }

    // Track different constraint types
    std::vector<bool> row_constraints(n, false);
    std::vector<bool> col_constraints(n, false);

    // Track conflict constraints between positions
    std::map<int, std::set<std::pair<int, int>>> row_conflicts; // row -> pairs of columns
    std::map<int, std::set<std::pair<int, int>>> col_conflicts; // col -> pairs of rows

    // Track diagonal conflicts specifically
    std::set<std::tuple<int, int, int, int>> diagonal_conflicts; // (row1,col1,row2,col2)

    // Count constraint types
    int row_at_least_one = 0;
    int col_at_least_one = 0;
    int row_conflict_count = 0;
    int col_conflict_count = 0;
    int diagonal_conflict_count = 0;

    // Analyze each clause to determine its type
    for (const auto &clause : formula)
    {
        if (clause.size() == n && std::all_of(clause.begin(), clause.end(),
                                              [](int lit)
                                              { return lit > 0; }))
        {
            // This is an at-least-one constraint for a row or column
            // Check if all variables are in the same row or column
            int common_row = -1;
            int common_col = -1;
            bool is_row = true;
            bool is_col = true;

            for (int lit : clause)
            {
                auto it = var_to_position.find(lit);
                if (it != var_to_position.end())
                {
                    int row = it->second.first;
                    int col = it->second.second;

                    if (common_row == -1)
                    {
                        common_row = row;
                    }
                    else if (common_row != row)
                    {
                        is_row = false;
                    }

                    if (common_col == -1)
                    {
                        common_col = col;
                    }
                    else if (common_col != col)
                    {
                        is_col = false;
                    }
                }
                else
                {
                    // Variable not in expected range
                    is_row = false;
                    is_col = false;
                    break;
                }
            }

            // Mark the constraint type
            if (is_row && common_row >= 0 && common_row < n)
            {
                row_constraints[common_row] = true;
                row_at_least_one++;
            }

            if (is_col && common_col >= 0 && common_col < n)
            {
                col_constraints[common_col] = true;
                col_at_least_one++;
            }
        }
        else if (clause.size() == 2 && clause[0] < 0 && clause[1] < 0)
        {
            // This is an at-most-one conflict constraint
            int var1 = std::abs(clause[0]);
            int var2 = std::abs(clause[1]);

            auto it1 = var_to_position.find(var1);
            auto it2 = var_to_position.find(var2);

            if (it1 != var_to_position.end() && it2 != var_to_position.end())
            {
                int row1 = it1->second.first;
                int col1 = it1->second.second;
                int row2 = it2->second.first;
                int col2 = it2->second.second;

                // Determine constraint type
                if (row1 == row2)
                {
                    // Same row conflict
                    if (col1 > col2)
                        std::swap(col1, col2);
                    row_conflicts[row1].insert({col1, col2});
                    row_conflict_count++;
                }
                else if (col1 == col2)
                {
                    // Same column conflict
                    if (row1 > row2)
                        std::swap(row1, row2);
                    col_conflicts[col1].insert({row1, row2});
                    col_conflict_count++;
                }
                else if (std::abs(row1 - row2) == std::abs(col1 - col2))
                {
                    // Diagonal conflict
                    diagonal_conflicts.insert({row1, col1, row2, col2});
                    diagonal_conflict_count++;
                }
            }
        }
    }

    // Validate that we have all the required constraints for N-Queens

    // 1. Every row must have an at-least-one constraint
    bool all_rows_constrained = std::all_of(row_constraints.begin(), row_constraints.end(),
                                            [](bool b)
                                            { return b; });
    if (!all_rows_constrained)
    {
        return false;
    }

    // 2. Every column must have an at-least-one constraint
    bool all_cols_constrained = std::all_of(col_constraints.begin(), col_constraints.end(),
                                            [](bool b)
                                            { return b; });
    if (!all_cols_constrained)
    {
        return false;
    }

    // 3. Expected number of constraints (exact counting)
    int expected_row_at_least_one = n;
    int expected_col_at_least_one = n;
    int expected_row_conflicts = n * (n * (n - 1) / 2); // Each row has n*(n-1)/2 position pairs
    int expected_col_conflicts = n * (n * (n - 1) / 2); // Each col has n*(n-1)/2 position pairs

    // Diagonal conflicts calculation:
    // For a board of size n, there are 2n-1 diagonals in each direction
    // Each diagonal with k positions has k*(k-1)/2 conflict pairs
    int expected_diagonal_conflicts = 0;
    for (int k = 2; k <= n; k++)
    {
        // Count conflicts for diagonals with k positions (there are 2 of each length except for the longest)
        int conflicts_per_diagonal = k * (k - 1) / 2;
        if (k == n)
        {
            // Only one diagonal of maximum length in each direction
            expected_diagonal_conflicts += 2 * conflicts_per_diagonal;
        }
        else
        {
            // Two diagonals of this length in each direction
            expected_diagonal_conflicts += 4 * conflicts_per_diagonal;
        }
    }

    // Check for number of constraints with some tolerance
    // For N-Queens, the pattern is very specific, so we want close matches
    bool has_required_row_constraints = row_at_least_one == expected_row_at_least_one;
    bool has_required_col_constraints = col_at_least_one == expected_col_at_least_one;
    // Allow some tolerance for conflict constraints since different encodings might exist
    bool has_enough_row_conflicts = row_conflict_count >= expected_row_conflicts * 0.9;
    bool has_enough_col_conflicts = col_conflict_count >= expected_col_conflicts * 0.9;
    bool has_enough_diag_conflicts = diagonal_conflict_count >= expected_diagonal_conflicts * 0.7;

    // The key distinguishing feature of N-Queens is the diagonal constraints
    // We must have a significant number of diagonal constraints
    if (!has_enough_diag_conflicts)
    {
        return false;
    }

    // We need exact matches on row/column at-least-one constraints
    if (!has_required_row_constraints || !has_required_col_constraints)
    {
        return false;
    }

    // For conflict constraints, we need enough of them to confirm the pattern
    if (!has_enough_row_conflicts || !has_enough_col_conflicts)
    {
        return false;
    }

    // Final check: Verify that we have conflicts for positions in the same row/column
    // Sample check - verify that we have conflicts for at least one row and one column
    bool found_complete_row = false;
    for (const auto &[row, conflicts] : row_conflicts)
    {
        if (conflicts.size() >= (n * (n - 1) / 2) * 0.8)
        {
            found_complete_row = true;
            break;
        }
    }

    bool found_complete_col = false;
    for (const auto &[col, conflicts] : col_conflicts)
    {
        if (conflicts.size() >= (n * (n - 1) / 2) * 0.8)
        {
            found_complete_col = true;
            break;
        }
    }

    if (!found_complete_row || !found_complete_col)
    {
        return false;
    }

    // If we pass all validation criteria, this is an N-Queens problem
    return true;
}

bool Preprocessor::validateGraphColoringStructure(const CNF &formula, int v, int c)
{
    // For this implementation, we'll do a simplified validation
    // A more complete validation would check the exact structural properties

    // Map variables to vertex-color pairs
    std::map<int, std::pair<int, int>> var_to_vertex_color;
    const int base = 1;

    for (int vertex = 0; vertex < v; vertex++)
    {
        for (int color = 0; color < c; color++)
        {
            int var = base + vertex * c + color;
            var_to_vertex_color[var] = {vertex, color};
        }
    }

    // Check for vertex at-least-one-color constraints
    std::vector<bool> vertex_constraints(v, false);

    // Check for vertex-color conflicts
    std::set<std::pair<int, int>> vertex_conflicts; // Pairs of vertices that conflict

    for (const auto &clause : formula)
    {
        if (clause.size() == c && std::all_of(clause.begin(), clause.end(),
                                              [](int lit)
                                              { return lit > 0; }))
        {
            // This could be an at-least-one-color constraint for a vertex
            bool is_vertex_constraint = true;
            int vertex_val = -1;
            std::set<int> colors;

            for (int lit : clause)
            {
                auto it = var_to_vertex_color.find(lit);
                if (it != var_to_vertex_color.end())
                {
                    if (vertex_val == -1)
                    {
                        vertex_val = it->second.first;
                    }
                    else if (vertex_val != it->second.first)
                    {
                        is_vertex_constraint = false;
                        break;
                    }
                    colors.insert(it->second.second);
                }
                else
                {
                    is_vertex_constraint = false;
                    break;
                }
            }

            // Check if this clause contains all colors for a vertex
            if (is_vertex_constraint && vertex_val >= 0 && vertex_val < v && colors.size() == c)
            {
                vertex_constraints[vertex_val] = true;
            }
        }
        else if (clause.size() == 2 && clause[0] < 0 && clause[1] < 0)
        {
            // This could be a conflict constraint between two vertices for the same color
            int var1 = std::abs(clause[0]);
            int var2 = std::abs(clause[1]);

            auto it1 = var_to_vertex_color.find(var1);
            auto it2 = var_to_vertex_color.find(var2);

            if (it1 != var_to_vertex_color.end() && it2 != var_to_vertex_color.end())
            {
                // Check if these represent different vertices with the same color
                if (it1->second.first != it2->second.first && it1->second.second == it2->second.second)
                {
                    int vertex1 = it1->second.first;
                    int vertex2 = it2->second.first;

                    // Add this pair to the vertex conflicts
                    if (vertex1 > vertex2)
                        std::swap(vertex1, vertex2);
                    vertex_conflicts.insert({vertex1, vertex2});
                }
            }
        }
    }

    // Verify all vertices have an at-least-one-color constraint
    bool all_vertices_constrained = std::all_of(vertex_constraints.begin(), vertex_constraints.end(),
                                                [](bool b)
                                                { return b; });

    // Verify we have a reasonable number of vertex conflicts
    // For a complete graph, there would be v*(v-1)/2 conflicts
    // For sparse graphs, fewer conflicts are expected
    int min_expected_conflicts = v; // Very conservative lower bound
    bool sufficient_conflicts = vertex_conflicts.size() >= min_expected_conflicts;

    return all_vertices_constrained && sufficient_conflicts;
}

bool Preprocessor::validateHamiltonianStructure(const CNF &formula, int n)
{
    // Simplified validation for Hamiltonian problems
    // A full validation would be more complex

    // Map variables to position-vertex pairs
    std::map<int, std::pair<int, int>> var_to_position_vertex;
    const int base = 1;

    for (int pos = 0; pos < n; pos++)
    {
        for (int vert = 0; vert < n; vert++)
        {
            int var = base + pos * n + vert;
            var_to_position_vertex[var] = {pos, vert};
        }
    }

    // Check for position constraints (each position has exactly one vertex)
    std::vector<bool> position_constraints(n, false);

    // Check for vertex constraints (each vertex appears exactly once)
    std::vector<bool> vertex_constraints(n, false);

    for (const auto &clause : formula)
    {
        if (clause.size() == n && std::all_of(clause.begin(), clause.end(),
                                              [](int lit)
                                              { return lit > 0; }))
        {
            // This could be an at-least-one constraint for a position or vertex
            bool is_position = true;
            bool is_vertex = true;
            int position_val = -1;
            int vertex_val = -1;

            for (int lit : clause)
            {
                auto it = var_to_position_vertex.find(lit);
                if (it != var_to_position_vertex.end())
                {
                    if (position_val == -1)
                    {
                        position_val = it->second.first;
                    }
                    else if (position_val != it->second.first)
                    {
                        is_position = false;
                    }

                    if (vertex_val == -1)
                    {
                        vertex_val = it->second.second;
                    }
                    else if (vertex_val != it->second.second)
                    {
                        is_vertex = false;
                    }
                }
                else
                {
                    is_position = false;
                    is_vertex = false;
                    break;
                }
            }

            if (is_position && position_val >= 0 && position_val < n)
            {
                position_constraints[position_val] = true;
            }

            if (is_vertex && vertex_val >= 0 && vertex_val < n)
            {
                vertex_constraints[vertex_val] = true;
            }
        }
    }

    // Verify position and vertex constraints
    bool all_positions_constrained = std::all_of(position_constraints.begin(), position_constraints.end(),
                                                 [](bool b)
                                                 { return b; });

    bool all_vertices_constrained = std::all_of(vertex_constraints.begin(), vertex_constraints.end(),
                                                [](bool b)
                                                { return b; });

    // A more thorough validation would check adjacency constraints as well

    return all_positions_constrained && all_vertices_constrained;
}

// Domain-Aware Resolution-Based Variable Elimination (DARVE)
CNF Preprocessor::eliminateVariables(CNF &formula)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    // Count occurrences and build literal-to-clause index
    std::map<int, int> pos_occurrences;
    std::map<int, int> neg_occurrences;
    std::set<int> variables;
    std::unordered_map<int, std::vector<size_t>> literal_to_clauses;

    // Calculate occurrence counts and build index
    for (size_t clause_idx = 0; clause_idx < formula.size(); clause_idx++)
    {
        const auto &clause = formula[clause_idx];
        for (int lit : clause)
        {
            int var = std::abs(lit);
            variables.insert(var);

            if (lit > 0)
            {
                pos_occurrences[var]++;
            }
            else
            {
                neg_occurrences[var]++;
            }

            // Add to literal index
            literal_to_clauses[lit].push_back(clause_idx);
        }
    }

    // Calculate elimination scores
    std::map<int, double> elimination_scores;
    std::vector<int> vars_sorted_by_score;

    // Score each variable for elimination based on problem type
    for (int var : variables)
    {
        int pos = pos_occurrences[var];
        int neg = neg_occurrences[var];

        // Skip variables that don't appear in both polarities
        if (pos == 0 || neg == 0)
            continue;

        // Variables in structural clauses should be given a lower score
        bool is_structural = false;
        for (size_t i = 0; i < formula.size() && i < clauses_meta.size(); i++)
        {
            if (clauses_meta[i].is_structural)
            {
                for (int lit : formula[i])
                {
                    if (std::abs(lit) == var)
                    {
                        is_structural = true;
                        break;
                    }
                }
                if (is_structural)
                    break;
            }
        }

        // Scoring strategy depends on problem type
        double score = 0.0;
        switch (problem_type)
        {
        case ProblemType::NQUEENS:
            // For N-Queens, preserve variables that represent positions
            score = pos * neg * (is_structural ? 10.0 : 1.0);

            // Add additional bias to preserve board structure
            {
                int n = static_cast<int>(std::sqrt(countVariables(formula)));
                int row = (var - 1) / n;
                int col = (var - 1) % n;

                // Bias against eliminating corner positions and center positions
                if ((row == 0 || row == n - 1) && (col == 0 || col == n - 1))
                {
                    score *= 5.0; // Avoid eliminating corner variables
                }
                if (row == n / 2 && col == n / 2)
                {
                    score *= 3.0; // Avoid eliminating center variables
                }
            }
            break;

        case ProblemType::PIGEONHOLE:
            // For Pigeonhole, focus on eliminating variables with balanced occurrences
            score = std::abs(pos - neg) < 2 ? pos * neg : pos * neg * 3.0;

            // If this is a variable in a structural clause, give higher score
            if (is_structural)
            {
                score *= 2.0;
            }
            break;

        default:
            // For generic problems, use product of positive and negative occurrences
            score = pos * neg * (is_structural ? 2.0 : 1.0);
            break;
        }

        elimination_scores[var] = score;
        vars_sorted_by_score.push_back(var);
    }

    // Sort variables by score (ascending)
    std::sort(vars_sorted_by_score.begin(), vars_sorted_by_score.end(),
              [&elimination_scores](int a, int b)
              {
                  return elimination_scores[a] < elimination_scores[b];
              });

    // Configure elimination limits based on problem type
    int max_vars_to_eliminate;
    switch (problem_type)
    {
    case ProblemType::NQUEENS:
        max_vars_to_eliminate = std::min(50, static_cast<int>(variables.size() / 6));
        break;
    case ProblemType::PIGEONHOLE:
        max_vars_to_eliminate = std::min(80, static_cast<int>(variables.size() / 4));
        break;
    default:
        max_vars_to_eliminate = std::min(100, static_cast<int>(variables.size() / 3));
        break;
    }

    const int max_resolvent_size = 15; // Don't create resolvents larger than this
    const int max_new_clauses = 20;    // Limit on number of new clauses per variable

    int num_eliminated = 0;
    CNF result = formula;
    std::vector<bool> clause_eliminated(formula.size(), false);

    // Attempt elimination for each variable in order of score
    for (int var : vars_sorted_by_score)
    {
        if (num_eliminated >= max_vars_to_eliminate)
            break;

        // Skip variables in assumptions
        bool is_assumption = false;
        for (int lit : assumption_literals)
        {
            if (std::abs(lit) == var)
            {
                is_assumption = true;
                break;
            }
        }
        if (is_assumption)
            continue;

        // For structured problems, be more conservative with variable elimination
        if (problem_type != ProblemType::GENERIC)
        {
            // Skip variables in structural clauses for N-Queens
            if (problem_type == ProblemType::NQUEENS && elimination_scores[var] > 10.0)
            {
                continue;
            }
        }

        // Use the index to find clauses containing this variable
        const auto &pos_clause_indices = literal_to_clauses[var];
        const auto &neg_clause_indices = literal_to_clauses[-var];

        // Collect the actual clauses
        std::vector<Clause> pos_clauses;
        std::vector<Clause> neg_clauses;

        for (size_t idx : pos_clause_indices)
        {
            if (!clause_eliminated[idx])
            {
                pos_clauses.push_back(result[idx]);
            }
        }

        for (size_t idx : neg_clause_indices)
        {
            if (!clause_eliminated[idx])
            {
                neg_clauses.push_back(result[idx]);
            }
        }

        // Check if elimination would increase the size too much
        if (pos_clauses.size() * neg_clauses.size() > pos_clauses.size() + neg_clauses.size() + max_new_clauses)
        {
            continue;
        }

        // Generate resolvents
        std::vector<Clause> resolvents;
        for (const auto &pos_clause : pos_clauses)
        {
            for (const auto &neg_clause : neg_clauses)
            {
                // Create resolvent
                Clause resolvent;

                // Add literals from positive clause except var
                for (int lit : pos_clause)
                {
                    if (lit != var)
                    {
                        resolvent.push_back(lit);
                    }
                }

                // Add literals from negative clause except -var
                for (int lit : neg_clause)
                {
                    if (lit != -var)
                    {
                        resolvent.push_back(lit);
                    }
                }

                // Sort and remove duplicates
                std::sort(resolvent.begin(), resolvent.end());
                resolvent.erase(std::unique(resolvent.begin(), resolvent.end()), resolvent.end());

                // Check for tautology
                bool is_tautology = false;
                for (size_t i = 1; i < resolvent.size(); i++)
                {
                    if (resolvent[i - 1] == -resolvent[i])
                    {
                        is_tautology = true;
                        break;
                    }
                }

                // Skip tautologies and resolvents that are too large
                if (!is_tautology && resolvent.size() <= max_resolvent_size)
                {
                    resolvents.push_back(resolvent);
                }
            }
        }

        // Check if we've actually reduced the formula size
        if (resolvents.size() <= pos_clauses.size() + neg_clauses.size())
        {
            // Perform the elimination
            // Keep track of the eliminated variable
            variable_map[var] = -1; // Mark as eliminated

            // Mark clauses containing this variable as eliminated
            for (size_t idx : pos_clause_indices)
            {
                clause_eliminated[idx] = true;
            }
            for (size_t idx : neg_clause_indices)
            {
                clause_eliminated[idx] = true;
            }

            // Add resolvents to the result
            for (const auto &resolvent : resolvents)
            {
                result.push_back(resolvent);
            }

            num_eliminated++;

            std::cout << "Eliminated variable " << var
                      << " (replaced " << pos_clauses.size() + neg_clauses.size()
                      << " clauses with " << resolvents.size() << " resolvents)\n";

            // Update the literal index for the new resolvents
            for (const auto &resolvent : resolvents)
            {
                size_t new_idx = result.size() - 1;
                for (int lit : resolvent)
                {
                    literal_to_clauses[lit].push_back(new_idx);
                }
            }
        }
    }

    // Build the final result by keeping only non-eliminated clauses
    CNF final_result;
    for (size_t i = 0; i < result.size(); i++)
    {
        if (i >= clause_eliminated.size() || !clause_eliminated[i])
        {
            final_result.push_back(result[i]);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    stats.updateTechniqueTiming("variable_elimination",
                                std::chrono::duration_cast<std::chrono::microseconds>(
                                    end_time - start_time));

    stats.variables_eliminated += num_eliminated;

    return final_result;
}

int Preprocessor::testLiteralAssignment(const CNF &formula, int var, bool value,
                                        std::unordered_map<int, bool> &implications)
{
    // Create a local assignments map starting with fixed variables
    std::unordered_map<int, bool> assignments = fixed_variables;

    // Add the test assignment
    assignments[var] = value;

    // Create a temporary copy of the formula for testing
    CNF temp_formula = formula;

    // Add the test assignment as a unit clause
    int literal = value ? var : -var;
    temp_formula.push_back({literal});

    // Track the initial size to determine how many new assignments were made
    int initial_size = assignments.size();

    bool changed = true;
    while (changed)
    {
        changed = false;

        for (auto it = temp_formula.begin(); it != temp_formula.end();)
        {
            // Check if the clause is satisfied
            bool is_satisfied = false;
            for (int lit : *it)
            {
                int lit_var = std::abs(lit);
                auto assign_it = assignments.find(lit_var);
                if (assign_it != assignments.end() &&
                    ((lit > 0 && assign_it->second) || (lit < 0 && !assign_it->second)))
                {
                    is_satisfied = true;
                    break;
                }
            }

            if (is_satisfied)
            {
                it = temp_formula.erase(it);
                continue;
            }

            // Count unassigned literals and track the last unassigned
            int unassigned_count = 0;
            int last_unassigned = 0;

            for (int lit : *it)
            {
                int lit_var = std::abs(lit);
                auto assign_it = assignments.find(lit_var);

                if (assign_it == assignments.end())
                {
                    unassigned_count++;
                    last_unassigned = lit;
                }
            }

            if (unassigned_count == 0)
            {
                // All literals are falsified, we have a contradiction
                return -1;
            }
            else if (unassigned_count == 1)
            {
                // Unit clause, propagate the assignment
                int unit_var = std::abs(last_unassigned);
                bool unit_value = (last_unassigned > 0);

                // Check for conflicts
                auto existing = assignments.find(unit_var);
                if (existing != assignments.end() && existing->second != unit_value)
                {
                    // Contradiction found
                    return -1;
                }

                // Add the assignment
                assignments[unit_var] = unit_value;
                changed = true;
            }

            ++it;
        }
    }

    // Copy all implications (excluding fixed variables and the test variable)
    for (const auto &[assigned_var, assigned_value] : assignments)
    {
        // Skip the test variable and fixed variables
        if (assigned_var == var || fixed_variables.find(assigned_var) != fixed_variables.end())
        {
            continue;
        }

        // This is a true implication
        implications[assigned_var] = assigned_value;
    }

    // Return the number of implications found
    return implications.size();
}

CNF Preprocessor::performSelfSubsumption(CNF &formula)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    // Create a literal-to-clauses index for quick lookup
    std::unordered_map<int, std::vector<size_t>> literal_to_clauses;

    // Build the index
    for (size_t i = 0; i < formula.size(); i++)
    {
        for (int lit : formula[i])
        {
            literal_to_clauses[lit].push_back(i);
        }
    }

    // Track strengthened clauses
    int strengthened_count = 0;
    int literals_removed = 0;
    bool changes_made = true;

    // Iterate until no more changes
    while (changes_made)
    {
        changes_made = false;

        // Iterate through each clause
        for (size_t i = 0; i < formula.size(); i++)
        {
            Clause &clause = formula[i];

            // Skip unit clauses - they can't be strengthened
            if (clause.size() <= 1)
                continue;

            // Store literals to remove (safer than modifying during iteration)
            std::vector<int> lits_to_remove;

            // Try to remove each literal from the clause
            for (size_t j = 0; j < clause.size(); j++)
            {
                int lit = clause[j];
                int neg_lit = -lit;

                // Look for clauses containing the negation of this literal
                const auto &resolvent_clause_indices = literal_to_clauses[neg_lit];

                for (size_t resolvent_idx : resolvent_clause_indices)
                {
                    // Skip self-resolution
                    if (resolvent_idx == i)
                        continue;

                    const Clause &resolvent = formula[resolvent_idx];

                    // Check if the resolvent subsumes the clause except for lit
                    bool subsumes = true;
                    for (int resolvent_lit : resolvent)
                    {
                        // Skip the negated literal used for resolution
                        if (resolvent_lit == neg_lit)
                            continue;

                        // Check if the literal appears in the original clause
                        if (std::find(clause.begin(), clause.end(), resolvent_lit) == clause.end())
                        {
                            subsumes = false;
                            break;
                        }
                    }

                    // If the resolvent subsumes the clause except for lit,
                    // mark the literal for removal
                    if (subsumes)
                    {
                        lits_to_remove.push_back(lit);
                        break;
                    }
                }
            }

            // Now apply all removals safely
            if (!lits_to_remove.empty())
            {
                changes_made = true;

                // Remove all marked literals
                for (int lit_to_remove : lits_to_remove)
                {
                    clause.erase(std::remove(clause.begin(), clause.end(), lit_to_remove),
                                 clause.end());

                    // Update tracking
                    strengthened_count++;
                    literals_removed++;

                    // Update literal-to-clauses index
                    auto &clauses_with_lit = literal_to_clauses[lit_to_remove];
                    clauses_with_lit.erase(
                        std::remove(clauses_with_lit.begin(), clauses_with_lit.end(), i),
                        clauses_with_lit.end());
                }

                // Add the modified clause to the index
                for (int lit : clause)
                {
                    // Make sure this clause index is in the list for this literal
                    auto &clauses = literal_to_clauses[lit];
                    if (std::find(clauses.begin(), clauses.end(), i) == clauses.end())
                    {
                        clauses.push_back(i);
                    }
                }
            }

            // Check if clause became a unit clause
            if (clause.size() == 1)
            {
                // Propagate this unit
                int unit_lit = clause[0];
                int unit_var = std::abs(unit_lit);
                bool unit_val = (unit_lit > 0);

                // Check if this contradicts an existing assignment
                auto it = fixed_variables.find(unit_var);
                if (it != fixed_variables.end() && it->second != unit_val)
                {
                    // Contradiction - formula is UNSAT
                    formula.clear();
                    formula.push_back(Clause{}); // Empty clause indicates UNSAT

                    auto end_time = std::chrono::high_resolution_clock::now();
                    stats.updateTechniqueTiming("self_subsumption",
                                                std::chrono::duration_cast<std::chrono::microseconds>(
                                                    end_time - start_time));
                    return formula;
                }

                // Record this assignment
                fixed_variables[unit_var] = unit_val;
                stats.variables_fixed++;

                // Store original formula size to detect changes
                size_t orig_size = formula.size();

                // Apply unit propagation to simplify the formula
                formula = unitPropagation(formula);

                // If formula is now UNSAT, return
                if (formula.empty() || (formula.size() == 1 && formula[0].empty()))
                {
                    auto end_time = std::chrono::high_resolution_clock::now();
                    stats.updateTechniqueTiming("self_subsumption",
                                                std::chrono::duration_cast<std::chrono::microseconds>(
                                                    end_time - start_time));
                    return formula;
                }

                // If formula changed, rebuild the index
                if (formula.size() != orig_size)
                {
                    literal_to_clauses.clear();
                    for (size_t idx = 0; idx < formula.size(); idx++)
                    {
                        for (int l : formula[idx])
                        {
                            literal_to_clauses[l].push_back(idx);
                        }
                    }

                    // Reset the outer loop to start fresh
                    break;
                }
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    stats.updateTechniqueTiming("self_subsumption",
                                std::chrono::duration_cast<std::chrono::microseconds>(
                                    end_time - start_time));

    return formula;
}

void Preprocessor::setProblemType(ProblemType type)
{
    problem_type = type;

    // Adapt configuration to the new problem type
    config.adaptToType(type);

    std::cout << "Manually set problem type to ";
    switch (type)
    {
    case ProblemType::NQUEENS:
        std::cout << "N-Queens";
        break;
    case ProblemType::PIGEONHOLE:
        std::cout << "Pigeonhole";
        break;
    case ProblemType::GRAPH_COLORING:
        std::cout << "Graph Coloring";
        break;
    case ProblemType::HAMILTONIAN:
        std::cout << "Hamiltonian";
        break;
    default:
        std::cout << "Generic";
        break;
    }
    std::cout << "\n";
}