#include "../include/DPLL.h"

// Global performance counters
int dpll_calls = 0;  // Tracks number of recursive DPLL calls
int backtracks = 0;  // Tracks number of backtracking operations

bool unitPropagation(SATInstance& instance) {
    bool changed = false;
    do {
        changed = false;
        for (auto it = instance.formula.begin(); it != instance.formula.end(); ) {
            if (it->size() == 1) {  // Found a unit clause
                int unitLiteral = (*it)[0];
                int var = abs(unitLiteral);
                bool value = (unitLiteral > 0);

                // Conflict detection
                if (instance.assignments.count(var) && instance.assignments[var] != value) {
                    if (instance.debug_output) {
                        std::cout << "Conflict detected: x" << var << " already assigned " << instance.assignments[var] << "\n";
                    }
                    
                    // VSIDS: Update activity for variables in the conflict
                    instance.updateActivitiesFromConflict(*it);
                    
                    return false; // Conflict detected
                }

                // Assign the variable and remove the clause
                instance.assignments[var] = value;
                if (instance.debug_output) {
                    std::cout << "Unit Propagation: Assigning x" << var << " = " << value << "\n";
                }
                
                it = instance.formula.erase(it);
                changed = true;

                // Remove negated literal from other clauses
                for (auto& clause : instance.formula) {
                    auto negPos = std::find(clause.begin(), clause.end(), -unitLiteral);
                    if (negPos != clause.end()) {
                        clause.erase(negPos);
                        if (clause.empty()) {
                            if (instance.debug_output) {
                                std::cout << "Empty clause detected after removing x" << var << "\n";
                            }
                            
                            // VSIDS: Update activity for variables in the conflict
                            instance.updateActivitiesFromConflict(clause);
                            
                            return false; // Empty clause detected
                        }
                    }
                }
                break;  // Restart iteration
            } else {
                // Check for implied units
                int unassignedLit = 0;
                size_t falseCount = 0;

                for (int lit : *it) {
                    int var = abs(lit);
                    if (!instance.assignments.count(var)) {
                        unassignedLit = lit; // Track unassigned literal
                    } else if ((lit > 0 && !instance.assignments[var]) || 
                               (lit < 0 && instance.assignments[var])) {
                        falseCount++; // This literal is false
                    }
                }

                // If all but one literal are false, the remaining is a unit
                if (falseCount == it->size() - 1 && unassignedLit != 0) {
                    int var = abs(unassignedLit);
                    bool value = (unassignedLit > 0);

                    // Conflict detection
                    if (instance.assignments.count(var) && instance.assignments[var] != value) {
                        if (instance.debug_output) {
                            std::cout << "Conflict detected: x" << var << " already assigned " << instance.assignments[var] << "\n";
                        }
                        
                        // VSIDS: Update activity for variables in the conflict
                        instance.updateActivitiesFromConflict(*it);
                        
                        return false; // Conflict detected
                    }

                    // Assign the variable and remove the clause
                    instance.assignments[var] = value;
                    if (instance.debug_output) {
                        std::cout << "Implied Unit: Assigning x" << var << " = " << value << "\n";
                    }
                    
                    it = instance.formula.erase(it);
                    changed = true;

                    // Remove negated literal from other clauses
                    for (auto& clause : instance.formula) {
                        auto negPos = std::find(clause.begin(), clause.end(), -unassignedLit);
                        if (negPos != clause.end()) {
                            clause.erase(negPos);
                            if (clause.empty()) {
                                if (instance.debug_output) {
                                    std::cout << "Empty clause detected after removing x" << var << "\n";
                                }
                                
                                // VSIDS: Update activity for variables in the conflict
                                instance.updateActivitiesFromConflict(clause);
                                
                                return false; // Empty clause detected
                            }
                        }
                    }
                    break;  // Restart iteration
                } else {
                    ++it;
                }
            }
        }
    } while (changed);
    
    if (instance.debug_output) {
        std::cout << "Unit propagation done. Formula size: " << instance.formula.size() << "\n";
    }
    
    return true; // Unit propagation completed successfully
}

void pureLiteralElimination(SATInstance& instance) {
    std::unordered_map<int, bool> seenLiterals;

    for (const auto& clause : instance.formula) {
        for (int literal : clause) {
            seenLiterals[literal] = true;
        }
    }

    std::vector<int> pureLiterals;
    for (const auto& [literal, _] : seenLiterals) {
        if (seenLiterals.find(-literal) == seenLiterals.end()) {
            pureLiterals.push_back(literal);
        }
    }

    for (int literal : pureLiterals) {
        instance.assignments[abs(literal)] = (literal > 0);
        if (instance.debug_output) {
            std::cout << "Pure Literal Elimination: Assigning x" << abs(literal) << " = " << (literal > 0) << "\n";
        }
        
        instance.formula.erase(std::remove_if(instance.formula.begin(), instance.formula.end(),
            [literal](const Clause& c) { return std::find(c.begin(), c.end(), literal) != c.end(); }),
            instance.formula.end());
    }
}

bool DPLL(SATInstance& instance) {
    dpll_calls++;
    
    if (instance.debug_output) {
        std::cout << "DPLL Call #" << dpll_calls << "\n";
        std::cout << "Current Assignments:\n";
        for (const auto& [var, val] : instance.assignments) {
            std::cout << "x" << var << " = " << val << "\n";
        }
        std::cout << "Formula size: " << instance.formula.size() << "\n";
    }

    // Base Case: All clauses satisfied
    if (instance.formula.empty()) {
        if (instance.debug_output) {
            std::cout << "Formula empty. SATISFIABLE.\n";
        }
        return true;
    }

    // Base Case: Found an empty clause → UNSAT
    for (const auto& clause : instance.formula) {
        if (clause.empty()) {
            if (instance.debug_output) {
                std::cout << "Empty clause detected. UNSATISFIABLE.\n";
            }
            
            // VSIDS: Update activity for variables in the conflict
            instance.updateActivitiesFromConflict(clause);
            
            backtracks++; // We backtrack when we hit UNSAT
            return false;
        }
    }

    // Apply optimizations
    if (!unitPropagation(instance)) {
        if (instance.debug_output) {
            std::cout << "Conflict detected during unit propagation. UNSATISFIABLE.\n";
        }
        backtracks++; // Conflict detected during unit propagation
        return false;
    }
    pureLiteralElimination(instance);

    // VSIDS: Decay variable activities
    instance.decayVarActivities();
    
    // VSIDS: Pick a variable with the highest activity score
    int variable = instance.selectVarVSIDS();

    if (variable == 0) {
        if (instance.debug_output) {
            std::cout << "No unassigned variables left. SATISFIABLE.\n";
        }
        return true;  // No unassigned variables left
    }

    // Try assigning variable = true
    if (instance.debug_output) {
        std::cout << "Trying x" << variable << " = true (VSIDS activity: " << instance.activity[variable] << ")\n";
    }
    
    SATInstance newInstanceTrue = instance;
    newInstanceTrue.assignments[variable] = true;
    if (DPLL(newInstanceTrue)) return true;

    // Try assigning variable = false
    if (instance.debug_output) {
        std::cout << "Backtracking: Trying x" << variable << " = false\n";
    }
    
    SATInstance newInstanceFalse = instance;
    newInstanceFalse.assignments[variable] = false;
    backtracks++; // Backtracking occurs here
    return DPLL(newInstanceFalse);
}