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

#include "Win32IDE.h"
#include "../RawrXD_Exports.h"
#include "../../include/ghost_text_renderer.h"
#include "IDELogger.h"
#include <richedit.h>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <functional>
#include <array>
#include <cctype>
#include <vector>

#include "../agentic/OllamaProvider.h"
#include "../agentic/OrchestratorBridge.h"

// ============================================================================
// CONSTANTS
// ============================================================================
static const UINT_PTR GHOST_TEXT_TIMER_ID   = 8888;
static const UINT_PTR TITAN_PAGING_HEARTBEAT_TIMER_ID = 8889;
static const UINT      GHOST_TEXT_DELAY_MS  = 77;    // Calibrated: Sprint TTFT p50=67ms + 10ms margin (Phase 14.2 A/B sweep)
static const int       GHOST_TEXT_MAX_CHARS = 512;    // Max ghost text length
static const int       GHOST_TEXT_MAX_LINES = 8;      // Max multi-line completions
static const uint64_t  GHOST_TEXT_CACHE_TTL_MS = 5000;  // 5s for 70B inference latency
static const size_t    GHOST_TEXT_CACHE_MAX_ITEMS = 256;

namespace {
struct TitanGhostExports {
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

bool resolveTitanGhostExports() {
    if (g_titanGhost.module) {
        return true;
    }

    static const std::array<const char*, 4> kTitanDllCandidates = {
        "D:\\rawrxd\\build_smoke_verify2\\bin\\RawrXD_Titan.dll",
        "D:\\rawrxd\\bin\\RawrXD_Titan.dll",
        "RawrXD_Titan.dll",
        "bin\\RawrXD_Titan.dll"
    };

    const char* loadedPath = nullptr;
    for (const char* candidate : kTitanDllCandidates) {
        g_titanGhost.module = LoadLibraryA(candidate);
        if (g_titanGhost.module) {
            loadedPath = candidate;
            break;
        }
    }
    if (!g_titanGhost.module) {
        LOG_WARNING("Titan DLL load failed for all candidate paths");
        return false;
    }

    if (loadedPath) {
        LOG_INFO(std::string("Titan DLL loaded from: ") + loadedPath);
    }

    g_titanGhost.initialize = reinterpret_cast<decltype(g_titanGhost.initialize)>(
        GetProcAddress(g_titanGhost.module, "RawrXD_Initialize"));
    g_titanGhost.shutdown = reinterpret_cast<decltype(g_titanGhost.shutdown)>(
        GetProcAddress(g_titanGhost.module, "RawrXD_Shutdown"));
    g_titanGhost.loadModel = reinterpret_cast<decltype(g_titanGhost.loadModel)>(
        GetProcAddress(g_titanGhost.module, "RawrXD_LoadModel"));
    g_titanGhost.selectModel = reinterpret_cast<decltype(g_titanGhost.selectModel)>(
        GetProcAddress(g_titanGhost.module, "RawrXD_SelectModel"));
    g_titanGhost.setSamplingParams = reinterpret_cast<decltype(g_titanGhost.setSamplingParams)>(
        GetProcAddress(g_titanGhost.module, "RawrXD_SetSamplingParams"));
    g_titanGhost.inferAsync = reinterpret_cast<decltype(g_titanGhost.inferAsync)>(
        GetProcAddress(g_titanGhost.module, "RawrXD_InferAsync"));
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
    g_titanGhost.streamPop = reinterpret_cast<decltype(g_titanGhost.streamPop)>(
        GetProcAddress(g_titanGhost.module, "RawrXD_StreamPop"));
    g_titanGhost.streamReset = reinterpret_cast<decltype(g_titanGhost.streamReset)>(
        GetProcAddress(g_titanGhost.module, "RawrXD_StreamReset"));

    const bool hasCoreInference =
        g_titanGhost.initialize && g_titanGhost.shutdown && g_titanGhost.loadModel &&
        g_titanGhost.selectModel && g_titanGhost.setSamplingParams && g_titanGhost.inferAsync &&
        g_titanGhost.waitForInference && g_titanGhost.beginStreaming && g_titanGhost.endStreaming;
    const bool hasStreamBridge =
        g_titanGhost.streamConfigureWindow && g_titanGhost.streamPop && g_titanGhost.streamReset;

    if (hasCoreInference && !hasStreamBridge) {
        LOG_INFO("Titan DLL loaded without stream bridge exports; ghost stream provider disabled for this lane");
    }

    return hasCoreInference && hasStreamBridge;
}

bool ensureTitanGhostReady(const std::string& modelPath) {
    if (modelPath.empty() || !resolveTitanGhostExports()) {
        return false;
    }
    if (!g_titanGhost.initialized) {
        if (g_titanGhost.initialize() != RAWRXD_SUCCESS) {
            return false;
        }
        g_titanGhost.initialized = true;
    }
    if (g_titanGhost.loadedModelPath == modelPath && g_titanGhost.activeModel != 0) {
        return true;
    }

    RAWRXD_MODEL_HANDLE modelHandle = 0;
    if (g_titanGhost.loadModel(modelPath.c_str(), &modelHandle) != RAWRXD_SUCCESS || modelHandle == 0) {
        return false;
    }
    if (g_titanGhost.selectModel(modelHandle) != RAWRXD_SUCCESS) {
        return false;
    }

    g_titanGhost.activeModel = modelHandle;
    g_titanGhost.loadedModelPath = modelPath;
    return true;
}

void drainTitanGhostPackets(std::string& out,
                            uint64_t* inoutLastSeq,
                            uint64_t* inoutGapCount,
                            uint64_t* inoutPacketCount);

} // namespace

bool Win32IDE::startTitanAgentInferenceAsync(const std::string& prompt, uint32_t timeoutMs, bool stageOnly) {
    if (prompt.empty() || !m_useTitanKernel || m_loadedModelPath.empty() || !m_hwndMain) {
        return false;
    }
    if (!ensureTitanGhostReady(m_loadedModelPath)) {
        postOutputPanelSafe("[Titan Agent] Titan backend unavailable\n");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        if (m_titanAgentRunning) {
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

    if (m_hwndEditor) {
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
    if (g_titanGhost.setSamplingParams) {
        g_titanGhost.setSamplingParams(&sampling);
    }

    RAWRXD_INFERENCE_HANDLE streamHandle = 0;
    if (g_titanGhost.beginStreaming(&streamHandle) != RAWRXD_SUCCESS) {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        m_titanAgentRunning = false;
        postOutputPanelSafe("[Titan Agent] beginStreaming failed\n");
        return false;
    }

    if (g_titanGhost.streamReset) {
        g_titanGhost.streamReset();
    }
    g_titanGhost.streamConfigureWindow(reinterpret_cast<uint64_t>(m_hwndMain), WM_TITAN_AGENT_STREAM, 0);

    RAWRXD_INFERENCE_HANDLE inferenceHandle = 0;
    if (g_titanGhost.inferAsync(prompt.c_str(), prompt.size(), &inferenceHandle) != RAWRXD_SUCCESS) {
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

    postOutputPanelSafe(stageOnly
                            ? "[Titan Agent] preview inference started (staging only)\n"
                            : "[Titan Agent] async inference started\n");

    if (m_hwndStatusBar && IsWindow(m_hwndStatusBar)) {
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L" [Titan] Paging... 0%");
    }

    // Start the paging heartbeat (displays aperture utilization)
    startTitanPagingHeartbeat(timeoutMs);

    HWND mainHwnd = m_hwndMain;
    std::thread([mainHwnd, inferenceHandle, timeoutMs]() {
        RAWRXD_STATUS status = g_titanGhost.waitForInference(inferenceHandle, timeoutMs);
        if (mainHwnd && IsWindow(mainHwnd)) {
            PostMessageA(mainHwnd, WM_TITAN_AGENT_DONE, (WPARAM)status, 0);
        }
    }).detach();

    return true;
}

void Win32IDE::cancelTitanAgentInferenceAsync() {
    uint64_t handle = 0;
    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        handle = m_titanAgentInferenceHandle;
    }
    if (handle != 0 && g_titanGhost.cancelInference) {
        g_titanGhost.cancelInference((RAWRXD_INFERENCE_HANDLE)handle);
        if (m_hwndStatusBar && IsWindow(m_hwndStatusBar)) {
            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L" [Titan] cancel requested");
        }
    }
}

void Win32IDE::onTitanAgentStreamMessage() {
    std::string combined;
    size_t postedChars = 0;
    int anchorPos = -1;
    bool stageOnly = false;
    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        if (!m_titanAgentRunning) {
            return;
        }
        drainTitanGhostPackets(m_titanAgentStreamText,
                               &m_titanAgentStreamSeq,
                               &m_titanAgentSeqGaps,
                               &m_titanAgentPackets);
        combined = m_titanAgentStreamText;
        postedChars = m_titanAgentPostedChars;
        anchorPos = m_titanAgentGhostAnchorPos;
        stageOnly = m_titanAgentStageOnly;
    }

    if (combined.size() <= postedChars) {
        return;
    }

    std::string delta = combined.substr(postedChars);
    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        m_titanAgentPostedChars = combined.size();
    }

    if (!delta.empty()) {
        postOutputPanelSafe(delta);
        uint64_t startMs = 0;
        {
            std::lock_guard<std::mutex> lock(m_titanAgentMutex);
            m_titanAgentLastPacketMs = GetTickCount64();
            startMs = m_titanAgentStartMs;
        }
        if (m_hwndStatusBar && IsWindow(m_hwndStatusBar) && startMs != 0) {
            const int elapsedSec = (int)((GetTickCount64() - startMs) / 1000);
            wchar_t sbuf[96] = {};
            swprintf_s(sbuf, 96, L" [Titan] Paging shards... %ds", elapsedSec);
            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)sbuf);
        }
    }

    if (stageOnly) {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        m_titanAgentStagedText = combined;
        return;
    }

    if (!m_hwndEditor || anchorPos < 0) {
        return;
    }

    CHARRANGE sel{};
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
    if (sel.cpMin != anchorPos) {
        return;
    }

    m_ghostTextRequestCursorPos = anchorPos;
    m_ghostTextContent = trimGhostText(combined);
    if (m_ghostTextContent.empty()) {
        m_ghostTextVisible = false;
        return;
    }
    m_ghostTextVisible = true;
    m_ghostTextAccepted = false;
    InvalidateRect(m_hwndEditor, nullptr, FALSE);
}

void Win32IDE::onTitanAgentDone(int status) {
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

    if (m_hwndMain) {
        KillTimer(m_hwndMain, TITAN_PAGING_HEARTBEAT_TIMER_ID);
    }

    // Stop the paging heartbeat
    stopTitanPagingHeartbeat();

    if (streamHandle != 0) {
        g_titanGhost.endStreaming((RAWRXD_INFERENCE_HANDLE)streamHandle);
    }

    if (status == (int)RAWRXD_SUCCESS) {
        postOutputPanelSafe("\n[Titan Agent] completed\n");
        if (m_hwndStatusBar && IsWindow(m_hwndStatusBar)) {
            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Ready");
        }
    } else {
        postOutputPanelSafe("\n[Titan Agent] failed status=" + std::to_string((int)status) + "\n");
        if (m_hwndStatusBar && IsWindow(m_hwndStatusBar)) {
            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L" [Titan] failed");
        }
    }

    if (stageOnly && status == (int)RAWRXD_SUCCESS && m_hwndEditor) {
        const std::string stagedTrimmed = trimGhostText(stagedText);
        if (!stagedTrimmed.empty()) {
            std::string preview = stagedTrimmed.substr(0, 900);
            if (stagedTrimmed.size() > preview.size()) {
                preview += "\n...[truncated]";
            }

            std::string prompt =
                "Sovereign Diff Preview\n\n"
                "Review staged Titan output before apply.\n"
                "Press OK to apply to editor selection/cursor, or Cancel to discard.\n\n"
                "--- Preview ---\n" +
                preview;

            const int choice = MessageBoxA(m_hwndMain ? m_hwndMain : m_hwndEditor,
                                           prompt.c_str(),
                                           "RawrXD Sovereign Diff",
                                           MB_OKCANCEL | MB_ICONQUESTION);
            if (choice == IDOK) {
                CHARRANGE replaceSel{};
                replaceSel.cpMin = (stageSelStart >= 0) ? stageSelStart : 0;
                replaceSel.cpMax = (stageSelEnd >= 0) ? stageSelEnd : replaceSel.cpMin;
                SendMessageA(m_hwndEditor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&replaceSel));
                SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(stagedTrimmed.c_str()));
                markFileModified();
                postOutputPanelSafe("[Titan Agent] staged preview applied\n");
            } else {
                postOutputPanelSafe("[Titan Agent] staged preview discarded\n");
            }
        }
    }

    if (gapCount > 0) {
        postOutputPanelSafe("[Titan Agent] stream seq gaps=" + std::to_string(gapCount) +
                            " packets=" + std::to_string(packetCount) +
                            " last_seq=" + std::to_string(lastSeq) + "\n");
    }
}

namespace {

void drainTitanGhostPackets(std::string& out,
                            uint64_t* inoutLastSeq,
                            uint64_t* inoutGapCount,
                            uint64_t* inoutPacketCount) {
    if (!g_titanGhost.streamPop) {
        return;
    }

    char buffer[513] = {};
    uint32_t chunkLen = 0;
    uint64_t seq = 0;
    uint64_t lastSeq = inoutLastSeq ? *inoutLastSeq : 0;
    uint64_t gapCount = inoutGapCount ? *inoutGapCount : 0;
    uint64_t packetCount = inoutPacketCount ? *inoutPacketCount : 0;
    while (g_titanGhost.streamPop(buffer, sizeof(buffer), &chunkLen, &seq) == RAWRXD_SUCCESS && chunkLen > 0) {
        if (lastSeq != 0 && seq > lastSeq + 1) {
            gapCount += (seq - (lastSeq + 1));
        }
        lastSeq = seq;
        packetCount += 1;
        out.append(buffer, buffer + chunkLen);
        chunkLen = 0;
        buffer[0] = '\0';
    }

    if (inoutLastSeq) *inoutLastSeq = lastSeq;
    if (inoutGapCount) *inoutGapCount = gapCount;
    if (inoutPacketCount) *inoutPacketCount = packetCount;
}

uint64_t nowMs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string buildGhostCacheKey(const std::string& filePath,
                               const std::string& language,
                               const std::string& prefix,
                               const std::string& suffix,
                               int line, int column) {
    const size_t prefixWindow = (prefix.size() > 512) ? 512 : prefix.size();
    const size_t suffixWindow = (suffix.size() > 256) ? 256 : suffix.size();
    const std::string prefixTail = prefix.substr(prefix.size() - prefixWindow, prefixWindow);
    const std::string suffixHead = suffix.substr(0, suffixWindow);
    const std::string seed = filePath + "|" + language + "|" + std::to_string(line) + "|" +
                             std::to_string(column) + "|" + prefixTail + "|" + suffixHead;
    return std::to_string(std::hash<std::string>{}(seed));
}

std::string trimLeftCopy(const std::string& in) {
    size_t i = 0;
    while (i < in.size() && std::isspace(static_cast<unsigned char>(in[i])) != 0) ++i;
    return in.substr(i);
}

std::string lowerCopy(const std::string& in) {
    std::string out = in;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string getLinePrefix(const std::string& context) {
    const size_t pos = context.find_last_of('\n');
    if (pos == std::string::npos) return context;
    if (pos + 1 >= context.size()) return "";
    return context.substr(pos + 1);
}

std::string buildSnippetCompletion(const std::string& context, const std::string& language) {
    const std::string line = trimLeftCopy(getLinePrefix(context));
    const std::string lowered = lowerCopy(line);
    const std::string lang = lowerCopy(language);

    if (lowered == "if") {
        return " () {\n    \n}";
    }
    if (lowered == "for") {
        return " (int i = 0; i < ; ++i) {\n    \n}";
    }
    if (lowered == "while") {
        return " () {\n    \n}";
    }
    if ((lang == "c++" || lang == "cpp" || lang == "c") && lowered == "switch") {
        return " () {\ncase :\n    break;\ndefault:\n    break;\n}";
    }
    return "";
}

std::string wideToUtf8Local(const wchar_t* wide) {
    if (!wide || !*wide) {
        return "";
    }

    const int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 1) {
        return "";
    }

    std::string utf8(static_cast<size_t>(utf8Len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), utf8Len, nullptr, nullptr);
    return utf8;
}

std::string getEditorRangeUtf8(HWND editor, LONG cpMin, LONG cpMax) {
    if (!editor || cpMax <= cpMin) {
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
} // namespace

// ============================================================================
// INITIALIZATION
// ============================================================================

void Win32IDE::initGhostText() {
    // Ghost text enabled by default (heap corruption fixed in render path).
    m_ghostTextEnabled        = true;
    m_ghostTextVisible        = false;
    m_ghostTextAccepted       = false;
    m_ghostTextPending        = false;
    m_ghostTextContent.clear();
    m_ghostTextLine           = -1;
    m_ghostTextColumn         = -1;
    m_ghostTextFont           = nullptr;

    // Wire ghost text overlay to editor so overlay can render/draw when used
    if (m_ghostTextRendererOverlay && m_hwndEditor) {
        m_ghostTextRendererOverlay->setEditorHwnd(m_hwndEditor);
        m_ghostTextRendererOverlay->initialize();
    }

    // Create ghost text font — italic version of editor font (DPI-scaled)
    LOGFONTA lf = {};
    lf.lfHeight         = -dpiScale(14);    // ~10.5pt at 96 DPI
    lf.lfWeight         = FW_NORMAL;
    lf.lfItalic         = TRUE;
    lf.lfCharSet        = DEFAULT_CHARSET;
    lf.lfQuality        = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    strncpy(lf.lfFaceName, m_currentTheme.fontName.c_str(), LF_FACESIZE - 1);
    lf.lfFaceName[LF_FACESIZE - 1] = '\0';
    m_ghostTextFont = CreateFontIndirectA(&lf);

    LOG_INFO("Ghost text renderer initialized (debounce=" +
             std::to_string(GHOST_TEXT_DELAY_MS) + "ms, default=enabled)");
}

void Win32IDE::shutdownGhostText() {
    {
        std::lock_guard<std::mutex> lock(m_titanGhostMutex);
        m_titanGhostStreamText.clear();
        m_titanGhostStreamActive = false;
    }
    dismissGhostText();
    if (m_ghostTextFont) {
        DeleteObject(m_ghostTextFont);
        m_ghostTextFont = nullptr;
    }
}

// ============================================================================
// DEBOUNCE TRIGGER — called from onEditorContentChanged()
// ============================================================================

void Win32IDE::triggerGhostTextCompletion() {
    if (!m_ghostTextEnabled || !m_hwndEditor) return;

    // Kill any existing ghost text timer and dismiss current ghost text
    KillTimer(m_hwndMain, GHOST_TEXT_TIMER_ID);
    ++m_ghostTextRequestSeq;
    dismissGhostText();

    // Start a new debounce timer
    SetTimer(m_hwndMain, GHOST_TEXT_TIMER_ID, GHOST_TEXT_DELAY_MS, nullptr);
}

// ============================================================================
// TIMER CALLBACK — fires after debounce period, requests completion
// ============================================================================

void Win32IDE::onGhostTextTimer() {
    KillTimer(m_hwndMain, GHOST_TEXT_TIMER_ID);

    if (!m_ghostTextEnabled || !m_hwndEditor) return;
    if (m_ghostTextPending) return;  // Already requesting

    // Gather context: text before cursor
    CHARRANGE sel;
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    int cursorPos = sel.cpMin;

    if (cursorPos <= 0) return;

    // Get up to 4KB of text before cursor for context
    int contextStart = (cursorPos > 4096) ? cursorPos - 4096 : 0;
    int contextLen = cursorPos - contextStart;

    std::string context = getEditorRangeUtf8(m_hwndEditor, contextStart, cursorPos);
    if (context.empty()) return;

    // Get current line/column for positioning
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, cursorPos, 0);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column = cursorPos - lineStart;

    // Store cursor position for ghost text rendering
    m_ghostTextLine   = lineIndex;
    m_ghostTextColumn = column;
    m_ghostTextRequestCursorPos = cursorPos;
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
    if (suffixLen > 0) {
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
        if (it != m_ghostTextCache.end() && (nowMs() - it->second.createdAtMs) <= GHOST_TEXT_CACHE_TTL_MS) {
            m_ghostTextMetrics.cacheHits++;
            m_ghostTextPending = false;
            onGhostTextReady(cursorCopy, it->second.completion.c_str());
            return;
        }
    }

    std::thread([this, contextCopy, suffixCopy, langCopy, fileCopy, cursorCopy, lineCopy, colCopy, requestSeq]() {
        DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
        if (_guard.cancelled) return;
        const uint64_t startedAt = nowMs();
        std::string completion = requestGhostTextCompletion(
            contextCopy, langCopy, suffixCopy, fileCopy, lineCopy, colCopy, requestSeq);
        const uint64_t elapsedMs = nowMs() - startedAt;

        if (requestSeq != m_ghostTextRequestSeq.load()) {
            std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
            m_ghostTextMetrics.staleDrops++;
            if (!isShuttingDown()) {
                PostMessageA(m_hwndMain, WM_GHOST_TEXT_READY, (WPARAM)cursorCopy, (LPARAM)nullptr);
            }
            return;
        }

        if (!completion.empty()) {
            std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
            if (m_ghostTextCache.size() >= GHOST_TEXT_CACHE_MAX_ITEMS) {
                m_ghostTextCache.erase(m_ghostTextCache.begin());
            }
            const std::string key = buildGhostCacheKey(fileCopy, langCopy, contextCopy, suffixCopy, lineCopy, colCopy);
            m_ghostTextCache[key] = GhostTextCacheEntry{completion, nowMs()};
            m_ghostTextMetrics.lastLatencyMs = (double)elapsedMs;
            const double reqCount = (double)std::max<uint64_t>(1, m_ghostTextMetrics.requests - m_ghostTextMetrics.cacheHits);
            m_ghostTextMetrics.avgLatencyMs =
                ((m_ghostTextMetrics.avgLatencyMs * (reqCount - 1.0)) + (double)elapsedMs) / reqCount;
        }

        // Post result to UI thread
        if (isShuttingDown()) return;
        if (!completion.empty()) {
            char* heapText = _strdup(completion.c_str());
            PostMessageA(m_hwndMain, WM_GHOST_TEXT_READY, (WPARAM)cursorCopy, (LPARAM)heapText);
        } else {
            PostMessageA(m_hwndMain, WM_GHOST_TEXT_READY, (WPARAM)cursorCopy, (LPARAM)nullptr);
        }
    }).detach();
}

// ============================================================================
// COMPLETION REQUEST — runs on background thread
// ============================================================================

std::string Win32IDE::requestGhostTextCompletion(const std::string& context,
                                                   const std::string& language) {
    // Legacy 2-arg overload — forward to FIM-aware version with empty suffix
    return requestGhostTextCompletion(context, language, "", "", 0, 0);
}

std::string Win32IDE::requestGhostTextCompletion(const std::string& context,
                                                   const std::string& language,
                                                   const std::string& suffix,
                                                   const std::string& filePath,
                                                   int cursorLine, int cursorCol,
                                                   uint64_t expectedSeq) {
    using namespace RawrXD::Prediction;

    const auto isStale = [this, expectedSeq]() -> bool {
        return expectedSeq != 0 && expectedSeq != m_ghostTextRequestSeq.load();
    };

    enum class GhostProviderKind {
        Titan,
        Local,
        Snippet,
        Lsp
    };
    const std::array<GhostProviderKind, 4> precedence = {
        GhostProviderKind::Titan,
        GhostProviderKind::Local,
        GhostProviderKind::Snippet,
        GhostProviderKind::Lsp
    };

    for (GhostProviderKind provider : precedence) {
        if (isStale()) return "";

        if (provider == GhostProviderKind::Titan) {
            std::string titanCompletion = requestTitanGhostTextCompletion(
                context, language, suffix, filePath, cursorLine, cursorCol, expectedSeq);
            if (!titanCompletion.empty()) {
                std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                m_ghostTextMetrics.localWins++;
                return titanCompletion;
            }
        }

        if (provider == GhostProviderKind::Local) {
            // ---- Primary: OrchestratorBridge FIM (uses AgentOllamaClient + FIMPromptBuilder) ----
            {
                auto& orchBridge = RawrXD::Agent::OrchestratorBridge::Instance();
                RawrXD::Prediction::PredictionContext bCtx;
                bCtx.prefix       = context;
                bCtx.suffix       = suffix;
                bCtx.language     = language;
                bCtx.filePath     = filePath;
                bCtx.cursorLine   = cursorLine;
                bCtx.cursorColumn = cursorCol;

                auto bResult = orchBridge.RequestGhostText(bCtx);
                if (bResult.success && !bResult.completion.empty()) {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.localWins++;
                    return trimGhostText(bResult.completion);
                }
            }

            if (isStale()) return "";

            // ---- Fallback: prediction backend, then native model, then local Ollama prompt ----
            if (!m_predictionProvider) {
                std::string baseUrl = m_ollamaBaseUrl.empty() ? "http://localhost:11434" : m_ollamaBaseUrl;
                m_predictionProvider = std::make_unique<OllamaProvider>(baseUrl);

                PredictionConfig cfg;
                cfg.model       = getResolvedOllamaModel().empty() ? "qwen2.5-coder:14b" : getResolvedOllamaModel();
                cfg.temperature = 0.2f;
                cfg.maxTokens   = 256;
                cfg.maxLines    = GHOST_TEXT_MAX_LINES;
                cfg.useFIM      = true;
                cfg.stopSequences = "<|endoftext|>,<|fim_pad|>,\n\n\n";
                m_predictionProvider->Configure(cfg);
            }

            if (m_predictionProvider->IsAvailable()) {
                PredictionContext ctx;
                ctx.prefix       = context;
                ctx.suffix       = suffix;
                ctx.language     = language;
                ctx.filePath     = filePath;
                ctx.cursorLine   = cursorLine;
                ctx.cursorColumn = cursorCol;

                PredictionResult result = m_predictionProvider->Predict(ctx);
                if (result.success && !result.completion.empty()) {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.localWins++;
                    return trimGhostText(result.completion);
                }
            }

            if (isStale()) return "";

            if (m_nativeEngine && m_nativeEngine->IsModelLoaded()) {
                auto tokens = m_nativeEngine->Tokenize(
                    "Complete the following " + language + " code. Output ONLY the completion, "
                    "no explanation, no markdown:\n\n" + context);

                auto generated = m_nativeEngine->Generate(tokens, 64);
                std::string result = trimGhostText(m_nativeEngine->Detokenize(generated));
                if (!result.empty()) {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.localWins++;
                    return result;
                }
            }

            if (isStale()) return "";

            if (!m_ollamaBaseUrl.empty()) {
                std::string response;
                std::string prompt = "Complete the following " + language + " code. "
                                     "Output ONLY the completion, no explanation, no markdown. "
                                     "Maximum 3 lines:\n\n" + context;
                if (trySendToOllama(prompt, response)) {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.localWins++;
                    return trimGhostText(response);
                }
            }
        }

        if (provider == GhostProviderKind::Snippet) {
            const std::string snippet = trimGhostText(buildSnippetCompletion(context, language));
            if (!snippet.empty()) {
                std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                m_ghostTextMetrics.snippetWins++;
                return snippet;
            }
        }

        if (provider == GhostProviderKind::Lsp) {
            if (filePath.empty()) continue;
            const int lspLine = cursorLine > 0 ? cursorLine - 1 : 0;

            // Fast path: direct LSP completion for current cursor.
            std::string uri = filePathToUri(filePath);
            auto lspItems = lspCompletion(uri, lspLine, cursorCol);
            for (const auto& item : lspItems) {
                if (item.insertText.empty()) continue;
                std::string lspText = trimGhostText(item.insertText);
                if (!lspText.empty()) {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.lspWins++;
                    return lspText;
                }
            }

            // Fallback: hybrid merge if direct LSP did not yield usable insert text.
            auto items = requestHybridCompletion(filePath, lspLine, cursorCol);
            for (const auto& item : items) {
                if (item.insertText.empty()) continue;
                if (item.source != "lsp" && item.source != "merged") continue;
                std::string lspText = trimGhostText(item.insertText);
                if (!lspText.empty()) {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.lspWins++;
                    return lspText;
                }
            }
        }
    }

    return "";
}

std::string Win32IDE::requestTitanGhostTextCompletion(const std::string& context,
                                                      const std::string& language,
                                                      const std::string& suffix,
                                                      const std::string& filePath,
                                                      int cursorLine, int cursorCol,
                                                      uint64_t expectedSeq) {
    if (!m_useTitanKernel || m_loadedModelPath.empty() || !m_hwndMain || !m_hwndEditor) {
        return "";
    }
    if (expectedSeq != 0 && expectedSeq != m_ghostTextRequestSeq.load()) {
        return "";
    }
    if (!ensureTitanGhostReady(m_loadedModelPath)) {
        return "";
    }

    RAWRXD_SAMPLING_PARAMS sampling{};
    sampling.temperature = 0.2f;
    sampling.top_p = 0.95f;
    sampling.top_k = 48;
    sampling.repetition_penalty = 1.05f;
    sampling.max_tokens = 96;
    if (g_titanGhost.setSamplingParams) {
        g_titanGhost.setSamplingParams(&sampling);
    }

    std::string prompt =
        "Complete the following " + language + " code at the cursor. "
        "Return only the completion text with no explanation and no markdown.\n\n"
        "[FILE]\n" + filePath +
        "\n[LINE]\n" + std::to_string(cursorLine) +
        "\n[COLUMN]\n" + std::to_string(cursorCol) +
        "\n[PREFIX]\n" + context +
        "\n[SUFFIX]\n" + suffix +
        "\n[COMPLETION]\n";

    {
        std::lock_guard<std::mutex> lock(m_titanGhostMutex);
        m_titanGhostStreamText.clear();
        m_titanGhostStreamSeq = 0;
        m_titanGhostSeqGaps = 0;
        m_titanGhostPackets = 0;
        m_titanGhostStreamActive = true;
    }

    if (m_agenticBridge) {
        m_agenticBridge->ResetGhostSeqTelemetry();
    }

    if (g_titanGhost.streamReset) {
        g_titanGhost.streamReset();
    }

    RAWRXD_INFERENCE_HANDLE streamHandle = 0;
    if (g_titanGhost.beginStreaming(&streamHandle) != RAWRXD_SUCCESS) {
        uint64_t lastGhostSeq = m_agenticBridge ? m_agenticBridge->GetLastGhostSeq() : 0;
        LOG_INFO("Titan ghost beginStreaming failed; last_seq=" + std::to_string(lastGhostSeq));
        std::lock_guard<std::mutex> lock(m_titanGhostMutex);
        m_titanGhostStreamActive = false;
        return "";
    }

    g_titanGhost.streamConfigureWindow(reinterpret_cast<uint64_t>(m_hwndMain), WM_TITAN_GHOST_STREAM, 0);

    RAWRXD_INFERENCE_HANDLE inferenceHandle = 0;
    if (g_titanGhost.inferAsync(prompt.c_str(), prompt.size(), &inferenceHandle) != RAWRXD_SUCCESS) {
        uint64_t lastGhostSeq = m_agenticBridge ? m_agenticBridge->GetLastGhostSeq() : 0;
        LOG_INFO("Titan ghost inferAsync failed; last_seq=" + std::to_string(lastGhostSeq));
        g_titanGhost.endStreaming(streamHandle);
        std::lock_guard<std::mutex> lock(m_titanGhostMutex);
        m_titanGhostStreamActive = false;
        return "";
    }

    const RAWRXD_STATUS waitStatus = g_titanGhost.waitForInference(inferenceHandle, 12000);
    PostMessageA(m_hwndMain, WM_TITAN_GHOST_STREAM, 0, 0);
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
        m_titanGhostStreamText = completion;
        m_titanGhostStreamSeq = lastSeq;
        m_titanGhostSeqGaps = gapCount;
        m_titanGhostPackets = packetCount;
        m_titanGhostStreamActive = false;
    }

    if (waitStatus != RAWRXD_SUCCESS) {
        uint64_t lastBridgeSeq = m_agenticBridge ? m_agenticBridge->GetLastGhostSeq() : 0;
        LOG_INFO("Titan ghost wait failed; status=" + std::to_string(static_cast<int>(waitStatus)) +
                 " stream_last_seq=" + std::to_string(lastSeq) +
                 " bridge_last_seq=" + std::to_string(lastBridgeSeq) +
                 " gaps=" + std::to_string(gapCount) +
                 " packets=" + std::to_string(packetCount));
        return "";
    }
    return trimGhostText(completion);
}

void Win32IDE_HandleTitanGhostStreamMessage(Win32IDE* ide) {
    if (!ide) {
        return;
    }

    std::string combined;
    bool streamActive = false;
    uint64_t prevGapCount = 0;
    uint64_t lastSeq = 0;
    uint64_t gapCount = 0;
    uint64_t packetCount = 0;

    {
        std::lock_guard<std::mutex> lock(ide->m_titanGhostMutex);
        prevGapCount = ide->m_titanGhostSeqGaps;
        lastSeq = ide->m_titanGhostStreamSeq;
        gapCount = ide->m_titanGhostSeqGaps;
        packetCount = ide->m_titanGhostPackets;
        drainTitanGhostPackets(ide->m_titanGhostStreamText, &lastSeq, &gapCount, &packetCount);
        ide->m_titanGhostStreamSeq = lastSeq;
        ide->m_titanGhostSeqGaps = gapCount;
        ide->m_titanGhostPackets = packetCount;
        combined = ide->m_titanGhostStreamText;
        streamActive = ide->m_titanGhostStreamActive;
    }

    if (gapCount > prevGapCount) {
        LOG_INFO("Titan ghost stream sequence gap detected; last_seq=" + std::to_string(lastSeq) +
                 " gaps=" + std::to_string(gapCount) +
                 " packets=" + std::to_string(packetCount));
    }

    if (ide->m_agenticBridge) {
        ide->m_agenticBridge->ObserveGhostStreamSeq(lastSeq);
    }

    if (!streamActive || combined.empty() || !ide->m_hwndEditor) {
        return;
    }

    CHARRANGE sel{};
    SendMessageA(ide->m_hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
    if (sel.cpMin != ide->m_ghostTextRequestCursorPos) {
        return;
    }

    ide->m_ghostTextContent = ide->trimGhostText(combined);
    if (ide->m_ghostTextContent.empty()) {
        return;
    }

    ide->m_ghostTextVisible = true;
    ide->m_ghostTextAccepted = false;
    InvalidateRect(ide->m_hwndEditor, nullptr, FALSE);
}

// ============================================================================
// GHOST TEXT DELIVERY — WM_GHOST_TEXT_READY handler (UI thread)
// ============================================================================

void Win32IDE::onGhostTextReady(int requestedCursorPos, const char* completionText) {
    m_ghostTextPending = false;

    if (!completionText || !m_hwndEditor) return;

    // Verify cursor hasn't moved since request
    CHARRANGE sel;
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);

    if (sel.cpMin != requestedCursorPos) {
        // Cursor moved — discard stale completion
        return;
    }

    // ARCHITECTURE ALIGNMENT: 
    // Directly update state and invalidate for RichEdit overlay rendering.
    // Ensure we don't just store, but activate the 'visible' state for WM_PAINT.
    m_ghostTextContent = completionText;
    m_ghostTextVisible = true;
    m_ghostTextAccepted = false;

    // Align with MASM Sovereign logic: trigger immediate repaint
    if (m_hwndEditor) {
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
        UpdateWindow(m_hwndEditor); // Force immediate paint to minimize flicker
    }

    // Record event
    recordEvent(AgentEventType::GhostTextRequested, "",
                m_ghostTextContent.substr(0, 128), "", 0, true);
}

// ============================================================================
// DISMISS — clears ghost text
// ============================================================================

void Win32IDE::dismissGhostText() {
    if (!m_ghostTextVisible) return;

    m_ghostTextVisible  = false;
    m_ghostTextContent.clear();
    m_ghostTextLine     = -1;
    m_ghostTextColumn   = -1;
    m_ghostTextRequestCursorPos = -1;
    m_ghostTextAccepted = false;

    if (m_hwndEditor) {
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    }
}

// ============================================================================
// ACCEPT — Tab key inserts the ghost text into the editor
// ============================================================================

void Win32IDE::acceptGhostText() {
    if (!m_ghostTextVisible || m_ghostTextContent.empty() || !m_hwndEditor) return;

    std::string textToInsert = m_ghostTextContent;
    m_ghostTextAccepted = true;

    // Dismiss first to avoid re-rendering ghost during insert
    dismissGhostText();

    // Insert text at current cursor position
    SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)textToInsert.c_str());

    // Record acceptance event
    recordEvent(AgentEventType::GhostTextAccepted, "",
                textToInsert.substr(0, 128), "", 0, true);

    LOG_INFO("Ghost text accepted (" + std::to_string(textToInsert.size()) + " chars)");
}

// ============================================================================
// RENDER — paints ghost text onto the editor surface
// ============================================================================

void Win32IDE::renderGhostText(HDC hdc) {
    if (!m_hwndEditor) return;

    bool showPagingStatus = false;
    uint64_t elapsedMs = 0;
    {
        std::lock_guard<std::mutex> lock(m_titanAgentMutex);
        if (m_titanAgentRunning) {
            const uint64_t now = GetTickCount64();
            const uint64_t startMs = m_titanAgentStartMs;
            const uint64_t lastPacketMs = m_titanAgentLastPacketMs;
            elapsedMs = (startMs > 0 && now >= startMs) ? (now - startMs) : 0;
            const uint64_t sinceLastPacketMs = (lastPacketMs > 0 && now >= lastPacketMs) ? (now - lastPacketMs) : elapsedMs;
            showPagingStatus = (m_titanAgentPostedChars == 0) || (sinceLastPacketMs >= 1200);
        }
    }

    if ((!m_ghostTextVisible || m_ghostTextContent.empty()) && !showPagingStatus) return;

    // Get current cursor position to know where to draw
    CHARRANGE sel;
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);

    // Get the pixel position of the cursor
    POINTL pt;
    SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)sel.cpMin, (LPARAM)&pt);

    if (pt.x < 0 || pt.y < 0) return;  // Cursor not visible

    // Get the text after cursor on the same line to know rendering offset
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int lineLen   = (int)SendMessageA(m_hwndEditor, EM_LINELENGTH, sel.cpMin, 0);
    int lineEnd   = lineStart + lineLen;

    // If cursor is not at end of line, render after end of line text
    // (ghost text appears after existing text)
    POINTL endPt;
    if (lineEnd > sel.cpMin) {
        SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)lineEnd, (LPARAM)&endPt);
    } else {
        endPt = pt;
    }

    // Setup ghost text rendering
    HFONT oldFont = (HFONT)SelectObject(hdc, m_ghostTextFont ? m_ghostTextFont : GetStockObject(SYSTEM_FONT));
    
    // Ghost text color: muted/grayed version of text color
    COLORREF ghostColor = RGB(
        (GetRValue(m_currentTheme.textColor) + GetRValue(m_currentTheme.backgroundColor)) / 2,
        (GetGValue(m_currentTheme.textColor) + GetGValue(m_currentTheme.backgroundColor)) / 2,
        (GetBValue(m_currentTheme.textColor) + GetBValue(m_currentTheme.backgroundColor)) / 2
    );
    SetTextColor(hdc, ghostColor);
    SetBkMode(hdc, TRANSPARENT);

    // Split ghost text into lines
    std::vector<std::string> lines;
    if (showPagingStatus) {
        static const std::array<const char*, 4> kSpinner = {"|", "/", "-", "\\"};
        const int phase = static_cast<int>((GetTickCount64() / 220ULL) % kSpinner.size());
        const uint64_t elapsedSec = elapsedMs / 1000ULL;
        lines.push_back("[Titan] Paging shards " + std::string(kSpinner[phase]) + "  t+" + std::to_string(elapsedSec) + "s");
    } else {
        std::istringstream stream(m_ghostTextContent);
        std::string line;
        int lineCount = 0;
        while (std::getline(stream, line) && lineCount < GHOST_TEXT_MAX_LINES) {
            lines.push_back(line);
            lineCount++;
        }
    }

    if (lines.empty()) {
        SelectObject(hdc, oldFont);
        return;
    }

    // Get line height
    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;

    // Draw first line at cursor/end-of-line position
    int drawX = endPt.x + 2;  // Small gap after existing text
    int drawY = endPt.y;

    // Get editor client rect for clipping
    RECT editorRC;
    GetClientRect(m_hwndEditor, &editorRC);

    for (size_t i = 0; i < lines.size(); i++) {
        if (drawY + lineHeight > editorRC.bottom) break;  // Don't draw below editor

        if (i == 0) {
            // First line: render after cursor
            TextOutA(hdc, drawX, drawY, lines[i].c_str(), (int)lines[i].size());
        } else {
            // Subsequent lines: render at left margin (indented to match cursor column)
            // Get x position of the start of the line (respect indentation)
            POINTL lineStartPt;
        SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)lineStart, (LPARAM)&lineStartPt);
            int indentX = lineStartPt.x;

            drawY += lineHeight;
            TextOutA(hdc, indentX, drawY, lines[i].c_str(), (int)lines[i].size());
        }
    }

    SelectObject(hdc, oldFont);
}

// ============================================================================
// KEY HANDLER — intercepts Tab/Esc in editor subclass
// ============================================================================

bool Win32IDE::handleGhostTextKey(UINT vk) {
    if (vk == VK_ESCAPE) {
        bool titanRunning = false;
        {
            std::lock_guard<std::mutex> lock(m_titanAgentMutex);
            titanRunning = m_titanAgentRunning;
        }
        if (titanRunning) {
            cancelTitanAgentInferenceAsync();
            dismissGhostText();
            postOutputPanelSafe("\n[Titan Agent] canceled by ESC\n");
            return true;
        }
    }

    if (!m_ghostTextVisible) return false;

    if (vk == VK_TAB) {
        bool titanRunning = false;
        {
            std::lock_guard<std::mutex> lock(m_titanAgentMutex);
            titanRunning = m_titanAgentRunning;
        }
        acceptGhostText();
        if (titanRunning) {
            // Promote currently streamed suggestion into concrete code and stop further streaming.
            cancelTitanAgentInferenceAsync();
            postOutputPanelSafe("\n[Titan Agent] committed current ghost text (TAB)\n");
        }
        return true;  // Consumed
    }

    if (vk == VK_ESCAPE) {
        dismissGhostText();
        return true;  // Consumed
    }

    // Any other key dismisses ghost text (typing, arrow keys, etc.)
    // Exception: don't dismiss on Shift/Ctrl/Alt alone
    if (vk != VK_SHIFT && vk != VK_CONTROL && vk != VK_MENU) {
        dismissGhostText();
    }

    return false;  // Not consumed — pass to editor
}

// ============================================================================
// TOGGLE — enable/disable ghost text
// ============================================================================

void Win32IDE::toggleGhostText() {
    m_ghostTextEnabled = !m_ghostTextEnabled;
    if (!m_ghostTextEnabled) {
        dismissGhostText();
    }
    appendToOutput(std::string("[Ghost Text] ") +
                   (m_ghostTextEnabled ? "Enabled" : "Disabled"),
                   "General", OutputSeverity::Info);
}

// ============================================================================
// TRIM HELPER — truncates completion to reasonable size
// ============================================================================

std::string Win32IDE::trimGhostText(const std::string& raw) {
    std::string result;
    int lineCount = 0;
    int charCount = 0;

    for (char c : raw) {
        if (charCount >= GHOST_TEXT_MAX_CHARS) break;
        if (c == '\n') {
            lineCount++;
            if (lineCount >= GHOST_TEXT_MAX_LINES) break;
        }
        result += c;
        charCount++;
    }

    // Strip trailing whitespace
    while (!result.empty() && (result.back() == ' ' || result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }

    return result;
}
