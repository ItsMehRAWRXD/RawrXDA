// Win32IDE_CopilotIntegration.cpp - Copilot streaming integration for IDE
#include "Win32IDE.h"
#include "CopilotStreaming.h"
#include <richedit.h>
#include <string>

// Global streaming instance
static CopilotStreaming* g_copilotStream = nullptr;
static std::string g_currentResponse;

// Initialize Copilot streaming
void Win32IDE::InitializeCopilotStreaming() {
    if (!g_copilotStream) {
        g_copilotStream = new CopilotStreaming(m_copilotChatOutput);
        
        // Set up callbacks
        g_copilotStream->OnToken([this](const std::string& token) {
            // Append token to output (thread-safe UI update)
            g_currentResponse += token;
            PostMessage(m_hwnd, WM_USER + 100, 0, 0); // Custom message to update UI
        });

        g_copilotStream->OnComplete([]() {
            g_currentResponse += "\\n\\n";
        });

        g_copilotStream->OnError([](const std::string& error) {
            MessageBoxA(NULL, error.c_str(), "Copilot Error", MB_ICONERROR);
        });
    }
}

// Handle Copilot send button
void Win32IDE::HandleCopilotSend() {
    if (!g_copilotStream) {
        InitializeCopilotStreaming();
    }

    // Get input text
    int inputLen = GetWindowTextLength(m_copilotChatInput);
    if (inputLen == 0) return;

    char* inputText = new char[inputLen + 1];
    GetWindowTextA(m_copilotChatInput, inputText, inputLen + 1);
    std::string prompt(inputText);
    delete[] inputText;

    // Get editor context (optional - current file content)
    std::string context;
    if (m_editorContent && m_editorContent[0] != '\\0') {
        context = "Current code:\\n" + std::string(m_editorContent);
    }

    // Add user message to output
    AppendCopilotMessage("User", prompt);

    // Clear input
    SetWindowTextA(m_copilotChatInput, "");

    // Reset response buffer
    g_currentResponse.clear();
    AppendCopilotMessage("Assistant", "");

    // Send to streaming proxy
    g_copilotStream->SendPrompt(prompt, context);
}

// Append message to chat output (formatted)
void Win32IDE::AppendCopilotMessage(const std::string& role, const std::string& content) {
    if (!m_copilotChatOutput) return;

    // Get current text length
    int currentLen = GetWindowTextLength(m_copilotChatOutput);
    
    // Build message with formatting
    std::string message;
    if (!content.empty()) {
        message = "\\n[" + role + "]\\n" + content + "\\n";
    } else {
        message = "\\n[" + role + "]\\n";
    }

    // Append to RichEdit control
    CHARRANGE cr;
    cr.cpMin = currentLen;
    cr.cpMax = currentLen;
    SendMessage(m_copilotChatOutput, EM_EXSETSEL, 0, (LPARAM)&cr);
    SendMessageA(m_copilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)message.c_str());

    // Auto-scroll to bottom
    SendMessage(m_copilotChatOutput, WM_VSCROLL, SB_BOTTOM, 0);
}

// Handle custom message for streaming token updates
void Win32IDE::HandleCopilotStreamUpdate() {
    if (!g_currentResponse.empty() && m_copilotChatOutput) {
        // Get current text
        int textLen = GetWindowTextLength(m_copilotChatOutput);
        
        // Find last "[Assistant]" position
        char* buffer = new char[textLen + 1];
        GetWindowTextA(m_copilotChatOutput, buffer, textLen + 1);
        std::string currentText(buffer);
        delete[] buffer;

        size_t lastAssistant = currentText.rfind("[Assistant]");
        if (lastAssistant != std::string::npos) {
            // Replace from [Assistant] onwards with new response
            CHARRANGE cr;
            cr.cpMin = lastAssistant + 11; // Length of "[Assistant]" + newline
            cr.cpMax = textLen;
            SendMessage(m_copilotChatOutput, EM_EXSETSEL, 0, (LPARAM)&cr);
            SendMessageA(m_copilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)g_currentResponse.c_str());
            
            // Auto-scroll
            SendMessage(m_copilotChatOutput, WM_VSCROLL, SB_BOTTOM, 0);
        }
    }
}

// Handle Copilot clear button
void Win32IDE::HandleCopilotClear() {
    if (m_copilotChatOutput) {
        SetWindowTextA(m_copilotChatOutput, "# Copilot Chat\\nReady for questions about your code.\\n");
    }
    if (g_copilotStream) {
        g_copilotStream->CancelStream();
    }
    g_currentResponse.clear();
}

// Cleanup on IDE shutdown
void Win32IDE::CleanupCopilotStreaming() {
    if (g_copilotStream) {
        delete g_copilotStream;
        g_copilotStream = nullptr;
    }
}
