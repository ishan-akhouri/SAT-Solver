# Advanced SAT Solver with DPLL and CDCL

A C++ implementation of modern SAT solving algorithms for Boolean satisfiability problems, featuring both DPLL with VSIDS and a full CDCL implementation with non-chronological backtracking.

## Overview

This SAT solver efficiently determines whether a given Boolean formula in conjunctive normal form (CNF) is satisfiable. The implementation includes two major solving approaches:

1. **DPLL Algorithm with VSIDS**: The classic Davis–Putnam–Logemann–Loveland algorithm enhanced with Variable State Independent Decaying Sum heuristic
2. **CDCL with Non-Chronological Backtracking**: A modern Conflict-Driven Clause Learning implementation that significantly outperforms DPLL on complex problems

## Features

- Choose between DPLL and CDCL solving algorithms
- Advanced optimizations:
  - **VSIDS variable selection heuristic** (both solvers)
  - **Unit Propagation with conflict detection** (both solvers)
  - **Watched literals optimization** (CDCL only)
  - **Conflict-Driven Clause Learning** with non-chronological backtracking (CDCL only)
  - **Restart strategy** for escaping unproductive search spaces (CDCL only)
  - **Pure Literal Elimination** (DPLL only)
- Performance tracking (execution time, conflicts, decisions, propagations, etc.)
- Built-in benchmark suite:
  - Simple satisfiable formula
  - 4-Queens puzzle (satisfiable)
  - 8-Queens puzzle (satisfiable)
  - Pigeonhole principle (unsatisfiable)
  - Random 3-SAT instances at various clause-to-variable ratios

## Project Structure

```
.
├── include/
│   ├── DPLL.h          # DPLL algorithm header
│   ├── CDCL.h          # CDCL algorithm header
│   └── SATInstance.h   # SAT problem representation with VSIDS support
├── src/
│   ├── DPLL.cpp        # DPLL algorithm implementation
│   ├── CDCL.cpp        # CDCL algorithm implementation 
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
3. Selects an unassigned variable using VSIDS
4. Recursively tries both TRUE and FALSE assignments
5. Backtracks if necessary

### CDCL (Conflict-Driven Clause Learning)

CDCL is a modern SAT solving algorithm that dramatically improves upon DPLL by:

1. **Learning from conflicts**: When the solver reaches a contradiction, it analyzes the conflict to learn a new clause that helps avoid similar conflicts in the future
2. **Non-chronological backtracking**: Instead of always backtracking to the most recent decision, the solver can jump directly to the decision level that caused the conflict
3. **Watched literals**: An efficient data structure for quick detection of unit clauses and conflicts
4. **Restart strategy**: Periodically restarting the search while keeping learned clauses helps escape from getting stuck in unproductive areas of the search space

The CDCL implementation tracks an implication graph with decision and propagation trails to perform conflict analysis. This approach significantly reduces the search space for complex problems.

### VSIDS (Variable State Independent Decaying Sum)

VSIDS is an adaptive variable selection heuristic that dramatically improves solver performance by prioritizing variables involved in recent conflicts. The implementation includes:

- **Activity Tracking**: Maintains an activity score for each variable
- **Conflict-Based Bumping**: Increases scores for variables involved in conflicts
- **Periodic Decay**: Gradually reduces all scores to favor recent conflicts
- **Intelligent Selection**: Always chooses the unassigned variable with highest activity

This approach focuses the search on the most contentious parts of the formula, significantly reducing search space exploration for structured problems.

### Unit Propagation

Unit propagation simplifies SAT solving by assigning values to variables that are forced by the formula. A unit clause is a clause with only one unassigned literal, meaning its variable must be assigned a specific value to satisfy the clause.

In CDCL, unit propagation is enhanced with the watched literals optimization, which significantly reduces the number of clauses that need to be checked after each assignment.

### Watched Literals Optimization

The watched literals technique efficiently detects unit clauses and conflicts during propagation:

1. For each clause, the solver watches only two literals
2. When a watched literal is falsified, the solver attempts to find a new literal to watch
3. If no new watch can be found, the clause becomes a unit clause or a conflict

This approach avoids having to scan all clauses after each variable assignment, providing a significant performance boost for large formulas.

## Usage

Compile the project:

```bash
g++ -std=c++17 -o sat_solver src/main.cpp src/DPLL.cpp src/CDCL.cpp -Iinclude
```

Run the program:

```bash
./sat_solver           # Run with CDCL (default)
./sat_solver --dpll    # Run with DPLL algorithm
./sat_solver --cdcl    # Run with CDCL algorithm (explicit)
./sat_solver --debug   # Run with detailed debug output
```

## Example Input Format

A Boolean formula in CNF is represented as a vector of clauses, where each clause is a vector of integers. Positive integers represent positive literals, and negative integers represent negated literals.

```cpp
// Example: (x1 ∨ x2) ∧ (~x1 ∨ x3) ∧ (~x2 ∨ ~x3)
CNF exampleCNF = {{1, 2}, {-1, 3}, {-2, -3}};
```

## Benchmark Suite

The program includes several test cases:

1. **Simple Satisfiable Formula**: A basic CNF formula to verify the solver works
2. **4-Queens Problem**: A classic constraint satisfaction problem encoded as CNF
3. **8-Queens Problem**: A larger version of the queens problem to demonstrate solver efficiency
4. **Pigeonhole Principle**: A provably unsatisfiable problem (5 pigeons, 4 holes)
5. **Random 3-SAT**: Randomly generated instances with varying clause-to-variable ratios to test solver robustness

The random 3-SAT tests include instances around the phase transition (clause-to-variable ratio of approximately 4.25), where problems are typically hardest to solve.

## Performance Metrics

The solver tracks different metrics depending on the algorithm:

For DPLL:
- Execution time in milliseconds
- Number of recursive DPLL calls
- Number of backtracking operations

For CDCL:
- Execution time in milliseconds
- Number of conflicts
- Number of decisions
- Number of unit propagations
- Number of learned clauses
- Maximum decision level reached
- Number of restarts

## Next Steps

Future enhancements planned for this solver include:
- Further optimization of clause database management
- Integration with SMT (Satisfiability Modulo Theories) solving
- Preprocessing techniques for formula simplification
- MaxSAT capabilities for optimization problems
- Parallel solving with portfolio approach

## License

[MIT License](LICENSE)

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.