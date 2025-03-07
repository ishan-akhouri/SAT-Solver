#include "../include/DPLL.h"

// Global performance counters
int dpll_calls = 0;  // Tracks number of recursive DPLL calls
int backtracks = 0;  // Tracks number of backtracking operations

/*
unitPropagation simplifies SAT solving by recursively assigning values to unit clauses.
A unit clause is a clause with only one literal, meaning its variable must be assigned
a specific value to satisfy the clause. This process helps reduce the search space
before backtracking, making solving more efficient.

Steps:
1. Scan the formula for unit clauses (single-literal clauses).
2. Assign the unit clause's variable to the required value (true/false).
3. Remove all clauses that are satisfied by the assignment.
4. Repeat until no more unit clauses exist.
5. Continue solving with a simplified formula.
*/
bool unitPropagation(SATInstance& instance) {
    bool changed = false; // Tracks whether any unit clauses were found and propagated in this iteration
    do {
        changed = false; // Begins a loop that runs until no new clauses are found
        for (auto it = instance.formula.begin(); it != instance.formula.end(); ) {
            if (it->size() == 1) {  // Found a unit clause
                int unitLiteral = (*it)[0]; // Extracts the only literal from the unit clause
                int var = abs(unitLiteral);
                bool value = (unitLiteral > 0);

                // Conflict detection
                if (instance.assignments.count(var) && instance.assignments[var] != value) {
                    std::cout << "Conflict detected: x" << var << " already assigned " << instance.assignments[var] << "\n";
                    return false; // Conflict detected
                }

                // Assign the variable and remove the clause
                instance.assignments[var] = value;
                std::cout << "Unit Propagation: Assigning x" << var << " = " << value << "\n";
                it = instance.formula.erase(it);
                changed = true;

                // Remove negated literal from other clauses
                for (auto& clause : instance.formula) {
                    auto negPos = std::find(clause.begin(), clause.end(), -unitLiteral);
                    if (negPos != clause.end()) {
                        clause.erase(negPos);
                        if (clause.empty()) {
                            std::cout << "Empty clause detected after removing x" << var << "\n";
                            return false; // Empty clause detected
                        }
                    }
                }
                break;  // Restart iteration, ensuring unit propagation applies to the simplified formula
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
                        std::cout << "Conflict detected: x" << var << " already assigned " << instance.assignments[var] << "\n";
                        return false; // Conflict detected
                    }

                    // Assign the variable and remove the clause
                    instance.assignments[var] = value;
                    std::cout << "Implied Unit: Assigning x" << var << " = " << value << "\n";
                    it = instance.formula.erase(it);
                    changed = true;

                    // Remove negated literal from other clauses
                    for (auto& clause : instance.formula) {
                        auto negPos = std::find(clause.begin(), clause.end(), -unassignedLit);
                        if (negPos != clause.end()) {
                            clause.erase(negPos);
                            if (clause.empty()) {
                                std::cout << "Empty clause detected after removing x" << var << "\n";
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
    } while (changed); // Repeat the loop until no more unit clauses are found
    
    std::cout << "Unit propagation done. Formula size: " << instance.formula.size() << "\n";
    return true; // Indicate that unit propagation has completed successfully
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
    std::unordered_map<int, bool> seenLiterals; // Track which literals appear in the formula

    for (const auto& clause : instance.formula) { // Loop through all clauses in the formula
        for (int literal : clause) { // Loop through all the literals in the clause
            seenLiterals[literal] = true; // Store each literal in the hashmap and mark it as seen
        }
    }

    std::vector<int> pureLiterals; // Store pure literals
    for (const auto& [literal, _] : seenLiterals) { // Loop through seen literals
        if (seenLiterals.find(-literal) == seenLiterals.end()) { // If the literal's negation doesn't exist
            pureLiterals.push_back(literal); // Store it into the pureLiterals vector
        }
    }

    for (int literal : pureLiterals) { // Loop through the pure literals
        instance.assignments[abs(literal)] = (literal > 0); // Assign the pure literal to true
        std::cout << "Pure Literal Elimination: Assigning x" << abs(literal) << " = " << (literal > 0) << "\n";
        instance.formula.erase(std::remove_if(instance.formula.begin(), instance.formula.end(), // Remove satisfied clauses from the CNF formula
            [literal](const Clause& c) { return std::find(c.begin(), c.end(), literal) != c.end(); }),
            instance.formula.end());
    }
}

/*
DPLL recursively solves SAT with two base cases:
1. If the formula is empty, return true (SAT).
2. If any clause is empty, return false (UNSAT).

It applies optimizations, picks an unassigned variable, and tries both true and false.
If neither works, return false (UNSAT).
*/
bool DPLL(SATInstance& instance) {
    dpll_calls++;
    std::cout << "DPLL Call #" << dpll_calls << "\n";
    std::cout << "Current Assignments:\n";
    for (const auto& [var, val] : instance.assignments) {
        std::cout << "x" << var << " = " << val << "\n";
    }
    std::cout << "Formula size: " << instance.formula.size() << "\n";

    // Base Case: All clauses satisfied
    if (instance.formula.empty()) {
        std::cout << "Formula empty. SATISFIABLE.\n";
        return true;
    }

    // Base Case: Found an empty clause → UNSAT
    for (const auto& clause : instance.formula) {
        if (clause.empty()) {
            std::cout << "Empty clause detected. UNSATISFIABLE.\n";
            backtracks++; // We backtrack when we hit UNSAT
            return false;
        }
    }

    // Apply optimizations
    if (!unitPropagation(instance)) {
        std::cout << "Conflict detected during unit propagation. UNSATISFIABLE.\n";
        backtracks++; // Conflict detected during unit propagation
        return false;
    }
    pureLiteralElimination(instance);

    // Pick a variable to assign (first unassigned one)
    int variable = 0;
    for (const auto& clause : instance.formula) { // Loop through the literals in each clause
        for (int literal : clause) {
            if (instance.assignments.find(abs(literal)) == instance.assignments.end()) { // If variable is not assigned, assign it
                variable = abs(literal);
                break; // Stop searching when an unassigned variable is found
            }
        }
        if (variable != 0) break;
    }

    if (variable == 0) {
        std::cout << "No unassigned variables left. SATISFIABLE.\n";
        return true;  // No unassigned variables left
    }

    // Try assigning variable = true
    std::cout << "Trying x" << variable << " = true\n";
    SATInstance newInstanceTrue = instance;
    newInstanceTrue.assignments[variable] = true;
    if (DPLL(newInstanceTrue)) return true;

    // Try assigning variable = false
    std::cout << "Backtracking: Trying x" << variable << " = false\n";
    SATInstance newInstanceFalse = instance;
    newInstanceFalse.assignments[variable] = false;
    backtracks++; // Backtracking occurs here
    return DPLL(newInstanceFalse);
}