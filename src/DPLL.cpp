#include "../include/DPLL.h" 

//This code implements the Davis-Putnam-Logemann-Loveland (DPLL) algorithm, 
//a recursive backtracking SAT solver that incorporates unit propagation to simplify the formula.

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
    bool changed = false; //Tracks whether any unit clauses were found and propagated in this iteration
    do {
        changed = false; //Begins a loop that runs until no new clauses are found
        for (auto& clause : instance.formula) { //Loop through all clauses in the CNF formula
            if (clause.size() == 1) {  // Found a unit clause
                int unitLiteral = clause[0]; // Extracts the only literal from the unit clause
                instance.assignments[abs(unitLiteral)] = (unitLiteral > 0); // Assign true or false based on the sign
                //If  ~x1 (represented as [-1]) exists, set x1 = false.
                
                // Removes clauses that contain the newly assigned literal since they are now satisfied
                instance.formula.erase(std::remove_if(instance.formula.begin(), instance.formula.end(),
                    [unitLiteral](const Clause& c) {
                        return std::find(c.begin(), c.end(), unitLiteral) != c.end();
                    }), instance.formula.end());
                
                changed = true; //Signals that a new assignment was made and another iteration is needed
                break;  // Restart iteration, ensuring unit propagation applies to the simplified formula
            }
        }
    } while (changed); //Repeat the loop until no more unit clauses are found
    
    return true; //Indicate that unit propagation has completed successfully
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


    for (const auto& clause : instance.formula) { //Loop through all clauses in the formula
        for (int literal : clause) { // Loop through all the literals in the clause
            seenLiterals[literal] = true; //Store each literal in the hashmap and mark it as seen
        }
    }

    std::vector<int> pureLiterals; //Store pure literals
    for (const auto& [literal, _] : seenLiterals) { //Loop through seen literals
        if (seenLiterals.find(-literal) == seenLiterals.end()) { //If the literal's negation doesn't exist
            pureLiterals.push_back(literal);//Store it into the pureLiterals vector
        }
    }

    
    for (int literal : pureLiterals) { //Loop through the pure literals
        instance.assignments[abs(literal)] = (literal > 0); //Assign the pure literal to true
        instance.formula.erase(std::remove_if(instance.formula.begin(), instance.formula.end(), //Remove satisfied clauses from the CNF formula
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

int dpll_calls = 0; //Global counter of recursive calls
int backtracks = 0; //Global counter of backtracks;

bool DPLL(SATInstance& instance) {

    dpll_calls++;
    // Base Case: All clauses satisfied
    if (instance.formula.empty()) return true;
    
    // Base Case: Found an empty clause → UNSAT
    for (const auto& clause : instance.formula) {
        if (clause.empty())
        {   backtracks++; //We backtrack when we hit UNSAT
            return false;
        }
            
    }

    // Apply optimizations
    pureLiteralElimination(instance);
    unitPropagation(instance);

    // Pick a variable to assign (first unassigned one)
    int variable = 0;
    for (const auto& clause : instance.formula) { // Loop through the literals in each clause
        for (int literal : clause) {
            if (instance.assignments.find(abs(literal)) == instance.assignments.end()) { //If variable is not assigned, assign it
                variable = abs(literal);
                break; //Stop searching when an unassigned variable is found
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