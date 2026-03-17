// =============================================================================
// Win32IDE Auto-Save System — VS Code Parity
// =============================================================================
// Implements: timer-based periodic saves, focus-loss saves, configurable modes,
// dirty-file tracking, external file change detection + reload prompt.
// =============================================================================
// Save modes (matching VS Code):
//   "off"            — disabled
//   "afterDelay"     — timer-based (default 1000ms in VS Code, we use 30s)
//   "onFocusChange"  — save when editor loses focus
//   "onWindowChange" — save when window deactivates
// =============================================================================

#include "Win32IDE.h"
#include <chrono>
#include <filesystem>
#include <fstream>

#ifndef IDM_FILE_AUTOSAVE
#define IDM_FILE_AUTOSAVE 106
#endif

// ─── Timer IDs ───────────────────────────────────────────────────────────────
static constexpr UINT_PTR TIMER_AUTOSAVE       = 9001;
static constexpr UINT_PTR TIMER_FILE_WATCH     = 9002;
static constexpr UINT     AUTOSAVE_DEFAULT_MS  = 30000;  // 30 seconds
static constexpr UINT     FILEWATCH_INTERVAL   = 2000;   // 2 seconds

// ─── Auto-Save Mode: use Win32IDE::AutoSaveMode from header ─────────────────
using AutoSaveMode = Win32IDE::AutoSaveMode;

// ─── File State Tracking ─────────────────────────────────────────────────────
struct TrackedFileState {
    std::string path;
    uint64_t lastKnownWriteTime = 0;  // filesystem last_write_time
    bool isDirty = false;              // local modifications not saved
    bool externallyModified = false;   // changed outside IDE
};

// =============================================================================
// initAutoSave — Sets up the auto-save system
// =============================================================================

void Win32IDE::initAutoSave()
{
    // Read config
    m_autoSaveEnabled = m_settings.autoSaveEnabled;
    m_autoSaveIntervalMs = m_settings.autoSaveIntervalSec * 1000;
    if (m_autoSaveIntervalMs < 1000) m_autoSaveIntervalMs = AUTOSAVE_DEFAULT_MS;

    // Determine mode from settings
    // Default: afterDelay if enabled, off if disabled
    m_autoSaveMode = m_autoSaveEnabled ? AutoSaveMode::AfterDelay : AutoSaveMode::Off;

    if (m_autoSaveEnabled) {
        startAutoSaveTimer();
    }

    // Always start file watch timer for external change detection
    if (m_hwndMain) {
        SetTimer(m_hwndMain, TIMER_FILE_WATCH, FILEWATCH_INTERVAL, nullptr);
    }

    appendToOutput(std::string("[Auto-Save] Initialized: ") + (m_autoSaveEnabled ? "ON" : "OFF") + ", interval=" + std::to_string(m_autoSaveIntervalMs) + "ms\n", "Output", OutputSeverity::Info);
}

// =============================================================================
// toggleAutoSave — Called from menu/command
// =============================================================================

void Win32IDE::toggleAutoSave()
{
    m_autoSaveEnabled = !m_autoSaveEnabled;
    m_settings.autoSaveEnabled = m_autoSaveEnabled;

    if (m_autoSaveEnabled) {
        m_autoSaveMode = AutoSaveMode::AfterDelay;
        startAutoSaveTimer();
        appendToOutput("[Auto-Save] Enabled — files save every "
                      + std::to_string(m_autoSaveIntervalMs / 1000) + "s\n",
                      "Output", OutputSeverity::Info);
    } else {
        m_autoSaveMode = AutoSaveMode::Off;
        stopAutoSaveTimer();
        appendToOutput("[Auto-Save] Disabled\n", "Output", OutputSeverity::Info);
    }

    // Update menu checkmark
    HMENU hMenu = GetMenu(m_hwndMain);
    if (hMenu) {
        CheckMenuItem(hMenu, IDM_FILE_AUTOSAVE,
                      m_autoSaveEnabled ? MF_CHECKED : MF_UNCHECKED);
    }
}

// =============================================================================
// setAutoSaveMode — Configure the save trigger
// =============================================================================

void Win32IDE::setAutoSaveMode(AutoSaveMode mode)
{
    m_autoSaveMode = mode;
    switch (mode) {
    case AutoSaveMode::Off:
        m_autoSaveEnabled = false;
        stopAutoSaveTimer();
        break;
    case AutoSaveMode::AfterDelay:
        m_autoSaveEnabled = true;
        startAutoSaveTimer();
        break;
    case AutoSaveMode::OnFocusChange:
        m_autoSaveEnabled = true;
        stopAutoSaveTimer();
        break;
    case AutoSaveMode::OnWindowChange:
        m_autoSaveEnabled = true;
        stopAutoSaveTimer();
        break;
    }
}

// =============================================================================
// startAutoSaveTimer / stopAutoSaveTimer
// =============================================================================

void Win32IDE::startAutoSaveTimer()
{
    if (m_hwndMain) {
        SetTimer(m_hwndMain, TIMER_AUTOSAVE, m_autoSaveIntervalMs, nullptr);
    }
}

void Win32IDE::stopAutoSaveTimer()
{
    if (m_hwndMain) {
        KillTimer(m_hwndMain, TIMER_AUTOSAVE);
    }
}

// =============================================================================
// handleAutoSaveTimer — Called from WM_TIMER
// =============================================================================

void Win32IDE::handleAutoSaveTimer(UINT_PTR timerId)
{
    if (timerId == TIMER_AUTOSAVE) {
        if (m_autoSaveEnabled && m_autoSaveMode == AutoSaveMode::AfterDelay) {
            autoSaveDirtyFiles();
        }
    }
    else if (timerId == TIMER_FILE_WATCH) {
        checkExternalFileChanges();
    }
}

// =============================================================================
// autoSaveDirtyFiles — Saves all modified files
// =============================================================================

void Win32IDE::autoSaveDirtyFiles()
{
    if (!m_fileModified) return; // current file not dirty
    if (m_currentFile.empty()) return; // untitled — don't auto-save

    // Save current file silently
    std::string path = m_currentFile;

    // Get editor content
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0) return;

    std::string content(textLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &content[0], textLen + 1);
    content.resize(textLen);

    // Write to file
    std::ofstream ofs(path, std::ios::trunc | std::ios::binary);
    if (!ofs.is_open()) {
        appendToOutput("[Auto-Save] Failed to save: " + path + "\n", "Output", OutputSeverity::Warning);
        return;
    }
    ofs.write(content.data(), content.size());
    ofs.close();

    m_fileModified = false;

    // Update title bar (remove asterisk)
    updateTitleBarText();

    // Update tracked file state
    try {
        m_lastKnownFileWriteTime = std::filesystem::last_write_time(path);
    } catch (...) {}

    // Subtle status bar notification
    if (m_hwndStatusBar)
        SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0, (LPARAM)"Auto-saved");
}

// =============================================================================
// onEditorFocusLost — Called when editor loses focus
// =============================================================================

void Win32IDE::onEditorFocusLost()
{
    if (m_autoSaveEnabled && m_autoSaveMode == AutoSaveMode::OnFocusChange) {
        autoSaveDirtyFiles();
    }
}

// =============================================================================
// onWindowDeactivated — Called on WM_ACTIVATEAPP(FALSE) or WM_ACTIVATE(WA_INACTIVE)
// =============================================================================

void Win32IDE::onWindowDeactivated()
{
    if (m_autoSaveEnabled &&
        (m_autoSaveMode == AutoSaveMode::OnWindowChange ||
         m_autoSaveMode == AutoSaveMode::OnFocusChange)) {
        autoSaveDirtyFiles();
    }
}

// =============================================================================
// checkExternalFileChanges — Periodic scan for external modifications
// =============================================================================

void Win32IDE::checkExternalFileChanges()
{
    if (m_currentFile.empty()) return;

    try {
        if (!std::filesystem::exists(m_currentFile)) return;

        auto ftime = std::filesystem::last_write_time(m_currentFile);

        if (m_lastKnownFileWriteTime.time_since_epoch().count() != 0 && ftime != m_lastKnownFileWriteTime) {
            // File was modified externally
            m_lastKnownFileWriteTime = ftime;

            // If we have local modifications, prompt for reload
            if (m_fileModified) {
                std::string msg = "The file '" +
                    std::filesystem::path(m_currentFile).filename().string() +
                    "' has been changed outside the editor.\n\n"
                    "Do you want to reload it? (Your changes will be lost)";

                int result = MessageBoxA(m_hwndMain, msg.c_str(),
                                        "File Changed Externally",
                                        MB_YESNO | MB_ICONQUESTION);
                if (result == IDYES) {
                    reloadCurrentFile();
                }
            } else {
                // No local changes — silently reload
                reloadCurrentFile();
                if (m_hwndStatusBar)
                    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0, (LPARAM)"File reloaded (external change)");
            }
        }
    } catch (...) {
        // Filesystem errors are non-fatal for the watcher
    }
}

// =============================================================================
// reloadCurrentFile — Re-reads the current file from disk
// =============================================================================

// reloadCurrentFile() defined in Win32IDE_Tier3Polish.cpp — use existing implementation

// =============================================================================
// shutdownAutoSave — Cleanup
// =============================================================================

void Win32IDE::shutdownAutoSave()
{
    stopAutoSaveTimer();
    if (m_hwndMain) {
        KillTimer(m_hwndMain, TIMER_FILE_WATCH);
    }

    // Offer to save dirty files on shutdown
    if (m_autoSaveEnabled && m_fileModified && !m_currentFile.empty()) {
        autoSaveDirtyFiles();
    }
}
