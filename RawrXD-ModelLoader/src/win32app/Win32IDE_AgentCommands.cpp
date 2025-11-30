// Agent menu implementation for Win32IDE
// Implements all agentic framework menu commands and integrations

#include "Win32IDE.h"
#include "Win32IDE_AgenticBridge.h"
#include "IDELogger.h"
#include <sstream>

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
    
    // Show model configuration dialog
    std::stringstream config;
    config << "Current Agent Configuration:\n\n";
    config << "Model: " << m_agenticBridge->GetCurrentModel() << "\n\n";
    config << "Available models:\n";
    config << "- bigdaddyg-personalized-agentic:latest (Default)\n";
    config << "- codestral:latest (Code-focused)\n";
    config << "- llama3.3:latest (General purpose)\n\n";
    config << "Enter new model name (or Cancel to keep current):";
    
    char newModel[256] = {0};
    strcpy_s(newModel, m_agenticBridge->GetCurrentModel().c_str());
    
    // For now, show simple message box (can be enhanced with proper dialog later)
    int result = MessageBoxA(m_hwndMain, config.str().c_str(), "Agent Model Configuration", MB_OKCANCEL | MB_ICONINFORMATION);
    
    if (result == IDOK) {
        // TODO: Add actual dialog for model selection
        appendToOutput("Model configuration dialog - TODO: implement selection UI\n", "Output", OutputSeverity::Info);
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
