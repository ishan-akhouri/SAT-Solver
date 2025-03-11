#ifndef SAT_INSTANCE_H
#define SAT_INSTANCE_H

#include <vector>
#include <unordered_map>
#include <iostream>
#include <cmath>
#include <algorithm>

using Clause = std::vector<int>;  // A clause is a disjunction (OR) of literals
using CNF = std::vector<Clause>;  // A CNF formula is a conjunction (AND) of clauses

struct SATInstance {
    CNF formula;                             // The formula in CNF form
    std::unordered_map<int, bool> assignments; // Variable assignments (var → value)
    
    // VSIDS related members
    std::unordered_map<int, double> activity;  // Store activity score for each variable
    double var_inc;        // Value to increase activity by
    double var_decay;      // Decay factor for activities
    int decisions;         // Counter used to trigger decay
    static constexpr int decay_interval = 100; // Number of decisions before applying decay
    
    // Debug control
    bool debug_output;     // Whether to print debug messages
    
    SATInstance(const CNF &f, bool debug = false) 
        : formula(f), var_inc(1.0), var_decay(0.95), decisions(0), debug_output(debug) {
        // Initialize activity scores for all variables in the formula
        for (const auto& clause : f) {
            for (int literal : clause) {
                activity[abs(literal)] = 0.0;  // Initialize with zero activity
            }
        }
    }
    
    // Increase the activity score of a variable
    void bumpVarActivity(int var) {
        activity[var] += var_inc;
        
        // Handle potential numerical overflow
        if (activity[var] > 1e100) {
            // Rescale all activity values
            for (auto& [v, act] : activity) {
                act *= 1e-100;
            }
            var_inc *= 1e-100;
        }
    }
    
    // Apply decay to all variable activities
    void decayVarActivities() {
        decisions++;
        
        // Only apply decay at certain intervals to avoid excessive computation
        if (decisions % decay_interval == 0) {
            var_inc /= var_decay;
            // No need to decay all variables here - we adjust the increment instead
        }
    }
    
    // Select the variable with highest VSIDS score
    int selectVarVSIDS() {
        int best_var = 0;
        double best_score = -1;
        
        for (const auto& [var, score] : activity) {
            // Skip already assigned variables
            if (assignments.find(var) != assignments.end()) {
                continue;
            }
            
            if (score > best_score) {
                best_score = score;
                best_var = var;
            }
        }
        
        if (debug_output && best_var != 0) {
            std::cout << "VSIDS selected var " << best_var << " with score " << best_score << "\n";
        }
        
        return best_var;
    }
    
    // Update activities for all literals in a conflict clause
    void updateActivitiesFromConflict(const Clause& conflict_clause) {
        for (int literal : conflict_clause) {
            bumpVarActivity(abs(literal));
        }
        
        if (debug_output) {
            std::cout << "Updated activities for conflict variables\n";
        }
    }
    
    // Initialize VSIDS by counting literal occurrences in the initial formula
    void initializeVSIDS() {
        // Count occurrences of each literal in the initial formula
        for (const auto& clause : formula) {
            for (int literal : clause) {
                int var = abs(literal);
                activity[var] += 1.0;
            }
        }
        
        if (debug_output) {
            std::cout << "Initialized VSIDS activities:\n";
            for (const auto& [var, score] : activity) {
                std::cout << "Var " << var << ": " << score << "\n";
            }
        }
    }
    
    void print() {
        if (!debug_output) return;
        
        std::cout << "SAT Problem in CNF:\n";
        for (const auto& clause : formula) {
            for (int literal : clause) {
                std::cout << (literal > 0 ? "x" : "~x") << abs(literal) << " ";
            }
            std::cout << "\n";
        }
    }
};

#endif