// ============================================================================
// RawrXD_AutonomousAgenticPipeline.h
// ============================================================================
// Fully functional, autonomous, and agentic multi-agent coordinator.
// Implements: IDE UI -> Chat Service -> Prompt Builder -> LLM API -> Token Stream -> Renderer
// Integrates with RawrXD_AgentHost (Sovereign) and AgentCoordinator for
// autonomous loops, self-healing, and multi-agent task distribution.
// Zero Qt; Win32 + STL only. std::expected for errors; observability via events.
//
// IDE wiring (e.g. from Win32IDE): setBuildPrompt(buildChatPrompt),
// setRouteLLM(routeWithIntelligence), setOnToken(onInferenceToken),
// setAppendRenderer(appendStreamingToken or appendCopilotResponse).
// ============================================================================

#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <chrono>
#include <atomic>
#include <optional>
#include <cstdint>

namespace RawrXD {

// ----------------------------------------------------------------------------
// Pipeline stages (IDE UI -> Chat -> Prompt -> LLM -> Token -> Renderer)
// ----------------------------------------------------------------------------
enum class PipelineStage : uint8_t {
    IDE_UI,
    ChatService,
    PromptBuilder,
    LLM_API,
    TokenStream,
    Renderer
};

const char* PipelineStageName(PipelineStage stage);

// ----------------------------------------------------------------------------
// Callbacks for each stage (inject IDE / backend implementations)
// ----------------------------------------------------------------------------
using BuildPromptFn = std::function<std::string(const std::string& userMessage)>;
using RouteLLMFn   = std::function<std::string(const std::string& prompt)>;
/** Streaming: call onToken(token, false) for each chunk, then onToken("", true) when done or on error. */
using OnTokenFn    = std::function<void(const std::string& token, bool isFinal)>;
using RouteLLMStreamingFn = std::function<bool(const std::string& prompt, const OnTokenFn& onToken)>;
using AppendRenderFn = std::function<void(const std::string& text)>;
/** Dequeue next task from external coordinator; return true if *outDesc and *outPriority were set. */
using DequeueTaskFn = std::function<bool(std::wstring* outDesc, int* outPriority)>;

// ----------------------------------------------------------------------------
// Result type for pipeline steps (std::expected-style for project rules)
// ----------------------------------------------------------------------------
struct PipelineError {
    std::string message;
    PipelineStage stage;
    int code;
};
template<typename T>
struct PipelineResult {
    bool success = false;
    T value{};
    PipelineError error{};
    static PipelineResult Ok(T v) {
        PipelineResult r;
        r.success = true;
        r.value = std::move(v);
        return r;
    }
    static PipelineResult Fail(PipelineStage s, const std::string& msg, int code = -1) {
        PipelineResult r;
        r.success = false;
        r.error = { msg, s, code };
        return r;
    }
};

// ----------------------------------------------------------------------------
// Autonomous agentic pipeline coordinator
// ----------------------------------------------------------------------------
class AutonomousAgenticPipelineCoordinator {
public:
    struct Config {
        bool enableAutonomousLoop = true;
        uint32_t coordinationIntervalMs = 1000u;
        uint32_t maxAutoFixRetries = 3u;
        bool enableSelfHealing = true;
        bool notifyExternalCoordinatorOnFailure = false;
    };

    AutonomousAgenticPipelineCoordinator();
    ~AutonomousAgenticPipelineCoordinator();

    // Pipeline wiring (Chat -> Prompt -> LLM -> Token -> Renderer)
    void setBuildPrompt(BuildPromptFn fn);
    void setRouteLLM(RouteLLMFn fn);
    void setRouteLLMStreaming(RouteLLMStreamingFn fn);
    void setOnToken(OnTokenFn fn);
    void setAppendRenderer(AppendRenderFn fn);
    void setDequeueTaskFn(DequeueTaskFn fn);
    void setContextWindow(int tokens);
    int getContextWindow() const;

    // Run one full pipeline cycle: userMessage -> prompt -> LLM -> token stream -> renderer
    PipelineResult<std::string> runPipeline(const std::string& userMessage);

    // Autonomous loop control
    void startAutonomousLoop();
    void stopAutonomousLoop();
    bool isAutonomousLoopRunning() const;

    // Multi-agent: submit task for coordinator; pipeline can be driven by tasks
    int submitAgentTask(const std::wstring& description, int priority = 1);
    void setExternalAgentCoordinator(void* coordinator);  // RawrXD_AgentCoordinator C API
    Config getConfig() const;
    void setConfig(const Config& c);

    // Self-healing: mark failure and trigger auto-fix cycle
    void reportPipelineFailure(PipelineStage stage, const std::string& message);
    void triggerAutoFixCycle();

    // Observability
    struct Stats {
        uint64_t pipelineRuns = 0;
        uint64_t pipelineFailures = 0;
        uint64_t autoFixCycles = 0;
        uint64_t tokensStreamed = 0;
        std::chrono::milliseconds totalPipelineTimeMs{0};
    };
    Stats getStats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace RawrXD

// ----------------------------------------------------------------------------
// C API for DLL / Win32 integration
// ----------------------------------------------------------------------------
extern "C" {

typedef void* RawrXD_PipelineCoordinatorHandle;

RawrXD_PipelineCoordinatorHandle RawrXD_AutonomousPipeline_Create(void);
void RawrXD_AutonomousPipeline_Destroy(RawrXD_PipelineCoordinatorHandle h);
void RawrXD_AutonomousPipeline_StartAutonomous(RawrXD_PipelineCoordinatorHandle h);
void RawrXD_AutonomousPipeline_StopAutonomous(RawrXD_PipelineCoordinatorHandle h);
int  RawrXD_AutonomousPipeline_RunPipeline(RawrXD_PipelineCoordinatorHandle h, const char* userMessage, char* outBuffer, int outBufferSize);

}
