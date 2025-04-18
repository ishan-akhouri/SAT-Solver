#ifndef PORTFOLIO_MANAGER_H
#define PORTFOLIO_MANAGER_H

#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <unordered_map>
#include <memory>
#include "CDCLSolverIncremental.h"

// Portfolio-based parallel SAT solver optimized for Random 3SAT problems
// Runs multiple diversely configured CDCLSolverIncremental instances in parallel,
// with each solver using different parameters targeting different characeteristics
// of Random 3SAT problems.
class PortfolioManager
{
private:
    // Problem instance
    CNF formula;

    // Thread and termination control
    std::vector<std::thread> solver_threads;
    std::atomic<bool> solution_found;
    std::atomic<bool> global_timeout;
    std::condition_variable termination_cv;
    std::mutex termination_mutex;

    // Resource management
    const size_t MAX_MEMORY_PER_SOLVER = 1024 * 1024 * 1024; // 1GB per solver
    std::atomic<size_t> total_memory_used;
    std::mutex resource_mutex;
    std::condition_variable resource_cv;
    int max_concurrent_solvers;
    std::atomic<int> active_solvers;
    std::atomic<bool> initialization_complete; // Track initialization status

    // Timing control
    std::chrono::milliseconds global_timeout_duration;
    std::chrono::high_resolution_clock::time_point portfolio_start_time;

    // Result storage
    std::unordered_map<int, bool> best_solution;
    int winning_solver_id;
    std::mutex result_mutex;

    // Statistics
    struct SolverStats
    {
        int conflicts;
        int decisions;
        int propagations;
        int restarts;
        int max_decision_level;
        int learned_clauses;
        std::chrono::microseconds solve_time;
        size_t peak_memory_usage;
        int termination_reason; // 0=solution, 1=timeout, 2=resource_limit, 3=external_stop
    };
    std::vector<SolverStats> solver_statistics;

    // Configuration presets optimized for Random 3SAT
    struct SolverConfig
    {
        double var_decay;
        bool use_luby_restarts;
        int restart_threshold;
        double random_polarity_freq;
        bool use_lbd;
        bool use_phase_saving;
        size_t max_learnt_clauses;
    };
    std::vector<SolverConfig> solver_configs;

    // Monitor thread
    std::thread monitor_thread;

public:
    // Constructs a new PortfolioManager object
    PortfolioManager(const CNF &cnf,
                     std::chrono::milliseconds timeout = std::chrono::minutes(30),
                     int num_threads = std::thread::hardware_concurrency());

    // Ensures proper termination of all threads
    ~PortfolioManager();

    // Solves the formula using the portfolio approach
    bool solve(const CNF &formula);

    // Get satisfying assignment if formula was satisfiable
    const std::unordered_map<int, bool> &getSolution() const;

    // Set the global timeout for the entire portfolio
    void setGlobalTimeout(std::chrono::milliseconds timeout);

    // Set maximum memory usage for the portfolio
    void setMaxMemoryUsage(size_t max_memory_mb);

    // Print detailed statistics about the solver performance
    void printStatistics() const;

    // Get best solution found
    std::vector<bool> getBestSolution() const;

    // Get winning solver ID
    int getWinningSolverId() const;

    // Get solver statistics
    const std::vector<SolverStats> &getSolverStatistics() const { return solver_statistics; }

    bool isSolutionFound() const { return solution_found; }

private:
    // Initialize diverse solver configurations
    void initializeConfigs();

    // Main solver thread function
    void solverThread(int solver_id, const CNF &formula);

    // Thread that monitors timeout and resource usage
    void monitorThread();

    // Apply configuration to a solver instance
    void configureSolver(CDCLSolverIncremental &solver, int config_id);

    // Record statistics from a solver run
    void recordStatistics(int solver_id, const CDCLSolverIncremental &solver,
                          std::chrono::microseconds solve_time);

    // Check if resources are available to start a new solver
    bool checkResourceAvailability();

    // Release resources allocated to a solver thread
    void releaseResources();

    // Check if the portfolio should terminate
    bool shouldTerminate() const;

    // Signal all solver threads to terminate
    void terminateAllSolvers();

    // Estimate memory usage for a formula
    size_t estimateMemoryUsage(const CNF &formula) const;

    // Get available system memory
    size_t getSystemAvailableMemory() const;
};

#endif // PORTFOLIO_MANAGER_H