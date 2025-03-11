#include <iostream>
#include "../include/SATInstance.h"
#include "../include/DPLL.h"
#include <chrono>
#include <iomanip> // For std::setw

int main(int argc, char* argv[]) {
    bool debug_mode = false;
    
    // Process command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--debug" || arg == "-d") {
            debug_mode = true;
        }
    }
    
    std::cout << "SAT Solver (VSIDS) Benchmarks" << std::endl;
    std::cout << (debug_mode ? "Debug mode: ON\n" : "Debug mode: OFF\n");
    
    // Define a helper function for running benchmarks
    auto runBenchmark = [&](const std::string& name, const CNF& cnf) {
        std::cout << "\n----------------------------------------\n";
        std::cout << "Testing " << name << ":\n";
        
        // Reset performance counters
        dpll_calls = 0;
        backtracks = 0;
        
        // Create instance
        SATInstance instance(cnf, debug_mode);
        if (debug_mode) instance.print();
        instance.initializeVSIDS();
        
        // Timing
        auto start = std::chrono::high_resolution_clock::now();
        bool result = DPLL(instance);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        
        // Print results in a formatted table
        std::cout << "Result:         " << (result ? "SATISFIABLE" : "UNSATISFIABLE") << std::endl;
        std::cout << "Execution Time: " << std::fixed << std::setprecision(3) << elapsed.count() << " ms" << std::endl;
        std::cout << "Recursive Calls: " << dpll_calls << std::endl;
        std::cout << "Backtracks:     " << backtracks << std::endl;
        
        // Print variable assignments if debug mode and satisfiable
        if (debug_mode && result) {
            std::cout << "\nVariable Assignments:\n";
            for (const auto& [var, val] : instance.assignments) {
                std::cout << "x" << var << " = " << val << "\n";
            }
        }
        
        return elapsed.count();
    };
    
    // Test 1: Simple Satisfiable Formula
    CNF exampleCNF = {{1, 2}, {-1, 3}, {-2, -3}};
    runBenchmark("Simple Satisfiable CNF", exampleCNF);
    
    // Test 2: 4-Queens Problem
    CNF queensCNF = {
        // At least one queen in each row
        {1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16},
        
        // At most one queen in each row
        {-1, -2}, {-1, -3}, {-1, -4}, {-2, -3}, {-2, -4}, {-3, -4},
        {-5, -6}, {-5, -7}, {-5, -8}, {-6, -7}, {-6, -8}, {-7, -8},
        {-9, -10}, {-9, -11}, {-9, -12}, {-10, -11}, {-10, -12}, {-11, -12},
        {-13, -14}, {-13, -15}, {-13, -16}, {-14, -15}, {-14, -16}, {-15, -16},
        
        // At least one queen in each column
        {1, 5, 9, 13}, {2, 6, 10, 14}, {3, 7, 11, 15}, {4, 8, 12, 16},
        
        // At most one queen in each column
        {-1, -5}, {-1, -9}, {-1, -13}, {-5, -9}, {-5, -13}, {-9, -13},
        {-2, -6}, {-2, -10}, {-2, -14}, {-6, -10}, {-6, -14}, {-10, -14},
        {-3, -7}, {-3, -11}, {-3, -15}, {-7, -11}, {-7, -15}, {-11, -15},
        {-4, -8}, {-4, -12}, {-4, -16}, {-8, -12}, {-8, -16}, {-12, -16},
        
        // No queens on same diagonal (top-left to bottom-right)
        {-1, -6}, {-1, -11}, {-1, -16}, {-2, -7}, {-2, -12}, {-3, -8},
        {-5, -10}, {-5, -15}, {-6, -11}, {-6, -16}, {-7, -12}, {-9, -14},
        
        // No queens on same diagonal (top-right to bottom-left)
        {-4, -7}, {-4, -10}, {-4, -13}, {-3, -6}, {-3, -9}, {-2, -5},
        {-8, -11}, {-8, -14}, {-7, -10}, {-7, -13}, {-6, -9}, {-12, -15}
    };
    double queensTime = runBenchmark("4-Queens Problem (Satisfiable)", queensCNF);
    
    // Test 3: Pigeonhole Principle
    CNF pigeonholeCNF = {
        // Each pigeon must be in at least one hole
        {1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}, {13, 14, 15, 16}, {17, 18, 19, 20},
        
        // Each pigeon can be in at most one hole
        {-1, -2}, {-1, -3}, {-1, -4}, {-2, -3}, {-2, -4}, {-3, -4},
        {-5, -6}, {-5, -7}, {-5, -8}, {-6, -7}, {-6, -8}, {-7, -8},
        {-9, -10}, {-9, -11}, {-9, -12}, {-10, -11}, {-10, -12}, {-11, -12},
        {-13, -14}, {-13, -15}, {-13, -16}, {-14, -15}, {-14, -16}, {-15, -16},
        {-17, -18}, {-17, -19}, {-17, -20}, {-18, -19}, {-18, -20}, {-19, -20},
        
        // No two pigeons in the same hole
        {-1, -5}, {-1, -9}, {-1, -13}, {-1, -17},
        {-5, -9}, {-5, -13}, {-5, -17},
        {-9, -13}, {-9, -17},
        {-13, -17},
        
        {-2, -6}, {-2, -10}, {-2, -14}, {-2, -18},
        {-6, -10}, {-6, -14}, {-6, -18},
        {-10, -14}, {-10, -18},
        {-14, -18},
        
        {-3, -7}, {-3, -11}, {-3, -15}, {-3, -19},
        {-7, -11}, {-7, -15}, {-7, -19},
        {-11, -15}, {-11, -19},
        {-15, -19},
        
        {-4, -8}, {-4, -12}, {-4, -16}, {-4, -20},
        {-8, -12}, {-8, -16}, {-8, -20},
        {-12, -16}, {-12, -20},
        {-16, -20}
    };
    double pigeonholeTime = runBenchmark("Pigeonhole Principle (Unsatisfiable)", pigeonholeCNF);
    
    // Generate 8-Queens problem for a more challenging benchmark
    if (!debug_mode) {  // Only run this larger benchmark in non-debug mode
        std::cout << "\n----------------------------------------\n";
        std::cout << "Generating 8-Queens Problem...\n";
        
        CNF queens8CNF;
        const int N = 8; // 8×8 board
        const int base = 1; // Starting variable index
        
        // At least one queen in each row
        for (int row = 0; row < N; row++) {
            Clause atLeastOneInRow;
            for (int col = 0; col < N; col++) {
                int var = base + row*N + col;
                atLeastOneInRow.push_back(var);
            }
            queens8CNF.push_back(atLeastOneInRow);
        }
        
        // At most one queen in each row
        for (int row = 0; row < N; row++) {
            for (int col1 = 0; col1 < N; col1++) {
                for (int col2 = col1+1; col2 < N; col2++) {
                    int var1 = base + row*N + col1;
                    int var2 = base + row*N + col2;
                    queens8CNF.push_back({-var1, -var2});
                }
            }
        }
        
        // At least one queen in each column
        for (int col = 0; col < N; col++) {
            Clause atLeastOneInCol;
            for (int row = 0; row < N; row++) {
                int var = base + row*N + col;
                atLeastOneInCol.push_back(var);
            }
            queens8CNF.push_back(atLeastOneInCol);
        }
        
        // At most one queen in each column
        for (int col = 0; col < N; col++) {
            for (int row1 = 0; row1 < N; row1++) {
                for (int row2 = row1+1; row2 < N; row2++) {
                    int var1 = base + row1*N + col;
                    int var2 = base + row2*N + col;
                    queens8CNF.push_back({-var1, -var2});
                }
            }
        }
        
        // No two queens on the same diagonal
        // Top-left to bottom-right diagonals
        for (int d = 0; d < 2*N-1; d++) {
            for (int i = 0; i < N; i++) {
                int row = i;
                int col = d - i;
                if (col >= 0 && col < N) {
                    for (int j = i+1; j < N; j++) {
                        int row2 = j;
                        int col2 = d - j;
                        if (col2 >= 0 && col2 < N) {
                            int var1 = base + row*N + col;
                            int var2 = base + row2*N + col2;
                            queens8CNF.push_back({-var1, -var2});
                        }
                    }
                }
            }
        }
        
        // Top-right to bottom-left diagonals
        for (int d = 0; d < 2*N-1; d++) {
            for (int i = 0; i < N; i++) {
                int row = i;
                int col = i - d + N - 1;
                if (col >= 0 && col < N) {
                    for (int j = i+1; j < N; j++) {
                        int row2 = j;
                        int col2 = j - d + N - 1;
                        if (col2 >= 0 && col2 < N) {
                            int var1 = base + row*N + col;
                            int var2 = base + row2*N + col2;
                            queens8CNF.push_back({-var1, -var2});
                        }
                    }
                }
            }
        }
        
        // Run 8-Queens benchmark
        double queens8Time = runBenchmark("8-Queens Problem (Satisfiable)", queens8CNF);
        
        // Print summary comparison
        std::cout << "\n----------------------------------------\n";
        std::cout << "Performance Summary:\n";
        std::cout << "4-Queens Time:  " << std::fixed << std::setprecision(3) << queensTime << " ms\n";
        std::cout << "8-Queens Time:  " << std::fixed << std::setprecision(3) << queens8Time << " ms\n";
        std::cout << "Time Ratio (8-Queens/4-Queens): " << std::fixed << std::setprecision(2) 
                  << queens8Time / queensTime << "x\n";
        std::cout << "Pigeonhole Time: " << std::fixed << std::setprecision(3) << pigeonholeTime << " ms\n";
    }
    
    return 0;
}