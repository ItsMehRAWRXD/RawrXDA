// Agent menu implementation for Win32IDE
// Implements all agentic framework menu commands and integrations

#include "Win32IDE.h"
#include "Win32IDE_AgenticBridge.h"
#include "ModelConnection.h"
#include "IDELogger.h"
#include <sstream>

// Initialize the Agentic Bridge
void Win32IDE::initializeAgenticBridge() {
    // DISABLED: Model inference removed per user request
    // No model loading or agentic framework initialization on startup
    return;
}

// Start Agent Loop - multi-turn agentic conversation
void Win32IDE::onAgentStartLoop() {

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
    
    // Start agent loop in background thread
    appendToOutput("🚀 Starting Agent Loop: " + promptStr + "\n", "Output", OutputSeverity::Info);
    
    std::thread([this, promptStr]() {
        if (m_agenticBridge->StartAgentLoop(promptStr, 10)) {

        } else {

        }
    }).detach();
}

// Execute single agent command
void Win32IDE::onAgentExecuteCommand() {

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
