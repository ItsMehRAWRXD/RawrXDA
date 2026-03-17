// ============================================================================
// native_inference_pipeline.cpp — Unified Native Inference Pipeline
// ============================================================================
// Connects LocalAICore to the Win32 IDE via window messages, background
// worker threads, and a clean text-in / text-out interface.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "native_inference_pipeline.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <cstdio>

namespace RawrXD {

// ============================================================================
// State names
// ============================================================================
const char* PipelineStateName(PipelineState s) {
    switch (s) {
        case PipelineState::Idle:      return "Idle";
        case PipelineState::Loading:   return "Loading";
        case PipelineState::Ready:     return "Ready";
        case PipelineState::Inferring: return "Inferring";
        case PipelineState::Stopping:  return "Stopping";
        case PipelineState::Error:     return "Error";
        default:                       return "Unknown";
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
NativeInferencePipeline::NativeInferencePipeline() {}

NativeInferencePipeline::~NativeInferencePipeline() {
    if (m_state.load() != PipelineState::Idle) {
        Shutdown();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================
PatchResult NativeInferencePipeline::Init(const PipelineConfig& cfg) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state.load() != PipelineState::Idle) {
        return PatchResult::error("Pipeline: already initialized");
    }

    m_config = cfg;

    // Initialize the core engine
    PatchResult r = m_core.Init();
    if (!r.success) {
        m_state.store(PipelineState::Error);
        return r;
    }

    // Set thread count
    if (cfg.inferenceThreads > 0) {
        m_core.SetThreadCount(cfg.inferenceThreads);
    }

    // Allocate output buffer (64KB initial)
    m_outputCapacity = 65536;
    m_outputBuf = static_cast<char*>(
        VirtualAlloc(nullptr, m_outputCapacity,
                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!m_outputBuf) {
        m_core.Shutdown();
        return PatchResult::error("Pipeline: output buffer allocation failed");
    }
    m_outputLen = 0;

    m_state.store(PipelineState::Idle);

    // Auto-load model if path provided
    if (cfg.autoLoadOnInit && cfg.modelPath[0] != '\0') {
        r = LoadModel(cfg.modelPath);
        if (!r.success) return r;
    }

    return PatchResult::ok("Pipeline: initialized");
}

PatchResult NativeInferencePipeline::Shutdown() {
    // Stop any running inference
    if (m_state.load() == PipelineState::Inferring) {
        StopInference();
        WaitForCompletion(5000);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Join worker thread if still joinable
    if (m_workerThread.joinable()) {
        m_stopRequested.store(true);
        m_workerThread.join();
    }

    // Unload model
    if (m_core.IsModelLoaded()) {
        m_core.UnloadModel();
    }

    // Shutdown core
    m_core.Shutdown();

    // Free output buffer
    if (m_outputBuf) {
        VirtualFree(m_outputBuf, 0, MEM_RELEASE);
        m_outputBuf = nullptr;
        m_outputLen = 0;
        m_outputCapacity = 0;
    }

    m_state.store(PipelineState::Idle);
    return PatchResult::ok("Pipeline: shutdown complete");
}

// ============================================================================
// Model Management
// ============================================================================
PatchResult NativeInferencePipeline::LoadModel(const char* ggufPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_core.IsReady()) {
        return PatchResult::error("Pipeline: core not initialized");
    }

    if (m_core.IsModelLoaded()) {
        return PatchResult::error("Pipeline: model already loaded, unload first");
    }

    m_state.store(PipelineState::Loading);

    // Post progress message
    if (m_config.targetHWND && m_config.postMessages) {
        PostMessageA((HWND)m_config.targetHWND, WM_NATIVE_AI_PROGRESS, 0, 0);
    }

    PatchResult r = m_core.LoadModel(ggufPath);
    if (!r.success) {
        m_state.store(PipelineState::Error);

        if (m_config.targetHWND && m_config.postMessages) {
            PostMessageA((HWND)m_config.targetHWND, WM_NATIVE_AI_ERROR, 0, 0);
        }
        return r;
    }

    // Save model path
    strncpy_s(m_config.modelPath, ggufPath, sizeof(m_config.modelPath) - 1);

    m_state.store(PipelineState::Ready);
    return PatchResult::ok("Pipeline: model loaded");
}

PatchResult NativeInferencePipeline::UnloadModel() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state.load() == PipelineState::Inferring) {
        return PatchResult::error("Pipeline: cannot unload during inference");
    }

    PatchResult r = m_core.UnloadModel();
    if (r.success) {
        m_state.store(PipelineState::Idle);
    }
    return r;
}

PatchResult NativeInferencePipeline::SwapModel(const char* newGgufPath) {
    PatchResult r = UnloadModel();
    if (!r.success) return r;
    return LoadModel(newGgufPath);
}

bool NativeInferencePipeline::IsModelReady() const {
    return m_state.load() == PipelineState::Ready;
}

// ============================================================================
// Inference
// ============================================================================
PatchResult NativeInferencePipeline::Infer(const char* prompt, uint32_t promptLen) {
    return InferWithConfig(prompt, promptLen, m_config.defaultSampler);
}

PatchResult NativeInferencePipeline::InferWithConfig(const char* prompt,
    uint32_t promptLen, const LocalAI::SamplerConfig& sampler) {

    if (m_state.load() != PipelineState::Ready) {
        return PatchResult::error("Pipeline: not ready for inference");
    }

    // Reset output
    m_outputLen = 0;
    m_tokenCount.store(0);
    m_stopRequested.store(false);
    m_inferStart = std::chrono::high_resolution_clock::now();
    m_state.store(PipelineState::Inferring);

    if (m_config.backgroundInference) {
        // Launch on worker thread
        if (m_workerThread.joinable()) {
            m_workerThread.join();
        }

        // Copy prompt for the worker thread
        char* promptCopy = static_cast<char*>(
            VirtualAlloc(nullptr, promptLen + 1,
                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!promptCopy) {
            m_state.store(PipelineState::Ready);
            return PatchResult::error("Pipeline: prompt copy allocation failed");
        }
        memcpy(promptCopy, prompt, promptLen);
        promptCopy[promptLen] = '\0';

        m_workerThread = std::thread([this, promptCopy, promptLen, sampler]() {
            InferenceWorker(promptCopy, promptLen, sampler);
            VirtualFree(promptCopy, 0, MEM_RELEASE);
        });

        return PatchResult::ok("Pipeline: inference started (async)");
    } else {
        // Synchronous inference
        InferenceWorker(prompt, promptLen, sampler);
        return PatchResult::ok("Pipeline: inference complete");
    }
}

void NativeInferencePipeline::InferenceWorker(const char* prompt,
    uint32_t promptLen, LocalAI::SamplerConfig sampler) {

    sampler.eosToken = m_core.GetModelConfig().eosToken;

    LocalAI::InferenceResult result = m_core.InferText(
        prompt, promptLen, sampler,
        TokenCallbackTrampoline, this);

    if (result.success) {
        m_state.store(PipelineState::Ready);

        if (m_config.targetHWND && m_config.postMessages) {
            PostMessageA((HWND)m_config.targetHWND,
                        WM_NATIVE_AI_COMPLETE,
                        (WPARAM)result.outputLen,
                        (LPARAM)result.tokensPerSec);
        }
    } else {
        m_state.store(PipelineState::Error);

        if (m_config.targetHWND && m_config.postMessages) {
            PostMessageA((HWND)m_config.targetHWND,
                        WM_NATIVE_AI_ERROR, 0, 0);
        }
    }

    // Free output tokens from InferenceResult
    if (result.outputTokens) {
        VirtualFree(result.outputTokens, 0, MEM_RELEASE);
    }
}

bool NativeInferencePipeline::TokenCallbackTrampoline(void* userData,
    uint32_t tokenId, const char* text, uint32_t textLen) {
    auto* self = static_cast<NativeInferencePipeline*>(userData);
    return self->OnToken(tokenId, text, textLen);
}

bool NativeInferencePipeline::OnToken(uint32_t tokenId,
    const char* text, uint32_t textLen) {

    // Check for stop request
    if (m_stopRequested.load(std::memory_order_relaxed)) {
        return false;
    }

    // Accumulate text output
    if (text && textLen > 0 && m_outputBuf) {
        if (m_outputLen + textLen < m_outputCapacity - 1) {
            memcpy(m_outputBuf + m_outputLen, text, textLen);
            m_outputLen += textLen;
            m_outputBuf[m_outputLen] = '\0';
        }
    }

    uint32_t count = m_tokenCount.fetch_add(1, std::memory_order_relaxed) + 1;

    // Compute latency
    auto now = std::chrono::high_resolution_clock::now();
    float totalMs = std::chrono::duration<float, std::milli>(now - m_inferStart).count();
    float latencyUs = (count > 0) ? (totalMs * 1000.0f / count) : 0.0f;

    // Post token to Win32 window
    if (m_config.targetHWND && m_config.postMessages) {
        // Allocate a stream entry for the message (receiver must free)
        auto* entry = static_cast<TokenStreamEntry*>(
            VirtualAlloc(nullptr, sizeof(TokenStreamEntry),
                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (entry) {
            entry->tokenId = tokenId;
            entry->textLen = (textLen < 63) ? textLen : 63;
            if (text) memcpy(entry->text, text, entry->textLen);
            entry->text[entry->textLen] = '\0';
            entry->latencyUs = latencyUs;
            entry->seqPosition = count;

            PostMessageA((HWND)m_config.targetHWND,
                        WM_NATIVE_AI_TOKEN,
                        (WPARAM)tokenId,
                        (LPARAM)entry);
        }
    }

    return true;
}

PatchResult NativeInferencePipeline::StopInference() {
    if (m_state.load() != PipelineState::Inferring) {
        return PatchResult::error("Pipeline: not currently inferring");
    }

    m_stopRequested.store(true, std::memory_order_release);
    m_state.store(PipelineState::Stopping);

    return PatchResult::ok("Pipeline: stop requested");
}

PatchResult NativeInferencePipeline::WaitForCompletion(uint32_t timeoutMs) {
    if (!m_workerThread.joinable()) {
        return PatchResult::ok("Pipeline: no active inference");
    }

    if (timeoutMs == 0) {
        m_workerThread.join();
        return PatchResult::ok("Pipeline: inference completed");
    }

    // Timed wait using polling
    auto start = std::chrono::steady_clock::now();
    while (m_state.load() == PipelineState::Inferring ||
           m_state.load() == PipelineState::Stopping) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        if ((uint32_t)elapsed.count() >= timeoutMs) {
            return PatchResult::error("Pipeline: wait timed out");
        }
        Sleep(10);
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    return PatchResult::ok("Pipeline: inference completed");
}

const char* NativeInferencePipeline::GetLastOutput(uint32_t* outLen) const {
    if (outLen) *outLen = m_outputLen;
    return m_outputBuf ? m_outputBuf : "";
}

// ============================================================================
// Context Management
// ============================================================================
PatchResult NativeInferencePipeline::ResetContext() {
    if (m_state.load() == PipelineState::Inferring) {
        return PatchResult::error("Pipeline: cannot reset during inference");
    }
    return m_core.ResetContext();
}

uint32_t NativeInferencePipeline::ContextLength() const {
    return m_core.ContextLength();
}

// ============================================================================
// Diagnostics
// ============================================================================
float NativeInferencePipeline::CurrentTokensPerSec() const {
    uint32_t count = m_tokenCount.load(std::memory_order_relaxed);
    if (count == 0) return 0.0f;

    auto now = std::chrono::high_resolution_clock::now();
    float ms = std::chrono::duration<float, std::milli>(now - m_inferStart).count();
    return (ms > 0.0f) ? (count * 1000.0f / ms) : 0.0f;
}

void NativeInferencePipeline::GetDiagnostics(char* buf, size_t bufLen) const {
    if (!buf || bufLen == 0) return;

    const auto& perf = m_core.GetPerformance();

    snprintf(buf, bufLen,
        "=== Native Inference Pipeline ===\n"
        "State: %s\n"
        "Model: %s\n"
        "Context: %u tokens\n"
        "Output: %u chars\n"
        "Current tok/s: %.1f\n"
        "Peak tok/s: %.1f\n"
        "Total Inferences: %llu\n"
        "Total Prompt Tokens: %llu\n"
        "Total Gen Tokens: %llu\n",
        PipelineStateName(m_state.load()),
        m_config.modelPath[0] ? m_config.modelPath : "(none)",
        m_core.ContextLength(),
        m_outputLen,
        CurrentTokensPerSec(),
        perf.peakTokensPerSec.load(),
        (unsigned long long)perf.totalInferences.load(),
        (unsigned long long)perf.totalPromptTokens.load(),
        (unsigned long long)perf.totalGenTokens.load()
    );
}

void NativeInferencePipeline::GetModelInfo(char* buf, size_t bufLen) const {
    if (!buf || bufLen == 0) return;

    if (!m_core.IsModelLoaded()) {
        strncpy_s(buf, bufLen, "No model loaded", bufLen - 1);
        return;
    }

    const auto& cfg = m_core.GetModelConfig();
    snprintf(buf, bufLen,
        "Arch: %s | Layers: %u | Hidden: %u | Heads: %u/%u\n"
        "FFN: %u | Vocab: %u | MaxCtx: %u | HeadDim: %u\n"
        "Quant: %u | RoPE theta: %.0f\n",
        LocalAI::ModelArchName(cfg.arch),
        cfg.nLayers, cfg.hiddenDim,
        cfg.nHeads, cfg.nKVHeads,
        cfg.ffnDim, cfg.vocabSize, cfg.maxSeqLen, cfg.headDim,
        (uint32_t)cfg.weightQuant, cfg.ropeTheta
    );
}

} // namespace RawrXD
