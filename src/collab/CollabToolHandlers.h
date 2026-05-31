// ============================================================================
// CollabToolHandlers.h — Agent Tool Handlers for Collaboration Features
// ============================================================================
// Exposes collaboration operations as agentic tools:
//   collab_host_session      — Start hosting a Live Share session
//   collab_join_session      — Join an existing session
//   collab_leave_session     — Leave current session
//   collab_share_file        — Share a file for collaborative editing
//   collab_edit_file         — Make an edit to a shared file
//   collab_list_participants — List session participants
//   collab_send_chat         — Send a chat message
//   collab_create_terminal   — Create a shared terminal
//   collab_terminal_input    — Write to a shared terminal
//   collab_pair_start        — Start pair programming mode
//   collab_pair_swap_roles   — Swap driver/navigator roles
//   collab_ai_suggest        — Request AI pair programming suggestion
//   collab_review_code       — AI code review at cursor
//   collab_status            — Get collaboration diagnostics
//
// All handlers return ToolCallResult. No exceptions, factory results.
// ============================================================================

#pragma once

#include <nlohmann/json.hpp>
#include "agentic/ToolCallResult.h"

namespace RawrXD {

class CollaborationManager;

namespace Agent {

class CollabToolHandlers {
public:
    // Bind the collaboration manager instance (call once at startup)
    static void SetManager(CollaborationManager* mgr);

    // ---- Tool implementations ----
    static ToolCallResult HostSession(const nlohmann::json& args);
    static ToolCallResult JoinSession(const nlohmann::json& args);
    static ToolCallResult LeaveSession(const nlohmann::json& args);
    static ToolCallResult ShareFile(const nlohmann::json& args);
    static ToolCallResult EditSharedFile(const nlohmann::json& args);
    static ToolCallResult ListParticipants(const nlohmann::json& args);
    static ToolCallResult SendChat(const nlohmann::json& args);
    static ToolCallResult CreateTerminal(const nlohmann::json& args);
    static ToolCallResult TerminalInput(const nlohmann::json& args);
    static ToolCallResult TerminalScrollback(const nlohmann::json& args);
    static ToolCallResult ListTerminals(const nlohmann::json& args);
    static ToolCallResult PairStart(const nlohmann::json& args);
    static ToolCallResult PairSwapRoles(const nlohmann::json& args);
    static ToolCallResult AISuggest(const nlohmann::json& args);
    static ToolCallResult ReviewCode(const nlohmann::json& args);
    static ToolCallResult GetStatus(const nlohmann::json& args);
    static ToolCallResult SetPermission(const nlohmann::json& args);
    static ToolCallResult KickParticipant(const nlohmann::json& args);

    // ---- Schema (OpenAI function-calling format) ----
    static nlohmann::json GetAllSchemas();

private:
    static CollaborationManager* s_manager;
};

} // namespace Agent
} // namespace RawrXD
