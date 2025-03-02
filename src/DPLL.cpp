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
pureLiteralElimination optimizes SAT solving by removing pure literals early.

A literal is pure if it appears in only one polarity (always positive or always negative).
Since pure literals cannot contribute to conflicts, they can be immediately assigned
and all clauses containing them can be removed, reducing the search space.

Steps:
1. Scan the formula to identify all literals.
2. Find literals that do not appear in both positive and negative forms.
3. Assign pure literals immediately and remove affected clauses.
4. Continue solving with a reduced formula.
*/
void pureLiteralElimination(SATInstance& instance) {
    std::unordered_map<int, bool> seenLiterals;

    //Identify Pure Literals
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

    //Assign Pure Literals & Remove Their Clauses
    for (int literal : pureLiterals) {
        instance.assignments[abs(literal)] = (literal > 0);
        instance.formula.erase(std::remove_if(instance.formula.begin(), instance.formula.end(),
            [literal](const Clause& c) { return std::find(c.begin(), c.end(), literal) != c.end(); }),
            instance.formula.end());
    }
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

    // Apply optimizations
    pureLiteralElimination(instance);
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