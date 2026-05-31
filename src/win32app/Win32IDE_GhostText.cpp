// ============================================================================
// Win32IDE_GhostText.cpp — Ghost Text / Inline Completion Renderer
// ============================================================================
// Provides GitHub Copilot-style inline code completions:
//   - Grayed-out "ghost text" rendered after the cursor position
//   - Tab to accept, Esc to dismiss
//   - Debounced trigger on typing pause (500ms)
//   - Integration with CompletionServer (local GGUF) or Ollama
//   - Multi-line ghost text support
//   - Cursor movement auto-dismisses
//
// Architecture:
//   - EN_CHANGE fires debounce timer (GHOST_TEXT_TIMER_ID)
//   - Timer fires → background thread requests completion
//   - WM_APP+400 delivers completion to UI thread
//   - Custom paint via editor subclass intercepts WM_PAINT
//   - Ghost text rendered in italic gray after the cursor position
// ============================================================================

#include "../RawrXD_Exports.h"
#include "../runtime/SemanticRetrieval.h"
#include "IDELogger.h"
#include "Win32IDE.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <richedit.h>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#pragma comment(lib, "Msimg32.lib")

#include "../agentic/OllamaProvider.h"
#include "../agentic/OrchestratorBridge.h"
#include "../agentic/agentic_controller_wiring.h"
#include "../skill_system/SkillInjectionHooks.h"
#include "../core/rawr_streaming_inference_core.hpp"

// Stub for METRICS (telemetry system)
namespace {
    struct MetricsStub {
        void gauge(const char* name, double value) { (void)name; (void)value; }
        double getGauge(const char* name) { (void)name; return 0.0; }
    };
    static MetricsStub METRICS;
}

// ============================================================================
// SKILL INJECTION INTEGRATION
// ============================================================================
// CRITICAL: Skill context is ALWAYS injected regardless of model/agent status.
// This provides the Cursor-style .cursorrules / system prompt injection pattern.
// The first 520 lines of context are guaranteed to contain active skill definitions.
// ============================================================================

// ============================================================================
// CONSTANTS
// ============================================================================
static const UINT_PTR GHOST_TEXT_TIMER_ID = 8888;
static const UINT_PTR TITAN_PAGING_HEARTBEAT_TIMER_ID = 8889;
static const UINT_PTR GHOST_TEXT_SPECULATIVE_TIMER_ID = 8890;  // Speculative prefetch timer
static const UINT_PTR GHOST_TEXT_RENDER_TIMER_ID = 8891;  // Micro-batched token render timer
static const UINT GHOST_TEXT_DELAY_MS = 77;   // Calibrated: Sprint TTFT p50=67ms + 10ms margin (Phase 14.2 A/B sweep)
static const UINT GHOST_TEXT_SPECULATIVE_IDLE_MS = 150;  // Idle cursor threshold for speculative prefetch
static const UINT GHOST_TEXT_RENDER_BATCH_MS = 12;  // ~one frame at 60-90 Hz; avoids per-token repaint
static const int GHOST_TEXT_MAX_CHARS = 512;  // Max ghost text length
static const int GHOST_TEXT_MAX_LINES = 8;    // Max multi-line completions
static const uint64_t GHOST_TEXT_CACHE_TTL_MS = 5000;  // 5s for 70B inference latency
static const uint64_t GHOST_TEXT_PREFIX_CACHE_TTL_MS = 10000;  // 10s for prefix cache (longer TTL)
static const size_t GHOST_TEXT_CACHE_MAX_ITEMS = 256;
static const size_t GHOST_TEXT_PREFIX_CACHE_MAX_ITEMS = 128;  // Prefix cache size
static const uint64_t GHOST_TEXT_PROVIDER_LOG_TTL_MS = 4000;  // prevent spam when providers unavailable
static const char* GHOST_TEXT_DEFAULT_OLLAMA_URL = "http://localhost:11434";
static const char* GHOST_DIFF_OVERLAY_CLASS = "RawrXD_GhostDiffOverlay";
static const int GHOST_DIFF_OVERLAY_WIDTH = 164;
static const int GHOST_DIFF_OVERLAY_HEIGHT = 28;

namespace
{
extern "C" int Bridge_ReadDraftBlockGhostA(char* out, int outCapacity, int* outTokenCount);

struct TitanGhostTelemetryState
{
    uint64_t streamSeq = 0;
    uint64_t seqGaps = 0;
    uint64_t packets = 0;
};

TitanGhostTelemetryState& titanGhostTelemetryFor(Win32IDE* ide)
{
    static std::mutex telemetryMutex;
    static std::unordered_map<Win32IDE*, TitanGhostTelemetryState> telemetryByIde;
    std::lock_guard<std::mutex> lock(telemetryMutex);
    return telemetryByIde[ide];
}

void PostGhostTokenMessage(HWND hwnd, uint64_t requestSeq, const std::string& token)
{
    if (!hwnd || token.empty())
    {
        return;
    }

    char* heapToken = _strdup(token.c_str());
    if (!heapToken)
    {
        return;
    }

    if (!PostMessageA(hwnd, WM_USER_GHOST_TOKEN, static_cast<WPARAM>(requestSeq), reinterpret_cast<LPARAM>(heapToken)))
    {
        free(heapToken);
    }
}

void PostGhostCompleteMessage(HWND hwnd, uint64_t requestSeq, const std::string& completion)
{
    if (!hwnd)
    {
        return;
    }

    char* heapText = nullptr;
    if (!completion.empty())
    {
        heapText = _strdup(completion.c_str());
        if (!heapText)
        {
            return;
        }
    }

    if (!PostMessageA(hwnd, WM_USER_GHOST_COMPLETE, static_cast<WPARAM>(requestSeq),
                      reinterpret_cast<LPARAM>(heapText)))
    {
        if (heapText)
        {
            free(heapText);
        }
    }
}

CHARRANGE MakeCollapsedSelectionSnapshot(LONG caretPos)
{
    CHARRANGE snapshot{};
    snapshot.cpMin = caretPos;
    snapshot.cpMax = caretPos;
    return snapshot;
}

bool IsSelectionSnapshotValid(const CHARRANGE& snapshot)
{
    return snapshot.cpMin >= 0 && snapshot.cpMax >= snapshot.cpMin;
}

std::string getEditorRangeUtf8(HWND editor, LONG cpMin, LONG cpMax);

// ============================================================================
// SPECULATIVE PREFETCH STATE
// ============================================================================
// Prefix cache: stores completion seeds for reuse without recomputation
struct PrefixCacheEntry
{
    std::string completion;      // The completion text
    std::string planId;         // Agentic plan ID (if from agentic)
    std::string sessionId;       // Session ID (if from agentic)
    bool fromAgentic = false;
    uint64_t createdAtMs = 0;
    uint64_t lastHitMs = 0;     // Last time this entry was used
    int hitCount = 0;           // Number of times this prefix was reused
    std::string prefixHash;     // Hash of the prefix that generated this
};

// Global prefix cache for speculative prefetch reuse
static std::unordered_map<std::string, PrefixCacheEntry> g_prefixCache;
static std::mutex g_prefixCacheMutex;
static std::string g_lastPrefetchKey;
static std::string g_lastPrefetchCompletion;
static std::string g_lastPrefetchLinePrefix;
static std::string g_lastPrefetchFilePath;
static std::string g_lastPrefetchLanguage;
static int g_lastPrefetchLineIndex = -1;
static std::atomic<int> g_speculativePrefetchHits{0};
static std::atomic<int> g_speculativePrefetchMisses{0};

// Speculative prefetch state
static std::atomic<bool> g_speculativePrefetchInProgress{false};
static std::atomic<uint64_t> g_speculativePrefetchGeneration{0};
static std::atomic<uint64_t> g_lastCursorIdleMs{0};
static std::atomic<int> g_lastCursorPos{0};
static std::string g_speculativePrefetchKey;

struct InferenceRequestToken
{
    uint64_t requestId = 0;
    uint64_t ghostSeq = 0;
    std::atomic<bool> cancelled{false};

    InferenceRequestToken(uint64_t id, uint64_t seq) : requestId(id), ghostSeq(seq) {}
};

static std::atomic<uint64_t> g_nextGhostInferenceRequestId{1};
static std::mutex g_ghostInferenceRequestTokensMutex;
static std::unordered_map<uint64_t, std::shared_ptr<InferenceRequestToken>> g_ghostInferenceRequestTokens;

std::shared_ptr<InferenceRequestToken> createGhostInferenceRequestToken(uint64_t ghostSeq)
{
    auto token = std::make_shared<InferenceRequestToken>(
        g_nextGhostInferenceRequestId.fetch_add(1, std::memory_order_acq_rel), ghostSeq);
    std::lock_guard<std::mutex> lock(g_ghostInferenceRequestTokensMutex);
    g_ghostInferenceRequestTokens[token->requestId] = token;
    return token;
}

void completeGhostInferenceRequestToken(const std::shared_ptr<InferenceRequestToken>& token)
{
    if (!token)
        return;

    std::lock_guard<std::mutex> lock(g_ghostInferenceRequestTokensMutex);
    auto it = g_ghostInferenceRequestTokens.find(token->requestId);
    if (it != g_ghostInferenceRequestTokens.end() && it->second == token)
    {
        g_ghostInferenceRequestTokens.erase(it);
    }
}

void cancelAllGhostInferenceRequestTokens()
{
    std::lock_guard<std::mutex> lock(g_ghostInferenceRequestTokensMutex);
    for (auto& pair : g_ghostInferenceRequestTokens)
    {
        if (pair.second)
        {
            pair.second->cancelled.store(true, std::memory_order_release);
        }
    }
    g_ghostInferenceRequestTokens.clear();
}

bool isGhostInferenceRequestCancelled(const std::shared_ptr<InferenceRequestToken>& token)
{
    return token && token->cancelled.load(std::memory_order_acquire);
}

struct ScopedGhostInferenceRequest
{
    explicit ScopedGhostInferenceRequest(std::shared_ptr<InferenceRequestToken> requestToken)
        : token(std::move(requestToken))
    {
    }

    ~ScopedGhostInferenceRequest()
    {
        completeGhostInferenceRequestToken(token);
    }

    std::shared_ptr<InferenceRequestToken> token;
};

bool IsLikelyPrintableKey(UINT vk)
{
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z'))
        return true;
    if (vk == VK_SPACE)
        return true;
    if (vk >= VK_OEM_1 && vk <= VK_OEM_8)
        return true;
    if (vk == VK_OEM_102)
        return true;
    return false;
}

std::string BuildSemanticContextBlock(const std::string& prefixText)
{
    if (prefixText.size() < 5)
    {
        return "";
    }

    return RawrXD::Runtime::SemanticRetrieval::BuildPromptSemanticContextBlock(prefixText, 4, "RELEVANT_CONTEXT");
}

std::string BuildGhostPromptContext(const std::string& editorContext, const std::string& linePrefix)
{
    std::string prompt;
    prompt.reserve(editorContext.size() + 2048);
    prompt += "### Current Code:\n";
    prompt += editorContext;
    prompt += "\n\n";

    const std::string semanticContext = BuildSemanticContextBlock(linePrefix);
    if (!semanticContext.empty())
    {
        prompt += semanticContext;
        prompt += "\n";
    }

    prompt += "### Continue inline:";
    return prompt;
}

bool IsRelevantGhostSuggestion(const std::string& suggestion)
{
    if (suggestion.empty())
        return false;

    std::string lowered = suggestion;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (lowered.find("explain") != std::string::npos)
        return false;
    if (lowered.find("this code") != std::string::npos)
        return false;
    if (lowered.find("here is") != std::string::npos)
        return false;

    if (suggestion.find(';') != std::string::npos)
        return true;
    if (suggestion.find('(') != std::string::npos)
        return true;
    if (suggestion.find('=') != std::string::npos)
        return true;
    if (suggestion.find('{') != std::string::npos)
        return true;

    return suggestion.size() > 2;
}

std::string ExtractGhostSuggestion(const std::string& text, const std::string& prefixEcho)
{
    if (text.empty())
        return "";

    std::string normalized = text;
    normalized.erase(std::remove(normalized.begin(), normalized.end(), '\r'), normalized.end());

    size_t searchStart = 0;
    std::string fallback;
    while (searchStart <= normalized.size())
    {
        const size_t lineEnd = normalized.find('\n', searchStart);
        std::string line = (lineEnd == std::string::npos) ? normalized.substr(searchStart)
                                                          : normalized.substr(searchStart, lineEnd - searchStart);

        while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
            line.erase(line.begin());
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
            line.pop_back();

        if (!line.empty() && line.rfind("```", 0) != 0)
        {
            if (!prefixEcho.empty() && line.rfind(prefixEcho, 0) == 0)
            {
                line.erase(0, prefixEcho.size());
                while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
                    line.erase(line.begin());
            }

            if (IsRelevantGhostSuggestion(line))
            {
                return line;
            }

            if (fallback.empty())
            {
                fallback = line;
            }
        }

        if (lineEnd == std::string::npos)
            break;
        searchStart = lineEnd + 1;
    }

    return fallback;
}

struct TitanGhostExports
{
    HMODULE module = nullptr;
    bool initialized = false;
    RAWRXD_MODEL_HANDLE activeModel = 0;
    std::string loadedModelPath;

    decltype(&RawrXD_Initialize) initialize = nullptr;
    decltype(&RawrXD_Shutdown) shutdown = nullptr;
    decltype(&RawrXD_LoadModel) loadModel = nullptr;
    decltype(&RawrXD_SelectModel) selectModel = nullptr;
    decltype(&RawrXD_SetSamplingParams) setSamplingParams = nullptr;
    decltype(&RawrXD_InferAsync) inferAsync = nullptr;
    decltype(&RawrXD_WaitForInference) waitForInference = nullptr;
    decltype(&RawrXD_CancelInference) cancelInference = nullptr;
    decltype(&RawrXD_BeginStreaming) beginStreaming = nullptr;
    decltype(&RawrXD_EndStreaming) endStreaming = nullptr;
    decltype(&RawrXD_StreamConfigureWindow) streamConfigureWindow = nullptr;
    decltype(&RawrXD_StreamPop) streamPop = nullptr;
    decltype(&RawrXD_StreamReset) streamReset = nullptr;
};

TitanGhostExports g_titanGhost;

static std::mutex g_activeTitanGhostInferenceMutex;
static RAWRXD_INFERENCE_HANDLE g_activeTitanGhostInferenceHandle = 0;
static uint64_t g_activeTitanGhostRequestSeq = 0;

void setActiveTitanGhostInference(RAWRXD_INFERENCE_HANDLE handle, uint64_t requestSeq)
{
    std::lock_guard<std::mutex> lock(g_activeTitanGhostInferenceMutex);
    g_activeTitanGhostInferenceHandle = handle;
    g_activeTitanGhostRequestSeq = requestSeq;
}

void clearActiveTitanGhostInference(RAWRXD_INFERENCE_HANDLE handle)
{
    std::lock_guard<std::mutex> lock(g_activeTitanGhostInferenceMutex);
    if (g_activeTitanGhostInferenceHandle == handle)
    {
        g_activeTitanGhostInferenceHandle = 0;
        g_activeTitanGhostRequestSeq = 0;
    }
}

void cancelActiveTitanGhostInference()
{
    RAWRXD_INFERENCE_HANDLE handle = 0;
    {
        std::lock_guard<std::mutex> lock(g_activeTitanGhostInferenceMutex);
        handle = g_activeTitanGhostInferenceHandle;
    }
    if (handle != 0 && g_titanGhost.cancelInference)
    {
        g_titanGhost.cancelInference(handle);
    }
}

bool resolveTitanGhostExports()
{
    if (g_titanGhost.module)
    {
        return true;
    }

    static const std::array<const char*, 4> kTitanDllCandidates = {
        "D:\\rawrxd\\build_smoke_verify2\\bin\\RawrXD_Titan.dll", "D:\\rawrxd\\bin\\RawrXD_Titan.dll",
        "RawrXD_Titan.dll", "bin\\RawrXD_Titan.dll"};

    const char* loadedPath = nullptr;
    for (const char* candidate : kTitanDllCandidates)
    {
        g_titanGhost.module = LoadLibraryA(candidate);
        if (g_titanGhost.module)
        {
            loadedPath = candidate;
            break;
        }
    }
    if (!g_titanGhost.module)
    {
        LOG_WARNING("Titan DLL load failed for all candidate paths");
        return false;
    }

    if (loadedPath)
    {
        LOG_INFO(std::string("Titan DLL loaded from: ") + loadedPath);
    }

    g_titanGhost.initialize =
        reinterpret_cast<decltype(g_titanGhost.initialize)>(GetProcAddress(g_titanGhost.module, "RawrXD_Initialize"));
    g_titanGhost.shutdown =
        reinterpret_cast<decltype(g_titanGhost.shutdown)>(GetProcAddress(g_titanGhost.module, "RawrXD_Shutdown"));
    g_titanGhost.loadModel =
        reinterpret_cast<decltype(g_titanGhost.loadModel)>(GetProcAddress(g_titanGhost.module, "RawrXD_LoadModel"));
    g_titanGhost.selectModel =
        reinterpret_cast<decltype(g_titanGhost.selectModel)>(GetProcAddress(g_titanGhost.module, "RawrXD_SelectModel"));
    g_titanGhost.setSamplingParams = reinterpret_cast<decltype(g_titanGhost.setSamplingParams)>(
        GetProcAddress(g_titanGhost.module, "RawrXD_SetSamplingParams"));
    g_titanGhost.inferAsync =
        reinterpret_cast<decltype(g_titanGhost.inferAsync)>(GetProcAddress(g_titanGhost.module, "RawrXD_InferAsync"));
    g_titanGhost.waitForInference = reinterpret_cast<decltype(g_titanGhost.waitForInference)>(
        GetProcAddress(g_titanGhost.module, "RawrXD_WaitForInference"));
    g_titanGhost.cancelInference = reinterpret_cast<decltype(g_titanGhost.cancelInference)>(
        GetProcAddress(g_titanGhost.module, "RawrXD_CancelInference"));
    g_titanGhost.beginStreaming = reinterpret_cast<decltype(g_titanGhost.beginStreaming)>(
        GetProcAddress(g_titanGhost.module, "RawrXD_BeginStreaming"));
    g_titanGhost.endStreaming = reinterpret_cast<decltype(g_titanGhost.endStreaming)>(
        GetProcAddress(g_titanGhost.module, "RawrXD_EndStreaming"));
    g_titanGhost.streamConfigureWindow = reinterpret_cast<decltype(g_titanGhost.streamConfigureWindow)>(
        GetProcAddress(g_titanGhost.module, "RawrXD_StreamConfigureWindow"));
    g_titanGhost.streamPop =
        reinterpret_cast<decltype(g_titanGhost.streamPop)>(GetProcAddress(g_titanGhost.module, "RawrXD_StreamPop"));
    g_titanGhost.streamReset =
        reinterpret_cast<decltype(g_titanGhost.streamReset)>(GetProcAddress(g_titanGhost.module, "RawrXD_StreamReset"));

    const bool hasCoreInference = g_titanGhost.initialize && g_titanGhost.shutdown && g_titanGhost.loadModel &&
                                  g_titanGhost.selectModel && g_titanGhost.setSamplingParams &&
                                  g_titanGhost.inferAsync && g_titanGhost.waitForInference &&
                                  g_titanGhost.beginStreaming && g_titanGhost.endStreaming;
    const bool hasStreamBridge =
        g_titanGhost.streamConfigureWindow && g_titanGhost.streamPop && g_titanGhost.streamReset;

    if (hasCoreInference && !hasStreamBridge)
    {
        LOG_INFO("Titan DLL loaded without stream bridge exports; ghost stream provider disabled for this lane");
    }

    return hasCoreInference && hasStreamBridge;
}

bool ensureTitanGhostReady(const std::string& modelPath)
{
    if (modelPath.empty() || !resolveTitanGhostExports())
    {
        return false;
    }
    if (!g_titanGhost.initialized)
    {
        if (g_titanGhost.initialize() != RAWRXD_SUCCESS)
        {
            return false;
        }
        g_titanGhost.initialized = true;
    }
    if (g_titanGhost.loadedModelPath == modelPath && g_titanGhost.activeModel != 0)
    {
        return true;
    }

    RAWRXD_MODEL_HANDLE modelHandle = 0;
    if (g_titanGhost.loadModel(modelPath.c_str(), &modelHandle) != RAWRXD_SUCCESS || modelHandle == 0)
    {
        return false;
    }
    if (g_titanGhost.selectModel(modelHandle) != RAWRXD_SUCCESS)
    {
        return false;
    }

    g_titanGhost.activeModel = modelHandle;
    g_titanGhost.loadedModelPath = modelPath;
    return true;
}

void drainTitanGhostPackets(std::string& out, uint64_t* inoutLastSeq, uint64_t* inoutGapCount,
                            uint64_t* inoutPacketCount);

}  // namespace

bool Win32IDE::startTitanAgentInferenceAsync(const std::string& prompt, uint32_t timeoutMs, bool stageOnly)
{
    if (prompt.empty() || !m_useTitanKernel || m_loadedModelPath.empty() || !m_hwndMain)
    {
        return false;
    }
    if (!ensureTitanGhostReady(m_loadedModelPath))
    {
        postOutputPanelSafe("[Titan Agent] Titan backend unavailable\n");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        if (m_titanAgentRunning)
        {
            postOutputPanelSafe("[Titan Agent] Inference already running\n");
            return false;
        }
        m_titanAgentRunning = true;
        m_titanAgentStreamText.clear();
        m_titanAgentStreamSeq = 0;
        m_titanAgentSeqGaps = 0;
        m_titanAgentPackets = 0;
        m_titanAgentPostedChars = 0;
        m_titanAgentGhostAnchorPos = -1;
        m_titanAgentStageOnly = stageOnly;
        m_titanAgentStagedText.clear();
        m_titanAgentStageSelStart = -1;
        m_titanAgentStageSelEnd = -1;
        m_titanAgentInferenceHandle = 0;
        m_titanAgentStreamHandle = 0;
        m_titanAgentStartMs = GetTickCount64();
        m_titanAgentLastPacketMs = m_titanAgentStartMs;
    }

    if (m_hwndEditor)
    {
        CHARRANGE sel{};
        SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        m_titanAgentGhostAnchorPos = sel.cpMin;
        m_titanAgentStageSelStart = sel.cpMin;
        m_titanAgentStageSelEnd = sel.cpMax;
    }

    RAWRXD_SAMPLING_PARAMS sampling{};
    sampling.temperature = 0.25f;
    sampling.top_p = 0.95f;
    sampling.top_k = 48;
    sampling.repetition_penalty = 1.05f;
    sampling.max_tokens = 256;
    if (g_titanGhost.setSamplingParams)
    {
        g_titanGhost.setSamplingParams(&sampling);
    }

    RAWRXD_INFERENCE_HANDLE streamHandle = 0;
    if (g_titanGhost.beginStreaming(&streamHandle) != RAWRXD_SUCCESS)
    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        m_titanAgentRunning = false;
        postOutputPanelSafe("[Titan Agent] beginStreaming failed\n");
        return false;
    }

    if (g_titanGhost.streamReset)
    {
        g_titanGhost.streamReset();
    }
    g_titanGhost.streamConfigureWindow(reinterpret_cast<uint64_t>(m_hwndMain), WM_TITAN_AGENT_STREAM, 0);

    RAWRXD_INFERENCE_HANDLE inferenceHandle = 0;
    if (g_titanGhost.inferAsync(prompt.c_str(), prompt.size(), &inferenceHandle) != RAWRXD_SUCCESS)
    {
        g_titanGhost.endStreaming(streamHandle);
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        m_titanAgentRunning = false;
        postOutputPanelSafe("[Titan Agent] inferAsync failed\n");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        m_titanAgentInferenceHandle = inferenceHandle;
        m_titanAgentStreamHandle = streamHandle;
    }

    postOutputPanelSafe(stageOnly ? "[Titan Agent] preview inference started (staging only)\n"
                                  : "[Titan Agent] async inference started\n");

    if (m_hwndStatusBar && IsWindow(m_hwndStatusBar))
    {
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L" [Titan] Paging... 0%");
    }

    // Start the paging heartbeat (displays aperture utilization)
    startTitanPagingHeartbeat(timeoutMs);

    HWND mainHwnd = m_hwndMain;
    std::thread(
        [mainHwnd, inferenceHandle, timeoutMs]()
        {
            RAWRXD_STATUS status = g_titanGhost.waitForInference(inferenceHandle, timeoutMs);
            if (mainHwnd && IsWindow(mainHwnd))
            {
                PostMessageA(mainHwnd, WM_TITAN_AGENT_DONE, (WPARAM)status, 0);
            }
        })
        .detach();

    return true;
}

void Win32IDE::cancelTitanAgentInferenceAsync()
{
    uint64_t handle = 0;
    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        handle = m_titanAgentInferenceHandle;
    }
    if (handle != 0 && g_titanGhost.cancelInference)
    {
        g_titanGhost.cancelInference((RAWRXD_INFERENCE_HANDLE)handle);
        if (m_hwndStatusBar && IsWindow(m_hwndStatusBar))
        {
            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L" [Titan] cancel requested");
        }
    }
}

void Win32IDE::onTitanAgentStreamMessage()
{
    std::string combined;
    size_t postedChars = 0;
    int anchorPos = -1;
    bool stageOnly = false;
    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        if (!m_titanAgentRunning)
        {
            return;
        }
        drainTitanGhostPackets(m_titanAgentStreamText, &m_titanAgentStreamSeq, &m_titanAgentSeqGaps,
                               &m_titanAgentPackets);
        combined = m_titanAgentStreamText;
        postedChars = m_titanAgentPostedChars;
        anchorPos = m_titanAgentGhostAnchorPos;
        stageOnly = m_titanAgentStageOnly;
    }

    if (combined.size() <= postedChars)
    {
        return;
    }

    std::string delta = combined.substr(postedChars);
    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        m_titanAgentPostedChars = combined.size();
    }

    if (!delta.empty())
    {
        postOutputPanelSafe(delta);
        uint64_t startMs = 0;
        {
            std::lock_guard<std::mutex> lock(m_titanAgentMutex);
            m_titanAgentLastPacketMs = GetTickCount64();
            startMs = m_titanAgentStartMs;
        }
        if (m_hwndStatusBar && IsWindow(m_hwndStatusBar) && startMs != 0)
        {
            const int elapsedSec = (int)((GetTickCount64() - startMs) / 1000);
            wchar_t sbuf[96] = {};
            swprintf_s(sbuf, 96, L" [Titan] Paging shards... %ds", elapsedSec);
            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)sbuf);
        }
    }

    if (stageOnly)
    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        m_titanAgentStagedText = combined;
        return;
    }

    if (!m_hwndEditor || anchorPos < 0)
    {
        return;
    }

    CHARRANGE sel{};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
    if (sel.cpMin != sel.cpMax || sel.cpMin != anchorPos)
    {
        return;
    }

    m_ghostTextRequestCursorPos = anchorPos;
    m_ghostTextSelectionSnapshot = MakeCollapsedSelectionSnapshot(static_cast<LONG>(anchorPos));
    const int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, anchorPos, 0);
    const int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    m_ghostTextRequestLinePrefix = getEditorRangeUtf8(m_hwndEditor, lineStart, anchorPos);
    m_ghostTextCommitContent = trimGhostText(combined);
    m_ghostTextContent = m_ghostTextCommitContent;
    if (m_ghostTextContent.empty())
    {
        m_ghostTextVisible = false;
        return;
    }
    m_ghostTextVisible = true;
    m_ghostTextAccepted = false;
    InvalidateRect(m_hwndEditor, nullptr, FALSE);
}

void Win32IDE::onTitanAgentDone(int status)
{
    onTitanAgentStreamMessage();

    uint64_t streamHandle = 0;
    uint64_t lastSeq = 0;
    uint64_t gapCount = 0;
    uint64_t packetCount = 0;
    bool stageOnly = false;
    std::string stagedText;
    int stageSelStart = -1;
    int stageSelEnd = -1;
    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        streamHandle = m_titanAgentStreamHandle;
        lastSeq = m_titanAgentStreamSeq;
        gapCount = m_titanAgentSeqGaps;
        packetCount = m_titanAgentPackets;
        stageOnly = m_titanAgentStageOnly;
        stagedText = m_titanAgentStagedText;
        stageSelStart = m_titanAgentStageSelStart;
        stageSelEnd = m_titanAgentStageSelEnd;
        m_titanAgentInferenceHandle = 0;
        m_titanAgentStreamHandle = 0;
        m_titanAgentStageOnly = false;
        m_titanAgentStagedText.clear();
        m_titanAgentStageSelStart = -1;
        m_titanAgentStageSelEnd = -1;
        m_titanAgentRunning = false;
        m_titanAgentStartMs = 0;
        m_titanAgentLastPacketMs = 0;
    }

    if (m_hwndMain)
    {
        KillTimer(m_hwndMain, TITAN_PAGING_HEARTBEAT_TIMER_ID);
    }

    // Stop the paging heartbeat
    stopTitanPagingHeartbeat();

    if (streamHandle != 0)
    {
        g_titanGhost.endStreaming((RAWRXD_INFERENCE_HANDLE)streamHandle);
    }

    if (status == (int)RAWRXD_SUCCESS)
    {
        postOutputPanelSafe("\n[Titan Agent] completed\n");
        if (m_hwndStatusBar && IsWindow(m_hwndStatusBar))
        {
            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Ready");
        }
    }
    else
    {
        postOutputPanelSafe("\n[Titan Agent] failed status=" + std::to_string((int)status) + "\n");
        if (m_hwndStatusBar && IsWindow(m_hwndStatusBar))
        {
            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L" [Titan] failed");
        }
    }

    if (stageOnly && status == (int)RAWRXD_SUCCESS && m_hwndEditor)
    {
        const std::string stagedTrimmed = trimGhostText(stagedText);
        if (!stagedTrimmed.empty())
        {
            std::string preview = stagedTrimmed.substr(0, 900);
            if (stagedTrimmed.size() > preview.size())
            {
                preview += "\n...[truncated]";
            }

            std::string prompt = "Sovereign Diff Preview\n\n"
                                 "Review staged Titan output before apply.\n"
                                 "Press OK to apply to editor selection/cursor, or Cancel to discard.\n\n"
                                 "--- Preview ---\n" +
                                 preview;

            const int choice = MessageBoxA(m_hwndMain ? m_hwndMain : m_hwndEditor, prompt.c_str(),
                                           "RawrXD Sovereign Diff", MB_OKCANCEL | MB_ICONQUESTION);
            if (choice == IDOK)
            {
                CHARRANGE replaceSel{};
                replaceSel.cpMin = (stageSelStart >= 0) ? stageSelStart : 0;
                replaceSel.cpMax = (stageSelEnd >= 0) ? stageSelEnd : replaceSel.cpMin;
                SendMessageA(m_hwndEditor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&replaceSel));
                SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(stagedTrimmed.c_str()));
                markFileModified();
                postOutputPanelSafe("[Titan Agent] staged preview applied\n");
            }
            else
            {
                postOutputPanelSafe("[Titan Agent] staged preview discarded\n");
            }
        }
    }

    if (gapCount > 0)
    {
        postOutputPanelSafe("[Titan Agent] stream seq gaps=" + std::to_string(gapCount) +
                            " packets=" + std::to_string(packetCount) + " last_seq=" + std::to_string(lastSeq) + "\n");
    }
}

namespace
{

void drainTitanGhostPackets(std::string& out, uint64_t* inoutLastSeq, uint64_t* inoutGapCount,
                            uint64_t* inoutPacketCount)
{
    if (!g_titanGhost.streamPop)
    {
        return;
    }

    char buffer[513] = {};
    uint32_t chunkLen = 0;
    uint64_t seq = 0;
    uint64_t lastSeq = inoutLastSeq ? *inoutLastSeq : 0;
    uint64_t gapCount = inoutGapCount ? *inoutGapCount : 0;
    uint64_t packetCount = inoutPacketCount ? *inoutPacketCount : 0;
    while (g_titanGhost.streamPop(buffer, sizeof(buffer), &chunkLen, &seq) == RAWRXD_SUCCESS && chunkLen > 0)
    {
        if (lastSeq != 0 && seq > lastSeq + 1)
        {
            gapCount += (seq - (lastSeq + 1));
        }
        lastSeq = seq;
        packetCount += 1;
        out.append(buffer, buffer + chunkLen);
        chunkLen = 0;
        buffer[0] = '\0';
    }

    if (inoutLastSeq)
        *inoutLastSeq = lastSeq;
    if (inoutGapCount)
        *inoutGapCount = gapCount;
    if (inoutPacketCount)
        *inoutPacketCount = packetCount;
}

uint64_t nowMs()
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string buildGhostCacheKey(const std::string& filePath, const std::string& language, const std::string& prefix,
                               const std::string& suffix, int line, int column)
{
    const size_t prefixWindow = (prefix.size() > 512) ? 512 : prefix.size();
    const size_t suffixWindow = (suffix.size() > 256) ? 256 : suffix.size();
    const std::string prefixTail = prefix.substr(prefix.size() - prefixWindow, prefixWindow);
    const std::string suffixHead = suffix.substr(0, suffixWindow);
    const std::string seed = filePath + "|" + language + "|" + std::to_string(line) + "|" + std::to_string(column) +
                             "|" + prefixTail + "|" + suffixHead;
    return std::to_string(std::hash<std::string>{}(seed));
}

// ============================================================================
// PREFIX CACHE OPERATIONS
// ============================================================================

// Store a completion in the prefix cache for reuse
void storePrefixCacheEntry(const std::string& key, const std::string& completion,
                           const std::string& planId, const std::string& sessionId, bool fromAgentic)
{
    std::lock_guard<std::mutex> lock(g_prefixCacheMutex);
    
    // Evict oldest entries if cache is full
    if (g_prefixCache.size() >= GHOST_TEXT_PREFIX_CACHE_MAX_ITEMS)
    {
        uint64_t oldestTime = UINT64_MAX;
        std::string oldestKey;
        for (const auto& pair : g_prefixCache)
        {
            if (pair.second.createdAtMs < oldestTime)
            {
                oldestTime = pair.second.createdAtMs;
                oldestKey = pair.first;
            }
        }
        if (!oldestKey.empty())
        {
            g_prefixCache.erase(oldestKey);
        }
    }
    
    PrefixCacheEntry entry;
    entry.completion = completion;
    entry.planId = planId;
    entry.sessionId = sessionId;
    entry.fromAgentic = fromAgentic;
    entry.createdAtMs = nowMs();
    entry.lastHitMs = entry.createdAtMs;
    entry.hitCount = 0;
    entry.prefixHash = key;
    
    g_prefixCache[key] = entry;
}

// Try to retrieve a completion from the prefix cache
bool tryGetPrefixCacheEntry(const std::string& key, std::string& outCompletion,
                            std::string& outPlanId, std::string& outSessionId, bool& outFromAgentic)
{
    std::lock_guard<std::mutex> lock(g_prefixCacheMutex);
    
    auto it = g_prefixCache.find(key);
    if (it == g_prefixCache.end())
    {
        return false;
    }
    
    const uint64_t now = nowMs();
    if (now - it->second.createdAtMs > GHOST_TEXT_PREFIX_CACHE_TTL_MS)
    {
        // Entry expired
        g_prefixCache.erase(it);
        return false;
    }
    
    // Update hit statistics
    it->second.lastHitMs = now;
    it->second.hitCount++;
    
    outCompletion = it->second.completion;
    outPlanId = it->second.planId;
    outSessionId = it->second.sessionId;
    outFromAgentic = it->second.fromAgentic;
    
    return true;
}

bool tryGetPrefixContinuation(const std::string& filePath, const std::string& language, int lineIndex,
                              const std::string& linePrefix, std::string& outCompletion)
{
    std::lock_guard<std::mutex> lock(g_prefixCacheMutex);

    if (g_lastPrefetchCompletion.empty() || g_lastPrefetchLinePrefix.empty())
        return false;
    if (g_lastPrefetchFilePath != filePath || g_lastPrefetchLanguage != language ||
        g_lastPrefetchLineIndex != lineIndex)
        return false;
    if (linePrefix.size() <= g_lastPrefetchLinePrefix.size())
        return false;
    if (linePrefix.compare(0, g_lastPrefetchLinePrefix.size(), g_lastPrefetchLinePrefix) != 0)
        return false;

    const std::string typedDelta = linePrefix.substr(g_lastPrefetchLinePrefix.size());
    if (typedDelta.empty() || typedDelta.size() > g_lastPrefetchCompletion.size())
        return false;

    bool matches = (g_lastPrefetchCompletion.compare(0, typedDelta.size(), typedDelta) == 0);
    if (!matches)
    {
        matches = true;
        for (size_t i = 0; i < typedDelta.size(); ++i)
        {
            const unsigned char a = static_cast<unsigned char>(typedDelta[i]);
            const unsigned char b = static_cast<unsigned char>(g_lastPrefetchCompletion[i]);
            if (std::tolower(a) != std::tolower(b))
            {
                matches = false;
                break;
            }
        }
    }
    if (!matches)
        return false;

    outCompletion = g_lastPrefetchCompletion.substr(typedDelta.size());
    return !outCompletion.empty();
}

// Get prefix cache statistics
void getPrefixCacheStats(int& hits, int& misses, int& entries)
{
    std::lock_guard<std::mutex> lock(g_prefixCacheMutex);
    hits = g_speculativePrefetchHits.load();
    misses = g_speculativePrefetchMisses.load();
    entries = static_cast<int>(g_prefixCache.size());
}

std::string trimLeftCopy(const std::string& in)
{
    size_t i = 0;
    while (i < in.size() && std::isspace(static_cast<unsigned char>(in[i])) != 0)
        ++i;
    return in.substr(i);
}

std::string lowerCopy(const std::string& in)
{
    std::string out = in;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string getLinePrefix(const std::string& context)
{
    const size_t pos = context.find_last_of('\n');
    if (pos == std::string::npos)
        return context;
    if (pos + 1 >= context.size())
        return "";
    return context.substr(pos + 1);
}

std::string buildSnippetCompletion(const std::string& context, const std::string& language)
{
    const std::string line = trimLeftCopy(getLinePrefix(context));
    const std::string lowered = lowerCopy(line);
    const std::string lang = lowerCopy(language);

    if (lowered == "if")
    {
        return " () {\n    \n}";
    }
    if (lowered == "for")
    {
        return " (int i = 0; i < ; ++i) {\n    \n}";
    }
    if (lowered == "while")
    {
        return " () {\n    \n}";
    }
    if ((lang == "c++" || lang == "cpp" || lang == "c") && lowered == "switch")
    {
        return " () {\ncase :\n    break;\ndefault:\n    break;\n}";
    }
    return "";
}

std::string wideToUtf8Local(const wchar_t* wide)
{
    if (!wide || !*wide)
    {
        return "";
    }

    const int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 1)
    {
        return "";
    }

    std::string utf8(static_cast<size_t>(utf8Len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), utf8Len, nullptr, nullptr);
    return utf8;
}

std::string getEditorRangeUtf8(HWND editor, LONG cpMin, LONG cpMax)
{
    if (!editor || cpMax <= cpMin)
    {
        return "";
    }

    const LONG charLen = cpMax - cpMin;
    std::vector<wchar_t> buffer(static_cast<size_t>(charLen) + 1, L'\0');
    TEXTRANGEW tr{};
    tr.chrg.cpMin = cpMin;
    tr.chrg.cpMax = cpMax;
    tr.lpstrText = buffer.data();
    SendMessageW(editor, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    buffer[static_cast<size_t>(charLen)] = L'\0';
    return wideToUtf8Local(buffer.data());
}

void applyEditorRangeColor(HWND editor, LONG cpMin, LONG cpMax, COLORREF color)
{
    if (!editor || cpMax <= cpMin)
    {
        return;
    }

    CHARRANGE range{};
    range.cpMin = cpMin;
    range.cpMax = cpMax;
    SendMessageA(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));

    CHARFORMAT2A cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    SendMessageA(editor, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

    CHARRANGE restore{};
    restore.cpMin = cpMax;
    restore.cpMax = cpMax;
    SendMessageA(editor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&restore));
}
}  // namespace

// ============================================================================
// INITIALIZATION
// ============================================================================

void Win32IDE::initGhostText()
{
    // Predictive Engine Init
    m_predictiveGhostText = std::make_unique<RawrXD::IDE::PredictiveGhostText>();

    // Ghost text enabled by default (heap corruption fixed in render path).
    m_ghostTextEnabled = true;
    m_ghostTextVisible = false;
    m_ghostTextAccepted = false;
    m_ghostTextPending = false;
    m_ghostTextContent.clear();
    m_ghostTextCommitContent.clear();
    m_ghostTextLine = -1;
    m_ghostTextColumn = -1;
    m_ghostTextPlanId.clear();
    m_ghostTextSessionId.clear();
    m_ghostTextPendingPlanId.clear();
    m_ghostTextPendingSessionId.clear();
    m_ghostTextBuffer.clear();
    m_ghostTextCommittedPrefix = 0;
    m_pendingGhostAppend.clear();
    m_ghostTextRenderScheduled = false;
    m_ghostTextStreamingActive = false;
    m_ghostTextFromAgentic = false;
    m_ghostTextPendingFromAgentic = false;
    m_ghostTextFont = nullptr;
    m_activeSuggestionContext = {};

    // GhostTextRenderer overlay wiring is handled by the renderer implementation
    // when/if it is instantiated. Keep ghost text lane functional without it.

    // Create ghost text font — italic version of editor font (DPI-scaled)
    LOGFONTA lf = {};
    lf.lfHeight = -dpiScale(14);  // ~10.5pt at 96 DPI
    lf.lfWeight = FW_NORMAL;
    lf.lfItalic = TRUE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    strncpy(lf.lfFaceName, m_currentTheme.fontName.c_str(), LF_FACESIZE - 1);
    lf.lfFaceName[LF_FACESIZE - 1] = '\0';
    m_ghostTextFont = CreateFontIndirectA(&lf);

    LOG_INFO("Ghost text renderer initialized (debounce=" + std::to_string(GHOST_TEXT_DELAY_MS) +
             "ms, default=enabled)");
}

void Win32IDE::shutdownGhostText()
{
    cancelGhostTextInferenceRequests();
    {
        std::lock_guard<std::mutex> lock(m_titanGhostMutex);
        m_titanGhostStreamText.clear();
        m_titanGhostStreamActive = false;
    }
    dismissGhostText();
    destroyGhostDiffOverlayUi();
    if (m_ghostTextFont)
    {
        DeleteObject(m_ghostTextFont);
        m_ghostTextFont = nullptr;
    }
}

void Win32IDE::cancelGhostTextInferenceRequests()
{
    cancelAllGhostInferenceRequestTokens();
    ++m_ghostTextRequestSeq;
    g_speculativePrefetchGeneration.fetch_add(1);
    g_speculativePrefetchInProgress.store(false);

    if (m_hwndMain)
    {
        KillTimer(m_hwndMain, GHOST_TEXT_TIMER_ID);
        KillTimer(m_hwndMain, GHOST_TEXT_SPECULATIVE_TIMER_ID);
        KillTimer(m_hwndMain, GHOST_TEXT_RENDER_TIMER_ID);
    }

    if (m_predictionProvider)
    {
        m_predictionProvider->Cancel();
    }

    cancelActiveTitanGhostInference();
    m_ghostTextBuffer.clear();
    m_ghostTextCommittedPrefix = 0;
    m_pendingGhostAppend.clear();
    m_ghostTextRenderScheduled = false;
    m_ghostTextStreamingActive = false;
    m_ghostTextPending = false;
}

// ============================================================================
// DEBOUNCE TRIGGER — called from onEditorContentChanged()
// ============================================================================

void Win32IDE::triggerGhostTextCompletion()
{
    if (!m_ghostTextEnabled || !m_hwndEditor)
        return;

    // Cancel any in-flight work and clear current ghost text before scheduling
    // fresh high-priority input. New keystrokes always supersede stale model work.
    dismissGhostText();

    // Start a new debounce timer
    SetTimer(m_hwndMain, GHOST_TEXT_TIMER_ID, GHOST_TEXT_DELAY_MS, nullptr);

    // Arm speculative prefetch idle timer — fires earlier than the main
    // debounce to populate the prefix cache before the user finishes typing.
    // Self-guards via m_ghostTextPending / m_ghostTextVisible / atomic flag.
    KillTimer(m_hwndMain, GHOST_TEXT_SPECULATIVE_TIMER_ID);
    SetTimer(m_hwndMain, GHOST_TEXT_SPECULATIVE_TIMER_ID, GHOST_TEXT_SPECULATIVE_IDLE_MS, nullptr);
}

// ============================================================================
// SPECULATIVE PREFETCH — triggers on idle cursor before user types
// ============================================================================

void Win32IDE::triggerSpeculativePrefetch()
{
    if (!m_ghostTextEnabled || !m_hwndEditor || m_ghostTextPending)
        return;
    
    // Don't prefetch if we already have visible ghost text
    if (m_ghostTextVisible)
        return;
    
    // Don't prefetch if a speculative request is already in progress.
    bool expectedIdle = false;
    if (!g_speculativePrefetchInProgress.compare_exchange_strong(expectedIdle, true))
        return;
    
    // Gather context for speculative prefetch
    CHARRANGE sel;
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    if (sel.cpMin != sel.cpMax)
    {
        g_speculativePrefetchInProgress.store(false);
        return;
    }
    int cursorPos = sel.cpMin;
    
    if (cursorPos <= 0)
    {
        g_speculativePrefetchInProgress.store(false);
        return;
    }
    
    // Get context
    int contextStart = (cursorPos > 4096) ? cursorPos - 4096 : 0;
    std::string context = getEditorRangeUtf8(m_hwndEditor, contextStart, cursorPos);
    if (context.empty())
    {
        g_speculativePrefetchInProgress.store(false);
        return;
    }
    
    const std::string linePrefix = getLinePrefix(context);
    if (linePrefix.size() < 5)
    {
        g_speculativePrefetchInProgress.store(false);
        return;
    }
    
    // Check prefix cache first - if we have a cached completion for this prefix, use it
    std::string language = getSyntaxLanguageName();
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, cursorPos, 0);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column = cursorPos - lineStart;
    
    int textLen = GetWindowTextLengthW(m_hwndEditor);
    int suffixEnd = (cursorPos + 2048 < textLen) ? cursorPos + 2048 : textLen;
    std::string suffix = getEditorRangeUtf8(m_hwndEditor, cursorPos, suffixEnd);
    
    std::string cacheKey = buildGhostCacheKey(m_currentFile, language, context, suffix, lineIndex, column);
    
    // Try prefix cache first (longer TTL, instant retrieval)
    std::string cachedCompletion, cachedPlanId, cachedSessionId;
    bool cachedFromAgentic;
    if (tryGetPrefixCacheEntry(cacheKey, cachedCompletion, cachedPlanId, cachedSessionId, cachedFromAgentic))
    {
        // Prefix cache hit - instant ghost text without model inference
        g_speculativePrefetchHits++;
        g_speculativePrefetchInProgress.store(false);
        m_ghostTextContent = trimGhostText(cachedCompletion);
        m_ghostTextCommitContent = m_ghostTextContent;
        m_ghostTextLine = lineIndex;
        m_ghostTextColumn = column;
        m_ghostTextRequestCursorPos = cursorPos;
        m_ghostTextSelectionSnapshot = MakeCollapsedSelectionSnapshot(static_cast<LONG>(cursorPos));
        m_ghostTextRequestLinePrefix = linePrefix;
        m_ghostTextVisible = true;
        m_ghostTextAccepted = false;
        m_ghostTextPlanId = cachedPlanId;
        m_ghostTextSessionId = cachedSessionId;
        m_ghostTextFromAgentic = cachedFromAgentic;
        m_activeSuggestionContext.range.cpMin = cursorPos;
        m_activeSuggestionContext.range.cpMax = cursorPos + static_cast<LONG>(m_ghostTextContent.size());
        m_activeSuggestionContext.state = SuggestionState::Pending;
        m_activeSuggestionContext.preview = m_ghostTextContent;
        m_activeSuggestionContext.stateChangedTickMs = static_cast<uint64_t>(GetTickCount64());
        
        if (m_hwndEditor)
        {
            InvalidateRect(m_hwndEditor, nullptr, FALSE);
            UpdateWindow(m_hwndEditor);
        }
        
        recordEvent(AgentEventType::GhostTextRequested, "", "speculative_prefetch_cache_hit", "", 0, true);
        return;
    }

    if (tryGetPrefixContinuation(m_currentFile, language, lineIndex, linePrefix, cachedCompletion))
    {
        g_speculativePrefetchHits++;
        g_speculativePrefetchInProgress.store(false);
        m_ghostTextContent = trimGhostText(cachedCompletion);
        m_ghostTextCommitContent = m_ghostTextContent;
        m_ghostTextLine = lineIndex;
        m_ghostTextColumn = column;
        m_ghostTextRequestCursorPos = cursorPos;
        m_ghostTextSelectionSnapshot = MakeCollapsedSelectionSnapshot(static_cast<LONG>(cursorPos));
        m_ghostTextRequestLinePrefix = linePrefix;
        m_ghostTextVisible = true;
        m_ghostTextAccepted = false;
        m_activeSuggestionContext.range.cpMin = cursorPos;
        m_activeSuggestionContext.range.cpMax = cursorPos + static_cast<LONG>(m_ghostTextContent.size());
        m_activeSuggestionContext.state = SuggestionState::Pending;
        m_activeSuggestionContext.preview = m_ghostTextContent;
        m_activeSuggestionContext.stateChangedTickMs = static_cast<uint64_t>(GetTickCount64());

        InvalidateRect(m_hwndEditor, nullptr, FALSE);
        UpdateWindow(m_hwndEditor);
        recordEvent(AgentEventType::GhostTextRequested, "", "speculative_prefix_continuation_hit", "", 0, true);
        return;
    }
    
    // No cache hit - check if we should speculatively prefetch
    // Only prefetch when at least one real provider can service the request.
    const bool hasTitan = m_useTitanKernel && !m_loadedModelPath.empty();
    const bool hasPrediction = (m_predictionProvider != nullptr) || !m_ollamaBaseUrl.empty();
    const bool hasNative = m_nativeEngine && m_nativeEngine->IsModelLoaded();
    const bool hasAgentic = rawrxd::isAgenticLayerAvailable();
    if (!hasTitan && !hasPrediction && !hasNative && !hasAgentic)
    {
        g_speculativePrefetchInProgress.store(false);
        return;
    }
    
    g_speculativePrefetchMisses++;
    g_speculativePrefetchKey = cacheKey;
    const uint64_t requestSeq = m_ghostTextRequestSeq.load();
    const uint64_t prefetchGeneration = g_speculativePrefetchGeneration.load();
    
    // Fire speculative prefetch in background
    std::string contextCopy = BuildGhostPromptContext(context, linePrefix);
    std::string suffixCopy = suffix;
    std::string langCopy = language;
    std::string fileCopy = m_currentFile;
    int cursorCopy = cursorPos;
    int lineCopy = lineIndex;
    int colCopy = column;
    std::string linePrefixCopy = linePrefix;
    auto requestToken = createGhostInferenceRequestToken(requestSeq);
    
    std::thread(
        [this, contextCopy, suffixCopy, langCopy, fileCopy, cursorCopy, lineCopy, colCopy, cacheKey,
         linePrefixCopy, requestSeq, prefetchGeneration, requestToken]()
        {
            ScopedGhostInferenceRequest requestScope(requestToken);
            DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
            if (_guard.cancelled || isGhostInferenceRequestCancelled(requestToken))
            {
                if (g_speculativePrefetchGeneration.load() == prefetchGeneration)
                    g_speculativePrefetchInProgress.store(false);
                return;
            }
            
            // Request completion
            GhostTextCacheEntry suggestion =
                requestGhostTextCompletion(contextCopy, langCopy, suffixCopy, fileCopy, lineCopy, colCopy, requestSeq);
            
            if (!isGhostInferenceRequestCancelled(requestToken) &&
                g_speculativePrefetchGeneration.load() == prefetchGeneration &&
                requestSeq == m_ghostTextRequestSeq.load() && !suggestion.completion.empty())
            {
                // Store in prefix cache for instant retrieval
                storePrefixCacheEntry(cacheKey, suggestion.completion, suggestion.planId, suggestion.sessionId, suggestion.fromAgentic);
                {
                    std::lock_guard<std::mutex> lock(g_prefixCacheMutex);
                    g_lastPrefetchKey = cacheKey;
                    g_lastPrefetchCompletion = suggestion.completion;
                    g_lastPrefetchLinePrefix = linePrefixCopy;
                    g_lastPrefetchFilePath = fileCopy;
                    g_lastPrefetchLanguage = langCopy;
                    g_lastPrefetchLineIndex = lineCopy;
                }
            }
            
            if (g_speculativePrefetchGeneration.load() == prefetchGeneration)
                g_speculativePrefetchInProgress.store(false);
        }).detach();
}

// ============================================================================
// IDLE PREFETCH TIMER — fires 150ms after last keystroke to populate prefix cache
// ============================================================================

void Win32IDE::onPrefetchIdleTimer()
{
    // One-shot — disarm immediately so we don't fire repeatedly.
    KillTimer(m_hwndMain, GHOST_TEXT_SPECULATIVE_TIMER_ID);

    // Skip if main ghost-text request already won the race or is in flight.
    if (!m_ghostTextEnabled || !m_hwndEditor)
        return;
    if (m_ghostTextPending || m_ghostTextVisible)
        return;
    if (g_speculativePrefetchInProgress.load())
        return;

    // Hand off to the existing speculative lane (cache hit -> instant paint,
    // miss -> background inference into prefix cache for the next keystroke).
    triggerSpeculativePrefetch();
}

// ============================================================================
// TIMER CALLBACK — fires after debounce period, requests completion
// ============================================================================

void Win32IDE::onGhostTextTimer()
{
    KillTimer(m_hwndMain, GHOST_TEXT_TIMER_ID);

    if (!m_ghostTextEnabled || !m_hwndEditor)
        return;
    if (m_ghostTextPending)
        return;  // Already requesting

    // Gather context: text before cursor
    CHARRANGE sel;
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    if (sel.cpMin != sel.cpMax)
        return;
    int cursorPos = sel.cpMin;

    if (cursorPos <= 0)
        return;

    // Get up to 4KB of text before cursor for context
    int contextStart = (cursorPos > 4096) ? cursorPos - 4096 : 0;

    std::string context = getEditorRangeUtf8(m_hwndEditor, contextStart, cursorPos);
    if (context.empty())
        return;

    // Keep a same-line prefix snapshot so stale async responses cannot paint over a moved/changed caret context.
    const std::string linePrefix = getLinePrefix(context);
    if (linePrefix.size() < 5)
    {
        dismissGhostText();
        return;
    }

    static DWORD lastSemantic = 0;
    const DWORD nowTick = GetTickCount();
    if ((nowTick - lastSemantic) < 250)
    {
        return;
    }
    lastSemantic = nowTick;

    // Semantic bridge: enrich the prompt context with nearest indexed code context.
    context = BuildGhostPromptContext(context, linePrefix);
    m_ghostTextRequestLinePrefix = linePrefix;

    // Get current line/column for positioning
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, cursorPos, 0);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column = cursorPos - lineStart;

    // Store cursor position for ghost text rendering
    m_ghostTextLine = lineIndex;
    m_ghostTextColumn = column;
    m_ghostTextRequestCursorPos = cursorPos;
    m_ghostTextSelectionSnapshot = MakeCollapsedSelectionSnapshot(static_cast<LONG>(cursorPos));
    m_ghostTextPending = true;
    const uint64_t requestSeq = m_ghostTextRequestSeq.load();

    {
        std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
        m_ghostTextMetrics.requests++;
    }

    // Detect language for context
    std::string language = getSyntaxLanguageName();

    // Gather suffix context (text after cursor, up to 2KB) for FIM
    int textLen = GetWindowTextLengthW(m_hwndEditor);
    int suffixEnd = (cursorPos + 2048 < textLen) ? cursorPos + 2048 : textLen;
    int suffixLen = suffixEnd - cursorPos;
    std::string suffix;
    if (suffixLen > 0)
    {
        suffix = getEditorRangeUtf8(m_hwndEditor, cursorPos, suffixEnd);
    }

    // Fire background thread for completion
    std::string contextCopy = context;
    std::string suffixCopy = suffix;
    std::string langCopy = language;
    std::string fileCopy = m_currentFile;
    int cursorCopy = cursorPos;
    int lineCopy = lineIndex;
    int colCopy = column;
    std::string cacheKey = buildGhostCacheKey(fileCopy, langCopy, contextCopy, suffixCopy, lineCopy, colCopy);

    {
        std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
        auto it = m_ghostTextCache.find(cacheKey);
        if (it != m_ghostTextCache.end() && (nowMs() - it->second.createdAtMs) <= GHOST_TEXT_CACHE_TTL_MS)
        {
            m_ghostTextMetrics.cacheHits++;
            m_ghostTextPending = false;
            m_ghostTextPendingPlanId = it->second.planId;
            m_ghostTextPendingSessionId = it->second.sessionId;
            m_ghostTextPendingFromAgentic = it->second.fromAgentic;
            onGhostTextReady(cursorCopy, it->second.completion.c_str());
            return;
        }
    }

    auto requestToken = createGhostInferenceRequestToken(requestSeq);

    // Start the streaming time model now. The surface is marked visible with
    // an empty buffer so the first token is allowed through the UI-thread
    // sequence/cursor guards, but rendering still no-ops until text arrives.
    m_ghostTextVisible = true;
    m_ghostTextAccepted = false;
    m_ghostTextStreamingActive = true;
    m_ghostTextStreamSessionId = requestSeq;
    m_ghostTextBuffer.clear();
    m_ghostTextCommittedPrefix = m_ghostTextRequestLinePrefix.size();
    m_pendingGhostAppend.clear();
    m_ghostTextRenderScheduled = false;
    m_ghostTextContent.clear();
    m_ghostTextCommitContent.clear();
    m_activeSuggestionContext.range.cpMin = cursorPos;
    m_activeSuggestionContext.range.cpMax = cursorPos;
    m_activeSuggestionContext.state = SuggestionState::Pending;
    m_activeSuggestionContext.preview.clear();
    m_activeSuggestionContext.stateChangedTickMs = static_cast<uint64_t>(GetTickCount64());

    std::thread(
        [this, contextCopy, suffixCopy, langCopy, fileCopy, cursorCopy, lineCopy, colCopy, requestSeq, requestToken]()
        {
            ScopedGhostInferenceRequest requestScope(requestToken);
            DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
            if (_guard.cancelled || isGhostInferenceRequestCancelled(requestToken))
                return;
            const uint64_t startedAt = nowMs();
            GhostTextCacheEntry suggestion =
                requestGhostTextCompletion(contextCopy, langCopy, suffixCopy, fileCopy, lineCopy, colCopy, requestSeq,
                                           true);
            std::string completion = suggestion.completion;
            const uint64_t elapsedMs = nowMs() - startedAt;

            if (isGhostInferenceRequestCancelled(requestToken))
                return;

            if (requestSeq != m_ghostTextRequestSeq.load())
            {
                std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                m_ghostTextMetrics.staleDrops++;
                return;
            }

            if (!completion.empty())
            {
                std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                if (m_ghostTextCache.size() >= GHOST_TEXT_CACHE_MAX_ITEMS)
                {
                    m_ghostTextCache.erase(m_ghostTextCache.begin());
                }
                const std::string key =
                    buildGhostCacheKey(fileCopy, langCopy, contextCopy, suffixCopy, lineCopy, colCopy);
                m_ghostTextPendingPlanId = suggestion.planId;
                m_ghostTextPendingSessionId = suggestion.sessionId;
                m_ghostTextPendingFromAgentic = suggestion.fromAgentic;
                m_ghostTextCache[key] = GhostTextCacheEntry{completion, suggestion.planId, suggestion.sessionId,
                                                            suggestion.fromAgentic, nowMs()};
                m_ghostTextMetrics.lastLatencyMs = (double)elapsedMs;
                const double reqCount =
                    (double)std::max<uint64_t>(1, m_ghostTextMetrics.requests - m_ghostTextMetrics.cacheHits);
                m_ghostTextMetrics.avgLatencyMs =
                    ((m_ghostTextMetrics.avgLatencyMs * (reqCount - 1.0)) + (double)elapsedMs) / reqCount;
            }

            // Flight Recorder telemetry: update speculative diagnostic gauges for UI status/overlay.
            {
                struct LocalDiagnosticFrame {
                    double total_ms = 0.0;
                    int tokens_produced = 0;
                    float acceptance_rate = 0.0f;
                    float draft_latency_ms = 0.0f;
                    float verify_latency_ms = 0.0f;
                    int expert_id = -1;
                };
                LocalDiagnosticFrame frame{};
                frame.total_ms = static_cast<double>(elapsedMs);
                frame.tokens_produced = static_cast<int>(std::max<size_t>(
                    1, completion.empty() ? 0 : (completion.size() / static_cast<size_t>(4))));
                frame.expert_id = -1;

                if (m_speculativeEngine)
                {
                    // TODO: Wire to actual speculative engine stats when available
                    frame.acceptance_rate = 0.85f;
                    frame.draft_latency_ms = static_cast<float>(elapsedMs) * 0.3f;
                    frame.verify_latency_ms = static_cast<float>(elapsedMs) * 0.7f;
                }

                const double effectiveTps =
                    (frame.total_ms > 0.0) ? (static_cast<double>(frame.tokens_produced) / (frame.total_ms / 1000.0)) : 0.0;
                double baselineTps = 20.0;
                const double roi = (effectiveTps > 0.0) ? (effectiveTps / baselineTps) : 0.0;

                METRICS.gauge("spec.telemetry.acceptance_rate", static_cast<double>(frame.acceptance_rate));
                METRICS.gauge("spec.telemetry.tokens_produced", static_cast<double>(frame.tokens_produced));
                METRICS.gauge("spec.telemetry.draft_latency_ms", frame.draft_latency_ms);
                METRICS.gauge("spec.telemetry.verify_latency_ms", frame.verify_latency_ms);
                METRICS.gauge("spec.telemetry.total_ms", frame.total_ms);
                METRICS.gauge("spec.telemetry.expert_id", static_cast<double>(frame.expert_id));
                METRICS.gauge("spec.telemetry.effective_tps", effectiveTps);
                METRICS.gauge("spec.telemetry.roi", roi);

                if (m_hwndMain && IsWindow(m_hwndMain))
                {
                    PostMessageW(m_hwndMain, WM_STATUSBAR_REFRESH_COPILOT, 0, 0);
                }
            }

            // Post result to UI thread
            if (isShuttingDown() || isGhostInferenceRequestCancelled(requestToken))
                return;
            (void)cursorCopy;
            PostGhostCompleteMessage(m_hwndMain, requestSeq, completion);
        })
        .detach();
}

// ============================================================================
// COMPLETION REQUEST — runs on background thread
// ============================================================================

Win32IDE::GhostTextCacheEntry Win32IDE::requestGhostTextCompletion(const std::string& context,
                                                                   const std::string& language)
{
    // Legacy 2-arg overload — forward to FIM-aware version with empty suffix
    return requestGhostTextCompletion(context, language, "", "", 0, 0, 0, false);
}

Win32IDE::GhostTextCacheEntry Win32IDE::requestGhostTextCompletion(const std::string& context,
                                                                   const std::string& language,
                                                                   const std::string& suffix,
                                                                   const std::string& filePath, int cursorLine,
                                                                   int cursorCol, uint64_t expectedSeq,
                                                                   bool streamToUi)
{
    using namespace RawrXD::Prediction;

    // Total deadline across all providers — cap at 8 seconds to prevent
    // the cascade from blocking a background thread for minutes.
    const auto cascadeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);

    const auto isStale = [this, expectedSeq]() -> bool
    { return expectedSeq != 0 && expectedSeq != m_ghostTextRequestSeq.load(); };

    const auto isPastDeadline = [&cascadeDeadline]() -> bool
    { return std::chrono::steady_clock::now() >= cascadeDeadline; };

    enum class GhostProviderKind
    {
        SymbolBridge,  // AST scope-aware candidates (fastest, deterministic)
        Titan,
        Agentic,
        Local,
        Snippet,
        Lsp
    };
    const std::array<GhostProviderKind, 6> precedence = {GhostProviderKind::SymbolBridge, GhostProviderKind::Titan,
                                                         GhostProviderKind::Agentic, GhostProviderKind::Local,
                                                         GhostProviderKind::Snippet, GhostProviderKind::Lsp};

    auto logProviderSkip = [this](const std::string& msg)
    {
        static std::atomic<uint64_t> s_lastLogMs{0};
        const uint64_t now = nowMs();
        const uint64_t prev = s_lastLogMs.load();
        if (now > prev && (now - prev) < GHOST_TEXT_PROVIDER_LOG_TTL_MS)
        {
            return;
        }
        s_lastLogMs.store(now);
        // This runs on a background thread; use a safe output surface.
        postOutputPanelSafe("[Ghost Text] " + msg + "\n");
    };

    std::string lastReason;
    
    // ============================================================================
    // SKILL INJECTION: Enrich prompt with active skill context
    // ============================================================================
    // CRITICAL: First 520 lines of skill context are ALWAYS injected
    // regardless of model/agent status. This provides Cursor-style
    // .cursorrules / system prompt injection for sovereign IDE.
    std::string skillEnrichedContext = RawrXD::SkillSystem::Hook_GhostText_CompletionRequest(
        context, filePath, cursorLine, cursorCol
    );
    
    // Use skill-enriched context for all provider requests
    const std::string& providerContext = skillEnrichedContext.empty() ? context : skillEnrichedContext;
    
    for (GhostProviderKind provider : precedence)
    {
        if (isStale() || isPastDeadline())
            return {};

        if (provider == GhostProviderKind::SymbolBridge)
        {
            // Fast path: AST scope-aware candidates from SymbolIndexBridge
            if (m_symbolBridgeReady && !m_symbolBridgeCandidates.empty())
            {
                // Find best matching candidate based on trigger prefix
                const std::string& prefix = m_symbolBridgeTrigger.prefix;
                std::string bestMatch;
                int bestScore = -1;
                
                for (const auto& candidate : m_symbolBridgeCandidates)
                {
                    int score = 0;
                    if (candidate.name.rfind(prefix, 0) == 0)
                        score += 100;  // Prefix match
                    if (candidate.kind == rawrxd::bridge::SymbolKind::Function)
                        score += 10;   // Prefer functions
                    if (candidate.kind == rawrxd::bridge::SymbolKind::Method)
                        score += 10;   // Prefer methods
                    if (candidate.accessibility == rawrxd::bridge::Accessibility::Public)
                        score += 5;    // Prefer public
                    
                    if (score > bestScore)
                    {
                        bestScore = score;
                        bestMatch = candidate.name;
                        
                        // Append signature for functions/methods
                        if (!candidate.signature.empty() && 
                            (candidate.kind == rawrxd::bridge::SymbolKind::Function ||
                             candidate.kind == rawrxd::bridge::SymbolKind::Method))
                        {
                            bestMatch += candidate.signature;
                        }
                    }
                }
                
                if (!bestMatch.empty())
                {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.localWins++;
                    return GhostTextCacheEntry{bestMatch, "", "", false, 0};
                }
            }
            lastReason = "SymbolBridge: no candidates or not ready";
        }

        if (provider == GhostProviderKind::Titan)
        {
            if (!m_useTitanKernel)
            {
                lastReason = "Titan provider disabled (m_useTitanKernel=false)";
                continue;
            }
            if (m_loadedModelPath.empty())
            {
                lastReason = "Titan provider skipped (no loaded model path)";
                continue;
            }
            std::string titanCompletion = requestTitanGhostTextCompletion(providerContext, language, suffix, filePath,
                                                                          cursorLine, cursorCol, expectedSeq,
                                                                          streamToUi);
            if (!titanCompletion.empty())
            {
                std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                m_ghostTextMetrics.localWins++;
                return GhostTextCacheEntry{titanCompletion, "", "", false, 0};
            }
            lastReason = "Titan provider returned empty completion";
        }

        if (provider == GhostProviderKind::Agentic)
        {
            if (!rawrxd::isAgenticLayerAvailable())
            {
                lastReason = "Agentic provider unavailable";
                continue;
            }

            std::string workspaceRoot = m_projectRoot;
            if (workspaceRoot.empty())
            {
                workspaceRoot = m_explorerRootPath;
            }
            if (workspaceRoot.empty())
            {
                workspaceRoot = m_currentDirectory;
            }

            std::string resolvedFilePath = filePath.empty() ? m_currentFile : filePath;
            if (workspaceRoot.empty() && !resolvedFilePath.empty())
            {
                const size_t slash = resolvedFilePath.find_last_of("\\/");
                if (slash != std::string::npos)
                {
                    workspaceRoot = resolvedFilePath.substr(0, slash);
                }
            }
            if (workspaceRoot.empty())
            {
                workspaceRoot = ".";
            }

            rawrxd::EditorContext editorContext;
            editorContext.workspaceRoot = workspaceRoot;
            editorContext.filePath = resolvedFilePath;
            editorContext.language = language;
            editorContext.modelPath = m_loadedModelPath;
            editorContext.cursorLine = cursorLine;
            editorContext.cursorColumn = cursorCol;

            const size_t prefixWindow = std::min<size_t>(providerContext.size(), 1536);
            const std::string promptPrefix = providerContext.substr(providerContext.size() - prefixWindow, prefixWindow);
            const rawrxd::CompletionResult result = rawrxd::requestInlineCompletion(promptPrefix, editorContext);
            const std::string bridged = trimGhostText(result.suggestion);
            if (result.success && !bridged.empty())
            {
                std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                m_ghostTextMetrics.localWins++;
                return GhostTextCacheEntry{bridged, result.planId, result.sessionId, !result.planId.empty(), 0};
            }
            lastReason = result.success ? "Agentic provider returned empty suggestion" : "Agentic provider failed";
        }

        if (provider == GhostProviderKind::Local)
        {
            // ---- Primary: OrchestratorBridge FIM (uses NativeInferenceClient + FIMPromptBuilder) ----
            {
                auto& orchBridge = RawrXD::Agent::OrchestratorBridge::Instance();
                RawrXD::Prediction::PredictionContext bCtx;
                bCtx.prefix = context;
                bCtx.suffix = suffix;
                bCtx.language = language;
                bCtx.filePath = filePath;
                bCtx.cursorLine = cursorLine;
                bCtx.cursorColumn = cursorCol;

                auto bResult = orchBridge.RequestGhostText(bCtx);
                if (bResult.success && !bResult.completion.empty())
                {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.localWins++;
                    return GhostTextCacheEntry{trimGhostText(bResult.completion), "", "", false, 0};
                }
                lastReason =
                    bResult.success ? "OrchestratorBridge returned empty completion" : "OrchestratorBridge failed";
            }

            if (isStale())
                return {};

            // ---- Fallback: prediction backend, then native model, then local Ollama prompt ----
            if (!m_predictionProvider)
            {
                std::string baseUrl = m_ollamaBaseUrl.empty() ? GHOST_TEXT_DEFAULT_OLLAMA_URL : m_ollamaBaseUrl;
                m_predictionProvider =
                    std::unique_ptr<RawrXD::Prediction::NativeStreamProvider, NativeStreamProviderDeleter>(
                        new RawrXD::Prediction::OllamaProvider(baseUrl));

                PredictionConfig cfg;
                cfg.model = getResolvedOllamaModel().empty() ? "qwen2.5-coder:14b" : getResolvedOllamaModel();
                cfg.temperature = 0.2f;
                cfg.maxTokens = 256;
                cfg.maxLines = GHOST_TEXT_MAX_LINES;
                cfg.useFIM = true;
                cfg.stopSequences = "<|endoftext|>,<|fim_pad|>,\n\n\n";
                m_predictionProvider->Configure(cfg);
            }

            if (m_predictionProvider->IsAvailable())
            {
                PredictionContext ctx;
                ctx.prefix = context;
                ctx.suffix = suffix;
                ctx.language = language;
                ctx.filePath = filePath;
                ctx.cursorLine = cursorLine;
                ctx.cursorColumn = cursorCol;

                std::string streamedCompletion;
                bool sawStreamToken = false;
                m_predictionProvider->PredictStreaming(
                    ctx,
                    [this, expectedSeq, streamToUi, &streamedCompletion, &sawStreamToken](const std::string& token,
                                                                                          bool /*isFinal*/) -> bool
                    {
                        if (expectedSeq != 0 && expectedSeq != m_ghostTextRequestSeq.load())
                        {
                            return false;
                        }
                        if (m_ghostTextAccepted || !m_ghostTextEnabled)
                        {
                            return false;
                        }

                        if (!token.empty())
                        {
                            sawStreamToken = true;
                            streamedCompletion += token;
                            if (streamToUi && expectedSeq != 0)
                            {
                                PostGhostTokenMessage(m_hwndMain, expectedSeq, token);
                            }
                        }
                        return true;
                    });

                const std::string trimmedStreamedCompletion = trimGhostText(streamedCompletion);
                if (!trimmedStreamedCompletion.empty())
                {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.localWins++;
                    return GhostTextCacheEntry{trimmedStreamedCompletion, "", "", false, 0};
                }

                lastReason = sawStreamToken ? "Ollama provider streamed only whitespace"
                                            : "Ollama provider stream returned no completion";
            }
            else
            {
                lastReason = "Ollama provider not available (connection check failed)";
            }

            if (isStale())
                return {};

            if (m_nativeEngine && m_nativeEngine->IsModelLoaded())
            {
                auto tokens = m_nativeEngine->Tokenize("Complete the following " + language +
                                                       " code. Output ONLY the completion, "
                                                       "no explanation, no markdown:\n\n" +
                                                       context);

                auto generated = m_nativeEngine->Generate(tokens, 64);
                std::string result = trimGhostText(m_nativeEngine->Detokenize(generated));
                if (!result.empty())
                {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.localWins++;
                    return GhostTextCacheEntry{result, "", "", false, 0};
                }
                lastReason = "Native engine produced empty completion";
            }

            if (isStale())
                return {};

            if (!m_ollamaBaseUrl.empty())
            {
                std::string response;
                std::string prompt = "Complete the following " + language +
                                     " code. "
                                     "Output ONLY the completion, no explanation, no markdown. "
                                     "Maximum 3 lines:\n\n" +
                                     context;
                if (trySendToOllama(prompt, response))
                {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.localWins++;
                    return GhostTextCacheEntry{trimGhostText(response), "", "", false, 0};
                }
                lastReason = "Raw Ollama prompt fallback failed";
            }
        }

        if (provider == GhostProviderKind::Snippet)
        {
            const std::string snippet = trimGhostText(buildSnippetCompletion(context, language));
            if (!snippet.empty())
            {
                std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                m_ghostTextMetrics.snippetWins++;
                return GhostTextCacheEntry{snippet, "", "", false, 0};
            }
            lastReason = "Snippet provider returned empty";
        }

        if (provider == GhostProviderKind::Lsp)
        {
            if (filePath.empty())
                continue;
            const int lspLine = cursorLine > 0 ? cursorLine - 1 : 0;

            // Fast path: direct LSP completion for current cursor.
            std::string uri = filePathToUri(filePath);
            auto lspItems = lspCompletion(uri, lspLine, cursorCol);
            for (const auto& item : lspItems)
            {
                if (item.insertText.empty())
                    continue;
                std::string lspText = trimGhostText(item.insertText);
                if (!lspText.empty())
                {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.lspWins++;
                    return GhostTextCacheEntry{lspText, "", "", false, 0};
                }
            }

            // Fallback: hybrid merge if direct LSP did not yield usable insert text.
            auto items = requestHybridCompletion(filePath, lspLine, cursorCol);
            
            // ── BRIDGE WIRING: Store all candidates for ranked rendering ──────────
            {
                std::lock_guard<std::mutex> lock(m_completionCandidatesMutex);
                m_completionCandidates = items;
                m_completionSelectedIndex = 0; // Default to top-ranked
            }
            
            for (const auto& item : items)
            {
                if (item.insertText.empty())
                    continue;
                if (item.source != "lsp" && item.source != "merged")
                    continue;
                std::string lspText = trimGhostText(item.insertText);
                if (!lspText.empty())
                {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.lspWins++;
                    return GhostTextCacheEntry{lspText, "", "", false, 0};
                }
            }
            lastReason = "LSP provider returned no usable insertText";
        }
    }

    if (!lastReason.empty())
    {
        logProviderSkip(lastReason);
    }
    return {};
}

std::string Win32IDE::requestTitanGhostTextCompletion(const std::string& context, const std::string& language,
                                                      const std::string& suffix, const std::string& filePath,
                                                      int cursorLine, int cursorCol, uint64_t expectedSeq,
                                                      bool streamToUi)
{
    if (!m_useTitanKernel || m_loadedModelPath.empty() || !m_hwndMain || !m_hwndEditor)
    {
        return "";
    }
    if (expectedSeq != 0 && expectedSeq != m_ghostTextRequestSeq.load())
    {
        return "";
    }
    if (!ensureTitanGhostReady(m_loadedModelPath))
    {
        return "";
    }

    RAWRXD_SAMPLING_PARAMS sampling{};
    sampling.temperature = 0.2f;
    sampling.top_p = 0.95f;
    sampling.top_k = 48;
    sampling.repetition_penalty = 1.05f;
    sampling.max_tokens = 96;
    if (g_titanGhost.setSamplingParams)
    {
        g_titanGhost.setSamplingParams(&sampling);
    }

    std::string prompt = "Complete the following " + language +
                         " code at the cursor. "
                         "Return only the completion text with no explanation and no markdown.\n\n"
                         "[FILE]\n" +
                         filePath + "\n[LINE]\n" + std::to_string(cursorLine) + "\n[COLUMN]\n" +
                         std::to_string(cursorCol) + "\n[PREFIX]\n" + context + "\n[SUFFIX]\n" + suffix +
                         "\n[COMPLETION]\n";

    {
        std::lock_guard<std::mutex> lock(m_titanGhostMutex);
        TitanGhostTelemetryState& telemetry = titanGhostTelemetryFor(this);
        m_titanGhostStreamText.clear();
        telemetry.streamSeq = 0;
        telemetry.seqGaps = 0;
        telemetry.packets = 0;
        m_titanGhostStreamActive = true;
        m_titanGhostStreamRequestSeq = expectedSeq;
    }

    if (m_agenticBridge)
    {
        m_agenticBridge->ResetGhostSeqTelemetry();
    }

    if (g_titanGhost.streamReset)
    {
        g_titanGhost.streamReset();
    }

    RAWRXD_INFERENCE_HANDLE streamHandle = 0;
    if (g_titanGhost.beginStreaming(&streamHandle) != RAWRXD_SUCCESS)
    {
        uint64_t lastGhostSeq = m_agenticBridge ? m_agenticBridge->GetLastGhostSeq() : 0;
        LOG_INFO("Titan ghost beginStreaming failed; last_seq=" + std::to_string(lastGhostSeq));
        std::lock_guard<std::mutex> lock(m_titanGhostMutex);
        m_titanGhostStreamActive = false;
        m_titanGhostStreamRequestSeq = 0;
        return "";
    }

    if (streamToUi)
    {
        g_titanGhost.streamConfigureWindow(reinterpret_cast<uint64_t>(m_hwndMain), WM_TITAN_GHOST_STREAM, 0);
    }

    RAWRXD_INFERENCE_HANDLE inferenceHandle = 0;
    if (g_titanGhost.inferAsync(prompt.c_str(), prompt.size(), &inferenceHandle) != RAWRXD_SUCCESS)
    {
        uint64_t lastGhostSeq = m_agenticBridge ? m_agenticBridge->GetLastGhostSeq() : 0;
        LOG_INFO("Titan ghost inferAsync failed; last_seq=" + std::to_string(lastGhostSeq));
        g_titanGhost.endStreaming(streamHandle);
        std::lock_guard<std::mutex> lock(m_titanGhostMutex);
        m_titanGhostStreamActive = false;
        m_titanGhostStreamRequestSeq = 0;
        return "";
    }
    setActiveTitanGhostInference(inferenceHandle, expectedSeq);

    const RAWRXD_STATUS waitStatus = g_titanGhost.waitForInference(inferenceHandle, 12000);
    clearActiveTitanGhostInference(inferenceHandle);
    if (streamToUi)
    {
        PostMessageA(m_hwndMain, WM_TITAN_GHOST_STREAM, 0, 0);
    }
    Sleep(25);

    std::string completion;
    {
        std::lock_guard<std::mutex> lock(m_titanGhostMutex);
        completion = m_titanGhostStreamText;
    }
    uint64_t lastSeq = 0;
    uint64_t gapCount = 0;
    uint64_t packetCount = 0;
    drainTitanGhostPackets(completion, &lastSeq, &gapCount, &packetCount);

    g_titanGhost.endStreaming(streamHandle);

    {
        std::lock_guard<std::mutex> lock(m_titanGhostMutex);
        TitanGhostTelemetryState& telemetry = titanGhostTelemetryFor(this);
        m_titanGhostStreamText = completion;
        telemetry.streamSeq = lastSeq;
        telemetry.seqGaps = gapCount;
        telemetry.packets = packetCount;
        m_titanGhostStreamActive = false;
        m_titanGhostStreamRequestSeq = 0;
    }

    if (waitStatus != RAWRXD_SUCCESS)
    {
        uint64_t lastBridgeSeq = m_agenticBridge ? m_agenticBridge->GetLastGhostSeq() : 0;
        LOG_INFO("Titan ghost wait failed; status=" + std::to_string(static_cast<int>(waitStatus)) +
                 " stream_last_seq=" + std::to_string(lastSeq) + " bridge_last_seq=" + std::to_string(lastBridgeSeq) +
                 " gaps=" + std::to_string(gapCount) + " packets=" + std::to_string(packetCount));
        return "";
    }
    return trimGhostText(completion);
}

void Win32IDE_HandleTitanGhostStreamMessage(Win32IDE* ide)
{
    if (!ide)
    {
        return;
    }

    std::string combined;
    bool streamActive = false;
    uint64_t prevGapCount = 0;
    uint64_t lastSeq = 0;
    uint64_t gapCount = 0;
    uint64_t packetCount = 0;
    uint64_t streamRequestSeq = 0;

    {
        std::lock_guard<std::mutex> lock(ide->m_titanGhostMutex);
        TitanGhostTelemetryState& telemetry = titanGhostTelemetryFor(ide);
        prevGapCount = telemetry.seqGaps;
        lastSeq = telemetry.streamSeq;
        gapCount = telemetry.seqGaps;
        packetCount = telemetry.packets;
        streamRequestSeq = ide->m_titanGhostStreamRequestSeq;
        drainTitanGhostPackets(ide->m_titanGhostStreamText, &lastSeq, &gapCount, &packetCount);
        telemetry.streamSeq = lastSeq;
        telemetry.seqGaps = gapCount;
        telemetry.packets = packetCount;
        combined = ide->m_titanGhostStreamText;
        streamActive = ide->m_titanGhostStreamActive;
    }

    if (gapCount > prevGapCount)
    {
        LOG_INFO("Titan ghost stream sequence gap detected; last_seq=" + std::to_string(lastSeq) +
                 " gaps=" + std::to_string(gapCount) + " packets=" + std::to_string(packetCount));
    }

    if (ide->m_agenticBridge)
    {
        ide->m_agenticBridge->ObserveGhostStreamSeq(lastSeq);
    }

    if (!streamActive || combined.empty() || !ide->m_hwndEditor)
    {
        return;
    }

    if (streamRequestSeq != 0 && streamRequestSeq != ide->m_ghostTextRequestSeq.load())
    {
        return;
    }

    CHARRANGE sel{};
    SendMessageA(ide->m_hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
    if (sel.cpMin != ide->m_ghostTextRequestCursorPos)
    {
        return;
    }

    ide->m_ghostTextCommitContent = ide->trimGhostText(combined);
    ide->m_ghostTextContent = ide->m_ghostTextCommitContent;
    ide->m_ghostTextBuffer = combined;
    if (ide->m_ghostTextContent.empty())
    {
        return;
    }

    ide->m_ghostTextVisible = true;
    ide->m_ghostTextAccepted = false;
    InvalidateRect(ide->m_hwndEditor, nullptr, FALSE);
}

// ============================================================================
// GHOST TEXT DELIVERY — WM_GHOST_TEXT_READY handler (UI thread)
// ============================================================================

void Win32IDE::onGhostTextReady(int requestedCursorPos, const char* completionText)
{
    m_ghostTextPending = false;

    if (!completionText || !m_hwndEditor)
        return;

    // ── UX IMPROVEMENT: Stale Completion Guard ───────────────────────────────
    // Verify cursor hasn't moved since request AND that we haven't dismissed.
    // This prevents stale async completions from corrupting state after Esc.
    CHARRANGE sel;
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);

    if (sel.cpMin != sel.cpMax || sel.cpMin != requestedCursorPos)
    {
        // Cursor moved or text is selected — discard stale completion without
        // rebasing the ghost overlay onto the user's current selection.
        return;
    }

    // If ghost text was dismissed while we were computing, don't render
    if (!m_ghostTextEnabled || m_ghostTextAccepted)
    {
        return;
    }

    // Validate same-line prefix snapshot before rendering asynchronous completion.
    const int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
    const int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    const std::string currentLinePrefix = getEditorRangeUtf8(m_hwndEditor, lineStart, sel.cpMin);
    if (!m_ghostTextRequestLinePrefix.empty() && currentLinePrefix != m_ghostTextRequestLinePrefix)
    {
        return;
    }

    const std::string extracted = trimGhostText(ExtractGhostSuggestion(completionText, m_ghostTextRequestLinePrefix));
    const std::string fullDraft = trimGhostText(completionText);
    if (!IsRelevantGhostSuggestion(extracted))
    {
        dismissGhostText();
        return;
    }

    // ARCHITECTURE ALIGNMENT:
    // Directly update state and invalidate for RichEdit overlay rendering.
    // Ensure we don't just store, but activate the 'visible' state for WM_PAINT.
    m_ghostTextCommitContent = fullDraft.empty() ? extracted : fullDraft;
    m_ghostTextContent = extracted;
    m_ghostTextVisible = true;
    m_ghostTextAccepted = false;
    m_ghostTextSelectionSnapshot = MakeCollapsedSelectionSnapshot(sel.cpMin);
    m_ghostTextStreamSessionId = 0;
    m_activeSuggestionContext.range.cpMin = requestedCursorPos;
    m_activeSuggestionContext.range.cpMax = requestedCursorPos + static_cast<LONG>(m_ghostTextContent.size());
    m_activeSuggestionContext.state = SuggestionState::Pending;
    m_activeSuggestionContext.preview = m_ghostTextContent;
    m_activeSuggestionContext.stateChangedTickMs = static_cast<uint64_t>(GetTickCount64());
    m_ghostTextPlanId = m_ghostTextPendingPlanId;
    m_ghostTextSessionId = m_ghostTextPendingSessionId;
    m_ghostTextFromAgentic = m_ghostTextPendingFromAgentic;
    m_ghostTextPendingPlanId.clear();
    m_ghostTextPendingSessionId.clear();
    m_ghostTextPendingFromAgentic = false;
    updateGhostTextPendingEditPreview(m_ghostTextCommitContent.empty() ? m_ghostTextContent : m_ghostTextCommitContent);

    // Align with MASM Sovereign logic: trigger immediate repaint
    if (m_hwndEditor)
    {
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
        UpdateWindow(m_hwndEditor);  // Force immediate paint to minimize flicker
    }

    // Record event
    recordEvent(AgentEventType::GhostTextRequested, "", m_ghostTextContent.substr(0, 128), "", 0, true);

    bool titanRunning = false;
    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        titanRunning = m_titanAgentRunning;
    }
    if (titanRunning)
    {
        scheduleTitanDraftPrefetch();
    }
}

void Win32IDE::onGhostTextTokenChunk(const char* tokenChunk, uint64_t sessionId)
{
    if (!tokenChunk || !*tokenChunk || !m_hwndEditor)
    {
        return;
    }
    if (sessionId == 0)
    {
        return;
    }
    if (sessionId != m_ghostTextRequestSeq.load())
    {
        return;
    }
    if (!m_ghostTextEnabled || m_ghostTextAccepted)
    {
        return;
    }

    CHARRANGE sel{};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));

    if (sel.cpMin != sel.cpMax)
    {
        dismissGhostText();
        return;
    }

    if (m_ghostTextStreamSessionId == 0)
    {
        m_ghostTextStreamSessionId = sessionId;
    }
    if (m_ghostTextStreamSessionId != sessionId)
    {
        // Ignore stale or cross-request token streams.
        return;
    }

    if (!m_ghostTextVisible)
    {
        m_ghostTextVisible = true;
        m_ghostTextAccepted = false;
        m_ghostTextContent.clear();
        m_ghostTextCommitContent.clear();
        m_ghostTextBuffer.clear();
        m_ghostTextRequestCursorPos = static_cast<int>(sel.cpMin);
        m_ghostTextSelectionSnapshot = MakeCollapsedSelectionSnapshot(sel.cpMin);
        m_activeSuggestionContext.range.cpMin = static_cast<LONG>(sel.cpMin);
        m_activeSuggestionContext.range.cpMax = static_cast<LONG>(sel.cpMin);
        m_activeSuggestionContext.state = SuggestionState::Pending;
        m_activeSuggestionContext.stateChangedTickMs = static_cast<uint64_t>(GetTickCount64());
    }

    // If caret moved since stream started, invalidate the stream instead of
    // rebasing it at the new caret. This preserves request causality.
    if (m_ghostTextRequestCursorPos >= 0 && m_ghostTextRequestCursorPos != static_cast<int>(sel.cpMin))
    {
        dismissGhostText();
        return;
    }

    m_pendingGhostAppend += tokenChunk;
    m_ghostTextStreamingActive = true;
    if (!m_ghostTextRenderScheduled)
    {
        m_ghostTextRenderScheduled = true;
        if (m_hwndMain && SetTimer(m_hwndMain, GHOST_TEXT_RENDER_TIMER_ID, GHOST_TEXT_RENDER_BATCH_MS, nullptr) == 0)
        {
            PostMessageA(m_hwndMain, WM_USER_GHOST_RENDER, 0, 0);
        }
    }
}

void Win32IDE::onGhostTextRenderMessage()
{
    if (m_hwndMain)
    {
        KillTimer(m_hwndMain, GHOST_TEXT_RENDER_TIMER_ID);
    }
    m_ghostTextRenderScheduled = false;

    if (!m_hwndEditor || !m_ghostTextEnabled || m_ghostTextAccepted)
    {
        m_pendingGhostAppend.clear();
        return;
    }

    const uint64_t activeSeq = m_ghostTextRequestSeq.load();
    if (m_ghostTextStreamSessionId != 0 && m_ghostTextStreamSessionId != activeSeq)
    {
        m_pendingGhostAppend.clear();
        return;
    }

    CHARRANGE sel{};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
    if (sel.cpMin != sel.cpMax)
    {
        dismissGhostText();
        return;
    }
    if (m_ghostTextRequestCursorPos >= 0 && m_ghostTextRequestCursorPos != static_cast<int>(sel.cpMin))
    {
        dismissGhostText();
        return;
    }

    const int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
    const int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    const std::string currentLinePrefix = getEditorRangeUtf8(m_hwndEditor, lineStart, sel.cpMin);
    if (!m_ghostTextRequestLinePrefix.empty() && currentLinePrefix != m_ghostTextRequestLinePrefix)
    {
        dismissGhostText();
        return;
    }

    if (m_pendingGhostAppend.empty())
    {
        return;
    }

    m_ghostTextBuffer += m_pendingGhostAppend;
    m_pendingGhostAppend.clear();

    const std::string displayText = trimGhostText(m_ghostTextBuffer);
    if (displayText.empty())
    {
        return;
    }

    m_ghostTextContent = displayText;
    m_ghostTextCommitContent = displayText;
    m_ghostTextVisible = true;
    m_ghostTextAccepted = false;
    m_ghostTextSelectionSnapshot = MakeCollapsedSelectionSnapshot(sel.cpMin);
    m_activeSuggestionContext.range.cpMin = static_cast<LONG>(m_ghostTextRequestCursorPos);
    m_activeSuggestionContext.range.cpMax =
        static_cast<LONG>(m_ghostTextRequestCursorPos + static_cast<int>(m_ghostTextContent.size()));
    m_activeSuggestionContext.state = SuggestionState::Pending;
    m_activeSuggestionContext.preview = m_ghostTextContent;
    m_activeSuggestionContext.stateChangedTickMs = static_cast<uint64_t>(GetTickCount64());
    updateGhostTextPendingEditPreview(m_ghostTextCommitContent.empty() ? m_ghostTextContent : m_ghostTextCommitContent);

    InvalidateRect(m_hwndEditor, nullptr, FALSE);
    bool titanRunning = false;
    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        titanRunning = m_titanAgentRunning;
    }
    if (titanRunning)
    {
        scheduleTitanDraftPrefetch();
    }
}

void Win32IDE::onGhostTextComplete(uint64_t sessionId, const char* completionText)
{
    m_ghostTextPending = false;

    if (sessionId == 0 || sessionId != m_ghostTextRequestSeq.load())
    {
        std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
        m_ghostTextMetrics.staleDrops++;
        return;
    }

    if (!m_hwndEditor || !m_ghostTextEnabled || m_ghostTextAccepted)
    {
        return;
    }

    if (m_ghostTextRenderScheduled || !m_pendingGhostAppend.empty())
    {
        onGhostTextRenderMessage();
        if (sessionId != m_ghostTextRequestSeq.load() || !m_ghostTextEnabled || m_ghostTextAccepted)
        {
            return;
        }
    }

    m_ghostTextStreamingActive = false;
    m_ghostTextStreamSessionId = 0;

    CHARRANGE sel{};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
    if (sel.cpMin != sel.cpMax)
    {
        dismissGhostText();
        return;
    }
    if (m_ghostTextRequestCursorPos >= 0 && m_ghostTextRequestCursorPos != static_cast<int>(sel.cpMin))
    {
        dismissGhostText();
        return;
    }

    const int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
    const int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    const std::string currentLinePrefix = getEditorRangeUtf8(m_hwndEditor, lineStart, sel.cpMin);
    if (!m_ghostTextRequestLinePrefix.empty() && currentLinePrefix != m_ghostTextRequestLinePrefix)
    {
        dismissGhostText();
        return;
    }

    const std::string rawCompletion = completionText ? completionText : "";
    const std::string extracted = trimGhostText(ExtractGhostSuggestion(rawCompletion, m_ghostTextRequestLinePrefix));
    const std::string fullDraft = trimGhostText(rawCompletion);

    if (!extracted.empty() && IsRelevantGhostSuggestion(extracted))
    {
        m_ghostTextBuffer = fullDraft.empty() ? extracted : fullDraft;
        m_ghostTextCommitContent = m_ghostTextBuffer;
        m_ghostTextContent = extracted;
    }
    else if (m_ghostTextContent.empty())
    {
        dismissGhostText();
        return;
    }

    m_ghostTextVisible = !m_ghostTextContent.empty();
    if (!m_ghostTextVisible)
    {
        return;
    }

    m_ghostTextSelectionSnapshot = MakeCollapsedSelectionSnapshot(sel.cpMin);
    m_activeSuggestionContext.range.cpMin = static_cast<LONG>(m_ghostTextRequestCursorPos);
    m_activeSuggestionContext.range.cpMax =
        static_cast<LONG>(m_ghostTextRequestCursorPos + static_cast<int>(m_ghostTextContent.size()));
    m_activeSuggestionContext.state = SuggestionState::Pending;
    m_activeSuggestionContext.preview = m_ghostTextContent;
    m_activeSuggestionContext.stateChangedTickMs = static_cast<uint64_t>(GetTickCount64());
    m_ghostTextPlanId = m_ghostTextPendingPlanId;
    m_ghostTextSessionId = m_ghostTextPendingSessionId;
    m_ghostTextFromAgentic = m_ghostTextPendingFromAgentic;
    m_ghostTextPendingPlanId.clear();
    m_ghostTextPendingSessionId.clear();
    m_ghostTextPendingFromAgentic = false;
    updateGhostTextPendingEditPreview(m_ghostTextCommitContent.empty() ? m_ghostTextContent : m_ghostTextCommitContent);

    InvalidateRect(m_hwndEditor, nullptr, FALSE);
    recordEvent(AgentEventType::GhostTextRequested, "", m_ghostTextContent.substr(0, 128), "", 0, true);

    bool titanRunning = false;
    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        titanRunning = m_titanAgentRunning;
    }
    if (titanRunning)
    {
        scheduleTitanDraftPrefetch();
    }
}

void Win32IDE::invalidateTitanDraftPrefetch()
{
    std::lock_guard<std::mutex> lock(m_titanDraftPrefetchMutex);
    ++m_titanPrefetchGeneration;
    m_titanPrefetchedDraftBlock.clear();
    m_titanPrefetchedDraftTokens = 0;
}

void Win32IDE::scheduleTitanDraftPrefetch()
{
    uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(m_titanDraftPrefetchMutex);
        if (m_titanDraftPrefetchInFlight)
            return;
        if (!m_titanPrefetchedDraftBlock.empty())
            return;
        m_titanDraftPrefetchInFlight = true;
        generation = m_titanPrefetchGeneration;
    }

    std::thread([this, generation]() {
        char draftBuf[512] = {0};
        int draftCount = 0;
        const int draftLen = Bridge_ReadDraftBlockGhostA(draftBuf, (int)sizeof(draftBuf), &draftCount);

        std::lock_guard<std::mutex> lock(m_titanDraftPrefetchMutex);
        m_titanDraftPrefetchInFlight = false;
        if (generation != m_titanPrefetchGeneration)
            return;
        if (draftLen > 0 && draftCount > 0)
        {
            m_titanPrefetchedDraftBlock.assign(draftBuf, draftLen);
            m_titanPrefetchedDraftTokens = draftCount;
        }
    }).detach();
}

// ============================================================================
// DISMISS — clears ghost text
// ============================================================================

void Win32IDE::dismissGhostText()
{
    const bool hadVisibleGhostText = m_ghostTextVisible;
    cancelGhostTextInferenceRequests();
    clearGhostTextPendingEdit(hadVisibleGhostText ? RawrXD::Review::EditState::Declined
                                                  : RawrXD::Review::EditState::Discarded);

    const bool shouldReportDismiss = hadVisibleGhostText && !m_ghostTextAccepted && m_ghostTextFromAgentic &&
                                     !m_ghostTextPlanId.empty();
    const std::string feedbackPlanId = m_ghostTextPlanId;
    const std::string feedbackSessionId = m_ghostTextSessionId;
    const std::string feedbackPreview = m_ghostTextContent;

    // Clear all ghost text state atomically
    m_ghostTextVisible = false;
    m_ghostTextContent.clear();
    m_ghostTextCommitContent.clear();
    m_ghostTextLine = -1;
    m_ghostTextColumn = -1;
    m_ghostTextRequestCursorPos = -1;
    m_ghostTextSelectionSnapshot = MakeCollapsedSelectionSnapshot(-1);
    m_ghostTextStreamSessionId = 0;
    m_ghostTextAccepted = false;
    m_ghostTextPlanId.clear();
    m_ghostTextSessionId.clear();
    m_ghostTextFromAgentic = false;
    m_ghostTextRequestLinePrefix.clear();  // Clear prefix to prevent stale matches
    invalidateTitanDraftPrefetch();
    if (m_activeSuggestionContext.state == SuggestionState::Pending)
    {
        m_activeSuggestionContext.state = SuggestionState::Rejected;
        m_activeSuggestionContext.stateChangedTickMs = static_cast<uint64_t>(GetTickCount64());
    }
    hideGhostDiffOverlayUi();

    if (m_hwndEditor && hadVisibleGhostText)
    {
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    }

    if (shouldReportDismiss)
    {
        rawrxd::reportInlineCompletionFeedback(feedbackSessionId, feedbackPlanId, false,
                                               feedbackPreview.empty()
                                                   ? "Ghost text dismissed before insertion."
                                                   : std::string("Ghost text dismissed before insertion: ") +
                                                         feedbackPreview.substr(0, 128));
    }
}

// ============================================================================
// ACCEPT — Tab key inserts the ghost text into the editor
// ============================================================================

void Win32IDE::acceptGhostText()
{
    if (!m_ghostTextVisible || (m_ghostTextContent.empty() && m_ghostTextCommitContent.empty()) || !m_hwndEditor)
        return;

    CHARRANGE initialSel{};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&initialSel));
    if (initialSel.cpMin != initialSel.cpMax ||
        (m_ghostTextRequestCursorPos >= 0 && initialSel.cpMin != m_ghostTextRequestCursorPos))
    {
        // Accept only at the original collapsed caret. If the user selected text
        // or moved the caret, preserve that selection and dismiss the stale ghost.
        dismissGhostText();
        return;
    }
    if (IsSelectionSnapshotValid(m_ghostTextSelectionSnapshot) &&
        (initialSel.cpMin != m_ghostTextSelectionSnapshot.cpMin || initialSel.cpMax != m_ghostTextSelectionSnapshot.cpMax))
    {
        dismissGhostText();
        return;
    }
    const int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, initialSel.cpMin, 0);
    const int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    const std::string currentLinePrefix = getEditorRangeUtf8(m_hwndEditor, lineStart, initialSel.cpMin);
    if (!m_ghostTextRequestLinePrefix.empty() && currentLinePrefix != m_ghostTextRequestLinePrefix)
    {
        dismissGhostText();
        return;
    }

    std::string textToInsert = !m_ghostTextCommitContent.empty() ? m_ghostTextCommitContent : m_ghostTextContent;
    bool titanRunning = false;
    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        titanRunning = m_titanAgentRunning;
    }

    // End-to-end RX ABI commit path: if Titan stream is active, pull the
    // current 8-token draft block via TITAN_OP_RX_READ_DRAFT_BLOCK before
    // falling back to cached ghost text.
    if (titanRunning)
    {
        std::string prefetched;
        int prefetchedTokens = 0;
        {
            std::lock_guard<std::mutex> lock(m_titanDraftPrefetchMutex);
            prefetched = m_titanPrefetchedDraftBlock;
            prefetchedTokens = m_titanPrefetchedDraftTokens;
            m_titanPrefetchedDraftBlock.clear();
            m_titanPrefetchedDraftTokens = 0;
        }

        if (!prefetched.empty() && prefetchedTokens > 0)
        {
            textToInsert = prefetched;
            postOutputPanelSafe("[Titan Agent] TAB commit via prefetched RX draft block (tokens=" +
                                std::to_string(prefetchedTokens) + ")\n");
        }
        else
        {
            char draftBuf[512] = {0};
            int draftCount = 0;
            const int draftLen = Bridge_ReadDraftBlockGhostA(draftBuf, (int)sizeof(draftBuf), &draftCount);
            if (draftLen > 0 && draftCount > 0)
            {
                textToInsert.assign(draftBuf, draftLen);
                postOutputPanelSafe("[Titan Agent] TAB commit via RX draft block (tokens=" +
                                    std::to_string(draftCount) + ")\n");
            }
        }
    }

    if (textToInsert.empty())
    {
        return;
    }

    updateGhostTextPendingEditPreview(textToInsert);
    const uint64_t pendingEditId = m_activeGhostPendingEditId;
    if (pendingEditId == 0)
    {
        return;
    }

    // Bridge to Predictive Engine only after the accept lane is validated.
    if (m_predictiveGhostText)
    {
        m_predictiveGhostText->acceptSuggestion();
    }

    const std::string feedbackPlanId = m_ghostTextPlanId;
    const std::string feedbackSessionId = m_ghostTextSessionId;
    const bool feedbackFromAgentic = m_ghostTextFromAgentic;
    m_ghostTextAccepted = true;

    // ── UX IMPROVEMENT: Zero-Jitter Tab Accept ──────────────────────────────
    // Suppress redraws during the insert to prevent cursor jitter and flicker.
    // This ensures the ghost text disappears and the real text appears atomically.
    SendMessageA(m_hwndEditor, WM_SETREDRAW, FALSE, 0);

    // Clear ghost state BEFORE insert (prevents ghost from re-rendering mid-insert)
    m_ghostTextVisible = false;
    m_ghostTextContent.clear();
    m_ghostTextCommitContent.clear();
    m_ghostTextLine = -1;
    m_ghostTextColumn = -1;
    m_ghostTextRequestCursorPos = -1;
    m_ghostTextSelectionSnapshot = MakeCollapsedSelectionSnapshot(-1);
    m_ghostTextStreamSessionId = 0;
    // Don't clear m_ghostTextAccepted - we just set it to true above
    m_ghostTextPlanId.clear();
    m_ghostTextSessionId.clear();
    m_ghostTextFromAgentic = false;
    m_ghostTextRequestLinePrefix.clear();
    invalidateTitanDraftPrefetch();
    hideGhostDiffOverlayUi();

    const LONG insertedStart = initialSel.cpMin;
    const bool approved = approvePendingEdit(pendingEditId);
    const LONG insertedEnd = insertedStart + static_cast<LONG>(textToInsert.size());
    if (!approved)
    {
        SendMessageA(m_hwndEditor, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
        UpdateWindow(m_hwndEditor);
        m_ghostTextAccepted = false;
        return;
    }

    CHARRANGE caretAfterInsert{};
    caretAfterInsert.cpMin = insertedEnd;
    caretAfterInsert.cpMax = insertedEnd;
    SendMessageA(m_hwndEditor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&caretAfterInsert));
    m_activeSuggestionContext.range.cpMin = insertedStart;
    m_activeSuggestionContext.range.cpMax = insertedEnd;
    m_activeSuggestionContext.state = SuggestionState::Accepted;
    m_activeSuggestionContext.preview = textToInsert;
    m_activeSuggestionContext.stateChangedTickMs = static_cast<uint64_t>(GetTickCount64());

    // Re-enable redraws and force a single atomic repaint
    SendMessageA(m_hwndEditor, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwndEditor, nullptr, FALSE);
    UpdateWindow(m_hwndEditor);

    // Apply color after the atomic repaint (subtle highlight, non-blocking)
    applyEditorRangeColor(m_hwndEditor, insertedStart, insertedEnd, m_currentTheme.textColor);

    // Record acceptance event
    recordEvent(AgentEventType::GhostTextAccepted, "", textToInsert.substr(0, 128), "", 0, true);

    if (feedbackFromAgentic && !feedbackPlanId.empty())
    {
        rawrxd::reportInlineCompletionFeedback(feedbackSessionId, feedbackPlanId, true,
                                               std::string("Ghost text accepted into editor: ") +
                                                   textToInsert.substr(0, 128));
    }

    LOG_INFO("Ghost text accepted (" + std::to_string(textToInsert.size()) + " chars)");
}

// ============================================================================
// RENDER — paints ghost text onto the editor surface
// ============================================================================

// -----------------------------------------------------------------------------
// Local renderer fallback for ghost text.
// Note: Some build lanes do not link Win32IDE_Layout_Pure.asm, which exports
// Layout_DrawGhostText. Keep this path self-contained so the Win32IDE target
// always links; the MASM path can be reintroduced via explicit build wiring.
// -----------------------------------------------------------------------------
static void DrawGhostTextFast(HDC hdc, int x, int y, const char* text)
{
    if (!hdc || !text || !*text)
        return;
    TextOutA(hdc, x, y, text, static_cast<int>(std::strlen(text)));
}

static void AlphaFillRect(HDC hdc, const RECT& rc, COLORREF color, BYTE alpha)
{
    const int width = rc.right - rc.left;
    const int height = rc.bottom - rc.top;
    if (width <= 0 || height <= 0)
        return;

    HDC hdcMem = CreateCompatibleDC(hdc);
    if (!hdcMem)
        return;

    HBITMAP bmp = CreateCompatibleBitmap(hdc, width, height);
    if (!bmp)
    {
        DeleteDC(hdcMem);
        return;
    }

    HBITMAP oldBmp = (HBITMAP)SelectObject(hdcMem, bmp);
    HBRUSH brush = CreateSolidBrush(color);
    RECT local{0, 0, width, height};
    FillRect(hdcMem, &local, brush);

    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = alpha;
    bf.AlphaFormat = 0;
    AlphaBlend(hdc, rc.left, rc.top, width, height, hdcMem, 0, 0, width, height, bf);

    DeleteObject(brush);
    SelectObject(hdcMem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(hdcMem);
}

void Win32IDE::renderSuggestionTint(HDC hdc)
{
    if (!m_hwndEditor)
        return;

    const uint64_t nowMs = static_cast<uint64_t>(GetTickCount64());
    const uint64_t pulseWindowMs = 220;
    const SuggestionState state = m_activeSuggestionContext.state;
    if (state == SuggestionState::None)
        return;

    const bool isPending = state == SuggestionState::Pending;
    const uint64_t ageMs = (nowMs >= m_activeSuggestionContext.stateChangedTickMs)
                               ? (nowMs - m_activeSuggestionContext.stateChangedTickMs)
                               : 0;
    if (!isPending && ageMs > pulseWindowMs)
    {
        m_activeSuggestionContext.state = SuggestionState::None;
        return;
    }

    const LONG cpMin = m_activeSuggestionContext.range.cpMin;
    const LONG cpMax = m_activeSuggestionContext.range.cpMax;
    if (cpMin < 0 || cpMax <= cpMin)
        return;

    const LONG textLen = (LONG)SendMessageA(m_hwndEditor, WM_GETTEXTLENGTH, 0, 0);
    const LONG safeMin = (std::max)(0L, (std::min)(cpMin, textLen));
    const LONG safeMax = (std::max)(safeMin, (std::min)(cpMax, textLen));
    if (safeMax <= safeMin)
        return;

    TEXTMETRICA tm{};
    GetTextMetricsA(hdc, &tm);
    const int lineHeight = tm.tmHeight + tm.tmExternalLeading;

    RECT editorRC{};
    GetClientRect(m_hwndEditor, &editorRC);

    const int startLine = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, safeMin, 0);
    const int endLine = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, safeMax, 0);
    for (int line = startLine; line <= endLine; ++line)
    {
        const LONG lineStart = (LONG)SendMessageA(m_hwndEditor, EM_LINEINDEX, line, 0);
        LONG lineNext = (LONG)SendMessageA(m_hwndEditor, EM_LINEINDEX, line + 1, 0);
        if (lineNext < 0)
            lineNext = textLen;

        const LONG segStart = (std::max)(safeMin, lineStart);
        const LONG segEnd = (std::min)(safeMax, lineNext);
        if (segEnd <= segStart)
            continue;

        POINTL pStart{};
        POINTL pEnd{};
        SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)segStart, (LPARAM)&pStart);
        SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)segEnd, (LPARAM)&pEnd);
        if (pStart.x < 0 || pStart.y < 0)
            continue;

        int x1 = pStart.x - 2;
        int x2 = pEnd.x + 8;
        if (line < endLine)
            x2 = editorRC.right - 6;
        if (x2 <= x1)
            x2 = x1 + 16;

        RECT tintRc{x1, pStart.y - 1, x2, pStart.y + lineHeight + 1};
        if (tintRc.right <= 0 || tintRc.left >= editorRC.right || tintRc.bottom <= 0 || tintRc.top >= editorRC.bottom)
            continue;

        if (tintRc.left < 0)
            tintRc.left = 0;
        if (tintRc.right > editorRC.right)
            tintRc.right = editorRC.right;
        if (tintRc.top < 0)
            tintRc.top = 0;
        if (tintRc.bottom > editorRC.bottom)
            tintRc.bottom = editorRC.bottom;

        COLORREF tintColor = RGB(51, 0, 0);
        BYTE tintAlpha = 76;
        if (!isPending)
        {
            const double t = 1.0 - static_cast<double>(ageMs) / static_cast<double>(pulseWindowMs);
            const double clamped = (std::max)(0.0, (std::min)(1.0, t));
            tintColor = (state == SuggestionState::Accepted) ? RGB(24, 88, 42) : RGB(102, 24, 24);
            tintAlpha = static_cast<BYTE>(28 + static_cast<int>(92.0 * clamped));
        }
        AlphaFillRect(hdc, tintRc, tintColor, tintAlpha);
    }
}

LRESULT CALLBACK Win32IDE::GhostDiffOverlayProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = reinterpret_cast<Win32IDE*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

    switch (uMsg)
    {
        case WM_NCHITTEST:
            return HTCLIENT;

        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_LBUTTONUP:
        {
            if (!pThis)
                return 0;
            RECT rc = {};
            GetClientRect(hwnd, &rc);
            const int x = static_cast<int>(static_cast<short>(LOWORD(lParam)));
            const int splitX = (rc.right - rc.left) / 2;
            if (x < splitX)
                pThis->acceptGhostText();
            else
                pThis->dismissGhostText();
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc = {};
            GetClientRect(hwnd, &rc);

            HBRUSH shellBrush = CreateSolidBrush(RGB(28, 0, 0));
            HPEN shellPen = CreatePen(PS_SOLID, 1, RGB(90, 20, 20));
            HPEN oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, shellPen));
            HBRUSH oldBrush = reinterpret_cast<HBRUSH>(SelectObject(hdc, shellBrush));
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 10, 10);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(shellPen);
            DeleteObject(shellBrush);

            RECT inner = rc;
            InflateRect(&inner, -2, -2);

            RECT leftRc = inner;
            leftRc.right = (inner.right + inner.left) / 2;
            RECT rightRc = inner;
            rightRc.left = leftRc.right;

            HBRUSH keepBrush = CreateSolidBrush(RGB(48, 84, 52));
            HBRUSH undoBrush = CreateSolidBrush(RGB(86, 52, 52));
            FillRect(hdc, &leftRc, keepBrush);
            FillRect(hdc, &rightRc, undoBrush);
            DeleteObject(keepBrush);
            DeleteObject(undoBrush);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(220, 220, 220));
            DrawTextA(hdc, "Keep", -1, &leftRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            DrawTextA(hdc, "Undo", -1, &rightRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

            HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(110, 30, 30));
            oldPen = reinterpret_cast<HPEN>(SelectObject(hdc, borderPen));
            oldBrush = reinterpret_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 10, 10);
            MoveToEx(hdc, leftRc.right, rc.top, nullptr);
            LineTo(hdc, leftRc.right, rc.bottom);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(borderPen);

            EndPaint(hwnd, &ps);
            return 0;
        }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void Win32IDE::hideGhostDiffOverlayUi()
{
    if (m_hwndGhostDiffOverlay && IsWindow(m_hwndGhostDiffOverlay))
        ShowWindow(m_hwndGhostDiffOverlay, SW_HIDE);
    m_ghostDiffOverlayVisible = false;
}

void Win32IDE::destroyGhostDiffOverlayUi()
{
    if (m_hwndGhostDiffOverlay && IsWindow(m_hwndGhostDiffOverlay))
        DestroyWindow(m_hwndGhostDiffOverlay);
    m_hwndGhostDiffOverlay = nullptr;
    m_ghostDiffOverlayVisible = false;
}

void Win32IDE::updateGhostDiffOverlayUi(const POINTL& anchorPt)
{
    if (!m_hwndEditor || !m_ghostTextVisible || m_ghostTextContent.empty())
    {
        hideGhostDiffOverlayUi();
        return;
    }

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = Win32IDE::GhostDiffOverlayProc;
        wc.hInstance = m_hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wc.lpszClassName = GHOST_DIFF_OVERLAY_CLASS;
        if (RegisterClassExA(&wc))
            classRegistered = true;
    }

    if (!m_hwndGhostDiffOverlay || !IsWindow(m_hwndGhostDiffOverlay))
    {
        m_hwndGhostDiffOverlay = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
                                                 GHOST_DIFF_OVERLAY_CLASS, "", WS_POPUP, 0, 0, GHOST_DIFF_OVERLAY_WIDTH,
                                                 GHOST_DIFF_OVERLAY_HEIGHT, m_hwndMain, nullptr, m_hInstance, nullptr);
        if (!m_hwndGhostDiffOverlay)
            return;
        SetWindowLongPtrA(m_hwndGhostDiffOverlay, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        SetLayeredWindowAttributes(m_hwndGhostDiffOverlay, 0, 238, LWA_ALPHA);
    }

    POINT screenPt = {anchorPt.x + 10, anchorPt.y + 22};
    ClientToScreen(m_hwndEditor, &screenPt);

    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    HMONITOR mon = MonitorFromPoint(screenPt, MONITOR_DEFAULTTONEAREST);
    if (GetMonitorInfoA(mon, &mi))
    {
        if (screenPt.x + GHOST_DIFF_OVERLAY_WIDTH > mi.rcWork.right)
            screenPt.x = mi.rcWork.right - GHOST_DIFF_OVERLAY_WIDTH;
        if (screenPt.y + GHOST_DIFF_OVERLAY_HEIGHT > mi.rcWork.bottom)
            screenPt.y = mi.rcWork.bottom - GHOST_DIFF_OVERLAY_HEIGHT;
        if (screenPt.x < mi.rcWork.left)
            screenPt.x = mi.rcWork.left;
        if (screenPt.y < mi.rcWork.top)
            screenPt.y = mi.rcWork.top;
    }

    SetWindowPos(m_hwndGhostDiffOverlay, HWND_TOPMOST, screenPt.x, screenPt.y, GHOST_DIFF_OVERLAY_WIDTH,
                 GHOST_DIFF_OVERLAY_HEIGHT, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    m_ghostDiffOverlayVisible = true;
}

void Win32IDE::renderGhostText(HDC hdc)
{
    if (!m_hwndEditor)
        return;

    // ── UX IMPROVEMENT: Stable Rendering Guard ───────────────────────────────
    // Don't render ghost text if editor is in the middle of an operation.
    // This prevents flicker during rapid typing or selection changes.
    if (m_ghostTextAccepted)
    {
        hideGhostDiffOverlayUi();
        return;
    }

    bool showPagingStatus = false;
    uint64_t elapsedMs = 0;
    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        if (m_titanAgentRunning)
        {
            const uint64_t now = GetTickCount64();
            const uint64_t startMs = m_titanAgentStartMs;
            const uint64_t lastPacketMs = m_titanAgentLastPacketMs;
            elapsedMs = (startMs > 0 && now >= startMs) ? (now - startMs) : 0;
            const uint64_t sinceLastPacketMs =
                (lastPacketMs > 0 && now >= lastPacketMs) ? (now - lastPacketMs) : elapsedMs;
            showPagingStatus = (m_titanAgentPostedChars == 0) || (sinceLastPacketMs >= 1200);
        }
    }

    if ((!m_ghostTextVisible || m_ghostTextContent.empty()) && !showPagingStatus)
    {
        hideGhostDiffOverlayUi();
        return;
    }

    // Get current cursor position to know where to draw
    CHARRANGE sel;
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    if (sel.cpMin != sel.cpMax || (m_ghostTextRequestCursorPos >= 0 && sel.cpMin != m_ghostTextRequestCursorPos))
    {
        hideGhostDiffOverlayUi();
        return;
    }

    // Get the pixel position of the cursor
    POINTL pt;
    SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)sel.cpMin, (LPARAM)&pt);

    if (pt.x < 0 || pt.y < 0)
    {
        hideGhostDiffOverlayUi();
        return;
    }

    // Get the text after cursor on the same line to know rendering offset
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int lineLen = (int)SendMessageA(m_hwndEditor, EM_LINELENGTH, sel.cpMin, 0);
    int lineEnd = lineStart + lineLen;

    POINTL endPt;
    if (lineEnd > sel.cpMin)
    {
        SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)lineEnd, (LPARAM)&endPt);
    }
    else
    {
        endPt = pt;
    }

    // Split ghost text into lines
    std::vector<std::string> lines;
    if (showPagingStatus)
    {
        static const std::array<const char*, 4> kSpinner = {"|", "/", "-", "\\"};
        const int phase = static_cast<int>((GetTickCount64() / 220ULL) % kSpinner.size());
        const uint64_t elapsedSec = elapsedMs / 1000ULL;
        lines.push_back("[Titan] Paging shards " + std::string(kSpinner[phase]) + "  t+" + std::to_string(elapsedSec) +
                        "s");
    }
    else
    {
        // ── BRIDGE WIRING: Pull from ranked completion candidates ────────────
        std::string ghostTextToRender;
        {
            std::lock_guard<std::mutex> lock(m_completionCandidatesMutex);
            if (!m_completionCandidates.empty() && m_completionSelectedIndex >= 0 && 
                m_completionSelectedIndex < static_cast<int>(m_completionCandidates.size()))
            {
                // Use selected candidate
                ghostTextToRender = m_completionCandidates[m_completionSelectedIndex].insertText;
            }
            else if (!m_completionCandidates.empty())
            {
                // Default to top-ranked candidate (index 0)
                ghostTextToRender = m_completionCandidates[0].insertText;
            }
            else
            {
                // Fallback to legacy ghost text content
                ghostTextToRender = m_ghostTextContent;
            }
        }
        
        std::istringstream stream(ghostTextToRender);
        std::string line;
        int lineCount = 0;
        while (std::getline(stream, line) && lineCount < GHOST_TEXT_MAX_LINES)
        {
            lines.push_back(line);
            lineCount++;
        }
    }

    if (lines.empty())
    {
        hideGhostDiffOverlayUi();
        return;
    }

    // Get line height for multi-line offsets
    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;

    int drawX = endPt.x + 2;
    int drawY = endPt.y;

    RECT editorRC;
    GetClientRect(m_hwndEditor, &editorRC);

    for (size_t i = 0; i < lines.size(); i++)
    {
        int lineDrawX = drawX;
        int lineDrawY = drawY;

        if (i > 0)
        {
            POINTL lineStartPt;
            SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)lineStart, (LPARAM)&lineStartPt);
            lineDrawX = lineStartPt.x;
            lineDrawY += static_cast<int>(i) * lineHeight;
        }

        if (lineDrawY + lineHeight > editorRC.bottom)
            break;

        // OPTIMIZED: Call MASM Layout Engine directly
        DrawGhostTextFast(hdc, lineDrawX, lineDrawY, lines[i].c_str());
    }

    if (!showPagingStatus)
        updateGhostDiffOverlayUi(endPt);
    else
        hideGhostDiffOverlayUi();
}

// ============================================================================
// KEY HANDLER — intercepts Tab/Esc in editor subclass
// ============================================================================

bool Win32IDE::handleGhostTextKey(UINT vk)
{
    if (vk == VK_ESCAPE)
    {
        bool titanRunning = false;
        {
            std::lock_guard<std::mutex> lock(m_titanAgentMutex);
            titanRunning = m_titanAgentRunning;
        }
        if (titanRunning)
        {
            cancelTitanAgentInferenceAsync();
            dismissGhostText();
            cancelGhostTextInferenceRequests();
            postOutputPanelSafe("\n[Titan Agent] canceled by ESC\n");
            return true;
        }
    }

    if (!m_ghostTextVisible)
        return false;

    if (vk == VK_TAB)
    {
        bool titanRunning = false;
        {
            std::lock_guard<std::mutex> lock(m_titanAgentMutex);
            titanRunning = m_titanAgentRunning;
        }
        acceptGhostText();
        if (titanRunning)
        {
            // Promote currently streamed suggestion into concrete code and stop further streaming.
            cancelTitanAgentInferenceAsync();
            postOutputPanelSafe("\n[Titan Agent] committed current ghost text (TAB)\n");
        }
        return true;  // Consumed
    }

    if (vk == VK_ESCAPE)
    {
        dismissGhostText();
        return true;  // Consumed
    }

    // Keep ghost visible for printable keys; WM_CHAR will trim matching prefix.
    if (IsLikelyPrintableKey(vk))
    {
        return false;
    }

    // Any other key dismisses ghost text (typing, arrow keys, etc.)
    // Exception: don't dismiss on Shift/Ctrl/Alt alone
    if (vk != VK_SHIFT && vk != VK_CONTROL && vk != VK_MENU)
    {
        dismissGhostText();
    }

    return false;  // Not consumed — pass to editor
}

bool Win32IDE::handleGhostTextTypedChar(wchar_t ch)
{
    if (!m_ghostTextVisible)
        return false;

    if (m_ghostTextRenderScheduled || !m_pendingGhostAppend.empty())
    {
        onGhostTextRenderMessage();
        if (!m_ghostTextVisible)
        {
            return false;
        }
    }

    // Ignore control characters; Tab/Esc are handled in keydown.
    if (ch < 0x20 || ch == L'\t' || ch == 0x1B)
        return false;

    // ── UX IMPROVEMENT: Cursor Position Validation ──────────────────────────
    // Verify cursor hasn't moved since ghost text was rendered.
    // If cursor moved, the ghost text is stale and should be dismissed.
    CHARRANGE currentSel{};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&currentSel));
    if (currentSel.cpMin != currentSel.cpMax)
    {
        // Do not trim/rebase ghost text while the user is replacing a selection.
        dismissGhostText();
        return false;
    }
    if (m_ghostTextRequestCursorPos >= 0 && currentSel.cpMin != m_ghostTextRequestCursorPos)
    {
        // Cursor moved - ghost text is stale, dismiss it
        dismissGhostText();
        return false;
    }

    std::string typedUtf8;
    {
        wchar_t in[2] = {ch, 0};
        char out[8] = {0};
        const int n = WideCharToMultiByte(CP_UTF8, 0, in, 1, out, (int)sizeof(out), nullptr, nullptr);
        if (n <= 0)
            return false;
        typedUtf8.assign(out, out + n);
    }

    std::string& renderText = m_ghostTextContent;
    std::string& commitText = m_ghostTextCommitContent.empty() ? m_ghostTextContent : m_ghostTextCommitContent;
    if (commitText.empty())
        return false;

    // ── UX IMPROVEMENT: Merge Rules with Fuzzy Matching ──────────────────────
    // Check if typed char matches the first char of ghost text (case-insensitive for letters)
    const bool exactMatch =
        (commitText.size() >= typedUtf8.size() && commitText.compare(0, typedUtf8.size(), typedUtf8) == 0);
    
    // Case-insensitive match for single letters (common in IDE typing)
    bool caseInsensitiveMatch = false;
    if (!exactMatch && typedUtf8.size() == 1 && commitText.size() >= 1)
    {
        const char typed = typedUtf8[0];
        const char ghost = commitText[0];
        // Match letters case-insensitively
        if (std::isalpha(static_cast<unsigned char>(typed)) && std::isalpha(static_cast<unsigned char>(ghost)))
        {
            caseInsensitiveMatch = (std::tolower(static_cast<unsigned char>(typed)) == 
                                    std::tolower(static_cast<unsigned char>(ghost)));
        }
    }

    if (!exactMatch && !caseInsensitiveMatch)
    {
        // Typed char doesn't match ghost text prefix - dismiss
        dismissGhostText();
        return false;
    }

    // Trim the matched prefix from ghost text
    auto trimPrefix = [&](std::string& s, size_t len) {
        if (s.size() >= len)
            s.erase(0, len);
    };
    
    const size_t trimLen = exactMatch ? typedUtf8.size() : 1;
    trimPrefix(commitText, trimLen);
    if (&renderText != &commitText)
        trimPrefix(renderText, trimLen);
    trimPrefix(m_ghostTextBuffer, trimLen);
    trimPrefix(m_pendingGhostAppend, trimLen);

    // Update cursor position tracking for subsequent chars
    m_ghostTextRequestCursorPos = currentSel.cpMin + static_cast<LONG>(typedUtf8.size());
    m_ghostTextSelectionSnapshot = MakeCollapsedSelectionSnapshot(static_cast<LONG>(m_ghostTextRequestCursorPos));
    m_ghostTextRequestLinePrefix += typedUtf8;

    // If fully consumed by typed prefix, dismiss ghost cleanly.
    if (commitText.empty() && renderText.empty())
    {
        dismissGhostText();
        return false;
    }

    m_activeSuggestionContext.range.cpMin = m_ghostTextRequestCursorPos;
    m_activeSuggestionContext.range.cpMax =
        m_ghostTextRequestCursorPos + static_cast<LONG>((!m_ghostTextContent.empty() ? m_ghostTextContent : m_ghostTextCommitContent).size());
    m_activeSuggestionContext.preview = !m_ghostTextContent.empty() ? m_ghostTextContent : m_ghostTextCommitContent;
    m_activeSuggestionContext.stateChangedTickMs = static_cast<uint64_t>(GetTickCount64());

    if (m_hwndEditor)
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    return false;
}

// ============================================================================
// TOGGLE — enable/disable ghost text
// ============================================================================

void Win32IDE::toggleGhostText()
{
    m_ghostTextEnabled = !m_ghostTextEnabled;
    if (!m_ghostTextEnabled)
    {
        dismissGhostText();
    }
    appendToOutput(std::string("[Ghost Text] ") + (m_ghostTextEnabled ? "Enabled" : "Disabled"), "General",
                   OutputSeverity::Info);
}

// ============================================================================
// TRIM HELPER — truncates completion to reasonable size
// ============================================================================

std::string Win32IDE::trimGhostText(const std::string& raw)
{
    std::string result;
    int lineCount = 0;
    int charCount = 0;

    for (char c : raw)
    {
        if (charCount >= GHOST_TEXT_MAX_CHARS)
            break;
        if (c == '\n')
        {
            lineCount++;
            if (lineCount >= GHOST_TEXT_MAX_LINES)
                break;
        }
        result += c;
        charCount++;
    }

    // Strip trailing whitespace
    while (!result.empty() && (result.back() == ' ' || result.back() == '\n' || result.back() == '\r'))
    {
        result.pop_back();
    }

    return result;
}
