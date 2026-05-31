// unified_inference_engine.h - Unified Inference Path for Chat and Agent
// Fixes P0: Agentic SubmitInference BackendError
// Part of RawrXD Production-Ready System

#ifndef UNIFIED_INFERENCE_ENGINE_H
#define UNIFIED_INFERENCE_ENGINE_H

// Forward declare to avoid conflicts with inference_playback_bridge.h
#ifndef INFERENCE_PLAYBACK_BRIDGE_H
#include "inference_playback_bridge.h"
#endif

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// INFERENCE MODES
// ============================================================================

typedef enum {
    INFERENCE_MODE_CHAT = 0,        // Direct chat inference
    INFERENCE_MODE_AGENT = 1,        // Agentic inference with tools
    INFERENCE_MODE_COMPLETION = 2,  // Code completion
    INFERENCE_MODE_EMBEDDING = 3,   // Embedding generation
    INFERENCE_MODE_RERANK = 4,      // Reranking
} InferenceMode;

// ============================================================================
// TOOL DEFINITIONS
// ============================================================================

typedef enum {
    TOOL_NONE = 0,
    TOOL_FILE_READ = 1,
    TOOL_FILE_WRITE = 2,
    TOOL_FILE_DELETE = 3,
    TOOL_FILE_SEARCH = 4,
    TOOL_TERMINAL_EXECUTE = 5,
    TOOL_GIT_COMMAND = 6,
    TOOL_LSP_DEFINITION = 7,
    TOOL_LSP_REFERENCES = 8,
    TOOL_LSP_HOVER = 9,
    TOOL_LSP_COMPLETION = 10,
    TOOL_WEB_FETCH = 11,
    TOOL_WEB_SEARCH = 12,
    TOOL_CODE_EXEC = 13,
    TOOL_MEMORY_STORE = 14,
    TOOL_MEMORY_RECALL = 15,
} ToolType;

typedef struct {
    ToolType type;
    char name[64];
    char description[256];
    char schema[1024];          // JSON schema for parameters
    bool requires_confirmation;
    bool is_destructive;
    bool is_async;
} ToolDefinition;

typedef struct {
    ToolType type;
    char tool_call_id[64];
    char name[64];
    char arguments[4096];       // JSON arguments
    bool is_executing;
    bool has_result;
    char result[16384];         // JSON result
    char error[1024];
    uint64_t start_time_ns;
    uint64_t end_time_ns;
} ToolCall;

typedef struct {
    ToolCall* calls;
    uint32_t call_count;
    uint32_t call_capacity;
    uint32_t active_count;
    uint32_t completed_count;
    uint32_t failed_count;
} ToolCallQueue;

// ============================================================================
// CONTEXT TYPES
// ============================================================================

typedef struct {
    char* content;
    uint64_t size;
    uint64_t capacity;
} DynamicBuffer;

typedef struct {
    char role[16];              // "system", "user", "assistant", "tool"
    char* content;
    uint64_t content_size;
    uint32_t token_count;
    ToolCall* tool_calls;
    uint32_t tool_call_count;
    char name[64];              // For tool responses
    char tool_call_id[64];      // For tool responses
} Message;

typedef struct {
    Message* messages;
    uint32_t message_count;
    uint32_t message_capacity;
    uint64_t total_tokens;
    uint64_t max_tokens;
} ConversationContext;

// ============================================================================
// LSP CONTEXT
// ============================================================================

typedef struct {
    char file_path[512];
    uint32_t line;
    uint32_t column;
    char symbol_name[256];
    char symbol_kind[32];
    char* definition;
    char* documentation;
    char** references;
    uint32_t reference_count;
    char* hover_text;
    char** completions;
    uint32_t completion_count;
} LSPContext;

// ============================================================================
// AGENT STATE
// ============================================================================

typedef enum {
    AGENT_STATE_IDLE = 0,
    AGENT_STATE_THINKING = 1,
    AGENT_STATE_TOOL_CALLING = 2,
    AGENT_STATE_WAITING_TOOL = 3,
    AGENT_STATE_RESPONDING = 4,
    AGENT_STATE_ERROR = 5,
    AGENT_STATE_CANCELLED = 6,
} AgentState;

typedef struct {
    AgentState state;
    uint32_t iteration_count;
    uint32_t max_iterations;
    uint32_t tool_calls_made;
    uint32_t tool_calls_succeeded;
    uint32_t tool_calls_failed;
    uint64_t total_time_ns;
    uint64_t inference_time_ns;
    uint64_t tool_time_ns;
    char current_goal[1024];
    char error_message[512];
    bool is_complete;
    bool is_cancelled;
} AgentStateTracker;

// ============================================================================
// UNIFIED INFERENCE ENGINE
// ============================================================================

typedef struct {
    // Core inference
    InferenceContext* inference;
    GGUFContext* model;
    
    // Mode
    InferenceMode mode;
    
    // Conversation
    ConversationContext conversation;
    
    // Tools
    ToolDefinition* tool_definitions;
    uint32_t tool_count;
    ToolCallQueue tool_queue;
    
    // LSP context
    LSPContext* lsp_context;
    bool has_lsp_context;
    
    // Agent state
    AgentStateTracker agent_state;
    
    // Memory (for agent)
    DynamicBuffer agent_memory;
    
    // Streaming
    bool is_streaming;
    char* stream_buffer;
    uint64_t stream_buffer_size;
    uint64_t stream_buffer_capacity;
    void (*stream_callback)(const char* token, void* user_data);
    void* stream_user_data;
    
    // Cancellation
    bool is_cancelled;
    void (*cancel_callback)(void* user_data);
    void* cancel_user_data;
    
    // Metrics
    uint64_t total_inferences;
    uint64_t total_tokens_generated;
    uint64_t total_tool_calls;
    uint64_t total_errors;
    float avg_tps;
    float avg_latency_ms;
    
    // Configuration
    float temperature;
    float top_p;
    float top_k;
    uint32_t max_tokens;
    uint32_t max_tool_iterations;
    bool enable_tools;
    bool enable_lsp_context;
    bool enable_memory;
    bool require_tool_confirmation;
    
} UnifiedInferenceEngine;

// ============================================================================
// ENGINE LIFECYCLE
// ============================================================================

// Create unified inference engine
UnifiedInferenceEngine* unified_engine_create(
    InferenceMode mode,
    uint32_t max_context_tokens
);

// Destroy unified inference engine
void unified_engine_destroy(UnifiedInferenceEngine* engine);

// Initialize with model
bool unified_engine_initialize(
    UnifiedInferenceEngine* engine,
    const char* model_path,
    uint32_t n_ctx,
    uint32_t batch_size
);

// Shutdown engine
void unified_engine_shutdown(UnifiedInferenceEngine* engine);

// ============================================================================
// MODE SWITCHING
// ============================================================================

// Switch inference mode
bool unified_engine_set_mode(
    UnifiedInferenceEngine* engine,
    InferenceMode mode
);

// Get current mode
InferenceMode unified_engine_get_mode(UnifiedInferenceEngine* engine);

// ============================================================================
// CONVERSATION API
// ============================================================================

// Add message to conversation
bool unified_engine_add_message(
    UnifiedInferenceEngine* engine,
    const char* role,
    const char* content
);

// Add tool result to conversation
bool unified_engine_add_tool_result(
    UnifiedInferenceEngine* engine,
    const char* tool_call_id,
    const char* result,
    bool is_error
);

// Clear conversation
void unified_engine_clear_conversation(UnifiedInferenceEngine* engine);

// Get conversation tokens
uint64_t unified_engine_get_token_count(UnifiedInferenceEngine* engine);

// ============================================================================
// UNIFIED SUBMIT INFERENCE (P0 FIX)
// ============================================================================

// Submit inference - UNIFIED PATH for chat and agent
// This fixes the BackendError in agentic path by using the SAME backend
typedef struct {
    bool success;
    char* response;
    uint64_t response_size;
    uint32_t tokens_generated;
    float tps;
    float latency_ms;
    ToolCall* tool_calls;
    uint32_t tool_call_count;
    char error_message[512];
    InferenceMetrics metrics;
} InferenceResult;

// Main inference function - works for BOTH chat and agent
InferenceResult unified_engine_submit(
    UnifiedInferenceEngine* engine,
    const char* prompt,
    InferenceMode mode
);

// Streaming inference
bool unified_engine_submit_streaming(
    UnifiedInferenceEngine* engine,
    const char* prompt,
    InferenceMode mode,
    void (*callback)(const char* token, void* user_data),
    void* user_data
);

// Cancel inference
void unified_engine_cancel(UnifiedInferenceEngine* engine);

// ============================================================================
// TOOL REGISTRATION
// ============================================================================

// Register tool
bool unified_engine_register_tool(
    UnifiedInferenceEngine* engine,
    const ToolDefinition* tool
);

// Register all built-in tools
bool unified_engine_register_builtin_tools(UnifiedInferenceEngine* engine);

// Unregister tool
bool unified_engine_unregister_tool(
    UnifiedInferenceEngine* engine,
    ToolType type
);

// Get tool definition
ToolDefinition* unified_engine_get_tool(
    UnifiedInferenceEngine* engine,
    ToolType type
);

// ============================================================================
// TOOL EXECUTION (P1 FIX - REAL EXECUTION)
// ============================================================================

// Tool execution result
typedef struct {
    bool success;
    char* output;
    uint64_t output_size;
    int32_t exit_code;
    uint64_t execution_time_ns;
    char error_message[512];
} ToolResult;

// Execute tool - REAL IMPLEMENTATION
ToolResult unified_engine_execute_tool(
    UnifiedInferenceEngine* engine,
    const ToolCall* call
);

// Execute file read
ToolResult tool_execute_file_read(
    const char* file_path,
    uint64_t offset,
    uint64_t size
);

// Execute file write
ToolResult tool_execute_file_write(
    const char* file_path,
    const char* content,
    uint64_t size,
    bool create_if_not_exists,
    bool append
);

// Execute file delete
ToolResult tool_execute_file_delete(const char* file_path);

// Execute file search
ToolResult tool_execute_file_search(
    const char* directory,
    const char* pattern,
    bool recursive
);

// Execute terminal command
ToolResult tool_execute_terminal(
    const char* command,
    const char* working_directory,
    uint32_t timeout_ms,
    bool capture_stderr
);

// Execute git command
ToolResult tool_execute_git(
    const char* repository_path,
    const char* command,
    const char* args
);

// Execute LSP definition
ToolResult tool_execute_lsp_definition(
    const char* file_path,
    uint32_t line,
    uint32_t column
);

// Execute LSP references
ToolResult tool_execute_lsp_references(
    const char* file_path,
    uint32_t line,
    uint32_t column
);

// Execute LSP hover
ToolResult tool_execute_lsp_hover(
    const char* file_path,
    uint32_t line,
    uint32_t column
);

// Execute LSP completion
ToolResult tool_execute_lsp_completion(
    const char* file_path,
    uint32_t line,
    uint32_t column
);

// Execute web fetch
ToolResult tool_execute_web_fetch(
    const char* url,
    uint32_t timeout_ms
);

// Execute web search
ToolResult tool_execute_web_search(
    const char* query,
    uint32_t max_results
);

// Execute code
ToolResult tool_execute_code(
    const char* language,
    const char* code,
    uint32_t timeout_ms
);

// ============================================================================
// LSP CONTEXT BRIDGE (P1 FIX)
// ============================================================================

// Set LSP context for prompt injection
bool unified_engine_set_lsp_context(
    UnifiedInferenceEngine* engine,
    const LSPContext* context
);

// Clear LSP context
void unified_engine_clear_lsp_context(UnifiedInferenceEngine* engine);

// Build prompt with LSP context
char* unified_engine_build_context_aware_prompt(
    UnifiedInferenceEngine* engine,
    const char* user_prompt
);

// ============================================================================
// AGENT LOOP (P0 FIX)
// ============================================================================

// Agent execution result
typedef struct {
    bool success;
    char* final_response;
    uint64_t final_response_size;
    uint32_t iterations_used;
    uint32_t tool_calls_made;
    uint32_t tool_calls_succeeded;
    uint32_t tool_calls_failed;
    uint64_t total_time_ns;
    uint64_t inference_time_ns;
    uint64_t tool_time_ns;
    char error_message[512];
} AgentResult;

// Run agent loop
AgentResult unified_engine_run_agent(
    UnifiedInferenceEngine* engine,
    const char* goal,
    uint32_t max_iterations
);

// Run agent loop with streaming
bool unified_engine_run_agent_streaming(
    UnifiedInferenceEngine* engine,
    const char* goal,
    uint32_t max_iterations,
    void (*thinking_callback)(const char* thought, void* user_data),
    void (*tool_callback)(const ToolCall* call, void* user_data),
    void (*response_callback)(const char* token, void* user_data),
    void* user_data
);

// ============================================================================
// GHOST TEXT (P1 FIX - REAL IMPLEMENTATION)
// ============================================================================

typedef struct {
    char* text;
    uint64_t text_size;
    uint32_t start_line;
    uint32_t start_column;
    uint32_t end_line;
    uint32_t end_column;
    float confidence;
    char source[32];         // "inference", "lsp", "snippet"
    bool is_visible;
    uint64_t display_time_ns;
} GhostTextSuggestion;

typedef struct {
    GhostTextSuggestion* suggestions;
    uint32_t suggestion_count;
    uint32_t suggestion_capacity;
    uint32_t current_index;
    bool is_showing;
    uint64_t trigger_time_ns;
    char trigger_file[512];
    uint32_t trigger_line;
    uint32_t trigger_column;
} GhostTextState;

// Initialize ghost text
bool unified_engine_init_ghost_text(UnifiedInferenceEngine* engine);

// Get ghost text suggestions
GhostTextState* unified_engine_get_ghost_text(
    UnifiedInferenceEngine* engine,
    const char* file_path,
    const char* prefix,
    uint32_t line,
    uint32_t column,
    uint32_t max_suggestions
);

// Accept ghost text
bool unified_engine_accept_ghost_text(
    UnifiedInferenceEngine* engine,
    uint32_t suggestion_index
);

// Dismiss ghost text
void unified_engine_dismiss_ghost_text(UnifiedInferenceEngine* engine);

// ============================================================================
// MEMORY API
// ============================================================================

// Store in agent memory
bool unified_engine_store_memory(
    UnifiedInferenceEngine* engine,
    const char* key,
    const char* value
);

// Recall from agent memory
const char* unified_engine_recall_memory(
    UnifiedInferenceEngine* engine,
    const char* key
);

// Clear agent memory
void unified_engine_clear_memory(UnifiedInferenceEngine* engine);

// ============================================================================
// STREAMING WITH BACKPRESSURE (P2 FIX)
// ============================================================================

typedef struct {
    uint32_t tokens_in_flight;
    uint32_t max_tokens_in_flight;
    uint64_t last_token_time_ns;
    float target_tps;
    float current_tps;
    bool is_paused;
    uint32_t batch_size;
    uint64_t batch_interval_ns;
} BackpressureState;

// Set streaming callback with backpressure
bool unified_engine_set_streaming_callback(
    UnifiedInferenceEngine* engine,
    void (*callback)(const char* token, void* user_data),
    void* user_data,
    float target_tps
);

// Get backpressure state
BackpressureState unified_engine_get_backpressure(UnifiedInferenceEngine* engine);

// Pause streaming
void unified_engine_pause_streaming(UnifiedInferenceEngine* engine);

// Resume streaming
void unified_engine_resume_streaming(UnifiedInferenceEngine* engine);

// ============================================================================
// METRICS AND STATISTICS
// ============================================================================

typedef struct {
    uint64_t total_inferences;
    uint64_t total_tokens;
    uint64_t total_tool_calls;
    uint64_t total_errors;
    float avg_tps;
    float avg_latency_ms;
    float success_rate;
    uint64_t uptime_ns;
    uint32_t active_requests;
    uint32_t queued_requests;
} EngineMetrics;

// Get engine metrics
EngineMetrics unified_engine_get_metrics(UnifiedInferenceEngine* engine);

// Reset metrics
void unified_engine_reset_metrics(UnifiedInferenceEngine* engine);

// ============================================================================
// ERROR HANDLING
// ============================================================================

typedef enum {
    ERROR_NONE = 0,
    ERROR_MODEL_NOT_LOADED = 1,
    ERROR_CONTEXT_OVERFLOW = 2,
    ERROR_TOOL_NOT_FOUND = 3,
    ERROR_TOOL_EXECUTION_FAILED = 4,
    ERROR_INFERENCE_FAILED = 5,
    ERROR_CANCELLED = 6,
    ERROR_TIMEOUT = 7,
    ERROR_MEMORY_ALLOCATION = 8,
    ERROR_INVALID_INPUT = 9,
    ERROR_LSP_UNAVAILABLE = 10,
} EngineError;

// Get last error
EngineError unified_engine_get_last_error(UnifiedInferenceEngine* engine);

// Get error message
const char* unified_engine_get_error_message(UnifiedInferenceEngine* engine);

// ============================================================================
// DEMO / TEST
// ============================================================================

#ifdef UNIFIED_INFERENCE_ENGINE_DEMO

int main(void);

#endif

#ifdef __cplusplus
}
#endif

#endif // UNIFIED_INFERENCE_ENGINE_H
