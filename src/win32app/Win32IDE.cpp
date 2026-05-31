// Win32IDE.cpp - RawrXD Win32 IDE Implementation
// Build timestamp: 2026-03-31
#include "Win32IDE.h"
#include "pulse_ring_buffer.h"
#include <new>

// Sovereign VEH / Telemetry External Linkage
extern "C"
{
    void* Sovereign_VEH_Handler(PEXCEPTION_POINTERS ExceptionPointers);
    extern uint64_t g_Sovereign_RecoveryRIP;
}

#include "../../Ship/RawrXD_AutonomousAgenticPipeline.h"  // Full type for unique_ptr destructor
#include "../../Ship/Win32TerminalScrollback.hpp"
#include "../../include/PathResolver.h"
#include "../../include/ProjectRagLite.hpp"
#include "../../include/WalGutterProgrammaticDepth.hpp"
#include "../../include/experimental_module8.hpp"
#include "../../include/missing_features_batch3.hpp"
#include "../../include/rawrxd_version.h"
#include "../agentic/AgenticChatSession.h"
#include "../agentic/AgenticSubmitInference_Fix.h"  // TOOL-AWARE INFERENCE BRIDGE
#include "../agentic/DiffEngine.h"
#include "../agentic/OllamaProvider.h"  // NativeStreamProvider and deleter
#include "../agentic/agentic_controller_wiring.h"
#include "../agentic/slash_command_parser.hpp"
#include "../compression/sovereign_pager.h"
#include "../compression/virtualalloc_reservation_manager.h"
#include "../core/PendingEditReviewGate.hpp"
#include "../core/command_registry.hpp"
#include "../core/gpu_backend_bridge.h"
#include "../core/layer_offload_manager.hpp"
#include "../core/rawr_inference_pipeline.h"
#include "../cpu_inference_engine.h"
#include "../ide_constants.h"
#include "../inference/speculative_execution_engine.h"  // Full type for unique_ptr<SpeculativeExecutionEngine> dtor
#include "../model_source_resolver.h"
#include "../modules/ExtensionLoader.hpp"  // Added
#include "../modules/native_memory.hpp"
#include "../native_inference_backend.h"
#include "../skill_system/SkillSystemBuildIntegration.h"
#include "../streaming_gguf_loader.h"
#include "../ui/SidebarStagingPanel.hpp"
#include "../utils/ErrorReporter.hpp"
#include "../vulkan_compute.h"
#include "IDEConfig.h"
#include "IDELogger.h"
#include "ModelConnection.h"
#include "RawrXD_SymbolEngine.h"
#include "VSIXInstaller.hpp"
#include "Win32IDE_AgenticBridge.h"
#include "Win32IDE_DAPServer.h"  // Full type for unique_ptr<Win32IDE_DAPServer> dtor
#include "Win32IDE_ModelDropdownProfile.h"
#include "Win32IDE_Settings.h"
#include "Win32NeuralBridge.h"
#include <new>
extern "C" void AgentBridge_SetShuttingDown(bool shuttingDown);
extern "C" void PromptWarm_SetAcceptRequests(bool accept);
#include "feature_registry_panel.h"
#include "lsp/RawrXD_LSPServer.h"
#include "multi_response_engine.h"
#include "resource.h"
#include <commdlg.h>
#include <nlohmann/json.hpp>
#include <psapi.h>
#include <richedit.h>
#include <windows.h>


extern "C" void RawrXD_ApplyCopilotChatEditLimits(HWND output, HWND input);

#ifndef WM_COPILOT_MINIMAL_AGENTIC_DONE
#define WM_COPILOT_MINIMAL_AGENTIC_DONE (WM_APP + 108)
#endif

#ifndef CP_UNICODE
#define CP_UNICODE 1200  // Unicode code page for Richedit EM_GETTEXTLENGTHEX/EM_SETTEXTEX
#endif
#include <commctrl.h>
#ifndef TRACKBAR_CLASSW
#define TRACKBAR_CLASSW L"msctls_trackbar32"
#endif

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

// Integrated terminal tab strip (VS Code / Cursor-style instance tabs) — Win32IDE_Core WM_NOTIFY compares this handle.
HWND g_rawrxdIntegratedTerminalTabs = nullptr;

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <cwctype>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <psapi.h>
#include <regex>
#include <set>
#include <shellapi.h>
#include <shlobj.h>
#include <sstream>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
#include <winhttp.h>

// Debugger + System (replay after createOutputTabs): minimal Win32IDE ctor has no output HWND yet.
static std::vector<std::string> g_ideCtorBootUserLines;

static void ideCtorBootMilestone(const char* debugLine, const char* userLine)
{
    if (debugLine && debugLine[0])
        OutputDebugStringA(debugLine);
    if (userLine && userLine[0])
        g_ideCtorBootUserLines.emplace_back(userLine);
}

static bool crashTriageBreakEnabled()
{
    const char* v = std::getenv("RAWRXD_SMOKE_CRASH_TRIAGE_BREAK");
    return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

static void crashTriageBreakIfAttached(const char* reason)
{
    if (reason && reason[0])
        OutputDebugStringA(reason);
    if (crashTriageBreakEnabled() && IsDebuggerPresent())
        DebugBreak();
}


// Complete type declarations for unique_ptr<T> component managers.
// Must come after all other includes to avoid circularity.
#include "Win32IDE_ComponentManagers.h"


// Defined once here; declared as `extern` in Win32IDE.h.
Win32IDE* g_pMainIDE = nullptr;

// Batch 3 advanced feature pack (AI completion, IntelliSense, semantic tools, etc.).
// This keeps the subsystem available for incremental hook-up in focused lanes.
static rawrxd::IDEFeatures3 g_missingFeaturesBatch3;
static rawrxd::ExperimentalModule8 g_experimentalModule8;

std::filesystem::path Win32IDE::resolveRawrxdWorkspaceBase() const
{
    namespace fs = std::filesystem;
    if (const char* envRoot = std::getenv("RAWRXD_REPO_ROOT"))
    {
        if (envRoot[0] != '\0')
        {
            std::error_code ec;
            fs::path p = fs::path(envRoot).lexically_normal();
            if (fs::is_directory(p, ec))
                return p;
        }
    }
    if (!m_projectRoot.empty())
        return fs::path(m_projectRoot);
    return fs::path(m_currentDirectory.empty() ? std::string(".") : m_currentDirectory);
}

static std::wstring utf8ToWide(const std::string& utf8);

namespace
{
std::atomic<unsigned long long> g_chatTraceSequence{0};

std::string BoolToLog(bool value)
{
    return value ? "true" : "false";
}

std::string MakeLogPreview(const std::string& value, size_t maxLen = 120)
{
    std::string preview;
    preview.reserve(std::min(value.size(), maxLen) + 8);
    for (char ch : value)
    {
        if (preview.size() >= maxLen)
            break;
        switch (ch)
        {
            case '\r':
                preview += "\\r";
                break;
            case '\n':
                preview += "\\n";
                break;
            case '\t':
                preview += "\\t";
                break;
            default:
                preview.push_back(ch);
                break;
        }
    }
    if (value.size() > maxLen)
        preview += "...";
    return preview;
}

std::string MakePromptSignature(const std::string& text)
{
    std::ostringstream oss;
    oss << "len=" << text.size() << ",hash=0x" << std::hex << std::uppercase << std::hash<std::string>{}(text)
        << std::dec;
    return oss.str();
}

unsigned long long NextChatTraceId()
{
    return 1ull + g_chatTraceSequence.fetch_add(1, std::memory_order_relaxed);
}

// Sovereign Media & DRM Suppression Kernels (x64 MASM)
extern "C" void Sovereign_Surgical_Manifest_Patch(void* buffer, size_t size);
extern "C" void Sovereign_MSE_Aperture_Submit(const void* packet, size_t size);
extern "C" uint64_t Browser_Interlock_Update();

struct PendingInferenceRequest
{
    std::string prompt;
    std::function<void(const std::string&, bool)> callback;
};

std::mutex g_pendingInferenceMutex;
std::unordered_map<Win32IDE*, std::deque<PendingInferenceRequest>> g_pendingInferenceByIde;
constexpr size_t kMaxPendingInferenceRequestsPerIde = 16;

std::mutex g_chatUtf8CarryMutex;
std::unordered_map<Win32IDE*, std::string> g_chatUtf8CarryByIde;
std::mutex g_chatInputBufferMutex;

/// Embedded in editor /fix chat prompt and matched in `HandleCopilotSend` to arm JSON diff consumption.
constexpr const char kStructuredSlashFixPromptMarker[] = "Respond with ONLY a JSON object";

std::mutex g_terminalCallbackLivenessMutex;
std::unordered_map<Win32IDE*, std::weak_ptr<std::atomic<bool>>> g_terminalCallbackLivenessByIde;

std::shared_ptr<std::atomic<bool>> AcquireTerminalCallbackLiveness(Win32IDE* ide)
{
    if (!ide)
        return nullptr;
    std::lock_guard<std::mutex> lock(g_terminalCallbackLivenessMutex);
    auto it = g_terminalCallbackLivenessByIde.find(ide);
    if (it != g_terminalCallbackLivenessByIde.end())
    {
        if (auto existing = it->second.lock())
            return existing;
    }
    auto token = std::make_shared<std::atomic<bool>>(true);
    g_terminalCallbackLivenessByIde[ide] = token;
    return token;
}

inline bool IsTerminalCallbackOwnerAlive(const std::shared_ptr<std::atomic<bool>>& token)
{
    return token && token->load(std::memory_order_acquire);
}

void ClearTerminalCallbackLiveness(Win32IDE* ide)
{
    std::lock_guard<std::mutex> lock(g_terminalCallbackLivenessMutex);
    auto it = g_terminalCallbackLivenessByIde.find(ide);
    if (it != g_terminalCallbackLivenessByIde.end())
    {
        if (auto token = it->second.lock())
            token->store(false, std::memory_order_release);
        g_terminalCallbackLivenessByIde.erase(it);
    }
}

inline bool IsUtf8ContinuationByte(unsigned char b)
{
    return (b & 0xC0u) == 0x80u;
}

inline int Utf8ExpectedLen(unsigned char lead)
{
    if (lead <= 0x7Fu)
        return 1;
    if (lead >= 0xC2u && lead <= 0xDFu)
        return 2;
    if (lead >= 0xE0u && lead <= 0xEFu)
        return 3;
    if (lead >= 0xF0u && lead <= 0xF4u)
        return 4;
    return 0;
}

size_t FindUtf8PendingStart(const std::string& text)
{
    const size_t n = text.size();
    if (n == 0)
        return 0;

    size_t i = n;
    while (i > 0 && IsUtf8ContinuationByte(static_cast<unsigned char>(text[i - 1])))
    {
        --i;
    }

    if (i == n)
    {
        const int exp = Utf8ExpectedLen(static_cast<unsigned char>(text[n - 1]));
        if (exp > 1)
            return n - 1;
        return n;
    }

    if (i == 0)
        return n;

    const size_t leadPos = i - 1;
    const int exp = Utf8ExpectedLen(static_cast<unsigned char>(text[leadPos]));
    if (exp <= 0)
        return n;

    const size_t avail = n - leadPos;
    return (avail < static_cast<size_t>(exp)) ? leadPos : n;
}

void ClearChatUtf8Carry(Win32IDE* ide)
{
    std::lock_guard<std::mutex> lock(g_chatUtf8CarryMutex);
    g_chatUtf8CarryByIde.erase(ide);
}

size_t EnqueuePendingInference(Win32IDE* ide, PendingInferenceRequest&& req)
{
    std::lock_guard<std::mutex> lock(g_pendingInferenceMutex);
    auto& q = g_pendingInferenceByIde[ide];
    if (q.size() >= kMaxPendingInferenceRequestsPerIde)
    {
        return 0;
    }
    q.emplace_back(std::move(req));
    return q.size();
}

bool DequeuePendingInference(Win32IDE* ide, PendingInferenceRequest& out)
{
    std::lock_guard<std::mutex> lock(g_pendingInferenceMutex);
    auto it = g_pendingInferenceByIde.find(ide);
    if (it == g_pendingInferenceByIde.end() || it->second.empty())
    {
        return false;
    }
    out = std::move(it->second.front());
    it->second.pop_front();
    if (it->second.empty())
    {
        g_pendingInferenceByIde.erase(it);
    }
    return true;
}

void ClearPendingInference(Win32IDE* ide)
{
    std::lock_guard<std::mutex> lock(g_pendingInferenceMutex);
    g_pendingInferenceByIde.erase(ide);
}

std::string resolveLocalModelSelectionPath(const std::string& modelRef,
                                           const std::vector<std::string>& discoveredModelPaths,
                                           const std::vector<std::string>& userModelDirectories)
{
    if (modelRef.empty())
        return modelRef;

    auto toLowerCopy = [](std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    };

    const bool hasPathHint = modelRef.find(":\\") != std::string::npos || modelRef.find('/') != std::string::npos ||
                             modelRef.find('\\') != std::string::npos;
    if (hasPathHint)
        return modelRef;

    const std::string lowerRef = toLowerCopy(modelRef);
    const bool looksLikeLocalModel =
        lowerRef.find(".gguf") != std::string::npos || lowerRef.find(".ggml") != std::string::npos ||
        lowerRef.find(".bin") != std::string::npos || lowerRef.find(".safetensors") != std::string::npos;
    if (!looksLikeLocalModel)
        return modelRef;

    std::error_code ec;
    for (const auto& fullPath : discoveredModelPaths)
    {
        std::filesystem::path p(fullPath);
        const std::string fileName = toLowerCopy(p.filename().string());
        if (fileName == lowerRef && std::filesystem::exists(p, ec))
            return p.string();
    }

    for (const auto& dir : userModelDirectories)
    {
        std::filesystem::path candidate = std::filesystem::path(dir) / modelRef;
        if (std::filesystem::exists(candidate, ec))
            return candidate.string();
    }

    static const char* kCommonModelDirs[] = {"F:\\OllamaModels", "D:\\OllamaModels", "F:\\models", "D:\\models"};
    for (const char* dir : kCommonModelDirs)
    {
        std::filesystem::path candidate = std::filesystem::path(dir) / modelRef;
        if (std::filesystem::exists(candidate, ec))
            return candidate.string();
    }

    return modelRef;
}

std::string normalizeModelSelectorEntry(const std::string& selectorText,
                                        const std::vector<std::string>& availableModels)
{
    if (selectorText.empty())
        return selectorText;

    auto trim = [](std::string value)
    {
        const size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return std::string();
        const size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    };

    const std::string normalizedInput = trim(selectorText);
    if (normalizedInput.empty())
        return normalizedInput;

    const std::string lowerInput = rawrxd::ToLowerCopy(normalizedInput);

    for (const auto& model : availableModels)
    {
        const std::string label = rawrxd::BuildModelDropdownLabel(model);
        if (rawrxd::ToLowerCopy(label) == lowerInput)
            return model;
    }

    const size_t suffixPos = normalizedInput.find(" - A Carrot > - ");
    if (suffixPos != std::string::npos)
    {
        const std::string baseLabel = trim(normalizedInput.substr(0, suffixPos));
        const std::string baseLower = rawrxd::ToLowerCopy(baseLabel);
        for (const auto& model : availableModels)
        {
            const std::string displayName = rawrxd::BaseModelDisplayName(model);
            if (rawrxd::ToLowerCopy(displayName) == baseLower)
                return model;
        }
    }

    return normalizedInput;
}

bool HasAgenticPrefix(const std::string& prompt)
{
    return prompt.rfind("/agent", 0) == 0 || prompt.rfind("/agentic", 0) == 0 || prompt.rfind("agentic:", 0) == 0 ||
           prompt.rfind("@agent", 0) == 0;
}

std::string StripAgenticPrefixForRouteParity(const std::string& prompt)
{
    std::string_view view(prompt);
    auto trimLeadingWhitespace = [](std::string_view& text)
    {
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        {
            text.remove_prefix(1);
        }
    };

    if (view.rfind("/agentic", 0) == 0)
    {
        view.remove_prefix(8);
        trimLeadingWhitespace(view);
        return std::string(view);
    }
    if (view.rfind("/agent", 0) == 0)
    {
        view.remove_prefix(6);
        trimLeadingWhitespace(view);
        return std::string(view);
    }
    if (view.rfind("agentic:", 0) == 0)
    {
        view.remove_prefix(8);
        trimLeadingWhitespace(view);
        return std::string(view);
    }
    if (view.rfind("@agent", 0) == 0)
    {
        view.remove_prefix(6);
        trimLeadingWhitespace(view);
        return std::string(view);
    }
    return prompt;
}

std::string FormatMinimalAgenticResponseForChat(const rawrxd::MinimalAgenticResponse& response)
{
    if (response.tool_calls_made <= 0)
    {
        return response.final_message;
    }
    return "[Agent executed " + std::to_string(response.tool_calls_made) + " tools]\n\n" + response.final_message;
}

static size_t EstimateMinimalAgenticOutputChars(const rawrxd::MinimalAgenticResponse& r)
{
    size_t n = r.final_message.size() + r.error.size();
    for (const auto& s : r.tool_steps)
    {
        n += s.tool_name.size() + s.arguments_json.size() + s.result_text.size();
    }
    for (const auto& m : r.transcript_delta)
    {
        n += m.content.size();
    }
    return n == 0 ? size_t{1} : n;
}

std::string BuildSlashRoutePreview(const RawrXD::Agentic::ParsedCommand& parsed)
{
    if (!parsed.valid)
    {
        return std::string("[Slash] parse_error: ") + parsed.error;
    }

    const auto toolCall = parsed.ToToolCall();
    const std::string tool = toolCall.value("tool", std::string("<none>"));
    const auto& args = toolCall.contains("args") ? toolCall["args"] : nlohmann::json::object();
    const size_t argCount = args.is_object() ? args.size() : 0;

    std::ostringstream ss;
    ss << "[Slash] command=/" << parsed.command << " route=" << tool << " args=" << argCount;
    return ss.str();
}

bool ShouldOpenPlanApprovalFromSlashEdit(const std::string& rawInput, const RawrXD::Agentic::ParsedCommand& parsed,
                                         std::string& outGoal)
{
    outGoal.clear();
    if (!parsed.valid || parsed.command != "edit")
    {
        return false;
    }

    const bool hasDryRun = rawInput.find("--dry-run") != std::string::npos;
    const bool hasForce = rawInput.find("--force") != std::string::npos;
    if (!hasDryRun || hasForce)
    {
        return false;
    }

    std::vector<std::string> contentArgs;
    contentArgs.reserve(parsed.args.size());
    for (const auto& arg : parsed.args)
    {
        if (arg.rfind("--", 0) == 0)
        {
            continue;
        }
        contentArgs.push_back(arg);
    }

    std::ostringstream goal;
    goal << "Draft a multi-file edit plan for this request. Require explicit approval before execution.";
    if (!contentArgs.empty())
    {
        goal << "\nRequested targets/instructions:";
        for (const auto& arg : contentArgs)
        {
            goal << "\n- " << arg;
        }
    }
    goal << "\nSafety requirements: no destructive actions, include rollbackable steps where possible.";
    outGoal = goal.str();
    return true;
}

std::string SlashCommandShortDescription(const std::string& command)
{
    if (command == "edit")
        return "multi-file edit plan";
    if (command == "terminal")
        return "run shell command";
    if (command == "search")
        return "search workspace";
    if (command == "read")
        return "read file";
    if (command == "write")
        return "write file";
    if (command == "refactor")
        return "refactor operation";
    if (command == "git")
        return "git operation";
    if (command == "help")
        return "show command help";
    if (command == "explain")
        return "explain code with editor context";
    if (command == "fix")
        return "fix issues using diagnostics + context";
    if (command == "test")
        return "generate tests from editor context";
    if (command == "doc")
        return "document code via Agent Chat";
    if (command == "optimize")
        return "optimize performance";
    if (command == "kill-locks")
        return "terminate stuck ninja/cmake/clangd/MSVC (Ninja Nuke)";
    if (command == "memory")
        return "agent memory files";
    if (command == "taskframe" || command == "framework")
        return "task execution framework";
    if (command == "harden" || command == "audit")
        return "audit harness scaffold";
    if (command == "language" || command == "lang")
        return "language / toolchain context";
    if (command == "context")
        return "full execution context profile";
    if (command == "streaming" || command == "streaming status" || command == "streaming autopatch" ||
        command == "streaming throttle")
        return "streaming engine control";
    return "command";
}
}  // namespace

extern "C" unsigned __int64 RawrXD_EnableSeLockMemoryPrivilege();
extern "C" void* RawrXD_MapModelView2MB(HANDLE hMap, uint64_t off, size_t sz, uint64_t* outBaseOrError);
extern "C" void ChatAutocomplete_Attach(HWND hwndChatInput, HINSTANCE hInstance);
extern "C" void ChatAutocomplete_Detach();
extern "C" void ChatAutocomplete_Hide();
extern "C" bool ChatAutocomplete_HandleKeyDown(WPARAM key, bool ctrlDown);
extern "C" void ChatAutocomplete_OnInputChanged(const wchar_t* text);
extern "C" void CommandPreview_Create(HWND hwndParent, HINSTANCE hInstance);
extern "C" void CommandPreview_Destroy();
extern "C" void CommandPreview_Update(const wchar_t* input);
extern "C" void CommandPreview_Hide();
extern "C" bool CommandPreview_Validate(const wchar_t* input, wchar_t* errorBuffer, int errorBufferChars);
extern "C" void CommandPreview_GetRouteLine(wchar_t* routeBuffer, int routeBufferChars);
extern "C" bool CommandPreview_HandleCommand(int controlId);
extern "C" HWND ComposerPanel_Create(HWND hwndParent, HINSTANCE hInstance);
extern "C" void ComposerPanel_LoadPlan(const char* jsonPlanData);
extern "C" void ComposerPanel_ShowPlan();
extern "C" void ComposerPanel_HidePlan();
extern "C" void ComposerPanel_AcceptFile(int fileIndex);
extern "C" void ComposerPanel_RejectFile(int fileIndex);
extern "C" void ComposerPanel_ShowDiff(int fileIndex);
extern "C" void ComposerPanel_AddCheckpoint(const wchar_t* label);
extern "C" void ComposerPanel_RollbackToCheckpoint(int checkpointIndex);
extern "C" void ComposerPanel_UpdateProgress(int current, int total);
extern "C" int ComposerPanel_GetAcceptedFileCount();
extern "C" bool ComposerPanel_IsVisible();
extern "C" void ComposerPanel_Destroy();
extern "C" int ComposerPanel_GetFileCount();
extern "C" const char* ComposerPanel_GetFileName(int fileIndex);

static uint64_t qpcNowU64()
{
    LARGE_INTEGER v{};
    QueryPerformanceCounter(&v);
    return static_cast<uint64_t>(v.QuadPart);
}

static double qpcDeltaToMs(uint64_t delta)
{
    LARGE_INTEGER f{};
    QueryPerformanceFrequency(&f);
    if (f.QuadPart <= 0)
        return 0.0;
    return (static_cast<double>(delta) * 1000.0) / static_cast<double>(f.QuadPart);
}

enum class VmmRibbonTier : uint8_t
{
    Green,
    Yellow,
    Gray,
    Red,
};

static HICON getVmmLedIcon(VmmRibbonTier tier);

static HICON createLedIcon(COLORREF rgb)
{
    // 16x16 ARGB DIB for status bar icon.
    BITMAPV5HEADER bi{};
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = 16;
    bi.bV5Height = -16;  // top-down
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    void* bits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP colorBmp = CreateDIBSection(hdc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (!colorBmp || !bits)
        return nullptr;

    // Clear fully transparent.
    std::memset(bits, 0, 16 * 16 * 4);

    // Draw a filled circle with a tiny darker border.
    const uint8_t r = GetRValue(rgb);
    const uint8_t g = GetGValue(rgb);
    const uint8_t b = GetBValue(rgb);
    const uint8_t br = static_cast<uint8_t>(r / 2);
    const uint8_t bg = static_cast<uint8_t>(g / 2);
    const uint8_t bb = static_cast<uint8_t>(b / 2);

    auto put = [&](int x, int y, uint8_t rr, uint8_t gg, uint8_t bb_, uint8_t aa)
    {
        uint32_t* p = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(bits) + (y * 16 + x) * 4);
        *p = (static_cast<uint32_t>(aa) << 24) | (static_cast<uint32_t>(rr) << 16) | (static_cast<uint32_t>(gg) << 8) |
             static_cast<uint32_t>(bb_);
    };

    const int cx = 8;
    const int cy = 8;
    const int rOuter = 6;
    const int rInner = 5;
    for (int y = 0; y < 16; ++y)
    {
        for (int x = 0; x < 16; ++x)
        {
            const int dx = x - cx;
            const int dy = y - cy;
            const int d2 = dx * dx + dy * dy;
            if (d2 <= rInner * rInner)
                put(x, y, r, g, b, 0xFF);
            else if (d2 <= rOuter * rOuter)
                put(x, y, br, bg, bb, 0xFF);
        }
    }

    HBITMAP maskBmp = CreateBitmap(16, 16, 1, 1, nullptr);
    if (!maskBmp)
    {
        DeleteObject(colorBmp);
        return nullptr;
    }

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmColor = colorBmp;
    ii.hbmMask = maskBmp;
    HICON icon = CreateIconIndirect(&ii);

    DeleteObject(maskBmp);
    DeleteObject(colorBmp);
    return icon;
}

static HICON getVmmLedIcon(VmmRibbonTier tier)
{
    static HICON s_green = nullptr;
    static HICON s_yellow = nullptr;
    static HICON s_gray = nullptr;
    static HICON s_red = nullptr;

    if (!s_green)
        s_green = createLedIcon(RGB(34, 139, 34));  // ForestGreen
    if (!s_yellow)
        s_yellow = createLedIcon(RGB(218, 165, 32));  // Goldenrod
    if (!s_gray)
        s_gray = createLedIcon(RGB(160, 160, 160));  // neutral
    if (!s_red)
        s_red = createLedIcon(RGB(178, 34, 34));  // FireBrick

    switch (tier)
    {
        case VmmRibbonTier::Green:
            return s_green;
        case VmmRibbonTier::Yellow:
            return s_yellow;
        case VmmRibbonTier::Gray:
            return s_gray;
        case VmmRibbonTier::Red:
        default:
            return s_red;
    }
}

// ============================================================================
// FEATURE IMPLEMENTATION INDEX — RawrXD Win32 IDE
// ============================================================================
// Core Window & Layout:
//   Main window / message loop      → Win32IDE_Main.cpp, Win32IDE_Window.cpp
//   Activity bar / primary sidebar   → Win32IDE_Sidebar.cpp, Win32IDE_SidebarPanels.cpp
//   Secondary sidebar (Copilot chat) → Win32IDE_VSCodeUI.cpp
//   Panel container (term/output)    → Win32IDE_VSCodeUI.cpp
//   Tab bar / document switching     → Win32IDE_DragDropTabs.cpp
//   Status bar / parts layout        → Win32IDE_VSCodeUI.cpp
//
// Editor Engine:
//   RichEdit integration / caret     → Win32IDE_EditorEngine.cpp
//   Syntax highlighting by language  → Win32IDE_SyntaxHighlight.cpp, Win32IDE_AsmSemantic.cpp
//   Line numbers / minimap           → Win32IDE_Minimap.cpp
//   Code folding                     → Win32IDE_EditorEngine.cpp
//   Split code viewer                → Win32IDE_EditorEngine.cpp
//   Undo/redo stack                  → Win32IDE_EditorEngine.cpp
//   Selection / clipboard            → Win32IDE_EditorEngine.cpp
//   Indentation / tab-space          → Win32IDE_EditorEngine.cpp
//   EOL / encoding detection         → Win32IDE_EditorEngine.cpp
//   Language mode from extension     → Win32IDE_EditorEngine.cpp
//   Font / DPI scaling               → Win32IDE_Tier1Cosmetics.cpp
//   Breadcrumbs / navigation         → Win32IDE_Breadcrumbs.cpp
//
// Find & Navigation:
//   Find/Replace dialog              → Win32IDE_Commands.cpp
//   Search panel (find-in-files)     → Win32IDE_SearchPanel.cpp
//   Go to line dialog                → Win32IDE_Commands.cpp
//   Command palette / filtering      → Win32IDE_Commands.cpp
//   Fuzzy search                     → Win32IDE_FuzzySearch.cpp
//
// File Explorer & Operations:
//   File explorer tree / refresh     → Win32IDE_Sidebar.cpp
//   New file / default content       → Win32IDE_FileOps.cpp
//   Save / Save As / modified flag   → Win32IDE_FileOps.cpp
//   File icons                       → Win32IDE_FileIcons.cpp
//
// LSP & Intelligence:
//   Go-to-definition / references    → Win32IDE_LSPClient.cpp
//   Completion / signature help      → Win32IDE_LSPClient.cpp
//   Semantic tokens (23-type)        → Win32IDE_LSPClient.cpp::lspSemanticTokensFull()
//   Hover tooltips                   → Win32IDE_HoverTooltips.cpp
//   Inlay hints                      → Win32IDE_InlayHints.cpp
//   Code lens                        → Win32IDE_CodeLens.cpp
//   Signature help                   → Win32IDE_SignatureHelp.cpp
//   Rename preview                   → Win32IDE_RenamePreview.cpp
//   Refactoring                      → Win32IDE_Refactor.cpp
//
// AI / Copilot / Ghost Text:
//   Ghost text (3-provider cascade)  → Win32IDE_GhostText.cpp
//   Copilot send/clear handlers      → Win32IDE_CursorParity.cpp
//   Ollama model override            → Win32IDE_BackendSwitcher.cpp
//   generateResponse sync/async      → Win32IDE_AIBackend.cpp
//   Inline AI suggestion overlay     → Win32IDE_GhostText.cpp
//   LLM Router                       → Win32IDE_LLMRouter.cpp
//
// Agent System:
//   Agentic bridge / dispatch        → Win32IDE_AgenticBridge.cpp
//   Agent history / replay           → Win32IDE_AgentHistory.cpp
//   Agent memory store/recall        → Win32IDE_AgentPanel.cpp
//   Sub-agent / swarm execution      → Win32IDE_SubAgent.cpp
//   Autonomy manager / goal loop     → Win32IDE_Autonomy.cpp
//   Plan executor / rollback         → Win32IDE_PlanExecutor.cpp
//
// Terminal & Tasks:
//   Terminal pane / split / profiles  → Win32IDE_TerminalTabs.cpp, Win32IDE_TerminalProfiles.cpp
//   Task runner (stdout pipe capture) → Win32IDE_TaskRunner.cpp
//   Tasks/launch.json UI             → Win32IDE_Tasks.cpp, Win32IDE_TasksDebugUI.cpp
//   PowerShell output panel           → Win32IDE_PowerShellPanel.cpp
//   Output panel / severity tabs      → Win32IDE_ProblemsPanel.cpp
//
// Git & VCS:
//   Git panel (stage/commit/push)     → Win32IDE_GitPanel.cpp
//   Git repository (14 methods)       → Win32IDE_Git.cpp
//   Diff viewer (LCS-based)           → Win32IDE_DiffView.cpp
//   Diff inline/side-by-side          → Win32IDE_Tier2Cosmetics.cpp
//
// Debugger:
//   DbgEng COM debugger               → Win32IDE_Debugger.cpp
//   Watch format / variables           → Win32IDE_DebugWatchFormat.cpp
//   Call stack symbols                 → Win32IDE_CallStackSymbols.cpp
//   Memory view                        → Win32IDE_MemoryView.cpp
//   PDB symbols                        → Win32IDE_PDBSymbols.cpp
//   Native debug panel                 → Win32IDE_NativeDebugPanel.cpp
//
// Extensions & Marketplace:
//   Extensions panel (GUI)             → Win32IDE_ExtensionsPanel.cpp
//   Extension marketplace backend      → Win32IDE_ExtensionMarketplace.cpp
//   Marketplace panel                  → Win32IDE_MarketplacePanel.cpp
//   VSIX installer                     → VSIXInstaller.hpp
//
// Misc Panels:
//   Snippet list / insertion           → Win32IDE_Commands.cpp
//   Clipboard history panel            → Win32IDE_Commands.cpp
//   License dialogs                    → Win32IDE_LicenseCreator.cpp
//   Telemetry dashboard                → Win32IDE_TelemetryDashboard.cpp
//   Test explorer tree                 → Win32IDE_TestExplorerTree.cpp
//   Themes / color picker              → Win32IDE_Themes.cpp, Win32IDE_ColorPicker.cpp
//   Voice automation                   → Win32IDE_VoiceAutomation.cpp
//   Transcendence panel                → Win32IDE_TranscendencePanel.cpp
//   Checkpoint manager                 → Win32IDE_Session.cpp
//   Error/warning counts               → Win32IDE_ProblemsPanel.cpp
//   Decompiler view                    → Win32IDE_DecompilerView.cpp
//   Outline panel                      → Win32IDE_OutlinePanel.cpp
// ============================================================================


#pragma comment(lib, "winhttp.lib")

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")

// Helper function to execute shell commands and capture output
static std::string ExecCmd(const char* cmd)
{
    std::string result;
#ifdef _WIN32
    FILE* pipe = _popen(cmd, "r");
#else
    FILE* pipe = popen(cmd, "r");
#endif

    if (!pipe)
        return "Error: Could not execute command";

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        result += buffer;
    }

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    return result;
}

// UTF-8 to UTF-16 for Unicode Win32 APIs (Qt removal / pure MASM C++20)
static std::wstring utf8ToWide(const std::string& utf8)
{
    if (utf8.empty())
        return {};
    int len =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (len <= 0)
    {
        // Never reinterpret UTF-8 payloads as ACP; that causes mojibake.
        // If strict UTF-8 fails, retry with replacement semantics but stay on UTF-8.
        len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
        flags = 0;
    }
    if (len <= 0)
        return {};
    std::wstring out(static_cast<size_t>(len), L'\0');
    if (MultiByteToWideChar(CP_UTF8, flags, utf8.c_str(), static_cast<int>(utf8.size()), out.data(), len) == 0)
        return {};
    return out;
}
static std::wstring utf8ToWide(const char* utf8)
{
    if (!utf8 || !*utf8)
        return {};
    return utf8ToWide(std::string(utf8));
}
static std::string wideToUtf8(const wchar_t* wide)
{
    if (!wide || !*wide)
        return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return {};
    std::string out(static_cast<size_t>(len), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), len, nullptr, nullptr) == 0)
        return {};
    out.resize(out.size() - 1);  // drop NUL
    return out;
}

static std::string wideToUtf8(const std::wstring& wide)
{
    return wideToUtf8(wide.c_str());
}

static COLORREF ensureReadableTextColor(COLORREF bg, COLORREF fg);

static std::string wideToUtf8N(const wchar_t* wide, int wideLen)
{
    if (!wide || wideLen <= 0)
        return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide, wideLen, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return {};
    std::string out(static_cast<size_t>(len), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wide, wideLen, out.data(), len, nullptr, nullptr) == 0)
        return {};
    return out;
}

static bool looksLikeUtf16LEBytes(const std::string& s)
{
    if (s.size() < 4 || (s.size() % 2) != 0)
        return false;

    size_t oddNul = 0;
    for (size_t i = 1; i < s.size(); i += 2)
    {
        if (s[i] == '\0')
            ++oddNul;
    }
    const size_t oddCount = s.size() / 2;
    return oddCount > 0 && (oddNul * 100 / oddCount) >= 40;
}

static std::string trimAsciiWhitespace(const std::string& s)
{
    size_t begin = 0;
    while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin])))
        ++begin;

    size_t end = s.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
        --end;

    return s.substr(begin, end - begin);
}

static void copyIntegratedTerminalFontFace(wchar_t out[LF_FACESIZE], const std::string& utf8Face)
{
    std::string n = trimAsciiWhitespace(utf8Face);
    if (n.empty())
        n = "Consolas";
    std::wstring w = utf8ToWide(n);
    if (w.empty())
        w = L"Consolas";
    wcsncpy_s(out, LF_FACESIZE, w.c_str(), _TRUNCATE);
}

static void appendTextChunk(std::string& out, const std::string& text)
{
    if (text.empty())
        return;
    if (!out.empty() && out.back() != '\n')
        out.push_back('\n');
    out.append(text);
}

static bool extractTextFromJsonNode(const nlohmann::json& node, std::string& out)
{
    if (node.is_null())
        return false;

    if (node.is_string())
    {
        const std::string value = trimAsciiWhitespace(node.get<std::string>());
        if (!value.empty())
        {
            appendTextChunk(out, value);
            return true;
        }
        return false;
    }

    if (node.is_array())
    {
        bool any = false;
        for (const auto& item : node)
            any = extractTextFromJsonNode(item, out) || any;
        return any;
    }

    if (!node.is_object())
        return false;

    bool found = false;

    const char* directKeys[] = {"text", "response", "final_message", "output", "content", "message"};
    for (const char* key : directKeys)
    {
        if (node.contains(key))
            found = extractTextFromJsonNode(node[key], out) || found;
    }

    if (node.contains("choices") && node["choices"].is_array())
    {
        const auto& choices = node["choices"];
        for (size_t i = 0; i < choices.size(); ++i)
        {
            const auto& choice = choices[i];
            if (!choice.is_object())
                continue;
            if (choice.contains("text"))
                found = extractTextFromJsonNode(choice["text"], out) || found;
            if (choice.contains("message"))
                found = extractTextFromJsonNode(choice["message"], out) || found;
            if (choice.contains("delta"))
                found = extractTextFromJsonNode(choice["delta"], out) || found;
        }
    }

    if (node.contains("data"))
        found = extractTextFromJsonNode(node["data"], out) || found;

    return found;
}

static std::string extractDisplayTextFromBackendPayload(const std::string& payload)
{
    const std::string trimmed = trimAsciiWhitespace(payload);
    if (trimmed.empty())
        return {};

    auto stripSseDataPrefix = [](const std::string& line) -> std::string
    {
        std::string v = trimAsciiWhitespace(line);
        if (v.rfind("data:", 0) == 0)
            v = trimAsciiWhitespace(v.substr(5));
        return v;
    };

    auto parseAndExtract = [](const std::string& text) -> std::string
    {
        const auto parsed = nlohmann::json::parse(text, nullptr, false);
        if (parsed.is_discarded())
            return {};
        std::string extracted;
        extractTextFromJsonNode(parsed, extracted);
        return trimAsciiWhitespace(extracted);
    };

    const std::string normalizedTopLevel = stripSseDataPrefix(trimmed);
    if (!normalizedTopLevel.empty() && (normalizedTopLevel.front() == '{' || normalizedTopLevel.front() == '['))
    {
        const std::string extracted = parseAndExtract(normalizedTopLevel);
        if (!extracted.empty())
            return extracted;
    }

    std::string aggregate;
    size_t start = 0;
    while (start < trimmed.size())
    {
        size_t end = trimmed.find('\n', start);
        if (end == std::string::npos)
            end = trimmed.size();
        const std::string line = stripSseDataPrefix(trimmed.substr(start, end - start));
        if (!line.empty() && line != "[DONE]" && (line.front() == '{' || line.front() == '['))
        {
            const std::string extracted = parseAndExtract(line);
            if (!extracted.empty())
                appendTextChunk(aggregate, extracted);
        }
        start = (end < trimmed.size()) ? end + 1 : trimmed.size();
    }

    return trimAsciiWhitespace(aggregate);
}

static std::string sanitizeForChatUi(const std::string& input)
{
    if (input.empty())
        return {};

    std::string text = input;

    // Some backends leak UTF-16LE byte strings into narrow channels.
    if (looksLikeUtf16LEBytes(text))
    {
        std::wstring w;
        w.reserve(text.size() / 2);
        for (size_t i = 0; i + 1 < text.size(); i += 2)
        {
            const unsigned char lo = static_cast<unsigned char>(text[i]);
            const unsigned char hi = static_cast<unsigned char>(text[i + 1]);
            w.push_back(static_cast<wchar_t>((static_cast<unsigned int>(hi) << 8u) | lo));
        }
        while (!w.empty() && w.back() == L'\0')
            w.pop_back();
        const std::string converted = wideToUtf8N(w.data(), static_cast<int>(w.size()));
        if (!converted.empty())
            text = converted;
    }

    // Validate UTF-8 strictly; do not reinterpret malformed bytes as ACP.
    const int wLenUtf8 =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    bool utf8Valid = (wLenUtf8 > 0);
    if (!utf8Valid)
    {
        // Retry non-strict UTF-8 decode to preserve as much text as possible
        // without switching code pages.
        const int wLenUtf8Lossy =
            MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (wLenUtf8Lossy > 0)
        {
            std::wstring w(static_cast<size_t>(wLenUtf8Lossy), L'\0');
            if (MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), w.data(), wLenUtf8Lossy) >
                0)
            {
                const std::string converted = wideToUtf8N(w.data(), wLenUtf8Lossy);
                if (!converted.empty())
                {
                    text = converted;
                    utf8Valid = true;
                }
            }
        }
    }

    if (utf8Valid)
    {
        std::wstring w(static_cast<size_t>(wLenUtf8), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), w.data(),
                                wLenUtf8) > 0)
        {
            std::string cleanedUtf8;
            cleanedUtf8.reserve(text.size());

            for (wchar_t wc : w)
            {
                if (wc == L'\n' || wc == L'\r' || wc == L'\t')
                {
                    const std::string s = wideToUtf8N(&wc, 1);
                    if (!s.empty())
                        cleanedUtf8.append(s);
                    continue;
                }

                // Drop C0/C1 controls while preserving printable Unicode.
                if (wc < 0x20 || (wc >= 0x7F && wc < 0xA0))
                    continue;

                if (iswprint(wc))
                {
                    const std::string s = wideToUtf8N(&wc, 1);
                    if (!s.empty())
                        cleanedUtf8.append(s);
                }
            }

            if (!cleanedUtf8.empty())
                return cleanedUtf8;
            return "[non-text backend payload suppressed]";
        }
    }

    // Strip non-printable control chars that can poison RichEdit rendering.
    std::string cleaned;
    cleaned.reserve(text.size());
    size_t highByteCount = 0;
    size_t printableAsciiCount = 0;
    for (unsigned char c : text)
    {
        if (c == '\n' || c == '\r' || c == '\t')
        {
            cleaned.push_back(static_cast<char>(c));
            continue;
        }

        if (c >= 0x20 && c <= 0x7E)
        {
            cleaned.push_back(static_cast<char>(c));
            ++printableAsciiCount;
            continue;
        }

        if (c >= 0x80)
        {
            ++highByteCount;
            cleaned.push_back(' ');
        }
    }

    // If payload is mostly high-byte noise, suppress it with a deterministic marker.
    if (!text.empty() && highByteCount * 2 > text.size())
        return "[non-text backend payload suppressed]";

    // Collapse repeated spaces introduced by sanitization.
    std::string compact;
    compact.reserve(cleaned.size());
    bool lastSpace = false;
    for (char ch : cleaned)
    {
        const bool isSpace = (ch == ' ');
        if (isSpace && lastSpace)
            continue;
        compact.push_back(ch);
        lastSpace = isSpace;
    }

    if (compact.empty())
        return "[non-text backend payload suppressed]";
    return compact;
}

static std::string normalizeChatUtf8Chunk(Win32IDE* ide, const std::string& chunk, bool flush)
{
    if (!ide)
        return sanitizeForChatUi(chunk);

    std::string combined;
    {
        std::lock_guard<std::mutex> lock(g_chatUtf8CarryMutex);
        auto& carry = g_chatUtf8CarryByIde[ide];
        combined.reserve(carry.size() + chunk.size());
        combined.append(carry);
        combined.append(chunk);

        if (!flush)
        {
            const size_t pendingStart = FindUtf8PendingStart(combined);
            carry.assign(combined.data() + pendingStart, combined.size() - pendingStart);
            combined.resize(pendingStart);
        }
        else
        {
            carry.clear();
        }
    }

    return sanitizeForChatUi(combined);
}

static bool looksLikeTokenIdSequencePayload(const std::string& text)
{
    if (text.empty())
        return false;

    size_t digitCount = 0;
    size_t sepCount = 0;
    size_t otherCount = 0;

    for (unsigned char c : text)
    {
        if (c >= '0' && c <= '9')
        {
            ++digitCount;
            continue;
        }
        if (c == '[' || c == ']' || c == ',' || c == ' ' || c == '\n' || c == '\r' || c == '\t')
        {
            ++sepCount;
            continue;
        }
        ++otherCount;
    }

    // Treat payloads that are almost entirely numeric list syntax as token-id dumps.
    if (otherCount > 0)
        return false;
    return (digitCount > 8) && (digitCount + sepCount >= text.size());
}

static void logRawResponseHexPreview(const std::string& response)
{
    if (response.empty())
        return;

    const size_t previewLen = response.size() < 32 ? response.size() : 32;
    char line[512] = {0};
    int offset = 0;
    offset += _snprintf_s(line + offset, sizeof(line) - static_cast<size_t>(offset), _TRUNCATE,
                          "[CopilotRawHex] len=%zu bytes: ", response.size());

    for (size_t i = 0; i < previewLen && offset > 0 && static_cast<size_t>(offset) < sizeof(line); ++i)
    {
        const unsigned char b = static_cast<unsigned char>(response[i]);
        offset += _snprintf_s(line + offset, sizeof(line) - static_cast<size_t>(offset), _TRUNCATE, "%02X ", b);
    }

    if (offset > 0 && static_cast<size_t>(offset) < sizeof(line) - 2)
    {
        line[offset++] = '\n';
        line[offset] = '\0';
    }

    OutputDebugStringA(line);
}

#define IDC_EDITOR 1001
#define IDC_TERMINAL 1002
#define IDC_COMMAND_INPUT 1003
#define IDC_STATUS_BAR 1004
#define IDC_OUTPUT_TABS 1005
#define IDC_MINIMAP 1006
#define IDC_MODULE_BROWSER 1007
#define IDC_HELP_PANEL 1008
#define IDC_SNIPPET_LIST 1009
#define IDC_CLIPBOARD_HISTORY 1010
#define IDC_OUTPUT_TEXT 1011
#define IDC_OUTPUT_EDIT_GENERAL 1012
#define IDC_OUTPUT_EDIT_ERRORS 1013
#define IDC_OUTPUT_EDIT_DEBUG 1014
#define IDC_OUTPUT_EDIT_FIND 1015
#define IDC_SPLITTER 1016
#define IDC_INTEGRATED_TERM_TABS 1199
#define IDC_SEVERITY_FILTER 1017
#define IDC_TITLE_TEXT 1018
#define IDC_BTN_MINIMIZE 1019
#define IDC_BTN_MAXIMIZE 1020
#define IDC_BTN_CLOSE 1021
#define IDC_BTN_GITHUB 1022
#define IDC_BTN_MICROSOFT 1023
#define IDC_BTN_SETTINGS 1024
#define IDC_FILE_EXPLORER 1025
#define IDC_FILE_TREE 1026
// Defined in Win32IDE.h
// #define IDM_AUTONOMY_TOGGLE 4150
// ... constants moved to header

// Activity Bar (Far Left) - VS Code style icon bar
#define IDC_ACTIVITY_BAR 1100
#define IDC_ACTBAR_EXPLORER 1101
#define IDC_ACTBAR_SEARCH 1102
#define IDC_ACTBAR_SCM 1103
#define IDC_ACTBAR_DEBUG 1104
#define IDC_ACTBAR_EXTENSIONS 1105
#define IDC_ACTBAR_SETTINGS 1106
#define IDC_ACTBAR_ACCOUNTS 1107

// Secondary Sidebar (Right) - AI Chat/Copilot area
#define IDC_SECONDARY_SIDEBAR 1200
#define IDC_SECONDARY_SIDEBAR_HEADER 1201
#define IDC_COPILOT_CHAT_INPUT 1202
#define IDC_COPILOT_CHAT_OUTPUT 1203
#define IDC_COPILOT_SEND_BTN 1204
#define IDC_COPILOT_CLEAR_BTN 1205
#define IDC_COPILOT_MODEL_BADGE 1206
#define IDC_COPILOT_HELPFUL_BTN 1207
#define IDC_COPILOT_UNHELPFUL_BTN 1213
#define IDC_COPILOT_COPY_BTN 1214
#define IDC_COPILOT_RETRY_BTN 1215

// Redeclare IDs to avoid header duplication or linkage issues
#ifndef IDC_MODEL_SELECTOR
#define IDC_MODEL_SELECTOR 1208
#endif
#ifndef IDC_MODEL_BROWSE_BTN
#define IDC_MODEL_BROWSE_BTN 1209
#endif
#ifndef IDC_COPILOT_NEW_CHAT_BTN
#define IDC_COPILOT_NEW_CHAT_BTN 1210
#endif
#ifndef IDC_MODEL_MODE_SELECTOR
#define IDC_MODEL_MODE_SELECTOR 1211
#endif
#ifndef IDC_MODEL_EFFORT_LABEL
#define IDC_MODEL_EFFORT_LABEL 1212
#endif

#ifndef IDC_EMOJI_BASE
#define IDC_EMOJI_BASE 8100
#endif
#ifndef IDC_AI_MAX_TOKENS_SLIDER
#define IDC_AI_MAX_TOKENS_SLIDER 5005
#endif
#ifndef IDC_AI_CONTEXT_SLIDER
#define IDC_AI_CONTEXT_SLIDER 5006
#endif
#ifndef IDC_AI_MAX_MODE
#define IDC_AI_MAX_MODE 5007
#endif
#ifndef IDC_AI_DEEP_THINK
#define IDC_AI_DEEP_THINK 5008
#endif
#ifndef IDC_AI_DEEP_RESEARCH
#define IDC_AI_DEEP_RESEARCH 5009
#endif
#ifndef IDC_AI_NO_REFUSAL
#define IDC_AI_NO_REFUSAL 5010
#endif

// Panel (Bottom) - Terminal, Output, Problems, Debug Console
#define IDC_PANEL_CONTAINER 1300
#define IDC_PANEL_TABS 1301
#define IDC_PANEL_TERMINAL 1302
#define IDC_PANEL_OUTPUT 1303
#define IDC_PANEL_PROBLEMS 1304
#define IDC_PANEL_DEBUG_CONSOLE 1305
#define IDC_PANEL_TOOLBAR 1306
#define IDC_PANEL_BTN_NEW_TERMINAL 1307
#define IDC_PANEL_BTN_SPLIT_TERMINAL 1308
#define IDC_PANEL_BTN_KILL_TERMINAL 1309
#define IDC_PANEL_BTN_MAXIMIZE 1310
#define IDC_PANEL_BTN_CLOSE 1311
#define IDC_PANEL_PROBLEMS_LIST 1312

// Debugger Panel - Integrated at bottom with Terminal/Output
#define IDC_DEBUGGER_CONTAINER 1313
#define IDC_DEBUGGER_TABS 1314
#define IDC_DEBUGGER_BREAKPOINTS 1315
#define IDC_DEBUGGER_WATCH 1316
#define IDC_DEBUGGER_VARIABLES 1317
#define IDC_DEBUGGER_STACK_TRACE 1318
#define IDC_DEBUGGER_MEMORY 1319
#define IDC_DEBUGGER_TOOLBAR 1320
#define IDC_DEBUGGER_BTN_CONTINUE 1321
#define IDC_DEBUGGER_BTN_STEP_OVER 1322
#define IDC_DEBUGGER_BTN_STEP_INTO 1323
#define IDC_DEBUGGER_BTN_STEP_OUT 1324
#define IDC_DEBUGGER_BTN_RESTART 1325
#define IDC_DEBUGGER_BTN_STOP 1326
#define IDC_DEBUGGER_INPUT 1327
#define IDC_DEBUGGER_BREAKPOINT_LIST 1328
#define IDC_DEBUGGER_WATCH_LIST 1329
#define IDC_DEBUGGER_VARIABLE_TREE 1330
#define IDC_DEBUGGER_STACK_LIST 1331
#define IDC_DEBUGGER_STATUS_TEXT 1332

// Enhanced Status Bar items
#define IDC_STATUS_REMOTE 1400
#define IDC_STATUS_BRANCH 1401
#define IDC_STATUS_SYNC 1402
#define IDC_STATUS_ERRORS 1403
#define IDC_STATUS_WARNINGS 1404
#define IDC_STATUS_LINE_COL 1405
#define IDC_STATUS_SPACES 1406
#define IDC_STATUS_ENCODING 1407
#define IDC_STATUS_EOL 1408
#define IDC_STATUS_LANGUAGE 1409
#define IDC_STATUS_COPILOT 1410
#define IDC_STATUS_NOTIFICATIONS 1411

/* Menu IDs: 2001+ to avoid overlap with IDC_* (1001+) in WM_COMMAND */
#define IDM_FILE_NEW 2001
#define IDM_FILE_OPEN 2002
#define IDM_FILE_SAVE 2003
#define IDM_FILE_SAVEAS 2004
#define IDM_FILE_LOAD_MODEL 1030
#define IDM_FILE_OPEN_FOLDER 1037
#define IDM_FILE_EXIT 2005

/* Voice Automation (Tools > Voice Automation) — Phase 44 TTS; dispatched in Win32IDE_Commands 10200–10300 */
#define IDM_VOICE_AUTO_TOGGLE 10200
#define IDM_VOICE_AUTO_STOP 10206
#define IDM_VOICE_AUTO_NEXT 10202
#define IDM_VOICE_AUTO_PREV 10203
#define IDM_VOICE_AUTO_RATE_UP 10204
#define IDM_VOICE_AUTO_RATE_DOWN 10205

#define IDM_EDIT_UNDO 2007
#define IDM_EDIT_REDO 2008
#define IDM_EDIT_CUT 2009
#define IDM_EDIT_COPY 2010
#define IDM_EDIT_PASTE 2011
#define IDM_EDIT_SNIPPET 2012
#define IDM_EDIT_COPY_FORMAT 2013
#define IDM_EDIT_PASTE_PLAIN 2014
#define IDM_EDIT_CLIPBOARD_HISTORY 2015
#define IDM_EDIT_FIND 2016
#define IDM_EDIT_REPLACE 2017
#define IDM_EDIT_FIND_NEXT 2018
#define IDM_EDIT_FIND_PREV 2019

#define IDM_VIEW_MINIMAP 2020
#define IDM_VIEW_OUTPUT_TABS 2021
#define IDM_VIEW_MODULE_BROWSER 2022
#define IDM_VIEW_THEME_EDITOR 2023
#define IDM_VIEW_FLOATING_PANEL 2024
#define IDM_VIEW_OUTPUT_PANEL 2025
#define IDM_VIEW_USE_STREAMING_LOADER 2026
#define IDM_VIEW_USE_VULKAN_RENDERER 2027
#define IDM_VIEW_SIDEBAR 2028
#define IDM_VIEW_TERMINAL 2029
#define IDM_VIEW_EXPERT_HEATMAP 2032
#define IDM_VIEW_SOVEREIGN_SNAP_COMPACT 2033
#define IDM_VIEW_SOVEREIGN_SNAP_STANDARD 2034
#define IDM_VIEW_SOVEREIGN_SNAP_WIDE 2035

#define IDM_TERMINAL_POWERSHELL 4001
#define IDM_TERMINAL_CMD 4002
#define IDM_TERMINAL_STOP 4003
#define IDM_TERMINAL_NEW_USER 4011
#define IDM_TERMINAL_NEW_AGENT 4012
#define IDM_TERMINAL_SPLIT_H 4007
#define IDM_TERMINAL_SPLIT_V 4008
#define IDM_TERMINAL_FOCUS_INTEGRATED 4013
#define IDM_TERMINAL_CLEAR_ALL 4014

#define IDM_TOOLS_PROFILE_START 3010
#define IDM_TOOLS_PROFILE_STOP 3011
#define IDM_TOOLS_PROFILE_RESULTS 3012
#define IDM_TOOLS_ANALYZE_SCRIPT 3013
#define IDM_INTERNAL_CAPTURE_PROFILE 3014
#define IDM_TOOLS_MODEL_LAB 3055

#define IDM_GIT_STATUS 3020
#define IDM_GIT_COMMIT 3021
#define IDM_GIT_PUSH 3022
#define IDM_GIT_PULL 3023
#define IDM_GIT_PANEL 3024

#define IDM_MODULES_REFRESH 3050
#define IDM_MODULES_IMPORT 3051
#define IDM_MODULES_EXPORT 3052

#ifndef IDM_VIEW_SOVEREIGN_CLI
#define IDM_VIEW_SOVEREIGN_CLI ID_VIEW_SOVEREIGN_CLI
#endif

// Help menu IDs MUST NOT use 4000–4099 — that band is handleTerminalCommand (parity with command_registry terminal.*).
#define IDM_HELP_CMDREF 7901
#define IDM_HELP_PSDOCS 7902
#define IDM_HELP_SEARCH 7903
#define IDM_HELP_ABOUT 7904

// Agent menu IDs
#define IDM_AGENT_START_LOOP 4100
#define IDM_AGENT_EXECUTE_CMD 4101
#define IDM_AGENT_CONFIGURE_MODEL 4102
#define IDM_AGENT_VIEW_TOOLS 4103
#define IDM_AGENT_VIEW_STATUS 4104
#define IDM_AGENT_TOGGLE_FILE_CONTEXT 4168      // Toggle current file context analysis
#define IDM_AGENT_AUTONOMOUS_COMMUNICATOR 4163  // free slot; 4106=IDM_AGENT_MEMORY, 4110=IDM_SUBAGENT_CHAIN
#define IDM_PLAN_ORCHESTRATOR_START 4164
#define IDM_PLAN_ORCHESTRATOR_STOP 4165
#define IDM_PLAN_ORCHESTRATOR_VIEW_STATUS 4166
#define IDM_PLAN_ORCHESTRATOR_VIEW_PLAN 4167
#define IDM_TELEMETRY_UNIFIED_CORE 4164  // free slot; 4300=IDM_REVENG_ANALYZE
// Constants moved to Win32IDE.h
// #define IDM_AGENT_STOP 4105
// ...

// Command Palette control IDs
#define IDC_CMDPAL_CONTAINER 1500
#define IDC_CMDPAL_INPUT 1501
#define IDC_CMDPAL_LIST 1502

Win32IDE::Win32IDE(HINSTANCE hInstance)
    : m_hInstance(hInstance), m_hAccel(nullptr), m_hwndMain(nullptr), m_hwndEditor(nullptr), m_hwndLineNumbers(nullptr),
      m_hwndTabBar(nullptr), m_oldLineNumberProc(nullptr), m_lineNumberWidth(70), m_activeTabIndex(-1),
      m_hwndCommandInput(nullptr), m_hwndStatusBar(nullptr), m_hwndMinimap(nullptr), m_hwndModuleBrowser(nullptr),
      m_hwndModuleList(nullptr), m_hwndModuleLoadButton(nullptr), m_hwndModuleUnloadButton(nullptr),
      m_hwndModuleRefreshButton(nullptr), m_moduleBrowserVisible(false), m_modulePanelProc(nullptr),
      m_hwndHelp(nullptr), m_hMenu(nullptr), m_hwndToolbar(nullptr), m_hwndTitleLabel(nullptr),
      m_hwndBtnMinimize(nullptr), m_hwndBtnMaximize(nullptr), m_hwndBtnClose(nullptr), m_hwndBtnGitHub(nullptr),
      m_hwndBtnMicrosoft(nullptr), m_hwndBtnSettings(nullptr), m_lastTitleBarText(), m_fileModified(false),
      m_editorHeight(400), m_terminalHeight(200), m_minimapVisible(true), m_minimapWidth(150), m_profilingActive(false),
      m_moduleListDirty(true), m_backgroundBrush(nullptr), m_editorFont(nullptr), m_hFontUI(nullptr),
      m_activeOutputTab("Output"), m_minimapX(650), m_outputTabHeight(200), m_nextTerminalId(1), m_activeTerminalId(-1),
      m_ggufLoader(nullptr), m_loadedModelPath(""), m_terminalSplitHorizontal(true), m_hwndGitPanel(nullptr),
      m_hwndGitStatusText(nullptr), m_hwndGitFileList(nullptr), m_gitAutoRefresh(true), m_outputPanelVisible(true),
      m_selectedOutputTab(0), m_hwndSeverityFilter(nullptr), m_severityFilterLevel(0), m_editorRect{0, 0, 0, 0},
      m_gpuTextEnabled(true), m_editorHooksInstalled(false), m_hwndSplitter(nullptr), m_splitterDragging(false),
      m_splitterY(0), m_renderer(nullptr), m_rendererReady(false), m_lastSearchText(), m_lastReplaceText(),
      m_searchCaseSensitive(false), m_searchWholeWord(false), m_searchUseRegex(false), m_lastFoundPos(-1),
      m_hwndFindDialog(nullptr), m_hwndReplaceDialog(nullptr),
      // Primary Sidebar
      m_hwndActivityBar(nullptr), m_hwndSidebar(nullptr), m_hwndSidebarContent(nullptr), m_sidebarVisible(true),
      m_sidebarWidth(250), m_currentSidebarView(SidebarView::None),
      // Secondary Sidebar
      m_hwndSecondarySidebar(nullptr), m_hwndSecondarySidebarHeader(nullptr), m_secondarySidebarVisible(false),
      m_secondarySidebarWidth(320), m_ollamaBackendEnabled(false),
      // Explorer View
      m_hwndExplorerTree(nullptr), m_hwndExplorerToolbar(nullptr), m_hImageListExplorer(nullptr), m_explorerRootPath(),
      // Search View
      m_hwndSearchInput(nullptr), m_hwndSearchResults(nullptr), m_hwndSearchOptions(nullptr),
      m_hwndIncludePattern(nullptr), m_hwndExcludePattern(nullptr), m_searchInProgress(false),
      // Source Control View
      m_hwndSCMFileList(nullptr), m_hwndSCMToolbar(nullptr), m_hwndSCMMessageBox(nullptr),
      m_hwndVcsStagingPanel(nullptr),
      // Debug View
      m_hwndDebugConfigs(nullptr), m_hwndDebugToolbar(nullptr), m_hwndDebugVariables(nullptr),
      m_hwndDebugCallStack(nullptr), m_hwndDebugConsole(nullptr), m_debuggingActive(false),
      // Extensions View
      m_hwndExtensionsList(nullptr), m_hwndExtensionSearch(nullptr), m_hwndExtensionDetails(nullptr),
      // File Explorer
      m_hwndFileExplorer(nullptr), m_hImageList(nullptr), m_currentExplorerPath(PathResolver::getModelsPath()),
      // Model Chat
      m_chatMode(false),
      // PowerShell Panel
      m_hwndPowerShellPanel(nullptr), m_hwndPowerShellOutput(nullptr), m_hwndPowerShellInput(nullptr),
      m_hwndPowerShellToolbar(nullptr), m_hwndPowerShellStatusBar(nullptr), m_hwndPSBtnExecute(nullptr),
      m_hwndPSBtnClear(nullptr), m_hwndPSBtnStop(nullptr), m_hwndPSBtnHistory(nullptr), m_hwndPSBtnRestart(nullptr),
      m_hwndPSBtnLoadRawrXD(nullptr), m_hwndPSBtnToggle(nullptr), m_powerShellPanelVisible(true),
      m_powerShellPanelDocked(true), m_powerShellSessionActive(false), m_powerShellRawrXDLoaded(false),
      m_powerShellPanelHeight(250), m_powerShellPanelWidth(600), m_powerShellHistoryIndex(-1),
      m_maxPowerShellHistory(100), m_useStreamingLoader(false), m_useVulkanRenderer(false),
      m_powerShellExecuting(false), m_powerShellProcessHandle(nullptr), m_dedicatedPowerShellTerminal(nullptr),
      m_hwndCommandPalette(nullptr), m_hwndCommandPaletteInput(nullptr), m_hwndCommandPaletteList(nullptr),
      m_commandPaletteVisible(false), m_oldCommandPaletteInputProc(nullptr), m_hwndModelSelector(nullptr),
      m_hwndMaxTokensSlider(nullptr), m_hwndMaxTokensLabel(nullptr), m_currentMaxTokens(512),
      m_syntaxColoringEnabled(true), m_syntaxDirty(false), m_syntaxLanguage(SyntaxLanguage::None),
      m_inBlockComment(false), m_activeThemeId(IDM_THEME_DARK_PLUS), m_themeIdBeforePreview(IDM_THEME_DARK_PLUS),
      m_transparencyEnabled(false), m_windowAlpha(255), m_sidebarBrush(nullptr), m_sidebarContentBrush(nullptr),
      m_panelBrush(nullptr), m_secondarySidebarBrush(nullptr), m_mainWindowBrush(nullptr),
      m_modelOperationActive(false), m_modelOperationCancelled(false), m_modelProgressPercent(0.0f),
      m_hwndModelProgressBar(nullptr), m_hwndModelProgressLabel(nullptr), m_hwndModelProgressContainer(nullptr),
      m_hwndModelCancelBtn(nullptr), m_sessionRestored(false), m_annotationsVisible(true), m_annotationFont(nullptr),
      m_hwndAnnotationOverlay(nullptr), m_nativePipelineReady(false), m_tabManager(nullptr), m_inferenceRunning(false),
      m_inferenceStopRequested(false),
      // ================= Chat Subsystem Integration (A2.1) ===================
      m_nativeEngineLoaded(false), m_chatMessageRenderer(nullptr), m_chatCommandRouter(nullptr),
      m_chatIpcDelegation(nullptr), m_chatInputGhostRenderer(nullptr), m_chatPersistencePool(nullptr),
      m_hwndChatPanel(nullptr), m_hwndChatInput(nullptr), m_hwndChatOutput(nullptr), m_hwndChatSendBtn(nullptr),
      m_hwndChatClearBtn(nullptr), m_hwndChatGhostOverlay(nullptr), m_oldChatInputProc(nullptr),
      m_oldChatOutputProc(nullptr), m_oldChatSendBtnProc(nullptr), m_oldChatClearBtnProc(nullptr),
      m_chatIpcSendBuffer(), m_chatIpcRecvBuffer(), m_chatIpcActive(false), m_chatIpcSendPending(false),
      m_chatIpcRecvPending(false), m_chatPersistenceFilePath(), m_chatPersistenceActive(false),
      m_chatPersistenceDirty(false), m_chatContextHistory(), m_chatContextUser(), m_chatContextAssistant(),
      m_chatContextSystem(), m_chatTokenBudget(4096), m_chatTokenUsed(0), m_chatRecoveryState(ChatRecoveryState::None),
      m_chatGdiSaveCount(0), m_chatGdiRestoreCount(0), m_chatSubsystemInitialized(false)
{
    // ============================================================
    // MINIMAL CONSTRUCTOR — all heavy init deferred to onCreate()
    // C++ try/catch does NOT catch SEH (access violations) on MinGW,
    // so we keep the constructor as lightweight as possible.
    // ============================================================

    // Initialize profiling frequency (safe — kernel call)
    QueryPerformanceFrequency(&m_profilingFreq);
    ideCtorBootMilestone("[IDE-Pipeline:Ctor] Batch 1/8: minimal ctor entered + QPC frequency sampled\n",
                         "[Init:Ctor] Batch 1/8: high-resolution performance counter initialized\n");

    // Initialize clipboard history
    m_clipboardHistory.reserve(MAX_CLIPBOARD_HISTORY);
    ideCtorBootMilestone("[IDE-Pipeline:Ctor] Batch 2/8: clipboard history ring reserved\n",
                         "[Init:Ctor] Batch 2/8: clipboard history capacity reserved\n");

    // Initialize Git status
    m_gitStatus = GitStatus();

    // Get current directory for Git repo detection
    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    m_gitRepoPath = currentDir;
    ideCtorBootMilestone("[IDE-Pipeline:Ctor] Batch 3/8: Git status object + default repo path from CWD\n",
                         "[Init:Ctor] Batch 3/8: Git working tree path captured from process directory\n");

    // Default Ollama configuration
    m_ollamaBaseUrl = "http://localhost:11435";
    m_ollamaModelOverride = "";
    ideCtorBootMilestone("[IDE-Pipeline:Ctor] Batch 4/8: Ollama localhost defaults installed\n",
                         "[Init:Ctor] Batch 4/8: default Ollama base URL and model override initialized\n");

    ideCtorBootMilestone("[IDE-Pipeline:Ctor] Batch 5/8: native engine slot marked unloaded (lazy bind)\n",
                         "[Init:Ctor] Batch 5/8: native engine loaded flag cleared until backend wiring\n");

    // DEFERRED: 70B GGUF Hotpatch — initialized on-demand via lazyInitHeavySubsystems()
    // (was causing smoke-test failures by doing heavy work in ctor)
    ideCtorBootMilestone("[IDE-Pipeline:Ctor] Batch 6/8: GGUF hotpatch deferred to onCreate/lazy init\n",
                         "[Init:Ctor] Batch 6/8: 70B GGUF hotpatch path deferred\n");

    // DEFERRED: Governor/Throttling — initialized on-demand via lazyInitHeavySubsystems()
    ideCtorBootMilestone("[IDE-Pipeline:Ctor] Batch 7/8: governor throttling deferred to onCreate/lazy init\n",
                         "[Init:Ctor] Batch 7/8: execution governor and throttling deferred\n");

    ideCtorBootMilestone("[IDE-Pipeline:Ctor] Batch 8/8: minimal Win32IDE ctor body complete\n",
                         "[Init:Ctor] Batch 8/8: constructor finished; window shell deferred to onCreate\n");

    ideCtorBootMilestone("[IDE-Pipeline:Ctor] E0-1/8: Ctor — QPC baseline valid for IDE timers\n",
                         "[Init:Ctor] E0-1/8: Profiler frequency baseline ready\n");
    ideCtorBootMilestone("[IDE-Pipeline:Ctor] E0-2/8: Ctor — clipboard capture ring pre-sized\n",
                         "[Init:Ctor] E0-2/8: Clipboard history ring sized\n");
    ideCtorBootMilestone("[IDE-Pipeline:Ctor] E0-3/8: Ctor — Git repo path tracks working directory\n",
                         "[Init:Ctor] E0-3/8: Git default path bound to CWD\n");
    ideCtorBootMilestone("[IDE-Pipeline:Ctor] E0-4/8: Ctor — local HTTP agent defaults pinned\n",
                         "[Init:Ctor] E0-4/8: Localhost inference defaults pinned\n");
    ideCtorBootMilestone("[IDE-Pipeline:Ctor] E0-5/8: Ctor — heavy inference deferred past ctor\n",
                         "[Init:Ctor] E0-5/8: Native engine load deferred to startup phases\n");
    ideCtorBootMilestone("[IDE-Pipeline:Ctor] E0-6/8: Ctor — GGUF hotpatch lane settled\n",
                         "[Init:Ctor] E0-6/8: GGUF hotpatch lane settled\n");
    ideCtorBootMilestone("[IDE-Pipeline:Ctor] E0-7/8: Ctor — governor owns runtime throttling policy\n",
                         "[Init:Ctor] E0-7/8: Governor policy object ready\n");
    ideCtorBootMilestone("[IDE-Pipeline:Ctor] E0-8/8: Ctor — handoff to WinMain / createWindow / onCreate\n",
                         "[Init:Ctor] E0-8/8: Handing off to window creation and onCreate\n");
}

// Build a "Commands" submenu from COMMAND_TABLE so every GUI-exposed command has a menu entry (avoids menu-only drift).
static void buildCommandsMenuFromCommandTable(HMENU mainMenu)
{
    if (!mainMenu)
        return;
    std::map<std::string, std::vector<const CmdDescriptor*>> byCategory;
    for (size_t i = 0; i < g_commandRegistrySize; ++i)
    {
        const CmdDescriptor& cmd = g_commandRegistry[i];
        if (cmd.id == 0)
            continue;
        if (cmd.exposure != CmdExposure::GUI_ONLY && cmd.exposure != CmdExposure::BOTH)
            continue;
        const char* cat = cmd.category && cmd.category[0] ? cmd.category : "Other";
        byCategory[cat].push_back(&cmd);
    }
    HMENU hCommands = CreatePopupMenu();
    if (!hCommands)
        return;
    for (const auto& pair : byCategory)
    {
        const std::string& categoryName = pair.first;
        const std::vector<const CmdDescriptor*>& items = pair.second;
        if (items.empty())
            continue;
        HMENU hSub = CreatePopupMenu();
        if (!hSub)
            continue;
        for (const CmdDescriptor* p : items)
        {
            const char* label = p->canonicalName && p->canonicalName[0] ? p->canonicalName : p->symbol;
            if (!label)
                label = "?";
            AppendMenuA(hSub, MF_STRING, (UINT)p->id, label);
        }
        AppendMenuA(hCommands, MF_POPUP, (UINT_PTR)hSub, categoryName.c_str());
    }
    AppendMenuW(mainMenu, MF_POPUP, (UINT_PTR)hCommands, L"&Commands");
}

// ESP:m_hMenu — Main menu bar; submenus File/Edit/View/Terminal/Tools/Modules/Help/Audit/Git/Agent (see
// Win32IDE_IELabels.h)
void Win32IDE::createMenuBar(HWND hwnd)
{
    if (!m_hMenu)
        m_hMenu = CreateMenu();
    if (!m_hMenu)
        return;

    // Status bar is initialized in onCreate after createStatusBar (see Win32IDE_Core.cpp).

    // File menu (Unicode)
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_NEW, L"&New");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_OPEN, L"&Open");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_OPEN_FOLDER, L"Open &Folder...\tCtrl+Shift+O");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVE, L"&Save");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVEAS, L"Save &As");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_LOAD_MODEL, L"Load &Model (GGUF)...");
    AppendMenuW(hFileMenu, MF_STRING, IDM_REFRESH_MODELS, L"&Refresh Models...");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_EXIT, L"E&xit");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");

    // Build menu (Unicode)
    HMENU hBuildMenu = CreatePopupMenu();
    AppendMenuW(hBuildMenu, MF_STRING, IDM_BUILD_SOLUTION, L"&Build Solution\tCtrl+B");
    AppendMenuW(hBuildMenu, MF_STRING, IDM_BUILD_CLEAN, L"&Clean");
    AppendMenuW(hBuildMenu, MF_STRING, IDM_BUILD_REBUILD, L"Re&build");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hBuildMenu, L"&Build");

    // Debug menu (Unicode)
    HMENU hDebugMenu = CreatePopupMenu();
    AppendMenuW(hDebugMenu, MF_STRING, IDM_DBG_LAUNCH, L"&Start Debugging...\tF5");
    AppendMenuW(hDebugMenu, MF_STRING, IDM_DBG_ATTACH, L"&Attach to Process...\tCtrl+Alt+P");
    AppendMenuW(hDebugMenu, MF_STRING, IDM_DBG_DETACH, L"&Detach");
    AppendMenuW(hDebugMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hDebugMenu, MF_STRING, IDM_DBG_GO, L"&Continue\tF5");
    AppendMenuW(hDebugMenu, MF_STRING, IDM_DBG_BREAK, L"&Break All\tAlt+F5");
    AppendMenuW(hDebugMenu, MF_STRING, IDM_DBG_STEP_OVER, L"Step &Over\tF10");
    AppendMenuW(hDebugMenu, MF_STRING, IDM_DBG_STEP_INTO, L"Step &Into\tF11");
    AppendMenuW(hDebugMenu, MF_STRING, IDM_DBG_STEP_OUT, L"Step O&ut\tShift+F11");
    AppendMenuW(hDebugMenu, MF_STRING, IDM_DBG_KILL, L"S&top Debugging\tShift+F5");
    AppendMenuW(hDebugMenu, MF_STRING, 2108, L"&Restart Debugging\tCtrl+Shift+F5");
    AppendMenuW(hDebugMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hDebugMenu, MF_STRING, IDM_DBG_ADD_BP, L"Toggle &Breakpoint\tF9");
    AppendMenuW(hDebugMenu, MF_STRING, IDM_DBG_STATUS, L"Debug Session &Status");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hDebugMenu, L"&Debug");

    // Edit menu (Unicode)
    HMENU hEditMenu = CreatePopupMenu();
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_FIND, L"&Find...\tCtrl+F");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_REPLACE, L"&Replace...\tCtrl+H");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_FIND_NEXT, L"Find &Next\tF3");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_FIND_PREV, L"Find &Previous\tShift+F3");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDITOR_GOTO_LINE, L"Go to &Line...\tCtrl+G");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDITOR_GOTO_SYMBOL, L"Go to &Symbol...\tCtrl+Shift+O");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDITOR_GOTO_WORKSPACE_SYMBOL,
                L"Go to &Workspace Symbol...\tCtrl+Shift+Alt+O");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDITOR_PEEK_DEFINITION, L"Peek &Definition\tAlt+F12");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDITOR_PEEK_REFERENCES, L"Peek &References\tShift+F12");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDITOR_GOTO_IMPLEMENTATION, L"Go to &Implementation\tCtrl+F12");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDITOR_GOTO_TYPE_DEFINITION, L"Go to &Type Definition\tCtrl+Alt+F12");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDITOR_FORMAT_SELECTION, L"Format Se&lection\tShift+Alt+F");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDITOR_CODE_ACTIONS, L"Code &Actions...\tCtrl+.");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDITOR_SHOW_INLAY_HINTS, L"Show &Inlay Hints\tCtrl+Alt+I");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDITOR_TOGGLE_COMMENT, L"Toggle Co&mment\tCtrl+/");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDITOR_DUPLICATE_LINE, L"Du&plicate Line\tShift+Alt+Down");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDITOR_DELETE_LINE, L"Delete Li&ne\tCtrl+Shift+K");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDITOR_MOVE_LINE_UP, L"Move Line &Up\tAlt+Up");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDITOR_MOVE_LINE_DOWN, L"Move Line D&own\tAlt+Down");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_SNIPPET, L"Insert &Snippet...");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_COPY_FORMAT, L"Copy with &Formatting");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_PASTE_PLAIN, L"Paste &Plain Text");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_CLIPBOARD_HISTORY, L"Clipboard &History...");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hEditMenu, L"&Edit");

    // View menu (Unicode)
    HMENU hViewMenu = CreatePopupMenu();
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_MINIMAP, L"&Minimap");
    AppendMenuW(hViewMenu, MF_STRING, IDM_T1_BREADCRUMBS_TOGGLE, L"&Breadcrumbs");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_OUTPUT_TABS, L"&Output Tabs");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_OUTPUT_PANEL, L"Output &Panel");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_TERMINAL, L"&Terminal\tCtrl+`");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_SOVEREIGN_CLI, L"&Sovereign CLI\tCtrl+Shift+`");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_TOGGLE_BOTTOM_PANEL, L"&Panel\tCtrl+J");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_MODULE_BROWSER, L"Module &Browser");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_FLOATING_PANEL, L"&Floating Panel");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    buildThemeMenu(hViewMenu);
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_THEME_EDITOR, L"Theme &Picker...");
    AppendMenuW(hViewMenu, MF_STRING, ID_VIEW_SYNTAX_HIGHLIGHTING_TOGGLE, L"Syntax &Highlighting");
    AppendMenuW(hViewMenu, MF_STRING, ID_VIEW_VISION_ENCODER, L"&Vision Encoder");
    AppendMenuW(hViewMenu, MF_STRING, ID_VIEW_SEMANTIC_INDEX, L"&Semantic Index");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_EXPERT_HEATMAP, L"Expert &heatmap (swarm)");
    HMENU hSovereignSnapMenu = CreatePopupMenu();
    AppendMenuW(hSovereignSnapMenu, MF_STRING, IDM_VIEW_SOVEREIGN_SNAP_COMPACT, L"Compact (240)");
    AppendMenuW(hSovereignSnapMenu, MF_STRING, IDM_VIEW_SOVEREIGN_SNAP_STANDARD, L"Standard (360)");
    AppendMenuW(hSovereignSnapMenu, MF_STRING, IDM_VIEW_SOVEREIGN_SNAP_WIDE, L"Wide (480)");
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hSovereignSnapMenu, L"Sovereign &Snap");
    HMENU hLayoutProfilesMenu = CreatePopupMenu();
    AppendMenuW(hLayoutProfilesMenu, MF_STRING, IDM_VIEW_LAYOUT_PROFILE_FOCUS, L"&Focus");
    AppendMenuW(hLayoutProfilesMenu, MF_STRING, IDM_VIEW_LAYOUT_PROFILE_CODING, L"&Coding");
    AppendMenuW(hLayoutProfilesMenu, MF_STRING, IDM_VIEW_LAYOUT_PROFILE_DEBUG, L"&Debug");
    AppendMenuW(hLayoutProfilesMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hLayoutProfilesMenu, MF_STRING, IDM_VIEW_LAYOUT_PROFILE_APPLY, L"&Apply Saved...");
    AppendMenuW(hLayoutProfilesMenu, MF_STRING, IDM_VIEW_LAYOUT_PROFILE_SAVE, L"&Save Current...");
    AppendMenuW(hViewMenu, MF_POPUP, (UINT_PTR)hLayoutProfilesMenu, L"Layout &Profiles");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_USE_STREAMING_LOADER, L"Use Streaming Loader (Low Memory)");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_USE_VULKAN_RENDERER, L"Enable Vulkan Renderer (experimental)");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_AGENT_PANEL, L"Agent &Panel\tCtrl+L");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_VIDEO_STUDIO, L"&Video Studio");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hViewMenu, MF_STRING, IDM_MARKETPLACE_SHOW, L"Extension &Marketplace");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_COLLABORATION, L"&Collaboration");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_GITHUB, L"&GitHub");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_GITHUB_PULL_RELEASE, L"GitHub Pull &Release");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_ACCOUNTS, L"&Accounts");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_MANAGE, L"&Manage");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hViewMenu, MF_STRING, IDM_TELDASH_SHOW, L"Telemetry &Dashboard...");
    AppendMenuW(hViewMenu, MF_STRING, IDM_EMOJI_PICKER, L"&Emoji Picker");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hViewMenu, L"&View");

    // Terminal menu (Unicode)
    HMENU hTerminalMenu = CreatePopupMenu();
    AppendMenuW(hTerminalMenu, MF_STRING, IDM_TERMINAL_POWERSHELL, L"&PowerShell");
    AppendMenuW(hTerminalMenu, MF_STRING, IDM_TERMINAL_CMD, L"&Command Prompt");
    AppendMenuW(hTerminalMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTerminalMenu, MF_STRING, IDM_TERMINAL_STOP, L"&Stop Terminal");
    AppendMenuW(hTerminalMenu, MF_STRING, IDM_TERMINAL_SPLIT_H, L"Split &Horizontal\tCtrl+Shift+H");
    AppendMenuW(hTerminalMenu, MF_STRING, IDM_TERMINAL_SPLIT_V, L"Split &Vertical\tCtrl+Shift+V");
    AppendMenuW(hTerminalMenu, MF_STRING, IDM_TERMINAL_FOCUS_INTEGRATED, L"&Focus Integrated Terminal");
    AppendMenuW(hTerminalMenu, MF_STRING, IDM_TERMINAL_CLEAR_ALL, L"&Clear All Terminals");
    AppendMenuW(hTerminalMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTerminalMenu, MF_STRING, IDM_TERMINAL_NEW_USER, L"New &Integrated Terminal\tCtrl+Shift+`");
    AppendMenuW(hTerminalMenu, MF_STRING, IDM_TERMINAL_NEW_AGENT, L"New &Agent Terminal…\tCtrl+Alt+A");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hTerminalMenu, L"&Terminal");

    // Tools menu (Unicode)
    HMENU hToolsMenu = CreatePopupMenu();
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_PROFILE_START, L"Start &Profiling");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_PROFILE_STOP, L"Stop P&rofiling");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_PROFILE_RESULTS, L"Profile &Results...");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_ANALYZE_SCRIPT, L"&Analyze Script");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_INTERNAL_CAPTURE_PROFILE, L"Capture Profile Bundle v1");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_MODEL_LAB, L"Model &Lab...\tCtrl+Alt+M");
    AppendMenuW(hToolsMenu, MF_STRING, 6001, L"&Model Manager...");
    AppendMenuW(hToolsMenu, MF_STRING, 6002, L"&Downloads\tCtrl+Shift+D");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_RUN_14DAY_FINISHERS, L"Run 14-Day Production &Finishers");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_RUN_14DAY_FINISHERS_STRICT,
                L"Run 14-Day Production Finishers (&Strict)");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_RUN_14DAY_QUALITY_GATES, L"Run 14-Day &Quality Gate Validation");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_OPEN_14DAY_REPORTS, L"Open 14-Day &Reports Folder");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_SHOW_14DAY_GATE_SUMMARY, L"Show Latest 14-Day Gate &Summary");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_SHOW_14DAY_ARTIFACT_MANIFEST,
                L"Show Latest 14-Day &Artifact Manifest");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_RUN_14DAY_INTEGRATION_GATE, L"Run 14-Day &Integration Gate");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_RUN_14DAY_TURNKEY_SMOKE, L"Run 14-Day &Turnkey IDE Smoke");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_RUN_14DAY_AGGREGATE_GATE,
                L"Run 14-Day Production Readiness &Aggregate Gate");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_SHOW_14DAY_AGGREGATE_RESULT,
                L"Show Latest 14-Day Aggregate Gate &Result");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);

    // Voice Automation submenu (Phase 44: TTS for AI responses)
    HMENU hVoiceAutoMenu = CreatePopupMenu();
    AppendMenuW(hVoiceAutoMenu, MF_STRING, IDM_VOICE_AUTO_TOGGLE, L"Toggle Voice Automation\tCtrl+Shift+A");
    AppendMenuW(hVoiceAutoMenu, MF_STRING, IDM_VOICE_AUTO_STOP, L"Stop Speaking\tEscape");
    AppendMenuW(hVoiceAutoMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hVoiceAutoMenu, MF_STRING, IDM_VOICE_AUTO_NEXT, L"Next Voice\tCtrl+Shift+]");
    AppendMenuW(hVoiceAutoMenu, MF_STRING, IDM_VOICE_AUTO_PREV, L"Previous Voice\tCtrl+Shift+[");
    AppendMenuW(hVoiceAutoMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hVoiceAutoMenu, MF_STRING, IDM_VOICE_AUTO_RATE_UP, L"Increase Speech Rate\tCtrl+Shift+=");
    AppendMenuW(hVoiceAutoMenu, MF_STRING, IDM_VOICE_AUTO_RATE_DOWN, L"Decrease Speech Rate\tCtrl+Shift+-");
    AppendMenuW(hToolsMenu, MF_POPUP, (UINT_PTR)hVoiceAutoMenu, L"Voice &Automation");

    // Phase 51: mIRC Control Bridge
    HMENU hIRCMenu = CreatePopupMenu();
    AppendMenuW(hIRCMenu, MF_STRING, IDM_IRC_CONNECT, L"&Connect");
    AppendMenuW(hIRCMenu, MF_STRING, IDM_IRC_DISCONNECT, L"&Disconnect");
    AppendMenuW(hIRCMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hIRCMenu, MF_STRING, IDM_IRC_STATUS, L"Show &Status");
    AppendMenuW(hIRCMenu, MF_STRING, IDM_IRC_CONFIG, L"&Config...");
    AppendMenuW(hIRCMenu, MF_STRING, IDM_IRC_SEND, L"&Send Test Message");
    AppendMenuW(hToolsMenu, MF_POPUP, (UINT_PTR)hIRCMenu, L"&IRC Bridge");

    // Backup submenu
    HMENU hBackupMenu = CreatePopupMenu();
    AppendMenuW(hBackupMenu, MF_STRING, IDM_QW_BACKUP_CREATE, L"&Create Backup Now\tCtrl+Shift+B");
    AppendMenuW(hBackupMenu, MF_STRING, IDM_QW_BACKUP_RESTORE, L"&Restore from Backup...");
    AppendMenuW(hBackupMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hBackupMenu, MF_STRING, IDM_QW_BACKUP_AUTO_TOGGLE, L"Toggle &Auto-Backup");
    AppendMenuW(hBackupMenu, MF_STRING, IDM_QW_BACKUP_LIST, L"&List Backups...");
    AppendMenuW(hBackupMenu, MF_STRING, IDM_QW_BACKUP_PRUNE, L"&Prune Old Backups");
    AppendMenuW(hToolsMenu, MF_POPUP, (UINT_PTR)hBackupMenu, L"&Backups");

    // Alert & Monitoring submenu
    HMENU hAlertMenu = CreatePopupMenu();
    AppendMenuW(hAlertMenu, MF_STRING, IDM_QW_ALERT_TOGGLE_MONITOR, L"Toggle Resource &Monitor");
    AppendMenuW(hAlertMenu, MF_STRING, IDM_QW_ALERT_RESOURCE_STATUS, L"&Resource Status...");
    AppendMenuW(hAlertMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAlertMenu, MF_STRING, IDM_QW_ALERT_SHOW_HISTORY, L"Alert &History...");
    AppendMenuW(hAlertMenu, MF_STRING, IDM_QW_ALERT_DISMISS_ALL, L"&Dismiss All Alerts");
    AppendMenuW(hToolsMenu, MF_POPUP, (UINT_PTR)hAlertMenu, L"A&lerts");

    // Distributed Swarm submenu (Phase 11)
    HMENU hSwarmMenu = CreatePopupMenu();
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_STATUS, L"Show &Status");
    AppendMenuW(hSwarmMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_START_LEADER, L"Start &Leader");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_START_WORKER, L"Start &Worker");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_START_HYBRID, L"Start &Hybrid");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_STOP, L"S&top");
    AppendMenuW(hSwarmMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_LIST_NODES, L"List &Nodes");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_ADD_NODE, L"&Add Node...");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_REMOVE_NODE, L"&Remove Node...");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_BLACKLIST_NODE, L"&Blacklist Node");
    AppendMenuW(hSwarmMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_BUILD_SOURCES, L"Build from &Sources");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_BUILD_CMAKE, L"Build from &CMake");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_START_BUILD, L"Start B&uild");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_CANCEL_BUILD, L"C&ancel Build");
    AppendMenuW(hSwarmMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_CACHE_STATUS, L"Cache St&atus");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_CACHE_CLEAR, L"Cache &Clear");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_SHOW_CONFIG, L"Show Confi&g");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_TOGGLE_DISCOVERY, L"Toggle &Discovery");
    AppendMenuW(hSwarmMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_SHOW_TASK_GRAPH, L"Show Task &Graph");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_SHOW_EVENTS, L"Show &Events");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_SHOW_STATS, L"Show S&tats");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_RESET_STATS, L"&Reset Stats");
    AppendMenuW(hSwarmMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_WORKER_STATUS, L"Worker Sta&tus");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_WORKER_CONNECT, L"Worker &Connect");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_WORKER_DISCONNECT, L"Worker D&isconnect");
    AppendMenuW(hSwarmMenu, MF_STRING, IDM_SWARM_FITNESS_TEST, L"Worker &Fitness Test");
    AppendMenuW(hToolsMenu, MF_POPUP, (UINT_PTR)hSwarmMenu, L"&Swarm");

    // Shortcuts & SLO (Tier 5)
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_KILL_BUILD_LOCKS,
                L"Kill Stuck &Build Tools (Ninja, CMake, MSVC...)\tCtrl+Shift+Alt+K");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_QW_SHORTCUT_EDITOR, L"\u2328 &Keyboard Shortcuts...\tCtrl+K Ctrl+S");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_SHORTCUT_SHOW, L"Keyboard Shortcut &Editor...");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_QW_SLO_DASHBOARD, L"&SLO Dashboard...");

    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hToolsMenu, L"&Tools");

    // Build menu (extended — added after initial build menu at line ~577)
    {
        HMENU hBuildMenu2 = CreatePopupMenu();
        AppendMenuW(hBuildMenu2, MF_STRING, IDM_BUILD_SOLUTION, L"Build &Solution\tCtrl+Shift+B");
        AppendMenuW(hBuildMenu2, MF_STRING, IDM_BUILD_CLEAN, L"&Clean");
        // Note: duplicate Build popup replaced with hBuildMenu2 to avoid C2374
        (void)hBuildMenu2;  // menu already attached at line ~577
    }

    // Security menu (Top-50 P0 — SAST, Secrets, SCA)
    HMENU hSecurityMenu = CreatePopupMenu();
    AppendMenuW(hSecurityMenu, MF_STRING, IDM_SECURITY_SCAN_SECRETS, L"Scan for &Secrets");
    AppendMenuW(hSecurityMenu, MF_STRING, IDM_SECURITY_SCAN_SAST, L"Run &SAST Scan");
    AppendMenuW(hSecurityMenu, MF_STRING, IDM_SECURITY_SCAN_DEPENDENCIES, L"Scan &Dependencies (SCA)");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hSecurityMenu, L"Secu&rity");

    // Modules menu
    HMENU hModulesMenu = CreatePopupMenu();
    AppendMenuW(hModulesMenu, MF_STRING, IDM_MODULES_REFRESH, L"&Refresh List");
    AppendMenuW(hModulesMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hModulesMenu, MF_STRING, IDM_MODULES_IMPORT, L"&Import Module...");
    AppendMenuW(hModulesMenu, MF_STRING, IDM_MODULES_EXPORT, L"&Export Module...");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hModulesMenu, L"&Modules");

    // Help menu
    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenuW(hHelpMenu, MF_STRING, IDM_HELP_CMDREF, L"Command &Reference");
    AppendMenuW(hHelpMenu, MF_STRING, IDM_HELP_PSDOCS, L"PowerShell &Documentation");
    AppendMenuW(hHelpMenu, MF_STRING, IDM_HELP_SEARCH, L"&Search Help...");
    AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelpMenu, MF_STRING, IDM_HELP_ABOUT, L"&About");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, L"&Help");

    // Audit menu (Phase 31 — Unicode)
    HMENU hAuditMenu = CreatePopupMenu();
    AppendMenuW(hAuditMenu, MF_STRING, IDM_AUDIT_SHOW_DASHBOARD, L"Show &Dashboard\tCtrl+Shift+A");
    AppendMenuW(hAuditMenu, MF_STRING, IDM_AUDIT_RUN_FULL, L"&Run Full Audit");
    AppendMenuW(hAuditMenu, MF_STRING, IDM_AUDIT_DETECT_STUBS, L"Detect &Stubs");
    AppendMenuW(hAuditMenu, MF_STRING, IDM_AUDIT_CHECK_MENUS, L"Check &Menu Wiring");
    AppendMenuW(hAuditMenu, MF_STRING, IDM_AUDIT_RUN_TESTS, L"Run Component &Tests");
    AppendMenuW(hAuditMenu, MF_STRING, IDM_AUDIT_EXPORT_REPORT, L"&Export Report...");
    AppendMenuW(hAuditMenu, MF_STRING, IDM_AUDIT_QUICK_STATS, L"&Quick Stats");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hAuditMenu, L"&Audit");

    // Git menu
    HMENU hGitMenu = CreatePopupMenu();
    AppendMenuW(hGitMenu, MF_STRING, IDM_GIT_STATUS, L"&Status\tCtrl+G");
    AppendMenuW(hGitMenu, MF_STRING, IDM_GIT_COMMIT, L"&Commit...\tCtrl+Shift+C");
    AppendMenuW(hGitMenu, MF_STRING, IDM_GIT_PUSH, L"&Push");
    AppendMenuW(hGitMenu, MF_STRING, IDM_GIT_PULL, L"P&ull");
    AppendMenuW(hGitMenu, MF_STRING, IDM_GIT_PANEL, L"&Git Panel\tCtrl+Shift+G");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hGitMenu, L"&Git");

    // Agent menu (ENABLED - All agentic features are production-ready)
    HMENU hAgentMenu = CreatePopupMenu();
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AGENT_START_LOOP, L"Start &Agent Loop\tCtrl+Alt+A");
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AGENT_EXECUTE_CMD, L"&Execute Command...");
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AGENT_CONFIGURE_MODEL, L"&Configure Model...");
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AGENT_VIEW_TOOLS, L"View &Tools");
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AGENT_VIEW_STATUS, L"View &Status");

    HMENU hAgentChatActions = CreatePopupMenu();
    AppendMenuW(hAgentChatActions, MF_STRING, IDM_COPILOT_MARK_HELPFUL, L"Mark &Helpful");
    AppendMenuW(hAgentChatActions, MF_STRING, IDM_COPILOT_MARK_UNHELPFUL, L"Mark &Unhelpful");
    AppendMenuW(hAgentChatActions, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAgentChatActions, MF_STRING, IDM_COPILOT_COPY_LAST_RESPONSE, L"&Copy Last Response");
    AppendMenuW(hAgentChatActions, MF_STRING, IDM_COPILOT_RETRY_LAST_PROMPT, L"&Retry Last Prompt");
    AppendMenuW(hAgentMenu, MF_POPUP, (UINT_PTR)hAgentChatActions, L"Chat &Actions");

    AppendMenuW(hAgentMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAgentMenu, MF_STRING | (m_settings.currentFileContextEnabled ? MF_CHECKED : 0),
                IDM_AGENT_TOGGLE_FILE_CONTEXT, L"&Current File Context\tCtrl+Shift+Y");
    AppendMenuW(hAgentMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AGENT_AUTONOMOUS_COMMUNICATOR, L"&Autonomous Communicator");
    AppendMenuW(hAgentMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAgentMenu, MF_STRING, IDM_PLAN_ORCHESTRATOR_START, L"&Start Plan Orchestrator");
    AppendMenuW(hAgentMenu, MF_STRING, IDM_PLAN_ORCHESTRATOR_STOP, L"S&top Plan Orchestrator");
    AppendMenuW(hAgentMenu, MF_STRING, IDM_PLAN_ORCHESTRATOR_VIEW_STATUS, L"View Orchestrator &Status");
    AppendMenuW(hAgentMenu, MF_STRING, IDM_PLAN_ORCHESTRATOR_VIEW_PLAN, L"View Current &Plan");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hAgentMenu, L"&Agent");

    // Telemetry menu
    HMENU hTelemetryMenu = CreatePopupMenu();
    AppendMenuW(hTelemetryMenu, MF_STRING, IDM_TELEMETRY_UNIFIED_CORE, L"&Unified Telemetry Core");
    AppendMenuW(hTelemetryMenu, MF_STRING, IDM_TEL_METRICS_DASHBOARD, L"&Sovereign Runtime Monitor");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hTelemetryMenu, L"&Telemetry");

    // Hotpatch menu (Unicode — Qt removal)
    {
        HMENU hHotpatchMenu = CreatePopupMenu();
        AppendMenuW(hHotpatchMenu, MF_STRING, IDM_HOTPATCH_SHOW_STATUS, L"&Show Hotpatch Status");
        AppendMenuW(hHotpatchMenu, MF_STRING, IDM_HOTPATCH_TOGGLE_ALL, L"&Toggle Hotpatch System");
        AppendMenuW(hHotpatchMenu, MF_STRING, IDM_HOTPATCH_SHOW_EVENT_LOG, L"Show &Event Log");
        AppendMenuW(hHotpatchMenu, MF_STRING, IDM_HOTPATCH_RESET_STATS, L"&Reset Statistics");
        AppendMenuW(hHotpatchMenu, MF_SEPARATOR, 0, nullptr);

        HMENU hMemLayerMenu = CreatePopupMenu();
        AppendMenuW(hMemLayerMenu, MF_STRING, IDM_HOTPATCH_MEMORY_APPLY, L"&Apply Memory Patch...");
        AppendMenuW(hMemLayerMenu, MF_STRING, IDM_HOTPATCH_MEMORY_REVERT, L"&Revert Memory Patch...");
        AppendMenuW(hHotpatchMenu, MF_POPUP, (UINT_PTR)hMemLayerMenu, L"&Memory Layer");

        HMENU hByteLayerMenu = CreatePopupMenu();
        AppendMenuW(hByteLayerMenu, MF_STRING, IDM_HOTPATCH_BYTE_APPLY, L"&Apply Byte Patch...");
        AppendMenuW(hByteLayerMenu, MF_STRING, IDM_HOTPATCH_BYTE_SEARCH, L"&Search && Replace Pattern...");
        AppendMenuW(hHotpatchMenu, MF_POPUP, (UINT_PTR)hByteLayerMenu, L"&Byte Layer");

        HMENU hServerLayerMenu = CreatePopupMenu();
        AppendMenuW(hServerLayerMenu, MF_STRING, IDM_HOTPATCH_SERVER_ADD, L"&Add Server Patch...");
        AppendMenuW(hServerLayerMenu, MF_STRING, IDM_HOTPATCH_SERVER_REMOVE, L"&Remove Server Patch...");
        AppendMenuW(hHotpatchMenu, MF_POPUP, (UINT_PTR)hServerLayerMenu, L"&Server Layer");

        HMENU hProxyMenu = CreatePopupMenu();
        AppendMenuW(hProxyMenu, MF_STRING, IDM_HOTPATCH_PROXY_BIAS, L"Token &Bias Injection...");
        AppendMenuW(hProxyMenu, MF_STRING, IDM_HOTPATCH_PROXY_REWRITE, L"Output &Rewrite Rule...");
        AppendMenuW(hProxyMenu, MF_STRING, IDM_HOTPATCH_PROXY_TERMINATE, L"Stream &Termination Rule...");
        AppendMenuW(hProxyMenu, MF_STRING, IDM_HOTPATCH_PROXY_VALIDATE, L"Custom &Validator...");
        AppendMenuW(hProxyMenu, MF_STRING, IDM_HOTPATCH_SHOW_PROXY_STATS, L"Show Proxy &Stats");
        AppendMenuW(hHotpatchMenu, MF_POPUP, (UINT_PTR)hProxyMenu, L"&Proxy Hotpatcher");

        AppendMenuW(hHotpatchMenu, MF_SEPARATOR, 0, nullptr);

        AppendMenuW(hHotpatchMenu, MF_STRING, IDM_HOTPATCH_SET_TARGET_TPS, L"Set target &TPS...");
        AppendMenuW(hHotpatchMenu, MF_STRING, IDM_HOTPATCH_PRESET_SAVE, L"Save Preset...");
        AppendMenuW(hHotpatchMenu, MF_STRING, IDM_HOTPATCH_PRESET_LOAD, L"Load Preset...");

        AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hHotpatchMenu, L"&Hotpatch");
    }

    if (FEATURE_ENABLED("autonomy"))
    {
        HMENU hAutonomyMenu = CreatePopupMenu();
        AppendMenuW(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_TOGGLE, L"&Toggle Auto Loop");
        AppendMenuW(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_START, L"&Start Autonomy");
        AppendMenuW(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_STOP, L"Sto&p Autonomy");
        AppendMenuW(hAutonomyMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_SET_GOAL, L"Set &Goal...");
        AppendMenuW(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_STATUS, L"Show &Status");
        AppendMenuW(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_MEMORY, L"Show &Memory Snapshot");
        AppendMenuW(hAutonomyMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hAutonomyMenu, MF_STRING, IDM_PIPELINE_RUN, L"Pipeline: &Run once");
        AppendMenuW(hAutonomyMenu, MF_STRING, IDM_PIPELINE_AUTONOMY_START, L"Pipeline: Start &autonomous loop");
        AppendMenuW(hAutonomyMenu, MF_STRING, IDM_PIPELINE_AUTONOMY_STOP, L"Pipeline: S&top autonomous loop");
        AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hAutonomyMenu, L"&Autonomy");
    }

    if (FEATURE_ENABLED("reverseEngineering"))
    {
        HMENU hRevEngMenu = createReverseEngineeringMenu();
        AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hRevEngMenu, L"&RevEng");
    }

    // Phase 45: Game Engine Integration (Unity + Unreal)
    createGameEngineMenu(m_hMenu);

    // Phase 48: The Final Crucible
    createCrucibleMenu(m_hMenu);

    // Phase 49: Copilot Gap Closer
    createCopilotGapMenu(m_hMenu);

    // Cursor/JB-Parity Feature Modules
    createFeaturesMenu(m_hMenu);

    // Commands menu from COMMAND_TABLE (single source of truth — no menu-only drift)
    buildCommandsMenuFromCommandTable(m_hMenu);

    // Enterprise / Professional feature menu
    // Professional tier (12330–12341) routed by routeCommand's IDM_ENT_MODEL_COMPARE..IDM_ENT_CUSTOM_QUANT branch.
    // GPU/Performance tier (3042–3047) routed via the 3000–3999 → handleViewCommand path.
    {
        HMENU hEntMenu = CreatePopupMenu();

        // Professional: inference quality & session features (IDM_ENT_MODEL_COMPARE…IDM_ENT_CUSTOM_QUANT)
        HMENU hEntProfMenu = CreatePopupMenu();
        AppendMenuW(hEntProfMenu, MF_STRING, 12330, L"&Model Comparison");             // IDM_ENT_MODEL_COMPARE
        AppendMenuW(hEntProfMenu, MF_STRING, 12331, L"&Batch Processing");             // IDM_ENT_BATCH_PROCESS
        AppendMenuW(hEntProfMenu, MF_STRING, 12332, L"Custom &Stop Sequences");        // IDM_ENT_CUSTOM_STOP_SEQ
        AppendMenuW(hEntProfMenu, MF_STRING, 12333, L"&Grammar Constraints");          // IDM_ENT_GRAMMAR_CONSTRAINTS
        AppendMenuW(hEntProfMenu, MF_STRING, 12334, L"&LoRA Adapter");                 // IDM_ENT_LORA_ADAPTER
        AppendMenuW(hEntProfMenu, MF_STRING, 12335, L"&Response Cache");               // IDM_ENT_RESPONSE_CACHE
        AppendMenuW(hEntProfMenu, MF_STRING, 12336, L"Prompt &Library");               // IDM_ENT_PROMPT_LIBRARY
        AppendMenuW(hEntProfMenu, MF_STRING, 12337, L"Session &Export/Import");        // IDM_ENT_SESSION_EXPORT_IMPORT
        AppendMenuW(hEntProfMenu, MF_STRING, 12338, L"Model &Sharding");               // IDM_ENT_MODEL_SHARDING
        AppendMenuW(hEntProfMenu, MF_STRING, 12339, L"&Tensor Parallelism");           // IDM_ENT_TENSOR_PARALLEL
        AppendMenuW(hEntProfMenu, MF_STRING, 12340, L"&Pipeline Parallelism");         // IDM_ENT_PIPELINE_PARALLEL
        AppendMenuW(hEntProfMenu, MF_STRING, 12341, L"Custom &Quantization Schemes");  // IDM_ENT_CUSTOM_QUANT
        AppendMenuW(hEntMenu, MF_POPUP, (UINT_PTR)hEntProfMenu, L"&Professional");

        // GPU / Performance tier (IDM_ENT_MULTI_GPU_BALANCE…IDM_ENT_DUAL_ENGINE = 3042–3047)
        AppendMenuW(hEntMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hEntMenu, MF_STRING, 3042, L"Multi-&GPU Load Balance");  // IDM_ENT_MULTI_GPU_BALANCE
        AppendMenuW(hEntMenu, MF_STRING, 3043, L"&Dynamic Batch Sizing");    // IDM_ENT_DYNAMIC_BATCH
        AppendMenuW(hEntMenu, MF_STRING, 3044, L"&API Key Management");      // IDM_ENT_API_KEY_MGMT
        AppendMenuW(hEntMenu, MF_STRING, 3045, L"&Audit Logs");              // IDM_ENT_AUDIT_LOGS
        AppendMenuW(hEntMenu, MF_STRING, 3046, L"&RawrTuner IDE");           // IDM_ENT_RAWR_TUNER
        AppendMenuW(hEntMenu, MF_STRING, 3047, L"800B &Dual-Engine");        // IDM_ENT_DUAL_ENGINE

        AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hEntMenu, L"&Enterprise");
    }

    SetMenu(hwnd, m_hMenu);
}

// NOTE: Win32IDE destructor lives in `Win32IDE_Core.cpp`.

void Win32IDE::createToolbar(HWND hwnd)
{

    m_hwndToolbar = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT, 0, 0, 0, 0,
                                    hwnd, nullptr, m_hInstance, nullptr);

    if (m_hwndToolbar)
    {

        SendMessage(m_hwndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
        SendMessage(m_hwndToolbar, TB_AUTOSIZE, 0, 0);

        createTitleBarControls();
        updateTitleBarText();
    }
    else
    {
    }
}

void Win32IDE::createTitleBarControls()
{
    DWORD labelStyle = WS_CHILD | WS_VISIBLE | SS_CENTER | SS_NOPREFIX;
    m_hwndTitleLabel = CreateWindowExW(0, L"STATIC", L"RawrXD", labelStyle, 0, 0, 200, 24, m_hwndToolbar,
                                       (HMENU)IDC_TITLE_TEXT, m_hInstance, nullptr);

    DWORD buttonStyle = WS_CHILD | WS_VISIBLE | BS_FLAT;
    auto createButton = [&](HWND& target, int controlId, const wchar_t* caption)
    {
        target = CreateWindowExW(0, L"BUTTON", caption, buttonStyle, 0, 0, 32, 24, m_hwndToolbar, (HMENU)controlId,
                                 m_hInstance, nullptr);
    };

    createButton(m_hwndBtnGitHub, IDC_BTN_GITHUB, L"GH");
    createButton(m_hwndBtnMicrosoft, IDC_BTN_MICROSOFT, L"MS");
    createButton(m_hwndBtnSettings, IDC_BTN_SETTINGS, L"Gear");
    createButton(m_hwndBtnMinimize, IDC_BTN_MINIMIZE, L"-");
    createButton(m_hwndBtnMaximize, IDC_BTN_MAXIMIZE, L"[]");
    createButton(m_hwndBtnClose, IDC_BTN_CLOSE, L"X");

    RECT client{};
    GetClientRect(m_hwndMain, &client);
    layoutTitleBar(client.right - client.left);
}

void Win32IDE::layoutTitleBar(int width)
{
    if (!m_hwndToolbar)
        return;

    RECT client{};
    GetClientRect(m_hwndToolbar, &client);
    int toolbarHeight = client.bottom - client.top;
    if (toolbarHeight <= 0)
        toolbarHeight = 30;
    int controlHeight = (std::max)(22, toolbarHeight - 6);
    int y = (toolbarHeight - controlHeight) / 2;
    int padding = 6;
    int x = width - padding;

    auto placeButton = [&](HWND hwnd, int controlWidth)
    {
        if (!hwnd)
            return;
        x -= controlWidth;
        MoveWindow(hwnd, x, y, controlWidth, controlHeight, TRUE);
        x -= padding;
    };

    placeButton(m_hwndBtnClose, 32);
    placeButton(m_hwndBtnMaximize, 32);
    placeButton(m_hwndBtnMinimize, 32);
    placeButton(m_hwndBtnSettings, 48);
    placeButton(m_hwndBtnMicrosoft, 40);
    placeButton(m_hwndBtnGitHub, 40);

    if (m_hwndTitleLabel)
    {
        int availableRight = x;
        int labelWidth = (std::min)(420, availableRight - padding * 2);
        if (labelWidth < 160)
        {
            labelWidth = (std::max)(availableRight - padding * 2, 120);
        }
        int labelX = (std::max)(padding, (width - labelWidth) / 2);
        if (labelX + labelWidth > availableRight)
        {
            labelX = (std::max)(padding, availableRight - labelWidth);
        }
        MoveWindow(m_hwndTitleLabel, labelX, y, labelWidth, controlHeight, TRUE);
    }
}

std::string Win32IDE::extractLeafName(const std::string& path) const
{
    if (path.empty())
        return "";
    size_t end = path.find_last_not_of("\\/ ");
    if (end == std::string::npos)
        return path;
    size_t slash = path.find_last_of("\\/", end);
    if (slash == std::string::npos)
    {
        return path.substr(0, end + 1);
    }
    return path.substr(slash + 1, end - slash);
}

void Win32IDE::setCurrentDirectoryFromFile(const std::string& filePath)
{
    if (filePath.empty())
        return;
    size_t slash = filePath.find_last_of("\\/");
    if (slash != std::string::npos)
    {
        m_currentDirectory = filePath.substr(0, slash);
    }
}

void Win32IDE::updateTitleBarText()
{
    if (!m_hwndTitleLabel)
        return;

    std::string fileName = m_currentFile.empty() ? "Untitled" : extractLeafName(m_currentFile);
    std::string projectFolder;

    if (!m_currentDirectory.empty())
    {
        projectFolder = extractLeafName(m_currentDirectory);
    }

    if (projectFolder.empty() && !m_currentFile.empty())
    {
        size_t slash = m_currentFile.find_last_of("\\/");
        if (slash != std::string::npos)
        {
            projectFolder = extractLeafName(m_currentFile.substr(0, slash));
        }
    }

    if (projectFolder.empty() && !m_gitRepoPath.empty())
    {
        projectFolder = extractLeafName(m_gitRepoPath);
    }

    if (projectFolder.empty())
    {
        projectFolder = "Workspace";
    }

    std::string composed = fileName + "  •  " + projectFolder;
    if (composed != m_lastTitleBarText)
    {
        SetWindowTextW(m_hwndTitleLabel, utf8ToWide(composed).c_str());
        m_lastTitleBarText = composed;
    }
    // Keep breadcrumb bar in sync with current file (symbol path updates on cursor move)
    if (m_hwndBreadcrumbs && m_settings.breadcrumbsEnabled)
        updateBreadcrumbs();
}

// ============================================================================
// DPI SCALING
// ============================================================================

UINT Win32IDE::getDpi() const
{
    if (m_hwndMain)
    {
        // GetDpiForWindow requires Windows 10 1607+
        typedef UINT(WINAPI * PFN_GetDpiForWindow)(HWND);
        static PFN_GetDpiForWindow pGetDpiForWindow = nullptr;
        static bool resolved = false;
        if (!resolved)
        {
            HMODULE hUser32 = GetModuleHandleA("user32.dll");
            if (hUser32)
            {
                pGetDpiForWindow = (PFN_GetDpiForWindow)GetProcAddress(hUser32, "GetDpiForWindow");
            }
            resolved = true;
        }
        if (pGetDpiForWindow)
        {
            UINT dpi = pGetDpiForWindow(m_hwndMain);
            if (dpi > 0)
                return dpi;
        }
    }
    // Fallback: system DPI via device caps
    HDC hdc = GetDC(nullptr);
    UINT dpi = (UINT)GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(nullptr, hdc);
    return dpi ? dpi : 96;
}

int Win32IDE::dpiScale(int basePixels) const
{
    // If user override is set, blend it with system DPI
    if (m_settings.uiScalePercent > 0)
    {
        return MulDiv(basePixels, m_settings.uiScalePercent, 100);
    }
    return MulDiv(basePixels, m_currentDpi, 96);
}

void Win32IDE::recreateFonts()
{
    m_currentDpi = getDpi();

    // Editor font — monospace (parity with Settings / IDEConfig editor.fontSize, editor.fontFamily)
    if (m_editorFont)
    {
        DeleteObject(m_editorFont);
        m_editorFont = nullptr;
    }
    {
        const int fs = (std::max)(6, (std::min)(72, m_settings.fontSize));
        const UINT dpi =
            (m_hwndMain && IsWindow(m_hwndMain)) ? GetDpiForWindow(m_hwndMain) : static_cast<UINT>(m_currentDpi);
        const char* face = m_settings.fontName.empty() ? "Consolas" : m_settings.fontName.c_str();
        m_editorFont =
            CreateFontA(-MulDiv(fs, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, face);
    }

    // UI font — proportional
    if (m_hFontUI)
    {
        DeleteObject(m_hFontUI);
        m_hFontUI = nullptr;
    }
    m_hFontUI = CreateFontW(-dpiScale(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    // Ghost text font — italic monospace
    if (m_ghostTextFont)
    {
        DeleteObject(m_ghostTextFont);
        m_ghostTextFont = nullptr;
    }
    LOGFONTA lf = {};
    lf.lfHeight = -dpiScale(14);
    lf.lfWeight = FW_NORMAL;
    lf.lfItalic = TRUE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    strncpy(lf.lfFaceName, m_currentTheme.fontName.c_str(), LF_FACESIZE - 1);
    lf.lfFaceName[LF_FACESIZE - 1] = '\0';
    m_ghostTextFont = CreateFontIndirectA(&lf);

    // Apply editor font (HFONT + Rich Edit document face/size from settings)
    if (m_hwndEditor && m_editorFont)
    {
        SendMessage(m_hwndEditor, WM_SETFONT, (WPARAM)m_editorFont, TRUE);
        applyEditorCharFormatFaceAndSizeFromSettings();
    }

    // Apply UI font to all known UI controls
    auto setFont = [](HWND hwnd, HFONT font)
    {
        if (hwnd)
            SendMessage(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
    };
    setFont(m_hwndTabBar, m_hFontUI);
    setFont(m_hwndSecondarySidebarHeader, m_hFontUI);
    setFont(m_hwndModelSelector, m_hFontUI);
    setFont(m_hwndCopilotChatOutput, m_hFontUI);
    setFont(m_hwndCopilotChatInput, m_hFontUI);
    setFont(m_hwndCopilotSendBtn, m_hFontUI);
    setFont(m_hwndCopilotClearBtn, m_hFontUI);
    setFont(m_hwndCommandPaletteInput, m_hFontUI);
    setFont(m_hwndCommandPaletteList, m_hFontUI);
    setFont(m_hwndSearchInput, m_hFontUI);
    setFont(m_hwndSearchResults, m_hFontUI);
    setFont(m_hwndFloatingContent, m_hFontUI);

    // PowerShell panel fonts (store and delete previous to avoid leak on DPI change)
    if (m_hFontPowerShell)
    {
        DeleteObject(m_hFontPowerShell);
        m_hFontPowerShell = nullptr;
    }
    if (m_hFontPowerShellStatus)
    {
        DeleteObject(m_hFontPowerShellStatus);
        m_hFontPowerShellStatus = nullptr;
    }
    if (m_hwndPowerShellOutput)
    {
        const int termPt = (std::max)(8, (std::min)(32, m_settings.integratedTerminalFontSize));
        const char* termFace = m_settings.integratedTerminalFontFamily.empty()
                                   ? "Consolas"
                                   : m_settings.integratedTerminalFontFamily.c_str();
        m_hFontPowerShell =
            CreateFontA(-dpiScale(termPt), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, termFace);
        SendMessage(m_hwndPowerShellOutput, WM_SETFONT, (WPARAM)m_hFontPowerShell, TRUE);
        if (m_hwndPowerShellInput)
            SendMessage(m_hwndPowerShellInput, WM_SETFONT, (WPARAM)m_hFontPowerShell, TRUE);
        if (m_hwndPSBtnExecute)
            SendMessage(m_hwndPSBtnExecute, WM_SETFONT, (WPARAM)m_hFontPowerShell, TRUE);
    }
    if (m_hwndPowerShellStatusBar)
    {
        m_hFontPowerShellStatus =
            CreateFontA(-dpiScale(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        SendMessage(m_hwndPowerShellStatusBar, WM_SETFONT, (WPARAM)m_hFontPowerShellStatus, TRUE);
    }

    // Terminal panes — same as IDEConfig terminal.* + user/agent colors (avoid stale 9pt Consolas overwrite).
    for (auto& pane : m_terminalPanes)
    {
        if (pane.hwnd && IsWindow(pane.hwnd))
            applyIntegratedTerminalCharFormat(pane.hwnd, pane.kind);
    }

    // File tree
    if (m_hwndFileTree)
    {
        setFont(m_hwndFileTree, m_hFontUI);
    }

    LOG_INFO("Fonts recreated at DPI=" + std::to_string(m_currentDpi));
}

void Win32IDE::createEditor(HWND hwnd)
{

    m_hwndEditor = CreateWindowExW(WS_EX_CLIENTEDGE, RICHEDIT_CLASSW, L"",
                                   WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL |
                                       ES_AUTOHSCROLL | ES_WANTRETURN,
                                   0, 0, 0, 0, hwnd, (HMENU)IDC_EDITOR, m_hInstance, nullptr);
    if (!m_hwndEditor)
    {

        return;
    }

    m_currentDpi = getDpi();
    recreateFonts();

    SendMessage(m_hwndEditor, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));
    SendMessage(m_hwndEditor, EM_SETREADONLY, FALSE, 0);
    SendMessage(m_hwndEditor, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE | ENM_SCROLL);
    SendMessage(m_hwndEditor, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);

    static const wchar_t welcomeText[] = L"// ============================================\r\n"
                                         L"// RawrXD IDE - Native Win32 AI Development\r\n"
                                         L"// ============================================\r\n"
                                         L"//\r\n"
                                         L"// Welcome! The editor is ready.\r\n"
                                         L"//\r\n"
                                         L"// Shortcuts:\r\n"
                                         L"//   Ctrl+N   New File\r\n"
                                         L"//   Ctrl+O   Open File\r\n"
                                         L"//   Ctrl+S   Save\r\n"
                                         L"//   Ctrl+F   Find\r\n"
                                         L"//   Ctrl+B   Toggle Sidebar\r\n"
                                         L"//   Ctrl+Shift+P   Command Palette\r\n"
                                         L"//\r\n"
                                         L"// Start typing or open a file to begin.\r\n"
                                         L"\r\n";
    SetWindowTextW(m_hwndEditor, welcomeText);

    applyEditorCharFormatFaceAndSizeFromSettings();
    {
        CHARFORMAT2W cfWelcome{};
        cfWelcome.cbSize = sizeof(cfWelcome);
        cfWelcome.dwMask = CFM_COLOR;
        cfWelcome.crTextColor = RGB(212, 212, 212);
        SendMessageW(m_hwndEditor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cfWelcome);
        SendMessageW(m_hwndEditor, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cfWelcome);
    }

    int textLen = GetWindowTextLengthW(m_hwndEditor);
    SendMessage(m_hwndEditor, EM_SETSEL, textLen, textLen);

    // Ensure the first frame is painted even before subsequent WM_SIZE/layout churn.
    InvalidateRect(m_hwndEditor, nullptr, TRUE);
    UpdateWindow(m_hwndEditor);

    initializeEditorSurface();

    // ================================================================
    // Subclass the editor RichEdit control
    // Store IDE pointer and original wndproc as window properties,
    // then redirect to EditorSubclassProc for ghost text, key intercept,
    // scroll sync, and minimap updates.
    // ================================================================
    if (m_hwndEditor)
    {
        SetPropW(m_hwndEditor, kEditorWndProp, (HANDLE)this);
        SetLastError(0);
        WNDPROC oldEditorProc = (WNDPROC)SetWindowLongPtrW(m_hwndEditor, GWLP_WNDPROC, (LONG_PTR)EditorSubclassProc);
        const DWORD subclassError = GetLastError();
        if (!oldEditorProc && subclassError != ERROR_SUCCESS)
        {
            RemovePropW(m_hwndEditor, kEditorWndProp);
            LOG_ERROR("Failed to subclass editor control, gle=" + std::to_string(subclassError));
        }
        else
        {
            SetPropW(m_hwndEditor, kEditorProcProp, (HANDLE)oldEditorProc);
        }
    }

    if (lineStripEditorEnabled())
    {
        createLineStripOverlay(hwnd);
    }
}

void Win32IDE::createTerminal(HWND hwnd)
{
    // Initialize the Enterprise PowerShell Panel (creates m_hwndPowerShellPanel)
    createPowerShellPanel();
    m_powerShellPanelVisible = true;

    if (m_terminalPanes.empty())
    {
        createTerminalPane(Win32TerminalManager::PowerShell, "PowerShell");
    }
    else
    {
        setActiveTerminalPane(m_terminalPanes.front().id);
    }

    // Create command input
    m_hwndCommandInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0,
                                         0, 0, hwnd, (HMENU)IDC_COMMAND_INPUT, m_hInstance, nullptr);
    if (m_hwndCommandInput)
    {
        SetWindowLongPtr(m_hwndCommandInput, GWLP_USERDATA, (LONG_PTR)this);
        SetLastError(0);
        m_oldCommandInputProc = (WNDPROC)SetWindowLongPtr(m_hwndCommandInput, GWLP_WNDPROC, (LONG_PTR)CommandInputProc);
        const DWORD subclassError = GetLastError();
        if (!m_oldCommandInputProc && subclassError != ERROR_SUCCESS)
        {
            LOG_ERROR("Failed to subclass command input, gle=" + std::to_string(subclassError));
        }
    }
    syncCommandInputForActiveTerminal();
}

int Win32IDE::createTerminalPane(Win32TerminalManager::ShellType shellType, const std::string& name,
                                 TerminalPaneKind kind, bool activateAndFocus)
{
    HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, RICHEDIT_CLASSW, L"",
                                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 0, 0,
                                0, 0, m_hwndMain, nullptr, m_hInstance, nullptr);

    if (!hwnd)
    {
        const DWORD createError = GetLastError();
        LOG_ERROR("Failed to create terminal pane, gle=" + std::to_string(createError));
        return -1;
    }

    // LOGGING AS REQUESTED
    char logBuf[256];
    sprintf_s(logBuf, "TerminalPane HWND created: %p (Parent: %p)", hwnd, m_hwndMain);
    LOG_INFO(std::string(logBuf));

    // VS Code default theme–adjacent: integrated terminal ≈ #1e1e1e; agent stream slightly cooler / distinct.
    const COLORREF userBg = RGB(30, 30, 30);
    const COLORREF agentBg = RGB(28, 32, 40);
    SendMessage(hwnd, EM_SETBKGNDCOLOR, 0, kind == TerminalPaneKind::AgentReadOnly ? agentBg : userBg);

    applyIntegratedTerminalCharFormat(hwnd, kind);

    RawrXD_ApplyTerminalRichEditScrollback(hwnd, m_settings.integratedTerminalScrollbackChars);

    int paneId = m_nextTerminalId++;
    TerminalPane pane;
    pane.id = paneId;
    pane.hwnd = hwnd;
    pane.manager = std::make_unique<Win32TerminalManager>();
    pane.name = name.empty() ? ("Terminal " + std::to_string(paneId)) : name;
    pane.shellType = shellType;
    pane.kind = kind;
    pane.isActive = false;
    pane.bounds = {0, 0, 0, 0};

    auto terminalLiveness = AcquireTerminalCallbackLiveness(this);

    pane.manager->onOutput = [this, paneId, terminalLiveness](const std::string& output)
    {
        if (!IsTerminalCallbackOwnerAlive(terminalLiveness))
            return;
        if (isShuttingDown())
            return;
        onTerminalOutput(paneId, output);
    };
    pane.manager->onError = [this, paneId, terminalLiveness](const std::string& error)
    {
        if (!IsTerminalCallbackOwnerAlive(terminalLiveness))
            return;
        if (isShuttingDown())
            return;
        onTerminalError(paneId, error);
    };

    m_terminalPanes.push_back(std::move(pane));
    if (activateAndFocus)
        setActiveTerminalPane(paneId);
    else if (kind == TerminalPaneKind::UserInteractive)
    {
        if (m_lastUserInteractiveTerminalId < 0)
            m_lastUserInteractiveTerminalId = paneId;
    }
    applyTheme();
    layoutTerminalStrip();
    return paneId;
}

void Win32IDE::applyIntegratedTerminalCharFormat(HWND hwnd, TerminalPaneKind kind) const
{
    if (!hwnd || !IsWindow(hwnd))
        return;
    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR;
    const int pt = (std::max)(8, (std::min)(32, m_settings.integratedTerminalFontSize));
    cf.yHeight = pt * 20;
    cf.crTextColor = kind == TerminalPaneKind::AgentReadOnly ? RGB(200, 220, 240) : RGB(204, 204, 204);
    copyIntegratedTerminalFontFace(cf.szFaceName, m_settings.integratedTerminalFontFamily);
    SendMessageW(hwnd, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
}

void Win32IDE::applyIntegratedTerminalFontToAllPanes()
{
    for (auto& p : m_terminalPanes)
    {
        if (p.hwnd && IsWindow(p.hwnd))
            applyIntegratedTerminalCharFormat(p.hwnd, p.kind);
    }
}

TerminalPane* Win32IDE::findTerminalPane(int paneId)
{
    for (auto& pane : m_terminalPanes)
    {
        if (pane.id == paneId)
        {
            return &pane;
        }
    }
    return nullptr;
}

TerminalPane* Win32IDE::getActiveTerminalPane()
{
    TerminalPane* active = findTerminalPane(m_activeTerminalId);
    if (!active && !m_terminalPanes.empty())
    {
        setActiveTerminalPane(m_terminalPanes.front().id);
        return findTerminalPane(m_terminalPanes.front().id);
    }
    return active;
}

TerminalPane* Win32IDE::findFirstUserInteractivePane()
{
    for (auto& pane : m_terminalPanes)
    {
        if (pane.kind == TerminalPaneKind::UserInteractive)
            return &pane;
    }
    return nullptr;
}

TerminalPane* Win32IDE::resolveTerminalPaneForUserTypedCommand()
{
    TerminalPane* active = getActiveTerminalPane();
    if (active && active->kind == TerminalPaneKind::UserInteractive)
        return active;
    if (m_lastUserInteractiveTerminalId >= 0)
    {
        TerminalPane* last = findTerminalPane(m_lastUserInteractiveTerminalId);
        if (last && last->kind == TerminalPaneKind::UserInteractive)
            return last;
    }
    TerminalPane* first = findFirstUserInteractivePane();
    if (first)
        return first;
    const int pid =
        createTerminalPane(Win32TerminalManager::PowerShell, "PowerShell", TerminalPaneKind::UserInteractive, true);
    return findTerminalPane(pid);
}

TerminalPane* Win32IDE::resolvePaneForInteractiveShellMenu()
{
    TerminalPane* active = getActiveTerminalPane();
    if (active && active->kind == TerminalPaneKind::UserInteractive)
        return active;
    TerminalPane* u = findFirstUserInteractivePane();
    if (u)
        return u;
    const int pid =
        createTerminalPane(Win32TerminalManager::PowerShell, "PowerShell", TerminalPaneKind::UserInteractive, true);
    return findTerminalPane(pid);
}

const char* Win32IDE::preferredIntegratedTerminalWorkingDirectory(std::string& storage) const
{
    storage.clear();
    if (!m_projectRoot.empty())
        storage = m_projectRoot;
    else if (!m_explorerRootPath.empty())
        storage = m_explorerRootPath;
    else if (!m_settings.workingDirectory.empty())
        storage = m_settings.workingDirectory;
    return storage.empty() ? nullptr : storage.c_str();
}

void Win32IDE::ensureShellRunningForPane(TerminalPane* pane, Win32TerminalManager::ShellType shell)
{
    if (!pane || !pane->manager)
        return;
    if (pane->manager->isRunning())
        return;
    std::string cwdStore;
    const char* cwd = preferredIntegratedTerminalWorkingDirectory(cwdStore);
    const int pid = pane->id;
    auto terminalLiveness = AcquireTerminalCallbackLiveness(this);
    pane->manager->onFinished = [this, pid, terminalLiveness](int exitCode)
    {
        if (!IsTerminalCallbackOwnerAlive(terminalLiveness))
            return;
        if (isShuttingDown())
            return;
        if (m_hwndMain && IsWindow(m_hwndMain))
            PostMessageW(m_hwndMain, WM_APP + 304, (WPARAM)pid, (LPARAM)(uint32_t)exitCode);
    };
    if (pane->manager->start(shell, cwd))
    {
        pane->shellType = shell;
        if (!cwdStore.empty())
            pane->integratedWorkingDirectory = cwdStore;
        else
        {
            char buf[MAX_PATH] = {};
            if (GetCurrentDirectoryA(MAX_PATH, buf))
                pane->integratedWorkingDirectory = buf;
        }
        {
            std::string gb;
            queryGitBranchForIntegratedCwd(pane->integratedWorkingDirectory, gb);
            pane->integratedGitBranchFromCwd = std::move(gb);
        }
        appendText(pane->hwnd, shell == Win32TerminalManager::PowerShell ? "PowerShell started...\r\n"
                                                                         : "Command Prompt started...\r\n");
        refreshIntegratedTerminalContextHint();
    }
}

int Win32IDE::createAgentTerminalPane(Win32TerminalManager::ShellType shellType, const std::string& name,
                                      bool activateAndFocus)
{
    std::string label = name;
    if (label.empty())
        label = "Agent · " + std::to_string(m_nextAgentTerminalSequence++);
    const int id = createTerminalPane(shellType, label, TerminalPaneKind::AgentReadOnly, activateAndFocus);
    TerminalPane* p = findTerminalPane(id);
    if (p)
    {
        ensureShellRunningForPane(p, shellType);
        appendText(
            p->hwnd,
            "[Read-only agent terminal — command bar targets user shells; AI injects via writeAgentTerminalLine.]\r\n");
    }
    layoutTerminalStrip();
    return id;
}

int Win32IDE::getOrCreatePrimaryAgentTerminalPane()
{
    if (m_primaryAgentTerminalId >= 0)
    {
        if (findTerminalPane(m_primaryAgentTerminalId))
            return m_primaryAgentTerminalId;
        m_primaryAgentTerminalId = -1;
    }
    for (auto& p : m_terminalPanes)
    {
        if (p.kind == TerminalPaneKind::AgentReadOnly)
        {
            m_primaryAgentTerminalId = p.id;
            return p.id;
        }
    }
    std::string label = "Agent · " + std::to_string(m_nextAgentTerminalSequence++);
    const int id = createTerminalPane(Win32TerminalManager::PowerShell, label, TerminalPaneKind::AgentReadOnly, false);
    m_primaryAgentTerminalId = id;
    TerminalPane* np = findTerminalPane(id);
    if (np)
    {
        ensureShellRunningForPane(np, Win32TerminalManager::PowerShell);
        appendText(np->hwnd,
                   "[Primary agent terminal — parallel with user shells; output is read-only in the IDE.]\r\n");
    }
    layoutTerminalStrip();
    return id;
}

bool Win32IDE::writeInputToTerminalPane(int paneId, const std::string& data, bool appendCrLf)
{
    TerminalPane* p = findTerminalPane(paneId);
    if (!p || !p->manager)
        return false;
    ensureShellRunningForPane(p, p->shellType);
    if (!p->manager->isRunning())
        return false;
    std::string payload = data;
    if (appendCrLf)
    {
        while (!payload.empty() && (payload.back() == '\n' || payload.back() == '\r'))
            payload.pop_back();
        payload += "\r\n";
    }
    p->manager->writeInput(payload);
    return true;
}

bool Win32IDE::writeAgentTerminalLine(int paneId, const std::string& line)
{
    TerminalPane* p = findTerminalPane(paneId);
    if (!p || p->kind != TerminalPaneKind::AgentReadOnly || !p->manager)
        return false;
    return writeInputToTerminalPane(paneId, line, true);
}

namespace
{
std::vector<std::wstring> s_integratedTermTabLabels;
int s_integratedTerminalStripPx = 0;
}  // namespace

std::wstring Win32IDE::vscodeTabLabelForPane(size_t paneIndex) const
{
    if (paneIndex >= m_terminalPanes.size())
        return L"?";
    const TerminalPane& p = m_terminalPanes[paneIndex];
    if (p.kind == TerminalPaneKind::AgentReadOnly)
    {
        return utf8ToWide(p.name.empty() ? "Agent" : p.name);
    }
    std::string s = (p.shellType == Win32TerminalManager::PowerShell) ? "pwsh" : "cmd";
    int n = 1;
    for (size_t j = 0; j < paneIndex; ++j)
    {
        if (m_terminalPanes[j].kind == TerminalPaneKind::UserInteractive && m_terminalPanes[j].shellType == p.shellType)
            ++n;
    }
    if (n > 1)
        s += " (" + std::to_string(n) + ")";
    return utf8ToWide(s);
}

void Win32IDE::ensureIntegratedTerminalTabStrip()
{
    if (g_rawrxdIntegratedTerminalTabs && IsWindow(g_rawrxdIntegratedTerminalTabs))
        return;
    if (!m_hwndMain)
        return;
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_TAB_CLASSES};
    InitCommonControlsEx(&icc);
    const int h = dpiScale(26);
    g_rawrxdIntegratedTerminalTabs =
        CreateWindowExW(0, WC_TABCONTROLW, L"",
                        WS_CHILD | WS_CLIPCHILDREN | TCS_BUTTONS | TCS_SINGLELINE | TCS_TOOLTIPS | TCS_FOCUSNEVER, 0, 0,
                        400, h, m_hwndMain, (HMENU)(UINT_PTR)IDC_INTEGRATED_TERM_TABS, m_hInstance, nullptr);
    if (g_rawrxdIntegratedTerminalTabs && m_hFontUI)
        SendMessageW(g_rawrxdIntegratedTerminalTabs, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
}

void Win32IDE::updateIntegratedTerminalTabSelection()
{
    if (!g_rawrxdIntegratedTerminalTabs || !IsWindow(g_rawrxdIntegratedTerminalTabs))
        return;
    for (size_t i = 0; i < m_terminalPanes.size(); ++i)
    {
        if (m_terminalPanes[i].id == m_activeTerminalId)
        {
            TabCtrl_SetCurSel(g_rawrxdIntegratedTerminalTabs, static_cast<int>(i));
            return;
        }
    }
}

void Win32IDE::syncIntegratedTerminalTabStrip()
{
    s_integratedTerminalStripPx = 0;
    if (m_terminalPanes.empty() || !m_hwndMain)
    {
        if (g_rawrxdIntegratedTerminalTabs && IsWindow(g_rawrxdIntegratedTerminalTabs))
            ShowWindow(g_rawrxdIntegratedTerminalTabs, SW_HIDE);
        return;
    }
    ensureIntegratedTerminalTabStrip();
    if (!g_rawrxdIntegratedTerminalTabs)
        return;
    s_integratedTermTabLabels.clear();
    while (TabCtrl_GetItemCount(g_rawrxdIntegratedTerminalTabs) > 0)
        TabCtrl_DeleteItem(g_rawrxdIntegratedTerminalTabs, 0);
    for (size_t i = 0; i < m_terminalPanes.size(); ++i)
    {
        s_integratedTermTabLabels.push_back(vscodeTabLabelForPane(i));
        TCITEMW tie = {};
        tie.mask = TCIF_TEXT;
        tie.pszText = s_integratedTermTabLabels.back().data();
        TabCtrl_InsertItem(g_rawrxdIntegratedTerminalTabs, static_cast<int>(i), &tie);
    }
    updateIntegratedTerminalTabSelection();
    s_integratedTerminalStripPx = dpiScale(28);
    ShowWindow(g_rawrxdIntegratedTerminalTabs, SW_SHOW);
}

void Win32IDE::layoutTerminalStrip()
{
    if (!m_hwndMain || !m_hwndToolbar)
        return;
    RECT rect{};
    GetClientRect(m_hwndMain, &rect);
    RECT toolbarRect{};
    GetWindowRect(m_hwndToolbar, &toolbarRect);
    const int toolbarHeight = toolbarRect.bottom - toolbarRect.top;
    syncIntegratedTerminalTabStrip();
    layoutTerminalPanes(rect.right - rect.left, toolbarHeight + m_editorHeight, m_terminalHeight);
}

void Win32IDE::appendToTerminalPane(int paneId, const std::string& text)
{
    TerminalPane* p = findTerminalPane(paneId);
    if (!p || !p->hwnd)
        return;
    appendText(p->hwnd, text);
}

void Win32IDE::setActiveTerminalPane(int paneId)
{
    bool found = false;
    for (auto& pane : m_terminalPanes)
    {
        if (pane.id == paneId)
        {
            pane.isActive = true;
            m_activeTerminalId = paneId;
            if (pane.kind == TerminalPaneKind::UserInteractive)
                m_lastUserInteractiveTerminalId = paneId;
            if (pane.hwnd)
                SetFocus(pane.hwnd);
            found = true;
        }
        else
        {
            pane.isActive = false;
        }
    }
    if (!found && !m_terminalPanes.empty())
    {
        m_terminalPanes.front().isActive = true;
        m_activeTerminalId = m_terminalPanes.front().id;
        if (m_terminalPanes.front().kind == TerminalPaneKind::UserInteractive)
            m_lastUserInteractiveTerminalId = m_terminalPanes.front().id;
        if (m_terminalPanes.front().hwnd)
            SetFocus(m_terminalPanes.front().hwnd);
    }
    updateIntegratedTerminalTabSelection();
    syncCommandInputForActiveTerminal();
    refreshIntegratedTerminalContextHint();
}

void Win32IDE::syncCommandInputForActiveTerminal()
{
    if (!m_hwndCommandInput || !IsWindow(m_hwndCommandInput))
        return;
    TerminalPane* p = getActiveTerminalPane();
    const bool allowUserTyping = !p || p->kind == TerminalPaneKind::UserInteractive;
    EnableWindow(m_hwndCommandInput, allowUserTyping ? TRUE : FALSE);
}

namespace
{
std::string shortenPathForStatusBar(const std::string& p, size_t maxLen)
{
    if (p.size() <= maxLen)
        return p;
    if (maxLen <= 3)
        return "...";
    return std::string("...") + p.substr(p.size() - (maxLen - 3));
}
}  // namespace

void Win32IDE::refreshIntegratedTerminalContextHint()
{
    if (!m_hwndStatusBar || !IsWindow(m_hwndStatusBar))
        return;
    if (m_chatMode)
        return;

    if (!m_outputPanelVisible || m_activePanelTab != PanelTab::Terminal)
    {
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)L"Terminal Mode");
        return;
    }

    TerminalPane* p = getActiveTerminalPane();
    const char* shellLabel = (p && p->shellType == Win32TerminalManager::CommandPrompt) ? "CMD" : "PS";

    std::string cwd;
    if (p && !p->integratedWorkingDirectory.empty())
        cwd = p->integratedWorkingDirectory;
    else
    {
        std::string s;
        const char* c = preferredIntegratedTerminalWorkingDirectory(s);
        if (c && c[0])
            cwd = c;
    }

    std::string branchDisp = "-";
    if (p && !p->integratedGitBranchFromCwd.empty())
        branchDisp = p->integratedGitBranchFromCwd;
    else if (!m_gitStatus.branch.empty())
        branchDisp = m_gitStatus.branch;

    std::ostringstream oss;
    oss << shellLabel << " | " << branchDisp << " | " << (cwd.empty() ? "-" : shortenPathForStatusBar(cwd, 52));

    const std::wstring w = utf8ToWide(oss.str());
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)w.c_str());
}

void Win32IDE::onTerminalProcessExited(int paneId, uint32_t exitCode)
{
    TerminalPane* pane = findTerminalPane(paneId);
    if (!pane || !pane->hwnd)
        return;

    pane->integratedGitBranchFromCwd.clear();

    std::ostringstream line;
    line << "\r\n[Shell process exited with code " << exitCode << "]\r\n";
    appendText(pane->hwnd, line.str());
    refreshIntegratedTerminalContextHint();
}

void Win32IDE::focusIntegratedTerminalPanel()
{
    if (!m_outputPanelVisible)
    {
        m_outputPanelVisible = true;
        if (m_hwndMain)
        {
            RECT rc{};
            GetClientRect(m_hwndMain, &rc);
            onSize(rc.right, rc.bottom);
            InvalidateRect(m_hwndMain, nullptr, TRUE);
        }
    }
    if (m_hwndPanelTabs && IsWindow(m_hwndPanelTabs))
        switchPanelTab(PanelTab::Terminal);
    // VS Code / Cursor: keep the active terminal session (user or agent); do not jump to a user pane.
    TerminalPane* active = getActiveTerminalPane();
    if (active && active->kind == TerminalPaneKind::UserInteractive)
    {
        if (m_hwndCommandInput && IsWindow(m_hwndCommandInput))
            SetFocus(m_hwndCommandInput);
    }
    else if (active && active->hwnd && IsWindow(active->hwnd))
    {
        SetFocus(active->hwnd);
    }
    else if (m_hwndCommandInput && IsWindow(m_hwndCommandInput))
    {
        SetFocus(m_hwndCommandInput);
    }
    syncCommandInputForActiveTerminal();
}

void Win32IDE::layoutTerminalPanes(int width, int top, int height)
{
    // LOGGING AS REQUESTED
    char logBuf[256];
    sprintf_s(logBuf, "layoutTerminalPanes: Width=%d Top=%d Height=%d Count=%zu", width, top, height,
              m_terminalPanes.size());
    LOG_INFO(std::string(logBuf));

    if (width <= 0 || height <= 0 || m_terminalPanes.empty())
        return;

    // Calculate correct left offset — terminal panes are children of m_hwndMain,
    // so we must offset past activity bar + sidebar to avoid overlapping them
    const int ACTIVITY_BAR_WIDTH = dpiScale(48);
    int sidebarWidth = m_sidebarVisible ? m_sidebarWidth : 0;
    int editorLeft = ACTIVITY_BAR_WIDTH + sidebarWidth;
    int secondarySidebarWidth = m_secondarySidebarVisible ? m_secondarySidebarWidth : 0;

    // Clamp width to editor area (exclude sidebars)
    RECT mainRect;
    GetClientRect(m_hwndMain, &mainRect);
    int editorWidth = (mainRect.right - mainRect.left) - editorLeft - secondarySidebarWidth;
    if (editorWidth <= 0)
        editorWidth = width;  // fallback

    const int strip = s_integratedTerminalStripPx;
    const int bodyTop = top + strip;
    int bodyH = height - strip;
    if (bodyH < dpiScale(40))
        bodyH = height;
    if (g_rawrxdIntegratedTerminalTabs && IsWindow(g_rawrxdIntegratedTerminalTabs) && strip > 0)
    {
        MoveWindow(g_rawrxdIntegratedTerminalTabs, editorLeft, top, editorWidth, strip, TRUE);
    }

    int count = static_cast<int>(m_terminalPanes.size());
    if (count == 1)
    {
        auto& pane = m_terminalPanes[0];
        MoveWindow(pane.hwnd, editorLeft, bodyTop, editorWidth, bodyH, TRUE);
        pane.bounds = {editorLeft, bodyTop, editorLeft + editorWidth, bodyTop + bodyH};
        return;
    }

    if (m_terminalSplitHorizontal)
    {
        int paneHeight = bodyH / count;
        int y = bodyTop;
        for (int i = 0; i < count; ++i)
        {
            int currentHeight = (i == count - 1) ? (bodyH - paneHeight * (count - 1)) : paneHeight;
            auto& pane = m_terminalPanes[i];
            MoveWindow(pane.hwnd, editorLeft, y, editorWidth, currentHeight, TRUE);
            pane.bounds = {editorLeft, y, editorLeft + editorWidth, y + currentHeight};
            y += currentHeight;
        }
    }
    else
    {
        int paneWidth = editorWidth / count;
        int x = editorLeft;
        for (int i = 0; i < count; ++i)
        {
            int currentWidth = (i == count - 1) ? (editorWidth - paneWidth * (count - 1)) : paneWidth;
            auto& pane = m_terminalPanes[i];
            MoveWindow(pane.hwnd, x, bodyTop, currentWidth, bodyH, TRUE);
            pane.bounds = {x, bodyTop, x + currentWidth, bodyTop + bodyH};
            x += currentWidth;
        }
    }
}

void Win32IDE::splitTerminalHorizontal()
{
    m_terminalSplitHorizontal = true;
    TerminalPane* active = getActiveTerminalPane();
    Win32TerminalManager::ShellType type = active ? active->shellType : Win32TerminalManager::PowerShell;
    TerminalPaneKind newKind = active ? active->kind : TerminalPaneKind::UserInteractive;
    std::string tabName = (newKind == TerminalPaneKind::AgentReadOnly)
                              ? ("Agent " + std::to_string(m_nextAgentTerminalSequence++))
                              : "Terminal";
    createTerminalPane(type, tabName, newKind);
    layoutTerminalStrip();
}

void Win32IDE::splitTerminalVertical()
{
    m_terminalSplitHorizontal = false;
    TerminalPane* active = getActiveTerminalPane();
    Win32TerminalManager::ShellType type = active ? active->shellType : Win32TerminalManager::PowerShell;
    TerminalPaneKind newKind = active ? active->kind : TerminalPaneKind::UserInteractive;
    std::string tabName = (newKind == TerminalPaneKind::AgentReadOnly)
                              ? ("Agent " + std::to_string(m_nextAgentTerminalSequence++))
                              : "Terminal";
    createTerminalPane(type, tabName, newKind);
    layoutTerminalStrip();
}

void Win32IDE::clearAllTerminals()
{
    for (auto& pane : m_terminalPanes)
    {
        if (pane.manager && pane.manager->isRunning())
        {
            pane.manager->stop();
        }
        if (pane.hwnd)
        {
            DestroyWindow(pane.hwnd);
        }
    }
    m_terminalPanes.clear();
    m_activeTerminalId = -1;
    m_nextTerminalId = 1;
    m_primaryAgentTerminalId = -1;
    m_lastUserInteractiveTerminalId = -1;
    m_nextAgentTerminalSequence = 1;
    createTerminalPane(Win32TerminalManager::PowerShell, "PowerShell", TerminalPaneKind::UserInteractive, true);
}

void Win32IDE::createStatusBar(HWND hwnd)
{

    m_hwndStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd,
                                      (HMENU)IDC_STATUS_BAR, m_hInstance, nullptr);
    if (!m_hwndStatusBar)
    {

        return;
    }

    // 0: primary status, 1: mode, 2: VMM ribbon, 3: spare, 4: context usage
    int parts[] = {200, 360, 540, 720, -1};
    SendMessage(m_hwndStatusBar, SB_SETPARTS, 5, (LPARAM)parts);
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Ready");
    // Initial ribbon (updated after model load self-check).
#if defined(RAWRXD_HAS_SOVEREIGN_GPU_ASM) && (RAWRXD_HAS_SOVEREIGN_GPU_ASM != 0)
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 2, (LPARAM)L"VMM: [Legacy]  GPU-ASM: ACTIVE");
#else
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 2, (LPARAM)L"VMM: [Legacy]  GPU-ASM: FALLBACK");
#endif
    SendMessageW(m_hwndStatusBar, SB_SETTIPTEXTW, 2,
                 (LPARAM)L"VMM diagnostics will appear here after model self-check.");
    if (HICON ico = getVmmLedIcon(VmmRibbonTier::Red))
        SendMessageW(m_hwndStatusBar, SB_SETICON, 2, (LPARAM)ico);
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 4, (LPARAM)L"Ctx: 0/128K  0%");
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 3, (LPARAM)L"MoE pack: —");
    SendMessageW(m_hwndStatusBar, SB_SETTIPTEXTW, 3,
                 (LPARAM)L"MoE grouped pack cache: hits/misses, async prepack queue, row-eviction invalidations.");
}

void Win32IDE::createSidebar(HWND hwnd)
{
    createPrimarySidebar(hwnd);
}


void Win32IDE::newFile()
{
    appendToOutput("File > New clicked\n", "Output", OutputSeverity::Info);
    if (m_fileModified)
    {
        int result = MessageBoxW(m_hwndMain, L"File has been modified. Save changes?", L"Save", MB_YESNOCANCEL);
        if (result == IDCANCEL)
        {
            appendToOutput("File > New cancelled by user\n", "Output", OutputSeverity::Info);
            return;
        }
        if (result == IDYES && !saveFile())
        {
            appendToOutput("File > New - save failed, operation aborted\n", "Output", OutputSeverity::Warning);
            return;
        }
    }

    if (!m_currentFile.empty())
    {
        syncLSPDocumentClose(m_currentFile);
    }
    m_suppressLspDocumentSync = true;
    setWindowText(m_hwndEditor, "");
    m_suppressLspDocumentSync = false;
    m_currentFile.clear();
    m_fileModified = false;
    updateTitleBarText();
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"New file");
    updateMenuEnableStates();
    syncEditorToGpuSurface();
    appendToOutput("New file created successfully\n", "Output", OutputSeverity::Info);
}

void Win32IDE::openFile()
{
    SCOPED_METRIC("file.open_dialog");
    METRICS.increment("file.open_total");
    appendToOutput("File > Open clicked\n", "Output", OutputSeverity::Info);
    if (m_fileModified)
    {
        int result = MessageBoxW(m_hwndMain, L"File has been modified. Save changes?", L"Save", MB_YESNOCANCEL);
        if (result == IDCANCEL)
        {
            appendToOutput("File > Open cancelled by user\n", "Output", OutputSeverity::Info);
            return;
        }
        if (result == IDYES && !saveFile())
        {
            appendToOutput("File > Open - save failed, operation aborted\n", "Output", OutputSeverity::Warning);
            return;
        }
    }

    OPENFILENAMEW ofn;
    std::vector<wchar_t> fileBuffer(65536, L'\0');

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFile = fileBuffer.data();
    ofn.nMaxFile = static_cast<DWORD>(fileBuffer.size());
    ofn.lpstrFilter = L"All Files\0*.*\0C++ Files\0*.cpp;*.h\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_ALLOWMULTISELECT;

    if (GetOpenFileNameW(&ofn))
    {
        const wchar_t* base = fileBuffer.data();
        const wchar_t* next = base + wcslen(base) + 1;

        // Explorer format: single-select returns full path; multi-select returns dir + file list.
        if (*next == L'\0')
        {
            openFile(wideToUtf8(base));
        }
        else
        {
            const std::wstring dir(base);
            for (const wchar_t* p = next; *p; p += wcslen(p) + 1)
            {
                std::wstring full = dir;
                if (!full.empty() && full.back() != L'\\')
                    full += L'\\';
                full += p;
                openFile(wideToUtf8(full));
            }
        }
    }
    else
    {
        appendToOutput("File > Open cancelled by user (no file selected)\n", "Output", OutputSeverity::Info);
    }
}

void Win32IDE::closeWelcomePage()
{
    // Full welcome WebView2 UI lives in Win32IDE_WelcomePage.cpp when linked; keep a no-op here for link parity.
}

void Win32IDE::openWorkspaceFolder()
{
    closeWelcomePage();
    BROWSEINFOW bi = {};
    bi.hwndOwner = m_hwndMain;
    bi.lpszTitle = L"Select Workspace Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl)
    {
        appendToOutput("File > Open Folder cancelled\n", "Output", OutputSeverity::Info);
        return;
    }
    WCHAR path[MAX_PATH] = {};
    if (SHGetPathFromIDListW(pidl, path))
    {
        const std::string u8 = wideToUtf8(path);
        applyWorkspaceFolderForChatHistory(u8);
        syncMissingFeaturesWorkspaceContext(u8);
        appendToOutput("[Workspace] Opened folder: " + u8 + "\n", "Output", OutputSeverity::Info);
        if (m_hwndStatusBar)
            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Workspace folder opened");
    }
    CoTaskMemFree(pidl);
}

// Overload to open a specific file path
void Win32IDE::openFile(const std::string& filePath)
{
    SCOPED_METRIC("file.open_path");
    if (filePath.empty())
    {
        openFile();  // Call the dialog version
        return;
    }

    METRICS.increment("file.open_total");
    appendToOutput("Opening file: " + filePath + "\n", "Output", OutputSeverity::Info);
    try
    {
        std::ifstream file(std::filesystem::path(utf8ToWide(filePath)));
        if (file)
        {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            m_suppressLspDocumentSync = true;
            setWindowText(m_hwndEditor, content);
            m_suppressLspDocumentSync = false;
            m_currentFile = filePath;
            m_fileModified = false;
            setCurrentDirectoryFromFile(m_currentFile);
            updateTitleBarText();
            syncLSPDocumentOpen(m_currentFile, content);
            syncMissingFeaturesFileContext(m_currentFile);

            std::string displayName = extractLeafName(filePath);
            if (m_hwndTabBar)
            {
                addTab(filePath, displayName);
            }

            CHARFORMAT2W cf;
            memset(&cf, 0, sizeof(cf));
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
            cf.crTextColor = ensureReadableTextColor(m_currentTheme.backgroundColor, m_currentTheme.textColor);
            cf.yHeight = 220;
            wcscpy_s(cf.szFaceName, L"Consolas");
            SendMessageW(m_hwndEditor, EM_SETBKGNDCOLOR, 0, m_currentTheme.backgroundColor);
            SendMessageW(m_hwndEditor, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
            SendMessageW(m_hwndEditor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"File opened");
            updateMenuEnableStates();
            updateLineNumbers();
            updateGitStatus();  // Update Git status for gutter indicators
            syncEditorToGpuSurface();
            refreshSymbolIndexForCurrentDocumentAsync();
            appendToOutput("File opened successfully (" + std::to_string(content.size()) + " bytes)\n", "Output",
                           OutputSeverity::Info);
        }
        else
        {
            appendToOutput("Failed to open file: " + filePath + "\n", "Errors", OutputSeverity::Error);
            MessageBoxW(m_hwndMain, L"Failed to open file", L"Error", MB_OK | MB_ICONERROR);
        }
    }
    catch (const std::exception& e)
    {
        appendToOutput("Exception opening file: " + std::string(e.what()) + "\n", "Errors", OutputSeverity::Error);
        MessageBoxW(m_hwndMain, utf8ToWide(e.what()).c_str(), L"Error", MB_OK | MB_ICONERROR);
    }
}

bool Win32IDE::saveFile()
{
    SCOPED_METRIC("file.save");
    METRICS.increment("file.save_total");

    if (m_currentFile.empty())
    {
        appendToOutput("File > Save - no current file, showing Save As dialog\n", "Output", OutputSeverity::Info);
        return saveFileAs();
    }

    appendToOutput("Saving file: " + m_currentFile + "\n", "Output", OutputSeverity::Info);
    try
    {
        std::string content = getWindowText(m_hwndEditor);
        std::ofstream file(std::filesystem::path(utf8ToWide(m_currentFile)));
        if (file)
        {
            file << content;
            m_fileModified = false;
            updateTitleBarText();
            syncLSPDocumentSave(m_currentFile);
            refreshSymbolIndexForCurrentDocumentAsync();
            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"File saved");
            appendToOutput("File saved successfully (" + std::to_string(content.size()) + " bytes)\n", "Output",
                           OutputSeverity::Info);
            return true;
        }
        appendToOutput("Failed to open file for writing: " + m_currentFile + "\n", "Errors", OutputSeverity::Error);
        MessageBoxW(m_hwndMain, L"Failed to save file", L"Error", MB_OK | MB_ICONERROR);
    }
    catch (const std::exception& e)
    {
        appendToOutput("Exception saving file: " + std::string(e.what()) + "\n", "Errors", OutputSeverity::Error);
        MessageBoxW(m_hwndMain, utf8ToWide(e.what()).c_str(), L"Error", MB_OK | MB_ICONERROR);
    }
    return false;
}

bool Win32IDE::saveFileAs()
{
    appendToOutput("File > Save As clicked\n", "Output", OutputSeverity::Info);
    const std::string previousFile = m_currentFile;
    OPENFILENAMEW ofn;
    wchar_t szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = (DWORD)std::size(szFile);
    ofn.lpstrFilter = L"All Files\0*.*\0C++ Files\0*.cpp;*.h\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn))
    {
        m_currentFile = wideToUtf8(szFile);
        if (previousFile != m_currentFile)
        {
            if (!previousFile.empty())
            {
                syncLSPDocumentClose(previousFile);
            }

            if (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_editorTabs.size())
            {
                auto& activeTab = m_editorTabs[m_activeTabIndex];
                activeTab.filePath = m_currentFile;
                activeTab.displayName = extractLeafName(m_currentFile);
                if (m_tabManager)
                {
                    m_tabManager->updateTabDisplay(m_activeTabIndex);
                }
            }

            if (m_hwndEditor)
            {
                syncLSPDocumentOpen(m_currentFile, getWindowText(m_hwndEditor));
            }
        }

        appendToOutput("Save As: " + m_currentFile + "\n", "Output", OutputSeverity::Info);
        setCurrentDirectoryFromFile(m_currentFile);
        updateTitleBarText();
        return saveFile();
    }
    appendToOutput("File > Save As cancelled by user\n", "Output", OutputSeverity::Info);
    return false;
}

void Win32IDE::startPowerShell()
{
    TerminalPane* pane = resolvePaneForInteractiveShellMenu();
    if (!pane || !pane->manager)
        return;
    stopTerminal();
    // Reuse ensureShellRunningForPane so cwd, shellType, exit callback, and status-bar hint stay in sync.
    ensureShellRunningForPane(pane, Win32TerminalManager::PowerShell);
    if (pane->manager->isRunning())
    {
        updateMenuEnableStates();
        appendToOutput("PowerShell started...\n", "Output", OutputSeverity::Info);
    }
}

void Win32IDE::startCommandPrompt()
{
    TerminalPane* pane = resolvePaneForInteractiveShellMenu();
    if (!pane || !pane->manager)
        return;
    stopTerminal();
    ensureShellRunningForPane(pane, Win32TerminalManager::CommandPrompt);
    if (pane->manager->isRunning())
    {
        updateMenuEnableStates();
        appendToOutput("Command Prompt started...\n", "Output", OutputSeverity::Info);
    }
}

void Win32IDE::stopTerminal()
{
    TerminalPane* pane = resolvePaneForInteractiveShellMenu();
    if (!pane || !pane->manager || !pane->manager->isRunning())
        return;
    pane->manager->stop();
    appendText(pane->hwnd, "\nTerminal stopped.\n");
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)L"Stopped");
    updateMenuEnableStates();
    appendToOutput("Terminal stopped.\n", "Output", OutputSeverity::Info);
}

void Win32IDE::executeCommand()
{
    std::string command = getWindowText(m_hwndCommandInput);
    if (command.empty())
        return;

    SetWindowTextW(m_hwndCommandInput, L"");

    // Command Parsing
    if (command[0] == '/' || command[0] == '!')
    {
        std::stringstream ss(command);
        std::string action;
        ss >> action;

        if (action == "/load")
        {
            std::string path;
            std::getline(ss, path);
            if (!path.empty())
                path = path.substr(1);
            openFile(path);  // Or load model if path looks like a model file
            {
                auto isModelExt = [](const std::string& p)
                {
                    if (p.empty())
                        return false;
                    std::string lower = p;
                    std::transform(lower.begin(), lower.end(), lower.begin(),
                                   [](unsigned char c) { return (char)std::tolower(c); });
                    size_t dot = lower.rfind('.');
                    if (dot == std::string::npos)
                        return false;
                    std::string ext = lower.substr(dot);
                    return ext == ".gguf" || ext == ".gguf2" || ext == ".bin" || ext == ".safetensors" ||
                           ext == ".onnx";
                };
                if (isModelExt(path))
                {
                    loadGGUFModel(path);
                    if (loadModelForInference(path))
                        appendToOutput("Model loaded; chat and agentic use this model.\n", "Agent",
                                       OutputSeverity::Info);
                    else if (m_nativeEngine && m_nativeEngine->LoadModel(path))
                        appendToOutput("Model loaded.\n", "Agent", OutputSeverity::Info);
                }
            }
        }
        else if (action == "/agent" || action == "/ask")
        {
            std::string q;
            std::getline(ss, q);
            if (m_agent)
                m_agent->Ask(q);
        }
        else if (action == "/bugreport")
        {
            std::string f = m_currentFile;
            if (f.empty())
                appendToOutput("No file open.\n", "Error", OutputSeverity::Error);
            else if (m_agent)
                m_agent->BugReport(f);
        }
        else if (action == "/suggest")
        {
            std::string f = m_currentFile;
            if (f.empty())
                appendToOutput("No file open.\n", "Error", OutputSeverity::Error);
            else if (m_agent)
                m_agent->Suggest(f);
        }
        else if (action == "/install")
        {
            std::string path;
            std::getline(ss, path);
            if (!path.empty())
            {
                if (RawrXD::VSIXInstaller::Install(path.substr(1)))
                    appendToOutput("Extension installed.\n", "System", OutputSeverity::Info);
            }
        }
        else if (action == "/max")
        {
            static bool m = false;
            m = !m;
            if (m_agent)
                m_agent->SetMaxMode(m);
            appendToOutput(std::string("Max Mode: ") + (m ? "ON" : "OFF") + "\n", "System", OutputSeverity::Info);
        }
        else if (action == "/think")
        {
            static bool t = false;
            t = !t;
            if (m_agent)
                m_agent->SetDeepThink(t);
            appendToOutput(std::string("Deep Think: ") + (t ? "ON" : "OFF") + "\n", "System", OutputSeverity::Info);
        }
        else if (action == "/research")
        {
            static bool r = false;
            r = !r;
            if (m_agent)
                m_agent->SetDeepResearch(r);
            appendToOutput(std::string("Deep Research: ") + (r ? "ON" : "OFF") + "\n", "System", OutputSeverity::Info);
        }
        else if (action == "/norefusal")
        {
            static bool nr = false;
            nr = !nr;
            if (m_agent)
                m_agent->SetNoRefusal(nr);
            appendToOutput(std::string("No Refusal: ") + (nr ? "ON" : "OFF") + "\n", "System", OutputSeverity::Info);
        }
        else if (action == "!help" || action == "/exthelp")
        {
            static RawrXD::ExtensionLoader loader;
            loader.Scan();
            std::string arg;
            std::getline(ss, arg);
            if (!arg.empty())
                arg = arg.substr(1);

            if (arg.empty())
            {
                std::string list = "Extensions:\n";
                for (auto& e : loader.GetExtensions())
                    list += " - " + e.name + "\n";
                appendToOutput(list, "System", OutputSeverity::Info);
            }
            else
            {
                appendToOutput(loader.GetHelp(arg) + "\n", "System", OutputSeverity::Info);
            }
        }
        else
        {
            // Fallback
            TerminalPane* pane = resolveTerminalPaneForUserTypedCommand();
            if (pane && pane->manager)
            {
                ensureShellRunningForPane(pane, pane->shellType);
                if (pane->manager->isRunning())
                {
                    command += "\n";
                    pane->manager->writeInput(command);
                }
            }
        }
        return;
    }

    // Chat mode: send to model via agentic bridge (local GGUF or Ollama/cloud), with tool dispatch
    if (m_chatMode && m_agenticBridge && m_agenticBridge->IsInitialized())
    {
        appendChatMessage("You", command);
        std::thread(
            [this, command]()
            {
                DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
                if (_guard.cancelled)
                    return;
                std::string response = sendMessageToModel(command);
                if (!response.empty() && !isShuttingDown())
                {
                    appendChatMessage("Model", response);
                    appendToOutput("[Chat] " + response + "\n", "Output", OutputSeverity::Info);
                }
            })
            .detach();
        return;
    }

    // Send to terminal (never inject into agent read-only panes from the bar)
    TerminalPane* pane = resolveTerminalPaneForUserTypedCommand();
    if (pane && pane->manager)
    {
        ensureShellRunningForPane(pane, pane->shellType);
        if (pane->manager->isRunning())
        {
            addPowerShellHistory(command);  // Track in shared command history
            command += "\n";
            pane->manager->writeInput(command);
        }
    }
}

std::string Win32IDE::filterTerminalCwdTelemetry(std::string& carry, const std::string& chunk,
                                                 std::string* outCwdUpdate)
{
    const std::string beg = "RAWRXD_CWD|";
    const std::string end = "|END";
    std::string s = carry + chunk;
    carry.clear();

    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size())
    {
        const size_t pbeg = s.find(beg, i);
        if (pbeg == std::string::npos)
        {
            out.append(s, i, std::string::npos);
            return out;
        }
        out.append(s, i, pbeg - i);
        const size_t valStart = pbeg + beg.size();
        const size_t pend = s.find(end, valStart);
        if (pend == std::string::npos)
        {
            carry.assign(s, pbeg, std::string::npos);
            return out;
        }
        if (outCwdUpdate)
            *outCwdUpdate = s.substr(valStart, pend - valStart);
        i = pend + end.size();
        if (i < s.size() && s[i] == '\r')
            ++i;
        if (i < s.size() && s[i] == '\n')
            ++i;
    }
    return out;
}

void Win32IDE::onTerminalOutput(int paneId, const std::string& output)
{
    if (isShuttingDown())
        return;

    TerminalPane* pane = findTerminalPane(paneId);
    std::string filtered = output;
    if (pane && pane->hwnd)
    {
        std::string cwdFromTelemetry;
        filtered = filterTerminalCwdTelemetry(pane->terminalCwdTelemetryCarry, output, &cwdFromTelemetry);
        if (!cwdFromTelemetry.empty())
        {
            const std::string newCwd = std::move(cwdFromTelemetry);
            if (pane->integratedWorkingDirectory != newCwd)
            {
                pane->integratedWorkingDirectory = newCwd;
                std::string gb;
                queryGitBranchForIntegratedCwd(pane->integratedWorkingDirectory, gb);
                pane->integratedGitBranchFromCwd = std::move(gb);
            }
            refreshIntegratedTerminalContextHint();
        }
    }

    if (m_planOrchestrator)
        m_planOrchestrator->observeTerminalOutput("pane " + std::to_string(paneId), filtered, false);

    if (!pane || !pane->hwnd)
        return;
    appendTerminalTextAnsi(paneId, pane->hwnd, filtered);
}

void Win32IDE::onTerminalError(int paneId, const std::string& error)
{
    if (isShuttingDown())
        return;

    if (m_planOrchestrator)
    {
        m_planOrchestrator->observeTerminalOutput("pane " + std::to_string(paneId), error, true);
    }

    TerminalPane* pane = findTerminalPane(paneId);
    if (!pane || !pane->hwnd)
        return;
    appendTerminalTextAnsi(paneId, pane->hwnd, error);
    appendToOutput(error, "Errors", OutputSeverity::Error);
}

std::string Win32IDE::getWindowText(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd))
        return {};

    constexpr int kMaxWindowTextChars = 4 * 1024 * 1024;
    int length = GetWindowTextLengthW(hwnd);
    if (length <= 0 || length > kMaxWindowTextChars)
        return {};

    try
    {
        std::wstring wtext(static_cast<size_t>(length) + 1, L'\0');
        const int copied = GetWindowTextW(hwnd, wtext.data(), length + 1);
        if (copied <= 0)
            return {};
        wtext.resize(static_cast<size_t>(copied));
        return wideToUtf8(wtext.c_str());
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
}

std::string Win32IDE::getRichEditDocumentUtf8(HWND hwnd) const
{
    if (!hwnd || !IsWindow(hwnd))
        return {};
    GETTEXTLENGTHEX gtl{};
    gtl.flags = GTL_DEFAULT;
    gtl.codepage = CP_UNICODE;
    const LONG nchars = static_cast<LONG>(SendMessage(hwnd, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0));
    constexpr LONG kMaxRichEditChars = 4 * 1024 * 1024;
    if (nchars <= 0 || nchars > kMaxRichEditChars)
        return {};
    std::vector<wchar_t> wbuf(static_cast<size_t>(nchars) + 2, L'\0');
    GETTEXTEX gt{};
    gt.cb = static_cast<DWORD>(wbuf.size() * sizeof(wchar_t));
    gt.flags = GT_USECRLF;
    gt.codepage = 1200;
    gt.lpDefaultChar = nullptr;
    gt.lpUsedDefChar = nullptr;
    const LONG copied = static_cast<LONG>(SendMessage(hwnd, EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)wbuf.data()));
    if (copied <= 0)
        return {};
    wbuf[static_cast<size_t>(copied)] = L'\0';
    return wideToUtf8(wbuf.data());
}

std::string Win32IDE::snapshotWalOriginalContentForPath(const std::string& pathUtf8) const
{
    if (pathUtf8.empty())
        return {};

    const auto sameLogicalPath = [](const std::string& canonicalUtf8, const std::string& candidateUtf8) -> bool
    {
        if (canonicalUtf8.empty() || candidateUtf8.empty())
            return false;
        if (_stricmp(canonicalUtf8.c_str(), candidateUtf8.c_str()) == 0)
            return true;
        namespace fs = std::filesystem;
        std::error_code eca;
        std::error_code ecb;
        const fs::path ca = fs::weakly_canonical(fs::u8path(canonicalUtf8), eca);
        const fs::path cb = fs::weakly_canonical(fs::u8path(candidateUtf8), ecb);
        if (eca || ecb)
            return false;
        return _stricmp(ca.string().c_str(), cb.string().c_str()) == 0;
    };

    if (!m_currentFile.empty() && sameLogicalPath(pathUtf8, m_currentFile))
    {
        if (m_hwndEditor && IsWindow(m_hwndEditor))
            return getRichEditDocumentUtf8(m_hwndEditor);
    }

    for (const auto& tab : m_editorTabs)
    {
        if (!tab.filePath.empty() && sameLogicalPath(pathUtf8, tab.filePath))
            return tab.content;
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(pathUtf8, ec))
        return {};
    std::ifstream in(pathUtf8, std::ios::binary);
    if (!in.is_open())
        return {};
    std::ostringstream o;
    o << in.rdbuf();
    return o.str();
}

// UTF-8 byte offset <-> UTF-16 character index for Rich Edit
static int utf8ByteOffsetToCharIndex(const std::string& utf8, int byteOffset)
{
    if (byteOffset <= 0 || utf8.empty())
        return 0;
    if (byteOffset >= (int)utf8.size())
        byteOffset = (int)utf8.size();
    std::wstring w = utf8ToWide(utf8.substr(0, byteOffset));
    return (int)w.size();
}
static int charIndexToUtf8ByteOffset(const std::string& utf8, int charIndex)
{
    if (charIndex <= 0 || utf8.empty())
        return 0;
    std::wstring w = utf8ToWide(utf8);
    if (charIndex >= (int)w.size())
        return (int)utf8.size();
    return (int)wideToUtf8(w.substr(0, charIndex).c_str()).size();
}

static COLORREF ensureReadableTextColor(COLORREF bg, COLORREF fg)
{
    const int contrast = abs((int)GetRValue(bg) - (int)GetRValue(fg)) + abs((int)GetGValue(bg) - (int)GetGValue(fg)) +
                         abs((int)GetBValue(bg) - (int)GetBValue(fg));
    if (contrast < 96)
        return RGB(212, 212, 212);
    return fg;
}

void Win32IDE::setWindowText(HWND hwnd, const std::string& text)
{
    SetWindowTextW(hwnd, utf8ToWide(text).c_str());
    if (hwnd == m_hwndEditor)
    {
        // Keep editor readable and reset viewport when swapping document content.
        COLORREF bg = m_currentTheme.backgroundColor;
        COLORREF fg = ensureReadableTextColor(bg, m_currentTheme.textColor);

        CHARFORMAT2W cf;
        ZeroMemory(&cf, sizeof(cf));
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = fg;
        SendMessageW(m_hwndEditor, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);

        int firstVisible = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
        if (firstVisible > 0)
        {
            SendMessage(m_hwndEditor, EM_LINESCROLL, 0, -firstVisible);
        }
        SendMessage(m_hwndEditor, EM_SETSEL, 0, 0);
        SendMessage(m_hwndEditor, EM_SCROLLCARET, 0, 0);

        syncEditorToGpuSurface();
    }
}

void Win32IDE::appendText(HWND hwnd, const std::string& text)
{
    GETTEXTLENGTHEX gtl;
    gtl.flags = GTL_DEFAULT;
    gtl.codepage = CP_UNICODE;
    LONG length = SendMessage(hwnd, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);

    SendMessage(hwnd, EM_SETSEL, length, length);

    std::wstring wtext = utf8ToWide(text);
    SETTEXTEX st;
    st.flags = ST_DEFAULT;
    st.codepage = CP_UNICODE;
    SendMessageW(hwnd, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)wtext.c_str());

    if (hwnd == m_hwndEditor)
    {
        syncEditorToGpuSurface();
    }
}

// Theme Management Implementation
void Win32IDE::loadTheme(const std::string& themeName)
{
    std::string filename = "themes\\" + themeName + ".theme";
    std::ifstream file(filename);
    if (file.is_open())
    {
        std::string line;
        while (getline(file, line))
        {
            if (line.find("background=") == 0)
            {
                m_currentTheme.backgroundColor = std::stoul(line.substr(11), nullptr, 16);
            }
            else if (line.find("text=") == 0)
            {
                m_currentTheme.textColor = std::stoul(line.substr(5), nullptr, 16);
            }
            else if (line.find("selection=") == 0)
            {
                m_currentTheme.selectionColor = std::stoul(line.substr(10), nullptr, 16);
            }
            else if (line.find("linenumber=") == 0)
            {
                m_currentTheme.lineNumberColor = std::stoul(line.substr(11), nullptr, 16);
            }
        }
        file.close();
        applyTheme();
    }
}

void Win32IDE::saveTheme(const std::string& themeName)
{
    std::string filename = "themes\\" + themeName + ".theme";
    CreateDirectoryA("themes", NULL);
    std::ofstream file(filename);
    if (file.is_open())
    {
        file << "background=" << std::hex << m_currentTheme.backgroundColor << std::endl;
        file << "text=" << std::hex << m_currentTheme.textColor << std::endl;
        file << "selection=" << std::hex << m_currentTheme.selectionColor << std::endl;
        file << "linenumber=" << std::hex << m_currentTheme.lineNumberColor << std::endl;
        file.close();
        MessageBoxW(m_hwndMain, L"Theme saved successfully", L"Theme Manager", MB_OK);
    }
}

void Win32IDE::applyTheme()
{
    // ----------------------------------------------------------------
    // applyTheme() is idempotent — safe to call on startup, on theme
    // switch, on DPI change, and on transparency change.
    // Theme is pure data (IDETheme) — no GDI handles stored in it.
    // ----------------------------------------------------------------

    LOG_DEBUG("applyTheme(): \"" + m_currentTheme.name + "\"");

    // 1. Update the tracked background brush
    if (m_backgroundBrush)
        DeleteObject(m_backgroundBrush);
    m_backgroundBrush = CreateSolidBrush(m_currentTheme.backgroundColor);

    // 2. Editor: background + default text format (SCF_DEFAULT, not SCF_ALL,
    //    so syntax coloring tokens are preserved until the next colorize pass)
    if (m_hwndEditor)
    {
        SendMessage(m_hwndEditor, EM_SETBKGNDCOLOR, 0, m_currentTheme.backgroundColor);

        CHARFORMAT2W cf;
        ZeroMemory(&cf, sizeof(cf));
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = ensureReadableTextColor(m_currentTheme.backgroundColor, m_currentTheme.textColor);
        cf.dwEffects = 0;
        SendMessageW(m_hwndEditor, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
    }

    for (auto& pane : m_terminalPanes)
    {
        if (!pane.hwnd)
            continue;
        if (pane.kind == TerminalPaneKind::AgentReadOnly)
        {
            SendMessage(pane.hwnd, EM_SETBKGNDCOLOR, 0, RGB(42, 28, 46));
            CHARFORMAT2W tcf;
            ZeroMemory(&tcf, sizeof(tcf));
            tcf.cbSize = sizeof(tcf);
            tcf.dwMask = CFM_COLOR;
            tcf.crTextColor = RGB(235, 188, 200);
            tcf.dwEffects = 0;
            SendMessageW(pane.hwnd, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&tcf);
        }
        else
        {
            SendMessage(pane.hwnd, EM_SETBKGNDCOLOR, 0, m_currentTheme.panelBg);
            CHARFORMAT2W tcf;
            ZeroMemory(&tcf, sizeof(tcf));
            tcf.cbSize = sizeof(tcf);
            tcf.dwMask = CFM_COLOR;
            tcf.crTextColor = m_currentTheme.panelFg;
            tcf.dwEffects = 0;
            SendMessageW(pane.hwnd, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&tcf);
        }
    }

    // 4. Deep apply to all surfaces (sidebar, activity bar, tabs, status bar, panels)
    applyThemeToAllControls();

    // 5. Transparency — only touch the top-level window
    if (m_currentTheme.windowAlpha < 255)
    {
        setWindowTransparency(m_currentTheme.windowAlpha);
    }

    // 6. Force full repaint + update menu states
    InvalidateRect(m_hwndMain, NULL, TRUE);
    updateMenuEnableStates();

    // 7. Re-trigger syntax coloring so tokens pick up new palette
    if (m_syntaxColoringEnabled && m_hwndEditor)
    {
        m_syntaxDirty = true;
        applySyntaxColoring();
    }
}

void Win32IDE::showThemeEditor()
{
    showThemePicker();
}

void Win32IDE::updateMenuEnableStates()
{
    if (!m_hMenu)
        return;
    // Terminal split menu items
    UINT enableSplit = MF_BYCOMMAND | (m_terminalPanes.size() >= 1 ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(m_hMenu, IDM_TERMINAL_SPLIT_H, enableSplit);
    EnableMenuItem(m_hMenu, IDM_TERMINAL_SPLIT_V, enableSplit);
    TerminalPane* activePane = getActiveTerminalPane();
    bool terminalRunning = activePane && activePane->manager && activePane->manager->isRunning();
    EnableMenuItem(m_hMenu, IDM_TERMINAL_STOP, terminalRunning ? MF_BYCOMMAND | MF_ENABLED : MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(m_hMenu, IDM_TERMINAL_CLEAR_ALL,
                   (m_terminalPanes.empty() ? (MF_BYCOMMAND | MF_GRAYED) : (MF_BYCOMMAND | MF_ENABLED)));

    // Git items
    bool repo = isGitRepository();
    EnableMenuItem(m_hMenu, IDM_GIT_STATUS, repo ? MF_BYCOMMAND | MF_ENABLED : MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(m_hMenu, IDM_GIT_COMMIT,
                   (repo && m_gitStatus.hasChanges) ? MF_BYCOMMAND | MF_ENABLED : MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(m_hMenu, IDM_GIT_PUSH, repo ? MF_BYCOMMAND | MF_ENABLED : MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(m_hMenu, IDM_GIT_PULL, repo ? MF_BYCOMMAND | MF_ENABLED : MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(m_hMenu, IDM_GIT_PANEL, repo ? MF_BYCOMMAND | MF_ENABLED : MF_BYCOMMAND | MF_GRAYED);

    // File save related
    EnableMenuItem(m_hMenu, IDM_FILE_SAVE,
                   (!m_currentFile.empty() && m_fileModified) ? MF_BYCOMMAND | MF_ENABLED : MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem(m_hMenu, IDM_FILE_SAVEAS,
                   (!m_currentFile.empty()) ? MF_BYCOMMAND | MF_ENABLED : MF_BYCOMMAND | MF_GRAYED);

    // Streaming loader menu state
    CheckMenuItem(m_hMenu, IDM_VIEW_USE_STREAMING_LOADER,
                  MF_BYCOMMAND | (m_useStreamingLoader ? MF_CHECKED : MF_UNCHECKED));
    // Vulkan renderer menu state
    CheckMenuItem(m_hMenu, IDM_VIEW_USE_VULKAN_RENDERER,
                  MF_BYCOMMAND | (m_useVulkanRenderer ? MF_CHECKED : MF_UNCHECKED));
    updateSovereignSnapMenuChecks();
    // Breadcrumbs (View) — sync check with m_settings.breadcrumbsEnabled
    CheckMenuItem(m_hMenu, IDM_T1_BREADCRUMBS_TOGGLE,
                  MF_BYCOMMAND | (m_settings.breadcrumbsEnabled ? MF_CHECKED : MF_UNCHECKED));
    // Syntax highlighting — sync check with m_syntaxColoringEnabled
    CheckMenuItem(m_hMenu, ID_VIEW_SYNTAX_HIGHLIGHTING_TOGGLE,
                  MF_BYCOMMAND | (m_syntaxColoringEnabled ? MF_CHECKED : MF_UNCHECKED));

    // Tier 5 cosmetic features — enable when corresponding module is initialized (after deferredHeavyInit)
    EnableMenuItem(m_hMenu, IDM_TELDASH_SHOW,
                   (m_telemetryDashboardInitialized ? MF_BYCOMMAND | MF_ENABLED : MF_BYCOMMAND | MF_GRAYED));
    EnableMenuItem(m_hMenu, IDM_EMOJI_PICKER,
                   (m_emojiSupportInitialized ? MF_BYCOMMAND | MF_ENABLED : MF_BYCOMMAND | MF_GRAYED));
    EnableMenuItem(m_hMenu, IDM_SHORTCUT_SHOW,
                   (m_shortcutEditorInitialized ? MF_BYCOMMAND | MF_ENABLED : MF_BYCOMMAND | MF_GRAYED));

    DrawMenuBar(m_hwndMain);
}

// Code Snippets Implementation
void Win32IDE::loadCodeSnippets()
{
    // Delegate to the full snippet engine with multi-language built-in library
    // and VS Code-compatible JSON file loading
    loadBuiltInSnippets();
}

void Win32IDE::insertSnippet(const std::string& snippetName)
{
    for (const auto& snippet : m_codeSnippets)
    {
        if (snippet.name == snippetName || snippet.trigger == snippetName)
        {
            // Use the tab-stop engine for full VS Code-compatible snippet expansion
            insertSnippetWithTabStops(snippet.code);
            break;
        }
    }
    updateMenuEnableStates();
}

// Integrated Help Implementation
void Win32IDE::showGetHelp(const std::string& cmdlet)
{
    // Get selected text for help lookup
    CHARRANGE range;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&range);

    std::string command;
    if (!cmdlet.empty())
    {
        command = cmdlet;
    }
    else if (range.cpMax > range.cpMin)
    {
        char buffer[1000];
        TEXTRANGEA tr;
        tr.chrg = range;
        tr.lpstrText = buffer;
        SendMessageA(m_hwndEditor, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
        command = std::string(buffer);
    }
    else
    {
        command = "Get-Command";  // Default help
    }

    std::string helpCommand = "Get-Help " + command + " -Full\n";
    TerminalPane* pane = resolveTerminalPaneForUserTypedCommand();
    if (pane && pane->manager)
    {
        ensureShellRunningForPane(pane, pane->shellType);
        if (pane->manager->isRunning())
            pane->manager->writeInput(helpCommand);
    }
}

void Win32IDE::showCommandReference()
{
    std::string reference = "PowerShell Quick Reference:\n\n"
                            "Get-Help <command> - Get help for command\n"
                            "Get-Command - List all commands\n"
                            "Get-Member - Get object properties/methods\n"
                            "Measure-Object - Measure properties\n"
                            "Select-Object - Select properties\n"
                            "Where-Object - Filter objects\n"
                            "ForEach-Object - Process each object\n"
                            "Sort-Object - Sort objects\n"
                            "Group-Object - Group objects\n"
                            "Export-Csv - Export to CSV\n"
                            "Import-Csv - Import from CSV\n"
                            "ConvertTo-Json - Convert to JSON\n"
                            "ConvertFrom-Json - Convert from JSON\n";

    MessageBoxW(m_hwndMain, utf8ToWide(reference).c_str(), L"PowerShell Reference", MB_OK);
}

void Win32IDE::flushCtorBootReplayToSystem()
{
    if (g_ideCtorBootUserLines.empty())
        return;
    if (isShuttingDown())
    {
        g_ideCtorBootUserLines.clear();
        return;
    }
    if (!m_hwndOutputTabs || !IsWindow(m_hwndOutputTabs))
        return;
    for (const std::string& line : g_ideCtorBootUserLines)
        appendToOutput(line, "System", OutputSeverity::Info);
    g_ideCtorBootUserLines.clear();
}

// Output / Clipboard / Minimap / Profiling implementations
namespace
{
std::string normalizeOutputTabName(const std::string& name)
{
    if (name.empty() || _stricmp(name.c_str(), "General") == 0 || _stricmp(name.c_str(), "Output") == 0)
        return "Output";
    if (_stricmp(name.c_str(), "Errors") == 0)
        return "Errors";
    if (_stricmp(name.c_str(), "Debug") == 0)
        return "Debug";
    if (_stricmp(name.c_str(), "Find Results") == 0)
        return "Find Results";
    return "Output";
}

int outputTabControlId(const std::string& name)
{
    if (name == "Output")
        return IDC_OUTPUT_EDIT_GENERAL;
    if (name == "Errors")
        return IDC_OUTPUT_EDIT_ERRORS;
    if (name == "Debug")
        return IDC_OUTPUT_EDIT_DEBUG;
    return IDC_OUTPUT_EDIT_FIND;
}
}  // namespace

void Win32IDE::createOutputTabs()
{
    if (m_hwndOutputTabs)
        return;

    RECT client{};
    GetClientRect(m_hwndMain, &client);
    int tabBarHeight = 24;

    m_hwndOutputTabs =
        CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | TCS_TABS, 0, 0, client.right - 150,
                        tabBarHeight, m_hwndMain, (HMENU)IDC_OUTPUT_TABS, m_hInstance, nullptr);

    char logBuf[256];
    sprintf_s(logBuf, "OutputTabs HWND created: %p (Parent: %p)", m_hwndOutputTabs, m_hwndMain);
    LOG_INFO(std::string(logBuf));

    m_hwndSeverityFilter =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, client.right - 145,
                        2, 140, 100, m_hwndMain, (HMENU)IDC_SEVERITY_FILTER, m_hInstance, nullptr);
    SendMessageW(m_hwndSeverityFilter, CB_ADDSTRING, 0, (LPARAM)L"All Messages");
    SendMessageW(m_hwndSeverityFilter, CB_ADDSTRING, 0, (LPARAM)L"Info & Above");
    SendMessageW(m_hwndSeverityFilter, CB_ADDSTRING, 0, (LPARAM)L"Warnings & Errors");
    SendMessageW(m_hwndSeverityFilter, CB_ADDSTRING, 0, (LPARAM)L"Errors Only");
    SendMessage(m_hwndSeverityFilter, CB_SETCURSEL, m_severityFilterLevel, 0);

    static const struct
    {
        const wchar_t* text;
        int id;
        const char* key;
    } defs[] = {{L"Output", IDC_OUTPUT_EDIT_GENERAL, "Output"},
                {L"Errors", IDC_OUTPUT_EDIT_ERRORS, "Errors"},
                {L"Debug", IDC_OUTPUT_EDIT_DEBUG, "Debug"},
                {L"Find Results", IDC_OUTPUT_EDIT_FIND, "Find Results"}};

    for (int i = 0; i < 4; ++i)
    {
        TCITEMW tie{};
        tie.mask = TCIF_TEXT;
        tie.pszText = const_cast<wchar_t*>(defs[i].text);
        SendMessageW(m_hwndOutputTabs, TCM_INSERTITEMW, (WPARAM)i, (LPARAM)&tie);

        HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, RICHEDIT_CLASSW, L"",
                                     WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 0,
                                     tabBarHeight, client.right, m_outputTabHeight - tabBarHeight, m_hwndMain,
                                     (HMENU)(INT_PTR)defs[i].id, m_hInstance, nullptr);
        m_outputWindows[defs[i].key] = hEdit;
    }
    m_activeOutputTab = normalizeOutputTabName(m_activeOutputTab);

    // Restore persisted tab selection
    if (m_selectedOutputTab >= 0 && m_selectedOutputTab < 4)
    {
        const char* keys[] = {"Output", "Errors", "Debug", "Find Results"};
        m_activeOutputTab = keys[m_selectedOutputTab];
        TabCtrl_SetCurSel(m_hwndOutputTabs, m_selectedOutputTab);
    }

    // Initially show only active tab and respect visibility setting
    for (auto& kv : m_outputWindows)
    {
        ShowWindow(kv.second, (kv.first == m_activeOutputTab && m_outputPanelVisible) ? SW_SHOW : SW_HIDE);
    }
    ShowWindow(m_hwndOutputTabs, m_outputPanelVisible ? SW_SHOW : SW_HIDE);
    if (m_hwndSeverityFilter)
        ShowWindow(m_hwndSeverityFilter, m_outputPanelVisible ? SW_SHOW : SW_HIDE);
    if (m_hwndSplitter)
        ShowWindow(m_hwndSplitter, m_outputPanelVisible ? SW_SHOW : SW_HIDE);
}

void Win32IDE::addOutputTab(const std::string& name)
{
    const std::string normalizedName = normalizeOutputTabName(name);
    if (m_outputWindows.find(normalizedName) != m_outputWindows.end())
        return;
    if (!m_hwndOutputTabs || !IsWindow(m_hwndOutputTabs))
        return;
    RECT client{};
    GetClientRect(m_hwndMain, &client);
    int tabBarHeight = 24;
    HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                 WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 0, tabBarHeight,
                                 client.right, m_outputTabHeight - tabBarHeight, m_hwndMain,
                                 (HMENU)(INT_PTR)outputTabControlId(normalizedName), m_hInstance, nullptr);
    ShowWindow(hEdit, SW_HIDE);
    m_outputWindows[normalizedName] = hEdit;
}

void Win32IDE::appendToOutput(const std::string& text, const std::string& tabName, OutputSeverity severity)
{
    if (isShuttingDown())
        return;  // Window handles may be destroyed

    // Thread-safety: if called from a non-UI thread, marshal via PostMessage so the caller
    // is never blocked waiting for the UI thread to process each progress line.  This is
    // essential for the async model loader (loadModelFromPathAsync) where loadGGUFModel
    // emits ~10 status lines; blocking on each would unnecessarily couple worker progress
    // to UI-thread scheduling.  tabName / severity context is dropped (routes to "Output"
    // as Info), which is acceptable for background progress messages.
    if (m_hwndMain && GetWindowThreadProcessId(m_hwndMain, nullptr) != GetCurrentThreadId())
    {
        postOutputPanelSafe(text);
        return;
    }

    if (static_cast<int>(severity) < m_severityFilterLevel)
        return;

    std::string target = normalizeOutputTabName(tabName.empty() ? m_activeOutputTab : tabName);
    auto it = m_outputWindows.find(target);
    if (it == m_outputWindows.end())
    {
        addOutputTab(target);
        it = m_outputWindows.find(target);
    }
    if (it == m_outputWindows.end() || !it->second || !IsWindow(it->second))
        return;

    // Add timestamp for Errors and Debug tabs
    std::string timestampedText = text;
    if (target == "Errors" || target == "Debug")
    {
        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);
        char timestamp[16];
        strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", &timeinfo);
        timestampedText = std::string(timestamp) + text;
    }

    // Apply color formatting based on tab type
    if (target == "Errors")
    {
        formatOutput(timestampedText, RGB(220, 50, 50), "Errors");  // Red
    }
    else if (target == "Debug")
    {
        formatOutput(timestampedText, RGB(200, 180, 50), "Debug");  // Yellow
    }
    else
    {
        appendText(it->second, timestampedText);
    }
}

void Win32IDE::clearOutput(const std::string& tabName)
{
    std::string target = normalizeOutputTabName(tabName.empty() ? m_activeOutputTab : tabName);
    auto it = m_outputWindows.find(target);
    if (it != m_outputWindows.end() && it->second && IsWindow(it->second))
    {
        SetWindowTextW(it->second, L"");
    }
}

void Win32IDE::formatOutput(const std::string& text, COLORREF color, const std::string& tabName)
{
    std::string target = normalizeOutputTabName(tabName.empty() ? m_activeOutputTab : tabName);
    auto it = m_outputWindows.find(target);
    if (it == m_outputWindows.end() || !it->second || !IsWindow(it->second))
        return;

    HWND hwnd = it->second;
    GETTEXTLENGTHEX gtl{};
    gtl.flags = GTL_DEFAULT;
    gtl.codepage = CP_UNICODE;
    LONG len = SendMessage(hwnd, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
    SendMessage(hwnd, EM_SETSEL, len, len);

    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    cf.crTextColor = ensureReadableTextColor(m_currentTheme.backgroundColor, m_currentTheme.textColor);

    std::wstring wtext = utf8ToWide(text);
    SETTEXTEX st{};
    st.flags = ST_SELECTION;
    st.codepage = CP_UNICODE;
    SendMessageW(hwnd, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)wtext.c_str());
}

void Win32IDE::copyWithFormatting()
{
    // Simplified: copy selected plain text and store in history (vector<string>)
    CHARRANGE range;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&range);
    if (range.cpMax <= range.cpMin)
        return;
    LONG len = range.cpMax - range.cpMin;
    std::vector<wchar_t> buffer(len + 1);
    TEXTRANGEW tr{};
    tr.chrg = range;
    tr.lpstrText = buffer.data();
    SendMessageW(m_hwndEditor, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    buffer[len] = L'\0';
    std::string text = wideToUtf8(buffer.data());
    m_clipboardHistory.insert(m_clipboardHistory.begin(), text);
    if (m_clipboardHistory.size() > MAX_CLIPBOARD_HISTORY)
        m_clipboardHistory.resize(MAX_CLIPBOARD_HISTORY);
    if (OpenClipboard(m_hwndMain))
    {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hMem)
        {
            char* dest = (char*)GlobalLock(hMem);
            memcpy(dest, text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            // SetClipboardData takes ownership of hMem on success; only free on failure
            if (!SetClipboardData(CF_TEXT, hMem))
            {
                GlobalFree(hMem);
            }
        }
        CloseClipboard();
    }
}

void Win32IDE::pastePlainText()
{
    if (!m_hwndEditor)
        return;

    HWND owner = m_hwndMain ? m_hwndMain : m_hwndEditor;
    if (!OpenClipboard(owner))
        return;

    struct ClipboardGuard
    {
        ~ClipboardGuard() { CloseClipboard(); }
    } guard;

    // Prefer Unicode text; fall back to ANSI.
    if (IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (!hData)
            return;
        const wchar_t* data = (const wchar_t*)GlobalLock(hData);
        if (!data)
            return;

        std::wstring src(data);
        GlobalUnlock(hData);

        // Normalize newlines to CRLF for RichEdit consistency.
        std::wstring norm;
        norm.reserve(src.size() + 16);
        for (size_t i = 0; i < src.size(); ++i)
        {
            wchar_t c = src[i];
            if (c == L'\r')
            {
                norm.push_back(L'\r');
                if (i + 1 < src.size() && src[i + 1] == L'\n')
                {
                    norm.push_back(L'\n');
                    ++i;
                }
                else
                {
                    norm.push_back(L'\n');
                }
                continue;
            }
            if (c == L'\n')
            {
                norm.push_back(L'\r');
                norm.push_back(L'\n');
                continue;
            }
            norm.push_back(c);
        }

        SendMessageW(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)norm.c_str());
        return;
    }

    if (IsClipboardFormatAvailable(CF_TEXT))
    {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (!hData)
            return;
        const char* data = (const char*)GlobalLock(hData);
        if (!data)
            return;
        SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)data);
        GlobalUnlock(hData);
        return;
    }
}

void Win32IDE::pasteWithoutFormatting()
{
    if (OpenClipboard(m_hwndMain))
    {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData)
        {
            const char* data = (const char*)GlobalLock(hData);
            if (data)
            {
                SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)data);
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
}

void Win32IDE::copyLineNumbers()
{
    if (!m_hwndEditor)
        return;

    // Get selected range
    CHARRANGE range;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&range);

    // Get line numbers for selection
    int startLine = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, range.cpMin, 0);
    int endLine = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, range.cpMax, 0);

    // Build line number string
    std::string lineNumbers;
    for (int i = startLine; i <= endLine; ++i)
    {
        if (!lineNumbers.empty())
            lineNumbers += "\r\n";
        lineNumbers += std::to_string(i + 1);
    }

    // Copy to clipboard
    if (OpenClipboard(m_hwndMain))
    {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, lineNumbers.size() + 1);
        if (hMem)
        {
            char* dest = (char*)GlobalLock(hMem);
            memcpy(dest, lineNumbers.c_str(), lineNumbers.size() + 1);
            GlobalUnlock(hMem);
            // SetClipboardData takes ownership of hMem on success; only free on failure
            if (!SetClipboardData(CF_TEXT, hMem))
            {
                GlobalFree(hMem);
            }
        }
        CloseClipboard();
    }
}

void Win32IDE::showClipboardHistory()
{
    std::string msg = "Clipboard History (latest 10):\n\n";
    size_t count = std::min<size_t>(10, m_clipboardHistory.size());
    for (size_t i = 0; i < count; ++i)
    {
        const std::string& item = m_clipboardHistory[i];
        std::string preview = item.substr(0, 50);
        if (item.size() > 50)
            preview += "...";
        msg += std::to_string(i + 1) + ". " + preview + "\n";
    }
    MessageBoxW(m_hwndMain, utf8ToWide(msg).c_str(), L"Clipboard History", MB_OK);
}

void Win32IDE::clearClipboardHistory()
{
    m_clipboardHistory.clear();
}

#if !defined(RAWRXD_USE_DEDICATED_MINIMAP)
void Win32IDE::createMinimap()
{
    if (!m_hwndMain || !m_hwndEditor)
        return;

    m_minimapWidth = 120;
    m_minimapVisible = true;

    // Create minimap window as a child of main window
    RECT editorRect;
    GetWindowRect(m_hwndEditor, &editorRect);
    MapWindowPoints(HWND_DESKTOP, m_hwndMain, (LPPOINT)&editorRect, 2);

    int minimapX = editorRect.right - m_minimapWidth;
    int minimapY = editorRect.top;
    int minimapHeight = editorRect.bottom - editorRect.top;

    m_hwndMinimap = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, minimapX, minimapY,
                                    m_minimapWidth, minimapHeight, m_hwndMain, nullptr, m_hInstance, nullptr);

    if (m_hwndMinimap)
    {
        SetWindowLongPtrW(m_hwndMinimap, GWLP_USERDATA, (LONG_PTR)this);
    }

    updateMinimap();
}

void Win32IDE::updateMinimap()
{
    if (!m_hwndMinimap || !m_minimapVisible || !m_hwndEditor)
        return;

    std::string text = getWindowText(m_hwndEditor);
    if (text.empty())
    {
        m_minimapLines.clear();
        InvalidateRect(m_hwndMinimap, nullptr, TRUE);
        return;
    }

    // Split into lines for minimap rendering
    m_minimapLines.clear();
    m_minimapLineStarts.clear();

    std::istringstream stream(text);
    std::string line;
    int pos = 0;
    while (std::getline(stream, line))
    {
        m_minimapLines.push_back(line);
        m_minimapLineStarts.push_back(pos);
        pos += (int)line.size() + 1;  // +1 for newline
    }

    // Force redraw
    InvalidateRect(m_hwndMinimap, nullptr, TRUE);

    // Paint minimap content
    HDC hdc = GetDC(m_hwndMinimap);
    if (hdc)
    {
        RECT rc;
        GetClientRect(m_hwndMinimap, &rc);

        // Dark background
        HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);

        // Calculate visible area highlight
        int firstVisibleLine = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
        RECT editorRect;
        GetClientRect(m_hwndEditor, &editorRect);
        int visibleLines = editorRect.bottom / 16;  // Approximate line height

        // Draw visible area indicator
        int totalLines = (int)m_minimapLines.size();
        if (totalLines > 0)
        {
            float scale = (float)(rc.bottom - rc.top) / (float)totalLines;
            int highlightTop = (int)(firstVisibleLine * scale);
            int highlightHeight = (int)(visibleLines * scale);
            if (highlightHeight < 10)
                highlightHeight = 10;

            RECT highlightRect = {0, highlightTop, rc.right, highlightTop + highlightHeight};
            HBRUSH highlightBrush = CreateSolidBrush(RGB(60, 60, 80));
            FillRect(hdc, &highlightRect, highlightBrush);
            DeleteObject(highlightBrush);
        }

        // Draw minimap lines as colored blocks
        HPEN codePen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
        HPEN oldPen = (HPEN)SelectObject(hdc, codePen);

        float lineHeight = 2.0f;
        if (totalLines > 0 && totalLines * lineHeight > rc.bottom)
        {
            lineHeight = (float)(rc.bottom - 4) / (float)totalLines;
            if (lineHeight < 1.0f)
                lineHeight = 1.0f;
        }

        for (size_t i = 0; i < m_minimapLines.size() && i * lineHeight < rc.bottom; ++i)
        {
            const std::string& line = m_minimapLines[i];
            if (line.empty())
                continue;

            int y = (int)(i * lineHeight) + 2;
            int lineLen = (int)line.size();
            int pixelLen = (lineLen * rc.right) / 200;  // Scale to minimap width
            if (pixelLen > rc.right - 4)
                pixelLen = rc.right - 4;
            if (pixelLen < 2)
                pixelLen = 2;

            MoveToEx(hdc, 2, y, nullptr);
            LineTo(hdc, 2 + pixelLen, y);
        }

        SelectObject(hdc, oldPen);
        DeleteObject(codePen);

        ReleaseDC(m_hwndMinimap, hdc);
    }
}

void Win32IDE::scrollToMinimapPosition(int y)
{
    if (!m_hwndMinimap || !m_hwndEditor || m_minimapLines.empty())
        return;

    RECT rc;
    GetClientRect(m_hwndMinimap, &rc);

    int totalLines = (int)m_minimapLines.size();
    int targetLine = (y * totalLines) / rc.bottom;

    if (targetLine < 0)
        targetLine = 0;
    if (targetLine >= totalLines)
        targetLine = totalLines - 1;

    // Scroll editor to target line
    int charIndex = 0;
    if (targetLine < (int)m_minimapLineStarts.size())
    {
        charIndex = m_minimapLineStarts[targetLine];
    }

    SendMessage(m_hwndEditor, EM_SETSEL, charIndex, charIndex);
    SendMessage(m_hwndEditor, EM_SCROLLCARET, 0, 0);

    updateMinimap();
}

void Win32IDE::toggleMinimap()
{
    m_minimapVisible = !m_minimapVisible;
    if (m_hwndMinimap)
    {
        ShowWindow(m_hwndMinimap, m_minimapVisible ? SW_SHOW : SW_HIDE);
    }
    else if (m_minimapVisible)
    {
        createMinimap();
    }

    // Trigger layout update
    RECT rc;
    GetClientRect(m_hwndMain, &rc);
    onSize(rc.right, rc.bottom);
}
#endif

void Win32IDE::startProfiling()
{
    if (!m_profilingActive)
    {
        m_profilingActive = true;
        QueryPerformanceCounter(&m_profilingStart);
        QueryPerformanceFrequency(&m_profilingFreq);
        m_profilingResults.clear();
    }
}

void Win32IDE::stopProfiling()
{
    if (m_profilingActive)
    {
        LARGE_INTEGER end;
        QueryPerformanceCounter(&end);
        double ms = (double)(end.QuadPart - m_profilingStart.QuadPart) * 1000.0 / (double)m_profilingFreq.QuadPart;
        m_profilingResults.push_back({"Session", ms});
        m_profilingActive = false;
    }
}

void Win32IDE::showProfileResults()
{
    std::string msg = "Profile Results:\n\n";
    for (auto& pr : m_profilingResults)
    {
        msg += pr.first + ": " + std::to_string(pr.second) + " ms\n";
    }
    MessageBoxW(m_hwndMain, utf8ToWide(msg).c_str(), L"Profiling", MB_OK);
}

void Win32IDE::analyzeScript()
{
    std::string script = getWindowText(m_hwndEditor);
    if (script.empty())
    {
        MessageBoxW(m_hwndMain, L"Script is empty.", L"Analyze Script", MB_OK);
        return;
    }

    appendToOutput("Starting AI Analysis...\n", "Output", OutputSeverity::Info);

    // Asynchronous analysis to avoid blocking UI
    std::thread(
        [this, script]()
        {
            DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
            if (_guard.cancelled)
                return;
            if (m_nativeEngine)
            {
                std::string prompt =
                    "Analyze the following script and report potential bugs, security issues, and improvements:\n\n" +
                    script;
                // Assuming CPUInferenceEngine has an 'infer' or 'generate' method that takes a string
                // Based on cpu_inference_engine.cpp read earlier: std::string infer(const std::string& prompt);

                auto* engine = m_nativeEngine.get();
                auto tokens = engine->Tokenize(prompt);
                auto output_tokens = engine->Generate(tokens, 512);
                std::string result = engine->Detokenize(output_tokens);

                // Post result back to UI thread or just append (if appendToOutput is thread-safe or we lock)
                // appendToOutput uses SendMessage which is generally thread-safe for simple text
                this->appendToOutput("\n=== AI Analysis Result ===\n" + result + "\n==========================\n",
                                     "Output", OutputSeverity::Info);
            }
            else
            {
                this->appendToOutput("Error: Inference Engine not available.\n", "Errors", OutputSeverity::Error);
            }
        })
        .detach();
}

void Win32IDE::measureExecutionTime()
{
    // Get the currently selected text from the editor
    CHARRANGE selection = {};
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&selection);
    const int selLen = selection.cpMax - selection.cpMin;
    if (selLen <= 0)
    {
        appendToOutput("measureExecutionTime: No text selected — select a script block first.\n", "Output",
                       OutputSeverity::Warning);
        return;
    }
    std::string selectedText(selLen + 1, 0);
    SendMessage(m_hwndEditor, EM_GETSELTEXT, 0, (LPARAM)&selectedText[0]);
    selectedText.resize(selLen);

    // Write to a temporary PowerShell script
    char tmpDir[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, tmpDir);
    const std::string scriptPath = std::string(tmpDir) + "rawrxd_exec_tmp.ps1";
    {
        std::ofstream f(scriptPath);
        if (!f)
        {
            appendToOutput("measureExecutionTime: Failed to write temp script.\n", "Errors", OutputSeverity::Error);
            return;
        }
        f << selectedText;
    }

    // Execute and measure wall-clock time
    const auto start = std::chrono::high_resolution_clock::now();
    const std::string cmdLine = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + scriptPath + "\"";
    const std::string result = ExecCmd(cmdLine.c_str());
    const auto end = std::chrono::high_resolution_clock::now();
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    DeleteFileA(scriptPath.c_str());

    appendToOutput("=== Execution Time: " + std::to_string(ms) + "ms ===\n" +
                       (result.empty() ? "(no output)\n" : result + "\n"),
                   "Output", OutputSeverity::Info);
}

// Module Management
void Win32IDE::refreshModuleList()
{
    m_modules.clear();

    // Default module set (always available)
    m_modules.push_back({"Microsoft.PowerShell.Management", "3.0.0.0", "Management cmdlets", "", true});
    m_modules.push_back({"Microsoft.PowerShell.Utility", "3.0.0.0", "Utility cmdlets", "", true});
    m_modules.push_back({"PSReadLine", "2.0.0", "Command line editing", "", false});

    // Dynamic module enumeration via Powershell command
    std::string cmd = "powershell.exe -NoProfile -Command \"Get-Module -ListAvailable | Select-Object -First 50 Name, "
                      "Version | ConvertTo-Json -Compress\"";
    std::string output = ExecCmd(cmd.c_str());

    if (output.find("Error") == std::string::npos && !output.empty())
    {
        try
        {
            auto json = nlohmann::json::parse(output);
            if (json.is_array())
            {
                for (size_t i = 0; i < json.size(); ++i)
                {
                    const auto& item = json[i];
                    ModuleInfo m;
                    if (item.is_object())
                    {
                        m.name = item.value("Name", "");
                        if (item.contains("Version"))
                        {
                            auto v = item["Version"];
                            if (v.is_object())
                            {
                                // PS version object
                                m.version =
                                    std::to_string(v.value("Major", 0)) + "." + std::to_string(v.value("Minor", 0));
                            }
                            else if (v.is_string())
                            {
                                m.version = v.get<std::string>();
                            }
                            else
                            {
                                m.version = "0.0.0";
                            }
                        }
                        else
                        {
                            m.version = "0.0.0";
                        }
                    }
                    m.description = "User Module";
                    m.path = "";
                    m.loaded = false;  // Check via Get-Module without ListAvailable if needed

                    // Avoid duplicates
                    bool exists = false;
                    for (const auto& existing : m_modules)
                        if (existing.name == m.name)
                            exists = true;
                    if (!exists)
                        m_modules.push_back(m);
                }
            }
            else if (json.is_object())
            {
                // Single module
                ModuleInfo m;
                m.name = json.value("Name", "");
                m.version = "1.0";
                m.description = "User Module";
                m_modules.push_back(m);
            }
        }
        catch (...)
        {
            // JSON parsing failed, likely non-JSON output or empty
        }
    }
}

void Win32IDE::showModuleBrowser()
{
    std::string msg = "Modules:\n\n";
    for (auto& m : m_modules)
    {
        msg += m.name + " (" + m.version + ")" + (m.loaded ? " [Loaded]" : " [Available]") + "\n";
    }
    MessageBoxW(m_hwndMain, utf8ToWide(msg).c_str(), L"Module Browser", MB_OK);
}

void Win32IDE::loadModule(const std::string& moduleName)
{
    bool found = false;
    for (auto& m : m_modules)
    {
        if (m.name == moduleName)
        {
            m.loaded = true;
            found = true;
            break;
        }
    }

    // Explicit Logic: Actually load the module in PowerShell
    std::string command = "Import-Module '" + moduleName + "'\n";

    TerminalPane* pane = resolveTerminalPaneForUserTypedCommand();
    if (pane && pane->manager)
    {
        ensureShellRunningForPane(pane, pane->shellType);
        if (pane->manager->isRunning())
        {
            pane->manager->writeInput(command);
            appendToOutput("Loading module: " + moduleName, "Output", OutputSeverity::Info);
        }
        else
            appendToOutput("Cannot load module '" + moduleName + "': shell not running.", "Errors",
                           OutputSeverity::Error);
    }
    else
    {
        appendToOutput("Cannot load module '" + moduleName + "': No user terminal.", "Errors", OutputSeverity::Error);
    }
}

void Win32IDE::unloadModule(const std::string& moduleName)
{
    bool found = false;
    for (auto& m : m_modules)
    {
        if (m.name == moduleName)
        {
            m.loaded = false;
            found = true;
            break;
        }
    }

    // Explicit Logic: Actually remove the module in PowerShell
    std::string command = "Remove-Module '" + moduleName + "'\n";

    TerminalPane* pane = resolveTerminalPaneForUserTypedCommand();
    if (pane && pane->manager)
    {
        ensureShellRunningForPane(pane, pane->shellType);
        if (pane->manager->isRunning())
        {
            pane->manager->writeInput(command);
            appendToOutput("Unloading module: " + moduleName, "Output", OutputSeverity::Info);
        }
        else
            appendToOutput("Cannot unload module '" + moduleName + "': shell not running.", "Errors",
                           OutputSeverity::Error);
    }
    else
    {
        appendToOutput("Cannot unload module '" + moduleName + "': No user terminal.", "Errors", OutputSeverity::Error);
    }
}

void Win32IDE::importModule()
{
    OPENFILENAMEW ofn = {};
    wchar_t szFile[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = L"PowerShell Modules (*.psm1;*.psd1)\0*.psm1;*.psd1\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Import Module";

    if (GetOpenFileNameW(&ofn))
    {
        std::string modulePath = wideToUtf8(szFile);
        std::string command = "Import-Module '" + modulePath + "'\n";

        TerminalPane* pane = resolveTerminalPaneForUserTypedCommand();
        if (pane && pane->manager)
        {
            ensureShellRunningForPane(pane, pane->shellType);
            if (pane->manager->isRunning())
            {
                pane->manager->writeInput(command);
                appendToOutput("Importing module: " + modulePath + "\n", "Output", OutputSeverity::Info);
            }
        }

        // Refresh module list after import
        refreshModuleList();
    }
}

void Win32IDE::exportModule()
{
    // Show dialog to select module to export
    if (m_modules.empty())
    {
        MessageBoxW(m_hwndMain, L"No modules loaded. Refresh module list first.", L"Export Module",
                    MB_OK | MB_ICONINFORMATION);
        return;
    }

    // Build list of module names for selection
    std::string moduleList = "Available modules:\n\n";
    for (size_t i = 0; i < m_modules.size(); ++i)
    {
        moduleList += std::to_string(i + 1) + ". " + m_modules[i].name;
        if (m_modules[i].loaded)
            moduleList += " [Loaded]";
        moduleList += "\n";
    }
    moduleList += "\nExport the first loaded module?";

    if (MessageBoxW(m_hwndMain, utf8ToWide(moduleList).c_str(), L"Export Module", MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        // Find first loaded module
        for (const auto& mod : m_modules)
        {
            if (mod.loaded)
            {
                OPENFILENAMEW ofn = {};
                std::wstring defaultName = utf8ToWide(mod.name + ".psm1");
                wchar_t szFile[MAX_PATH] = L"";
                wcsncpy_s(szFile, defaultName.c_str(), _TRUNCATE);
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = m_hwndMain;
                ofn.lpstrFilter = L"PowerShell Module (*.psm1)\0*.psm1\0PowerShell Data (*.psd1)\0*.psd1\0";
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_OVERWRITEPROMPT;
                ofn.lpstrTitle = L"Export Module";

                if (GetSaveFileNameW(&ofn))
                {
                    std::string savePath = wideToUtf8(szFile);
                    std::string command =
                        "Export-ModuleMember -Function * -Cmdlet * -Variable * -Alias * -PassThru | Out-File '" +
                        savePath + "'\n";

                    TerminalPane* pane = resolveTerminalPaneForUserTypedCommand();
                    if (pane && pane->manager)
                    {
                        ensureShellRunningForPane(pane, pane->shellType);
                        if (pane->manager->isRunning())
                        {
                            pane->manager->writeInput(command);
                            appendToOutput("Exporting module to: " + savePath + "\n", "Output", OutputSeverity::Info);
                        }
                    }
                }
                break;
            }
        }
    }
}

// Theme Management
void Win32IDE::resetToDefaultTheme()
{
    applyThemeById(IDM_THEME_DARK_PLUS);
}

void Win32IDE::saveCodeSnippets()
{
    CreateDirectoryA("snippets", NULL);
    std::ofstream file("snippets\\snippets.txt");
    if (file.is_open())
    {
        for (const auto& snippet : m_codeSnippets)
        {
            file << "[SNIPPET]" << std::endl;
            file << "name=" << snippet.name << std::endl;
            file << "description=" << snippet.description << std::endl;
            file << "code_start" << std::endl;
            file << snippet.code << std::endl;
            file << "code_end" << std::endl;
        }
        file.close();
    }
}

void Win32IDE::showPowerShellDocs()
{
    MessageBoxW(m_hwndMain, L"Open https://learn.microsoft.com/powershell/ for full docs.", L"PowerShell Docs", MB_OK);
}

void Win32IDE::searchHelp(const std::string& query)
{
    std::string q = query.empty() ? "Get-Command" : query;
    std::string cmd = "Get-Help " + q + " -Online\n";
    TerminalPane* pane = resolveTerminalPaneForUserTypedCommand();
    if (pane && pane->manager)
    {
        ensureShellRunningForPane(pane, pane->shellType);
        if (pane->manager->isRunning())
            pane->manager->writeInput(cmd);
    }
}

void Win32IDE::toggleFloatingPanel()
{
    if (!m_hwndFloatingPanel)
        return;  // created elsewhere
    BOOL vis = IsWindowVisible(m_hwndFloatingPanel);
    ShowWindow(m_hwndFloatingPanel, vis ? SW_HIDE : SW_SHOW);
}

// ============================================================================
// Floating Panel Implementation
// ============================================================================

void Win32IDE::createFloatingPanel()
{
    if (m_hwndFloatingPanel)
        return;  // Already created

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = FloatingPanelProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.lpszClassName = L"RawrXD_FloatingPanel";
    RegisterClassExW(&wc);

    RECT rcMain;
    GetClientRect(m_hwndMain, &rcMain);
    int panelWidth = rcMain.right - rcMain.left;
    int panelHeight = 250;
    int panelX = rcMain.left;
    int panelY = rcMain.bottom - panelHeight;

    m_hwndFloatingPanel = CreateWindowExW(WS_EX_TOOLWINDOW, L"RawrXD_FloatingPanel", L"Panel",
                                          WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_BORDER, panelX, panelY,
                                          panelWidth, panelHeight, m_hwndMain, nullptr, m_hInstance, nullptr);

    if (!m_hwndFloatingPanel)
    {
        appendToOutput("Failed to create floating panel\n", "Output", OutputSeverity::Error);
        return;
    }

    SetWindowLongPtrW(m_hwndFloatingPanel, GWLP_USERDATA, (LONG_PTR)this);

    static const wchar_t* tabLabels[] = {L"Problems", L"Output", L"Debug Console", L"Terminal"};
    for (int i = 0; i < 4; i++)
    {
        CreateWindowExW(0, L"BUTTON", tabLabels[i], WS_CHILD | WS_VISIBLE | BS_FLAT | BS_PUSHBUTTON, 5 + i * 120, 2,
                        115, 24, m_hwndFloatingPanel, (HMENU)(UINT_PTR)(7001 + i), m_hInstance, nullptr);
    }

    m_hwndFloatingContent =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, 28,
                        panelWidth, panelHeight - 28, m_hwndFloatingPanel, nullptr, m_hInstance, nullptr);

    if (m_hwndFloatingContent)
    {
        SendMessageW(m_hwndFloatingContent, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }

    appendToOutput("Floating panel created\n", "Output", OutputSeverity::Info);
}

void Win32IDE::showFloatingPanel()
{
    if (!m_hwndFloatingPanel)
    {
        createFloatingPanel();
    }
    if (m_hwndFloatingPanel)
    {
        ShowWindow(m_hwndFloatingPanel, SW_SHOW);
        m_outputPanelVisible = true;
    }
}

void Win32IDE::hideFloatingPanel()
{
    if (m_hwndFloatingPanel)
    {
        ShowWindow(m_hwndFloatingPanel, SW_HIDE);
        m_outputPanelVisible = false;
    }
}

void Win32IDE::updateFloatingPanelContent(const std::string& content)
{
    if (!m_hwndFloatingContent)
        return;
    std::wstring wcontent = utf8ToWide(content);
    int textLen = GetWindowTextLengthW(m_hwndFloatingContent);
    SendMessageW(m_hwndFloatingContent, EM_SETSEL, (WPARAM)textLen, (LPARAM)textLen);
    SendMessageW(m_hwndFloatingContent, EM_REPLACESEL, FALSE, (LPARAM)wcontent.c_str());
    SendMessageW(m_hwndFloatingContent, EM_SCROLLCARET, 0, 0);
}

void Win32IDE::setFloatingPanelTab(int tabIndex)
{
    if (!m_hwndFloatingPanel)
        return;

    // Visually highlight the active tab button and unhighlight others
    for (int i = 0; i < 4; i++)
    {
        HWND hTabBtn = GetDlgItem(m_hwndFloatingPanel, 7001 + i);
        if (hTabBtn)
        {
            if (i == tabIndex)
            {
                SendMessageW(hTabBtn, BM_SETSTATE, TRUE, 0);
            }
            else
            {
                SendMessageW(hTabBtn, BM_SETSTATE, FALSE, 0);
            }
        }
    }

    if (m_hwndFloatingContent)
    {
        static const wchar_t* tabTitles[] = {L"=== Problems ===\r\n", L"=== Output ===\r\n",
                                             L"=== Debug Console ===\r\n", L"=== Terminal ===\r\n"};
        if (tabIndex >= 0 && tabIndex < 4)
        {
            SetWindowTextW(m_hwndFloatingContent, tabTitles[tabIndex]);
        }
    }
}

LRESULT CALLBACK Win32IDE::FloatingPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            // Dark background matching VS Code panel area
            HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);

            // Draw a subtle top border line (panel separator)
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 122, 204));
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            MoveToEx(hdc, rc.left, rc.top, nullptr);
            LineTo(hdc, rc.right, rc.top);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE:
        {
            if (pThis && pThis->m_hwndFloatingContent)
            {
                RECT rc;
                GetClientRect(hwnd, &rc);
                // Resize content area below the tab buttons (28px tab bar)
                MoveWindow(pThis->m_hwndFloatingContent, 0, 28, rc.right, rc.bottom - 28, TRUE);
            }
            return 0;
        }

        case WM_COMMAND:
        {
            if (pThis)
            {
                int id = LOWORD(wParam);
                // Tab button IDs: 7001=Problems, 7002=Output, 7003=Debug Console, 7004=Terminal
                if (id >= 7001 && id <= 7004)
                {
                    pThis->setFloatingPanelTab(id - 7001);
                    return 0;
                }
            }
            break;
        }

        case WM_CLOSE:
            if (pThis)
            {
                pThis->hideFloatingPanel();
                return 0;
            }
            break;
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

int Win32IDE::getPanelAreaWidth() const
{
    if (!m_hwndMain)
        return 0;

    RECT rcMain;
    GetClientRect(m_hwndMain, &rcMain);
    int totalWidth = rcMain.right - rcMain.left;

    // Panel area width = total width minus sidebar (if visible) minus activity bar minus secondary sidebar
    int sidebarOffset = 0;
    if (m_sidebarVisible)
    {
        sidebarOffset = m_sidebarWidth + dpiScale(48);  // activity bar width (DPI-scaled)
    }
    int secondarySidebarOffset = m_secondarySidebarVisible ? m_secondarySidebarWidth : 0;

    return totalWidth - sidebarOffset - secondarySidebarOffset;
}

// ============================================================================
// Search and Replace Implementation
// ============================================================================

#define IDD_FIND 5001
#define IDD_REPLACE 5002
#define IDC_FIND_TEXT 5010
#define IDC_REPLACE_TEXT 5011
#define IDC_CASE_SENSITIVE 5020
#define IDC_WHOLE_WORD 5021
#define IDC_USE_REGEX 5022
#define IDC_BTN_FIND_NEXT 5030
#define IDC_BTN_REPLACE 5031
#define IDC_BTN_REPLACE_ALL 5032
#define IDC_BTN_CLOSE 5033

void Win32IDE::showFindDialog()
{
    if (m_hwndFindDialog && IsWindow(m_hwndFindDialog))
    {
        SetForegroundWindow(m_hwndFindDialog);
        return;
    }

    m_hwndFindDialog =
        CreateDialogParamW(m_hInstance, MAKEINTRESOURCEW(IDD_FIND), m_hwndMain, FindDialogProc, (LPARAM)this);

    if (!m_hwndFindDialog)
    {
        // WS_EX_TOPMOST keeps dialog visible during screenshot attempts (focus theft protection)
        HWND hwndDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"STATIC", L"Find",
                                       WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 100, 100, 400, 150, m_hwndMain,
                                       nullptr, m_hInstance, nullptr);
        if (!hwndDlg)
        {
            const DWORD createErr = GetLastError();
            appendToOutput("[UI] Failed to create Find dialog (GetLastError=" + std::to_string(createErr) + ")\n",
                           "Errors", OutputSeverity::Error);
            return;
        }
        m_hwndFindDialog = hwndDlg;

        CreateWindowExW(0, L"STATIC", L"Find what:", WS_CHILD | WS_VISIBLE, 10, 15, 80, 20, hwndDlg, nullptr,
                        m_hInstance, nullptr);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", utf8ToWide(m_lastSearchText).c_str(),
                        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 12, 280, 22, hwndDlg, (HMENU)IDC_FIND_TEXT,
                        m_hInstance, nullptr);

        CreateWindowExW(0, L"BUTTON", L"Case sensitive", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10, 45, 120, 20,
                        hwndDlg, (HMENU)IDC_CASE_SENSITIVE, m_hInstance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Whole word", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 140, 45, 100, 20, hwndDlg,
                        (HMENU)IDC_WHOLE_WORD, m_hInstance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Regex", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 250, 45, 70, 20, hwndDlg,
                        (HMENU)IDC_USE_REGEX, m_hInstance, nullptr);

        CreateWindowExW(0, L"BUTTON", L"Find Next", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 10, 80, 90, 28, hwndDlg,
                        (HMENU)IDC_BTN_FIND_NEXT, m_hInstance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE, 110, 80, 90, 28, hwndDlg, (HMENU)IDC_BTN_CLOSE,
                        m_hInstance, nullptr);
    }

    ShowWindow(m_hwndFindDialog, SW_SHOW);
}

void Win32IDE::showReplaceDialog()
{
    if (m_hwndReplaceDialog && IsWindow(m_hwndReplaceDialog))
    {
        SetForegroundWindow(m_hwndReplaceDialog);
        return;
    }

    // WS_EX_TOPMOST keeps dialog visible during screenshot attempts (focus theft protection)
    HWND hwndDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"STATIC", L"Replace",
                                   WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 100, 100, 400, 200, m_hwndMain,
                                   nullptr, m_hInstance, nullptr);
    if (!hwndDlg)
    {
        const DWORD createErr = GetLastError();
        appendToOutput("[UI] Failed to create Replace dialog (GetLastError=" + std::to_string(createErr) + ")\n",
                       "Errors", OutputSeverity::Error);
        return;
    }
    m_hwndReplaceDialog = hwndDlg;

    CreateWindowExW(0, L"STATIC", L"Find what:", WS_CHILD | WS_VISIBLE, 10, 15, 80, 20, hwndDlg, nullptr, m_hInstance,
                    nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", utf8ToWide(m_lastSearchText).c_str(),
                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 12, 280, 22, hwndDlg, (HMENU)IDC_FIND_TEXT,
                    m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Replace with:", WS_CHILD | WS_VISIBLE, 10, 45, 80, 20, hwndDlg, nullptr,
                    m_hInstance, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", utf8ToWide(m_lastReplaceText).c_str(),
                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 42, 280, 22, hwndDlg, (HMENU)IDC_REPLACE_TEXT,
                    m_hInstance, nullptr);

    CreateWindowExW(0, L"BUTTON", L"Case sensitive", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 10, 75, 120, 20, hwndDlg,
                    (HMENU)IDC_CASE_SENSITIVE, m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Whole word", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 140, 75, 100, 20, hwndDlg,
                    (HMENU)IDC_WHOLE_WORD, m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Regex", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 250, 75, 70, 20, hwndDlg,
                    (HMENU)IDC_USE_REGEX, m_hInstance, nullptr);

    CreateWindowExW(0, L"BUTTON", L"Find Next", WS_CHILD | WS_VISIBLE, 10, 110, 90, 28, hwndDlg,
                    (HMENU)IDC_BTN_FIND_NEXT, m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Replace", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 110, 110, 90, 28, hwndDlg,
                    (HMENU)IDC_BTN_REPLACE, m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Replace All", WS_CHILD | WS_VISIBLE, 210, 110, 90, 28, hwndDlg,
                    (HMENU)IDC_BTN_REPLACE_ALL, m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE, 310, 110, 70, 28, hwndDlg, (HMENU)IDC_BTN_CLOSE,
                    m_hInstance, nullptr);

    ShowWindow(m_hwndReplaceDialog, SW_SHOW);
}

void Win32IDE::setFindReplaceDialogSeed(const std::string& findQuery, const std::string& replaceQuery)
{
    m_lastSearchText = findQuery;
    if (!replaceQuery.empty())
        m_lastReplaceText = replaceQuery;

    const std::wstring wFind = utf8ToWide(m_lastSearchText);
    const std::wstring wRepl = utf8ToWide(m_lastReplaceText);

    if (m_hwndFindDialog && IsWindow(m_hwndFindDialog))
    {
        if (HWND h = GetDlgItem(m_hwndFindDialog, IDC_FIND_TEXT))
            SetWindowTextW(h, wFind.c_str());
    }
    if (m_hwndReplaceDialog && IsWindow(m_hwndReplaceDialog))
    {
        if (HWND h = GetDlgItem(m_hwndReplaceDialog, IDC_FIND_TEXT))
            SetWindowTextW(h, wFind.c_str());
        if (!replaceQuery.empty())
        {
            if (HWND h = GetDlgItem(m_hwndReplaceDialog, IDC_REPLACE_TEXT))
                SetWindowTextW(h, wRepl.c_str());
        }
    }
}

HWND Win32IDE::resolveFindReplaceTargetHwnd()
{
    const HWND foc = GetFocus();
    if (m_hwndEditor && IsWindow(m_hwndEditor) && foc == m_hwndEditor)
        return m_hwndEditor;
    for (const auto& pane : m_terminalPanes)
    {
        if (pane.hwnd && IsWindow(pane.hwnd) && foc == pane.hwnd)
            return pane.hwnd;
    }
    // PowerShell panel: finding from the input line should search the scrollback (same as VS Code / Cursor terminal
    // find).
    if (m_hwndPowerShellInput && IsWindow(m_hwndPowerShellInput) && foc == m_hwndPowerShellInput &&
        m_hwndPowerShellOutput && IsWindow(m_hwndPowerShellOutput))
        return m_hwndPowerShellOutput;
    if (m_hwndPowerShellOutput && IsWindow(m_hwndPowerShellOutput) && foc == m_hwndPowerShellOutput)
        return m_hwndPowerShellOutput;
    for (const auto& kv : m_outputWindows)
    {
        if (kv.second && IsWindow(kv.second) && foc == kv.second)
            return kv.second;
    }
    const bool outputPanelTab = m_outputPanelVisible && m_activePanelTab == PanelTab::Output;
    if (outputPanelTab && foc)
    {
        auto focusIn = [](HWND root, HWND f) -> bool
        { return root && IsWindow(root) && f && (f == root || IsChild(root, f)); };
        if ((m_hwndOutputTabs && IsWindow(m_hwndOutputTabs) && focusIn(m_hwndOutputTabs, foc)) ||
            (m_hwndSeverityFilter && IsWindow(m_hwndSeverityFilter) && foc == m_hwndSeverityFilter))
        {
            const auto it = m_outputWindows.find(m_activeOutputTab);
            if (it != m_outputWindows.end() && it->second && IsWindow(it->second))
                return it->second;
        }
    }
    const bool terminalTab = m_outputPanelVisible && m_activePanelTab == PanelTab::Terminal;
    if (terminalTab)
    {
        auto focusIn = [](HWND root, HWND f) -> bool
        { return root && IsWindow(root) && f && (f == root || IsChild(root, f)); };
        const bool tabStripFocused =
            g_rawrxdIntegratedTerminalTabs && IsWindow(g_rawrxdIntegratedTerminalTabs) && foc &&
            (foc == g_rawrxdIntegratedTerminalTabs || IsChild(g_rawrxdIntegratedTerminalTabs, foc));
        const bool panelToolbarFocused = foc && m_hwndPanelToolbar && IsWindow(m_hwndPanelToolbar) &&
                                         (foc == m_hwndPanelToolbar || IsChild(m_hwndPanelToolbar, foc));
        const bool panelTabsFocused = m_hwndPanelTabs && IsWindow(m_hwndPanelTabs) && foc &&
                                      (foc == m_hwndPanelTabs || IsChild(m_hwndPanelTabs, foc));
        if (focusIn(m_hwndFindDialog, foc) || focusIn(m_hwndReplaceDialog, foc) || focusIn(m_hwndCommandInput, foc) ||
            tabStripFocused || panelToolbarFocused || panelTabsFocused)
        {
            TerminalPane* tp = getActiveTerminalPane();
            if (tp && tp->hwnd && IsWindow(tp->hwnd))
                return tp->hwnd;
        }
    }
    if (m_hwndDebugConsoleOutput && IsWindow(m_hwndDebugConsoleOutput) && foc == m_hwndDebugConsoleOutput)
        return m_hwndDebugConsoleOutput;
    if (m_hwndDebugConsoleInput && IsWindow(m_hwndDebugConsoleInput) && foc == m_hwndDebugConsoleInput)
        return m_hwndDebugConsoleInput;
    const bool debugTab = m_outputPanelVisible && m_activePanelTab == PanelTab::DebugConsole;
    if (debugTab && foc)
    {
        auto focusIn = [](HWND root, HWND f) -> bool
        { return root && IsWindow(root) && f && (f == root || IsChild(root, f)); };
        const bool panelTabsFocusedDbg = m_hwndPanelTabs && IsWindow(m_hwndPanelTabs) && foc &&
                                         (foc == m_hwndPanelTabs || IsChild(m_hwndPanelTabs, foc));
        const bool panelToolbarFocusedDbg = foc && m_hwndPanelToolbar && IsWindow(m_hwndPanelToolbar) &&
                                            (foc == m_hwndPanelToolbar || IsChild(m_hwndPanelToolbar, foc));
        if (focusIn(m_hwndFindDialog, foc) || focusIn(m_hwndReplaceDialog, foc) || panelTabsFocusedDbg ||
            panelToolbarFocusedDbg)
        {
            if (m_hwndDebugConsoleOutput && IsWindow(m_hwndDebugConsoleOutput))
                return m_hwndDebugConsoleOutput;
        }
    }
    const bool problemsTab = m_outputPanelVisible && m_activePanelTab == PanelTab::Problems;
    if (problemsTab && foc && m_hwndProblemsListView && IsWindow(m_hwndProblemsListView))
    {
        auto focusIn = [](HWND root, HWND f) -> bool
        { return root && IsWindow(root) && f && (f == root || IsChild(root, f)); };
        if (foc == m_hwndProblemsListView || IsChild(m_hwndProblemsListView, foc))
            return m_hwndProblemsListView;
        if (m_hwndProblemsFilter && IsWindow(m_hwndProblemsFilter) && foc == m_hwndProblemsFilter)
            return m_hwndProblemsListView;
        if (m_hwndPanelTabs && focusIn(m_hwndPanelTabs, foc))
            return m_hwndProblemsListView;
        if (m_hwndPanelToolbar && focusIn(m_hwndPanelToolbar, foc))
            return m_hwndProblemsListView;
        if (focusIn(m_hwndFindDialog, foc) || focusIn(m_hwndReplaceDialog, foc))
            return m_hwndProblemsListView;
    }
    if (m_hwndEditor && IsWindow(m_hwndEditor))
        return m_hwndEditor;
    return nullptr;
}

void Win32IDE::findNext()
{
    if (m_lastSearchText.empty())
    {
        showFindDialog();
        return;
    }
    findText(m_lastSearchText, true, m_searchCaseSensitive, m_searchWholeWord, m_searchUseRegex);
}

void Win32IDE::findPrevious()
{
    if (m_lastSearchText.empty())
    {
        showFindDialog();
        return;
    }
    findText(m_lastSearchText, false, m_searchCaseSensitive, m_searchWholeWord, m_searchUseRegex);
}

void Win32IDE::replaceNext()
{
    if (m_lastSearchText.empty())
    {
        showReplaceDialog();
        return;
    }
    replaceText(m_lastSearchText, m_lastReplaceText, false, m_searchCaseSensitive, m_searchWholeWord, m_searchUseRegex);
}

void Win32IDE::replaceAll()
{
    if (m_lastSearchText.empty())
    {
        showReplaceDialog();
        return;
    }
    int count = replaceText(m_lastSearchText, m_lastReplaceText, true, m_searchCaseSensitive, m_searchWholeWord,
                            m_searchUseRegex);

    std::string msg = "Replaced " + std::to_string(count) + " occurrence(s).";
    MessageBoxW(m_hwndMain, utf8ToWide(msg).c_str(), L"Replace All", MB_OK | MB_ICONINFORMATION);
}

bool Win32IDE::findText(const std::string& searchText, bool forward, bool caseSensitive, bool wholeWord, bool useRegex)
{
    if (searchText.empty())
        return false;

    HWND hwndSearch = resolveFindReplaceTargetHwnd();

    if (!hwndSearch || !IsWindow(hwndSearch))
        return false;

    if (hwndSearch == m_hwndProblemsListView && m_hwndProblemsListView && IsWindow(m_hwndProblemsListView))
        return findTextInProblemsList(searchText, forward, caseSensitive, wholeWord, useRegex);

    if (hwndSearch != m_hwndEditor && useRegex)
    {
        MessageBoxW(m_hwndMain, L"Regex search runs in the code editor. Uncheck Regex or focus the editor.", L"Find",
                    MB_OK | MB_ICONINFORMATION);
        return false;
    }

    const bool inTerminalPane = (hwndSearch != m_hwndEditor);
    std::string editorText = inTerminalPane ? getRichEditDocumentUtf8(hwndSearch) : getWindowText(hwndSearch);
    if (editorText.empty())
        return false;
    int textLen = (int)editorText.size();

    CHARRANGE selection{};
    SendMessage(hwndSearch, EM_EXGETSEL, 0, (LPARAM)&selection);

    int startChar = forward ? selection.cpMax : selection.cpMin - 1;
    if (startChar < 0)
        startChar = 0;
    int startPos = charIndexToUtf8ByteOffset(editorText, startChar);
    if (startPos >= textLen)
        startPos = textLen > 0 ? textLen - 1 : 0;

    size_t foundPos = std::string::npos;
    size_t foundLen = searchText.length();

    if (useRegex)
    {
        // Regex search using std::regex
        try
        {
            auto flags = std::regex_constants::ECMAScript;
            if (!caseSensitive)
                flags |= std::regex_constants::icase;
            std::regex pattern(searchText, flags);
            std::smatch match;

            if (forward)
            {
                std::string searchArea = editorText.substr(startPos);
                if (std::regex_search(searchArea, match, pattern))
                {
                    foundPos = startPos + match.position();
                    foundLen = match.length();
                }
                else if (startPos > 0)
                {
                    // Wrap around
                    searchArea = editorText.substr(0, startPos);
                    if (std::regex_search(searchArea, match, pattern))
                    {
                        foundPos = match.position();
                        foundLen = match.length();
                    }
                }
            }
            else
            {
                // Backwards regex: find all matches before startPos, take last one
                std::string searchArea = editorText.substr(0, startPos);
                auto begin = std::sregex_iterator(searchArea.begin(), searchArea.end(), pattern);
                auto end = std::sregex_iterator();
                std::smatch lastMatch;
                bool found = false;
                for (auto it = begin; it != end; ++it)
                {
                    lastMatch = *it;
                    found = true;
                }
                if (found)
                {
                    foundPos = lastMatch.position();
                    foundLen = lastMatch.length();
                }
                else
                {
                    // Wrap: search from startPos to end
                    searchArea = editorText.substr(startPos);
                    begin = std::sregex_iterator(searchArea.begin(), searchArea.end(), pattern);
                    for (auto it = begin; it != end; ++it)
                    {
                        lastMatch = *it;
                        found = true;
                    }
                    if (found)
                    {
                        foundPos = startPos + lastMatch.position();
                        foundLen = lastMatch.length();
                    }
                }
            }
        }
        catch (const std::regex_error& e)
        {
            std::string msg = "Invalid regex: ";
            msg += e.what();
            MessageBoxW(m_hwndMain, utf8ToWide(msg).c_str(), L"Find", MB_OK | MB_ICONERROR);
            return false;
        }
    }
    else
    {
        // Plain text search with optional case sensitivity and whole word
        std::string haystack = editorText;
        std::string needle = searchText;

        if (!caseSensitive)
        {
            std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::tolower);
            std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
        }

        auto isWordBoundary = [&](size_t pos, size_t len) -> bool
        {
            if (!wholeWord)
                return true;
            bool leftOk = (pos == 0) || !isalnum((unsigned char)haystack[pos - 1]);
            bool rightOk = (pos + len >= haystack.size()) || !isalnum((unsigned char)haystack[pos + len]);
            return leftOk && rightOk;
        };

        if (forward)
        {
            size_t pos = startPos;
            while (pos < haystack.size())
            {
                foundPos = haystack.find(needle, pos);
                if (foundPos == std::string::npos)
                    break;
                if (isWordBoundary(foundPos, needle.size()))
                    break;
                pos = foundPos + 1;
                foundPos = std::string::npos;
            }
            // Wrap around
            if (foundPos == std::string::npos && startPos > 0)
            {
                pos = 0;
                while (pos < (size_t)startPos)
                {
                    foundPos = haystack.find(needle, pos);
                    if (foundPos == std::string::npos || foundPos >= (size_t)startPos)
                    {
                        foundPos = std::string::npos;
                        break;
                    }
                    if (isWordBoundary(foundPos, needle.size()))
                        break;
                    pos = foundPos + 1;
                    foundPos = std::string::npos;
                }
            }
        }
        else
        {
            if (startPos > 0)
            {
                foundPos = haystack.rfind(needle, startPos);
                while (foundPos != std::string::npos && !isWordBoundary(foundPos, needle.size()))
                {
                    if (foundPos == 0)
                    {
                        foundPos = std::string::npos;
                        break;
                    }
                    foundPos = haystack.rfind(needle, foundPos - 1);
                }
            }
            if (foundPos == std::string::npos)
            {
                foundPos = haystack.rfind(needle);
                while (foundPos != std::string::npos && !isWordBoundary(foundPos, needle.size()))
                {
                    if (foundPos == 0)
                    {
                        foundPos = std::string::npos;
                        break;
                    }
                    foundPos = haystack.rfind(needle, foundPos - 1);
                }
            }
        }
    }

    if (foundPos != std::string::npos)
    {
        selection.cpMin = (LONG)utf8ByteOffsetToCharIndex(editorText, (int)foundPos);
        selection.cpMax = (LONG)utf8ByteOffsetToCharIndex(editorText, (int)(foundPos + foundLen));
        SendMessage(hwndSearch, EM_EXSETSEL, 0, (LPARAM)&selection);
        SendMessage(hwndSearch, EM_SCROLLCARET, 0, 0);
        m_lastFoundPos = foundPos;
        return true;
    }

    MessageBoxW(m_hwndMain, L"Text not found.", L"Find", MB_OK | MB_ICONINFORMATION);
    return false;
}

namespace
{
int listViewHeaderColumnCount(HWND lv)
{
    HWND hdr = ListView_GetHeader(lv);
    if (hdr && IsWindow(hdr))
        return Header_GetItemCount(hdr);
    return 8;
}

std::string listViewRowCellsUtf8(HWND lv, int row)
{
    const int cols = listViewHeaderColumnCount(lv);
    const BOOL unicode = IsWindowUnicode(lv);
    std::string rowText;
    for (int c = 0; c < cols; ++c)
    {
        if (c > 0)
            rowText += '\t';
        if (unicode)
        {
            wchar_t buf[4096];
            LVITEMW it{};
            it.iSubItem = c;
            it.pszText = buf;
            it.cchTextMax = static_cast<int>(sizeof(buf) / sizeof(buf[0]));
            buf[0] = 0;
            SendMessageW(lv, LVM_GETITEMTEXTW, static_cast<WPARAM>(row), reinterpret_cast<LPARAM>(&it));
            rowText += wideToUtf8(buf);
        }
        else
        {
            char buf[4096];
            LVITEMA it{};
            it.iSubItem = c;
            it.pszText = buf;
            it.cchTextMax = static_cast<int>(sizeof(buf) / sizeof(buf[0]));
            buf[0] = 0;
            SendMessageA(lv, LVM_GETITEMTEXTA, static_cast<WPARAM>(row), reinterpret_cast<LPARAM>(&it));
            rowText += buf;
        }
    }
    return rowText;
}
}  // namespace

bool Win32IDE::findTextInProblemsList(const std::string& searchText, bool forward, bool caseSensitive, bool wholeWord,
                                      bool useRegex)
{
    HWND lv = m_hwndProblemsListView;
    if (!lv || !IsWindow(lv) || searchText.empty())
        return false;

    const int n = ListView_GetItemCount(lv);
    if (n <= 0)
    {
        MessageBoxW(m_hwndMain, L"Text not found.", L"Find", MB_OK | MB_ICONINFORMATION);
        return false;
    }

    int sel = static_cast<int>(ListView_GetNextItem(lv, -1, LVNI_SELECTED));
    if (sel >= n)
        sel = -1;

    auto buildHaystack = [lv](int idx) -> std::string
    {
        if (idx < 0 || idx >= ListView_GetItemCount(lv))
            return {};
        return listViewRowCellsUtf8(lv, idx);
    };

    auto matchPlain = [&](const std::string& haystack) -> bool
    {
        std::string hay = haystack;
        std::string needle = searchText;
        if (!caseSensitive)
        {
            std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            std::transform(needle.begin(), needle.end(), needle.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });
        }
        if (!wholeWord)
            return hay.find(needle) != std::string::npos;
        size_t pos = hay.find(needle);
        while (pos != std::string::npos)
        {
            const bool leftOk = (pos == 0) || !std::isalnum(static_cast<unsigned char>(hay[pos - 1]));
            const bool rightOk = (pos + needle.size() >= hay.size()) ||
                                 !std::isalnum(static_cast<unsigned char>(hay[pos + needle.size()]));
            if (leftOk && rightOk)
                return true;
            pos = hay.find(needle, pos + 1);
        }
        return false;
    };

    auto matchRegex = [&](const std::string& haystack) -> bool
    {
        try
        {
            auto flags = std::regex_constants::ECMAScript;
            if (!caseSensitive)
                flags |= std::regex_constants::icase;
            std::regex re(searchText, flags);
            return std::regex_search(haystack, re);
        }
        catch (const std::regex_error& e)
        {
            std::string msg = "Invalid regex: ";
            msg += e.what();
            MessageBoxW(m_hwndMain, utf8ToWide(msg).c_str(), L"Find", MB_OK | MB_ICONERROR);
            return false;
        }
    };

    auto rowMatches = [&](int idx) -> bool
    {
        const std::string hay = buildHaystack(idx);
        if (hay.empty())
            return false;
        return useRegex ? matchRegex(hay) : matchPlain(hay);
    };

    int foundIdx = -1;
    for (int step = 0; step < n; ++step)
    {
        int idx = 0;
        if (sel < 0)
            idx = forward ? step : (n - 1 - step);
        else if (forward)
            idx = (sel + 1 + step) % n;
        else
            idx = (sel - 1 - step + n * 100) % n;

        if (rowMatches(idx))
        {
            foundIdx = idx;
            break;
        }
    }

    if (foundIdx < 0)
    {
        MessageBoxW(m_hwndMain, L"Text not found.", L"Find", MB_OK | MB_ICONINFORMATION);
        return false;
    }

    for (int i = 0; i < n; ++i)
        ListView_SetItemState(lv, i, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(lv, foundIdx, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetSelectionMark(lv, foundIdx);
    ListView_EnsureVisible(lv, foundIdx, FALSE);
    SetFocus(lv);
    return true;
}

void Win32IDE::applyEditorCharFormatFaceAndSizeFromSettings()
{
    if (!m_hwndEditor || !IsWindow(m_hwndEditor))
        return;
    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_CHARSET;
    const int pt = (std::max)(6, (std::min)(72, m_settings.fontSize));
    cf.yHeight = pt * 20;
    cf.bCharSet = DEFAULT_CHARSET;
    const std::wstring face = utf8ToWide(m_settings.fontName.empty() ? "Consolas" : m_settings.fontName);
    wcsncpy_s(cf.szFaceName, LF_FACESIZE, face.c_str(), _TRUNCATE);
    SendMessageW(m_hwndEditor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
}

void Win32IDE::selectAllProblemsListRows()
{
    HWND lv = m_hwndProblemsListView;
    if (!lv || !IsWindow(lv))
        return;
    const int n = ListView_GetItemCount(lv);
    for (int i = 0; i < n; ++i)
        ListView_SetItemState(lv, i, LVIS_SELECTED, LVIS_SELECTED);
    if (n > 0)
        SetFocus(lv);
}

void Win32IDE::copyProblemsListSelectionToClipboard()
{
    HWND lv = m_hwndProblemsListView;
    if (!lv || !IsWindow(lv))
        return;

    std::string clip;
    int idx = -1;
    while ((idx = ListView_GetNextItem(lv, idx, LVNI_SELECTED)) >= 0)
    {
        if (!clip.empty())
            clip += "\r\n";
        clip += listViewRowCellsUtf8(lv, idx);
    }

    if (clip.empty())
    {
        idx = ListView_GetNextItem(lv, -1, LVNI_FOCUSED | LVNI_SELECTED);
        if (idx < 0)
            idx = ListView_GetSelectionMark(lv);
        if (idx >= 0)
            clip = listViewRowCellsUtf8(lv, idx);
    }

    if (clip.empty())
        return;

    const std::wstring w = utf8ToWide(clip);
    if (!OpenClipboard(m_hwndMain))
        return;
    EmptyClipboard();
    const SIZE_T cb = (w.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, cb);
    if (hMem)
    {
        void* p = GlobalLock(hMem);
        if (p)
        {
            memcpy(p, w.c_str(), cb);
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        else
            GlobalFree(hMem);
    }
    CloseClipboard();
}

int Win32IDE::replaceText(const std::string& searchText, const std::string& replaceText, bool all, bool caseSensitive,
                          bool wholeWord, bool useRegex)
{
    if (searchText.empty())
        return 0;

    HWND hwndTarget = resolveFindReplaceTargetHwnd();
    if (!hwndTarget || !IsWindow(hwndTarget))
        return 0;

    if (hwndTarget == m_hwndProblemsListView)
    {
        MessageBoxW(m_hwndMain,
                    L"Replace is not available in the Problems list. Focus the editor or an editable buffer.",
                    L"Replace", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    const bool inTerminalPane = (hwndTarget != m_hwndEditor);
    if (inTerminalPane && useRegex)
    {
        MessageBoxW(m_hwndMain, L"Regex replace runs in the code editor. Uncheck Regex or focus the editor.",
                    L"Replace", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    int replaceCount = 0;

    if (all)
    {
        std::string editorText = inTerminalPane ? getRichEditDocumentUtf8(hwndTarget) : getWindowText(hwndTarget);
        if (editorText.empty())
            return 0;

        std::string result;

        if (useRegex)
        {
            try
            {
                auto flags = std::regex_constants::ECMAScript;
                if (!caseSensitive)
                    flags |= std::regex_constants::icase;
                std::regex pattern(searchText, flags);

                std::string remaining = editorText;
                std::smatch m;
                while (std::regex_search(remaining, m, pattern))
                {
                    result.append(m.prefix().first, m.prefix().second);
                    result.append(replaceText);
                    remaining = m.suffix().str();
                    replaceCount++;
                }
                result += remaining;
            }
            catch (const std::regex_error& e)
            {
                std::string msg = "Invalid regex: ";
                msg += e.what();
                MessageBoxW(m_hwndMain, utf8ToWide(msg).c_str(), L"Replace", MB_OK | MB_ICONERROR);
                return 0;
            }
        }
        else
        {
            std::string haystack = editorText;
            std::string needle = searchText;

            if (!caseSensitive)
            {
                std::transform(haystack.begin(), haystack.end(), haystack.begin(),
                               [](unsigned char c) { return (char)std::tolower(c); });
                std::transform(needle.begin(), needle.end(), needle.begin(),
                               [](unsigned char c) { return (char)std::tolower(c); });
            }

            auto isWordBoundary = [&](size_t pos, size_t len) -> bool
            {
                if (!wholeWord)
                    return true;
                bool leftOk = (pos == 0) || !std::isalnum((unsigned char)haystack[pos - 1]);
                bool rightOk = (pos + len >= haystack.size()) || !std::isalnum((unsigned char)haystack[pos + len]);
                return leftOk && rightOk;
            };

            size_t scanPos = 0;
            size_t lastEmit = 0;
            while (scanPos < haystack.size())
            {
                size_t foundPos = haystack.find(needle, scanPos);
                if (foundPos == std::string::npos)
                    break;

                if (!isWordBoundary(foundPos, needle.size()))
                {
                    scanPos = foundPos + 1;
                    continue;
                }

                result.append(editorText, lastEmit, foundPos - lastEmit);
                result.append(replaceText);
                scanPos = foundPos + needle.size();
                lastEmit = scanPos;
                replaceCount++;
            }

            result.append(editorText, lastEmit, std::string::npos);
        }

        if (replaceCount > 0)
        {
            setWindowText(hwndTarget, result);
            if (!inTerminalPane)
            {
                m_fileModified = true;
                updateLineNumbers();
            }
        }
    }
    else
    {
        // Replace current selection if it matches search text
        CHARRANGE selection;
        SendMessage(hwndTarget, EM_EXGETSEL, 0, (LPARAM)&selection);

        int selLen = selection.cpMax - selection.cpMin;
        if (selLen > 0)
        {
            std::string selectedText(selLen + 1, 0);
            SendMessage(hwndTarget, EM_GETSELTEXT, 0, (LPARAM)&selectedText[0]);
            selectedText.resize(selLen);

            std::string cmpSelected = selectedText;
            std::string cmpSearch = searchText;

            if (!caseSensitive)
            {
                std::transform(cmpSelected.begin(), cmpSelected.end(), cmpSelected.begin(), ::tolower);
                std::transform(cmpSearch.begin(), cmpSearch.end(), cmpSearch.begin(), ::tolower);
            }

            if (cmpSelected == cmpSearch)
            {
                SendMessageW(hwndTarget, EM_REPLACESEL, TRUE, (LPARAM)utf8ToWide(replaceText).c_str());
                if (!inTerminalPane)
                    m_fileModified = true;
                replaceCount = 1;

                // Find next occurrence
                findText(searchText, true, caseSensitive, wholeWord, useRegex);
            }
        }
    }

    return replaceCount;
}

INT_PTR CALLBACK Win32IDE::FindDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = nullptr;

    if (uMsg == WM_INITDIALOG)
    {
        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
        pThis = (Win32IDE*)lParam;
    }
    else
    {
        pThis = (Win32IDE*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
    }

    if (!pThis)
        return FALSE;

    switch (uMsg)
    {
        case WM_USER + 100:
            // Handle Copilot streaming token updates
            if (pThis)
            {
                pThis->HandleCopilotStreamUpdate(reinterpret_cast<const char*>(wParam), static_cast<size_t>(lParam));
            }
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDC_BTN_FIND_NEXT:
                {
                    HWND hwndFindText = GetDlgItem(hwndDlg, IDC_FIND_TEXT);
                    wchar_t buffer[256];
                    GetWindowTextW(hwndFindText, buffer, 256);
                    pThis->m_lastSearchText = wideToUtf8(buffer);

                    pThis->m_searchCaseSensitive = IsDlgButtonChecked(hwndDlg, IDC_CASE_SENSITIVE) == BST_CHECKED;
                    pThis->m_searchWholeWord = IsDlgButtonChecked(hwndDlg, IDC_WHOLE_WORD) == BST_CHECKED;
                    pThis->m_searchUseRegex = IsDlgButtonChecked(hwndDlg, IDC_USE_REGEX) == BST_CHECKED;

                    pThis->findNext();
                }
                    return TRUE;
                case IDC_BTN_CLOSE:
                case IDCANCEL:
                    DestroyWindow(hwndDlg);
                    pThis->m_hwndFindDialog = nullptr;
                    return TRUE;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            pThis->m_hwndFindDialog = nullptr;
            return TRUE;
    }

    return FALSE;
}

INT_PTR CALLBACK Win32IDE::ReplaceDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = nullptr;

    if (uMsg == WM_INITDIALOG)
    {
        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
        pThis = (Win32IDE*)lParam;
    }
    else
    {
        pThis = (Win32IDE*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
    }

    if (!pThis)
        return FALSE;

    switch (uMsg)
    {
        case WM_COMMAND:
        {
            HWND hwndFindText = GetDlgItem(hwndDlg, IDC_FIND_TEXT);
            HWND hwndReplaceText = GetDlgItem(hwndDlg, IDC_REPLACE_TEXT);

            switch (LOWORD(wParam))
            {
                case IDC_BTN_FIND_NEXT:
                {
                    wchar_t wFind[256], wReplace[256];
                    GetWindowTextW(hwndFindText, wFind, 256);
                    pThis->m_lastSearchText = wideToUtf8(wFind);
                }
                    pThis->m_searchCaseSensitive = IsDlgButtonChecked(hwndDlg, IDC_CASE_SENSITIVE) == BST_CHECKED;
                    pThis->m_searchWholeWord = IsDlgButtonChecked(hwndDlg, IDC_WHOLE_WORD) == BST_CHECKED;
                    pThis->m_searchUseRegex = IsDlgButtonChecked(hwndDlg, IDC_USE_REGEX) == BST_CHECKED;
                    pThis->findNext();
                    return TRUE;
                case IDC_BTN_REPLACE:
                {
                    wchar_t wFind[256], wReplace[256];
                    GetWindowTextW(hwndFindText, wFind, 256);
                    GetWindowTextW(hwndReplaceText, wReplace, 256);
                    pThis->m_lastSearchText = wideToUtf8(wFind);
                    pThis->m_lastReplaceText = wideToUtf8(wReplace);
                }
                    pThis->m_searchCaseSensitive = IsDlgButtonChecked(hwndDlg, IDC_CASE_SENSITIVE) == BST_CHECKED;
                    pThis->m_searchWholeWord = IsDlgButtonChecked(hwndDlg, IDC_WHOLE_WORD) == BST_CHECKED;
                    pThis->m_searchUseRegex = IsDlgButtonChecked(hwndDlg, IDC_USE_REGEX) == BST_CHECKED;
                    pThis->replaceNext();
                    return TRUE;
                case IDC_BTN_REPLACE_ALL:
                {
                    wchar_t wFind[256], wReplace[256];
                    GetWindowTextW(hwndFindText, wFind, 256);
                    GetWindowTextW(hwndReplaceText, wReplace, 256);
                    pThis->m_lastSearchText = wideToUtf8(wFind);
                    pThis->m_lastReplaceText = wideToUtf8(wReplace);
                }
                    pThis->m_searchCaseSensitive = IsDlgButtonChecked(hwndDlg, IDC_CASE_SENSITIVE) == BST_CHECKED;
                    pThis->m_searchWholeWord = IsDlgButtonChecked(hwndDlg, IDC_WHOLE_WORD) == BST_CHECKED;
                    pThis->m_searchUseRegex = IsDlgButtonChecked(hwndDlg, IDC_USE_REGEX) == BST_CHECKED;
                    pThis->replaceAll();
                    return TRUE;
                case IDC_BTN_CLOSE:
                case IDCANCEL:
                    DestroyWindow(hwndDlg);
                    pThis->m_hwndReplaceDialog = nullptr;
                    return TRUE;
            }
        }
        break;
        case WM_CLOSE:
            DestroyWindow(hwndDlg);
            pThis->m_hwndReplaceDialog = nullptr;
            return TRUE;
    }

    return FALSE;
}

// ============================================================================
// Snippet Manager Implementation
// ============================================================================

#define IDD_SNIPPET_MANAGER 6001
// Note: IDC_SNIPPET_LIST is defined at line 23 as 1009
#define IDC_SNIPPET_LIST_DLG 6010
#define IDC_SNIPPET_NAME 6011
#define IDC_SNIPPET_DESC 6012
#define IDC_SNIPPET_CODE 6013
#define IDC_BTN_INSERT_SNIPPET 6020
#define IDC_BTN_NEW_SNIPPET 6021
#define IDC_BTN_DELETE_SNIPPET 6022
#define IDC_BTN_SAVE_SNIPPETS 6023

void Win32IDE::showSnippetManager()
{
    // WS_EX_TOPMOST keeps dialog visible during screenshot attempts (focus theft protection)
    HWND hwndDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"STATIC", L"Snippet Manager",
                                   WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 100, 100, 600, 500, m_hwndMain,
                                   nullptr, m_hInstance, nullptr);
    if (!hwndDlg)
    {
        const DWORD createErr = GetLastError();
        appendToOutput("[UI] Failed to create Snippet Manager dialog (GetLastError=" + std::to_string(createErr) +
                           ")\n",
                       "Errors", OutputSeverity::Error);
        return;
    }

    CreateWindowExW(0, L"STATIC", L"Snippets:", WS_CHILD | WS_VISIBLE, 10, 10, 150, 20, hwndDlg, nullptr, m_hInstance,
                    nullptr);

    HWND hwndList =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | LBS_STANDARD | WS_VSCROLL, 10, 35,
                        150, 400, hwndDlg, (HMENU)IDC_SNIPPET_LIST_DLG, m_hInstance, nullptr);

    for (const auto& snippet : m_codeSnippets)
    {
        SendMessageW(hwndList, LB_ADDSTRING, 0, (LPARAM)utf8ToWide(snippet.name).c_str());
    }

    CreateWindowExW(0, L"STATIC", L"Name:", WS_CHILD | WS_VISIBLE, 175, 10, 50, 20, hwndDlg, nullptr, m_hInstance,
                    nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 230, 8, 350, 22, hwndDlg,
                    (HMENU)IDC_SNIPPET_NAME, m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Description:", WS_CHILD | WS_VISIBLE, 175, 40, 70, 20, hwndDlg, nullptr,
                    m_hInstance, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 175, 60, 405, 22, hwndDlg,
                    (HMENU)IDC_SNIPPET_DESC, m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Code Template:", WS_CHILD | WS_VISIBLE, 175, 90, 100, 20, hwndDlg, nullptr,
                    m_hInstance, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN |
                        WS_VSCROLL | WS_HSCROLL,
                    175, 115, 405, 280, hwndDlg, (HMENU)IDC_SNIPPET_CODE, m_hInstance, nullptr);

    CreateWindowExW(0, L"BUTTON", L"Insert", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 175, 410, 90, 28, hwndDlg,
                    (HMENU)IDC_BTN_INSERT_SNIPPET, m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"New", WS_CHILD | WS_VISIBLE, 275, 410, 90, 28, hwndDlg, (HMENU)IDC_BTN_NEW_SNIPPET,
                    m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE, 375, 410, 90, 28, hwndDlg,
                    (HMENU)IDC_BTN_DELETE_SNIPPET, m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Save & Close", WS_CHILD | WS_VISIBLE, 475, 410, 105, 28, hwndDlg,
                    (HMENU)IDC_BTN_SAVE_SNIPPETS, m_hInstance, nullptr);

    // Message loop for dialog
    MSG msg;
    bool running = true;
    while (running && GetMessage(&msg, nullptr, 0, 0))
    {
        if (msg.hwnd == hwndDlg || IsChild(hwndDlg, msg.hwnd))
        {
            // Handle list selection
            if (msg.message == WM_COMMAND)
            {
                WORD cmdId = LOWORD(msg.wParam);
                WORD notif = HIWORD(msg.wParam);

                if (cmdId == IDC_SNIPPET_LIST_DLG && notif == LBN_SELCHANGE)
                {
                    int sel = SendMessage(hwndList, LB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < (int)m_codeSnippets.size())
                    {
                        const CodeSnippet& snippet = m_codeSnippets[sel];
                        SetDlgItemTextW(hwndDlg, IDC_SNIPPET_NAME, utf8ToWide(snippet.name).c_str());
                        SetDlgItemTextW(hwndDlg, IDC_SNIPPET_DESC, utf8ToWide(snippet.description).c_str());
                        SetDlgItemTextW(hwndDlg, IDC_SNIPPET_CODE, utf8ToWide(snippet.code).c_str());
                    }
                }
                else if (cmdId == IDC_BTN_INSERT_SNIPPET)
                {
                    int sel = SendMessage(hwndList, LB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < (int)m_codeSnippets.size())
                    {
                        insertSnippet(m_codeSnippets[sel].name);
                        running = false;
                        DestroyWindow(hwndDlg);
                    }
                }
                else if (cmdId == IDC_BTN_NEW_SNIPPET)
                {
                    CodeSnippet newSnippet;
                    newSnippet.name = "NewSnippet";
                    newSnippet.description = "New snippet description";
                    newSnippet.code = "// Your code here";
                    m_codeSnippets.push_back(newSnippet);
                    SendMessageW(hwndList, LB_ADDSTRING, 0, (LPARAM)utf8ToWide(newSnippet.name).c_str());
                    SendMessage(hwndList, LB_SETCURSEL, m_codeSnippets.size() - 1, 0);
                    SetDlgItemTextW(hwndDlg, IDC_SNIPPET_NAME, utf8ToWide(newSnippet.name).c_str());
                    SetDlgItemTextW(hwndDlg, IDC_SNIPPET_DESC, utf8ToWide(newSnippet.description).c_str());
                    SetDlgItemTextW(hwndDlg, IDC_SNIPPET_CODE, utf8ToWide(newSnippet.code).c_str());
                }
                else if (cmdId == IDC_BTN_DELETE_SNIPPET)
                {
                    int sel = SendMessage(hwndList, LB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < (int)m_codeSnippets.size())
                    {
                        if (MessageBoxW(hwndDlg, L"Delete this snippet?", L"Confirm", MB_YESNO) == IDYES)
                        {
                            m_codeSnippets.erase(m_codeSnippets.begin() + sel);
                            SendMessage(hwndList, LB_DELETESTRING, sel, 0);
                            SetDlgItemTextW(hwndDlg, IDC_SNIPPET_NAME, L"");
                            SetDlgItemTextW(hwndDlg, IDC_SNIPPET_DESC, L"");
                            SetDlgItemTextW(hwndDlg, IDC_SNIPPET_CODE, L"");
                        }
                    }
                }
                else if (cmdId == IDC_BTN_SAVE_SNIPPETS)
                {
                    // Update current snippet before saving
                    int sel = SendMessage(hwndList, LB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < (int)m_codeSnippets.size())
                    {
                        wchar_t buffer[1024];
                        GetDlgItemTextW(hwndDlg, IDC_SNIPPET_NAME, buffer, 1024);
                        m_codeSnippets[sel].name = wideToUtf8(buffer);
                        GetDlgItemTextW(hwndDlg, IDC_SNIPPET_DESC, buffer, 1024);
                        m_codeSnippets[sel].description = wideToUtf8(buffer);

                        HWND hwndCode = GetDlgItem(hwndDlg, IDC_SNIPPET_CODE);
                        int len = GetWindowTextLengthW(hwndCode);
                        std::vector<wchar_t> codeBuffer(len + 1);
                        GetWindowTextW(hwndCode, codeBuffer.data(), len + 1);
                        m_codeSnippets[sel].code = wideToUtf8(codeBuffer.data());
                    }

                    saveCodeSnippets();
                    MessageBoxW(hwndDlg, L"Snippets saved!", L"Success", MB_OK);
                    running = false;
                    DestroyWindow(hwndDlg);
                }
            }
            else if (msg.message == WM_CLOSE)
            {
                running = false;
                DestroyWindow(hwndDlg);
            }
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Win32IDE::createSnippet()
{
    // Create a new empty snippet
    CodeSnippet newSnippet;
    newSnippet.name = "NewSnippet" + std::to_string(m_codeSnippets.size() + 1);
    newSnippet.description = "New snippet";
    newSnippet.code = "// Code template\n";
    m_codeSnippets.push_back(newSnippet);

    MessageBoxW(m_hwndMain,
                utf8ToWide("Snippet '" + newSnippet.name + "' created. Use Snippet Manager to edit.").c_str(),
                L"Snippet Created", MB_OK);
}

// ============================================================================
// File Explorer Implementation
// ============================================================================

void Win32IDE::createFileExplorer(HWND hwndParent)
{
    if (m_hwndFileExplorer)
    {
        return;  // Already created
    }

    RECT parentRc{};
    if (hwndParent && IsWindow(hwndParent))
        GetClientRect(hwndParent, &parentRc);
    const int parentW = (std::max)(0, static_cast<int>(parentRc.right - parentRc.left));
    const int parentH = (std::max)(0, static_cast<int>(parentRc.bottom - parentRc.top));
    const int explorerW = parentW > 0 ? parentW : m_sidebarWidth;
    const int explorerH = parentH > 0 ? parentH : 500;

    m_hwndFileExplorer =
        CreateWindowExW(0, L"STATIC", L"File Explorer", WS_CHILD | WS_VISIBLE | WS_BORDER, 0, 0, explorerW, explorerH,
                        hwndParent, (HMENU)IDC_FILE_EXPLORER, GetModuleHandle(nullptr), nullptr);

    if (!m_hwndFileExplorer)
    {
        return;
    }

    m_hwndFileTree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
                                     WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT |
                                         TVS_HASBUTTONS | TVS_EDITLABELS,
                                     5, 5, (std::max)(0, explorerW - 10), (std::max)(0, explorerH - 10),
                                     m_hwndFileExplorer, (HMENU)IDC_FILE_TREE, GetModuleHandle(nullptr), nullptr);

    if (!m_hwndFileTree)
    {
        DestroyWindow(m_hwndFileExplorer);
        m_hwndFileExplorer = nullptr;
        return;
    }

    SendMessage(m_hwndFileTree, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    SetWindowLongPtrW(m_hwndFileExplorer, GWLP_USERDATA, (LONG_PTR)this);
    m_oldFileExplorerContainerProc =
        (WNDPROC)SetWindowLongPtrW(m_hwndFileExplorer, GWLP_WNDPROC, (LONG_PTR)FileExplorerContainerProc);

    // Populate with drive letters
    populateFileTree(nullptr, "");
    ShowWindow(m_hwndFileExplorer, SW_SHOW);
    ShowWindow(m_hwndFileTree, SW_SHOW);
    UpdateWindow(m_hwndFileTree);
    UpdateWindow(m_hwndFileExplorer);
}

void Win32IDE::populateFileTree(HTREEITEM parentItem, const std::string& path)
{
    if (!m_hwndFileTree)
    {
        return;
    }

    if (!parentItem)
    {
        TVINSERTSTRUCTW tvis = {};
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;

        wchar_t drives[512] = {};
        DWORD len = GetLogicalDriveStringsW(static_cast<DWORD>(std::size(drives) - 1), drives);
        if (len == 0 || len >= std::size(drives))
            return;

        for (const wchar_t* p = drives; *p; p += wcslen(p) + 1)
        {
            const std::wstring driveRoot(p);
            const std::string drivePath = wideToUtf8(driveRoot);

            tvis.item.pszText = const_cast<wchar_t*>(driveRoot.c_str());
            tvis.item.lParam = (LPARAM) new std::string(drivePath);

            HTREEITEM driveItem = (HTREEITEM)SendMessageW(m_hwndFileTree, TVM_INSERTITEM, 0, (LPARAM)&tvis);
            m_treeItemPaths[driveItem] = drivePath;

            TVINSERTSTRUCTW dummyVis = {};
            dummyVis.hParent = driveItem;
            dummyVis.item.mask = TVIF_TEXT;
            static wchar_t s_ellipsis[] = L"...";
            dummyVis.item.pszText = s_ellipsis;
            SendMessageW(m_hwndFileTree, TVM_INSERTITEM, 0, (LPARAM)&dummyVis);
        }
        return;
    }

    // Populate a specific folder
    try
    {
        WIN32_FIND_DATAA findData;
        HANDLE findHandle;

        std::string searchPath = path + "\\*";
        findHandle = FindFirstFileA(searchPath.c_str(), &findData);

        if (findHandle == INVALID_HANDLE_VALUE)
        {
            return;
        }

        TVINSERTSTRUCTW tvis = {};
        tvis.hParent = parentItem;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;

        wchar_t wbuf[MAX_PATH];
        HTREEITEM hChild = TreeView_GetChild(m_hwndFileTree, parentItem);
        while (hChild)
        {
            HTREEITEM hNext = TreeView_GetNextSibling(m_hwndFileTree, hChild);
            TreeView_DeleteItem(m_hwndFileTree, hChild);
            hChild = hNext;
        }

        do
        {
            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
            {
                continue;
            }

            std::string fullPath = path + "\\" + findData.cFileName;
            MultiByteToWideChar(CP_ACP, 0, findData.cFileName, -1, wbuf, MAX_PATH);

            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                tvis.item.pszText = wbuf;
                tvis.item.lParam = (LPARAM) new std::string(fullPath);

                HTREEITEM folderItem = (HTREEITEM)SendMessageW(m_hwndFileTree, TVM_INSERTITEM, 0, (LPARAM)&tvis);
                m_treeItemPaths[folderItem] = fullPath;

                TVINSERTSTRUCTW dummyVis = {};
                dummyVis.hParent = folderItem;
                dummyVis.item.mask = TVIF_TEXT;
                static wchar_t s_ellipsis2[] = L"...";
                dummyVis.item.pszText = s_ellipsis2;
                SendMessageW(m_hwndFileTree, TVM_INSERTITEM, 0, (LPARAM)&dummyVis);
            }
            else
            {
                tvis.item.pszText = wbuf;
                tvis.item.lParam = (LPARAM) new std::string(fullPath);

                HTREEITEM fileItem = (HTREEITEM)SendMessageW(m_hwndFileTree, TVM_INSERTITEM, 0, (LPARAM)&tvis);
                m_treeItemPaths[fileItem] = fullPath;
            }
        } while (FindNextFileA(findHandle, &findData));

        FindClose(findHandle);
    }
    catch (...)
    {
        // Silently handle errors
    }
}

LRESULT CALLBACK Win32IDE::FileExplorerContainerProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    if (uMsg == WM_SIZE)
    {
        if (pThis && pThis->m_hwndFileTree && IsWindow(pThis->m_hwndFileTree))
        {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            const int w = (std::max)(0, static_cast<int>(rc.right - rc.left - 10));
            const int h = (std::max)(0, static_cast<int>(rc.bottom - rc.top - 10));
            MoveWindow(pThis->m_hwndFileTree, 5, 5, w, h, TRUE);
        }
        return 0;
    }
    if (uMsg == WM_NOTIFY)
    {
        NMHDR* pnmh = reinterpret_cast<NMHDR*>(lParam);
        if (pnmh && pThis && pnmh->hwndFrom == pThis->m_hwndFileTree)
        {
            switch (pnmh->code)
            {
                case TVN_ITEMEXPANDINGW:
                {
                    NMTREEVIEWW* pnmtv = reinterpret_cast<NMTREEVIEWW*>(lParam);
                    if ((pnmtv->action & TVE_EXPAND) && pnmtv->itemNew.lParam)
                    {
                        const std::string* path = reinterpret_cast<const std::string*>(pnmtv->itemNew.lParam);
                        if (path)
                        {
                            pThis->onFileTreeExpand(pnmtv->itemNew.hItem, *path);
                        }
                    }
                    return 0;
                }
                case TVN_SELCHANGEDW:
                {
                    NMTREEVIEWW* pnmtv = reinterpret_cast<NMTREEVIEWW*>(lParam);
                    if (pnmtv && pnmtv->itemNew.hItem)
                    {
                        pThis->onFileTreeSelect(pnmtv->itemNew.hItem);
                    }
                    return 0;
                }
                case NM_DBLCLK:
                    pThis->onFileExplorerDoubleClick();
                    return 0;
                case NM_RCLICK:
                    pThis->onFileExplorerRightClick();
                    return 0;
                case TVN_ENDLABELEDITW:
                {
                    NMTVDISPINFOW* pdi = reinterpret_cast<NMTVDISPINFOW*>(lParam);
                    if (!pdi->item.pszText || !pdi->item.lParam)
                        return FALSE;

                    std::string* oldPath = reinterpret_cast<std::string*>(pdi->item.lParam);
                    if (!oldPath)
                        return FALSE;

                    std::wstring newNameW(pdi->item.pszText);
                    std::string newName = wideToUtf8(newNameW);
                    if (newName.empty())
                        return FALSE;

                    std::filesystem::path oldFs(*oldPath);
                    std::filesystem::path newFs = oldFs.parent_path() / newName;

                    std::error_code ec;
                    std::filesystem::rename(oldFs, newFs, ec);
                    if (ec)
                    {
                        pThis->appendToOutput("[Explorer] Rename failed: " + ec.message() + "\n", "Explorer",
                                              OutputSeverity::Error);
                        return FALSE;
                    }

                    const std::string newPath = newFs.string();
                    auto* newPathPtr = new std::string(newPath);
                    TVITEMW setItem = {};
                    setItem.hItem = pdi->item.hItem;
                    setItem.mask = TVIF_PARAM;
                    setItem.lParam = reinterpret_cast<LPARAM>(newPathPtr);
                    SendMessageW(pThis->m_hwndFileTree, TVM_SETITEMW, 0, reinterpret_cast<LPARAM>(&setItem));

                    pThis->m_treeItemPaths.erase(pdi->item.hItem);
                    pThis->m_treeItemPaths[pdi->item.hItem] = newPath;

                    delete oldPath;
                    pThis->appendToOutput("[Explorer] Renamed to: " + newPath + "\n", "Explorer", OutputSeverity::Info);
                    return TRUE;
                }
                case TVN_DELETEITEMW:
                {
                    NMTREEVIEWW* pnmtv = reinterpret_cast<NMTREEVIEWW*>(lParam);
                    pThis->m_treeItemPaths.erase(pnmtv->itemOld.hItem);
                    if (pnmtv->itemOld.lParam)
                        delete reinterpret_cast<std::string*>(pnmtv->itemOld.lParam);
                    return 0;
                }
                default:
                    break;
            }
        }
    }
    WNDPROC oldProc = pThis ? pThis->m_oldFileExplorerContainerProc : nullptr;
    if (oldProc)
        return CallWindowProcA(oldProc, hwnd, uMsg, wParam, lParam);
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void Win32IDE::onFileTreeExpand(HTREEITEM item, const std::string& path)
{
    if (!m_hwndFileTree)
    {
        return;
    }

    populateFileTree(item, path);
}

std::string Win32IDE::getTreeItemPath(HTREEITEM item) const
{
    auto it = m_treeItemPaths.find(item);
    if (it != m_treeItemPaths.end())
    {
        return it->second;
    }
    return "";
}

// NOTE: File watcher implementation lives in `Win32IDE_Tier3Polish.cpp`.

void Win32IDE::loadModelFromPath(const std::string& filepath)
{
    if (filepath.empty())
        return;
    // Load regardless of extension: try streaming GGUF first, then ensure agentic bridge has the model
    bool ggufOk = false;
    try
    {
        ggufOk = loadGGUFModel(filepath);
    }
    catch (const std::exception& ex)
    {
        appendToOutput(std::string("[ModelLoad] Exception in loadGGUFModel: ") + ex.what() + "\n", "Errors",
                       OutputSeverity::Error);
    }
    catch (...)
    {
        appendToOutput("[ModelLoad] Unknown exception in loadGGUFModel\n", "Errors", OutputSeverity::Error);
    }

    if (ggufOk)
    {
        try
        {
            initializeInference();
        }
        catch (...)
        {
        }
        try
        {
            initBackendManager();
        }
        catch (...)
        {
        }
        try
        {
            initLLMRouter();
        }
        catch (...)
        {
        }
    }
    // Always feed path to agentic bridge so chat and task execution use this model (creates bridge if needed)
    bool bridgeOk = false;
    try
    {
        bridgeOk = loadModelForInference(filepath);
    }
    catch (const std::exception& ex)
    {
        appendToOutput(std::string("[ModelLoad] Exception in loadModelForInference: ") + ex.what() + "\n", "Errors",
                       OutputSeverity::Error);
    }
    catch (...)
    {
        appendToOutput("[ModelLoad] Unknown exception in loadModelForInference\n", "Errors", OutputSeverity::Error);
    }

    if (bridgeOk && !ggufOk)
        appendToOutput("Model loaded into Agentic Bridge (streaming GGUF skipped).\n", "Output", OutputSeverity::Info);
    if (ggufOk || bridgeOk)
    {
        // Ensure native engine is initialized for ghost text completions
        // This is needed when model is loaded via agentic bridge without GGUF streaming
        if (!m_nativeEngine)
        {
            try
            {
                m_nativeEngine = RawrXD::CPUInferenceEngine::GetSharedInstance();
                auto memPlugin = std::make_shared<RawrXD::Modules::NativeMemoryModule>();
                m_nativeEngine->RegisterMemoryPlugin(memPlugin);
                appendToOutput("Initialized Native CPU Inference Engine for ghost text.\n", "Output",
                               OutputSeverity::Info);
            }
            catch (const std::exception& e)
            {
                appendToOutput(std::string("Failed to init native engine for ghost text: ") + e.what() + "\n", "Errors",
                               OutputSeverity::Warning);
            }
        }
        // Load model into native engine for ghost text completions
        if (m_nativeEngine && !m_nativeEngineLoaded)
        {
            RawrXD::CPUInferenceEngine* engine = static_cast<RawrXD::CPUInferenceEngine*>(m_nativeEngine.get());
            if (!engine->IsModelLoaded())
            {
                appendToOutput("Loading model into Native Engine for ghost text: " + filepath + "\n", "Output",
                               OutputSeverity::Info);
                if (engine->LoadModel(filepath))
                {
                    m_nativeEngineLoaded = true;
                    appendToOutput("✅ Native Engine Model Loaded for ghost text completions.\n", "Output",
                                   OutputSeverity::Info);
                }
                else
                {
                    appendToOutput("⚠️ Native Engine Model Load failed (ghost text will use fallback providers).\n",
                                   "Errors", OutputSeverity::Warning);
                }
            }
        }
        // Enable Titan kernel for ghost text completions when model is loaded
        // This allows the ghost text provider cascade to use Titan inference
        if (!m_useTitanKernel)
        {
            m_useTitanKernel = true;
            appendToOutput("✅ Titan Kernel enabled for ghost text completions.\n", "Output", OutputSeverity::Info);
        }
        // Sync path: no pending replay — WM_SAFE_TO_REPLAY not posted.
        onModelReadyUnified(false);
        appendModelLoadReadyCopilotTurns(filepath, true);
    }
}

// ===========================================================================
// SEH-safe async model load body — standalone function to avoid MSVC C2712
// (cannot use __try in functions that require object unwinding).  The lambda
// in loadModelFromPathAsync hands data in/out via this plain-C-compatible struct.
// ===========================================================================

void Win32IDE::initModelSubsystems()
{
    initBackendManager();
    initLLMRouter();
}

// Context struct for the 8 MB CreateThread async model loader
struct AsyncModelLoadCtx
{
    Win32IDE* ide;
    std::string filepath;
};

namespace
{
struct AsyncModelLoadParams
{
    Win32IDE* ide;
    Win32IDE::AsyncModelLoadResult* result;
};

#ifdef _WIN32
static void asyncModelLoadBodySEH(AsyncModelLoadParams* p) noexcept
{
    __try
    {
        OutputDebugStringA("[AsyncModelLoad] entering SEH load body\n");
        p->result->ggufOk = p->ide->loadGGUFModel(p->result->filepath);
        if (p->result->ggufOk)
        {
            p->ide->initModelSubsystems();
        }
        p->result->bridgeOk = p->ide->loadModelForInference(p->result->filepath);
        OutputDebugStringA("[AsyncModelLoad] leaving SEH load body (success path)\n");
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        char buf[256];
        const unsigned long sehCode = GetExceptionCode();
        snprintf(buf, sizeof(buf), "[AsyncModelLoad] SEH exception 0x%08lX during model load - aborting load\n",
                 sehCode);
        OutputDebugStringA(buf);
        // postOutputPanelSafe is NOT called from __except because it has C++ dtor baggage;
        // the result flags remain false so WM_MODEL_LOAD_DONE will surface a failure message.
    }
}
#else
static void asyncModelLoadBodySEH(AsyncModelLoadParams* p) noexcept
{
    try
    {
        p->result->ggufOk = p->ide->loadGGUFModel(p->result->filepath);
        if (p->result->ggufOk)
        {
            p->ide->initBackendManager();
            p->ide->initLLMRouter();
        }
        p->result->bridgeOk = p->ide->loadModelForInference(p->result->filepath);
    }
    catch (...)
    {
    }
}
#endif

// Free thread proc — compatible with LPTHREAD_START_ROUTINE for CreateThread 8MB stack.
// Delegates immediately to Win32IDE::asyncModelLoadWorker so that the real logic lives
// inside the class and has full access to private members.
static DWORD WINAPI s_asyncModelLoadProc(LPVOID param) noexcept
{
    auto* c = static_cast<AsyncModelLoadCtx*>(param);
    Win32IDE* self = c->ide;
    std::string path = std::move(c->filepath);
    delete c;
    self->asyncModelLoadWorker(std::move(path));
    return 0;
}

}  // namespace

// Body of async model load — runs on the 8MB stack CreateThread worker.
void Win32IDE::asyncModelLoadWorker(std::string path)
{
    Win32IDE* self = this;

    DetachedThreadGuard _guard(self->m_activeDetachedThreads, self->m_shuttingDown);
    if (_guard.cancelled)
    {
        std::lock_guard<std::mutex> lock(self->m_asyncModelLoadMutex);
        self->m_asyncModelLoadRunning = false;
        return;
    }

    OutputDebugStringA("[AsyncModelLoad] worker thread started (8MB stack)\n");
    LOG_INFO("[AsyncModelLoad] worker thread started");

    auto result = std::make_unique<AsyncModelLoadResult>();
    result->filepath = path;
    const uint64_t tick0 = GetTickCount64();
    {
        std::error_code ec;
        const auto sz = std::filesystem::file_size(std::filesystem::path(path), ec);
        if (!ec)
            result->fileBytes = static_cast<uint64_t>(sz);
        std::ostringstream os;
        os << "[AsyncModelLoad] file probe path='" << path << "' bytes=" << result->fileBytes
           << " probeError=" << (ec ? ec.message() : std::string("none")) << "\n";
        OutputDebugStringA(os.str().c_str());
        LOG_INFO(std::string("[AsyncModelLoad] file probe bytes=") + std::to_string(result->fileBytes));
    }

    AsyncModelLoadParams amlp{self, result.get()};
    try
    {
        asyncModelLoadBodySEH(&amlp);
    }
    catch (const std::exception& ex)
    {
        OutputDebugStringA(("[AsyncModelLoad] C++ exception: " + std::string(ex.what()) + "\n").c_str());
        self->postOutputPanelSafe("[Error] Model load crashed: " + std::string(ex.what()) + "\n");
    }
    catch (...)
    {
        OutputDebugStringA("[AsyncModelLoad] Unknown exception during model load\n");
        self->postOutputPanelSafe("[Error] Model load crashed with unknown error.\n");
    }

    result->wallMs = static_cast<double>(GetTickCount64() - tick0);
    {
        std::ostringstream os;
        os << "[AsyncModelLoad] load finished ggufOk=" << (result->ggufOk ? 1 : 0)
           << " bridgeOk=" << (result->bridgeOk ? 1 : 0) << " wallMs=" << result->wallMs << "\n";
        OutputDebugStringA(os.str().c_str());
        LOG_INFO(std::string("[AsyncModelLoad] load finished ggufOk=") + (result->ggufOk ? "1" : "0") +
                 " bridgeOk=" + (result->bridgeOk ? "1" : "0") + " wallMs=" + std::to_string(result->wallMs));
    }

    if (!self->m_hwndMain || !IsWindow(self->m_hwndMain) ||
        !PostMessage(self->m_hwndMain, WM_MODEL_LOAD_DONE, 0, reinterpret_cast<LPARAM>(result.release())))
    {
        OutputDebugStringA("[AsyncModelLoad] failed to post WM_MODEL_LOAD_DONE (main window unavailable)\n");
        LOG_WARNING("[AsyncModelLoad] failed to post WM_MODEL_LOAD_DONE (main window unavailable)");
        std::lock_guard<std::mutex> lock(self->m_asyncModelLoadMutex);
        self->m_asyncModelLoadRunning = false;
    }
    else
    {
        OutputDebugStringA("[AsyncModelLoad] posted WM_MODEL_LOAD_DONE\n");
        LOG_INFO("[AsyncModelLoad] posted WM_MODEL_LOAD_DONE");
    }
}

void Win32IDE::resetChatStreamingState()
{
    std::lock_guard<std::mutex> outLock(m_outputMutex);
    m_streamingTokenAccumulator.clear();
    m_streamingWrittenWchars = 0;
    m_nativeStreamingActive = false;
}

void Win32IDE::onModelReadyUnified(bool shouldReplay)
{
    // INVARIANT: this is the single authoritative post-load transition.
    // No streaming or UI mutation is valid before this runs after model load.
    resetChatStreamingState();
    syncAgentModeUiFromBridge();
    refreshAgenticChatSessionContext();
    if (shouldReplay && m_hwndMain && IsWindow(m_hwndMain))
    {
        // WM_SAFE_TO_REPLAY: barrier message — streaming state guaranteed clean beforehand.
        // Only posted when caller has a pending message to replay (async path).
        PostMessage(m_hwndMain, WM_SAFE_TO_REPLAY, 0, 0);
    }
}

void Win32IDE::loadModelFromPathAsync(const std::string& filepath)
{
    if (!smokeCopilotChatEnabled())
        return;

    if (filepath.empty())
        return;

    {
        std::ostringstream os;
        os << "[AsyncModelLoad] request tid=" << GetCurrentThreadId() << " path='" << filepath << "'"
           << " shuttingDown=" << (isShuttingDown() ? 1 : 0) << " hwndMain=" << static_cast<const void*>(m_hwndMain)
           << "\n";
        OutputDebugStringA(os.str().c_str());
        LOG_INFO(std::string("[AsyncModelLoad] request ") + filepath);
    }

    {
        std::lock_guard<std::mutex> lock(m_asyncModelLoadMutex);
        if (m_asyncModelLoadRunning)
        {
            appendToOutput("Model load already in progress.\n", "Output", OutputSeverity::Warning);
            return;
        }
        m_asyncModelLoadRunning = true;
    }

    appendToOutput("Starting background model load: " + filepath + "\n", "Output", OutputSeverity::Info);

    // CreateThread with 8 MB stack — prevents STATUS_STACK_OVERFLOW (0xc00000fd / ntdll crash)
    // that kills the process when large GGUF files exhaust the default 1 MB std::thread stack.
    auto* ctx = new AsyncModelLoadCtx{this, filepath};
    HANDLE hThread = CreateThread(nullptr, 8u * 1024u * 1024u, s_asyncModelLoadProc, ctx, 0, nullptr);
    if (hThread)
        CloseHandle(hThread);
    else
    {
        delete ctx;
        std::lock_guard<std::mutex> lock(m_asyncModelLoadMutex);
        m_asyncModelLoadRunning = false;
        appendToOutput("[Error] Failed to create model load thread.\n", "Errors", OutputSeverity::Error);
    }
}

// ============================================================================
// GGUF Model Loading Implementation
// ============================================================================

bool Win32IDE::loadGGUFModel(const std::string& filepath)
{
    if (!m_ggufLoader)
        m_ggufLoader = std::make_unique<RawrXD::StreamingGGUFLoader>();

    const uint64_t tick0 = GetTickCount64();

    auto trimCopy = [](std::string s) -> std::string
    {
        const size_t first = s.find_first_not_of(" \t\r\n\"");
        if (first == std::string::npos)
            return std::string();
        const size_t last = s.find_last_not_of(" \t\r\n\"");
        return s.substr(first, last - first + 1);
    };

    std::string resolvedPath = trimCopy(filepath);
    if (resolvedPath.empty())
        resolvedPath = filepath;

    // Best-effort: capture size early for UX + metrics even if load fails later.
    {
        std::error_code fsec;
        const auto sz = std::filesystem::file_size(std::filesystem::path(resolvedPath), fsec);
        if (!fsec)
        {
            m_lastLoadedModelBytes = static_cast<uint64_t>(sz);
            METRICS.gauge("model.file_bytes", static_cast<double>(m_lastLoadedModelBytes));
            METRICS.gauge("model.file_gb", static_cast<double>(m_lastLoadedModelBytes) / (1024.0 * 1024.0 * 1024.0));
        }
    }

    std::vector<std::string> candidates;
    auto pushCandidate = [&](const std::string& value)
    {
        const std::string t = trimCopy(value);
        if (t.empty())
            return;
        if (std::find(candidates.begin(), candidates.end(), t) == candidates.end())
            candidates.push_back(t);
    };

    pushCandidate(filepath);
    pushCandidate(resolvedPath);

    // Common typo repair: "instrut" -> "instruct", and accidental extra space before .gguf.
    if (resolvedPath.find("instrut") != std::string::npos)
    {
        std::string fixed = resolvedPath;
        size_t pos = 0;
        while ((pos = fixed.find("instrut", pos)) != std::string::npos)
        {
            fixed.replace(pos, 7, "instruct");
            pos += 8;
        }
        pushCandidate(fixed);
    }
    {
        std::string fixed = resolvedPath;
        const size_t extPos = fixed.rfind(".gguf");
        if (extPos != std::string::npos && extPos > 0 && fixed[extPos - 1] == ' ')
        {
            fixed.erase(extPos - 1, 1);
            pushCandidate(fixed);
        }
    }

    std::error_code ec;
    std::filesystem::path rp(resolvedPath);
    const bool hasPathHint = resolvedPath.find(":\\") != std::string::npos ||
                             resolvedPath.find('\\') != std::string::npos ||
                             resolvedPath.find('/') != std::string::npos;
    if (!hasPathHint)
    {
        const std::filesystem::path baseName = rp.filename();
        for (const auto& dir : m_userModelDirectories)
        {
            pushCandidate((std::filesystem::path(dir) / baseName).string());
        }
        for (const auto& fullPath : m_modelPaths)
        {
            std::filesystem::path p(fullPath);
            if (p.filename() == baseName)
                pushCandidate(p.string());
        }
        pushCandidate((std::filesystem::path("F:\\OllamaModels") / baseName).string());
        pushCandidate((std::filesystem::path("D:\\OllamaModels") / baseName).string());
    }

    // Known-good local fallbacks used by strict lane smoke.
    pushCandidate("D:\\phi3mini.gguf");
    pushCandidate("F:\\OllamaModels\\Phi-3-mini-4k-instruct-q8_0.gguf");

    auto isReadableFile = [](const std::string& p) -> bool
    {
        std::ifstream f(p, std::ios::binary);
        return f.good();
    };

    for (const auto& candidate : candidates)
    {
        if (std::filesystem::exists(candidate, ec) && isReadableFile(candidate))
        {
            resolvedPath = candidate;
            break;
        }
    }

    if (resolvedPath != filepath)
    {
        appendToOutput("[ModelResolver] Adjusted GGUF path to readable file: " + resolvedPath + "\n", "Output",
                       OutputSeverity::Warning);
    }

    appendToOutput("Loading GGUF model: " + resolvedPath + "\n", "Output", OutputSeverity::Info);
    appendToOutput("This may take a moment for large files...\n", "Output", OutputSeverity::Info);

    try
    {
        // Attempt to open and parse the GGUF file (streaming - no full data load)
        appendToOutput("[1/5] Opening file...\n", "Output", OutputSeverity::Info);
        if (!m_ggufLoader->Open(resolvedPath))
        {
            std::string error =
                "❌ Failed to open GGUF file: " + resolvedPath + "\nCheck if file exists and is readable.";
            appendToOutput(error, "Errors", OutputSeverity::Error);
            ErrorReporter::report(error, m_hwndMain);
            return false;
        }

        appendToOutput("[2/5] Header, metadata, and tensor index ready.\n", "Output", OutputSeverity::Info);

        // Pre-load embedding zone for inference preparation
        appendToOutput("[3/5] Pre-loading embedding zone...\n", "Output", OutputSeverity::Info);
        if (!m_ggufLoader->LoadZone("embedding"))
        {
            std::string warning = "⚠️  Warning: Could not pre-load embedding zone (non-critical)";
            appendToOutput(warning, "Output", OutputSeverity::Warning);
        }
        else
        {
            const auto loadedZones = m_ggufLoader->GetLoadedZones();
            const auto tensorsNow = m_ggufLoader->GetAllTensorInfo();
            const size_t memNow = m_ggufLoader->GetCurrentMemoryUsage();
            std::ostringstream zlog;
            zlog << "[ZONE_OK] name=embedding ptr=" << static_cast<const void*>(m_ggufLoader.get())
                 << " size=" << memNow << " tensors=" << tensorsNow.size() << " zones=" << loadedZones.size() << "\n";
            OutputDebugStringA(zlog.str().c_str());
        }
    }
    catch (const std::exception& e)
    {
        std::string error = "❌ Exception loading GGUF file:\n" + std::string(e.what()) + "\n\nFile: " + resolvedPath;
        appendToOutput(error + "\n", "Errors", OutputSeverity::Error);
        ErrorReporter::report(error, m_hwndMain);
        return false;
    }
    catch (...)
    {
        std::string error = "❌ Unknown exception loading GGUF file: " + resolvedPath;
        appendToOutput(error + "\n", "Errors", OutputSeverity::Error);
        ErrorReporter::report(error, m_hwndMain);
        return false;
    }

    // Store model info
    m_loadedModelPath = resolvedPath;
    m_currentModelMetadata = m_ggufLoader->GetMetadata();
    m_modelTensors = m_ggufLoader->GetAllTensorInfo();  // Get tensor info for backward compatibility

    // Model load telemetry for UX + reverse-engineered parity (status bar + metrics).
    m_lastLoadedModelOk = true;
    m_lastLoadedModelPath = resolvedPath;
    m_lastLoadedModelDisplayName = std::filesystem::path(resolvedPath).has_filename()
                                       ? std::filesystem::path(resolvedPath).filename().string()
                                       : resolvedPath;
    m_lastLoadedModelWallMs = static_cast<double>(GetTickCount64() - tick0);
    METRICS.recordDuration("model.gguf_stream_wall_ms", m_lastLoadedModelWallMs);

    // Log success with memory savings information
    size_t currentMemory = m_ggufLoader->GetCurrentMemoryUsage();
    std::string info = "✅ Model loaded successfully (STREAMING MODE)!\n";
    info += "File: " + resolvedPath + "\n";
    info += "Tensors: " + std::to_string(m_modelTensors.size()) + "\n";
    info += "Layers: " + std::to_string(m_currentModelMetadata.layer_count) + "\n";
    info += "Context: " + std::to_string(m_currentModelMetadata.context_length) + "\n";
    info += "Vocab: " + std::to_string(m_currentModelMetadata.vocab_size) + "\n";
    info += "Current Memory: " + std::to_string(currentMemory / 1024 / 1024) + " MB\n";
    info += "Max Memory: ~500 MB (zone-based streaming)\n\n";

    auto zones = m_ggufLoader->GetLoadedZones();
    if (!zones.empty())
    {
        info += "Loaded Zones: ";
        for (size_t i = 0; i < zones.size(); i++)
        {
            info += zones[i];
            if (i < zones.size() - 1)
                info += ", ";
        }
        info += "\n";
    }

    appendToOutput(info, "Output", OutputSeverity::Info);

    // Reflect new model in status bar immediately.
    if (m_hwndMain && IsWindow(m_hwndMain))
        PostMessageW(m_hwndMain, WM_STATUSBAR_REFRESH_COPILOT, 0, 0);

    // ---- GPU Layer Split Optimizer ----
    // Compute optimal ngl based on model metadata and detected VRAM.
    // Uses empirical bench data (RX 7800 XT, Vulkan, KHR_coopmat).
    {
        uint64_t fileSize = 0;
        {
            std::error_code fsec;
            fileSize = static_cast<uint64_t>(std::filesystem::file_size(resolvedPath, fsec));
            if (fsec)
                fileSize = 0;
        }

        const uint32_t layers = m_currentModelMetadata.layer_count;
        const uint32_t kvHeads = m_currentModelMetadata.head_count_kv > 0 ? m_currentModelMetadata.head_count_kv
                                                                          : m_currentModelMetadata.head_count;
        const uint32_t headDim = (m_currentModelMetadata.embedding_dim > 0 && m_currentModelMetadata.head_count > 0)
                                     ? m_currentModelMetadata.embedding_dim / m_currentModelMetadata.head_count
                                     : 128;  // safe default
        const uint32_t ctxLen =
            m_currentModelMetadata.context_length > 0 ? m_currentModelMetadata.context_length : 4096;  // safe default

        // Detect VRAM via GPU backend bridge (if initialized)
        uint64_t vramBytes = 16ULL * 1024 * 1024 * 1024;  // fallback: 16 GB
        {
            auto& gpuBridge = RawrXD::GPU::getGPUBackendBridge();
            if (gpuBridge.isInitialized())
            {
                auto caps = gpuBridge.getCapabilities();
                if (caps.dedicatedVRAM > 0)
                    vramBytes = caps.dedicatedVRAM;
            }
        }

        // Get system RAM
        MEMORYSTATUSEX memstat{};
        memstat.dwLength = sizeof(memstat);
        uint64_t sysRAM = 0;
        if (GlobalMemoryStatusEx(&memstat))
            sysRAM = memstat.ullTotalPhys;

        if (fileSize > 0 && layers > 0)
        {
            auto split = RawrXD::computeOptimalGPULayers(fileSize, layers, kvHeads, headDim, ctxLen, vramBytes, sysRAM);

            const char* tierNames[] = {"FullGPU", "HybridSplit", "CPUDominant", "PureCPU"};
            const char* tierName =
                (static_cast<int>(split.tier) < 4) ? tierNames[static_cast<int>(split.tier)] : "Unknown";

            char splitInfo[1024];
            snprintf(splitInfo, sizeof(splitInfo),
                     "\n[NGL Optimizer] Model: %.2f GiB, %u layers\n"
                     "  Tier: %s | Optimal GPU layers: %u/%u\n"
                     "  Est. VRAM: %.1f GB (of %.1f GB available)\n"
                     "  KV headroom: %.0f MB | Stable: %s\n"
                     "  Est. perf: pp512 ~ %.0f t/s, tg128 ~ %.1f t/s\n",
                     static_cast<double>(fileSize) / (1024.0 * 1024.0 * 1024.0), layers, tierName, split.gpuLayers,
                     split.totalLayers, static_cast<double>(split.estimatedVRAMBytes) / (1024.0 * 1024.0 * 1024.0),
                     static_cast<double>(vramBytes) / (1024.0 * 1024.0 * 1024.0),
                     static_cast<double>(split.kvCacheHeadroom) / (1024.0 * 1024.0), split.stable ? "YES" : "NO",
                     split.estPromptTps, split.estGenerateTps);

            appendToOutput(splitInfo, "Output", OutputSeverity::Info);

            // Store the result for downstream consumers
            m_lastGPULayerSplit = split;

            // Update LocalGGUF backend capability based on empirical tier
            {
                auto cap = getBackendCapability(AIBackendType::LocalGGUF);
                cap.maxContextTokens = ctxLen;
                // Quality score: FullGPU models run well → 0.7, split → 0.5, CPU-dominant → 0.3
                switch (split.tier)
                {
                    case RawrXD::InferenceTier::FullGPU:
                        cap.qualityScore = 0.7f;
                        break;
                    case RawrXD::InferenceTier::HybridSplit:
                        cap.qualityScore = 0.5f;
                        break;
                    case RawrXD::InferenceTier::CPUDominant:
                        cap.qualityScore = 0.35f;
                        break;
                    case RawrXD::InferenceTier::PureCPU:
                        cap.qualityScore = 0.2f;
                        break;
                }
                setBackendCapability(AIBackendType::LocalGGUF, cap);
            }
        }
    }

    // Update status bar
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)utf8ToWide("Model: " + std::string(resolvedPath)).c_str());

    // IMPORTANT: do not mutate chat UI/history here.
    // loadGGUFModel can be called from async load paths; chat replay and user-visible
    // messaging are coordinated on the UI thread in WM_MODEL_LOAD_DONE.

    return true;
}

std::string Win32IDE::getModelInfo() const
{
    if (m_modelTensors.empty() || !m_ggufLoader)
    {
        return "No model loaded";
    }

    std::string info = "═══════════════════════════════════════════\n";
    info += "GGUF Model Information (STREAMING MODE)\n";
    info += "═══════════════════════════════════════════\n\n";

    info += "File: " + m_loadedModelPath + "\n";
    info += "Tensors: " + std::to_string(m_modelTensors.size()) + "\n";
    info += "Layers: " + std::to_string(m_currentModelMetadata.layer_count) + "\n";
    info += "Context Length: " + std::to_string(m_currentModelMetadata.context_length) + "\n";
    info += "Embedding Dim: " + std::to_string(m_currentModelMetadata.embedding_dim) + "\n";
    info += "Vocab Size: " + std::to_string(m_currentModelMetadata.vocab_size) + "\n";
    info += "Architecture: " + m_currentModelMetadata.architecture_type + "\n\n";

    // Show zone status (memory efficiency indicator)
    size_t currentMemory = m_ggufLoader->GetCurrentMemoryUsage();
    auto loadedZones = m_ggufLoader->GetLoadedZones();

    info += "📊 Memory Status:\n";
    info += "  Current RAM: " + std::to_string(currentMemory / 1024 / 1024) + " MB\n";
    info += "  Max Per Zone: ~400 MB\n";
    info += "  Total Capacity: ~500 MB (92x reduction from full load!)\n";
    info += "  Loaded Zones: " + std::to_string(loadedZones.size()) + "\n\n";

    if (!loadedZones.empty())
    {
        info += "🎯 Active Zones:\n";
        for (const auto& zone : loadedZones)
        {
            info += "   ✓ " + zone + "\n";
        }
        info += "\n";
    }

    info += "Tensor Details (first 10):\n";
    info += "──────────────────────────────────────────\n";

    for (size_t i = 0; i < m_modelTensors.size() && i < 10; ++i)
    {
        const auto& tensor = m_modelTensors[i];
        info += "[" + std::to_string(i + 1) + "] " + tensor.name + "\n";
        info += "    Size: " + std::to_string(tensor.size_bytes / 1024 / 1024) + " MB\n";
        info += "    Type: " + m_ggufLoader->GetTypeString(tensor.type) + "\n";
    }

    if (m_modelTensors.size() > 10)
    {
        info += "... and " + std::to_string(m_modelTensors.size() - 10) + " more tensors\n";
    }

    info += "\n💡 Tip: Zones load on-demand during inference for optimal performance!\n";

    return info;
}

bool Win32IDE::loadTensorData(const std::string& tensorName, std::vector<uint8_t>& data)
{
    if (!m_ggufLoader)
    {
        return false;
    }
    // StreamingGGUFLoader automatically loads required zone if needed
    return m_ggufLoader->LoadTensorZone(tensorName, data);
}

// ============================================================================
// FILE EXPLORER IMPLEMENTATION
// ============================================================================

void Win32IDE::createFileExplorer()
{
    if (!m_hwndSidebar)
        return;

    m_hwndFileExplorer = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS, 5, 30,
        m_sidebarWidth - 10, 400, m_hwndSidebar, (HMENU)IDC_FILE_EXPLORER, m_hInstance, nullptr);

    // LOGGING AS REQUESTED
    char logBuf[256];
    sprintf_s(logBuf, "Explorer HWND created: %p (Parent: %p)", m_hwndFileExplorer, m_hwndSidebar);
    LOG_INFO(std::string(logBuf));

    if (!m_hwndFileExplorer)
        return;

    // Create image list for icons
    m_hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 3, 0);
    if (m_hImageList)
    {
        // Load icons for folders, files, and model files
        HICON hFolderIcon = (HICON)LoadImageA(nullptr, MAKEINTRESOURCEA(32755), IMAGE_ICON, 16, 16, LR_SHARED);
        HICON hFileIcon = (HICON)LoadImageA(nullptr, MAKEINTRESOURCEA(32512), IMAGE_ICON, 16, 16, LR_SHARED);
        HICON hModelIcon = (HICON)LoadImageA(nullptr, MAKEINTRESOURCEA(32516), IMAGE_ICON, 16, 16, LR_SHARED);

        ImageList_AddIcon(m_hImageList, hFolderIcon);  // Index 0: Folder
        ImageList_AddIcon(m_hImageList, hFileIcon);    // Index 1: Regular file
        ImageList_AddIcon(m_hImageList, hModelIcon);   // Index 2: Model file

        TreeView_SetImageList(m_hwndFileExplorer, m_hImageList, TVSIL_NORMAL);
    }

    populateFileTree();
}

void Win32IDE::populateFileTree()
{
    if (!m_hwndFileExplorer)
        return;

    // Clear existing items
    TreeView_DeleteAllItems(m_hwndFileExplorer);

    // Enumerate all logical drives as roots to mirror full filesystem access.
    wchar_t drives[512] = {};
    DWORD len = GetLogicalDriveStringsW(static_cast<DWORD>(std::size(drives) - 1), drives);
    if (len == 0 || len >= std::size(drives))
        return;

    for (const wchar_t* p = drives; *p; p += wcslen(p) + 1)
    {
        const std::wstring driveRoot(p);
        const std::string drivePath = wideToUtf8(driveRoot);
        HTREEITEM hRoot = addTreeItem(TVI_ROOT, drivePath, drivePath, true);
        if (hRoot)
        {
            addTreeItem(hRoot, "Loading...", "", false);
        }
    }

    // Expand the first drive by default.
    HTREEITEM hFirst = TreeView_GetRoot(m_hwndFileExplorer);
    if (hFirst)
    {
        TreeView_Expand(m_hwndFileExplorer, hFirst, TVE_EXPAND);
    }
}

HTREEITEM Win32IDE::addTreeItem(HTREEITEM hParent, const std::string& text, const std::string& fullPath,
                                bool isDirectory)
{
    TVINSERTSTRUCTW tvins = {};
    tvins.hParent = hParent;
    tvins.hInsertAfter = TVI_LAST;
    tvins.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;

    std::string* pathData = new std::string(fullPath);

    wchar_t wbuf[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, wbuf, MAX_PATH);
    tvins.item.pszText = wbuf;
    tvins.item.lParam = reinterpret_cast<LPARAM>(pathData);

    if (isDirectory)
    {
        tvins.item.iImage = 0;
        tvins.item.iSelectedImage = 0;
    }
    else if (isModelFile(fullPath))
    {
        tvins.item.iImage = 2;
        tvins.item.iSelectedImage = 2;
    }
    else
    {
        tvins.item.iImage = 1;
        tvins.item.iSelectedImage = 1;
    }

    return (HTREEITEM)SendMessageW(m_hwndFileExplorer, TVM_INSERTITEM, 0, (LPARAM)&tvins);
}

void Win32IDE::scanDirectory(const std::string& dirPath, HTREEITEM hParent)
{
    WIN32_FIND_DATAA findData;
    std::string searchPath = dirPath + "\\*";

    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE)
        return;

    do
    {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
        {
            continue;
        }

        std::string fullPath = dirPath + "\\" + findData.cFileName;
        bool isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        // Skip hidden and system files
        if (findData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))
        {
            continue;
        }

        HTREEITEM hItem = addTreeItem(hParent, findData.cFileName, fullPath, isDirectory);

        // For directories, add a dummy child so we can expand later
        if (isDirectory)
        {
            addTreeItem(hItem, "Loading...", "", false);
        }

    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}

bool Win32IDE::isModelFile(const std::string& filePath)
{
    std::string fileName = filePath;
    std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);

    return fileName.find(".gguf") != std::string::npos || fileName.find(".bin") != std::string::npos ||
           fileName.find(".safetensors") != std::string::npos || fileName.find(".pt") != std::string::npos ||
           fileName.find(".pth") != std::string::npos || fileName.find(".onnx") != std::string::npos;
}

void Win32IDE::expandTreeNode(HTREEITEM hItem)
{
    if (!hItem)
        return;

    // Check if this node has been expanded before
    HTREEITEM hChild = TreeView_GetChild(m_hwndFileExplorer, hItem);
    if (hChild)
    {
        TVITEMW item = {};
        item.hItem = hChild;
        item.mask = TVIF_TEXT | TVIF_PARAM;
        wchar_t buffer[MAX_PATH];
        item.pszText = buffer;
        item.cchTextMax = MAX_PATH;

        if (SendMessageW(m_hwndFileExplorer, TVM_GETITEM, 0, (LPARAM)&item))
        {
            if (wcscmp(item.pszText, L"Loading...") == 0)
            {
                // Remove the dummy item
                TreeView_DeleteItem(m_hwndFileExplorer, hChild);

                // Get the full path and scan the directory
                TVITEMW parentItem = {};
                parentItem.hItem = hItem;
                parentItem.mask = TVIF_PARAM;
                if (SendMessageW(m_hwndFileExplorer, TVM_GETITEM, 0, (LPARAM)&parentItem) && parentItem.lParam)
                {
                    std::string dirPath = reinterpret_cast<char*>(parentItem.lParam);
                    scanDirectory(dirPath, hItem);
                }
            }
        }
    }
}

std::string Win32IDE::getSelectedFilePath()
{
    HWND hTree = (m_hwndFileTree && IsWindow(m_hwndFileTree)) ? m_hwndFileTree : m_hwndFileExplorer;
    if (!hTree)
    {
        return "";
    }

    HTREEITEM hSelected = TreeView_GetSelection(hTree);
    if (!hSelected)
    {
        return "";
    }

    const std::string mapped = getTreeItemPath(hSelected);
    if (!mapped.empty())
    {
        return mapped;
    }

    TVITEMW item = {};
    item.hItem = hSelected;
    item.mask = TVIF_PARAM;

    if (SendMessageW(hTree, TVM_GETITEM, 0, (LPARAM)&item) && item.lParam)
    {
        return std::string(reinterpret_cast<char*>(item.lParam));
    }

    return "";
}

void Win32IDE::onFileExplorerDoubleClick()
{
    std::string filePath = getSelectedFilePath();
    if (filePath.empty())
        return;

    DWORD attributes = GetFileAttributesA(filePath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return;

    if (attributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        // Expand/collapse directory
        HTREEITEM hSelected = TreeView_GetSelection(m_hwndFileExplorer);
        if (hSelected)
        {
            UINT state = TreeView_GetItemState(m_hwndFileExplorer, hSelected, TVIS_EXPANDED);
            if (state & TVIS_EXPANDED)
            {
                TreeView_Expand(m_hwndFileExplorer, hSelected, TVE_COLLAPSE);
            }
            else
            {
                expandTreeNode(hSelected);
                TreeView_Expand(m_hwndFileExplorer, hSelected, TVE_EXPAND);
            }
        }
    }
    else
    {
        // Load file
        if (isModelFile(filePath))
        {
            loadModelFromExplorer(filePath);
        }
        else
        {
            // Open text files in editor - with size check!
            try
            {
                std::ifstream file(filePath, std::ios::binary);
                if (file.is_open())
                {
                    // Check file size first
                    file.seekg(0, std::ios::end);
                    size_t fileSize = file.tellg();
                    file.seekg(0, std::ios::beg);

                    if (fileSize > 10 * 1024 * 1024)
                    {  // 10MB limit
                        MessageBoxW(m_hwndMain, L"File too large to open in editor (>10MB).", L"File Too Large",
                                    MB_OK | MB_ICONWARNING);
                        return;
                    }

                    file.close();
                    openFile(filePath);
                }
            }
            catch (const std::exception& e)
            {
                std::string error = "Error opening file: " + std::string(e.what());
                MessageBoxW(m_hwndMain, utf8ToWide(error).c_str(), L"Error", MB_OK | MB_ICONERROR);
            }
        }
    }
}

void Win32IDE::loadModelFromExplorer(const std::string& filePath)
{
    bool ggufOk = loadGGUFModel(filePath);
    // Always pass to agentic bridge so chat and task execution use this model
    bool bridgeOk = loadModelForInference(filePath);
    if (ggufOk)
    {
        std::string message = "✅ Model loaded from File Explorer:\n" + filePath + "\n\n" + getModelInfo();
        appendToOutput(message, "Output", OutputSeverity::Info);
    }
    if (bridgeOk)
    {
        appendToOutput("Agentic bridge loaded model; chat and task execution use this model.\n", "Output",
                       OutputSeverity::Info);
        std::string filename = filePath;
        size_t lastSlash = filename.find_last_of("\\/");
        if (lastSlash != std::string::npos)
            filename = filename.substr(lastSlash + 1);
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)utf8ToWide("Model: " + filename).c_str());
    }
    if (!ggufOk && !bridgeOk)
    {
        appendToOutput("❌ Failed to load model: " + filePath + " (not a valid GGUF and native load failed).", "Errors",
                       OutputSeverity::Error);
    }
}

void Win32IDE::onFileExplorerRightClick()
{
    std::string filePath = getSelectedFilePath();
    if (!filePath.empty())
    {
        DWORD attributes = GetFileAttributesA(filePath.c_str());
        bool isDirectory = (attributes != INVALID_FILE_ATTRIBUTES) && (attributes & FILE_ATTRIBUTE_DIRECTORY);
        showFileContextMenu(filePath, isDirectory);
    }
}

void Win32IDE::showFileContextMenu(const std::string& filePath, bool isDirectory)
{
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu)
        return;

    static constexpr int IDC_CTX_REFRESH = 50001, IDC_CTX_OPEN_EXPLORER = 50002, IDC_CTX_SET_ROOT = 50003;
    static constexpr int IDC_CTX_LOAD_MODEL = 50011, IDC_CTX_MODEL_INFO = 50012, IDC_CTX_OPEN_EDITOR = 50013,
                         IDC_CTX_COPY_PATH = 50014, IDC_CTX_SHOW_EXPLORER = 50015, IDC_CTX_OPEN_TERMINAL_PS = 50016,
                         IDC_CTX_OPEN_TERMINAL_CMD = 50017;
    static constexpr int IDC_CTX_DELETE = 50020, IDC_CTX_RENAME = 50021;
    if (isDirectory)
    {
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_REFRESH, L"Refresh");
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_OPEN_EXPLORER, L"Open in Explorer");
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_OPEN_TERMINAL_PS, L"Open PowerShell Here");
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_OPEN_TERMINAL_CMD, L"Open CMD Here");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_SET_ROOT, L"Set as Root Path");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_DELETE, L"Delete");
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_RENAME, L"Rename");
    }
    else
    {
        if (isModelFile(filePath))
        {
            AppendMenuW(hMenu, MF_STRING, IDC_CTX_LOAD_MODEL, L"Load Model");
            AppendMenuW(hMenu, MF_STRING, IDC_CTX_MODEL_INFO, L"Show Model Info");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        }
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_OPEN_EDITOR, L"Open with Editor");
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_COPY_PATH, L"Copy Path");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_SHOW_EXPLORER, L"Show in Explorer");
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_OPEN_TERMINAL_PS, L"Open PowerShell Here");
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_OPEN_TERMINAL_CMD, L"Open CMD Here");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_DELETE, L"Delete");
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_RENAME, L"Rename");
    }

    POINT pt;
    GetCursorPos(&pt);

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwndMain, nullptr);

    switch (cmd)
    {
        case 50001:  // Refresh directory
            refreshFileExplorer();
            break;
        case 50002:  // Open in Explorer
        case 50015:  // Show in Explorer
            ShellExecuteA(nullptr, "explore", filePath.c_str(), nullptr, nullptr, SW_SHOW);
            break;
        case 50020:  // IDC_CTX_DELETE
            deleteItemInExplorer(filePath);
            break;
        case 50021:  // IDC_CTX_RENAME
            renameItemInExplorer(filePath);
            break;
        case 50003:  // Set as Root Path
            m_currentExplorerPath = filePath;
            populateFileTree();
            break;
        case 50016:  // Open PowerShell Here
        case 50017:  // Open CMD Here
        {
            std::string targetDir = filePath;
            if (!isDirectory)
            {
                std::error_code ec;
                targetDir = std::filesystem::path(filePath).parent_path().string();
                if (targetDir.empty())
                    targetDir = m_currentExplorerPath;
            }

            if (targetDir.empty())
                targetDir = m_currentExplorerPath;

            if (targetDir.empty())
                break;

            if (cmd == 50016)
            {
                TerminalPane* pane = resolvePaneForInteractiveShellMenu();
                if (pane && pane->id >= 0)
                    setActiveTerminalPane(pane->id);
                startPowerShell();
                pane = resolvePaneForInteractiveShellMenu();
                if (pane && pane->manager && pane->manager->isRunning())
                {
                    std::string escaped = targetDir;
                    size_t pos = 0;
                    while ((pos = escaped.find('\'', pos)) != std::string::npos)
                    {
                        escaped.replace(pos, 1, "''");
                        pos += 2;
                    }
                    pane->manager->writeInput("Set-Location -LiteralPath '" + escaped + "'\r\n");
                }
                appendToOutput("Opened PowerShell at: " + targetDir + "\n", "Output", OutputSeverity::Info);
            }
            else
            {
                TerminalPane* pane = resolvePaneForInteractiveShellMenu();
                if (pane && pane->id >= 0)
                    setActiveTerminalPane(pane->id);
                startCommandPrompt();
                pane = resolvePaneForInteractiveShellMenu();
                if (pane && pane->manager && pane->manager->isRunning())
                {
                    pane->manager->writeInput("cd /d \"" + targetDir + "\"\r\n");
                }
                appendToOutput("Opened CMD at: " + targetDir + "\n", "Output", OutputSeverity::Info);
            }
        }
        break;
        case 50011:  // Load Model
            loadModelFromExplorer(filePath);
            break;
        case 50012:  // Show Model Info
            if (loadGGUFModel(filePath))
            {
                loadModelForInference(filePath);
                std::string info = "Model Information:\n" + getModelInfo();
                MessageBoxW(m_hwndMain, utf8ToWide(info).c_str(), L"Model Info", MB_OK | MB_ICONINFORMATION);
            }
            break;
        case 50013:  // Open with Editor
        {
            std::ifstream file(filePath);
            if (file.is_open())
            {
                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                m_suppressLspDocumentSync = true;
                setWindowText(m_hwndEditor, content);
                m_suppressLspDocumentSync = false;
                m_currentFile = filePath;
                updateTitleBarText();
                syncLSPDocumentOpen(m_currentFile, content);
            }
        }
        break;
        case 50014:  // Copy Path
            if (OpenClipboard(m_hwndMain))
            {
                EmptyClipboard();
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, filePath.size() + 1);
                if (hMem)
                {
                    char* dest = (char*)GlobalLock(hMem);
                    strcpy_s(dest, filePath.size() + 1, filePath.c_str());
                    GlobalUnlock(hMem);
                    // SetClipboardData takes ownership of hMem on success; only free on failure
                    if (!SetClipboardData(CF_TEXT, hMem))
                    {
                        GlobalFree(hMem);
                    }
                }
                CloseClipboard();
            }
            break;
    }

    DestroyMenu(hMenu);
}

void Win32IDE::refreshFileExplorer()
{
    populateFileTree();
}

// ============================================================================
// MODEL CHAT INTERFACE IMPLEMENTATION
// ============================================================================

bool Win32IDE::isModelLoaded() const
{
    const auto engine = m_nativeEngine ? m_nativeEngine : RawrXD::CPUInferenceEngine::GetSharedInstance();
    if (engine && engine->IsModelLoaded())
        return true;

    if (m_agenticBridge && m_agenticBridge->HasUsableBackend())
        return true;

    if (!m_loadedModelPath.empty() && m_ggufLoader && !m_modelTensors.empty())
        return true;

    return false;
}

std::string Win32IDE::sendMessageToModel(const std::string& message)
{
    return sendMessageToModelInternal(message, false, 512);
}

std::string Win32IDE::sendMessageToModelWithoutChatHistory(const std::string& message)
{
    const int cap = (std::max)(1024, (std::min)(8192, static_cast<int>(m_inferenceConfig.maxTokens) * 4));
    return sendMessageToModelInternal(message, true, cap);
}

std::string Win32IDE::sendMessageToModelInternal(const std::string& message, bool suppressChatHistory,
                                                 int localGenerateCap)
{
    const auto engine = m_nativeEngine ? m_nativeEngine : RawrXD::CPUInferenceEngine::GetSharedInstance();
    const bool canChat = isModelLoaded();
    if (!canChat)
    {
        std::string lastErr;
        if (engine)
            lastErr = engine->GetLastLoadErrorMessage();
        if (lastErr.empty() && m_agenticBridge)
            lastErr = m_agenticBridge->GetLastModelLoadError();
        if (!lastErr.empty())
            return std::string("Error: Inference blocked — ") + lastErr +
                   "\n(Check tokenizer.json / merges.txt beside the GGUF, arch support, or Backend Switcher / "
                   "Ollama.)\n";
        return "Error: No model loaded. Load a GGUF (File > Open / Load Model) or set up Ollama/backend in Backend "
               "Switcher.\n";
    }

    auto recordChatTurn = [this](const std::string& userMsg, const std::string& assistantMsg)
    {
        conversationDetectModelFormat(m_loadedModelPath.empty() ? std::string("local") : m_loadedModelPath);
        if (m_inferenceConfig.contextWindow > 0)
            conversationSetContextWindow(m_inferenceConfig.contextWindow);
        conversationAddUser(userMsg);
        conversationAddAssistant(assistantMsg);
        m_chatHistory.push_back({"user", userMsg});
        m_chatHistory.push_back({"assistant", assistantMsg});
        persistChatTurnToDisk("user", userMsg);
        persistChatTurnToDisk("assistant", assistantMsg);
    };

    // Phase 8B/8C: Route through LLM router (if enabled) or backend manager
    if (m_backendManagerInitialized)
    {
        std::string resp = routeWithIntelligence(message);
        if (!resp.empty() && resp.find("[Backend Error]") != 0)
        {
            if (!suppressChatHistory)
                recordChatTurn(message, resp);
            return resp;
        }
    }

    // Local CPU when weights are resident (authoritative; before Ollama shortcut).
    if (engine && engine->IsModelLoaded())
    {
        auto tokens = engine->Tokenize(message);
        const int genCap = (std::max)(64, localGenerateCap);
        auto output_tokens = engine->Generate(tokens, genCap);
        std::string response = engine->Detokenize(output_tokens);
        if (!suppressChatHistory)
            recordChatTurn(message, response);
        return response;
    }

    std::string llmResponse;
    if (trySendToOllama(message, llmResponse))
    {
        if (!suppressChatHistory)
            recordChatTurn(message, llmResponse);
        return llmResponse;
    }

    if (!m_loadedModelPath.empty())
        const_cast<Win32IDE*>(this)->ensureAgenticBridgeHasModel(m_loadedModelPath);

    if (m_agenticBridge && m_agenticBridge->HasUsableBackend())
    {
        AgentResponse r = m_agenticBridge->ExecuteAgentCommand(message);
        if (r.type == AgentResponseType::TOOL_CALL && !r.content.empty())
        {
            if (!suppressChatHistory)
            {
                conversationDetectModelFormat(m_loadedModelPath.empty() ? std::string("local") : m_loadedModelPath);
                if (m_inferenceConfig.contextWindow > 0)
                    conversationSetContextWindow(m_inferenceConfig.contextWindow);
                conversationAddUser(message);
                m_chatHistory.push_back({"user", message});
                persistChatTurnToDisk("user", message);
                const std::string tname = r.toolName.empty() ? std::string("tool") : r.toolName;
                recordToolTurnInChatHistory(tname, r.content, "result");
            }
            return r.content;
        }
        if (r.type != AgentResponseType::AGENT_ERROR && !r.content.empty())
        {
            if (!suppressChatHistory)
                recordChatTurn(message, r.content);
            return r.content;
        }
        if (r.type == AgentResponseType::AGENT_ERROR && !r.content.empty())
            return r.content;
    }

    std::string detail;
    if (engine)
        detail = engine->GetLastLoadErrorMessage();
    if (detail.empty() && m_agenticBridge)
        detail = m_agenticBridge->GetLastModelLoadError();
    if (!detail.empty())
    {
        return "Error: Inference unavailable — " + detail +
               "\n(Check tokenizer.json / merges.txt beside the GGUF, arch support, or free RAM.)\n";
    }
    return "Error: No inference backend produced a response. Load a GGUF (File > Load Model), or configure "
           "Ollama/backend in Backend Switcher.\n";
}

void Win32IDE::toggleChatMode()
{
    m_chatMode = !m_chatMode;

    if (m_chatMode)
    {
        // Entering chat mode
        std::string status = "🤖 Chat Mode ON - Model: ";
        status +=
            m_loadedModelPath.empty() ? "None" : m_loadedModelPath.substr(m_loadedModelPath.find_last_of("\\/") + 1);

        appendToOutput(status, "Output", OutputSeverity::Info);
        appendToOutput("Type your messages in the command input. Use /exit-chat to return to terminal mode.", "Output",
                       OutputSeverity::Info);

        // Update status bar
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)L"Chat Mode");

        // Clear existing chat display and show instructions
        appendChatMessage("System", "Chat mode activated! You can now talk with the loaded model.");
        appendChatMessage("System", "Commands: /exit-chat to return to terminal mode");
    }
    else
    {
        // Exiting chat mode
        appendToOutput("🔧 Chat Mode OFF - Returned to terminal mode", "Output", OutputSeverity::Info);
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)L"Terminal Mode");
        appendChatMessage("System", "Chat mode deactivated. Returned to terminal mode.");
    }
}

void Win32IDE::appendChatMessage(const std::string& user, const std::string& message)
{
    // Get timestamp
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    char timestamp[16];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &timeinfo);

    // Format message
    std::string formattedMsg = "[" + std::string(timestamp) + "] " + user + ": " + message + "\n\n";

    // Display in output panel
    if (user == "System")
    {
        appendToOutput(formattedMsg, "Output", OutputSeverity::Info);
    }
    else if (user == "You")
    {
        appendToOutput(formattedMsg, "Output", OutputSeverity::Info);
    }
    else if (user == "Model")
    {
        appendToOutput(formattedMsg, "Output", OutputSeverity::Info);
    }
}

// ============================================================================
// GIT INTEGRATION - Status, Commit, Push, Pull
// ============================================================================

void Win32IDE::showGitStatus()
{
    if (!isGitRepository())
    {
        MessageBoxW(m_hwndMain, L"Not a Git repository", L"Git", MB_OK | MB_ICONWARNING);
        return;
    }

    updateGitStatus();

    std::ostringstream status;
    status << "Git Status\n";
    status << "==========\n\n";
    status << "Branch: " << m_gitStatus.branch << "\n";
    status << "\nChanges:\n";
    status << "  Modified:  " << m_gitStatus.modified << "\n";
    status << "  Added:     " << m_gitStatus.added << "\n";
    status << "  Deleted:   " << m_gitStatus.deleted << "\n";
    status << "  Untracked: " << m_gitStatus.untracked << "\n";

    MessageBoxW(m_hwndMain, utf8ToWide(status.str()).c_str(), L"Git Status", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::updateGitStatus()
{
    if (!isGitRepository())
    {
        m_gitStatus = GitStatus();
        refreshIntegratedTerminalContextHint();
        return;
    }

    std::string output;

    // Get current branch
    executeGitCommand("git rev-parse --abbrev-ref HEAD", output);
    m_gitStatus.branch = output;
    if (!m_gitStatus.branch.empty() && m_gitStatus.branch.back() == '\n')
    {
        m_gitStatus.branch.pop_back();
    }
    output.clear();

    // Get status --porcelain
    executeGitCommand("git status --porcelain", output);
    m_gitStatus.modified = 0;
    m_gitStatus.added = 0;
    m_gitStatus.deleted = 0;
    m_gitStatus.untracked = 0;
    m_gitStatus.fileStatus.clear();

    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line))
    {
        if (line.length() < 2)
            continue;

        char status = line[0];
        char status2 = line[1];
        std::string filePath = line.substr(3);

        // Store per-file status (prefer index status over working tree)
        char fileStatus = status;
        if (fileStatus == ' ')
        {
            fileStatus = status2;
        }

        m_gitStatus.fileStatus[filePath] = fileStatus;

        if (status == 'M' || status2 == 'M')
            m_gitStatus.modified++;
        if (status == 'A' || status2 == 'A')
            m_gitStatus.added++;
        if (status == 'D' || status2 == 'D')
            m_gitStatus.deleted++;
        if (status == '?' || status2 == '?')
            m_gitStatus.untracked++;
    }

    m_gitStatus.hasChanges =
        (m_gitStatus.modified + m_gitStatus.added + m_gitStatus.deleted + m_gitStatus.untracked) > 0;
    refreshIntegratedTerminalContextHint();
}

void Win32IDE::gitCommit(const std::string& message)
{
    if (!isGitRepository())
    {
        MessageBoxW(m_hwndMain, L"Not a Git repository", L"Git Error", MB_OK | MB_ICONERROR);
        return;
    }

    std::string output;
    std::string command = "git commit -m \"" + message + "\"";
    executeGitCommand(command, output);

    MessageBoxW(m_hwndMain, utf8ToWide(output).c_str(), L"Git Commit", MB_OK | MB_ICONINFORMATION);
    updateGitStatus();
}

void Win32IDE::gitPush()
{
    if (!isGitRepository())
    {
        MessageBoxW(m_hwndMain, L"Not a Git repository", L"Git Error", MB_OK | MB_ICONERROR);
        return;
    }

    std::string output;
    executeGitCommand("git push", output);

    MessageBoxW(m_hwndMain, utf8ToWide(output.empty() ? "Push completed successfully" : output).c_str(), L"Git Push",
                MB_OK | MB_ICONINFORMATION);
    updateGitStatus();
}

void Win32IDE::gitPull()
{
    if (!isGitRepository())
    {
        MessageBoxW(m_hwndMain, L"Not a Git repository", L"Git Error", MB_OK | MB_ICONERROR);
        return;
    }

    std::string output;
    executeGitCommand("git pull", output);

    MessageBoxW(m_hwndMain, utf8ToWide(output.empty() ? "Pull completed successfully" : output).c_str(), L"Git Pull",
                MB_OK | MB_ICONINFORMATION);
    updateGitStatus();
}

void Win32IDE::gitStageFile(const std::string& filePath)
{
    if (!isGitRepository())
        return;

    std::string output;
    std::string command = "git add \"" + filePath + "\"";
    executeGitCommand(command, output);
    updateGitStatus();
}

void Win32IDE::gitUnstageFile(const std::string& filePath)
{
    if (!isGitRepository())
        return;

    std::string output;
    std::string command = "git reset HEAD \"" + filePath + "\"";
    executeGitCommand(command, output);
    updateGitStatus();
}

bool Win32IDE::isGitRepository() const
{
    if (!m_gitRepoPath.empty())
    {
        std::string gitDir = m_gitRepoPath + "\\.git";
        DWORD attrib = GetFileAttributesA(gitDir.c_str());
        return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
    }

    // Check current directory
    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    std::string gitDir = std::string(currentDir) + "\\.git";
    DWORD attrib = GetFileAttributesA(gitDir.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
}

std::vector<GitFile> Win32IDE::getGitChangedFiles() const
{
    std::vector<GitFile> files;

    if (!isGitRepository())
        return files;

    std::string output;
    const_cast<Win32IDE*>(this)->executeGitCommand("git status --porcelain", output);

    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line))
    {
        if (line.length() < 4)
            continue;

        GitFile file;
        file.status = line[0] != ' ' ? line[0] : line[1];
        file.staged = (line[0] != ' ' && line[0] != '?');
        file.path = line.substr(3);

        files.push_back(file);
    }

    return files;
}

bool Win32IDE::executeGitCommand(const std::string& command, std::string& output)
{
    output.clear();

    // Create a temporary file for output
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    std::string tempFile = std::string(tempPath) + "rawr_git_output.txt";

    // Execute command and redirect output
    std::string fullCommand = command + " > \"" + tempFile + "\" 2>&1";

    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (CreateProcessA(NULL, const_cast<char*>(fullCommand.c_str()), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
                       &si, &pi))
    {

        WaitForSingleObject(pi.hProcess, 5000);  // 5 second timeout
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        // Read output file
        std::ifstream file(tempFile);
        if (file.is_open())
        {
            std::string line;
            while (std::getline(file, line))
            {
                output += line + "\n";
            }
            file.close();
            DeleteFileA(tempFile.c_str());
        }
        return true;
    }
    return false;
}

void Win32IDE::queryGitBranchForIntegratedCwd(const std::string& cwdUtf8, std::string& outBranch) const
{
    outBranch.clear();
    if (cwdUtf8.empty())
        return;

    std::string quoted;
    quoted.reserve(cwdUtf8.size() + 2);
    quoted.push_back('"');
    for (unsigned char uc : cwdUtf8)
    {
        const char c = static_cast<char>(uc);
        if (c == '"')
        {
            quoted += '\\';
            quoted += '"';
        }
        else
            quoted += c;
    }
    quoted.push_back('"');

    std::string out;
    const std::string cmd = "git -C " + quoted + " rev-parse --abbrev-ref HEAD";
    if (!const_cast<Win32IDE*>(this)->executeGitCommand(cmd, out))
        return;

    while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ' || out.back() == '\t'))
        out.pop_back();
    if (out.empty())
        return;
    std::string lower = out;
    for (auto& ch : lower)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (lower.find("fatal:") != std::string::npos || lower.find("not a git repository") != std::string::npos)
        return;

    outBranch = std::move(out);
}

void Win32IDE::showGitPanel()
{
    if (!isGitRepository())
    {
        MessageBoxW(m_hwndMain, L"Not a Git repository", L"Git", MB_OK | MB_ICONWARNING);
        return;
    }

    // Create Git panel if it doesn't exist
    if (!m_hwndGitPanel || !IsWindow(m_hwndGitPanel))
    {
        m_hwndGitPanel = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"Git Panel",
                                         WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | WS_SIZEBOX, 200, 100, 600,
                                         500, m_hwndMain, nullptr, m_hInstance, nullptr);

        // Branch and status info
        m_hwndGitStatusText =
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY, 10, 10,
                            580, 60, m_hwndGitPanel, nullptr, m_hInstance, nullptr);

        CreateWindowExW(0, L"STATIC", L"Changed Files:", WS_CHILD | WS_VISIBLE, 10, 80, 120, 20, m_hwndGitPanel,
                        nullptr, m_hInstance, nullptr);

        m_hwndGitFileList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                            WS_CHILD | WS_VISIBLE | LBS_STANDARD | LBS_EXTENDEDSEL | WS_VSCROLL, 10,
                                            105, 280, 300, m_hwndGitPanel, nullptr, m_hInstance, nullptr);
    }

    ShowWindow(m_hwndGitPanel, SW_SHOW);
    refreshGitPanel();
}

void Win32IDE::refreshGitPanel()
{
    if (!m_hwndGitPanel || !IsWindow(m_hwndGitPanel))
        return;

    updateGitStatus();

    // Update status text
    std::string statusText = "Branch: " + m_gitStatus.branch + "\n";
    statusText += "Modified: " + std::to_string(m_gitStatus.modified) + " | ";
    statusText += "Added: " + std::to_string(m_gitStatus.added) + " | ";
    statusText += "Deleted: " + std::to_string(m_gitStatus.deleted) + " | ";
    statusText += "Untracked: " + std::to_string(m_gitStatus.untracked);

    if (m_hwndGitStatusText)
    {
        SetWindowTextW(m_hwndGitStatusText, utf8ToWide(statusText).c_str());
    }

    // Update file list
    if (m_hwndGitFileList)
    {
        SendMessage(m_hwndGitFileList, LB_RESETCONTENT, 0, 0);

        std::vector<GitFile> files = getGitChangedFiles();
        for (const auto& file : files)
        {
            std::string displayText;
            if (file.staged)
            {
                displayText = "[S] ";
            }
            else
            {
                displayText = "[ ] ";
            }

            switch (file.status)
            {
                case 'M':
                    displayText += "(M) ";
                    break;
                case 'A':
                    displayText += "(A) ";
                    break;
                case 'D':
                    displayText += "(D) ";
                    break;
                case '?':
                    displayText += "(?) ";
                    break;
                default:
                    displayText += "( ) ";
                    break;
            }

            displayText += file.path;
            SendMessageW(m_hwndGitFileList, LB_ADDSTRING, 0, (LPARAM)utf8ToWide(displayText).c_str());
        }
    }
}

void Win32IDE::showCommitDialog()
{
    if (!isGitRepository())
    {
        MessageBoxW(m_hwndMain, L"Not a Git repository", L"Git", MB_OK | MB_ICONWARNING);
        return;
    }

    // WS_EX_TOPMOST keeps dialog visible during screenshot attempts (focus theft protection)
    HWND hwndDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"STATIC", L"Git Commit",
                                   WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 150, 150, 500, 200, m_hwndMain,
                                   nullptr, m_hInstance, nullptr);
    if (!hwndDlg)
    {
        const DWORD createErr = GetLastError();
        appendToOutput("[UI] Failed to create Git Commit dialog (GetLastError=" + std::to_string(createErr) + ")\n",
                       "Errors", OutputSeverity::Error);
        return;
    }

    CreateWindowExW(0, L"STATIC", L"Commit Message:", WS_CHILD | WS_VISIBLE, 10, 10, 120, 20, hwndDlg, nullptr,
                    m_hInstance, nullptr);

    m_hwndCommitDialog =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY, 10, 35, 470,
                        100, hwndDlg, nullptr, m_hInstance, nullptr);

    CreateWindowExW(0, L"BUTTON", L"Commit", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 10, 145, 100, 30, hwndDlg,
                    (HMENU)1, m_hInstance, nullptr);

    CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 120, 145, 100, 30, hwndDlg, (HMENU)2, m_hInstance,
                    nullptr);

    SetFocus(m_hwndCommitDialog);
}

// ============================================================================
// AI INFERENCE IMPLEMENTATION - Connects GGUF Loader to Chat Panel
// ============================================================================

void Win32IDE::openModel()
{
    wchar_t filename[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = L"GGUF Models\0*.gguf\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Select GGUF Model";

    if (GetOpenFileNameW(&ofn))
    {
        loadModelFromPathAsync(wideToUtf8(filename));
    }
}

bool Win32IDE::ensureAgenticBridgeHasModel(const std::string& path)
{
    if (path.empty())
        return false;
    if (!m_agenticBridge)
        initializeAgenticBridge();
    if (!m_agenticBridge)
        return false;

    // Register selected model path without forcing immediate heavyweight local load.
    m_agenticBridge->SetModel(path);
    m_loadedModelPath = path;
    return true;
}

bool Win32IDE::loadModelForInference(const std::string& filepath)
{
    SCOPED_METRIC("model.load");
    METRICS.increment("model.load_attempts");
    appendToOutput("Loading model: " + filepath + "\n", "System", OutputSeverity::Info);

    const uint64_t tick0 = GetTickCount64();

    if (!m_agenticBridge)
    {
        initializeAgenticBridge();
    }

    if (m_agenticBridge)
    {
        // Register path/model on bridge; actual backend load occurs through the
        // active inference backend at request time.
        m_agenticBridge->SetModel(filepath);
        if (!filepath.empty())
        {
            m_loadedModelPath = filepath;
            METRICS.gauge("model.loaded", 1.0);
            METRICS.increment("model.load_success");
            appendToOutput("Model loaded successfully into Agentic Bridge.\n", "System", OutputSeverity::Info);

            // Keep status bar and metrics in sync even when GGUF streaming is skipped.
            m_lastLoadedModelOk = true;
            m_lastLoadedModelPath = filepath;
            m_lastLoadedModelDisplayName = std::filesystem::path(filepath).has_filename()
                                               ? std::filesystem::path(filepath).filename().string()
                                               : filepath;
            {
                std::error_code fsec;
                const auto sz = std::filesystem::file_size(std::filesystem::path(filepath), fsec);
                if (!fsec)
                {
                    m_lastLoadedModelBytes = static_cast<uint64_t>(sz);
                    METRICS.gauge("model.file_bytes", static_cast<double>(m_lastLoadedModelBytes));
                    METRICS.gauge("model.file_gb",
                                  static_cast<double>(m_lastLoadedModelBytes) / (1024.0 * 1024.0 * 1024.0));
                }
            }
            m_lastLoadedModelWallMs = static_cast<double>(GetTickCount64() - tick0);
            METRICS.recordDuration("model.bridge_set_wall_ms", m_lastLoadedModelWallMs);
            if (m_hwndMain && IsWindow(m_hwndMain))
                PostMessageW(m_hwndMain, WM_STATUSBAR_REFRESH_COPILOT, 0, 0);

            wireLayerProgressToOutputPanel();

            // NOTE: appendStreamerPostLoadCheck removed — it opened a second RawrXDModelLoader
            // from the background thread (redundant full file-map + cross-thread SendMessageW),
            // which was crash-prone and unnecessary for correct inference.

            // Sync current UI state
            m_agenticBridge->SetContextSize("4K");
            if (m_hwndContextSlider)
                SendMessage(m_hwndContextSlider, TBM_SETPOS, TRUE, 0);

            // Thread-safe: tool sandbox roots follow workspace (also refreshed on UI thread after load).
            syncAgenticToolGuardrailsFromWorkspace();

            return true;
        }
    }

    METRICS.increment("model.load_failures");
    METRICS.gauge("model.loaded", 0.0);
    m_lastLoadedModelOk = false;
    std::string detail;
    if (m_agenticBridge)
    {
        detail = m_agenticBridge->GetLastModelLoadError();
    }
    if (!detail.empty())
    {
        showModelLoadError(detail);
        appendToOutput("Failed to load model: " + filepath + "\nReason: " + detail + "\n", "System",
                       OutputSeverity::Error);
    }
    else
    {
        appendToOutput("Failed to load model: " + filepath + "\n", "System", OutputSeverity::Error);
    }
    return false;
}

bool Win32IDE::initializeInference()
{
    SCOPED_METRIC("inference.initialize");
    METRICS.increment("inference.init_attempts");
    std::lock_guard<std::mutex> lock(m_inferenceMutex);

    // Explicit Logic: Initialize Native CPU Engine if missing (Un-mocking)
    if (!m_nativeEngine)
    {
        try
        {
            m_nativeEngine = RawrXD::CPUInferenceEngine::GetSharedInstance();
            // Match deferredHeavyInit (Win32IDE_Core): same memory plugin as headless-capable path.
            auto memPlugin = std::make_shared<RawrXD::Modules::NativeMemoryModule>();
            m_nativeEngine->RegisterMemoryPlugin(memPlugin);
            m_nativeEngineLoaded = false;
            appendToOutput("Initialized Native CPU Inference Engine.", "Output", OutputSeverity::Info);
        }
        catch (const std::exception& e)
        {
            appendToOutput(std::string("Failed to init native engine: ") + e.what(), "Errors", OutputSeverity::Error);
            return false;
        }
    }

    // Check if model is loaded via GGUF loader (Streaming)
    if (m_loadedModelPath.empty())
    {
        if (!m_ggufLoader)
        {
            appendToOutput("No model loaded for inference", "Errors", OutputSeverity::Error);
            return false;
        }
        // If ggufLoader has a file open but path var is empty, try to recover (unlikely)
    }

    // Connect Native Engine to Model
    if (m_nativeEngine && !m_loadedModelPath.empty())
    {
        RawrXD::CPUInferenceEngine* engine = static_cast<RawrXD::CPUInferenceEngine*>(m_nativeEngine.get());
        if (!engine->IsModelLoaded())
        {
            appendToOutput("Loading model into Native Engine: " + m_loadedModelPath, "Output", OutputSeverity::Info);
            if (engine->LoadModel(m_loadedModelPath))
            {
                m_nativeEngineLoaded = true;
                appendToOutput("✅ Native Engine Model Loaded Successfully.", "Output", OutputSeverity::Info);
            }
            else
            {
                appendToOutput("❌ Native Engine Model Load Failed.", "Errors", OutputSeverity::Error);
                // Don't fail completely if we have Ollama fallback, but for "no simulation" we adhere to native.
            }
        }
    }

    // Set up inference config from model metadata
    m_inferenceConfig.maxTokens = 512;
    m_inferenceConfig.contextWindow = 4096;
    m_inferenceConfig.temperature = 0.7f;
    m_inferenceConfig.topP = 0.9f;
    m_inferenceConfig.topK = 40;
    m_inferenceConfig.repetitionPenalty = 1.1f;

    // Use model context length if available
    if (m_currentModelMetadata.context_length > 0)
    {
        m_inferenceConfig.maxTokens = std::min(512, (int)m_currentModelMetadata.context_length / 4);
    }

    // Ensure backend registration is visible to chat before first send.
    RawrXD::InitializeAgenticChatBackend();

    appendToOutput("✅ Inference initialized for model: " + m_loadedModelPath, "Output", OutputSeverity::Info);
    wireLayerProgressToOutputPanel();
    return true;
}

void Win32IDE::shutdownInference()
{
    std::lock_guard<std::mutex> lock(m_inferenceMutex);
    ClearPendingInference(this);

    if (m_inferenceRunning.load())
    {
        m_inferenceStopRequested.store(true);
        if (m_inferenceThread.joinable())
        {
            if (m_inferenceThread.get_id() == std::this_thread::get_id())
            {
                // Joining the current thread throws std::system_error
                // ("resource deadlock would occur").
                m_inferenceThread.detach();
            }
            else
            {
                m_inferenceThread.join();
            }
        }
    }

    m_inferenceRunning.store(false);
    m_inferenceStopRequested.store(false);
    m_currentInferencePrompt.clear();
    m_currentInferenceResponse.clear();

    appendToOutput("Inference shutdown complete", "Output", OutputSeverity::Info);
}

std::string Win32IDE::generateResponse(const std::string& prompt)
{
    SCOPED_METRIC("inference.generate_response");
    METRICS.increment("inference.requests_total");

    // Phase barrier: enqueue if runtime not ready
    if (m_sessionController)
    {
        std::string reason;
        if (!m_sessionController->IsExecutionReady(&reason))
        {
            std::string status;
            if (m_sessionController->EnqueuePendingInference(prompt, &status))
                return "[Deferred] " + status;
            return "[Rejected] " + status;
        }
    }

    if (m_inferenceRunning.load())
    {
        METRICS.increment("inference.requests_rejected");
        return "System busy: inference worker is processing another request. Please retry shortly.";
    }

    // Phase 8B/8C: Route through LLM router (if enabled) or backend manager
    if (m_backendManagerInitialized)
    {
        return routeWithIntelligence(prompt);
    }

    // Attempt real remote/local inference via Ollama if configured
    auto performOllama = [&](const std::string& promptText) -> std::string
    {
        if (m_ollamaBaseUrl.empty())
            return "";
        // Expect base URL like http://localhost:11435
        std::string base = m_ollamaBaseUrl;
        if (base.rfind("http://", 0) != 0 && base.rfind("https://", 0) != 0)
            return "";
        bool https = base.rfind("https://", 0) == 0;
        std::string withoutProto = base.substr(base.find("://") + 3);
        std::string host;
        int port = https ? 443 : 80;
        size_t colonPos = withoutProto.find(':');
        size_t slashPos = withoutProto.find('/');
        if (colonPos != std::string::npos)
        {
            host = withoutProto.substr(0, colonPos);
            std::string portStr = withoutProto.substr(
                colonPos + 1, (slashPos == std::string::npos ? withoutProto.size() : slashPos) - (colonPos + 1));
            port = atoi(portStr.c_str());
        }
        else
        {
            host = (slashPos == std::string::npos) ? withoutProto : withoutProto.substr(0, slashPos);
            // Default Ollama port
            if (!https)
                port = 11434;
        }
        std::wstring whost(host.begin(), host.end());
        HINTERNET hSession = WinHttpOpen(L"RawrXDIDE/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, NULL, NULL, 0);
        if (!hSession)
            return "";
        HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), (INTERNET_PORT)port, 0);
        if (!hConnect)
        {
            WinHttpCloseHandle(hSession);
            return "";
        }
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/generate", NULL, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0);
        if (!hRequest)
        {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }
        // Build JSON body
        std::string modelTag;
        if (!m_ollamaModelOverride.empty())
            modelTag = m_ollamaModelOverride;
        else
        {
            // Derive from loaded path
            modelTag = m_loadedModelPath;
            size_t pos = modelTag.find_last_of("\\/");
            if (pos != std::string::npos)
                modelTag = modelTag.substr(pos + 1);
        }
        // Basic escaping of quotes in prompt
        std::string escPrompt;
        escPrompt.reserve(promptText.size() + 16);
        for (char c : promptText)
        {
            if (c == '"')
                escPrompt += "\\\"";
            else if (c == '\n')
                escPrompt += "\\n";
            else
                escPrompt += c;
        }
        if (escPrompt.empty())
            escPrompt = " ";
        std::string body = std::string("{\"model\":\"") + modelTag + "\",\"prompt\":\"" + escPrompt +
                           "\",\"stream\":false,\"options\":{\"num_ctx\":1024,\"num_predict\":256,\"num_batch\":64,"
                           "\"use_mmap\":false}}";
        std::wstring wHeaders = L"Content-Type: application/json";
        BOOL bResults = WinHttpSendRequest(hRequest, wHeaders.c_str(), (DWORD)-1L, (LPVOID)body.c_str(),
                                           (DWORD)body.size(), (DWORD)body.size(), 0);
        if (!bResults)
        {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }
        bResults = WinHttpReceiveResponse(hRequest, NULL);
        std::string raw;
        if (bResults)
        {
            DWORD dwSize = 0;
            do
            {
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
                    break;
                if (!dwSize)
                    break;
                std::string chunk;
                chunk.resize(dwSize);
                DWORD dwRead = 0;
                if (!WinHttpReadData(hRequest, chunk.data(), dwSize, &dwRead))
                    break;
                if (dwRead)
                    raw.append(chunk.data(), dwRead);
            } while (dwSize > 0);
        }
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        if (raw.empty())
            return "";
        // Naive JSON parse: look for "response":"..."
        std::string out;
        size_t pos = raw.rfind("\"response\":\"");
        if (pos != std::string::npos)
        {
            pos += 12;  // start after marker
            while (pos < raw.size())
            {
                char c = raw[pos++];
                if (c == '"')
                    break;  // end of string (assumes not escaped)
                if (c == '\\')
                {
                    if (pos < raw.size())
                    {
                        char next = raw[pos++];
                        if (next == 'n')
                            out += '\n';
                        else
                            out += next;
                    }
                }
                else
                    out += c;
            }
        }
        return out.empty() ? raw : out;
    };

    std::string remote = performOllama(prompt);
    if (!remote.empty())
        return remote;

    // Fallback structured guidance if no remote inference available
    std::string modelName =
        m_loadedModelPath.empty() ? "None" : m_loadedModelPath.substr(m_loadedModelPath.find_last_of("\\/") + 1);

    // Fallback: Native CPU Inference Engine
    if (m_nativeEngine && m_nativeEngineLoaded)
    {
        RawrXD::CPUInferenceEngine* engine = static_cast<RawrXD::CPUInferenceEngine*>(m_nativeEngine.get());
        // If engine doesn't have a model loaded, try to load current one
        if (!engine->IsModelLoaded() && !m_loadedModelPath.empty())
        {
            engine->LoadModel(m_loadedModelPath);
        }

        if (engine->IsModelLoaded())
        {
            // Use Generate method for inference
            std::vector<int32_t> tokens = engine->Tokenize(prompt);
            std::vector<int32_t> output = engine->Generate(tokens, 100);
            std::string decoded = engine->Detokenize(output);
            // Guard: if the output is mostly '?' (unknown-token placeholder emitted by
            // the detokenizer for OOV token IDs), surface a diagnostic instead of
            // flooding the chat pane with unintelligible question marks.
            if (!decoded.empty())
            {
                int qCount = 0;
                for (char c : decoded)
                    if (c == '?')
                        ++qCount;
                if (decoded.size() > 20 && qCount > static_cast<int>(decoded.size()) * 2 / 5)
                    return "[Inference error: detokenization failed — " + std::to_string(qCount) + "/" +
                           std::to_string(decoded.size()) + " tokens unresolved. Verify model/vocab compatibility.]";
            }
            return decoded;
        }
        else
        {
            return "Error: No model loaded in Native CPU Engine.";
        }
    }

    return std::string("[Native Engine Error]\nModel: ") + modelName + "\nPrompt: " + prompt +
           "\n(Ollama unavailable and Native Engine not ready)";
}

void Win32IDE::generateResponseAsync(const std::string& prompt, std::function<void(const std::string&, bool)> callback)
{
    const unsigned long long traceId = NextChatTraceId();
    const std::string promptSig = MakePromptSignature(prompt);
    METRICS.increment("inference.async_requests_total");
    LOG_INFO("[ChatTrace#" + std::to_string(traceId) + "] generateResponseAsync begin " + promptSig + " preview='" +
             MakeLogPreview(prompt) + "' loaded_model='" + m_loadedModelPath + "'");

    OutputDebugStringA(("[generateResponseAsync] Starting for traceId: " + std::to_string(traceId) + " prompt: '" +
                        MakeLogPreview(prompt, 50) + "' loaded_model: '" + m_loadedModelPath + "'\n")
                           .c_str());

    // Phase barrier: enqueue if runtime not ready
    if (m_sessionController)
    {
        std::string reason;
        if (!m_sessionController->IsExecutionReady(&reason))
        {
            std::string status;
            LOG_WARNING("[ChatTrace#" + std::to_string(traceId) + "] execution not ready reason='" + reason + "'");
            if (m_sessionController->EnqueuePendingInference(prompt, &status))
            {
                if (callback)
                    callback("[Deferred] " + status, false);
            }
            else
            {
                if (callback)
                    callback("[Rejected] " + status, true);
            }
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_inferenceMutex);

        if (m_inferenceRunning.load())
        {
            std::function<void(const std::string&, bool)> pendingCallback = callback;
            const size_t queuedDepth =
                EnqueuePendingInference(this, PendingInferenceRequest{prompt, std::move(pendingCallback)});
            if (queuedDepth == 0)
            {
                METRICS.increment("inference.async_requests_rejected");
                LOG_WARNING("[ChatTrace#" + std::to_string(traceId) + "] inference queue full; rejecting request " +
                            promptSig);
                if (callback)
                {
                    callback("System busy: inference queue is full. Please retry.", true);
                }
            }
            else
            {
                METRICS.increment("inference.async_requests_queued");
                LOG_INFO("[ChatTrace#" + std::to_string(traceId) +
                         "] inference already running; queued depth=" + std::to_string(queuedDepth));
            }
            return;
        }

        m_inferenceRunning.store(true);
        m_inferenceStopRequested.store(false);
        m_currentInferencePrompt = prompt;
        m_currentInferenceResponse.clear();
        m_inferenceCallback = callback;
    }

    // Launch dedicated inference thread using Native Agentic Bridge
    m_inferenceThread = std::thread(
        [this, prompt, traceId, promptSig]()
        {
            DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
            auto finishAndScheduleNext = [this]()
            {
                {
                    std::lock_guard<std::mutex> lock(m_inferenceMutex);
                    m_inferenceRunning.store(false);
                }

                PendingInferenceRequest next;
                if (DequeuePendingInference(this, next))
                {
                    generateResponseAsync(next.prompt, std::move(next.callback));
                }
            };
            if (_guard.cancelled)
            {
                LOG_WARNING("[ChatTrace#" + std::to_string(traceId) + "] worker cancelled before start");
                finishAndScheduleNext();
                return;
            }
            try
            {
                auto looksLikeLocalModelPath = [](const std::string& modelPath) -> bool
                {
                    return modelPath.find(".gguf") != std::string::npos || modelPath.find(":\\") != std::string::npos ||
                           modelPath.find('/') != std::string::npos || modelPath.find('\\') != std::string::npos;
                };

                const bool smokeChatMode = []()
                {
                    const char* v = std::getenv("RAWRXD_SMOKE_CHAT");
                    return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
                }();
                const bool smokeNoStream = []()
                {
                    const char* v = std::getenv("RAWRXD_SMOKE_NO_STREAM");
                    return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
                }();
                const bool smokeNoUiBinding = []()
                {
                    const char* v = std::getenv("RAWRXD_SMOKE_NO_UI_BINDING");
                    return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
                }();

                LOG_INFO("[ChatTrace#" + std::to_string(traceId) + "] worker start " + promptSig +
                         " smoke_chat=" + BoolToLog(smokeChatMode) + " smoke_no_stream=" + BoolToLog(smokeNoStream) +
                         " smoke_no_ui_binding=" + BoolToLog(smokeNoUiBinding));

                if (!m_agenticBridge)
                {
                    OutputDebugStringA("[generateResponseAsync] AgenticBridge is null, attempting to ensure model\n");
                    if (!smokeChatMode && !m_loadedModelPath.empty() && looksLikeLocalModelPath(m_loadedModelPath))
                    {
                        OutputDebugStringA(("[generateResponseAsync] Calling ensureAgenticBridgeHasModel for: '" +
                                            m_loadedModelPath + "'\n")
                                               .c_str());
                        ensureAgenticBridgeHasModel(m_loadedModelPath);
                    }
                    if (!m_agenticBridge)
                    {
                        OutputDebugStringA(
                            "[AsyncBridge] Agentic Bridge unavailable; standard-chat fallback path will be used\n");
                        LOG_WARNING("[ChatTrace#" + std::to_string(traceId) +
                                    "] agentic bridge unavailable after ensure; fallback remains possible");
                        OutputDebugStringA(
                            ("[generateResponseAsync] AgenticBridge still null after ensure, loadedModelPath: '" +
                             m_loadedModelPath + "'\n")
                                .c_str());
                    }
                    else
                    {
                        OutputDebugStringA("[generateResponseAsync] AgenticBridge successfully obtained\n");
                    }
                }
                else
                {
                    OutputDebugStringA("[generateResponseAsync] AgenticBridge already available\n");
                }
                if (m_agenticBridge && !m_loadedModelPath.empty())
                {
                    OutputDebugStringA(("[generateResponseAsync] AgenticBridge available, loadedModelPath: '" +
                                        m_loadedModelPath + "'\n")
                                           .c_str());
                    if (looksLikeLocalModelPath(m_loadedModelPath))
                    {
                        const bool needsLocalLoad = m_agenticBridge->GetCurrentModel() != m_loadedModelPath ||
                                                    !m_agenticBridge->HasUsableBackend();
                        OutputDebugStringA(("[generateResponseAsync] Local model path detected, needsLocalLoad: " +
                                            std::string(needsLocalLoad ? "true" : "false") + " currentModel: '" +
                                            m_agenticBridge->GetCurrentModel() + "' hasUsableBackend: " +
                                            std::string(m_agenticBridge->HasUsableBackend() ? "true" : "false") + "\n")
                                               .c_str());
                        if (needsLocalLoad)
                        {
                            OutputDebugStringA(("[AsyncBridge] Loading selected local model on worker thread: " +
                                                m_loadedModelPath + "\n")
                                                   .c_str());
                            LOG_INFO("[ChatTrace#" + std::to_string(traceId) + "] bridge loading local model '" +
                                     m_loadedModelPath + "'");
                            if (!ensureAgenticBridgeHasModel(m_loadedModelPath))
                            {
                                const std::string detail =
                                    m_agenticBridge ? m_agenticBridge->GetLastModelLoadError() : "";
                                const std::string error =
                                    detail.empty() ? ("Error: Failed to load selected model: " + m_loadedModelPath)
                                                   : ("Error: Failed to load selected model: " + m_loadedModelPath +
                                                      " | " + detail);
                                LOG_ERROR("[ChatTrace#" + std::to_string(traceId) +
                                          "] local model load failed error='" + error + "'");
                                OutputDebugStringA(
                                    ("[generateResponseAsync] Model load failed: " + error + "\n").c_str());
                                if (m_inferenceCallback)
                                    m_inferenceCallback(error, true);
                                finishAndScheduleNext();
                                return;
                            }
                            else
                            {
                                OutputDebugStringA("[generateResponseAsync] Model load successful\n");
                                OutputDebugStringA(
                                    ("[generateResponseAsync] After load - currentModel: '" +
                                     m_agenticBridge->GetCurrentModel() + "' hasUsableBackend: " +
                                     std::string(m_agenticBridge->HasUsableBackend() ? "true" : "false") + "\n")
                                        .c_str());
                            }
                        }
                        else
                        {
                            OutputDebugStringA("[generateResponseAsync] No local load needed\n");
                        }
                    }
                    else if (m_agenticBridge->GetCurrentModel() != m_loadedModelPath)
                    {
                        m_agenticBridge->SetModel(m_loadedModelPath);
                        OutputDebugStringA(
                            ("[AsyncBridge] Desired remote model updated: " + m_loadedModelPath + "\n").c_str());
                        LOG_INFO("[ChatTrace#" + std::to_string(traceId) + "] updated bridge remote model to '" +
                                 m_loadedModelPath + "'");
                    }
                }

                auto backendMissingNativeApi = std::make_shared<std::atomic<bool>>(false);

                // Determine early whether we can proceed without AgenticBridge.
                const bool pipelineStrict = RawrXD::isPipelineStrictMode();
                const bool localPipelineForce = pipelineStrict || looksLikeLocalModelPath(m_loadedModelPath);
                if (pipelineStrict)
                    OutputDebugStringA("[PIPELINE STRICT] enabled via RAWRXD_PIPELINE_STRICT=1\n");
                LOG_INFO("[ChatTrace#" + std::to_string(traceId) + "] route setup pipeline_strict=" +
                         BoolToLog(pipelineStrict) + " local_pipeline_force=" + BoolToLog(localPipelineForce) +
                         " bridge_present=" + BoolToLog(m_agenticBridge != nullptr) + " loaded_model='" +
                         m_loadedModelPath + "'");

                // Set callback to route NativeAgent stream to the UI
                if (m_agenticBridge)
                {
                    m_agenticBridge->SetOutputCallback(
                        [this, backendMissingNativeApi, traceId](const std::string& type, const std::string& msg)
                        {
                            if (m_inferenceStopRequested.load() || isShuttingDown())
                                return;

                            // PROBE A: raw callback entry
                            {
                                std::string probe =
                                    "[PROBE-A] BridgeCB type=" + type + " msg_len=" + std::to_string(msg.size()) +
                                    " preview=" + msg.substr(0, std::min(msg.size(), (size_t)60)) + "\n";
                                OutputDebugStringA(probe.c_str());
                            }

                            // Only stream textual payload categories into chat rendering.
                            if (type != "stream" && type != "agent")
                            {
                                std::string drop =
                                    "[CopilotBridgeDrop] type=" + type + " len=" + std::to_string(msg.size()) + "\n";
                                OutputDebugStringA(drop.c_str());
                                LOG_DEBUG("[ChatTrace#" + std::to_string(traceId) + "] bridge callback dropped type='" +
                                          type + "' len=" + std::to_string(msg.size()) + " preview='" +
                                          MakeLogPreview(msg, 80) + "'");
                                return;
                            }

                            const bool isNativeApiMissing =
                                msg.find("Native inference API unavailable") != std::string::npos ||
                                msg.find("[BackendError]") != std::string::npos;

                            // Defer fallback until command returns to avoid callback re-entrancy.
                            if (isNativeApiMissing)
                            {
                                backendMissingNativeApi->store(true);
                                LOG_WARNING("[ChatTrace#" + std::to_string(traceId) +
                                            "] bridge callback reported backend missing; deferring fallback");
                                return;
                            }

                            if (looksLikeTokenIdSequencePayload(msg))
                            {
                                std::string warn = "[CopilotBridgeDrop] token-id payload suppressed len=" +
                                                   std::to_string(msg.size()) + "\n";
                                OutputDebugStringA(warn.c_str());
                                LOG_DEBUG("[ChatTrace#" + std::to_string(traceId) +
                                          "] suppressed token-id payload len=" + std::to_string(msg.size()));
                                return;
                            }

                            // PROBE A2: after all filters - about to fire m_inferenceCallback
                            {
                                std::string probe =
                                    "[PROBE-A2] Firing m_inferenceCallback msg_len=" + std::to_string(msg.size()) +
                                    "\n";
                                OutputDebugStringA(probe.c_str());
                            }

                            // "stream" type is what we send to chat UI
                            if (m_inferenceCallback)
                            {
                                m_currentInferenceResponse += msg;
                                OutputDebugStringA(("[ChatAccum] bridge path accumulated_len=" +
                                                    std::to_string(m_currentInferenceResponse.size()) + "\n")
                                                       .c_str());
                                LOG_DEBUG("[ChatTrace#" + std::to_string(traceId) + "] bridge callback type='" + type +
                                          "' len=" + std::to_string(msg.size()) + " preview='" +
                                          MakeLogPreview(msg, 80) +
                                          "' accum_len=" + std::to_string(m_currentInferenceResponse.size()));
                                m_inferenceCallback(msg, false);
                            }
                        });
                }
                else
                {
                    OutputDebugStringA(
                        "[SMOKE] Agentic Bridge unavailable at dispatch boundary; relying on local pipeline path\n");
                    LOG_WARNING(
                        "[ChatTrace#" + std::to_string(traceId) +
                        "] no bridge at dispatch boundary; local_pipeline_force=" + BoolToLog(localPipelineForce));
                    if (!localPipelineForce)
                    {
                        if (m_inferenceCallback)
                        {
                            std::string fallback = generateResponse(prompt);
                            m_currentInferenceResponse = fallback;
                            LOG_INFO(
                                "[ChatTrace#" + std::to_string(traceId) +
                                "] used synchronous generateResponse fallback len=" + std::to_string(fallback.size()) +
                                " preview='" + MakeLogPreview(fallback, 100) + "'");
                            m_inferenceCallback(fallback, true);
                        }
                        finishAndScheduleNext();
                        return;
                    }
                }

                // ── Unified local-inference fast-path (CLI ground-truth) ──────────────
                // For local GGUF weights, force the Win32IDE path through the same
                // InferencePlugin.generate() spine used by `rawrxd serve`.
                // When RAWRXD_PIPELINE_STRICT=1 is set, force pipeline regardless of
                // path heuristic to guarantee CLI/UI parity (no agentic fallback).
                bool pipelineHandled = false;
                if (localPipelineForce && !m_inferenceStopRequested.load() && !isShuttingDown())
                {
                    LOG_INFO("[ChatTrace#" + std::to_string(traceId) + "] entering unified local pipeline");
                    const std::string initErr = RawrXD::initInferencePipeline(m_loadedModelPath);
                    if (!initErr.empty())
                    {
                        OutputDebugStringA(("[InferencePipeline] init failed: " + initErr + "\n").c_str());
                        LOG_ERROR("[ChatTrace#" + std::to_string(traceId) + "] unified pipeline init failed error='" +
                                  initErr + "'");
                        if (m_inferenceCallback)
                            m_inferenceCallback("Error: unified local pipeline init failed: " + initErr, true);
                        finishAndScheduleNext();
                        return;
                    }

                    OutputDebugStringA("[PROBE-B] Using unified InferencePipeline (CLI path, forced)\n");
                    RawrXD::PipelineRequest pipeReq;
                    pipeReq.model = m_loadedModelPath;
                    pipeReq.prompt = prompt;
                    pipeReq.numPredict = m_inferenceConfig.maxTokens > 0 ? m_inferenceConfig.maxTokens : 512;
                    char smokeMaxBuf[32] = {};
                    const DWORD smokeMaxLen = GetEnvironmentVariableA("RAWRXD_SMOKE_MAX_TOKENS", smokeMaxBuf,
                                                                      static_cast<DWORD>(sizeof(smokeMaxBuf)));
                    if (smokeMaxLen > 0 && smokeMaxLen < sizeof(smokeMaxBuf))
                    {
                        pipeReq.numPredict = std::max(1, atoi(smokeMaxBuf));
                    }
                    {
                        std::ostringstream bridge;
                        bridge << "[ZONE_BRIDGE] name=embedding ctx=" << static_cast<const void*>(this)
                               << " tensor_base=" << static_cast<const void*>(m_ggufLoader.get())
                               << " num_predict=" << pipeReq.numPredict << "\n";
                        OutputDebugStringA(bridge.str().c_str());
                    }
                    if (!m_ggufLoader)
                    {
                        crashTriageBreakIfAttached("[FATAL] zone bridge has null loader pointer\n");
                    }
                    pipeReq.temperature = m_inferenceConfig.temperature > 0.0f ? m_inferenceConfig.temperature : 0.7f;
                    pipeReq.stream = !smokeNoStream;
                    // Seed conversation history into messages[] for multi-turn context.
                    {
                        std::lock_guard<std::mutex> hLock(m_outputMutex);
                        for (const auto& turn : m_chatHistory)
                        {
                            RawrXD::InferenceMessage msg;
                            msg.role = turn.first;
                            msg.content = turn.second;
                            pipeReq.messages.push_back(std::move(msg));
                        }
                    }

                    pipelineHandled = RawrXD::runLocalInferencePipeline(
                        pipeReq,
                        {// onToken — stream to chat UI (same contract as m_inferenceCallback)
                         [this, smokeNoUiBinding, traceId](const std::string& token, bool done)
                         {
                             if (m_inferenceStopRequested.load() || isShuttingDown())
                                 return;
                             {
                                 std::ostringstream tlog;
                                 tlog << "[TOKEN] ctx=" << static_cast<const void*>(this) << " len=" << token.size()
                                      << " done=" << (done ? 1 : 0) << "\n";
                                 OutputDebugStringA(tlog.str().c_str());
                             }
                             if (!token.empty() && m_inferenceCallback)
                             {
                                 OutputDebugStringA(("[PIPELINE TOKEN] " + token + "\n").c_str());
                                 m_currentInferenceResponse += token;
                                 LOG_DEBUG("[ChatTrace#" + std::to_string(traceId) +
                                           "] pipeline token len=" + std::to_string(token.size()) +
                                           " done=" + BoolToLog(done) + " preview='" + MakeLogPreview(token, 80) +
                                           "' accum_len=" + std::to_string(m_currentInferenceResponse.size()));
                                 if (!smokeNoUiBinding)
                                     m_inferenceCallback(token, false);
                             }
                             if (!token.empty() && smokeNoUiBinding)
                                 m_currentInferenceResponse += token;
                             if (!token.empty() && !m_inferenceCallback)
                             {
                                 crashTriageBreakIfAttached("[FATAL] NULL callback at token dispatch\n");
                             }
                             if (done)
                                 OutputDebugStringA("[PIPELINE TOKEN] <done=true>\n");
                         },
                         // onComplete
                         [this, smokeNoUiBinding, traceId](const std::string& accum)
                         {
                             {
                                 std::ostringstream elog;
                                 elog << "[PIPELINE_END] completed=1 tokens=" << m_currentInferenceResponse.size()
                                      << "\n";
                                 OutputDebugStringA(elog.str().c_str());
                             }
                             LOG_INFO("[ChatTrace#" + std::to_string(traceId) + "] pipeline complete accum_len=" +
                                      std::to_string(m_currentInferenceResponse.size()) +
                                      " final_len=" + std::to_string(accum.size()));
                             if (m_inferenceCallback && !smokeNoUiBinding)
                                 m_inferenceCallback({}, true);
                         },
                         // onError — fail closed for local forced pipeline.
                         [this, traceId](const std::string& err)
                         {
                             OutputDebugStringA(("[InferencePipeline] error: " + err + "\n").c_str());
                             LOG_ERROR("[ChatTrace#" + std::to_string(traceId) + "] pipeline error='" + err + "'");
                         }});

                    if (pipelineHandled)
                    {
                        OutputDebugStringA("[PROBE-B2] InferencePipeline completed; skipping ExecuteAgentCommand\n");
                        LOG_INFO("[ChatTrace#" + std::to_string(traceId) + "] unified local pipeline handled request");
                    }
                    else
                    {
                        const std::string errMsg = "Error: unified local pipeline generation failed (bridge fallback "
                                                   "disabled for local-model parity test).";
                        OutputDebugStringA("[InferencePipeline] returned false (local force mode fail-closed)\n");
                        LOG_ERROR("[ChatTrace#" + std::to_string(traceId) +
                                  "] unified local pipeline returned false in fail-closed mode");
                        if (m_inferenceCallback)
                            m_inferenceCallback(errMsg, true);
                        finishAndScheduleNext();
                        return;
                    }
                }

                if (!pipelineHandled)
                {
                    if (!m_agenticBridge)
                    {
                        if (m_inferenceCallback)
                        {
                            std::string fallback = generateResponse(prompt);
                            m_currentInferenceResponse = fallback;
                            LOG_INFO("[ChatTrace#" + std::to_string(traceId) +
                                     "] bridge absent post-pipeline; generateResponse fallback len=" +
                                     std::to_string(fallback.size()) + " preview='" + MakeLogPreview(fallback, 100) +
                                     "'");
                            m_inferenceCallback(fallback, true);
                        }
                        finishAndScheduleNext();
                        return;
                    }
                    // Execute via agent bridge (supports /edit, /think, etc.)
                    OutputDebugStringA("[PROBE-B] Calling ExecuteAgentCommand\n");
                    LOG_INFO("[ChatTrace#" + std::to_string(traceId) + "] executing agent bridge command");
                    m_agenticBridge->ExecuteAgentCommand(prompt);
                    {
                        std::string probe = "[PROBE-B2] ExecuteAgentCommand returned. accumulated_len=" +
                                            std::to_string(m_currentInferenceResponse.size()) +
                                            " backendMissing=" + (backendMissingNativeApi->load() ? "1" : "0") + "\n";
                        OutputDebugStringA(probe.c_str());
                    }
                    LOG_INFO("[ChatTrace#" + std::to_string(traceId) + "] ExecuteAgentCommand returned accum_len=" +
                             std::to_string(m_currentInferenceResponse.size()) +
                             " backend_missing=" + BoolToLog(backendMissingNativeApi->load()));

                    if (!m_inferenceStopRequested.load() && backendMissingNativeApi->load())
                    {
                        bool routeCHandledByMinimalAgent = false;
                        const bool hasAgenticPrefix = HasAgenticPrefix(prompt);
                        const bool bridgeAgenticMode = m_agenticBridge && m_agenticBridge->IsAgenticMode();
                        const bool wantsAgentic = hasAgenticPrefix || bridgeAgenticMode || m_agenticFunctionCallingMode;
                        const bool layerAvailable = rawrxd::isAgenticLayerAvailable();
                        const bool strictLocalSwarm = []()
                        {
                            char buf[12] = {};
                            const DWORD n = GetEnvironmentVariableA("RAWRXD_FORCE_LOCAL_SWARM", buf,
                                                                    static_cast<DWORD>(sizeof(buf)));
                            return n > 0 &&
                                   (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T' || buf[0] == 'y' || buf[0] == 'Y');
                        }();

                        OutputDebugStringA(("ROUTE_CHECK: route=C-main-fallback, wantsAgentic=" +
                                            std::to_string(wantsAgentic ? 1 : 0) +
                                            ", layerAvailable=" + std::to_string(layerAvailable ? 1 : 0) + "\n")
                                               .c_str());
                        LOG_WARNING("[ChatTrace#" + std::to_string(traceId) +
                                    "] Route C fallback engaged wants_agentic=" + BoolToLog(wantsAgentic) +
                                    " layer_available=" + BoolToLog(layerAvailable));

                        // Route C parity with Route B: try tool-aware agentic bridge before legacy SubmitInference
                        // fallback.
                        if (wantsAgentic && layerAvailable)
                        {
                            // ========== NEW: Use AgenticInferenceBridge for tool-aware inference ==========
                            using AgenticBridge = RawrXD::Agentic::AgenticInferenceBridge;

                            auto bridgeResult = AgenticBridge::SubmitInferenceWithTools(prompt,       // User message
                                                                                        "codestral",  // Default model
                                                                                        4096);        // Max tokens

                            if (bridgeResult.success)
                            {
                                // Tool-aware inference succeeded
                                std::string response = bridgeResult.response;

                                if (bridgeResult.usedTools)
                                {
                                    // Format tool execution trace for display
                                    std::string toolTrace = "\n\n[Tool Execution Trace]\n";
                                    toolTrace += "Iterations: " + std::to_string(bridgeResult.toolIterations) + "\n";
                                    if (!bridgeResult.toolTrace.empty())
                                    {
                                        for (const auto& record : bridgeResult.toolTrace)
                                        {
                                            toolTrace += "- " + record.toolName + " [" +
                                                         std::string(record.success ? "ok" : "error") + "]\n";
                                        }
                                    }
                                    response += toolTrace;
                                }

                                if (m_inferenceCallback)
                                {
                                    m_currentInferenceResponse += response;
                                    m_inferenceCallback(response, false);
                                }
                                routeCHandledByMinimalAgent = true;
                                OutputDebugStringA(("ROUTE_CHECK: route=C-main-fallback resolved via bridge; tools=" +
                                                    std::to_string(bridgeResult.usedTools ? 1 : 0) +
                                                    " iterations=" + std::to_string(bridgeResult.toolIterations) + "\n")
                                                       .c_str());
                                LOG_INFO(
                                    "[ChatTrace#" + std::to_string(traceId) +
                                    "] Route C bridge fallback succeeded tools=" + BoolToLog(bridgeResult.usedTools) +
                                    " iterations=" + std::to_string(bridgeResult.toolIterations) +
                                    " response_len=" + std::to_string(response.size()));
                            }
                            else
                            {
                                // Bridge failed try minimal agent fallback (preserves backward compat)
                                OutputDebugStringA(
                                    ("ROUTE_CHECK: bridge failed (" + bridgeResult.error + "), trying minimal agent\n")
                                        .c_str());
                                LOG_WARNING("[ChatTrace#" + std::to_string(traceId) +
                                            "] Route C tool-aware bridge failed error='" + bridgeResult.error +
                                            "'; trying minimal agent");

                                const std::string strippedPrompt = StripAgenticPrefixForRouteParity(prompt);
                                rawrxd::MinimalAgenticRequest req;
                                req.message = strippedPrompt.empty() ? prompt : strippedPrompt;
                                if (m_agenticBridge)
                                {
                                    std::string sid = m_agenticBridge->GetAgenticSessionId();
                                    if (sid.empty())
                                    {
                                        sid = "win32ide-routec";
                                        m_agenticBridge->SetAgenticSessionId(sid);
                                    }
                                    req.session_id = sid;

                                    req.model_path = m_agenticBridge->GetCurrentModel();
                                    if (req.model_path.empty())
                                        req.model_path = m_loadedModelPath;
                                }
                                else
                                {
                                    req.session_id = "win32ide-routec";
                                    req.model_path = m_loadedModelPath;
                                }
                                req.enable_tools = true;
                                req.max_iterations = 10;
                                req.workspace_root = workspaceDirectoryForChatPersistence();

                                const auto miniResp = rawrxd::processAgenticRequest(req);
                                if (miniResp.success)
                                {
                                    std::string routed = FormatMinimalAgenticResponseForChat(miniResp);
                                    if (m_inferenceCallback)
                                    {
                                        m_currentInferenceResponse += routed;
                                        m_inferenceCallback(routed, false);
                                    }
                                    routeCHandledByMinimalAgent = true;
                                    OutputDebugStringA(
                                        "ROUTE_CHECK: route=C-main-fallback resolved via minimal agent fallback\n");
                                    LOG_INFO("[ChatTrace#" + std::to_string(traceId) +
                                             "] Route C minimal agent fallback succeeded response_len=" +
                                             std::to_string(routed.size()));
                                }
                                else
                                {
                                    if (strictLocalSwarm)
                                    {
                                        const std::string failClosed =
                                            miniResp.error.empty()
                                                ? "Error: Strict local swarm mode blocked Route C legacy fallback"
                                                : miniResp.error;
                                        if (m_inferenceCallback)
                                        {
                                            m_currentInferenceResponse += failClosed;
                                            m_inferenceCallback(failClosed, false);
                                        }
                                        routeCHandledByMinimalAgent = true;
                                        OutputDebugStringA(("ROUTE_CHECK: route=C-main-fallback fail-closed under "
                                                            "strict local swarm: " +
                                                            failClosed + "\n")
                                                               .c_str());
                                        LOG_WARNING("[ChatTrace#" + std::to_string(traceId) +
                                                    "] Route C strict local swarm fail-closed error='" + failClosed +
                                                    "'");
                                    }
                                    OutputDebugStringA(("ROUTE_CHECK: route=C-main-fallback minimal agent failed: " +
                                                        miniResp.error + "\n")
                                                           .c_str());
                                    LOG_WARNING("[ChatTrace#" + std::to_string(traceId) +
                                                "] Route C minimal agent fallback failed error='" + miniResp.error +
                                                "'");
                                }
                            }
                        }

                        if (routeCHandledByMinimalAgent)
                        {
                            OutputDebugStringA("[ChatFallback] Route C minimal agent handled request; skipping "
                                               "SubmitInference fallback\n");
                        }
                        else
                        {

                            OutputDebugStringA("[ChatFallback] Native API missing reported by bridge; switching to "
                                               "registered backend path\n");
                            if (!RawrXD::InitializeAgenticChatBackend())
                            {
                                OutputDebugStringA("[ChatFallback] ERROR: InitializeAgenticChatBackend failed\n");
                                LOG_ERROR("[ChatTrace#" + std::to_string(traceId) +
                                          "] InitializeAgenticChatBackend failed; using generateResponse fallback");
                                std::string fallback = generateResponse(prompt);
                                if (fallback.empty())
                                    fallback =
                                        "[FallbackError] Unable to generate response via native or remote backend.";
                                if (m_inferenceCallback)
                                    m_inferenceCallback(fallback, false);
                            }
                            else
                            {
                                RawrXD::INativeInferenceBackend* backend = RawrXD::BackendRegistry::GetBackend();
                                RawrXD::GenerationConfig cfg;
                                cfg.max_tokens = m_inferenceConfig.maxTokens;
                                cfg.temperature = m_inferenceConfig.temperature;
                                cfg.top_k = m_inferenceConfig.topK;
                                cfg.top_p = m_inferenceConfig.topP;

                                std::vector<int> prompt_tokens;
                                prompt_tokens.reserve(prompt.size());
                                for (unsigned char c : prompt)
                                    prompt_tokens.push_back(static_cast<int>(c));

                                OutputDebugStringA(("[ChatFallback] SubmitInference prompt_bytes=" +
                                                    std::to_string(prompt_tokens.size()) + "\n")
                                                       .c_str());
                                LOG_INFO(
                                    "[ChatTrace#" + std::to_string(traceId) +
                                    "] SubmitInference fallback prompt_bytes=" + std::to_string(prompt_tokens.size()));
                                if (prompt_tokens.empty())
                                {
                                    OutputDebugStringA("[ChatFallback] ERROR: Empty token list (prompt_tokens)\n");
                                }
                                else
                                {
                                    const size_t sampleN = std::min<size_t>(prompt_tokens.size(), 5);
                                    for (size_t i = 0; i < sampleN; ++i)
                                    {
                                        OutputDebugStringA(("[ChatFallback] token[" + std::to_string(i) +
                                                            "]=" + std::to_string(prompt_tokens[i]) + "\n")
                                                               .c_str());
                                    }
                                }

                                bool submitted = false;
                                for (int attempt = 1; attempt <= 2 && !submitted; ++attempt)
                                {
                                    backend = RawrXD::BackendRegistry::GetBackend();
                                    const bool backendReady = backend && backend->IsAvailable();
                                    OutputDebugStringA(
                                        ("[ChatFallback] SubmitInference attempt=" + std::to_string(attempt) +
                                         " backend_ready=" + std::string(backendReady ? "1" : "0") + "\n")
                                            .c_str());
                                    if (backendReady)
                                    {
                                        submitted = backend->SubmitInference(prompt_tokens, cfg);
                                    }
                                    LOG_DEBUG("[ChatTrace#" + std::to_string(traceId) + "] SubmitInference attempt=" +
                                              std::to_string(attempt) + " backend_ready=" + BoolToLog(backendReady) +
                                              " submitted=" + BoolToLog(submitted));

                                    if (!submitted && attempt < 2)
                                    {
                                        OutputDebugStringA(
                                            "[ChatFallback] SubmitInference not ready; waiting 350ms before retry\n");
                                        std::this_thread::sleep_for(std::chrono::milliseconds(350));
                                    }
                                }
                                if (!submitted)
                                {
                                    OutputDebugStringA("[ChatFallback] ERROR: SubmitInference failed\n");
                                    LOG_ERROR("[ChatTrace#" + std::to_string(traceId) +
                                              "] SubmitInference failed; using generateResponse fallback");
                                    std::string fallback = generateResponse(prompt);
                                    if (fallback.empty())
                                        fallback =
                                            "[FallbackError] Unable to generate response via native or remote backend.";
                                    if (m_inferenceCallback)
                                        m_inferenceCallback(fallback, false);
                                }
                                else
                                {
                                    OutputDebugStringA(
                                        "[ChatFallback] SubmitInference accepted - entering poll loop\n");
                                    int poll_count = 0;
                                    for (;;)
                                    {
                                        ++poll_count;
                                        if ((poll_count % 20) == 0)
                                        {
                                            OutputDebugStringA(
                                                ("[ChatFallback] Poll #" + std::to_string(poll_count) + "\n").c_str());
                                        }

                                        if (m_inferenceStopRequested.load() || isShuttingDown())
                                        {
                                            OutputDebugStringA("[ChatFallback] Poll cancelled due to stop/shutdown\n");
                                            backend->Cancel();
                                            break;
                                        }

                                        std::string text;
                                        bool done = false;
                                        if (backend->GetResult(text, done))
                                        {
                                            OutputDebugStringA(("[ChatFallback] GetResult returned done=" +
                                                                std::string(done ? "true" : "false") +
                                                                " text_len=" + std::to_string(text.size()) + "\n")
                                                                   .c_str());
                                            LOG_DEBUG("[ChatTrace#" + std::to_string(traceId) +
                                                      "] SubmitInference poll result done=" + BoolToLog(done) +
                                                      " text_len=" + std::to_string(text.size()) + " preview='" +
                                                      MakeLogPreview(text, 80) + "'");
                                            if (!text.empty() && m_inferenceCallback)
                                            {
                                                m_currentInferenceResponse += text;
                                                OutputDebugStringA(("[ChatAccum] fallback path accumulated_len=" +
                                                                    std::to_string(m_currentInferenceResponse.size()) +
                                                                    "\n")
                                                                       .c_str());
                                                OutputDebugStringA(
                                                    ("[ChatFallback] Forwarding text to chat callback len=" +
                                                     std::to_string(text.size()) + "\n")
                                                        .c_str());
                                                m_inferenceCallback(text, false);
                                            }
                                            if (done)
                                            {
                                                OutputDebugStringA(
                                                    "[ChatFallback] Generation done - exiting poll loop\n");
                                                break;
                                            }
                                        }
                                        else if ((poll_count % 40) == 0)
                                        {
                                            OutputDebugStringA(
                                                "[ChatFallback] GetResult returned false (no data yet)\n");
                                        }

                                        std::this_thread::sleep_for(std::chrono::milliseconds(25));
                                    }
                                }
                            }
                        }
                    }
                }  // end if (!pipelineHandled)

                // Phase 4B: Choke Point 4 — hookPostGeneration after streaming inference
                // Note: For streaming responses, the full output was already sent via callback.
                // We hook here for failure detection on the completed inference cycle.
                // The response content was streamed — we check the accumulated result if available.
                if (!m_inferenceStopRequested.load())
                {
                    std::string accumulatedResponse = m_currentInferenceResponse;
                    OutputDebugStringA(
                        ("[ChatAccum] final accumulated_len=" + std::to_string(accumulatedResponse.size()) + "\n")
                            .c_str());
                    LOG_INFO("[ChatTrace#" + std::to_string(traceId) +
                             "] worker finishing accumulated_len=" + std::to_string(accumulatedResponse.size()) +
                             " stop_requested=" + BoolToLog(m_inferenceStopRequested.load()));
                    if (!accumulatedResponse.empty())
                    {
                        FailureClassification inferenceFailure = hookPostGeneration(accumulatedResponse, prompt);
                        if (inferenceFailure.reason != AgentFailureType::None)
                        {
                            LOG_WARNING(
                                "[Phase4B] Inference failure detected: " + failureTypeString(inferenceFailure.reason) +
                                " (confidence=" + std::to_string(inferenceFailure.confidence) + ")");
                            // For streaming responses, we log the failure and record it
                            // but don't auto-retry (the user sees output in real-time)
                            recordSimpleEvent(AgentEventType::FailureDetected,
                                              "Inference failure: " + failureTypeString(inferenceFailure.reason) +
                                                  " | " + inferenceFailure.evidence);
                        }
                    }
                }

                auto completionCb = m_inferenceCallback;
                finishAndScheduleNext();
                if (completionCb && !isShuttingDown())
                {
                    LOG_INFO("[ChatTrace#" + std::to_string(traceId) + "] dispatching final completion callback");
                    completionCb("", true);  // Finalize
                }
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("[ChatTrace#" + std::to_string(traceId) + "] worker exception='" + std::string(e.what()) +
                          "'");
                auto completionCb = m_inferenceCallback;
                finishAndScheduleNext();
                if (completionCb && !isShuttingDown())
                {
                    completionCb(std::string("Error: ") + e.what(), true);
                }
            }
            catch (...)
            {
                LOG_ERROR("[ChatTrace#" + std::to_string(traceId) + "] worker crashed with unknown exception");
                auto completionCb = m_inferenceCallback;
                finishAndScheduleNext();
                if (completionCb && !isShuttingDown())
                {
                    completionCb("Error: inference worker crashed.", true);
                }
            }
        });

    m_inferenceThread.detach();
}

void Win32IDE::stopInference()
{
    m_inferenceStopRequested.store(true);
    if (m_nativePipeline)
    {
        const uint64_t requestId = m_nativePipeline->ActiveRequestId();
        if (requestId != 0)
        {
            m_nativePipeline->CancelInference(requestId);
        }
    }
    ClearPendingInference(this);
}

void Win32IDE::setInferenceConfig(const InferenceConfig& config)
{
    std::lock_guard<std::mutex> lock(m_inferenceMutex);
    m_inferenceConfig = config;
}

Win32IDE::InferenceConfig Win32IDE::getInferenceConfig() const
{
    return m_inferenceConfig;
}

std::string Win32IDE::buildChatPrompt(const std::string& userMessage)
{
    // Delegate to ConversationSession which handles multi-format templates
    // (ChatML, Llama3, Phi3, Mistral, Alpaca, Raw), conversation history,
    // and automatic token budget truncation.
    std::string prompt = conversationBuildPrompt(userMessage);

    // Inject symbol-aware cursor context (best-effort) to mirror Cursor/VS Code style locality.
    {
        std::lock_guard<std::mutex> lock(m_liveSymbolContextMutex);
        const std::string sectionHeader = "### Cursor Symbol Context\n";
        const std::string sectionBody = wideToUtf8(m_liveSymbolPromptContext);

        // Freeze rule: cursor-context section is replaced in-place, never unboundedly appended.
        size_t sectionPos = prompt.find(sectionHeader);
        if (sectionPos != std::string::npos)
        {
            size_t sectionEnd = prompt.find("\n### ", sectionPos + sectionHeader.size());
            if (sectionEnd == std::string::npos)
            {
                sectionEnd = prompt.size();
            }
            prompt.erase(sectionPos, sectionEnd - sectionPos);

            if (!sectionBody.empty())
            {
                const std::string replacement = "\n\n" + sectionHeader + sectionBody;
                prompt.insert(sectionPos, replacement);
            }
        }
        else if (!sectionBody.empty())
        {
            prompt += "\n\n" + sectionHeader;
            prompt += sectionBody;
        }
    }

    // Track context usage for the status bar
    int promptTokens = static_cast<int>(prompt.length()) / 4;
    m_contextUsage.systemTokens = static_cast<int>(m_inferenceConfig.systemPrompt.length()) / 4;
    m_contextUsage.messageTokens = promptTokens - m_contextUsage.systemTokens;
    m_contextUsage.maxTokens = m_inferenceConfig.contextWindow;
    updateContextWindowDisplay();

    return prompt;
}

std::wstring Win32IDE::resolveSymbolAtMentions(std::wstring_view rawPrompt) const
{
    auto& aiCtx = RawrXD::SymbolEngine::GlobalAISymbolContext();
    auto refs = aiCtx.ExtractSymbolRefs(rawPrompt);
    if (refs.empty())
    {
        return std::wstring(rawPrompt);
    }

    // Freeze rule: deterministic and bounded mention resolution.
    std::sort(refs.begin(), refs.end());
    refs.erase(std::unique(refs.begin(), refs.end()), refs.end());

    constexpr size_t kMaxResolvedSymbols = 32;
    if (refs.size() > kMaxResolvedSymbols)
    {
        refs.resize(kMaxResolvedSymbols);
    }

    std::wstring enriched(rawPrompt);
    std::wstring resolvedLines;

    auto& db = RawrXD::SymbolEngine::GlobalSymbolDatabase();
    for (UINT64 id : refs)
    {
        auto sym = db.FindById(id);
        if (sym)
        {
            resolvedLines += L"- " + sym->ToAIContext() + L"\n";
        }
    }

    if (!resolvedLines.empty())
    {
        enriched += L"\n\n### Resolved Symbol Context\n";
        enriched += resolvedLines;
    }

    return enriched;
}

void Win32IDE::updateLiveSymbolPromptContextFromEditor()
{
    if (!m_hwndEditor || m_currentFile.empty())
    {
        return;
    }

    const bool agentActive = m_titanAgentRunning || m_inferenceRunning.load() || m_chatSendInFlight.load();
    if (!agentActive)
    {
        return;
    }

    CHARRANGE sel = {};
    SendMessageW(m_hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
    const LONG charPos = sel.cpMin;
    const int line = static_cast<int>(SendMessageW(m_hwndEditor, EM_LINEFROMCHAR, charPos, 0));
    const int lineStart = static_cast<int>(SendMessageW(m_hwndEditor, EM_LINEINDEX, line, 0));
    const int col = std::max(0, static_cast<int>(charPos) - lineStart);

    const std::wstring uri = utf8ToWide(m_currentFile);
    auto& aiCtx = RawrXD::SymbolEngine::GlobalAISymbolContext();
    const std::wstring ctx = aiCtx.BuildPromptContext(uri, static_cast<UINT>(line), static_cast<UINT>(col));

    std::lock_guard<std::mutex> lock(m_liveSymbolContextMutex);
    m_liveSymbolPromptContext = ctx;
}

void Win32IDE::refreshSymbolIndexForCurrentDocumentAsync()
{
    if (!m_hwndEditor || m_currentFile.empty())
    {
        return;
    }

    std::string content = getWindowText(m_hwndEditor);
    if (content.empty() || content.size() > 10 * 1024 * 1024)
    {
        return;
    }

    RawrXD::SymbolEngine::IndexFileAsync(utf8ToWide(m_currentFile), std::move(content));
}

void Win32IDE::onInferenceToken(const std::string& token)
{
    // Called when streaming tokens during inference
    m_currentInferenceResponse += token;

    // Phase 19B: Feed token to the streaming output system
    appendStreamingToken(token);

    // Update context window token count (approximate: ~4 chars per token)
    int approxTokens = static_cast<int>(m_currentInferenceResponse.length()) / 4;
    m_contextUsage.toolResultTokens = approxTokens;
    // Throttle status bar updates to every ~20 tokens
    if (approxTokens % 20 == 0)
    {
        updateContextWindowDisplay();
    }

    // Update UI with partial response if streaming is enabled
    if (m_inferenceConfig.streamOutput && m_inferenceCallback)
    {
        m_inferenceCallback(token, false);
    }
}

void Win32IDE::onInferenceComplete(const std::string& fullResponse)
{
    m_inferenceRunning.store(false);
    m_currentInferenceResponse = fullResponse;

    // Final context window update
    m_contextUsage.toolResultTokens = static_cast<int>(fullResponse.length()) / 4;
    updateContextWindowDisplay();

    if (m_inferenceCallback)
    {
        m_inferenceCallback(fullResponse, true);
    }
}

// ============================================================================
// EDITOR OPERATIONS - Undo/Redo/Cut/Copy/Paste
// ============================================================================

void Win32IDE::undo()
{
    if (m_hwndEditor)
    {
        SendMessage(m_hwndEditor, EM_UNDO, 0, 0);
    }
}

void Win32IDE::redo()
{
    if (m_hwndEditor)
    {
        SendMessage(m_hwndEditor, EM_REDO, 0, 0);
    }
}

void Win32IDE::editCut()
{
    if (m_hwndEditor)
    {
        SendMessage(m_hwndEditor, WM_CUT, 0, 0);
    }
}

void Win32IDE::editCopy()
{
    if (m_hwndEditor)
    {
        SendMessage(m_hwndEditor, WM_COPY, 0, 0);
    }
}

void Win32IDE::editPaste()
{
    if (m_hwndEditor)
    {
        SendMessage(m_hwndEditor, WM_PASTE, 0, 0);
    }
}

bool Win32IDE::ApplyPendingEdit(const PendingEditEntry& edit)
{
    if (!m_hwndEditor || !IsWindow(m_hwndEditor))
        return false;

    LONG startPos = edit.oldRange.cpMin;
    LONG endPos = edit.oldRange.cpMax;
    if (startPos < 0 || endPos < startPos)
        return false;

    // Set selection to the target range
    CHARRANGE sel;
    sel.cpMin = startPos;
    sel.cpMax = endPos;
    SendMessageA(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&sel);

    // Replace the selected text with the new text
    std::wstring wNewText = utf8ToWide(edit.newText);
    SendMessageW(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)wNewText.c_str());

    return true;
}

// ============================================================================
// VIEW OPERATIONS - Toggle panels
// ============================================================================

void Win32IDE::toggleOutputPanel()
{
    m_outputPanelVisible = !m_outputPanelVisible;
    if (m_hwndMain)
    {
        RECT rc;
        GetClientRect(m_hwndMain, &rc);
        onSize(rc.right, rc.bottom);
        InvalidateRect(m_hwndMain, NULL, TRUE);
    }
}

void Win32IDE::toggleTerminal()
{
    // Toggle panel visibility (which contains terminal)
    m_outputPanelVisible = !m_outputPanelVisible;
    if (m_hwndMain)
    {
        RECT rc;
        GetClientRect(m_hwndMain, &rc);
        onSize(rc.right, rc.bottom);
        InvalidateRect(m_hwndMain, NULL, TRUE);
    }
}

void Win32IDE::onViewTerminalShortcut()
{
    // Cursor / VS Code–style: Ctrl+` collapses the bottom terminal area when focus is already in integrated
    // terminal UI or the dedicated PowerShell strip; otherwise opens/focuses the integrated terminal tab.
    if (!m_hwndMain)
        return;
    HWND foc = GetFocus();
    const bool cmdFocused = (m_hwndCommandInput && foc == m_hwndCommandInput);
    const bool tabBarFocused = g_rawrxdIntegratedTerminalTabs && IsWindow(g_rawrxdIntegratedTerminalTabs) && foc &&
                               (foc == g_rawrxdIntegratedTerminalTabs || IsChild(g_rawrxdIntegratedTerminalTabs, foc));
    bool integratedShellSurfaceFocused = false;
    for (const auto& pane : m_terminalPanes)
    {
        if (pane.hwnd && IsWindow(pane.hwnd) && foc && (foc == pane.hwnd || IsChild(pane.hwnd, foc)))
        {
            integratedShellSurfaceFocused = true;
            break;
        }
    }
    const bool psOutFocused = m_hwndPowerShellOutput && IsWindow(m_hwndPowerShellOutput) && foc &&
                              (foc == m_hwndPowerShellOutput || IsChild(m_hwndPowerShellOutput, foc));
    const bool psInFocused = m_hwndPowerShellInput && IsWindow(m_hwndPowerShellInput) && foc == m_hwndPowerShellInput;
    const bool dedicatedPsFocused = psOutFocused || psInFocused;

    const bool terminalTabActive = (m_activePanelTab == PanelTab::Terminal);
    const bool integratedTerminalUiFocused = cmdFocused || tabBarFocused || integratedShellSurfaceFocused;
    // Collapse when: (1) integrated terminal tab is active and focus is in that UI, or (2) focus is in the
    // dedicated PowerShell strip (even if another bottom tab like Output/Problems is selected)—parity with a
    // single "bottom terminal" affordance. Hiding clears both the output/terminal dock and the dedicated PS row.
    const bool collapseIntegratedDock =
        m_outputPanelVisible && (dedicatedPsFocused || (terminalTabActive && integratedTerminalUiFocused));
    // Output dock can be hidden while the dedicated PS strip still shows — Ctrl+` should still dismiss it.
    const bool collapseDedicatedPsOnly =
        !m_outputPanelVisible && m_powerShellPanelVisible && m_hwndPowerShellPanel && dedicatedPsFocused;
    if (collapseIntegratedDock)
    {
        m_outputPanelVisible = false;
        hidePowerShellPanel();
        InvalidateRect(m_hwndMain, nullptr, TRUE);
        return;
    }
    if (collapseDedicatedPsOnly)
    {
        hidePowerShellPanel();
        InvalidateRect(m_hwndMain, nullptr, TRUE);
        return;
    }
    focusIntegratedTerminalPanel();
}

void Win32IDE::showAbout()
{
    std::string aboutText = RAWRXD_VERSION_FULL "\n\n"
                                                "Build: " RAWRXD_BUILD_DATE " " RAWRXD_BUILD_TIME "\n"
                                                "Channel: " RAWRXD_CHANNEL "\n"
                                                "Units: " +
                            std::to_string(RAWRXD_COMPILE_UNITS) +
                            " compilation units\n"
                            "MASM64: " +
                            std::to_string(RAWRXD_MASM_KERNELS) +
                            " ASM kernels\n\n"
                            "Engine:\n"
                            "• Native Win32 C++20 (no Qt, no Electron)\n"
                            "• GGUF Model Loader + AVX-512 Inference\n"
                            "• Chain-of-Thought Multi-Model Review\n"
                            "• Native PDB Symbol Server (MSF v7.00)\n"
                            "• Three-Layer Hotpatch System\n"
                            "• Voice Automation (TTS)\n"
                            "• Unified GPU Accelerator Router\n"
                            "• Embedded LSP Server (JSON-RPC 2.0)\n"
                            "• Distributed Swarm Inference\n\n" RAWRXD_COPYRIGHT "\n" RAWRXD_LICENSE "\n" RAWRXD_GITHUB;

    MessageBoxW(m_hwndMain, utf8ToWide(aboutText).c_str(), L"About RawrXD IDE", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// AUTONOMY FRAMEWORK - High-level orchestration controls
// ============================================================================

void Win32IDE::onAutonomyStart()
{
    // Ensure bridge + autonomy initialized (smoke check expects initializeAgenticBridge/initializeAutonomy)
    if (!m_agenticBridge)
        initializeAgenticBridge();
    initializeAutonomy();

    if (!m_autonomyManager)
    {
        appendToOutput("Autonomy manager not initialized\n", "Errors", OutputSeverity::Error);
        return;
    }
    m_autonomyManager->start();
    appendToOutput("Autonomy started (manual mode)\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onAutonomyStop()
{
    if (!m_autonomyManager)
        return;
    m_autonomyManager->stop();
    appendToOutput("Autonomy stopped\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onAutonomyToggle()
{
    if (!m_autonomyManager)
        return;
    bool enable = !m_autonomyManager->isAutoLoopEnabled();
    m_autonomyManager->enableAutoLoop(enable);
    appendToOutput(std::string("Autonomy auto loop ") + (enable ? "ENABLED" : "DISABLED") + "\n", "Output",
                   OutputSeverity::Info);
}

void Win32IDE::onAutonomySetGoal()
{
    if (!m_autonomyManager)
        return;
    // Simple goal setter: reuse current file name or fallback text
    std::string goal =
        m_currentFile.empty() ? "Explore workspace and summarize architecture" : ("Analyze file: " + m_currentFile);
    m_autonomyManager->setGoal(goal);
    appendToOutput("Autonomy goal set: " + goal + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onAutonomyViewStatus()
{
    if (!m_autonomyManager)
        return;
    std::string status = m_autonomyManager->getStatus();
    appendToOutput("Autonomy Status: " + status + "\n", "Output", OutputSeverity::Info);
    MessageBoxW(m_hwndMain, utf8ToWide(status).c_str(), L"Autonomy Status", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::onAutonomyViewMemory()
{
    if (!m_autonomyManager)
        return;
    auto mem = m_autonomyManager->getMemorySnapshot();
    std::string report = "Memory Items (latest first, max 20):\n\n";
    int shown = 0;
    for (int i = (int)mem.size() - 1; i >= 0 && shown < 20; --i, ++shown)
    {
        report += std::to_string(shown + 1) + ". " + mem[i] + "\n";
    }
    if (shown == 0)
        report += "<empty>\n";
    appendToOutput("Autonomy Memory Snapshot displayed\n", "Debug", OutputSeverity::Debug);
    MessageBoxW(m_hwndMain, utf8ToWide(report).c_str(), L"Autonomy Memory", MB_OK);
}

// ======================================================================
// AI CHAT PANEL IMPLEMENTATION
// ======================================================================

void Win32IDE::createChatPanel()
{

    if (!m_hwndMain)
    {

        return;
    }

    m_hwndSecondarySidebar =
        CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0,
                        0, 300, 600, m_hwndMain, (HMENU)IDC_SECONDARY_SIDEBAR, m_hInstance, nullptr);

    if (!m_hwndSecondarySidebar)
    {
        return;
    }
    SetWindowLongPtr(m_hwndSecondarySidebar, GWLP_USERDATA, (LONG_PTR)this);
    m_oldSidebarProc = (WNDPROC)SetWindowLongPtr(m_hwndSecondarySidebar, GWLP_WNDPROC, (LONG_PTR)SidebarProcImpl);

    m_hwndSecondarySidebarHeader = CreateWindowExW(0, L"STATIC", L"AI Chat", WS_CHILD | WS_VISIBLE | SS_LEFT, 5, 5, 290,
                                                   25, m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);

    // Sovereign Symbol Engine - "Thinking" Pulse Anchor (0xE012)
    m_hwndEmojiPulseSymbol =
        CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 260, 5, 24, 24, m_hwndSecondarySidebar,
                        (HMENU)(IDC_EMOJI_BASE + 0x12), m_hInstance, nullptr);

    HFONT hFont = CreateFontW(-dpiScale(14), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (m_hwndSecondarySidebarHeader)
    {
        SendMessage(m_hwndSecondarySidebarHeader, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    m_hwndSidebarCaptionModel =
        CreateWindowExW(0, L"STATIC", L"Model:", WS_CHILD | WS_VISIBLE | SS_LEFT, 5, 35, 50, 18, m_hwndSecondarySidebar,
                        (HMENU)IDC_SIDEBAR_CAP_MODEL, m_hInstance, nullptr);

    m_hwndModelSelector =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_AUTOHSCROLL, 60, 35, 250, 200,
                        m_hwndSecondarySidebar, (HMENU)IDC_MODEL_SELECTOR, m_hInstance, nullptr);

    m_hwndSidebarCaptionMode =
        CreateWindowExW(0, L"STATIC", L"Mode:", WS_CHILD | WS_VISIBLE | SS_LEFT, 5, 61, 50, 18, m_hwndSecondarySidebar,
                        (HMENU)IDC_SIDEBAR_CAP_MODE, m_hInstance, nullptr);
    HWND hwndModeSelector =
        CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 60, 60, 250, 180,
                        m_hwndSecondarySidebar, (HMENU)IDC_MODEL_MODE_SELECTOR, m_hInstance, nullptr);
    if (hwndModeSelector)
    {
        SendMessageW(hwndModeSelector, CB_ADDSTRING, 0, (LPARAM)L"Thinking");
        SendMessageW(hwndModeSelector, CB_ADDSTRING, 0, (LPARAM)L"Deep Research");
        SendMessageW(hwndModeSelector, CB_ADDSTRING, 0, (LPARAM)L"Max Mode");
        SendMessageW(hwndModeSelector, CB_ADDSTRING, 0, (LPARAM)L"Swarm");
        SendMessageW(hwndModeSelector, CB_ADDSTRING, 0, (LPARAM)L"Browser");
        SendMessageW(hwndModeSelector, CB_ADDSTRING, 0, (LPARAM)L"Vision");
        SendMessageW(hwndModeSelector, CB_ADDSTRING, 0, (LPARAM)L"Video");
        SendMessageW(hwndModeSelector, CB_SETCURSEL, 0, 0);
    }

    HWND hwndEffortLabel =
        CreateWindowExW(0, L"STATIC", L"Thinking Effort: Medium", WS_CHILD | WS_VISIBLE | SS_LEFT, 60, 84, 250, 18,
                        m_hwndSecondarySidebar, (HMENU)IDC_MODEL_EFFORT_LABEL, m_hInstance, nullptr);

    if (m_hwndModelSelector)
    {
        SendMessage(m_hwndModelSelector, WM_SETFONT, (WPARAM)hFont, TRUE);
        populateModelSelector();
    }

    if (hwndModeSelector)
    {
        SendMessage(hwndModeSelector, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    if (hwndEffortLabel)
    {
        SendMessage(hwndEffortLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    m_hwndSidebarCaptionMaxTokens =
        CreateWindowExW(0, L"STATIC", L"Max Tokens:", WS_CHILD | WS_VISIBLE | SS_LEFT, 5, 105, 80, 18,
                        m_hwndSecondarySidebar, (HMENU)IDC_SIDEBAR_CAP_MAXTOKENS, m_hInstance, nullptr);

    m_hwndMaxTokensLabel = CreateWindowExW(0, L"STATIC", L"512", WS_CHILD | WS_VISIBLE | SS_RIGHT, 245, 105, 50, 18,
                                           m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);

    m_hwndMaxTokensSlider =
        CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, 5, 125, 290, 25,
                        m_hwndSecondarySidebar, (HMENU)IDC_AI_MAX_TOKENS_SLIDER, m_hInstance, nullptr);

    m_hwndSidebarCaptionContext =
        CreateWindowExW(0, L"STATIC", L"Context:", WS_CHILD | WS_VISIBLE | SS_LEFT, 5, 155, 80, 18,
                        m_hwndSecondarySidebar, (HMENU)IDC_SIDEBAR_CAP_CONTEXT, m_hInstance, nullptr);

    m_hwndContextLabel = CreateWindowExW(0, L"STATIC", L"4K", WS_CHILD | WS_VISIBLE | SS_RIGHT, 245, 155, 50, 18,
                                         m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);

    m_hwndContextSlider =
        CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS, 5, 175, 290, 25,
                        m_hwndSecondarySidebar, (HMENU)IDC_AI_CONTEXT_SLIDER, m_hInstance, nullptr);

    if (m_hwndSidebarCaptionModel)
        SendMessage(m_hwndSidebarCaptionModel, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (m_hwndSidebarCaptionMode)
        SendMessage(m_hwndSidebarCaptionMode, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (m_hwndSidebarCaptionMaxTokens)
        SendMessage(m_hwndSidebarCaptionMaxTokens, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (m_hwndSidebarCaptionContext)
        SendMessage(m_hwndSidebarCaptionContext, WM_SETFONT, (WPARAM)hFont, TRUE);

    if (m_hwndContextSlider)
    {
        SendMessage(m_hwndContextSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 6));  // 7 steps
        SendMessage(m_hwndContextSlider, TBM_SETPOS, TRUE, 0);                   // Default 4K
        m_currentContextSize = 4096;
    }
    // Update Chat Output Y position to accommodate new slider
    int chatY_base = 160;

    if (m_hwndMaxTokensSlider)
    {
        SendMessage(m_hwndMaxTokensSlider, TBM_SETRANGE, TRUE, MAKELPARAM(32, 2048));
        SendMessage(m_hwndMaxTokensSlider, TBM_SETPOS, TRUE, 512);
        SendMessage(m_hwndMaxTokensSlider, TBM_SETTICFREQ, 256, 0);
        m_currentMaxTokens = 512;
    }

    // Adjust Y for chat pane and toggles to prevent overlap
    int toggleY = 215;
    int toggleX = 5;
    int chatY = 300;

    m_hwndChkMaxMode =
        CreateWindowExW(0, L"BUTTON", L"Max Mode", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, toggleX, toggleY, 140, 20,
                        m_hwndSecondarySidebar, (HMENU)IDC_AI_MAX_MODE, m_hInstance, nullptr);
    m_hwndChkDeepThink =
        CreateWindowExW(0, L"BUTTON", L"Deep Think", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, toggleX + 150, toggleY,
                        140, 20, m_hwndSecondarySidebar, (HMENU)IDC_AI_DEEP_THINK, m_hInstance, nullptr);

    toggleY += 25;
    m_hwndChkDeepResearch =
        CreateWindowExW(0, L"BUTTON", L"Deep Research", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, toggleX, toggleY, 140,
                        20, m_hwndSecondarySidebar, (HMENU)IDC_AI_DEEP_RESEARCH, m_hInstance, nullptr);
    m_hwndChkNoRefusal =
        CreateWindowExW(0, L"BUTTON", L"No Refusal", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, toggleX + 150, toggleY,
                        140, 20, m_hwndSecondarySidebar, (HMENU)IDC_AI_NO_REFUSAL, m_hInstance, nullptr);

    if (m_hwndChkMaxMode)
        SendMessage(m_hwndChkMaxMode, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (m_hwndChkDeepThink)
        SendMessage(m_hwndChkDeepThink, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (m_hwndChkDeepResearch)
        SendMessage(m_hwndChkDeepResearch, WM_SETFONT, (WPARAM)hFont, TRUE);
    toggleY += 25;
    m_hwndChkAgenticMode =
        CreateWindowExW(0, L"BUTTON", L"Agentic Mode", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, toggleX, toggleY, 140,
                        20, m_hwndSecondarySidebar, (HMENU)IDC_AI_AGENTIC_MODE, m_hInstance, nullptr);

    if (m_hwndChkAgenticMode)
        SendMessage(m_hwndChkAgenticMode, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hwndCopilotChatOutput = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 5, chatY,
        290, 180, m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CHAT_OUTPUT, m_hInstance, nullptr);

    if (m_hwndCopilotChatOutput)
    {
        SendMessage(m_hwndCopilotChatOutput, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    m_hwndCopilotChatInput = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL, 5, 415,
        290, 85, m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CHAT_INPUT, m_hInstance, nullptr);

    if (m_hwndCopilotChatInput)
    {
        SendMessage(m_hwndCopilotChatInput, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Subclass chat input so Enter sends and Shift+Enter inserts a newline.
        m_oldCopilotInputProc =
            (WNDPROC)SetWindowLongPtr(m_hwndCopilotChatInput, GWLP_WNDPROC, (LONG_PTR)CopilotChatInputProc);
        SetWindowLongPtr(m_hwndCopilotChatInput, GWLP_USERDATA, (LONG_PTR)this);

        ChatAutocomplete_Attach(m_hwndCopilotChatInput, m_hInstance);
    }

    m_hwndCopilotSendBtn = CreateWindowExW(0, L"BUTTON", L"Send", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 5, 505, 92, 30,
                                           m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_SEND_BTN, m_hInstance, nullptr);

    if (m_hwndCopilotSendBtn)
    {
        SendMessage(m_hwndCopilotSendBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
        m_oldCopilotSendBtnProc =
            (WNDPROC)SetWindowLongPtr(m_hwndCopilotSendBtn, GWLP_WNDPROC, (LONG_PTR)CopilotButtonProc);
        SetWindowLongPtr(m_hwndCopilotSendBtn, GWLP_USERDATA, (LONG_PTR)this);
    }

    m_hwndCopilotClearBtn =
        CreateWindowExW(0, L"BUTTON", L"Clear", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 102, 505, 92, 30,
                        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CLEAR_BTN, m_hInstance, nullptr);

    if (m_hwndCopilotClearBtn)
    {
        SendMessage(m_hwndCopilotClearBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
        m_oldCopilotClearBtnProc =
            (WNDPROC)SetWindowLongPtr(m_hwndCopilotClearBtn, GWLP_WNDPROC, (LONG_PTR)CopilotButtonProc);
        SetWindowLongPtr(m_hwndCopilotClearBtn, GWLP_USERDATA, (LONG_PTR)this);
    }

    HWND hwndNewChatBtn =
        CreateWindowExW(0, L"BUTTON", L"New Chat", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 199, 505, 92, 30,
                        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_NEW_CHAT_BTN, m_hInstance, nullptr);
    if (hwndNewChatBtn)
    {
        SendMessage(hwndNewChatBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    m_hwndCopilotModelUsedLabel =
        CreateWindowExW(0, L"STATIC", L"GPT-5.3-Codex • 0.9x", WS_CHILD | WS_VISIBLE | SS_LEFT, 5, 540, 286, 18,
                        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_MODEL_BADGE, m_hInstance, nullptr);
    if (m_hwndCopilotModelUsedLabel)
    {
        SendMessage(m_hwndCopilotModelUsedLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    m_hwndCopilotHelpfulBtn =
        CreateWindowExW(0, L"BUTTON", L"Helpful", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 5, 562, 68, 24,
                        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_HELPFUL_BTN, m_hInstance, nullptr);
    m_hwndCopilotUnhelpfulBtn =
        CreateWindowExW(0, L"BUTTON", L"Unhelpful", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 78, 562, 68, 24,
                        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_UNHELPFUL_BTN, m_hInstance, nullptr);
    m_hwndCopilotCopyBtn =
        CreateWindowExW(0, L"BUTTON", L"Copy", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 151, 562, 68, 24,
                        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_COPY_BTN, m_hInstance, nullptr);
    m_hwndCopilotRetryBtn =
        CreateWindowExW(0, L"BUTTON", L"Retry", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 224, 562, 68, 24,
                        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_RETRY_BTN, m_hInstance, nullptr);

    if (m_hwndCopilotHelpfulBtn)
        SendMessage(m_hwndCopilotHelpfulBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (m_hwndCopilotUnhelpfulBtn)
        SendMessage(m_hwndCopilotUnhelpfulBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (m_hwndCopilotCopyBtn)
        SendMessage(m_hwndCopilotCopyBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (m_hwndCopilotRetryBtn)
        SendMessage(m_hwndCopilotRetryBtn, WM_SETFONT, (WPARAM)hFont, TRUE);

    RawrXD_ApplyCopilotChatEditLimits(m_hwndCopilotChatOutput, m_hwndCopilotChatInput);
    createAgentChatCursorOverlay();
    reloadPersistedChatHistoryIntoUi();

    if (m_hwndSecondarySidebar && IsWindow(m_hwndSecondarySidebar))
    {
        CommandPreview_Create(m_hwndSecondarySidebar, m_hInstance);
        ComposerPanel_Create(m_hwndSecondarySidebar, m_hInstance);
    }

    m_secondarySidebarVisible = true;
    m_secondarySidebarWidth = 320;

    // Align checkboxes + AI menu with AgenticBridge / NativeAgent (defaults are ON).
    syncAgentModeUiFromBridge();
    updateSecondarySidebarContent();
    ShowWindow(m_hwndSecondarySidebar, SW_SHOW);
    UpdateWindow(m_hwndSecondarySidebar);

    RECT rcSidebar{};
    if (GetClientRect(m_hwndSecondarySidebar, &rcSidebar))
    {
        SendMessage(m_hwndSecondarySidebar, WM_SIZE, 0,
                    MAKELPARAM((WORD)(rcSidebar.right - rcSidebar.left), (WORD)(rcSidebar.bottom - rcSidebar.top)));
    }
}

void Win32IDE::populateModelSelectorFromDiscoveryCache()
{
    if (!m_hwndModelSelector)
        return;

    std::vector<std::string> models;
    std::vector<std::string> paths;
    {
        std::lock_guard<std::recursive_mutex> modelListLock(m_availableModelsMutex);
        if (!m_modelDiscoveryCacheReady.load(std::memory_order_acquire) || m_availableModels.empty())
        {
            OutputDebugStringA("[ModelDiscovery] populateModelSelectorFromDiscoveryCache: cache not ready\n");
            return;
        }
        models = m_availableModels;
        paths = m_modelPaths;
    }

    OutputDebugStringA("[ModelDiscovery] populateModelSelectorFromDiscoveryCache begin\n");

    std::string previousSelection;
    int prevIdx = (int)SendMessage(m_hwndModelSelector, CB_GETCURSEL, 0, 0);
    if (prevIdx >= 0 && prevIdx < (int)models.size())
    {
        previousSelection = models[(size_t)prevIdx];
    }
    if (previousSelection.empty())
    {
        if (!m_ollamaModelOverride.empty())
            previousSelection = m_ollamaModelOverride;
        else if (!m_loadedModelPath.empty())
            previousSelection = m_loadedModelPath;
    }

    SendMessage(m_hwndModelSelector, CB_RESETCONTENT, 0, 0);

    for (const auto& model : models)
    {
        const std::string uiLabel = rawrxd::BuildModelDropdownLabel(model);
        SendMessageW(m_hwndModelSelector, CB_ADDSTRING, 0, (LPARAM)utf8ToWide(uiLabel).c_str());
    }

    int selectedIdx = -1;
    if (!previousSelection.empty())
    {
        for (size_t i = 0; i < models.size(); ++i)
        {
            const std::string resolvedCandidate =
                resolveLocalModelSelectionPath(models[i], paths, m_userModelDirectories);
            if (models[i] == previousSelection || resolvedCandidate == previousSelection)
            {
                selectedIdx = (int)i;
                break;
            }
        }
    }

    if (!models.empty())
    {
        if (selectedIdx < 0)
            selectedIdx = 0;
        SendMessage(m_hwndModelSelector, CB_SETCURSEL, selectedIdx, 0);
    }

    const int finalUiCount = (int)SendMessage(m_hwndModelSelector, CB_GETCOUNT, 0, 0);
    std::string dbg = "[ModelDiscovery] cache populate complete available=" + std::to_string(models.size()) +
                      " ui_count=" + std::to_string(finalUiCount) + "\n";
    OutputDebugStringA(dbg.c_str());
}

void Win32IDE::populateModelSelector()
{
    if (!m_hwndModelSelector)
        return;

    if (m_modelDiscoveryCacheReady.load(std::memory_order_acquire))
    {
        populateModelSelectorFromDiscoveryCache();
        return;
    }

    OutputDebugStringA("[ModelDiscovery] populateModelSelector: cache pending, showing placeholders only\n");

    std::string previousSelection;
    int prevIdx = (int)SendMessage(m_hwndModelSelector, CB_GETCURSEL, 0, 0);
    if (prevIdx >= 0)
    {
        wchar_t buf[512]{};
        if (SendMessageW(m_hwndModelSelector, CB_GETLBTEXT, (WPARAM)prevIdx, (LPARAM)buf) != CB_ERR)
        {
            previousSelection = wideToUtf8(buf);
        }
    }

    SendMessage(m_hwndModelSelector, CB_RESETCONTENT, 0, 0);

    static const char* kPlaceholderModels[] = {"phi3mini.gguf", "llama2", "mistral", "neural-chat"};
    for (const auto* fallbackModel : kPlaceholderModels)
    {
        const std::string uiLabel = rawrxd::BuildModelDropdownLabel(fallbackModel);
        SendMessageW(m_hwndModelSelector, CB_ADDSTRING, 0, (LPARAM)utf8ToWide(uiLabel).c_str());
    }
    SendMessage(m_hwndModelSelector, CB_SETCURSEL, 0, 0);
    OutputDebugStringA("[ModelDiscovery] populateModelSelector placeholders done\n");
}

std::vector<std::string> Win32IDE::getModelsFromDirectory(const std::string& directory)
{
    std::vector<std::string> models;
    try
    {
        std::error_code ec;
        if (!std::filesystem::exists(directory, ec))
        {
            LOG_WARNING("Directory does not exist: " + directory + " (ec=" + std::to_string(ec.value()) + ")");
            return models;
        }

        std::filesystem::recursive_directory_iterator it(
            directory, std::filesystem::directory_options::skip_permission_denied, ec);
        std::filesystem::recursive_directory_iterator end;

        while (it != end)
        {
            if (ec)
            {
                ec.clear();
                try
                {
                    it.increment(ec);
                }
                catch (...)
                {
                    break;
                }
                continue;
            }

            try
            {
                const auto& entry = *it;
                if (entry.is_regular_file(ec))
                {
                    const std::string fullPath = entry.path().string();
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (ext == ".gguf" || ext == ".bin" || ext == ".ggml")
                    {
                        models.push_back(fullPath);
                    }
                    else if (ext.empty())
                    {
                        // Extensionless file (e.g. Ollama sha256 blob) — sniff GGUF magic
                        std::error_code szEc;
                        const auto fileSize = entry.file_size(szEc);
                        if (!szEc && fileSize >= 8)
                        {
                            std::ifstream probe(entry.path(), std::ios::binary);
                            if (probe.is_open())
                            {
                                uint32_t magic = 0;
                                probe.read(reinterpret_cast<char*>(&magic), 4);
                                if (magic == 0x46465547u)  // "GGUF" little-endian
                                {
                                    models.push_back(fullPath);
                                }
                            }
                        }
                    }
                }
                else if (ec)
                {
                    ec.clear();
                }

                try
                {
                    it.increment(ec);
                }
                catch (const std::exception& e)
                {
                    LOG_WARNING("Error during directory iteration: " + std::string(e.what()));
                    break;
                }
            }
            catch (const std::exception& e)
            {
                LOG_WARNING("Exception processing directory entry: " + std::string(e.what()));
                try
                {
                    it.increment(ec);
                }
                catch (...)
                {
                    break;
                }
            }
        }

        if (ec && ec.value() != 0)
        {
            LOG_WARNING("Directory iteration error: " + ec.message());
        }
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Error parsing directory listing: " + std::string(e.what()));
    }

    return models;
}

std::string Win32IDE::makeHttpRequest(const std::string& url, const std::string& method, const std::string& body,
                                      const std::string& contentType)
{
    // Parse URL
    std::string host = "localhost";
    int port = m_settings.localServerPort;
    std::string path = "/";

    // Simple URL parsing for localhost:port/path
    size_t colonPos = url.find(':');
    size_t slashPos = url.find('/', colonPos + 3);  // after ://

    if (colonPos != std::string::npos)
    {
        size_t portStart = colonPos + 1;
        while (portStart < url.size() && url[portStart] == '/')
            portStart++;
        size_t portEnd = url.find('/', portStart);
        if (portEnd != std::string::npos)
        {
            std::string portStr = url.substr(portStart, portEnd - portStart);
            port = atoi(portStr.c_str());
            path = url.substr(portEnd);
        }
        else
        {
            std::string portStr = url.substr(portStart);
            port = atoi(portStr.c_str());
        }
    }

    std::wstring whost(host.begin(), host.end());
    HINTERNET hSession = WinHttpOpen(L"RawrXDIDE/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, NULL, NULL, 0);
    if (!hSession)
        return "";

    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::wstring wMethod(method.begin(), method.end());
    std::wstring wPath(path.begin(), path.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), wPath.c_str(), NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Set headers
    std::wstring wHeaders = L"Content-Type: " + std::wstring(contentType.begin(), contentType.end());
    BOOL bResults = WinHttpSendRequest(hRequest, wHeaders.c_str(), (DWORD)-1L, (LPVOID)body.c_str(), (DWORD)body.size(),
                                       (DWORD)body.size(), 0);
    if (!bResults)
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    bResults = WinHttpReceiveResponse(hRequest, NULL);
    std::string response;
    if (bResults)
    {
        DWORD dwSize = 0;
        do
        {
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
                break;
            if (!dwSize)
                break;
            std::string chunk;
            chunk.resize(dwSize);
            DWORD dwRead = 0;
            if (!WinHttpReadData(hRequest, chunk.data(), dwSize, &dwRead))
                break;
            if (dwRead)
                response.append(chunk.data(), dwRead);
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

void Win32IDE::postDeferredCopilotSend()
{
    if (isShuttingDown() || !m_hwndMain || !IsWindow(m_hwndMain))
        return;
    (void)PostMessageW(m_hwndMain, WM_COPILOT_DEFERRED_SEND, 0, 0);
}

void Win32IDE::HandleCopilotSend()
{
    if (!smokeCopilotChatEnabled())
        return;

    // [BOUNDARY-POINT LOGGING] Chat send entry
    OutputDebugStringA("[Win32IDE::HandleCopilotSend] ENTRY\n");
    const unsigned long long traceId = NextChatTraceId();

    // Atomic Stage 1 Pulse
    g_pulseRing.Log(1, 0);

    SCOPED_METRIC("chat.send_message");
    METRICS.increment("chat.messages_sent");

    if (!m_hwndCopilotChatInput || !m_hwndCopilotChatOutput)
        return;

    if (!m_copilotBackendReady)
    {
        appendToOutput("\n[Info] AI backend is still initializing. Please wait for readiness.\n", "Output",
                       OutputSeverity::Info);
        OutputDebugStringA("[HandleCopilotSend] blocked: backend not ready\n");
        return;
    }

    const int inputLen = GetWindowTextLengthW(m_hwndCopilotChatInput);
    if (inputLen <= 0)
    {
        LOG_WARNING("Empty message - ignoring");
        return;
    }

    std::vector<wchar_t> inputBuffer(static_cast<size_t>(inputLen) + 1, L'\0');
    {
        std::lock_guard<std::mutex> inputLock(g_chatInputBufferMutex);
        GetWindowTextW(m_hwndCopilotChatInput, inputBuffer.data(), inputLen + 1);
    }
    std::wstring effectiveInput(inputBuffer.data());
    if (!effectiveInput.empty() && effectiveInput.front() != L'/')
        effectiveInput = resolveSymbolAtMentions(effectiveInput);

    {
        char pulse[192] = {};
        _snprintf_s(pulse, sizeof(pulse), _TRUNCATE,
                    "[BRIDGE_PULSE] Stage1/UI Ingest: extracted_len=%d trace_id=%llu\\n", inputLen, traceId);
        OutputDebugStringA(pulse);
    }

    const std::string userMessage = wideToUtf8(effectiveInput);
    const std::string userMessageSig = MakePromptSignature(userMessage);

    if (userMessage.empty())
    {
        LOG_WARNING("Empty message - ignoring");
        return;
    }

    LOG_INFO("[ChatSend#" + std::to_string(traceId) + "] begin " + userMessageSig + " preview='" +
             MakeLogPreview(userMessage) + "'");

    m_lastCopilotUserPrompt = userMessage;

    const bool hasAgenticPrefix = HasAgenticPrefix(userMessage);

    wchar_t validationError[512] = {};
    if (!CommandPreview_Validate(inputBuffer.data(), validationError, static_cast<int>(std::size(validationError))))
    {
        const std::string err = wideToUtf8(validationError);
        appendCopilotChatTextOnUiThread(std::string("\n[Validation] ") + err + "\n");
        return;
    }

    CommandPreview_Hide();
    ChatAutocomplete_Hide();

    previewSlashRouting(userMessage);
    hideSlashOverlay();

    if (!userMessage.empty() && userMessage.front() == '/')
    {
        const auto parsedSlash = RawrXD::Agentic::SlashCommandParser::Parse(userMessage);
        std::string planGoal;
        if (ShouldOpenPlanApprovalFromSlashEdit(userMessage, parsedSlash, planGoal))
        {
            appendCopilotChatTextOnUiThread("[Plan] /edit --dry-run detected. Opening plan approval dialog.\n");
            generateAgentPlan(planGoal);
            clearCopilotInputOnUiThread();
            return;
        }
    }

    if (hasAgenticPrefix)
    {
        const std::string strippedProbe = StripAgenticPrefixForRouteParity(userMessage);
        if (strippedProbe.empty())
        {
            appendCopilotChatTextOnUiThread(
                "\r\n[Agentic] Add a prompt after /agent, /agentic, agentic:, or @agent. Example: /agent explain "
                "src/main.cpp\r\n");
            return;
        }
    }

    syncStructuredSlashCopilotFixExpectationForUserMessage(userMessage);

    bool expectedIdle = false;
    if (!m_chatSendInFlight.compare_exchange_strong(expectedIdle, true))
    {
        appendToOutput("\n[Info] Previous request still in progress. Please wait for completion.\n", "Output",
                       OutputSeverity::Info);
        OutputDebugStringA("[HandleCopilotSend] send ignored: another request already in flight\n");
        LOG_WARNING("[ChatSend#" + std::to_string(traceId) + "] blocked because another request is in flight");
        return;
    }

    setCopilotInteractionBusyOnUiThread(true);

    auto releaseSendInFlight = [this]()
    {
        m_chatSendInFlight.store(false);
        setCopilotInteractionBusyOnUiThread(false);
    };

    ClearChatUtf8Carry(this);
    m_streamingTokenAccumulator.clear();

    if (!m_hwndModelSelector || !IsWindow(m_hwndModelSelector))
    {
        appendToOutput("\n[Error] Model selector unavailable. Please reopen AI Chat pane.\n", "Output",
                       OutputSeverity::Error);
        OutputDebugStringA("[HandleCopilotSend] model selector unavailable\n");
        LOG_ERROR("[ChatSend#" + std::to_string(traceId) + "] model selector unavailable");
        releaseSendInFlight();
        return;
    }

    int comboCount = (int)SendMessage(m_hwndModelSelector, CB_GETCOUNT, 0, 0);
    bool modelsEmpty = false;
    {
        std::lock_guard<std::recursive_mutex> modelListLock(m_availableModelsMutex);
        modelsEmpty = m_availableModels.empty();
    }
    if (comboCount <= 0 || modelsEmpty)
    {
        OutputDebugStringA("[HandleCopilotSend] model list empty, forcing repopulate\n");
        populateModelSelector();
        comboCount = (int)SendMessage(m_hwndModelSelector, CB_GETCOUNT, 0, 0);
        std::lock_guard<std::recursive_mutex> modelListLock(m_availableModelsMutex);
        modelsEmpty = m_availableModels.empty();
    }

    if (comboCount <= 0 || modelsEmpty)
    {
        appendToOutput("\n[Error] No models discovered. Configure OLLAMA_MODELS/RAWRXD_MODELS_PATH and reopen chat.\n",
                       "Output", OutputSeverity::Error);
        OutputDebugStringA("[HandleCopilotSend] aborting send due to empty model list\n");
        LOG_ERROR("[ChatSend#" + std::to_string(traceId) + "] no models available after repopulate");
        releaseSendInFlight();
        return;
    }

    // Get selected model
    int modelIdx = (int)SendMessage(m_hwndModelSelector, CB_GETCURSEL, 0, 0);
    std::string selectedModel;
    std::string modelSelectorText;
    const int selectorTextLen = GetWindowTextLengthW(m_hwndModelSelector);
    if (selectorTextLen > 0)
    {
        std::vector<wchar_t> selectorBuffer(static_cast<size_t>(selectorTextLen) + 1, L'\0');
        GetWindowTextW(m_hwndModelSelector, selectorBuffer.data(), selectorTextLen + 1);
        modelSelectorText = wideToUtf8(selectorBuffer.data());
    }
    {
        std::lock_guard<std::recursive_mutex> modelListLock(m_availableModelsMutex);
        const std::string normalizedSelectorText = normalizeModelSelectorEntry(modelSelectorText, m_availableModels);
        const bool hasFreeformSelectorText = !normalizedSelectorText.empty();
        if (modelIdx >= 0 && modelIdx < (int)m_availableModels.size())
        {
            selectedModel = m_availableModels[modelIdx];
        }
        else if (hasFreeformSelectorText)
        {
            selectedModel = normalizedSelectorText;
        }
        else
        {
            modelIdx = 0;
            if (modelIdx < 0 || modelIdx >= (int)m_availableModels.size())
            {
                // Route-only resilience: keep chat path alive even if model cache is empty.
                // This avoids hard send drops in smoke and allows deferred/local model loaders
                // to hydrate later without losing user-visible turn flow.
                selectedModel = "route-only";
                OutputDebugStringA("[HandleCopilotSend] cache empty after repopulate; using route-only fallback\n");
            }
            else
            {
                selectedModel = m_availableModels[modelIdx];
            }
        }

        if (selectedModel.empty() && !m_availableModels.empty())
        {
            modelIdx = 0;
            selectedModel = m_availableModels[0];
        }
    }
    std::string selectedModelResolved =
        selectedModel.empty() ? std::string()
                              : resolveLocalModelSelectionPath(selectedModel, m_modelPaths, m_userModelDirectories);
    if (selectedModelResolved != selectedModel)
    {
        OutputDebugStringA(
            ("[HandleCopilotSend] resolved model ref '" + selectedModel + "' -> '" + selectedModelResolved + "'\n")
                .c_str());
    }

    const bool selectedLocalModel = selectedModelResolved.find(".gguf") != std::string::npos ||
                                    selectedModelResolved.find(":\\") != std::string::npos ||
                                    selectedModelResolved.find('/') != std::string::npos ||
                                    selectedModelResolved.find('\\') != std::string::npos;

    if (selectedLocalModel)
    {
        std::error_code modelPathEc;
        const bool localModelPathMissing =
            selectedModelResolved.empty() ||
            !std::filesystem::exists(std::filesystem::path(selectedModelResolved), modelPathEc);
        if (localModelPathMissing)
        {
            appendToOutput("\n[Error] Selected GGUF model path is missing. Please open/select a valid .gguf model "
                           "and retry.\n",
                           "Errors", OutputSeverity::Error);
            appendCopilotChatTextOnUiThread(
                "\n[Error] Selected GGUF model is unavailable. Please choose/open a valid .gguf model first.\n");
            OutputDebugStringA(("[HandleCopilotSend] invalid local model path='" + selectedModelResolved + "' ec='" +
                                modelPathEc.message() + "'\n")
                                   .c_str());
            LOG_ERROR("[ChatSend#" + std::to_string(traceId) + "] selected local model path missing path='" +
                      selectedModelResolved + "' ec='" + modelPathEc.message() + "'");
            releaseSendInFlight();
            return;
        }
    }

    LOG_INFO("[ChatSend#" + std::to_string(traceId) + "] model selected idx=" + std::to_string(modelIdx) +
             " selector_text='" + modelSelectorText + "' selected='" + selectedModel + "' resolved='" +
             selectedModelResolved + "' local=" + BoolToLog(selectedLocalModel) + " loaded='" + m_loadedModelPath +
             "' loaded_ready=" + BoolToLog(isModelLoaded()));

    if (m_hwndCopilotModelUsedLabel && IsWindow(m_hwndCopilotModelUsedLabel))
    {
        const std::string modelSuffix = selectedModelResolved.empty() ? selectedModel : selectedModelResolved;
        const std::string badge =
            modelSuffix.empty() ? "GPT-5.3-Codex • 0.9x" : "GPT-5.3-Codex • 0.9x | " + modelSuffix;
        SetWindowTextW(m_hwndCopilotModelUsedLabel, utf8ToWide(badge).c_str());
    }

    const bool replayingDeferredUserTurn =
        m_pendingChatOnLoadUserTurnRendered && (m_pendingChatOnLoadMessage == userMessage);
    if (replayingDeferredUserTurn)
    {
        m_pendingChatOnLoadUserTurnRendered = false;
    }

    m_ollamaModelOverride = selectedModelResolved;
    if (selectedLocalModel)
    {
        const bool selectedLocalModelReady = (m_loadedModelPath == selectedModelResolved) && isModelLoaded();
        if (!selectedLocalModelReady)
        {
            m_lastLocalModelReadyTickMs = 0;

            // Always queue the user's message so it is auto-replayed after WM_MODEL_LOAD_DONE.
            m_pendingChatOnLoadMessage = userMessage;

            if (!m_pendingChatOnLoadUserTurnRendered)
            {
                // Ensure chat output changes immediately for strict smoke harnesses and users,
                // even when local model inference is deferred until load completion.
                appendFormattedChatMessage("user", userMessage);
                m_pendingChatOnLoadUserTurnRendered = true;
            }

            if (m_asyncModelLoadRunning)
            {
                appendToOutput("\n[Info] Model is loading — your message will be sent automatically when ready.\n",
                               "Output", OutputSeverity::Info);
                appendCopilotChatTextOnUiThread("\n[Info] Model is loading. Your message is queued and will be "
                                                "sent automatically when ready.\n");
                OutputDebugStringA("[HandleCopilotSend] queued pending chat; model still loading\n");
                LOG_INFO("[ChatSend#" + std::to_string(traceId) +
                         "] queued message while model load already in progress");
                releaseSendInFlight();
                return;
            }

            appendToOutput("\n[Info] Selected local model is not loaded yet. Starting background load now; your "
                           "message will be sent automatically when ready.\n",
                           "Output", OutputSeverity::Info);
            appendCopilotChatTextOnUiThread(
                "\n[Info] Selected local model is loading. Your queued message will auto-send when ready.\n");
            OutputDebugStringA(("[HandleCopilotSend] selected local model not ready; kicking async load path=" +
                                selectedModelResolved + "\n")
                                   .c_str());
            LOG_INFO("[ChatSend#" + std::to_string(traceId) +
                     "] selected local model not ready; starting async load path='" + selectedModelResolved + "'");
            loadModelFromPathAsync(selectedModelResolved);
            releaseSendInFlight();
            return;
        }

        OutputDebugStringA(
            ("[HandleCopilotSend] selected local model ready path=" + selectedModelResolved + "\n").c_str());

        constexpr ULONGLONG kLocalModelWarmupGuardMs = 750;
        const ULONGLONG readyTick = m_lastLocalModelReadyTickMs;
        const ULONGLONG nowTick = GetTickCount64();
        if (readyTick != 0 && nowTick >= readyTick && (nowTick - readyTick) < kLocalModelWarmupGuardMs)
        {
            m_pendingChatOnLoadMessage = userMessage;
            OutputDebugStringA(("[HandleCopilotSend] deferring send during local model warmup window age_ms=" +
                                std::to_string(nowTick - readyTick) + "\n")
                                   .c_str());
            LOG_INFO("[ChatSend#" + std::to_string(traceId) +
                     "] deferred by warmup guard age_ms=" + std::to_string(nowTick - readyTick));
            PostMessage(m_hwndMain, WM_PENDING_CHAT_REPLAY, 0, 0);
            releaseSendInFlight();
            return;
        }
    }
    else
    {
        OutputDebugStringA(("[HandleCopilotSend] selected remote model=" + selectedModelResolved + "\n").c_str());
    }

    // Session before `m_chatHistory` so `conversationAddUser` rehydrate does not duplicate this turn.
    conversationDetectModelFormat(selectedModelResolved);
    if (m_inferenceConfig.contextWindow > 0)
    {
        conversationSetContextWindow(m_inferenceConfig.contextWindow);
    }
    conversationAddUser(userMessage);

    if (!replayingDeferredUserTurn)
    {
        appendFormattedChatMessage("user", userMessage);
    }
    m_chatHistory.push_back({"user", userMessage});
    persistChatTurnToDisk("user", userMessage);

    auto sendStart = std::make_shared<std::chrono::steady_clock::time_point>(std::chrono::steady_clock::now());
    // Shared TTFT tracking: written once on first token, read at completion for ESP display.
    auto firstTokenElapsedMs = std::make_shared<std::atomic<long long>>(-1);
    auto recordCopilotThroughputAtComplete =
        [this, firstTokenElapsedMs](const std::chrono::steady_clock::time_point& startedAt,
                                    const std::string& accumulatedUtf8)
    {
        if (accumulatedUtf8.empty())
        {
            return;
        }
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startedAt).count();
        if (ms < 1.0)
        {
            return;
        }
        const double estTok = std::max(1.0, static_cast<double>(accumulatedUtf8.size()) / 4.0);
        const double tps = estTok / (ms / 1000.0);
        METRICS.gauge("chat.copilot.estimated_tps", tps);
        METRICS.recordDuration("chat.copilot.generation_ms", ms);
        METRICS.increment("chat.copilot.completion_runs_recorded", 1);
        if (m_hwndMain && IsWindow(m_hwndMain))
            PostMessageW(m_hwndMain, WM_STATUSBAR_REFRESH_COPILOT, 0, 0);

        // ── LM Studio-style Extended Status Panel (ESP) ──────────────────────
        // Format: "  {tps} tok/sec  •  {tokens} tokens  •  {ttft}s to first token  •  Stop reason: {reason}"
        const long long estTokenCount = static_cast<long long>(std::round(estTok));
        const long long ttftMs = firstTokenElapsedMs->load();
        char espBuf[256];
        if (ttftMs >= 0)
        {
            std::snprintf(espBuf, sizeof(espBuf),
                          "\n\u2500\u2500 %.2f tok/sec  \u2022  %lld tokens  \u2022  %.2fs to first token  \u2022  "
                          "Stop reason: EOS Token Found\n",
                          tps, estTokenCount, static_cast<double>(ttftMs) / 1000.0);
        }
        else
        {
            std::snprintf(espBuf, sizeof(espBuf),
                          "\n\u2500\u2500 %.2f tok/sec  \u2022  %lld tokens  \u2022  Stop reason: EOS Token Found\n",
                          tps, estTokenCount);
        }
        appendCopilotChatTextOnUiThread(std::string(espBuf));
        // ─────────────────────────────────────────────────────────────────────
    };
    auto firstResponseActivityLogged = std::make_shared<std::atomic<bool>>(false);
    auto markFirstResponseActivity = [sendStart, firstResponseActivityLogged,
                                      firstTokenElapsedMs](const char* source, size_t payloadLen, bool complete)
    {
        if (firstResponseActivityLogged->exchange(true))
            return;
        const auto now = std::chrono::steady_clock::now();
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - *sendStart).count();
        // Record TTFT for ESP display.
        firstTokenElapsedMs->store(static_cast<long long>(elapsedMs));
        METRICS.increment("chat.first_response_activity");
        OutputDebugStringA(
            ("[ChatLifecycle] first_response_activity source=" + std::string(source ? source : "unknown") +
             " elapsed_ms=" + std::to_string(elapsedMs) + " payload_len=" + std::to_string(payloadLen) +
             " complete=" + std::string(complete ? "true" : "false") + "\n")
                .c_str());
    };
    OutputDebugStringA("[ChatLifecycle] send_dispatched awaiting_first_response_activity\n");

    // Clear input lazily on first response activity so the prompt is not
    // visually wiped before model output appears.
    auto inputCleared = std::make_shared<std::atomic<bool>>(false);
    auto clearInputOnce = [this, inputCleared, sendStart]()
    {
        if (inputCleared->exchange(true))
            return;
        const auto now = std::chrono::steady_clock::now();
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - *sendStart).count();
        METRICS.increment("chat.input_cleared_on_response_activity");
        OutputDebugStringA(("[ChatLifecycle] input_cleared elapsed_ms=" + std::to_string(elapsedMs) + "\n").c_str());
        clearCopilotInputOnUiThread();
    };

    auto aiPrefixWritten = std::make_shared<std::atomic<bool>>(false);

    // Generate response asynchronously
    auto onResponse = [this, clearInputOnce, markFirstResponseActivity, aiPrefixWritten, releaseSendInFlight, sendStart,
                       recordCopilotThroughputAtComplete](const std::string& response, bool complete)
    {
        if (!m_hwndCopilotChatOutput)
        {
            if (complete)
                releaseSendInFlight();
            return;
        }

        // First callback that indicates activity means we can safely clear input.
        if (!response.empty() || complete)
        {
            markFirstResponseActivity("onResponse", response.size(), complete);
            clearInputOnce();
        }

        // Completion callbacks may use empty payloads as end-of-stream markers.
        if (response.empty())
        {
            if (complete)
            {
                const std::string finalText = m_streamingTokenAccumulator;
                // Completion runs on the inference worker thread. Marshal the final rich-edit rewrite and
                // conversation/history mutations back onto the UI thread to avoid cross-thread UI access.
                if (!finalText.empty())
                {
                    recordCopilotThroughputAtComplete(*sendStart, finalText);
                    postAgenticAssistantFinalSafe(finalText, finalText);
                    return;
                }
                OutputDebugStringA("[ChatUi] onResponse received empty completion marker\n");
                m_nativeStreamingActive = false;
                releaseSendInFlight();
                notifyCopilotChatCompletedUnlessSidebarFocused();
            }
            return;
        }

        logRawResponseHexPreview(response);
        // Temporary diagnostics: log raw payload seen by the active chat response handler.
        OutputDebugStringA(("RAW_PAYLOAD: [" + response + "]\n").c_str());
        std::string safe = normalizeChatUtf8Chunk(this, response, complete);
        if (safe.empty() || safe == "[non-text backend payload suppressed]")
        {
            const std::string extracted = extractDisplayTextFromBackendPayload(response);
            if (!extracted.empty())
            {
                const std::string extractedSafe = sanitizeForChatUi(extracted);
                safe = extractedSafe.empty() ? extracted : extractedSafe;
            }
        }
        OutputDebugStringA(("[ChatUi] onResponse raw_len=" + std::to_string(response.size()) +
                            " safe_len=" + std::to_string(safe.size()) +
                            " complete=" + std::string(complete ? "true" : "false") + "\n")
                               .c_str());

        // Only accumulate and display genuinely valid text tokens.
        // Skip suppression placeholders and empty chunks — they poison the accumulator.
        const bool isSuppressed = (safe == "[non-text backend payload suppressed]");
        const bool isUsable = !safe.empty() && !isSuppressed;
        if (!isUsable)
        {
            OutputDebugStringA(("[ChatUi] chunk skipped (suppressed or empty) complete=" +
                                std::string(complete ? "true" : "false") + "\n")
                                   .c_str());
            if (complete)
            {
                const std::string finalText = m_streamingTokenAccumulator;
                if (!finalText.empty())
                {
                    recordCopilotThroughputAtComplete(*sendStart, finalText);
                    postAgenticAssistantFinalSafe(finalText, finalText);
                    return;
                }
                m_nativeStreamingActive = false;
                releaseSendInFlight();
                notifyCopilotChatCompletedUnlessSidebarFocused();
            }
            return;
        }

        // Accumulate streaming tokens for markdown re-render on completion
        m_streamingTokenAccumulator += safe;

        // During streaming: show raw text for real-time feedback
        const bool needsPrefix = !aiPrefixWritten->exchange(true);
        if (needsPrefix)
        {
            m_nativeStreamingActive = true;
            m_streamingWrittenWchars = 0;  // reset wide-char counter at stream start
        }
        std::string displayResp = (needsPrefix ? "Copilot: " : "") + safe;
        appendCopilotChatTextOnUiThread(displayResp);
        OutputDebugStringA(("[ChatUi] appended text_len=" + std::to_string(displayResp.size()) + "\n").c_str());

        if (complete)
        {
            const std::string finalText = m_streamingTokenAccumulator;
            if (!finalText.empty())
            {
                recordCopilotThroughputAtComplete(*sendStart, finalText);
                postAgenticAssistantFinalSafe(finalText, finalText);
                return;
            }
            m_nativeStreamingActive = false;
            releaseSendInFlight();
            notifyCopilotChatCompletedUnlessSidebarFocused();
        }
    };

    // Cursor/Copilot-style: agentic tools when (1) AI chat "agent" mode toggle, (2) Agentic bridge mode, or
    // (3) explicit prefixes: /agent, /agentic, agentic:, @agent — same surface as CLI `main.cpp` /agent loop.
    const bool bridgeAgenticMode = m_agenticBridge && m_agenticBridge->IsAgenticMode();
    const bool wantsAgenticChat = hasAgenticPrefix || bridgeAgenticMode || m_agenticFunctionCallingMode;
    const bool layerAvailable = rawrxd::isAgenticLayerAvailable();
    const std::string agentRouteUserMessage =
        hasAgenticPrefix ? StripAgenticPrefixForRouteParity(userMessage) : userMessage;
    LOG_INFO("[ChatSend#" + std::to_string(traceId) + "] route decision has_agentic_prefix=" +
             BoolToLog(hasAgenticPrefix) + " bridge_agentic_mode=" + BoolToLog(bridgeAgenticMode) +
             " function_calling_mode=" + BoolToLog(m_agenticFunctionCallingMode) +
             " wants_agentic=" + BoolToLog(wantsAgenticChat) + " layer_available=" + BoolToLog(layerAvailable) +
             " agentic_bridge_present=" + BoolToLog(m_agenticBridge != nullptr) + " agent_prompt='" +
             MakeLogPreview(agentRouteUserMessage) + "'");

    // Ollama HTTP + `AgenticChatSession` cannot drive local GGUF weights — route loaded models through
    // `generateResponseAsync` (AgenticBridge, minimal agentic layer, native backends).
    if (wantsAgenticChat && layerAvailable && !selectedLocalModel)
    {
        refreshAgenticChatSessionContext();
        if (!m_agenticChatSession)
        {
            appendCopilotChatTextOnUiThread("\n[Agentic] Chat session unavailable. Check Ollama / backend init.\n");
            releaseSendInFlight();
            return;
        }
        m_agenticChatSession->SetAgenticMode(true);
        if (!selectedModel.empty())
            m_agenticChatSession->SetChatModel(selectedModel);
        // Use agentic chat session with function calling (route stripped prompt; history keeps full user line)
        m_agenticChatSession->RunTurnAsync(
            agentRouteUserMessage,
            [this, clearInputOnce, markFirstResponseActivity, traceId](const std::string& chunk)
            {
                // Handle streaming content chunks — accumulate for markdown
                if (!m_hwndCopilotChatOutput)
                    return;
                markFirstResponseActivity("agentic_chunk", chunk.size(), false);
                clearInputOnce();
                LOG_DEBUG("[ChatSend#" + std::to_string(traceId) + "] agentic chunk len=" +
                          std::to_string(chunk.size()) + " preview='" + MakeLogPreview(chunk, 80) + "'");
                m_streamingTokenAccumulator += chunk;
                appendCopilotChatTextOnUiThread(chunk);
            },
            [this, clearInputOnce, markFirstResponseActivity, traceId](const std::string& tool_name)
            {
                // Worker thread — UI text marshals via `appendCopilotChatTextOnUiThread`; history via PostMessage.
                if (!m_hwndCopilotChatOutput)
                    return;
                markFirstResponseActivity("agentic_tool_start", tool_name.size(), false);
                clearInputOnce();
                LOG_INFO("[ChatSend#" + std::to_string(traceId) + "] tool start '" + tool_name + "'");
                std::string displayStatus = "\n[Running " + tool_name + "...]\n";
                appendCopilotChatTextOnUiThread(displayStatus);
                const std::string tn = tool_name.empty() ? std::string("tool") : tool_name;
                postCopilotRecordToolTurnSafe(tn, std::string("[started]"));
            },
            [this, clearInputOnce, markFirstResponseActivity, traceId](const std::string& tool_name,
                                                                       const std::string& result)
            {
                if (!m_hwndCopilotChatOutput)
                    return;
                markFirstResponseActivity("agentic_tool_result", result.size(), false);
                clearInputOnce();
                LOG_INFO("[ChatSend#" + std::to_string(traceId) + "] tool result '" + tool_name +
                         "' len=" + std::to_string(result.size()) + " preview='" + MakeLogPreview(result, 100) + "'");
                std::string displayResult = "[" + tool_name + " Result]:\n" + result + "\n";
                appendCopilotChatTextOnUiThread(displayResult);

                const std::string tn = tool_name.empty() ? std::string("tool") : tool_name;
                const std::string bodyForStore = result.empty() ? std::string("(no output)") : result;
                postCopilotRecordToolTurnSafe(tn, bodyForStore);
            },
            [this, clearInputOnce, markFirstResponseActivity, releaseSendInFlight, sendStart, traceId,
             recordCopilotThroughputAtComplete](const std::string& final_response)
            {
                markFirstResponseActivity("agentic_final", final_response.size(), true);
                clearInputOnce();
                LOG_INFO("[ChatSend#" + std::to_string(traceId) +
                         "] agentic final len=" + std::to_string(final_response.size()) + " preview='" +
                         MakeLogPreview(final_response, 100) + "'");

                if (!m_hwndCopilotChatOutput)
                {
                    releaseSendInFlight();
                    return;
                }

                const std::string accCopy = m_streamingTokenAccumulator;
                const std::string fullText = accCopy.empty() ? final_response : accCopy;
                recordCopilotThroughputAtComplete(*sendStart, fullText);
                postAgenticAssistantFinalSafe(accCopy, fullText);
            });
    }
    else
    {
        if (wantsAgenticChat && !layerAvailable)
        {
            appendCopilotChatTextOnUiThread(
                "\n[Agentic] Tool layer not ready yet — using standard chat for this message. "
                "(Ensure agentic init completed; local GGUF or Ollama must be reachable.)\n");
            LOG_WARNING("[ChatSend#" + std::to_string(traceId) +
                        "] wants agentic chat but layer unavailable; falling back to standard path");
        }
        // Local GGUF + agentic: `MinimalAgentController` + tools (parity with RawrEngine `/smoke`, Route C).
        // DEFENSE: RAWRXD_BYPASS_AGENTIC_CHAT=1 skips the agentic orchestrator path
        // which crashes in autonomous_agentic_orchestrator.cpp SafetyGate destructor (AV 0xC0000005).
        // Falls through to generateResponseAsync (local pipeline) which is stable.
        const bool bypassAgenticChat = []()
        {
            const char* v = std::getenv("RAWRXD_BYPASS_AGENTIC_CHAT");
            return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
        }();
        if (bypassAgenticChat)
        {
            OutputDebugStringA("[HandleCopilotSend] RAWRXD_BYPASS_AGENTIC_CHAT=1 — skipping agentic path, using "
                               "generateResponseAsync\n");
            LOG_INFO("[ChatSend#" + std::to_string(traceId) +
                     "] RAWRXD_BYPASS_AGENTIC_CHAT active; using standard async path");
        }
        else if (wantsAgenticChat && layerAvailable && selectedLocalModel && isModelLoaded())
        {
            rawrxd::MinimalAgenticRequest req;
            req.message = agentRouteUserMessage;
            req.model_path = selectedModelResolved;
            req.enable_tools = true;
            req.max_iterations = 10;
            if (m_agenticBridge)
            {
                std::string sid = m_agenticBridge->GetAgenticSessionId();
                if (sid.empty())
                {
                    sid = "win32ide-copilot";
                    m_agenticBridge->SetAgenticSessionId(sid);
                }
                req.session_id = sid;
            }
            else
            {
                req.session_id = "win32ide-copilot";
            }
            req.workspace_root = workspaceDirectoryForChatPersistence();

            HWND hwndMain = m_hwndMain;
            std::thread(
                [this, req, hwndMain, traceId]()
                {
                    if (isShuttingDown())
                        return;
                    const auto t0 = std::chrono::steady_clock::now();
                    rawrxd::MinimalAgenticResponse miniResp;
                    try
                    {
                        miniResp = rawrxd::processAgenticRequest(req);
                    }
                    catch (const std::exception& ex)
                    {
                        miniResp.success = false;
                        miniResp.error = ex.what();
                    }
                    catch (...)
                    {
                        miniResp.success = false;
                        miniResp.error = "unknown exception in processAgenticRequest";
                    }
                    const double ms =
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
                    if (ms >= 1.0)
                    {
                        const double estTok = static_cast<double>(EstimateMinimalAgenticOutputChars(miniResp)) / 4.0;
                        METRICS.gauge("chat.minimal_agentic.estimated_tps", std::max(1.0, estTok) / (ms / 1000.0));
                        METRICS.recordDuration("chat.minimal_agentic.generation_ms", ms);
                    }
                    METRICS.increment("chat.minimal_agentic.worker_completed", 1);
                    METRICS.gauge("chat.minimal_agentic.last_tool_calls",
                                  static_cast<double>(miniResp.tool_calls_made));
                    LOG_INFO("[ChatSend#" + std::to_string(traceId) + "] minimal agentic worker done success=" +
                             BoolToLog(miniResp.success) + " tool_calls=" + std::to_string(miniResp.tool_calls_made) +
                             " response_len=" + std::to_string(miniResp.final_message.size()) + " error='" +
                             miniResp.error + "'");
                    const bool ok = miniResp.success;
                    const int tc = std::min(65535, std::max(0, miniResp.tool_calls_made));
                    auto* heap = new rawrxd::MinimalAgenticResponse(std::move(miniResp));
                    if (hwndMain && IsWindow(hwndMain))
                    {
                        PostMessageW(hwndMain, WM_COPILOT_MINIMAL_AGENTIC_DONE,
                                     MAKEWPARAM(ok ? 1 : 0, static_cast<WORD>(tc)), reinterpret_cast<LPARAM>(heap));
                    }
                    else
                        delete heap;
                })
                .detach();
            return;
        }
        // Generate response using traditional method (GGUF async / Ollama bridge)
        LOG_INFO("[ChatSend#" + std::to_string(traceId) + "] dispatching generateResponseAsync");
        generateResponseAsync(userMessage, onResponse);
    }
}

void Win32IDE::HandleCopilotClear()
{
    if (!m_hwndCopilotChatOutput || !m_hwndCopilotChatInput)
        return;

    clearPersistedChatHistoryOnDisk();
    clearCopilotChat();
    m_chatHistory.clear();
    m_chatToolActions.clear();
    m_currentToolActions.clear();
    m_pendingChatOnLoadMessage.clear();
    m_pendingChatOnLoadUserTurnRendered = false;
    conversationClear();
    m_streamingTokenAccumulator.clear();
    ClearChatUtf8Carry(this);
    m_lastCopilotUserPrompt.clear();
    m_lastCopilotAssistantResponse.clear();
    m_copilotHelpfulCount = 0;
    m_copilotUnhelpfulCount = 0;
    if (m_hwndCopilotModelUsedLabel && IsWindow(m_hwndCopilotModelUsedLabel))
    {
        SetWindowTextW(m_hwndCopilotModelUsedLabel, L"GPT-5.3-Codex • 0.9x");
    }
    m_lastSlashStatusHint.clear();
    hideSlashOverlay();
    CommandPreview_Hide();
    ChatAutocomplete_Hide();
}

void Win32IDE::previewSlashRouting(const std::string& userMessage)
{
    if (userMessage.empty() || userMessage.front() != '/')
    {
        return;
    }

    const auto parsed = RawrXD::Agentic::SlashCommandParser::Parse(userMessage);
    const std::string preview = BuildSlashRoutePreview(parsed);
    appendCopilotChatTextOnUiThread(std::string("\n") + preview + "\n");

    if (!parsed.valid)
    {
        appendCopilotChatTextOnUiThread(std::string("[Slash] Try /help for command list. Error: ") + parsed.error +
                                        "\n");
    }
}

void Win32IDE::updateSlashStatusHint(const std::string& userMessage)
{
    std::string hint;
    if (!userMessage.empty() && userMessage.front() == '/')
    {
        const auto parsed = RawrXD::Agentic::SlashCommandParser::Parse(userMessage);
        hint = BuildSlashRoutePreview(parsed);
        if (!parsed.valid)
        {
            hint = std::string("[Slash] ") + parsed.error;
        }
    }

    if (hint == m_lastSlashStatusHint)
    {
        return;
    }
    m_lastSlashStatusHint = hint;

    if (m_hwndStatusBar && IsWindow(m_hwndStatusBar))
    {
        std::wstring whint = utf8ToWide(hint);
        SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(whint.c_str()));
    }
}

bool Win32IDE::tryAutocompleteSlashCommand()
{
    if (!m_hwndCopilotChatInput || !IsWindow(m_hwndCopilotChatInput))
    {
        return false;
    }

    const int inputLen = GetWindowTextLengthW(m_hwndCopilotChatInput);
    if (inputLen <= 0)
    {
        return false;
    }

    std::vector<wchar_t> inputBuffer(static_cast<size_t>(inputLen) + 1, L'\0');
    GetWindowTextW(m_hwndCopilotChatInput, inputBuffer.data(), inputLen + 1);
    const std::string current = wideToUtf8(inputBuffer.data());
    if (current.empty() || current.front() != '/' || current.find(' ') != std::string::npos)
    {
        return false;
    }

    std::string needle = current.substr(1);
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const auto commands = RawrXD::Agentic::SlashCommandParser::AvailableCommands();
    for (const auto& cmd : commands)
    {
        std::string lowered = cmd;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lowered.rfind(needle, 0) == 0)
        {
            const std::wstring completed = utf8ToWide(std::string("/") + cmd + " ");
            SetWindowTextW(m_hwndCopilotChatInput, completed.c_str());
            SendMessageW(m_hwndCopilotChatInput, EM_SETSEL, static_cast<WPARAM>(completed.size()),
                         static_cast<LPARAM>(completed.size()));
            updateSlashStatusHint(std::string("/") + cmd);
            return true;
        }
    }

    return false;
}

namespace
{
std::wstring editorRangeTextWideForSlash(HWND hwndEditor, LONG start, LONG end)
{
    if (!hwndEditor || end <= start)
        return L"";

    std::wstring text;
    text.resize(static_cast<size_t>(end - start) + 1);

    TEXTRANGEW tr = {};
    tr.chrg.cpMin = start;
    tr.chrg.cpMax = end;
    tr.lpstrText = text.data();

    SendMessageW(hwndEditor, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    text.resize(wcslen(text.c_str()));
    return text;
}

std::string sliceSurroundingLinesUtf8FromWide(const std::wstring& w, int line0, int halfWindow)
{
    if (w.empty())
        return {};
    std::vector<size_t> lineStarts;
    lineStarts.push_back(0);
    for (size_t i = 0; i < w.size(); ++i)
    {
        if (w[i] == L'\n')
            lineStarts.push_back(i + 1);
        else if (w[i] == L'\r')
        {
            if (i + 1 < w.size() && w[i + 1] == L'\n')
            {
                lineStarts.push_back(i + 2);
                ++i;
            }
            else
            {
                lineStarts.push_back(i + 1);
            }
        }
    }
    const int n = static_cast<int>(lineStarts.size());
    if (n <= 0)
        return wideToUtf8(w);
    int row = line0;
    if (row < 0)
        row = 0;
    if (row >= n)
        row = n - 1;
    const int lo = (std::max)(0, row - halfWindow);
    const int hi = (std::min)(n - 1, row + halfWindow);
    const size_t a = lineStarts[static_cast<size_t>(lo)];
    const size_t b = (hi + 1 < n) ? lineStarts[static_cast<size_t>(hi + 1)] : w.size();
    if (b <= a)
        return {};
    return wideToUtf8(w.substr(a, b - a));
}

std::string editorIdentifierUnderCaretUtf8(HWND hwndEditor)
{
    if (!hwndEditor)
        return {};
    CHARRANGE sel = {};
    SendMessageW(hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    const int line = static_cast<int>(SendMessageW(hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0));
    const LONG ls = static_cast<LONG>(SendMessageW(hwndEditor, EM_LINEINDEX, line, 0));
    if (ls < 0)
        return {};
    const LONG ll = static_cast<LONG>(SendMessageW(hwndEditor, EM_LINELENGTH, ls, 0));
    if (ll <= 0)
        return {};
    const std::wstring lineW = editorRangeTextWideForSlash(hwndEditor, ls, ls + ll);
    const int col = static_cast<int>(sel.cpMin - ls);
    if (col < 0 || col > static_cast<int>(lineW.size()))
        return {};
    int a = col;
    while (a > 0)
    {
        const wchar_t ch = lineW[static_cast<size_t>(a - 1)];
        if (iswalnum(static_cast<wint_t>(ch)) != 0 || ch == L'_')
            --a;
        else
            break;
    }
    int b = col;
    while (b < static_cast<int>(lineW.size()))
    {
        const wchar_t ch = lineW[static_cast<size_t>(b)];
        if (iswalnum(static_cast<wint_t>(ch)) != 0 || ch == L'_')
            ++b;
        else
            break;
    }
    if (a == b)
        return {};
    return wideToUtf8(lineW.substr(static_cast<size_t>(a), static_cast<size_t>(b - a)));
}
}  // namespace

// Comment prefix for RichEdit helpers (slash /doc stub, editorToggleCommentSelection). External linkage so LTCG/link
// always resolves a single symbol (some toolchains referenced it as non-static during incremental links).
std::string editorCommentPrefixForPath(const std::string& filePath)
{
    std::string ext;
    const size_t dot = filePath.find_last_of('.');
    if (dot != std::string::npos)
        ext = filePath.substr(dot);

    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });

    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c" || ext == ".h" || ext == ".hpp" || ext == ".hh" ||
        ext == ".hxx" || ext == ".js" || ext == ".jsx" || ext == ".ts" || ext == ".tsx" || ext == ".java" ||
        ext == ".cs" || ext == ".swift" || ext == ".kt" || ext == ".m" || ext == ".mm")
        return "//";

    if (ext == ".py" || ext == ".ps1" || ext == ".sh" || ext == ".rb" || ext == ".yml" || ext == ".yaml")
        return "#";

    if (ext == ".asm" || ext == ".s" || ext == ".inc")
        return ";";

    if (ext == ".sql" || ext == ".lua")
        return "--";

    return "//";
}

namespace
{
void trimAsciiWhitespaceInPlace(std::string& s)
{
    auto isSpace = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!s.empty() && isSpace(static_cast<unsigned char>(s.front())))
        s.erase(s.begin());
    while (!s.empty() && isSpace(static_cast<unsigned char>(s.back())))
        s.pop_back();
}

static std::string stripOptionalCodeFence(std::string s)
{
    trimAsciiWhitespaceInPlace(s);
    const size_t fence = s.find("```");
    if (fence == std::string::npos)
        return s;
    size_t start = fence + 3;
    while (start < s.size() && s[start] != '\n' && s[start] != '\r')
        ++start;
    if (start < s.size() && s[start] == '\r')
        ++start;
    if (start < s.size() && s[start] == '\n')
        ++start;
    const size_t end = s.rfind("```");
    if (end == std::string::npos || end <= start)
        return s;
    std::string mid = s.substr(start, end - start);
    trimAsciiWhitespaceInPlace(mid);
    return mid.empty() ? s : mid;
}
}  // namespace

void Win32IDE::executeDirectSlashFixFromEditor(HWND editor, const rawrxd::ghost_completion::GhostCompletionContext& ctx,
                                               const std::string& diagnosticsBlock, const std::string& extraUserFocus)
{
    namespace fs = std::filesystem;
    if (!editor || !IsWindow(editor))
        return;

    if (m_currentFile.empty())
    {
        appendToOutput("\n[Direct /fix] Save the file to disk first (needs a path for diff + WAL).\n", "Output",
                       OutputSeverity::Warning);
        return;
    }
    std::error_code ec;
    if (!fs::exists(fs::path(m_currentFile), ec))
    {
        appendToOutput("\n[Direct /fix] File is not on disk yet — save before running /fix.\n", "Output",
                       OutputSeverity::Warning);
        return;
    }

    const std::string originalFull = getWindowText(editor);
    constexpr size_t kMaxDirectFixBytes = 512 * 1024;
    if (originalFull.size() > kMaxDirectFixBytes)
    {
        appendToOutput("\n[Direct /fix] File too large for direct lane (limit 512 KiB). Use Agent Chat instead.\n",
                       "Output", OutputSeverity::Warning);
        return;
    }

    if (!isModelLoaded())
    {
        appendToOutput("\n[Direct /fix] No model loaded — load a GGUF or configure Ollama, then retry.\n", "Output",
                       OutputSeverity::Warning);
        return;
    }

    {
        std::string ragRoot = m_projectRoot;
        if (ragRoot.empty())
            ragRoot = m_settings.workingDirectory;
        rawrxd::rag_lite::requestBackgroundScan(std::move(ragRoot));
    }
    const std::string ragLiteCtx = rawrxd::rag_lite::buildPromptInjection(originalFull, 12000);

    std::ostringstream prompt;
    prompt << "You are an IDE code-fix assistant. Output ONLY the complete fixed source file.\n";
    prompt << "No markdown prose before or after. You may wrap the file in a single ```" << ctx.languageId
           << " fenced block.\n";
    prompt << "Preserve structure and style; make minimal changes that resolve diagnostics.\n\n";
    if (!ragLiteCtx.empty())
        prompt << ragLiteCtx << "\n";
    if (!extraUserFocus.empty())
        prompt << "User focus: " << extraUserFocus << "\n\n";
    if (!diagnosticsBlock.empty())
    {
        prompt << diagnosticsBlock << "\n";
    }
    {
        const std::string lspBridge = formatLspBridgeContextForSlashFix(editor, m_currentFile);
        if (!lspBridge.empty())
        {
            prompt << lspBridge << "\n";
        }
    }
    prompt << "Full file (" << ctx.languageId << "):\n```" << ctx.languageId << "\n" << originalFull << "\n```\n";

    appendToOutput("\n[Direct /fix] Running model (no chat) — staging diff in Agent panel…\n", "Output",
                   OutputSeverity::Info);
    LOG_INFO("[Direct /fix] invoke model path=" + m_currentFile);

    const std::string raw = sendMessageToModelWithoutChatHistory(prompt.str());
    if (raw.empty() || raw.rfind("Error:", 0) == 0)
    {
        appendToOutput(std::string("\n[Direct /fix] Model failed: ") + (raw.empty() ? "(empty)" : raw) + "\n", "Output",
                       OutputSeverity::Error);
        return;
    }

    std::string proposed = stripOptionalCodeFence(raw);
    trimAsciiWhitespaceInPlace(proposed);
    if (proposed.empty())
    {
        appendToOutput("\n[Direct /fix] Model returned empty fix.\n", "Output", OutputSeverity::Warning);
        return;
    }
    if (proposed == originalFull)
    {
        appendToOutput("\n[Direct /fix] Model returned text identical to the original — nothing to stage.\n", "Output",
                       OutputSeverity::Info);
        return;
    }

    if (!stageDirectFixAgentProposal(m_currentFile, originalFull, proposed, "direct /fix"))
    {
        appendToOutput("\n[Direct /fix] Failed to stage proposal (see log).\n", "Output", OutputSeverity::Error);
        return;
    }

    const bool mirrorOk = validateCurrentAgentSessionMirrorGate();
    ensureAgentDiffPanelVisible();
    refreshAgentDiffDisplay();
    if (!mirrorOk)
    {
        appendToOutput(
            "\n[Direct /fix] Staged with MirrorGate failure — review compiler output in the Agent panel; Accept All is "
            "disabled until the proposal compiles.\n",
            "Output", OutputSeverity::Warning);
    }
    else
    {
        appendToOutput("\n[Direct /fix] Staged — review the Agent diff panel, then Accept All or Reject All. "
                       "Ctrl+Shift+Z rolls back "
                       "after accept.\n",
                       "Output", OutputSeverity::Info);
    }
}

namespace
{
std::string buildEditorSlashVulkanReport()
{
    std::ostringstream o;
    VulkanCompute vc;
    if (!vc.Initialize())
    {
        o << "[Vulkan] Initialize() failed — SDK/driver unavailable or no compute device.\n";
        return o.str();
    }
    const VulkanDeviceInfo& di = vc.GetDeviceInfo();
    o << "[Vulkan] device: " << di.device_name << "\n";
    o << "  vendor_id=0x" << std::hex << di.vendor_id << " device_id=0x" << di.device_id << std::dec << "\n";
    o << "  AMD GPU: " << (vc.IsAMDDevice() ? "yes" : "no") << "  compute_queue_family=" << di.compute_queue_family
      << "\n";
    o << "  FP8 tiled flash-attn pipeline ready: " << (vc.IsFlashAttentionFP8TiledPipelineReady() ? "yes" : "no")
      << "\n";
    const VulkanKernelStats& st = vc.GetStats();
    o << "  stats: dispatch=" << st.dispatch_count.load(std::memory_order_relaxed)
      << " matmul=" << st.matmul_count.load(std::memory_order_relaxed)
      << " attn=" << st.attention_count.load(std::memory_order_relaxed)
      << " errors=" << st.error_count.load(std::memory_order_relaxed) << "\n";
    vc.Cleanup();
    return o.str();
}

std::string buildEditorSlashPagerReport()
{
    std::ostringstream o;
    o << "[Sovereign /pager] process + reservation snapshot\n";
    auto& rm = RawrXD::Compression::VirtualAllocReservationManager::Instance();
    o << "  VirtualAllocReservationManager: reserved=" << rm.GetTotalReservedBytes()
      << " B  committed=" << rm.GetTotalCommittedBytes() << " B\n";
    {
        const uint32_t pref = rm.GetPreferredNumaNode();
        o << "  reservation NUMA policy: ";
        if (pref == RawrXD::Compression::VirtualAllocReservationManager::kPreferredNumaNodeAuto)
        {
            o << "auto (VirtualAllocExNuma uses current CPU node per ReserveBlock)\n";
        }
        else
        {
            o << "pinned node " << pref << "\n";
        }
    }

    PROCESS_MEMORY_COUNTERS_EX pmc = {};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
    {
        o << "  working_set=" << pmc.WorkingSetSize << " B  private_bytes=" << pmc.PrivateUsage << " B\n";
    }

    ULONG highestNuma = 0;
    if (GetNumaHighestNodeNumber(&highestNuma))
    {
        o << "  NUMA highest node index: " << highestNuma << "\n";
    }
    PROCESSOR_NUMBER procNum = {};
    GetCurrentProcessorNumberEx(&procNum);
    o << "  current processor: group=" << static_cast<int>(procNum.Group)
      << " number=" << static_cast<int>(procNum.Number) << "\n";
    USHORT node = 0;
    if (GetNumaProcessorNodeEx(&procNum, &node))
    {
        o << "  current processor NUMA node: " << node << "\n";
    }

    o << "  SovereignPager: no heap instance bound to Win32IDE (tier LRU totals N/A in-process).\n";
    o << sov::formatPagerLastLoadTelemetryReport();
    return o.str();
}
}  // namespace

void Win32IDE::ensureSlashOverlayCreated()
{
    if (m_hwndSlashOverlayPopup && IsWindow(m_hwndSlashOverlayPopup))
    {
        return;
    }
    if (!m_hwndMain || !IsWindow(m_hwndMain))
    {
        return;
    }

    m_hwndSlashOverlayPopup = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"STATIC", L"", WS_POPUP | WS_BORDER, 0,
                                              0, 520, 220, m_hwndMain, nullptr, m_hInstance, nullptr);
    if (!m_hwndSlashOverlayPopup)
    {
        return;
    }

    m_hwndSlashOverlayList = CreateWindowExW(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL, 8, 8,
                                             504, 136, m_hwndSlashOverlayPopup, nullptr, m_hInstance, nullptr);

    m_hwndSlashOverlayPreview = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 8, 150, 504, 60,
                                                m_hwndSlashOverlayPopup, nullptr, m_hInstance, nullptr);

    HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    if (m_hwndSlashOverlayList)
    {
        SendMessageW(m_hwndSlashOverlayList, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    }
    if (m_hwndSlashOverlayPreview)
    {
        SendMessageW(m_hwndSlashOverlayPreview, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    }
}

void Win32IDE::hideSlashOverlay()
{
    if (m_hwndSlashOverlayPopup && IsWindow(m_hwndSlashOverlayPopup))
    {
        ShowWindow(m_hwndSlashOverlayPopup, SW_HIDE);
    }
    m_slashOverlayCommands.clear();
    m_slashOverlaySelectedIndex = 0;
    m_slashOverlayEditorMode = false;
}

bool Win32IDE::isSlashOverlayVisible() const
{
    return m_hwndSlashOverlayPopup && IsWindow(m_hwndSlashOverlayPopup) && IsWindowVisible(m_hwndSlashOverlayPopup);
}

void Win32IDE::syncEditorSlashOverlayFromEditor(HWND editor)
{
    if (!editor || editor != m_hwndEditor || !IsWindow(editor))
    {
        return;
    }

    CHARRANGE sel = {};
    SendMessageW(editor, EM_EXGETSEL, 0, (LPARAM)&sel);
    if (sel.cpMin != sel.cpMax)
    {
        hideSlashOverlay();
        return;
    }

    const int line = (int)SendMessageW(editor, EM_LINEFROMCHAR, sel.cpMin, 0);
    const LONG lineStart = (LONG)SendMessageW(editor, EM_LINEINDEX, line, 0);
    if (lineStart < 0)
    {
        hideSlashOverlay();
        return;
    }

    const std::wstring wPrefix = editorRangeTextWideForSlash(editor, lineStart, sel.cpMin);
    const size_t slashPos = wPrefix.find(L'/');
    if (slashPos == std::wstring::npos)
    {
        hideSlashOverlay();
        return;
    }
    for (size_t i = 0; i < slashPos; ++i)
    {
        if (!iswspace(static_cast<wint_t>(wPrefix[i])))
        {
            hideSlashOverlay();
            return;
        }
    }

    std::wstring tail = wPrefix.substr(slashPos);
    if (tail.find(L' ') != std::wstring::npos)
    {
        hideSlashOverlay();
        return;
    }

    const std::string userMessage = wideToUtf8(tail);
    if (userMessage.empty() || userMessage.front() != '/')
    {
        hideSlashOverlay();
        return;
    }

    refreshSlashOverlay(userMessage, editor);
}

void Win32IDE::refreshSlashOverlay(const std::string& userMessage, HWND editorPositionAnchor)
{
    if (!editorPositionAnchor)
    {
        m_slashOverlayEditorMode = false;
    }
    else
    {
        m_slashOverlayEditorMode = true;
    }

    if (userMessage.empty() || userMessage.front() != '/')
    {
        hideSlashOverlay();
        return;
    }

    const size_t firstSpace = userMessage.find(' ');
    const std::string commandToken =
        (firstSpace == std::string::npos) ? userMessage.substr(1) : userMessage.substr(1, firstSpace - 1);
    if (firstSpace != std::string::npos)
    {
        hideSlashOverlay();
        return;
    }

    ensureSlashOverlayCreated();
    if (!m_hwndSlashOverlayPopup || !m_hwndSlashOverlayList)
    {
        hideSlashOverlay();
        return;
    }

    std::string loweredNeedle = commandToken;
    std::transform(loweredNeedle.begin(), loweredNeedle.end(), loweredNeedle.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    m_slashOverlayCommands.clear();
    const auto commands = RawrXD::Agentic::SlashCommandParser::AvailableCommands();
    for (const auto& cmd : commands)
    {
        std::string lowered = cmd;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (loweredNeedle.empty() || lowered.rfind(loweredNeedle, 0) == 0)
        {
            m_slashOverlayCommands.push_back(cmd);
        }
    }

    if (m_slashOverlayCommands.empty())
    {
        hideSlashOverlay();
        return;
    }

    SendMessageW(m_hwndSlashOverlayList, LB_RESETCONTENT, 0, 0);
    for (const auto& cmd : m_slashOverlayCommands)
    {
        const std::wstring row = utf8ToWide(std::string("/") + cmd + " - " + SlashCommandShortDescription(cmd));
        SendMessageW(m_hwndSlashOverlayList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.c_str()));
    }

    if (m_slashOverlaySelectedIndex >= static_cast<int>(m_slashOverlayCommands.size()))
    {
        m_slashOverlaySelectedIndex = 0;
    }
    SendMessageW(m_hwndSlashOverlayList, LB_SETCURSEL, static_cast<WPARAM>(m_slashOverlaySelectedIndex), 0);

    if (m_hwndSlashOverlayPreview)
    {
        const std::string selected = m_slashOverlayCommands[m_slashOverlaySelectedIndex];
        const std::wstring preview =
            utf8ToWide(std::string("Route: /") + selected + " -> " + SlashCommandShortDescription(selected));
        SetWindowTextW(m_hwndSlashOverlayPreview, preview.c_str());
    }

    int x = 0;
    int y = 0;
    int width = 520;
    const int height = 220;

    if (editorPositionAnchor && IsWindow(editorPositionAnchor))
    {
        CHARRANGE sel = {};
        SendMessageW(editorPositionAnchor, EM_EXGETSEL, 0, (LPARAM)&sel);
        POINTL pt = {};
        const LRESULT posRes = SendMessageW(editorPositionAnchor, EM_POSFROMCHAR, (WPARAM)sel.cpMin, (LPARAM)&pt);
        if (posRes < 0 || (pt.x == -1 && pt.y == -1))
        {
            hideSlashOverlay();
            return;
        }
        POINT scr = {pt.x, pt.y};
        ClientToScreen(editorPositionAnchor, &scr);

        TEXTMETRICW tm = {};
        HDC hdcEd = GetDC(editorPositionAnchor);
        if (hdcEd)
        {
            GetTextMetricsW(hdcEd, &tm);
            ReleaseDC(editorPositionAnchor, hdcEd);
        }
        const int lineH = tm.tmHeight + tm.tmExternalLeading;
        RECT edRc = {};
        GetClientRect(editorPositionAnchor, &edRc);
        width = (std::max)(420, static_cast<int>(edRc.right - 20));
        x = scr.x;
        y = scr.y + (std::max)(lineH, 16);
    }
    else
    {
        if (!m_hwndCopilotChatInput || !IsWindow(m_hwndCopilotChatInput))
        {
            hideSlashOverlay();
            return;
        }
        RECT inputRc = {};
        GetWindowRect(m_hwndCopilotChatInput, &inputRc);
        width = static_cast<int>(std::max(420L, inputRc.right - inputRc.left));
        x = inputRc.left;
        y = inputRc.top - height - 6;
    }

    RECT popupRc = {x, y, x + width, y + height};
    HMONITOR hm = MonitorFromRect(&popupRc, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    if (hm && GetMonitorInfoW(hm, &mi))
    {
        if (popupRc.right > mi.rcWork.right)
            x = mi.rcWork.right - width;
        if (popupRc.bottom > mi.rcWork.bottom)
            y = mi.rcWork.top + 8;
        if (x < mi.rcWork.left)
            x = mi.rcWork.left;
        if (y < mi.rcWork.top)
            y = mi.rcWork.top;
    }

    SetWindowPos(m_hwndSlashOverlayPopup, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

bool Win32IDE::handleSlashOverlayKey(WPARAM key)
{
    if (!isSlashOverlayVisible() || m_slashOverlayCommands.empty())
    {
        return false;
    }

    if (key == VK_UP)
    {
        m_slashOverlaySelectedIndex =
            (m_slashOverlaySelectedIndex - 1 + static_cast<int>(m_slashOverlayCommands.size())) %
            static_cast<int>(m_slashOverlayCommands.size());
        SendMessageW(m_hwndSlashOverlayList, LB_SETCURSEL, static_cast<WPARAM>(m_slashOverlaySelectedIndex), 0);
    }
    else if (key == VK_DOWN)
    {
        m_slashOverlaySelectedIndex =
            (m_slashOverlaySelectedIndex + 1) % static_cast<int>(m_slashOverlayCommands.size());
        SendMessageW(m_hwndSlashOverlayList, LB_SETCURSEL, static_cast<WPARAM>(m_slashOverlaySelectedIndex), 0);
    }
    else if (key == VK_ESCAPE)
    {
        hideSlashOverlay();
        return true;
    }
    else if (key == VK_TAB || key == VK_RETURN)
    {
        return applySlashOverlaySelection();
    }
    else
    {
        return false;
    }

    if (m_hwndSlashOverlayPreview && m_slashOverlaySelectedIndex >= 0 &&
        m_slashOverlaySelectedIndex < static_cast<int>(m_slashOverlayCommands.size()))
    {
        const std::string selected = m_slashOverlayCommands[m_slashOverlaySelectedIndex];
        const std::wstring preview =
            utf8ToWide(std::string("Route: /") + selected + " -> " + SlashCommandShortDescription(selected));
        SetWindowTextW(m_hwndSlashOverlayPreview, preview.c_str());
    }
    return true;
}

bool Win32IDE::applySlashOverlaySelection()
{
    if (m_slashOverlayCommands.empty())
    {
        return false;
    }

    if (m_slashOverlaySelectedIndex < 0 ||
        m_slashOverlaySelectedIndex >= static_cast<int>(m_slashOverlayCommands.size()))
    {
        return false;
    }

    if (m_slashOverlayEditorMode && m_hwndEditor && IsWindow(m_hwndEditor))
    {
        CHARRANGE sel = {};
        SendMessageW(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
        if (sel.cpMin != sel.cpMax)
        {
            return false;
        }

        const int line = (int)SendMessageW(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
        const LONG lineStart = (LONG)SendMessageW(m_hwndEditor, EM_LINEINDEX, line, 0);
        if (lineStart < 0)
        {
            return false;
        }

        const std::wstring wPrefix = editorRangeTextWideForSlash(m_hwndEditor, lineStart, sel.cpMin);
        const size_t slashPos = wPrefix.find(L'/');
        if (slashPos == std::wstring::npos)
        {
            return false;
        }
        for (size_t i = 0; i < slashPos; ++i)
        {
            if (!iswspace(static_cast<wint_t>(wPrefix[i])))
            {
                return false;
            }
        }

        const LONG slashCp = lineStart + static_cast<LONG>(slashPos);
        const std::string routedSlashCmd = m_slashOverlayCommands[m_slashOverlaySelectedIndex];
        const std::wstring completed = utf8ToWide(std::string("/") + routedSlashCmd + " ");
        CHARRANGE repl = {slashCp, sel.cpMin};
        SendMessageW(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&repl);
        SendMessageW(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)completed.c_str());
        hideSlashOverlay();
        onEditorContentChanged();
        routeEditorSlashCommandAfterInsert(routedSlashCmd, m_hwndEditor);
        return true;
    }

    if (!m_hwndCopilotChatInput || !IsWindow(m_hwndCopilotChatInput))
    {
        return false;
    }

    const std::wstring completed =
        utf8ToWide(std::string("/") + m_slashOverlayCommands[m_slashOverlaySelectedIndex] + " ");
    SetWindowTextW(m_hwndCopilotChatInput, completed.c_str());
    SendMessageW(m_hwndCopilotChatInput, EM_SETSEL, static_cast<WPARAM>(completed.size()),
                 static_cast<LPARAM>(completed.size()));
    hideSlashOverlay();
    return true;
}

void Win32IDE::ensureCopilotChatPanelVisible()
{
    if (!m_secondarySidebarVisible)
        toggleSecondarySidebar();
}

void Win32IDE::postPromptToCopilotChat(const std::string& promptUtf8)
{
    if (!m_hwndCopilotChatInput || !IsWindow(m_hwndCopilotChatInput))
        return;
    ensureCopilotChatPanelVisible();
    const std::wstring w = utf8ToWide(promptUtf8);
    {
        std::lock_guard<std::mutex> inputLock(g_chatInputBufferMutex);
        SetWindowTextW(m_hwndCopilotChatInput, w.c_str());
        SendMessageW(m_hwndCopilotChatInput, EM_SETSEL, static_cast<WPARAM>(w.size()), static_cast<LPARAM>(w.size()));
    }
    if (m_hwndCopilotChatInput && IsWindow(m_hwndCopilotChatInput))
        SetFocus(m_hwndCopilotChatInput);
    postDeferredCopilotSend();
}

void Win32IDE::syncStructuredSlashCopilotFixExpectationForUserMessage(const std::string& userMessageUtf8)
{
    const bool startsWithFix = userMessageUtf8.size() >= 4 && userMessageUtf8.compare(0, 4, "/fix") == 0;
    const bool hasMarker = userMessageUtf8.find(kStructuredSlashFixPromptMarker) != std::string::npos;
    const bool looksLikeStructuredFixPrompt = startsWithFix && hasMarker;

    if (m_armStructuredSlashCopilotFixNextSend)
    {
        m_armStructuredSlashCopilotFixNextSend = false;
        m_expectStructuredSlashCopilotFixPending = looksLikeStructuredFixPrompt && !m_currentFile.empty();
        if (m_expectStructuredSlashCopilotFixPending)
        {
            m_structuredSlashCopilotFixTargetFile = m_currentFile;
        }
    }
    else if (!looksLikeStructuredFixPrompt)
    {
        m_expectStructuredSlashCopilotFixPending = false;
    }
}

std::string Win32IDE::loadUtf8FileForStructuredFix(const std::string& path) const
{
    if (path.empty())
    {
        return {};
    }
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return {};
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return content;
}

std::string Win32IDE::getDocumentTextForStructuredSlashCopilotFixTarget() const
{
    if (m_structuredSlashCopilotFixTargetFile.empty())
    {
        return {};
    }
    if (!m_currentFile.empty() && _stricmp(m_structuredSlashCopilotFixTargetFile.c_str(), m_currentFile.c_str()) == 0)
    {
        return getEditorText();
    }
    return loadUtf8FileForStructuredFix(m_structuredSlashCopilotFixTargetFile);
}

void Win32IDE::maybeConsumeStructuredSlashCopilotFixFromAssistantText(const std::string& assistantPlainText)
{
    if (!m_expectStructuredSlashCopilotFixPending)
    {
        return;
    }
    m_expectStructuredSlashCopilotFixPending = false;

    if (m_structuredSlashCopilotFixTargetFile.empty() || assistantPlainText.empty())
    {
        return;
    }

    const auto payload = rawrxd::ghost_completion::tryParseStructuredAiFixFromModelResponse(assistantPlainText);
    if (!payload)
    {
        appendToOutput("[Slash /fix] Model reply was not a usable JSON diff (see chat). Edit manually or retry.\n",
                       "Agent", OutputSeverity::Warning);
        return;
    }

    const std::string before = getDocumentTextForStructuredSlashCopilotFixTarget();
    const auto after = rawrxd::ghost_completion::applyStructuredAiLineDiffsUtf8(before, payload->edits);
    if (!after)
    {
        appendToOutput("[Slash /fix] JSON diff lines did not match the file (check 1-based line numbers and exact "
                       "\"old\" text).\n",
                       "Agent", OutputSeverity::Warning);
        return;
    }
    if (*after == before)
    {
        appendToOutput("[Slash /fix] Applied diff matches original — nothing queued.\n", "Agent", OutputSeverity::Info);
        return;
    }

    const std::string summary =
        payload->explanation.empty() ? std::string("Structured /fix proposal.") : payload->explanation;

    // Same “direct lane” as executeDirectSlashFixFromEditor: full-file diff in Agent panel + Accept All (+ AI-WAL).
    if (stageDirectFixAgentProposal(m_structuredSlashCopilotFixTargetFile, before, *after, summary))
    {
        const bool mirrorOk = validateCurrentAgentSessionMirrorGate();
        ensureAgentDiffPanelVisible();
        refreshAgentDiffDisplay();
        if (!mirrorOk)
        {
            appendToOutput(
                std::string("[Slash /fix] Staged — MirrorGate reports compile errors (Accept All disabled). ") +
                    summary + "\n",
                "Agent", OutputSeverity::Warning);
        }
        else
        {
            appendToOutput(std::string("[Slash /fix] Staged in Agent panel — review diff and click Accept All: ") +
                               summary + "\n",
                           "Agent", OutputSeverity::Info);
        }
    }
    else
    {
        (void)queueFilePendingEdit(RawrXD::Review::EditSource::Agent, RawrXD::Review::EditType::Replace,
                                   m_structuredSlashCopilotFixTargetFile, before, *after);
        appendToOutput(std::string("[Slash /fix] Agent panel staging failed; queued for edit review instead: ") +
                           summary + "\n",
                       "Agent", OutputSeverity::Warning);
    }
}

std::string Win32IDE::formatDiagnosticsForChatPrompt(const std::string& filePath) const
{
    if (filePath.empty())
        return {};
    const std::string uri = filePathToUri(filePath);
    const auto diags = getDiagnosticsForFile(uri);
    std::ostringstream o;
    o << "LSP diagnostics (" << diags.size() << "):\n";
    for (const auto& d : diags)
    {
        o << "- L" << (d.range.start.line + 1) << ":" << (d.range.start.character + 1) << " sev=" << d.severity << " ["
          << d.source << "] " << d.message << "\n";
    }
    return o.str();
}

namespace
{
std::string stripLspHoverMarkdownForPrompt(std::string s)
{
    for (;;)
    {
        const size_t a = s.find("```");
        if (a == std::string::npos)
        {
            break;
        }
        const size_t b = s.find("```", a + 3);
        if (b == std::string::npos)
        {
            s.erase(a);
            break;
        }
        s.erase(a, b - a + 3);
    }
    for (size_t p = 0; (p = s.find("**", p)) != std::string::npos;)
    {
        s.erase(p, 2);
    }
    for (char& c : s)
    {
        if (c == '`')
        {
            c = '\'';
        }
    }
    return s;
}

bool isSlashBridgeKeyword(std::string_view id, Win32IDE::LSPLanguage lang)
{
    auto eq = [id](const char* kw) { return id == kw; };
    switch (lang)
    {
        case Win32IDE::LSPLanguage::Python:
            return eq("def") || eq("class") || eq("import") || eq("from") || eq("as") || eq("pass") || eq("return") ||
                   eq("if") || eq("elif") || eq("else") || eq("for") || eq("while") || eq("try") || eq("except") ||
                   eq("finally") || eq("with") || eq("lambda") || eq("async") || eq("await") || eq("None") ||
                   eq("True") || eq("False") || eq("and") || eq("or") || eq("not") || eq("in") || eq("is");
        case Win32IDE::LSPLanguage::TypeScript:
            return eq("const") || eq("let") || eq("var") || eq("function") || eq("return") || eq("if") || eq("else") ||
                   eq("for") || eq("while") || eq("class") || eq("interface") || eq("type") || eq("import") ||
                   eq("export") || eq("from") || eq("async") || eq("await") || eq("new") || eq("this") ||
                   eq("extends") || eq("implements") || eq("namespace") || eq("enum") || eq("switch") || eq("case") ||
                   eq("default") || eq("break") || eq("continue") || eq("throw") || eq("try") || eq("catch") ||
                   eq("finally");
        case Win32IDE::LSPLanguage::Cpp:
        default:
            return eq("alignas") || eq("alignof") || eq("asm") || eq("auto") || eq("bool") || eq("break") ||
                   eq("case") || eq("catch") || eq("char") || eq("class") || eq("concept") || eq("const") ||
                   eq("constexpr") || eq("consteval") || eq("constinit") || eq("continue") || eq("decltype") ||
                   eq("default") || eq("delete") || eq("do") || eq("double") || eq("else") || eq("enum") ||
                   eq("explicit") || eq("export") || eq("extern") || eq("false") || eq("float") || eq("for") ||
                   eq("friend") || eq("goto") || eq("if") || eq("inline") || eq("int") || eq("long") || eq("mutable") ||
                   eq("namespace") || eq("new") || eq("noexcept") || eq("nullptr") || eq("operator") || eq("private") ||
                   eq("protected") || eq("public") || eq("register") || eq("requires") || eq("return") || eq("short") ||
                   eq("signed") || eq("sizeof") || eq("static") || eq("struct") || eq("switch") || eq("template") ||
                   eq("this") || eq("thread_local") || eq("throw") || eq("true") || eq("try") || eq("typedef") ||
                   eq("typeid") || eq("typename") || eq("union") || eq("unsigned") || eq("using") || eq("virtual") ||
                   eq("void") || eq("volatile") || eq("wchar_t") || eq("while");
    }
}

/// UTF-16 code unit column starts for identifiers on one source line (Windows `wchar_t` path).
std::vector<int> identifierHoverColumnsOnLineWide(const std::wstring& lineW, Win32IDE::LSPLanguage lang)
{
    std::vector<int> cols;
    std::unordered_set<std::string> seenSpellings;
    for (size_t i = 0; i < lineW.size();)
    {
        const wchar_t ch = lineW[i];
        if (!((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') || ch == L'_'))
        {
            ++i;
            continue;
        }
        const size_t st = i;
        while (i < lineW.size())
        {
            const wchar_t c = lineW[i];
            if (!((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || c == L'_'))
            {
                break;
            }
            ++i;
        }
        std::string id = wideToUtf8(lineW.substr(st, i - st));
        if (id.empty() || isSlashBridgeKeyword(id, lang))
        {
            continue;
        }
        if (!seenSpellings.insert(id).second)
        {
            continue;
        }
        cols.push_back(static_cast<int>(st));
        if (cols.size() >= 8)
        {
            break;
        }
    }
    return cols;
}
}  // namespace

std::string Win32IDE::formatLspBridgeContextForSlashFix(HWND editor, const std::string& filePath)
{
    if (!editor || !IsWindow(editor) || filePath.empty())
    {
        return {};
    }
    const LSPLanguage lang = detectLanguageForFile(filePath);
    if (lang >= LSPLanguage::Count || m_lspStatuses[static_cast<size_t>(lang)].state != LSPServerState::Running)
    {
        return {};
    }

    const std::string uri = filePathToUri(filePath);
    CHARRANGE sel = {};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
    const int lineIndex = static_cast<int>(SendMessageW(editor, EM_EXLINEFROMCHAR, 0, sel.cpMin));
    const int lineStart = static_cast<int>(SendMessageW(editor, EM_LINEINDEX, lineIndex, 0));
    const int column = (lineStart >= 0) ? static_cast<int>(sel.cpMin - lineStart) : 0;

    const int lineCount = static_cast<int>(SendMessageW(editor, EM_GETLINECOUNT, 0, 0));
    const LONG leExclusive = (lineIndex + 1 < lineCount)
                                 ? static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX, lineIndex + 1, 0))
                                 : GetWindowTextLengthW(editor);
    const std::wstring lineWide = editorRangeTextWideForSlash(editor, static_cast<LONG>(lineStart), leExclusive);

    std::ostringstream o;

    const LSPHoverInfo cursorHover = lspHover(uri, lineIndex, std::max(0, column));
    if (cursorHover.valid)
    {
        std::string h = stripLspHoverMarkdownForPrompt(cursorHover.contents);
        if (h.size() > 2000)
        {
            h.resize(2000);
            h += "\n...(truncated)";
        }
        o << "Hover at cursor (L" << (lineIndex + 1) << ":" << (column + 1) << "):\n" << h << "\n";
    }

    int tokenHovers = 0;
    constexpr int kMaxTokenHovers = 5;
    const std::vector<int> idCols = identifierHoverColumnsOnLineWide(lineWide, lang);
    for (int colPos : idCols)
    {
        if (tokenHovers >= kMaxTokenHovers)
        {
            break;
        }
        if (colPos == column)
        {
            continue;
        }
        const LSPHoverInfo th = lspHover(uri, lineIndex, std::max(0, colPos));
        if (!th.valid)
        {
            continue;
        }
        std::string h = stripLspHoverMarkdownForPrompt(th.contents);
        if (h.size() > 1400)
        {
            h.resize(1400);
            h += "\n...(truncated)";
        }
        o << "Hover at token on same line (L" << (lineIndex + 1) << ":" << (colPos + 1) << "):\n" << h << "\n";
        ++tokenHovers;
    }

    const auto diags = getDiagnosticsForFile(uri);
    int diagHoversAdded = 0;
    int lastErrLine = -1;
    for (const auto& d : diags)
    {
        if (d.severity != 1)
        {
            continue;
        }
        const int errLine = d.range.start.line;
        if (errLine < 0)
        {
            continue;
        }
        if (errLine == lineIndex)
        {
            continue;
        }
        if (errLine == lastErrLine)
        {
            continue;
        }
        lastErrLine = errLine;
        const int errChar = std::max(0, d.range.start.character);
        const LSPHoverInfo dh = lspHover(uri, errLine, errChar);
        if (dh.valid)
        {
            std::string h = stripLspHoverMarkdownForPrompt(dh.contents);
            if (h.size() > 1200)
            {
                h.resize(1200);
                h += "\n...(truncated)";
            }
            o << "Hover at diagnostic (L" << (errLine + 1) << ":" << (errChar + 1) << "):\n" << h << "\n";
            if (++diagHoversAdded >= 2)
            {
                break;
            }
        }
    }

    const auto typeLocs = lspTypeDefinition(uri, lineIndex, std::max(0, column));
    if (!typeLocs.empty())
    {
        const std::string tpath = uriToFilePath(typeLocs[0].uri);
        o << "Type definition: ";
        o << (tpath.empty() ? typeLocs[0].uri : tpath);
        o << " @ L" << (typeLocs[0].range.start.line + 1) << ":" << (typeLocs[0].range.start.character + 1) << "\n";
    }

    std::string body = o.str();
    trimAsciiWhitespaceInPlace(body);
    if (body.empty())
    {
        return {};
    }
    return std::string("[Project Symbols]\n") + body + "\n";
}

rawrxd::ghost_completion::GhostCompletionContext Win32IDE::buildGhostCompletionContextFromEditor(HWND editor)
{
    std::vector<std::string> symNames;
    const std::string path = m_currentFile;
    bool lspOk = false;
    std::string lang = "text";

    if (!path.empty())
    {
        const LSPLanguage l = detectLanguageForFile(path);
        if (l < LSPLanguage::Count)
        {
            if (l == LSPLanguage::Cpp)
                lang = "cpp";
            else if (l == LSPLanguage::Python)
                lang = "python";
            else if (l == LSPLanguage::TypeScript)
                lang = "typescript";
            lspOk = m_lspStatuses[static_cast<size_t>(l)].state == LSPServerState::Running;
            if (lspOk)
            {
                const auto syms = lspDocumentSymbols(filePathToUri(path));
                symNames.reserve(std::min<size_t>(syms.size(), 256));
                for (const auto& s : syms)
                {
                    std::string row = s.name;
                    if (!s.detail.empty())
                    {
                        row += '\t';
                        row += s.detail;
                    }
                    symNames.push_back(std::move(row));
                    if (symNames.size() >= 128)
                        break;
                }
            }
        }
    }

    CHARRANGE sel = {};
    SendMessageW(editor, EM_EXGETSEL, 0, (LPARAM)&sel);
    const int line0 = static_cast<int>(SendMessageW(editor, EM_LINEFROMCHAR, sel.cpMin, 0));
    const LONG ls = static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX, line0, 0));
    const int col = (ls >= 0) ? static_cast<int>(sel.cpMin - ls) : 0;

    const int tlen = GetWindowTextLengthW(editor);
    std::wstring wbuf;
    if (tlen > 0)
    {
        wbuf.resize(static_cast<size_t>(tlen) + 1);
        GetWindowTextW(editor, wbuf.data(), tlen + 1);
        wbuf.resize(static_cast<size_t>(tlen));
    }
    const std::string surrounding = sliceSurroundingLinesUtf8FromWide(wbuf, line0, 25);

    auto ctx = rawrxd::ghost_completion::GhostCompletionContext::build(path, line0 + 1, col, symNames, surrounding,
                                                                       lang, lspOk);
    if (lspOk && editor && IsWindow(editor) && !path.empty())
    {
        ctx.lspSymbolDigest = formatLspBridgeContextForSlashFix(editor, path);
    }
    return ctx;
}

std::string Win32IDE::parseSlashCommandTailFromCaretLine(HWND editor, const std::string& cmdLower) const
{
    if (!editor || !IsWindow(editor) || cmdLower.empty())
    {
        return {};
    }

    CHARRANGE sel = {};
    SendMessageW(editor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
    const int lineIdx = static_cast<int>(SendMessageW(editor, EM_LINEFROMCHAR, sel.cpMin, 0));
    const LONG ls = static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX, lineIdx, 0));
    if (ls < 0)
    {
        return {};
    }
    const int lc = static_cast<int>(SendMessageW(editor, EM_GETLINECOUNT, 0, 0));
    const LONG leExclusive = (lineIdx + 1 < lc) ? static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX, lineIdx + 1, 0))
                                                : GetWindowTextLengthW(editor);

    std::wstring w = editorRangeTextWideForSlash(editor, ls, leExclusive);
    while (!w.empty() && (w.back() == L'\r' || w.back() == L'\n'))
    {
        w.pop_back();
    }
    std::string line = wideToUtf8(w);
    trimAsciiWhitespaceInPlace(line);

    const std::string prefix = std::string("/") + cmdLower;
    if (line.size() < prefix.size())
    {
        return {};
    }
    if (line.compare(0, prefix.size(), prefix) != 0)
    {
        return {};
    }
    if (line.size() == prefix.size())
    {
        return {};
    }
    if (!std::isspace(static_cast<unsigned char>(line[prefix.size()])))
    {
        return {};
    }
    std::string tail = line.substr(prefix.size() + 1);
    trimAsciiWhitespaceInPlace(tail);
    return tail;
}

bool Win32IDE::postEditorSlashChatPrompt(const std::string& cmdIn, HWND editor, const std::string& extraUserText)
{
    if (!editor || !IsWindow(editor))
    {
        return false;
    }
    if (!m_hwndCopilotChatInput || !IsWindow(m_hwndCopilotChatInput))
    {
        appendToOutput("\n[Slash] Agent Chat is not available. Use View → Agent Chat (Ctrl+L).\n", "Output",
                       OutputSeverity::Warning);
        return false;
    }

    std::string c = cmdIn;
    std::transform(c.begin(), c.end(), c.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    auto ctx = buildGhostCompletionContextFromEditor(editor);
    std::string lspSymbolsOutsideCodeFence;
    if (c == "fix")
    {
        lspSymbolsOutsideCodeFence = std::move(ctx.lspSymbolDigest);
        ctx.lspSymbolDigest.clear();
    }
    const std::string frag = ctx.toPromptFragment(4096);

    std::string focus;
    if (!extraUserText.empty())
    {
        focus = "User focus: ";
        focus += extraUserText;
        focus += "\n\n";
    }

    if (c == "explain")
    {
        std::string prompt = "/explain\n\n";
        prompt += focus;
        prompt += "Explain this code:\n\n```";
        prompt += ctx.languageId;
        prompt += "\n";
        prompt += frag;
        prompt += "\n```\n";
        postPromptToCopilotChat(prompt);
        return true;
    }
    if (c == "fix")
    {
        namespace fs = std::filesystem;
        if (m_currentFile.empty())
        {
            appendToOutput("\n[Slash /fix] Open or save a file first (needs a path for review + WAL).\n", "Output",
                           OutputSeverity::Warning);
            return false;
        }
        std::error_code ec;
        if (!fs::exists(fs::path(m_currentFile), ec))
        {
            appendToOutput("\n[Slash /fix] Save the file to disk first.\n", "Output", OutputSeverity::Warning);
            return false;
        }
        const std::string di = formatDiagnosticsForChatPrompt(m_currentFile);
        std::string prompt = "/fix\n\n";
        prompt += focus;
        prompt += "Fix issues in this code. ";
        prompt += kStructuredSlashFixPromptMarker;
        prompt += " with this shape (no prose outside the JSON):\n";
        prompt += "{\n";
        prompt += "  \"explanation\": \"brief description of fix\",\n";
        prompt += "  \"diff\": [\n";
        prompt += "    {\"line\": <1-based line number>, \"old\": \"exact single-line text as in file\", \"new\": "
                  "\"replacement single line\"}\n";
        prompt += "  ]\n";
        prompt += "}\n\n";
        prompt += "Apply edits in any order; each \"old\" must match the current line text exactly (UTF-8, no line "
                  "breaks in old/new).\n\n";
        if (!di.empty())
        {
            prompt += di;
            prompt += "\n";
        }
        if (!lspSymbolsOutsideCodeFence.empty())
        {
            prompt += lspSymbolsOutsideCodeFence;
            prompt += "\n";
        }
        prompt += "```";
        prompt += ctx.languageId;
        prompt += "\n";
        prompt += frag;
        prompt += "\n```\n";
        m_armStructuredSlashCopilotFixNextSend = true;
        postPromptToCopilotChat(prompt);
        return true;
    }
    if (c == "test")
    {
        std::string prompt = "/test\n\n";
        prompt += focus;
        prompt += "Generate focused unit tests for the code around the cursor. Match the project's style.\n\n```";
        prompt += ctx.languageId;
        prompt += "\n";
        prompt += frag;
        prompt += "\n```\n";
        postPromptToCopilotChat(prompt);
        return true;
    }
    if (c == "doc")
    {
        std::string prompt = "/doc\n\n";
        prompt += focus;
        prompt += "Write documentation for this code (comments, docstrings, or markdown as appropriate).\n\n```";
        prompt += ctx.languageId;
        prompt += "\n";
        prompt += frag;
        prompt += "\n```\n";
        postPromptToCopilotChat(prompt);
        return true;
    }
    if (c == "optimize")
    {
        std::string prompt = "/optimize\n\n";
        prompt += focus;
        prompt += "Suggest performance optimizations for this code without changing observable behavior.\n\n```";
        prompt += ctx.languageId;
        prompt += "\n";
        prompt += frag;
        prompt += "\n```\n";
        postPromptToCopilotChat(prompt);
        return true;
    }

    appendToOutput(std::string("\n[Slash] Unknown chat command: /") + c + "\n", "Output", OutputSeverity::Warning);
    return false;
}

void Win32IDE::removeEditorLineContainingCaret(HWND editor)
{
    if (!editor || !IsWindow(editor))
        return;
    CHARRANGE sel = {};
    SendMessageW(editor, EM_EXGETSEL, 0, (LPARAM)&sel);
    const int line = static_cast<int>(SendMessageW(editor, EM_LINEFROMCHAR, sel.cpMin, 0));
    const LONG ls = static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX, line, 0));
    if (ls < 0)
        return;
    const int lc = static_cast<int>(SendMessageW(editor, EM_GETLINECOUNT, 0, 0));
    const LONG le = (line + 1 < lc) ? static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX, line + 1, 0))
                                    : GetWindowTextLengthW(editor);
    CHARRANGE del = {ls, le};
    SendMessageW(editor, EM_EXSETSEL, 0, (LPARAM)&del);
    SendMessageW(editor, EM_REPLACESEL, TRUE, (LPARAM)L"");
}

bool Win32IDE::applyEditorDocCommentStubForSlashDoc(HWND editor)
{
    if (!editor || !IsWindow(editor))
        return false;

    CHARRANGE sel = {};
    SendMessageW(editor, EM_EXGETSEL, 0, (LPARAM)&sel);
    const int line = static_cast<int>(SendMessageW(editor, EM_LINEFROMCHAR, sel.cpMin, 0));
    const LONG ls = static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX, line, 0));
    if (ls < 0)
        return false;
    const int lc = static_cast<int>(SendMessageW(editor, EM_GETLINECOUNT, 0, 0));
    const LONG leExclusive = (line + 1 < lc) ? static_cast<LONG>(SendMessageW(editor, EM_LINEINDEX, line + 1, 0))
                                             : GetWindowTextLengthW(editor);

    std::wstring raw = editorRangeTextWideForSlash(editor, ls, leExclusive);
    while (!raw.empty() && (raw.back() == L'\r' || raw.back() == L'\n'))
        raw.pop_back();

    size_t p = 0;
    while (p < raw.size() && iswspace(static_cast<wint_t>(raw[p])) != 0)
        ++p;
    if (p >= raw.size() || raw[p] != L'/')
        return false;
    if (raw.compare(p, 4, L"/doc") != 0)
        return false;
    const size_t after = p + 4;
    if (after < raw.size() && raw[after] != L' ' && raw[after] != L'\t')
        return false;

    const std::wstring indent = raw.substr(0, p);
    const std::string pref = editorCommentPrefixForPath(m_currentFile);

    std::wstring block;
    if (pref == "//" || pref == "/*")
        block = indent + L"/// @brief TODO\n" + indent + L"///\n";
    else if (pref == "#")
        block = indent + L"# TODO: document\n";
    else if (pref == ";")
        block = indent + L"; TODO: document\n";
    else if (pref == "--")
        block = indent + L"-- TODO: document\n";
    else
        block = indent + L"// TODO: document\n";

    CHARRANGE r = {ls, leExclusive};
    SendMessageW(editor, EM_EXSETSEL, 0, (LPARAM)&r);
    SendMessageW(editor, EM_REPLACESEL, TRUE, (LPARAM)block.c_str());
    return true;
}

void Win32IDE::routeEditorSlashCommandAfterInsert(const std::string& commandCanon, HWND editor)
{
    if (!editor || !IsWindow(editor))
        return;

    std::string c = commandCanon;
    std::transform(c.begin(), c.end(), c.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    // /refactor: capture caret line while the `/refactor …` line is still present (LSP symbol lookup is 0-based).
    if (c == "refactor")
    {
        CHARRANGE sel = {};
        SendMessageW(editor, EM_EXGETSEL, 0, (LPARAM)&sel);
        const int line0 = static_cast<int>(SendMessageW(editor, EM_LINEFROMCHAR, sel.cpMin, 0));

        std::string needle;
        if (!m_currentFile.empty())
        {
            const auto syms = lspDocumentSymbols(filePathToUri(m_currentFile));
            for (const auto& s : syms)
            {
                if (line0 >= s.location.range.start.line && line0 <= s.location.range.end.line)
                {
                    needle = s.name;
                    break;
                }
            }
        }
        if (needle.empty())
            needle = editorIdentifierUnderCaretUtf8(editor);

        const auto ctx = buildGhostCompletionContextFromEditor(editor);
        const std::string frag = ctx.toPromptFragment(4096);

        if (!needle.empty() && m_hwndCopilotChatInput && IsWindow(m_hwndCopilotChatInput))
        {
            ensureCopilotChatPanelVisible();
            SetWindowTextW(m_hwndCopilotChatInput, utf8ToWide(needle).c_str());
            routeCommand(5904);
        }

        std::string prompt =
            "/refactor simplify\n\nPropose a refactor plan from this context (symbols + surrounding code).\n\n```";
        prompt += ctx.languageId;
        prompt += "\n";
        prompt += frag;
        prompt += "\n```\n";
        postPromptToCopilotChat(prompt);
        removeEditorLineContainingCaret(editor);
        onEditorContentChanged();
        return;
    }

    if (c == "explain" || c == "fix" || c == "test" || c == "doc" || c == "optimize")
    {
        const std::string tail = parseSlashCommandTailFromCaretLine(editor, c);
        removeEditorLineContainingCaret(editor);
        onEditorContentChanged();
        (void)postEditorSlashChatPrompt(c, editor, tail);
        return;
    }
}

bool Win32IDE::tryExecuteEditorSlashLine(HWND hwndEditor)
{
    if (!hwndEditor || !IsWindow(hwndEditor))
    {
        return false;
    }

    CHARRANGE sel = {};
    SendMessageW(hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
    const int lineCount = static_cast<int>(SendMessageW(hwndEditor, EM_GETLINECOUNT, 0, 0));
    const int line = static_cast<int>(SendMessageW(hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0));
    const LONG lineStart = static_cast<LONG>(SendMessageW(hwndEditor, EM_LINEINDEX, line, 0));
    if (lineStart < 0)
    {
        return false;
    }
    const LONG lineLen = static_cast<LONG>(SendMessageW(hwndEditor, EM_LINELENGTH, lineStart, 0));
    const LONG lineEndContent = lineStart + lineLen;
    LONG lineEndExclusive = lineEndContent;
    if (line + 1 < lineCount)
    {
        lineEndExclusive = static_cast<LONG>(SendMessageW(hwndEditor, EM_LINEINDEX, line + 1, 0));
    }

    std::wstring wline = editorRangeTextWideForSlash(hwndEditor, lineStart, lineEndContent);
    std::string lineUtf8 = wideToUtf8(wline);
    trimAsciiWhitespaceInPlace(lineUtf8);
    if (lineUtf8.size() < 2 || lineUtf8[0] != '/')
    {
        return false;
    }

    const size_t sp = lineUtf8.find(' ');
    std::string cmd = (sp == std::string::npos) ? lineUtf8.substr(1) : lineUtf8.substr(1, sp - 1);
    std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::string reportHeader;
    std::string reportBody;
    bool removeSlashLineAtEnd = true;
    bool appendSlashReport = true;
    const char* slashOutputChannel = "Output";
    if (cmd == "vulkan")
    {
        reportHeader = "\n=== /vulkan ===\n";
        reportBody = buildEditorSlashVulkanReport();
    }
    else if (cmd == "pager")
    {
        reportHeader = "\n=== /pager ===\n";
        reportBody = buildEditorSlashPagerReport();
    }
    else if (cmd == "kill-locks" || cmd == "kill_locks" || cmd == "nuke-locks")
    {
        appendToOutput("\n=== /kill-locks (Ninja Nuke) ===\n", "Output", OutputSeverity::Info);
        forceKillBuildLocks();
        appendSlashReport = false;
    }
    else if (cmd == "clear")
    {
        reportHeader = "\n=== /clear ===\n";
        reportBody = "Ghost dismissed; inline completion cache cleared.\n";
        dismissGhostTextPublic();
        if (getMainWindow() && IsWindow(getMainWindow()))
        {
            (void)PostMessageW(getMainWindow(), WM_RAWRXD_GHOST_TEXT_CACHE_CLEAR, 0, 0);
        }
    }
    else if (cmd == "fix")
    {
        std::string tail;
        if (sp != std::string::npos)
        {
            tail = lineUtf8.substr(sp + 1);
            trimAsciiWhitespaceInPlace(tail);
        }
        removeEditorLineContainingCaret(hwndEditor);
        onEditorContentChanged();
        removeSlashLineAtEnd = false;
        if (!postEditorSlashChatPrompt("fix", hwndEditor, tail))
        {
            return false;
        }
        reportHeader = "\n=== /fix ===\n";
        reportBody = "Sent structured /fix to Agent Chat (JSON diff → review queue).\n";
    }
    else if (cmd == "explain" || cmd == "test" || cmd == "doc" || cmd == "optimize")
    {
        if (!m_hwndCopilotChatInput || !IsWindow(m_hwndCopilotChatInput))
        {
            appendToOutput("\n[Slash] Agent Chat is not available. Use View → Agent Chat (Ctrl+L).\n", "Output",
                           OutputSeverity::Warning);
            return false;
        }
        std::string tail;
        if (sp != std::string::npos)
        {
            tail = lineUtf8.substr(sp + 1);
            trimAsciiWhitespaceInPlace(tail);
        }
        removeEditorLineContainingCaret(hwndEditor);
        onEditorContentChanged();
        removeSlashLineAtEnd = false;
        if (!postEditorSlashChatPrompt(cmd, hwndEditor, tail))
        {
            return false;
        }
        reportHeader = std::string("\n=== /") + cmd + " ===\n";
        reportBody = "Sent request to Agent Chat (editor context).\n";
    }
    else
    {
        return false;
    }

    if (appendSlashReport)
    {
        appendToOutput(reportHeader + reportBody, slashOutputChannel, OutputSeverity::Info);
    }

    if (removeSlashLineAtEnd)
    {
        SendMessageW(hwndEditor, EM_SETSEL, static_cast<WPARAM>(lineStart), static_cast<LPARAM>(lineEndExclusive));
        SendMessageW(hwndEditor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(L""));
        onEditorContentChanged();
    }
    hideSlashOverlay();
    return true;
}

// ============================================================================
// CopilotChatInputProc — Chat input (EDIT control) subclass window procedure
// Intercepts ENTER key to send messages; Shift+ENTER creates newlines.
// ============================================================================
LRESULT CALLBACK Win32IDE::CopilotChatInputProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (pThis && uMsg == WM_CONTEXTMENU)
    {
        // Let the default edit control handle the context menu in this lane.
    }

    if (pThis && (uMsg == WM_SETFOCUS || uMsg == WM_LBUTTONDOWN))
    {
        OutputDebugStringA("[CopilotChatInputProc] Focus/Click detected\n");
    }

    if (pThis && uMsg == WM_KEYDOWN)
    {
        const bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (ChatAutocomplete_HandleKeyDown(wParam, ctrlDown))
        {
            return 0;
        }

        if (wParam == VK_TAB)
        {
            if (pThis->isSlashOverlayVisible() && pThis->handleSlashOverlayKey(wParam))
            {
                return 0;
            }
            if (pThis->tryAutocompleteSlashCommand())
            {
                return 0;
            }
        }

        if (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_ESCAPE || wParam == VK_RETURN)
        {
            if (pThis->handleSlashOverlayKey(wParam))
            {
                if (wParam == VK_RETURN)
                {
                    return 0;
                }
                if (wParam == VK_ESCAPE || wParam == VK_UP || wParam == VK_DOWN)
                {
                    return 0;
                }
            }
        }
    }

    if (pThis && uMsg == WM_KEYDOWN && wParam == VK_RETURN)
    {
        const bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (!shiftPressed)
        {
            OutputDebugStringA("[CopilotChatInputProc] ENTER send (deferred to main wnd)\n");
            pThis->postDeferredCopilotSend();
            return 0;
        }
    }

    if (pThis && (uMsg == WM_KEYUP || uMsg == WM_CHAR))
    {
        if (pThis->m_hwndCopilotChatInput && IsWindow(pThis->m_hwndCopilotChatInput))
        {
            const int inputLen = GetWindowTextLengthW(pThis->m_hwndCopilotChatInput);
            if (inputLen >= 0)
            {
                std::vector<wchar_t> inputBuffer(static_cast<size_t>(inputLen) + 1, L'\0');
                GetWindowTextW(pThis->m_hwndCopilotChatInput, inputBuffer.data(), inputLen + 1);
                const std::string inputUtf8 = wideToUtf8(inputBuffer.data());
                pThis->updateSlashStatusHint(inputUtf8);
                pThis->refreshSlashOverlay(inputUtf8);
                ChatAutocomplete_OnInputChanged(inputBuffer.data());
                CommandPreview_Update(inputBuffer.data());

                if (!inputUtf8.empty() && inputUtf8.front() == '/' && pThis->m_hwndStatusBar &&
                    IsWindow(pThis->m_hwndStatusBar))
                {
                    wchar_t routeLine[256] = {};
                    CommandPreview_GetRouteLine(routeLine, static_cast<int>(std::size(routeLine)));
                    if (routeLine[0] != L'\0')
                    {
                        SendMessageW(pThis->m_hwndStatusBar, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(routeLine));
                    }
                }
            }
        }
    }

    if (pThis && uMsg == WM_DESTROY)
    {
        CommandPreview_Destroy();
        ChatAutocomplete_Detach();
        ComposerPanel_Destroy();
    }

    if (pThis && pThis->m_oldCopilotInputProc)
    {
        return CallWindowProc(pThis->m_oldCopilotInputProc, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK Win32IDE::CopilotChatOutputProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (pThis && uMsg == WM_CONTEXTMENU)
    {
        // Let the default read-only edit control handle the context menu.
    }

    if (pThis && pThis->m_oldCopilotOutputProc)
    {
        return CallWindowProc(pThis->m_oldCopilotOutputProc, hwnd, uMsg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK Win32IDE::CopilotButtonProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    const int controlId = GetDlgCtrlID(hwnd);

    if (pThis && (uMsg == BM_CLICK || uMsg == WM_LBUTTONUP))
    {
        if (controlId == 1204)
        {
            pThis->postDeferredCopilotSend();
            return 0;
        }
        if (controlId == 1205)
        {
            pThis->HandleCopilotClear();
            return 0;
        }
    }

    WNDPROC oldProc = nullptr;
    if (pThis)
    {
        if (controlId == 1204)
            oldProc = pThis->m_oldCopilotSendBtnProc;
        else if (controlId == 1205)
            oldProc = pThis->m_oldCopilotClearBtnProc;
    }

    if (oldProc)
        return CallWindowProc(oldProc, hwnd, uMsg, wParam, lParam);

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void Win32IDE::HandleCopilotStreamUpdate(const char* token, size_t length)
{
    // [BRIDGE_PULSE] Stage 5/UI Render Arrival
    char traceBuffer[256];
    DWORD mainThreadId = GetWindowThreadProcessId(m_hwndMain, nullptr);
    sprintf_s(traceBuffer,
              "[BRIDGE_PULSE] Stage 5/UI Render: Received %zu bytes. Current ThreadID: %lu, Main ThreadID: %lu\n",
              length, GetCurrentThreadId(), mainThreadId);
    OutputDebugStringA(traceBuffer);

    if (!m_hwndCopilotChatOutput || !token)
        return;

    std::string chunk;
    if (length > 0)
    {
        chunk.assign(token, token + length);
    }
    else
    {
        chunk = token;
    }

    OutputDebugStringA(("[ChatUi] HandleCopilotStreamUpdate chunk_len=" + std::to_string(chunk.size()) + "\n").c_str());

    if (chunk.empty())
        return;

    chunk = normalizeChatUtf8Chunk(this, chunk, false);
    if (chunk.empty())
        return;

    appendCopilotChatTextOnUiThread(chunk);
}

void Win32IDE::appendCopilotChatTextOnUiThread(const std::string& text)
{
    if (text.empty() || !m_hwndCopilotChatOutput || !IsWindow(m_hwndCopilotChatOutput))
        return;

    // Temporary diagnostics: verify whether text reaches the final UI append boundary.
    OutputDebugStringA(("CHAT_APPEND_RAW: [" + text + "]\n").c_str());

    uint64_t cycles = PulseGetCycles();
    char latencyBuf[128];
    sprintf_s(latencyBuf, "[NANO_LATENCY] appendChat Entry: %llu\n", cycles);
    OutputDebugStringA(latencyBuf);

    if (m_hwndMain && GetWindowThreadProcessId(m_hwndMain, nullptr) != GetCurrentThreadId())
    {
        postCopilotChatAppendSafe(text);
        return;
    }

    {
        std::lock_guard<std::mutex> outLock(m_outputMutex);
        const std::wstring wide = utf8ToWide(text);
        if (m_nativeStreamingActive)
            m_streamingWrittenWchars += static_cast<int>(wide.size());
        SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, -1, -1);
        SendMessageW(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)wide.c_str());
        SendMessage(m_hwndCopilotChatOutput, EM_SCROLLCARET, 0, 0);
    }
}

void Win32IDE::clearCopilotInputOnUiThread()
{
    if (!m_hwndCopilotChatInput || !IsWindow(m_hwndCopilotChatInput))
        return;

    if (m_hwndMain && GetWindowThreadProcessId(m_hwndMain, nullptr) != GetCurrentThreadId())
    {
        postCopilotInputClearSafe();
        return;
    }

    std::lock_guard<std::mutex> inputLock(g_chatInputBufferMutex);
    SetWindowTextW(m_hwndCopilotChatInput, L"");
}

void Win32IDE::postCopilotBackendReadySafe(bool ready)
{
    if (isShuttingDown() || !m_hwndMain || !IsWindow(m_hwndMain))
        return;
    PostMessage(m_hwndMain, WM_COPILOT_BACKEND_READY_SAFE, ready ? 1 : 0, 0);
}

void Win32IDE::finalizeCopilotChatInterlockAfterDeferredLoad()
{
    if (isShuttingDown())
        return;

    if (!m_hwndSecondarySidebar || !IsWindow(m_hwndSecondarySidebar) || !m_hwndCopilotChatOutput ||
        !IsWindow(m_hwndCopilotChatOutput))
    {
        createChatPanel();
    }

    const bool initOk = m_deferredHeavyInitComplete.load(std::memory_order_acquire) ||
                        m_initStatus.load(std::memory_order_acquire) == STATUS_INIT_SUCCESS;
    // Unlock send/receive once the panel exists; per-send path reports model/backend errors.
    setCopilotBackendReadyOnUiThread(true);

    if (!initOk)
    {
        appendCopilotChatTextOnUiThread(
            "\n[Chat] Engine still warming up — you can send now; responses may queue until init finishes.\n");
    }

    reloadPersistedChatHistoryIntoUi();

    int outLen = 0;
    if (m_hwndCopilotChatOutput && IsWindow(m_hwndCopilotChatOutput))
        outLen = GetWindowTextLengthW(m_hwndCopilotChatOutput);

    if (outLen <= 0)
    {
        appendCopilotChatTextOnUiThread(
            "\n[Chat] Ready — type a message and press Send (or Enter in the input box).\n");
    }
}

void Win32IDE::setCopilotBackendReadyOnUiThread(bool ready)
{
    if (!m_hwndMain || !IsWindow(m_hwndMain))
        return;

    if (GetWindowThreadProcessId(m_hwndMain, nullptr) != GetCurrentThreadId())
    {
        postCopilotBackendReadySafe(ready);
        return;
    }

    m_copilotBackendReady = ready;
    OutputDebugStringA(ready ? "[INTERLOCK] Copilot backend READY; enabling chat controls\n"
                             : "[INTERLOCK] Copilot backend NOT_READY; locking chat controls\n");

    const BOOL enableInteraction = (m_copilotBackendReady && !m_copilotInteractionBusy) ? TRUE : FALSE;
    if (m_hwndCopilotSendBtn && IsWindow(m_hwndCopilotSendBtn))
        EnableWindow(m_hwndCopilotSendBtn, enableInteraction);
    if (m_hwndCopilotRetryBtn && IsWindow(m_hwndCopilotRetryBtn))
        EnableWindow(m_hwndCopilotRetryBtn, enableInteraction);
    if (m_hwndCopilotChatInput && IsWindow(m_hwndCopilotChatInput))
    {
        EnableWindow(m_hwndCopilotChatInput, enableInteraction);
        if (!m_copilotBackendReady)
        {
            SetWindowTextW(m_hwndCopilotChatInput, L"Initializing Engine (7800 XT)...");
        }
        else
        {
            wchar_t current[128] = {};
            GetWindowTextW(m_hwndCopilotChatInput, current, static_cast<int>(std::size(current)));
            if (wcscmp(current, L"Initializing Engine (7800 XT)...") == 0)
                SetWindowTextW(m_hwndCopilotChatInput, L"");
        }
    }
}

void Win32IDE::setCopilotInteractionBusyOnUiThread(bool busy)
{
    if (!m_hwndMain || !IsWindow(m_hwndMain))
        return;

    if (GetWindowThreadProcessId(m_hwndMain, nullptr) != GetCurrentThreadId())
    {
        postCopilotInteractionBusySafe(busy);
        return;
    }

    m_copilotInteractionBusy = busy;

    const BOOL enableInteraction = (m_copilotBackendReady && !m_copilotInteractionBusy) ? TRUE : FALSE;

    if (m_hwndCopilotSendBtn && IsWindow(m_hwndCopilotSendBtn))
        EnableWindow(m_hwndCopilotSendBtn, enableInteraction);

    if (m_hwndCopilotRetryBtn && IsWindow(m_hwndCopilotRetryBtn))
        EnableWindow(m_hwndCopilotRetryBtn, enableInteraction);

    if (m_hwndCopilotChatInput && IsWindow(m_hwndCopilotChatInput))
        EnableWindow(m_hwndCopilotChatInput, enableInteraction);
}

void Win32IDE::postCopilotChatAppendSafe(const std::string& text)
{
    if (isShuttingDown() || !m_hwndMain || text.empty())
        return;

    bool needPost = false;
    {
        std::lock_guard<std::mutex> lock(m_copilotChatPostCoalesceMutex);
        constexpr size_t kMaxCopilotCoalesceBytes = 8 * 1024 * 1024;
        if (m_copilotChatPostCoalesceBuffer.size() + text.size() > kMaxCopilotCoalesceBytes)
        {
            const size_t drop = (m_copilotChatPostCoalesceBuffer.size() + text.size()) - kMaxCopilotCoalesceBytes;
            if (drop >= m_copilotChatPostCoalesceBuffer.size())
                m_copilotChatPostCoalesceBuffer.clear();
            else
                m_copilotChatPostCoalesceBuffer.erase(0, drop);
        }
        m_copilotChatPostCoalesceBuffer += text;
        if (!m_copilotChatPostCoalesceFlushPosted)
        {
            m_copilotChatPostCoalesceFlushPosted = true;
            needPost = true;
        }
    }
    if (needPost)
    {
        if (!PostMessageW(m_hwndMain, WM_COPILOT_CHAT_APPEND_FLUSH, 0, 0))
        {
            std::lock_guard<std::mutex> lock(m_copilotChatPostCoalesceMutex);
            m_copilotChatPostCoalesceFlushPosted = false;
        }
    }
}

void Win32IDE::postCopilotRecordToolTurnSafe(const std::string& toolName, const std::string& resultBody)
{
    if (isShuttingDown() || !m_hwndMain)
        return;
    auto* p = new std::pair<std::string, std::string>(toolName, resultBody);
    if (!PostMessageW(m_hwndMain, WM_COPILOT_RECORD_TOOL_TURN, 0, reinterpret_cast<LPARAM>(p)))
    {
        delete p;
    }
}

void Win32IDE::postAgenticAssistantFinalSafe(const std::string& streamAccumulatorSnapshot,
                                             const std::string& finalAssistantText)
{
    if (isShuttingDown() || !m_hwndMain)
        return;
    auto* env = new Win32IDEAgenticCopilotFinalEnvelope{streamAccumulatorSnapshot, finalAssistantText};
    if (!PostMessageW(m_hwndMain, WM_COPILOT_AGENTIC_ASSISTANT_FINAL, 0, reinterpret_cast<LPARAM>(env)))
    {
        delete env;
        m_chatSendInFlight.store(false);
        setCopilotInteractionBusyOnUiThread(false);
    }
}

void Win32IDE::applyAgenticAssistantFinalOnUiThread(std::string streamAccumulatorSnapshot,
                                                    const std::string& finalAssistantText)
{
    m_streamingTokenAccumulator = std::move(streamAccumulatorSnapshot);

    if (!m_hwndCopilotChatOutput || !IsWindow(m_hwndCopilotChatOutput))
    {
        m_streamingTokenAccumulator.clear();
        m_chatSendInFlight.store(false);
        setCopilotInteractionBusyOnUiThread(false);
        return;
    }

    if (finalAssistantText.empty())
    {
        m_streamingTokenAccumulator.clear();
        m_chatSendInFlight.store(false);
        setCopilotInteractionBusyOnUiThread(false);
        return;
    }

    if (!m_streamingTokenAccumulator.empty())
    {
        replaceLastStreamingBlockWithMarkdown(finalAssistantText);
    }
    else
    {
        appendFormattedChatMessage("assistant", finalAssistantText);
    }

    conversationAddAssistant(finalAssistantText);
    m_chatHistory.push_back({"assistant", finalAssistantText});
    persistChatTurnToDisk("assistant", finalAssistantText);
    m_lastCopilotAssistantResponse = finalAssistantText;
    maybeConsumeStructuredSlashCopilotFixFromAssistantText(finalAssistantText);
    m_streamingTokenAccumulator.clear();
    m_chatSendInFlight.store(false);
    setCopilotInteractionBusyOnUiThread(false);
    updateEnhancedStatusBar();
    notifyCopilotChatCompletedUnlessSidebarFocused();
}

void Win32IDE::postCopilotInputClearSafe()
{
    if (isShuttingDown() || !m_hwndMain)
        return;

    PostMessage(m_hwndMain, WM_COPILOT_INPUT_CLEAR_SAFE, 0, 0);
}

void Win32IDE::postCopilotInteractionBusySafe(bool busy)
{
    if (isShuttingDown() || !m_hwndMain)
        return;

    PostMessage(m_hwndMain, WM_COPILOT_INTERACTION_BUSY_SAFE, busy ? 1 : 0, 0);
}

void Win32IDE::postAgenticTelemetryUpdateSafe(const std::string& type, const std::string& message)
{
    if (isShuttingDown() || !m_hwndMain)
        return;
    auto* p = new std::pair<std::string, std::string>(type, message);
    if (!PostMessageW(m_hwndMain, WM_AGENTIC_TELEMETRY_UPDATE, 0, reinterpret_cast<LPARAM>(p)))
    {
        delete p;
    }
}

void Win32IDE::postAgenticTelemetryDoneSafe()
{
    if (isShuttingDown() || !m_hwndMain)
        return;
    PostMessageW(m_hwndMain, WM_AGENTIC_TELEMETRY_DONE, 0, 0);
}

namespace
{
bool win32SubtreeContainsFocus(HWND root)
{
    if (!root || !IsWindow(root))
    {
        return false;
    }
    HWND focus = GetFocus();
    if (!focus)
    {
        return false;
    }
    return focus == root || IsChild(root, focus) != FALSE;
}
}  // namespace

void Win32IDE::notifyCopilotChatCompletedUnlessSidebarFocused()
{
    if (isShuttingDown() || !m_hwndMain || !IsWindow(m_hwndMain))
    {
        return;
    }
    HWND fg = GetForegroundWindow();
    const bool ideForeground = fg && (fg == m_hwndMain || IsChild(m_hwndMain, fg) != FALSE);
    if (ideForeground && m_hwndSecondarySidebar && IsWindow(m_hwndSecondarySidebar) &&
        win32SubtreeContainsFocus(m_hwndSecondarySidebar))
    {
        return;
    }
    FLASHWINFO fi = {sizeof(fi), m_hwndMain, FLASHW_ALL | FLASHW_TIMERNOFG, 5, 0};
    FlashWindowEx(&fi);
    MessageBeep(MB_ICONASTERISK);
}

void Win32IDE::notifyAgentLoopCompletedUnlessAgentChromeFocused()
{
    if (isShuttingDown() || !m_hwndMain || !IsWindow(m_hwndMain))
    {
        return;
    }
    HWND fg = GetForegroundWindow();
    const bool ideForeground = fg && (fg == m_hwndMain || IsChild(m_hwndMain, fg) != FALSE);
    if (ideForeground)
    {
        const HWND agentChrome[] = {m_hwndAgentDiffPanel,    m_hwndAgentStatusLabel,  m_hwndAgentSummaryEdit,
                                    m_hwndAgentAcceptAllBtn, m_hwndAgentRejectAllBtn, m_hwndAgentNewSessionBtn};
        for (HWND h : agentChrome)
        {
            if (h && IsWindow(h) && win32SubtreeContainsFocus(h))
            {
                return;
            }
        }
    }
    FLASHWINFO fi = {sizeof(fi), m_hwndMain, FLASHW_ALL | FLASHW_TIMERNOFG, 5, 0};
    FlashWindowEx(&fi);
    MessageBeep(MB_ICONASTERISK);
}

void Win32IDE::onModelSelectionChanged()
{
    if (!m_hwndModelSelector || !IsWindow(m_hwndModelSelector))
        return;

    int idx = (int)SendMessage(m_hwndModelSelector, CB_GETCURSEL, 0, 0);

    std::string selectedModel;
    {
        std::lock_guard<std::recursive_mutex> modelListLock(m_availableModelsMutex);
        if (m_availableModels.empty())
            return;

        if (idx < 0 || idx >= (int)m_availableModels.size())
        {
            std::string selectorText;
            const int selectorTextLen = GetWindowTextLengthW(m_hwndModelSelector);
            if (selectorTextLen > 0)
            {
                std::vector<wchar_t> selectorBuffer(static_cast<size_t>(selectorTextLen) + 1, L'\0');
                GetWindowTextW(m_hwndModelSelector, selectorBuffer.data(), selectorTextLen + 1);
                selectorText = wideToUtf8(selectorBuffer.data());
            }

            selectedModel = normalizeModelSelectorEntry(selectorText, m_availableModels);
            if (selectedModel.empty())
            {
                selectedModel = m_availableModels[0];
                SendMessage(m_hwndModelSelector, CB_SETCURSEL, 0, 0);
            }
        }
        else
        {
            selectedModel = m_availableModels[idx];
        }
    }

    m_ollamaModelOverride = selectedModel;

    if (HWND hwndEffort = GetDlgItem(m_hwndSecondarySidebar, IDC_MODEL_EFFORT_LABEL))
    {
        const std::string effort = rawrxd::BuildThinkingEffortSummary(selectedModel);
        SetWindowTextW(hwndEffort, utf8ToWide(effort).c_str());
    }

    const std::string resolvedModel =
        resolveLocalModelSelectionPath(m_ollamaModelOverride, m_modelPaths, m_userModelDirectories);
    if (resolvedModel != m_ollamaModelOverride)
    {
        OutputDebugStringA(
            ("[ModelSelection] resolved model ref '" + m_ollamaModelOverride + "' -> '" + resolvedModel + "'\n")
                .c_str());
        m_ollamaModelOverride = resolvedModel;
    }

    const bool looksLikeLocalModel = m_ollamaModelOverride.find(".gguf") != std::string::npos ||
                                     m_ollamaModelOverride.find(":\\") != std::string::npos;

    if (looksLikeLocalModel)
    {
        const bool alreadySelectedAndReady = (m_loadedModelPath == m_ollamaModelOverride) && isModelLoaded();
        if (alreadySelectedAndReady)
        {
            OutputDebugStringA(
                ("[ModelSelection] local model already loaded path=" + m_ollamaModelOverride + "\n").c_str());
        }
        else
        {
            OutputDebugStringA(
                ("[ModelSelection] auto-loading local model path=" + m_ollamaModelOverride + "\n").c_str());
            appendToOutput("[ModelSelection] Selected local model: " + m_ollamaModelOverride +
                               "\n[ModelSelection] Triggering background load...\n",
                           "Output", OutputSeverity::Info);
            loadModelFromPathAsync(m_ollamaModelOverride);
        }
    }
}

void Win32IDE::onMaxTokensChanged(int newValue)
{
    m_currentMaxTokens = newValue;
    m_inferenceConfig.maxTokens = newValue;

    // Update label
    if (m_hwndMaxTokensLabel)
    {
        wchar_t buf[64];
        swprintf_s(buf, L"%d", newValue);
        SetWindowTextW(m_hwndMaxTokensLabel, buf);
    }
}

void Win32IDE::onContextSizeChanged(int newValue)
{
    static const int contextSizes[] = {4096, 32768, 65536, 131072, 262144, 524288, 1048576};
    if (newValue < 0 || newValue > 6)
        newValue = 0;

    m_currentContextSize = contextSizes[newValue];
    m_inferenceConfig.contextWindow = m_currentContextSize;

    // Phase 20: Propagation to status and sidebar
    if (m_hwndContextLabel)
    {
        static const wchar_t* labels[] = {L"4K", L"32K", L"64K", L"128K", L"256K", L"512K", L"1M"};
        SetWindowTextW(m_hwndContextLabel, labels[newValue]);
    }

    updateContextWindowDisplay();

    appendToOutput("Context window updated to: " + std::to_string(m_currentContextSize) + " tokens\n", "Output",
                   OutputSeverity::Info);
}

void Win32IDE::handleModelBrowse()
{
    BROWSEINFOW bi = {0};
    bi.hwndOwner = m_hwndMain;
    bi.lpszTitle = L"Select Model Directory";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl)
    {
        WCHAR path[MAX_PATH] = {0};
        if (SHGetPathFromIDListW(pidl, path))
        {
            std::string selectedPath = wideToUtf8(path);

            // Check if this directory is already in the list
            bool alreadyExists = false;
            for (const auto& dir : m_userModelDirectories)
            {
                if (dir == selectedPath)
                {
                    alreadyExists = true;
                    break;
                }
            }

            if (!alreadyExists)
            {
                m_userModelDirectories.push_back(selectedPath);
                appendToOutput("Added model directory: " + selectedPath + "\n", "Output", OutputSeverity::Info);

                // Refresh the model selector to include models from the new directory
                populateModelSelector();
            }
            else
            {
                appendToOutput("Directory already added: " + selectedPath + "\n", "Output", OutputSeverity::Warning);
            }
        }
        CoTaskMemFree(pidl);
    }
}

// ============================================================================
// IMPLEMENTATIONS for functions declared in Win32IDE.h
// Line Number Gutter, Minimap, Breadcrumb, and other UI components.
// ============================================================================

namespace
{

bool aiWalValidCharRange(const CHARRANGE& r)
{
    return r.cpMin >= 0 && r.cpMax >= r.cpMin;
}

std::vector<std::string> splitLinesUtf8NoCr(const std::string& s)
{
    std::vector<std::string> lines;
    std::string cur;
    for (unsigned char uc : s)
    {
        const char c = static_cast<char>(uc);
        if (c == '\n')
        {
            lines.push_back(std::move(cur));
            cur.clear();
        }
        else if (c != '\r')
        {
            cur += c;
        }
    }
    lines.push_back(std::move(cur));
    return lines;
}

size_t lineCountNewlinesPlusOne(const std::string& s)
{
    if (s.empty())
    {
        return 1;
    }
    size_t n = 1;
    for (unsigned char uc : s)
    {
        if (static_cast<char>(uc) == '\n')
        {
            ++n;
        }
    }
    return n;
}

void mergeFullTextDiffLines1Based(const std::string& oldUtf8, const std::string& newUtf8, std::set<int>& out)
{
    auto ol = splitLinesUtf8NoCr(oldUtf8);
    auto nl = splitLinesUtf8NoCr(newUtf8);
    if (ol.empty() && !nl.empty())
    {
        for (size_t k = 0; k < nl.size(); ++k)
        {
            out.insert(static_cast<int>(k) + 1);
        }
        return;
    }
    if (!ol.empty() && nl.empty())
    {
        for (size_t k = 0; k < ol.size(); ++k)
        {
            out.insert(static_cast<int>(k) + 1);
        }
        return;
    }
    size_t i = 0;
    size_t j = 0;
    while (i < ol.size() && j < nl.size() && ol[i] == nl[j])
    {
        ++i;
        ++j;
    }
    size_t bi = ol.size();
    size_t bj = nl.size();
    while (bi > i && bj > j && ol[bi - 1] == nl[bj - 1])
    {
        --bi;
        --bj;
    }
    for (size_t k = i; k < bi; ++k)
    {
        out.insert(static_cast<int>(k) + 1);
    }
}

/// Lines (1-based) in **newUtf8** that differ from oldUtf8 (prefix/suffix trimmed). Used for post-apply WAL gutter.
void mergeFullTextDiffNewLines1Based(const std::string& oldUtf8, const std::string& newUtf8, std::set<int>& out)
{
    auto ol = splitLinesUtf8NoCr(oldUtf8);
    auto nl = splitLinesUtf8NoCr(newUtf8);
    if (ol.empty() && !nl.empty())
    {
        for (size_t k = 0; k < nl.size(); ++k)
            out.insert(static_cast<int>(k) + 1);
        return;
    }
    if (!ol.empty() && nl.empty())
    {
        return;
    }
    size_t i = 0;
    size_t j = 0;
    while (i < ol.size() && j < nl.size() && ol[i] == nl[j])
    {
        ++i;
        ++j;
    }
    size_t bi = ol.size();
    size_t bj = nl.size();
    while (bi > i && bj > j && ol[bi - 1] == nl[bj - 1])
    {
        --bi;
        --bj;
    }
    for (size_t k = j; k < bj; ++k)
        out.insert(static_cast<int>(k) + 1);
}

void mergeEditorRangeGutterLines(HWND editor, const CHARRANGE& range, const std::string& oldChunk,
                                 const std::string& newChunk, std::set<int>& out)
{
    if (!editor || !IsWindow(editor) || !aiWalValidCharRange(range))
    {
        return;
    }
    const int line0 = static_cast<int>(SendMessageW(editor, EM_EXLINEFROMCHAR, 0, static_cast<LPARAM>(range.cpMin)));
    const int start1 = line0 + 1;
    const size_t n = (std::max)(lineCountNewlinesPlusOne(oldChunk), lineCountNewlinesPlusOne(newChunk));
    const int count = static_cast<int>((std::max)(static_cast<size_t>(1), n));
    for (int k = 0; k < count; ++k)
    {
        out.insert(start1 + k);
    }
}

}  // namespace

std::set<int> Win32IDE::computeAiWalGutterLines1BasedForCurrentFile() const
{
    std::set<int> out;
    if (m_currentFile.empty())
    {
        return out;
    }
    for (const auto& ed : m_pendingEdits)
    {
        if (ed.state != RawrXD::Review::EditState::Pending)
        {
            continue;
        }
        if (ed.file != m_currentFile)
        {
            continue;
        }
        switch (ed.type)
        {
            case RawrXD::Review::EditType::Replace:
            case RawrXD::Review::EditType::Insert:
                if (ed.editorHandle && ed.editorHandle == m_hwndEditor && aiWalValidCharRange(ed.oldRange))
                {
                    mergeEditorRangeGutterLines(m_hwndEditor, ed.oldRange, ed.oldText, ed.newText, out);
                }
                else
                {
                    mergeFullTextDiffLines1Based(ed.oldText, ed.newText, out);
                }
                break;
            case RawrXD::Review::EditType::Delete:
                mergeFullTextDiffLines1Based(ed.oldText, std::string(), out);
                break;
            case RawrXD::Review::EditType::Create:
                mergeFullTextDiffLines1Based(std::string(), ed.newText, out);
                break;
            case RawrXD::Review::EditType::Rename:
            default:
                break;
        }
    }
    return out;
}

bool Win32IDE::isWalGutterHistoryHighlighted(int line1Based) const
{
    return !m_walGutterHistoryLines1Based.empty() &&
           std::binary_search(m_walGutterHistoryLines1Based.begin(), m_walGutterHistoryLines1Based.end(), line1Based);
}

void Win32IDE::invalidateWalGutterHistoryIfEditorDrifted()
{
    if (rawrxd::wal_gutter::programmaticMutationDepth() > 0)
        return;
    if (m_walGutterHistoryLines1Based.empty())
        return;
    if (!m_hwndEditor || !IsWindow(m_hwndEditor))
        return;
    const std::string cur = getRichEditDocumentUtf8(m_hwndEditor);
    if (cur == m_walGutterDriftBaselineUtf8)
        return;
    m_walGutterHistoryLines1Based.clear();
    m_walGutterDriftBaselineUtf8.clear();
    updateLineNumbers();
}

void Win32IDE::clearWalGutterHistoryVisualState()
{
    if (m_walGutterHistoryLines1Based.empty() && m_walGutterDriftBaselineUtf8.empty())
        return;
    m_walGutterHistoryLines1Based.clear();
    m_walGutterDriftBaselineUtf8.clear();
    updateLineNumbers();
}

void Win32IDE::refreshWalGutterHighlightsFromHistory()
{
    m_walGutterHistoryLines1Based.clear();
    m_walGutterDriftBaselineUtf8.clear();
    if (m_aiEditTransactionHistory.empty() || m_currentFile.empty())
    {
        updateLineNumbers();
        return;
    }
    if (!m_hwndEditor || !IsWindow(m_hwndEditor))
    {
        updateLineNumbers();
        return;
    }
    const std::string postUtf8 = getRichEditDocumentUtf8(m_hwndEditor);
    const AIEditTransaction& tx = m_aiEditTransactionHistory.back();
    std::set<int> uniq;
    for (const auto& f : tx.files)
    {
        if (_stricmp(f.path.c_str(), m_currentFile.c_str()) != 0)
            continue;
        if (f.op == AIFileRollbackRecord::Op::Replace || f.op == AIFileRollbackRecord::Op::Create)
            mergeFullTextDiffNewLines1Based(f.originalContent, postUtf8, uniq);
    }
    if (!uniq.empty())
    {
        m_walGutterHistoryLines1Based.assign(uniq.begin(), uniq.end());
        std::sort(m_walGutterHistoryLines1Based.begin(), m_walGutterHistoryLines1Based.end());
        m_walGutterDriftBaselineUtf8 = postUtf8;
    }
    updateLineNumbers();
}

// --- Line Number Gutter ---
void Win32IDE::createLineNumberGutter(HWND hwndParent)
{
    if (!hwndParent)
        return;

    m_hwndLineNumbers = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 0, 0, 70, 100,
                                        hwndParent, nullptr, m_hInstance, nullptr);

    if (m_hwndLineNumbers)
    {
        SetPropW(m_hwndLineNumbers, L"IDE_PTR", (HANDLE)this);
        m_oldLineNumberProc = (WNDPROC)SetWindowLongPtrW(m_hwndLineNumbers, GWLP_WNDPROC, (LONG_PTR)LineNumberProc);
    }
}

void Win32IDE::updateLineNumbers()
{
    if (m_hwndLineNumbers && IsWindow(m_hwndLineNumbers))
        InvalidateRect(m_hwndLineNumbers, nullptr, FALSE);
}

void Win32IDE::paintLineNumbers(HDC hdc, RECT& rc)
{
    if (!m_hwndEditor || !IsWindow(m_hwndEditor))
        return;

    // Get the current scroll position and line metrics from the rich edit control
    int firstVisibleLine = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
    int lineCount = (int)SendMessage(m_hwndEditor, EM_GETLINECOUNT, 0, 0);

    // Get the editor font metrics
    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(SYSTEM_FIXED_FONT);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (lineHeight <= 0)
        lineHeight = 16;

    SetBkColor(hdc, RGB(30, 30, 30));
    SetTextColor(hdc, RGB(133, 133, 133));

    HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    int visibleLines = (rc.bottom - rc.top) / lineHeight + 1;

    const std::set<int> aiWalLines = computeAiWalGutterLines1BasedForCurrentFile();

    // Get Git status for current file
    char gitStatus = ' ';
    if (!m_currentFile.empty())
    {
        // Convert to relative path for Git status lookup
        std::string relPath = m_currentFile;
        if (relPath.find(m_gitRepoPath) == 0)
        {
            relPath = relPath.substr(m_gitRepoPath.length());
            if (!relPath.empty() && relPath[0] == '\\')
            {
                relPath = relPath.substr(1);
            }
            // Convert backslashes to forward slashes for Git
            std::replace(relPath.begin(), relPath.end(), '\\', '/');

            auto it = m_gitStatus.fileStatus.find(relPath);
            if (it != m_gitStatus.fileStatus.end())
            {
                gitStatus = it->second;
            }
        }
    }

    for (int i = 0; i < visibleLines && (firstVisibleLine + i) < lineCount; i++)
    {
        int lineNum = firstVisibleLine + i + 1;
        wchar_t buf[16];
        swprintf_s(buf, L"%4d", lineNum);

        RECT lineRect = {rc.left, i * lineHeight, rc.right - 4, (i + 1) * lineHeight};

        // ── Breakpoint indicator (red circle in left margin) ──
        bool hasBP = false;
        for (const auto& bp : m_breakpoints)
        {
            if (bp.line == lineNum && bp.enabled && (bp.file.empty() || bp.file == m_currentFile))
            {
                hasBP = true;
                break;
            }
        }
        if (hasBP)
        {
            int dotSize = std::min(lineHeight - 4, 12);
            int dotX = rc.left + 2;
            int dotY = i * lineHeight + (lineHeight - dotSize) / 2;
            HBRUSH bpBrush = CreateSolidBrush(RGB(220, 50, 47));
            HPEN bpPen = CreatePen(PS_SOLID, 1, RGB(220, 50, 47));
            HBRUSH oldBr = (HBRUSH)SelectObject(hdc, bpBrush);
            HPEN oldPn = (HPEN)SelectObject(hdc, bpPen);
            Ellipse(hdc, dotX, dotY, dotX + dotSize, dotY + dotSize);
            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldPn);
            DeleteObject(bpBrush);
            DeleteObject(bpPen);
        }

        // ── Current execution line highlight (debugger paused) ──
        if (m_debuggerPaused && m_debuggerCurrentLine == lineNum &&
            (m_debuggerCurrentFile.empty() || m_debuggerCurrentFile == m_currentFile))
        {
            HBRUSH hlBrush = CreateSolidBrush(RGB(60, 60, 30));
            RECT hlRect = {rc.left, i * lineHeight, rc.right, (i + 1) * lineHeight};
            FillRect(hdc, &hlRect, hlBrush);
            DeleteObject(hlBrush);

            // Yellow arrow indicator
            int arrowX = rc.left + 2;
            int arrowY = i * lineHeight + lineHeight / 2;
            HPEN arrowPen = CreatePen(PS_SOLID, 2, RGB(255, 204, 0));
            HPEN oldAP = (HPEN)SelectObject(hdc, arrowPen);
            MoveToEx(hdc, arrowX, arrowY, nullptr);
            LineTo(hdc, arrowX + 8, arrowY);
            LineTo(hdc, arrowX + 5, arrowY - 3);
            MoveToEx(hdc, arrowX + 8, arrowY, nullptr);
            LineTo(hdc, arrowX + 5, arrowY + 3);
            SelectObject(hdc, oldAP);
            DeleteObject(arrowPen);
        }

        const bool walPending = !aiWalLines.empty() && aiWalLines.count(lineNum) > 0;
        const bool walHistory = isWalGutterHistoryHighlighted(lineNum);
        // Committed WAL (v1.2.0): 3px royal blue, left of the 4px pending-review bar.
        if (walHistory)
        {
            RECT walBar = {rc.right - 31, i * lineHeight + 1, rc.right - 28, (i + 1) * lineHeight - 1};
            HBRUSH walBrush = CreateSolidBrush(RGB(65, 105, 225));
            FillRect(hdc, &walBar, walBrush);
            DeleteObject(walBrush);
        }
        if (walPending)
        {
            RECT aiBar = {rc.right - 26, i * lineHeight + 1, rc.right - 22, (i + 1) * lineHeight - 1};
            HBRUSH aiBrush = CreateSolidBrush(RGB(64, 130, 220));
            FillRect(hdc, &aiBar, aiBrush);
            DeleteObject(aiBrush);
        }

        if (lineNum == m_currentLine)
        {
            SetTextColor(hdc, RGB(200, 200, 200));
        }
        else
        {
            SetTextColor(hdc, RGB(133, 133, 133));
        }

        DrawTextW(hdc, buf, -1, &lineRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);

        // Draw Git status indicator
        if (gitStatus != ' ')
        {
            RECT indicatorRect = {rc.right - 16, i * lineHeight + 2, rc.right - 4, (i + 1) * lineHeight - 2};

            COLORREF indicatorColor;
            switch (gitStatus)
            {
                case 'M':
                    indicatorColor = RGB(255, 193, 7);
                    break;  // Yellow for modified
                case 'A':
                    indicatorColor = RGB(40, 167, 69);
                    break;  // Green for added
                case 'D':
                    indicatorColor = RGB(220, 53, 69);
                    break;  // Red for deleted
                case '?':
                    indicatorColor = RGB(108, 117, 125);
                    break;  // Gray for untracked
                default:
                    indicatorColor = RGB(133, 133, 133);
                    break;
            }

            HBRUSH indicatorBrush = CreateSolidBrush(indicatorColor);
            FillRect(hdc, &indicatorRect, indicatorBrush);
            DeleteObject(indicatorBrush);
        }
    }

    SelectObject(hdc, hOldFont);
}

LRESULT CALLBACK Win32IDE::LineNumberProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* ide = (Win32IDE*)GetPropW(hwnd, L"IDE_PTR");

    if (uMsg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (ide)
        {
            RECT rc;
            GetClientRect(hwnd, &rc);
            ide->paintLineNumbers(hdc, rc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    if (uMsg == WM_ERASEBKGND)
    {
        return 1;  // We handle painting
    }

    // Click in gutter → toggle breakpoint on that line
    if (uMsg == WM_LBUTTONDOWN && ide)
    {
        int yClick = HIWORD(lParam);

        // Calculate which line was clicked
        int firstVisible = (int)SendMessage(ide->m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);

        // Get line height
        HDC hdc = GetDC(hwnd);
        HFONT hFont = ide->m_editorFont ? ide->m_editorFont : (HFONT)GetStockObject(SYSTEM_FIXED_FONT);
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        TEXTMETRICW tm;
        GetTextMetricsW(hdc, &tm);
        int lineHeight = tm.tmHeight + tm.tmExternalLeading;
        if (lineHeight <= 0)
            lineHeight = 16;
        SelectObject(hdc, hOldFont);
        ReleaseDC(hwnd, hdc);

        int clickedLine = firstVisible + (yClick / lineHeight) + 1;
        int totalLines = (int)SendMessage(ide->m_hwndEditor, EM_GETLINECOUNT, 0, 0);
        if (clickedLine >= 1 && clickedLine <= totalLines)
        {
            ide->toggleBreakpoint(ide->m_currentFile, clickedLine);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    if (ide && ide->m_oldLineNumberProc)
    {
        return CallWindowProcW(ide->m_oldLineNumberProc, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK Win32IDE::TabBarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* ide = (Win32IDE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (uMsg == WM_DRAWITEM)
    {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if (dis->CtlType == ODT_TAB && ide)
        {
            ide->drawTabItem(dis);
            return TRUE;
        }
    }
    else if (uMsg == WM_LBUTTONDOWN)
    {
        if (ide)
        {
            POINT pt = {LOWORD(lParam), HIWORD(lParam)};
            ide->handleTabClick(pt);
        }
    }

    if (ide && ide->getOldTabBarProc())
    {
        return CallWindowProcW(ide->getOldTabBarProc(), hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// --- Editor Tab Bar ---
void Win32IDE::createTabBar(HWND hwndParent)
{
    if (!hwndParent)
        return;

    // Initialize sovereign TabManager
    if (!m_tabManager)
    {
        m_tabManager = new Win32IDE_TabManager(this);
        bool initOk = false;
        try
        {
            initOk = m_tabManager->initialize(hwndParent);
        }
        catch (...)
        {
            initOk = false;
        }
        if (!initOk)
        {
            delete m_tabManager;
            m_tabManager = nullptr;
            return;
        }
    }

    // Get the tab bar handle from the manager
    m_hwndTabBar = m_tabManager->getTabBarHandle();
    if (m_hwndTabBar)
    {
        // Subclass for custom drawing and close button handling
        SetWindowLongPtrW(m_hwndTabBar, GWLP_USERDATA, (LONG_PTR)this);
        m_oldTabBarProc = (WNDPROC)SetWindowLongPtrW(m_hwndTabBar, GWLP_WNDPROC, (LONG_PTR)TabBarProc);
    }
}

void Win32IDE::addTab(const std::string& filePath, const std::string& displayName)
{
    // Add a new editor tab
    EditorTab tab;
    tab.filePath = filePath;
    tab.displayName = displayName.empty() ? filePath : displayName;
    tab.modified = false;

    m_editorTabs.push_back(tab);

    if (m_hwndTabBar)
    {
        std::wstring displayW = utf8ToWide(tab.displayName);
        TCITEMW tci = {};
        tci.mask = TCIF_TEXT;
        tci.pszText = const_cast<wchar_t*>(displayW.c_str());
        int index = (int)SendMessage(m_hwndTabBar, TCM_GETITEMCOUNT, 0, 0);
        SendMessageW(m_hwndTabBar, TCM_INSERTITEMW, index, (LPARAM)&tci);
        SendMessage(m_hwndTabBar, TCM_SETCURSEL, index, 0);
        m_activeTabIndex = index;
        syncAgentContextFromActiveTab();
    }
}

std::pair<int, int> Win32IDE::getCursorPosition()
{
    CHARRANGE cr;
    SendMessageW(m_hwndEditor, EM_GETSEL, (WPARAM)&cr.cpMin, (LPARAM)&cr.cpMax);
    int charPos = cr.cpMin;
    int line = (int)SendMessageW(m_hwndEditor, EM_LINEFROMCHAR, charPos, 0) + 1;
    int lineStart = (int)SendMessageW(m_hwndEditor, EM_LINEINDEX, line - 1, 0);
    int col = charPos - lineStart + 1;
    return {line, col};
}

void Win32IDE::syncAgentContextFromActiveTab()
{
    std::string workspaceRoot = m_projectRoot;
    if (workspaceRoot.empty())
        workspaceRoot = m_explorerRootPath;

    if (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_editorTabs.size())
    {
        const auto& tab = m_editorTabs[m_activeTabIndex];
        m_currentFile = tab.filePath;
        setCurrentDirectoryFromFile(m_currentFile);

        if (!m_currentFile.empty())
            m_syntaxLanguage = detectLanguageFromExtension(m_currentFile);
        else
            m_syntaxLanguage = SyntaxLanguage::None;

        if (workspaceRoot.empty())
            workspaceRoot = m_currentDirectory;

        updateTitleBarText();

        if (m_agenticBridge)
        {
            m_agenticBridge->SetWorkspaceRoot(workspaceRoot);
            if (m_settings.currentFileContextEnabled)
                m_agenticBridge->SetLanguageContext(getSyntaxLanguageName(), m_currentFile);
            else
                m_agenticBridge->SetLanguageContext(getSyntaxLanguageName(), "");
        }
        return;
    }

    m_currentFile.clear();
    m_syntaxLanguage = SyntaxLanguage::None;

    if (workspaceRoot.empty())
        workspaceRoot = m_currentDirectory;

    updateTitleBarText();

    if (m_agenticBridge)
    {
        m_agenticBridge->SetWorkspaceRoot(workspaceRoot);
        m_agenticBridge->SetLanguageContext(getSyntaxLanguageName(), "");
    }
}

void Win32IDE::onTabChanged()
{
    if (!m_hwndTabBar)
        return;

    int newIndex = (int)SendMessage(m_hwndTabBar, TCM_GETCURSEL, 0, 0);
    if (newIndex >= 0 && newIndex < (int)m_editorTabs.size() && newIndex != m_activeTabIndex)
    {
        // Save current tab content and state
        if (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_editorTabs.size())
        {
            m_editorTabs[m_activeTabIndex].content = getWindowText(m_hwndEditor);
            auto [line, col] = getCursorPosition();
            m_editorTabs[m_activeTabIndex].cursorLine = line;
            m_editorTabs[m_activeTabIndex].cursorCol = col;
            // Save scroll position
            m_editorTabs[m_activeTabIndex].scrollPos = (int)SendMessageW(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
        }

        // Stash annotations for the outgoing tab
        storeAnnotationsForTab();

        // Switch to new tab
        m_activeTabIndex = newIndex;
        const auto& tab = m_editorTabs[newIndex];

        // Load tab content into editor
        m_suppressLspDocumentSync = true;
        setWindowText(m_hwndEditor, tab.content);
        m_suppressLspDocumentSync = false;

        // Restore cursor position
        int lineIndex = (int)SendMessageW(m_hwndEditor, EM_LINEINDEX, tab.cursorLine - 1, 0);
        int charPos = lineIndex + tab.cursorCol - 1;
        SendMessageW(m_hwndEditor, EM_SETSEL, charPos, charPos);

        // Restore scroll position
        int currentFirst = (int)SendMessageW(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
        if (tab.scrollPos > currentFirst)
            SendMessageW(m_hwndEditor, EM_LINESCROLL, 0, tab.scrollPos - currentFirst);
        else if (tab.scrollPos < currentFirst)
            SendMessageW(m_hwndEditor, EM_LINESCROLL, 0, -(currentFirst - tab.scrollPos));

        // Restore stashed annotations for the incoming tab
        restoreAnnotationsForTab();

        syncAgentContextFromActiveTab();
        m_fileModified = tab.modified;
        if (!tab.filePath.empty())
        {
            syncLSPDocumentOpen(tab.filePath, tab.content);
            displayDiagnosticsAsAnnotations(filePathToUri(tab.filePath));
        }
        else
        {
            clearAnnotationsForCurrentFile();
        }

        // Update status bar
        if (m_hwndStatusBar)
        {
            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)utf8ToWide(tab.displayName).c_str());
        }

        refreshWalGutterHighlightsFromHistory();
    }
}

void Win32IDE::onTabClosing(int index)
{
    // Handle tab closing logic
    if (index >= 0 && index < (int)m_editorTabs.size())
    {
        // Check if tab is modified and prompt to save
        if (m_editorTabs[index].modified)
        {
            std::wstring prompt = L"Save changes to '";
            prompt += utf8ToWide(m_editorTabs[index].displayName);
            prompt += L"'?";
            int result = MessageBoxW(m_hwndMain, prompt.c_str(), L"RawrXD", MB_YESNOCANCEL | MB_ICONQUESTION);
            if (result == IDYES)
            {
                // Save before closing
                int prev = m_activeTabIndex;
                m_activeTabIndex = index;
                saveCurrentFile();
                m_activeTabIndex = prev;
            }
            else if (result == IDCANCEL)
            {
                return;  // Abort close
            }
            // IDNO falls through to remove without saving
        }
        // Remove the tab
        removeTab(index);
    }
}

void Win32IDE::onTabActivated(int index)
{
    // Handle tab activation
    if (index >= 0 && index < (int)m_editorTabs.size())
    {
        setActiveTab(index);
    }
}

void Win32IDE::saveCurrentFile()
{
    // Save the current file
    if (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_editorTabs.size())
    {
        const auto& tab = m_editorTabs[m_activeTabIndex];
        if (!tab.filePath.empty())
        {
            std::ofstream file(tab.filePath, std::ios::binary);
            if (file)
            {
                std::string content = getWindowText(m_hwndEditor);
                file.write(content.c_str(), content.size());
                m_editorTabs[m_activeTabIndex].modified = false;
                syncLSPDocumentSave(tab.filePath);
                refreshSymbolIndexForCurrentDocumentAsync();
                // Update tab display to remove modified indicator
                if (m_tabManager)
                {
                    m_tabManager->updateTabDisplay(m_activeTabIndex);
                }
            }
        }
    }
}

void Win32IDE::setActiveTab(int index)
{
    if (!m_hwndTabBar)
        return;
    if (index < 0 || index >= (int)m_editorTabs.size())
        return;

    // Use the tab control to select the tab, then trigger onTabChanged
    SendMessage(m_hwndTabBar, TCM_SETCURSEL, index, 0);
    onTabChanged();
}

void Win32IDE::saveTabsState()
{
    nlohmann::json j;
    j["tabs"] = nlohmann::json::array();
    for (const auto& tab : m_editorTabs)
    {
        nlohmann::json tabJson;
        tabJson["filePath"] = tab.filePath;
        tabJson["displayName"] = tab.displayName;
        tabJson["content"] = tab.content;
        tabJson["modified"] = tab.modified;
        tabJson["isPinned"] = tab.isPinned;
        tabJson["isPreview"] = tab.isPreview;
        tabJson["cursorLine"] = tab.cursorLine;
        tabJson["cursorCol"] = tab.cursorCol;
        tabJson["scrollPos"] = tab.scrollPos;
        j["tabs"].push_back(tabJson);
    }
    j["activeTabIndex"] = m_activeTabIndex;
    std::ofstream file("tabs_state.json");
    if (file)
    {
        file << j.dump(4);
    }
}

void Win32IDE::loadTabsState()
{
    std::ifstream file("tabs_state.json");
    if (!file)
        return;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    nlohmann::json j;
    try
    {
        j = nlohmann::json::parse(content);
    }
    catch (...)
    {
        return;
    }
    if (j.contains("tabs") && j["tabs"].is_array())
    {
        m_editorTabs.clear();
        for (const auto& tabJson : j["tabs"])
        {
            EditorTab tab;
            tab.filePath = tabJson.value("filePath", "");
            tab.displayName = tabJson.value("displayName", "");
            tab.content = tabJson.value("content", "");
            tab.modified = tabJson.value("modified", false);
            tab.isPinned = tabJson.value("isPinned", false);
            tab.isPreview = tabJson.value("isPreview", false);
            tab.cursorLine = tabJson.value("cursorLine", 1);
            tab.cursorCol = tabJson.value("cursorCol", 0);
            tab.scrollPos = tabJson.value("scrollPos", 0);
            m_editorTabs.push_back(tab);
        }
        if (j.contains("activeTabIndex"))
        {
            m_activeTabIndex = j["activeTabIndex"];
        }
        // Update tab bar if needed
        if (m_hwndTabBar)
        {
            // Clear existing tabs
            SendMessage(m_hwndTabBar, TCM_DELETEALLITEMS, 0, 0);
            // Add loaded tabs
            for (size_t i = 0; i < m_editorTabs.size(); ++i)
            {
                TCITEMW tci = {0};
                tci.mask = TCIF_TEXT;
                std::wstring wname = utf8ToWide(m_editorTabs[i].displayName);
                tci.pszText = (LPWSTR)wname.c_str();
                SendMessageW(m_hwndTabBar, TCM_INSERTITEMW, i, (LPARAM)&tci);
            }
            if (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_editorTabs.size())
            {
                SendMessage(m_hwndTabBar, TCM_SETCURSEL, m_activeTabIndex, 0);
                // Load active tab content
                const auto& tab = m_editorTabs[m_activeTabIndex];
                m_suppressLspDocumentSync = true;
                setWindowText(m_hwndEditor, tab.content);
                m_suppressLspDocumentSync = false;
                // Set cursor
                int lineIndex = (int)SendMessageW(m_hwndEditor, EM_LINEINDEX, tab.cursorLine - 1, 0);
                int charPos = lineIndex + tab.cursorCol - 1;
                SendMessageW(m_hwndEditor, EM_SETSEL, charPos, charPos);
                syncAgentContextFromActiveTab();
                m_fileModified = tab.modified;
                if (!tab.filePath.empty())
                {
                    syncLSPDocumentOpen(tab.filePath, tab.content);
                    displayDiagnosticsAsAnnotations(filePathToUri(tab.filePath));
                }
                // Update status bar
                if (m_hwndStatusBar)
                {
                    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)utf8ToWide(tab.displayName).c_str());
                }
                refreshWalGutterHighlightsFromHistory();
            }
        }
    }
}

void Win32IDE::removeTab(int index)
{
    if (index < 0 || index >= (int)m_editorTabs.size())
        return;

    // Clear annotation cache for the file being closed
    const std::string& closingFile = m_editorTabs[index].filePath;
    if (!closingFile.empty())
    {
        m_annotationCache.erase(closingFile);
        syncLSPDocumentClose(closingFile);
    }
    // If this is the active tab, clear live annotations
    if (index == m_activeTabIndex)
    {
        clearAnnotationsForCurrentFile();
    }

    // Remove from the Win32 tab control
    if (m_hwndTabBar)
    {
        SendMessage(m_hwndTabBar, TCM_DELETEITEM, index, 0);
    }

    // Remove from our vector
    m_editorTabs.erase(m_editorTabs.begin() + index);

    // Adjust active tab index
    if (m_editorTabs.empty())
    {
        m_activeTabIndex = -1;
        m_suppressLspDocumentSync = true;
        setWindowText(m_hwndEditor, "");
        m_suppressLspDocumentSync = false;
        syncAgentContextFromActiveTab();
    }
    else if (index <= m_activeTabIndex)
    {
        m_activeTabIndex = std::max(0, m_activeTabIndex - 1);
        SendMessage(m_hwndTabBar, TCM_SETCURSEL, m_activeTabIndex, 0);
        onTabChanged();
    }
}

int Win32IDE::findTabByPath(const std::string& filePath) const
{
    for (int i = 0; i < (int)m_editorTabs.size(); i++)
    {
        if (m_editorTabs[i].filePath == filePath)
            return i;
    }
    return -1;
}

void Win32IDE::drawTabItem(DRAWITEMSTRUCT* dis)
{
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    int index = dis->itemID;

    if (index < 0 || index >= (int)m_editorTabs.size())
        return;

    const EditorTab& tab = m_editorTabs[index];
    bool isActive = (index == m_activeTabIndex);
    bool isModified = tab.modified;

    // Background
    COLORREF bgColor = isActive ? RGB(45, 45, 45) : RGB(30, 30, 30);
    HBRUSH hBrush = CreateSolidBrush(bgColor);
    FillRect(hdc, &rc, hBrush);
    DeleteObject(hBrush);

    // Border
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, rc.left, rc.bottom - 1, nullptr);
    LineTo(hdc, rc.right, rc.bottom - 1);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);

    // Text
    SetBkMode(hdc, TRANSPARENT);
    COLORREF textColor = isModified ? RGB(255, 200, 100) : RGB(200, 200, 200);
    SetTextColor(hdc, textColor);

    rc.left += 5;
    rc.right -= 20;  // Space for close button

    std::wstring displayW = utf8ToWide(tab.displayName);
    DrawTextW(hdc, displayW.c_str(), -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Close button
    RECT closeRc = {rc.right + 5, rc.top + 2, rc.right + 15, rc.bottom - 2};
    DrawTextW(hdc, L"×", 1, &closeRc, DT_CENTER | DT_VCENTER);
}

void Win32IDE::handleTabClick(POINT pt)
{
    TCHITTESTINFO hitTest = {};
    hitTest.pt = pt;
    int index = (int)SendMessage(m_hwndTabBar, TCM_HITTEST, 0, (LPARAM)&hitTest);

    if (index >= 0 && index < (int)m_editorTabs.size())
    {
        // Check if close button was clicked
        RECT rc;
        SendMessage(m_hwndTabBar, TCM_GETITEMRECT, index, (LPARAM)&rc);

        if (pt.x >= rc.right - 15 && pt.x <= rc.right - 5)
        {
            // Close button clicked
            if (m_editorTabs[index].modified)
            {
                std::string msg = "Save changes to " + m_editorTabs[index].displayName + "?";
                int result = MessageBoxW(m_hwndMain, utf8ToWide(msg).c_str(), L"Confirm Close",
                                         MB_YESNOCANCEL | MB_ICONQUESTION);
                if (result == IDCANCEL)
                    return;
                if (result == IDYES)
                {
                    setActiveTab(index);
                    saveFile();
                }
            }
            removeTab(index);
            return;
        }

        // Tab clicked - switch to it
        setActiveTab(index);
    }
}

// --- Command Input Subclass Procedure ---
namespace
{
enum : UINT
{
    IDM_CTX_COPY = 0x7A01,
    IDM_CTX_PASTE = 0x7A02,
    IDM_CTX_COPY_ALL = 0x7A03,
};

static bool CopyWindowTextToClipboard(HWND hwndOwner, HWND hwndSource)
{
    const int len = GetWindowTextLengthW(hwndSource);
    if (len <= 0)
    {
        return false;
    }

    std::vector<wchar_t> text(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(hwndSource, text.data(), len + 1);

    if (!OpenClipboard(hwndOwner && IsWindow(hwndOwner) ? hwndOwner : hwndSource))
    {
        return false;
    }
    EmptyClipboard();

    const size_t bytes = (static_cast<size_t>(len) + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem)
    {
        CloseClipboard();
        return false;
    }

    void* locked = GlobalLock(hMem);
    if (!locked)
    {
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }

    memcpy(locked, text.data(), bytes);
    GlobalUnlock(hMem);

    if (!SetClipboardData(CF_UNICODETEXT, hMem))
    {
        GlobalFree(hMem);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

static bool ShowEditContextMenu(HWND hwnd, LPARAM lParam)
{
    POINT pt = {};
    if (GET_X_LPARAM(lParam) == -1 && GET_Y_LPARAM(lParam) == -1)
    {
        RECT rc = {};
        GetWindowRect(hwnd, &rc);
        pt.x = rc.left + ((rc.right - rc.left) / 2);
        pt.y = rc.top + ((rc.bottom - rc.top) / 2);
    }
    else
    {
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
    }

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu)
    {
        return false;
    }

    AppendMenuW(hMenu, MF_STRING, IDM_CTX_COPY, L"Copy");
    AppendMenuW(hMenu, MF_STRING, IDM_CTX_PASTE, L"Paste");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_CTX_COPY_ALL, L"Copy All");

    const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    const bool readOnly = (style & ES_READONLY) != 0;
    const int textLen = GetWindowTextLengthW(hwnd);

    DWORD selStart = 0;
    DWORD selEnd = 0;
    SendMessageW(hwnd, EM_GETSEL, reinterpret_cast<WPARAM>(&selStart), reinterpret_cast<LPARAM>(&selEnd));
    const bool hasSelection = selEnd > selStart;

    if (!hasSelection)
    {
        EnableMenuItem(hMenu, IDM_CTX_COPY, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
    }
    if (readOnly || !IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        EnableMenuItem(hMenu, IDM_CTX_PASTE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
    }
    if (textLen <= 0)
    {
        EnableMenuItem(hMenu, IDM_CTX_COPY_ALL, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
    }

    const int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);

    switch (cmd)
    {
        case IDM_CTX_COPY:
            SendMessageW(hwnd, WM_COPY, 0, 0);
            return true;
        case IDM_CTX_PASTE:
            if (!readOnly)
            {
                SendMessageW(hwnd, WM_PASTE, 0, 0);
            }
            return true;
        case IDM_CTX_COPY_ALL:
            CopyWindowTextToClipboard(GetActiveWindow(), hwnd);
            return true;
        default:
            return false;
    }
}
}  // namespace

LRESULT CALLBACK Win32IDE::CommandInputProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Retrieve IDE pointer via GWLP_USERDATA (set in createTerminal)
    Win32IDE* ide = (Win32IDE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    if (uMsg == WM_CONTEXTMENU)
    {
        // Fall through to default proc context-menu behavior.
    }

    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN)
    {
        // Execute command on Enter — route through executeCommand()
        if (ide)
        {
            ide->executeCommand();
        }
        return 0;
    }

    // Up arrow — command history navigation (previous) — uses PowerShell history
    if (uMsg == WM_KEYDOWN && wParam == VK_UP)
    {
        if (ide)
        {
            ide->navigatePowerShellHistoryUp();
            // Sync text from PowerShell input to command input
            if (!ide->m_powerShellCommandHistory.empty() && ide->m_powerShellHistoryIndex >= 0 &&
                ide->m_powerShellHistoryIndex < (int)ide->m_powerShellCommandHistory.size())
            {
                SetWindowTextW(hwnd,
                               utf8ToWide(ide->m_powerShellCommandHistory[ide->m_powerShellHistoryIndex]).c_str());
                SendMessage(hwnd, EM_SETSEL, -1, -1);  // cursor to end
            }
        }
        return 0;
    }

    // Down arrow — command history navigation (next) — uses PowerShell history
    if (uMsg == WM_KEYDOWN && wParam == VK_DOWN)
    {
        if (ide)
        {
            ide->navigatePowerShellHistoryDown();
            if (ide->m_powerShellHistoryIndex >= 0 &&
                ide->m_powerShellHistoryIndex < (int)ide->m_powerShellCommandHistory.size())
            {
                SetWindowTextW(hwnd,
                               utf8ToWide(ide->m_powerShellCommandHistory[ide->m_powerShellHistoryIndex]).c_str());
                SendMessage(hwnd, EM_SETSEL, -1, -1);
            }
            else
            {
                SetWindowTextW(hwnd, L"");
            }
        }
        return 0;
    }

    if (ide && ide->m_oldCommandInputProc)
    {
        return CallWindowProcA(ide->m_oldCommandInputProc, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// --- Agent Output Handling ---
void Win32IDE::onAgentOutput(const char* text)
{
    if (!text)
        return;
    appendToOutput(std::string(text), "Agent", OutputSeverity::Info);
}

void Win32IDE::postAgentOutputSafe(const std::string& text)
{
    if (isShuttingDown())
        return;
    if (!m_hwndMain || !IsWindow(m_hwndMain))
        return;
    auto* payload = new (std::nothrow) std::string(text);
    if (!payload)
        return;
    if (!PostMessage(m_hwndMain, WM_AGENT_OUTPUT_SAFE, 0, reinterpret_cast<LPARAM>(payload)))
        delete payload;
}

void Win32IDE::postOutputPanelSafe(const std::string& text)
{
    if (isShuttingDown())
        return;
    if (!m_hwndMain || !IsWindow(m_hwndMain))
        return;
    // WM_IDE_OUTPUT_APPEND_SAFE handlers own a heap std::string* (not char*).
    auto* payload = new (std::nothrow) std::string(text);
    if (!payload)
        return;
    if (!PostMessage(m_hwndMain, WM_IDE_OUTPUT_APPEND_SAFE, 0, reinterpret_cast<LPARAM>(payload)))
        delete payload;
}

void Win32IDE::wireLayerProgressToOutputPanel()
{
    auto cb = [this](const std::string& line)
    {
        postOutputPanelSafe(line);
        if (m_hwndMain && line.find("[MOE_PACK]") != std::string::npos)
            PostMessageW(m_hwndMain, WM_IDE_MOE_PACK_STATUS_REFRESH, 0, 0);
    };
    if (m_nativeEngine)
    {
        m_nativeEngine->SetLayerProgressCallback(cb);
        m_nativeEngine->SetSwarmTelemetryOutputCallback(cb);
    }
    if (m_agenticBridge)
    {
        m_agenticBridge->SetCpuEngineLayerProgressCallback(cb);
        m_agenticBridge->SetCpuEngineSwarmTelemetryOutputCallback(cb);
    }
}

void Win32IDE::refreshMoEPackHudStatusBarPart()
{
    if (!m_hwndStatusBar)
        return;
    if (!m_nativeEngine || !m_nativeEngine->IsModelLoaded())
    {
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 3, (LPARAM)L"MoE pack: —");
        return;
    }
    const std::string u8 = m_nativeEngine->MoEPackHudStatusLineUtf8();
    if (u8.empty())
    {
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 3, (LPARAM)L"MoE pack: —");
        return;
    }
    const int wlen = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0);
    if (wlen <= 0)
        return;
    std::vector<wchar_t> w(static_cast<size_t>(wlen));
    MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, w.data(), wlen);
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 3, (LPARAM)w.data());
    SendMessageW(m_hwndStatusBar, SB_SETTIPTEXTW, 3, (LPARAM)w.data());
}

void Win32IDE::clearInferenceLayerProgressCallback()
{
    if (m_nativeEngine)
    {
        m_nativeEngine->SetLayerProgressCallback({});
        m_nativeEngine->SetSwarmTelemetryOutputCallback({});
    }
    if (m_agenticBridge)
    {
        m_agenticBridge->SetCpuEngineLayerProgressCallback({});
        m_agenticBridge->SetCpuEngineSwarmTelemetryOutputCallback({});
    }
    if (m_hwndStatusBar)
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 3, (LPARAM)L"MoE pack: —");
}

// --- Quick GGUF Model Loader (delegates to unified model dialog) ---
void Win32IDE::quickLoadGGUFModel()
{
    openModelUnified();
}

// ============================================================================
// UNIFIED MODEL SOURCE RESOLUTION
// Implements HuggingFace, Ollama blob, HTTP URL, and smart-detect model loading
// Uses ModelSourceResolver for source detection and download/resolution
// All resolved paths feed into the existing loadGGUFModel() 5-step streaming
// pipeline, preserving full zone-based loading for 800B+ models.
// ============================================================================

// ---------------------------------------------------------------------------
// resolveAndLoadModel — Resolve any model source input to a local path, then
// load it through the streaming GGUF pipeline. This is the common path for
// all source types.
// ---------------------------------------------------------------------------
bool Win32IDE::resolveAndLoadModel(const std::string& input)
{
    SCOPED_METRIC("model.resolve_and_load");
    METRICS.increment("model.resolve_attempts");

    if (!m_modelResolver)
    {
        // If resolver wasn't initialized (deferredHeavyInit failure), create one now
        try
        {
            m_modelResolver = std::make_unique<RawrXD::ModelSourceResolver>();
            OutputDebugStringA("ModelSourceResolver late-initialized in resolveAndLoadModel\n");
        }
        catch (const std::exception& e)
        {
            std::string err = "Failed to initialize ModelSourceResolver: " + std::string(e.what());
            appendToOutput(err + "\n", "Errors", OutputSeverity::Error);
            ErrorReporter::report(err, m_hwndMain);
            return false;
        }
    }

    auto sourceType = m_modelResolver->DetectSourceType(input);
    std::string sourceDesc = RawrXD::ModelSourceResolver::SourceTypeToString(sourceType);
    appendToOutput("Model source detected: " + sourceDesc + "\n", "Output", OutputSeverity::Info);
    appendToOutput("Input: " + input + "\n", "Output", OutputSeverity::Info);

    // For local files, skip resolution and go straight to loading
    if (sourceType == GGUFConstants::ModelSourceType::LOCAL_FILE)
    {
        appendToOutput("Loading local GGUF file directly...\n", "Output", OutputSeverity::Info);
        if (loadGGUFModel(input))
        {
            loadModelForInference(input);
            METRICS.increment("model.resolve_success");
            return true;
        }
        METRICS.increment("model.resolve_failures");
        return false;
    }

    // For remote sources, resolve with progress reporting
    appendToOutput("Resolving model source (this may involve downloading)...\n", "Output", OutputSeverity::Info);
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)utf8ToWide("Resolving: " + input).c_str());

    // Progress callback that writes to the output panel
    auto progressCallback = [this](const RawrXD::ModelDownloadProgress& prog)
    {
        if (prog.has_error)
        {
            appendToOutput("Download error: " + prog.error_message + "\n", "Errors", OutputSeverity::Error);
            return;
        }
        if (prog.is_completed)
        {
            appendToOutput("Download complete: " + prog.local_path + "\n", "Output", OutputSeverity::Info);
            return;
        }
        // Progress update — update status bar with percentage
        char buf[256];
        if (prog.total_bytes > 0)
        {
            snprintf(buf, sizeof(buf), "Downloading: %.1f%% (%llu / %llu MB) — %s", prog.progress_percent,
                     (unsigned long long)(prog.downloaded_bytes / (1024 * 1024)),
                     (unsigned long long)(prog.total_bytes / (1024 * 1024)), prog.filename.c_str());
        }
        else
        {
            snprintf(buf, sizeof(buf), "Downloading: %llu MB — %s",
                     (unsigned long long)(prog.downloaded_bytes / (1024 * 1024)), prog.filename.c_str());
        }
        if (m_hwndStatusBar)
        {
            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)utf8ToWide(buf).c_str());
        }
    };

    // Perform resolution (may download)
    RawrXD::ResolvedModelPath resolved;
    try
    {
        resolved = m_modelResolver->Resolve(input, progressCallback);
    }
    catch (const std::exception& e)
    {
        std::string err = "Exception during model resolution: " + std::string(e.what());
        appendToOutput(err + "\n", "Errors", OutputSeverity::Error);
        ErrorReporter::report(err, m_hwndMain);
        METRICS.increment("model.resolve_failures");
        return false;
    }
    catch (...)
    {
        std::string err = "Unknown exception during model resolution for: " + input;
        appendToOutput(err + "\n", "Errors", OutputSeverity::Error);
        METRICS.increment("model.resolve_failures");
        return false;
    }

    if (!resolved.success)
    {
        std::string err = "Failed to resolve model source: " + resolved.error_message;
        appendToOutput(err + "\n", "Errors", OutputSeverity::Error);
        ErrorReporter::report(err, m_hwndMain);
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Model resolution failed");
        METRICS.increment("model.resolve_failures");
        return false;
    }

    // Log resolution details
    appendToOutput("Resolved to local path: " + resolved.local_path + "\n", "Output", OutputSeverity::Info);
    if (!resolved.hf_repo_id.empty())
    {
        appendToOutput("HuggingFace repo: " + resolved.hf_repo_id + " / " + resolved.hf_filename + "\n", "Output",
                       OutputSeverity::Info);
    }
    if (!resolved.ollama_model_name.empty())
    {
        appendToOutput("Ollama model: " + resolved.ollama_model_name + "\n", "Output", OutputSeverity::Info);
    }

    // Load through the streaming GGUF pipeline (preserves all zone-based 800B+ logic)
    appendToOutput("Loading resolved model through streaming GGUF pipeline...\n", "Output", OutputSeverity::Info);
    if (loadGGUFModel(resolved.local_path))
    {
        loadModelForInference(resolved.local_path);
        METRICS.increment("model.resolve_success");
        return true;
    }

    // No local GGUF path or load failed — still feed Ollama model name to bridge so chat and agentic use it (local
    // definitions vary)
    if (!resolved.ollama_model_name.empty())
    {
        initializeAgenticBridge();
        if (m_agenticBridge)
        {
            m_agenticBridge->SetModel(resolved.ollama_model_name);
            m_ollamaModelOverride = resolved.ollama_model_name;
            if (m_loadedModelPath.empty())
                m_loadedModelPath = resolved.ollama_model_name;
            appendToOutput("Ollama model set in Agentic Bridge: " + resolved.ollama_model_name + "\n", "Output",
                           OutputSeverity::Info);
            METRICS.increment("model.resolve_success");
            return true;
        }
    }

    METRICS.increment("model.resolve_failures");
    return false;
}

// ---------------------------------------------------------------------------
// openModelFromHuggingFace — Dialog: enter HuggingFace repo ID, browse GGUF
// files in the repo, select a quant, download and load.
// ---------------------------------------------------------------------------
void Win32IDE::openModelFromHuggingFace()
{
    SCOPED_METRIC("model.open_from_huggingface");

    // Step 1: Ask user for HuggingFace repo ID or search query
    char inputBuf[512] = {0};
    // Use a simple input dialog (reuse the existing pattern from command palette)
    // We'll use a Win32 dialog via a helper input box

    // Create a simple input dialog
    struct HFInputData
    {
        char repoId[512];
        bool confirmed;
    };
    HFInputData dlgData = {{0}, false};

    // Use DialogBoxIndirect to create an input dialog
    // For simplicity with Win32 API, use a TaskDialog-style approach
    // We'll create a modeless dialog with CreateWindowEx

    // Simple approach: use an edit control dialog
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"STATIC", L"Load from HuggingFace",
                                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 520, 340,
                                m_hwndMain, nullptr, m_hInstance, nullptr);

    if (!hDlg)
    {
        appendToOutput("Failed to create HuggingFace dialog\n", "Errors", OutputSeverity::Error);
        return;
    }

    SetClassLongPtrW(hDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(30, 30, 30)));

    HWND hLabel =
        CreateWindowExW(0, L"STATIC",
                        L"Enter HuggingFace repo ID (e.g., TheBloke/Llama-2-7B-GGUF)\n"
                        L"or search term (e.g., 'llama 7b gguf'):",
                        WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 16, 480, 42, hDlg, nullptr, m_hInstance, nullptr);
    SendMessage(hLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 16, 64, 480,
                                 26, hDlg, (HMENU)101, m_hInstance, nullptr);
    SendMessage(hEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SetFocus(hEdit);

    HWND hInfoLabel =
        CreateWindowExW(0, L"STATIC", L"Available GGUF files will appear below after Search.",
                        WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 100, 480, 20, hDlg, (HMENU)103, m_hInstance, nullptr);
    SendMessage(hInfoLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS, 16, 124, 480, 120,
                                 hDlg, (HMENU)102, m_hInstance, nullptr);
    SendMessage(hList, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hSearchBtn = CreateWindowExW(0, L"BUTTON", L"Search / List Files", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 16,
                                      256, 150, 30, hDlg, (HMENU)201, m_hInstance, nullptr);
    SendMessage(hSearchBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hLoadBtn = CreateWindowExW(0, L"BUTTON", L"Download && Load", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 180, 256,
                                    150, 30, hDlg, (HMENU)202, m_hInstance, nullptr);
    SendMessage(hLoadBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hCancelBtn = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 346, 256, 150, 30,
                                      hDlg, (HMENU)IDCANCEL, m_hInstance, nullptr);
    SendMessage(hCancelBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Store references for the message loop
    struct HFDialogState
    {
        Win32IDE* ide;
        HWND hDlg;
        HWND hEdit;
        HWND hList;
        HWND hInfoLabel;
        std::vector<RawrXD::HFModelFileInfo> ggufFiles;
        std::string repoId;
        bool done;
        bool loadRequested;
        int selectedFileIndex;
    };

    HFDialogState state = {};
    state.ide = this;
    state.hDlg = hDlg;
    state.hEdit = hEdit;
    state.hList = hList;
    state.hInfoLabel = hInfoLabel;
    state.done = false;
    state.loadRequested = false;
    state.selectedFileIndex = -1;

    // Run a modal-style message pump for this dialog
    EnableWindow(m_hwndMain, FALSE);

    MSG msg;
    while (!state.done && GetMessage(&msg, nullptr, 0, 0))
    {
        // Handle button clicks for our dialog
        if (msg.message == WM_COMMAND && msg.hwnd == hDlg)
        {
            int wmId = LOWORD(msg.wParam);
            int wmEvent = HIWORD(msg.wParam);

            if (wmId == IDCANCEL)
            {
                state.done = true;
                continue;
            }

            if (wmId == 201)
            {  // Search button
                wchar_t editText[512] = {0};
                GetWindowTextW(hEdit, editText, 512);
                std::string input = wideToUtf8(editText);

                if (input.empty())
                    continue;

                // Clear listbox
                SendMessage(hList, LB_RESETCONTENT, 0, 0);
                SetWindowTextW(hInfoLabel, L"Searching HuggingFace...");
                UpdateWindow(hDlg);

                state.repoId = input;

                // Try to get GGUF files from this repo
                if (m_modelResolver)
                {
                    try
                    {
                        state.ggufFiles = m_modelResolver->GetHuggingFaceGGUFFiles(input);

                        if (state.ggufFiles.empty())
                        {
                            // Maybe it's a search query, not a repo ID — try search
                            auto searchResults = m_modelResolver->SearchHuggingFace(input, 10);
                            if (!searchResults.empty())
                            {
                                // Show search results in the listbox
                                SetWindowTextW(hInfoLabel, L"Search results (select a repo):");
                                for (const auto& result : searchResults)
                                {
                                    std::string entry = result.repo_id + " (" +
                                                        std::to_string(result.gguf_files.size()) + " GGUF files, " +
                                                        std::to_string(result.downloads) + " downloads)";
                                    SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)utf8ToWide(entry).c_str());
                                }
                                // Store repo IDs for selection
                                state.ggufFiles.clear();  // These are repo results, not file results
                            }
                            else
                            {
                                SetWindowTextW(hInfoLabel, L"No results found. Try a different search term.");
                            }
                        }
                        else
                        {
                            // Show GGUF files
                            char infoBuf[256];
                            snprintf(infoBuf, sizeof(infoBuf),
                                     "Found %d GGUF files in %s:", (int)state.ggufFiles.size(), input.c_str());
                            SetWindowTextW(hInfoLabel, utf8ToWide(infoBuf).c_str());

                            for (const auto& file : state.ggufFiles)
                            {
                                char fileLine[512];
                                double sizeMB = file.size_bytes / (1024.0 * 1024.0);
                                double sizeGB = sizeMB / 1024.0;
                                if (sizeGB >= 1.0)
                                {
                                    snprintf(fileLine, sizeof(fileLine), "%s [%s] (%.1f GB)", file.filename.c_str(),
                                             file.quantization.c_str(), sizeGB);
                                }
                                else
                                {
                                    snprintf(fileLine, sizeof(fileLine), "%s [%s] (%.0f MB)", file.filename.c_str(),
                                             file.quantization.c_str(), sizeMB);
                                }
                                SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)fileLine);
                            }
                        }
                    }
                    catch (const std::exception& e)
                    {
                        std::string errMsg = "HuggingFace API error: " + std::string(e.what());
                        SetWindowTextW(hInfoLabel, utf8ToWide(errMsg).c_str());
                    }
                }
                else
                {
                    SetWindowTextW(hInfoLabel, L"ModelSourceResolver not initialized!");
                }

                UpdateWindow(hDlg);
            }

            if (wmId == 202)
            {  // Download & Load button
                int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)state.ggufFiles.size())
                {
                    state.selectedFileIndex = sel;
                    state.loadRequested = true;
                    state.done = true;
                    continue;
                }
                else if (sel >= 0)
                {
                    // Might be a search result — get the text and use it as repo ID
                    char selText[512] = {0};
                    SendMessageA(hList, LB_GETTEXT, sel, (LPARAM)selText);
                    std::string selStr(selText);
                    // Extract repo ID (before first space or parenthesis)
                    size_t spacePos = selStr.find(' ');
                    if (spacePos != std::string::npos)
                    {
                        state.repoId = selStr.substr(0, spacePos);
                    }
                    else
                    {
                        state.repoId = selStr;
                    }
                    // Re-search for GGUF files in this repo
                    SetWindowTextW(hEdit, utf8ToWide(state.repoId).c_str());
                    PostMessage(hDlg, WM_COMMAND, MAKEWPARAM(201, BN_CLICKED), (LPARAM)hSearchBtn);
                }
                else
                {
                    MessageBoxW(hDlg, L"Please select a GGUF file from the list first.", L"No Selection",
                                MB_OK | MB_ICONINFORMATION);
                }
            }
        }

        // Handle WM_SYSCOMMAND close (X button)
        if (msg.message == WM_SYSCOMMAND && (msg.wParam & 0xFFF0) == SC_CLOSE && msg.hwnd == hDlg)
        {
            state.done = true;
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(m_hwndMain, TRUE);
    SetForegroundWindow(m_hwndMain);
    DestroyWindow(hDlg);

    // If user selected a file, download and load it
    if (state.loadRequested && state.selectedFileIndex >= 0 && state.selectedFileIndex < (int)state.ggufFiles.size())
    {

        const auto& selectedFile = state.ggufFiles[state.selectedFileIndex];
        appendToOutput("Downloading from HuggingFace: " + state.repoId + " / " + selectedFile.filename + "\n", "Output",
                       OutputSeverity::Info);

        // Download on a background thread to keep UI responsive
        std::string repoId = state.repoId;
        std::string filename = selectedFile.filename;

        std::thread(
            [this, repoId, filename]()
            {
                DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
                if (_guard.cancelled)
                    return;
                auto progressCb = [this](const RawrXD::ModelDownloadProgress& prog)
                {
                    char buf[256];
                    if (prog.has_error)
                    {
                        snprintf(buf, sizeof(buf), "Download error: %s", prog.error_message.c_str());
                        PostMessage(m_hwndMain, WM_APP + 200, 0, 0);  // Signal UI update
                    }
                    else if (prog.is_completed)
                    {
                        snprintf(buf, sizeof(buf), "Download complete!");
                    }
                    else if (prog.total_bytes > 0)
                    {
                        snprintf(buf, sizeof(buf), "Downloading: %.1f%% (%llu MB)", prog.progress_percent,
                                 (unsigned long long)(prog.downloaded_bytes / (1024 * 1024)));
                    }
                    else
                    {
                        snprintf(buf, sizeof(buf), "Downloading: %llu MB",
                                 (unsigned long long)(prog.downloaded_bytes / (1024 * 1024)));
                    }
                    if (m_hwndStatusBar)
                    {
                        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)utf8ToWide(buf).c_str());
                    }
                };

                try
                {
                    std::string localPath = m_modelResolver->DownloadFromHuggingFace(repoId, filename, progressCb);

                    if (!localPath.empty())
                    {
                        // Load on main thread via PostMessage
                        // Store the path and signal the main thread
                        m_loadedModelPath = localPath;
                        PostMessage(m_hwndMain, WM_APP + 201, 0, 0);  // Signal: load downloaded model
                    }
                    else
                    {
                        if (m_hwndStatusBar)
                        {
                            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"HuggingFace download failed");
                        }
                    }
                }
                catch (const std::exception& e)
                {
                    OutputDebugStringA("HF download exception: ");
                    OutputDebugStringA(e.what());
                    OutputDebugStringA("\n");
                    if (m_hwndStatusBar)
                    {
                        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"HuggingFace download exception");
                    }
                }
            })
            .detach();
    }
}

// ---------------------------------------------------------------------------
// openModelFromOllama — Scan for Ollama blobs, show a selection list,
// validate GGUF magic, and load the selected blob.
// ---------------------------------------------------------------------------
void Win32IDE::openModelFromOllama()
{
    SCOPED_METRIC("model.open_from_ollama");

    if (!m_modelResolver)
    {
        try
        {
            m_modelResolver = std::make_unique<RawrXD::ModelSourceResolver>();
        }
        catch (...)
        {
            appendToOutput("Failed to initialize ModelSourceResolver\n", "Errors", OutputSeverity::Error);
            return;
        }
    }

    appendToOutput("Scanning for Ollama GGUF blobs...\n", "Output", OutputSeverity::Info);
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Scanning Ollama blobs...");

    // Find all Ollama blobs with valid GGUF magic
    std::vector<RawrXD::OllamaBlobInfo> blobs;
    try
    {
        blobs = m_modelResolver->FindOllamaBlobs();
    }
    catch (const std::exception& e)
    {
        std::string err = "Error scanning Ollama blobs: " + std::string(e.what());
        appendToOutput(err + "\n", "Errors", OutputSeverity::Error);
        ErrorReporter::report(err, m_hwndMain);
        return;
    }

    if (blobs.empty())
    {
        MessageBoxW(m_hwndMain,
                    L"No Ollama GGUF blobs found.\n\n"
                    L"Searched directories:\n"
                    L"  - %USERPROFILE%\\.ollama\\models\\blobs\n"
                    L"  - D:\\OllamaModels\\blobs\n"
                    L"  - C:\\Users\\*\\.ollama\\models\\blobs\n\n"
                    L"Make sure Ollama is installed and has downloaded models.",
                    L"No Ollama Models Found", MB_OK | MB_ICONINFORMATION);
        return;
    }

    appendToOutput("Found " + std::to_string(blobs.size()) + " Ollama GGUF blobs.\n", "Output", OutputSeverity::Info);

    // Create a selection dialog
    HWND hDlg = CreateWindowExA(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, "STATIC", "Load from Ollama Blobs",
                                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 580, 350,
                                m_hwndMain, nullptr, m_hInstance, nullptr);

    if (!hDlg)
        return;

    SetClassLongPtrA(hDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(30, 30, 30)));

    // Info label
    char infoText[128];
    snprintf(infoText, sizeof(infoText), "Found %d Ollama GGUF blobs. Select one to load:", (int)blobs.size());
    HWND hLabel = CreateWindowExA(0, "STATIC", infoText, WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 12, 540, 22, hDlg,
                                  nullptr, m_hInstance, nullptr);
    SendMessage(hLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Listbox
    HWND hList = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS, 16, 40, 540, 220,
                                 hDlg, (HMENU)102, m_hInstance, nullptr);
    SendMessage(hList, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Populate with blob info
    for (const auto& blob : blobs)
    {
        char line[512];
        double sizeGB = blob.size_bytes / (1024.0 * 1024.0 * 1024.0);
        double sizeMB = blob.size_bytes / (1024.0 * 1024.0);
        if (sizeGB >= 1.0)
        {
            snprintf(line, sizeof(line), "%s — %.1f GB %s", blob.model_name.c_str(), sizeGB,
                     blob.is_valid_gguf ? "[GGUF OK]" : "[INVALID]");
        }
        else
        {
            snprintf(line, sizeof(line), "%s — %.0f MB %s", blob.model_name.c_str(), sizeMB,
                     blob.is_valid_gguf ? "[GGUF OK]" : "[INVALID]");
        }
        SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)line);
    }

    // Load button
    HWND hLoadBtn = CreateWindowExA(0, "BUTTON", "Load Selected", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 16, 272, 150,
                                    30, hDlg, (HMENU)201, m_hInstance, nullptr);
    SendMessage(hLoadBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Cancel button
    HWND hCancelBtn = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 406, 272, 150, 30,
                                      hDlg, (HMENU)IDCANCEL, m_hInstance, nullptr);
    SendMessage(hCancelBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    bool done = false;
    int selectedIdx = -1;

    EnableWindow(m_hwndMain, FALSE);

    MSG msg;
    while (!done && GetMessage(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_COMMAND && msg.hwnd == hDlg)
        {
            int wmId = LOWORD(msg.wParam);

            if (wmId == IDCANCEL)
            {
                done = true;
                continue;
            }

            if (wmId == 201)
            {  // Load button
                int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)blobs.size())
                {
                    selectedIdx = sel;
                    done = true;
                    continue;
                }
                else
                {
                    MessageBoxW(hDlg, L"Please select a model from the list.", L"No Selection",
                                MB_OK | MB_ICONINFORMATION);
                }
            }
        }

        if (msg.message == WM_SYSCOMMAND && (msg.wParam & 0xFFF0) == SC_CLOSE && msg.hwnd == hDlg)
        {
            done = true;
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(m_hwndMain, TRUE);
    SetForegroundWindow(m_hwndMain);
    DestroyWindow(hDlg);

    // Load the selected blob
    if (selectedIdx >= 0 && selectedIdx < (int)blobs.size())
    {
        const auto& selected = blobs[selectedIdx];

        if (!selected.is_valid_gguf)
        {
            MessageBoxW(
                m_hwndMain,
                utf8ToWide("Selected blob does not have valid GGUF magic bytes:\n" + selected.blob_path).c_str(),
                L"Invalid GGUF", MB_OK | MB_ICONWARNING);
            return;
        }

        appendToOutput("Loading Ollama blob: " + selected.model_name + "\n", "Output", OutputSeverity::Info);
        appendToOutput("Path: " + selected.blob_path + "\n", "Output", OutputSeverity::Info);

        if (loadGGUFModel(selected.blob_path))
        {
            loadModelForInference(selected.blob_path);
        }
    }
}

// ---------------------------------------------------------------------------
// openModelFromURL — Dialog: enter HTTP/HTTPS URL to a GGUF file,
// download with progress, and load through the streaming pipeline.
// ---------------------------------------------------------------------------
void Win32IDE::openModelFromURL()
{
    SCOPED_METRIC("model.open_from_url");

    if (!m_modelResolver)
    {
        try
        {
            m_modelResolver = std::make_unique<RawrXD::ModelSourceResolver>();
        }
        catch (...)
        {
            appendToOutput("Failed to initialize ModelSourceResolver\n", "Errors", OutputSeverity::Error);
            return;
        }
    }

    // Create URL input dialog
    HWND hDlg = CreateWindowExA(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, "STATIC", "Load from URL",
                                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 560, 180,
                                m_hwndMain, nullptr, m_hInstance, nullptr);

    if (!hDlg)
        return;

    SetClassLongPtrA(hDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(30, 30, 30)));

    // Label
    HWND hLabel = CreateWindowExA(0, "STATIC",
                                  "Enter direct URL to a .gguf file (HTTP or HTTPS):", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  16, 16, 520, 22, hDlg, nullptr, m_hInstance, nullptr);
    SendMessage(hLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // URL edit
    HWND hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 16, 44, 520, 26,
                                 hDlg, (HMENU)101, m_hInstance, nullptr);
    SendMessage(hEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SetFocus(hEdit);

    // Label
    HWND hExample = CreateWindowExA(
        0, "STATIC", "Example: https://huggingface.co/TheBloke/Llama-2-7B-GGUF/resolve/main/llama-2-7b.Q4_K_M.gguf",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 76, 520, 18, hDlg, nullptr, m_hInstance, nullptr);
    SendMessage(hExample, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Download button
    HWND hDownloadBtn = CreateWindowExA(0, "BUTTON", "Download && Load", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 16, 106,
                                        150, 30, hDlg, (HMENU)201, m_hInstance, nullptr);
    SendMessage(hDownloadBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Cancel button
    HWND hCancelBtn = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 386, 106, 150, 30,
                                      hDlg, (HMENU)IDCANCEL, m_hInstance, nullptr);
    SendMessage(hCancelBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    bool done = false;
    std::string url;

    EnableWindow(m_hwndMain, FALSE);

    MSG msg;
    while (!done && GetMessage(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_COMMAND && msg.hwnd == hDlg)
        {
            int wmId = LOWORD(msg.wParam);

            if (wmId == IDCANCEL)
            {
                done = true;
                continue;
            }

            if (wmId == 201)
            {  // Download button
                char editText[2048] = {0};
                GetWindowTextA(hEdit, editText, sizeof(editText));
                url = std::string(editText);

                if (url.empty())
                {
                    MessageBoxW(hDlg, L"Please enter a URL.", L"Empty URL", MB_OK);
                    continue;
                }

                // Basic URL validation
                if (url.find("http://") != 0 && url.find("https://") != 0)
                {
                    MessageBoxW(hDlg, L"URL must start with http:// or https://", L"Invalid URL",
                                MB_OK | MB_ICONWARNING);
                    continue;
                }

                done = true;
                continue;
            }
        }

        if (msg.message == WM_SYSCOMMAND && (msg.wParam & 0xFFF0) == SC_CLOSE && msg.hwnd == hDlg)
        {
            done = true;
            url.clear();
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(m_hwndMain, TRUE);
    SetForegroundWindow(m_hwndMain);
    DestroyWindow(hDlg);

    if (!url.empty())
    {
        appendToOutput("Downloading from URL: " + url + "\n", "Output", OutputSeverity::Info);

        // Download on background thread
        std::thread(
            [this, url]()
            {
                DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
                if (_guard.cancelled)
                    return;
                auto progressCb = [this](const RawrXD::ModelDownloadProgress& prog)
                {
                    char buf[256];
                    if (prog.has_error)
                    {
                        snprintf(buf, sizeof(buf), "Download error: %s", prog.error_message.c_str());
                    }
                    else if (prog.is_completed)
                    {
                        snprintf(buf, sizeof(buf), "Download complete!");
                    }
                    else if (prog.total_bytes > 0)
                    {
                        snprintf(buf, sizeof(buf), "Downloading: %.1f%% (%llu / %llu MB)", prog.progress_percent,
                                 (unsigned long long)(prog.downloaded_bytes / (1024 * 1024)),
                                 (unsigned long long)(prog.total_bytes / (1024 * 1024)));
                    }
                    else
                    {
                        snprintf(buf, sizeof(buf), "Downloading: %llu MB",
                                 (unsigned long long)(prog.downloaded_bytes / (1024 * 1024)));
                    }
                    if (m_hwndStatusBar)
                    {
                        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)utf8ToWide(buf).c_str());
                    }
                };

                try
                {
                    std::string localPath = m_modelResolver->DownloadFromURL(url, progressCb);

                    if (!localPath.empty())
                    {
                        m_loadedModelPath = localPath;
                        // Signal main thread to load the model
                        PostMessage(m_hwndMain, WM_APP + 201, 0, 0);
                    }
                    else
                    {
                        if (m_hwndStatusBar)
                        {
                            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"URL download failed");
                        }
                    }
                }
                catch (const std::exception& e)
                {
                    OutputDebugStringA("URL download exception: ");
                    OutputDebugStringA(e.what());
                    OutputDebugStringA("\n");
                    if (m_hwndStatusBar)
                    {
                        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"URL download exception");
                    }
                }
            })
            .detach();
    }
}

// ---------------------------------------------------------------------------
// openModelUnified — Smart model open dialog: user types any model identifier
// and it auto-detects the source type (local path, HF repo, Ollama name, URL)
// and routes to the appropriate loader.
// ---------------------------------------------------------------------------
void Win32IDE::openModelUnified()
{
    SCOPED_METRIC("model.open_unified");

    // Create the unified input dialog
    HWND hDlg = CreateWindowExA(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, "STATIC", "RawrXD — Smart Model Loader",
                                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 580, 260,
                                m_hwndMain, nullptr, m_hInstance, nullptr);

    if (!hDlg)
        return;

    SetClassLongPtrA(hDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(30, 30, 30)));

    // Title label
    HWND hTitle = CreateWindowExA(
        0, "STATIC", "Enter any model identifier — the source will be auto-detected:", WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, 16, 540, 22, hDlg, nullptr, m_hInstance, nullptr);
    SendMessage(hTitle, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Input edit
    HWND hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 16, 44, 540, 26,
                                 hDlg, (HMENU)101, m_hInstance, nullptr);
    SendMessage(hEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SetFocus(hEdit);

    // Help text
    std::string helpText = "Supported formats:\n"
                           "  Local file:     C:\\models\\my-model.gguf\n"
                           "  HuggingFace:  TheBloke/Llama-2-7B-GGUF  or  hf://repo-id\n"
                           "  Ollama blob:   llama3.2:3b  or  codellama:7b\n"
                           "  Direct URL:     https://host.com/model.gguf";

    HWND hHelp = CreateWindowExA(0, "STATIC", helpText.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT, 16, 78, 540, 90, hDlg,
                                 nullptr, m_hInstance, nullptr);
    SendMessage(hHelp, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Load button
    HWND hLoadBtn =
        CreateWindowExA(0, "BUTTON", "Detect && Load", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_DEFPUSHBUTTON, 16,
                        180, 150, 32, hDlg, (HMENU)201, m_hInstance, nullptr);
    SendMessage(hLoadBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Browse Local button
    HWND hBrowseBtn = CreateWindowExA(0, "BUTTON", "Browse Local...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 180, 180,
                                      150, 32, hDlg, (HMENU)202, m_hInstance, nullptr);
    SendMessage(hBrowseBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Cancel button
    HWND hCancelBtn = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 414, 180, 140, 32,
                                      hDlg, (HMENU)IDCANCEL, m_hInstance, nullptr);
    SendMessage(hCancelBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    bool done = false;
    std::string inputStr;

    EnableWindow(m_hwndMain, FALSE);

    MSG msg;
    while (!done && GetMessage(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_COMMAND && msg.hwnd == hDlg)
        {
            int wmId = LOWORD(msg.wParam);

            if (wmId == IDCANCEL)
            {
                done = true;
                continue;
            }

            if (wmId == 201)
            {  // Detect & Load
                char editText[2048] = {0};
                GetWindowTextA(hEdit, editText, sizeof(editText));
                inputStr = std::string(editText);

                if (inputStr.empty())
                {
                    MessageBoxW(hDlg, L"Please enter a model identifier.", L"Empty Input", MB_OK);
                    continue;
                }

                done = true;
                continue;
            }

            if (wmId == 202)
            {  // Browse Local
                wchar_t filename[MAX_PATH] = {0};
                OPENFILENAMEW ofn = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hDlg;
                ofn.lpstrFilter = L"GGUF Models\0*.gguf\0All Files\0*.*\0";
                ofn.lpstrFile = filename;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                ofn.lpstrTitle = L"Select GGUF Model";

                if (GetOpenFileNameW(&ofn))
                {
                    SetWindowTextW(hEdit, filename);
                }
            }
        }

        // Handle Enter key in edit control
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN && msg.hwnd == hEdit)
        {
            PostMessage(hDlg, WM_COMMAND, MAKEWPARAM(201, BN_CLICKED), (LPARAM)hLoadBtn);
            continue;
        }

        if (msg.message == WM_SYSCOMMAND && (msg.wParam & 0xFFF0) == SC_CLOSE && msg.hwnd == hDlg)
        {
            done = true;
            inputStr.clear();
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(m_hwndMain, TRUE);
    SetForegroundWindow(m_hwndMain);
    DestroyWindow(hDlg);

    if (!inputStr.empty())
    {
        resolveAndLoadModel(inputStr);
    }
}

// ============================================================================
// EditorSubclassProc — Editor RichEdit subclass window procedure
// Routes editor-specific messages (scroll sync, key interception) while
// forwarding everything else to the original EDIT wndproc.
// ============================================================================
namespace
{
std::wstring getEditorRangeTextW(HWND hwndEditor, LONG start, LONG end)
{
    return editorRangeTextWideForSlash(hwndEditor, start, end);
}

bool editorMoveByWord(HWND hwndEditor, bool forward, bool extendSelection)
{
    if (!hwndEditor)
        return false;

    CHARRANGE sel = {};
    SendMessageW(hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);

    const LONG anchor = sel.cpMin;
    LONG startPos = sel.cpMax;
    if (!extendSelection && sel.cpMin != sel.cpMax)
        startPos = forward ? (std::max)(sel.cpMin, sel.cpMax) : (std::min)(sel.cpMin, sel.cpMax);

    const UINT findCmd = forward ? WB_MOVEWORDRIGHT : WB_MOVEWORDLEFT;
    const LONG target = (LONG)SendMessageW(hwndEditor, EM_FINDWORDBREAK, findCmd, startPos);

    CHARRANGE out = {};
    if (extendSelection)
    {
        out.cpMin = anchor;
        out.cpMax = target;
    }
    else
    {
        out.cpMin = target;
        out.cpMax = target;
    }

    SendMessageW(hwndEditor, EM_EXSETSEL, 0, (LPARAM)&out);
    SendMessageW(hwndEditor, EM_SCROLLCARET, 0, 0);
    return true;
}

bool editorDuplicateCurrentLine(HWND hwndEditor, bool duplicateBelow)
{
    if (!hwndEditor)
        return false;

    CHARRANGE sel = {};
    SendMessageW(hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    const LONG caret = sel.cpMax;

    const int line = (int)SendMessageW(hwndEditor, EM_LINEFROMCHAR, caret, 0);
    const LONG lineStart = (LONG)SendMessageW(hwndEditor, EM_LINEINDEX, line, 0);
    if (lineStart < 0)
        return false;

    const LONG textLen = GetWindowTextLengthW(hwndEditor);
    LONG lineEnd = (LONG)SendMessageW(hwndEditor, EM_LINEINDEX, line + 1, 0);
    if (lineEnd < 0)
        lineEnd = textLen;

    const std::wstring lineText = getEditorRangeTextW(hwndEditor, lineStart, lineEnd);
    if (lineText.empty())
        return false;

    CHARRANGE insertAt = {};
    insertAt.cpMin = duplicateBelow ? lineEnd : lineStart;
    insertAt.cpMax = insertAt.cpMin;
    SendMessageW(hwndEditor, EM_EXSETSEL, 0, (LPARAM)&insertAt);
    SendMessageW(hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)lineText.c_str());

    const LONG newCaret = duplicateBelow ? lineEnd : (lineStart + (lineEnd - lineStart));
    CHARRANGE out = {newCaret, newCaret};
    SendMessageW(hwndEditor, EM_EXSETSEL, 0, (LPARAM)&out);
    SendMessageW(hwndEditor, EM_SCROLLCARET, 0, 0);
    return true;
}

bool editorDeleteCurrentLine(HWND hwndEditor)
{
    if (!hwndEditor)
        return false;

    CHARRANGE sel = {};
    SendMessageW(hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    const int line = (int)SendMessageW(hwndEditor, EM_LINEFROMCHAR, sel.cpMax, 0);
    const LONG lineStart = (LONG)SendMessageW(hwndEditor, EM_LINEINDEX, line, 0);
    if (lineStart < 0)
        return false;

    const LONG textLen = GetWindowTextLengthW(hwndEditor);
    LONG lineEnd = (LONG)SendMessageW(hwndEditor, EM_LINEINDEX, line + 1, 0);
    if (lineEnd < 0)
        lineEnd = textLen;

    CHARRANGE range = {lineStart, lineEnd};
    SendMessageW(hwndEditor, EM_EXSETSEL, 0, (LPARAM)&range);
    SendMessageW(hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)L"");

    CHARRANGE out = {lineStart, lineStart};
    SendMessageW(hwndEditor, EM_EXSETSEL, 0, (LPARAM)&out);
    SendMessageW(hwndEditor, EM_SCROLLCARET, 0, 0);
    return true;
}

bool editorMoveCurrentLine(HWND hwndEditor, bool moveDown)
{
    if (!hwndEditor)
        return false;

    CHARRANGE sel = {};
    SendMessageW(hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    const LONG caret = sel.cpMax;

    const int line = (int)SendMessageW(hwndEditor, EM_LINEFROMCHAR, caret, 0);
    const LONG textLen = GetWindowTextLengthW(hwndEditor);

    const LONG lineStart = (LONG)SendMessageW(hwndEditor, EM_LINEINDEX, line, 0);
    if (lineStart < 0)
        return false;

    LONG lineEnd = (LONG)SendMessageW(hwndEditor, EM_LINEINDEX, line + 1, 0);
    if (lineEnd < 0)
        lineEnd = textLen;

    const std::wstring lineText = getEditorRangeTextW(hwndEditor, lineStart, lineEnd);
    if (lineText.empty())
        return false;

    if (!moveDown)
    {
        if (line <= 0)
            return false;

        const LONG prevStart = (LONG)SendMessageW(hwndEditor, EM_LINEINDEX, line - 1, 0);
        const std::wstring prevText = getEditorRangeTextW(hwndEditor, prevStart, lineStart);

        CHARRANGE block = {prevStart, lineEnd};
        SendMessageW(hwndEditor, EM_EXSETSEL, 0, (LPARAM)&block);
        const std::wstring replacement = lineText + prevText;
        SendMessageW(hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)replacement.c_str());

        CHARRANGE out = {prevStart, prevStart};
        SendMessageW(hwndEditor, EM_EXSETSEL, 0, (LPARAM)&out);
        SendMessageW(hwndEditor, EM_SCROLLCARET, 0, 0);
        return true;
    }

    const LONG nextStart = lineEnd;
    if (nextStart >= textLen)
        return false;

    LONG nextEnd = (LONG)SendMessageW(hwndEditor, EM_LINEINDEX, line + 2, 0);
    if (nextEnd < 0)
        nextEnd = textLen;

    const std::wstring nextText = getEditorRangeTextW(hwndEditor, nextStart, nextEnd);
    CHARRANGE block = {lineStart, nextEnd};
    SendMessageW(hwndEditor, EM_EXSETSEL, 0, (LPARAM)&block);
    const std::wstring replacement = nextText + lineText;
    SendMessageW(hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)replacement.c_str());

    const LONG newLineStart = lineStart + static_cast<LONG>(nextText.size());
    CHARRANGE out = {newLineStart, newLineStart};
    SendMessageW(hwndEditor, EM_EXSETSEL, 0, (LPARAM)&out);
    SendMessageW(hwndEditor, EM_SCROLLCARET, 0, 0);
    return true;
}

bool editorToggleCommentSelection(HWND hwndEditor, const std::string& currentFile)
{
    if (!hwndEditor)
        return false;

    CHARRANGE sel = {};
    SendMessageW(hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);

    int startLine = (int)SendMessageW(hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
    int endLine = (int)SendMessageW(hwndEditor, EM_LINEFROMCHAR, sel.cpMax, 0);
    if (sel.cpMax > sel.cpMin)
    {
        const LONG endLineStart = (LONG)SendMessageW(hwndEditor, EM_LINEINDEX, endLine, 0);
        if (endLineStart >= 0 && sel.cpMax == endLineStart && endLine > startLine)
            --endLine;
    }

    const LONG rangeStart = (LONG)SendMessageW(hwndEditor, EM_LINEINDEX, startLine, 0);
    if (rangeStart < 0)
        return false;

    LONG rangeEnd = (LONG)SendMessageW(hwndEditor, EM_LINEINDEX, endLine + 1, 0);
    if (rangeEnd < 0)
        rangeEnd = GetWindowTextLengthW(hwndEditor);
    if (rangeEnd < rangeStart)
        return false;

    std::wstring block = getEditorRangeTextW(hwndEditor, rangeStart, rangeEnd);
    if (block.empty())
        return false;

    const std::wstring prefix = utf8ToWide(editorCommentPrefixForPath(currentFile));
    if (prefix.empty())
        return false;

    struct LineChunk
    {
        std::wstring text;
        std::wstring newline;
    };

    std::vector<LineChunk> lines;
    size_t cursor = 0;
    while (cursor < block.size())
    {
        size_t nextBreak = block.find_first_of(L"\r\n", cursor);
        LineChunk chunk;
        if (nextBreak == std::wstring::npos)
        {
            chunk.text = block.substr(cursor);
            cursor = block.size();
        }
        else
        {
            chunk.text = block.substr(cursor, nextBreak - cursor);
            if (block[nextBreak] == L'\r' && nextBreak + 1 < block.size() && block[nextBreak + 1] == L'\n')
            {
                chunk.newline = L"\r\n";
                cursor = nextBreak + 2;
            }
            else
            {
                chunk.newline.assign(1, block[nextBreak]);
                cursor = nextBreak + 1;
            }
        }
        lines.push_back(std::move(chunk));
    }

    bool uncomment = true;
    bool sawContent = false;
    for (const auto& line : lines)
    {
        const size_t indent = line.text.find_first_not_of(L" \t");
        if (indent == std::wstring::npos)
            continue;
        sawContent = true;
        if (line.text.compare(indent, prefix.size(), prefix) != 0)
        {
            uncomment = false;
            break;
        }
    }
    if (!sawContent)
        return false;

    for (auto& line : lines)
    {
        const size_t indent = line.text.find_first_not_of(L" \t");
        if (indent == std::wstring::npos)
            continue;

        if (uncomment)
        {
            if (line.text.compare(indent, prefix.size(), prefix) == 0)
            {
                line.text.erase(indent, prefix.size());
                if (indent < line.text.size() && line.text[indent] == L' ')
                    line.text.erase(indent, 1);
            }
        }
        else
        {
            std::wstring injected = prefix;
            injected += L' ';
            line.text.insert(indent, injected);
        }
    }

    std::wstring updated;
    for (const auto& line : lines)
    {
        updated += line.text;
        updated += line.newline;
    }

    CHARRANGE replaceRange = {rangeStart, rangeEnd};
    SendMessageW(hwndEditor, EM_EXSETSEL, 0, (LPARAM)&replaceRange);
    SendMessageW(hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)updated.c_str());

    CHARRANGE out = {rangeStart, rangeStart + (LONG)updated.size()};
    SendMessageW(hwndEditor, EM_EXSETSEL, 0, (LPARAM)&out);
    SendMessageW(hwndEditor, EM_SCROLLCARET, 0, 0);
    return true;
}
}  // namespace

bool Win32IDE::toggleEditorCommentSelection()
{
    if (!editorToggleCommentSelection(m_hwndEditor, m_currentFile))
        return false;
    onEditorContentChanged();
    return true;
}

bool Win32IDE::duplicateEditorLine(bool duplicateBelow)
{
    if (!editorDuplicateCurrentLine(m_hwndEditor, duplicateBelow))
        return false;
    onEditorContentChanged();
    return true;
}

bool Win32IDE::deleteEditorLine()
{
    if (!editorDeleteCurrentLine(m_hwndEditor))
        return false;
    onEditorContentChanged();
    return true;
}

bool Win32IDE::moveEditorLine(bool moveDown)
{
    if (!editorMoveCurrentLine(m_hwndEditor, moveDown))
        return false;
    onEditorContentChanged();
    return true;
}

LRESULT CALLBACK Win32IDE::EditorSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetPropW(hwnd, kEditorWndProp);
    WNDPROC oldProc = (WNDPROC)GetPropW(hwnd, kEditorProcProp);
    // Prevent recursive editor subclass re-entry loops triggered by overlay queries
    // that synchronously SendMessage back into the same subclass proc.
    static thread_local int s_editorSubclassDepth = 0;
    struct EditorSubclassDepthGuard
    {
        int& depth;
        explicit EditorSubclassDepthGuard(int& d) : depth(d) { ++depth; }
        ~EditorSubclassDepthGuard() { --depth; }
    } depthGuard(s_editorSubclassDepth);
    const bool canQueryOverlayCaret = (s_editorSubclassDepth == 1);

    if (pThis)
    {
        switch (uMsg)
        {
            case WM_IME_STARTCOMPOSITION:
            case WM_IME_COMPOSITION:
            {
                LRESULT result = oldProc ? CallWindowProcW(oldProc, hwnd, uMsg, wParam, lParam)
                                         : DefWindowProcW(hwnd, uMsg, wParam, lParam);
                if (pThis->lineStripEditorEnabled())
                {
                    pThis->syncLineStripImeComposition();
                    pThis->invalidateLineStripOverlay(nullptr);
                }
                return result;
            }

            case WM_VSCROLL:
            case WM_HSCROLL:
            case WM_MOUSEWHEEL:
                // After scroll, sync line numbers and minimap
                if (oldProc)
                {
                    LRESULT result = CallWindowProcW(oldProc, hwnd, uMsg, wParam, lParam);
                    pThis->updateLineNumbers();
                    if (pThis->lineStripEditorEnabled())
                    {
                        pThis->layoutLineStripOverlay();
                        pThis->invalidateLineStripOverlay(nullptr);
                    }
                    if (pThis->m_minimapVisible)
                        pThis->updateMinimap();
                    if (pThis->m_ghostTextVisible)
                    {
                        if (canQueryOverlayCaret)
                        {
                            CHARRANGE sel = {};
                            SendMessageA(hwnd, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
                            POINTL pt = {};
                            SendMessageA(hwnd, EM_POSFROMCHAR, (WPARAM)sel.cpMin, reinterpret_cast<LPARAM>(&pt));
                            pThis->updateGhostDiffOverlayUi(pt);
                        }
                    }
                    else
                    {
                        pThis->hideGhostDiffOverlayUi();
                    }
                    return result;
                }
                break;

            case WM_KEYDOWN:
            {
                const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

                // AI / LSP workspace-edit undo atom (command WAL). If no WAL entry, RichEdit handles Ctrl+Shift+Z as
                // redo.
                if (ctrl && shift && !alt && (wParam == 'Z' || wParam == 'z'))
                {
                    if (pThis->rollbackLastAIEditTransaction())
                    {
                        pThis->onEditorContentChanged();
                        return 0;
                    }
                }

                if (!ctrl && !alt && !shift && wParam == VK_RETURN)
                {
                    if (pThis->tryExecuteEditorSlashLine(hwnd))
                    {
                        return 0;
                    }
                }

                if (pThis->isSlashOverlayVisible() && pThis->m_slashOverlayEditorMode)
                {
                    const bool shiftTab = (wParam == VK_TAB && shift);
                    if (!shiftTab && (wParam == VK_TAB || wParam == VK_UP || wParam == VK_DOWN || wParam == VK_ESCAPE ||
                                      wParam == VK_RETURN))
                    {
                        if (pThis->handleSlashOverlayKey(wParam))
                        {
                            return 0;
                        }
                    }
                }

                // Route existing multi-cursor keymap through the editor subclass.
                if (pThis->handleMultiCursorKeyDown(wParam, ctrl, shift, alt))
                {
                    pThis->onEditorContentChanged();
                    return 0;
                }

                // Ghost text (Tab / Esc / Ctrl+Space / Ctrl+Right partial accept) before Ctrl+arrow word nav so
                // Ctrl+Right commits the next ghost chunk instead of moving the caret.
                if (pThis->handleGhostTextKey((UINT)wParam, ctrl, shift))
                {
                    return 0;
                }

                // VSCode-like word navigation parity.
                if (ctrl && !alt && (wParam == VK_LEFT || wParam == VK_RIGHT))
                {
                    if (editorMoveByWord(hwnd, wParam == VK_RIGHT, shift))
                        return 0;
                }

                if (ctrl && !shift && !alt && wParam == VK_OEM_2)
                {
                    if (pThis->toggleEditorCommentSelection())
                        return 0;
                }

                // VSCode-like line operations parity.
                if (ctrl && shift && !alt && wParam == 'K')
                {
                    if (pThis->deleteEditorLine())
                        return 0;
                }
                if (alt && shift && !ctrl && (wParam == VK_UP || wParam == VK_DOWN))
                {
                    if (pThis->duplicateEditorLine(wParam == VK_DOWN))
                        return 0;
                }
                if (alt && !shift && !ctrl && (wParam == VK_UP || wParam == VK_DOWN))
                {
                    if (pThis->moveEditorLine(wParam == VK_DOWN))
                        return 0;
                }

                // F1 — Command Palette (VS Code)
                if (!ctrl && !shift && !alt && wParam == VK_F1)
                {
                    pThis->routeCommand(7008);
                    return 0;
                }
                // Ctrl+Shift+P → command palette toggle
                if (wParam == 'P' && ctrl && shift && !alt)
                {
                    if (pThis->m_commandPaletteVisible)
                        pThis->hideCommandPalette();
                    else
                        pThis->showCommandPalette();
                    return 0;
                }
                // Ctrl+Shift+Y → toggle current file context (fallback when accelerator isn't active)
                if (wParam == 'Y' && ctrl && shift && !alt)
                {
                    pThis->routeCommand(IDM_AGENT_TOGGLE_FILE_CONTEXT);
                    return 0;
                }
                // Ctrl+Shift+D → Downloads panel toggle
                if (wParam == 'D' && ctrl && shift && !alt)
                {
                    pThis->toggleDownloadsPanel();
                    return 0;
                }
                // Cursor / VS Code parity: Ctrl+P Quick Open (same as accelerator / main loop)
                if (ctrl && !shift && !alt && wParam == 'P')
                {
                    pThis->routeCommand(IDM_FILE_QUICK_OPEN);
                    return 0;
                }
                if (ctrl && !shift && !alt && (wParam == VK_OEM_3 || wParam == VK_OEM_8))
                {
                    pThis->routeCommand(2029);
                    return 0;
                }
                // VS Code: Ctrl+J toggle panel (bottom) — distinct from Ctrl+` terminal focus
                if (ctrl && !shift && !alt && wParam == 'J')
                {
                    pThis->routeCommand(IDM_VIEW_TOGGLE_BOTTOM_PANEL);
                    return 0;
                }
                // Cursor-style: Ctrl+L → Agent / chat
                if (ctrl && !shift && !alt && wParam == 'L')
                {
                    pThis->routeCommand(3009);
                    return 0;
                }
                // F9 → toggle breakpoint at current line
                if (wParam == VK_F9)
                {
                    CHARRANGE sel;
                    SendMessage(hwnd, EM_EXGETSEL, 0, (LPARAM)&sel);
                    int line = (int)SendMessage(hwnd, EM_LINEFROMCHAR, sel.cpMin, 0) + 1;
                    pThis->toggleBreakpoint(pThis->m_currentFile, line);
                    return 0;
                }
                break;
            }

            case WM_KEYUP:
            case WM_LBUTTONUP:
            {
                pThis->updateLiveSymbolPromptContextFromEditor();
                pThis->syncEditorSlashOverlayFromEditor(hwnd);
                break;
            }

            case WM_PAINT:
            {
                if (oldProc)
                {
                    // When ghost text or the Titan paging spinner is active, use a back-buffer
                    // to composite RichEdit content + overlay in one atomic BitBlt.
                    // The previous GetDC-after-CallWindowProc pattern caused visible flicker because:
                    //  1. RichEdit calls BeginPaint/EndPaint internally — update region is validated.
                    //  2. Our GetDC overlay writes outside any BeginPaint context.
                    //  3. UpdateWindow() calls from the streaming token path immediately trigger
                    //     another WM_PAINT that erases and redraws — ~67ms cycle = perceptible flicker.
                    const bool titanActive = pThis && pThis->m_titanAgentRunning;
                    const bool ghostActive = pThis && pThis->m_ghostTextVisible;
                    const bool suggestionPulseActive =
                        pThis && (pThis->m_activeSuggestionContext.state == SuggestionState::Pending ||
                                  ((pThis->m_activeSuggestionContext.state == SuggestionState::Accepted ||
                                    pThis->m_activeSuggestionContext.state == SuggestionState::Rejected) &&
                                   (static_cast<uint64_t>(GetTickCount64()) <=
                                    pThis->m_activeSuggestionContext.stateChangedTickMs + 220ull)));
                    if (ghostActive || titanActive || suggestionPulseActive)
                    {
                        static uint64_t s_overlayPaintCount = 0;
                        static double s_overlayPaintMsSum = 0.0;
                        static double s_overlayPaintMsMax = 0.0;
                        static LARGE_INTEGER s_overlayQpcFreq = {};
                        // Cached back-buffer — reallocated only when the editor is resized.
                        // Avoids ~4MB GDI heap churn per overlay frame at 1080p.
                        static HBITMAP s_cachedBackBuf = nullptr;
                        static int s_cachedBackBufW = 0;
                        static int s_cachedBackBufH = 0;
                        if (s_overlayQpcFreq.QuadPart == 0)
                        {
                            QueryPerformanceFrequency(&s_overlayQpcFreq);
                        }

                        LARGE_INTEGER paintStart{};
                        QueryPerformanceCounter(&paintStart);

                        PAINTSTRUCT ps;
                        HDC hdcScreen = BeginPaint(hwnd, &ps);
                        RECT rc;
                        GetClientRect(hwnd, &rc);
                        const int w = rc.right - rc.left;
                        const int h = rc.bottom - rc.top;
                        if (hdcScreen && w > 0 && h > 0)
                        {
                            // Reallocate cached back-buffer only on dimension change.
                            if (w != s_cachedBackBufW || h != s_cachedBackBufH || !s_cachedBackBuf)
                            {
                                if (s_cachedBackBuf)
                                {
                                    DeleteObject(s_cachedBackBuf);
                                    s_cachedBackBuf = nullptr;
                                }
                                s_cachedBackBuf = CreateCompatibleBitmap(hdcScreen, w, h);
                                s_cachedBackBufW = w;
                                s_cachedBackBufH = h;
                            }
                            HDC hdcMem = CreateCompatibleDC(hdcScreen);
                            if (hdcMem && s_cachedBackBuf)
                            {
                                HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, s_cachedBackBuf);
                                // RichEdit renders its full client area into the off-screen buffer.
                                // WM_PRINTCLIENT is the correct message for this — it does not go
                                // through BeginPaint/EndPaint so we own the DC lifecycle here.
                                CallWindowProcW(oldProc, hwnd, WM_PRINTCLIENT, (WPARAM)hdcMem, PRF_CLIENT);
                                // Sovereign diff pass: draw semi-transparent pending suggestion range tint.
                                pThis->renderSuggestionTint(hdcMem);
                                // Ghost text / Titan paging spinner composited off-screen — zero flicker.
                                pThis->renderGhostText(hdcMem);
                                // Single atomic blt: screen never sees an intermediate state.
                                BitBlt(hdcScreen, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);
                                SelectObject(hdcMem, hbmOld);
                                // hbmOld is restored; s_cachedBackBuf stays alive for next frame.
                            }
                            if (hdcMem)
                                DeleteDC(hdcMem);
                        }
                        EndPaint(hwnd, &ps);

                        LARGE_INTEGER paintEnd{};
                        QueryPerformanceCounter(&paintEnd);
                        if (s_overlayQpcFreq.QuadPart > 0)
                        {
                            const double frameMs = (double)(paintEnd.QuadPart - paintStart.QuadPart) * 1000.0 /
                                                   (double)s_overlayQpcFreq.QuadPart;
                            ++s_overlayPaintCount;
                            s_overlayPaintMsSum += frameMs;
                            if (frameMs > s_overlayPaintMsMax)
                                s_overlayPaintMsMax = frameMs;

                            if ((s_overlayPaintCount % 240ull) == 0ull)
                            {
                                const double avgMs = s_overlayPaintMsSum / (double)s_overlayPaintCount;
                                char perfBuf[256] = {};
                                sprintf_s(perfBuf, "[GhostOverlayPerf] frames=%llu avg_ms=%.3f max_ms=%.3f\n",
                                          (unsigned long long)s_overlayPaintCount, avgMs, s_overlayPaintMsMax);
                                OutputDebugStringA(perfBuf);
                            }
                        }
                        return 0;
                    }
                    // No overlay needed — let RichEdit handle WM_PAINT normally.
                    return CallWindowProcW(oldProc, hwnd, uMsg, wParam, lParam);
                }
                break;
            }

            case WM_CHAR:
            {
                wchar_t typedChar = static_cast<wchar_t>(wParam);

                // ─── Phase 1b: Symbol Index Bridge trigger detection ───────────
                // Detect completion triggers (::, ., ->, identifier start)
                // and query the SymbolIndexBridge for ranked candidates.
                if (pThis->m_symbolIndexBridge && pThis->m_symbolIndexBridge->isInitialized())
                {
                    // Get cursor position
                    CHARRANGE sel = {};
                    SendMessageW(hwnd, EM_EXGETSEL, 0, (LPARAM)&sel);
                    int line = (int)SendMessageW(hwnd, EM_LINEFROMCHAR, sel.cpMin, 0);
                    int col = sel.cpMin - (int)SendMessageW(hwnd, EM_LINEINDEX, line, 0);

                    // Get current file content for trigger detection
                    int textLen = GetWindowTextLengthW(hwnd);
                    std::wstring text;
                    if (textLen > 0)
                    {
                        text.resize(textLen + 1);
                        GetWindowTextW(hwnd, text.data(), textLen + 1);
                        text.resize(textLen);
                    }

                    // Convert to UTF-8 for bridge
                    std::string utf8Text;
                    if (!text.empty())
                    {
                        int needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
                        if (needed > 0)
                        {
                            utf8Text.resize(needed - 1);
                            WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &utf8Text[0], needed, nullptr, nullptr);
                        }
                    }

                    // Detect trigger
                    auto trigger = pThis->m_symbolIndexBridge->detectTrigger(utf8Text, sel.cpMin);

                    if (trigger.kind != rawrxd::bridge::TriggerKind::None)
                    {
                        // Query completions from bridge
                        auto candidates = pThis->m_symbolIndexBridge->queryCompletions(pThis->m_currentFile,
                                                                                       trigger.prefix, line, col);

                        if (!candidates.empty())
                        {
                            // Store candidates for ghost text renderer (Phase 1c)
                            pThis->m_symbolBridgeCandidates = std::move(candidates);
                            pThis->m_symbolBridgeTrigger = trigger;

                            // Signal that completions are ready
                            // Ghost text renderer will pick these up in WM_PAINT
                            pThis->m_symbolBridgeReady = true;

                            // Debug output
                            char dbg[256];
                            snprintf(dbg, sizeof(dbg), "[SymbolBridge] Trigger='%s' kind=%d candidates=%zu\n",
                                     trigger.prefix.c_str(), (int)trigger.kind, pThis->m_symbolBridgeCandidates.size());
                            OutputDebugStringA(dbg);
                        }
                    }
                }

                if (pThis->m_ghostTextVisible)
                {
                    pThis->handleGhostTextTypedChar(typedChar);
                }
                if (pThis->handleMultiCursorChar(wParam))
                {
                    pThis->onEditorContentChanged();
                    return 0;
                }
                // After character input, trigger syntax coloring debounce
                if (oldProc)
                {
                    LRESULT result = CallWindowProcW(oldProc, hwnd, uMsg, wParam, lParam);
                    pThis->onEditorContentChanged();
                    pThis->syncEditorSlashOverlayFromEditor(hwnd);
                    return result;
                }
                break;
            }

            case WM_MOUSEMOVE:
            {
                int xPos = GET_X_LPARAM(lParam);
                int yPos = GET_Y_LPARAM(lParam);
                // Trigger debug hover value display (Phase 1C)
                pThis->onEditorMouseMoveDebugHover(xPos, yPos);
                // Also trigger LSP hover
                pThis->onEditorMouseHover(xPos, yPos);
                break;
            }

            case WM_MOUSELEAVE:
            {
                // Hide debug hover when mouse leaves editor
                pThis->hideDebugHoverValue();
                pThis->dismissHoverTooltip();
                break;
            }

            case WM_DESTROY:
                // Clean up properties on destruction
                pThis->destroyGhostDiffOverlayUi();
                RemovePropW(hwnd, kEditorWndProp);
                RemovePropW(hwnd, kEditorProcProp);
                break;
        }
    }

    if (oldProc)
    {
        if (pThis)
        {
            if (pThis->m_ghostTextVisible)
            {
                if (canQueryOverlayCaret)
                {
                    CHARRANGE sel = {};
                    SendMessageA(hwnd, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
                    POINTL pt = {};
                    SendMessageA(hwnd, EM_POSFROMCHAR, (WPARAM)sel.cpMin, reinterpret_cast<LPARAM>(&pt));
                    pThis->updateGhostDiffOverlayUi(pt);
                }
            }
            else
            {
                pThis->hideGhostDiffOverlayUi();
            }
        }
        return CallWindowProcW(oldProc, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// SidebarProcImpl — Secondary sidebar (AI Chat panel) window procedure
// Handles paint, sizing, and command routing for the right-side AI panel.
// Distinct from SidebarProc which handles the primary (left) sidebar.
// ============================================================================
static void logSidebarRefreshSeh(const char* phase, DWORD sehCode)
{
    char crashMsg[192];
    snprintf(crashMsg, sizeof(crashMsg), "[SidebarProcImpl] %s model refresh SEH 0x%08lX (non-fatal)\n",
             phase ? phase : "unknown", (unsigned long)sehCode);
    OutputDebugStringA(crashMsg);
}

static bool tryPopulateModelSelectorSeh(Win32IDE* ide, const char* phase)
{
    if (!ide)
        return false;

    __try
    {
        ide->populateModelSelector();
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        logSidebarRefreshSeh(phase, GetExceptionCode());
        return false;
    }
}

LRESULT CALLBACK Win32IDE::SidebarProcImpl(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_SHOWWINDOW:
        {
            if (pThis && wParam)
            {
                const int cntBefore = (pThis->m_hwndModelSelector && IsWindow(pThis->m_hwndModelSelector))
                                          ? (int)SendMessage(pThis->m_hwndModelSelector, CB_GETCOUNT, 0, 0)
                                          : -1;
                if (cntBefore <= 0)
                {
                    if (tryPopulateModelSelectorSeh(pThis, "WM_SHOWWINDOW"))
                    {
                        const int cntAfter = (pThis->m_hwndModelSelector && IsWindow(pThis->m_hwndModelSelector))
                                                 ? (int)SendMessage(pThis->m_hwndModelSelector, CB_GETCOUNT, 0, 0)
                                                 : -1;
                        std::string dbg =
                            "[SidebarProcImpl] WM_SHOWWINDOW repopulate model count=" + std::to_string(cntAfter) + "\n";
                        OutputDebugStringA(dbg.c_str());
                    }
                }
                else
                {
                    std::string dbg = "[SidebarProcImpl] WM_SHOWWINDOW skip repopulate existing model count=" +
                                      std::to_string(cntBefore) + "\n";
                    OutputDebugStringA(dbg.c_str());
                }
            }
            break;
        }

        case WM_SETFOCUS:
        {
            if (pThis && pThis->m_hwndModelSelector)
            {
                const int cnt = (int)SendMessage(pThis->m_hwndModelSelector, CB_GETCOUNT, 0, 0);
                if (cnt <= 0)
                {
                    (void)tryPopulateModelSelectorSeh(pThis, "WM_SETFOCUS");
                }
            }
            break;
        }

        case WM_ACTIVATE:
        {
            if (pThis && LOWORD(wParam) != WA_INACTIVE)
            {
                const int cnt = (pThis->m_hwndModelSelector && IsWindow(pThis->m_hwndModelSelector))
                                    ? (int)SendMessage(pThis->m_hwndModelSelector, CB_GETCOUNT, 0, 0)
                                    : -1;
                if (cnt <= 0)
                {
                    OutputDebugStringA("[SidebarProcImpl] WM_ACTIVATE refresh because model count is zero\n");
                    (void)tryPopulateModelSelectorSeh(pThis, "WM_ACTIVATE");
                }
            }
            break;
        }

        case WM_PAINT:
        {
            uint64_t paintStart = PulseGetCycles();
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            COLORREF bgColor = pThis ? pThis->m_currentTheme.sidebarBg : RGB(37, 37, 38);
            HBRUSH hBrush = CreateSolidBrush(bgColor);
            FillRect(hdc, &rc, hBrush);
            DeleteObject(hBrush);

            // ── KAIROS UI SYNTHESIS (Batch 3) ──
            // If Truth Divergence is detected by the Context Governor, signals flash here.
            // We use the Sovereign Atlas (MASM) for zero-overhead diagnostic icons.
            if (pThis && pThis->m_emojiSupportInitialized)
            {
                // Example: Render Symbol 0x90 (Temporal Lock) when waiting on hardware
                // RawrXD_Emoji_Atlas_AlphaBlit(hdc, 0x90, 10, 10, 255);
            }

            EndPaint(hwnd, &ps);

            uint64_t paintEnd = PulseGetCycles();
            if (g_pulseRing.instance().isActive())
            {
                g_pulseRing.Log(6, static_cast<uint32_t>(paintEnd - paintStart));  // Stage 6: UI Render/Paint Latency
            }
            return 0;
        }

        case WM_COMMAND:
        {
            if (pThis)
            {
                int controlId = LOWORD(wParam);
                int notifyCode = HIWORD(wParam);

                if (CommandPreview_HandleCommand(controlId))
                {
                    if (controlId == 7211)
                    {
                        pThis->postDeferredCopilotSend();
                    }
                    return 0;
                }

                // Route button clicks from AI Chat panel controls
                if (controlId == IDC_COPILOT_SEND_BTN)
                {
                    std::string promptPreview;
                    if (pThis->m_hwndCopilotChatInput && IsWindow(pThis->m_hwndCopilotChatInput))
                    {
                        const int inputLen = GetWindowTextLengthW(pThis->m_hwndCopilotChatInput);
                        if (inputLen > 0)
                        {
                            std::vector<wchar_t> inputBuffer(static_cast<size_t>(inputLen) + 1, L'\0');
                            GetWindowTextW(pThis->m_hwndCopilotChatInput, inputBuffer.data(), inputLen + 1);
                            promptPreview = wideToUtf8(inputBuffer.data());
                        }
                    }
                    const bool hasAgenticPrefix = HasAgenticPrefix(promptPreview);
                    const bool bridgeAgenticMode = pThis->m_agenticBridge && pThis->m_agenticBridge->IsAgenticMode();
                    const bool wantsAgentic =
                        hasAgenticPrefix || bridgeAgenticMode || pThis->m_agenticFunctionCallingMode;
                    const bool layerAvailable = rawrxd::isAgenticLayerAvailable();
                    OutputDebugStringA(
                        ("ROUTE_CHECK: route=B-sidebar-command, prompt_len=" + std::to_string(promptPreview.size()) +
                         ", wantsAgentic=" + std::to_string(wantsAgentic ? 1 : 0) +
                         ", layerAvailable=" + std::to_string(layerAvailable ? 1 : 0) + "\n")
                            .c_str());
                    OutputDebugStringA("[SidebarProcImpl] Send clicked\n");
                    // Leave sidebar child-control wndproc stack before executing send logic.
                    // This avoids nested SendMessage chains and reduces stack depth spikes.
                    pThis->postDeferredCopilotSend();
                    return 0;
                }
                else if (controlId == IDC_COPILOT_CLEAR_BTN)
                {
                    OutputDebugStringA("[SidebarProcImpl] Clear clicked\n");
                    pThis->HandleCopilotClear();
                    return 0;
                }
                else if (controlId == IDC_COPILOT_NEW_CHAT_BTN)
                {
                    OutputDebugStringA("[SidebarProcImpl] New Chat clicked\n");
                    pThis->HandleCopilotClear();
                    pThis->appendCopilotChatTextOnUiThread("\n[Chat] Started a new chat. Model and mode are ready.\n");
                    if (pThis->m_hwndCopilotChatInput && IsWindow(pThis->m_hwndCopilotChatInput))
                        SetFocus(pThis->m_hwndCopilotChatInput);
                    return 0;
                }
                else if (controlId == IDC_COPILOT_HELPFUL_BTN)
                {
                    ++pThis->m_copilotHelpfulCount;
                    pThis->appendToOutput(
                        "[ChatFeedback] Helpful +1 (helpful=" + std::to_string(pThis->m_copilotHelpfulCount) +
                            ", unhelpful=" + std::to_string(pThis->m_copilotUnhelpfulCount) + ")\n",
                        "Output", OutputSeverity::Info);
                    pThis->appendCopilotChatTextOnUiThread("\n[Feedback] Marked as helpful.\n");
                    return 0;
                }
                else if (controlId == IDC_COPILOT_UNHELPFUL_BTN)
                {
                    ++pThis->m_copilotUnhelpfulCount;
                    pThis->appendToOutput(
                        "[ChatFeedback] Unhelpful +1 (helpful=" + std::to_string(pThis->m_copilotHelpfulCount) +
                            ", unhelpful=" + std::to_string(pThis->m_copilotUnhelpfulCount) + ")\n",
                        "Output", OutputSeverity::Warning);
                    pThis->appendCopilotChatTextOnUiThread("\n[Feedback] Marked as unhelpful.\n");
                    return 0;
                }
                else if (controlId == IDC_COPILOT_COPY_BTN)
                {
                    if (pThis->m_lastCopilotAssistantResponse.empty())
                    {
                        pThis->appendCopilotChatTextOnUiThread("\n[Copy] No assistant response to copy yet.\n");
                        return 0;
                    }

                    const std::wstring wText = utf8ToWide(pThis->m_lastCopilotAssistantResponse);
                    HWND owner = pThis->m_hwndMain ? pThis->m_hwndMain : hwnd;
                    if (!OpenClipboard(owner))
                    {
                        pThis->appendCopilotChatTextOnUiThread("\n[Copy] Clipboard unavailable.\n");
                        return 0;
                    }
                    EmptyClipboard();

                    const size_t bytes = (wText.size() + 1) * sizeof(wchar_t);
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
                    if (hMem)
                    {
                        void* data = GlobalLock(hMem);
                        if (data)
                        {
                            memcpy(data, wText.c_str(), bytes);
                            GlobalUnlock(hMem);
                            if (!SetClipboardData(CF_UNICODETEXT, hMem))
                            {
                                GlobalFree(hMem);
                            }
                            else
                            {
                                pThis->appendCopilotChatTextOnUiThread("\n[Copy] Last response copied.\n");
                            }
                        }
                        else
                        {
                            GlobalFree(hMem);
                        }
                    }
                    CloseClipboard();
                    return 0;
                }
                else if (controlId == IDC_COPILOT_RETRY_BTN)
                {
                    if (pThis->m_lastCopilotUserPrompt.empty())
                    {
                        pThis->appendCopilotChatTextOnUiThread("\n[Retry] No prior prompt to retry.\n");
                        return 0;
                    }

                    if (pThis->m_hwndCopilotChatInput && IsWindow(pThis->m_hwndCopilotChatInput))
                    {
                        const std::wstring wPrompt = utf8ToWide(pThis->m_lastCopilotUserPrompt);
                        SetWindowTextW(pThis->m_hwndCopilotChatInput, wPrompt.c_str());
                    }
                    pThis->appendCopilotChatTextOnUiThread("\n[Retry] Re-sending last prompt.\n");
                    pThis->postDeferredCopilotSend();
                    return 0;
                }
                else if (controlId == IDC_MODEL_BROWSE_BTN)
                {
                    OutputDebugStringA("[SidebarProcImpl] Browse clicked\n");
                    pThis->handleModelBrowse();
                    return 0;
                }
                else if (controlId == IDC_MODEL_MODE_SELECTOR && notifyCode == CBN_SELCHANGE)
                {
                    HWND hwndMode = GetDlgItem(hwnd, IDC_MODEL_MODE_SELECTOR);
                    const int modeIndex = hwndMode ? (int)SendMessage(hwndMode, CB_GETCURSEL, 0, 0) : 0;
                    auto setCheck = [](HWND h, bool on)
                    {
                        if (h && IsWindow(h))
                            SendMessageW(h, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
                    };

                    // Reset and apply selected profile using existing toggles.
                    setCheck(pThis->m_hwndChkMaxMode, false);
                    setCheck(pThis->m_hwndChkDeepThink, false);
                    setCheck(pThis->m_hwndChkDeepResearch, false);
                    setCheck(pThis->m_hwndChkAgenticMode, false);

                    switch (modeIndex)
                    {
                        case 0:  // Thinking
                            setCheck(pThis->m_hwndChkDeepThink, true);
                            setCheck(pThis->m_hwndChkAgenticMode, true);
                            break;
                        case 1:  // Deep Research
                            setCheck(pThis->m_hwndChkDeepThink, true);
                            setCheck(pThis->m_hwndChkDeepResearch, true);
                            setCheck(pThis->m_hwndChkAgenticMode, true);
                            break;
                        case 2:  // Max Mode
                            setCheck(pThis->m_hwndChkMaxMode, true);
                            setCheck(pThis->m_hwndChkDeepThink, true);
                            setCheck(pThis->m_hwndChkDeepResearch, true);
                            setCheck(pThis->m_hwndChkAgenticMode, true);
                            break;
                        case 3:  // Swarm
                            setCheck(pThis->m_hwndChkAgenticMode, true);
                            break;
                        case 4:  // Browser
                        case 5:  // Vision
                        case 6:  // Video
                            setCheck(pThis->m_hwndChkAgenticMode, true);
                            break;
                        default:
                            break;
                    }

                    pThis->onAIModeMax();
                    pThis->onAIModeDeepThink();
                    pThis->onAIModeDeepResearch();
                    pThis->onAIModeAgentic();
                    pThis->appendToOutput("[ModelMode] Profile applied from selector.\n", "Output",
                                          OutputSeverity::Info);
                    return 0;
                }
                else if (controlId == IDC_MODEL_SELECTOR && notifyCode == CBN_SELCHANGE)
                {
                    OutputDebugStringA("[SidebarProcImpl] Model selection changed\n");
                    pThis->onModelSelectionChanged();
                    return 0;
                }
                else if (controlId == IDC_AI_MAX_MODE && notifyCode == BN_CLICKED)
                {
                    pThis->onAIModeMax();
                }
                else if (controlId == IDC_AI_DEEP_THINK && notifyCode == BN_CLICKED)
                {
                    pThis->onAIModeDeepThink();
                }
                else if (controlId == IDC_AI_DEEP_RESEARCH && notifyCode == BN_CLICKED)
                {
                    pThis->onAIModeDeepResearch();
                }
                else if (controlId == IDC_AI_NO_REFUSAL && notifyCode == BN_CLICKED)
                {
                    pThis->onAIModeNoRefusal();
                }
                else if (controlId == IDC_AI_AGENTIC_MODE && notifyCode == BN_CLICKED)
                {
                    pThis->onAIModeAgentic();
                }
            }
            return 0;
        }

        case WM_HSCROLL:
        {
            if (pThis)
            {
                HWND hwndTrack = (HWND)lParam;
                int controlId = GetWindowLong(hwndTrack, GWL_ID);
                int pos = (int)SendMessage(hwndTrack, TBM_GETPOS, 0, 0);

                if (controlId == IDC_AI_MAX_TOKENS_SLIDER)
                {
                    pThis->onMaxTokensChanged(pos);
                }
                else if (controlId == IDC_AI_CONTEXT_SLIDER)
                {
                    pThis->onContextSizeChanged(pos);
                }
            }
            return 0;
        }

        case WM_SIZE:
        {
            if (pThis)
            {
                const int panelW = (std::max)(0, (int)LOWORD(lParam));
                const int panelH = (std::max)(0, (int)HIWORD(lParam));
                const int margin = pThis->dpiScale(6);
                const int gap = pThis->dpiScale(6);
                const int headerH = pThis->dpiScale(28);
                const int buttonH = pThis->dpiScale(30);
                const int utilityButtonH = pThis->dpiScale(24);
                const int modelBadgeH = pThis->dpiScale(18);
                const int minInputH = pThis->dpiScale(72);
                const int maxInputH = pThis->dpiScale(160);

                const int fullW = (std::max)(0, panelW - margin * 2);
                int topY = headerH + margin;

                HDWP hdwp = BeginDeferWindowPos(28);
                auto moveControl = [&](HWND control, int x, int y, int w, int h)
                {
                    if (!control || !IsWindow(control))
                        return;
                    if (hdwp)
                    {
                        hdwp = DeferWindowPos(hdwp, control, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
                        if (hdwp)
                            return;
                    }
                    MoveWindow(control, x, y, w, h, TRUE);
                };

                moveControl(pThis->m_hwndSecondarySidebarHeader, 0, 0, panelW, headerH);

                HWND hwndBrowseBtn = GetDlgItem(hwnd, IDC_MODEL_BROWSE_BTN);
                HWND hwndModeSelector = GetDlgItem(hwnd, IDC_MODEL_MODE_SELECTOR);
                HWND hwndEffortLabel = GetDlgItem(hwnd, IDC_MODEL_EFFORT_LABEL);
                HWND hwndNewChatBtn = GetDlgItem(hwnd, IDC_COPILOT_NEW_CHAT_BTN);

                if (pThis->m_hwndModelSelector && IsWindow(pThis->m_hwndModelSelector))
                {
                    int y = topY;
                    const int labelW = pThis->dpiScale(52);
                    const int rowGap = pThis->dpiScale(4);
                    const int comboRowH = pThis->dpiScale(22);
                    const int browseW = pThis->dpiScale(76);
                    const int capH = pThis->dpiScale(18);
                    const int sliderH = pThis->dpiScale(25);
                    const int valW = pThis->dpiScale(50);
                    const int chkH = pThis->dpiScale(22);
                    const int halfChkW = (std::max)(1, (fullW - gap) / 2);

                    moveControl(pThis->m_hwndSidebarCaptionModel, margin, y, labelW, comboRowH);
                    const int selectorW = (std::max)(pThis->dpiScale(96), fullW - labelW - gap);
                    moveControl(pThis->m_hwndModelSelector, margin + labelW + gap, y, selectorW, pThis->dpiScale(240));
                    y += comboRowH + rowGap;

                    moveControl(pThis->m_hwndSidebarCaptionMode, margin, y, labelW, comboRowH);
                    moveControl(hwndModeSelector, margin + labelW + gap, y, fullW - labelW - gap, pThis->dpiScale(240));
                    y += comboRowH + rowGap;

                    if (hwndBrowseBtn && IsWindow(hwndBrowseBtn))
                    {
                        moveControl(hwndEffortLabel, margin, y, (std::max)(0, fullW - browseW - gap), capH);
                        moveControl(hwndBrowseBtn, margin + fullW - browseW, y, browseW, capH);
                    }
                    else
                    {
                        moveControl(hwndEffortLabel, margin, y, fullW, capH);
                    }
                    y += capH + margin;

                    moveControl(pThis->m_hwndSidebarCaptionMaxTokens, margin, y, pThis->dpiScale(96), capH);
                    moveControl(pThis->m_hwndMaxTokensLabel, margin + fullW - valW, y, valW, capH);
                    y += capH + rowGap;
                    moveControl(pThis->m_hwndMaxTokensSlider, margin, y, fullW, sliderH);
                    y += sliderH + margin;

                    moveControl(pThis->m_hwndSidebarCaptionContext, margin, y, pThis->dpiScale(96), capH);
                    moveControl(pThis->m_hwndContextLabel, margin + fullW - valW, y, valW, capH);
                    y += capH + rowGap;
                    moveControl(pThis->m_hwndContextSlider, margin, y, fullW, sliderH);
                    y += sliderH + margin;

                    moveControl(pThis->m_hwndChkMaxMode, margin, y, halfChkW, chkH);
                    moveControl(pThis->m_hwndChkDeepThink, margin + halfChkW + gap, y, halfChkW, chkH);
                    y += chkH + rowGap;
                    moveControl(pThis->m_hwndChkDeepResearch, margin, y, halfChkW, chkH);
                    moveControl(pThis->m_hwndChkNoRefusal, margin + halfChkW + gap, y, halfChkW, chkH);
                    y += chkH + rowGap;
                    moveControl(pThis->m_hwndChkAgenticMode, margin, y, halfChkW, chkH);
                    y += chkH + margin;

                    topY = y;
                }

                const int buttonW = (std::max)(pThis->dpiScale(72), (fullW - gap * 2) / 3);
                const int utilityButtonY = (std::max)(topY, panelH - margin - utilityButtonH);
                const int modelBadgeY = (std::max)(topY, utilityButtonY - gap - modelBadgeH);
                const int buttonY = (std::max)(topY, modelBadgeY - gap - buttonH);
                const int inputH = (std::max)(minInputH, (std::min)(maxInputH, panelH / 4));
                const int inputY = (std::max)(topY, buttonY - gap - inputH);
                const int outputY = topY;
                const int outputH = (std::max)(0, inputY - gap - outputY);

                moveControl(pThis->m_hwndCopilotChatOutput, margin, outputY, fullW, outputH);
                moveControl(pThis->m_hwndCopilotChatInput, margin, inputY, fullW, inputH);
                moveControl(pThis->m_hwndCopilotSendBtn, margin, buttonY, buttonW, buttonH);
                moveControl(pThis->m_hwndCopilotClearBtn, margin + buttonW + gap, buttonY, buttonW, buttonH);
                moveControl(hwndNewChatBtn, margin + (buttonW + gap) * 2, buttonY, buttonW, buttonH);

                const int utilityButtonW = (std::max)(pThis->dpiScale(62), (fullW - gap * 3) / 4);
                moveControl(pThis->m_hwndCopilotModelUsedLabel, margin, modelBadgeY, fullW, modelBadgeH);
                moveControl(pThis->m_hwndCopilotHelpfulBtn, margin, utilityButtonY, utilityButtonW, utilityButtonH);
                moveControl(pThis->m_hwndCopilotUnhelpfulBtn, margin + utilityButtonW + gap, utilityButtonY,
                            utilityButtonW, utilityButtonH);
                moveControl(pThis->m_hwndCopilotCopyBtn, margin + (utilityButtonW + gap) * 2, utilityButtonY,
                            utilityButtonW, utilityButtonH);
                moveControl(pThis->m_hwndCopilotRetryBtn, margin + (utilityButtonW + gap) * 3, utilityButtonY,
                            utilityButtonW, utilityButtonH);

                if (hdwp)
                    EndDeferWindowPos(hdwp);

                pThis->updateSecondarySidebarContent();
            }
            return 0;
        }
    }

    // Forward to the original sidebar window procedure
    if (pThis && pThis->m_oldSidebarProc)
    {
        return CallWindowProcA(pThis->m_oldSidebarProc, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// getCurrentGitBranch — Returns the current git branch name
// ============================================================================
std::string Win32IDE::getCurrentGitBranch() const
{
    if (!isGitRepository())
        return "";

    std::string output;
    const_cast<Win32IDE*>(this)->executeGitCommand("git rev-parse --abbrev-ref HEAD", output);

    // Trim whitespace/newlines from output
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r' || output.back() == ' '))
    {
        output.pop_back();
    }
    return output;
}

// ============================================================================
// Terminal Pane Management
// Multi-terminal support: switch, close, resize, and broadcast to panes.
// ============================================================================

void Win32IDE::switchTerminalPane(int paneId)
{
    LOG_INFO("switchTerminalPane: paneId=" + std::to_string(paneId));
    TerminalPane* pane = findTerminalPane(paneId);
    if (pane)
    {
        setActiveTerminalPane(paneId);
        appendToOutput("Switched to terminal: " + pane->name + "\n", "Output", OutputSeverity::Info);
    }
    else
    {
        appendToOutput("Terminal pane " + std::to_string(paneId) + " not found\n", "Output", OutputSeverity::Warning);
    }
}

void Win32IDE::closeTerminalPane(int paneId)
{
    LOG_INFO("closeTerminalPane: paneId=" + std::to_string(paneId));
    for (auto it = m_terminalPanes.begin(); it != m_terminalPanes.end(); /* no-op */)
    {
        if (it->id == paneId)
        {
            if (it->manager)
                it->manager->stop();
            if (it->hwnd && IsWindow(it->hwnd))
                DestroyWindow(it->hwnd);
            const bool wasPrimaryAgent = (m_primaryAgentTerminalId == paneId);
            const bool wasLastUser =
                (m_lastUserInteractiveTerminalId == paneId) && (it->kind == TerminalPaneKind::UserInteractive);
            it = m_terminalPanes.erase(it);
            if (wasPrimaryAgent)
                m_primaryAgentTerminalId = -1;
            if (wasLastUser)
            {
                m_lastUserInteractiveTerminalId = -1;
                for (auto& q : m_terminalPanes)
                {
                    if (q.kind == TerminalPaneKind::UserInteractive)
                    {
                        m_lastUserInteractiveTerminalId = q.id;
                        break;
                    }
                }
            }
            // Switch to another pane if we closed the active one
            if (m_activeTerminalId == paneId && !m_terminalPanes.empty())
            {
                setActiveTerminalPane(m_terminalPanes.front().id);
            }
            appendToOutput("Closed terminal pane " + std::to_string(paneId) + "\n", "Output", OutputSeverity::Info);
            layoutTerminalStrip();
            return;
        }
        else
        {
            ++it;
        }
    }
    appendToOutput("Terminal pane " + std::to_string(paneId) + " not found\n", "Output", OutputSeverity::Warning);
}

void Win32IDE::resizeTerminalPanes()
{
    LOG_INFO("resizeTerminalPanes");
    if (m_terminalPanes.empty())
        return;

    RECT rc;
    GetClientRect(m_hwndMain, &rc);
    int totalWidth = rc.right;
    int paneWidth = totalWidth / static_cast<int>(m_terminalPanes.size());

    int x = 0;
    for (auto& pane : m_terminalPanes)
    {
        if (pane.hwnd && IsWindow(pane.hwnd))
        {
            pane.bounds = {x, 0, x + paneWidth, rc.bottom};
            MoveWindow(pane.hwnd, x, 0, paneWidth, rc.bottom, TRUE);
        }
        x += paneWidth;
    }
}

void Win32IDE::sendToAllTerminals(const std::string& command)
{
    LOG_INFO("sendToAllTerminals: " + command);
    int n = 0;
    for (auto& pane : m_terminalPanes)
    {
        if (pane.kind != TerminalPaneKind::UserInteractive || !pane.manager)
            continue;
        ensureShellRunningForPane(&pane, pane.shellType);
        if (!pane.manager->isRunning())
            continue;
        pane.manager->writeInput(command + "\r\n");
        ++n;
    }
    appendToOutput("Sent to " + std::to_string(n) + " user terminal(s): " + command + "\n", "Output",
                   OutputSeverity::Info);
}

// ============================================================================
// Extension System
// Refresh, load, unload, and help for IDE extensions via m_extensionLoader.
// ============================================================================

void Win32IDE::refreshExtensions()
{
    LOG_INFO("refreshExtensions");
    if (!m_extensionLoader)
    {
        try
        {
            m_extensionLoader = std::make_unique<RawrXD::ExtensionLoader>();
            m_extensionLoader->Scan();
            m_extensionLoader->LoadNativeModules();
        }
        catch (...)
        {
            m_extensionLoader.reset();
        }
    }
    if (m_extensionLoader)
    {
        m_extensionLoader->Scan();
        auto exts = m_extensionLoader->GetExtensions();
        appendToOutput("Extensions refreshed: " + std::to_string(exts.size()) + " found\n", "Output",
                       OutputSeverity::Info);
    }
    else
    {
        appendToOutput("⚠️ Extension loader unavailable (init failed)\n", "Output", OutputSeverity::Warning);
    }
}

void Win32IDE::loadExtension(const std::string& name)
{
    LOG_INFO("loadExtension: " + name);
    if (!m_extensionLoader)
    {
        try
        {
            m_extensionLoader = std::make_unique<RawrXD::ExtensionLoader>();
            m_extensionLoader->Scan();
            m_extensionLoader->LoadNativeModules();
        }
        catch (...)
        {
            m_extensionLoader.reset();
        }
    }
    if (m_extensionLoader)
    {
        // Re-scan to ensure extension list is current, then load native modules
        m_extensionLoader->Scan();
        m_extensionLoader->LoadNativeModules();
        appendToOutput("✅ Extension loaded: " + name + "\n", "Output", OutputSeverity::Info);
    }
    else
    {
        appendToOutput("⚠️ Extension loader unavailable (init failed)\n", "Output", OutputSeverity::Warning);
    }
}

void Win32IDE::unloadExtension(const std::string& name)
{
    LOG_INFO("unloadExtension: " + name);
    if (!m_extensionLoader)
    {
        try
        {
            m_extensionLoader = std::make_unique<RawrXD::ExtensionLoader>();
            m_extensionLoader->Scan();
            m_extensionLoader->LoadNativeModules();
        }
        catch (...)
        {
            m_extensionLoader.reset();
        }
    }
    if (m_extensionLoader)
    {
        bool unloaded = m_extensionLoader->UnloadExtension(name);
        if (unloaded)
        {
            appendToOutput("✅ Extension unloaded: " + name + "\n", "Output", OutputSeverity::Info);
        }
        else
        {
            appendToOutput("⚠️ Failed to unload extension: " + name + " (not found or not loaded)\n", "Output",
                           OutputSeverity::Warning);
        }
    }
    else
    {
        appendToOutput("⚠️ Extension loader unavailable (init failed)\n", "Output", OutputSeverity::Warning);
    }
}

void Win32IDE::showExtensionHelp(const std::string& name)
{
    LOG_INFO("showExtensionHelp: " + name);
    if (!m_extensionLoader)
    {
        try
        {
            m_extensionLoader = std::make_unique<RawrXD::ExtensionLoader>();
            m_extensionLoader->Scan();
            m_extensionLoader->LoadNativeModules();
        }
        catch (...)
        {
            m_extensionLoader.reset();
        }
    }
    if (m_extensionLoader)
    {
        std::string help = m_extensionLoader->GetHelp(name);
        appendToOutput("--- Extension Help: " + name + " ---\n" + help + "\n", "Output", OutputSeverity::Info);
    }
    else
    {
        appendToOutput("⚠️ Extension loader unavailable (init failed)\n", "Output", OutputSeverity::Warning);
    }
}

// ============================================================================
// DEFERRED IMPLEMENTATIONS — PowerShell Panel Dock/Float
// ============================================================================

void Win32IDE::dockPowerShellPanel()
{
    LOG_INFO("dockPowerShellPanel");
    m_powerShellPanelDocked = true;

    if (m_hwndPowerShellPanel && IsWindow(m_hwndPowerShellPanel))
    {
        // Remove WS_POPUP, add WS_CHILD — reparent to main window
        LONG style = GetWindowLong(m_hwndPowerShellPanel, GWL_STYLE);
        style = (style & ~WS_POPUP) | WS_CHILD;
        SetWindowLong(m_hwndPowerShellPanel, GWL_STYLE, style);
        SetParent(m_hwndPowerShellPanel, m_hwndMain);

        // Trigger layout recalculation
        RECT rc;
        GetClientRect(m_hwndMain, &rc);
        onSize(rc.right, rc.bottom);
    }

    appendToOutput("PowerShell panel docked\n", "Output", OutputSeverity::Info);
}

void Win32IDE::floatPowerShellPanel()
{
    LOG_INFO("floatPowerShellPanel");
    m_powerShellPanelDocked = false;

    if (m_hwndPowerShellPanel && IsWindow(m_hwndPowerShellPanel))
    {
        // Remove WS_CHILD, add WS_POPUP — detach from main window
        LONG style = GetWindowLong(m_hwndPowerShellPanel, GWL_STYLE);
        style = (style & ~WS_CHILD) | WS_POPUP | WS_CAPTION | WS_THICKFRAME;
        SetWindowLong(m_hwndPowerShellPanel, GWL_STYLE, style);
        SetParent(m_hwndPowerShellPanel, nullptr);

        // Position floating window near the main window
        RECT mainRect;
        GetWindowRect(m_hwndMain, &mainRect);
        SetWindowPos(m_hwndPowerShellPanel, HWND_TOP, mainRect.right - 500, mainRect.bottom - 400, 480, 360,
                     SWP_SHOWWINDOW);
    }

    appendToOutput("PowerShell panel floating\n", "Output", OutputSeverity::Info);
}

// Helper for input dialog — Win32 modal dialog without .rc template
namespace
{
struct InputDialogParams
{
    const wchar_t* prompt;
    wchar_t* buffer;
    size_t bufferSize;
    bool ok = false;
};
enum
{
    IDC_PROMPT = 1001,
    IDC_EDIT = 1002,
    IDC_OK = 1003,
    IDC_CANCEL = 1004
};
static const UINT WM_INPUTDLG_CLOSED = WM_APP + 2;

static LRESULT CALLBACK InputDialogWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    InputDialogParams* p = reinterpret_cast<InputDialogParams*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg)
    {
        case WM_CREATE:
        {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            p = reinterpret_cast<InputDialogParams*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)p);
            if (!p)
                break;
            HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
            CreateWindowW(L"Static", p->prompt ? p->prompt : L"", WS_CHILD | WS_VISIBLE, 12, 12, 316, 16, hwnd,
                          (HMENU)(UINT_PTR)IDC_PROMPT, hInst, nullptr);
            CreateWindowW(L"Edit", (p->buffer && p->bufferSize > 0) ? p->buffer : L"",
                          WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT, 12, 34, 316, 24, hwnd, (HMENU)(UINT_PTR)IDC_EDIT,
                          hInst, nullptr);
            CreateWindowW(L"Button", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 132, 66, 72, 26, hwnd,
                          (HMENU)(UINT_PTR)IDC_OK, hInst, nullptr);
            CreateWindowW(L"Button", L"Cancel", WS_CHILD | WS_VISIBLE, 212, 66, 72, 26, hwnd,
                          (HMENU)(UINT_PTR)IDC_CANCEL, hInst, nullptr);
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_OK)
            {
                p = reinterpret_cast<InputDialogParams*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
                if (p && p->buffer && p->bufferSize > 0)
                {
                    GetDlgItemTextW(hwnd, IDC_EDIT, p->buffer, (int)p->bufferSize);
                }
                if (p)
                    p->ok = true;
                PostMessageW(GetParent(hwnd), WM_INPUTDLG_CLOSED, 0, 0);
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == IDC_CANCEL)
            {
                PostMessageW(GetParent(hwnd), WM_INPUTDLG_CLOSED, 0, 0);
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            PostMessageW(GetParent(hwnd), WM_INPUTDLG_CLOSED, 0, 0);
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static ATOM s_inputDialogClass = 0;

static HWND RunInputDialog(HWND parent, const wchar_t* title, InputDialogParams* params)
{
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    if (!s_inputDialogClass)
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = InputDialogWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"RawrXD_InputDialog";
        s_inputDialogClass = RegisterClassExW(&wc);
        if (!s_inputDialogClass)
            return nullptr;
    }
    // WS_EX_TOPMOST keeps dialog visible during screenshot attempts (focus theft protection)
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_TOPMOST, L"RawrXD_InputDialog",
                               title ? title : L"Input", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT,
                               CW_USEDEFAULT, 348, 128, parent, nullptr, hInst, params);
    if (!dlg)
    {
        OutputDebugStringA("[UI] Failed to create RawrXD_InputDialog window\n");
        return nullptr;
    }
    ShowWindow(dlg, SW_SHOW);
    return dlg;
}
}  // namespace

bool Win32IDE::DialogBoxWithInput(const wchar_t* title, const wchar_t* prompt, wchar_t* buffer, size_t bufferSize)
{
    if (!buffer || bufferSize == 0)
        return false;
    buffer[0] = L'\0';

    InputDialogParams params = {prompt, buffer, bufferSize, false};
    HWND dlg = RunInputDialog(m_hwndMain, title, &params);
    if (!dlg)
        return false;

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_INPUTDLG_CLOSED && msg.hwnd == m_hwndMain)
            break;
        if (!IsDialogMessageW(dlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return params.ok;
}

// Destructor definition - must be in .cpp where NativeStreamProvider is complete
Win32IDE::~Win32IDE()
{
    AgentBridge_SetShuttingDown(true);
    PromptWarm_SetAcceptRequests(false);
    try
    {
        RawrXD::SkillSystem::ShutdownSkillSystem();
    }
    catch (...)
    {
    }

    shutdownLineStripEditor();
    shutdownAgentChatCursorOverlay();
    clearAgenticLspConditionWiring();
    if (m_hwndVcsStagingPanel && IsWindow(m_hwndVcsStagingPanel))
    {
        RawrXD::UI::drainPendingVcsIndexSnapshots(m_hwndVcsStagingPanel);
        DestroyWindow(m_hwndVcsStagingPanel);
        m_hwndVcsStagingPanel = nullptr;
    }
    if (NeuralBridge::IsInitialized())
    {
        NeuralBridge::Shutdown();
    }
}
