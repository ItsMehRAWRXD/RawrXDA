// ============================================================================
// Win32IDE_Session.cpp — Session Persistence
// ============================================================================
// Saves and restores IDE state across launches:
//   - Open tabs (file paths, display names, active tab)
//   - Editor cursor position and scroll offset
//   - Panel visibility and height
//   - Sidebar state and width
//   - Loaded model path for inference/chat restore
//   - Current working directory
//
// Session file: %APPDATA%\RawrXD\session.json
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <psapi.h>
#include <shlobj.h>
#include <richedit.h>
#include <sstream>

namespace
{
static constexpr UINT_PTR kSessionSaveDebounceTimerId = 0x7D13;

int clampDpiForSession(UINT dpi)
{
    return dpi > 0 ? static_cast<int>(dpi) : 96;
}

int toLogicalWidth96(int physicalWidth, UINT currentDpi)
{
    return MulDiv(physicalWidth, 96, clampDpiForSession(currentDpi));
}

int toPhysicalWidthFrom96(int logicalWidth96, UINT currentDpi)
{
    return MulDiv(logicalWidth96, clampDpiForSession(currentDpi), 96);
}
}

namespace
{
const char* snapStateToString(SnapState state)
{
    switch (state)
    {
        case SnapState::Compact:
            return "compact";
        case SnapState::Standard:
            return "standard";
        case SnapState::Wide:
            return "wide";
        case SnapState::Custom:
            return "custom";
        default:
            return "none";
    }
}

SnapState snapStateFromString(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (value == "compact")
        return SnapState::Compact;
    if (value == "standard")
        return SnapState::Standard;
    if (value == "wide")
        return SnapState::Wide;
    if (value == "custom")
        return SnapState::Custom;
    return SnapState::None;
}

uint64_t fileTimeToUInt64(const FILETIME& ft)
{
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return u.QuadPart;
}

std::string makeTimestampTag()
{
    SYSTEMTIME st{};
    GetLocalTime(&st);
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << st.wYear
        << std::setw(2) << st.wMonth
        << std::setw(2) << st.wDay
        << "_"
        << std::setw(2) << st.wHour
        << std::setw(2) << st.wMinute
        << std::setw(2) << st.wSecond;
    return oss.str();
}

const char* sidebarViewToString(Win32IDE::SidebarView view)
{
    switch (view)
    {
        case Win32IDE::SidebarView::Explorer:
            return "explorer";
        case Win32IDE::SidebarView::Search:
            return "search";
        case Win32IDE::SidebarView::SourceControl:
            return "sourceControl";
        case Win32IDE::SidebarView::RunDebug:
            return "runDebug";
        case Win32IDE::SidebarView::Extensions:
            return "extensions";
        case Win32IDE::SidebarView::DiskRecovery:
            return "diskRecovery";
        default:
            return "none";
    }
}

Win32IDE::SidebarView sidebarViewFromString(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (value == "explorer")
        return Win32IDE::SidebarView::Explorer;
    if (value == "search")
        return Win32IDE::SidebarView::Search;
    if (value == "sourcecontrol")
        return Win32IDE::SidebarView::SourceControl;
    if (value == "rundebug")
        return Win32IDE::SidebarView::RunDebug;
    if (value == "extensions")
        return Win32IDE::SidebarView::Extensions;
    if (value == "diskrecovery")
        return Win32IDE::SidebarView::DiskRecovery;
    return Win32IDE::SidebarView::None;
}

std::wstring utf8ToWideLocal(const std::string& utf8)
{
    if (utf8.empty())
        return std::wstring();

    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (length <= 1)
        return std::wstring();

    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], length);
    wide.resize(static_cast<size_t>(length - 1));
    return wide;
}

std::string wideToUtf8Local(const std::wstring& wide)
{
    if (wide.empty())
        return std::string();

    const int length = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1)
        return std::string();

    std::string utf8(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], length, nullptr, nullptr);
    utf8.resize(static_cast<size_t>(length - 1));
    return utf8;
}

std::string trimAscii(std::string value)
{
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string sanitizeProfileFileName(std::string value)
{
    value = trimAscii(value);
    std::replace(value.begin(), value.end(), ' ', '-');

    for (char& ch : value)
    {
        const bool allowed = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
                             ch == '-' || ch == '_';
        if (!allowed)
            ch = '-';
    }

    while (value.find("--") != std::string::npos)
    {
        value.erase(value.find("--"), 1);
    }

    if (value.empty())
        value = "profile";
    return value;
}

bool equalsIgnoreCase(std::string lhs, std::string rhs)
{
    std::transform(lhs.begin(), lhs.end(), lhs.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::transform(rhs.begin(), rhs.end(), rhs.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lhs == rhs;
}

int snapStateToPresetWidth96(SnapState state)
{
    switch (state)
    {
        case SnapState::Compact:
            return 240;
        case SnapState::Standard:
            return 360;
        case SnapState::Wide:
            return 480;
        default:
            return 0;
    }
}
}

// ============================================================================
// SESSION FILE PATH
// ============================================================================
std::string Win32IDE::getSessionFilePath() const {
    // Use %APPDATA%\RawrXD\session.json
    char appDataPath[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath))) {
        std::string dir = std::string(appDataPath) + "\\RawrXD";
        CreateDirectoryA(dir.c_str(), nullptr);
        return dir + "\\session.json";
    }
    // Fallback: use executable directory
    return "session.json";
}

std::string Win32IDE::getLayoutProfilesDirectory() const
{
    try
    {
        return (std::filesystem::current_path() / ".rawrxd" / "profiles").string();
    }
    catch (...)
    {
        return ".rawrxd\\profiles";
    }
}

Win32IDE::LayoutProfile Win32IDE::captureCurrentLayoutProfile(const std::string& name) const
{
    LayoutProfile profile;
    profile.name = name;
    profile.snapState = m_activeSnapState;
    profile.sidebarWidth96 = toLogicalWidth96(m_sidebarWidth, m_currentDpi);
    profile.secondarySidebarWidth96 = toLogicalWidth96(m_secondarySidebarWidth, m_currentDpi);
    profile.bottomPanelHeight96 = toLogicalWidth96(m_panelHeight, m_currentDpi);
    profile.sidebarView = m_sidebarVisible ? m_currentSidebarView : SidebarView::None;
    profile.rightView = m_secondarySidebarVisible ? "agent" : "none";
    profile.panelTerminal = m_panelVisible && m_activePanelTab == PanelTab::Terminal;
    profile.panelAgent = m_secondarySidebarVisible;
    profile.panelProblems = m_panelVisible && m_activePanelTab == PanelTab::Problems;
    return profile;
}

bool Win32IDE::saveLayoutProfile(const LayoutProfile& profile) const
{
    if (trimAscii(profile.name).empty())
        return false;

    try
    {
        const std::filesystem::path dir(getLayoutProfilesDirectory());
        std::filesystem::create_directories(dir);

        nlohmann::json root;
        root["version"] = 1;
        root["name"] = profile.name;
        root["snap"] = snapStateToString(profile.snapState);
        root["sidebarWidth96"] = profile.sidebarWidth96;
        root["secondarySidebarWidth96"] = profile.secondarySidebarWidth96;
        root["bottomPanelHeight96"] = profile.bottomPanelHeight96;
        root["sidebarView"] = sidebarViewToString(profile.sidebarView);
        root["rightView"] = profile.rightView;
        root["panels"] = {
            {"terminal", profile.panelTerminal},
            {"agent", profile.panelAgent},
            {"problems", profile.panelProblems},
        };

        const std::filesystem::path filePath = dir / (sanitizeProfileFileName(profile.name) + ".json");
        std::ofstream out(filePath, std::ios::binary);
        if (!out.is_open())
            return false;

        out << root.dump(2);
        return out.good();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to save layout profile: " + std::string(e.what()));
        return false;
    }
}

bool Win32IDE::loadLayoutProfile(const std::string& name, LayoutProfile& profile) const
{
    if (trimAscii(name).empty())
        return false;

    try
    {
        const std::filesystem::path filePath = std::filesystem::path(getLayoutProfilesDirectory()) /
                                               (sanitizeProfileFileName(name) + ".json");
        std::ifstream in(filePath, std::ios::binary);
        if (!in.is_open())
            return false;

        const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        nlohmann::json root = nlohmann::json::parse(content);

        profile.name = root.value("name", name);
        profile.snapState = snapStateFromString(root.value("snap", "custom"));
        profile.sidebarWidth96 = root.value("sidebarWidth96", 240);
        profile.secondarySidebarWidth96 = root.value("secondarySidebarWidth96", 320);
        profile.bottomPanelHeight96 = root.value("bottomPanelHeight96", 250);
        profile.sidebarView = sidebarViewFromString(root.value("sidebarView", "none"));
        profile.rightView = root.value("rightView", "none");

        const auto panels = root.value("panels", nlohmann::json::object());
        profile.panelTerminal = panels.value("terminal", false);
        profile.panelAgent = panels.value("agent", equalsIgnoreCase(profile.rightView, "agent"));
        profile.panelProblems = panels.value("problems", false);
        return true;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to load layout profile: " + std::string(e.what()));
        return false;
    }
}

void Win32IDE::applyLayoutProfile(const LayoutProfile& profile, bool persistSession)
{
    const int presetWidth96 = snapStateToPresetWidth96(profile.snapState);
    if (presetWidth96 > 0)
    {
        m_activeSnapState = profile.snapState;
        m_sidebarWidth = toPhysicalWidthFrom96(presetWidth96, m_currentDpi);
        m_secondarySidebarWidth = toPhysicalWidthFrom96(presetWidth96, m_currentDpi);
    }
    else
    {
        m_activeSnapState = profile.snapState;
        if (profile.sidebarWidth96 > 0)
            m_sidebarWidth = toPhysicalWidthFrom96(profile.sidebarWidth96, m_currentDpi);
        if (profile.secondarySidebarWidth96 > 0)
            m_secondarySidebarWidth = toPhysicalWidthFrom96(profile.secondarySidebarWidth96, m_currentDpi);
    }

    if (profile.bottomPanelHeight96 > 0)
        m_panelHeight = toPhysicalWidthFrom96(profile.bottomPanelHeight96, m_currentDpi);

    const SidebarView targetSidebarView = profile.sidebarView;
    const bool showSidebar = targetSidebarView != SidebarView::None;
    const bool showAgentPanel = profile.panelAgent || equalsIgnoreCase(profile.rightView, "agent");
    const bool showBottomPanel = profile.panelTerminal || profile.panelProblems;
    const PanelTab targetPanelTab = profile.panelProblems ? PanelTab::Problems : PanelTab::Terminal;

    m_sidebarVisible = showSidebar;
    m_secondarySidebarVisible = showAgentPanel;
    m_panelVisible = showBottomPanel;
    m_panelMaximized = false;
    m_activePanelTab = targetPanelTab;

    if (m_hwndSidebar)
        ShowWindow(m_hwndSidebar, m_sidebarVisible ? SW_SHOW : SW_HIDE);
    if (m_hwndSecondarySidebar)
        ShowWindow(m_hwndSecondarySidebar, m_secondarySidebarVisible ? SW_SHOW : SW_HIDE);
    if (m_hwndPanelContainer)
        ShowWindow(m_hwndPanelContainer, m_panelVisible ? SW_SHOW : SW_HIDE);

    if (m_sidebarVisible && m_hwndSidebarContent)
        setSidebarView(targetSidebarView);
    else
        m_currentSidebarView = targetSidebarView;

    if (m_hwndPanelTabs)
        switchPanelTab(targetPanelTab);

    updateSovereignSnapMenuChecks();

    if (m_hwndMain && IsWindow(m_hwndMain))
    {
        RECT rc = {};
        GetClientRect(m_hwndMain, &rc);
        onSize(rc.right, rc.bottom);
    }

    appendToOutput("Layout profile applied: " + profile.name + "\n", "Output", OutputSeverity::Info);
    if (m_hwndStatusBar)
    {
        const std::wstring status = utf8ToWideLocal("Layout profile: " + profile.name);
        SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 0, (LPARAM)status.c_str());
    }

    if (persistSession)
        saveSessionDebounced();
}

void Win32IDE::applyBuiltInLayoutProfile(const std::string& profileName)
{
    LayoutProfile profile;
    if (equalsIgnoreCase(profileName, "focus"))
    {
        profile.name = "Focus";
        profile.snapState = SnapState::Compact;
        profile.sidebarWidth96 = 240;
        profile.secondarySidebarWidth96 = 240;
        profile.bottomPanelHeight96 = 220;
        profile.sidebarView = SidebarView::None;
        profile.rightView = "none";
    }
    else if (equalsIgnoreCase(profileName, "coding"))
    {
        profile.name = "Coding";
        profile.snapState = SnapState::Standard;
        profile.sidebarWidth96 = 240;
        profile.secondarySidebarWidth96 = 360;
        profile.bottomPanelHeight96 = 240;
        profile.sidebarView = SidebarView::Explorer;
        profile.rightView = "agent";
        profile.panelTerminal = true;
        profile.panelAgent = true;
    }
    else
    {
        profile.name = "Debug";
        profile.snapState = SnapState::Wide;
        profile.sidebarWidth96 = 280;
        profile.secondarySidebarWidth96 = 480;
        profile.bottomPanelHeight96 = 260;
        profile.sidebarView = SidebarView::RunDebug;
        profile.rightView = "agent";
        profile.panelAgent = true;
        profile.panelProblems = true;
    }

    applyLayoutProfile(profile, true);
}

bool Win32IDE::applyStartupLayoutProfileFromEnv()
{
    char value[256] = {};
    const DWORD length = GetEnvironmentVariableA("RAWRXD_LAYOUT_PROFILE", value, static_cast<DWORD>(sizeof(value)));
    if (length == 0 || length >= sizeof(value))
        return false;

    const std::string requested = trimAscii(std::string(value));
    if (requested.empty())
        return false;

    if (equalsIgnoreCase(requested, "focus") || equalsIgnoreCase(requested, "coding") ||
        equalsIgnoreCase(requested, "debug"))
    {
        applyBuiltInLayoutProfile(requested);
        return true;
    }

    LayoutProfile profile;
    if (!loadLayoutProfile(requested, profile))
    {
        LOG_WARNING("Startup layout profile not found: " + requested);
        return false;
    }

    applyLayoutProfile(profile, true);
    return true;
}

void Win32IDE::promptAndSaveCurrentLayoutProfile()
{
    wchar_t nameBuffer[128] = {};
    if (!DialogBoxWithInput(L"Save Layout Profile", L"Enter profile name:", nameBuffer, _countof(nameBuffer)))
        return;

    const std::string profileName = trimAscii(wideToUtf8Local(nameBuffer));
    if (profileName.empty())
    {
        MessageBoxW(m_hwndMain, L"Profile name cannot be empty.", L"Layout Profiles", MB_OK | MB_ICONWARNING);
        return;
    }

    LayoutProfile profile = captureCurrentLayoutProfile(profileName);
    if (!saveLayoutProfile(profile))
    {
        MessageBoxW(m_hwndMain, L"Failed to save layout profile.", L"Layout Profiles", MB_OK | MB_ICONERROR);
        return;
    }

    appendToOutput("Layout profile saved: " + profileName + "\n", "Output", OutputSeverity::Success);
    if (m_hwndStatusBar)
    {
        const std::wstring status = utf8ToWideLocal("Saved layout profile: " + profileName);
        SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 0, (LPARAM)status.c_str());
    }
}

void Win32IDE::promptAndApplySavedLayoutProfile()
{
    wchar_t nameBuffer[128] = {};
    if (!DialogBoxWithInput(L"Apply Layout Profile", L"Enter saved profile name:", nameBuffer, _countof(nameBuffer)))
        return;

    const std::string profileName = trimAscii(wideToUtf8Local(nameBuffer));
    if (profileName.empty())
    {
        MessageBoxW(m_hwndMain, L"Profile name cannot be empty.", L"Layout Profiles", MB_OK | MB_ICONWARNING);
        return;
    }

    LayoutProfile profile;
    if (!loadLayoutProfile(profileName, profile))
    {
        MessageBoxW(m_hwndMain, L"Saved layout profile not found.", L"Layout Profiles", MB_OK | MB_ICONWARNING);
        return;
    }

    applyLayoutProfile(profile, true);
}

std::string Win32IDE::captureProfileBundleV1(const std::string& baselineLabel, int sampleSeconds)
{
    try {
        const int boundedSeconds = std::max(5, std::min(sampleSeconds, 300));
        const std::string stamp = makeTimestampTag();
        const std::string folderName = baselineLabel + "_" + stamp;

        const std::filesystem::path bundleDir = std::filesystem::current_path() / "profiles" / folderName;
        std::filesystem::create_directories(bundleDir);

        // 1) session_state.json
        nlohmann::json state;
        state["schema"] = "profile_bundle_v1";
        state["capturedAt"] = stamp;
        state["snapState"] = snapStateToString(m_activeSnapState);
        state["dpi"] = static_cast<int>(m_currentDpi);
        state["sidebar"] = {
            {"visible", m_sidebarVisible},
            {"widthPx", m_sidebarWidth},
            {"width96", toLogicalWidth96(m_sidebarWidth, m_currentDpi)},
            {"secondaryWidthPx", m_secondarySidebarWidth},
            {"secondaryWidth96", toLogicalWidth96(m_secondarySidebarWidth, m_currentDpi)},
            {"view", static_cast<int>(m_currentSidebarView)}
        };
        state["panel"] = {
            {"visible", m_panelVisible},
            {"height", m_panelHeight},
            {"maximized", m_panelMaximized},
            {"activeTab", static_cast<int>(m_activePanelTab)}
        };
        state["model"] = {
            {"loadedPath", m_loadedModelPath}
        };
        {
            std::ofstream out(bundleDir / "session_state.json");
            out << state.dump(2);
        }

        // 2) environment.log
        {
            std::ofstream env(bundleDir / "environment.log");
            env << "ProfileBundle=v1\n";
            env << "CurrentDir=" << std::filesystem::current_path().string() << "\n";
            const char* username = std::getenv("USERNAME");
            const char* ollamaModels = std::getenv("OLLAMA_MODELS");
            env << "USERNAME=" << (username ? username : "") << "\n";
            env << "OLLAMA_MODELS=" << (ollamaModels ? ollamaModels : "") << "\n";
            env << "GPU_NOTE=Collect adapter name/driver via DXGI pass in next profile iteration\n";
        }

        // 3) perf_baseline.csv (CPU/memory/IO, 1 Hz)
        {
            std::ofstream perf(bundleDir / "perf_baseline.csv");
            perf << "sample,cpu_percent,working_set_mb,private_bytes_mb,io_read_mb,io_write_mb\n";

            HANDLE hProc = GetCurrentProcess();
            SYSTEM_INFO si{};
            GetSystemInfo(&si);
            const uint64_t logicalCpuCount = std::max<uint64_t>(1, si.dwNumberOfProcessors);

            FILETIME c0{}, e0{}, k0{}, u0{};
            GetProcessTimes(hProc, &c0, &e0, &k0, &u0);
            uint64_t prevProc100ns = fileTimeToUInt64(k0) + fileTimeToUInt64(u0);
            uint64_t lastWallMs = GetTickCount64();

            for (int i = 0; i < boundedSeconds; ++i) {
                Sleep(1000);

                FILETIME c1{}, e1{}, k1{}, u1{};
                GetProcessTimes(hProc, &c1, &e1, &k1, &u1);
                const uint64_t nowProc100ns = fileTimeToUInt64(k1) + fileTimeToUInt64(u1);
                const uint64_t nowWallMs = GetTickCount64();

                const uint64_t dProc100ns = (nowProc100ns >= prevProc100ns) ? (nowProc100ns - prevProc100ns) : 0;
                const uint64_t dWallMs = (nowWallMs > lastWallMs) ? (nowWallMs - lastWallMs) : 1;
                const double cpuPct = (static_cast<double>(dProc100ns) / (static_cast<double>(dWallMs) * 10000.0)) /
                                      static_cast<double>(logicalCpuCount) * 100.0;

                PROCESS_MEMORY_COUNTERS_EX pmc{};
                pmc.cb = sizeof(pmc);
                SIZE_T ws = 0;
                SIZE_T priv = 0;
                if (GetProcessMemoryInfo(hProc, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
                    ws = pmc.WorkingSetSize;
                    priv = pmc.PrivateUsage;
                }

                IO_COUNTERS io{};
                unsigned long long readMb = 0;
                unsigned long long writeMb = 0;
                if (GetProcessIoCounters(hProc, &io)) {
                    readMb = static_cast<unsigned long long>(io.ReadTransferCount / (1024ull * 1024ull));
                    writeMb = static_cast<unsigned long long>(io.WriteTransferCount / (1024ull * 1024ull));
                }

                perf << i
                     << "," << std::fixed << std::setprecision(2) << cpuPct
                     << "," << std::fixed << std::setprecision(2)
                     << (static_cast<double>(ws) / (1024.0 * 1024.0))
                     << "," << std::fixed << std::setprecision(2)
                     << (static_cast<double>(priv) / (1024.0 * 1024.0))
                     << "," << readMb
                     << "," << writeMb
                     << "\n";

                prevProc100ns = nowProc100ns;
                lastWallMs = nowWallMs;
            }
        }

        // 4) contract_results.json (baseline status record)
        {
            nlohmann::json contracts;
            contracts["router_hybrid"] = { {"pass", nullptr}, {"fail", nullptr}, {"status", "unknown"} };
            contracts["debug_swarm"] = { {"pass", nullptr}, {"fail", nullptr}, {"status", "unknown"} };
            contracts["dual_multi_response"] = { {"pass", nullptr}, {"fail", nullptr}, {"status", "unknown"} };
            contracts["note"] = "Populate from contract runner artifacts when available.";
            std::ofstream out(bundleDir / "contract_results.json");
            out << contracts.dump(2);
        }

        // 5) memory_map_trace.txt (GGUF loader baseline signature)
        {
            std::ofstream mm(bundleDir / "memory_map_trace.txt");
            mm << "ProfileBundle=v1\n";
            mm << "LoaderSignature=baseline\n";
            mm << "ModelPath=" << m_loadedModelPath << "\n";
            if (!m_loadedModelPath.empty()) {
                std::error_code ec;
                const auto sz = std::filesystem::file_size(m_loadedModelPath, ec);
                if (!ec) {
                    mm << "ModelFileBytes=" << static_cast<unsigned long long>(sz) << "\n";
                }
            }
            mm << "MapViewOfFile3Trace=not_available_in_v1\n";
            mm << "PageFaultCounters=not_available_in_v1\n";
        }

        LOG_INFO("Profile Bundle v1 captured at: " + bundleDir.string());
        return bundleDir.string();
    } catch (const std::exception& ex) {
        LOG_ERROR(std::string("captureProfileBundleV1 failed: ") + ex.what());
        return std::string();
    }
}

// ============================================================================
// SAVE SESSION — master entry point
// ============================================================================
void Win32IDE::saveSessionDebounced(UINT delayMs)
{
    if (!m_hwndMain || !IsWindow(m_hwndMain))
    {
        saveSession();
        return;
    }

    m_sessionSaveDebouncePending = true;
    KillTimer(m_hwndMain, kSessionSaveDebounceTimerId);
    SetTimer(m_hwndMain, kSessionSaveDebounceTimerId, delayMs, nullptr);
}

void Win32IDE::saveSession() {
    LOG_INFO("Saving IDE session...");
    
    try {
        nlohmann::json session;
        session["version"] = 2;
        session["schemaVersion"] = "2.3";     // Hardening: explicit schema version for forward compat
        session["annotationFormat"] = 2;  // v2 = actions array; v1 = single actionType
        session["timestamp"] = std::to_string(GetTickCount64());
        
        // Save each section
        saveSessionTabs(session);
        saveSessionPanelState(session);
        saveSessionEditorState(session);
        saveSessionAnnotations(session);
        saveSessionTheme(session);
        
        // Save current file
        session["currentFile"] = m_currentFile;

        // Save active model so the next launch can restore inference/chat state.
        if (!m_loadedModelPath.empty()) {
            session["loadedModelPath"] = m_loadedModelPath;
        }
        
        // Save working directory
        char cwd[MAX_PATH] = {0};
        GetCurrentDirectoryA(MAX_PATH, cwd);
        session["workingDirectory"] = std::string(cwd);
        
        // Write to file
        std::string path = getSessionFilePath();
        std::ofstream out(path);
        if (out.is_open()) {
            out << session.dump(2);
            out.close();
            LOG_INFO("Session saved to: " + path);
        } else {
            LOG_ERROR("Failed to write session file: " + path);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Session save error: " + std::string(e.what()));
    }
}

// ============================================================================
// LOAD SESSION — compatibility/manual entry point
// ============================================================================
void Win32IDE::loadSession() {
    restoreSession();
}

// ============================================================================
// RESTORE SESSION — master entry point
// ============================================================================
void Win32IDE::restoreSession() {
    LOG_INFO("Restoring IDE session...");
    
    try {
        std::string path = getSessionFilePath();
        std::ifstream in(path);
        if (!in.is_open()) {
            LOG_INFO("No session file found at: " + path);
            return;
        }
        
        std::string content((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
        in.close();
        
        nlohmann::json session = nlohmann::json::parse(content);
        
        // Validate version (accept v1 and v2)
        int version = session.value("version", 0);
        if (version < 1 || version > 2) {
            LOG_WARNING("Unknown session version: " + std::to_string(version) + " — skipping restore");
            return;
        }
        if (version == 1) {
            LOG_INFO("Session: upgrading from v1 format (legacy single-action annotations)");
        }

        // Hardening: log schema version for diagnostics
        std::string schemaVer = session.value("schemaVersion", "1.0");
        LOG_INFO("Session: schema version " + schemaVer + ", data version " + std::to_string(version));
        
        // Restore each section
        restoreSessionTabs(session);
        restoreSessionPanelState(session);
        restoreSessionEditorState(session);
        restoreSessionAnnotations(session);
        restoreSessionTheme(session);
        
        // Restore working directory
        std::string cwd = session.value("workingDirectory", "");
        if (!cwd.empty()) {
            SetCurrentDirectoryA(cwd.c_str());
        }

        // Restore loaded model after the working directory is set so relative paths resolve.
        std::string savedModelPath = session.value("loadedModelPath", "");
        if (!savedModelPath.empty()) {
            bool restoredModel = false;
            DWORD modelAttrs = GetFileAttributesA(savedModelPath.c_str());
            if (modelAttrs != INVALID_FILE_ATTRIBUTES && !(modelAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
                LOG_INFO("Session: restoring model from path " + savedModelPath);
                bool ggufOk = loadGGUFModel(savedModelPath);
                if (ggufOk) {
                    initializeInference();
                    initBackendManager();
                    initLLMRouter();
                }
                bool bridgeOk = loadModelForInference(savedModelPath);
                restoredModel = ggufOk || bridgeOk;
            } else {
                LOG_INFO("Session: attempting logical model restore via bridge for " + savedModelPath);
                restoredModel = ensureAgenticBridgeHasModel(savedModelPath);
                if (!restoredModel) {
                    LOG_WARNING("Session model missing or unavailable: " + savedModelPath);
                }
            }

            if (!restoredModel) {
                m_loadedModelPath.clear();
            }
        }
        
        m_sessionRestored = true;
        LOG_INFO("Session restored successfully.");

        // v1→v2 write-once migration: if we just loaded a v1 session,
        // re-save immediately as v2 so the legacy format is retired on disk.
        if (version == 1) {
            LOG_INFO("Session: performing v1→v2 migration write...");
            saveSession();
            LOG_INFO("Session: v1→v2 migration complete — legacy format retired.");
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR("Session restore error: " + std::string(e.what()));
    }
}

// ============================================================================
// SAVE/RESTORE TABS
// ============================================================================
void Win32IDE::saveSessionTabs(nlohmann::json& session) {
    nlohmann::json tabs = nlohmann::json::array();
    
    for (const auto& tab : m_editorTabs) {
        nlohmann::json t;
        t["filePath"] = tab.filePath;
        t["displayName"] = tab.displayName;
        t["modified"] = tab.modified;
        tabs.push_back(t);
    }
    
    session["tabs"] = tabs;
    session["activeTabIndex"] = m_activeTabIndex;
}

void Win32IDE::restoreSessionTabs(const nlohmann::json& session) {
    if (!session.contains("tabs")) return;
    
    const auto& tabs = session["tabs"];
    int activeIdx = session.value("activeTabIndex", 0);
    
    for (size_t ti = 0; ti < tabs.size(); ti++) {
        const auto& t = tabs[ti];
        std::string filePath = t.value("filePath", "");
        std::string displayName = t.value("displayName", "");
        
        if (filePath.empty()) continue;
        
        // Only restore tabs for files that still exist
        DWORD attrs = GetFileAttributesA(filePath.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            LOG_WARNING("Session tab file missing: " + filePath);
            continue;
        }
        
        addTab(filePath, displayName);
    }
    
    // Set the active tab
    if (activeIdx >= 0 && activeIdx < (int)m_editorTabs.size()) {
        setActiveTab(activeIdx);
    }
}

// ============================================================================
// SAVE/RESTORE PANEL STATE
// ============================================================================
void Win32IDE::saveSessionPanelState(nlohmann::json& session) {
    nlohmann::json panel;
    
    panel["visible"] = m_panelVisible;
    panel["height"] = m_panelHeight;
    panel["maximized"] = m_panelMaximized;
    panel["activeTab"] = (int)m_activePanelTab;
    
    // Sidebar state
    panel["sidebarVisible"] = m_sidebarVisible;
    panel["sidebarWidth"] = m_sidebarWidth;
    panel["secondarySidebarWidth"] = m_secondarySidebarWidth;
    panel["sidebarWidth96"] = toLogicalWidth96(m_sidebarWidth, m_currentDpi);
    panel["secondarySidebarWidth96"] = toLogicalWidth96(m_secondarySidebarWidth, m_currentDpi);
    panel["sidebarSnapState"] = snapStateToString(m_activeSnapState);
    panel["sidebarView"] = (int)m_currentSidebarView;
    
    session["panel"] = panel;
}

void Win32IDE::restoreSessionPanelState(const nlohmann::json& session) {
    if (!session.contains("panel")) return;
    
    const auto& panel = session["panel"];
    
    // Restore panel state
    m_panelVisible = panel.value("visible", true);
    m_panelHeight = panel.value("height", 200);
    m_panelMaximized = panel.value("maximized", false);
    
    int tabIdx = panel.value("activeTab", 0);
    if (tabIdx >= 0 && tabIdx <= 3) {
        m_activePanelTab = static_cast<PanelTab>(tabIdx);
    }
    
    // Restore sidebar state
    m_sidebarVisible = panel.value("sidebarVisible", true);
    if (panel.contains("sidebarWidth96"))
        m_sidebarWidth = toPhysicalWidthFrom96(panel.value("sidebarWidth96", 240), m_currentDpi);
    else
        m_sidebarWidth = panel.value("sidebarWidth", 240);

    if (panel.contains("secondarySidebarWidth96"))
        m_secondarySidebarWidth = toPhysicalWidthFrom96(panel.value("secondarySidebarWidth96", 320), m_currentDpi);
    else
        m_secondarySidebarWidth = panel.value("secondarySidebarWidth", 320);

    if (panel.contains("sidebarSnapState"))
    {
        m_activeSnapState = snapStateFromString(panel.value("sidebarSnapState", "standard"));
    }
    else
    {
        auto nearLegacyPreset = [this](int widthValue, int basePixels) -> bool
        {
            const int target = dpiScale(basePixels);
            const int tol = dpiScale(12);
            const int delta = (widthValue >= target) ? (widthValue - target) : (target - widthValue);
            return delta <= tol;
        };

        if (nearLegacyPreset(m_secondarySidebarWidth, 240))
            m_activeSnapState = SnapState::Compact;
        else if (nearLegacyPreset(m_secondarySidebarWidth, 360))
            m_activeSnapState = SnapState::Standard;
        else if (nearLegacyPreset(m_secondarySidebarWidth, 480))
            m_activeSnapState = SnapState::Wide;
        else
            m_activeSnapState = SnapState::Custom;
    }
    
    int viewIdx = panel.value("sidebarView", 0);
    m_currentSidebarView = static_cast<SidebarView>(viewIdx);

    if (m_activeSnapState == SnapState::Compact)
    {
        applySovereignSnapPreset(240, L"Compact", false);
    }
    else if (m_activeSnapState == SnapState::Standard)
    {
        applySovereignSnapPreset(360, L"Standard", false);
    }
    else if (m_activeSnapState == SnapState::Wide)
    {
        applySovereignSnapPreset(480, L"Wide", false);
    }
    else
    {
        updateSovereignSnapMenuChecks();

        // Apply the restored layout
        if (m_hwndMain)
        {
            RECT rc;
            GetClientRect(m_hwndMain, &rc);
            SendMessage(m_hwndMain, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rc.right, rc.bottom));
        }
    }
}

// ============================================================================
// SAVE/RESTORE EDITOR STATE (cursor, scroll, selection)
// ============================================================================
void Win32IDE::saveSessionEditorState(nlohmann::json& session) {
    nlohmann::json editor;
    
    if (m_hwndEditor) {
        // Cursor position
        CHARRANGE sel;
        SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
        editor["selStart"] = (int)sel.cpMin;
        editor["selEnd"] = (int)sel.cpMax;
        
        // Scroll position
        POINT scrollPos;
        SendMessage(m_hwndEditor, EM_GETSCROLLPOS, 0, (LPARAM)&scrollPos);
        editor["scrollX"] = (int)scrollPos.x;
        editor["scrollY"] = (int)scrollPos.y;
        
        // First visible line
        int firstVisible = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
        editor["firstVisibleLine"] = firstVisible;
        
        // Current line and column
        int line = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
        int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, line, 0);
        editor["cursorLine"] = line;
        editor["cursorColumn"] = (int)(sel.cpMin - lineStart);
    }
    
    // Syntax coloring state
    editor["syntaxColoringEnabled"] = m_syntaxColoringEnabled;
    editor["syntaxLanguage"] = (int)m_syntaxLanguage;
    
    // Annotation visibility
    editor["annotationsVisible"] = m_annotationsVisible;
    
    session["editor"] = editor;
}

void Win32IDE::restoreSessionEditorState(const nlohmann::json& session) {
    if (!session.contains("editor")) return;
    
    const auto& editor = session["editor"];
    
    // Restore syntax coloring preference
    m_syntaxColoringEnabled = editor.value("syntaxColoringEnabled", true);
    
    int langIdx = editor.value("syntaxLanguage", 0);
    m_syntaxLanguage = static_cast<SyntaxLanguage>(langIdx);
    
    // Restore annotation visibility
    m_annotationsVisible = editor.value("annotationsVisible", true);
    
    if (!m_hwndEditor) return;
    
    // Restore cursor/selection (defer slightly so content is loaded first)
    int selStart = editor.value("selStart", 0);
    int selEnd = editor.value("selEnd", 0);
    int scrollX = editor.value("scrollX", 0);
    int scrollY = editor.value("scrollY", 0);
    
    // Use PostMessage to defer restoration until after content is loaded
    // Store values for deferred restore
    m_sessionFilePath = getSessionFilePath(); // Tag that we're in restore mode
    
    // Set cursor position
    CHARRANGE cr;
    cr.cpMin = selStart;
    cr.cpMax = selEnd;
    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
    
    // Restore scroll position
    POINT scrollPos;
    scrollPos.x = scrollX;
    scrollPos.y = scrollY;
    SendMessage(m_hwndEditor, EM_SETSCROLLPOS, 0, (LPARAM)&scrollPos);
    
    // Update line number gutter
    updateLineNumbers();
    
    LOG_INFO("Editor state restored: cursor at " + std::to_string(selStart) + 
             ", scroll Y=" + std::to_string(scrollY));
}

// ============================================================================
// SAVE/RESTORE THEME + TRANSPARENCY
// ============================================================================

void Win32IDE::saveSessionTheme(nlohmann::json& session) {
    nlohmann::json theme;

    theme["name"]         = m_currentTheme.name;
    theme["themeId"]      = m_activeThemeId;
    theme["alpha"]        = (int)m_windowAlpha;
    theme["transparency"] = m_transparencyEnabled;

    session["theme"] = theme;
    LOG_DEBUG("Session: saved theme \"" + m_currentTheme.name +
              "\" alpha=" + std::to_string(m_windowAlpha));
}

void Win32IDE::restoreSessionTheme(const nlohmann::json& session) {
    if (!session.contains("theme")) return;

    const auto& theme = session["theme"];
    std::string savedName = theme.value("name", "");
    int savedId           = theme.value("themeId", (int)IDM_THEME_DARK_PLUS);
    int savedAlpha        = theme.value("alpha", 255);

    // Validate theme ID is in valid range; fallback to Dark+ if not
    if (savedId < IDM_THEME_DARK_PLUS || savedId > IDM_THEME_ABYSS) {
        // Try to match by name from m_themes map
        bool found = false;
        for (int id = IDM_THEME_DARK_PLUS; id <= IDM_THEME_ABYSS; id++) {
            IDETheme candidate = getBuiltinTheme(id);
            if (candidate.name == savedName) {
                savedId = id;
                found = true;
                break;
            }
        }
        if (!found) {
            LOG_WARNING("Session: unknown theme \"" + savedName + "\" — falling back to Dark+");
            savedId = IDM_THEME_DARK_PLUS;
        }
    }

    // Apply the restored theme
    m_currentTheme = getBuiltinTheme(savedId);
    m_activeThemeId = savedId;
    applyTheme();
    applyThemeToAllControls();

    // Restore transparency (clamp to 30-255 for safety)
    BYTE alpha = (BYTE)std::clamp(savedAlpha, 30, 255);
    if (alpha < 255) {
        setWindowTransparency(alpha);
    }

    // Re-trigger syntax coloring with restored theme palette
    if (m_syntaxColoringEnabled) {
        onEditorContentChanged();
    }

    LOG_INFO("Session: restored theme \"" + m_currentTheme.name +
             "\" alpha=" + std::to_string(alpha));
}
