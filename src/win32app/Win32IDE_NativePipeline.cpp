// ============================================================================
// Win32IDE_NativePipeline.cpp — Native Inference Pipeline Integration
// ============================================================================
// Bridges NativeInferencePipeline into the Win32 IDE UI:
//   - initNativePipeline / shutdownNativePipeline lifecycle
//   - loadNativeModel for GGUF model loading
//   - generateNativeResponse for synchronous / streaming inference
//   - WM_NATIVE_AI_* message handlers for token streaming into Copilot chat
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include "../core/context_deterioration_hotpatch.hpp"
#include "../../include/feature_flags_runtime.h"
#include <cstdio>
#include <sstream>

// Win32-native debug logging — unified LOG_* pipeline (per .cursorrules)
#ifndef RAWRXD_LOG_INFO
#define RAWRXD_LOG_INFO(msg) do { \
    std::ostringstream _oss; _oss << msg; \
    OutputDebugStringA(("[INFO] " + _oss.str() + "\n").c_str()); \
    LOG_INFO(_oss.str()); \
} while(0)
#endif
#ifndef RAWRXD_LOG_WARNING
#define RAWRXD_LOG_WARNING(msg) do { \
    std::ostringstream _oss; _oss << msg; \
    OutputDebugStringA(("[WARN] " + _oss.str() + "\n").c_str()); \
    LOG_WARNING(_oss.str()); \
} while(0)
#endif
#ifndef RAWRXD_LOG_ERROR
#define RAWRXD_LOG_ERROR(msg) do { \
    std::ostringstream _oss; _oss << msg; \
    OutputDebugStringA(("[ERROR] " + _oss.str() + "\n").c_str()); \
    LOG_ERROR(_oss.str()); \
} while(0)
#endif

// ============================================================================
// initNativePipeline — Create and initialize the zero-dependency pipeline
// ============================================================================
bool Win32IDE::initNativePipeline() {
    if (m_nativePipeline && m_nativePipelineReady) {
        RAWRXD_LOG_INFO("[NativePipeline] Already initialized");
        return true;
    }

    RAWRXD_LOG_INFO("[NativePipeline] Initializing native inference pipeline...");

    m_nativePipeline = std::make_unique<RawrXD::NativeInferencePipeline>();

    RawrXD::PipelineConfig cfg;

    // Wire to this window for streaming messages
    cfg.targetHWND          = m_hwndMain;
    cfg.postMessages        = true;
    cfg.backgroundInference = true;
    cfg.enableTelemetry     = RawrXD::Flags::FeatureFlagsRuntime::Instance().isEnabled(
        RawrXD::License::FeatureID::InferenceStatistics);

    // Use inference config defaults from IDE settings
    cfg.defaultSampler.temperature    = m_inferenceConfig.temperature;
    cfg.defaultSampler.topP           = m_inferenceConfig.topP;
    cfg.defaultSampler.topK           = m_inferenceConfig.topK;
    cfg.defaultSampler.repeatPenalty  = m_inferenceConfig.repetitionPenalty;
    cfg.maxContextLen                 = m_inferenceConfig.contextWindow;

    // Thread count: use half of logical cores for inference, min 1
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    cfg.inferenceThreads = (si.dwNumberOfProcessors > 2)
        ? si.dwNumberOfProcessors / 2 : 1;

    PatchResult r = m_nativePipeline->Init(cfg);
    if (!r.success) {
        RAWRXD_LOG_ERROR("[NativePipeline] Init failed: " << r.detail);
        m_nativePipeline.reset();
        m_nativePipelineReady = false;
        return false;
    }

    m_nativePipelineReady = true;
    RAWRXD_LOG_INFO("[NativePipeline] Initialized successfully ("
                    << cfg.inferenceThreads << " threads)");

    // Update status bar
    if (m_hwndStatusBar) {
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 0,
                    (LPARAM)"Native AI: Ready (no model)");
    }

    return true;
}

// ============================================================================
// shutdownNativePipeline — Clean shutdown of the pipeline
// ============================================================================
void Win32IDE::shutdownNativePipeline() {
    if (!m_nativePipeline) return;

    RAWRXD_LOG_INFO("[NativePipeline] Shutting down...");

    m_nativePipeline->Shutdown();
    m_nativePipeline.reset();
    m_nativePipelineReady = false;

    RAWRXD_LOG_INFO("[NativePipeline] Shutdown complete");
}

// ============================================================================
// loadNativeModel — Load a GGUF model into the native pipeline
// ============================================================================
bool Win32IDE::loadNativeModel(const std::string& ggufPath) {
    // Auto-initialize pipeline if not already initialized
    if (!m_nativePipelineReady) {
        if (!initNativePipeline()) {
            RAWRXD_LOG_ERROR("[NativePipeline] Cannot load model: pipeline init failed");
            return false;
        }
    }

    RAWRXD_LOG_INFO("[NativePipeline] Loading model: " << ggufPath);

    // Update status bar
    if (m_hwndStatusBar) {
        std::string status = "Native AI: Loading " + ggufPath.substr(
            ggufPath.find_last_of("\\/") + 1);
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 0,
                    (LPARAM)status.c_str());
    }

    PatchResult r = m_nativePipeline->LoadModel(ggufPath.c_str());
    if (!r.success) {
        RAWRXD_LOG_ERROR("[NativePipeline] Load failed: " << r.detail);

        if (m_hwndStatusBar) {
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0,
                        (LPARAM)"Native AI: Model load failed");
        }
        return false;
    }

    // Get model info for display
    char modelInfo[512];
    m_nativePipeline->GetModelInfo(modelInfo, sizeof(modelInfo));
    RAWRXD_LOG_INFO("[NativePipeline] Model loaded: " << modelInfo);

    // Update status bar with model name
    if (m_hwndStatusBar) {
        std::string fileName = ggufPath.substr(ggufPath.find_last_of("\\/") + 1);
        std::string status = "Native AI: " + fileName;
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 0,
                    (LPARAM)status.c_str());
    }

    // Notify copilot chat that a native model is available
    appendCopilotResponse(
        "✅ Native model loaded: " + ggufPath.substr(ggufPath.find_last_of("\\/") + 1) +
        "\r\n" + std::string(modelInfo) +
        "\r\nReady for local inference.\r\n");

    return true;
}

// ============================================================================
// generateNativeResponse — Run inference through the native pipeline
// ============================================================================
std::string Win32IDE::generateNativeResponse(const std::string& prompt) {
    if (!m_nativePipeline || !m_nativePipeline->IsModelReady()) {
        return "⚠️ Native pipeline not ready. Please load a GGUF model first.";
    }

    RAWRXD_LOG_INFO("[NativePipeline] Generating response for: "
                    << prompt.substr(0, 80) << "...");

    // Build the full chat prompt with system instructions and history
    std::string fullPrompt = buildChatPrompt(prompt);

    // Context deterioration hotpatch: keep quality at 100% by truncating
    // context before models hit their deterioration sweet-spot limit
    uint32_t ctxMax = (m_currentModelMetadata.context_length > 0)
        ? static_cast<uint32_t>(m_currentModelMetadata.context_length)
        : static_cast<uint32_t>(m_inferenceConfig.contextWindow);
    if (ctxMax == 0) ctxMax = 4096;
    const char* modelName = nullptr;
    if (!m_loadedModelPath.empty()) {
        size_t pos = m_loadedModelPath.find_last_of("\\/");
        modelName = m_loadedModelPath.c_str() + (pos == std::string::npos ? 0 : pos + 1);
    }
    auto& ctxHotpatch = ContextDeteriorationHotpatch::instance();
    if (ctxHotpatch.isEnabled()) {
        auto result = ctxHotpatch.prepareContextForInference(
            fullPrompt, ctxMax, modelName);
        fullPrompt = std::move(result.modifiedPrompt);
        if (result.mitigation != DeteriorationMitigation::None) {
            RAWRXD_LOG_INFO("[ContextHotpatch] " << result.description
                << " quality=" << result.qualityScore << "% saved "
                << result.droppedTokens << " tokens");
        }
    }

    // Apply current inference config
    RawrXD::LocalAI::SamplerConfig sampler;
    sampler.temperature   = m_inferenceConfig.temperature;
    sampler.topP          = m_inferenceConfig.topP;
    sampler.topK          = m_inferenceConfig.topK;
    sampler.repeatPenalty = m_inferenceConfig.repetitionPenalty;

    PatchResult r = m_nativePipeline->InferWithConfig(
        fullPrompt.c_str(),
        static_cast<uint32_t>(fullPrompt.size()),
        sampler);

    if (!r.success) {
        RAWRXD_LOG_ERROR("[NativePipeline] Inference failed: " << r.detail);
        return std::string("⚠️ Inference error: ") + r.detail;
    }

    // For background inference, the tokens will stream via WM_NATIVE_AI_TOKEN
    // For synchronous, we can return the output now
    if (m_nativePipeline->GetState() == RawrXD::PipelineState::Inferring) {
        return ""; // Empty — tokens will arrive via messages
    }

    // Synchronous completion
    uint32_t outLen = 0;
    const char* output = m_nativePipeline->GetLastOutput(&outLen);
    return std::string(output, outLen);
}

// ============================================================================
// WM_NATIVE_AI_TOKEN handler — streaming token arrived from worker thread
// ============================================================================
void Win32IDE::onNativeAIToken(WPARAM wParam, LPARAM lParam) {
    auto* entry = reinterpret_cast<RawrXD::TokenStreamEntry*>(lParam);
    if (!entry) return;
    if (!RawrXD::Flags::FeatureFlagsRuntime::Instance().isEnabled(
            RawrXD::License::FeatureID::TokenStreaming)) {
        VirtualFree(entry, 0, MEM_RELEASE);
        return;
    }

    // Append the token text to the active chat response
    std::string tokenText(entry->text, entry->textLen);

    // Update the copilot chat output with the streaming token
    if (m_hwndCopilotChatOutput && !tokenText.empty()) {
        // Append to the edit control
        int len = GetWindowTextLengthA(m_hwndCopilotChatOutput);
        SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, len, len);
        SendMessage(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE,
                    (LPARAM)tokenText.c_str());

        // Auto-scroll to bottom
        SendMessage(m_hwndCopilotChatOutput, EM_SCROLLCARET, 0, 0);
    }

    // Update status bar with throughput
    float tps = m_nativePipeline ? m_nativePipeline->CurrentTokensPerSec() : 0.0f;
    if (m_hwndStatusBar && entry->seqPosition % 10 == 0 &&
        RawrXD::Flags::FeatureFlagsRuntime::Instance().isEnabled(
            RawrXD::License::FeatureID::InferenceStatistics)) {
        char tpsBuf[64];
        snprintf(tpsBuf, sizeof(tpsBuf), "Native AI: %.1f tok/s", tps);
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)tpsBuf);
    }

    // Free the entry (allocated by the pipeline with VirtualAlloc)
    VirtualFree(entry, 0, MEM_RELEASE);
}

// ============================================================================
// WM_NATIVE_AI_COMPLETE handler — inference finished
// ============================================================================
void Win32IDE::onNativeAIComplete(WPARAM wParam, LPARAM lParam) {
    uint32_t outputLen    = static_cast<uint32_t>(wParam);
    float    tokensPerSec = *reinterpret_cast<float*>(&lParam);

    RAWRXD_LOG_INFO("[NativePipeline] Inference complete: "
                    << outputLen << " chars, "
                    << tokensPerSec << " tok/s");

    // Append completion marker to chat
    if (m_hwndCopilotChatOutput) {
        char summary[128];
        if (RawrXD::Flags::FeatureFlagsRuntime::Instance().isEnabled(
                RawrXD::License::FeatureID::InferenceStatistics)) {
            snprintf(summary, sizeof(summary),
                     "\r\n\r\n[Native AI — %.1f tok/s]\r\n",
                     tokensPerSec);
        } else {
            snprintf(summary, sizeof(summary),
                     "\r\n\r\n[Native AI — Complete]\r\n");
        }

        int len = GetWindowTextLengthA(m_hwndCopilotChatOutput);
        SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, len, len);
        SendMessage(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE,
                    (LPARAM)summary);
        SendMessage(m_hwndCopilotChatOutput, EM_SCROLLCARET, 0, 0);
    }

    // Get full output and store in chat history
    if (m_nativePipeline) {
        uint32_t outLen = 0;
        const char* output = m_nativePipeline->GetLastOutput(&outLen);
        if (output && outLen > 0) {
            m_chatHistory.push_back({"assistant", std::string(output, outLen)});
        }

        // Update status bar
        if (m_hwndStatusBar) {
            char statusBuf[128];
            if (RawrXD::Flags::FeatureFlagsRuntime::Instance().isEnabled(
                    RawrXD::License::FeatureID::InferenceStatistics)) {
                snprintf(statusBuf, sizeof(statusBuf),
                         "Native AI: Ready (%.1f tok/s)", tokensPerSec);
            } else {
                snprintf(statusBuf, sizeof(statusBuf), "Native AI: Ready");
            }
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)statusBuf);
        }
    }
}

// ============================================================================
// WM_NATIVE_AI_ERROR handler — inference failed
// ============================================================================
void Win32IDE::onNativeAIError() {
    RAWRXD_LOG_ERROR("[NativePipeline] Inference error occurred");

    // Show error in chat
    if (m_hwndCopilotChatOutput) {
        const char* errMsg = "\r\n⚠️ [Native AI Error — inference failed]\r\n";
        int len = GetWindowTextLengthA(m_hwndCopilotChatOutput);
        SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, len, len);
        SendMessage(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE,
                    (LPARAM)errMsg);
        SendMessage(m_hwndCopilotChatOutput, EM_SCROLLCARET, 0, 0);
    }

    // Get diagnostics
    if (m_nativePipeline) {
        char diagBuf[1024];
        m_nativePipeline->GetDiagnostics(diagBuf, sizeof(diagBuf));
        RAWRXD_LOG_ERROR("[NativePipeline] Diagnostics:\n" << diagBuf);
    }

    // Update status bar
    if (m_hwndStatusBar) {
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 0,
                    (LPARAM)"Native AI: Error");
    }
}

// ============================================================================
// WM_NATIVE_AI_PROGRESS handler — model loading progress
// ============================================================================
void Win32IDE::onNativeAIProgress() {
    RAWRXD_LOG_INFO("[NativePipeline] Progress update received");

    if (m_hwndStatusBar) {
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 0,
                    (LPARAM)"Native AI: Loading model...");
    }
}
