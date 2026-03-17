// ============================================================================
// Win32IDE_Session.cpp — Session Persistence
// ============================================================================
// Saves and restores IDE state across launches:
//   - Open tabs (file paths, display names, active tab)
//   - Editor cursor position and scroll offset
//   - Panel visibility and height
//   - Sidebar state and width
//   - Current working directory
//
// Session file: %APPDATA%\RawrXD\session.json
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <shlobj.h>
#include <richedit.h>

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

// ============================================================================
// SAVE SESSION — master entry point
// ============================================================================
void Win32IDE::saveSession() {
    LOG_INFO("Saving IDE session...");
    
    try {
        nlohmann::json session;
        session["version"] = 1;
        session["timestamp"] = std::to_string(GetTickCount64());
        
        // Save each section
        saveSessionTabs(session);
        saveSessionPanelState(session);
        saveSessionEditorState(session);
        saveSessionAnnotations(session);
        saveSessionTheme(session);
        
        // Save current file
        session["currentFile"] = m_currentFile;
        
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
        
        // Validate version
        int version = session.value("version", 0);
        if (version < 1) {
            LOG_WARNING("Unknown session version: " + std::to_string(version));
            return;
        }
        
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
        
        m_sessionRestored = true;
        LOG_INFO("Session restored successfully.");
        
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
    m_sidebarWidth = panel.value("sidebarWidth", 240);
    
    int viewIdx = panel.value("sidebarView", 0);
    m_currentSidebarView = static_cast<SidebarView>(viewIdx);
    
    // Apply the restored layout
    if (m_hwndMain) {
        RECT rc;
        GetClientRect(m_hwndMain, &rc);
        SendMessage(m_hwndMain, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rc.right, rc.bottom));
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
