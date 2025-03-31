#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include "../include/CDCLSolverIncremental.h"

// Class for incrementally solving N-Queens problems
class IncrementalQueensSolver {
private:
    int n;  // Board size
    std::unique_ptr<CDCLSolverIncremental> solver;
    
    // Map board positions to variable IDs
    int posToVar(int row, int col) const {
        return row * n + col + 1;  // 1-indexed variables
    }
    
    // Map variable ID to board position
    std::pair<int, int> varToPos(int var) const {
        var--; // Adjust for 1-indexing
        return {var / n, var % n};
    }
    
public:
    IncrementalQueensSolver(int board_size) : n(board_size) {
        // Create initial CNF formula with constraints
        CNF initial_formula;
        
        // At least one queen in each row
        for (int row = 0; row < n; row++) {
            Clause at_least_one_in_row;
            for (int col = 0; col < n; col++) {
                at_least_one_in_row.push_back(posToVar(row, col));
            }
            initial_formula.push_back(at_least_one_in_row);
        }
        
        // At most one queen in each row
        for (int row = 0; row < n; row++) {
            for (int col1 = 0; col1 < n; col1++) {
                for (int col2 = col1 + 1; col2 < n; col2++) {
                    initial_formula.push_back({-posToVar(row, col1), -posToVar(row, col2)});
                }
            }
        }
        
        // At least one queen in each column
        for (int col = 0; col < n; col++) {
            Clause at_least_one_in_col;
            for (int row = 0; row < n; row++) {
                at_least_one_in_col.push_back(posToVar(row, col));
            }
            initial_formula.push_back(at_least_one_in_col);
        }
        
        // At most one queen in each column
        for (int col = 0; col < n; col++) {
            for (int row1 = 0; row1 < n; row1++) {
                for (int row2 = row1 + 1; row2 < n; row2++) {
                    initial_formula.push_back({-posToVar(row1, col), -posToVar(row2, col)});
                }
            }
        }
        
        // No two queens on the same diagonal (top-left to bottom-right)
        for (int sum = 0; sum < 2*n - 1; sum++) {
            for (int row1 = 0; row1 < n; row1++) {
                int col1 = sum - row1;
                if (col1 < 0 || col1 >= n) continue;
                
                for (int row2 = row1 + 1; row2 < n; row2++) {
                    int col2 = sum - row2;
                    if (col2 < 0 || col2 >= n) continue;
                    
                    initial_formula.push_back({-posToVar(row1, col1), -posToVar(row2, col2)});
                }
            }
        }
        
        // No two queens on the same diagonal (top-right to bottom-left)
        for (int diff = -(n-1); diff < n; diff++) {
            for (int row1 = 0; row1 < n; row1++) {
                int col1 = row1 - diff;
                if (col1 < 0 || col1 >= n) continue;
                
                for (int row2 = row1 + 1; row2 < n; row2++) {
                    int col2 = row2 - diff;
                    if (col2 < 0 || col2 >= n) continue;
                    
                    initial_formula.push_back({-posToVar(row1, col1), -posToVar(row2, col2)});
                }
            }
        }
        
        // Create solver with the initial formula
        solver = std::make_unique<CDCLSolverIncremental>(initial_formula, false);
        
        std::cout << "Created N-Queens solver for " << n << "x" << n << " board.\n";
        std::cout << "Formula has " << solver->getNumVars() << " variables and " 
                  << solver->getNumClauses() << " clauses.\n";
    }
    
    // Find a solution with given constraints
    bool solve() {
        auto start = std::chrono::high_resolution_clock::now();
        bool result = solver->solve();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        
        std::cout << "Solution " << (result ? "FOUND" : "NOT FOUND") << "\n";
        std::cout << "Time: " << std::fixed << std::setprecision(3) << elapsed.count() << " ms\n";
        std::cout << "Conflicts: " << solver->getConflicts() << "\n";
        std::cout << "Decisions: " << solver->getDecisions() << "\n";
        std::cout << "Propagations: " << solver->getPropagations() << "\n";
        std::cout << "Restarts: " << solver->getRestarts() << "\n";
        
        if (result) {
            printSolution();
        }
        
        return result;
    }
    
    // Add a constraint that the current solution is excluded
    void excludeCurrentSolution() {
        if (!solver) return;
        
        const auto& assignments = solver->getAssignments();
        
        // Create a clause that excludes the current solution
        Clause blocking_clause;
        for (int row = 0; row < n; row++) {
            for (int col = 0; col < n; col++) {
                int var = posToVar(row, col);
                auto it = assignments.find(var);
                if (it != assignments.end() && it->second) {
                    // If this position has a queen, exclude it in the next solution
                    blocking_clause.push_back(-var);
                }
            }
        }
        
        // Add the blocking clause to the solver
        solver->addClause(blocking_clause);
        
        std::cout << "Added blocking clause to exclude current solution.\n";
    }
    
    // Find all solutions
    int findAllSolutions(int max_solutions = -1) {
        int count = 0;
        
        while ((max_solutions == -1 || count < max_solutions) && solve()) {
            count++;
            excludeCurrentSolution();
        }
        
        std::cout << "Found " << count << " solutions.\n";
        return count;
    }
    
    // Place a queen at a specific position
    void placeQueen(int row, int col) {
        if (!solver) return;
        
        std::vector<int> assumptions = {posToVar(row, col)};
        solver->setAssumptions(assumptions);
        
        std::cout << "Added constraint: Queen at position (" << row << ", " << col << ")\n";
    }
    
    // Print the current solution
    void printSolution() const {
        if (!solver) return;
        
        const auto& assignments = solver->getAssignments();
        
        std::cout << "Solution:\n";
        for (int row = 0; row < n; row++) {
            for (int col = 0; col < n; col++) {
                int var = posToVar(row, col);
                auto it = assignments.find(var);
                
                if (it != assignments.end() && it->second) {
                    std::cout << "Q ";
                } else {
                    std::cout << ". ";
                }
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }
};

// Example incremental solving application
int main(int argc, char* argv[]) {
    std::cout << "Incremental N-Queens Solver Demo\n";
    std::cout << "--------------------------------\n\n";
    
    // Parse board size argument
    int n = 8;  // Default size
    if (argc > 1) {
        n = std::stoi(argv[1]);
    }
    
    if (n < 4) {
        std::cout << "Board size must be at least 4x4\n";
        return 1;
    }
    
    // Create the solver
    IncrementalQueensSolver solver(n);
    
    std::cout << "\n1. Finding first solution...\n";
    bool result = solver.solve();
    
    if (!result) {
        std::cout << "No solution exists!\n";
        return 1;
    }
    
    std::cout << "\n2. Finding another solution with queen at position (0, 0)...\n";
    solver.placeQueen(0, 0);
    result = solver.solve();
    
    if (!result) {
        std::cout << "No solution exists with queen at (0, 0)!\n";
    }
    
    std::cout << "\n3. Finding first 5 solutions...\n";
    solver = IncrementalQueensSolver(n);  // Reset the solver
    int num_solutions = solver.findAllSolutions(5);
    
    std::cout << "\nTotal solutions found: " << num_solutions << "\n";
    
    return 0;
}