// ChatPanelModelCaller.cpp — Verified ModelCaller connection for Chat Panel
// Uses the real static ModelCaller API (ai_model_caller.h)

#include "ChatPanelModelCaller.h"
#include "ai_model_caller.h"
#include <sstream>

namespace RawrXD {

ChatPanelModelCaller::ChatPanelModelCaller(IChatPanel* chatPanel)
    : m_chatPanel(chatPanel), m_initialized(false) {
}

bool ChatPanelModelCaller::Initialize(const std::string& modelEndpoint) {
    if (m_initialized) return true;
    
    try {
        // Initialize the static ModelCaller
        if (!ModelCaller::Initialize(modelEndpoint)) {
            m_lastError = "ModelCaller::Initialize failed for endpoint: " + modelEndpoint;
            return false;
        }
        
        // Set default system prompt if not already set
        if (m_systemPrompt.empty()) {
            m_systemPrompt = 
                "You are RawrXD, an AI coding assistant. "
                "You help with code completion, explanation, and refactoring. "
                "You have access to the current file context. "
                "Be concise and helpful.";
        }
        
        m_initialized = true;
        m_lastError.clear();
        
        return true;
    } catch (const std::exception& e) {
        m_lastError = std::string("Failed to initialize ModelCaller: ") + e.what();
        return false;
    }
}

void ChatPanelModelCaller::Shutdown() {
    ModelCaller::Shutdown();
    m_initialized = false;
}

void ChatPanelModelCaller::OnContextUpdate(const ContextFrame& frame) {
    if (!m_enabled || !m_initialized || !m_chatPanel) return;
    
    // Only update if context changed
    if (frame.version == m_lastVersion) return;
    m_lastVersion = frame.version;
    
    // Update chat panel with current context
    m_chatPanel->SetContextFile(frame.filePath);
    m_chatPanel->SetContextLanguage(frame.languageId);
}

void ChatPanelModelCaller::OnContextEvent(const ContextEvent& event) {
    if (!m_enabled || !m_initialized || !m_chatPanel) return;
    
    switch (event.type) {
        case ContextEvent::EDITOR_CHANGED:
            // Context changed, clear any stale responses
            break;
            
        case ContextEvent::AI_RESPONSE:
            // AI generated a response, display it
            if (event.payload) {
                auto* interaction = static_cast<AIInteraction*>(event.payload);
                HandleModelResponse(interaction->response);
            }
            break;
            
        default:
            break;
    }
}

void ChatPanelModelCaller::SetSystemPrompt(const std::string& prompt) {
    m_systemPrompt = prompt;
}

void ChatPanelModelCaller::SendToModel(const ContextFrame& frame, const std::string& userMessage) {
    if (!ModelCaller::IsReady()) {
        HandleModelError("ModelCaller not initialized");
        return;
    }
    
    // Build prompt with context
    std::string prompt = BuildPrompt(frame, userMessage);
    
    // Show typing indicator
    m_chatPanel->ShowTypingIndicator(true);
    
    // Use static ModelCaller::generateCode for chat responses
    try {
        std::string response = ModelCaller::generateCode(
            prompt,           // instruction
            frame.languageId, // fileType
            ""                // context (already in prompt)
        );
        
        m_chatPanel->ShowTypingIndicator(false);
        HandleModelResponse(response);
    } catch (const std::exception& e) {
        m_chatPanel->ShowTypingIndicator(false);
        HandleModelError(std::string("Model error: ") + e.what());
    }
}

std::string ChatPanelModelCaller::BuildPrompt(const ContextFrame& frame, const std::string& userMessage) {
    std::ostringstream prompt;
    
    // System prompt
    prompt << "System: " << m_systemPrompt << "\n\n";
    
    // File context
    if (!frame.filePath.empty()) {
        prompt << "Current file: " << frame.filePath << "\n";
        prompt << "Language: " << frame.languageId << "\n\n";
    }
    
    // Current line context
    std::string currentLine = frame.CurrentLine();
    if (!currentLine.empty()) {
        prompt << "Current line: " << currentLine << "\n\n";
    }
    
    // Recent symbols for context
    if (!frame.symbols.empty()) {
        prompt << "Visible symbols:\n";
        int count = 0;
        for (const auto& sym : frame.symbols) {
            if (count >= 10) break; // Limit to 10 symbols
            prompt << "  " << sym.kind << " " << sym.name << "\n";
            count++;
        }
        prompt << "\n";
    }
    
    // Diagnostics
    if (!frame.diagnostics.empty()) {
        prompt << "Current diagnostics:\n";
        int count = 0;
        for (const auto& diag : frame.diagnostics) {
            if (count >= 5) break; // Limit to 5 diagnostics
            prompt << "  [" << diag.severity << "] " << diag.message << "\n";
            count++;
        }
        prompt << "\n";
    }
    
    // User message
    prompt << "User: " << userMessage << "\n\n";
    prompt << "Assistant: ";
    
    return prompt.str();
}

void ChatPanelModelCaller::HandleModelResponse(const std::string& response) {
    if (!m_chatPanel) return;
    
    // Add AI message to chat panel
    m_chatPanel->AppendAIMessage(response);
    
    // Clear any error state
    m_lastError.clear();
}

void ChatPanelModelCaller::HandleModelError(const std::string& error) {
    if (!m_chatPanel) return;
    
    m_lastError = error;
    
    // Show error in chat panel
    m_chatPanel->AppendSystemMessage("Error: " + error);
}

} // namespace RawrXD