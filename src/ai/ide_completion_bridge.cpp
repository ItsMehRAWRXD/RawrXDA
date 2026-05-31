// ide_completion_bridge.cpp - IDE integration implementation
// Part of the Copilot-like inference pipeline.

#include "ide_completion_bridge.h"
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace RawrXD {

IDECompletionBridge::IDECompletionBridge(StreamingInferenceEngine* engine)
    : engine_(engine)
    , arbiter_()
{
    stats_ = {};
}

IDECompletionBridge::~IDECompletionBridge() {
    CancelCompletion();
    CancelDebounceTimer();
}

void IDECompletionBridge::RequestCompletion(
    const CompletionRequest& request,
    std::function<void(const CompletionResult&)> callback
) {
    // Cancel any existing completion
    CancelCompletion();
    
    // Start debounce timer
    if (config_.debounce_ms.count() > 0) {
        StartDebounceTimer(config_.debounce_ms);
        return;
    }
    
    // Extract context
    ContextWindow context = ExtractContext(request);
    
    // Build prompt
    std::string prompt = BuildPrompt(context, request.type);
    
    // Select kernel based on context type
    TaskType task_type;
    switch (request.type) {
        case IDEContextType::CODE_COMPLETION:
            task_type = TaskType::AUTOCOMPLETE;
            break;
        case IDEContextType::INLINE_EDIT:
            task_type = TaskType::INLINE_EDIT;
            break;
        case IDEContextType::FUNCTION_GENERATION:
        case IDEContextType::DOC_COMMENT:
        case IDEContextType::REFACTOR:
            task_type = TaskType::FULL_GENERATION;
            break;
        default:
            task_type = TaskType::AUTOCOMPLETE;
    }
    
    auto selection = arbiter_.SelectKernel(task_type, {
        .first_token = request.timeout,
        .per_token = std::chrono::microseconds(5000),
        .min_confidence = 0.8f
    });
    
    // Set kernel mode
    engine_->SetKernelMode(selection.kernel_mode);
    
    // Store callback
    current_callback_ = callback;
    generating_.store(true);
    cancelled_.store(false);
    current_completion_.clear();
    
    // Start generation
    generation_thread_ = std::thread([this, context, request, prompt]() {
        auto start_time = std::chrono::steady_clock::now();
        
        // Generate tokens
        engine_->GenerateStreaming(
            context,
            request.max_tokens,
            [this](const std::string& token) {
                if (!cancelled_.load()) {
                    OnTokenGenerated(token);
                }
            },
            [this]() {
                OnCompletionFinished();
            },
            nullptr
        );
        
        // Calculate latency
        auto end_time = std::chrono::steady_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        // Update stats
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.completions_requested++;
            stats_.avg_latency = (stats_.avg_latency * (stats_.completions_requested - 1) + latency) 
                                 / stats_.completions_requested;
            
            int kernel = engine_->GetKernelMode();
            if (kernel == 1) stats_.kernel_q4k_count++;
            else if (kernel == 3) stats_.kernel_q5k_count++;
            else if (kernel == 4) stats_.kernel_q6k_count++;
        }
        
        generating_.store(false);
    });
}

void IDECompletionBridge::CancelCompletion() {
    cancelled_.store(true);
    engine_->Stop();
    
    if (generation_thread_.joinable()) {
        generation_thread_.join();
    }
    
    ghost_text_.visible = false;
    ghost_text_.text.clear();
}

void IDECompletionBridge::StartDebounceTimer(std::chrono::milliseconds delay) {
    CancelDebounceTimer();
    
    debounce_active_.store(true);
    debounce_thread_ = std::thread([this, delay]() {
        std::this_thread::sleep_for(delay);
        
        if (debounce_active_.load()) {
            // Debounce complete, start actual generation
            // This will be called from RequestCompletion after debounce
        }
    });
}

void IDECompletionBridge::CancelDebounceTimer() {
    debounce_active_.store(false);
    if (debounce_thread_.joinable()) {
        debounce_thread_.join();
    }
}

void IDECompletionBridge::OnCompletionFinished() {
    // Completion done, ghost text is ready for accept/reject
}

IDECompletionBridge::Stats IDECompletionBridge::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

// Factory function
std::unique_ptr<IDECompletionBridge> CreateIDECompletionBridge(StreamingInferenceEngine* engine) {
    return std::make_unique<IDECompletionBridge>(engine);
}

} // namespace RawrXD