// Agent menu implementation for Win32IDE
// Implements all agentic framework menu commands and integrations

#include "Win32IDE.h"
#include "Win32IDE_AgenticBridge.h"
#include "ModelConnection.h"
#include "IDELogger.h"
#include <sstream>
#include <algorithm>

// Initialize the Agentic Bridge
void Win32IDE::initializeAgenticBridge() {
    LOG_INFO("Initializing Agentic Bridge");
    
    if (!m_agenticBridge) {
        m_agenticBridge = std::make_unique<AgenticBridge>(this);
        
        // Set output callback to send agent responses to Copilot Chat
        m_agenticBridge->SetOutputCallback([this](const std::string& title, const std::string& content) {
            appendToOutput(title + ":\n" + content + "\n", "Output", OutputSeverity::Info);
            
            // Also send to Copilot Chat if available
            if (m_hwndCopilotChatOutput) {
                std::string formatted = "🤖 " + title + "\n" + content + "\n\n";
                SendMessageA(m_hwndCopilotChatOutput, EM_SETSEL, -1, -1);
                SendMessageA(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)formatted.c_str());
            }
        });
        
        // Initialize with default framework path
        if (m_agenticBridge->Initialize("", "bigdaddyg-personalized-agentic:latest")) {
            LOG_INFO("Agentic Bridge initialized successfully");
            appendToOutput("✅ Agentic Framework initialized\n", "Output", OutputSeverity::Info);
            
            // Initialize Autonomy Manager
            m_autonomyManager = std::make_unique<AutonomyManager>(m_agenticBridge.get());
        } else {
            LOG_ERROR("Failed to initialize Agentic Bridge");
            appendToOutput("❌ Failed to initialize Agentic Framework\n", "Errors", OutputSeverity::Error);
            MessageBoxA(m_hwndMain, 
                "Failed to initialize Agentic Framework.\nMake sure Agentic-Framework.ps1 is in the Powershield folder.", 
                "Agent Error", MB_OK | MB_ICONERROR);
        }
    }
}

// Start Agent Loop - multi-turn agentic conversation
void Win32IDE::onAgentStartLoop() {
    LOG_INFO("onAgentStartLoop called");
    
    if (!m_agenticBridge) {
        initializeAgenticBridge();
    }
    
    if (!m_agenticBridge || !m_agenticBridge->IsInitialized()) {
        MessageBoxA(m_hwndMain, "Agentic Framework not initialized", "Agent Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Show input dialog for user prompt
    char prompt[1024] = {0};
    if (DialogBoxParamA(m_hInstance, "AGENT_PROMPT_DLG", m_hwndMain, 
        [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> INT_PTR {
            switch (msg) {
                case WM_INITDIALOG: {
                    SetWindowTextA(GetDlgItem(hwnd, 101), "Enter your task for the agent:");
                    return TRUE;
                }
                case WM_COMMAND:
                    if (LOWORD(wp) == IDOK) {
                        GetDlgItemTextA(hwnd, 102, (char*)lp, 1024);
                        EndDialog(hwnd, IDOK);
                        return TRUE;
                    } else if (LOWORD(wp) == IDCANCEL) {
                        EndDialog(hwnd, IDCANCEL);
                        return TRUE;
                    }
                    break;
            }
            return FALSE;
        }, (LPARAM)prompt) != IDOK) {
        return;
    }
    
    // Fallback to simple input box if dialog fails
    if (strlen(prompt) == 0) {
        strcpy_s(prompt, "Analyze the current file and suggest improvements");
    }
    
    std::string promptStr(prompt);
    
    // Enrich the user prompt with language context
    promptStr = buildLanguageAwarePrompt(promptStr);
    
    // Start agent loop in background thread
    appendToOutput("🚀 Starting Agent Loop: " + promptStr + "\n", "Output", OutputSeverity::Info);
    
    std::thread([this, promptStr]() {
        if (m_agenticBridge->StartAgentLoop(promptStr, 10)) {
            LOG_INFO("Agent loop completed successfully");
        } else {
            LOG_ERROR("Agent loop failed");
        }
    }).detach();
}

// Execute single agent command
void Win32IDE::onAgentExecuteCommand() {
    LOG_INFO("onAgentExecuteCommand called");
    
    if (!m_agenticBridge) {
        initializeAgenticBridge();
    }
    
    if (!m_agenticBridge || !m_agenticBridge->IsInitialized()) {
        MessageBoxA(m_hwndMain, "Agentic Framework not initialized", "Agent Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Get command from Copilot Chat input
    if (m_hwndCopilotChatInput) {
        char input[2048] = {0};
        GetWindowTextA(m_hwndCopilotChatInput, input, sizeof(input));
        
        if (strlen(input) == 0) {
            MessageBoxA(m_hwndMain, "Enter a command in the Copilot Chat input box", "Agent", MB_OK | MB_ICONINFORMATION);
            return;
        }
        
        std::string command(input);
        appendToOutput("⚡ Executing Agent Command: " + command + "\n", "Output", OutputSeverity::Info);
        
        // Execute in background
        std::thread([this, command]() {
            AgentResponse response = m_agenticBridge->ExecuteAgentCommand(command);

            // Phase 4B: Choke Point 3 — hookAgentCommand after direct command execution
            FailureClassification cmdFailure = hookAgentCommand(response.content, command);
            if (cmdFailure.reason != AgentFailureType::None) {
                // Failure detected — attempt bounded retry
                AgentResponse retryResponse = executeWithBoundedRetry(command);
                if (!retryResponse.content.empty() &&
                    retryResponse.type != AgentResponseType::AGENT_ERROR) {
                    response = retryResponse;
                }
            }
            
            std::string output = "Agent Response:\n";
            output += "Type: " + std::to_string((int)response.type) + "\n";
            output += "Content: " + response.content + "\n";
            
            if (!response.toolName.empty()) {
                output += "Tool: " + response.toolName + "\n";
                output += "Args: " + response.toolArgs + "\n";
            }
            
            appendToOutput(output, "Output", OutputSeverity::Info);
        }).detach();
        
        // Clear input
        SetWindowTextA(m_hwndCopilotChatInput, "");
    } else {
        MessageBoxA(m_hwndMain, "Copilot Chat input not available", "Agent Error", MB_OK | MB_ICONERROR);
    }
}

// Configure AI model
void Win32IDE::onAgentConfigureModel() {
    LOG_INFO("onAgentConfigureModel called");
    
    if (!m_agenticBridge) {
        initializeAgenticBridge();
    }
    
    if (!m_agenticBridge) {
        return;
    }
    
    // Retrieve available models from the local model service (fallback to defaults)
    std::vector<std::string> availableModels;
    try {
        ModelConnection connection;
        availableModels = connection.getAvailableModels();
    } catch (...) {
        availableModels.clear();
    }

    if (availableModels.empty()) {
        availableModels = {
            "bigdaddyg-personalized-agentic:latest",
            "codestral:latest",
            "llama3.3:latest"
        };
    }

    // Ensure current model is present and pre-selected
    const std::string currentModel = m_agenticBridge->GetCurrentModel();
    if (std::find(availableModels.begin(), availableModels.end(), currentModel) == availableModels.end()) {
        availableModels.insert(availableModels.begin(), currentModel);
    }

    // Create a lightweight modal selection dialog (no resource template required)
    RECT parentRect{};
    GetWindowRect(m_hwndMain, &parentRect);

    const int dlgWidth = 420;
    const int dlgHeight = 200;
    const int dlgX = parentRect.left + ((parentRect.right - parentRect.left) - dlgWidth) / 2;
    const int dlgY = parentRect.top + ((parentRect.bottom - parentRect.top) - dlgHeight) / 2;

    EnableWindow(m_hwndMain, FALSE);

    HWND hwndDlg = CreateWindowExA(WS_EX_DLGMODALFRAME, "STATIC", "Select Agent Model",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        dlgX, dlgY, dlgWidth, dlgHeight, m_hwndMain, nullptr, m_hInstance, nullptr);

    if (!hwndDlg) {
        EnableWindow(m_hwndMain, TRUE);
        MessageBoxA(m_hwndMain, "Failed to create model selection dialog", "Agent", MB_OK | MB_ICONERROR);
        return;
    }

    CreateWindowExA(0, "STATIC", "Choose an available model:", WS_CHILD | WS_VISIBLE,
        10, 10, dlgWidth - 20, 20, hwndDlg, nullptr, m_hInstance, nullptr);

    HWND hwndCombo = CreateWindowExA(WS_EX_CLIENTEDGE, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        10, 35, dlgWidth - 40, 140, hwndDlg, (HMENU)1001, m_hInstance, nullptr);

    int selectedIndex = 0;
    for (size_t i = 0; i < availableModels.size(); ++i) {
        const auto& model = availableModels[i];
        SendMessageA(hwndCombo, CB_ADDSTRING, 0, (LPARAM)model.c_str());
        if (model == currentModel) {
            selectedIndex = static_cast<int>(i);
        }
    }
    SendMessageA(hwndCombo, CB_SETCURSEL, selectedIndex, 0);

    HWND hwndOk = CreateWindowExA(0, "BUTTON", "Use Model", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        dlgWidth - 200, dlgHeight - 50, 90, 30, hwndDlg, (HMENU)IDOK, m_hInstance, nullptr);
    HWND hwndCancel = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE,
        dlgWidth - 100, dlgHeight - 50, 80, 30, hwndDlg, (HMENU)IDCANCEL, m_hInstance, nullptr);

    bool accepted = false;
    std::string selectedModel = currentModel;

    MSG msg{};
    while (IsWindow(hwndDlg) && GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_COMMAND) {
            const WORD cmdId = LOWORD(msg.wParam);
            const WORD cmdCode = HIWORD(msg.wParam);
            HWND sender = (HWND)msg.hwnd;

            if (cmdId == IDOK && cmdCode == BN_CLICKED) {
                int sel = (int)SendMessageA(hwndCombo, CB_GETCURSEL, 0, 0);
                if (sel != CB_ERR) {
                    char buffer[256] = {0};
                    SendMessageA(hwndCombo, CB_GETLBTEXT, sel, (LPARAM)buffer);
                    selectedModel = buffer;
                    accepted = true;
                }
                DestroyWindow(hwndDlg);
                break;
            } else if (cmdId == IDCANCEL && cmdCode == BN_CLICKED) {
                DestroyWindow(hwndDlg);
                break;
            }
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(m_hwndMain, TRUE);
    SetForegroundWindow(m_hwndMain);

    if (accepted && !selectedModel.empty()) {
        m_agenticBridge->SetModel(selectedModel);
        appendToOutput("✅ Agent model set to: " + selectedModel + "\n", "Output", OutputSeverity::Info);
        MessageBoxA(m_hwndMain, ("Model updated to: " + selectedModel).c_str(), "Agent Model", MB_OK | MB_ICONINFORMATION);
    }
}

// View available agent tools
void Win32IDE::onAgentViewTools() {
    LOG_INFO("onAgentViewTools called");
    
    if (!m_agenticBridge) {
        initializeAgenticBridge();
    }
    
    if (!m_agenticBridge || !m_agenticBridge->IsInitialized()) {
        MessageBoxA(m_hwndMain, "Agentic Framework not initialized", "Agent Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    std::vector<std::string> tools = m_agenticBridge->GetAvailableTools();
    
    std::stringstream toolsList;
    toolsList << "Available Agent Tools:\n\n";
    
    for (const auto& tool : tools) {
        toolsList << "• " << tool << "\n";
    }
    
    toolsList << "\nThese tools can be invoked by the agent to perform tasks.\n";
    toolsList << "Example: TOOL:shell:{\"cmd\":\"Get-Process\"}";
    
    MessageBoxA(m_hwndMain, toolsList.str().c_str(), "Agent Tools", MB_OK | MB_ICONINFORMATION);
    
    // Also log to output
    appendToOutput(toolsList.str() + "\n", "Output", OutputSeverity::Info);
}

// View agent status
void Win32IDE::onAgentViewStatus() {
    LOG_INFO("onAgentViewStatus called");
    
    if (!m_agenticBridge) {
        appendToOutput("Agentic Bridge not initialized\n", "Output", OutputSeverity::Warning);
        MessageBoxA(m_hwndMain, "Agentic Framework not initialized.\nUse Agent > Start Loop to initialize.", "Agent Status", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    std::string status = m_agenticBridge->GetAgentStatus();
    appendToOutput("=== Agent Status ===\n" + status + "\n", "Output", OutputSeverity::Info);
    MessageBoxA(m_hwndMain, status.c_str(), "Agent Status", MB_OK | MB_ICONINFORMATION);
}

// Stop agent loop
void Win32IDE::onAgentStop() {
    LOG_INFO("onAgentStop called");
    
    if (!m_agenticBridge) {
        return;
    }
    
    if (m_agenticBridge->IsAgentLoopRunning()) {
        m_agenticBridge->StopAgentLoop();
        appendToOutput("🛑 Agent loop stopped\n", "Output", OutputSeverity::Warning);
        MessageBoxA(m_hwndMain, "Agent loop stopped", "Agent", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxA(m_hwndMain, "No agent loop is currently running", "Agent", MB_OK | MB_ICONINFORMATION);
    }
}

// ============================================================================
// KEYWORD: handleAgentCommand IMPLEMENTATION
// Routes all AI/Agent commands from the 4100-4300 range
// ============================================================================
void Win32IDE::handleAgentCommand(int commandId) {
    // Ensure agent bridge is ready (lazy init)
    if (!m_agenticBridge) initializeAgenticBridge();

    // Push current language context to the agent on every command dispatch
    if (m_agenticBridge) {
        m_agenticBridge->SetLanguageContext(getSyntaxLanguageName(), m_currentFile);
    }
 
    switch (commandId) {
        // --- Agent Execution ---
        case IDM_AGENT_START_LOOP:
            onAgentStartLoop();
            break;
        case IDM_AGENT_EXECUTE_CMD:
            onAgentExecuteCommand();
            break;
        case IDM_AGENT_CONFIGURE_MODEL:
            onAgentConfigureModel();
            break;
        case IDM_AGENT_VIEW_TOOLS:
            onAgentViewTools();
            break;
        case IDM_AGENT_VIEW_STATUS:
            onAgentViewStatus();
            break;
        case IDM_AGENT_STOP:
            onAgentStop();
            break;

        // --- Autonomy ---
        case IDM_AUTONOMY_TOGGLE:
            onAutonomyToggle();
            break;
        case IDM_AUTONOMY_START:
            onAutonomyStart();
            break;
        case IDM_AUTONOMY_STOP:
            onAutonomyStop();
            break;
        case IDM_AUTONOMY_SET_GOAL:
            onAutonomySetGoal();
            break;
        case IDM_AUTONOMY_STATUS:
            onAutonomyViewStatus();
            break;
        case IDM_AUTONOMY_MEMORY:
            onAutonomyViewMemory();
            break;

        // --- AI Options (Max Mode / Reasoning) ---
        case IDM_AI_MODE_MAX: {
            bool current = (GetMenuState(m_hMenu, IDM_AI_MODE_MAX, MF_BYCOMMAND) & MF_CHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_MODE_MAX, current ? MF_UNCHECKED : MF_CHECKED);
            if (m_agenticBridge) m_agenticBridge->SetMaxMode(!current);
            appendToOutput(std::string("Max Mode ") + (!current ? "ENABLED" : "DISABLED") + "\n", "Output", OutputSeverity::Info);
            break;
        }
        case IDM_AI_MODE_DEEP_THINK: {
            bool current = (GetMenuState(m_hMenu, IDM_AI_MODE_DEEP_THINK, MF_BYCOMMAND) & MF_CHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_MODE_DEEP_THINK, current ? MF_UNCHECKED : MF_CHECKED);
            if (m_agenticBridge) m_agenticBridge->SetDeepThinking(!current);
            appendToOutput(std::string("Deep Thinking (CoT) ") + (!current ? "ENABLED" : "DISABLED") + "\n", "Output", OutputSeverity::Info);
            break;
        }
        case IDM_AI_MODE_DEEP_RESEARCH: {
            bool current = (GetMenuState(m_hMenu, IDM_AI_MODE_DEEP_RESEARCH, MF_BYCOMMAND) & MF_CHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_MODE_DEEP_RESEARCH, current ? MF_UNCHECKED : MF_CHECKED);
            if (m_agenticBridge) m_agenticBridge->SetDeepResearch(!current);
            appendToOutput(std::string("Deep Research ") + (!current ? "ENABLED" : "DISABLED") + "\n", "Output", OutputSeverity::Info);
            break;
        }
        case IDM_AI_MODE_NO_REFUSAL: {
            bool current = (GetMenuState(m_hMenu, IDM_AI_MODE_NO_REFUSAL, MF_BYCOMMAND) & MF_CHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_MODE_NO_REFUSAL, current ? MF_UNCHECKED : MF_CHECKED);
            if (m_agenticBridge) m_agenticBridge->SetNoRefusal(!current);
            appendToOutput(std::string("No Refusal Mode ") + (!current ? "ENABLED" : "DISABLED") + "\n", "Output", OutputSeverity::Info);
            break;
        }

        // --- Context Window Size ---
        case IDM_AI_CONTEXT_4K:
        case IDM_AI_CONTEXT_32K:
        case IDM_AI_CONTEXT_64K:
        case IDM_AI_CONTEXT_128K:
        case IDM_AI_CONTEXT_256K:
        case IDM_AI_CONTEXT_512K:
        case IDM_AI_CONTEXT_1M: {
            int size = 4096;
            if (commandId == IDM_AI_CONTEXT_32K) size = 32768;
            if (commandId == IDM_AI_CONTEXT_64K) size = 65536;
            if (commandId == IDM_AI_CONTEXT_128K) size = 131072;
            if (commandId == IDM_AI_CONTEXT_256K) size = 262144;
            if (commandId == IDM_AI_CONTEXT_512K) size = 524288;
            if (commandId == IDM_AI_CONTEXT_1M) size = 1048576;
            
            m_inferenceConfig.contextWindow = size;
            if (m_agenticBridge) {
                // Determine string representation
                std::string s = "4k";
                if (commandId == IDM_AI_CONTEXT_32K) s = "32k";
                if (commandId == IDM_AI_CONTEXT_64K) s = "64k";
                if (commandId == IDM_AI_CONTEXT_128K) s = "128k";
                if (commandId == IDM_AI_CONTEXT_256K) s = "256k";
                if (commandId == IDM_AI_CONTEXT_512K) s = "512k";
                if (commandId == IDM_AI_CONTEXT_1M) s = "1m";
                m_agenticBridge->SetContextSize(s);
            }
            appendToOutput("Context window set to " + std::to_string(size) + " tokens\n", "Output", OutputSeverity::Info);
            
            // Uncheck all context items
            CheckMenuItem(m_hMenu, IDM_AI_CONTEXT_4K, MF_UNCHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_CONTEXT_32K, MF_UNCHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_CONTEXT_64K, MF_UNCHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_CONTEXT_128K, MF_UNCHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_CONTEXT_256K, MF_UNCHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_CONTEXT_512K, MF_UNCHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_CONTEXT_1M, MF_UNCHECKED);
            
            // Check active item
            CheckMenuItem(m_hMenu, commandId, MF_CHECKED);
            break;
        }

        // --- Reverse Engineering Commands ---
        case IDM_REVENG_ANALYZE:
            handleReverseEngineeringAnalyze();
            break;
        case IDM_REVENG_DISASM:
            handleReverseEngineeringDisassemble();
            break;
        case IDM_REVENG_DUMPBIN:
            handleReverseEngineeringDumpBin();
            break;
        case IDM_REVENG_COMPILE:
            handleReverseEngineeringCompile();
            break;
        case IDM_REVENG_COMPARE:
            handleReverseEngineeringCompare();
            break;
        case IDM_REVENG_DETECT_VULNS:
            handleReverseEngineeringDetectVulns();
            break;
        case IDM_REVENG_EXPORT_IDA:
            handleReverseEngineeringExportIDA();
            break;
        case IDM_REVENG_EXPORT_GHIDRA:
            handleReverseEngineeringExportGhidra();
            break;
        case IDM_REVENG_CFG:
            handleReverseEngineeringCFG();
            break;
        case IDM_REVENG_FUNCTIONS:
            handleReverseEngineeringFunctions();
            break;
        case IDM_REVENG_DEMANGLE:
            handleReverseEngineeringDemangle();
            break;

        default:
            appendToOutput("Unknown Agent Command ID: " + std::to_string(commandId) + "\n", "Debug", OutputSeverity::Warning);
            break;
    }
}

// ============================================================================
// AI MODE TOGGLE HANDLERS
// Pure state mutation + engine propagation + logging.
// These mirror the inline switch logic in handleAgentCommand but provide
// a clean callable API for programmatic use (e.g., from SidebarProcImpl).
// ============================================================================

void Win32IDE::onAIModeMax() {
    LOG_INFO("onAIModeMax toggled");
    bool current = (GetMenuState(m_hMenu, IDM_AI_MODE_MAX, MF_BYCOMMAND) & MF_CHECKED) != 0;
    bool newState = !current;
    CheckMenuItem(m_hMenu, IDM_AI_MODE_MAX, newState ? MF_CHECKED : MF_UNCHECKED);
    if (m_agenticBridge) m_agenticBridge->SetMaxMode(newState);
    if (m_nativeEngine) m_nativeEngine->SetMaxMode(newState);
    if (m_hwndChkMaxMode) SendMessage(m_hwndChkMaxMode, BM_SETCHECK, newState ? BST_CHECKED : BST_UNCHECKED, 0);
    appendToOutput(std::string("Max Mode ") + (newState ? "ENABLED" : "DISABLED") + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onAIModeDeepThink() {
    LOG_INFO("onAIModeDeepThink toggled");
    bool current = (GetMenuState(m_hMenu, IDM_AI_MODE_DEEP_THINK, MF_BYCOMMAND) & MF_CHECKED) != 0;
    bool newState = !current;
    CheckMenuItem(m_hMenu, IDM_AI_MODE_DEEP_THINK, newState ? MF_CHECKED : MF_UNCHECKED);
    if (m_agenticBridge) m_agenticBridge->SetDeepThinking(newState);
    if (m_nativeEngine) m_nativeEngine->SetDeepThinking(newState);
    if (m_hwndChkDeepThink) SendMessage(m_hwndChkDeepThink, BM_SETCHECK, newState ? BST_CHECKED : BST_UNCHECKED, 0);
    appendToOutput(std::string("Deep Thinking (CoT) ") + (newState ? "ENABLED" : "DISABLED") + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onAIModeDeepResearch() {
    LOG_INFO("onAIModeDeepResearch toggled");
    bool current = (GetMenuState(m_hMenu, IDM_AI_MODE_DEEP_RESEARCH, MF_BYCOMMAND) & MF_CHECKED) != 0;
    bool newState = !current;
    CheckMenuItem(m_hMenu, IDM_AI_MODE_DEEP_RESEARCH, newState ? MF_CHECKED : MF_UNCHECKED);
    if (m_agenticBridge) m_agenticBridge->SetDeepResearch(newState);
    if (m_nativeEngine) m_nativeEngine->SetDeepResearch(newState);
    if (m_hwndChkDeepResearch) SendMessage(m_hwndChkDeepResearch, BM_SETCHECK, newState ? BST_CHECKED : BST_UNCHECKED, 0);
    appendToOutput(std::string("Deep Research ") + (newState ? "ENABLED" : "DISABLED") + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onAIModeNoRefusal() {
    LOG_INFO("onAIModeNoRefusal toggled");
    bool current = (GetMenuState(m_hMenu, IDM_AI_MODE_NO_REFUSAL, MF_BYCOMMAND) & MF_CHECKED) != 0;
    bool newState = !current;
    CheckMenuItem(m_hMenu, IDM_AI_MODE_NO_REFUSAL, newState ? MF_CHECKED : MF_UNCHECKED);
    if (m_agenticBridge) m_agenticBridge->SetNoRefusal(newState);
    if (m_hwndChkNoRefusal) SendMessage(m_hwndChkNoRefusal, BM_SETCHECK, newState ? BST_CHECKED : BST_UNCHECKED, 0);
    appendToOutput(std::string("No Refusal Mode ") + (newState ? "ENABLED" : "DISABLED") + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onAIContextSize(int sizeEnum) {
    LOG_INFO("onAIContextSize: " + std::to_string(sizeEnum));
    
    // Map enum value to token count
    static const struct { int menuId; int tokens; const char* label; } contextMap[] = {
        { IDM_AI_CONTEXT_4K,   4096,    "4K"   },
        { IDM_AI_CONTEXT_32K,  32768,   "32K"  },
        { IDM_AI_CONTEXT_64K,  65536,   "64K"  },
        { IDM_AI_CONTEXT_128K, 131072,  "128K" },
        { IDM_AI_CONTEXT_256K, 262144,  "256K" },
        { IDM_AI_CONTEXT_512K, 524288,  "512K" },
        { IDM_AI_CONTEXT_1M,   1048576, "1M"   },
    };
    
    int tokens = 4096;
    std::string label = "4K";
    for (const auto& entry : contextMap) {
        if (entry.menuId == sizeEnum) {
            tokens = entry.tokens;
            label = entry.label;
            break;
        }
    }
    
    m_inferenceConfig.contextWindow = tokens;
    m_currentContextSize = static_cast<size_t>(tokens);
    
    if (m_agenticBridge) {
        std::string sizeStr = label;
        std::transform(sizeStr.begin(), sizeStr.end(), sizeStr.begin(), ::tolower);
        m_agenticBridge->SetContextSize(sizeStr);
    }
    if (m_nativeEngine) m_nativeEngine->SetContextSize(static_cast<size_t>(tokens));
    
    // Update menu checkmarks
    for (const auto& entry : contextMap) {
        CheckMenuItem(m_hMenu, entry.menuId, (entry.menuId == sizeEnum) ? MF_CHECKED : MF_UNCHECKED);
    }
    
    updateContextSliderLabel();
    appendToOutput("Context window set to " + label + " (" + std::to_string(tokens) + " tokens)\n", "Output", OutputSeverity::Info);
}

// ============================================================================
// AUTONOMY INITIALIZATION
// ============================================================================
void Win32IDE::initializeAutonomy() {
    LOG_INFO("Initializing Autonomy Manager");
    
    if (!m_agenticBridge) {
        appendToOutput("⚠️ Cannot initialize autonomy: Agentic Bridge not ready\n", "Output", OutputSeverity::Warning);
        return;
    }
    
    if (!m_autonomyManager) {
        m_autonomyManager = std::make_unique<AutonomyManager>(m_agenticBridge.get());
    }
    
    appendToOutput("✅ Autonomy Manager initialized\n", "Output", OutputSeverity::Info);
}

// ============================================================================
// MEMORY PLUGIN SYSTEM
// ============================================================================
void Win32IDE::loadMemoryPlugin(const std::string& path) {
    LOG_INFO("loadMemoryPlugin: " + path);
    
    if (path.empty()) {
        appendToOutput("⚠️ Empty plugin path\n", "Output", OutputSeverity::Warning);
        return;
    }
    
    // Attempt to load the plugin DLL and register with the native engine
    HMODULE hPlugin = LoadLibraryA(path.c_str());
    if (!hPlugin) {
        DWORD err = GetLastError();
        appendToOutput("❌ Failed to load memory plugin: " + path + " (error " + std::to_string(err) + ")\n", 
                       "Errors", OutputSeverity::Error);
        return;
    }
    
    // Look for the standard plugin factory export
    using CreatePluginFn = RawrXD::IMemoryPlugin* (*)();
    auto createFn = (CreatePluginFn)GetProcAddress(hPlugin, "CreateMemoryPlugin");
    if (!createFn) {
        appendToOutput("❌ Plugin missing CreateMemoryPlugin export: " + path + "\n", "Errors", OutputSeverity::Error);
        FreeLibrary(hPlugin);
        return;
    }
    
    auto* rawPlugin = createFn();
    if (rawPlugin && m_nativeEngine) {
        m_nativeEngine->RegisterMemoryPlugin(std::shared_ptr<RawrXD::IMemoryPlugin>(rawPlugin));
        appendToOutput("✅ Memory plugin loaded: " + path + "\n", "Output", OutputSeverity::Info);
    } else {
        appendToOutput("⚠️ Plugin created but no native engine available\n", "Output", OutputSeverity::Warning);
        delete rawPlugin;
        FreeLibrary(hPlugin);
    }
}