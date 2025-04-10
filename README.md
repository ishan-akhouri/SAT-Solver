# Advanced SAT Solver with DPLL, CDCL, and Incremental Solving

A C++ implementation of modern SAT solving algorithms for Boolean satisfiability problems, featuring DPLL with VSIDS, a full CDCL implementation with non-chronological backtracking, and a state-of-the-art incremental SAT solver with sophisticated clause database management.

## Overview

This SAT solver efficiently determines whether a given Boolean formula in conjunctive normal form (CNF) is satisfiable. The implementation includes three major solving approaches:

1. **DPLL Algorithm with VSIDS**: The classic Davis–Putnam–Logemann–Loveland algorithm enhanced with Variable State Independent Decaying Sum heuristic
2. **CDCL with Non-Chronological Backtracking**: A modern Conflict-Driven Clause Learning implementation that significantly outperforms DPLL on complex problems
3. **Incremental CDCL Solver**: A sophisticated incremental solver with reference-counted clause database management, optimized for solving sequences of related formulas efficiently

## Features

### Standard SAT Solving (DPLL and CDCL)
- Choose between DPLL and CDCL solving algorithms
- Advanced optimizations:
  - **VSIDS variable selection heuristic** (both solvers)
  - **Unit Propagation with conflict detection** (both solvers)
  - **Watched literals optimization** (CDCL only)
  - **Conflict-Driven Clause Learning** with non-chronological backtracking (CDCL only)
  - **Restart strategy** for escaping unproductive search spaces (CDCL only)
  - **Pure Literal Elimination** (DPLL only)

### Incremental SAT Solving
- **Reference-counted clause database management** for efficient memory utilization
- **Assumption-based incremental solving** for efficient "what-if" scenarios
- **Watch literal management** that preserves state between solving calls
- **Conflict-Driven Clause Learning** with state preservation between incremental calls
- **UNSAT core extraction** to identify the minimal set of contradictory assumptions
- **Adaptive restart strategies** with Luby sequence support
- **Clause minimization techniques**:
  - Recursive minimization
  - Self-subsumption
  - Binary resolution
  - Vivification (optional)
- **Dynamic clause quality assessment**:
  - Activity-based scoring
  - Literal Block Distance (LBD) metrics
  - Smart clause deletion policies

### Performance Features
- **Efficient memory management** with reference counting for clauses
- **Sophisticated watch list implementation** for unit propagation
- **Robust conflict analysis** with First UIP scheme
- **Non-chronological backtracking** for efficient search space exploration
- **Phase saving** for branching decisions
- **Timeout detection** to prevent excessive runtime
- **Enhanced stuck detection** with recovery mechanisms

## Project Structure

```
.
├── include/
│   ├── SATInstance.h         # Common SAT problem representation with VSIDS
│   ├── DPLL.h                # DPLL algorithm header
│   ├── CDCL.h                # CDCL algorithm header
│   ├── CDCLSolverIncremental.h   # Incremental CDCL solver header
│   ├── ClauseDatabase.h      # Reference-counted clause database
│   └── ClauseMinimizer.h     # Clause minimization techniques
├── src/
│   ├── DPLL.cpp              # DPLL algorithm implementation
│   ├── CDCL.cpp              # CDCL algorithm implementation
│   ├── CDCLSolverIncremental.cpp # Incremental CDCL solver implementation
│   ├── ClauseDatabase.cpp    # Clause database implementation
│   ├── ClauseMinimizer.cpp   # Clause minimization techniques
│   ├── main.cpp              # Main test harness for standard SAT solving
│   ├── main_incremental.cpp  # Main test harness for incremental SAT solving
│   └── IncrementalQueensSolver.cpp # Example application (N-Queens)
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

### Incremental CDCL with Clause Database Management

The incremental solver extends CDCL with sophisticated clause database management and state preservation between solving calls:

1. **Reference-counted clause management**: Using smart pointers for efficient memory management without dangling pointers
2. **Assumption-based solving**: Temporary constraints are handled as assumptions rather than requiring formula reconstruction
3. **Clause database optimization**: 
   - Efficient garbage collection
   - Quality-based clause retention using LBD and activity metrics
   - Watched literals preservation between calls
4. **UNSAT core extraction**: When problems are unsatisfiable, the solver identifies minimal sets of contradictory assumptions
5. **State preservation**: Learned clauses, variable activities, and phase information persist across solving calls

This paradigm shift is critical for applications like bounded model checking, planning problems, and interactive constraint solving, where we repeatedly solve related formulas with small variations.

### VSIDS (Variable State Independent Decaying Sum)

VSIDS is an adaptive variable selection heuristic that dramatically improves solver performance by prioritizing variables involved in recent conflicts. The implementation includes:

- **Activity Tracking**: Maintains an activity score for each variable
- **Conflict-Based Bumping**: Increases scores for variables involved in conflicts
- **Periodic Decay**: Gradually reduces all scores to favor recent conflicts
- **Intelligent Selection**: Always chooses the unassigned variable with highest activity

This approach focuses the search on the most contentious parts of the formula, significantly reducing search space exploration for structured problems.

### Watched Literals Optimization

The watched literals technique efficiently detects unit clauses and conflicts during propagation:

1. For each clause, the solver watches only two literals
2. When a watched literal is falsified, the solver attempts to find a new literal to watch
3. If no new watch can be found, the clause becomes a unit clause or a conflict

This approach avoids having to scan all clauses after each variable assignment, providing a significant performance boost for large formulas.

### Clause Minimization Techniques

The incremental solver implements several sophisticated clause minimization techniques:

1. **Recursive Minimization**: Uses the implication graph to identify and remove redundant literals
2. **Self-Subsumption**: Identifies and removes literals that can be implied by the rest of the clause
3. **Binary Resolution**: Uses binary clauses to strengthen the learned clause
4. **Vivification**: Tentatively assigns literals to strengthen the clause

These techniques significantly reduce the size of learned clauses, making them more effective for pruning the search space.

## Usage

### Standard SAT Solver

Compile the standard solver:

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

### Incremental SAT Solver

Compile the incremental solver:

```bash
g++ -std=c++17 -o sat_solver_incremental src/main_incremental.cpp src/CDCLSolverIncremental.cpp src/ClauseDatabase.cpp src/ClauseMinimizer.cpp -Iinclude
```

Run the program:

```bash
./sat_solver_incremental                       # Run all demonstrations
./sat_solver_incremental incremental           # Demonstrate incremental solving
./sat_solver_incremental benchmarks            # Run standard benchmarks
./sat_solver_incremental random                # Benchmark random instances
./sat_solver_incremental minimization          # Demonstrate clause minimization
./sat_solver_incremental unsat-core            # Demonstrate UNSAT core extraction
./sat_solver_incremental enumerate 20 3.0      # Enumerate solutions (20 vars, ratio 3.0)
./sat_solver_incremental debug                 # Run basic debugging tests
```

### Using the Solver in Your Code

#### Standard CDCL:

```cpp
#include "CDCL.h"

// Create a formula in CNF
CNF formula = {{1, 2}, {-1, 3}, {-2, -3}};

// Create solver
CDCLSolver solver(formula);

// Solve
bool result = solver.solve();

// Get the assignments if satisfiable
if (result) {
    const auto& assignments = solver.getAssignments();
}
```

#### Incremental Solving:

```cpp
#include "CDCLSolverIncremental.h"

// Create a formula in CNF
CNF formula = {{1, 2}, {-1, 3}, {-2, -3}};

// Create solver
CDCLSolverIncremental solver(formula);

// Solve
bool result = solver.solve();

// Add a new clause
solver.addClause({4, 5});

// Solve with assumptions
std::vector<int> assumptions = {1, -4};
result = solver.solve(assumptions);

// Get UNSAT core if unsatisfiable
if (!result) {
    std::vector<int> core = solver.getUnsatCore();
}
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
5. **Hard Pigeonhole Principle**: A more challenging instance (6 pigeons, 5 holes)
6. **Random 3-SAT**: Randomly generated instances with varying clause-to-variable ratios to test solver robustness
7. **Incremental 3-Colorability**: Demonstrates incremental solving with graph coloring
8. **UNSAT Core Extraction**: Shows how to identify minimal sets of contradictory assumptions

The random 3-SAT tests include instances around the phase transition (clause-to-variable ratio of approximately 4.25), where problems are typically hardest to solve.

## Performance Metrics

The solvers track different metrics depending on the algorithm:

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

For Incremental CDCL:
- All CDCL metrics plus:
- UNSAT core size
- Clause minimization statistics
- State preservation effectiveness
- Incremental solving efficiency

## Real-World Applications

The incremental solver is particularly well-suited for:

1. **Bounded Model Checking**: Verify increasingly longer execution paths
2. **Planning Problems**: Add constraints incrementally as the plan unfolds
3. **Circuit Verification**: Check properties with incremental constraints
4. **Interactive Constraint Solving**: Add user constraints in real-time
5. **N-Queens Solving**: Find all solutions for the N-Queens problem
6. **Graph Coloring**: Incrementally add edges and verify colorability

## Next Steps

Future enhancements planned for this solver include:
- **Parallelization Strategies**: Multithreading implementation and portfolio-based parallel solving
- **Advanced Preprocessing Techniques**: Variable and clause elimination, subsumption, etc.
- **MaxSAT Extensions**: Core-guided MaxSAT algorithms and optimization capabilities
- **QBF Solving**: Quantified Boolean Formula solving for PSPACE-hard problems
- **Comprehensive Benchmarking and Analysis**: Performance against SAT competition benchmarks

## License

[MIT License](LICENSE)

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.