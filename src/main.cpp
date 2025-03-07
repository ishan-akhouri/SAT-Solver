#include <iostream>
#include "../include/SATInstance.h"
#include "../include/DPLL.h"
#include <chrono>

int main() {
    std::cout << "SAT Solver Starting..." << std::endl;

    // Test 1: Simple Satisfiable Formula
    std::cout << "\nTesting Simple Satisfiable CNF:\n";
    CNF exampleCNF = {{1, 2}, {-1, 3}, {-2, -3}};
    SATInstance instance1(exampleCNF);
    instance1.print();

    auto start = std::chrono::high_resolution_clock::now();
    bool result1 = DPLL(instance1);
    auto end = std::chrono::high_resolution_clock::now();

    std::cout << (result1 ? "SATISFIABLE" : "UNSATISFIABLE") << std::endl;
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Execution Time: " << elapsed.count() << " ms\n";
    std::cout << "Recursive Calls: " << dpll_calls << "\n";
    std::cout << "Backtracks: " << backtracks << "\n";

    // Reset performance counters
    dpll_calls = 0;
    backtracks = 0;

    // Test 2: 4-Queens Problem (Satisfiable)
    std::cout << "\nTesting 4-Queens Problem (Satisfiable):\n";
    CNF satCNF = {
        // At least one queen in each row
        {1, 2, 3, 4},     // Row 1: Queen in column 1, 2, 3, or 4
        {5, 6, 7, 8},     // Row 2: Queen in column 1, 2, 3, or 4
        {9, 10, 11, 12},  // Row 3: Queen in column 1, 2, 3, or 4
        {13, 14, 15, 16}, // Row 4: Queen in column 1, 2, 3, or 4
        
        // At most one queen in each row (no two queens in same row)
        // Row 1
        {-1, -2}, {-1, -3}, {-1, -4}, {-2, -3}, {-2, -4}, {-3, -4},
        // Row 2
        {-5, -6}, {-5, -7}, {-5, -8}, {-6, -7}, {-6, -8}, {-7, -8},
        // Row 3
        {-9, -10}, {-9, -11}, {-9, -12}, {-10, -11}, {-10, -12}, {-11, -12},
        // Row 4
        {-13, -14}, {-13, -15}, {-13, -16}, {-14, -15}, {-14, -16}, {-15, -16},
        
        // At least one queen in each column
        {1, 5, 9, 13},    // Column 1: Queen in row 1, 2, 3, or 4
        {2, 6, 10, 14},   // Column 2: Queen in row 1, 2, 3, or 4
        {3, 7, 11, 15},   // Column 3: Queen in row 1, 2, 3, or 4
        {4, 8, 12, 16},   // Column 4: Queen in row 1, 2, 3, or 4
        
        // At most one queen in each column (no two queens in same column)
        // Column 1
        {-1, -5}, {-1, -9}, {-1, -13}, {-5, -9}, {-5, -13}, {-9, -13},
        // Column 2
        {-2, -6}, {-2, -10}, {-2, -14}, {-6, -10}, {-6, -14}, {-10, -14},
        // Column 3
        {-3, -7}, {-3, -11}, {-3, -15}, {-7, -11}, {-7, -15}, {-11, -15},
        // Column 4
        {-4, -8}, {-4, -12}, {-4, -16}, {-8, -12}, {-8, -16}, {-12, -16},
        
        // No queens on same diagonal (top-left to bottom-right)
        {-1, -6}, {-1, -11}, {-1, -16}, // Diagonals starting from row 1
        {-2, -7}, {-2, -12},            // Diagonals starting from row 1
        {-3, -8},                       // Diagonal starting from row 1
        {-5, -10}, {-5, -15},           // Diagonals starting from row 2
        {-6, -11}, {-6, -16},           // Diagonals starting from row 2
        {-7, -12},                      // Diagonal starting from row 2
        {-9, -14},                      // Diagonal starting from row 3
        
        // No queens on same diagonal (top-right to bottom-left)
        {-4, -7}, {-4, -10}, {-4, -13}, // Diagonals starting from row 1
        {-3, -6}, {-3, -9},             // Diagonals starting from row 1
        {-2, -5},                       // Diagonal starting from row 1
        {-8, -11}, {-8, -14},           // Diagonals starting from row 2
        {-7, -10}, {-7, -13},           // Diagonals starting from row 2
        {-6, -9},                       // Diagonal starting from row 2
        {-12, -15}                      // Diagonal starting from row 3
    };
    SATInstance instance2(satCNF);
    instance2.print();

    start = std::chrono::high_resolution_clock::now();
    bool result2 = DPLL(instance2);
    end = std::chrono::high_resolution_clock::now();

    std::cout << (result2 ? "SATISFIABLE" : "UNSATISFIABLE") << std::endl;
    elapsed = end - start;
    std::cout << "Execution Time: " << elapsed.count() << " ms\n";
    std::cout << "Recursive Calls: " << dpll_calls << "\n";
    std::cout << "Backtracks: " << backtracks << "\n";

    // Reset performance counters
    dpll_calls = 0;
    backtracks = 0;

    // Test 3: Pigeonhole Principle (Unsatisfiable)
    std::cout << "\nTesting Pigeonhole Principle (Unsatisfiable):\n";
    CNF unsatCNF = {
        // Each pigeon must be in at least one hole
        {1, 2, 3, 4},     // Pigeon 1 in hole 1, 2, 3, or 4
        {5, 6, 7, 8},     // Pigeon 2 in hole 1, 2, 3, or 4
        {9, 10, 11, 12},  // Pigeon 3 in hole 1, 2, 3, or 4
        {13, 14, 15, 16}, // Pigeon 4 in hole 1, 2, 3, or 4
        {17, 18, 19, 20}, // Pigeon 5 in hole 1, 2, 3, or 4
        
        // Each pigeon can be in at most one hole
        // Pigeon 1
        {-1, -2}, {-1, -3}, {-1, -4}, {-2, -3}, {-2, -4}, {-3, -4},
        // Pigeon 2
        {-5, -6}, {-5, -7}, {-5, -8}, {-6, -7}, {-6, -8}, {-7, -8},
        // Pigeon 3
        {-9, -10}, {-9, -11}, {-9, -12}, {-10, -11}, {-10, -12}, {-11, -12},
        // Pigeon 4
        {-13, -14}, {-13, -15}, {-13, -16}, {-14, -15}, {-14, -16}, {-15, -16},
        // Pigeon 5
        {-17, -18}, {-17, -19}, {-17, -20}, {-18, -19}, {-18, -20}, {-19, -20},
        
        // No two pigeons in the same hole
        // Hole 1 (variables 1, 5, 9, 13, 17)
        {-1, -5}, {-1, -9}, {-1, -13}, {-1, -17},
        {-5, -9}, {-5, -13}, {-5, -17},
        {-9, -13}, {-9, -17},
        {-13, -17},
        
        // Hole 2 (variables 2, 6, 10, 14, 18)
        {-2, -6}, {-2, -10}, {-2, -14}, {-2, -18},
        {-6, -10}, {-6, -14}, {-6, -18},
        {-10, -14}, {-10, -18},
        {-14, -18},
        
        // Hole 3 (variables 3, 7, 11, 15, 19)
        {-3, -7}, {-3, -11}, {-3, -15}, {-3, -19},
        {-7, -11}, {-7, -15}, {-7, -19},
        {-11, -15}, {-11, -19},
        {-15, -19},
        
        // Hole 4 (variables 4, 8, 12, 16, 20)
        {-4, -8}, {-4, -12}, {-4, -16}, {-4, -20},
        {-8, -12}, {-8, -16}, {-8, -20},
        {-12, -16}, {-12, -20},
        {-16, -20}
    };
    SATInstance instance3(unsatCNF);
    instance3.print();

    start = std::chrono::high_resolution_clock::now();
    bool result3 = DPLL(instance3);
    end = std::chrono::high_resolution_clock::now();

    std::cout << (result3 ? "SATISFIABLE" : "UNSATISFIABLE") << std::endl;
    elapsed = end - start;
    std::cout << "Execution Time: " << elapsed.count() << " ms\n";
    std::cout << "Recursive Calls: " << dpll_calls << "\n";
    std::cout << "Backtracks: " << backtracks << "\n";

    return 0;
}