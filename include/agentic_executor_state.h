#pragma once

// C++20 State Machine for AgenticExecutor
// Explicit execution flow with deterministic transitions

#include <cstdint>
#include <string>
#include <vector>
#include <map>

/**
 * @enum AgentExecutionState
 * @brief Explicit state machine for agent execution
 *
 * Transition rules:
 * IDLE → PLANNING → EXECUTING → VERIFYING → (CORRECTING or COMPLETE)
 * Any state → FAILED
 * FAILED → (IDLE or COMPLETE)
 */
enum class AgentExecutionState : uint8_t
{
    IDLE       = 0x00,  // No execution in progress
    PLANNING   = 0x10,  // Decomposing task into steps
    EXECUTING  = 0x20,  // Running a step
    VERIFYING  = 0x30,  // Checking step result
    CORRECTING = 0x40,  // Attempting recovery after failure
    FAILED     = 0x50,  // Unrecoverable failure
    COMPLETE   = 0x60   // Execution finished successfully
};

/**
 * @struct ExecutionStateSnapshot
 * @brief Capture of runtime context at execution point
 *
 * Used for:
 * - Deterministic replay
 * - Failure analysis
 * - LLM context injection
 */
struct ExecutionStateSnapshot
{
    // Execution context
    uint64_t timestamp_ms = 0;           // Milliseconds since epoch
    uint32_t iteration_count = 0;        // Agent loop iteration
    uint32_t retry_count = 0;            // Retries for current step
    
    // Memory state
    uint64_t memory_bytes_used = 0;      // Current memory occupancy
    uint64_t context_window_tokens = 0;  // Estimated token count
    
    // Runtime environment (for MASM integration)
    uint64_t stack_pointer = 0;          // rsp
    uint64_t instruction_pointer = 0;    // rip (where we are in execution)
    uint64_t process_id = 0;             // PID of active compilation/execution
    
    // File system snapshots
    std::string current_directory;
    std::vector<std::string> modified_files;  // Files edited in this iteration
    std::vector<std::string> created_processes;  // Child processes spawned
    
    // Last error context
    std::string last_error_message;
    int32_t last_error_code = 0;
    
    // Deterministic hash for replay
    std::string execution_hash;  // SHA256 of all inputs to this step
    
    std::string toJson() const;
    static ExecutionStateSnapshot fromJson(const std::string& json_str);
};

/**
 * @struct AgentStep
 * @brief Typed internal representation of a single execution step
 *
 * Replaces raw JSON at the execution core.
 * JSON is only used at boundaries (API, config files).
 */
struct AgentStep
{
    // Identity
    std::string step_id;                 // UUID for tracking
    std::string action;                  // normalize to UPPERCASE
    std::string description;
    
    // Execution
    std::map<std::string, std::string> params;  // Action-specific parameters
    int timeout_ms = 30000;              // Default 30s timeout
    int max_retries = 2;
    
    // Verification
    std::string success_criteria;        // LLM-evaluable condition
    
    // Runtime tracking
    uint64_t created_at_ms = 0;
    uint64_t started_at_ms = 0;
    uint64_t completed_at_ms = 0;
    
    AgentExecutionState state = AgentExecutionState::IDLE;
    bool success = false;
    std::string error_message;
    
    // Replay
    int execution_attempt = 0;  // Which attempt (0 = initial, 1+ = retries)
    std::string input_hash;     // Hash of inputs for deterministic comparison
    std::string output_hash;    // Hash of outputs for validation
    
    std::string toJson() const;
    static AgentStep fromJson(const std::string& json_str);
};

/**
 * @struct AgentExecutionContext
 * @brief Complete execution context for introspection by LLM
 *
 * Feeds into planNextAction() for runtime-aware planning.
 * Replaces implicit heap of disconnected variables.
 */
struct AgentExecutionContext
{
    // Current execution state
    AgentExecutionState state = AgentExecutionState::IDLE;
    ExecutionStateSnapshot snapshot;
    
    // Task history
    std::vector<AgentStep> completed_steps;
    AgentStep* current_step = nullptr;  // Non-owning pointer
    
    // Environment
    std::string workspace_root;
    std::string goal;
    int max_iterations = 64;
    int current_iteration = 0;
    
    // Failure recovery
    std::string last_failure_reason;
    std::string correction_plan;
    
    std::string toJson() const;
};

/**
 * @class ExecutionStateCapture
 * @brief RAII helper for capturing execution state at key points
 *
 * Usage:
 *   ExecutionStateCapture capture;
 *   capture.markStackPointer();
 *   capture.recordFileModification("path/file.cpp");
 *   ExecutionStateSnapshot snap = capture.snapshot();
 */
class ExecutionStateCapture
{
public:
    ExecutionStateCapture();
    ~ExecutionStateCapture() = default;
    
    void markStackPointer();
    void markInstructionPointer();
    void recordFileModification(const std::string& path);
    void recordProcessCreation(uint64_t pid);
    void recordError(int32_t code, const std::string& message);
    
    ExecutionStateSnapshot snapshot() const;
    
private:
    ExecutionStateSnapshot m_snapshot;
};
