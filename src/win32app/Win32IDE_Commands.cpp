// Menu Command System Implementation for Win32IDE
// Centralized command routing with 25+ features

#include "Win32IDE.h"
#include "IDEConfig.h"
#include <commctrl.h>
#include <richedit.h>
#include <fstream>
#include <functional>
#include <algorithm>
#include <cctype>
#include <set>

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
            appendToOutput("[Policy] Active Policies:\n"
                           "  Use /policies in the REPL or GET /api/policies via HTTP.\n"
                           "  Policy engine governs retry limits, swarm-to-chain redirects, and adaptive thresholds.",
                           "General", OutputSeverity::Info);
            break;

        case 5028: // Generate Suggestions
            appendToOutput("[Policy] Generating policy suggestions...\n"
                           "  Use /suggest in the REPL or GET /api/policies/suggestions via HTTP.\n"
                           "  The engine analyzes recent heuristics to recommend new policies.",
                           "General", OutputSeverity::Info);
            break;

        case 5029: // Show Heuristics
            appendToOutput("[Policy] Computing heuristics...\n"
                           "  Use /heuristics in the REPL or GET /api/policies/heuristics via HTTP.\n"
                           "  Heuristics: failure rate, avg latency, retry success %, swarm fan-out.",
                           "General", OutputSeverity::Info);
            break;

        case 5030: // Export Policies
            appendToOutput("[Policy] Export:\n"
                           "  Use /policy export <file> in the REPL or GET /api/policies/export via HTTP.",
                           "General", OutputSeverity::Info);
            break;

        case 5031: // Import Policies
            appendToOutput("[Policy] Import:\n"
                           "  Use /policy import <file> in the REPL or POST /api/policies/import via HTTP.",
                           "General", OutputSeverity::Info);
            break;

        case 5032: // Policy Stats
            appendToOutput("[Policy] Stats:\n"
                           "  Use GET /api/policies/stats via HTTP.\n"
                           "  Shows accepted/rejected/pending counts and heuristic summary.",
                           "General", OutputSeverity::Info);
            break;

        // ================================================================
        // Explainability — Phase 8A (5033+)
        // ================================================================
        case 5033: // Session Explanation
            appendToOutput("[Explain] Session Explanation:\n"
                           "  Use /explain session in the REPL or GET /api/agents/explain via HTTP.\n"
                           "  Returns a narrative summary of agent activity, failures, and policy decisions.",
                           "General", OutputSeverity::Info);
            break;

        case 5034: // Trace Last Agent
            appendToOutput("[Explain] Trace Last Agent:\n"
                           "  Use /explain last in the REPL or GET /api/agents/explain?agent_id=<id> via HTTP.\n"
                           "  Builds a causal decision trace showing each event and its outcome.",
                           "General", OutputSeverity::Info);
            break;

        case 5035: // Export Snapshot
            appendToOutput("[Explain] Export Snapshot:\n"
                           "  Use /explain snapshot <file> in the REPL.\n"
                           "  Exports the full explainability snapshot (traces, attributions, narrative) to JSON.",
                           "General", OutputSeverity::Info);
            break;

        case 5036: // Explainability Stats
            appendToOutput("[Explain] Stats:\n"
                           "  Use GET /api/agents/explain/stats via HTTP.\n"
                           "  Shows event counts, agent spawns, failures, retries, policy firings.",
                           "General", OutputSeverity::Info);
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
            -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
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
            -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI"
        );
        if (s_listFont) SendMessage(m_hwndCommandPaletteList, WM_SETFONT, (WPARAM)s_listFont, TRUE);
        // Set item height for owner-draw
        SendMessageA(m_hwndCommandPaletteList, LB_SETITEMHEIGHT, 0, MAKELPARAM(24, 0));
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

    // Route the command — all command ranges handled by routeCommand
    routeCommand(commandId);

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
            s_normalFont = CreateFontA(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
        }
        if (!s_boldFont) {
            s_boldFont = CreateFontA(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
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
