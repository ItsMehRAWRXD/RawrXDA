// ============================================================================
// RawrXD_AutonomousAgenticPipeline.cpp
// ============================================================================
// Fully functional, autonomous, and agentic multi-agent coordinator.
// Pipeline: IDE UI -> Chat Service -> Prompt Builder -> LLM API -> Token Stream -> Renderer
// Integrates with Sovereign/AgentHost and AgentCoordinator for autonomous
// compilation/fix cycles and self-healing. Zero Qt; Win32 + STL.
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include "RawrXD_AutonomousAgenticPipeline.h"
#include <windows.h>
#include <mutex>
#include <thread>
#include <queue>
#include <sstream>

namespace RawrXD {

// ----------------------------------------------------------------------------
// Pipeline stage names (observability)
// ----------------------------------------------------------------------------
const char* PipelineStageName(PipelineStage stage) {
    switch (stage) {
        case PipelineStage::IDE_UI:         return "IDE_UI";
        case PipelineStage::ChatService:    return "ChatService";
        case PipelineStage::PromptBuilder:  return "PromptBuilder";
        case PipelineStage::LLM_API:         return "LLM_API";
        case PipelineStage::TokenStream:    return "TokenStream";
        case PipelineStage::Renderer:        return "Renderer";
        default:                            return "Unknown";
    }
}

// ----------------------------------------------------------------------------
// Implementation
// ----------------------------------------------------------------------------
struct AutonomousAgenticPipelineCoordinator::Impl {
    AutonomousAgenticPipelineCoordinator::Config config;
    BuildPromptFn buildPrompt;
    RouteLLMFn routeLLM;
    RouteLLMStreamingFn routeLLMStreaming;
    OnTokenFn onToken;
    AppendRenderFn appendRenderer;
    DequeueTaskFn dequeueTaskFn;

    std::mutex mutex;
    std::atomic<bool> autonomousRunning{false};
    std::thread autonomousThread;
    void* externalAgentCoordinator = nullptr;  // AgentCoordinator C API

    std::queue<std::pair<std::wstring, int>> pendingTasks;
    std::wstring lastFailedMessage;
    int lastFailedRetryCount = 0;
    std::atomic<uint64_t> pipelineRuns{0};
    std::atomic<uint64_t> pipelineFailures{0};
    std::atomic<uint64_t> autoFixCycles{0};
    std::atomic<uint64_t> tokensStreamed{0};
    std::chrono::milliseconds totalPipelineTimeMs{0};
    mutable std::mutex statsMutex;

    PipelineResult<std::string> runPipelineInternal(const std::string& userMessage);
    void reportPipelineFailureInternal(PipelineStage stage, const std::string& message);
    void reportPipelineFailureInternal(PipelineStage stage, const std::string& message, const std::string& failedMessage);
    void triggerAutoFixCycleInternal(const std::string& failedMessage);

    void log(const wchar_t* prefix, const std::string& msg) {
        std::wostringstream wss;
        wss << L"[AutonomousPipeline] " << prefix << L" ";
        for (char c : msg) wss << (wchar_t)(unsigned char)c;
        wss << L"\n";
        OutputDebugStringW(wss.str().c_str());
    }
    void logInfo(const std::string& msg) {
        log(L"INFO:", msg);
    }
    void logWarn(const std::string& msg) {
        log(L"WARN:", msg);
    }
    void logError(const std::string& msg) {
        log(L"ERROR:", msg);
    }

    void autonomousLoopBody() {
        while (autonomousRunning.load(std::memory_order_relaxed)) {
            std::wstring taskDesc;
            int priority = 1;
            // 1. Try external coordinator dequeue (Task 2: bidirectional)
            if (dequeueTaskFn) {
                std::wstring outDesc;
                int outPri = 1;
                if (dequeueTaskFn(&outDesc, &outPri)) {
                    taskDesc = std::move(outDesc);
                    priority = outPri;
                }
            }
            // 2. Else pull from internal queue
            if (taskDesc.empty()) {
                std::lock_guard<std::mutex> lock(mutex);
                if (!pendingTasks.empty()) {
                    auto p = pendingTasks.front();
                    pendingTasks.pop();
                    taskDesc = p.first;
                    priority = p.second;
                }
            }
            if (!taskDesc.empty()) {
                std::string msg(taskDesc.begin(), taskDesc.end());
                auto result = runPipelineInternal(msg);
                if (!result.success) {
                    reportPipelineFailureInternal(result.error.stage, result.error.message, msg);
                    if (config.enableSelfHealing && config.maxAutoFixRetries > 0) {
                        triggerAutoFixCycleInternal(msg);
                    }
                    if (config.notifyExternalCoordinatorOnFailure && externalAgentCoordinator) {
                        logInfo("Notifying external coordinator of pipeline failure (reserved for C API)");
                    }
                }
            }
            Sleep(config.coordinationIntervalMs);
        }
    }
};

AutonomousAgenticPipelineCoordinator::AutonomousAgenticPipelineCoordinator()
    : m_impl(std::make_unique<Impl>()) {
    m_impl->logInfo("AutonomousAgenticPipelineCoordinator created");
}

AutonomousAgenticPipelineCoordinator::~AutonomousAgenticPipelineCoordinator() {
    stopAutonomousLoop();
    m_impl->logInfo("AutonomousAgenticPipelineCoordinator destroyed");
}

void AutonomousAgenticPipelineCoordinator::setBuildPrompt(BuildPromptFn fn) {
    m_impl->buildPrompt = std::move(fn);
}
void AutonomousAgenticPipelineCoordinator::setRouteLLM(RouteLLMFn fn) {
    m_impl->routeLLM = std::move(fn);
}
void AutonomousAgenticPipelineCoordinator::setRouteLLMStreaming(RouteLLMStreamingFn fn) {
    m_impl->routeLLMStreaming = std::move(fn);
}
void AutonomousAgenticPipelineCoordinator::setOnToken(OnTokenFn fn) {
    m_impl->onToken = std::move(fn);
}
void AutonomousAgenticPipelineCoordinator::setAppendRenderer(AppendRenderFn fn) {
    m_impl->appendRenderer = std::move(fn);
}
void AutonomousAgenticPipelineCoordinator::setDequeueTaskFn(DequeueTaskFn fn) {
    m_impl->dequeueTaskFn = std::move(fn);
}

PipelineResult<std::string> AutonomousAgenticPipelineCoordinator::runPipeline(const std::string& userMessage) {
    return m_impl->runPipelineInternal(userMessage);
}

PipelineResult<std::string> AutonomousAgenticPipelineCoordinator::Impl::runPipelineInternal(const std::string& userMessage) {
    auto start = std::chrono::steady_clock::now();

    if (!buildPrompt) {
        return PipelineResult<std::string>::Fail(PipelineStage::PromptBuilder, "BuildPrompt not set");
    }
    std::string prompt = buildPrompt(userMessage);
    if (prompt.empty()) {
        return PipelineResult<std::string>::Fail(PipelineStage::PromptBuilder, "Empty prompt");
    }

    std::string fullResponse;
    // Task 3: prefer streaming path when set
    if (routeLLMStreaming && onToken) {
        std::string streamed;
        bool ok = routeLLMStreaming(prompt, [this, &streamed](const std::string& token, bool isFinal) {
            streamed += token;
            if (onToken) onToken(token, isFinal);
            if (!token.empty()) tokensStreamed++;
        });
        if (!ok) {
            return PipelineResult<std::string>::Fail(PipelineStage::LLM_API, "Streaming route failed");
        }
        fullResponse = streamed;
        if (appendRenderer) appendRenderer(fullResponse);
    } else if (routeLLM) {
        fullResponse = routeLLM(prompt);
        if (onToken && !fullResponse.empty()) {
            std::string token;
            for (size_t i = 0; i < fullResponse.size(); ++i) {
                char c = fullResponse[i];
                if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                    if (!token.empty()) { onToken(token, false); tokensStreamed++; token.clear(); }
                    token.push_back(c); onToken(token, false); tokensStreamed++; token.clear();
                } else token.push_back(c);
            }
            if (!token.empty()) { onToken(token, false); tokensStreamed++; }
            onToken("", true);
        }
        if (appendRenderer) appendRenderer(fullResponse);
    } else {
        return PipelineResult<std::string>::Fail(PipelineStage::LLM_API, "RouteLLM not set");
    }

    pipelineRuns++;
    lastFailedRetryCount = 0;  // reset on success
    auto end = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(statsMutex);
        totalPipelineTimeMs += std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    }
    return PipelineResult<std::string>::Ok(fullResponse);
}

void AutonomousAgenticPipelineCoordinator::startAutonomousLoop() {
    if (m_impl->autonomousRunning.exchange(true)) {
        m_impl->logWarn("Autonomous loop already running");
        return;
    }
    if (!m_impl->config.enableAutonomousLoop) {
        m_impl->autonomousRunning = false;
        return;
    }
    m_impl->autonomousThread = std::thread([this]() {
        m_impl->autonomousLoopBody();
    });
    m_impl->logInfo("Autonomous loop started");
}

void AutonomousAgenticPipelineCoordinator::stopAutonomousLoop() {
    if (!m_impl->autonomousRunning.exchange(false)) return;
    if (m_impl->autonomousThread.joinable()) {
        m_impl->autonomousThread.join();
    }
    m_impl->logInfo("Autonomous loop stopped");
}

bool AutonomousAgenticPipelineCoordinator::isAutonomousLoopRunning() const {
    return m_impl->autonomousRunning.load(std::memory_order_relaxed);
}

int AutonomousAgenticPipelineCoordinator::submitAgentTask(const std::wstring& description, int priority) {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    static int taskId = 0;
    m_impl->pendingTasks.push({ description, priority });
    m_impl->logInfo("Task submitted: " + std::string(description.begin(), description.end()));
    return ++taskId;
}

void AutonomousAgenticPipelineCoordinator::setExternalAgentCoordinator(void* coordinator) {
    m_impl->externalAgentCoordinator = coordinator;
}
AutonomousAgenticPipelineCoordinator::Config AutonomousAgenticPipelineCoordinator::getConfig() const {
    return m_impl->config;
}
void AutonomousAgenticPipelineCoordinator::setConfig(const Config& c) {
    m_impl->config = c;
}

void AutonomousAgenticPipelineCoordinator::reportPipelineFailure(PipelineStage stage, const std::string& message) {
    m_impl->reportPipelineFailureInternal(stage, message);
}

void AutonomousAgenticPipelineCoordinator::Impl::reportPipelineFailureInternal(PipelineStage stage, const std::string& message) {
    pipelineFailures++;
    logError(std::string(PipelineStageName(stage)) + ": " + message);
}
void AutonomousAgenticPipelineCoordinator::Impl::reportPipelineFailureInternal(PipelineStage stage, const std::string& message, const std::string& failedMessage) {
    reportPipelineFailureInternal(stage, message);
    lastFailedMessage.assign(failedMessage.begin(), failedMessage.end());
}

void AutonomousAgenticPipelineCoordinator::triggerAutoFixCycle() {
    std::string msg(m_impl->lastFailedMessage.begin(), m_impl->lastFailedMessage.end());
    m_impl->triggerAutoFixCycleInternal(msg.empty() ? "manual retry" : msg);
}

void AutonomousAgenticPipelineCoordinator::Impl::triggerAutoFixCycleInternal(const std::string& failedMessage) {
    autoFixCycles++;
    lastFailedMessage.assign(failedMessage.begin(), failedMessage.end());
    if (lastFailedRetryCount < static_cast<int>(config.maxAutoFixRetries)) {
        lastFailedRetryCount++;
        std::lock_guard<std::mutex> lock(mutex);
        pendingTasks.push({ lastFailedMessage, 1 });
        logInfo("AUTO-FIX: re-submitted last failed task (retry " + std::to_string(lastFailedRetryCount) + "/" + std::to_string(config.maxAutoFixRetries) + ")");
    } else {
        logWarn("AUTO-FIX: retries exhausted; not re-submitting");
    }
}

AutonomousAgenticPipelineCoordinator::Stats AutonomousAgenticPipelineCoordinator::getStats() const {
    Stats s;
    s.pipelineRuns = m_impl->pipelineRuns.load(std::memory_order_relaxed);
    s.pipelineFailures = m_impl->pipelineFailures.load(std::memory_order_relaxed);
    s.autoFixCycles = m_impl->autoFixCycles.load(std::memory_order_relaxed);
    s.tokensStreamed = m_impl->tokensStreamed.load(std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(m_impl->statsMutex);
        s.totalPipelineTimeMs = m_impl->totalPipelineTimeMs;
    }
    return s;
}

} // namespace RawrXD

// ============================================================================
// C API
// ============================================================================
extern "C" {

RawrXD_PipelineCoordinatorHandle RawrXD_AutonomousPipeline_Create(void) {
    try {
        return new RawrXD::AutonomousAgenticPipelineCoordinator();
    } catch (...) {
        return nullptr;
    }
}

void RawrXD_AutonomousPipeline_Destroy(RawrXD_PipelineCoordinatorHandle h) {
    delete static_cast<RawrXD::AutonomousAgenticPipelineCoordinator*>(h);
}

void RawrXD_AutonomousPipeline_StartAutonomous(RawrXD_PipelineCoordinatorHandle h) {
    auto* p = static_cast<RawrXD::AutonomousAgenticPipelineCoordinator*>(h);
    if (p) p->startAutonomousLoop();
}

void RawrXD_AutonomousPipeline_StopAutonomous(RawrXD_PipelineCoordinatorHandle h) {
    auto* p = static_cast<RawrXD::AutonomousAgenticPipelineCoordinator*>(h);
    if (p) p->stopAutonomousLoop();
}

int RawrXD_AutonomousPipeline_RunPipeline(RawrXD_PipelineCoordinatorHandle h, const char* userMessage, char* outBuffer, int outBufferSize) {
    if (!h || !userMessage || !outBuffer || outBufferSize <= 0) return -1;
    auto* p = static_cast<RawrXD::AutonomousAgenticPipelineCoordinator*>(h);
    auto result = p->runPipeline(userMessage);
    if (!result.success) return -1;
    size_t len = result.value.size();
    if (len >= (size_t)outBufferSize) len = (size_t)outBufferSize - 1;
    memcpy(outBuffer, result.value.c_str(), len);
    outBuffer[len] = '\0';
    return (int)len;
}

}
