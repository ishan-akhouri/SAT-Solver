#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>
#include <chrono>
#include <iostream>
#include "../include/SATInstance.h"

// Problem type identification
enum class ProblemType
{
    GENERIC = 0,        // General SAT problem (no specific structure)
    NQUEENS = 1,        // N-Queens problem
    PIGEONHOLE = 2,     // Pigeonhole problem
    GRAPH_COLORING = 3, // Graph Coloring
    HAMILTONIAN = 4     // Hamiltonian path/cycle
};

// Preprocessing phases
enum class PreprocessingPhase
{
    INITIAL,             // Basic simplifications
    STRUCTURAL_PRESERVE, // Structure-preserving transformations
    AGGRESSIVE,          // Aggressive simplifications
    FINAL                // Final touch-ups
};

// Clause metadata for structural information
struct ClauseMeta
{
    Clause clause;            // The actual clause
    bool marked_for_deletion; // Deletion flag
    int activity;             // Activity/importance score
    int origin_phase;         // Which phase created this clause
    bool is_learned;          // Whether it's an original or learned clause
    bool is_structural;       // Whether it's part of problem structure
    bool is_assumption;       // Whether it's an assumption

    // Constructor
    ClauseMeta(const Clause &c, PreprocessingPhase phase,
               bool structural = false, bool assumption = false)
        : clause(c),
          marked_for_deletion(false),
          activity(0),
          origin_phase(static_cast<int>(phase)),
          is_learned(false),
          is_structural(structural),
          is_assumption(assumption) {}
};

// Preprocessor configuration options
struct PreprocessorConfig
{
    // Basic technique enablement
    bool use_unit_propagation = true;
    bool use_pure_literal_elimination = true;
    bool use_subsumption = true;
    bool use_self_subsumption = true;

    // Phase control
    bool enable_initial_phase = true;
    bool enable_final_phase = true;

    // Aggressive techniques
    bool enable_aggressive_phase = false;
    bool enable_structural_phase = false;
    bool use_failed_literal = false;
    bool use_variable_elimination = false;
    bool use_blocked_clause = false;

    // Problem-specific settings
    std::map<ProblemType, std::map<std::string, bool>> technique_enablement;

    // Constructor
    PreprocessorConfig() {}

    // Adapt configuration based on problem type
    void adaptToType(ProblemType type);
};

// Statistics for preprocessing
struct PreprocessingStats
{
    // Before/after metrics
    int original_variables = 0;
    int original_clauses = 0;
    int simplified_variables = 0;
    int simplified_clauses = 0;

    // Timing metrics (in microseconds)
    std::chrono::microseconds total_time{0};
    std::map<std::string, std::chrono::microseconds> technique_times;
    std::map<PreprocessingPhase, std::chrono::microseconds> phase_times;

    // Effectiveness metrics
    double variables_reduction_percent = 0.0;
    double clauses_reduction_percent = 0.0;

    // Variable/clause operations
    int variables_eliminated = 0;
    int variables_fixed = 0;
    int clauses_removed = 0;
    int clauses_added = 0;

    // Methods to update and calculate statistics
    void updateTechniqueTiming(const std::string &technique,
                               std::chrono::microseconds elapsed);
    void updatePhaseTiming(PreprocessingPhase phase,
                           std::chrono::microseconds elapsed);
    void calculateReductions();
};

class Preprocessor
{
private:
    // Problem identification
    ProblemType problem_type;

    // Configuration
    PreprocessorConfig config;

    // Statistics tracking
    PreprocessingStats stats;

    // Phase management
    std::map<PreprocessingPhase, CNF> phase_snapshots;

    // Variable mapping for solution reconstruction
    std::unordered_map<int, int> variable_map;
    std::unordered_map<int, bool> fixed_variables;

    // Clause meta integration
    std::vector<ClauseMeta> clauses_meta;
    std::vector<int> assumption_literals;

    // Phase management
    void executePhase(PreprocessingPhase phase, CNF &formula);
    void snapshotFormula(PreprocessingPhase phase, const CNF &formula);

    // Technique selection
    bool shouldApplyTechnique(const std::string &technique);

    // Utility functions
    int countVariables(const CNF &formula);

    // Clause meta integration methods
    void initializeClauseMeta(const CNF &formula);
    bool isStructuralClause(const Clause &clause);
    void updateClauseMeta(CNF &formula);

    // Helper function for perfect square detection
    bool isPerfectSquare(int n);

    // Helper function to test a literal assignment
    int testLiteralAssignment(const CNF &formula, int var, bool value, std::unordered_map<int, bool> &implications);

public:
    // Constructor with optional configuration
    Preprocessor(const PreprocessorConfig &config = PreprocessorConfig());

    // Main preprocessing entry point
    CNF preprocess(const CNF &formula);

    // Solution mapping
    std::unordered_map<int, bool> mapSolutionToOriginal(
        const std::unordered_map<int, bool> &solution);

    // Statistics and reporting
    PreprocessingStats getStats() const;
    void printStats() const;

    // Individual preprocessing techniques
    CNF unitPropagation(CNF &formula);
    CNF pureLiteralElimination(CNF &formula);
    CNF performBasicSubsumption(CNF &formula);
    CNF finalUnitPropagation(CNF &formula);
    CNF performSelfSubsumption(CNF &formula);

    // Aggressive preprocessing techniques
    CNF detectFailedLiterals(CNF &formula);
    CNF eliminateVariables(CNF &formula);
    CNF eliminateBlockedClauses(CNF &formula);

    // Helper methods for problem detection
    ProblemType detectProblemType(const CNF &formula);

    // Clause meta
    void setAssumptions(const std::vector<int> &assumptions);

    // Problem detection validation
    bool validateHamiltonianStructure(const CNF &formula, int n);
    bool validateGraphColoringStructure(const CNF &formula, int v, int c);
    bool validateNQueensStructure(const CNF &formula, int n);
    bool validatePigeonholeStructure(const CNF &formula, int m, int n);

    // Set problem type
    void setProblemType(ProblemType type);
};

#endif // PREPROCESSOR_H