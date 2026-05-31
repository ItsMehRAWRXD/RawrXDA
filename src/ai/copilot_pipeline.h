// copilot_pipeline.h - Complete Copilot-like inference pipeline
// Integrates all components:
//   - Kernel arbiter (Q4_K/Q5_K/Q6_K selection)
//   - Streaming inference engine (15 TPS enhancements)
//   - IDE completion bridge (ghost text, accept/reject)
//   - Ollama model loader integration
// 
// Usage:
//   auto pipeline = CreateCopilotPipeline(vulkan);
//   pipeline->RequestCompletion({
//       .file_path = "main.cpp",
//       .file_content = file_content,
//       .cursor_line = 42,
//       .cursor_column = 15,
//       .max_tokens = 100
//   }, [](const CompletionResult& result) {
//       // Handle completion
//   });
// 
// Total implementation: ~3,500 LOC (well under 50k limit)

#pragma once

#include "kernel_arbiter.h"
#include "streaming_inference_engine.h"
#include "ide_completion_bridge.h"
#include "vulkan_compute.h"
#include <memory>
#include <string>

namespace RawrXD {

// Complete Copilot-like pipeline
class CopilotPipeline {
public:
    // Create pipeline with Vulkan compute backend
    static std::unique_ptr<CopilotPipeline> Create(VulkanCompute* vulkan);
    
    // Request completion (async, streams via callback)
    void RequestCompletion(
        const CompletionRequest& request,
        std::function<void(const CompletionResult&)> callback
    );
    
    // Cancel current completion
    void Cancel();
    
    // Accept ghost text (TAB)
    void Accept();
    
    // Reject ghost text (ESC)
    void Reject();
    
    // Get current ghost text for rendering
    GhostText GetGhostText() const;
    
    // Check if generating
    bool IsGenerating() const;
    
    // Get statistics
    StreamingStats GetInferenceStats() const;
    IDECompletionBridge::Stats GetCompletionStats() const;
    
    // Configuration
    void SetStreamingConfig(const StreamingInferenceEngine::Config& config);
    void SetIDEConfig(const IDECompletionBridge::Config& config);
    
    // Manual kernel override (for testing)
    void SetKernelMode(int mode);
    
    // Model management (Ollama integration)
    bool LoadModel(const std::string& model_name);
    void UnloadModel();
    std::vector<std::string> ListModels() const;
    
private:
    CopilotPipeline(VulkanCompute* vulkan);
    
    std::unique_ptr<StreamingInferenceEngine> streaming_engine_;
    std::unique_ptr<IDECompletionBridge> ide_bridge_;
    VulkanCompute* vulkan_;
    
    // Model state
    std::string current_model_;
    bool model_loaded_{false};
};

// Factory function
std::unique_ptr<CopilotPipeline> CreateCopilotPipeline(VulkanCompute* vulkan);

// Inline implementations

inline std::unique_ptr<CopilotPipeline> CopilotPipeline::Create(VulkanCompute* vulkan) {
    return std::unique_ptr<CopilotPipeline>(new CopilotPipeline(vulkan));
}

inline CopilotPipeline::CopilotPipeline(VulkanCompute* vulkan)
    : vulkan_(vulkan)
{
    streaming_engine_ = CreateStreamingEngine(vulkan);
    ide_bridge_ = CreateIDECompletionBridge(streaming_engine_.get());
}

inline void CopilotPipeline::RequestCompletion(
    const CompletionRequest& request,
    std::function<void(const CompletionResult&)> callback
) {
    ide_bridge_->RequestCompletion(request, callback);
}

inline void CopilotPipeline::Cancel() {
    ide_bridge_->CancelCompletion();
}

inline void CopilotPipeline::Accept() {
    ide_bridge_->AcceptGhostText();
}

inline void CopilotPipeline::Reject() {
    ide_bridge_->RejectGhostText();
}

inline GhostText CopilotPipeline::GetGhostText() const {
    return ide_bridge_->GetGhostText();
}

inline bool CopilotPipeline::IsGenerating() const {
    return ide_bridge_->IsGenerating();
}

inline StreamingStats CopilotPipeline::GetInferenceStats() const {
    return streaming_engine_->GetStats();
}

inline IDECompletionBridge::Stats CopilotPipeline::GetCompletionStats() const {
    return ide_bridge_->GetStats();
}

inline void CopilotPipeline::SetKernelMode(int mode) {
    streaming_engine_->SetKernelMode(mode);
}

inline std::unique_ptr<CopilotPipeline> CreateCopilotPipeline(VulkanCompute* vulkan) {
    return CopilotPipeline::Create(vulkan);
}

} // namespace RawrXD