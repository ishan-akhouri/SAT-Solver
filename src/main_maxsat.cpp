#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <random>
#include <fstream>
#include "MaxSATSolver.h"
#include "WeightedMaxSATSolver.h"

// Helper function to generate random MaxSAT instances
void generateRandomMaxSAT(int num_vars, int num_hard, int num_soft, 
                         CNF& hard_clauses, CNF& soft_clauses, int seed = 42) {
    // Initialize random seed
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> var_dist(1, num_vars);
    std::uniform_int_distribution<> sign_dist(0, 1);
    
    // Generate random hard clauses (3-SAT)
    for (int i = 0; i < num_hard; i++) {
        Clause clause;
        
        // Generate 3 distinct literals for the clause
        while (clause.size() < 3) {
            int var = var_dist(gen);
            int lit = sign_dist(gen) == 0 ? var : -var;
            
            // Ensure no duplicate literals in the clause
            if (std::find(clause.begin(), clause.end(), lit) == clause.end() &&
                std::find(clause.begin(), clause.end(), -lit) == clause.end()) {
                clause.push_back(lit);
            }
        }
        
        hard_clauses.push_back(clause);
    }
    
    // Generate random soft clauses (unit clauses)
    for (int i = 0; i < num_soft; i++) {
        int var = var_dist(gen);
        int lit = sign_dist(gen) == 0 ? var : -var;
        soft_clauses.push_back({lit});
    }
}

// Generate random weights for soft clauses
std::vector<int> generateRandomWeights(int num_clauses, int min_weight = 1, int max_weight = 10, int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> weight_dist(min_weight, max_weight);
    
    std::vector<int> weights;
    for (int i = 0; i < num_clauses; i++) {
        weights.push_back(weight_dist(gen));
    }
    
    return weights;
}

void testSmallExample() {
    std::cout << "===== Small MaxSAT Example =====" << std::endl;
    
    // Create a simple MaxSAT problem
    // Hard clauses: (x1 OR x2) AND (NOT x1 OR x3)
    CNF hard_clauses = {{1, 2}, {-1, 3}};
    
    // Soft clauses: (NOT x2) AND (NOT x3)
    CNF soft_clauses = {{-2}, {-3}};
    
    // Create MaxSAT solver
    MaxSATSolver solver(hard_clauses, true);  // Enable debug output
    
    // Add soft clauses
    solver.addSoftClauses(soft_clauses);
    
    std::cout << "Solving with linear search:" << std::endl;
    int linear_result = solver.solve();
    
    std::cout << "Result: " << linear_result << " violated soft clauses" << std::endl;
    
    // Print the satisfying assignment
    std::cout << "Satisfying assignment:" << std::endl;
    const auto& assignment = solver.getAssignment();
    for (const auto& [var, value] : assignment) {
        std::cout << "x" << var << " = " << (value ? "true" : "false") << std::endl;
    }
    
    std::cout << "\nSolving with binary search:" << std::endl;
    
    // Create a new solver for binary search
    MaxSATSolver binary_solver(hard_clauses, true);
    binary_solver.addSoftClauses(soft_clauses);
    
    int binary_result = binary_solver.solveBinarySearch();
    
    std::cout << "Result: " << binary_result << " violated soft clauses" << std::endl;
    std::cout << std::endl;
}

void testSmallWeightedExample() {
    std::cout << "===== Small Weighted MaxSAT Example =====" << std::endl;
    
    // Create a simple MaxSAT problem
    // Hard clauses: (x1 OR x2) AND (NOT x1 OR x3)
    CNF hard_clauses = {{1, 2}, {-1, 3}};
    
    // Soft clauses: (NOT x2) with weight 3 AND (NOT x3) with weight 1
    CNF soft_clauses = {{-2}, {-3}};
    std::vector<int> weights = {3, 1};
    
    // Create Weighted MaxSAT solver
    WeightedMaxSATSolver solver(hard_clauses, true);  // Enable debug output
    
    // Add soft clauses with weights
    for (size_t i = 0; i < soft_clauses.size(); i++) {
        solver.addSoftClause(soft_clauses[i], weights[i]);
    }
    
    std::cout << "Solving with stratified approach:" << std::endl;
    int stratified_result = solver.solveStratified();
    
    std::cout << "Result: " << stratified_result << " total weight of violated soft clauses" << std::endl;
    
    std::cout << "\nSolving with binary search:" << std::endl;
    
    // Create a new solver for binary search
    WeightedMaxSATSolver binary_solver(hard_clauses, true);
    for (size_t i = 0; i < soft_clauses.size(); i++) {
        binary_solver.addSoftClause(soft_clauses[i], weights[i]);
    }
    
    int binary_result = binary_solver.solveBinarySearch();
    
    std::cout << "Result: " << binary_result << " total weight of violated soft clauses" << std::endl;
    std::cout << std::endl;
}

void benchmarkMaxSAT() {
    std::cout << "===== MaxSAT Benchmarks =====" << std::endl;
    
    // Configuration
    const int num_instances = 5;
    const int num_vars = 50;
    const int num_hard = 100;
    const int num_soft = 50;
    
    // Results tracking
    double total_linear_time = 0.0;
    double total_binary_time = 0.0;
    int total_linear_calls = 0;
    int total_binary_calls = 0;
    
    for (int i = 0; i < num_instances; i++) {
        std::cout << "Instance " << (i+1) << ":" << std::endl;
        
        // Generate random instance
        CNF hard_clauses, soft_clauses;
        generateRandomMaxSAT(num_vars, num_hard, num_soft, hard_clauses, soft_clauses, i);
        
        // Linear search
        MaxSATSolver linear_solver(hard_clauses);
        linear_solver.addSoftClauses(soft_clauses);
        
        auto linear_start = std::chrono::high_resolution_clock::now();
        int linear_result = linear_solver.solve();
        auto linear_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> linear_elapsed = linear_end - linear_start;
        
        // Binary search
        MaxSATSolver binary_solver(hard_clauses);
        binary_solver.addSoftClauses(soft_clauses);
        
        auto binary_start = std::chrono::high_resolution_clock::now();
        int binary_result = binary_solver.solveBinarySearch();
        auto binary_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> binary_elapsed = binary_end - binary_start;
        
        // Print results
        std::cout << "  Linear search: " << linear_result << " violated clauses, " 
                  << std::fixed << std::setprecision(2) << linear_elapsed.count() 
                  << "ms, " << linear_solver.getNumSolverCalls() << " solver calls" << std::endl;
                  
        std::cout << "  Binary search: " << binary_result << " violated clauses, " 
                  << std::fixed << std::setprecision(2) << binary_elapsed.count() 
                  << "ms, " << binary_solver.getNumSolverCalls() << " solver calls" << std::endl;
        
        // Verify results match
        if (linear_result != binary_result) {
            std::cout << "  WARNING: Results don't match!" << std::endl;
        }
        
        total_linear_time += linear_elapsed.count();
        total_binary_time += binary_elapsed.count();
        total_linear_calls += linear_solver.getNumSolverCalls();
        total_binary_calls += binary_solver.getNumSolverCalls();
    }
    
    // Print summary
    std::cout << "\nSummary:" << std::endl;
    std::cout << "  Linear search: avg " << (total_linear_time / num_instances) 
              << "ms, avg " << (total_linear_calls / num_instances) << " solver calls" << std::endl;
    std::cout << "  Binary search: avg " << (total_binary_time / num_instances) 
              << "ms, avg " << (total_binary_calls / num_instances) << " solver calls" << std::endl;
    
    double speedup = total_linear_time / total_binary_time;
    std::cout << "  Binary search speedup: " << std::fixed << std::setprecision(2) 
              << speedup << "x" << std::endl;
}

void benchmarkWeightedMaxSAT() {
    std::cout << "===== Weighted MaxSAT Benchmarks =====" << std::endl;
    
    // Configuration
    const int num_instances = 5;
    const int num_vars = 50;
    const int num_hard = 100;
    const int num_soft = 50;
    
    // Results tracking
    double total_stratified_time = 0.0;
    double total_binary_time = 0.0;
    int total_stratified_calls = 0;
    int total_binary_calls = 0;
    
    for (int i = 0; i < num_instances; i++) {
        std::cout << "Instance " << (i+1) << ":" << std::endl;
        
        // Generate random instance
        CNF hard_clauses, soft_clauses;
        generateRandomMaxSAT(num_vars, num_hard, num_soft, hard_clauses, soft_clauses, i);
        
        // Generate random weights
        std::vector<int> weights = generateRandomWeights(num_soft, 1, 10, i);
        
        // Stratified approach
        WeightedMaxSATSolver stratified_solver(hard_clauses);
        for (size_t j = 0; j < soft_clauses.size(); j++) {
            stratified_solver.addSoftClause(soft_clauses[j], weights[j]);
        }
        
        auto stratified_start = std::chrono::high_resolution_clock::now();
        int stratified_result = stratified_solver.solveStratified();
        auto stratified_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> stratified_elapsed = stratified_end - stratified_start;
        
        // Binary search
        WeightedMaxSATSolver binary_solver(hard_clauses);
        for (size_t j = 0; j < soft_clauses.size(); j++) {
            binary_solver.addSoftClause(soft_clauses[j], weights[j]);
        }
        
        auto binary_start = std::chrono::high_resolution_clock::now();
        int binary_result = binary_solver.solveBinarySearch();
        auto binary_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> binary_elapsed = binary_end - binary_start;
        
        // Print results
        std::cout << "  Stratified: " << stratified_result << " total weight violated, " 
                  << std::fixed << std::setprecision(2) << stratified_elapsed.count() 
                  << "ms, " << stratified_solver.getNumSolverCalls() << " solver calls" << std::endl;
                  
        std::cout << "  Binary search: " << binary_result << " total weight violated, " 
                  << std::fixed << std::setprecision(2) << binary_elapsed.count() 
                  << "ms, " << binary_solver.getNumSolverCalls() << " solver calls" << std::endl;
        
        // Verify results match (or binary is better)
        if (stratified_result != -1 && binary_result != -1 && stratified_result < binary_result) {
            std::cout << "  WARNING: Binary search found worse solution!" << std::endl;
        }
        
        total_stratified_time += stratified_elapsed.count();
        total_binary_time += binary_elapsed.count();
        total_stratified_calls += stratified_solver.getNumSolverCalls();
        total_binary_calls += binary_solver.getNumSolverCalls();
    }
    
    // Print summary
    std::cout << "\nSummary:" << std::endl;
    std::cout << "  Stratified: avg " << (total_stratified_time / num_instances) 
              << "ms, avg " << (total_stratified_calls / num_instances) << " solver calls" << std::endl;
    std::cout << "  Binary search: avg " << (total_binary_time / num_instances) 
              << "ms, avg " << (total_binary_calls / num_instances) << " solver calls" << std::endl;
    
    double speedup = total_stratified_time / total_binary_time;
    std::cout << "  Binary search speedup: " << std::fixed << std::setprecision(2) 
              << speedup << "x" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "MaxSAT Solver based on Incremental SAT" << std::endl;
    std::cout << "===============================================" << std::endl << std::endl;
    
    // Run tests for unweighted MaxSAT
    testSmallExample();
    std::cout << std::endl;
    
    benchmarkMaxSAT();
    std::cout << std::endl;
    
    // Run tests for weighted MaxSAT
    testSmallWeightedExample();
    std::cout << std::endl;
    
    benchmarkWeightedMaxSAT();
    
    return 0;
}