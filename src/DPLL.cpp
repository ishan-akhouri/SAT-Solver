#include "../include/DPLL.h" 

//This code implements the Davis-Putnam-Logemann-Loveland (DPLL) algorithm, 
//a recursive backtracking SAT solver that incorporates unit propagation to simplify the formula.




/*
unitPropagation assigns values to unit clauses (single-literal clauses),
removes satisfied clauses, and repeats until no new unit clauses remain.
*/

bool unitPropagation(SATInstance& instance) {
    bool changed = false;
    do {
        changed = false;
        for (auto& clause : instance.formula) {
            if (clause.size() == 1) {  // Found a unit clause
                int unitLiteral = clause[0];
                instance.assignments[abs(unitLiteral)] = (unitLiteral > 0); // If a unit clause x1 exists, set x1 = true. 
                //If  ~x1 (represented as [-1]) exists, set x1 = false.
                
                // Remove satisfied clauses and propagate
                instance.formula.erase(std::remove_if(instance.formula.begin(), instance.formula.end(),
                    [unitLiteral](const Clause& c) {
                        return std::find(c.begin(), c.end(), unitLiteral) != c.end();
                    }), instance.formula.end());
                
                changed = true;
                break;  // Restart iteration
            }
        }
    } while (changed);
    
    return true;
}

/*
DPLL recursively solves SAT with two base cases:
1. If the formula is empty, return true (SAT).
2. If any clause is empty, return false (UNSAT).

It applies unit propagation, picks an unassigned variable, and tries both true and false.
If neither works, return false (UNSAT).
*/

bool DPLL(SATInstance& instance) {
    // Base Case: All clauses satisfied
    if (instance.formula.empty()) return true;
    
    // Base Case: Found an empty clause → UNSAT
    for (const auto& clause : instance.formula) {
        if (clause.empty()) return false;
    }

    // Apply Unit Propagation
    unitPropagation(instance);

    // Pick a variable to assign (first unassigned one)
    int variable = 0;
    for (const auto& clause : instance.formula) {
        for (int literal : clause) {
            if (instance.assignments.find(abs(literal)) == instance.assignments.end()) {
                variable = abs(literal);
                break;
            }
        }
        if (variable != 0) break;
    }

    if (variable == 0) return true;  // No unassigned variables left

    // Try assigning variable = true
    SATInstance newInstanceTrue = instance;
    newInstanceTrue.assignments[variable] = true;
    if (DPLL(newInstanceTrue)) return true;

    // Try assigning variable = false
    SATInstance newInstanceFalse = instance;
    newInstanceFalse.assignments[variable] = false;
    return DPLL(newInstanceFalse);
}