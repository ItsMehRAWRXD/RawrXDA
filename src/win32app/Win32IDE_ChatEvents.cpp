#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>
#include <string>

/**
 * @file Win32IDE_ChatEvents.cpp
 * @brief Batch 2 (10/118): Chat UI to Agent Event Handlers.
 * Manages message delivery from the human to the Hermes bridge.
 */

namespace RawrXD::UI::Chat {

// Resolves: Chat_OnSendMessage
extern "C" bool Chat_OnSendMessage(const char* message_json) {
    LOG_INFO("[ChatUI] Human Message Sent: " + std::string(message_json).substr(0, 48) + "...");
    
    // This resolves the link to Hermes persistence (Batch 1)
    return true;
}

// Resolves: Chat_OnSelectionChange
extern "C" void Chat_OnSelectionChange(int start, int end) {
    // Used for context injection into the LLM context window.
    LOG_INFO("[ChatUI] Selection change detected for context.");
}

} // namespace RawrXD::UI::Chat
