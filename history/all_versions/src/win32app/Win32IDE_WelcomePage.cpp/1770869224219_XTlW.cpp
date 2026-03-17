// ============================================================================
// Win32IDE_WelcomePage.cpp — Tier 1 Cosmetic #6: Welcome / Onboarding Page
// ============================================================================
// VS Code-style "Get Started" welcome page shown on first launch.
// Uses WebView2 to render an HTML welcome page with:
//   - Clone Repo / Open Folder / New File quick actions
//   - Recent files list
//   - Links to docs, settings, extensions
//   - Interactive walkthroughs for first-time users
//
// Pattern:  WebView2 HTML content, no exceptions
// Threading: UI thread only
// ============================================================================

#include "Win32IDE.h"
#include <sstream>
#include <fstream>

#ifndef RAWRXD_LOG_INFO
#define RAWRXD_LOG_INFO(msg) do { \
    std::ostringstream _oss; _oss << "[INFO] " << msg << "\n"; \
    OutputDebugStringA(_oss.str().c_str()); \
} while(0)
#endif

// ============================================================================
// FIRST LAUNCH DETECTION
// ============================================================================

bool Win32IDE::isFirstLaunch()
{
    char appDataPath[MAX_PATH] = {};
    if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath) != S_OK) {
        return true;
    }
    std::string markerPath = std::string(appDataPath) + "\\RawrXD\\.first_launch_done";
    return !GetFileAttributesA(markerPath.c_str()) || GetFileAttributesA(markerPath.c_str()) == INVALID_FILE_ATTRIBUTES;
}

void Win32IDE::markFirstLaunchDone()
{
    char appDataPath[MAX_PATH] = {};
    if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath) != S_OK) return;

    std::string dir = std::string(appDataPath) + "\\RawrXD";
    CreateDirectoryA(dir.c_str(), nullptr);

    std::string markerPath = dir + "\\.first_launch_done";
    std::ofstream f(markerPath);
    if (f) {
        f << "1";
        f.close();
    }
}

// ============================================================================
// GENERATE WELCOME HTML — VS Code-style Get Started page
// ============================================================================

std::string Win32IDE::generateWelcomeHTML()
{
    // Build recent files list
    std::string recentFilesHtml;
    for (size_t i = 0; i < m_recentFiles.size() && i < 10; i++) {
        const auto& path = m_recentFiles[i];
        // Extract filename
        size_t lastSlash = path.find_last_of("\\/");
        std::string name = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
        recentFilesHtml += "<li class='recent-item' onclick=\"sendAction('openRecent:" + std::to_string(i) + "')\">"
                           "<span class='file-icon'>📄</span>"
                           "<span class='file-name'>" + name + "</span>"
                           "<span class='file-path'>" + path + "</span>"
                           "</li>";
    }

    if (recentFilesHtml.empty()) {
        recentFilesHtml = "<li class='recent-item empty'>No recent files</li>";
    }

    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    background: #1e1e1e;
    color: #cccccc;
    height: 100vh;
    overflow-y: auto;
}
.container {
    max-width: 900px;
    margin: 0 auto;
    padding: 40px 30px;
}
.header {
    text-align: center;
    margin-bottom: 48px;
}
.header h1 {
    font-size: 28px;
    font-weight: 300;
    color: #e0e0e0;
    margin-bottom: 8px;
}
.header .subtitle {
    font-size: 14px;
    color: #888;
}
.header .version {
    font-size: 12px;
    color: #555;
    margin-top: 4px;
}
.columns {
    display: flex;
    gap: 40px;
    align-items: flex-start;
}
.column {
    flex: 1;
}
.section-title {
    font-size: 13px;
    font-weight: 600;
    text-transform: uppercase;
    color: #888;
    margin-bottom: 16px;
    letter-spacing: 0.5px;
}
.action-list {
    list-style: none;
}
.action-item {
    display: flex;
    align-items: center;
    padding: 10px 12px;
    margin-bottom: 4px;
    border-radius: 6px;
    cursor: pointer;
    transition: background 0.15s;
}
.action-item:hover {
    background: #2a2d2e;
}
.action-item .icon {
    font-size: 20px;
    margin-right: 14px;
    width: 28px;
    text-align: center;
}
.action-item .label {
    font-size: 14px;
    color: #3794ff;
}
.action-item .desc {
    font-size: 12px;
    color: #888;
    margin-top: 2px;
}
.recent-list {
    list-style: none;
    max-height: 300px;
    overflow-y: auto;
}
.recent-item {
    display: flex;
    flex-direction: column;
    padding: 8px 12px;
    border-radius: 4px;
    cursor: pointer;
    transition: background 0.15s;
    margin-bottom: 2px;
}
.recent-item:hover {
    background: #2a2d2e;
}
.recent-item .file-icon {
    margin-right: 8px;
}
.recent-item .file-name {
    font-size: 13px;
    color: #e0e0e0;
}
.recent-item .file-path {
    font-size: 11px;
    color: #666;
    margin-top: 2px;
}
.recent-item.empty {
    color: #555;
    cursor: default;
}
.recent-item.empty:hover { background: transparent; }
.walkthroughs {
    margin-top: 36px;
    border-top: 1px solid #333;
    padding-top: 24px;
}
.walkthrough-item {
    display: flex;
    align-items: center;
    padding: 12px 16px;
    border: 1px solid #333;
    border-radius: 8px;
    margin-bottom: 10px;
    cursor: pointer;
    transition: border-color 0.15s, background 0.15s;
}
.walkthrough-item:hover {
    border-color: #3794ff;
    background: #1a2332;
}
.walkthrough-item .wt-icon {
    font-size: 24px;
    margin-right: 16px;
}
.walkthrough-item .wt-title {
    font-size: 14px;
    color: #e0e0e0;
    font-weight: 500;
}
.walkthrough-item .wt-desc {
    font-size: 12px;
    color: #888;
    margin-top: 2px;
}
.footer {
    text-align: center;
    margin-top: 40px;
    padding-top: 16px;
    border-top: 1px solid #333;
}
.footer label {
    font-size: 12px;
    color: #666;
    cursor: pointer;
}
.footer input[type="checkbox"] {
    margin-right: 6px;
}
.kbd {
    background: #333;
    border: 1px solid #555;
    border-radius: 3px;
    padding: 1px 6px;
    font-size: 11px;
    font-family: monospace;
    color: #ccc;
}
</style>
</head>
<body>
<div class="container">
    <div class="header">
        <h1>Welcome to RawrXD IDE</h1>
        <div class="subtitle">Advanced GGUF Model Loader with Live Hotpatching &amp; Agentic Correction</div>
        <div class="version">Build 2026.02 &middot; Powered by MASM64 + Win32</div>
    </div>

    <div class="columns">
        <div class="column">
            <div class="section-title">Start</div>
            <ul class="action-list">
                <li class="action-item" onclick="sendAction('newFile')">
                    <div class="icon">📝</div>
                    <div>
                        <div class="label">New File</div>
                        <div class="desc">Create a new untitled file</div>
                    </div>
                </li>
                <li class="action-item" onclick="sendAction('openFile')">
                    <div class="icon">📂</div>
                    <div>
                        <div class="label">Open File</div>
                        <div class="desc">Open an existing file</div>
                    </div>
                </li>
                <li class="action-item" onclick="sendAction('openFolder')">
                    <div class="icon">🗂️</div>
                    <div>
                        <div class="label">Open Folder</div>
                        <div class="desc">Open a workspace folder</div>
                    </div>
                </li>
                <li class="action-item" onclick="sendAction('cloneRepo')">
                    <div class="icon">🔗</div>
                    <div>
                        <div class="label">Clone Repository</div>
                        <div class="desc">Clone a Git repository from URL</div>
                    </div>
                </li>
                <li class="action-item" onclick="sendAction('loadModel')">
                    <div class="icon">🧠</div>
                    <div>
                        <div class="label">Load AI Model</div>
                        <div class="desc">Open a GGUF model for inference</div>
                    </div>
                </li>
            </ul>
        </div>

        <div class="column">
            <div class="section-title">Recent</div>
            <ul class="recent-list">
                )" << recentFilesHtml << R"(
            </ul>
        </div>
    </div>

    <div class="walkthroughs">
        <div class="section-title">Walkthroughs</div>

        <div class="walkthrough-item" onclick="sendAction('walkthrough:editor')">
            <div class="wt-icon">⌨️</div>
            <div>
                <div class="wt-title">Get Started with the Editor</div>
                <div class="wt-desc">Learn navigation, shortcuts, and essential features</div>
            </div>
        </div>

        <div class="walkthrough-item" onclick="sendAction('walkthrough:ai')">
            <div class="wt-icon">🤖</div>
            <div>
                <div class="wt-title">AI-Powered Development</div>
                <div class="wt-desc">Load models, use ghost text, and run agentic loops</div>
            </div>
        </div>

        <div class="walkthrough-item" onclick="sendAction('walkthrough:extensions')">
            <div class="wt-icon">🧩</div>
            <div>
                <div class="wt-title">Extensions &amp; Plugins</div>
                <div class="wt-desc">Browse marketplace, install VSIX, and configure plugins</div>
            </div>
        </div>

        <div class="walkthrough-item" onclick="sendAction('settings')">
            <div class="wt-icon">⚙️</div>
            <div>
                <div class="wt-title">Customize Settings</div>
                <div class="wt-desc">Configure themes, fonts, AI parameters, and more</div>
            </div>
        </div>
    </div>

    <div class="footer">
        <label>
            <input type="checkbox" id="showOnStartup" checked onchange="sendAction('toggleWelcome:' + this.checked)">
            Show Welcome Page on Startup
        </label>
        &nbsp;&nbsp;&middot;&nbsp;&nbsp;
        <span style="color: #555; font-size: 12px;">
            <span class="kbd">Ctrl+Shift+P</span> Command Palette
            &nbsp;&middot;&nbsp;
            <span class="kbd">Ctrl+,</span> Settings
        </span>
    </div>
</div>

<script>
function sendAction(action) {
    if (window.chrome && window.chrome.webview) {
        window.chrome.webview.postMessage(JSON.stringify({ type: 'welcomeAction', action: action }));
    }
}
</script>
</body>
</html>)";

    return html.str();
}

// ============================================================================
// SHOW WELCOME PAGE — Create WebView2 tab with welcome HTML
// ============================================================================

void Win32IDE::showWelcomePage()
{
    if (m_welcomePageShown) return;

    // Add a "Welcome" tab to the editor
    addTab("__welcome__", "Welcome");

    // Generate HTML content
    std::string welcomeHtml = generateWelcomeHTML();

    // For now, display in the editor as a placeholder message if no WebView2
    // In production, this uses the WebView2 container (createMonacoEditor path)
    if (m_hwndEditor) {
        std::string plainText =
            "=== Welcome to RawrXD IDE ===\r\n\r\n"
            "Advanced GGUF Model Loader with Live Hotpatching & Agentic Correction\r\n"
            "Build 2026.02 | Powered by MASM64 + Win32\r\n\r\n"
            "--- Quick Start ---\r\n"
            "  Ctrl+N         New File\r\n"
            "  Ctrl+O         Open File\r\n"
            "  Ctrl+Shift+P   Command Palette\r\n"
            "  Ctrl+,         Settings\r\n\r\n"
            "--- Getting Started ---\r\n"
            "  1. Open or create files using the Explorer sidebar\r\n"
            "  2. Load a GGUF model for AI-powered development\r\n"
            "  3. Use the Copilot Chat panel for code assistance\r\n"
            "  4. Install extensions from the marketplace\r\n\r\n"
            "--- AI Features ---\r\n"
            "  Ghost Text:     Inline AI completions as you type\r\n"
            "  Agentic Loop:   Autonomous multi-step code generation\r\n"
            "  Failure Detect: Automatic hallucination/refusal detection\r\n\r\n"
            "Tip: Use 'View > Welcome' to show this page again.\r\n";

        SetWindowTextA(m_hwndEditor, plainText.c_str());
    }

    // Write the welcome HTML to temp for WebView2 if available
    char tempPath[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tempPath)) {
        std::string htmlPath = std::string(tempPath) + "rawrxd_welcome.html";
        std::ofstream f(htmlPath);
        if (f) {
            f << welcomeHtml;
            f.close();
            RAWRXD_LOG_INFO("Welcome page HTML written to " << htmlPath);
        }
    }

    m_welcomePageShown = true;
    markFirstLaunchDone();
    RAWRXD_LOG_INFO("Welcome page displayed");
}

// ============================================================================
// CLOSE WELCOME PAGE
// ============================================================================

void Win32IDE::closeWelcomePage()
{
    int tabIdx = findTabByPath("__welcome__");
    if (tabIdx >= 0) {
        removeTab(tabIdx);
    }
    m_welcomePageShown = false;
}

// ============================================================================
// WELCOME ACTION HANDLER — Process user clicks from welcome page
// ============================================================================

void Win32IDE::onWelcomeAction(const std::string& action)
{
    if (action == "newFile") {
        closeWelcomePage();
        newFile();
    }
    else if (action == "openFile") {
        closeWelcomePage();
        openFile();
    }
    else if (action == "openFolder") {
        closeWelcomePage();
        // Open folder dialog
        char folderPath[MAX_PATH] = {};
        BROWSEINFOA bi = {};
        bi.hwndOwner = m_hwndMain;
        bi.lpszTitle = "Select Workspace Folder";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        PIDLIST_ABSOLUTE pidl = SHBrowseForFolderA(&bi);
        if (pidl) {
            if (SHGetPathFromIDListA(pidl, folderPath)) {
                m_settings.workingDirectory = folderPath;
                // Refresh file tree
                SetCurrentDirectoryA(folderPath);
            }
            CoTaskMemFree(pidl);
        }
    }
    else if (action == "cloneRepo") {
        closeWelcomePage();
        // Show clone dialog (basic URL input)
        // Dispatched via command palette for now
        PostMessage(m_hwndMain, WM_COMMAND, 8001 /* IDM_GIT_CLONE */, 0);
    }
    else if (action == "loadModel") {
        closeWelcomePage();
        openModel();
    }
    else if (action == "settings") {
        showSettingsGUI();
    }
    else if (action.find("openRecent:") == 0) {
        int idx = atoi(action.c_str() + 11);
        closeWelcomePage();
        openRecentFile(idx);
    }
    else if (action.find("toggleWelcome:") == 0) {
        bool checked = (action.find("true") != std::string::npos);
        m_settings.showWelcomeOnStartup = checked;
        saveSettings();
    }

    RAWRXD_LOG_INFO("Welcome action: " << action);
}
