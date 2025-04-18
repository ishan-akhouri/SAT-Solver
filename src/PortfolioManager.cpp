#include "../include/PortfolioManager.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <cmath>
#include <pthread.h>
#include <sys/resource.h>

// Constructor
PortfolioManager::PortfolioManager(const CNF& cnf, 
                                  std::chrono::milliseconds timeout,
                                  int num_threads)
    : formula(cnf),
      solution_found(false),
      global_timeout(false),
      total_memory_used(0),
      max_concurrent_solvers(std::max(1, num_threads)),
      active_solvers(0),
      initialization_complete(false),
      global_timeout_duration(timeout),
      portfolio_start_time(std::chrono::high_resolution_clock::now()),
      winning_solver_id(-1) {
    
    // Initialize solver configurations
    initializeConfigs();
    
    // Estimate max solvers based on memory
    size_t estimated_memory = estimateMemoryUsage(formula);
    size_t system_memory = getSystemAvailableMemory();
    int memory_based_max = std::max(1, static_cast<int>(system_memory / estimated_memory));
    
    // Use the smaller of thread-based or memory-based limit
    max_concurrent_solvers = std::min(max_concurrent_solvers, memory_based_max);
    
    // Initialize statistics
    solver_statistics.resize(solver_configs.size());
    for (auto& stats : solver_statistics) {
        stats.termination_reason = -1; // Not started
    }
    
    std::cout << "Portfolio SAT Solver initialized with " << solver_configs.size() 
              << " configurations, running up to " << max_concurrent_solvers 
              << " solvers concurrently." << std::endl;
}

// Destructor
PortfolioManager::~PortfolioManager() {
    // Ensure termination
    terminateAllSolvers();
    
    // Join monitor thread if running
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }
    
    // Join all solver threads
    for (auto& thread : solver_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

// Initialize diverse solver configurations
void PortfolioManager::initializeConfigs() {
    // Config 1: Aggressive approach for random 3-SAT
    solver_configs.push_back({
        .var_decay = 0.98,    // More aggressive decay
        .use_luby_restarts = true,
        .restart_threshold = 30,    // Aggressive restarts
        .random_polarity_freq = 0.15,  // High randomization for diversity
        .use_lbd = true,
        .use_phase_saving = true,
        .max_learnt_clauses = 20000  // Aggressive learning
    });

    // Config 2: Very aggressive for hard instances
    solver_configs.push_back({
        .var_decay = 0.98,    // Very aggressive decay
        .use_luby_restarts = true,
        .restart_threshold = 25,    // Very aggressive restarts
        .random_polarity_freq = 0.10,  // Moderate randomization
        .use_lbd = true,
        .use_phase_saving = false,  // No phase saving for diversity
        .max_learnt_clauses = 25000  // Very aggressive learning
    });

    // Config 3: Balanced for random 3-SAT
    solver_configs.push_back({
        .var_decay = 0.97,    // Balanced decay
        .use_luby_restarts = false,  // Geometric restarts
        .restart_threshold = 50,     // Moderate restarts
        .random_polarity_freq = 0.08,  // Light randomization
        .use_lbd = false,
        .use_phase_saving = true,    // Keep phase information
        .max_learnt_clauses = 15000  // Balanced learning
    });

    // Config 4: Conservative backup
    solver_configs.push_back({
        .var_decay = 0.95,    // Conservative decay
        .use_luby_restarts = false,
        .restart_threshold = 100,     // Conservative restarts
        .random_polarity_freq = 0.05,  // Minimal randomization
        .use_lbd = false,
        .use_phase_saving = true,    // Keep phase information
        .max_learnt_clauses = 8000   // Minimal learning
    });
}

// Main solving method
bool PortfolioManager::solve(const CNF& formula) {
    // Reset state with proper synchronization
    {
        std::lock_guard<std::mutex> lock(result_mutex);
        solution_found = false;
        winning_solver_id = -1;
    }
    active_solvers = 0;
    initialization_complete = false;

    
    // Calculate formula ratio
    size_t num_vars = 0;
    for (const auto& clause : formula) {
        for (int literal : clause) {
            num_vars = std::max(num_vars, static_cast<size_t>(std::abs(literal)));
        }
    }
    double ratio = static_cast<double>(formula.size()) / num_vars;
    
    // Optimized adaptive delay based on ratio
    int base_delay = 0;  // Removed base delay
    int adaptive_delay = base_delay;
    if (ratio > 4.0) {
        adaptive_delay = static_cast<int>((ratio - 4.0) * 2);  // Reduced multiplier from 5 to 2
    }
    
    // Launch monitor thread first
    monitor_thread = std::thread(&PortfolioManager::monitorThread, this);
    
    // Launch solver threads with minimal delay
    for (size_t i = 0; i < solver_configs.size(); i++) {
        solver_threads.emplace_back(&PortfolioManager::solverThread, this, i, std::ref(formula));
        
        // Minimal delay for higher ratios
        if (adaptive_delay > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(adaptive_delay * 100));  // Convert to microseconds
        }
    }
    
    // Wait for all solver threads to complete
    for (auto& thread : solver_threads) {
        thread.join();
    }
    
    // Signal monitor thread to exit and wait for it
    {
        std::lock_guard<std::mutex> lock(result_mutex);
        active_solvers = 0;
    }
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }
    
    // Clear thread vectors
    solver_threads.clear();
    monitor_thread = std::thread();
    
    return solution_found;
}

// Individual solver thread
void PortfolioManager::solverThread(int solver_id, const CNF& formula) {
    try {
        // Set process priority
        setpriority(PRIO_PROCESS, 0, -10);  // High priority

        // Create solver instance
        CDCLSolverIncremental solver(formula, false, this);
        
        // Apply configuration
        configureSolver(solver, solver_id);
        
        // Start timing
        auto start = std::chrono::high_resolution_clock::now();
        
        // Solve
        bool result = solver.solve();
        
        // End timing
        auto end = std::chrono::high_resolution_clock::now();
        auto solve_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        // Print result
        std::cout << "    Solver " << solver_id << " completed with result: " 
                  << (result ? "SAT" : "UNSAT") 
                  << " in " << solve_time.count() << "µs"
                  << " (conflicts: " << solver.getConflicts() 
                  << ", decisions: " << solver.getDecisions() 
                  << ", restarts: " << solver.getRestarts() << ")\n";
        
        bool need_termination = false;
        // Record result if solution found
        if (result) {
            std::lock_guard<std::mutex> lock(result_mutex);
            if (!solution_found) {
                solution_found = true;
                best_solution = solver.getAssignments();
                winning_solver_id = solver_id;
                solver_statistics[solver_id].termination_reason = 0;  // Solution found
                need_termination = true;
                
            }
        }

        // Call terminate outside the lock to prevent potential deadlock
        if (need_termination) terminateAllSolvers();
        
        // Record statistics with the actual solver time
        recordStatistics(solver_id, solver, solve_time);

        // Decrement active solvers count using atomic directly
        active_solvers.fetch_sub(1);

        // Notify others of available resources
        resource_cv.notify_all();
        
    } catch (const std::exception& e) {
        std::cerr << "Solver " << solver_id << " failed: " << e.what() << std::endl;
        
        // Decrement active solvers using atomic directly
        active_solvers.fetch_sub(1);
        // Notify others of available resources
        resource_cv.notify_all();

        std::lock_guard<std::mutex> lock(result_mutex);
        solver_statistics[solver_id].termination_reason = 2;  // Resource limit
    }
}

// Monitor thread for timeouts and resource management
void PortfolioManager::monitorThread() {
    while (!solution_found && !global_timeout) {
        // Check global timeout
        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - portfolio_start_time);
            
        if (elapsed >= global_timeout_duration) {
            std::cout << "Portfolio timeout reached after " << elapsed.count() << "ms" << std::endl;
            global_timeout = true;
            terminateAllSolvers();
            break;
        }
        
        // Only check for completion if initialization is done
        if (initialization_complete && active_solvers.load() == 0) {
            std::cout << "All solvers completed, exiting monitor thread" << std::endl;
            break;
        }
        
        // Mark initialization as complete after a very short delay
        if (!initialization_complete && elapsed > std::chrono::milliseconds(1)) {  // Reduced from 10ms to 1ms
            initialization_complete = true;
        }
        
        // Reduce CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(1));  

    }
}

// Configure an individual solver instance
void PortfolioManager::configureSolver(CDCLSolverIncremental& solver, int config_id) {
    const auto& config = solver_configs[config_id];
    
    // Apply configuration to solver
    solver.setVarDecay(config.var_decay);
    solver.setRestartStrategy(config.use_luby_restarts, config.restart_threshold);
    solver.setRandomizedPolarities(config.random_polarity_freq);
    
    // Set maximum learned clauses
    solver.setMaxLearnts(config.max_learnt_clauses);
}

// Resource management
bool PortfolioManager::checkResourceAvailability() {
    std::unique_lock<std::mutex> lock(resource_mutex);
    
    // Use predicate version of wait_for to handle spurious wakeups
    bool has_resources = resource_cv.wait_for(lock, std::chrono::seconds(1), [this] {
        return active_solvers < max_concurrent_solvers || shouldTerminate();
    });
    
    if (shouldTerminate()) {
        return false;
    }
    
    if (!has_resources) {
        return false;  // Still no resources available
    }
    
    active_solvers.fetch_add(1);
    total_memory_used += estimateMemoryUsage(formula);
    return true;
}

void PortfolioManager::releaseResources() {
    std::lock_guard<std::mutex> lock(resource_mutex);
    active_solvers.fetch_sub(1);
    total_memory_used -= estimateMemoryUsage(formula);
    resource_cv.notify_all();
}

// Termination control
bool PortfolioManager::shouldTerminate() const {
    return solution_found || global_timeout;
}

void PortfolioManager::terminateAllSolvers() {
    {
        std::lock_guard<std::mutex> lock(termination_mutex);
        global_timeout = true;
        termination_cv.notify_all();
    }
    // Notify resource waiters outside the termination_mutex lock
    resource_cv.notify_all();
}

// Statistics recording
void PortfolioManager::recordStatistics(int solver_id, const CDCLSolverIncremental& solver, 
                                       std::chrono::microseconds solve_time) {
    std::lock_guard<std::mutex> lock(result_mutex);
    SolverStats stats;
    stats.conflicts = solver.getConflicts();
    stats.decisions = solver.getDecisions();
    stats.propagations = solver.getPropagations();
    stats.restarts = solver.getRestarts();
    stats.max_decision_level = solver.getMaxDecisionLevel();
    stats.learned_clauses = solver.getNumLearnts();
    stats.solve_time = solve_time;
    stats.peak_memory_usage = estimateMemoryUsage(formula);
    stats.termination_reason = solver_statistics[solver_id].termination_reason;
    
    solver_statistics[solver_id] = stats;
}

// Memory management
size_t PortfolioManager::estimateMemoryUsage(const CNF& formula) const {
    // Optimized memory estimation
    const size_t BASE_MEMORY = 25 * 1024 * 1024;  // Reduced from 50MB to 25MB
    const size_t CLAUSE_MEMORY = 80;  // Reduced from 100 to 80 bytes per clause
    const size_t VAR_MEMORY = 40;  // Reduced from 50 to 40 bytes per variable
    
    size_t num_vars = 0;
    for (const auto& clause : formula) {
        for (int lit : clause) {
            num_vars = std::max(num_vars, static_cast<size_t>(std::abs(lit)));
        }
    }
    
    return BASE_MEMORY + (formula.size() * CLAUSE_MEMORY) + (num_vars * VAR_MEMORY);
}

size_t PortfolioManager::getSystemAvailableMemory() const {
    // Platform-specific implementation to get available system memory
    #ifdef _WIN32
        // Windows implementation
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        return static_cast<size_t>(memInfo.ullAvailPhys);
    #elif defined(__APPLE__) || defined(__linux__)
        // Linux/MacOS implementation (simplified)
        // In a real implementation, you'd use sysinfo() on Linux or sysctl on MacOS
        return 8ULL * 1024 * 1024 * 1024;  // Default 8GB if can't determine
    #else
        return 4ULL * 1024 * 1024 * 1024;  // Default 4GB on unknown platforms
    #endif
}

// Accessors
const std::unordered_map<int, bool>& PortfolioManager::getSolution() const {
    return best_solution;
}

void PortfolioManager::setGlobalTimeout(std::chrono::milliseconds timeout) {
    global_timeout_duration = timeout;
}

void PortfolioManager::setMaxMemoryUsage(size_t max_memory_mb) {
    size_t max_memory_bytes = max_memory_mb * 1024 * 1024;
    size_t memory_per_solver = estimateMemoryUsage(formula);
    max_concurrent_solvers = std::max(1, static_cast<int>(max_memory_bytes / memory_per_solver));
}

// Statistics reporting
void PortfolioManager::printStatistics() const {
    std::cout << "\nPortfolio Solver Statistics:\n";
    std::cout << "----------------------------\n";
    
    for (int i = 0; i < solver_statistics.size(); i++) {
        const auto& stats = solver_statistics[i];
        const auto& config = solver_configs[i];
        
        std::cout << "Solver " << i << ":\n";
        std::cout << "  Configuration:\n";
        std::cout << "    Variable Decay: " << config.var_decay << "\n";
        std::cout << "    Restart Strategy: " << (config.use_luby_restarts ? "Luby" : "Geometric") << "\n";
        std::cout << "    Restart Threshold: " << config.restart_threshold << "\n";
        std::cout << "    Random Polarity Freq: " << config.random_polarity_freq << "\n";
        std::cout << "    LBD-based Deletion: " << (config.use_lbd ? "Yes" : "No") << "\n";
        std::cout << "    Phase Saving: " << (config.use_phase_saving ? "Yes" : "No") << "\n";
        std::cout << "    Max Learned Clauses: " << config.max_learnt_clauses << "\n";
        
        if (stats.termination_reason >= 0) {
            std::cout << "  Performance:\n";
            std::cout << "    Conflicts: " << stats.conflicts << "\n";
            std::cout << "    Decisions: " << stats.decisions << "\n";
            std::cout << "    Propagations: " << stats.propagations << "\n";
            std::cout << "    Restarts: " << stats.restarts << "\n";
            std::cout << "    Max Decision Level: " << stats.max_decision_level << "\n";
            std::cout << "    Learned Clauses: " << stats.learned_clauses << "\n";
            std::cout << "    Solve Time: " << stats.solve_time.count() << "µs\n";
            std::cout << "    Peak Memory: " << (stats.peak_memory_usage / (1024 * 1024)) << "MB\n";
            
            std::string termination;
            switch (stats.termination_reason) {
                case 0: termination = "Solution Found"; break;
                case 1: termination = "Timeout"; break;
                case 2: termination = "Resource Limit"; break;
                case 3: termination = "External Stop"; break;
                default: termination = "Unknown";
            }
            std::cout << "    Termination Reason: " << termination << "\n";
            
            if (i == winning_solver_id && solution_found) {
                std::cout << "  *** WINNING CONFIGURATION ***\n";
            }
        } else {
            std::cout << "  Status: Did not run (resource constraints)\n";
        }
        std::cout << "\n";
    }
    
    // Portfolio summary
    auto end_time = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - portfolio_start_time);
    
    std::cout << "Portfolio Summary:\n";
    std::cout << "  Total Runtime: " << total_time.count() << "µs\n";
    std::cout << "  Solver Configurations: " << solver_configs.size() << "\n";
    std::cout << "  Max Concurrent Solvers: " << max_concurrent_solvers << "\n";
    std::cout << "  Result: " << (solution_found ? "SATISFIABLE" : "UNSATISFIABLE") << "\n";
    
    if (solution_found) {
        std::cout << "  Winning Configuration: " << winning_solver_id << "\n";
    }
}

int PortfolioManager::getWinningSolverId() const {
    return winning_solver_id;
}