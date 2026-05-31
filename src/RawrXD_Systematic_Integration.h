// RawrXD_Systematic_Integration.h
// Systematic integration of ExecutionScheduler v2 + Extension API Bridge
// Phase 1: Foundation | Phase 2: Coordination | Phase 3: Optimization
#pragma once

#include "ExecutionScheduler_v2.h"
#include "extensions/extension_api_bridge.h"
#include <memory>
#include <functional>
#include <map>

namespace RawrXD {

// ============================================================================
// SYSTEMATIC INTEGRATION — 3 PHASES
// ============================================================================
//
// PHASE 1 (Foundation): Lock-free infrastructure + basic bridge
// PHASE 2 (Coordination): Event-driven communication + async operations  
// PHASE 3 (Optimization): Budget enforcement + predictive scheduling
//
// Both systems unified under single execution fabric

// ============================================================================
// PHASE 1: FOUNDATION — Lock-Free Infrastructure + Basic Bridge
// ============================================================================

class Phase1_Foundation {
public:
    // Initialize lock-free scheduler with extension bridge hooks
    static void init(int num_threads);
    
    // Basic task submission from extensions
    static TaskID submit_extension_task(
        const std::string& extension_id,
        std::function<void()> work,
        uint32_t priority = 5
    );
    
    // Extension lifecycle management
    static void register_extension(const std::string& id);
    static void unregister_extension(const std::string& id);
    
private:
    static std::map<std::string, ExtensionId> s_extension_map;
    static std::unique_ptr<ExecutionScheduler_v2> s_scheduler;
};

// ============================================================================
// PHASE 2: COORDINATION — Event-Driven Communication + Async Operations
// ============================================================================

class Phase2_Coordination {
public:
    // Event-driven extension notifications
    static void emit_extension_event(const std::string& event_type,
                                      const std::string& json_payload);
    
    // Async file operations via deferred commands
    static void async_read_file(const std::string& path,
                                 std::function<void(const std::vector<uint8_t>&)> callback);
    
    static void async_write_file(const std::string& path,
                                  const std::vector<uint8_t>& data,
                                  std::function<void(bool)> callback);
    
    // Command bridge: Extension commands → Scheduler tasks
    static void register_command_handler(
        const std::string& command_id,
        std::function<void*(void* user_data)> handler
    );
    
    // Event subscription with lock-free dispatch
    static uint64_t subscribe_event(const std::string& event_type,
                                      std::function<void(const std::string&)> handler);
    
private:
    struct EventSubscription {
        uint64_t id;
        std::string event_type;
        std::function<void(const std::string&)> handler;
    };
    
    static std::atomic<uint64_t> s_next_subscription_id;
    static LockFreeWorkStealingQueue<EventSubscription, 1024> s_event_queue;
};

// ============================================================================
// PHASE 3: OPTIMIZATION — Budget Enforcement + Predictive Scheduling
// ============================================================================

class Phase3_Optimization {
public:
    // Phase-aware extension execution
    static void execute_extension_phase(const std::string& extension_id,
                                         ExecutionPhase phase,
                                         const PhaseBudget& budget);
    
    // Predictive task scheduling based on extension profiles
    static void schedule_with_prediction(const std::string& extension_id,
                                          std::function<void()> work,
                                          MonotonicClock::Duration predicted_duration);
    
    // Telemetry-driven optimization
    static void record_extension_metrics(const std::string& extension_id,
                                          const BottleneckMetrics& metrics);
    
    // Auto-tune phase budgets based on historical data
    static void auto_tune_budgets();
    
private:
    struct ExtensionProfile {
        std::string id;
        MonotonicClock::Duration avg_task_duration;
        MonotonicClock::Duration max_task_duration;
        uint64_t task_count;
        double budget_exceeded_rate;
    };
    
    static std::map<std::string, ExtensionProfile> s_profiles;
    static std::unique_ptr<LockFreeTelemetryRing<65536>> s_optimization_telemetry;
};

// ============================================================================
// UNIFIED EXECUTION FABRIC
// ============================================================================

class UnifiedExecutionFabric {
public:
    // Initialize all 3 phases
    static void initialize(int num_threads);
    static void shutdown();
    
    // Unified task submission (extensions + internal)
    static TaskID submit_task(std::function<void()> work,
                               uint32_t priority = 5,
                               const std::vector<TaskID>& deps = {},
                               const std::string& extension_id = "");
    
    // Phase-bounded execution
    static void execute_bounded(ExecutionPhase phase,
                                 const PhaseBudget& budget,
                                 std::function<void()> work);
    
    // Extension API integration
    static Extensions::ExtensionAPIBridge& get_extension_bridge();
    static ExecutionScheduler_v2& get_scheduler();
    
    // Telemetry and metrics
    static BottleneckMetrics collect_metrics();
    static void print_metrics_report();
    
private:
    static std::unique_ptr<ExecutionScheduler_v2> s_scheduler;
    static std::unique_ptr<Extensions::ExtensionAPIBridge> s_extension_bridge;
    static std::atomic<bool> s_initialized{false};
};

// ============================================================================
// SYSTEMATIC VALIDATION
// ============================================================================

class SystematicValidator {
public:
    struct ValidationResult {
        bool phase1_passed;
        bool phase2_passed;
        bool phase3_passed;
        std::string report;
    };
    
    // Validate each phase independently
    static ValidationResult validate_all_phases();
    
    // Performance regression tests
    static bool validate_no_regression();
    
    // Integration stress test
    static bool stress_test_extensions_under_load();
    
private:
    static bool validate_phase1();
    static bool validate_phase2();
    static bool validate_phase3();
};

// ============================================================================
// C API FOR FFI (Both Systems Exposed)
// ============================================================================

extern "C" {
    // Phase 1: Foundation
    void rawrxd_init_foundation(int num_threads);
    uint64_t rawrxd_submit_task(void (*work)(void*), void* user_data);
    
    // Phase 2: Coordination
    void rawrxd_emit_event(const char* event_type, const char* json_payload);
    uint64_t rawrxd_subscribe_event(const char* event_type,
                                     void (*handler)(const char*, void*),
                                     void* user_data);
    
    // Phase 3: Optimization
    void rawrxd_set_phase_budget(int phase, uint64_t max_ns);
    void rawrxd_execute_bounded(int phase, uint64_t budget_ns,
                                 void (*work)(void*), void* user_data);
    
    // Unified
    void rawrxd_initialize_all(int num_threads);
    void rawrxd_shutdown();
}

} // namespace RawrXD
