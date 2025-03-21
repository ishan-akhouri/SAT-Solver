#include "../include/CDCL.h"
#include <algorithm>
#include <iostream>
#include <cassert>
#include <limits>

CDCLSolver::CDCLSolver(const CNF& formula, bool debug) 
    : instance(formula, debug), 
      decision_level(0),
      conflicts(0),
      decisions(0),
      propagations(0),
      learned_clauses(0),
      max_decision_level(0),
      restarts(0),
      restart_threshold(100),  // Initial restart after 100 conflicts
      restart_multiplier(1.5), // Increase threshold by 1.5x after each restart
      debug_output(debug) {
    
    // Find the number of variables in the formula
    num_variables = 0;
    for (const auto& clause : formula) {
        for (int literal : clause) {
            num_variables = std::max(num_variables, static_cast<size_t>(abs(literal)));
        }
    }
    
    // Initialize watched literals data structure
    watches.resize(2 * num_variables + 1); // +1 for 1-indexing, *2 for both polarities
    
    // Initialize VSIDS activities
    instance.initializeVSIDS();
    
    if (debug_output) {
        std::cout << "CDCL Solver initialized with " << num_variables << " variables and " 
                  << formula.size() << " clauses.\n";
    }
}

bool CDCLSolver::solve() {
    // Initialize watched literals
    initWatchedLiterals();
    
    // Check for empty clauses in the formula (immediately unsatisfiable)
    for (const auto& clause : instance.formula) {
        if (clause.empty()) {
            return false; // Formula has an empty clause, it's UNSAT
        }
    }
    
    // Check for initial unit clauses before entering the main loop
    if (!unitPropagate()) {
        return false; // Conflict during initial unit propagation
    }
    
    // Main CDCL loop
    while (!isSatisfied()) {
        // Check if we should restart
        if (conflicts >= restart_threshold) {
            if (debug_output) {
                std::cout << "Restarting after " << conflicts << " conflicts\n";
            }
            
            // Restart by backtracking to decision level 0
            backtrack(0);
            
            // Update restart threshold for next restart
            restart_threshold = static_cast<int>(restart_threshold * restart_multiplier);
            restarts++;
        }
        
        // Perform unit propagation
        bool conflict = !unitPropagate();
        
        if (conflict) {
            conflicts++;
            
            // Handle conflict at decision level 0
            if (decision_level == 0) {
                if (debug_output) {
                    std::cout << "Conflict at decision level 0. Formula is UNSATISFIABLE.\n";
                }
                return false;
            }
            
            // Analyze conflict and learn a new clause
            Clause learned_clause;
            int backtrack_level = analyzeConflict(instance.formula.back(), learned_clause);
            
            // Add the learned clause to the formula
            addLearnedClause(learned_clause);
            
            // Backtrack to the computed level
            backtrack(backtrack_level);
            
            // Bump VSIDS scores for variables in the learned clause
            for (int lit : learned_clause) {
                instance.bumpVarActivity(abs(lit));
            }
            
            // Decay VSIDS scores periodically
            instance.decayVarActivities();
        } else {
            // No conflict, make a new decision
            if (!makeDecision()) {
                // Check if we have any empty clauses (UNSAT)
                for (const auto& clause : instance.formula) {
                    if (clause.empty()) {
                        return false;
                    }
                }
                
                // Check if all clauses are satisfied
                bool all_satisfied = true;
                for (const auto& clause : instance.formula) {
                    bool clause_satisfied = false;
                    for (int lit : clause) {
                        int var = abs(lit);
                        auto it = instance.assignments.find(var);
                        if (it != instance.assignments.end() && 
                            ((lit > 0 && it->second) || (lit < 0 && !it->second))) {
                            clause_satisfied = true;
                            break;
                        }
                    }
                    if (!clause_satisfied) {
                        all_satisfied = false;
                        break;
                    }
                }
                
                if (all_satisfied) {
                    if (debug_output) {
                        std::cout << "All clauses satisfied. Formula is SATISFIABLE.\n";
                    }
                    return true;
                } else {
                    if (debug_output) {
                        std::cout << "No more decisions possible but formula not satisfied. UNSATISFIABLE.\n";
                    }
                    return false;
                }
            }
        }
    }
    
    // If we get here, the formula is satisfied
    return true;
}

void CDCLSolver::initWatchedLiterals() {
    // Clear existing watches
    for (auto& watch_list : watches) {
        watch_list.clear();
    }
    
    // For each clause, add watches for the first two literals
    for (size_t clause_idx = 0; clause_idx < instance.formula.size(); clause_idx++) {
        const Clause& clause = instance.formula[clause_idx];
        
        if (clause.empty()) {
            if (debug_output) {
                std::cout << "Empty clause detected during initialization. Formula is UNSATISFIABLE.\n";
            }
            // Add a dummy conflict clause to detect unsatisfiability later
            instance.formula.push_back({});
            return;
        }
        
        if (clause.size() == 1) {
            // Unit clause - watch the only literal
            int lit = clause[0];
            size_t watch_idx = lit > 0 ? lit : num_variables + abs(lit);
            watches[watch_idx].push_back(clause_idx);
        } else {
            // Watch first two literals
            for (int i = 0; i < 2 && i < static_cast<int>(clause.size()); i++) {
                int lit = clause[i];
                size_t watch_idx = lit > 0 ? lit : num_variables + abs(lit);
                watches[watch_idx].push_back(clause_idx);
            }
        }
    }
    
    if (debug_output) {
        std::cout << "Watched literals initialized.\n";
        printWatches();
    }
}
bool CDCLSolver::unitPropagate() {
    bool propagated = false;
    
    do {
        propagated = false;
        
        // First check if there are any unit clauses
        for (size_t i = 0; i < instance.formula.size(); i++) {
            Clause& clause = instance.formula[i];
            
            // Skip satisfied clauses
            bool clause_satisfied = false;
            for (int lit : clause) {
                int var = abs(lit);
                auto it = instance.assignments.find(var);
                if (it != instance.assignments.end() && 
                    ((lit > 0 && it->second) || (lit < 0 && !it->second))) {
                    clause_satisfied = true;
                    break;
                }
            }
            if (clause_satisfied) continue;
            
            // Count unassigned literals
            std::vector<int> unassigned_lits;
            for (int lit : clause) {
                int var = abs(lit);
                if (instance.assignments.find(var) == instance.assignments.end()) {
                    unassigned_lits.push_back(lit);
                }
            }
            
            // If all literals are assigned and false, this is a conflict
            if (unassigned_lits.empty()) {
                if (debug_output) {
                    std::cout << "Conflict detected: all literals in clause are false\n";
                }
                
                // Add conflict clause
                instance.formula.push_back(clause);
                return false;
            }
            
            // If exactly one literal is unassigned, this is a unit clause
            if (unassigned_lits.size() == 1) {
                int unit_lit = unassigned_lits[0];
                int var = abs(unit_lit);
                bool value = (unit_lit > 0);
                
                if (debug_output) {
                    std::cout << "Unit propagation: x" << var << " = " << value << " at level " 
                              << decision_level << "\n";
                }
                
                // Record the propagation in the trail
                trail.emplace_back(unit_lit, decision_level, clause);
                var_to_trail[var] = trail.size() - 1;
                
                // Update the assignment
                instance.assignments[var] = value;
                propagations++;
                propagated = true;
                break;
            }
        }
    } while (propagated);
    
    return true; // No conflict
}

int CDCLSolver::analyzeConflict(const Clause& conflict_clause, Clause& learned_clause) {
    if (debug_output) {
        std::cout << "Analyzing conflict in clause: ";
        printClause(conflict_clause);
    }
    
    // Initialize the learned clause with the conflict clause
    learned_clause = conflict_clause;
    
    // Count literals from the current decision level
    std::unordered_set<int> current_level_vars;
    for (int lit : learned_clause) {
        int var = abs(lit);
        if (var_to_trail.find(var) != var_to_trail.end()) {
            const ImplicationNode& node = trail[var_to_trail[var]];
            if (node.decision_level == decision_level) {
                current_level_vars.insert(var);
            }
        }
    }
    
    if (debug_output) {
        std::cout << "Current level variables in conflict: " << current_level_vars.size() << "\n";
    }
    
    // Find the second highest decision level in the clause
    int backtrack_level = 0;
    
    // Resolve the conflict by combining with antecedent clauses
    size_t trail_index = trail.size() - 1;
    while (current_level_vars.size() > 1) { // Keep one literal from current level for UIP
        // Find the most recent assignment in the learned clause
        while (trail_index > 0) {
            const ImplicationNode& node = trail[trail_index];
            int var = abs(node.literal);
            
            // Check if this variable is in the learned clause
            if (current_level_vars.find(var) != current_level_vars.end() && 
                !node.is_decision && node.decision_level == decision_level) {
                // Found a non-decision variable from the current level to resolve with
                break;
            }
            
            trail_index--;
        }
        
        if (trail_index == 0) {
            // Couldn't find a suitable variable, abort
            break;
        }
        
        // Get the variable and its antecedent
        const ImplicationNode& node = trail[trail_index];
        int var = abs(node.literal);
        const Clause& antecedent = node.antecedent;
        
        if (debug_output) {
            std::cout << "Resolving with antecedent of x" << var << ": ";
            printClause(antecedent);
        }
        
        // Resolve the learned clause with the antecedent
        // Remove the current variable from the learned clause
        learned_clause.erase(
            std::remove_if(learned_clause.begin(), learned_clause.end(), 
                          [var](int lit) { return abs(lit) == var; }),
            learned_clause.end());
        
        // Add literals from the antecedent (except the current variable)
        for (int lit : antecedent) {
            if (abs(lit) != var && std::find(learned_clause.begin(), learned_clause.end(), lit) == learned_clause.end()) {
                learned_clause.push_back(lit);
                
                // Check if this is a variable from the current decision level
                int lit_var = abs(lit);
                if (var_to_trail.find(lit_var) != var_to_trail.end()) {
                    const ImplicationNode& lit_node = trail[var_to_trail[lit_var]];
                    if (lit_node.decision_level == decision_level) {
                        current_level_vars.insert(lit_var);
                    } else if (lit_node.decision_level > backtrack_level && lit_node.decision_level != decision_level) {
                        // Update the second highest decision level
                        backtrack_level = lit_node.decision_level;
                    }
                }
            }
        }
        
        // Remove the variable from the current level set
        current_level_vars.erase(var);
        
        if (debug_output) {
            std::cout << "After resolution, learned clause: ";
            printClause(learned_clause);
            std::cout << "Current level variables remaining: " << current_level_vars.size() << "\n";
        }
        
        // Move to the next variable in the trail
        trail_index--;
    }
    
    // Remove duplicates from the learned clause
    std::sort(learned_clause.begin(), learned_clause.end());
    learned_clause.erase(std::unique(learned_clause.begin(), learned_clause.end()), learned_clause.end());
    
    if (debug_output) {
        std::cout << "Final learned clause: ";
        printClause(learned_clause);
        std::cout << "Backtrack level: " << backtrack_level << "\n";
    }
    
    return backtrack_level;
}

void CDCLSolver::backtrack(int level) {
    if (debug_output) {
        std::cout << "Backtracking from level " << decision_level << " to level " << level << "\n";
    }
    
    // Remove assignments made after the backtrack level
    while (!trail.empty()) {
        const ImplicationNode& node = trail.back();
        if (node.decision_level <= level) {
            break;
        }
        
        // Remove the variable from assignments
        int var = abs(node.literal);
        instance.assignments.erase(var);
        var_to_trail.erase(var);
        
        // Remove from trail
        trail.pop_back();
    }
    
    // Update the decision level
    decision_level = level;
    
    if (debug_output) {
        std::cout << "After backtracking, trail size: " << trail.size() << "\n";
        printTrail();
    }
}

bool CDCLSolver::makeDecision() {
    // Use VSIDS to select the next variable
    int var = instance.selectVarVSIDS();
    
    if (var == 0) {
        // No unassigned variables left
        return false;
    }
    
    // Increment decision level
    decision_level++;
    max_decision_level = std::max(max_decision_level, decision_level);
    decisions++;
    
    // Assign the variable (always try true first)
    bool value = true;
    int literal = var; // positive literal
    
    if (debug_output) {
        std::cout << "Decision: x" << var << " = " << value << " at level " << decision_level << "\n";
    }
    
    // Add to trail with empty antecedent (it's a decision)
    trail.emplace_back(literal, decision_level, Clause{}, true);
    var_to_trail[var] = trail.size() - 1;
    
    // Update the assignment
    instance.assignments[var] = value;
    
    return true;
}

void CDCLSolver::addLearnedClause(const Clause& clause) {
    if (clause.empty()) {
        if (debug_output) {
            std::cout << "Learned an empty clause. Formula is UNSATISFIABLE.\n";
        }
        // Empty clause means the formula is unsatisfiable
        instance.formula.push_back(clause);
        return;
    }
    
    // Add the learned clause to the formula
    instance.formula.push_back(clause);
    learned_clauses++;
    
    if (debug_output) {
        std::cout << "Added learned clause: ";
        printClause(clause);
    }
    
    // Set up watched literals for the new clause
    size_t clause_idx = instance.formula.size() - 1;
    
    if (clause.size() == 1) {
        // Unit clause - watch the only literal
        int lit = clause[0];
        size_t watch_idx = lit > 0 ? lit : num_variables + abs(lit);
        watches[watch_idx].push_back(clause_idx);
    } else {
        // Watch first two literals
        for (int i = 0; i < 2 && i < static_cast<int>(clause.size()); i++) {
            int lit = clause[i];
            size_t watch_idx = lit > 0 ? lit : num_variables + abs(lit);
            watches[watch_idx].push_back(clause_idx);
        }
    }
}

// In CDCL.cpp
bool CDCLSolver::isSatisfied() const {
    // Check if any clause is empty (unsatisfiable)
    for (const auto& clause : instance.formula) {
        if (clause.empty()) {
            return false; // Empty clause means UNSAT
        }
    }
    
    // Check if all variables are assigned AND all clauses are satisfied
    bool all_vars_assigned = instance.assignments.size() >= num_variables;
    
    if (!all_vars_assigned) {
        return false; // Not all variables are assigned yet
    }
    
    // Now verify that all clauses are satisfied
    for (const auto& clause : instance.formula) {
        bool clause_satisfied = false;
        for (int literal : clause) {
            int var = abs(literal);
            auto it = instance.assignments.find(var);
            if (it != instance.assignments.end()) {
                bool value = it->second;
                if ((literal > 0 && value) || (literal < 0 && !value)) {
                    clause_satisfied = true;
                    break;
                }
            }
        }
        
        if (!clause_satisfied) {
            return false; // Found an unsatisfied clause
        }
    }
    
    return true; // All clauses are satisfied
}

void CDCLSolver::printTrail() const {
    if (!debug_output) return;
    
    std::cout << "Trail (decision level, literal, is_decision):\n";
    for (size_t i = 0; i < trail.size(); i++) {
        const ImplicationNode& node = trail[i];
        int var = abs(node.literal);
        bool value = node.literal > 0;
        
        std::cout << "[" << i << "] Level " << node.decision_level << ": x" << var << " = " << value;
        if (node.is_decision) {
            std::cout << " (decision)";
        } else {
            std::cout << " (propagation from: ";
            printClause(node.antecedent);
            std::cout << ")";
        }
        std::cout << "\n";
    }
}

void CDCLSolver::printWatches() const {
    if (!debug_output) return;
    
    std::cout << "Watched literals:\n";
    for (size_t i = 1; i <= 2 * num_variables; i++) {
        if (!watches[i].empty()) {
            int lit;
            if (i <= num_variables) {
                lit = i; // Positive literal
            } else {
                lit = -(i - num_variables); // Negative literal
            }
            
            std::cout << "Literal " << lit << " is watched by clauses: ";
            for (size_t clause_idx : watches[i]) {
                std::cout << clause_idx << " ";
            }
            std::cout << "\n";
        }
    }
}

void CDCLSolver::printClause(const Clause& clause) const {
    if (!debug_output) return;
    
    std::cout << "(";
    for (size_t i = 0; i < clause.size(); i++) {
        int lit = clause[i];
        if (lit > 0) {
            std::cout << "x" << lit;
        } else {
            std::cout << "~x" << -lit;
        }
        
        if (i < clause.size() - 1) {
            std::cout << " ∨ ";
        }
    }
    std::cout << ")";
}