# SAT Solver using DPLL Algorithm

A C++ implementation of the Davis–Putnam–Logemann–Loveland (DPLL) algorithm for solving Boolean satisfiability problems.

## Overview

This SAT solver efficiently determines whether a given Boolean formula in conjunctive normal form (CNF) is satisfiable. The implementation uses the DPLL algorithm with key optimizations:

- **Unit Propagation**: Simplifies the formula by recursively assigning values to unit clauses
- **Pure Literal Elimination**: Removes literals that appear in only one polarity

## Features

- Solves arbitrary Boolean formulas in CNF form
- Advanced optimizations:
  - Unit Propagation with conflict detection
  - Implied unit identification
  - Pure Literal Elimination
- Performance tracking (execution time, recursive calls, backtracks)
- Built-in test cases for verification:
  - Simple satisfiable formula
  - 4-Queens puzzle (satisfiable)
  - Pigeonhole principle (unsatisfiable)

## Project Structure

```
.
├── include/
│   ├── DPLL.h          # DPLL algorithm header
│   └── SATInstance.h   # SAT problem representation
├── src/
│   ├── DPLL.cpp        # DPLL algorithm implementation
│   └── main.cpp        # Main test harness
└── README.md
```

## Algorithm Details

### DPLL Algorithm

The Davis–Putnam–Logemann–Loveland (DPLL) algorithm is a complete, backtracking-based search algorithm for deciding the satisfiability of propositional logic formulas in conjunctive normal form. The algorithm:

1. Checks for base cases:
   - If the formula is empty, return SAT
   - If any clause is empty, return UNSAT
2. Applies optimizations:
   - Unit propagation
   - Pure literal elimination
3. Selects an unassigned variable
4. Recursively tries both TRUE and FALSE assignments
5. Backtracks if necessary

### Unit Propagation

Unit propagation simplifies SAT solving by recursively assigning values to unit clauses. A unit clause is a clause with only one literal, meaning its variable must be assigned a specific value to satisfy the clause. This process helps reduce the search space before backtracking.

Steps:
1. Scan the formula for unit clauses (single-literal clauses)
2. Assign the unit clause's variable to the required value (true/false)
3. Remove all clauses that are satisfied by the assignment
4. Remove the negated literal from other clauses
5. Repeat until no more unit clauses exist

### Conflict Detection

Conflict detection identifies contradictions early in the search process. When assigning a value to a variable would lead to a contradiction (such as creating an empty clause or contradicting a previous assignment), the solver immediately backtracks instead of continuing down an unsatisfiable path. This significantly reduces unnecessary recursive calls, especially for unsatisfiable formulas.

### Implied Units

Implied unit detection extends unit propagation by identifying situations where all but one literal in a clause have been assigned to FALSE, forcing the remaining literal to be TRUE to satisfy the clause. This reduces the search space by making necessary assignments earlier in the solving process, without requiring explicit branching decisions.

### Pure Literal Elimination

Pure literal elimination optimizes SAT solving by removing pure literals early. A literal is pure if it appears in only one polarity (always positive or always negative). Since pure literals cannot contribute to conflicts, they can be assigned immediately, and all clauses containing them can be removed.

## Usage

Compile the project:

```bash
g++ -std=c++11 -o sat_solver src/main.cpp src/DPLL.cpp -Iinclude
```

Run the program:

```bash
./sat_solver
```

## Example Input Format

A Boolean formula in CNF is represented as a vector of clauses, where each clause is a vector of integers. Positive integers represent positive literals, and negative integers represent negated literals.

```cpp
// Example: (x1 ∨ x2) ∧ (~x1 ∨ x3) ∧ (~x2 ∨ ~x3)
CNF exampleCNF = {{1, 2}, {-1, 3}, {-2, -3}};
```

## Test Cases

The program includes several test cases:

1. **Simple Satisfiable Formula**: A basic CNF formula to verify the solver works
2. **4-Queens Problem**: A classic constraint satisfaction problem encoded as CNF
3. **Pigeonhole Principle**: A provably unsatisfiable problem (5 pigeons, 4 holes)

## Performance Metrics

The solver tracks:
- Execution time in milliseconds
- Number of recursive DPLL calls
- Number of backtracking operations

## License

[MIT License](LICENSE)

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
