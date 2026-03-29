// ============================================================================
// Win32IDE_Settings.cpp — Multi-Tab Settings Dialog
// ============================================================================
// Provides a proper settings UI with persistent configuration:
//   - General: autosave, line numbers, word wrap, font size, working directory
//   - AI/Model: temperature, top-p, top-k, max tokens, context window, model path
//   - Editor: tab size, encoding, EOL style, syntax coloring toggle
//   - Theme: current theme selector, transparency
//   - Shortcuts: display of registered shortcuts (read-only for now)
//
// Settings are persisted in the session system (%APPDATA%\RawrXD\settings.json).
// Changes apply immediately with a "dirty" flag for restart-required items.
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include "../../include/local_parity_kernel.h"
#include "../core/amd_gpu_accelerator.h"
#include "../core/configuration_service.hpp"
#include <richedit.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>

// ============================================================================
// SETTINGS FILE PATH
// ============================================================================

std::string Win32IDE::getSettingsFilePath() const {
    char appDataPath[MAX_PATH] = {};
    if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath) == S_OK) {
        std::string dir = std::string(appDataPath) + "\\RawrXD";
        CreateDirectoryA(dir.c_str(), nullptr);
        return dir + "\\settings.json";
    }
    return "settings.json";
}

// ============================================================================
// LOAD SETTINGS — from JSON file
// ============================================================================

void Win32IDE::loadSettings() {
    using RawrXD::ConfigurationScope;
    using RawrXD::ConfigurationService;

    std::string path = getSettingsFilePath();
    auto& config = ConfigurationService::instance();
    config.setStoragePath(ConfigurationScope::User, path);

    if (!config.load(ConfigurationScope::User)) {
        LOG_INFO("No valid settings file found, using defaults: " + path);
        applyDefaultSettings();
        return;
    }

    // Minimal notification hook for now. Consumers can subscribe to respond
    // to key-level changes as we expand Settings Service scope.
    static bool s_subscribedToConfigChanges = false;
    if (!s_subscribedToConfigChanges) {
        config.subscribe([](const std::string& key, ConfigurationScope scope) {
            if (scope == ConfigurationScope::User) {
                LOG_DEBUG("Configuration changed (user): " + key);
            }
        });
        s_subscribedToConfigChanges = true;
    }

    try {
        // General
        m_settings.autoSaveEnabled   = config.getBool("autoSave", m_settings.autoSaveEnabled, ConfigurationScope::User);
        m_settings.lineNumbersVisible = config.getBool("lineNumbers", m_settings.lineNumbersVisible, ConfigurationScope::User);
        m_settings.wordWrapEnabled   = config.getBool("wordWrap", m_settings.wordWrapEnabled, ConfigurationScope::User);
        m_settings.fontSize          = config.getInt("fontSize", m_settings.fontSize, ConfigurationScope::User);
        m_settings.fontName          = config.getString("fontName", m_settings.fontName, ConfigurationScope::User);
        m_settings.workingDirectory  = config.getString("workingDirectory", m_settings.workingDirectory, ConfigurationScope::User);
        m_settings.autoSaveIntervalSec = config.getInt("autoSaveInterval", m_settings.autoSaveIntervalSec, ConfigurationScope::User);

        // AI/Model
        m_settings.aiTemperature     = static_cast<float>(config.getFloat("aiTemperature", m_settings.aiTemperature, ConfigurationScope::User));
        m_settings.aiTopP            = static_cast<float>(config.getFloat("aiTopP", m_settings.aiTopP, ConfigurationScope::User));
        m_settings.aiTopK            = config.getInt("aiTopK", m_settings.aiTopK, ConfigurationScope::User);
        m_settings.aiMaxTokens       = config.getInt("aiMaxTokens", m_settings.aiMaxTokens, ConfigurationScope::User);
        m_settings.aiContextWindow   = config.getInt("aiContextWindow", m_settings.aiContextWindow, ConfigurationScope::User);
        m_settings.aiModelPath       = config.getString("aiModelPath", m_settings.aiModelPath, ConfigurationScope::User);
        m_settings.aiOllamaUrl       = config.getString("aiOllamaUrl", m_settings.aiOllamaUrl, ConfigurationScope::User);
        m_settings.ghostTextEnabled  = config.getBool("ghostText", m_settings.ghostTextEnabled, ConfigurationScope::User);
        m_settings.failureDetectorEnabled = config.getBool("failureDetector", m_settings.failureDetectorEnabled, ConfigurationScope::User);
        m_settings.failureMaxRetries = config.getInt("failureMaxRetries", m_settings.failureMaxRetries, ConfigurationScope::User);
        m_settings.amdUnifiedMemoryEnabled = config.getBool("amdUnifiedMemoryEnabled", m_settings.amdUnifiedMemoryEnabled, ConfigurationScope::User);

        // Display Scaling
        m_settings.uiScalePercent    = config.getInt("uiScalePercent", m_settings.uiScalePercent, ConfigurationScope::User);

        // Editor
        m_settings.tabSize           = config.getInt("tabSize", m_settings.tabSize, ConfigurationScope::User);
        m_settings.useSpaces         = config.getBool("useSpaces", m_settings.useSpaces, ConfigurationScope::User);
        m_settings.encoding          = config.getString("encoding", m_settings.encoding, ConfigurationScope::User);
        m_settings.eolStyle          = config.getString("eolStyle", m_settings.eolStyle, ConfigurationScope::User);
        m_settings.syntaxColoringEnabled = config.getBool("syntaxColoring", m_settings.syntaxColoringEnabled, ConfigurationScope::User);
        m_settings.minimapEnabled    = config.getBool("minimap", m_settings.minimapEnabled, ConfigurationScope::User);

        // Theme
        m_settings.themeId           = config.getInt("themeId", m_settings.themeId, ConfigurationScope::User);
        m_settings.windowAlpha       = static_cast<BYTE>(config.getInt("windowAlpha", static_cast<int>(m_settings.windowAlpha), ConfigurationScope::User));

        // Server
        m_settings.localServerEnabled = config.getBool("localServer", m_settings.localServerEnabled, ConfigurationScope::User);
        m_settings.localServerPort    = config.getInt("localServerPort", m_settings.localServerPort, ConfigurationScope::User);

        // Display / UX toggles
        m_settings.breadcrumbsEnabled  = config.getBool("breadcrumbs", m_settings.breadcrumbsEnabled, ConfigurationScope::User);
        m_settings.smoothScrollEnabled  = config.getBool("smoothScroll", m_settings.smoothScrollEnabled, ConfigurationScope::User);

        // Local Parity (no API key)
        m_settings.localParityEnabled   = config.getBool("localParityEnabled", m_settings.localParityEnabled, ConfigurationScope::User);
        m_settings.localParityModelPath = config.getString("localParityModelPath", m_settings.localParityModelPath, ConfigurationScope::User);
        m_settings.updateManifestUrl    = config.getString("updateManifestUrl", m_settings.updateManifestUrl, ConfigurationScope::User);

        m_settings.agentTerminalIsolated = config.getBool("agentTerminalIsolated", m_settings.agentTerminalIsolated, ConfigurationScope::User);

        // Workflow Executor
        m_settings.workflowExecutorEnabled    = config.getBool("workflowExecutorEnabled", m_settings.workflowExecutorEnabled, ConfigurationScope::User);
        m_settings.workflowExecutorAgentCount = config.getInt("workflowExecutorAgentCount", m_settings.workflowExecutorAgentCount, ConfigurationScope::User);

        LOG_INFO("Settings loaded from " + path);
    } catch (const std::exception& e) {
        LOG_WARNING("Failed to load settings from ConfigurationService: " + std::string(e.what()));
        applyDefaultSettings();
    }
}

// ============================================================================
// SAVE SETTINGS — to JSON file
// ============================================================================

void Win32IDE::saveSettings() {
    using RawrXD::ConfigurationScope;
    using RawrXD::ConfigurationService;

    auto& config = ConfigurationService::instance();
    config.setStoragePath(ConfigurationScope::User, getSettingsFilePath());

    // General
    config.setBool("autoSave", m_settings.autoSaveEnabled, ConfigurationScope::User);
    config.setBool("lineNumbers", m_settings.lineNumbersVisible, ConfigurationScope::User);
    config.setBool("wordWrap", m_settings.wordWrapEnabled, ConfigurationScope::User);
    config.setInt("fontSize", m_settings.fontSize, ConfigurationScope::User);
    config.setString("fontName", m_settings.fontName, ConfigurationScope::User);
    config.setString("workingDirectory", m_settings.workingDirectory, ConfigurationScope::User);
    config.setInt("autoSaveInterval", m_settings.autoSaveIntervalSec, ConfigurationScope::User);

    // AI/Model
    config.setFloat("aiTemperature", m_settings.aiTemperature, ConfigurationScope::User);
    config.setFloat("aiTopP", m_settings.aiTopP, ConfigurationScope::User);
    config.setInt("aiTopK", m_settings.aiTopK, ConfigurationScope::User);
    config.setInt("aiMaxTokens", m_settings.aiMaxTokens, ConfigurationScope::User);
    config.setInt("aiContextWindow", m_settings.aiContextWindow, ConfigurationScope::User);
    config.setString("aiModelPath", m_settings.aiModelPath, ConfigurationScope::User);
    config.setString("aiOllamaUrl", m_settings.aiOllamaUrl, ConfigurationScope::User);
    config.setBool("ghostText", m_settings.ghostTextEnabled, ConfigurationScope::User);
    config.setBool("failureDetector", m_settings.failureDetectorEnabled, ConfigurationScope::User);
    config.setInt("failureMaxRetries", m_settings.failureMaxRetries, ConfigurationScope::User);
    config.setBool("amdUnifiedMemoryEnabled", m_settings.amdUnifiedMemoryEnabled, ConfigurationScope::User);

    // Display Scaling
    config.setInt("uiScalePercent", m_settings.uiScalePercent, ConfigurationScope::User);

    // Editor
    config.setInt("tabSize", m_settings.tabSize, ConfigurationScope::User);
    config.setBool("useSpaces", m_settings.useSpaces, ConfigurationScope::User);
    config.setString("encoding", m_settings.encoding, ConfigurationScope::User);
    config.setString("eolStyle", m_settings.eolStyle, ConfigurationScope::User);
    config.setBool("syntaxColoring", m_settings.syntaxColoringEnabled, ConfigurationScope::User);
    config.setBool("minimap", m_settings.minimapEnabled, ConfigurationScope::User);

    // Theme
    config.setInt("themeId", m_settings.themeId, ConfigurationScope::User);
    config.setInt("windowAlpha", (int)m_settings.windowAlpha, ConfigurationScope::User);

    // Server
    config.setBool("localServer", m_settings.localServerEnabled, ConfigurationScope::User);
    config.setInt("localServerPort", m_settings.localServerPort, ConfigurationScope::User);

    // Display / UX toggles
    config.setBool("breadcrumbs", m_settings.breadcrumbsEnabled, ConfigurationScope::User);
    config.setBool("smoothScroll", m_settings.smoothScrollEnabled, ConfigurationScope::User);

    // Local Parity (no API key)
    config.setBool("localParityEnabled", m_settings.localParityEnabled, ConfigurationScope::User);
    config.setString("localParityModelPath", m_settings.localParityModelPath, ConfigurationScope::User);
    config.setString("updateManifestUrl", m_settings.updateManifestUrl, ConfigurationScope::User);

    // Agent terminal isolation (Top-001)
    config.setBool("agentTerminalIsolated", m_settings.agentTerminalIsolated, ConfigurationScope::User);

    // Workflow Executor
    config.setBool("workflowExecutorEnabled", m_settings.workflowExecutorEnabled, ConfigurationScope::User);
    config.setInt("workflowExecutorAgentCount", m_settings.workflowExecutorAgentCount, ConfigurationScope::User);

    std::string path = getSettingsFilePath();
    if (config.save(ConfigurationScope::User)) {
        LOG_INFO("Settings saved via ConfigurationService to " + path);
    } else {
        LOG_ERROR("Failed to write settings via ConfigurationService: " + path);
    }
}

// ============================================================================
// DEFAULT SETTINGS
// ============================================================================

void Win32IDE::applyDefaultSettings() {
    m_settings.autoSaveEnabled     = false;
    m_settings.lineNumbersVisible  = true;
    m_settings.wordWrapEnabled     = false;
    m_settings.fontSize            = 14;
    m_settings.fontName            = "Consolas";
    m_settings.workingDirectory    = "";
    m_settings.autoSaveIntervalSec = 60;

    m_settings.aiTemperature       = 0.7f;
    m_settings.aiTopP              = 0.9f;
    m_settings.aiTopK              = 40;
    m_settings.aiMaxTokens         = 512;
    m_settings.aiContextWindow     = 4096;
    m_settings.aiModelPath         = "";
    m_settings.aiOllamaUrl         = "http://localhost:11434";
    m_settings.ghostTextEnabled    = true;
    m_settings.failureDetectorEnabled = true;
    m_settings.failureMaxRetries   = 3;
    m_settings.amdUnifiedMemoryEnabled = false;

    m_settings.tabSize             = 4;
    m_settings.useSpaces           = true;
    m_settings.encoding            = "UTF-8";
    m_settings.eolStyle            = "LF";
    m_settings.syntaxColoringEnabled = true;
    m_settings.minimapEnabled      = true;

    m_settings.themeId             = IDM_THEME_DARK_PLUS;
    m_settings.windowAlpha         = 255;

    m_settings.localServerEnabled  = false;
    m_settings.localServerPort     = 11435;

    m_settings.breadcrumbsEnabled   = true;
    m_settings.smoothScrollEnabled  = true;

    m_settings.localParityEnabled   = false;
    m_settings.localParityModelPath = "";
    m_settings.updateManifestUrl    = "";
    m_settings.agentTerminalIsolated = true;  // Default: isolate agent to avoid interrupting user

    m_settings.workflowExecutorEnabled    = false;
    m_settings.workflowExecutorAgentCount = 4;

    m_settings.uiScalePercent      = 0;   // 0 = auto (follow system DPI)
}

// ============================================================================
// APPLY SETTINGS — push settings into the IDE state
// ============================================================================

void Win32IDE::applySettings() {
    // AI config
    m_inferenceConfig.temperature    = m_settings.aiTemperature;
    m_inferenceConfig.topP           = m_settings.aiTopP;
    m_inferenceConfig.topK           = m_settings.aiTopK;
    m_inferenceConfig.maxTokens      = m_settings.aiMaxTokens;
    m_inferenceConfig.contextWindow  = m_settings.aiContextWindow;
    m_ollamaBaseUrl                  = m_settings.aiOllamaUrl;

    // Ghost text
    m_ghostTextEnabled               = m_settings.ghostTextEnabled;

    // Failure detector
    m_failureDetectorEnabled         = m_settings.failureDetectorEnabled;
    m_failureMaxRetries              = m_settings.failureMaxRetries;

    // Syntax coloring
    m_syntaxColoringEnabled          = m_settings.syntaxColoringEnabled;

    // Minimap
    m_minimapVisible                 = m_settings.minimapEnabled;

    // Smooth scroll & breadcrumbs (so Settings menu toggles take effect)
    m_smoothScroll.enabled           = m_settings.smoothScrollEnabled;
    if (m_hwndBreadcrumbs) {
        ShowWindow(m_hwndBreadcrumbs, m_settings.breadcrumbsEnabled ? SW_SHOW : SW_HIDE);
        if (m_settings.breadcrumbsEnabled)
            updateBreadcrumbs(); // restart content when re-enabled from Settings
    }
    // Layout so editor resizes when breadcrumb bar visibility changes
    if (m_hwndMain && IsWindow(m_hwndMain)) {
        RECT rc;
        if (GetClientRect(m_hwndMain, &rc))
            onSize(rc.right, rc.bottom);
    }

    // Theme
    if (m_settings.themeId >= IDM_THEME_DARK_PLUS && m_settings.themeId <= IDM_THEME_ABYSS) {
        applyThemeById(m_settings.themeId);
    }
    if (m_settings.windowAlpha > 0) {
        setWindowTransparency(m_settings.windowAlpha);
    }

    // Word wrap
    if (m_hwndEditor) {
        SendMessageA(m_hwndEditor, EM_SETTARGETDEVICE, 0,
                     m_settings.wordWrapEnabled ? 0 : 1);
    }

    if (m_settings.localParityEnabled && !m_settings.localParityModelPath.empty())
        LocalParity_SetModelPath(m_settings.localParityModelPath.c_str());
    else
        LocalParity_SetModelPath(nullptr);

    // AMD unified memory (SAM / host-backed executor) — toggled from Settings GUI
    if (m_settings.amdUnifiedMemoryEnabled) {
        AccelResult um = AMDGPUAccelerator::instance().enableUnifiedMemory();
        if (!um.success) {
            LOG_WARNING(std::string("AMD unified memory enable failed: ") + (um.detail ? um.detail : ""));
        } else {
            LOG_INFO(std::string("AMD unified memory: ") + (um.detail ? um.detail : "enabled"));
        }
    } else {
        AccelResult um = AMDGPUAccelerator::instance().disableUnifiedMemory();
        if (!um.success) {
            LOG_WARNING(std::string("AMD unified memory disable failed: ") + (um.detail ? um.detail : ""));
        }
    }

    // Workflow Executor — propagate persisted state to bridge and UI
    if (m_agenticBridge) {
        m_agenticBridge->SetWorkflowExecutorEnabled(m_settings.workflowExecutorEnabled);
        m_agenticBridge->SetWorkflowExecutorAgentCount(m_settings.workflowExecutorAgentCount);
    }
    syncAgentModeUiFromBridge();

    LOG_INFO("Settings applied");
}

// ============================================================================
// SETTINGS DIALOG — summary view + "Open full Settings" to property-grid UI
// ============================================================================

static WNDPROC s_oldSettingsSummaryProc = nullptr;

LRESULT CALLBACK SettingsSummaryWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_COMMAND && LOWORD(wp) == 1000) {
            Win32IDE* ide = (Win32IDE*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
            if (ide) {
                ide->showSettingsGUIDialog();
            }
            DestroyWindow(hwnd);
            return 0;
        }
        if (msg == WM_COMMAND && LOWORD(wp) == 1002) {
            DestroyWindow(hwnd);
            return 0;
        }
        return s_oldSettingsSummaryProc ? CallWindowProcA(s_oldSettingsSummaryProc, hwnd, msg, wp, lp)
                                       : DefWindowProcA(hwnd, msg, wp, lp);
}

void Win32IDE::showSettingsDialog() {
    // Build settings summary text for a simple dialog
    // (Full tab control upgrade is a future polish item)
    std::ostringstream oss;

    oss << "=== GENERAL ===\r\n";
    oss << "Auto-Save: " << (m_settings.autoSaveEnabled ? "ON" : "OFF")
        << " (interval: " << m_settings.autoSaveIntervalSec << "s)\r\n";
    oss << "Line Numbers: " << (m_settings.lineNumbersVisible ? "ON" : "OFF") << "\r\n";
    oss << "Word Wrap: " << (m_settings.wordWrapEnabled ? "ON" : "OFF") << "\r\n";
    oss << "Font: " << m_settings.fontName << " " << m_settings.fontSize << "pt\r\n";
    oss << "\r\n=== AI / MODEL ===\r\n";
    oss << "Temperature: " << m_settings.aiTemperature << "\r\n";
    oss << "Top-P: " << m_settings.aiTopP << "\r\n";
    oss << "Top-K: " << m_settings.aiTopK << "\r\n";
    oss << "Max Tokens: " << m_settings.aiMaxTokens << "\r\n";
    oss << "Context Window: " << m_settings.aiContextWindow << "\r\n";
    oss << "Model Path: " << (m_settings.aiModelPath.empty() ? "(none)" : m_settings.aiModelPath) << "\r\n";
    oss << "Ollama URL: " << m_settings.aiOllamaUrl << "\r\n";
    oss << "Ghost Text: " << (m_settings.ghostTextEnabled ? "ON" : "OFF") << "\r\n";
    oss << "Failure Detector: " << (m_settings.failureDetectorEnabled ? "ON" : "OFF")
        << " (retries: " << m_settings.failureMaxRetries << ")\r\n";
    oss << "AMD Unified Memory (SAM): " << (m_settings.amdUnifiedMemoryEnabled ? "ON" : "OFF") << "\r\n";
    oss << "\r\n=== EDITOR ===\r\n";
    oss << "Tab Size: " << m_settings.tabSize << "\r\n";
    oss << "Use Spaces: " << (m_settings.useSpaces ? "ON" : "OFF") << "\r\n";
    oss << "Encoding: " << m_settings.encoding << "\r\n";
    oss << "EOL: " << m_settings.eolStyle << "\r\n";
    oss << "Syntax Coloring: " << (m_settings.syntaxColoringEnabled ? "ON" : "OFF") << "\r\n";
    oss << "Minimap: " << (m_settings.minimapEnabled ? "ON" : "OFF") << "\r\n";
    oss << "\r\n=== THEME ===\r\n";
    oss << "Theme ID: " << m_settings.themeId << "\r\n";
    oss << "Transparency: " << (int)m_settings.windowAlpha << "/255\r\n";
    oss << "\r\n=== LOCAL SERVER ===\r\n";
    oss << "Enabled: " << (m_settings.localServerEnabled ? "ON" : "OFF") << "\r\n";
    oss << "Port: " << m_settings.localServerPort << "\r\n";
    oss << "\r\n[Settings file: " << getSettingsFilePath() << "]\r\n";

    // Use a scrollable edit dialog
    HWND hDlg = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        "STATIC", "RawrXD Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 550, 600,
        m_hwndMain, nullptr, m_hInstance, nullptr);

    if (!hDlg) {
        // Fallback to MessageBox
        MessageBoxA(m_hwndMain, oss.str().c_str(), "RawrXD Settings", MB_OK);
        return;
    }

    HWND hEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", oss.str().c_str(),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        10, 10, 510, 500,
        hDlg, nullptr, m_hInstance, nullptr);

    // Apply theme colors
    HFONT hFont = CreateFontA(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    SendMessageA(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Open full Settings (property-grid dialog), Save (no-op for read-only summary), Close
    HWND hFull = CreateWindowExA(0, "BUTTON", "Open full Settings...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, 520, 180, 30, hDlg, (HMENU)1000, m_hInstance, nullptr);
    HWND hClose = CreateWindowExA(0, "BUTTON", "Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        440, 520, 80, 30, hDlg, (HMENU)1002, m_hInstance, nullptr);

    SendMessageA(hFull, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageA(hClose, WM_SETFONT, (WPARAM)hFont, TRUE);

    SetWindowLongPtrA(hDlg, GWLP_USERDATA, (LONG_PTR)this);
    s_oldSettingsSummaryProc = (WNDPROC)SetWindowLongPtrA(hDlg, GWLP_WNDPROC, (LONG_PTR)SettingsSummaryWndProc);

    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);
}
