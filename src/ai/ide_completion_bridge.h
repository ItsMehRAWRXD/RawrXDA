// ide_completion_bridge.h - IDE integration for Copilot-like completions
// Provides:
//   - Context extraction from editor
//   - Inline ghost text rendering
//   - TAB accept / ESC cancel
//   - Real-time streaming display
// Part of the Copilot-like inference pipeline.

#pragma once

#include "streaming_inference_engine.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace RawrXD {

// IDE context types
enum class IDEContextType : uint8_t {
    CODE_COMPLETION = 0,      // Standard autocomplete
    INLINE_EDIT = 1,         // Edit at cursor
    FUNCTION_GENERATION = 2,  // Generate entire function
    DOC_COMMENT = 3,          // Generate documentation
    REFACTOR = 4,             // Refactor suggestion
};

// Completion request
struct CompletionRequest {
    IDEContextType type;
    std::string file_path;
    std::string file_content;
    int cursor_line;
    int cursor_column;
    std::string language;         // "cpp", "python", etc.
    std::string prefix;            // Lines before cursor
    std::string suffix;            // Lines after cursor
    int max_tokens;
    std::chrono::milliseconds timeout;
};

// Completion result
struct CompletionResult {
    std::string text;              // Generated text
    std::string display_text;      // Text to show as ghost
    int start_line;                // Where to insert
    int start_column;
    int end_line;                  // Where ghost ends
    int end_column;
    float confidence;
    std::chrono::microseconds latency;
    int kernel_used;               // Which quantization kernel
    bool accepted;
};

// Ghost text state for inline rendering
struct GhostText {
    std::string text;
    int start_line;
    int start_column;
    bool visible;
    std::chrono::steady_clock::time_point created;
};

// IDE completion bridge
class IDECompletionBridge {
public:
    IDECompletionBridge(StreamingInferenceEngine* engine);
    ~IDECompletionBridge();
    
    // Request completion (async)
    void RequestCompletion(
        const CompletionRequest& request,
        std::function<void(const CompletionResult&)> callback
    );
    
    // Cancel current completion
    void CancelCompletion();
    
    // Accept ghost text (TAB pressed)
    void AcceptGhostText();
    
    // Reject ghost text (ESC pressed)
    void RejectGhostText();
    
    // Get current ghost text (for rendering)
    GhostText GetGhostText() const;
    
    // Check if completion is in progress
    bool IsGenerating() const { return generating_.load(); }
    
    // Set context type for kernel selection
    void SetContextType(IDEContextType type) { current_context_type_ = type; }
    
    // Statistics
    struct Stats {
        int completions_requested;
        int completions_accepted;
        int completions_rejected;
        std::chrono::microseconds avg_latency;
        float avg_confidence;
        int kernel_q4k_count;
        int kernel_q5k_count;
        int kernel_q6k_count;
    };
    Stats GetStats() const;
    
    // Configuration
    struct Config {
        int max_context_lines = 400;        // Sliding window size
        int min_prefix_chars = 10;           // Minimum chars before cursor
        int max_completion_tokens = 100;     // Max tokens to generate
        bool enable_speculative = true;      // Use speculative decode
        bool enable_adaptive_quant = true;   // Switch kernels dynamically
        std::chrono::milliseconds debounce_ms{150}; // Debounce typing
    };
    void SetConfig(const Config& config) { config_ = config; }
    
private:
    // Context extraction
    ContextWindow ExtractContext(const CompletionRequest& request);
    
    // Build prompt from context
    std::string BuildPrompt(const ContextWindow& context, IDEContextType type);
    
    // Token streaming callback
    void OnTokenGenerated(const std::string& token);
    
    // Completion finished callback
    void OnCompletionFinished();
    
    // Debounce timer
    void StartDebounceTimer(std::chrono::milliseconds delay);
    void CancelDebounceTimer();
    
    // Members
    StreamingInferenceEngine* engine_;
    KernelArbiter arbiter_;
    
    // Current state
    std::atomic<bool> generating_{false};
    std::atomic<bool> cancelled_{false};
    std::mutex state_mutex_;
    
    // Ghost text
    GhostText ghost_text_;
    std::string current_completion_;
    std::function<void(const CompletionResult&)> current_callback_;
    
    // Debounce
    std::thread debounce_thread_;
    std::atomic<bool> debounce_active_{false};
    
    // Configuration
    Config config_;
    IDEContextType current_context_type_{IDEContextType::CODE_COMPLETION};
    
    // Statistics
    mutable std::mutex stats_mutex_;
    Stats stats_;
    
    // Generation thread
    std::thread generation_thread_;
};

// Inline implementation for hot paths

inline ContextWindow IDECompletionBridge::ExtractContext(const CompletionRequest& request) {
    ContextWindow ctx;
    ctx.file_path = request.file_path;
    ctx.cursor_line = request.cursor_line;
    ctx.cursor_column = request.cursor_column;
    
    // Build sliding window
    std::istringstream stream(request.file_content);
    std::string line;
    std::vector<std::string> lines;
    
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    
    // Take lines around cursor
    int start_line = std::max(0, request.cursor_line - config_.max_context_lines / 2);
    int end_line = std::min(static_cast<int>(lines.size()), 
                           request.cursor_line + config_.max_context_lines / 2);
    
    for (int i = start_line; i < end_line; i++) {
        if (i < request.cursor_line) {
            ctx.prefix += lines[i] + "\n";
        } else if (i == request.cursor_line) {
            // Split at cursor
            if (static_cast<int>(lines[i].length()) > request.cursor_column) {
                ctx.prefix += lines[i].substr(0, request.cursor_column);
                ctx.suffix = lines[i].substr(request.cursor_column) + "\n";
            } else {
                ctx.prefix += lines[i];
            }
        } else {
            ctx.suffix += lines[i] + "\n";
        }
    }
    
    ctx.full_context = ctx.prefix + ctx.suffix;
    
    // Hash for KV cache
    uint64_t hash = 14695981039346656037ULL;
    for (char c : ctx.full_context) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    ctx.hash = hash;
    
    return ctx;
}

inline std::string IDECompletionBridge::BuildPrompt(const ContextWindow& context, IDEContextType type) {
    std::string prompt;
    
    switch (type) {
        case IDEContextType::CODE_COMPLETION:
            prompt = "Complete the code at the cursor position.\n\n";
            prompt += "<file>\n" + context.full_context + "\n</file>\n";
            prompt += "<cursor/>\n";
            break;
            
        case IDEContextType::INLINE_EDIT:
            prompt = "Edit the code at the cursor position.\n\n";
            prompt += "<file>\n" + context.full_context + "\n</file>\n";
            prompt += "<cursor/>\n";
            break;
            
        case IDEContextType::FUNCTION_GENERATION:
            prompt = "Generate a function based on the context.\n\n";
            prompt += "<file>\n" + context.full_context + "\n</file>\n";
            prompt += "<cursor/>\n";
            break;
            
        case IDEContextType::DOC_COMMENT:
            prompt = "Generate documentation for the code.\n\n";
            prompt += "<file>\n" + context.full_context + "\n</file>\n";
            prompt += "<cursor/>\n";
            break;
            
        case IDEContextType::REFACTOR:
            prompt = "Suggest a refactoring.\n\n";
            prompt += "<file>\n" + context.full_context + "\n</file>\n";
            prompt += "<cursor/>\n";
            break;
    }
    
    return prompt;
}

inline void IDECompletionBridge::OnTokenGenerated(const std::string& token) {
    current_completion_ += token;
    
    // Update ghost text
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        ghost_text_.text = current_completion_;
        ghost_text_.visible = true;
    }
}

inline void IDECompletionBridge::AcceptGhostText() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (ghost_text_.visible && current_callback_) {
        CompletionResult result;
        result.text = current_completion_;
        result.display_text = current_completion_;
        result.accepted = true;
        result.confidence = 0.9f; // TODO: Get from engine
        
        current_callback_(result);
        
        // Update stats
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.completions_accepted++;
        }
    }
    
    ghost_text_.visible = false;
    ghost_text_.text.clear();
}

inline void IDECompletionBridge::RejectGhostText() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    ghost_text_.visible = false;
    ghost_text_.text.clear();
    current_completion_.clear();
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.completions_rejected++;
    }
}

inline GhostText IDECompletionBridge::GetGhostText() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return ghost_text_;
}

// Factory function
std::unique_ptr<IDECompletionBridge> CreateIDECompletionBridge(StreamingInferenceEngine* engine);

} // namespace RawrXD