// ============================================================================
// RawrXD IDE - DEBUGGER IMPLEMENTATION
// ============================================================================
// Full debugger with breakpoints, variables, call stack, watch expressions,
// and live integration with the NativeDebuggerEngine (DbgEng COM).
//
// Phase 13: Integration & Honesty — all execution-control, breakpoint,
// watch, variable, stack, and memory methods now route through the real
// DbgEng-backed NativeDebuggerEngine singleton instead of toggling booleans.
//
// UI creation code (createDebuggerUI) is unchanged — Win32 controls are real.
// ============================================================================

#include "../core/dap_client_service.h"
#include "../core/native_debugger_engine.h"
#include "../core/native_debugger_types.h"
#include "IDEConfig.h"
#include "Win32IDE.h"
#include "debugger_error_handler.h"
#include "debugger_frame_tracker.h"
#include "debugger_status_text.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <richedit.h>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>

using namespace RawrXD::Debugger;

namespace
{

namespace fs = std::filesystem;
using RawrXD::Debug::DapBreakpointSetResult;
using RawrXD::Debug::DapClientService;
using RawrXD::Debug::DapLaunchConfig;
using RawrXD::Debug::DapSourceBreakpoint;

struct DapLaunchSelection
{
    std::string name;
    std::string type;
    std::string request;
    std::string program;
    std::string cwd;
    std::vector<std::string> args;
    int processId = 0;
    bool noDebug = false;
    std::string adapterExecutable;
    std::vector<std::string> adapterArgs;
};

struct DapUiSession
{
    std::unique_ptr<DapClientService> service;
    std::string configName;
    std::string configType;
    std::string request;
    std::string adapterExecutable;
    std::string workspaceRoot;
    int currentThreadId = 1;
    int currentFrameId = -1;  // -1 = no valid frame yet; DAP frame 0 is a legitimate ID
    bool initializedSeen = false;
};

std::mutex g_dapSessionMutex;
std::unordered_map<Win32IDE*, std::shared_ptr<DapUiSession>> g_dapSessions;

std::shared_ptr<DapUiSession> getDapSession(Win32IDE* ide)
{
    std::lock_guard<std::mutex> lock(g_dapSessionMutex);
    auto it = g_dapSessions.find(ide);
    return it == g_dapSessions.end() ? nullptr : it->second;
}

void storeDapSession(Win32IDE* ide, std::shared_ptr<DapUiSession> session)
{
    std::lock_guard<std::mutex> lock(g_dapSessionMutex);
    if (session)
    {
        g_dapSessions[ide] = std::move(session);
    }
    else
    {
        g_dapSessions.erase(ide);
    }
}

std::string replaceAll(std::string value, const std::string& needle, const std::string& replacement)
{
    size_t pos = 0;
    while ((pos = value.find(needle, pos)) != std::string::npos)
    {
        value.replace(pos, needle.size(), replacement);
        pos += replacement.size();
    }
    return value;
}

// VS Code launch.json / tasks.json often use JSON with comments; strip // and /* */ outside strings.
std::string stripVSCodeJsonComments(std::string_view s)
{
    std::string out;
    out.reserve(s.size());
    bool inString = false;
    bool escape = false;
    for (size_t i = 0; i < s.size(); ++i)
    {
        const char c = s[i];
        if (escape)
        {
            out += c;
            escape = false;
            continue;
        }
        if (inString)
        {
            if (c == '\\')
            {
                out += c;
                escape = true;
            }
            else if (c == '"')
            {
                out += c;
                inString = false;
            }
            else
            {
                out += c;
            }
            continue;
        }
        if (c == '"')
        {
            out += c;
            inString = true;
            continue;
        }
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '/')
        {
            while (i < s.size() && s[i] != '\n' && s[i] != '\r')
            {
                ++i;
            }
            if (i < s.size())
            {
                out += s[i];
            }
            continue;
        }
        if (c == '/' && i + 1 < s.size() && s[i + 1] == '*')
        {
            i += 2;
            while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/'))
            {
                ++i;
            }
            if (i + 1 < s.size())
            {
                i += 1;
            }
            continue;
        }
        out += c;
    }
    return out;
}

std::string expandLaunchEnvVars(std::string value)
{
    for (;;)
    {
        const auto open = value.find("${env:");
        if (open == std::string::npos)
        {
            break;
        }
        const auto close = value.find('}', open);
        if (close == std::string::npos)
        {
            break;
        }
        const std::string key = value.substr(open + 7, close - open - 7);
        const char* ev = std::getenv(key.c_str());
        const std::string repl = ev ? std::string(ev) : std::string();
        value.replace(open, close - open + 1, repl);
    }
    return value;
}

std::string resolveLaunchValue(std::string value, const std::string& workspaceRoot, const std::string& currentFile)
{
    const fs::path currentPath(currentFile);
    value = replaceAll(std::move(value), "${workspaceFolder}", workspaceRoot);
    value = replaceAll(std::move(value), "${workspaceRoot}", workspaceRoot);
    value = replaceAll(std::move(value), "${file}", currentFile);
    value = replaceAll(std::move(value), "${fileDirname}",
                       currentPath.has_parent_path() ? currentPath.parent_path().string() : ".");
    value = replaceAll(std::move(value), "${fileBasename}", currentPath.filename().string());
    value = expandLaunchEnvVars(std::move(value));
    return value;
}

std::vector<std::string> parseStringArray(const nlohmann::json& value, const std::string& workspaceRoot,
                                          const std::string& currentFile)
{
    std::vector<std::string> out;
    if (!value.is_array())
    {
        return out;
    }

    for (const auto& item : value)
    {
        if (item.is_string())
        {
            out.push_back(resolveLaunchValue(item.get<std::string>(), workspaceRoot, currentFile));
        }
    }
    return out;
}

std::vector<nlohmann::json> extractLaunchConfigObjects(const std::string& raw)
{
    std::vector<nlohmann::json> configs;
    int depth = 0;
    size_t objectStart = std::string::npos;
    bool inString = false;
    bool escape = false;

    for (size_t i = 0; i < raw.size(); ++i)
    {
        const char ch = raw[i];

        if (escape)
        {
            escape = false;
            continue;
        }
        if (ch == '\\')
        {
            if (inString)
            {
                escape = true;
            }
            continue;
        }
        if (ch == '"')
        {
            inString = !inString;
            continue;
        }
        if (inString)
        {
            continue;
        }
        if (ch == '{')
        {
            if (depth == 0)
            {
                objectStart = i;
            }
            ++depth;
            continue;
        }
        if (ch == '}')
        {
            --depth;
            if (depth == 0 && objectStart != std::string::npos)
            {
                const std::string objectText = raw.substr(objectStart, i - objectStart + 1);
                const nlohmann::json parsed = nlohmann::json::parse(objectText, nullptr, false);
                if (!parsed.is_discarded() && parsed.is_object() && parsed.contains("type") &&
                    parsed.contains("request"))
                {
                    configs.push_back(parsed);
                }
                objectStart = std::string::npos;
            }
        }
    }

    return configs;
}

std::string normalizeDapSourcePath(const std::string& file, const std::string& workspaceRoot)
{
    if (file.empty())
    {
        return {};
    }

    std::error_code ec;
    fs::path sourcePath(file);
    if (sourcePath.is_relative() && !workspaceRoot.empty())
    {
        sourcePath = fs::path(workspaceRoot) / sourcePath;
    }

    sourcePath = sourcePath.lexically_normal();
    const fs::path canonicalPath = fs::weakly_canonical(sourcePath, ec);
    fs::path usePath = (!ec && !canonicalPath.empty()) ? canonicalPath : sourcePath;
    std::string out = usePath.string();

#ifdef _WIN32
    // Stabilize casing / spelling for adapters when the file exists (GetFinalPathNameByHandleW).
    auto utf8ToWide = [](const std::string& u8) -> std::wstring
    {
        if (u8.empty())
            return {};
        const int n = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0);
        if (n <= 0)
            return {};
        std::wstring ws(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, ws.data(), n);
        if (!ws.empty() && ws.back() == L'\0')
            ws.pop_back();
        return ws;
    };
    auto wideToUtf8 = [](const std::wstring& ws) -> std::string
    {
        if (ws.empty())
            return {};
        const int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (n <= 0)
            return {};
        std::string u8(static_cast<size_t>(n), '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, u8.data(), n, nullptr, nullptr);
        if (!u8.empty() && u8.back() == '\0')
            u8.pop_back();
        return u8;
    };

    const std::wstring wPath = utf8ToWide(out);
    if (!wPath.empty())
    {
        HANDLE h = CreateFileW(wPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE)
        {
            wchar_t buf[32768];
            const DWORD len = GetFinalPathNameByHandleW(h, buf, static_cast<DWORD>((sizeof(buf) / sizeof(buf[0])) - 1),
                                                        FILE_NAME_NORMALIZED);
            CloseHandle(h);
            if (len > 0 && len < (sizeof(buf) / sizeof(buf[0])))
            {
                std::wstring fp(buf, len);
                if (fp.rfind(L"\\\\?\\UNC\\", 0) == 0)
                {
                    fp = L"\\\\" + fp.substr(8);
                }
                else if (fp.rfind(L"\\\\?\\", 0) == 0)
                {
                    fp = fp.substr(4);
                }
                const std::string stable = wideToUtf8(fp);
                if (!stable.empty())
                {
                    out = stable;
                }
            }
        }
    }
#endif
    return out;
}

void applyDapHydrationToBreakpoints(std::vector<Breakpoint>& breakpoints, const std::string& workspaceRoot,
                                    const std::string& normalizedFile,
                                    const std::vector<DapSourceBreakpoint>& requested,
                                    const std::vector<DapBreakpointSetResult>& results, bool requestFailed)
{
    if (requestFailed)
    {
        for (auto& bp : breakpoints)
        {
            if (!bp.enabled || bp.line <= 0)
            {
                continue;
            }
            if (normalizeDapSourcePath(bp.file, workspaceRoot) != normalizedFile)
            {
                continue;
            }
            bp.dapVerified = false;
            if (bp.dapStatus.empty())
            {
                bp.dapStatus = "DAP setBreakpoints failed";
            }
        }
        return;
    }

    for (size_t i = 0; i < requested.size(); ++i)
    {
        const int line = (i < results.size() && results[i].line > 0) ? results[i].line : requested[i].line;
        const std::string& reqCond = requested[i].condition.value_or("");

        for (auto& bp : breakpoints)
        {
            if (!bp.enabled || bp.line <= 0)
            {
                continue;
            }
            if (normalizeDapSourcePath(bp.file, workspaceRoot) != normalizedFile)
            {
                continue;
            }
            if (bp.line != line || bp.condition != reqCond)
            {
                continue;
            }
            if (i < results.size())
            {
                bp.dapVerified = results[i].verified;
                bp.dapStatus = results[i].message;
            }
            else
            {
                bp.dapVerified = false;
                if (bp.dapStatus.empty())
                {
                    bp.dapStatus = "Missing DAP breakpoint result";
                }
            }
            break;
        }
    }
}

std::unordered_map<std::string, std::vector<DapSourceBreakpoint>> buildHydratedBreakpointsByFile(
    const std::vector<Breakpoint>& breakpoints, const std::string& workspaceRoot)
{
    std::unordered_map<std::string, std::vector<DapSourceBreakpoint>> byFile;
    std::unordered_map<std::string, std::set<std::pair<int, std::string>>> dedupe;

    for (const auto& bp : breakpoints)
    {
        if (!bp.enabled || bp.line <= 0)
        {
            continue;
        }

        const std::string normalizedFile = normalizeDapSourcePath(bp.file, workspaceRoot);
        if (normalizedFile.empty())
        {
            continue;
        }

        const auto dedupeKey = std::make_pair(bp.line, bp.condition);
        auto& seen = dedupe[normalizedFile];
        if (!seen.insert(dedupeKey).second)
        {
            continue;
        }

        DapSourceBreakpoint dapBp;
        dapBp.line = bp.line;
        if (!bp.condition.empty())
        {
            dapBp.condition = bp.condition;
        }
        byFile[normalizedFile].push_back(std::move(dapBp));
    }

    return byFile;
}

bool loadDapLaunchSelection(const std::string& workspaceRoot, const std::string& currentFile,
                            DapLaunchSelection& outSelection)
{
    if (workspaceRoot.empty())
    {
        return false;
    }

    const fs::path primary = fs::path(workspaceRoot) / ".vscode" / "launch.json";
    const fs::path fallback = fs::path(workspaceRoot) / ".rawrxd" / "launch.json";
    const fs::path launchPath = fs::exists(primary) ? primary : fallback;
    if (!fs::exists(launchPath))
    {
        return false;
    }

    std::ifstream in(launchPath, std::ios::binary);
    if (!in)
    {
        return false;
    }

    const std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::string jsonText = stripVSCodeJsonComments(raw);
    std::vector<nlohmann::json> configObjects;
    const nlohmann::json doc = nlohmann::json::parse(jsonText, nullptr, false);
    if (!doc.is_discarded() && doc.contains("configurations") && doc["configurations"].is_array())
    {
        for (const auto& cfg : doc["configurations"])
        {
            configObjects.push_back(cfg);
        }
    }
    else
    {
        configObjects = extractLaunchConfigObjects(jsonText);
    }

    if (configObjects.empty())
    {
        return false;
    }

    const auto chooseConfig = [&](const std::string& preferredRequest) -> const nlohmann::json*
    {
        for (const auto& cfg : configObjects)
        {
            if (!cfg.is_object())
            {
                continue;
            }
            const std::string type = cfg.value("type", "");
            const std::string request = cfg.value("request", "launch");
            if ((type == "cppdbg" || type == "cppvsdbg") && request == preferredRequest)
            {
                return &cfg;
            }
        }
        for (const auto& cfg : configObjects)
        {
            if (!cfg.is_object())
            {
                continue;
            }
            const std::string type = cfg.value("type", "");
            if (type == "cppdbg" || type == "cppvsdbg")
            {
                return &cfg;
            }
        }
        return nullptr;
    };

    const nlohmann::json* selectedConfig = chooseConfig("launch");
    if (!selectedConfig)
    {
        return false;
    }

    {
        const auto& cfg = *selectedConfig;
        if (!cfg.is_object())
        {
            return false;
        }

        const std::string type = cfg.value("type", "");
        const std::string request = cfg.value("request", "launch");
        if (type != "cppdbg" && type != "cppvsdbg")
        {
            return false;
        }

        DapLaunchSelection selection;
        selection.name = cfg.value("name", type);
        selection.type = type;
        selection.request = request;
        selection.noDebug = cfg.value("noDebug", false);
        selection.cwd = resolveLaunchValue(cfg.value("cwd", workspaceRoot), workspaceRoot, currentFile);
        selection.program = resolveLaunchValue(cfg.value("program", ""), workspaceRoot, currentFile);
        selection.args = parseStringArray(cfg.value("args", nlohmann::json::array()), workspaceRoot, currentFile);
        selection.adapterExecutable = resolveLaunchValue(
            cfg.value("adapterExecutable", cfg.value("debugAdapter", cfg.value("debugAdapterPath", ""))), workspaceRoot,
            currentFile);
        selection.adapterArgs =
            parseStringArray(cfg.value("adapterArgs", nlohmann::json::array()), workspaceRoot, currentFile);

        if (request == "attach")
        {
            if (cfg.contains("processId") && cfg["processId"].is_number_integer())
            {
                selection.processId = cfg["processId"].get<int>();
            }
        }

        outSelection = std::move(selection);
        return true;
    }
}

std::string findCpptoolsAdapter()
{
    char* userProfile = nullptr;
    size_t len = 0;
    if (_dupenv_s(&userProfile, &len, "USERPROFILE") != 0 || !userProfile || len == 0)
    {
        return {};
    }

    std::string result;
    try
    {
        const fs::path extensionRoot = fs::path(userProfile) / ".vscode" / "extensions";
        if (fs::exists(extensionRoot))
        {
            for (const auto& entry : fs::directory_iterator(extensionRoot))
            {
                if (!entry.is_directory())
                {
                    continue;
                }

                const std::string name = entry.path().filename().string();
                if (name.rfind("ms-vscode.cpptools", 0) != 0)
                {
                    continue;
                }

                const fs::path adapter = entry.path() / "debugAdapters" / "bin" / "OpenDebugAD7.exe";
                if (fs::exists(adapter))
                {
                    result = adapter.string();
                    break;
                }
            }
        }
    }
    catch (...)
    {
    }

    free(userProfile);
    return result;
}

std::string resolveAdapterExecutable(const DapLaunchSelection& selection)
{
    if (!selection.adapterExecutable.empty() && fs::exists(selection.adapterExecutable))
    {
        return selection.adapterExecutable;
    }
    if (selection.type == "cppdbg" || selection.type == "cppvsdbg")
    {
        return findCpptoolsAdapter();
    }
    return {};
}

char* makePostedDapEventPayload(const std::string& event, const nlohmann::json& body)
{
    nlohmann::json payload;
    payload["event"] = event;
    payload["body"] = body;
    const std::string serialized = payload.dump();
    return _strdup(serialized.c_str());
}

}  // namespace

// Debugger Control IDs
#define IDC_DEBUGGER_CONTAINER 2100
#define IDC_DEBUGGER_TOOLBAR 2101
#define IDC_DEBUGGER_TABS 2102
#define IDC_DEBUGGER_BTN_CONTINUE 2103
#define IDC_DEBUGGER_BTN_STEP_OVER 2104
#define IDC_DEBUGGER_BTN_STEP_INTO 2105
#define IDC_DEBUGGER_BTN_STEP_OUT 2106
#define IDC_DEBUGGER_BTN_STOP 2107
#define IDC_DEBUGGER_BTN_RESTART 2108
#define IDC_DEBUGGER_STATUS_TEXT 2109
#define IDC_DEBUGGER_BREAKPOINT_LIST 2110
#define IDC_DEBUGGER_WATCH_LIST 2111
#define IDC_DEBUGGER_VARIABLE_TREE 2112
#define IDC_DEBUGGER_STACK_LIST 2113
#define IDC_DEBUGGER_MEMORY 2114
#define IDC_DEBUGGER_INPUT 2115

// ============================================================================
// DEBUGGER UI CREATION
// ============================================================================

void Win32IDE::createDebuggerUI()
{
    if (!m_hwndMain)
        return;

    // Create main debugger container (at bottom, alongside terminal)
    m_hwndDebuggerContainer = CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC", "Debugger", WS_CHILD | WS_VISIBLE, 0, 0, 400,
                                              200, m_hwndMain, (HMENU)IDC_DEBUGGER_CONTAINER, m_hInstance, nullptr);

    if (!m_hwndDebuggerContainer)
        return;

    // Subclass the container so WM_NOTIFY from child list views reaches us
    SetWindowLongPtrA(m_hwndDebuggerContainer, GWLP_USERDATA, (LONG_PTR)this);
    m_oldDebuggerContainerProc =
        (WNDPROC)SetWindowLongPtrA(m_hwndDebuggerContainer, GWLP_WNDPROC, (LONG_PTR)DebuggerContainerProc);

    // Create toolbar with control buttons
    m_hwndDebuggerToolbar = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 0, 0, 400, 30,
                                            m_hwndDebuggerContainer, (HMENU)IDC_DEBUGGER_TOOLBAR, m_hInstance, nullptr);

    // Create toolbar buttons
    HWND btnContinue = CreateWindowExA(0, "BUTTON", "▶ Continue", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 5, 5, 80, 22,
                                       m_hwndDebuggerToolbar, (HMENU)IDC_DEBUGGER_BTN_CONTINUE, m_hInstance, nullptr);

    HWND btnStepOver = CreateWindowExA(0, "BUTTON", "⟿ Step Over", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 90, 5, 80, 22,
                                       m_hwndDebuggerToolbar, (HMENU)IDC_DEBUGGER_BTN_STEP_OVER, m_hInstance, nullptr);

    HWND btnStepInto =
        CreateWindowExA(0, "BUTTON", "↓ Step Into", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 175, 5, 80, 22,
                        m_hwndDebuggerToolbar, (HMENU)IDC_DEBUGGER_BTN_STEP_INTO, m_hInstance, nullptr);

    HWND btnStepOut = CreateWindowExA(0, "BUTTON", "↑ Step Out", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 260, 5, 75, 22,
                                      m_hwndDebuggerToolbar, (HMENU)IDC_DEBUGGER_BTN_STEP_OUT, m_hInstance, nullptr);

    HWND btnStop = CreateWindowExA(0, "BUTTON", "■ Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 340, 5, 55, 22,
                                   m_hwndDebuggerToolbar, (HMENU)IDC_DEBUGGER_BTN_STOP, m_hInstance, nullptr);

    // Create status text
    m_hwndDebuggerStatus =
        CreateWindowExA(0, "STATIC", "Debugger: Not Attached", WS_CHILD | WS_VISIBLE, 5, 35, 390, 20,
                        m_hwndDebuggerContainer, (HMENU)IDC_DEBUGGER_STATUS_TEXT, m_hInstance, nullptr);

    // Create tab control for different debugger views
    m_hwndDebuggerTabs =
        CreateWindowExA(0, WC_TABCONTROLA, "", WS_CHILD | WS_VISIBLE | TCS_TABS | TCS_FIXEDWIDTH, 5, 60, 390, 135,
                        m_hwndDebuggerContainer, (HMENU)IDC_DEBUGGER_TABS, m_hInstance, nullptr);

    // Add tabs
    TCITEMA tie;
    tie.mask = TCIF_TEXT;

    tie.pszText = const_cast<char*>("Breakpoints");
    TabCtrl_InsertItem(m_hwndDebuggerTabs, 0, &tie);

    tie.pszText = const_cast<char*>("Watch");
    TabCtrl_InsertItem(m_hwndDebuggerTabs, 1, &tie);

    tie.pszText = const_cast<char*>("Variables");
    TabCtrl_InsertItem(m_hwndDebuggerTabs, 2, &tie);

    tie.pszText = const_cast<char*>("Stack Trace");
    TabCtrl_InsertItem(m_hwndDebuggerTabs, 3, &tie);

    tie.pszText = const_cast<char*>("Memory");
    TabCtrl_InsertItem(m_hwndDebuggerTabs, 4, &tie);

    // Create tab content windows
    m_hwndDebuggerBreakpoints =
        CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL, 10, 85,
                        380, 100, m_hwndDebuggerContainer, (HMENU)IDC_DEBUGGER_BREAKPOINT_LIST, m_hInstance, nullptr);

    m_hwndDebuggerWatch =
        CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | LVS_REPORT | LVS_SINGLESEL, 10, 85, 380, 100,
                        m_hwndDebuggerContainer, (HMENU)IDC_DEBUGGER_WATCH_LIST, m_hInstance, nullptr);

    m_hwndDebuggerVariables = CreateWindowExA(
        WS_EX_CLIENTEDGE, WC_TREEVIEWA, "", WS_CHILD | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT, 10, 85, 380,
        100, m_hwndDebuggerContainer, (HMENU)IDC_DEBUGGER_VARIABLE_TREE, m_hInstance, nullptr);

    m_hwndDebuggerStackTrace =
        CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | LVS_REPORT | LVS_SINGLESEL, 10, 85, 380, 100,
                        m_hwndDebuggerContainer, (HMENU)IDC_DEBUGGER_STACK_LIST, m_hInstance, nullptr);

    m_hwndDebuggerMemory =
        CreateWindowExA(WS_EX_CLIENTEDGE, WC_EDITA, "", WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10, 85, 380,
                        100, m_hwndDebuggerContainer, (HMENU)IDC_DEBUGGER_MEMORY, m_hInstance, nullptr);

    // Setup list view columns
    LVCOLUMNA lvc;
    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    // Breakpoints columns
    lvc.iSubItem = 0;
    lvc.pszText = const_cast<char*>("File");
    lvc.cx = 150;
    lvc.fmt = LVCFMT_LEFT;
    ListView_InsertColumn(m_hwndDebuggerBreakpoints, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.pszText = const_cast<char*>("Line");
    lvc.cx = 60;
    ListView_InsertColumn(m_hwndDebuggerBreakpoints, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.pszText = const_cast<char*>("Hits");
    lvc.cx = 45;
    ListView_InsertColumn(m_hwndDebuggerBreakpoints, 2, &lvc);

    lvc.iSubItem = 3;
    lvc.pszText = const_cast<char*>("DAP");
    lvc.cx = 110;
    ListView_InsertColumn(m_hwndDebuggerBreakpoints, 3, &lvc);

    // Watch columns
    lvc.iSubItem = 0;
    lvc.pszText = const_cast<char*>("Expression");
    lvc.cx = 150;
    ListView_InsertColumn(m_hwndDebuggerWatch, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.pszText = const_cast<char*>("Value");
    lvc.cx = 150;
    ListView_InsertColumn(m_hwndDebuggerWatch, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.pszText = const_cast<char*>("Type");
    lvc.cx = 80;
    ListView_InsertColumn(m_hwndDebuggerWatch, 2, &lvc);

    // Stack trace columns
    lvc.iSubItem = 0;
    lvc.pszText = const_cast<char*>("Function");
    lvc.cx = 150;
    ListView_InsertColumn(m_hwndDebuggerStackTrace, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.pszText = const_cast<char*>("File");
    lvc.cx = 150;
    ListView_InsertColumn(m_hwndDebuggerStackTrace, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.pszText = const_cast<char*>("Line");
    lvc.cx = 60;
    ListView_InsertColumn(m_hwndDebuggerStackTrace, 2, &lvc);

    m_debuggerEnabled = true;
    updateDebuggerUI();
}

// ============================================================================
// DEBUGGER STATE MANAGEMENT
// ============================================================================

void Win32IDE::attachDebugger()
{
    METRICS.increment("debugger.attach_total");
    if (m_debuggerAttached)
        return;

    auto& errorHandler = DebuggerErrorHandlerInstance::instance();
    const std::weak_ptr<void> callbackToken = m_callbackLifetimeToken;

    errorHandler.setErrorCallback(
        [this, callbackToken](const DebuggerError& error, bool canRecover)
        {
            if (callbackToken.expired())
            {
                return;
            }

            OutputSeverity outputSeverity = OutputSeverity::Error;
            if (error.severity == ErrorSeverity::Info)
            {
                outputSeverity = OutputSeverity::Info;
            }
            else if (error.severity == ErrorSeverity::Warning)
            {
                outputSeverity = OutputSeverity::Warning;
            }

            std::string msg = canRecover ? "⚠️ Debugger recoverable error: " : "❌ Debugger error: ";
            msg += DebuggerError::errorTypeToString(error.type);
            if (!error.message.empty())
            {
                msg += " - ";
                msg += error.message;
            }
            if (!error.context.empty())
            {
                msg += " [";
                msg += error.context;
                msg += "]";
            }

            METRICS.increment("debugger_error_events_total");

            appendToOutput(msg, "Output", outputSeverity);

            // Marshal status-bar updates onto the UI thread.
            if (m_hwndMain && IsWindow(m_hwndMain))
            {
                auto* postedMessage = new (std::nothrow) std::string(msg);
                if (postedMessage)
                {
                    if (!PostMessageA(m_hwndMain, WM_DEBUGGER_ERROR_STATUS, static_cast<WPARAM>(canRecover ? 1 : 0),
                                      reinterpret_cast<LPARAM>(postedMessage)))
                    {
                        delete postedMessage;
                        updateDebuggerErrorStatus(msg);
                    }
                }
                else
                {
                    updateDebuggerErrorStatus(msg);
                }
            }
            else
            {
                updateDebuggerErrorStatus(msg);
            }
        });

    errorHandler.setRecoveryCallback(
        [this, callbackToken](DebuggerError& error)
        {
            if (callbackToken.expired())
            {
                return false;
            }

            // For ReattachAdapter and RestartSession, schedule an async reattach on the
            // UI thread via PostMessage so we don't call detach/attach re-entrantly from
            // inside a debug-engine or DAP callback thread.
            if (error.suggestedStrategy == RecoveryStrategy::ReattachAdapter ||
                error.suggestedStrategy == RecoveryStrategy::RestartSession)
            {
                const HWND hwnd = m_hwndMain;
                if (hwnd && IsWindow(hwnd))
                {
                    const auto now = std::chrono::steady_clock::now();
                    {
                        std::lock_guard<std::mutex> lock(m_debuggerMutex);
                        if (m_debuggerReattachPending.load())
                        {
                            return true;  // A reattach is already queued
                        }
                        if (m_lastDebuggerReattachRequestTime != std::chrono::steady_clock::time_point{})
                        {
                            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                       now - m_lastDebuggerReattachRequestTime)
                                                       .count();
                            if (elapsedMs < REATTACH_COOLDOWN_MS)
                            {
                                return true;  // Debounce bursty identical recovery triggers
                            }
                        }
                        m_debuggerReattachPending.store(true);
                        m_lastDebuggerReattachRequestTime = now;
                    }

                    std::string msg = "🔧 Debugger recovery: scheduling reattach for: ";
                    msg += DebuggerError::errorTypeToString(error.type);
                    appendToOutput(msg, "Output", OutputSeverity::Warning);
                    if (!PostMessageA(hwnd, WM_DEBUGGER_REATTACH_SAFE, 0, 0))
                    {
                        {
                            std::lock_guard<std::mutex> lock(m_debuggerMutex);
                            m_debuggerReattachPending.store(false);
                        }
                        appendToOutput("⚠️ Debugger recovery reattach post failed", "Output", OutputSeverity::Warning);
                        return false;
                    }
                    return true;  // Mark as handled; reattach fires on UI thread
                }
                return false;
            }

            std::string msg = "🔧 Debugger recovery attempt: ";
            msg += DebuggerError::errorTypeToString(error.type);
            if (!error.context.empty())
            {
                msg += " [";
                msg += error.context;
                msg += "]";
            }
            appendToOutput(msg, "Output", OutputSeverity::Debug);

            // Return false so default recovery strategy still executes for other types.
            return false;
        });

    // Wire the frame context callback: called by DebuggerFrameTracker::selectFrame()
    // whenever the selected frame changes programmatically (e.g., from navigate back/forward).
    // Keeps source highlight and DAP currentFrameId in sync without refetching frames.
    DebuggerFrameTrackerInstance::instance().setFrameContextCallback(
        [this, callbackToken](const EnhancedStackFrame& frame)
        {
            if (callbackToken.expired())
                return;
            if (!frame.file.empty() && frame.line > 0)
            {
                m_debuggerCurrentFile = frame.file;
                m_debuggerCurrentLine = frame.line;
                highlightDebuggerLine(frame.file, frame.line);
            }
            if (auto dapSession = getDapSession(this))
            {
                if (frame.dapFrameId >= 0)
                {
                    dapSession->currentFrameId = frame.dapFrameId;
                }
            }
        });

    DebuggerFrameTrackerInstance::instance().setErrorReportCallback(
        [callbackToken](DebuggerErrorType type, ErrorSeverity severity, const std::string& message,
                        const std::string& context)
        {
            if (callbackToken.expired())
                return;
            DebuggerErrorHandlerInstance::instance().reportError(type, severity, message, context);
        });

    const std::string workspaceRoot = !m_projectRoot.empty()
                                          ? m_projectRoot
                                          : (!m_currentDirectory.empty() ? m_currentDirectory : m_explorerRootPath);

    DapLaunchSelection dapSelection;
    if (loadDapLaunchSelection(workspaceRoot, m_currentFile, dapSelection))
    {
        const std::string adapterExecutable = resolveAdapterExecutable(dapSelection);
        if (!adapterExecutable.empty())
        {
            auto dapSession = std::make_shared<DapUiSession>();
            dapSession->service = std::make_unique<DapClientService>();
            dapSession->configName = dapSelection.name;
            dapSession->configType = dapSelection.type;
            dapSession->request = dapSelection.request;
            dapSession->adapterExecutable = adapterExecutable;
            dapSession->workspaceRoot = workspaceRoot;

            const std::weak_ptr<void> callbackToken = m_callbackLifetimeToken;
            const HWND hwndMain = m_hwndMain;
            dapSession->service->setEventCallback(
                [callbackToken, hwndMain](const std::string& event, const nlohmann::json& body)
                {
                    if (callbackToken.expired() || !hwndMain || !IsWindow(hwndMain))
                    {
                        return;
                    }

                    char* payload = makePostedDapEventPayload(event, body);
                    if (payload)
                    {
                        PostMessageA(hwndMain, WM_DAP_EVENT_SAFE, 0, reinterpret_cast<LPARAM>(payload));
                    }
                });

            const bool adapterStarted = dapSession->service->startAdapter(adapterExecutable, dapSelection.adapterArgs);
            if (!adapterStarted)
            {
                appendToOutput("❌ DAP adapter failed to start: " + adapterExecutable, "Output", OutputSeverity::Error);
            }

            const bool initialized = adapterStarted && dapSession->service->initialize(
                                                           "RawrXD IDE", workspaceRoot,
                                                           dapSelection.type.empty() ? "cppdbg" : dapSelection.type);
            if (adapterStarted && !initialized)
            {
                appendToOutput("❌ DAP initialize request failed or timed out (20s)", "Output", OutputSeverity::Error);
            }

            if (initialized)
            {
                bool started = false;
                if (dapSelection.request == "attach")
                {
                    started = dapSelection.processId > 0 && dapSession->service->attach(dapSelection.processId);
                    if (!started)
                    {
                        appendToOutput("❌ DAP attach failed (processId=" + std::to_string(dapSelection.processId) +
                                           ")",
                                       "Output", OutputSeverity::Error);
                    }
                }
                else
                {
                    DapLaunchConfig cfg;
                    cfg.program = dapSelection.program;
                    cfg.cwd = dapSelection.cwd;
                    cfg.args = dapSelection.args;
                    cfg.request = dapSelection.request.empty() ? "launch" : dapSelection.request;
                    cfg.noDebug = dapSelection.noDebug;
                    started = !cfg.program.empty() && dapSession->service->launch(cfg);
                    if (!started)
                    {
                        appendToOutput("❌ DAP launch failed for program: " + cfg.program, "Output",
                                       OutputSeverity::Error);
                    }
                }

                if (started)
                {
                    storeDapSession(this, dapSession);

                    // DAP configuration phase (after launch/attach, before configurationDone):
                    // initialize → launch|attach → setBreakpoints (all sources) → setExceptionBreakpoints
                    // → surface unverified bindings → configurationDone → adapter may continue the debuggee.
                    METRICS.increment("debugger.breakpoint_hydration_attempts");
                    for (auto& bp : m_breakpoints)
                    {
                        bp.dapVerified = std::nullopt;
                        bp.dapStatus.clear();
                    }
                    const auto byFile = buildHydratedBreakpointsByFile(m_breakpoints, workspaceRoot);
                    std::unordered_set<std::string> pathsToSync;
                    for (const auto& kv : byFile)
                    {
                        pathsToSync.insert(kv.first);
                    }
                    {
                        std::lock_guard<std::mutex> lock(m_debuggerMutex);
                        for (const auto& p : m_dapBreakpointDirtyNormalizedPaths)
                        {
                            pathsToSync.insert(p);
                        }
                        m_dapBreakpointDirtyNormalizedPaths.clear();
                    }
                    std::vector<std::string> orderedPaths(pathsToSync.begin(), pathsToSync.end());
                    std::sort(orderedPaths.begin(), orderedPaths.end());
                    bool breakpointSyncOk = true;
                    for (const auto& path : orderedPaths)
                    {
                        std::vector<DapSourceBreakpoint> bps;
                        if (const auto it = byFile.find(path); it != byFile.end())
                        {
                            bps = it->second;
                        }
                        std::vector<DapBreakpointSetResult> binding;
                        const bool ok = dapSession->service->setBreakpoints(path, bps, nullptr, &binding);
                        applyDapHydrationToBreakpoints(m_breakpoints, workspaceRoot, path, bps, binding, !ok);
                        if (!ok)
                        {
                            breakpointSyncOk = false;
                            appendToOutput("⚠️ DAP breakpoint sync failed for: " + path, "Output",
                                           OutputSeverity::Warning);
                        }
                    }
                    if (breakpointSyncOk)
                    {
                        METRICS.increment("debugger.breakpoint_hydration_success");
                    }
                    else
                    {
                        METRICS.increment("debugger.breakpoint_hydration_failures");
                    }

                    updateBreakpointList();

                    // --- Configure exception breakpoints ---
                    // Enable common exception filters that most DAP adapters support
                    // These catch first/second-chance exceptions and user-unhandled exceptions
                    bool exceptionBreakpointOk = true;
                    try
                    {
                        std::vector<std::string> exceptionFilters = {"cpp_throw", "cpp_catch", "user_unhandled"};
                        if (!dapSession->service->setExceptionBreakpoints(exceptionFilters))
                        {
                            exceptionBreakpointOk = false;
                            appendToOutput("⚠️ DAP setExceptionBreakpoints partially failed or not supported", "Output",
                                           OutputSeverity::Debug);
                        }
                        else
                        {
                            appendToOutput("✅ Exception breakpoints configured (cpp_throw, cpp_catch, user_unhandled)",
                                           "Output", OutputSeverity::Debug);
                        }
                    }
                    catch (const std::exception& ex)
                    {
                        appendToOutput(std::string("⚠️ DAP exception breakpoint config failed: ") + ex.what(), "Output",
                                       OutputSeverity::Warning);
                        exceptionBreakpointOk = false;
                    }
                    catch (...)
                    {
                        appendToOutput("❌ DAP exception breakpoint config: unknown error", "Output",
                                       OutputSeverity::Error);
                        exceptionBreakpointOk = false;
                    }

                    updateBreakpointList();
                    refreshDapUnverifiedBreakpointUi(true);

                    const bool configurationDoneOk = dapSession->service->configurationDone();

                    if (!breakpointSyncOk || !configurationDoneOk)
                    {
                        appendToOutput("⚠️ DAP startup incomplete (breakpoint sync/configurationDone failed) — falling "
                                       "back to native debugger",
                                       "Output", OutputSeverity::Warning);
                    }
                    else
                    {
                        m_debuggerAttached = true;
                        m_debuggerPaused = false;
                        size_t unverifiedDapBp = 0;
                        {
                            std::lock_guard<std::mutex> lock(m_debuggerMutex);
                            for (const auto& bp : m_breakpoints)
                            {
                                if (!bp.enabled)
                                {
                                    continue;
                                }
                                if (bp.dapVerified.has_value() && !*bp.dapVerified)
                                {
                                    ++unverifiedDapBp;
                                }
                            }
                        }
                        std::string status = "✅ Debugger Attached | DAP: " + dapSelection.name;
                        if (unverifiedDapBp > 0)
                        {
                            status += " | ⚠ ";
                            status += std::to_string(unverifiedDapBp);
                            status += " BP unverified";
                        }
                        SetWindowTextA(m_hwndDebuggerStatus, status.c_str());
                        appendToOutput("🔍 Debugger attached — DAP session active", "Output", OutputSeverity::Info);
                        updateDebuggerUI();
                        return;
                    }

                    storeDapSession(this, nullptr);
                }

                dapSession->service->stopAdapter();
            }

            appendToOutput(
                "⚠️ DAP startup failed for current launch.json configuration — falling back to native debugger",
                "Output", OutputSeverity::Warning);
        }
    }

    auto& engine = NativeDebuggerEngine::Instance();

    // Ensure engine is initialized (Phase 12 initPhase12 should have done this,
    // but guard against late startup)
    if (!engine.isInitialized())
    {
        DebugConfig config;
        config.breakOnEntry = true;
        config.autoLoadSymbols = true;
        config.enableSourceStepping = true;
        config.maxEventHistory = 10000;
        config.symbolPath = "srv*C:\\Symbols*https://msdl.microsoft.com/download/symbols";
        DebugResult initR = engine.initialize(config);
        if (!initR.success)
        {
            std::string err = "❌ Debugger engine init failed: ";
            err += initR.detail;
            SetWindowTextA(m_hwndDebuggerStatus, err.c_str());
            appendToOutput(err, "Output", OutputSeverity::Error);
            return;
        }
    }

    // If we have a current file open, try to launch it as target.
    // For C++ projects the user would have built an .exe; use the build output path.
    // If no target is available, we still mark attached so user can later attach by PID.
    DebugResult attachR = DebugResult::ok("Debugger ready — use Launch or Attach to PID");

    // Attempt to launch if we have a compiled binary path
    if (!m_currentFile.empty())
    {
        // Derive the expected .exe from the project build dir
        std::string exePath;
        // Check if current file IS an exe
        if (m_currentFile.size() > 4 && m_currentFile.substr(m_currentFile.size() - 4) == ".exe")
        {
            exePath = m_currentFile;
        }
        if (!exePath.empty())
        {
            attachR = engine.launchProcess(exePath);
            if (attachR.success)
            {
                setCurrentBinaryForReverseEngineering(exePath);
                appendToOutput("[RE] Binary set for analysis (Reverse Engineering menu).\n", "Output",
                               OutputSeverity::Info);
            }
            else
            {
                std::string warn = "⚠️ Launch failed: ";
                warn += attachR.detail;
                warn += " — attach by PID instead";
                appendToOutput(warn, "Output", OutputSeverity::Warning);
                // Fall through — still mark debugger as attached in ready state
            }
        }
    }

    m_debuggerAttached = true;
    m_debuggerPaused = false;

    // Register callbacks so engine events flow back to UI.
    // Lifetime safety is enforced by clearing callbacks during detach/destruction.
    engine.setBreakpointHitCallback(
        [](const NativeBreakpoint* bp, const RegisterSnapshot* regs, void* userData)
        {
            auto* ide = static_cast<Win32IDE*>(userData);
            if (ide && bp)
            {
                ide->onDebuggerBreakpoint(bp->sourceFile, bp->sourceLine);
            }
        },
        this);

    engine.setOutputCallback(
        [](const char* text, uint32_t /*category*/, void* userData)
        {
            auto* ide = static_cast<Win32IDE*>(userData);
            if (ide && text)
            {
                ide->onDebuggerOutput(text);
            }
        },
        this);

    engine.setStateCallback(
        [](DebugSessionState newState, void* userData)
        {
            auto* ide = static_cast<Win32IDE*>(userData);
            if (!ide)
                return;
            switch (newState)
            {
                case DebugSessionState::Running:
                case DebugSessionState::Stepping:
                    ide->onDebuggerContinued();
                    break;
                case DebugSessionState::Broken:
                    // Engine broke — start a fresh paused-state epoch.
                    ide->m_debuggerPaused = true;
                    DebuggerFrameTrackerInstance::instance().clear();
                    ide->m_lastDroppedFrameCount.store(0);
                    {
                        std::lock_guard<std::mutex> lock(ide->m_debuggerMutex);
                        ide->m_callStack.clear();
                        ide->m_localVariables.clear();
                        ide->m_selectedStackFrameIndex = 0;
                        ide->m_pendingEvalRequests.clear();
                        ide->m_lastFrameUpdateTime = std::chrono::steady_clock::now();
                        ide->m_lastFrameRefreshAttemptTime = std::chrono::steady_clock::time_point{};
                        ide->m_debuggerCurrentFile.clear();
                        ide->m_debuggerCurrentLine = -1;
                    }
                    SetWindowTextA(ide->m_hwndDebuggerStatus, "⏸ Break — target paused");
                    ide->clearDebuggerHighlight();
                    ide->updateVariables();
                    ide->updateCallStack();
                    ide->updateMemoryView();
                    break;
                case DebugSessionState::Terminated:
                case DebugSessionState::Idle:
                    ide->onDebuggerTerminated();
                    break;
                case DebugSessionState::Error:
                    SetWindowTextA(ide->m_hwndDebuggerStatus, "❌ Engine error");
                    ide->appendToOutput("❌ Debug engine entered error state", "Output", OutputSeverity::Error);
                    break;
                default:
                    break;
            }
        },
        this);

    std::string status = "✅ Debugger Attached | Engine: ";
    status += attachR.detail;
    SetWindowTextA(m_hwndDebuggerStatus, status.c_str());
    appendToOutput("🔍 Debugger attached — NativeDebuggerEngine active", "Output", OutputSeverity::Info);
}

void Win32IDE::detachDebugger()
{
    if (!m_debuggerAttached)
        return;

    if (auto dapSession = getDapSession(this))
    {
        // Send proper DAP disconnect request before stopping adapter
        try
        {
            if (!dapSession->service->disconnect(true, false))
            {
                appendToOutput("⚠️ DAP disconnect request failed or not supported by adapter", "Output",
                               OutputSeverity::Debug);
            }
        }
        catch (const std::exception& ex)
        {
            appendToOutput(std::string("⚠️ DAP disconnect error: ") + ex.what(), "Output", OutputSeverity::Debug);
        }
        catch (...)
        {
            appendToOutput("⚠️ DAP disconnect: unknown error", "Output", OutputSeverity::Debug);
        }

        try
        {
            dapSession->service->stopAdapter();
        }
        catch (const std::exception& ex)
        {
            appendToOutput(std::string("⚠️ DAP adapter stop error: ") + ex.what(), "Output", OutputSeverity::Warning);
        }
        catch (...)
        {
            appendToOutput("⚠️ DAP adapter stop failed", "Output", OutputSeverity::Warning);
        }

        storeDapSession(this, nullptr);
        clearDapBreakpointBindingOnAll();

        m_debuggerAttached = false;
        m_debuggerPaused = false;
        m_callStack.clear();
        m_localVariables.clear();
        {
            std::lock_guard<std::mutex> lock(m_debuggerMutex);
            m_pendingEvalRequests.clear();
            m_lastFrameUpdateTime = std::chrono::steady_clock::time_point{};
            m_lastFrameRefreshAttemptTime = std::chrono::steady_clock::time_point{};
            m_debuggerReattachPending.store(false);
            m_lastDebuggerReattachRequestTime = std::chrono::steady_clock::time_point{};
        }
        m_debuggerCurrentFile.clear();
        m_debuggerCurrentLine = -1;
        m_lastDroppedFrameCount.store(0);

        // Reset frame tracker and error handler for clean next session
        DebuggerFrameTrackerInstance::instance().clear();
        DebuggerFrameTrackerInstance::instance().setFrameContextCallback(nullptr);
        DebuggerFrameTrackerInstance::instance().setErrorReportCallback(nullptr);
        auto& errorHandler = DebuggerErrorHandlerInstance::instance();
        errorHandler.setErrorCallback(nullptr);
        errorHandler.setRecoveryCallback(nullptr);
        errorHandler.clear();

        SetWindowTextA(m_hwndDebuggerStatus, "⏹ DAP Debugger Detached");
        appendToOutput("🔍 Debugger detached — DAP session closed gracefully", "Output", OutputSeverity::Info);
        updateDebuggerUI();
        return;
    }

    auto& engine = NativeDebuggerEngine::Instance();

    // Detach from target process via DbgEng
    DebugResult r = engine.detach();
    if (!r.success)
    {
        std::string warn = "⚠️ Engine detach note: ";
        warn += r.detail;
        appendToOutput(warn, "Output", OutputSeverity::Warning);
    }

    // Clear callbacks to prevent dangling pointers
    engine.setBreakpointHitCallback(nullptr, nullptr);
    engine.setOutputCallback(nullptr, nullptr);
    engine.setStateCallback(nullptr, nullptr);

    m_debuggerAttached = false;
    m_debuggerPaused = false;
    m_callStack.clear();
    m_localVariables.clear();
    {
        std::lock_guard<std::mutex> lock(m_debuggerMutex);
        m_pendingEvalRequests.clear();
        m_lastFrameUpdateTime = std::chrono::steady_clock::time_point{};
        m_lastFrameRefreshAttemptTime = std::chrono::steady_clock::time_point{};
        m_debuggerReattachPending.store(false);
        m_lastDebuggerReattachRequestTime = std::chrono::steady_clock::time_point{};
    }
    m_debuggerCurrentFile.clear();
    m_debuggerCurrentLine = -1;
    m_lastDroppedFrameCount.store(0);

    // Reset frame tracker and error handler for clean next session
    DebuggerFrameTrackerInstance::instance().clear();
    DebuggerFrameTrackerInstance::instance().setFrameContextCallback(nullptr);
    DebuggerFrameTrackerInstance::instance().setErrorReportCallback(nullptr);
    auto& errorHandler = DebuggerErrorHandlerInstance::instance();
    errorHandler.setErrorCallback(nullptr);
    errorHandler.setRecoveryCallback(nullptr);
    errorHandler.clear();

    std::string status = "⏹ Debugger Detached";
    SetWindowTextA(m_hwndDebuggerStatus, status.c_str());
    appendToOutput("🔍 Debugger detached — engine released", "Output", OutputSeverity::Info);

    updateDebuggerUI();
}

void Win32IDE::pauseExecution()
{
    if (!m_debuggerAttached || m_debuggerPaused)
        return;

    if (auto dapSession = getDapSession(this))
    {
        if (!dapSession->service->pause(dapSession->currentThreadId))
        {
            appendToOutput("❌ DAP pause failed", "Output", OutputSeverity::Error);
            return;
        }
        SetWindowTextA(m_hwndDebuggerStatus, "⏸ DAP pause requested");
        return;
    }

    auto& engine = NativeDebuggerEngine::Instance();
    DebugResult r = engine.breakExecution();
    if (!r.success)
    {
        std::string err = "❌ Break failed: ";
        err += r.detail;
        SetWindowTextA(m_hwndDebuggerStatus, err.c_str());
        appendToOutput(err, "Output", OutputSeverity::Error);
        return;
    }

    m_debuggerPaused = true;
    SetWindowTextA(m_hwndDebuggerStatus, "⏸ Debugger Paused — Execution halted via DbgEng");
    appendToOutput("⏸ Execution paused (DebugBreakProcess)", "Output", OutputSeverity::Info);

    // Refresh all inspection panels with real data from the engine
    updateVariables();
    updateCallStack();
    updateMemoryView();
    updateDebuggerUI();
}

void Win32IDE::resumeExecution()
{
    if (!m_debuggerAttached || !m_debuggerPaused)
        return;

    if (auto dapSession = getDapSession(this))
    {
        if (!dapSession->service->continueExec(dapSession->currentThreadId))
        {
            appendToOutput("❌ DAP continue failed", "Output", OutputSeverity::Error);
            return;
        }
        m_debuggerPaused = false;
        SetWindowTextA(m_hwndDebuggerStatus, "▶ DAP target running");
        clearDebuggerHighlight();
        updateDebuggerUI();
        return;
    }

    auto& engine = NativeDebuggerEngine::Instance();
    DebugResult r = engine.go();
    if (!r.success)
    {
        std::string err = "❌ Continue failed: ";
        err += r.detail;
        SetWindowTextA(m_hwndDebuggerStatus, err.c_str());
        appendToOutput(err, "Output", OutputSeverity::Error);
        return;
    }

    m_debuggerPaused = false;
    SetWindowTextA(m_hwndDebuggerStatus, "▶ Debugger Running — target resumed");
    appendToOutput("▶ Execution resumed (IDebugControl::SetExecutionStatus)", "Output", OutputSeverity::Info);

    clearDebuggerHighlight();
    updateDebuggerUI();
}

void Win32IDE::stepOverExecution()
{
    if (!m_debuggerAttached)
        return;

    if (auto dapSession = getDapSession(this))
    {
        if (!dapSession->service->next(dapSession->currentThreadId))
        {
            appendToOutput("❌ DAP step over failed", "Output", OutputSeverity::Error);
            return;
        }
        dapSession->currentFrameId = -1;  // Stale until stopped event fires
        m_debuggerPaused = false;
        SetWindowTextA(m_hwndDebuggerStatus, "⟿ DAP stepping...");
        return;
    }

    auto& engine = NativeDebuggerEngine::Instance();
    DebugResult r = engine.stepOver();
    if (!r.success)
    {
        std::string err = "❌ Step Over failed: ";
        err += r.detail;
        appendToOutput(err, "Output", OutputSeverity::Error);
        return;
    }

    onDebuggerContinued();
    SetWindowTextA(m_hwndDebuggerStatus, "⟿ Stepping over...");
    appendToOutput("⟿ Step Over executed (DEBUG_STATUS_STEP_OVER)", "Output", OutputSeverity::Debug);
}

void Win32IDE::stepIntoExecution()
{
    if (!m_debuggerAttached)
        return;

    if (auto dapSession = getDapSession(this))
    {
        if (!dapSession->service->stepIn(dapSession->currentThreadId))
        {
            appendToOutput("❌ DAP step into failed", "Output", OutputSeverity::Error);
            return;
        }
        dapSession->currentFrameId = -1;  // Stale until stopped event fires
        m_debuggerPaused = false;
        SetWindowTextA(m_hwndDebuggerStatus, "↓ DAP stepping in...");
        return;
    }

    auto& engine = NativeDebuggerEngine::Instance();
    DebugResult r = engine.stepInto();
    if (!r.success)
    {
        std::string err = "❌ Step Into failed: ";
        err += r.detail;
        appendToOutput(err, "Output", OutputSeverity::Error);
        return;
    }

    onDebuggerContinued();
    SetWindowTextA(m_hwndDebuggerStatus, "↓ Stepping into...");
    appendToOutput("↓ Step Into executed (DEBUG_STATUS_STEP_INTO)", "Output", OutputSeverity::Debug);
}

void Win32IDE::stepOutExecution()
{
    if (!m_debuggerAttached)
        return;

    if (auto dapSession = getDapSession(this))
    {
        if (!dapSession->service->stepOut(dapSession->currentThreadId))
        {
            appendToOutput("❌ DAP step out failed", "Output", OutputSeverity::Error);
            return;
        }
        dapSession->currentFrameId = -1;  // Stale until stopped event fires
        m_debuggerPaused = false;
        SetWindowTextA(m_hwndDebuggerStatus, "↑ DAP stepping out...");
        return;
    }

    auto& engine = NativeDebuggerEngine::Instance();
    DebugResult r = engine.stepOut();
    if (!r.success)
    {
        std::string err = "❌ Step Out failed: ";
        err += r.detail;
        appendToOutput(err, "Output", OutputSeverity::Error);
        return;
    }

    onDebuggerContinued();
    SetWindowTextA(m_hwndDebuggerStatus, "↑ Stepping out...");
    appendToOutput("↑ Step Out executed (DEBUG_STATUS_STEP_BRANCH)", "Output", OutputSeverity::Debug);
}

void Win32IDE::stopDebugger()
{
    if (!m_debuggerAttached)
        return;

    if (auto dapSession = getDapSession(this))
    {
        dapSession->service->stopAdapter();
        storeDapSession(this, nullptr);
        clearDapBreakpointBindingOnAll();
        onDebuggerTerminated();
        SetWindowTextA(m_hwndDebuggerStatus, "⏹ DAP Debugger Stopped");
        return;
    }

    auto& engine = NativeDebuggerEngine::Instance();

    // Terminate the target process via DbgEng before detaching
    DebugResult r = engine.terminateTarget();
    if (!r.success)
    {
        std::string warn = "⚠️ Terminate target note: ";
        warn += r.detail;
        appendToOutput(warn, "Output", OutputSeverity::Warning);
    }

    detachDebugger();
    SetWindowTextA(m_hwndDebuggerStatus, "⏹ Debugger Stopped — target terminated");
}

void Win32IDE::restartDebugger()
{
    // Remember the target path before stopping
    auto& engine = NativeDebuggerEngine::Instance();
    std::string previousTarget = engine.getTargetName();
    uint32_t previousPID = engine.getTargetPID();

    stopDebugger();
    attachDebugger();

    // If we had a target, relaunch it
    if (!previousTarget.empty())
    {
        DebugResult r = engine.launchProcess(previousTarget);
        if (r.success)
        {
            std::string msg = "🔄 Debugger Restarted — relaunched: " + previousTarget;
            SetWindowTextA(m_hwndDebuggerStatus, msg.c_str());
            appendToOutput(msg, "Output", OutputSeverity::Info);
        }
        else
        {
            std::string msg = "🔄 Debugger Restarted — relaunch failed: ";
            msg += r.detail;
            SetWindowTextA(m_hwndDebuggerStatus, msg.c_str());
            appendToOutput(msg, "Output", OutputSeverity::Warning);
        }
    }
    else
    {
        SetWindowTextA(m_hwndDebuggerStatus, "🔄 Debugger Restarted — ready");
    }
}

void Win32IDE::handleDebuggerToolbarCommand(int commandId)
{
    switch (commandId)
    {
        case IDC_DEBUGGER_BTN_CONTINUE:
            resumeExecution();
            break;
        case IDC_DEBUGGER_BTN_STEP_OVER:
            stepOverExecution();
            break;
        case IDC_DEBUGGER_BTN_STEP_INTO:
            stepIntoExecution();
            break;
        case IDC_DEBUGGER_BTN_STEP_OUT:
            stepOutExecution();
            break;
        case IDC_DEBUGGER_BTN_STOP:
            stopDebugger();
            break;
        case IDC_DEBUGGER_BTN_RESTART:
            restartDebugger();
            break;
        default:
            break;
    }
}

// ============================================================================
// BREAKPOINT MANAGEMENT
// ============================================================================

void Win32IDE::markDapBreakpointDirtyForLogicalFile(const std::string& file)
{
    const std::string workspaceRoot = !m_projectRoot.empty()
                                          ? m_projectRoot
                                          : (!m_currentDirectory.empty() ? m_currentDirectory : m_explorerRootPath);
    const std::string n = normalizeDapSourcePath(file, workspaceRoot);
    if (n.empty())
    {
        return;
    }
    std::lock_guard<std::mutex> lock(m_debuggerMutex);
    m_dapBreakpointDirtyNormalizedPaths.insert(n);
}

void Win32IDE::refreshDapUnverifiedBreakpointUi(bool reportToErrorHandler)
{
    if (!getDapSession(this))
    {
        return;
    }
    size_t unverified = 0;
    std::string exampleLoc;
    {
        std::lock_guard<std::mutex> lock(m_debuggerMutex);
        for (const auto& bp : m_breakpoints)
        {
            if (!bp.enabled)
            {
                continue;
            }
            if (bp.dapVerified.has_value() && !*bp.dapVerified)
            {
                ++unverified;
                if (exampleLoc.empty())
                {
                    exampleLoc = bp.file + ":" + std::to_string(bp.line);
                }
            }
        }
    }
    if (unverified == 0)
    {
        return;
    }
    std::string msg = "DAP: " + std::to_string(unverified) + " breakpoint(s) not verified — target may not stop there";
    if (!exampleLoc.empty())
    {
        msg += " (e.g. ";
        msg += exampleLoc;
        msg += ")";
    }
    appendToOutput(std::string("⚠ ") + msg, "Output", OutputSeverity::Warning);
    updateDebuggerErrorStatus(msg);
    if (reportToErrorHandler)
    {
        DebuggerErrorHandlerInstance::instance().reportError(DebuggerErrorType::BreakpointLocationInvalid,
                                                             ErrorSeverity::Warning, msg,
                                                             "dap_setBreakpoints_pre_configurationDone");
    }
}

void Win32IDE::clearDapBreakpointBindingOnAll()
{
    std::lock_guard<std::mutex> lock(m_debuggerMutex);
    m_dapBreakpointDirtyNormalizedPaths.clear();
    for (auto& bp : m_breakpoints)
    {
        bp.dapVerified.reset();
        bp.dapStatus.clear();
    }
}

void Win32IDE::syncDapBreakpointsForLogicalFile(const std::string& file)
{
    if (!getDapSession(this))
    {
        return;
    }
    const std::string workspaceRoot = !m_projectRoot.empty()
                                          ? m_projectRoot
                                          : (!m_currentDirectory.empty() ? m_currentDirectory : m_explorerRootPath);
    const std::string dapPath = normalizeDapSourcePath(file, workspaceRoot);
    if (dapPath.empty())
    {
        return;
    }
    auto dapSession = getDapSession(this);
    if (!dapSession)
    {
        return;
    }

    for (auto& bp : m_breakpoints)
    {
        if (normalizeDapSourcePath(bp.file, workspaceRoot) != dapPath)
        {
            continue;
        }
        bp.dapVerified = std::nullopt;
        bp.dapStatus.clear();
    }

    std::vector<DapSourceBreakpoint> dapBreakpoints;
    for (const auto& existing : m_breakpoints)
    {
        if (!existing.enabled)
        {
            continue;
        }
        if (normalizeDapSourcePath(existing.file, workspaceRoot) != dapPath)
        {
            continue;
        }
        DapSourceBreakpoint dapBp;
        dapBp.line = existing.line;
        if (!existing.condition.empty())
        {
            dapBp.condition = existing.condition;
        }
        dapBreakpoints.push_back(std::move(dapBp));
    }

    std::vector<DapBreakpointSetResult> binding;
    const bool ok = dapSession->service->setBreakpoints(dapPath, dapBreakpoints, nullptr, &binding);
    applyDapHydrationToBreakpoints(m_breakpoints, workspaceRoot, dapPath, dapBreakpoints, binding, !ok);
    {
        std::lock_guard<std::mutex> lock(m_debuggerMutex);
        m_dapBreakpointDirtyNormalizedPaths.erase(dapPath);
    }
    updateBreakpointList();
    refreshDapUnverifiedBreakpointUi(false);
    if (!ok)
    {
        appendToOutput("⚠️ DAP setBreakpoints failed for: " + dapPath, "Output", OutputSeverity::Warning);
    }
}

void Win32IDE::addBreakpoint(const std::string& file, int line)
{
    // Check if breakpoint already exists in our UI list
    for (auto& bp : m_breakpoints)
    {
        if (bp.file == file && bp.line == line)
        {
            bp.enabled = true;
            updateBreakpointList();
            if (!getDapSession(this))
            {
                markDapBreakpointDirtyForLogicalFile(file);
            }
            syncDapBreakpointsForLogicalFile(file);
            return;
        }
    }

    // Add to NativeDebuggerEngine (source-line breakpoint via DbgEng)
    auto& engine = NativeDebuggerEngine::Instance();
    DebugResult r = engine.addBreakpointBySourceLine(file, line);
    if (!r.success)
    {
        std::string warn = "⚠️ Engine breakpoint at " + file + ":" + std::to_string(line) + " — " + r.detail;
        appendToOutput(warn, "Output", OutputSeverity::Warning);
        // Still add to UI list so user can see it; engine may resolve later
        // when symbols are loaded
    }

    // Add to our UI-side tracking vector
    Breakpoint bp;
    bp.file = file;
    bp.line = line;
    bp.enabled = true;
    bp.condition = "";
    bp.hitCount = 0;

    m_breakpoints.push_back(bp);
    updateBreakpointList();
    if (!getDapSession(this))
    {
        markDapBreakpointDirtyForLogicalFile(file);
    }
    syncDapBreakpointsForLogicalFile(file);

    std::string msg = "🔴 Breakpoint added at " + file + ":" + std::to_string(line);
    if (r.success)
        msg += " (engine confirmed)";
    appendToOutput(msg, "Output", OutputSeverity::Debug);
}

void Win32IDE::setBreakpoint(const std::string& file, int line)
{
    METRICS.increment("debugger.breakpoint_sets");

    // Check if breakpoint already exists at this location
    for (auto& bp : m_breakpoints)
    {
        if (bp.file == file && bp.line == line)
        {
            // Re-enable if it was disabled
            if (!bp.enabled)
            {
                bp.enabled = true;

                // Re-enable in engine: find matching engine breakpoint and enable it
                auto& engine = NativeDebuggerEngine::Instance();
                const auto& engineBPs = engine.getBreakpoints();
                for (const auto& ebp : engineBPs)
                {
                    if (ebp.sourceLine == line)
                    {
                        engine.enableBreakpoint(ebp.id, true);
                        break;
                    }
                }

                updateBreakpointList();
                if (!getDapSession(this))
                {
                    markDapBreakpointDirtyForLogicalFile(file);
                }
                syncDapBreakpointsForLogicalFile(file);
                std::string msg = "🔴 Breakpoint re-enabled at " + file + ":" + std::to_string(line);
                appendToOutput(msg, "Output", OutputSeverity::Debug);
            }
            return;
        }
    }

    // Create in engine first
    auto& engine = NativeDebuggerEngine::Instance();
    DebugResult r = engine.addBreakpointBySourceLine(file, line);
    if (!r.success)
    {
        std::string warn = "⚠️ Engine BP at " + file + ":" + std::to_string(line) + " — " + r.detail;
        appendToOutput(warn, "Output", OutputSeverity::Warning);
    }

    // Create and add to our UI-side tracking
    Breakpoint bp;
    bp.file = file;
    bp.line = line;
    bp.enabled = true;
    bp.condition = "";
    bp.hitCount = 0;

    m_breakpoints.push_back(bp);
    updateBreakpointList();
    if (!getDapSession(this))
    {
        markDapBreakpointDirtyForLogicalFile(file);
    }
    syncDapBreakpointsForLogicalFile(file);

    // Highlight the breakpoint line in the editor if it's the current file
    if (file == m_currentFile || file == m_debuggerCurrentFile)
    {
        highlightDebuggerLine(file, line);
    }

    std::string msg = "🔴 Breakpoint set at " + file + ":" + std::to_string(line);
    if (r.success)
        msg += " (engine confirmed)";
    appendToOutput(msg, "Output", OutputSeverity::Info);
}

void Win32IDE::removeBreakpoint(const std::string& file, int line)
{
    auto it = std::find_if(m_breakpoints.begin(), m_breakpoints.end(),
                           [&](const Breakpoint& bp) { return bp.file == file && bp.line == line; });

    if (it != m_breakpoints.end())
    {
        // Remove from engine: find matching engine breakpoint by source line
        auto& engine = NativeDebuggerEngine::Instance();
        const auto& engineBPs = engine.getBreakpoints();
        for (const auto& ebp : engineBPs)
        {
            if (ebp.sourceLine == line)
            {
                DebugResult r = engine.removeBreakpoint(ebp.id);
                if (!r.success)
                {
                    std::string warn = "⚠️ Engine BP remove: ";
                    warn += r.detail;
                    appendToOutput(warn, "Output", OutputSeverity::Warning);
                }
                break;
            }
        }

        m_breakpoints.erase(it);
        updateBreakpointList();
        if (!getDapSession(this))
        {
            markDapBreakpointDirtyForLogicalFile(file);
        }
        syncDapBreakpointsForLogicalFile(file);

        std::string msg = "⚪ Breakpoint removed from " + file + ":" + std::to_string(line);
        appendToOutput(msg, "Output", OutputSeverity::Debug);
    }
}

void Win32IDE::toggleBreakpoint(const std::string& file, int line)
{
    METRICS.increment("debugger.breakpoint_toggles");
    auto it = std::find_if(m_breakpoints.begin(), m_breakpoints.end(),
                           [&](const Breakpoint& bp) { return bp.file == file && bp.line == line; });

    if (it != m_breakpoints.end())
    {
        it->enabled = !it->enabled;

        // Toggle enable state in engine
        auto& engine = NativeDebuggerEngine::Instance();
        const auto& engineBPs = engine.getBreakpoints();
        for (const auto& ebp : engineBPs)
        {
            if (ebp.sourceLine == line)
            {
                engine.enableBreakpoint(ebp.id, it->enabled);
                break;
            }
        }

        if (!getDapSession(this))
        {
            markDapBreakpointDirtyForLogicalFile(file);
        }
        syncDapBreakpointsForLogicalFile(file);
        updateBreakpointList();
    }
    else
    {
        addBreakpoint(file, line);
    }
}

void Win32IDE::clearAllBreakpoints()
{
    const std::string workspaceRoot = !m_projectRoot.empty()
                                          ? m_projectRoot
                                          : (!m_currentDirectory.empty() ? m_currentDirectory : m_explorerRootPath);
    std::vector<std::string> affectedNormalizedPaths;
    for (const auto& bp : m_breakpoints)
    {
        const std::string n = normalizeDapSourcePath(bp.file, workspaceRoot);
        if (n.empty())
        {
            continue;
        }
        if (std::find(affectedNormalizedPaths.begin(), affectedNormalizedPaths.end(), n) ==
            affectedNormalizedPaths.end())
        {
            affectedNormalizedPaths.push_back(n);
        }
    }

    // Remove all breakpoints from engine
    auto& engine = NativeDebuggerEngine::Instance();
    DebugResult r = engine.removeAllBreakpoints();
    if (!r.success)
    {
        std::string warn = "⚠️ Engine remove-all BPs: ";
        warn += r.detail;
        appendToOutput(warn, "Output", OutputSeverity::Warning);
    }

    if (!getDapSession(this))
    {
        std::lock_guard<std::mutex> lock(m_debuggerMutex);
        for (const auto& p : affectedNormalizedPaths)
        {
            m_dapBreakpointDirtyNormalizedPaths.insert(p);
        }
    }

    m_breakpoints.clear();

    if (auto dapSession = getDapSession(this))
    {
        for (const auto& path : affectedNormalizedPaths)
        {
            if (!dapSession->service->setBreakpoints(path, {}))
            {
                appendToOutput("⚠️ DAP clear-breakpoints failed for: " + path, "Output", OutputSeverity::Warning);
            }
        }
    }

    updateBreakpointList();
    appendToOutput("🗑 All breakpoints cleared (UI + engine)", "Output", OutputSeverity::Info);
}

void Win32IDE::updateBreakpointList()
{
    if (!m_hwndDebuggerBreakpoints)
        return;

    ListView_DeleteAllItems(m_hwndDebuggerBreakpoints);

    LVITEMA lvi;
    lvi.mask = LVIF_TEXT;

    const bool dapSessionActive = (getDapSession(this) != nullptr);

    {
        std::lock_guard<std::mutex> lock(m_debuggerMutex);

        for (size_t i = 0; i < m_breakpoints.size(); ++i)
        {
            const auto& bp = m_breakpoints[i];

            lvi.iItem = static_cast<int>(i);
            lvi.iSubItem = 0;
            lvi.pszText = const_cast<char*>(bp.file.c_str());
            ListView_InsertItem(m_hwndDebuggerBreakpoints, &lvi);

            lvi.iSubItem = 1;
            std::string line_str = std::to_string(bp.line);
            lvi.pszText = const_cast<char*>(line_str.c_str());
            ListView_SetItem(m_hwndDebuggerBreakpoints, &lvi);

            lvi.iSubItem = 2;
            std::string hits_str = std::to_string(bp.hitCount);
            lvi.pszText = const_cast<char*>(hits_str.c_str());
            ListView_SetItem(m_hwndDebuggerBreakpoints, &lvi);

            lvi.iSubItem = 3;
            std::string dap_str;
            if (!dapSessionActive || !bp.enabled)
            {
                dap_str = "—";
            }
            else if (!bp.dapVerified.has_value())
            {
                dap_str = "?";
            }
            else if (*bp.dapVerified)
            {
                dap_str = bp.dapStatus.empty() ? "*" : ("* " + bp.dapStatus);
            }
            else
            {
                dap_str = bp.dapStatus.empty() ? "o" : ("o " + bp.dapStatus);
            }
            if (dap_str.size() > 200)
            {
                dap_str.resize(197);
                dap_str += "...";
            }
            lvi.pszText = const_cast<char*>(dap_str.c_str());
            ListView_SetItem(m_hwndDebuggerBreakpoints, &lvi);
        }
    }
}

// ============================================================================
// WATCH EXPRESSION MANAGEMENT
// ============================================================================

void Win32IDE::addWatchExpression(const std::string& expression)
{
    // Register with the engine's watch system
    auto& engine = NativeDebuggerEngine::Instance();
    uint32_t watchId = engine.addWatch(expression);

    WatchItem item;
    item.expression = expression;
    item.value = "...";
    item.type = "pending";
    item.enabled = true;

    // If engine is live and paused, evaluate immediately
    if (m_debuggerAttached && m_debuggerPaused)
    {
        engine.updateWatches();
        const auto& watches = engine.getWatches();
        for (const auto& w : watches)
        {
            if (w.id == watchId && w.lastResult.success)
            {
                item.value = w.lastResult.value;
                item.type = w.lastResult.type;
                break;
            }
        }
    }

    m_watchList.push_back(item);
    updateWatchList();

    std::string msg = "👁 Watch added: " + expression;
    if (watchId > 0)
        msg += " (engine ID: " + std::to_string(watchId) + ")";
    appendToOutput(msg, "Output", OutputSeverity::Debug);
}

void Win32IDE::removeWatchExpression(const std::string& expression)
{
    auto it = std::find_if(m_watchList.begin(), m_watchList.end(),
                           [&](const WatchItem& item) { return item.expression == expression; });

    if (it != m_watchList.end())
    {
        // Find and remove from engine watch list
        auto& engine = NativeDebuggerEngine::Instance();
        const auto& watches = engine.getWatches();
        for (const auto& w : watches)
        {
            if (w.expression == expression)
            {
                engine.removeWatch(w.id);
                break;
            }
        }

        m_watchList.erase(it);
        updateWatchList();

        std::string msg = "👁 Watch removed: " + expression;
        appendToOutput(msg, "Output", OutputSeverity::Debug);
    }
}

void Win32IDE::updateWatchList()
{
    if (!m_hwndDebuggerWatch)
        return;

    if (getDapSession(this))
    {
        if (m_debuggerAttached && m_debuggerPaused)
        {
            for (auto& item : m_watchList)
            {
                evaluateWatch(item);
            }
        }

        ListView_DeleteAllItems(m_hwndDebuggerWatch);
        LVITEMA lvi;
        lvi.mask = LVIF_TEXT;
        for (size_t i = 0; i < m_watchList.size(); ++i)
        {
            auto& item = m_watchList[i];
            lvi.iItem = static_cast<int>(i);
            lvi.iSubItem = 0;
            lvi.pszText = const_cast<char*>(item.expression.c_str());
            ListView_InsertItem(m_hwndDebuggerWatch, &lvi);
            lvi.iSubItem = 1;
            lvi.pszText = const_cast<char*>(item.value.c_str());
            ListView_SetItem(m_hwndDebuggerWatch, &lvi);
            lvi.iSubItem = 2;
            lvi.pszText = const_cast<char*>(item.type.c_str());
            ListView_SetItem(m_hwndDebuggerWatch, &lvi);
        }
        return;
    }

    // Refresh watch values from engine if we're paused
    if (m_debuggerAttached && m_debuggerPaused)
    {
        auto& engine = NativeDebuggerEngine::Instance();
        engine.updateWatches();
        const auto& watches = engine.getWatches();

        // Sync engine results → our UI watch items
        for (auto& item : m_watchList)
        {
            for (const auto& w : watches)
            {
                if (w.expression == item.expression)
                {
                    if (w.lastResult.success)
                    {
                        item.value = w.lastResult.value;
                        item.type = w.lastResult.type;
                    }
                    else
                    {
                        item.value = "<unavailable>";
                        item.type = "error";
                    }
                    break;
                }
            }
        }
    }

    ListView_DeleteAllItems(m_hwndDebuggerWatch);

    LVITEMA lvi;
    lvi.mask = LVIF_TEXT;

    for (size_t i = 0; i < m_watchList.size(); ++i)
    {
        auto& item = m_watchList[i];

        lvi.iItem = static_cast<int>(i);
        lvi.iSubItem = 0;
        lvi.pszText = const_cast<char*>(item.expression.c_str());
        ListView_InsertItem(m_hwndDebuggerWatch, &lvi);

        lvi.iSubItem = 1;
        lvi.pszText = const_cast<char*>(item.value.c_str());
        ListView_SetItem(m_hwndDebuggerWatch, &lvi);

        lvi.iSubItem = 2;
        lvi.pszText = const_cast<char*>(item.type.c_str());
        ListView_SetItem(m_hwndDebuggerWatch, &lvi);
    }
}

void Win32IDE::evaluateWatch(WatchItem& item)
{
    // Try DAP first if a DAP session is active
    if (auto dapSession = getDapSession(this))
    {
        int frameId = -1;
        bool shouldQueue = false;
        bool shouldRefreshFrames = false;
        const auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(m_debuggerMutex);

            const bool hasFrame = dapSession->currentFrameId >= 0;
            const bool frameStale =
                m_lastFrameUpdateTime.time_since_epoch().count() != 0 &&
                std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastFrameUpdateTime).count() >
                    FRAME_STALE_MS;

            if (!hasFrame || !m_debuggerPaused || frameStale)
            {
                // Frame not valid/current yet - queue for later evaluation.
                item.value = frameStale ? "<refreshing frame context>" : "<waiting for frame context>";
                item.type = "pending";
                shouldQueue = true;
                if (frameStale)
                {
                    const bool cooldownPassed =
                        m_lastFrameRefreshAttemptTime.time_since_epoch().count() == 0 ||
                        std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastFrameRefreshAttemptTime)
                                .count() >= FRAME_REFRESH_COOLDOWN_MS;
                    if (cooldownPassed)
                    {
                        shouldRefreshFrames = true;
                        m_lastFrameRefreshAttemptTime = now;
                    }
                }
            }
            else
            {
                frameId = dapSession->currentFrameId;
            }
        }

        if (shouldRefreshFrames)
        {
            updateCallStack();
        }

        if (shouldQueue)
        {
            queueEvalRequest(item.expression, dapSession->currentFrameId);
            return;
        }

        try
        {
            const std::string result = dapSession->service->evaluate(frameId, item.expression, "watch",
                                                                     static_cast<uint32_t>(EVAL_TIMEOUT_MS));

            // Parse DAP evaluate response (typically "value: type" format)
            if (!result.empty())
            {
                item.value = result;
                item.type = "dap";
                return;
            }

            appendToOutput("⏱️ DAP watch eval timed out/failed after " + std::to_string(EVAL_TIMEOUT_MS) + "ms for '" +
                               item.expression + "'",
                           "Output", OutputSeverity::Warning);
            item.value = "<timeout>";
            item.type = "error";
            return;
        }
        catch (const std::exception& ex)
        {
            std::string err = "⚠️ DAP watch eval failed for '" + item.expression + "': ";
            err += ex.what();
            appendToOutput(err, "Output", OutputSeverity::Warning);
            item.value = "<DAP error>";
            item.type = "error";
            return;
        }
        catch (...)
        {
            std::string err = "❌ DAP watch eval: unknown error for '" + item.expression + "'";
            appendToOutput(err, "Output", OutputSeverity::Error);
            item.value = "<DAP error>";
            item.type = "error";
            return;
        }
    }

    // Fall back to native debugger engine
    auto& engine = NativeDebuggerEngine::Instance();
    EvalResult evalRes;
    DebugResult r = engine.evaluate(item.expression, evalRes);
    if (r.success)
    {
        item.value = evalRes.value;
        item.type = evalRes.type;
    }
    else
    {
        item.value = "<error: ";
        item.value += r.detail;
        item.value += ">";
        item.type = "error";
    }
}

// ============================================================================
// VARIABLE & STACK INSPECTION
// ============================================================================

void Win32IDE::updateVariables()
{
    if (auto dapSession = getDapSession(this))
    {
        m_localVariables.clear();

        if (m_debuggerAttached && m_debuggerPaused)
        {
            if (m_callStack.empty())
            {
                updateCallStack();
            }

            if (dapSession->currentFrameId >= 0)
            {  // -1 = no frame yet; 0+ are valid DAP frame IDs
                try
                {
                    const auto scopes = dapSession->service->scopes(dapSession->currentFrameId);

                    // Enumerate ALL scopes and collect variables under each, grouped by scope name.
                    // This populates m_localVariables with entries that carry a scopeGroup label so
                    // the tree view can show "[Locals]", "[Registers]", "[Statics]" headers.
                    for (const auto& scope : scopes)
                    {
                        const int ref = scope.value("variablesReference", 0);
                        if (ref <= 0)
                            continue;

                        const std::string scopeName = scope.value("name", "Scope");
                        const bool expensive = scope.value("expensive", false);
                        if (expensive)
                            continue;  // Skip heap-dumping scopes until user expands

                        // Insert a group header entry (name only, no value).
                        Variable header;
                        header.name = "[" + scopeName + "]";
                        header.value = "";
                        header.type = "__scope_header";
                        header.variablesReference = 0;
                        header.expanded = false;
                        m_localVariables.push_back(std::move(header));

                        const auto vars = dapSession->service->variables(ref);
                        for (const auto& entry : vars)
                        {
                            Variable var;
                            var.name = entry.value("name", "");
                            var.value = entry.value("value", "");
                            var.type = entry.value("type", "dap");
                            var.variablesReference = entry.value("variablesReference", 0);
                            var.expanded = false;
                            m_localVariables.push_back(std::move(var));
                        }
                    }
                }
                catch (const std::exception& ex)
                {
                    appendToOutput(std::string("❌ DAP variables error: ") + ex.what(), "Output",
                                   OutputSeverity::Error);
                }
                catch (...)
                {
                    appendToOutput("❌ DAP variables: unknown error", "Output", OutputSeverity::Error);
                }
            }
        }

        if (m_hwndDebuggerVariables)
        {
            TreeView_DeleteAllItems(m_hwndDebuggerVariables);
            if (m_localVariables.empty())
            {
                TVINSERTSTRUCTA tvis = {};
                tvis.hParent = TVI_ROOT;
                tvis.hInsertAfter = TVI_LAST;
                tvis.item.mask = TVIF_TEXT;
                const char* placeholder = (m_debuggerAttached && m_debuggerPaused)
                                              ? "(No DAP scopes for this frame)"
                                              : "Start a DAP session to inspect variables";
                tvis.item.pszText = const_cast<char*>(placeholder);
                TreeView_InsertItem(m_hwndDebuggerVariables, &tvis);
            }
            else
            {
                HTREEITEM currentScopeParent = TVI_ROOT;
                for (const auto& var : m_localVariables)
                {
                    if (var.type == "__scope_header")
                    {
                        // Insert as a bold-ish top-level node that acts as the scope group
                        TVINSERTSTRUCTA tvis = {};
                        tvis.hParent = TVI_ROOT;
                        tvis.hInsertAfter = TVI_LAST;
                        tvis.item.mask = TVIF_TEXT | TVIF_STATE | TVIF_CHILDREN;
                        tvis.item.pszText = const_cast<char*>(var.name.c_str());
                        tvis.item.state = TVIS_EXPANDED;
                        tvis.item.stateMask = TVIS_EXPANDED;
                        tvis.item.cChildren = 1;  // has children (the variables)
                        currentScopeParent = TreeView_InsertItem(m_hwndDebuggerVariables, &tvis);
                    }
                    else
                    {
                        TVINSERTSTRUCTA tvis = {};
                        tvis.hParent = currentScopeParent;
                        tvis.hInsertAfter = TVI_LAST;
                        tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
                        std::string itemText = var.name;
                        if (!var.value.empty())
                            itemText += " = " + var.value;
                        if (!var.type.empty() && var.type != "dap")
                            itemText += "  (" + var.type + ")";
                        tvis.item.pszText = const_cast<char*>(itemText.c_str());
                        tvis.item.lParam = static_cast<LPARAM>(var.variablesReference);
                        tvis.item.cChildren = (var.variablesReference > 0) ? 1 : 0;
                        TreeView_InsertItem(m_hwndDebuggerVariables, &tvis);
                    }
                }
            }
        }
        return;
    }

    auto& engine = NativeDebuggerEngine::Instance();
    m_localVariables.clear();

    if (m_debuggerAttached && m_debuggerPaused)
    {
        int frameIndex = m_selectedStackFrameIndex;
        if (frameIndex < 0)
            frameIndex = 0;
        if (!m_callStack.empty() && (size_t)frameIndex >= m_callStack.size())
            frameIndex = static_cast<int>(m_callStack.size()) - 1;

        std::map<std::string, std::string> frameLocals;
        DebugResult r = engine.getFrameLocals(frameIndex, frameLocals);
        if (r.success)
        {
            for (const auto& local : frameLocals)
            {
                Variable var;
                var.name = local.first;
                var.value = local.second;
                var.type = "auto";
                var.expanded = false;
                m_localVariables.push_back(var);
            }
        }
        else
        {
            appendToOutput(std::string("Failed to fetch frame locals: ") + r.detail, "Output", OutputSeverity::Warning);
        }
        if (m_localVariables.empty() && !m_callStack.empty() && frameIndex >= 0 &&
            (size_t)frameIndex < m_callStack.size())
        {
            const auto& frame = m_callStack[static_cast<size_t>(frameIndex)];
            for (const auto& local : frame.locals)
            {
                Variable var;
                var.name = local.first;
                var.value = local.second;
                var.type = "auto";
                var.expanded = false;
                m_localVariables.push_back(var);
            }
        }
    }

    if (m_hwndDebuggerVariables)
    {
        TreeView_DeleteAllItems(m_hwndDebuggerVariables);

        if (m_localVariables.empty())
        {
            TVINSERTSTRUCTA tvis = {};
            tvis.hParent = TVI_ROOT;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT;
            const char* placeholder = (m_debuggerAttached && m_debuggerPaused)
                                          ? "(No locals for this frame)"
                                          : "Attach and break to inspect variables";
            tvis.item.pszText = const_cast<char*>(placeholder);
            TreeView_InsertItem(m_hwndDebuggerVariables, &tvis);
        }
        else
        {
            for (const auto& var : m_localVariables)
            {
                TVINSERTSTRUCTA tvis = {};
                tvis.hParent = TVI_ROOT;
                tvis.hInsertAfter = TVI_LAST;
                tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
                std::string item_text = var.name + " = " + var.value + " (" + var.type + ")";
                tvis.item.pszText = const_cast<char*>(item_text.c_str());
                tvis.item.lParam = static_cast<LPARAM>(var.variablesReference);
                tvis.item.cChildren = (var.variablesReference > 0) ? 1 : 0;
                TreeView_InsertItem(m_hwndDebuggerVariables, &tvis);
            }
        }
    }
}

void Win32IDE::updateCallStack()
{
    if (!m_hwndDebuggerStackTrace)
        return;

    if (auto dapSession = getDapSession(this))
    {
        auto& frameTracker = DebuggerFrameTrackerInstance::instance();
        auto& errorHandler = DebuggerErrorHandlerInstance::instance();
        constexpr int DAP_STACKTRACE_MAX_LEVELS = 500;
        constexpr int DAP_FALLBACK_THREAD_PROBE_MAX = 8;

        auto clearDapFrameContext = [&]() noexcept
        {
            try
            {
                frameTracker.clear();
                m_lastDroppedFrameCount.store(0);
                std::lock_guard<std::mutex> lock(m_debuggerMutex);
                m_callStack.clear();
                m_localVariables.clear();
                m_selectedStackFrameIndex = 0;
                m_pendingEvalRequests.clear();
                m_lastFrameUpdateTime = std::chrono::steady_clock::time_point{};
                m_lastFrameRefreshAttemptTime = std::chrono::steady_clock::time_point{};
                m_debuggerCurrentFile.clear();
                m_debuggerCurrentLine = -1;
                if (dapSession)
                {
                    dapSession->currentFrameId = -1;
                }
            }
            catch (...)
            {
                // Best effort only.
            }
        };

        auto contextScope = errorHandler.scopedContext("updateCallStack[DAP]");

        if (m_debuggerAttached && m_debuggerPaused)
        {
            try
            {
                using DapFrames = std::vector<RawrXD::Debug::DapStackFrame>;
                const auto fetchFramesWithRetry = [&](int threadId) noexcept -> DebuggerResult<DapFrames>
                {
                    auto framesResult = errorHandler.retryWithBackoff(
                        [&]() noexcept -> DebuggerResult<DapFrames>
                        {
                            try
                            {
                                const auto frames =
                                    dapSession->service->stackTrace(threadId, 0, DAP_STACKTRACE_MAX_LEVELS);
                                if (!frames.empty())
                                {
                                    return DebuggerResult<DapFrames>(frames);
                                }

                                DebuggerError err;
                                err.type = DebuggerErrorType::AdapterCommunicationFailed;
                                err.severity = ErrorSeverity::Warning;
                                err.message =
                                    "DAP stackTrace returned no frames for threadId=" + std::to_string(threadId);
                                err.context = "updateCallStack[DAP]";
                                err.isRecoverable = true;
                                err.suggestedStrategy = RecoveryStrategy::Retry;
                                return DebuggerResult<DapFrames>(err);
                            }
                            catch (const std::exception& ex)
                            {
                                DebuggerError err;
                                err.type = DebuggerErrorType::AdapterCommunicationFailed;
                                err.severity = ErrorSeverity::Error;
                                err.message = std::string("DAP stackTrace failed: ") + ex.what();
                                err.context = "updateCallStack[DAP]";
                                err.isRecoverable = true;
                                err.suggestedStrategy = RecoveryStrategy::Retry;
                                return DebuggerResult<DapFrames>(err);
                            }
                            catch (...)
                            {
                                DebuggerError err;
                                err.type = DebuggerErrorType::AdapterCommunicationFailed;
                                err.severity = ErrorSeverity::Error;
                                err.message = "DAP stackTrace failed: unknown error";
                                err.context = "updateCallStack[DAP]";
                                err.isRecoverable = true;
                                err.suggestedStrategy = RecoveryStrategy::Retry;
                                return DebuggerResult<DapFrames>(err);
                            }
                        },
                        3);

                    if (framesResult.success)
                    {
                        return framesResult;
                    }

                    if (framesResult.error)
                    {
                        errorHandler.reportError(*framesResult.error);
                    }
                    return DebuggerResult<DapFrames>();
                };

                int resolvedThreadId = (dapSession->currentThreadId > 0) ? dapSession->currentThreadId : 1;
                DapFrames dapFrames;
                {
                    const auto primary = fetchFramesWithRetry(resolvedThreadId);
                    if (primary.success)
                    {
                        dapFrames = primary.value;
                    }
                }

                if (dapFrames.empty())
                {
                    for (int probeThreadId = 1; probeThreadId <= DAP_FALLBACK_THREAD_PROBE_MAX; ++probeThreadId)
                    {
                        if (probeThreadId == resolvedThreadId)
                        {
                            continue;
                        }
                        const auto candidate = fetchFramesWithRetry(probeThreadId);
                        if (!candidate.success || candidate.value.empty())
                        {
                            continue;
                        }

                        resolvedThreadId = probeThreadId;
                        dapFrames = candidate.value;
                        appendToOutput("⚠️ DAP thread fallback: switched stackTrace context to threadId=" +
                                           std::to_string(resolvedThreadId),
                                       "Output", OutputSeverity::Warning);
                        break;
                    }
                }

                if (dapFrames.empty())
                {
                    errorHandler.reportError(DebuggerErrorType::AdapterCommunicationFailed, ErrorSeverity::Error,
                                             "DAP stackTrace failed after retries and thread fallback",
                                             "updateCallStack[DAP]");
                    clearDapFrameContext();
                    clearDebuggerHighlight();
                    updateVariables();
                    updateWatchList();
                }
                else
                {
                    // Convert DAP frames to EnhancedStackFrame for frame tracker
                    std::vector<EnhancedStackFrame> enhanced;
                    enhanced.reserve(dapFrames.size());
                    for (const auto& frame : dapFrames)
                    {
                        EnhancedStackFrame ef;
                        ef.function = frame.name;
                        ef.file = frame.sourcePath;
                        ef.line = frame.line;
                        ef.dapFrameId = frame.id;
                        ef.threadId = resolvedThreadId;
                        ef.captureTime = std::chrono::steady_clock::now();
                        ef.state = FrameState::Unknown;  // validateFrame will set
                        enhanced.push_back(std::move(ef));
                    }

                    // Feed into frame tracker for validation + history
                    size_t validCount = frameTracker.updateFromDapFrames(enhanced);
                    const auto frameStats = frameTracker.getStatistics();
                    const size_t droppedFrames = frameStats.droppedFrames;
                    const size_t prevDropped = m_lastDroppedFrameCount.exchange(droppedFrames);
                    if (droppedFrames > 0 && droppedFrames != prevDropped)
                    {
                        errorHandler.reportError(DebuggerErrorType::FrameDataCorrupted, ErrorSeverity::Warning,
                                                 "Call stack truncated: dropped " + std::to_string(droppedFrames) +
                                                     " frame(s) beyond tracker limit",
                                                 "updateCallStack[DAP]");
                        appendToOutput("⚠️ Frame truncation: dropped " + std::to_string(droppedFrames) +
                                           " frame(s) beyond tracking limit",
                                       "Output", OutputSeverity::Warning);
                    }
                    else if (droppedFrames == 0 && prevDropped > 0)
                    {
                        appendToOutput("✅ Frame truncation cleared", "Output", OutputSeverity::Info);
                    }

                    if (validCount == 0)
                    {
                        errorHandler.reportError(DebuggerErrorType::FrameDataCorrupted, ErrorSeverity::Warning,
                                                 "DAP frames received but none passed validation",
                                                 "updateCallStack[DAP]");
                    }

                    // Validate + attempt recovery on invalid frames
                    if (!frameTracker.validateStack())
                    {
                        errorHandler.reportError(DebuggerErrorType::FrameDataCorrupted, ErrorSeverity::Warning,
                                                 "Some DAP frames failed validation; attempting recovery",
                                                 "updateCallStack[DAP]");

                        for (size_t i = 0; i < frameTracker.frameCount(); ++i)
                        {
                            const auto* f = frameTracker.getFrame(i);
                            if (f && f->state != FrameState::Valid)
                            {
                                frameTracker.attemptFrameRecovery(i);
                            }
                        }
                    }

                    // Sync back to m_callStack for backward compatibility
                    std::string selectedFileToHighlight;
                    int selectedLineToHighlight = -1;
                    {
                        std::lock_guard<std::mutex> lock(m_debuggerMutex);
                        m_callStack.clear();
                        auto displayFrames = frameTracker.getDisplayFrames();
                        for (const auto& ef : displayFrames)
                        {
                            StackFrame sf;
                            sf.function = ef.function;
                            sf.file = ef.file;
                            sf.line = ef.line;
                            sf.dapFrameId = ef.dapFrameId;
                            m_callStack.push_back(std::move(sf));
                        }

                        if (!m_callStack.empty())
                        {
                            const size_t selectedIndex =
                                std::min(frameTracker.getCurrentFrameIndex(), m_callStack.size() - 1);
                            m_selectedStackFrameIndex = static_cast<int>(selectedIndex);
                            dapSession->currentThreadId = resolvedThreadId;
                            dapSession->currentFrameId = m_callStack[selectedIndex].dapFrameId;
                            m_lastFrameUpdateTime = std::chrono::steady_clock::now();
                            if (!m_callStack[selectedIndex].file.empty())
                            {
                                m_debuggerCurrentFile = m_callStack[selectedIndex].file;
                                m_debuggerCurrentLine = m_callStack[selectedIndex].line;
                                selectedFileToHighlight = m_callStack[selectedIndex].file;
                                selectedLineToHighlight = m_callStack[selectedIndex].line;
                            }
                        }
                    }

                    if (!selectedFileToHighlight.empty() && selectedLineToHighlight > 0)
                    {
                        highlightDebuggerLine(selectedFileToHighlight, selectedLineToHighlight);
                    }
                }
            }
            catch (const std::exception& ex)
            {
                clearDapFrameContext();
                clearDebuggerHighlight();
                updateVariables();
                updateWatchList();
                DebuggerError err;
                err.type = DebuggerErrorType::AdapterCommunicationFailed;
                err.severity = ErrorSeverity::Error;
                err.message = std::string("DAP stackTrace failed: ") + ex.what();
                err.isRecoverable = true;
                err.suggestedStrategy = RecoveryStrategy::Retry;
                errorHandler.reportError(err);
                appendToOutput(std::string("❌ DAP stackTrace error: ") + ex.what(), "Output", OutputSeverity::Error);
            }
            catch (...)
            {
                clearDapFrameContext();
                clearDebuggerHighlight();
                updateVariables();
                updateWatchList();
                DebuggerError err;
                err.type = DebuggerErrorType::AdapterCommunicationFailed;
                err.severity = ErrorSeverity::Critical;
                err.message = "DAP stackTrace: unknown error";
                err.isRecoverable = false;
                errorHandler.reportError(err);
                appendToOutput("❌ DAP stackTrace: unknown error", "Output", OutputSeverity::Error);
            }
        }

        ListView_DeleteAllItems(m_hwndDebuggerStackTrace);
        LVITEMA lvi;
        lvi.mask = LVIF_TEXT;
        {
            std::lock_guard<std::mutex> lock(m_debuggerMutex);
            if (m_callStack.empty())
            {
                lvi.iItem = 0;
                lvi.iSubItem = 0;
                lvi.pszText = const_cast<char*>("Start a DAP session to inspect call stack");
                ListView_InsertItem(m_hwndDebuggerStackTrace, &lvi);
            }
            else
            {
                for (size_t i = 0; i < m_callStack.size(); ++i)
                {
                    const auto& frame = m_callStack[i];
                    lvi.iItem = static_cast<int>(i);
                    lvi.iSubItem = 0;
                    lvi.pszText = const_cast<char*>(frame.function.c_str());
                    ListView_InsertItem(m_hwndDebuggerStackTrace, &lvi);
                    lvi.iSubItem = 1;
                    lvi.pszText = const_cast<char*>(frame.file.c_str());
                    ListView_SetItem(m_hwndDebuggerStackTrace, &lvi);
                    lvi.iSubItem = 2;
                    std::string lineText = std::to_string(frame.line);
                    lvi.pszText = const_cast<char*>(lineText.c_str());
                    ListView_SetItem(m_hwndDebuggerStackTrace, &lvi);
                }
            }
        }
        // Apply selection highlight to the current frame row
        {
            int selIdx = 0;
            size_t callStackSize = 0;
            {
                std::lock_guard<std::mutex> lock(m_debuggerMutex);
                selIdx = m_selectedStackFrameIndex;
                callStackSize = m_callStack.size();
            }
            if (m_hwndDebuggerStackTrace && selIdx >= 0 && selIdx < static_cast<int>(callStackSize))
            {
                ListView_SetItemState(m_hwndDebuggerStackTrace, selIdx, LVIS_SELECTED | LVIS_FOCUSED,
                                      LVIS_SELECTED | LVIS_FOCUSED);
                ListView_EnsureVisible(m_hwndDebuggerStackTrace, selIdx, FALSE);
            }
        }
        updateDebuggerErrorStatus();
        return;
    }

    // Fetch real stack frames from engine when paused
    if (m_debuggerAttached && m_debuggerPaused)
    {
        auto& engine = NativeDebuggerEngine::Instance();
        auto& frameTracker = DebuggerFrameTrackerInstance::instance();
        auto& errorHandler = DebuggerErrorHandlerInstance::instance();

        auto clearNativeFrameContext = [&]() noexcept
        {
            try
            {
                frameTracker.clear();
                m_lastDroppedFrameCount.store(0);
                std::lock_guard<std::mutex> lock(m_debuggerMutex);
                m_callStack.clear();
                m_localVariables.clear();
                m_selectedStackFrameIndex = 0;
                m_pendingEvalRequests.clear();
                m_lastFrameUpdateTime = std::chrono::steady_clock::time_point{};
                m_lastFrameRefreshAttemptTime = std::chrono::steady_clock::time_point{};
                m_debuggerCurrentFile.clear();
                m_debuggerCurrentLine = -1;
            }
            catch (...)
            {
                // Best effort only.
            }
        };

        auto contextScope = errorHandler.scopedContext("updateCallStack");

        try
        {
            std::vector<NativeStackFrame> nativeFrames;
            DebugResult r = engine.walkStack(nativeFrames, 256);

            // Error handling for failed walkStack
            if (!r.success)
            {
                DebuggerError err;
                err.type = DebuggerErrorType::FrameStackWalkFailed;
                err.severity = ErrorSeverity::Error;
                err.message = "Failed to walk the stack: " + std::string(r.detail ? r.detail : "Unknown error");
                err.isRecoverable = true;
                err.suggestedStrategy = RecoveryStrategy::Retry;
                errorHandler.reportError(err);
                clearNativeFrameContext();
                clearDebuggerHighlight();
                updateVariables();
                updateWatchList();
            }
            else if (nativeFrames.empty())
            {
                clearNativeFrameContext();
                clearDebuggerHighlight();
                updateVariables();
                updateWatchList();
            }
            else
            {
                // Use the new frame tracking system
                size_t frameCount = frameTracker.updateFromNativeFrames(nativeFrames);
                const auto frameStats = frameTracker.getStatistics();
                const size_t droppedFrames = frameStats.droppedFrames;
                const size_t prevDropped = m_lastDroppedFrameCount.exchange(droppedFrames);
                if (droppedFrames > 0 && droppedFrames != prevDropped)
                {
                    errorHandler.reportError(DebuggerErrorType::FrameDataCorrupted, ErrorSeverity::Warning,
                                             "Call stack truncated: dropped " + std::to_string(droppedFrames) +
                                                 " frame(s) beyond tracker limit",
                                             "updateCallStack");
                    appendToOutput("⚠️ Frame truncation: dropped " + std::to_string(droppedFrames) +
                                       " frame(s) beyond tracking limit",
                                   "Output", OutputSeverity::Warning);
                }
                else if (droppedFrames == 0 && prevDropped > 0)
                {
                    appendToOutput("✅ Frame truncation cleared", "Output", OutputSeverity::Info);
                }

                if (frameCount == 0)
                {
                    DebuggerError err;
                    err.type = DebuggerErrorType::FrameDataCorrupted;
                    err.severity = ErrorSeverity::Warning;
                    err.message = "Stack walk returned frames but none passed validation";
                    err.isRecoverable = true;
                    err.suggestedStrategy = RecoveryStrategy::Fallback;
                    errorHandler.reportError(err);
                }

                // Validate the entire stack
                if (!frameTracker.validateStack())
                {
                    errorHandler.reportError(DebuggerErrorType::FrameDataCorrupted, ErrorSeverity::Warning,
                                             "Some frames failed validation; attempting recovery", "updateCallStack");

                    // Try to recover invalid frames
                    for (size_t i = 0; i < frameTracker.frameCount(); ++i)
                    {
                        if (!frameTracker.getFrame(i) || frameTracker.getFrame(i)->state != FrameState::Valid)
                        {
                            frameTracker.attemptFrameRecovery(i);
                        }
                    }
                }

                // Update old m_callStack for backward compatibility
                std::string selectedFileToHighlight;
                int selectedLineToHighlight = -1;
                {
                    std::lock_guard<std::mutex> lock(m_debuggerMutex);
                    m_callStack.clear();
                    auto displayFrames = frameTracker.getDisplayFrames();
                    for (const auto& ef : displayFrames)
                    {
                        StackFrame sf;
                        sf.function = ef.function;
                        sf.file = ef.file;
                        sf.line = ef.line;
                        sf.dapFrameId = ef.dapFrameId;
                        sf.locals = ef.locals;
                        m_callStack.push_back(sf);
                    }

                    // Update debugger current position from selected frame
                    if (!m_callStack.empty())
                    {
                        const size_t selectedIndex =
                            std::min(frameTracker.getCurrentFrameIndex(), m_callStack.size() - 1);
                        m_selectedStackFrameIndex = static_cast<int>(selectedIndex);
                        const auto& selected = m_callStack[selectedIndex];
                        if (!selected.file.empty())
                        {
                            m_debuggerCurrentFile = selected.file;
                            m_debuggerCurrentLine = selected.line;
                            selectedFileToHighlight = selected.file;
                            selectedLineToHighlight = selected.line;
                        }
                    }
                }

                if (!selectedFileToHighlight.empty() && selectedLineToHighlight > 0)
                {
                    highlightDebuggerLine(selectedFileToHighlight, selectedLineToHighlight);
                }
            }
        }
        catch (const std::exception& ex)
        {
            clearNativeFrameContext();
            clearDebuggerHighlight();
            updateVariables();
            updateWatchList();
            DebuggerError err;
            err.type = DebuggerErrorType::FrameStackWalkFailed;
            err.severity = ErrorSeverity::Critical;
            err.message = std::string("Exception during stack walk: ") + ex.what();
            err.isRecoverable = false;
            errorHandler.reportError(err);
        }
        catch (...)
        {
            clearNativeFrameContext();
            clearDebuggerHighlight();
            updateVariables();
            updateWatchList();
            DebuggerError err;
            err.type = DebuggerErrorType::FrameStackWalkFailed;
            err.severity = ErrorSeverity::Critical;
            err.message = "Unknown exception during stack walk";
            err.isRecoverable = false;
            errorHandler.reportError(err);
        }
    }

    ListView_DeleteAllItems(m_hwndDebuggerStackTrace);

    LVITEMA lvi;
    lvi.mask = LVIF_TEXT;

    if (m_callStack.empty())
    {
        lvi.iItem = 0;
        lvi.iSubItem = 0;
        lvi.pszText = const_cast<char*>("Attach and break to inspect call stack");
        ListView_InsertItem(m_hwndDebuggerStackTrace, &lvi);
        lvi.iSubItem = 1;
        lvi.pszText = const_cast<char*>("");
        ListView_SetItem(m_hwndDebuggerStackTrace, &lvi);
        lvi.iSubItem = 2;
        lvi.pszText = const_cast<char*>("");
        ListView_SetItem(m_hwndDebuggerStackTrace, &lvi);
    }
    else
    {
        for (size_t i = 0; i < m_callStack.size(); ++i)
        {
            const auto& frame = m_callStack[i];

            lvi.iItem = static_cast<int>(i);
            lvi.iSubItem = 0;
            lvi.pszText = const_cast<char*>(frame.function.c_str());
            ListView_InsertItem(m_hwndDebuggerStackTrace, &lvi);

            lvi.iSubItem = 1;
            lvi.pszText = const_cast<char*>(frame.file.c_str());
            ListView_SetItem(m_hwndDebuggerStackTrace, &lvi);

            lvi.iSubItem = 2;
            std::string line_str = std::to_string(frame.line);
            lvi.pszText = const_cast<char*>(line_str.c_str());
            ListView_SetItem(m_hwndDebuggerStackTrace, &lvi);
        }
    }

    // Apply selection highlight to the current frame row
    {
        int selIdx = m_selectedStackFrameIndex;
        if (m_hwndDebuggerStackTrace && selIdx >= 0 && selIdx < static_cast<int>(m_callStack.size()))
        {
            ListView_SetItemState(m_hwndDebuggerStackTrace, selIdx, LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(m_hwndDebuggerStackTrace, selIdx, FALSE);
        }
    }

    updateDebuggerErrorStatus();
}

void Win32IDE::selectStackFrame(int index)
{
    auto& frameTracker = DebuggerFrameTrackerInstance::instance();
    auto& errorHandler = DebuggerErrorHandlerInstance::instance();

    auto contextScope = errorHandler.scopedContext("selectStackFrame");

    // Validate index with error reporting
    if (!errorHandler.validateIndex(static_cast<size_t>(index), frameTracker.frameCount(), "selectStackFrame"))
    {
        return;
    }

    // Try to select the frame
    if (!frameTracker.selectFrame(static_cast<size_t>(index)))
    {
        errorHandler.reportError(DebuggerErrorType::FrameIndexOutOfRange, ErrorSeverity::Error,
                                 "Failed to select frame at index " + std::to_string(index), "selectStackFrame");
        return;
    }

    const auto* frame = frameTracker.getCurrentFrame();
    if (!frame)
    {
        errorHandler.reportError(DebuggerErrorType::FrameDataCorrupted, ErrorSeverity::Error, "Selected frame is null",
                                 "selectStackFrame");
        return;
    }

    // Update backward-compatible m_callStack state
    {
        std::lock_guard<std::mutex> lock(m_debuggerMutex);
        m_selectedStackFrameIndex = index;
    }

    // Navigate the editor to the selected frame's source location
    if (!frame->file.empty() && frame->line > 0)
    {
        m_debuggerCurrentFile = frame->file;
        m_debuggerCurrentLine = frame->line;
        highlightDebuggerLine(frame->file, frame->line);
    }

    // For DAP sessions, update the frame context
    if (auto dapSession = getDapSession(this))
    {
        if (frame->dapFrameId >= 0)
        {
            dapSession->currentFrameId = frame->dapFrameId;
        }
    }

    // Trigger variable update for the new frame
    updateVariables();
    updateWatchList();
}

void Win32IDE::navigateFrameBackward()
{
    auto& frameTracker = DebuggerFrameTrackerInstance::instance();
    auto& errorHandler = DebuggerErrorHandlerInstance::instance();

    if (!frameTracker.canNavigateBack())
    {
        errorHandler.reportError(DebuggerErrorType::FrameNavigationLimitReached, ErrorSeverity::Info,
                                 "Cannot navigate to previous frame", "navigateFrameBackward");
        return;
    }

    if (frameTracker.navigateFrameBack())
    {
        // Do NOT call updateCallStack() — that refetches frames from the adapter
        // and calls updateFromDapFrames/updateFromNativeFrames which destroys
        // forward navigation history by unconditionally pushing to FrameHistory.
        // Instead, sync only the UI with the frame the tracker just selected.
        const int selIdx = static_cast<int>(frameTracker.getCurrentFrameIndex());
        {
            std::lock_guard<std::mutex> lock(m_debuggerMutex);
            m_selectedStackFrameIndex = selIdx;
        }
        // ListView: scroll to and highlight selected row
        if (m_hwndDebuggerStackTrace && selIdx >= 0 && selIdx < static_cast<int>(m_callStack.size()))
        {
            ListView_SetItemState(m_hwndDebuggerStackTrace, selIdx, LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(m_hwndDebuggerStackTrace, selIdx, FALSE);
        }
        // Refresh variable panels for new frame context
        updateVariables();
        updateWatchList();
    }
    else
    {
        errorHandler.reportError(DebuggerErrorType::FrameNavigationFailed, ErrorSeverity::Warning,
                                 "Frame navigation backward failed", "navigateFrameBackward");
    }
}

void Win32IDE::navigateFrameForward()
{
    auto& frameTracker = DebuggerFrameTrackerInstance::instance();
    auto& errorHandler = DebuggerErrorHandlerInstance::instance();

    if (!frameTracker.canNavigateForward())
    {
        errorHandler.reportError(DebuggerErrorType::FrameNavigationLimitReached, ErrorSeverity::Info,
                                 "Cannot navigate to next frame", "navigateFrameForward");
        return;
    }

    if (frameTracker.navigateFrameForward())
    {
        // Do NOT call updateCallStack() — same reason as navigateFrameBackward.
        const int selIdx = static_cast<int>(frameTracker.getCurrentFrameIndex());
        {
            std::lock_guard<std::mutex> lock(m_debuggerMutex);
            m_selectedStackFrameIndex = selIdx;
        }
        if (m_hwndDebuggerStackTrace && selIdx >= 0 && selIdx < static_cast<int>(m_callStack.size()))
        {
            ListView_SetItemState(m_hwndDebuggerStackTrace, selIdx, LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(m_hwndDebuggerStackTrace, selIdx, FALSE);
        }
        updateVariables();
        updateWatchList();
    }
    else
    {
        errorHandler.reportError(DebuggerErrorType::FrameNavigationFailed, ErrorSeverity::Warning,
                                 "Frame navigation forward failed", "navigateFrameForward");
    }
}

void Win32IDE::updateDebuggerErrorStatus(const std::string& message)
{
    auto& errorHandler = DebuggerErrorHandlerInstance::instance();
    auto stats = errorHandler.getStatistics();
    const size_t droppedFrames = m_lastDroppedFrameCount.load();

    if (!message.empty())
    {
        std::string statusMessage = rawrxd::win32app::formatDebuggerStatusText(message);

        if (m_hwndDebuggerStatus && IsWindow(m_hwndDebuggerStatus))
        {
            SetWindowTextA(m_hwndDebuggerStatus, statusMessage.c_str());
        }
        else
        {
            appendToOutput("⚠️ Debugger status unavailable: " + statusMessage, "Output", OutputSeverity::Warning);
        }
        return;
    }

    if (stats.totalErrors == 0)
    {
        if (droppedFrames > 0)
        {
            std::ostringstream status;
            status << "Ready | Truncated Frames: " << droppedFrames;
            SetWindowTextA(m_hwndDebuggerStatus, status.str().c_str());
        }
        else
        {
            SetWindowTextA(m_hwndDebuggerStatus, "Ready");
        }
        return;
    }

    std::ostringstream status;
    status << "Errors: " << stats.totalErrors << " | Recovered: " << stats.recoveredErrors;
    if (droppedFrames > 0)
    {
        status << " | Truncated Frames: " << droppedFrames;
    }

    SetWindowTextA(m_hwndDebuggerStatus, status.str().c_str());
}

void Win32IDE::updateMemoryView()
{
    if (!m_hwndDebuggerMemory)
        return;

    if (auto dapSession = getDapSession(this))
    {
        std::ostringstream oss;
        oss << "DAP Memory Inspector\n";
        oss << "===================\n\n";

        if (!m_debuggerAttached || !m_debuggerPaused)
        {
            oss << "Debugger not paused — continue, break, or step to inspect session state.\n";
            SetWindowTextA(m_hwndDebuggerMemory, oss.str().c_str());
            return;
        }

        // --- Register summary via evaluate ---
        // Many DAP adapters expose CPU registers through evaluate("$rsp") etc.
        // We try a set of common expressions; empty result means unsupported.
        struct RegQuery
        {
            const char* label;
            const char* expr;
        };
        static constexpr RegQuery kRegs[] = {
            {"RIP", "$rip"}, {"RSP", "$rsp"}, {"RBP", "$rbp"}, {"RAX", "$rax"},
            {"RBX", "$rbx"}, {"RCX", "$rcx"}, {"RDX", "$rdx"},
        };
        const int fid = dapSession->currentFrameId;
        std::string rspValue;
        bool anyReg = false;

        try
        {
            for (const auto& rq : kRegs)
            {
                try
                {
                    const std::string val = dapSession->service->evaluate(fid, rq.expr, "repl");
                    if (!val.empty())
                    {
                        oss << rq.label << ": " << val << "\n";
                        anyReg = true;
                        if (std::string(rq.expr) == "$rsp")
                            rspValue = val;
                    }
                }
                catch (const std::exception& ex)
                {
                    // Individual register evaluation failed; log and continue with other registers
                    appendToOutput("⚠️ DAP register eval '" + std::string(rq.expr) + "' failed: " + ex.what(), "Output",
                                   OutputSeverity::Debug);
                    // Continue to next register
                }
                catch (...)
                {
                    appendToOutput("❌ DAP register eval '" + std::string(rq.expr) + "': unknown error", "Output",
                                   OutputSeverity::Debug);
                }
            }
        }
        catch (const std::exception& ex)
        {
            appendToOutput("❌ DAP register evaluation loop error: " + std::string(ex.what()), "Output",
                           OutputSeverity::Error);
        }

        if (!anyReg)
        {
            oss << "(Register evaluate not supported by this adapter)\n";
        }
        oss << "\n";

        // --- Stack memory dump via readMemory ---
        // If the adapter supports readMemory (supportsReadMemoryRequest capability),
        // use RSP-value or the instruction pointer as the memory reference.
        bool readDone = false;
        if (!rspValue.empty())
        {
            try
            {
                // rspValue is typically something like "0x7fffd3e4a0" or "7fffd3e4a0"
                const std::string memRef = rspValue;
                static constexpr uint64_t kDumpBytes = 256;
                uint64_t bytesRead = 0;
                const std::string raw = dapSession->service->readMemory(memRef, 0, kDumpBytes, &bytesRead);
                if (!raw.empty() && bytesRead > 0)
                {
                    // Parse the address for display (strip leading "0x" if present)
                    uint64_t baseAddr = 0;
                    try
                    {
                        baseAddr = std::stoull(rspValue, nullptr, 0);
                    }
                    catch (...)
                    {
                    }

                    oss << "Stack Memory @ " << memRef << " (" << bytesRead << " bytes):\n";
                    // Hex dump: 16 bytes per row
                    constexpr int kCols = 16;
                    for (uint64_t row = 0; row < bytesRead; row += kCols)
                    {
                        const uint64_t rowAddr = baseAddr + row;
                        oss << std::hex << std::setw(16) << std::setfill('0') << rowAddr << "  ";
                        const uint64_t rowEnd = std::min<uint64_t>(row + kCols, bytesRead);
                        // Hex bytes
                        for (uint64_t col = row; col < rowEnd; ++col)
                        {
                            oss << std::hex << std::setw(2) << std::setfill('0')
                                << (static_cast<unsigned>(static_cast<unsigned char>(raw[col]))) << " ";
                        }
                        // Padding if last row is short
                        for (uint64_t col = rowEnd; col < row + kCols; ++col)
                            oss << "   ";
                        oss << " |";
                        // ASCII
                        for (uint64_t col = row; col < rowEnd; ++col)
                        {
                            const unsigned char c = static_cast<unsigned char>(raw[col]);
                            oss << (char)(c >= 0x20 && c < 0x7F ? c : '.');
                        }
                        oss << "|\n";
                    }
                    readDone = true;
                }
            }
            catch (const std::exception& ex)
            {
                appendToOutput("⚠️ DAP readMemory failed: " + std::string(ex.what()), "Output", OutputSeverity::Warning);
                oss << "(readMemory call failed: " << ex.what() << ")\n";
            }
            catch (...)
            {
                appendToOutput("❌ DAP readMemory: unknown error", "Output", OutputSeverity::Error);
                oss << "(readMemory call failed: unknown error)\n";
            }
        }
        if (!readDone && rspValue.empty())
        {
            oss << "(readMemory not supported by this adapter or RSP unavailable)\n";
            oss << "Adapter: " << dapSession->adapterExecutable << "\n";
            oss << "Config:  " << dapSession->configName << "\n";
            oss << "Frame:   " << dapSession->currentFrameId << "\n";
        }

        SetWindowTextA(m_hwndDebuggerMemory, oss.str().c_str());
        return;
    }

    std::ostringstream oss;
    oss << "Memory Inspector\n";
    oss << "================\n\n";

    auto& engine = NativeDebuggerEngine::Instance();

    if (m_debuggerAttached && m_debuggerPaused)
    {
        // Show registers summary at the top
        RegisterSnapshot regs;
        DebugResult regR = engine.captureRegisters(regs);
        if (regR.success)
        {
            oss << "RIP: 0x" << std::hex << std::setw(16) << std::setfill('0') << regs.rip << "\n";
            oss << "RSP: 0x" << std::hex << std::setw(16) << std::setfill('0') << regs.rsp << "\n";
            oss << "RBP: 0x" << std::hex << std::setw(16) << std::setfill('0') << regs.rbp << "\n";
            oss << "RAX: 0x" << std::hex << std::setw(16) << std::setfill('0') << regs.rax;
            oss << "  RBX: 0x" << std::hex << std::setw(16) << std::setfill('0') << regs.rbx << "\n";
            oss << "RCX: 0x" << std::hex << std::setw(16) << std::setfill('0') << regs.rcx;
            oss << "  RDX: 0x" << std::hex << std::setw(16) << std::setfill('0') << regs.rdx << "\n";
            oss << "FLAGS: " << engine.formatFlags(regs.rflags) << "\n";
            oss << "\n";

            // Read 256 bytes at RSP (stack memory)
            uint8_t stackBuf[256] = {};
            uint64_t bytesRead = 0;
            DebugResult memR = engine.readMemory(regs.rsp, stackBuf, sizeof(stackBuf), &bytesRead);
            if (memR.success && bytesRead > 0)
            {
                oss << "Stack Memory (RSP):\n";
                oss << engine.formatHexDump(regs.rsp, stackBuf, bytesRead, 16) << "\n";
            }
        }
        else
        {
            oss << "Register capture failed: " << regR.detail << "\n\n";
        }

        // Show memory region stats
        std::vector<MemoryRegion> regions;
        DebugResult qR = engine.queryMemoryRegions(regions);
        if (qR.success)
        {
            uint64_t totalCommit = 0;
            uint64_t totalReserve = 0;
            for (const auto& mr : regions)
            {
                if (mr.state == 0x1000)
                    totalCommit += mr.size;  // MEM_COMMIT
                if (mr.state == 0x2000)
                    totalReserve += mr.size;  // MEM_RESERVE
            }
            oss << std::dec;
            oss << "Memory Regions: " << regions.size() << "\n";
            oss << "Committed: " << (totalCommit / 1024 / 1024) << " MB\n";
            oss << "Reserved:  " << (totalReserve / 1024 / 1024) << " MB\n";
        }
    }
    else
    {
        oss << "Debugger not paused — attach and break to inspect memory.\n\n";
        oss << "Breakpoints: " << m_breakpoints.size() << "\n";
        oss << "Watch Expressions: " << m_watchList.size() << "\n";
        oss << "Stack Depth: " << m_callStack.size() << " frames\n";
    }

    SetWindowTextA(m_hwndDebuggerMemory, oss.str().c_str());
}

void Win32IDE::updateDebuggerUI()
{
    updateBreakpointList();
    updateWatchList();
    updateVariables();
    updateCallStack();
    updateMemoryView();
}

// ============================================================================
// DEBUGGER CALLBACKS
// ============================================================================

void Win32IDE::onDebuggerBreakpoint(const std::string& file, int line)
{
    m_debuggerPaused = true;
    m_debuggerCurrentFile = file;
    m_debuggerCurrentLine = line;

    // Update hit count on matching UI breakpoint
    for (auto& bp : m_breakpoints)
    {
        if (bp.file == file && bp.line == line)
        {
            bp.hitCount++;
            break;
        }
    }

    std::string msg = "🔴 Breakpoint hit at " + file + ":" + std::to_string(line);
    appendToOutput(msg, "Output", OutputSeverity::Warning);
    SetWindowTextA(m_hwndDebuggerStatus, ("⏸ Break: " + file + ":" + std::to_string(line)).c_str());

    highlightDebuggerLine(file, line);

    // Refresh all inspection panels with live data
    updateVariables();
    updateCallStack();
    updateMemoryView();
    updateBreakpointList();  // Show updated hit counts
}

void Win32IDE::onDebuggerException(const std::string& message)
{
    pauseExecution();
    std::string msg = "⚠️ Exception: " + message;
    appendToOutput(msg, "Output", OutputSeverity::Error);
}

void Win32IDE::onDebuggerOutput(const std::string& text)
{
    appendToOutput("📤 " + text, "Output", OutputSeverity::Debug);
}

void Win32IDE::onDebuggerContinued()
{
    m_debuggerPaused = false;
    m_lastDroppedFrameCount.store(0);
    {
        std::lock_guard<std::mutex> lock(m_debuggerMutex);
        m_callStack.clear();
        m_localVariables.clear();
        m_selectedStackFrameIndex = 0;
        m_pendingEvalRequests.clear();
        m_lastFrameUpdateTime = std::chrono::steady_clock::time_point{};
        m_lastFrameRefreshAttemptTime = std::chrono::steady_clock::time_point{};
        m_debuggerReattachPending.store(false);
        m_lastDebuggerReattachRequestTime = std::chrono::steady_clock::time_point{};
        m_debuggerCurrentFile.clear();
        m_debuggerCurrentLine = -1;
    }
    if (auto dapSession = getDapSession(this))
    {
        dapSession->currentFrameId = -1;
    }
    // Clear stale frames from previous pause — frames are invalid once execution resumes
    DebuggerFrameTrackerInstance::instance().clear();
    SetWindowTextA(m_hwndDebuggerStatus, getDapSession(this) ? "▶ DAP target running" : "▶ Debugger Running");
    clearDebuggerHighlight();
    updateDebuggerUI();
}

void Win32IDE::onDebuggerTerminated()
{
    const bool wasDapSession = (getDapSession(this) != nullptr);
    if (wasDapSession)
    {
        if (auto dapSession = getDapSession(this))
        {
            dapSession->service->stopAdapter();
        }
        storeDapSession(this, nullptr);
    }

    m_debuggerAttached = false;
    m_debuggerPaused = false;
    m_callStack.clear();
    m_localVariables.clear();
    {
        std::lock_guard<std::mutex> lock(m_debuggerMutex);
        m_pendingEvalRequests.clear();
        m_lastFrameUpdateTime = std::chrono::steady_clock::time_point{};
        m_lastFrameRefreshAttemptTime = std::chrono::steady_clock::time_point{};
        m_debuggerReattachPending.store(false);
        m_lastDebuggerReattachRequestTime = std::chrono::steady_clock::time_point{};
    }
    m_debuggerCurrentFile.clear();
    m_debuggerCurrentLine = -1;
    m_lastDroppedFrameCount.store(0);

    // Reset frame tracker and error handler for clean next session
    DebuggerFrameTrackerInstance::instance().clear();
    DebuggerFrameTrackerInstance::instance().setFrameContextCallback(nullptr);
    DebuggerFrameTrackerInstance::instance().setErrorReportCallback(nullptr);
    auto& errorHandler = DebuggerErrorHandlerInstance::instance();
    errorHandler.setErrorCallback(nullptr);
    errorHandler.setRecoveryCallback(nullptr);
    errorHandler.clear();

    SetWindowTextA(m_hwndDebuggerStatus, wasDapSession ? "⏹ DAP target terminated" : "⏹ Target terminated");
    appendToOutput(wasDapSession ? "⏹ DAP debug target terminated" : "⏹ Debug target terminated", "Output",
                   OutputSeverity::Info);
    clearDebuggerHighlight();
    updateDebuggerUI();
}

void Win32IDE::handlePostedDapEvent(const char* payloadJson)
{
    if (!payloadJson)
    {
        return;
    }

    const nlohmann::json payload = nlohmann::json::parse(payloadJson, nullptr, false);
    if (payload.is_discarded() || !payload.is_object())
    {
        appendToOutput("❌ DAP event payload parse failed", "Output", OutputSeverity::Error);
        return;
    }

    const std::string event = payload.value("event", "");
    const nlohmann::json body = payload.value("body", nlohmann::json::object());
    auto session = getDapSession(this);

    if (event == "initialized")
    {
        if (session)
        {
            session->initializedSeen = true;
        }
        return;
    }

    if (event == "stopped")
    {
        if (!session)
        {
            return;
        }

        try
        {
            session->currentThreadId = body.value("threadId", session->currentThreadId);
            if (auto dapSession = getDapSession(this))
            {
                dapSession->currentFrameId = -1;
            }
            session->currentFrameId = -1;  // Temporarily invalidate
            m_debuggerPaused = true;
            m_callStack.clear();
            m_localVariables.clear();
            m_selectedStackFrameIndex = 0;
            m_lastFrameUpdateTime = std::chrono::steady_clock::now();

            const std::string reason = body.value("reason", "");
            m_debuggerCurrentFile.clear();
            m_debuggerCurrentLine = -1;
            const std::string description = body.value("description", "");
            m_lastDroppedFrameCount.store(0);
            std::string statusText = "⏸ Paused";
            if (!description.empty())
            {
                statusText = "⏸ " + description;
            }
            else if (reason == "breakpoint")
            {
                statusText = "⏸ Breakpoint hit";
            }
            else if (reason == "exception")
            {
                const std::string excText = body.value("text", "");
                statusText = "⏸ Exception" + (excText.empty() ? "" : ": " + excText);
                appendToOutput("⚠️ DAP exception: " + (excText.empty() ? "(unknown)" : excText), "Output",
                               OutputSeverity::Error);
            }
            else if (reason == "step")
            {
                statusText = "⏸ Step complete";
            }
            else if (reason == "pause")
            {
                statusText = "⏸ Paused by user";
            }
            else if (!reason.empty())
            {
                statusText = "⏸ Stopped: " + reason;
            }
            SetWindowTextA(m_hwndDebuggerStatus, statusText.c_str());

            try
            {
                updateCallStack();
            }
            catch (const std::exception& ex)
            {
                appendToOutput(std::string("⚠️ Call stack update failed: ") + ex.what(), "Output",
                               OutputSeverity::Warning);
            }
            catch (...)
            {
                appendToOutput("❌ Call stack update: unknown error", "Output", OutputSeverity::Error);
            }

            // After updateCallStack(), sync the DAP frame ID to the currently selected frame.
            {
                std::lock_guard<std::mutex> lock(m_debuggerMutex);
                if (!m_callStack.empty() && m_selectedStackFrameIndex >= 0 &&
                    static_cast<size_t>(m_selectedStackFrameIndex) < m_callStack.size() &&
                    m_callStack[static_cast<size_t>(m_selectedStackFrameIndex)].dapFrameId >= 0)
                {
                    session->currentFrameId = m_callStack[static_cast<size_t>(m_selectedStackFrameIndex)].dapFrameId;
                }
            }

            try
            {
                updateVariables();
            }
            catch (const std::exception& ex)
            {
                appendToOutput(std::string("⚠️ Variables update failed: ") + ex.what(), "Output",
                               OutputSeverity::Warning);
            }
            catch (...)
            {
                appendToOutput("❌ Variables update: unknown error", "Output", OutputSeverity::Error);
            }

            try
            {
                updateWatchList();
            }
            catch (const std::exception& ex)
            {
                appendToOutput(std::string("⚠️ Watch list update failed: ") + ex.what(), "Output",
                               OutputSeverity::Warning);
            }
            catch (...)
            {
                appendToOutput("❌ Watch list update: unknown error", "Output", OutputSeverity::Error);
            }

            try
            {
                updateMemoryView();
            }
            catch (const std::exception& ex)
            {
                appendToOutput(std::string("⚠️ Memory view update failed: ") + ex.what(), "Output",
                               OutputSeverity::Warning);
            }
            catch (...)
            {
                appendToOutput("❌ Memory view update: unknown error", "Output", OutputSeverity::Error);
            }

            // Process any deferred evaluations now that frame is valid
            try
            {
                processPendingEvalRequests();
            }
            catch (const std::exception& ex)
            {
                appendToOutput(std::string("⚠️ Pending eval processing failed: ") + ex.what(), "Output",
                               OutputSeverity::Debug);
            }
            catch (...)
            {
                appendToOutput("❌ Pending eval processing: unknown error", "Output", OutputSeverity::Debug);
            }
        }
        catch (const std::exception& ex)
        {
            appendToOutput(std::string("❌ DAP 'stopped' event handler error: ") + ex.what(), "Output",
                           OutputSeverity::Error);
        }
        catch (...)
        {
            appendToOutput("❌ DAP 'stopped' event handler: unknown error", "Output", OutputSeverity::Error);
        }

        return;
    }

    if (event == "continued")
    {
        if (session)
        {
            session->currentFrameId = -1;
        }
        try
        {
            onDebuggerContinued();
        }
        catch (const std::exception& ex)
        {
            appendToOutput(std::string("⚠️ Continued event handler failed: ") + ex.what(), "Output",
                           OutputSeverity::Warning);
        }
        catch (...)
        {
            appendToOutput("❌ Continued event handler: unknown error", "Output", OutputSeverity::Error);
        }
        // New stop event starts a fresh frame-selection epoch.
        DebuggerFrameTrackerInstance::instance().clear();
        m_lastDroppedFrameCount.store(0);
        {
            std::lock_guard<std::mutex> lock(m_debuggerMutex);
            m_callStack.clear();
            m_localVariables.clear();
            m_selectedStackFrameIndex = 0;
            m_pendingEvalRequests.clear();
            m_lastFrameUpdateTime = std::chrono::steady_clock::now();
            m_lastFrameRefreshAttemptTime = std::chrono::steady_clock::time_point{};
            m_debuggerCurrentFile.clear();
            m_debuggerCurrentLine = -1;
        }
        try
        {
            const std::string text = body.value("output", "");
        }
        catch (const std::exception& ex)
        {
            appendToOutput(std::string("⚠️ Output event handler failed: ") + ex.what(), "Output", OutputSeverity::Debug);
        }
        catch (...)
        {
            // Silently ignore unknown errors for debug output events
        }
        return;
    }

    if (event == "terminated" || event == "exited")
    {
        try
        {
            onDebuggerTerminated();
        }
        catch (const std::exception& ex)
        {
            appendToOutput(std::string("⚠️ Termination handler failed: ") + ex.what(), "Output",
                           OutputSeverity::Warning);
        }
        catch (...)
        {
            appendToOutput("❌ Termination handler: unknown error", "Output", OutputSeverity::Error);
        }
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

// Process pending evaluations after frame context becomes valid
void Win32IDE::processPendingEvalRequests()
{
    std::vector<DeferredEvalRequest> pending;
    {
        std::lock_guard<std::mutex> lock(m_debuggerMutex);
        auto dapSession = getDapSession(this);
        if (!dapSession || dapSession->currentFrameId < 0 || !m_debuggerPaused)
        {
            return;  // Frame still not valid
        }
        pending.swap(m_pendingEvalRequests);
    }

    auto now = std::chrono::steady_clock::now();
    for (const auto& req : pending)
    {
        // Skip if request has timed out (> 10 seconds)
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - req.requestTime).count();
        if (elapsed > 10000)
        {
            appendToOutput("⏱️ Watch evaluation timeout for: " + req.expression, "Output", OutputSeverity::Warning);
            continue;
        }

        // Resolve request against current watch list and evaluate through timeout-safe path.
        bool found = false;
        for (auto& item : m_watchList)
        {
            if (item.expression == req.expression)
            {
                found = true;
                evaluateWatch(item);
                if (item.type == "pending")
                {
                    // Still waiting on a valid frame context; preserve original request age.
                    std::lock_guard<std::mutex> lock(m_debuggerMutex);
                    m_pendingEvalRequests.push_back(req);
                }
                break;
            }
        }

        if (!found)
        {
            // Watch was removed; drop the deferred request.
            continue;
        }
    }
}

// Queue an evaluation request to be processed when frame becomes valid
void Win32IDE::queueEvalRequest(const std::string& expression, int evalContext)
{
    std::lock_guard<std::mutex> lock(m_debuggerMutex);

    // Avoid unbounded duplicate entries for the same expression while waiting.
    for (auto& req : m_pendingEvalRequests)
    {
        if (req.expression == expression)
        {
            req.evalContext = evalContext;
            req.requestTime = std::chrono::steady_clock::now();
            return;
        }
    }

    if (m_pendingEvalRequests.size() >= MAX_PENDING_EVAL_REQUESTS)
    {
        m_pendingEvalRequests.erase(m_pendingEvalRequests.begin());
    }

    m_pendingEvalRequests.push_back({expression, evalContext, std::chrono::steady_clock::now()});
}

void Win32IDE::highlightDebuggerLine(const std::string& file, int line)
{
    m_debuggerCurrentFile = file;
    m_debuggerCurrentLine = line;

    // Open the file in the editor if it's not the current file
    if (!file.empty() && file != m_currentFile)
    {
        openFile(file);
    }

    // Scroll to the line and highlight it in the editor
    if (m_hwndEditor && line > 0)
    {
        // Calculate character index for the target line
        int charIndex = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, line - 1, 0);
        if (charIndex >= 0)
        {
            int lineLen = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, charIndex, 0);
            // Select the entire line to highlight it
            CHARRANGE cr = {charIndex, charIndex + lineLen};
            SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
            SendMessage(m_hwndEditor, EM_SCROLLCARET, 0, 0);
            // Apply yellow background highlight via CHARFORMAT2
            CHARFORMAT2A cf = {};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_BACKCOLOR;
            cf.crBackColor = RGB(255, 255, 180);  // Light yellow for breakpoint line
            SendMessageA(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        }
    }

    // Update the debugger status bar
    if (m_hwndDebuggerStatus)
    {
        std::string status = "\xe2\x96\xb6 " + file + ":" + std::to_string(line);
        SetWindowTextA(m_hwndDebuggerStatus, status.c_str());
    }
}

void Win32IDE::clearDebuggerHighlight()
{
    // Clear the highlight in the editor by resetting background color
    if (m_hwndEditor && m_debuggerCurrentLine > 0)
    {
        int charIndex = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, m_debuggerCurrentLine - 1, 0);
        if (charIndex >= 0)
        {
            int lineLen = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, charIndex, 0);
            CHARRANGE cr = {charIndex, charIndex + lineLen};
            SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
            // Reset to default background
            CHARFORMAT2A cf = {};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_BACKCOLOR;
            cf.dwEffects = CFE_AUTOBACKCOLOR;
            SendMessageA(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            // Deselect
            CHARRANGE desel = {0, 0};
            SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&desel);
        }
    }
    m_debuggerCurrentFile = "";
    m_debuggerCurrentLine = -1;
}

bool Win32IDE::isBreakpointAtLine(const std::string& file, int line) const
{
    return std::any_of(m_breakpoints.begin(), m_breakpoints.end(),
                       [&](const Breakpoint& bp) { return bp.file == file && bp.line == line && bp.enabled; });
}

void Win32IDE::expandVariable(const std::string& name)
{
    auto it = std::find_if(m_localVariables.begin(), m_localVariables.end(),
                           [&](const Variable& var) { return var.name == name; });
    if (it == m_localVariables.end())
        return;

    it->expanded = true;

    // For DAP sessions: lazily fetch children if not already loaded
    if (it->variablesReference > 0 && it->children.empty())
    {
        if (auto dapSession = getDapSession(this))
        {
            try
            {
                const auto entries = dapSession->service->variables(it->variablesReference);
                for (const auto& entry : entries)
                {
                    Variable child;
                    child.name = entry.value("name", "");
                    child.value = entry.value("value", "");
                    child.type = entry.value("type", "dap");
                    child.variablesReference = entry.value("variablesReference", 0);
                    child.expanded = false;
                    it->children.push_back(std::move(child));
                }
            }
            catch (const std::exception& ex)
            {
                appendToOutput(std::string("❌ DAP expand error: ") + ex.what(), "Output", OutputSeverity::Error);
            }
            catch (...)
            {
                appendToOutput("❌ DAP expand: unknown error", "Output", OutputSeverity::Error);
            }
        }
    }

    updateVariables();
}

void Win32IDE::collapseVariable(const std::string& name)
{
    auto it = std::find_if(m_localVariables.begin(), m_localVariables.end(),
                           [&](const Variable& var) { return var.name == name; });

    if (it != m_localVariables.end())
    {
        it->expanded = false;
        it->children.clear();  // discard so children are re-fetched fresh on next expand
        updateVariables();
    }
}

void Win32IDE::expandVariableTreeItem(int variablesReference, HTREEITEM hItemParent)
{
    if (!m_hwndDebuggerVariables || variablesReference <= 0)
        return;

    auto dapSession = getDapSession(this);
    if (!dapSession)
        return;

    try
    {
        const auto entries = dapSession->service->variables(variablesReference);
        for (const auto& entry : entries)
        {
            const std::string cname = entry.value("name", "");
            const std::string cvalue = entry.value("value", "");
            const std::string ctype = entry.value("type", "dap");
            const int childRef = entry.value("variablesReference", 0);

            std::string childText = cname + " = " + cvalue + " (" + ctype + ")";

            TVINSERTSTRUCTA childTvis = {};
            childTvis.hParent = hItemParent;
            childTvis.hInsertAfter = TVI_LAST;
            childTvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
            childTvis.item.pszText = const_cast<char*>(childText.c_str());
            childTvis.item.lParam = static_cast<LPARAM>(childRef);
            childTvis.item.cChildren = (childRef > 0) ? 1 : 0;
            TreeView_InsertItem(m_hwndDebuggerVariables, &childTvis);
        }
    }
    catch (const std::exception& ex)
    {
        appendToOutput(std::string("❌ DAP variable expand error: ") + ex.what(), "Output", OutputSeverity::Error);
    }
    catch (...)
    {
        appendToOutput("❌ DAP variable expand: unknown error", "Output", OutputSeverity::Error);
    }
}

void Win32IDE::expandDebuggerVariableNode(HTREEITEM hItem)
{
    if (!m_hwndDebuggerVariables || !hItem)
        return;

    // Avoid re-querying DAP children if this node has already been populated.
    if (TreeView_GetChild(m_hwndDebuggerVariables, hItem) != nullptr)
    {
        return;
    }

    TVITEMA item = {};
    item.mask = TVIF_PARAM;
    item.hItem = hItem;
    if (!TreeView_GetItem(m_hwndDebuggerVariables, &item))
    {
        return;
    }

    const int variablesReference = static_cast<int>(item.lParam);
    if (variablesReference <= 0)
    {
        return;
    }

    expandVariableTreeItem(variablesReference, hItem);
}

std::string Win32IDE::formatDebuggerValue(const std::string& value, const std::string& type)
{
    return "(" + type + ") " + value;
}

void Win32IDE::debuggerStepCommand(const std::string& command)
{
    if (command == "over")
    {
        stepOverExecution();
    }
    else if (command == "into")
    {
        stepIntoExecution();
    }
    else if (command == "out")
    {
        stepOutExecution();
    }
    else if (command == "continue")
    {
        resumeExecution();
    }
    else if (command == "pause")
    {
        pauseExecution();
    }
}

void Win32IDE::debuggerSetVariable(const std::string& name, const std::string& value)
{
    // Try to set via engine expression evaluation (e.g., "varName = newValue")
    auto& engine = NativeDebuggerEngine::Instance();
    std::string assignExpr = name + " = " + value;
    EvalResult evalRes;
    DebugResult r = engine.evaluate(assignExpr, evalRes);

    if (r.success)
    {
        // Update our local cache too
        auto it = std::find_if(m_localVariables.begin(), m_localVariables.end(),
                               [&](const Variable& var) { return var.name == name; });
        if (it != m_localVariables.end())
        {
            it->value = value;
        }
        updateVariables();

        std::string msg = "✏️ Set " + name + " = " + value + " (engine confirmed)";
        appendToOutput(msg, "Output", OutputSeverity::Info);
    }
    else
    {
        // Fallback: update UI-only cache
        auto it = std::find_if(m_localVariables.begin(), m_localVariables.end(),
                               [&](const Variable& var) { return var.name == name; });
        if (it != m_localVariables.end())
        {
            it->value = value;
            updateVariables();
        }

        std::string msg = "✏️ Set " + name + " = " + value + " (UI only — engine: ";
        msg += r.detail;
        msg += ")";
        appendToOutput(msg, "Output", OutputSeverity::Warning);
    }
}

void Win32IDE::debuggerInspectMemory(uint64_t address, size_t bytes)
{
    auto& engine = NativeDebuggerEngine::Instance();

    // Cap at 4KB to prevent UI overload
    if (bytes > 4096)
        bytes = 4096;

    std::vector<uint8_t> buffer(bytes, 0);
    uint64_t bytesRead = 0;
    DebugResult r = engine.readMemory(address, buffer.data(), bytes, &bytesRead);

    std::ostringstream oss;
    oss << "Memory at 0x" << std::hex << address << " (" << std::dec << bytes << " bytes requested):\n";

    if (r.success && bytesRead > 0)
    {
        oss << engine.formatHexDump(address, buffer.data(), bytesRead, 16);
    }
    else
    {
        oss << "Read failed: " << r.detail << "\n";
    }

    appendToOutput(oss.str(), "Output", OutputSeverity::Debug);

    // Also update the memory view edit control with this dump
    if (m_hwndDebuggerMemory && r.success && bytesRead > 0)
    {
        std::string dump = engine.formatHexDump(address, buffer.data(), bytesRead, 16);
        SetWindowTextA(m_hwndDebuggerMemory, dump.c_str());
    }
}

void Win32IDE::debuggerEvaluateExpression(const std::string& expression)
{
    auto& engine = NativeDebuggerEngine::Instance();
    EvalResult evalRes;
    DebugResult r = engine.evaluate(expression, evalRes);

    std::string msg;
    if (r.success)
    {
        msg = "📐 " + expression + " = " + evalRes.value;
        if (!evalRes.type.empty())
        {
            msg += " (" + evalRes.type + ")";
        }
        if (evalRes.isPointer)
        {
            std::ostringstream addrStr;
            addrStr << " [0x" << std::hex << evalRes.rawValue << "]";
            msg += addrStr.str();
        }
    }
    else
    {
        msg = "📐 " + expression + " = <error: ";
        msg += r.detail;
        msg += ">";
    }
    appendToOutput(msg, "Output", OutputSeverity::Debug);
}

void Win32IDE::toggleDebugger()
{
    if (m_debuggerAttached)
    {
        detachDebugger();
    }
    else
    {
        attachDebugger();
    }
}

void Win32IDE::onStackFrameSelected(int index)
{
    selectStackFrame(index);
}

// ============================================================================
// DEBUGGER CONTAINER SUBCLASS — routes WM_NOTIFY from child list views
// ============================================================================

LRESULT CALLBACK Win32IDE::DebuggerContainerProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = reinterpret_cast<Win32IDE*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));

    if (uMsg == WM_NOTIFY && pThis)
    {
        const NMHDR* nmhdr = reinterpret_cast<const NMHDR*>(lParam);
        if (nmhdr && nmhdr->hwndFrom == pThis->m_hwndDebuggerStackTrace)
        {
            if (nmhdr->code == LVN_ITEMCHANGED)
            {
                const NMLISTVIEW* nmlv = reinterpret_cast<const NMLISTVIEW*>(lParam);
                // Only act when a row transitions to selected (not on deselect noise)
                if ((nmlv->uNewState & LVIS_SELECTED) && !(nmlv->uOldState & LVIS_SELECTED))
                {
                    pThis->selectStackFrame(nmlv->iItem);
                }
            }
        }
        else if (nmhdr && nmhdr->hwndFrom == pThis->m_hwndDebuggerVariables)
        {
            if (nmhdr->code == TVN_ITEMEXPANDINGA)
            {
                const NMTREEVIEWA* nmtv = reinterpret_cast<const NMTREEVIEWA*>(lParam);
                if ((nmtv->action & TVE_EXPAND) != 0)
                {
                    pThis->expandDebuggerVariableNode(nmtv->itemNew.hItem);
                }
            }
        }
    }

    WNDPROC oldProc = pThis ? pThis->m_oldDebuggerContainerProc : nullptr;
    if (oldProc)
        return CallWindowProcA(oldProc, hwnd, uMsg, wParam, lParam);
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}
