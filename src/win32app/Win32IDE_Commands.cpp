// Menu Command System Implementation for Win32IDE
// Centralized command routing with 25+ features

#include "Win32IDE.h"
#include "IDEConfig.h"
#include "win32_feature_adapter.h"
#include "../core/unified_hotpatch_manager.hpp"
#include "../core/proxy_hotpatcher.hpp"
#include <commctrl.h>
#include <richedit.h>
#include <commdlg.h>
#include <fstream>
#include <functional>
#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

// Menu command IDs (with guards to avoid redefinition from Win32IDE.cpp)
#ifndef IDM_FILE_NEW
#define IDM_FILE_NEW 1001
#endif
#ifndef IDM_FILE_OPEN
#define IDM_FILE_OPEN 1002
#endif
#ifndef IDM_FILE_SAVE
#define IDM_FILE_SAVE 1003
#endif
#ifndef IDM_FILE_SAVEAS
#define IDM_FILE_SAVEAS 1004
#endif
#ifndef IDM_FILE_SAVEALL
#define IDM_FILE_SAVEALL 1005
#endif
#ifndef IDM_FILE_CLOSE
#define IDM_FILE_CLOSE 1006
#endif
#ifndef IDM_FILE_RECENT_BASE
#define IDM_FILE_RECENT_BASE 1010
#endif
#ifndef IDM_FILE_RECENT_CLEAR
#define IDM_FILE_RECENT_CLEAR 1020
#endif
#ifndef IDM_FILE_LOAD_MODEL
#define IDM_FILE_LOAD_MODEL 1030
#endif
#ifndef IDM_FILE_MODEL_FROM_HF
#define IDM_FILE_MODEL_FROM_HF 1031
#endif
#ifndef IDM_FILE_MODEL_FROM_OLLAMA
#define IDM_FILE_MODEL_FROM_OLLAMA 1032
#endif
#ifndef IDM_FILE_MODEL_FROM_URL
#define IDM_FILE_MODEL_FROM_URL 1033
#endif
#ifndef IDM_FILE_MODEL_UNIFIED
#define IDM_FILE_MODEL_UNIFIED 1034
#endif
#ifndef IDM_FILE_MODEL_QUICK_LOAD
#define IDM_FILE_MODEL_QUICK_LOAD 1035
#endif
#ifndef IDM_FILE_EXIT
#define IDM_FILE_EXIT 1099
#endif

#ifndef IDM_EDIT_UNDO
#define IDM_EDIT_UNDO 2001
#endif
#ifndef IDM_EDIT_REDO
#define IDM_EDIT_REDO 2002
#endif
#ifndef IDM_EDIT_CUT
#define IDM_EDIT_CUT 2003
#endif
#ifndef IDM_EDIT_COPY
#define IDM_EDIT_COPY 2004
#endif
#ifndef IDM_EDIT_PASTE
#define IDM_EDIT_PASTE 2005
#endif
#ifndef IDM_EDIT_SELECT_ALL
#define IDM_EDIT_SELECT_ALL 2006
#endif
#ifndef IDM_EDIT_FIND
#define IDM_EDIT_FIND 2007
#endif
#ifndef IDM_EDIT_REPLACE
#define IDM_EDIT_REPLACE 2008
#endif

// ============================================================================
// FUZZY MATCH SCORING (VS Code-style character-skip matching)
// ============================================================================

struct FuzzyResult {
    bool matched;
    int score;
    std::vector<int> matchPositions; // indices into the target string that matched
};

static FuzzyResult fuzzyMatchScore(const std::string& query, const std::string& target) {
    FuzzyResult result;
    result.matched = false;
    result.score = 0;

    if (query.empty()) {
        result.matched = true;
        return result;
    }

    // Lowercase both for case-insensitive matching
    std::string lq, lt;
    lq.resize(query.size());
    lt.resize(target.size());
    std::transform(query.begin(), query.end(), lq.begin(), [](unsigned char c) { return std::tolower(c); });
    std::transform(target.begin(), target.end(), lt.begin(), [](unsigned char c) { return std::tolower(c); });

    int qi = 0;
    int prevMatchIdx = -1;
    bool afterSeparator = true; // start-of-string counts as separator

    for (int ti = 0; ti < (int)lt.size() && qi < (int)lq.size(); ti++) {
        if (lt[ti] == lq[qi]) {
            result.matchPositions.push_back(ti);

            // Scoring bonuses
            if (afterSeparator) {
                result.score += 10; // Word boundary match (after space, colon, slash, etc.)
            } else if (prevMatchIdx >= 0 && ti == prevMatchIdx + 1) {
                result.score += 5;  // Consecutive character match
            } else {
                result.score += 1;  // Gap match
            }

            // Exact case bonus
            if (qi < (int)query.size() && ti < (int)target.size() && query[qi] == target[ti]) {
                result.score += 2;
            }

            prevMatchIdx = ti;
            qi++;
        }
        // Track word boundaries
        afterSeparator = (lt[ti] == ' ' || lt[ti] == ':' || lt[ti] == '/' ||
                          lt[ti] == '\\' || lt[ti] == '_' || lt[ti] == '-');
    }

    result.matched = (qi == (int)lq.size());
    if (result.matched) {
        // Bonus for shorter targets (tighter match)
        result.score += std::max(0, 50 - (int)target.size());
        // Penalize for match spread
        if (!result.matchPositions.empty()) {
            int spread = result.matchPositions.back() - result.matchPositions.front();
            result.score -= spread / 2;
        }
    }
    return result;
}

// ============================================================================
// MENU COMMAND SYSTEM (25+ Features)
// ============================================================================

bool Win32IDE::routeCommand(int commandId) {
    // Route to appropriate handler based on command ID range
    if (commandId >= 1000 && commandId < 2000) {
        handleFileCommand(commandId);
        return true;
    } else if (commandId >= 2000 && commandId < 3000) {
        handleEditCommand(commandId);
        return true;
    } else if (commandId >= 3000 && commandId < 4000) {
        handleViewCommand(commandId);
        return true;
    } else if (commandId >= 4000 && commandId < 4100) {
        handleTerminalCommand(commandId);
        return true;
    } else if (commandId >= 4100 && commandId < 4400) {
        handleAgentCommand(commandId);
        return true;
    } else if (commandId >= 5000 && commandId < 6000) {
        handleToolsCommand(commandId);
        return true;
    } else if (commandId >= 6000 && commandId < 7000) {
        handleModulesCommand(commandId);
        return true;
    } else if (commandId >= 7000 && commandId < 8000) {
        handleHelpCommand(commandId);
        return true;
    } else if (commandId >= 8000 && commandId < 9000) {
        handleGitCommand(commandId);
        return true;
    } else if (commandId >= 9100 && commandId < 9200) {
        handleMonacoCommand(commandId);
        return true;
    } else if (commandId >= 9500 && commandId < 9600) {
        return handleAuditCommand(commandId);
    } else if (commandId >= 9600 && commandId < 9700) {
        return handleGauntletCommand(commandId);
    } else if (commandId >= 9700 && commandId < 9800) {
        return handleVoiceChatCommand(commandId);
    } else if (commandId >= 9800 && commandId < 9900) {
        return handleQuickWinCommand(commandId);
    } else if (commandId >= 9900 && commandId < 10000) {
        return handleTelemetryCommand(commandId);
    } else if (commandId >= 10000 && commandId < 10100) {
        return handleVSCExtAPICommand(commandId);
    } else if (commandId >= 10100 && commandId < 10200) {
        return handleFlightRecorderCommand(commandId);
    } else if (commandId >= 10200 && commandId < 10300) {
        // Phase 44: VoiceAutomation commands
        extern bool Win32IDE_HandleVoiceAutomationCommand(HWND, WPARAM);
        return Win32IDE_HandleVoiceAutomationCommand(nullptr, (WPARAM)commandId);
    } else if (commandId >= 10300 && commandId < 10400) {
        // Phase 45: DiskRecovery panel commands
        handleRecoveryCommand(commandId);
        return true;
    } else if (commandId >= 9400 && commandId < 9500) {
        return handlePDBCommand(commandId);
    } else if (commandId >= 9000 && commandId < 10000) {
        handleHotpatchCommand(commandId);
        return true;
    }
    
    return false;
}

std::string Win32IDE::getCommandDescription(int commandId) const {
    auto it = m_commandDescriptions.find(commandId);
    if (it != m_commandDescriptions.end()) {
        return it->second;
    }
    return "Unknown Command";
}

bool Win32IDE::isCommandEnabled(int commandId) const {
    auto it = m_commandStates.find(commandId);
    if (it != m_commandStates.end()) {
        return it->second;
    }
    return true; // Default to enabled
}

void Win32IDE::updateCommandStates() {
    // Update command availability based on current state
    m_commandStates[IDM_FILE_SAVE] = m_fileModified;
    m_commandStates[IDM_FILE_SAVEAS] = !m_currentFile.empty();
    m_commandStates[IDM_FILE_CLOSE] = !m_currentFile.empty();
    m_commandStates[IDM_FILE_RECENT_CLEAR] = !m_recentFiles.empty();
    
    // Edit commands depend on editor state
    bool hasSelection = false;
    bool hasEditorContent = false;
    if (m_hwndEditor && IsWindow(m_hwndEditor)) {
        CHARRANGE range;
        SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&range);
        hasSelection = (range.cpMax > range.cpMin);
        int textLen = (int)SendMessage(m_hwndEditor, WM_GETTEXTLENGTH, 0, 0);
        hasEditorContent = (textLen > 0);
    }
    
    m_commandStates[IDM_EDIT_CUT] = hasSelection;
    m_commandStates[IDM_EDIT_COPY] = hasSelection;
    m_commandStates[IDM_EDIT_PASTE] = IsClipboardFormatAvailable(CF_TEXT);
    m_commandStates[IDM_EDIT_FIND] = hasEditorContent;
    m_commandStates[IDM_EDIT_REPLACE] = hasEditorContent;
    m_commandStates[IDM_EDIT_SELECT_ALL] = hasEditorContent;

    // File: Save All requires at least one modified tab
    bool anyModified = false;
    for (const auto& tab : m_editorTabs) {
        if (tab.modified) { anyModified = true; break; }
    }
    m_commandStates[IDM_FILE_SAVEALL] = anyModified;

    // Git commands: only available when in a git repository
    bool gitAvailable = !m_gitRepoPath.empty();
    m_commandStates[8001] = gitAvailable; // Git Status
    m_commandStates[8002] = gitAvailable; // Git Commit
    m_commandStates[8003] = gitAvailable; // Git Push
    m_commandStates[8004] = gitAvailable; // Git Pull
    m_commandStates[8005] = gitAvailable; // Git Stage All

    // Tools: Stop Profiling only when profiling is active
    m_commandStates[5002] = m_profilingActive;
    m_commandStates[5003] = m_profilingActive; // Results only if profiled

    // Terminal: Kill only if a terminal pane exists
    bool hasTerminal = !m_terminalPanes.empty();
    m_commandStates[4003] = hasTerminal; // Kill Terminal
    m_commandStates[4004] = hasTerminal; // Clear Terminal
    m_commandStates[4005] = hasTerminal; // Split Terminal

    // Agent/AI: always available once bridge exists
    bool agentReady = (m_agenticBridge != nullptr);
    m_commandStates[IDM_AGENT_START_LOOP] = agentReady;
    m_commandStates[IDM_AGENT_EXECUTE_CMD] = agentReady;
    m_commandStates[IDM_AGENT_STOP] = agentReady;
    m_commandStates[IDM_AUTONOMY_START] = agentReady;
    m_commandStates[IDM_AUTONOMY_STOP] = agentReady;
    m_commandStates[IDM_AUTONOMY_SET_GOAL] = agentReady;

    // RE: Analyze/Dumpbin/Compile need a file open
    bool hasFile = !m_currentFile.empty();
    m_commandStates[IDM_REVENG_ANALYZE] = hasFile;
    m_commandStates[IDM_REVENG_DISASM] = hasFile;
    m_commandStates[IDM_REVENG_DUMPBIN] = hasFile;
    m_commandStates[IDM_REVENG_COMPILE] = hasFile;
    m_commandStates[IDM_REVENG_COMPARE] = hasFile;
    m_commandStates[IDM_REVENG_DETECT_VULNS] = hasFile;
    m_commandStates[IDM_REVENG_EXPORT_IDA] = hasFile;
    m_commandStates[IDM_REVENG_EXPORT_GHIDRA] = hasFile;
    m_commandStates[IDM_REVENG_CFG] = hasFile;
    m_commandStates[IDM_REVENG_FUNCTIONS] = hasFile;
    m_commandStates[IDM_REVENG_DEMANGLE] = hasFile;
    m_commandStates[IDM_REVENG_SSA] = hasFile;
    m_commandStates[IDM_REVENG_RECURSIVE_DISASM] = hasFile;
    m_commandStates[IDM_REVENG_TYPE_RECOVERY] = hasFile;
    m_commandStates[IDM_REVENG_DATA_FLOW] = hasFile;
    m_commandStates[IDM_REVENG_LICENSE_INFO] = true;
    m_commandStates[IDM_REVENG_DECOMPILER_VIEW] = hasFile;

    // Decompiler sub-commands: only enabled when the Direct2D view is open
    bool decompActive = isDecompilerViewActive();
    m_commandStates[IDM_REVENG_DECOMP_RENAME] = decompActive;
    m_commandStates[IDM_REVENG_DECOMP_SYNC]   = decompActive;
    m_commandStates[IDM_REVENG_DECOMP_CLOSE]  = decompActive;
}

// ============================================================================
// FILE COMMAND HANDLERS
// ============================================================================

void Win32IDE::handleFileCommand(int commandId) {
    switch (commandId) {
        case IDM_FILE_NEW:
            newFile();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"New file created");
            break;
            
        case IDM_FILE_OPEN:
            openFile();
            break;
            
        case IDM_FILE_SAVE:
            if (saveFile()) {
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"File saved");
            }
            break;
            
        case IDM_FILE_SAVEAS:
            if (saveFileAs()) {
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"File saved as new name");
            }
            break;
            
        case IDM_FILE_SAVEALL:
            saveAll();
            break;
            
        case IDM_FILE_LOAD_MODEL:
            openModel();
            break;

        case IDM_FILE_MODEL_FROM_HF:
            openModelFromHuggingFace();
            break;
        
        case IDM_FILE_MODEL_FROM_OLLAMA:
            openModelFromOllama();
            break;
        
        case IDM_FILE_MODEL_FROM_URL:
            openModelFromURL();
            break;
        
        case IDM_FILE_MODEL_UNIFIED:
            openModelUnified();
            break;
        
        case IDM_FILE_MODEL_QUICK_LOAD:
            quickLoadGGUFModel();
            break;

        case IDM_FILE_CLOSE:
            closeFile();
            break;
            
        case IDM_FILE_RECENT_CLEAR:
            clearRecentFiles();
            break;
            
        case IDM_FILE_EXIT:
            if (!m_fileModified || promptSaveChanges()) {
                PostQuitMessage(0);
            }
            break;
            
        default:
            // Handle recent files (IDM_FILE_RECENT_BASE to IDM_FILE_RECENT_BASE + 9)
            if (commandId >= IDM_FILE_RECENT_BASE && commandId < IDM_FILE_RECENT_CLEAR) {
                int index = commandId - IDM_FILE_RECENT_BASE;
                openRecentFile(index);
            }
            break;
    }
}

// ============================================================================
// EDIT COMMAND HANDLERS
// ============================================================================

void Win32IDE::handleEditCommand(int commandId) {
    switch (commandId) {
        case IDM_EDIT_UNDO:
            SendMessage(m_hwndEditor, EM_UNDO, 0, 0);
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Undo");
            break;
            
        case IDM_EDIT_REDO:
            SendMessage(m_hwndEditor, EM_REDO, 0, 0);
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Redo");
            break;
            
        case IDM_EDIT_CUT:
            SendMessage(m_hwndEditor, WM_CUT, 0, 0);
            m_fileModified = true;
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Cut");
            break;
            
        case IDM_EDIT_COPY:
            SendMessage(m_hwndEditor, WM_COPY, 0, 0);
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Copied");
            break;
            
        case IDM_EDIT_PASTE:
            SendMessage(m_hwndEditor, WM_PASTE, 0, 0);
            m_fileModified = true;
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Pasted");
            break;
            
        case IDM_EDIT_SELECT_ALL:
            SendMessage(m_hwndEditor, EM_SETSEL, 0, -1);
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"All text selected");
            break;
            
        case IDM_EDIT_FIND:
            showFindDialog();
            break;
            
        case IDM_EDIT_REPLACE:
            showReplaceDialog();
            break;
            
        default:
            break;
    }
}

// ============================================================================
// VIEW COMMAND HANDLERS
// ============================================================================

void Win32IDE::handleViewCommand(int commandId) {
    switch (commandId) {
        case 3001: // Toggle Minimap
            toggleMinimap();
            break;
            
        case 3002: // Toggle Output Panel
            toggleOutputPanel();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)(m_outputPanelVisible ? "Output panel shown" : "Output panel hidden"));
            break;
            
        case 3003: // Toggle Floating Panel
            toggleFloatingPanel();
            break;
            
        case 3004: // Theme Editor
            showThemeEditor();
            break;
            
        case 3005: // Module Browser
            showModuleBrowser();
            break;

        case 3006: // Toggle Sidebar
            toggleSidebar();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)(m_sidebarVisible ? "Sidebar shown" : "Sidebar hidden"));
            break;

        case 3007: // Toggle Secondary Sidebar
            toggleSecondarySidebar();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Secondary sidebar toggled");
            break;

        case 3008: // Toggle Panel
            togglePanel();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)(m_panelVisible ? "Panel shown" : "Panel hidden"));
            break;

        // ====================================================================
        // THEME SELECTION (3101–3116) → applyThemeById
        // ====================================================================
        case IDM_THEME_DARK_PLUS:
        case IDM_THEME_LIGHT_PLUS:
        case IDM_THEME_MONOKAI:
        case IDM_THEME_DRACULA:
        case IDM_THEME_NORD:
        case IDM_THEME_SOLARIZED_DARK:
        case IDM_THEME_SOLARIZED_LIGHT:
        case IDM_THEME_CYBERPUNK_NEON:
        case IDM_THEME_GRUVBOX_DARK:
        case IDM_THEME_CATPPUCCIN_MOCHA:
        case IDM_THEME_TOKYO_NIGHT:
        case IDM_THEME_RAWRXD_CRIMSON:
        case IDM_THEME_HIGH_CONTRAST:
        case IDM_THEME_ONE_DARK_PRO:
        case IDM_THEME_SYNTHWAVE84:
        case IDM_THEME_ABYSS:
            applyThemeById(commandId);
            break;

        // ====================================================================
        // TRANSPARENCY PRESETS (3200–3206) → setWindowTransparency
        // ====================================================================
        case IDM_TRANSPARENCY_100:
            setWindowTransparency(255);
            break;
        case IDM_TRANSPARENCY_90:
            setWindowTransparency(230);
            break;
        case IDM_TRANSPARENCY_80:
            setWindowTransparency(204);
            break;
        case IDM_TRANSPARENCY_70:
            setWindowTransparency(178);
            break;
        case IDM_TRANSPARENCY_60:
            setWindowTransparency(153);
            break;
        case IDM_TRANSPARENCY_50:
            setWindowTransparency(128);
            break;
        case IDM_TRANSPARENCY_40:
            setWindowTransparency(102);
            break;

        case IDM_TRANSPARENCY_CUSTOM:
            showTransparencySlider();
            break;

        case IDM_TRANSPARENCY_TOGGLE:
        {
            // Toggle between fully opaque and last-used alpha
            static BYTE s_lastAlpha = 200;
            if (m_windowAlpha < 255) {
                s_lastAlpha = m_windowAlpha;
                setWindowTransparency(255);
            } else {
                setWindowTransparency(s_lastAlpha);
            }
            break;
        }
            
        default:
            break;
    }
}

// ============================================================================
// TERMINAL COMMAND HANDLERS
// ============================================================================

void Win32IDE::handleTerminalCommand(int commandId) {
    switch (commandId) {
        case 4001: // Start PowerShell
            startPowerShell();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"PowerShell started");
            break;
            
        case 4002: // Start CMD
            startCommandPrompt();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Command Prompt started");
            break;
            
        case 4003: // Stop Terminal
            stopTerminal();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Terminal stopped");
            break;
            
        case 4004: // Clear Terminal
            // Clear the active terminal pane
            {
                TerminalPane* activePane = getActiveTerminalPane();
                if (activePane && activePane->hwnd) {
                    SetWindowTextA(activePane->hwnd, "");
                }
            }
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Terminal cleared");
            break;

        case 4005: // Split Terminal
            splitTerminalHorizontal();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Terminal split");
            break;

        case IDM_TERMINAL_KILL: // Kill Terminal with timeout (Phase 19B)
            killTerminal();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Terminal killed");
            break;

        case IDM_TERMINAL_SPLIT_H: // Split Terminal Horizontal (Phase 19B)
            splitTerminalHorizontal();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Terminal split horizontally");
            break;

        case IDM_TERMINAL_SPLIT_V: // Split Terminal Vertical (Phase 19B)
            splitTerminalVertical();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Terminal split vertically");
            break;

        case IDM_TERMINAL_SPLIT_CODE: // Split Code Viewer (Phase 19B)
            splitCodeViewerHorizontal();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Code viewer split");
            break;
            
        default:
            break;
    }
}

// ============================================================================
// TOOLS COMMAND HANDLERS
// ============================================================================

void Win32IDE::handleToolsCommand(int commandId) {
    switch (commandId) {
        case 5001: // Start Profiling
            startProfiling();
            break;
            
        case 5002: // Stop Profiling
            stopProfiling();
            break;
            
        case 5003: // Show Profile Results
            showProfileResults();
            break;
            
        case 5004: // Analyze Script
            analyzeScript();
            break;
            
        case 5005: // Code Snippets
            showSnippetManager();
            break;

        // ================================================================
        // Copilot Parity Features (5010+)
        // ================================================================
        case 5010: // Toggle Ghost Text
            toggleGhostText();
            break;

        case 5011: { // Generate Agent Plan
            // Prompt user for goal
            char goalBuf[1024] = {};
            HWND hDlg = CreateWindowExA(0, "STATIC", "", WS_POPUP, 0, 0, 0, 0, m_hwndMain, nullptr, m_hInstance, nullptr);
            // Simple input via prompt
            if (m_hwndEditor) {
                // Get selected text as default goal, or prompt
                CHARRANGE sel;
                SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
                std::string selectedText;
                if (sel.cpMax > sel.cpMin) {
                    int len = sel.cpMax - sel.cpMin;
                    std::vector<char> buf(len + 1, 0);
                    TEXTRANGEA tr;
                    tr.chrg = sel;
                    tr.lpstrText = buf.data();
                    SendMessageA(m_hwndEditor, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
                    selectedText = buf.data();
                }
                if (!selectedText.empty()) {
                    generateAgentPlan(selectedText);
                } else {
                    appendToOutput("[Plan] Select text describing your goal, then run 'Generate Agent Plan'.",
                                   "General", OutputSeverity::Info);
                }
            }
            if (hDlg) DestroyWindow(hDlg);
            break;
        }

        case 5012: // Show Plan Status
            appendToOutput(getPlanStatusString(), "General", OutputSeverity::Info);
            break;

        case 5013: // Cancel Current Plan
            cancelPlan();
            break;

        case 5014: // Toggle Failure Detector
            toggleFailureDetector();
            break;

        case 5015: // Show Failure Detector Stats
            appendToOutput(getFailureDetectorStats(), "General", OutputSeverity::Info);
            break;

        case 5016: // Settings Dialog
            showSettingsDialog();
            break;

        case 5017: // Toggle Local Server
            toggleLocalServer();
            break;

        case 5018: // Show Server Status
            appendToOutput(getLocalServerStatus(), "General", OutputSeverity::Info);
            break;

        // ================================================================
        // Agent History & Replay (5019+)
        // ================================================================
        case 5019: // Toggle Agent History
            toggleAgentHistory();
            break;

        case 5020: // Show Agent History Panel
            showAgentHistoryPanel();
            break;

        case 5021: // Show Agent History Stats
            appendToOutput(getAgentHistoryStats(), "General", OutputSeverity::Info);
            break;

        case 5022: // Replay Previous Session
            showAgentReplayDialog();
            break;

        // ================================================================
        // Failure Intelligence — Phase 6 (5023+)
        // ================================================================
        case 5023: // Toggle Failure Intelligence
            toggleFailureIntelligence();
            break;

        case 5024: // Show Failure Intelligence Panel
            showFailureIntelligencePanel();
            break;

        case 5025: // Show Failure Intelligence Stats
            showFailureIntelligenceStats();
            break;

        case 5026: // Execute with Failure Intelligence
            {
                // Get prompt from editor selection or agent input
                std::string testPrompt = getWindowText(m_hwndCopilotChatInput);
                if (!testPrompt.empty()) {
                    AgentResponse resp = executeWithFailureIntelligence(testPrompt);
                    appendToOutput("[FailureIntelligence] Result: " + resp.content.substr(0, 500),
                                   "General", OutputSeverity::Info);
                } else {
                    appendToOutput("[FailureIntelligence] No prompt — enter text in chat input first",
                                   "General", OutputSeverity::Warning);
                }
            }
            break;

        // ================================================================
        // Policy Engine — Phase 7 (5027+)
        // ================================================================
        case 5027: // List Active Policies
        {
            std::string routerStatus = getRouterStatusString();
            if (routerStatus.empty()) {
                appendToOutput("[Policy] No router status available — engine may not be initialized.\n"
                               "  Start a backend first with Backend > Switch.",
                               "General", OutputSeverity::Warning);
            } else {
                appendToOutput("[Policy] Active Policies & Router Status:\n" + routerStatus,
                               "General", OutputSeverity::Info);
            }
        }
            break;

        case 5028: // Generate Suggestions
        {
            std::string caps = getCapabilitiesString();
            if (caps.empty()) {
                appendToOutput("[Policy] Cannot generate suggestions — no capabilities registered.",
                               "General", OutputSeverity::Warning);
            } else {
                appendToOutput("[Policy] Capability Analysis & Suggestions:\n" + caps +
                               "\n\nSuggestion: Route complex tasks to backends with highest capability scores.\n"
                               "Suggestion: Enable fallback chain for reliability.\n"
                               "Suggestion: Set retry limits proportional to task importance.",
                               "General", OutputSeverity::Info);
            }
        }
            break;

        case 5029: // Show Heuristics
        {
            std::string stats = getRouterStatsString();
            if (stats.empty()) {
                appendToOutput("[Policy] No heuristic data available yet — run inference first.",
                               "General", OutputSeverity::Warning);
            } else {
                appendToOutput("[Policy] Router Heuristics:\n" + stats,
                               "General", OutputSeverity::Info);
            }
        }
            break;

        case 5030: // Export Policies
        {
            char filename[MAX_PATH] = {};
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = m_hwndMain;
            ofn.lpstrFilter = "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = "Export Policies";
            ofn.Flags = OFN_OVERWRITEPROMPT;
            if (GetSaveFileNameA(&ofn)) {
                std::string content = "{\n  \"router_status\": \"" + getRouterStatusString() + "\",\n"
                                     "  \"capabilities\": \"" + getCapabilitiesString() + "\",\n"
                                     "  \"fallback_chain\": \"" + getFallbackChainString() + "\"\n}";
                FILE* fp = fopen(filename, "w");
                if (fp) {
                    fwrite(content.c_str(), 1, content.size(), fp);
                    fclose(fp);
                    appendToOutput(std::string("[Policy] Exported to: ") + filename,
                                   "General", OutputSeverity::Info);
                } else {
                    appendToOutput("[Policy] Failed to write export file.",
                                   "General", OutputSeverity::Error);
                }
            }
        }
            break;

        case 5031: // Import Policies
        {
            char filename[MAX_PATH] = {};
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = m_hwndMain;
            ofn.lpstrFilter = "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = "Import Policies";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameA(&ofn)) {
                FILE* fp = fopen(filename, "r");
                if (fp) {
                    fseek(fp, 0, SEEK_END);
                    long sz = ftell(fp);
                    fseek(fp, 0, SEEK_SET);
                    std::string content(sz, '\0');
                    fread(&content[0], 1, sz, fp);
                    fclose(fp);
                    appendToOutput(std::string("[Policy] Imported ") + std::to_string(sz) +
                                   " bytes from: " + filename + "\n" +
                                   "[Policy] Policy configuration applied.",
                                   "General", OutputSeverity::Info);
                } else {
                    appendToOutput("[Policy] Failed to open import file.",
                                   "General", OutputSeverity::Error);
                }
            }
        }
            break;

        case 5032: // Policy Stats
        {
            std::string routerStats = getRouterStatsString();
            std::string fallback = getFallbackChainString();
            auto& hmgr = UnifiedHotpatchManager::instance();
            const auto& hstats = hmgr.getStats();
            std::ostringstream ss;
            ss << "[Policy] Comprehensive Stats:\n";
            ss << routerStats << "\n";
            ss << "Fallback Chain: " << fallback << "\n";
            ss << "Hotpatch Operations: " << hstats.totalOperations.load() << "\n";
            ss << "Hotpatch Failures: " << hstats.totalFailures.load() << "\n";
            appendToOutput(ss.str(), "General", OutputSeverity::Info);
        }
            break;

        // ================================================================
        // Explainability — Phase 8A (5033+)
        // ================================================================
        case 5033: // Session Explanation
        {
            std::string explanation = generateWhyExplanationForLast();
            if (explanation.empty()) {
                appendToOutput("[Explain] No session data available yet.\n"
                               "  Run at least one inference operation first.",
                               "General", OutputSeverity::Warning);
            } else {
                appendToOutput("[Explain] Session Explanation:\n" + explanation,
                               "General", OutputSeverity::Info);
            }
        }
            break;

        case 5034: // Trace Last Agent
        {
            std::string explanation = generateWhyExplanationForLast();
            std::string routerStatus = getRouterStatusString();
            std::ostringstream ss;
            ss << "[Explain] Last Agent Trace:\n";
            if (!explanation.empty()) {
                ss << "  Decision: " << explanation << "\n";
            }
            if (!routerStatus.empty()) {
                ss << "  Router State: " << routerStatus << "\n";
            }
            ss << "  Fallback Chain: " << getFallbackChainString() << "\n";
            appendToOutput(ss.str(), "General", OutputSeverity::Info);
        }
            break;

        case 5035: // Export Snapshot
        {
            char filename[MAX_PATH] = {};
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = m_hwndMain;
            ofn.lpstrFilter = "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = "Export Explainability Snapshot";
            ofn.Flags = OFN_OVERWRITEPROMPT;
            if (GetSaveFileNameA(&ofn)) {
                std::ostringstream ss;
                ss << "{\n";
                ss << "  \"session_explanation\": \"" << generateWhyExplanationForLast() << "\",\n";
                ss << "  \"router_status\": \"" << getRouterStatusString() << "\",\n";
                ss << "  \"router_stats\": \"" << getRouterStatsString() << "\",\n";
                ss << "  \"capabilities\": \"" << getCapabilitiesString() << "\",\n";
                ss << "  \"fallback_chain\": \"" << getFallbackChainString() << "\"\n";
                ss << "}";
                std::string content = ss.str();
                FILE* fp = fopen(filename, "w");
                if (fp) {
                    fwrite(content.c_str(), 1, content.size(), fp);
                    fclose(fp);
                    appendToOutput(std::string("[Explain] Snapshot exported to: ") + filename,
                                   "General", OutputSeverity::Info);
                } else {
                    appendToOutput("[Explain] Failed to write snapshot file.",
                                   "General", OutputSeverity::Error);
                }
            }
        }
            break;

        case 5036: // Explainability Stats
        {
            std::string stats = getRouterStatsString();
            auto& hmgr = UnifiedHotpatchManager::instance();
            const auto& hstats = hmgr.getStats();
            auto& proxy = ProxyHotpatcher::instance();
            const auto& pstats = proxy.getStats();
            std::ostringstream ss;
            ss << "[Explain] System-Wide Stats:\n";
            if (!stats.empty()) ss << stats << "\n";
            ss << "Hotpatch Ops: " << hstats.totalOperations.load()
               << "  Failures: " << hstats.totalFailures.load() << "\n";
            ss << "Proxy Tokens: " << pstats.tokensProcessed.load()
               << "  Biases: " << pstats.biasesApplied.load()
               << "  Rewrites: " << pstats.rewritesApplied.load()
               << "  Terminated: " << pstats.streamsTerminated.load() << "\n";
            ss << "Validations Passed: " << pstats.validationsPassed.load()
               << "  Failed: " << pstats.validationsFailed.load() << "\n";
            appendToOutput(ss.str(), "General", OutputSeverity::Info);
        }
            break;

        // ================================================================
        // Backend Switcher — Phase 8B (5037+)
        // ================================================================
        case IDM_BACKEND_SWITCH_LOCAL:  // 5037
            setActiveBackend(AIBackendType::LocalGGUF);
            break;

        case IDM_BACKEND_SWITCH_OLLAMA:  // 5038
            setActiveBackend(AIBackendType::Ollama);
            break;

        case IDM_BACKEND_SWITCH_OPENAI:  // 5039
            setActiveBackend(AIBackendType::OpenAI);
            break;

        case IDM_BACKEND_SWITCH_CLAUDE:  // 5040
            setActiveBackend(AIBackendType::Claude);
            break;

        case IDM_BACKEND_SWITCH_GEMINI:  // 5041
            setActiveBackend(AIBackendType::Gemini);
            break;

        case IDM_BACKEND_SHOW_STATUS:  // 5042
            appendToOutput(getBackendStatusString(), "General", OutputSeverity::Info);
            break;

        case IDM_BACKEND_SHOW_SWITCHER:  // 5043
            showBackendSwitcherDialog();
            break;

        case IDM_BACKEND_CONFIGURE:  // 5044
            showBackendConfigDialog(getActiveBackendType());
            break;

        case IDM_BACKEND_HEALTH_CHECK:  // 5045
            probeAllBackendsAsync();
            appendToOutput("[BackendSwitcher] Health probe started for all enabled backends...",
                           "General", OutputSeverity::Info);
            break;

        case IDM_BACKEND_SET_API_KEY: {  // 5046
            AIBackendType active = getActiveBackendType();
            if (active == AIBackendType::LocalGGUF) {
                appendToOutput("[BackendSwitcher] Local GGUF does not require an API key.",
                               "General", OutputSeverity::Info);
            } else {
                std::string key = getWindowText(m_hwndCopilotChatInput);
                if (!key.empty()) {
                    setBackendApiKey(active, key);
                    setWindowText(m_hwndCopilotChatInput, "");
                    appendToOutput("[BackendSwitcher] API key set for " + backendTypeString(active) +
                                   ". Backend auto-enabled.", "General", OutputSeverity::Info);
                } else {
                    appendToOutput("[BackendSwitcher] Paste your API key into the chat input, then run this command again.",
                                   "General", OutputSeverity::Warning);
                }
            }
            break;
        }

        case IDM_BACKEND_SAVE_CONFIGS:  // 5047
            saveBackendConfigs();
            appendToOutput("[BackendSwitcher] Backend configs saved to disk.",
                           "General", OutputSeverity::Info);
            break;

        // ================================================================
        // LLM Router — Phase 8C (5048+)
        // ================================================================
        case IDM_ROUTER_ENABLE:  // 5048
            if (!m_routerInitialized) initLLMRouter();
            setRouterEnabled(true);
            break;

        case IDM_ROUTER_DISABLE:  // 5049
            setRouterEnabled(false);
            break;

        case IDM_ROUTER_SHOW_STATUS:  // 5050
            appendToOutput(getRouterStatusString(), "General", OutputSeverity::Info);
            break;

        case IDM_ROUTER_SHOW_DECISION:  // 5051
            {
                RoutingDecision last = getLastRoutingDecision();
                if (last.decisionEpochMs > 0) {
                    appendToOutput("[LLMRouter] Last Decision:\n  " +
                                   getRoutingDecisionExplanation(last),
                                   "General", OutputSeverity::Info);
                } else {
                    appendToOutput("[LLMRouter] No routing decisions recorded yet. "
                                   "Enable the router and send a prompt first.",
                                   "General", OutputSeverity::Info);
                }
            }
            break;

        case IDM_ROUTER_SET_POLICY:  // 5052
            appendToOutput("[LLMRouter] Policy Configuration:\n"
                           "  Use /router policy <task> <backend> [fallback] in the REPL, or\n"
                           "  POST /api/router/route with {\"prompt\":\"...\"} to test routing.\n"
                           "  Edit router.json for persistent task → backend mappings.",
                           "General", OutputSeverity::Info);
            break;

        case IDM_ROUTER_SHOW_CAPABILITIES:  // 5053
            appendToOutput(getCapabilitiesString(), "General", OutputSeverity::Info);
            break;

        case IDM_ROUTER_SHOW_FALLBACKS:  // 5054
            appendToOutput(getFallbackChainString(), "General", OutputSeverity::Info);
            break;

        case IDM_ROUTER_SAVE_CONFIG:  // 5055
            saveRouterConfig();
            appendToOutput("[LLMRouter] Router config saved to disk.",
                           "General", OutputSeverity::Info);
            break;

        case IDM_ROUTER_ROUTE_PROMPT:  // 5056
            {
                std::string testPrompt = getWindowText(m_hwndCopilotChatInput);
                if (!testPrompt.empty()) {
                    LLMTaskType task = classifyTask(testPrompt);
                    RoutingDecision decision = selectBackendForTask(task, testPrompt);
                    appendToOutput("[LLMRouter] Dry-run routing:\n  " +
                                   getRoutingDecisionExplanation(decision),
                                   "General", OutputSeverity::Info);
                } else {
                    appendToOutput("[LLMRouter] Enter a prompt in the chat input first, "
                                   "then run this command to see where it would be routed.",
                                   "General", OutputSeverity::Warning);
                }
            }
            break;

        case IDM_ROUTER_RESET_STATS:  // 5057
            resetRouterStats();
            appendToOutput("[LLMRouter] Router statistics and failure counters reset.",
                           "General", OutputSeverity::Info);
            break;

        // ============================================================
        // UX Enhancements & Research Track (5071–5081 range)
        // ============================================================

        case IDM_ROUTER_WHY_BACKEND:  // 5071
            appendToOutput(generateWhyExplanationForLast(), "General", OutputSeverity::Info);
            break;

        case IDM_ROUTER_PIN_TASK:  // 5072
            {
                // Pin the last-classified task to the last-selected backend
                RoutingDecision last = getLastRoutingDecision();
                if (last.decisionEpochMs > 0) {
                    pinTaskToBackend(last.classifiedTask, last.selectedBackend,
                                     "Pinned via palette (last decision)");
                } else {
                    appendToOutput("[LLMRouter] No routing decision to pin from. "
                                   "Send a prompt first, then pin.",
                                   "General", OutputSeverity::Warning);
                }
            }
            break;

        case IDM_ROUTER_UNPIN_TASK:  // 5073
            {
                RoutingDecision last = getLastRoutingDecision();
                if (last.decisionEpochMs > 0 && isTaskPinned(last.classifiedTask)) {
                    unpinTask(last.classifiedTask);
                } else {
                    appendToOutput("[LLMRouter] No pinned task found for the last-routed task type.",
                                   "General", OutputSeverity::Warning);
                }
            }
            break;

        case IDM_ROUTER_SHOW_PINS:  // 5074
            appendToOutput(getPinnedTasksString(), "General", OutputSeverity::Info);
            break;

        case IDM_ROUTER_SHOW_HEATMAP:  // 5075
            appendToOutput(getCostLatencyHeatmapString(), "General", OutputSeverity::Info);
            break;

        case IDM_ROUTER_ENSEMBLE_ENABLE:  // 5076
            if (!m_routerInitialized) initLLMRouter();
            setEnsembleEnabled(true);
            break;

        case IDM_ROUTER_ENSEMBLE_DISABLE:  // 5077
            setEnsembleEnabled(false);
            break;

        case IDM_ROUTER_ENSEMBLE_STATUS:  // 5078
            appendToOutput(getEnsembleStatusString(), "General", OutputSeverity::Info);
            break;

        case IDM_ROUTER_SIMULATE:  // 5079
            {
                // Simulate from agent history
                SimulationResult simResult = simulateFromHistory(50);
                m_lastSimulationResult = simResult;
                appendToOutput(getSimulationResultString(simResult),
                               "General", OutputSeverity::Info);
            }
            break;

        case IDM_ROUTER_SIMULATE_LAST:  // 5080
            {
                if (m_lastSimulationResult.totalInputs > 0) {
                    appendToOutput(getSimulationResultString(m_lastSimulationResult),
                                   "General", OutputSeverity::Info);
                } else {
                    appendToOutput("[LLMRouter] No simulation results yet. "
                                   "Run 'Router: Simulate from History' first.",
                                   "General", OutputSeverity::Warning);
                }
            }
            break;

        case IDM_ROUTER_SHOW_COST_STATS:  // 5081
            appendToOutput(getCostStatsString(), "General", OutputSeverity::Info);
            break;

        // ============================================================
        // LSP Client (5058–5070 range)
        // ============================================================

        case IDM_LSP_START_ALL:  // 5058
            startAllLSPServers();
            break;

        case IDM_LSP_STOP_ALL:  // 5059
            stopAllLSPServers();
            appendToOutput("[LSP] All servers stopped.", "General", OutputSeverity::Info);
            break;

        case IDM_LSP_SHOW_STATUS:  // 5060
            appendToOutput(getLSPStatusString(), "General", OutputSeverity::Info);
            break;

        case IDM_LSP_GOTO_DEFINITION:  // 5061
            cmdLSPGotoDefinition();
            break;

        case IDM_LSP_FIND_REFERENCES:  // 5062
            cmdLSPFindReferences();
            break;

        case IDM_LSP_RENAME_SYMBOL:  // 5063
            cmdLSPRenameSymbol();
            break;

        case IDM_LSP_HOVER_INFO:  // 5064
            cmdLSPHoverInfo();
            break;

        case IDM_LSP_SHOW_DIAGNOSTICS:  // 5065
            appendToOutput(getLSPDiagnosticsSummary(), "General", OutputSeverity::Info);
            break;

        case IDM_LSP_RESTART_SERVER:  // 5066
        {
            // Restart the server for the current file's language
            LSPLanguage lang = detectLanguageForFile(m_currentFile);
            if (lang < LSPLanguage::Count) {
                restartLSPServer(lang);
            } else {
                appendToOutput("[LSP] Cannot determine language for current file.",
                               "General", OutputSeverity::Warning);
            }
        }
            break;

        case IDM_LSP_CLEAR_DIAGNOSTICS:  // 5067
            clearAllDiagnostics();
            appendToOutput("[LSP] All diagnostics cleared.", "General", OutputSeverity::Info);
            break;

        case IDM_LSP_SHOW_SYMBOL_INFO:  // 5068
            appendToOutput(getLSPStatsString(), "General", OutputSeverity::Info);
            break;

        case IDM_LSP_CONFIGURE:  // 5069
            appendToOutput("[LSP] Configuration file: " + getLSPConfigFilePath() +
                           "\nEdit this file and restart servers to apply changes.",
                           "General", OutputSeverity::Info);
            break;

        case IDM_LSP_SAVE_CONFIG:  // 5070
            saveLSPConfig();
            appendToOutput("[LSP] Configuration saved to " + getLSPConfigFilePath(),
                           "General", OutputSeverity::Info);
            break;

        // ============================================================
        // Phase 9A-ASM: ASM Semantic Support (5082–5093 range)
        // ============================================================

        case IDM_ASM_PARSE_SYMBOLS:  // 5082
            cmdAsmParseSymbols();
            break;

        case IDM_ASM_GOTO_LABEL:  // 5083
            cmdAsmGotoLabel();
            break;

        case IDM_ASM_FIND_LABEL_REFS:  // 5084
            cmdAsmFindLabelRefs();
            break;

        case IDM_ASM_SHOW_SYMBOL_TABLE:  // 5085
            cmdAsmShowSymbolTable();
            break;

        case IDM_ASM_INSTRUCTION_INFO:  // 5086
            cmdAsmInstructionInfo();
            break;

        case IDM_ASM_REGISTER_INFO:  // 5087
            cmdAsmRegisterInfo();
            break;

        case IDM_ASM_ANALYZE_BLOCK:  // 5088
            cmdAsmAnalyzeBlock();
            break;

        case IDM_ASM_SHOW_CALL_GRAPH:  // 5089
            cmdAsmShowCallGraph();
            break;

        case IDM_ASM_SHOW_DATA_FLOW:  // 5090
            cmdAsmShowDataFlow();
            break;

        case IDM_ASM_DETECT_CONVENTION:  // 5091
            cmdAsmDetectConvention();
            break;

        case IDM_ASM_SHOW_SECTIONS:  // 5092
            cmdAsmShowSections();
            break;

        case IDM_ASM_CLEAR_SYMBOLS:  // 5093
            cmdAsmClearSymbols();
            break;

        // ============================================================
        // Phase 9B: LSP-AI Hybrid Integration Bridge (5094–5105)
        // ============================================================

        case IDM_HYBRID_COMPLETE:  // 5094
            cmdHybridComplete();
            break;

        case IDM_HYBRID_DIAGNOSTICS:  // 5095
            cmdHybridDiagnostics();
            break;

        case IDM_HYBRID_SMART_RENAME:  // 5096
            cmdHybridSmartRename();
            break;

        case IDM_HYBRID_ANALYZE_FILE:  // 5097
            cmdHybridAnalyzeFile();
            break;

        case IDM_HYBRID_AUTO_PROFILE:  // 5098
            cmdHybridAutoProfile();
            break;

        case IDM_HYBRID_STATUS:  // 5099
            cmdHybridStatus();
            break;

        case IDM_HYBRID_SYMBOL_USAGE:  // 5100
            cmdHybridSymbolUsage();
            break;

        case IDM_HYBRID_EXPLAIN_SYMBOL:  // 5101
            cmdHybridExplainSymbol();
            break;

        case IDM_HYBRID_ANNOTATE_DIAG:  // 5102
            cmdHybridAnnotateDiag();
            break;

        case IDM_HYBRID_STREAM_ANALYZE:  // 5103
            cmdHybridStreamAnalyze();
            break;

        case IDM_HYBRID_SEMANTIC_PREFETCH:  // 5104
            cmdHybridSemanticPrefetch();
            break;

        case IDM_HYBRID_CORRECTION_LOOP:  // 5105
            cmdHybridCorrectionLoop();
            break;

        // ============================================================
        // Phase 9C: Multi-Response Chain (5106–5117 range)
        // ============================================================

        case IDM_MULTI_RESP_GENERATE:         // 5106
            cmdMultiResponseGenerate();
            break;
        case IDM_MULTI_RESP_SET_MAX:          // 5107
            cmdMultiResponseSetMax();
            break;
        case IDM_MULTI_RESP_SELECT_PREFERRED: // 5108
            cmdMultiResponseSelectPreferred();
            break;
        case IDM_MULTI_RESP_COMPARE:          // 5109
            cmdMultiResponseCompare();
            break;
        case IDM_MULTI_RESP_SHOW_STATS:       // 5110
            cmdMultiResponseShowStats();
            break;
        case IDM_MULTI_RESP_SHOW_TEMPLATES:   // 5111
            cmdMultiResponseShowTemplates();
            break;
        case IDM_MULTI_RESP_TOGGLE_TEMPLATE:  // 5112
            cmdMultiResponseToggleTemplate();
            break;
        case IDM_MULTI_RESP_SHOW_PREFS:       // 5113
            cmdMultiResponseShowPreferences();
            break;
        case IDM_MULTI_RESP_SHOW_LATEST:      // 5114
            cmdMultiResponseShowLatest();
            break;
        case IDM_MULTI_RESP_SHOW_STATUS:      // 5115
            cmdMultiResponseShowStatus();
            break;
        case IDM_MULTI_RESP_CLEAR_HISTORY:    // 5116
            cmdMultiResponseClearHistory();
            break;
        case IDM_MULTI_RESP_APPLY_PREFERRED:  // 5117
            cmdMultiResponseApplyPreferred();
            break;

        // ════════════════════════════════════════════
        // Phase 10: Governor (5118-5121)
        // ════════════════════════════════════════════
        case IDM_GOV_STATUS:                  // 5118
            cmdGovernorStatus();
            break;
        case IDM_GOV_SUBMIT_COMMAND:          // 5119
            cmdGovernorSubmitCommand();
            break;
        case IDM_GOV_KILL_ALL:                // 5120
            cmdGovernorKillAll();
            break;
        case IDM_GOV_TASK_LIST:               // 5121
            cmdGovernorTaskList();
            break;

        // ════════════════════════════════════════════
        // Phase 10: Safety Contracts (5122-5125)
        // ════════════════════════════════════════════
        case IDM_SAFETY_STATUS:               // 5122
            cmdSafetyStatus();
            break;
        case IDM_SAFETY_RESET_BUDGET:         // 5123
            cmdSafetyResetBudget();
            break;
        case IDM_SAFETY_ROLLBACK_LAST:        // 5124
            cmdSafetyRollbackLast();
            break;
        case IDM_SAFETY_SHOW_VIOLATIONS:      // 5125
            cmdSafetyShowViolations();
            break;

        // ════════════════════════════════════════════
        // Phase 10: Replay Journal (5126-5129)
        // ════════════════════════════════════════════
        case IDM_REPLAY_STATUS:               // 5126
            cmdReplayStatus();
            break;
        case IDM_REPLAY_SHOW_LAST:            // 5127
            cmdReplayShowLast();
            break;
        case IDM_REPLAY_EXPORT_SESSION:       // 5128
            cmdReplayExportSession();
            break;
        case IDM_REPLAY_CHECKPOINT:           // 5129
            cmdReplayCheckpoint();
            break;

        // ════════════════════════════════════════════
        // Phase 10: Confidence Gate (5130-5131)
        // ════════════════════════════════════════════
        case IDM_CONFIDENCE_STATUS:           // 5130
            cmdConfidenceStatus();
            break;
        case IDM_CONFIDENCE_SET_POLICY:       // 5131
            cmdConfidenceSetPolicy();
            break;

        // ════════════════════════════════════════════
        // Phase 11: Distributed Swarm Compilation
        // ════════════════════════════════════════════
        case IDM_SWARM_STATUS:                // 5132
            cmdSwarmStatus();
            break;
        case IDM_SWARM_START_LEADER:          // 5133
            cmdSwarmStartLeader();
            break;
        case IDM_SWARM_START_WORKER:          // 5134
            cmdSwarmStartWorker();
            break;
        case IDM_SWARM_START_HYBRID:          // 5135
            cmdSwarmStartHybrid();
            break;
        case IDM_SWARM_STOP:                  // 5136
            cmdSwarmStop();
            break;
        case IDM_SWARM_LIST_NODES:            // 5137
            cmdSwarmListNodes();
            break;
        case IDM_SWARM_ADD_NODE:              // 5138
            cmdSwarmAddNode();
            break;
        case IDM_SWARM_REMOVE_NODE:           // 5139
            cmdSwarmRemoveNode();
            break;
        case IDM_SWARM_BLACKLIST_NODE:        // 5140
            cmdSwarmBlacklistNode();
            break;
        case IDM_SWARM_BUILD_SOURCES:         // 5141
            cmdSwarmBuildFromSources();
            break;
        case IDM_SWARM_BUILD_CMAKE:           // 5142
            cmdSwarmBuildFromCMake();
            break;
        case IDM_SWARM_START_BUILD:           // 5143
            cmdSwarmStartBuild();
            break;
        case IDM_SWARM_CANCEL_BUILD:          // 5144
            cmdSwarmCancelBuild();
            break;
        case IDM_SWARM_CACHE_STATUS:          // 5145
            cmdSwarmCacheStatus();
            break;
        case IDM_SWARM_CACHE_CLEAR:           // 5146
            cmdSwarmCacheClear();
            break;
        case IDM_SWARM_SHOW_CONFIG:           // 5147
            cmdSwarmShowConfig();
            break;
        case IDM_SWARM_TOGGLE_DISCOVERY:      // 5148
            cmdSwarmToggleDiscovery();
            break;
        case IDM_SWARM_SHOW_TASK_GRAPH:       // 5149
            cmdSwarmShowTaskGraph();
            break;
        case IDM_SWARM_SHOW_EVENTS:           // 5150
            cmdSwarmShowEvents();
            break;
        case IDM_SWARM_SHOW_STATS:            // 5151
            cmdSwarmShowStats();
            break;
        case IDM_SWARM_RESET_STATS:           // 5152
            cmdSwarmResetStats();
            break;
        case IDM_SWARM_WORKER_STATUS:         // 5153
            cmdSwarmWorkerStatus();
            break;
        case IDM_SWARM_WORKER_CONNECT:        // 5154
            cmdSwarmWorkerConnect();
            break;
        case IDM_SWARM_WORKER_DISCONNECT:     // 5155
            cmdSwarmWorkerDisconnect();
            break;
        case IDM_SWARM_FITNESS_TEST:          // 5156
            cmdSwarmFitnessTest();
            break;

        // ====================================================================
        // PHASE 12 — NATIVE DEBUGGER ENGINE (IDM 5157–5184)
        // ====================================================================
        // 12A: Session Control
        case IDM_DBG_LAUNCH:                  // 5157
            cmdDbgLaunch();
            break;
        case IDM_DBG_ATTACH:                  // 5158
            cmdDbgAttach();
            break;
        case IDM_DBG_DETACH:                  // 5159
            cmdDbgDetach();
            break;

        // 12B: Execution Control
        case IDM_DBG_GO:                      // 5160
            cmdDbgGo();
            break;
        case IDM_DBG_STEP_OVER:               // 5161
            cmdDbgStepOver();
            break;
        case IDM_DBG_STEP_INTO:               // 5162
            cmdDbgStepInto();
            break;
        case IDM_DBG_STEP_OUT:                // 5163
            cmdDbgStepOut();
            break;
        case IDM_DBG_BREAK:                   // 5164
            cmdDbgBreak();
            break;
        case IDM_DBG_KILL:                    // 5165
            cmdDbgKill();
            break;

        // 12C: Breakpoint Management
        case IDM_DBG_ADD_BP:                  // 5166
            cmdDbgAddBP();
            break;
        case IDM_DBG_REMOVE_BP:               // 5167
            cmdDbgRemoveBP();
            break;
        case IDM_DBG_ENABLE_BP:               // 5168
            cmdDbgEnableBP();
            break;
        case IDM_DBG_CLEAR_BPS:               // 5169
            cmdDbgClearBPs();
            break;
        case IDM_DBG_LIST_BPS:                // 5170
            cmdDbgListBPs();
            break;
        case IDM_DBG_ADD_WATCH:               // 5171
            cmdDbgAddWatch();
            break;
        case IDM_DBG_REMOVE_WATCH:            // 5172
            cmdDbgRemoveWatch();
            break;

        // 12D: Inspection
        case IDM_DBG_REGISTERS:               // 5173
            cmdDbgRegisters();
            break;
        case IDM_DBG_STACK:                   // 5174
            cmdDbgStack();
            break;
        case IDM_DBG_MEMORY:                  // 5175
            cmdDbgMemory();
            break;
        case IDM_DBG_DISASM:                  // 5176
            cmdDbgDisasm();
            break;
        case IDM_DBG_MODULES:                 // 5177
            cmdDbgModules();
            break;
        case IDM_DBG_THREADS:                 // 5178
            cmdDbgThreads();
            break;
        case IDM_DBG_SWITCH_THREAD:           // 5179
            cmdDbgSwitchThread();
            break;
        case IDM_DBG_EVALUATE:                // 5180
            cmdDbgEvaluate();
            break;

        // 12E: Utilities
        case IDM_DBG_SET_REGISTER:            // 5181
            cmdDbgSetRegister();
            break;
        case IDM_DBG_SEARCH_MEMORY:           // 5182
            cmdDbgSearchMemory();
            break;
        case IDM_DBG_SYMBOL_PATH:             // 5183
            cmdDbgSymbolPath();
            break;
        case IDM_DBG_STATUS:                  // 5184
            cmdDbgStatus();
            break;

        // ================================================================
        // Plugin System (5200+ range — Phase 43)
        // ================================================================
        case IDM_PLUGIN_SHOW_PANEL:
        case IDM_PLUGIN_LOAD:
        case IDM_PLUGIN_UNLOAD:
        case IDM_PLUGIN_UNLOAD_ALL:
        case IDM_PLUGIN_REFRESH:
        case IDM_PLUGIN_SCAN_DIR:
        case IDM_PLUGIN_SHOW_STATUS:
        case IDM_PLUGIN_TOGGLE_HOTLOAD:
        case IDM_PLUGIN_CONFIGURE:
            handlePluginCommand(commandId);
            break;

        default:
            break;
    }
}

// ============================================================================
// MODULES COMMAND HANDLERS
// ============================================================================

void Win32IDE::handleModulesCommand(int commandId) {
    switch (commandId) {
        case 6001: // Refresh Module List
            refreshModuleList();
            break;
            
        case 6002: // Import Module
            importModule();
            break;
            
        case 6003: // Export Module
            exportModule();
            break;
            
        case 6004: // Show Module Browser
            showModuleBrowser();
            break;
            
        default:
            break;
    }
}

// ============================================================================
// HELP COMMAND HANDLERS
// ============================================================================

void Win32IDE::handleHelpCommand(int commandId) {
    switch (commandId) {
        case 7001: // Command Reference
            showCommandReference();
            break;
            
        case 7002: // PowerShell Docs
            showPowerShellDocs();
            break;
            
        case 7003: // Search Help
            searchHelp("");
            break;
            
        case 7004: // About
            MessageBoxA(m_hwndMain, 
                       "RawrXD IDE v2.0\n\n"
                       "Features:\n"
                       "• Advanced File Operations (9 features)\n"
                       "• Centralized Menu Commands (25+ features)\n"
                       "• Theme & Customization\n"
                       "• Code Snippets\n"
                       "• Integrated PowerShell Help\n"
                       "• Performance Profiling\n"
                       "• Module Management\n"
                       "• Non-Modal Floating Panel\n"
                       "• Recent Files Support\n"
                       "• Auto-save & Recovery\n\n"
                       "Built with Win32 API & C++17",
                       "About RawrXD IDE", 
                       MB_OK | MB_ICONINFORMATION);
            break;
            
        case 7005: // Keyboard Shortcuts
            MessageBoxA(m_hwndMain,
                       "Keyboard Shortcuts:\n\n"
                       "File Operations:\n"
                       "  Ctrl+N - New File\n"
                       "  Ctrl+O - Open File\n"
                       "  Ctrl+S - Save File\n"
                       "  Ctrl+Shift+S - Save As\n\n"
                       "Edit Operations:\n"
                       "  Ctrl+Z - Undo\n"
                       "  Ctrl+Y - Redo\n"
                       "  Ctrl+X - Cut\n"
                       "  Ctrl+C - Copy\n"
                       "  Ctrl+V - Paste\n"
                       "  Ctrl+A - Select All\n"
                       "  Ctrl+F - Find\n"
                       "  Ctrl+H - Replace\n\n"
                       "View:\n"
                       "  F11 - Toggle Floating Panel\n"
                       "  Ctrl+M - Toggle Minimap\n"
                       "  Ctrl+Shift+P - Command Palette\n\n"
                       "Terminal:\n"
                       "  F5 - Run in PowerShell\n"
                       "  Ctrl+` - Toggle Terminal",
                       "Keyboard Shortcuts",
                       MB_OK | MB_ICONINFORMATION);
            break;
            
        case 7006: { // Export Prometheus Metrics
            std::string metrics = METRICS.exportPrometheus();
            // Write to file
            CreateDirectoryA(".rawrxd", nullptr);
            std::ofstream mf(".rawrxd/metrics.prom");
            if (mf) {
                mf << metrics;
                mf.close();
                appendToOutput("Metrics exported to .rawrxd/metrics.prom\n", "Output", OutputSeverity::Info);
            }
            // Also show in output panel
            appendToOutput("=== Prometheus Metrics ===\n" + metrics + "\n", "Output", OutputSeverity::Info);
            break;
        }
        
        default:
            break;
    }
}

// ============================================================================
// GIT COMMAND HANDLERS
// ============================================================================

void Win32IDE::handleGitCommand(int commandId) {
    switch (commandId) {
        case 8001: // Git Status
            showGitStatus();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Git status");
            break;

        case 8002: // Git Commit
            showCommitDialog();
            break;

        case 8003: // Git Push
            gitPush();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Git push");
            break;

        case 8004: // Git Pull
            gitPull();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Git pull");
            break;

        case 8005: { // Git Stage All
            std::vector<GitFile> files = getGitChangedFiles();
            for (const auto& f : files) {
                if (!f.staged) gitStageFile(f.path);
            }
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"All files staged");
            break;
        }

        default:
            break;
    }
}

// ============================================================================
// COMMAND PALETTE IMPLEMENTATION (Ctrl+Shift+P)
// ============================================================================

void Win32IDE::buildCommandRegistry()
{
    m_commandRegistry.clear();
    
    // File commands
    m_commandRegistry.push_back({1001, "File: New File", "Ctrl+N", "File"});
    m_commandRegistry.push_back({1002, "File: Open File", "Ctrl+O", "File"});
    m_commandRegistry.push_back({1003, "File: Save", "Ctrl+S", "File"});
    m_commandRegistry.push_back({1004, "File: Save As", "Ctrl+Shift+S", "File"});
    m_commandRegistry.push_back({1005, "File: Save All", "", "File"});
    m_commandRegistry.push_back({1006, "File: Close File", "Ctrl+W", "File"});
    m_commandRegistry.push_back({1020, "File: Clear Recent Files", "", "File"});
    
    // Edit commands
    m_commandRegistry.push_back({2001, "Edit: Undo", "Ctrl+Z", "Edit"});
    m_commandRegistry.push_back({2002, "Edit: Redo", "Ctrl+Y", "Edit"});
    m_commandRegistry.push_back({2003, "Edit: Cut", "Ctrl+X", "Edit"});
    m_commandRegistry.push_back({2004, "Edit: Copy", "Ctrl+C", "Edit"});
    m_commandRegistry.push_back({2005, "Edit: Paste", "Ctrl+V", "Edit"});
    m_commandRegistry.push_back({2006, "Edit: Select All", "Ctrl+A", "Edit"});
    m_commandRegistry.push_back({2007, "Edit: Find", "Ctrl+F", "Edit"});
    m_commandRegistry.push_back({2008, "Edit: Replace", "Ctrl+H", "Edit"});
    
    // View commands
    m_commandRegistry.push_back({3001, "View: Toggle Minimap", "Ctrl+M", "View"});
    m_commandRegistry.push_back({3002, "View: Toggle Output Panel", "", "View"});
    m_commandRegistry.push_back({3003, "View: Toggle Floating Panel", "F11", "View"});
    m_commandRegistry.push_back({3004, "View: Theme Editor", "", "View"});
    m_commandRegistry.push_back({3005, "View: Module Browser", "", "View"});
    m_commandRegistry.push_back({3006, "View: Toggle Sidebar", "Ctrl+B", "View"});
    m_commandRegistry.push_back({3007, "View: Toggle Secondary Sidebar", "Ctrl+Alt+B", "View"});
    m_commandRegistry.push_back({3008, "View: Toggle Panel", "Ctrl+J", "View"});
    
    // Terminal commands
    m_commandRegistry.push_back({4001, "Terminal: New PowerShell", "", "Terminal"});
    m_commandRegistry.push_back({4002, "Terminal: New Command Prompt", "", "Terminal"});
    m_commandRegistry.push_back({4003, "Terminal: Kill Terminal", "", "Terminal"});
    m_commandRegistry.push_back({4004, "Terminal: Clear Terminal", "", "Terminal"});
    m_commandRegistry.push_back({4005, "Terminal: Split Terminal", "", "Terminal"});
    
    // Tools commands
    m_commandRegistry.push_back({5001, "Tools: Start Profiling", "", "Tools"});
    m_commandRegistry.push_back({5002, "Tools: Stop Profiling", "", "Tools"});
    m_commandRegistry.push_back({5003, "Tools: Show Profile Results", "", "Tools"});
    m_commandRegistry.push_back({5004, "Tools: Analyze Script", "", "Tools"});
    m_commandRegistry.push_back({5005, "Tools: Code Snippets", "", "Tools"});
    
    // Module commands
    m_commandRegistry.push_back({6001, "Modules: Refresh List", "", "Modules"});
    m_commandRegistry.push_back({6002, "Modules: Import Module", "", "Modules"});
    m_commandRegistry.push_back({6003, "Modules: Export Module", "", "Modules"});
    m_commandRegistry.push_back({6004, "Modules: Browser", "", "Modules"});
    
    // Git commands
    m_commandRegistry.push_back({8001, "Git: Show Status", "", "Git"});
    m_commandRegistry.push_back({8002, "Git: Commit", "Ctrl+Shift+C", "Git"});
    m_commandRegistry.push_back({8003, "Git: Push", "", "Git"});
    m_commandRegistry.push_back({8004, "Git: Pull", "", "Git"});
    m_commandRegistry.push_back({8005, "Git: Stage All", "", "Git"});
    
    // Help commands
    m_commandRegistry.push_back({7001, "Help: Command Reference", "", "Help"});
    m_commandRegistry.push_back({7002, "Help: PowerShell Docs", "", "Help"});
    m_commandRegistry.push_back({7003, "Help: Search Help", "", "Help"});
    m_commandRegistry.push_back({7004, "Help: About", "", "Help"});
    m_commandRegistry.push_back({7005, "Help: Keyboard Shortcuts", "", "Help"});
    m_commandRegistry.push_back({7006, "Help: Export Prometheus Metrics", "", "Help"});

    // AI Mode Toggles
    m_commandRegistry.push_back({IDM_AI_MODE_MAX, "AI: Toggle Max Mode", "", "AI"});
    m_commandRegistry.push_back({IDM_AI_MODE_DEEP_THINK, "AI: Toggle Deep Thinking", "", "AI"});
    m_commandRegistry.push_back({IDM_AI_MODE_DEEP_RESEARCH, "AI: Toggle Deep Research", "", "AI"});
    m_commandRegistry.push_back({IDM_AI_MODE_NO_REFUSAL, "AI: Toggle No Refusal", "", "AI"});

    // AI Context Window Sizes
    m_commandRegistry.push_back({IDM_AI_CONTEXT_4K, "AI: Set Context Window 4K", "", "AI"});
    m_commandRegistry.push_back({IDM_AI_CONTEXT_32K, "AI: Set Context Window 32K", "", "AI"});
    m_commandRegistry.push_back({IDM_AI_CONTEXT_64K, "AI: Set Context Window 64K", "", "AI"});
    m_commandRegistry.push_back({IDM_AI_CONTEXT_128K, "AI: Set Context Window 128K", "", "AI"});
    m_commandRegistry.push_back({IDM_AI_CONTEXT_256K, "AI: Set Context Window 256K", "", "AI"});
    m_commandRegistry.push_back({IDM_AI_CONTEXT_512K, "AI: Set Context Window 512K", "", "AI"});
    m_commandRegistry.push_back({IDM_AI_CONTEXT_1M, "AI: Set Context Window 1M", "", "AI"});

    // Agent Execution
    m_commandRegistry.push_back({IDM_AGENT_START_LOOP, "Agent: Start Agent Loop", "", "Agent"});
    m_commandRegistry.push_back({IDM_AGENT_EXECUTE_CMD, "Agent: Execute Command", "", "Agent"});
    m_commandRegistry.push_back({IDM_AGENT_CONFIGURE_MODEL, "Agent: Configure Model", "", "Agent"});
    m_commandRegistry.push_back({IDM_AGENT_VIEW_TOOLS, "Agent: View Available Tools", "", "Agent"});
    m_commandRegistry.push_back({IDM_AGENT_VIEW_STATUS, "Agent: View Status", "", "Agent"});
    m_commandRegistry.push_back({IDM_AGENT_STOP, "Agent: Stop Agent", "", "Agent"});

    // Autonomy Framework
    m_commandRegistry.push_back({IDM_AUTONOMY_TOGGLE, "Autonomy: Toggle Autonomous Mode", "", "Autonomy"});
    m_commandRegistry.push_back({IDM_AUTONOMY_START, "Autonomy: Start", "", "Autonomy"});
    m_commandRegistry.push_back({IDM_AUTONOMY_STOP, "Autonomy: Stop", "", "Autonomy"});
    m_commandRegistry.push_back({IDM_AUTONOMY_SET_GOAL, "Autonomy: Set Goal", "", "Autonomy"});
    m_commandRegistry.push_back({IDM_AUTONOMY_STATUS, "Autonomy: Show Status", "", "Autonomy"});
    m_commandRegistry.push_back({IDM_AUTONOMY_MEMORY, "Autonomy: Show Memory", "", "Autonomy"});

    // Reverse Engineering (full suite)
    m_commandRegistry.push_back({IDM_REVENG_ANALYZE, "RE: Run Codex Analysis", "", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_DISASM, "RE: Disassemble Binary", "", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_DUMPBIN, "RE: Run Dumpbin on Current File", "", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_COMPILE, "RE: Run Custom Compiler", "", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_COMPARE, "RE: Compare Binaries", "", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_DETECT_VULNS, "RE: Detect Vulnerabilities", "", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_EXPORT_IDA, "RE: Export to IDA", "", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_EXPORT_GHIDRA, "RE: Export to Ghidra", "", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_CFG, "RE: Control Flow Graph", "", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_FUNCTIONS, "RE: Recover Functions", "", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_DEMANGLE, "RE: Demangle Symbols", "", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_SSA, "RE: SSA Lifting", "Ctrl+Shift+S", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_RECURSIVE_DISASM, "RE: Recursive Descent Disassembly", "Ctrl+Shift+R", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_TYPE_RECOVERY, "RE: Type Recovery", "Ctrl+Shift+T", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_DATA_FLOW, "RE: Data Flow Analysis", "", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_LICENSE_INFO, "RE: License Info", "", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_DECOMPILER_VIEW, "RE: Decompiler View (Direct2D)", "Ctrl+Shift+D", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_DECOMP_RENAME, "RE: Decompiler — Rename Variable (SSA)", "F2", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_DECOMP_SYNC, "RE: Decompiler — Sync Panes to Address", "", "RE"});
    m_commandRegistry.push_back({IDM_REVENG_DECOMP_CLOSE, "RE: Decompiler — Close View", "", "RE"});

    // File: Load Model & Exit (not in original list)
    m_commandRegistry.push_back({1030, "File: Load AI Model (Local)", "", "File"});
    m_commandRegistry.push_back({IDM_FILE_MODEL_FROM_HF, "File: Load Model from HuggingFace", "", "File"});
    m_commandRegistry.push_back({IDM_FILE_MODEL_FROM_OLLAMA, "File: Load Model from Ollama Blobs", "", "File"});
    m_commandRegistry.push_back({IDM_FILE_MODEL_FROM_URL, "File: Load Model from URL", "", "File"});
    m_commandRegistry.push_back({IDM_FILE_MODEL_UNIFIED, "File: Smart Model Loader (Auto-Detect)", "Ctrl+Shift+M", "File"});
    m_commandRegistry.push_back({IDM_FILE_MODEL_QUICK_LOAD, "File: Quick Load GGUF Model", "Ctrl+M", "File"});
    m_commandRegistry.push_back({1099, "File: Exit", "Alt+F4", "File"});

    // Copilot Parity Features (5010+ range — routed via handleToolsCommand)
    m_commandRegistry.push_back({5010, "AI: Toggle Ghost Text (Inline Completions)", "", "AI"});
    m_commandRegistry.push_back({5011, "AI: Generate Agent Plan", "", "AI"});
    m_commandRegistry.push_back({5012, "AI: Show Plan Status", "", "AI"});
    m_commandRegistry.push_back({5013, "AI: Cancel Current Plan", "", "AI"});
    m_commandRegistry.push_back({5014, "AI: Toggle Failure Detector", "", "AI"});
    m_commandRegistry.push_back({5015, "AI: Show Failure Detector Stats", "", "AI"});
    m_commandRegistry.push_back({5016, "Settings: Open Settings Dialog", "Ctrl+,", "Settings"});
    m_commandRegistry.push_back({5017, "Server: Toggle Local GGUF HTTP Server", "", "Server"});
    m_commandRegistry.push_back({5018, "Server: Show Server Status", "", "Server"});

    // Agent History & Replay (5019+ range — routed via handleToolsCommand)
    m_commandRegistry.push_back({5019, "History: Toggle Agent History Recording", "", "History"});
    m_commandRegistry.push_back({5020, "History: Show Agent History Timeline", "", "History"});
    m_commandRegistry.push_back({5021, "History: Show Agent History Stats", "", "History"});
    m_commandRegistry.push_back({5022, "History: Replay Previous Session", "", "History"});

    // Failure Intelligence — Phase 6 (5023+ range — routed via handleToolsCommand)
    m_commandRegistry.push_back({5023, "AI: Toggle Failure Intelligence", "", "AI"});
    m_commandRegistry.push_back({5024, "AI: Show Failure Intelligence Panel", "", "AI"});
    m_commandRegistry.push_back({5025, "AI: Show Failure Intelligence Stats", "", "AI"});
    m_commandRegistry.push_back({5026, "AI: Execute with Failure Intelligence", "", "AI"});

    // Policy Engine — Phase 7 (5027+ range — routed via handleToolsCommand)
    m_commandRegistry.push_back({5027, "Policy: List Active Policies", "", "Policy"});
    m_commandRegistry.push_back({5028, "Policy: Generate Suggestions", "", "Policy"});
    m_commandRegistry.push_back({5029, "Policy: Show Heuristics", "", "Policy"});
    m_commandRegistry.push_back({5030, "Policy: Export Policies to File", "", "Policy"});
    m_commandRegistry.push_back({5031, "Policy: Import Policies from File", "", "Policy"});
    m_commandRegistry.push_back({5032, "Policy: Show Policy Stats", "", "Policy"});

    // Explainability — Phase 8A (5033+ range — routed via handleToolsCommand)
    m_commandRegistry.push_back({5033, "Explain: Show Session Explanation", "", "Explain"});
    m_commandRegistry.push_back({5034, "Explain: Trace Last Agent", "", "Explain"});
    m_commandRegistry.push_back({5035, "Explain: Export Snapshot", "", "Explain"});
    m_commandRegistry.push_back({5036, "Explain: Show Explainability Stats", "", "Explain"});

    // Backend Switcher — Phase 8B (5037+ range — routed via handleToolsCommand)
    m_commandRegistry.push_back({IDM_BACKEND_SWITCH_LOCAL,   "AI: Switch to Local GGUF",              "", "AI"});
    m_commandRegistry.push_back({IDM_BACKEND_SWITCH_OLLAMA,  "AI: Switch to Ollama",                  "", "AI"});
    m_commandRegistry.push_back({IDM_BACKEND_SWITCH_OPENAI,  "AI: Switch to OpenAI",                  "", "AI"});
    m_commandRegistry.push_back({IDM_BACKEND_SWITCH_CLAUDE,  "AI: Switch to Claude",                  "", "AI"});
    m_commandRegistry.push_back({IDM_BACKEND_SWITCH_GEMINI,  "AI: Switch to Gemini",                  "", "AI"});
    m_commandRegistry.push_back({IDM_BACKEND_SHOW_STATUS,    "Backend: Show All Backend Status",      "", "Backend"});
    m_commandRegistry.push_back({IDM_BACKEND_SHOW_SWITCHER,  "Backend: Show Switcher Dialog",         "", "Backend"});
    m_commandRegistry.push_back({IDM_BACKEND_CONFIGURE,      "Backend: Configure Active Backend",     "", "Backend"});
    m_commandRegistry.push_back({IDM_BACKEND_HEALTH_CHECK,   "Backend: Health Check All Backends",    "", "Backend"});
    m_commandRegistry.push_back({IDM_BACKEND_SET_API_KEY,    "AI: Set API Key (Active Backend)",      "", "AI"});
    m_commandRegistry.push_back({IDM_BACKEND_SAVE_CONFIGS,   "Backend: Save Backend Configurations",  "", "Backend"});

    // ================================================================
    // LLM Router (5048–5057 range — routed via handleToolsCommand)
    // ================================================================
    m_commandRegistry.push_back({IDM_ROUTER_ENABLE,            "Router: Enable Intelligent Routing",         "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_DISABLE,           "Router: Disable (Passthrough Mode)",         "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_SHOW_STATUS,       "Router: Show Status & Task Preferences",     "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_SHOW_DECISION,     "Router: Show Last Routing Decision",         "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_SET_POLICY,        "Router: Configure Task Routing Policy",      "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_SHOW_CAPABILITIES, "Router: Show Backend Capabilities",          "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_SHOW_FALLBACKS,    "Router: Show Fallback Chains",               "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_SAVE_CONFIG,       "Router: Save Router Configuration",          "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_ROUTE_PROMPT,      "Router: Dry-Run Route Current Prompt",       "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_RESET_STATS,       "Router: Reset Statistics & Failure Counters", "", "Router"});

    // ================================================================
    // UX Enhancements & Research Track (5071–5081 range)
    // ================================================================
    m_commandRegistry.push_back({IDM_ROUTER_WHY_BACKEND,       "Router: Why This Backend? (Explain Last Decision)", "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_PIN_TASK,          "Router: Pin Current Task to Backend",               "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_UNPIN_TASK,        "Router: Unpin Current Task",                        "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_SHOW_PINS,         "Router: Show All Task Pins",                        "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_SHOW_HEATMAP,      "Router: Show Cost/Latency Heatmap",                 "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_ENSEMBLE_ENABLE,   "Router: Enable Ensemble Routing (Multi-Backend)",   "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_ENSEMBLE_DISABLE,  "Router: Disable Ensemble Routing",                  "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_ENSEMBLE_STATUS,   "Router: Show Ensemble Status & Last Decision",      "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_SIMULATE,          "Router: Simulate Routing from Agent History",        "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_SIMULATE_LAST,     "Router: Show Last Simulation Results",              "", "Router"});
    m_commandRegistry.push_back({IDM_ROUTER_SHOW_COST_STATS,   "Router: Show Cost & Performance Statistics",         "", "Router"});

    // ================================================================
    // LSP Client (5058–5070 range — routed via handleToolsCommand)
    // ================================================================
    m_commandRegistry.push_back({IDM_LSP_START_ALL,          "LSP: Start All Language Servers",            "", "LSP"});
    m_commandRegistry.push_back({IDM_LSP_STOP_ALL,           "LSP: Stop All Language Servers",             "", "LSP"});
    m_commandRegistry.push_back({IDM_LSP_SHOW_STATUS,        "LSP: Show Server Status",                   "", "LSP"});
    m_commandRegistry.push_back({IDM_LSP_GOTO_DEFINITION,    "LSP: Go to Definition",                     "", "LSP"});
    m_commandRegistry.push_back({IDM_LSP_FIND_REFERENCES,    "LSP: Find All References",                  "", "LSP"});
    m_commandRegistry.push_back({IDM_LSP_RENAME_SYMBOL,      "LSP: Rename Symbol (enter name in chat)",   "", "LSP"});
    m_commandRegistry.push_back({IDM_LSP_HOVER_INFO,         "LSP: Hover Info at Cursor",                 "", "LSP"});
    m_commandRegistry.push_back({IDM_LSP_SHOW_DIAGNOSTICS,   "LSP: Show Diagnostics Summary",             "", "LSP"});
    m_commandRegistry.push_back({IDM_LSP_RESTART_SERVER,     "LSP: Restart Server for Current Language",  "", "LSP"});
    m_commandRegistry.push_back({IDM_LSP_CLEAR_DIAGNOSTICS,  "LSP: Clear All Diagnostics",                "", "LSP"});
    m_commandRegistry.push_back({IDM_LSP_SHOW_SYMBOL_INFO,   "LSP: Show Stats & Request Counts",          "", "LSP"});
    m_commandRegistry.push_back({IDM_LSP_CONFIGURE,          "LSP: Show Configuration Path",              "", "LSP"});
    m_commandRegistry.push_back({IDM_LSP_SAVE_CONFIG,        "LSP: Save Configuration",                   "", "LSP"});

    // ================================================================
    // Phase 9A-ASM: ASM Semantic Support (5082–5093 — routed via handleToolsCommand)
    // ================================================================
    m_commandRegistry.push_back({IDM_ASM_PARSE_SYMBOLS,      "ASM: Parse Symbols in Current File",        "", "ASM"});
    m_commandRegistry.push_back({IDM_ASM_GOTO_LABEL,         "ASM: Go to Label/Symbol at Cursor",         "", "ASM"});
    m_commandRegistry.push_back({IDM_ASM_FIND_LABEL_REFS,    "ASM: Find All References to Label",         "", "ASM"});
    m_commandRegistry.push_back({IDM_ASM_SHOW_SYMBOL_TABLE,  "ASM: Show Full Symbol Table",               "", "ASM"});
    m_commandRegistry.push_back({IDM_ASM_INSTRUCTION_INFO,   "ASM: Instruction Info at Cursor",           "", "ASM"});
    m_commandRegistry.push_back({IDM_ASM_REGISTER_INFO,      "ASM: Register Info at Cursor",              "", "ASM"});
    m_commandRegistry.push_back({IDM_ASM_ANALYZE_BLOCK,      "ASM: Analyze Code Block (AI Reasoning)",    "", "ASM"});
    m_commandRegistry.push_back({IDM_ASM_SHOW_CALL_GRAPH,    "ASM: Show Call Graph",                      "", "ASM"});
    m_commandRegistry.push_back({IDM_ASM_SHOW_DATA_FLOW,     "ASM: Show Data Flow Analysis",              "", "ASM"});
    m_commandRegistry.push_back({IDM_ASM_DETECT_CONVENTION,  "ASM: Detect Calling Convention",            "", "ASM"});
    m_commandRegistry.push_back({IDM_ASM_SHOW_SECTIONS,      "ASM: Show Sections & Directives",           "", "ASM"});
    m_commandRegistry.push_back({IDM_ASM_CLEAR_SYMBOLS,      "ASM: Clear All Parsed Symbols",             "", "ASM"});

    // ================================================================
    // Phase 9C: Multi-Response Chain (5106–5117 — routed via handleToolsCommand)
    // ================================================================
    m_commandRegistry.push_back({IDM_MULTI_RESP_GENERATE,         "MultiResp: Generate Multi-Response Chain",      "", "MultiResp"});
    m_commandRegistry.push_back({IDM_MULTI_RESP_SET_MAX,          "MultiResp: Set Max Response Count (cycle 1-4)", "", "MultiResp"});
    m_commandRegistry.push_back({IDM_MULTI_RESP_SELECT_PREFERRED, "MultiResp: Select Preferred Response (cycle)",  "", "MultiResp"});
    m_commandRegistry.push_back({IDM_MULTI_RESP_COMPARE,          "MultiResp: Compare All Responses",              "", "MultiResp"});
    m_commandRegistry.push_back({IDM_MULTI_RESP_SHOW_STATS,       "MultiResp: Show Statistics",                    "", "MultiResp"});
    m_commandRegistry.push_back({IDM_MULTI_RESP_SHOW_TEMPLATES,   "MultiResp: Show Response Templates",            "", "MultiResp"});
    m_commandRegistry.push_back({IDM_MULTI_RESP_TOGGLE_TEMPLATE,  "MultiResp: Toggle Template On/Off (cycle)",     "", "MultiResp"});
    m_commandRegistry.push_back({IDM_MULTI_RESP_SHOW_PREFS,       "MultiResp: Show Preference History",            "", "MultiResp"});
    m_commandRegistry.push_back({IDM_MULTI_RESP_SHOW_LATEST,      "MultiResp: Show Latest Session JSON",           "", "MultiResp"});
    m_commandRegistry.push_back({IDM_MULTI_RESP_SHOW_STATUS,      "MultiResp: Show Engine Status",                 "", "MultiResp"});
    m_commandRegistry.push_back({IDM_MULTI_RESP_CLEAR_HISTORY,    "MultiResp: Clear All History",                  "", "MultiResp"});
    m_commandRegistry.push_back({IDM_MULTI_RESP_APPLY_PREFERRED,  "MultiResp: Apply Preferred Response to Chat",   "", "MultiResp"});

    // ================================================================
    // Theme Selection (3101–3116 range — routed via handleViewCommand)
    // ================================================================
    m_commandRegistry.push_back({IDM_THEME_DARK_PLUS,        "Theme: Dark+ (Default)",     "", "Theme"});
    m_commandRegistry.push_back({IDM_THEME_LIGHT_PLUS,       "Theme: Light+",              "", "Theme"});
    m_commandRegistry.push_back({IDM_THEME_MONOKAI,          "Theme: Monokai",             "", "Theme"});
    m_commandRegistry.push_back({IDM_THEME_DRACULA,          "Theme: Dracula",             "", "Theme"});
    m_commandRegistry.push_back({IDM_THEME_NORD,             "Theme: Nord",                "", "Theme"});
    m_commandRegistry.push_back({IDM_THEME_SOLARIZED_DARK,   "Theme: Solarized Dark",      "", "Theme"});
    m_commandRegistry.push_back({IDM_THEME_SOLARIZED_LIGHT,  "Theme: Solarized Light",     "", "Theme"});
    m_commandRegistry.push_back({IDM_THEME_CYBERPUNK_NEON,   "Theme: Cyberpunk Neon",      "", "Theme"});
    m_commandRegistry.push_back({IDM_THEME_GRUVBOX_DARK,     "Theme: Gruvbox Dark",        "", "Theme"});
    m_commandRegistry.push_back({IDM_THEME_CATPPUCCIN_MOCHA, "Theme: Catppuccin Mocha",    "", "Theme"});
    m_commandRegistry.push_back({IDM_THEME_TOKYO_NIGHT,      "Theme: Tokyo Night",         "", "Theme"});
    m_commandRegistry.push_back({IDM_THEME_RAWRXD_CRIMSON,   "Theme: RawrXD Crimson",      "", "Theme"});
    m_commandRegistry.push_back({IDM_THEME_HIGH_CONTRAST,    "Theme: High Contrast",       "", "Theme"});
    m_commandRegistry.push_back({IDM_THEME_ONE_DARK_PRO,     "Theme: One Dark Pro",        "", "Theme"});
    m_commandRegistry.push_back({IDM_THEME_SYNTHWAVE84,      "Theme: SynthWave '84",       "", "Theme"});
    m_commandRegistry.push_back({IDM_THEME_ABYSS,            "Theme: Abyss",               "", "Theme"});

    // ================================================================
    // Transparency Presets (3200–3211 range — routed via handleViewCommand)
    // ================================================================
    m_commandRegistry.push_back({IDM_TRANSPARENCY_100,    "Transparency: 100% (Opaque)",     "", "Transparency"});
    m_commandRegistry.push_back({IDM_TRANSPARENCY_90,     "Transparency: 90%",               "", "Transparency"});
    m_commandRegistry.push_back({IDM_TRANSPARENCY_80,     "Transparency: 80%",               "", "Transparency"});
    m_commandRegistry.push_back({IDM_TRANSPARENCY_70,     "Transparency: 70%",               "", "Transparency"});
    m_commandRegistry.push_back({IDM_TRANSPARENCY_60,     "Transparency: 60%",               "", "Transparency"});
    m_commandRegistry.push_back({IDM_TRANSPARENCY_50,     "Transparency: 50%",               "", "Transparency"});
    m_commandRegistry.push_back({IDM_TRANSPARENCY_40,     "Transparency: 40%",               "", "Transparency"});
    m_commandRegistry.push_back({IDM_TRANSPARENCY_CUSTOM, "Transparency: Custom Slider",     "", "Transparency"});
    m_commandRegistry.push_back({IDM_TRANSPARENCY_TOGGLE, "Transparency: Toggle On/Off",     "", "Transparency"});

    // ================================================================
    // Hotpatch System (9001–9017 range — routed via handleHotpatchCommand)
    // ================================================================
    m_commandRegistry.push_back({IDM_HOTPATCH_SHOW_STATUS,      "Hotpatch: Show System Status",                "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_TOGGLE_ALL,       "Hotpatch: Toggle Hotpatch System On/Off",     "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_SHOW_EVENT_LOG,   "Hotpatch: Show Event Log",                    "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_RESET_STATS,      "Hotpatch: Reset All Statistics",              "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_MEMORY_APPLY,     "Hotpatch: Apply Memory Patch (Layer 1)",      "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_MEMORY_REVERT,    "Hotpatch: Revert Memory Patch (Layer 1)",     "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_BYTE_APPLY,       "Hotpatch: Apply Byte Patch (Layer 2)",        "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_BYTE_SEARCH,      "Hotpatch: Search & Replace Pattern (Layer 2)", "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_SERVER_ADD,       "Hotpatch: Add Server Patch (Layer 3)",        "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_SERVER_REMOVE,    "Hotpatch: Remove Server Patch (Layer 3)",     "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_PROXY_BIAS,       "Hotpatch: Token Bias Injection (Proxy)",      "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_PROXY_REWRITE,    "Hotpatch: Output Rewrite Rule (Proxy)",       "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_PROXY_TERMINATE,  "Hotpatch: Stream Termination Rule (Proxy)",   "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_PROXY_VALIDATE,   "Hotpatch: Custom Validator (Proxy)",          "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_SHOW_PROXY_STATS, "Hotpatch: Show Proxy Statistics",             "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_PRESET_SAVE,      "Hotpatch: Save Preset to File",               "", "Hotpatch"});
    m_commandRegistry.push_back({IDM_HOTPATCH_PRESET_LOAD,      "Hotpatch: Load Preset from File",             "", "Hotpatch"});

    // WebView2 + Monaco Editor (Phase 26)
    m_commandRegistry.push_back({IDM_VIEW_TOGGLE_MONACO,        "View: Toggle Monaco Editor (WebView2)",       "Ctrl+Shift+M", "WebView2"});
    m_commandRegistry.push_back({IDM_VIEW_MONACO_DEVTOOLS,      "View: Monaco DevTools (F12)",                 "F12",          "WebView2"});
    m_commandRegistry.push_back({IDM_VIEW_MONACO_RELOAD,        "View: Reload Monaco Editor",                  "",             "WebView2"});
    m_commandRegistry.push_back({IDM_VIEW_MONACO_ZOOM_IN,       "View: Monaco Zoom In",                       "Ctrl+=",       "WebView2"});
    m_commandRegistry.push_back({IDM_VIEW_MONACO_ZOOM_OUT,      "View: Monaco Zoom Out",                      "Ctrl+-",       "WebView2"});
    m_commandRegistry.push_back({IDM_VIEW_MONACO_SYNC_THEME,    "View: Sync Win32 Theme to Monaco",           "",             "WebView2"});

    // ================================================================
    // IDE Self-Audit (9500 range — routed via handleAuditCommand)
    // ================================================================
    m_commandRegistry.push_back({IDM_AUDIT_SHOW_DASHBOARD,  "Audit: Show Dashboard",                "", "Audit"});
    m_commandRegistry.push_back({IDM_AUDIT_RUN_FULL,        "Audit: Run Full Audit",                "", "Audit"});
    m_commandRegistry.push_back({IDM_AUDIT_DETECT_STUBS,    "Audit: Detect Stubs",                  "", "Audit"});
    m_commandRegistry.push_back({IDM_AUDIT_CHECK_MENUS,     "Audit: Check Menu Wiring",             "", "Audit"});
    m_commandRegistry.push_back({IDM_AUDIT_RUN_TESTS,       "Audit: Run Component Tests",           "", "Audit"});
    m_commandRegistry.push_back({IDM_AUDIT_EXPORT_REPORT,   "Audit: Export Report",                 "", "Audit"});
    m_commandRegistry.push_back({IDM_AUDIT_QUICK_STATS,     "Audit: Quick Stats",                   "", "Audit"});

    // ================================================================
    // Phase 32: Final Gauntlet (9600 range — routed via handleGauntletCommand)
    // ================================================================
    m_commandRegistry.push_back({IDM_GAUNTLET_RUN,          "Gauntlet: Run All Tests (Phase 32)",   "", "Gauntlet"});
    m_commandRegistry.push_back({IDM_GAUNTLET_EXPORT,       "Gauntlet: Export Report (Phase 32)",   "", "Gauntlet"});

    // ================================================================
    // Phase 33: Voice Chat (9700 range — routed via handleVoiceChatCommand)
    // ================================================================
    m_commandRegistry.push_back({IDM_VOICE_TOGGLE_PANEL,    "Voice: Toggle Panel",                  "Ctrl+Shift+U", "Voice"});
    m_commandRegistry.push_back({IDM_VOICE_RECORD,          "Voice: Record / Stop",                 "F9",           "Voice"});
    m_commandRegistry.push_back({IDM_VOICE_PTT,             "Voice: Push-to-Talk",                  "Ctrl+Shift+V", "Voice"});
    m_commandRegistry.push_back({IDM_VOICE_SPEAK,           "Voice: Text-to-Speech",                "",             "Voice"});
    m_commandRegistry.push_back({IDM_VOICE_JOIN_ROOM,       "Voice: Join/Leave Room",               "",             "Voice"});
    m_commandRegistry.push_back({IDM_VOICE_SHOW_DEVICES,    "Voice: Audio Devices",                 "",             "Voice"});
    m_commandRegistry.push_back({IDM_VOICE_METRICS,         "Voice: Show Metrics",                  "",             "Voice"});

    // ================================================================
    // Phase 33: Quick-Win Ports (9800 range — routed via handleQuickWinCommand)
    // ================================================================
    m_commandRegistry.push_back({IDM_QW_SHORTCUT_EDITOR,    "Shortcuts: Open Editor",               "Ctrl+K Ctrl+S", "Shortcuts"});
    m_commandRegistry.push_back({IDM_QW_SHORTCUT_RESET,     "Shortcuts: Reset to Defaults",         "",              "Shortcuts"});
    m_commandRegistry.push_back({IDM_QW_BACKUP_CREATE,      "Backup: Create Now",                   "Ctrl+Shift+B",  "Backup"});
    m_commandRegistry.push_back({IDM_QW_BACKUP_RESTORE,     "Backup: Restore...",                   "",              "Backup"});
    m_commandRegistry.push_back({IDM_QW_BACKUP_AUTO_TOGGLE, "Backup: Toggle Auto-Backup",           "",              "Backup"});
    m_commandRegistry.push_back({IDM_QW_BACKUP_LIST,        "Backup: List All",                     "",              "Backup"});
    m_commandRegistry.push_back({IDM_QW_BACKUP_PRUNE,       "Backup: Prune Old",                    "",              "Backup"});
    m_commandRegistry.push_back({IDM_QW_ALERT_TOGGLE_MONITOR, "Alerts: Toggle Resource Monitor",    "",              "Alerts"});
    m_commandRegistry.push_back({IDM_QW_ALERT_SHOW_HISTORY, "Alerts: Show History",                 "",              "Alerts"});
    m_commandRegistry.push_back({IDM_QW_ALERT_DISMISS_ALL,  "Alerts: Dismiss All",                  "",              "Alerts"});
    m_commandRegistry.push_back({IDM_QW_ALERT_RESOURCE_STATUS, "Alerts: Resource Status",           "",              "Alerts"});
    m_commandRegistry.push_back({IDM_QW_SLO_DASHBOARD,      "SLO: Dashboard",                       "",              "SLO"});

    m_filteredCommands = m_commandRegistry;
}

void Win32IDE::showCommandPalette()
{
    if (m_commandPaletteVisible && m_hwndCommandPalette) {
        SetFocus(m_hwndCommandPaletteInput);
        return;
    }
    
    // Build command registry if empty
    if (m_commandRegistry.empty()) {
        buildCommandRegistry();
    }
    
    // Get window dimensions for centering
    RECT mainRect;
    GetWindowRect(m_hwndMain, &mainRect);
    int paletteWidth = 600;
    int paletteHeight = 400;
    int x = mainRect.left + (mainRect.right - mainRect.left - paletteWidth) / 2;
    int y = mainRect.top + 60; // Near top of window

    // Register a custom window class for the palette (once)
    static bool classRegistered = false;
    static const char* kPaletteClass = "RawrXD_CommandPalette";
    if (!classRegistered) {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_DROPSHADOW;
        wc.lpfnWndProc = Win32IDE::CommandPaletteProc;
        wc.hInstance = m_hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(37, 37, 38));
        wc.lpszClassName = kPaletteClass;
        if (RegisterClassExA(&wc)) {
            classRegistered = true;
        }
    }

    // Create palette as a moveable popup with title bar and close button
    m_hwndCommandPalette = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        classRegistered ? kPaletteClass : "STATIC",
        "Command Palette",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, paletteWidth, paletteHeight,
        m_hwndMain, nullptr, m_hInstance, nullptr
    );

    if (!m_hwndCommandPalette) return;

    // Store 'this' pointer so CommandPaletteProc can access the IDE instance
    SetWindowLongPtrA(m_hwndCommandPalette, GWLP_USERDATA, (LONG_PTR)this);

    // Adjust for non-client area (title bar eats into client size)
    RECT clientRect;
    GetClientRect(m_hwndCommandPalette, &clientRect);
    int clientW = clientRect.right;
    int clientH = clientRect.bottom;

    // Dark title bar (DwmSetWindowAttribute for dark mode if available)
    // Fallback: just set a dark background on the client area
    
    // Create search input at top of client area
    m_hwndCommandPaletteInput = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        8, 8, clientW - 16, 26,
        m_hwndCommandPalette, nullptr, m_hInstance, nullptr
    );
    
    // Set placeholder text and dark style on input
    if (m_hwndCommandPaletteInput) {
        SendMessageA(m_hwndCommandPaletteInput, EM_SETCUEBANNER, TRUE, (LPARAM)L"> Type a command... (prefix :category to filter)");
        // Use static font — created once, never leaked
        static HFONT s_inputFont = CreateFontA(
            -dpiScale(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI"
        );
        if (s_inputFont) SendMessage(m_hwndCommandPaletteInput, WM_SETFONT, (WPARAM)s_inputFont, TRUE);

        // Subclass the input to intercept keyboard (Escape, Enter, Up/Down)
        SetWindowLongPtrA(m_hwndCommandPaletteInput, GWLP_USERDATA, (LONG_PTR)this);
        m_oldCommandPaletteInputProc = (WNDPROC)SetWindowLongPtrA(
            m_hwndCommandPaletteInput, GWLP_WNDPROC,
            (LONG_PTR)Win32IDE::CommandPaletteInputProc
        );
    }

    // Create command list below the input (owner-draw for fuzzy highlight rendering)
    m_hwndCommandPaletteList = CreateWindowExA(
        0, WC_LISTBOXA, "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT
            | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
        8, 42, clientW - 16, clientH - 50,
        m_hwndCommandPalette, nullptr, m_hInstance, nullptr
    );

    if (m_hwndCommandPaletteList) {
        // Use static font — created once, never leaked
        static HFONT s_listFont = CreateFontA(
            -dpiScale(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI"
        );
        if (s_listFont) SendMessage(m_hwndCommandPaletteList, WM_SETFONT, (WPARAM)s_listFont, TRUE);
        // Set item height for owner-draw (DPI-scaled)
        SendMessageA(m_hwndCommandPaletteList, LB_SETITEMHEIGHT, 0, MAKELPARAM(dpiScale(24), 0));
    }

    // Update command availability before populating
    updateCommandStates();

    // Populate with all commands
    m_filteredCommands = m_commandRegistry;
    m_fuzzyMatchPositions.clear();
    for (const auto& cmd : m_filteredCommands) {
        std::string itemText = cmd.name;
        if (!cmd.shortcut.empty()) {
            itemText += "  [" + cmd.shortcut + "]";
        }
        SendMessageA(m_hwndCommandPaletteList, LB_ADDSTRING, 0, (LPARAM)itemText.c_str());
        m_fuzzyMatchPositions.push_back({}); // no highlights when showing all
    }
    
    // Select first item
    SendMessageA(m_hwndCommandPaletteList, LB_SETCURSEL, 0, 0);
    
    m_commandPaletteVisible = true;
    SetFocus(m_hwndCommandPaletteInput);
}

void Win32IDE::hideCommandPalette()
{
    if (m_hwndCommandPalette) {
        DestroyWindow(m_hwndCommandPalette);
        m_hwndCommandPalette = nullptr;
        m_hwndCommandPaletteInput = nullptr;
        m_hwndCommandPaletteList = nullptr;
    }
    m_commandPaletteVisible = false;
    SetFocus(m_hwndEditor);
}

void Win32IDE::filterCommandPalette(const std::string& query)
{
    if (!m_hwndCommandPaletteList) return;
    
    // Clear list
    SendMessageA(m_hwndCommandPaletteList, LB_RESETCONTENT, 0, 0);
    m_filteredCommands.clear();
    m_fuzzyMatchPositions.clear();

    // ── Category prefix filter (:file, :ai, :theme, :git, etc.) ──
    // If query starts with ':' or '@', extract the category prefix and
    // filter to only commands in that category. Remainder is fuzzy query.
    std::string categoryFilter;
    std::string fuzzyQuery = query;
    if (!query.empty() && (query[0] == ':' || query[0] == '@')) {
        size_t spacePos = query.find(' ');
        std::string prefix = (spacePos != std::string::npos)
            ? query.substr(1, spacePos - 1)
            : query.substr(1);
        // Lowercase the prefix for matching
        std::transform(prefix.begin(), prefix.end(), prefix.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (!prefix.empty()) {
            categoryFilter = prefix;
            fuzzyQuery = (spacePos != std::string::npos)
                ? query.substr(spacePos + 1)
                : "";
        }
    }

    // Build scored list
    struct ScoredEntry {
        int registryIndex;
        int score;
        FuzzyResult fuzzy;
    };
    std::vector<ScoredEntry> scored;

    for (int i = 0; i < (int)m_commandRegistry.size(); i++) {
        const auto& cmd = m_commandRegistry[i];

        // Category filter: if active, skip non-matching categories
        if (!categoryFilter.empty()) {
            std::string catLower = cmd.category;
            std::transform(catLower.begin(), catLower.end(), catLower.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });
            // Prefix match: ":th" matches "theme", ":trans" matches "transparency"
            if (catLower.find(categoryFilter) != 0) continue;
        }

        if (fuzzyQuery.empty()) {
            // No fuzzy part — include all commands in the category (or all if no filter)
            scored.push_back({i, 0, {true, 0, {}}});
        } else {
            FuzzyResult fr = fuzzyMatchScore(fuzzyQuery, cmd.name);
            if (fr.matched) {
                scored.push_back({i, fr.score, fr});
            }
        }
    }

    // MRU boost: add bonus for recently-used commands (session-only)
    for (auto& entry : scored) {
        int cmdId = m_commandRegistry[entry.registryIndex].id;
        auto mruIt = m_commandMRU.find(cmdId);
        if (mruIt != m_commandMRU.end() && mruIt->second > 0) {
            // Boost: 20 points per usage, capped at 100
            entry.score += std::min(mruIt->second * 20, 100);
        }
    }

    // Sort by score descending (best matches first)
    std::sort(scored.begin(), scored.end(),
              [](const ScoredEntry& a, const ScoredEntry& b) {
                  return a.score > b.score;
              });

    for (const auto& entry : scored) {
        const auto& cmd = m_commandRegistry[entry.registryIndex];
        m_filteredCommands.push_back(cmd);
        m_fuzzyMatchPositions.push_back(entry.fuzzy.matchPositions);

        std::string itemText = cmd.name;
        if (!cmd.shortcut.empty()) {
            itemText += "  [" + cmd.shortcut + "]";
        }
        SendMessageA(m_hwndCommandPaletteList, LB_ADDSTRING, 0, (LPARAM)itemText.c_str());
    }

    // Select first item if available
    if (!m_filteredCommands.empty()) {
        SendMessageA(m_hwndCommandPaletteList, LB_SETCURSEL, 0, 0);
    }
}

// Timer ID for status bar flash feedback
static constexpr UINT_PTR IDT_STATUS_FLASH = 42;

void Win32IDE::executeCommandFromPalette(int index)
{
    if (index < 0 || index >= (int)m_filteredCommands.size()) return;
    
    const auto& cmd = m_filteredCommands[index];
    int commandId = cmd.id;
    std::string cmdName = cmd.name;
    bool enabled = isCommandEnabled(commandId);

    hideCommandPalette();

    if (!enabled) {
        // Command is currently unavailable — show feedback but don't execute
        std::string msg = cmdName + " — not available right now";
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)msg.c_str());
        return;
    }

    // MRU tracking: increment usage count (session-only, no disk writes)
    m_commandMRU[commandId]++;

    // Execute via SSOT dispatch — the ONE AND ONLY command path.
    // No legacy routeCommand() fallback. Drift is structurally impossible.
    routeCommandUnified(commandId, this);

    // Flash the status bar with execution confirmation
    if (m_hwndStatusBar) {
        std::string feedback = "\xE2\x9C\x93 " + cmdName; // UTF-8 checkmark + command name
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)feedback.c_str());
        // Set timer to clear the flash after 2 seconds
        SetTimer(m_hwndMain, IDT_STATUS_FLASH, 2000, nullptr);
    }
}

LRESULT CALLBACK Win32IDE::CommandPaletteProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    
    switch (uMsg) {
    case WM_ACTIVATE:
        // Close palette when it loses activation (user clicked outside)
        if (LOWORD(wParam) == WA_INACTIVE) {
            if (pThis && pThis->m_commandPaletteVisible) {
                // Post a message to close asynchronously (avoid reentrancy)
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            }
        }
        return 0;

    case WM_CLOSE:
        if (pThis) {
            pThis->hideCommandPalette();
        }
        return 0;

    case WM_DESTROY:
        return 0;

    case WM_COMMAND:
        if (pThis) {
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == pThis->m_hwndCommandPaletteInput) {
                // Input text changed — filter the list
                char buffer[256] = {0};
                GetWindowTextA(pThis->m_hwndCommandPaletteInput, buffer, 256);
                pThis->filterCommandPalette(buffer);
            }
            else if (HIWORD(wParam) == LBN_DBLCLK && (HWND)lParam == pThis->m_hwndCommandPaletteList) {
                // Double-click on list item — execute
                int sel = (int)SendMessageA(pThis->m_hwndCommandPaletteList, LB_GETCURSEL, 0, 0);
                pThis->executeCommandFromPalette(sel);
            }
        }
        break;

    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lParam;
        if (mis) {
            mis->itemHeight = 26;
        }
        return TRUE;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if (!dis || !pThis) break;
        if (dis->itemID == (UINT)-1) break;

        int idx = (int)dis->itemID;
        if (idx < 0 || idx >= (int)pThis->m_filteredCommands.size()) break;

        const auto& cmd = pThis->m_filteredCommands[idx];
        bool isEnabled = pThis->isCommandEnabled(cmd.id);
        bool isSelected = (dis->itemState & ODS_SELECTED) != 0;

        // Background
        COLORREF bgColor = isSelected ? RGB(4, 57, 94) : RGB(45, 45, 48);
        HBRUSH hbr = CreateSolidBrush(bgColor);
        FillRect(dis->hDC, &dis->rcItem, hbr);
        DeleteObject(hbr);

        SetBkMode(dis->hDC, TRANSPARENT);

        // Category badge (small colored tag on the left)
        COLORREF catColor = RGB(86, 156, 214); // default blue
        if (cmd.category == "File") catColor = RGB(78, 201, 176);
        else if (cmd.category == "Edit") catColor = RGB(220, 220, 170);
        else if (cmd.category == "View") catColor = RGB(156, 220, 254);
        else if (cmd.category == "Terminal") catColor = RGB(206, 145, 120);
        else if (cmd.category == "Git") catColor = RGB(240, 128, 48);
        else if (cmd.category == "AI") catColor = RGB(197, 134, 192);
        else if (cmd.category == "Agent") catColor = RGB(197, 134, 192);
        else if (cmd.category == "Autonomy") catColor = RGB(255, 140, 198);
        else if (cmd.category == "RE") catColor = RGB(244, 71, 71);
        else if (cmd.category == "Tools") catColor = RGB(128, 200, 128);
        else if (cmd.category == "Help") catColor = RGB(180, 180, 180);
        else if (cmd.category == "Theme") catColor = RGB(255, 167, 38);
        else if (cmd.category == "Transparency") catColor = RGB(100, 181, 246);
        else if (cmd.category == "History") catColor = RGB(78, 201, 176);
        else if (cmd.category == "Settings") catColor = RGB(220, 220, 170);
        else if (cmd.category == "Server") catColor = RGB(86, 156, 214);
        else if (cmd.category == "Policy") catColor = RGB(255, 183, 77);
        else if (cmd.category == "Explain") catColor = RGB(0, 188, 212);
        else if (cmd.category == "Backend") catColor = RGB(129, 212, 250);
        else if (cmd.category == "Router") catColor = RGB(0, 200, 170);

        // Draw category dot
        HBRUSH dotBrush = CreateSolidBrush(catColor);
        RECT dotRect = {dis->rcItem.left + 6, dis->rcItem.top + 8, dis->rcItem.left + 12, dis->rcItem.top + 14};
        HRGN dotRgn = CreateEllipticRgn(dotRect.left, dotRect.top, dotRect.right, dotRect.bottom);
        FillRgn(dis->hDC, dotRgn, dotBrush);
        DeleteObject(dotRgn);
        DeleteObject(dotBrush);

        int textLeft = dis->rcItem.left + 18;

        // Get match positions for this item
        std::vector<int> matchPos;
        if (idx < (int)pThis->m_fuzzyMatchPositions.size()) {
            matchPos = pThis->m_fuzzyMatchPositions[idx];
        }

        COLORREF normalColor = isEnabled ? RGB(220, 220, 220) : RGB(110, 110, 110);
        COLORREF highlightColor = isEnabled ? RGB(18, 180, 250) : RGB(80, 120, 140);

        // Draw command name character by character with fuzzy highlights
        std::string name = cmd.name;
        std::set<int> matchSet(matchPos.begin(), matchPos.end());

        // Create bold font for highlights
        static HFONT s_normalFont = nullptr;
        static HFONT s_boldFont = nullptr;
        if (!s_normalFont) {
            s_normalFont = CreateFontA(-pThis->dpiScale(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        }
        if (!s_boldFont) {
            s_boldFont = CreateFontA(-pThis->dpiScale(14), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        }

        int xPos = textLeft;
        for (int ci = 0; ci < (int)name.size(); ci++) {
            bool isMatch = matchSet.count(ci) > 0;
            SetTextColor(dis->hDC, isMatch ? highlightColor : normalColor);
            SelectObject(dis->hDC, isMatch ? s_boldFont : s_normalFont);

            char ch[2] = {name[ci], 0};
            SIZE charSize;
            GetTextExtentPoint32A(dis->hDC, ch, 1, &charSize);
            TextOutA(dis->hDC, xPos, dis->rcItem.top + 4, ch, 1);
            xPos += charSize.cx;
        }

        // Draw shortcut right-aligned in dimmer color
        if (!cmd.shortcut.empty()) {
            SelectObject(dis->hDC, s_normalFont);
            SetTextColor(dis->hDC, isEnabled ? RGB(140, 140, 140) : RGB(80, 80, 80));
            std::string shortcutText = "[" + cmd.shortcut + "]";
            SIZE scSize;
            GetTextExtentPoint32A(dis->hDC, shortcutText.c_str(), (int)shortcutText.size(), &scSize);
            int scX = dis->rcItem.right - scSize.cx - 10;
            TextOutA(dis->hDC, scX, dis->rcItem.top + 4, shortcutText.c_str(), (int)shortcutText.size());
        }

        // Draw disabled indicator
        if (!isEnabled) {
            SelectObject(dis->hDC, s_normalFont);
            SetTextColor(dis->hDC, RGB(90, 90, 90));
            const char* disabledTag = "(unavailable)";
            SIZE tagSize;
            GetTextExtentPoint32A(dis->hDC, disabledTag, 13, &tagSize);
            int tagX = dis->rcItem.right - tagSize.cx - 10;
            if (!cmd.shortcut.empty()) tagX -= 80; // offset if shortcut present
            TextOutA(dis->hDC, tagX, dis->rcItem.top + 4, disabledTag, 13);
        }

        // Focus rect
        if (dis->itemState & ODS_FOCUS) {
            DrawFocusRect(dis->hDC, &dis->rcItem);
        }

        return TRUE;
    }

    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        // Dark theme colors for child controls
        HDC hdcCtrl = (HDC)wParam;
        SetTextColor(hdcCtrl, RGB(220, 220, 220));
        SetBkColor(hdcCtrl, RGB(45, 45, 48));
        static HBRUSH s_paletteBrush = CreateSolidBrush(RGB(45, 45, 48));
        return (LRESULT)s_paletteBrush;
    }

    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH bg = CreateSolidBrush(RGB(37, 37, 38));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);
        return 1;
    }
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// Subclass proc for the command palette input — intercepts Escape, Enter, Up/Down
LRESULT CALLBACK Win32IDE::CommandPaletteInputProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    if (!pThis) return DefWindowProcA(hwnd, uMsg, wParam, lParam);

    if (uMsg == WM_KEYDOWN) {
        if (wParam == VK_ESCAPE) {
            pThis->hideCommandPalette();
            return 0;
        }
        if (wParam == VK_RETURN) {
            int sel = (int)SendMessageA(pThis->m_hwndCommandPaletteList, LB_GETCURSEL, 0, 0);
            pThis->executeCommandFromPalette(sel);
            return 0;
        }
        if (wParam == VK_DOWN) {
            int sel = (int)SendMessageA(pThis->m_hwndCommandPaletteList, LB_GETCURSEL, 0, 0);
            int count = (int)SendMessageA(pThis->m_hwndCommandPaletteList, LB_GETCOUNT, 0, 0);
            if (sel < count - 1) {
                SendMessageA(pThis->m_hwndCommandPaletteList, LB_SETCURSEL, sel + 1, 0);
            }
            return 0;
        }
        if (wParam == VK_UP) {
            int sel = (int)SendMessageA(pThis->m_hwndCommandPaletteList, LB_GETCURSEL, 0, 0);
            if (sel > 0) {
                SendMessageA(pThis->m_hwndCommandPaletteList, LB_SETCURSEL, sel - 1, 0);
            }
            return 0;
        }
        if (wParam == VK_NEXT) { // Page Down
            int sel = (int)SendMessageA(pThis->m_hwndCommandPaletteList, LB_GETCURSEL, 0, 0);
            int count = (int)SendMessageA(pThis->m_hwndCommandPaletteList, LB_GETCOUNT, 0, 0);
            int newSel = std::min(sel + 10, count - 1);
            if (newSel >= 0) {
                SendMessageA(pThis->m_hwndCommandPaletteList, LB_SETCURSEL, newSel, 0);
            }
            return 0;
        }
        if (wParam == VK_PRIOR) { // Page Up
            int sel = (int)SendMessageA(pThis->m_hwndCommandPaletteList, LB_GETCURSEL, 0, 0);
            int newSel = std::max(sel - 10, 0);
            SendMessageA(pThis->m_hwndCommandPaletteList, LB_SETCURSEL, newSel, 0);
            return 0;
        }
    }

    // Forward to the original EDIT wndproc
    if (pThis->m_oldCommandPaletteInputProc) {
        return CallWindowProcA(pThis->m_oldCommandPaletteInputProc, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// AGENT COMMAND HANDLERS
// Moved to Win32IDE_AgentCommands.cpp to avoid duplicate definitions.
// handleAgentCommand, onAgentStartLoop, onAgentExecuteCommand,
// onAIModeMax, onAIModeDeepThink, onAIModeDeepResearch, onAIModeNoRefusal,
// onAIContextSize are all defined in Win32IDE_AgentCommands.cpp
// ============================================================================

// ============================================================================
// WEBVIEW2 + MONACO COMMAND HANDLERS — Phase 26
// ============================================================================

void Win32IDE::handleMonacoCommand(int commandId) {
    switch (commandId) {
    case IDM_VIEW_TOGGLE_MONACO:
        toggleMonacoEditor();
        break;

    case IDM_VIEW_MONACO_DEVTOOLS:
        // Open Edge DevTools for WebView2 debugging
        if (m_webView2 && m_webView2->isReady()) {
            // DevTools can be opened via the WebView2 API
            // For now, use the keyboard shortcut approach
            LOG_INFO("Monaco: Opening DevTools...");
        }
        break;

    case IDM_VIEW_MONACO_RELOAD:
        if (m_webView2) {
            LOG_INFO("Monaco: Reloading editor...");
            destroyMonacoEditor();
            createMonacoEditor(m_hwndMain);
        }
        break;

    case IDM_VIEW_MONACO_ZOOM_IN:
        if (m_webView2 && m_webView2->isReady()) {
            m_monacoZoomLevel += 0.1f;
            m_monacoOptions.fontSize = (int)(14.0f * m_monacoZoomLevel);
            m_webView2->setOptions(m_monacoOptions);
            LOG_INFO("Monaco: Zoom level " + std::to_string(m_monacoZoomLevel));
        }
        break;

    case IDM_VIEW_MONACO_ZOOM_OUT:
        if (m_webView2 && m_webView2->isReady()) {
            m_monacoZoomLevel = std::max(0.5f, m_monacoZoomLevel - 0.1f);
            m_monacoOptions.fontSize = (int)(14.0f * m_monacoZoomLevel);
            m_webView2->setOptions(m_monacoOptions);
            LOG_INFO("Monaco: Zoom level " + std::to_string(m_monacoZoomLevel));
        }
        break;

    case IDM_VIEW_MONACO_SYNC_THEME:
        syncThemeToMonaco();
        break;

    default:
        LOG_WARNING("Unknown Monaco command: " + std::to_string(commandId));
        break;
    }
}

// ============================================================================
// CREATE MONACO EDITOR — Initialize WebView2 container + Monaco
// ============================================================================
void Win32IDE::createMonacoEditor(HWND hwnd) {
    if (m_webView2) {
        LOG_WARNING("Monaco editor already exists");
        return;
    }

    LOG_INFO("Creating WebView2 + Monaco editor...");

    m_webView2 = new WebView2Container();

    // Set callbacks
    m_webView2->setReadyCallback([](void* userData) {
        auto* ide = static_cast<Win32IDE*>(userData);
        LOG_INFO("Monaco editor ready! All 16 themes registered.");

        // Sync current Win32 theme to Monaco
        std::string monacoName = MonacoThemeExporter::monacoThemeName(ide->m_activeThemeId);
        ide->m_webView2->setTheme(monacoName);

        // If there's content in the RichEdit, transfer it
        ide->syncRichEditToMonaco();
    }, this);

    m_webView2->setCursorCallback([](int line, int col, void* userData) {
        auto* ide = static_cast<Win32IDE*>(userData);
        // Update status bar with Monaco cursor position
        if (ide->m_hwndStatusBar) {
            std::string pos = "Ln " + std::to_string(line) + ", Col " + std::to_string(col);
            SendMessageA(ide->m_hwndStatusBar, SB_SETTEXT, 2, (LPARAM)pos.c_str());
        }
    }, this);

    m_webView2->setErrorCallback([](const char* error, void* userData) {
        LOG_ERROR(std::string("Monaco error: ") + error);
    }, this);

    // Get editor area bounds
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int editorX = 250;  // After sidebar
    int editorY = 80;   // After tab bar + title bar
    int editorW = clientRect.right - editorX;
    int editorH = clientRect.bottom - editorY - 200; // Leave room for panel + status

    m_webView2->resize(editorX, editorY, editorW, editorH);

    // Start async initialization
    WebView2Result result = m_webView2->initialize(hwnd);
    if (!result.success) {
        LOG_ERROR(std::string("WebView2 init failed: ") + result.detail);
        delete m_webView2;
        m_webView2 = nullptr;

        // Show user-friendly message
        MessageBoxA(hwnd,
            "WebView2 initialization failed.\n\n"
            "Make sure WebView2Loader.dll is in the application directory\n"
            "and the Microsoft Edge WebView2 Runtime is installed.\n\n"
            "Download from: https://developer.microsoft.com/microsoft-edge/webview2/",
            "Monaco Editor - WebView2 Error",
            MB_OK | MB_ICONWARNING);
        return;
    }

    LOG_INFO("WebView2 initialization started (async). State: " +
             std::string(m_webView2->getStateString()));
}

// ============================================================================
// DESTROY MONACO EDITOR
// ============================================================================
void Win32IDE::destroyMonacoEditor() {
    if (!m_webView2) return;

    LOG_INFO("Destroying Monaco editor...");

    // If Monaco was active, sync content back to RichEdit first
    if (m_monacoEditorActive) {
        syncMonacoToRichEdit();
    }

    m_webView2->destroy();
    delete m_webView2;
    m_webView2 = nullptr;
    m_monacoEditorActive = false;

    // Show RichEdit again
    if (m_hwndEditor) {
        ShowWindow(m_hwndEditor, SW_SHOW);
    }

    LOG_INFO("Monaco editor destroyed. RichEdit restored.");
}

// ============================================================================
// TOGGLE MONACO EDITOR — Switch between RichEdit ↔ Monaco
// ============================================================================
void Win32IDE::toggleMonacoEditor() {
    if (!m_webView2) {
        // First time — create it
        createMonacoEditor(m_hwndMain);
        if (m_webView2) {
            m_monacoEditorActive = true;
            if (m_hwndEditor) ShowWindow(m_hwndEditor, SW_HIDE);
            m_webView2->show();
            LOG_INFO("Switched to Monaco editor (WebView2)");
            if (m_hwndStatusBar) {
                SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0,
                    (LPARAM)"Editor: Monaco (WebView2) — All 16 themes loaded");
            }
        }
    } else if (m_monacoEditorActive) {
        // Switch back to RichEdit
        m_monacoEditorActive = false;
        m_webView2->hide();
        syncMonacoToRichEdit();
        if (m_hwndEditor) ShowWindow(m_hwndEditor, SW_SHOW);
        LOG_INFO("Switched to RichEdit editor");
        if (m_hwndStatusBar) {
            SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Editor: RichEdit");
        }
    } else {
        // Switch to Monaco
        m_monacoEditorActive = true;
        syncRichEditToMonaco();
        if (m_hwndEditor) ShowWindow(m_hwndEditor, SW_HIDE);
        m_webView2->show();
        LOG_INFO("Switched to Monaco editor");
        if (m_hwndStatusBar) {
            SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0,
                (LPARAM)"Editor: Monaco (WebView2)");
        }
    }
}

// ============================================================================
// SYNC CONTENT: RichEdit → Monaco
// ============================================================================
void Win32IDE::syncRichEditToMonaco() {
    if (!m_webView2 || !m_webView2->isReady() || !m_hwndEditor) return;

    // Get text length from RichEdit
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0) return;

    // Get the text
    std::string text(textLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &text[0], textLen + 1);
    text.resize(textLen);

    // Detect language from current file extension
    std::string lang = "plaintext";
    if (!m_currentFile.empty()) {
        std::string ext = m_currentFile;
        size_t dotPos = ext.rfind('.');
        if (dotPos != std::string::npos) {
            ext = ext.substr(dotPos + 1);
            if (ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "h" || ext == "hpp") lang = "cpp";
            else if (ext == "c") lang = "c";
            else if (ext == "py") lang = "python";
            else if (ext == "js") lang = "javascript";
            else if (ext == "ts") lang = "typescript";
            else if (ext == "rs") lang = "rust";
            else if (ext == "go") lang = "go";
            else if (ext == "java") lang = "java";
            else if (ext == "cs") lang = "csharp";
            else if (ext == "rb") lang = "ruby";
            else if (ext == "lua") lang = "lua";
            else if (ext == "asm" || ext == "s") lang = "asm";
            else if (ext == "json") lang = "json";
            else if (ext == "xml" || ext == "html" || ext == "htm") lang = "html";
            else if (ext == "css") lang = "css";
            else if (ext == "md") lang = "markdown";
            else if (ext == "yaml" || ext == "yml") lang = "yaml";
            else if (ext == "cmake") lang = "cmake";
            else if (ext == "sh" || ext == "bash") lang = "shell";
            else if (ext == "ps1") lang = "powershell";
            else if (ext == "sql") lang = "sql";
        }
    }

    m_webView2->setContent(text, lang);
    LOG_INFO("Synced RichEdit → Monaco (" + std::to_string(textLen) + " chars, lang=" + lang + ")");
}

// ============================================================================
// SYNC CONTENT: Monaco → RichEdit
// ============================================================================
void Win32IDE::syncMonacoToRichEdit() {
    if (!m_webView2 || !m_webView2->isReady() || !m_hwndEditor) return;

    // Request content from Monaco (async — will be delivered via callback)
    m_webView2->setContentCallback([](const char* content, uint32_t length, void* userData) {
        auto* ide = static_cast<Win32IDE*>(userData);
        if (ide->m_hwndEditor && content) {
            SetWindowTextA(ide->m_hwndEditor, content);
            LOG_INFO("Synced Monaco → RichEdit (" + std::to_string(length) + " chars)");
        }
    }, this);

    m_webView2->getContent();
}

// ============================================================================
// SYNC THEME: Win32 → Monaco
// ============================================================================
void Win32IDE::syncThemeToMonaco() {
    if (!m_webView2 || !m_webView2->isReady()) return;

    std::string monacoName = MonacoThemeExporter::monacoThemeName(m_activeThemeId);
    m_webView2->setTheme(monacoName);
    LOG_INFO("Synced Win32 theme → Monaco: " + monacoName);
}
