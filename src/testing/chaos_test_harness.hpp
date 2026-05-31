#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace RawrXD::Testing {

// Failure modes for chaos testing
enum class FailureMode {
    None,           // No failure injection
    Exception,      // Throw exceptions randomly
    MemoryPressure, // Simulate low memory conditions
    ThreadStarvation, // Delay thread execution
    NetworkLatency, // Simulate network delays
    ResourceExhaustion, // Exhaust resources (file handles, etc.)
    DataCorruption, // Corrupt data randomly
    Timeout,        // Force timeouts
    Crash,          // Simulate crashes (SIGABRT, etc.)
    Custom          // User-defined failure
};

// Failure injection policy
struct FailurePolicy {
    FailureMode mode = FailureMode::None;
    double probability = 0.0;          // 0.0 to 1.0
    std::chrono::milliseconds delay{0}; // For thread starvation/latency
    size_t max_failures = 0;            // 0 = unlimited
    std::string custom_message;         // For custom failures
    std::function<void()> custom_action; // For custom failure actions
    
    // Advanced options
    bool fail_on_first_call = false;    // Only fail on first invocation
    bool cumulative_probability = false; // Increase probability over time
    std::vector<std::string> target_functions; // Only inject in specific functions
};

// Failure statistics
struct FailureStats {
    std::atomic<size_t> total_injections{0};
    std::atomic<size_t> successful_injections{0};
    std::atomic<size_t> blocked_injections{0};
    std::atomic<size_t> recovered_injections{0};
    
    std::unordered_map<FailureMode, size_t> failures_by_mode;
    std::unordered_map<std::string, size_t> failures_by_function;
    
    void reset() {
        total_injections.store(0);
        successful_injections.store(0);
        blocked_injections.store(0);
        recovered_injections.store(0);
        failures_by_mode.clear();
        failures_by_function.clear();
    }
};

// Copyable snapshot of failure statistics
struct FailureStatsSnapshot {
    size_t total_injections{0};
    size_t successful_injections{0};
    size_t blocked_injections{0};
    size_t recovered_injections{0};
    std::unordered_map<FailureMode, size_t> failures_by_mode;
    std::unordered_map<std::string, size_t> failures_by_function;
};

// Chaos test harness for injecting failures into production code
class ChaosTestHarness {
public:
    ChaosTestHarness() : rng_(std::random_device{}()) {}
    
    // Configure failure injection
    void set_policy(const FailurePolicy& policy) {
        std::lock_guard lock(mutex_);
        policy_ = policy;
    }
    
    // Enable/disable chaos testing
    void enable() { enabled_.store(true); }
    void disable() { enabled_.store(false); }
    bool is_enabled() const { return enabled_.load(); }
    
    // Inject failure if conditions are met
    // Returns true if failure was injected, false otherwise
    bool inject_failure(const std::string& function_name = "") {
        if (!enabled_.load()) {
            return false;
        }
        
        std::lock_guard lock(mutex_);
        stats_.total_injections++;
        
        // Check if this function is targeted
        if (!policy_.target_functions.empty() && !function_name.empty()) {
            bool found = false;
            for (const auto& target : policy_.target_functions) {
                if (function_name.find(target) != std::string::npos) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                stats_.blocked_injections++;
                return false;
            }
        }
        
        // Check max failures
        if (policy_.max_failures > 0 && stats_.successful_injections.load() >= policy_.max_failures) {
            stats_.blocked_injections++;
            return false;
        }
        
        // Check probability
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double probability = policy_.probability;
        
        if (policy_.cumulative_probability) {
            // Increase probability with each injection
            probability = std::min(1.0, probability * (1.0 + stats_.total_injections.load() * 0.1));
        }
        
        if (dist(rng_) > probability) {
            stats_.blocked_injections++;
            return false;
        }
        
        // Inject the failure
        stats_.successful_injections++;
        stats_.failures_by_mode[policy_.mode]++;
        if (!function_name.empty()) {
            stats_.failures_by_function[function_name]++;
        }
        
        execute_failure(policy_.mode, function_name);
        return true;
    }
    
    // Execute the actual failure
    void execute_failure(FailureMode mode, const std::string& function_name = "") {
        switch (mode) {
            case FailureMode::Exception:
                throw std::runtime_error("Chaos test failure in: " + 
                    (function_name.empty() ? "unknown" : function_name));
                
            case FailureMode::MemoryPressure:
                // Simulate memory pressure by allocating large blocks
                {
                    std::vector<std::vector<char>> pressure;
                    for (int i = 0; i < 100; ++i) {
                        pressure.emplace_back(1024 * 1024); // 1MB blocks
                    }
                }
                break;
                
            case FailureMode::ThreadStarvation:
                std::this_thread::sleep_for(policy_.delay);
                break;
                
            case FailureMode::NetworkLatency:
                std::this_thread::sleep_for(policy_.delay);
                break;
                
            case FailureMode::ResourceExhaustion:
                // Exhaust file handles or other resources
                {
                    std::vector<FILE*> files;
                    for (int i = 0; i < 100; ++i) {
                        FILE* f = tmpfile();
                        if (f) files.push_back(f);
                    }
                    // Files will be closed when vector goes out of scope
                }
                break;
                
            case FailureMode::DataCorruption:
                // This should be handled by the caller
                break;
                
            case FailureMode::Timeout:
                std::this_thread::sleep_for(std::chrono::hours(1));
                break;
                
            case FailureMode::Crash:
                std::abort();
                break;
                
            case FailureMode::Custom:
                if (policy_.custom_action) {
                    policy_.custom_action();
                }
                break;
                
            case FailureMode::None:
            default:
                break;
        }
    }
    
    // Get statistics snapshot
    FailureStatsSnapshot get_stats() const {
        std::lock_guard lock(mutex_);
        FailureStatsSnapshot result;
        result.total_injections = stats_.total_injections.load();
        result.successful_injections = stats_.successful_injections.load();
        result.blocked_injections = stats_.blocked_injections.load();
        result.recovered_injections = stats_.recovered_injections.load();
        result.failures_by_mode = stats_.failures_by_mode;
        result.failures_by_function = stats_.failures_by_function;
        return result;
    }
    
    // Reset statistics
    void reset_stats() {
        std::lock_guard lock(mutex_);
        stats_.reset();
    }
    
    // Scoped chaos testing
    class ScopedChaos {
    public:
        ScopedChaos(ChaosTestHarness& harness, const FailurePolicy& policy)
            : harness_(harness), old_policy_(harness.get_policy()) {
            harness_.set_policy(policy);
            harness_.enable();
        }
        
        ~ScopedChaos() {
            harness_.set_policy(old_policy_);
            harness_.disable();
        }
        
    private:
        ChaosTestHarness& harness_;
        FailurePolicy old_policy_;
    };
    
    // Create scoped chaos testing
    ScopedChaos scoped_chaos(const FailurePolicy& policy) {
        return ScopedChaos(*this, policy);
    }
    
    // Get current policy
    FailurePolicy get_policy() const {
        std::lock_guard lock(mutex_);
        return policy_;
    }
    
private:
    std::atomic<bool> enabled_{false};
    FailurePolicy policy_;
    FailureStats stats_;
    mutable std::mutex mutex_;
    std::mt19937 rng_;
};

// Chaos test context for tracking failures across test boundaries
class ChaosTestContext {
public:
    ChaosTestContext() = default;
    
    // Begin a chaos test session
    void begin_session(const std::string& session_name) {
        std::lock_guard lock(mutex_);
        current_session_ = session_name;
        session_failures_.clear();
        session_recoveries_.clear();
    }
    
    // End a chaos test session
    void end_session() {
        std::lock_guard lock(mutex_);
        current_session_.clear();
    }
    
    // Record a failure
    void record_failure(const std::string& component, const std::string& error) {
        std::lock_guard lock(mutex_);
        session_failures_[component].push_back(error);
    }
    
    // Record a recovery
    void record_recovery(const std::string& component, const std::string& recovery_action) {
        std::lock_guard lock(mutex_);
        session_recoveries_[component].push_back(recovery_action);
    }
    
    // Get session statistics
    struct SessionStats {
        std::unordered_map<std::string, std::vector<std::string>> failures;
        std::unordered_map<std::string, std::vector<std::string>> recoveries;
        size_t total_failures() const {
            size_t total = 0;
            for (const auto& [_, v] : failures) {
                total += v.size();
            }
            return total;
        }
        size_t total_recoveries() const {
            size_t total = 0;
            for (const auto& [_, v] : recoveries) {
                total += v.size();
            }
            return total;
        }
    };
    
    SessionStats get_session_stats() const {
        std::lock_guard lock(mutex_);
        return {session_failures_, session_recoveries_};
    }
    
    // Verify no failures occurred
    bool verify_no_failures() const {
        std::lock_guard lock(mutex_);
        return session_failures_.empty();
    }
    
    // Verify all failures were recovered
    bool verify_all_recovered() const {
        std::lock_guard lock(mutex_);
        for (const auto& [component, failures] : session_failures_) {
            auto it = session_recoveries_.find(component);
            if (it == session_recoveries_.end() || it->second.size() < failures.size()) {
                return false;
            }
        }
        return true;
    }
    
private:
    std::string current_session_;
    std::unordered_map<std::string, std::vector<std::string>> session_failures_;
    std::unordered_map<std::string, std::vector<std::string>> session_recoveries_;
    mutable std::mutex mutex_;
};

// Chaos test runner for executing tests with failure injection
class ChaosTestRunner {
public:
    ChaosTestRunner() = default;
    
    // Run a test with chaos injection
    template<typename TestFunc>
    bool run_with_chaos(
        TestFunc&& test_func,
        const FailurePolicy& policy,
        const std::string& test_name = "unnamed"
    ) {
        ChaosTestHarness harness;
        harness.set_policy(policy);
        harness.enable();
        
        ChaosTestContext context;
        context.begin_session(test_name);
        
        try {
            test_func(harness, context);
            context.end_session();
            
            auto stats = context.get_session_stats();
            if (stats.total_failures() > 0 && stats.total_recoveries() < stats.total_failures()) {
                // Some failures were not recovered
                return false;
            }
            
            return true;
        } catch (...) {
            context.end_session();
            return false;
        }
    }
    
    // Run a test with multiple failure modes
    template<typename TestFunc>
    bool run_with_multiple_chaos(
        TestFunc&& test_func,
        const std::vector<FailurePolicy>& policies,
        const std::string& test_name = "unnamed"
    ) {
        for (const auto& policy : policies) {
            if (!run_with_chaos(test_func, policy, test_name + "_" + std::to_string(static_cast<int>(policy.mode)))) {
                return false;
            }
        }
        return true;
    }
};

} // namespace RawrXD::Testing
