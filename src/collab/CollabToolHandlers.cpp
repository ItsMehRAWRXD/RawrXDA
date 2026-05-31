// ============================================================================
// CollabToolHandlers.cpp — Agent Tool Handlers for Collaboration
// ============================================================================

#ifdef RAWRXD_GOLD_BUILD
#include "CollabToolHandlers.h"

namespace RawrXD
{
namespace Agent
{

CollaborationManager* CollabToolHandlers::s_manager = nullptr;

void CollabToolHandlers::SetManager(CollaborationManager*) {}

static ToolCallResult goldCollabUnavailable()
{
    return ToolCallResult::Error("Collaboration tools are not linked in RawrXD_Gold", ToolOutcome::ExecutionError);
}

ToolCallResult CollabToolHandlers::HostSession(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::JoinSession(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::LeaveSession(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::ShareFile(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::EditSharedFile(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::ListParticipants(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::SendChat(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::CreateTerminal(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::TerminalInput(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::TerminalScrollback(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::ListTerminals(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::PairStart(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::PairSwapRoles(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::AISuggest(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::ReviewCode(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::GetStatus(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::SetPermission(const nlohmann::json&)
{
    return goldCollabUnavailable();
}
ToolCallResult CollabToolHandlers::KickParticipant(const nlohmann::json&)
{
    return goldCollabUnavailable();
}

nlohmann::json CollabToolHandlers::GetAllSchemas()
{
    return nlohmann::json::array();
}

}  // namespace Agent
}  // namespace RawrXD

#else

#include "CollabToolHandlers.h"
#include "CollaborationManager.h"

namespace RawrXD
{
namespace Agent
{

CollaborationManager* CollabToolHandlers::s_manager = nullptr;

void CollabToolHandlers::SetManager(CollaborationManager* mgr)
{
    s_manager = mgr;
}

// ---- Helper: ensure manager is wired ----
// Called from static member functions which have access to s_manager.
#define CHECK_MGR()                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!s_manager)                                                                                                \
            return ToolCallResult::Error("Collaboration manager not initialized", ToolOutcome::ExecutionError);        \
    } while (0)

// ============================================================================
// Session lifecycle
// ============================================================================

ToolCallResult CollabToolHandlers::HostSession(const nlohmann::json& args)
{
    CHECK_MGR();
    uint16_t port = args.value("port", (uint16_t)9876);
    std::string name = args.value("hostName", "Host");

    if (s_manager->hostSession(port, name))
    {
        nlohmann::json meta = {{"sessionToken", s_manager->getSessionToken()}, {"port", port}};
        return ToolCallResult::Ok("Live Share session hosted on port " + std::to_string(port) +
                                      ". Token: " + s_manager->getSessionToken(),
                                  meta);
    }
    return ToolCallResult::Error("Failed to host session on port " + std::to_string(port));
}

ToolCallResult CollabToolHandlers::JoinSession(const nlohmann::json& args)
{
    CHECK_MGR();
    std::string address = args.value("address", "127.0.0.1");
    uint16_t port = args.value("port", (uint16_t)9876);
    std::string name = args.value("userName", "Peer");

    if (s_manager->joinSession(address, port, name))
        return ToolCallResult::Ok("Joined session at " + address + ":" + std::to_string(port));
    return ToolCallResult::Error("Failed to join session");
}

ToolCallResult CollabToolHandlers::LeaveSession(const nlohmann::json& args)
{
    CHECK_MGR();
    (void)args;
    s_manager->leaveSession();
    return ToolCallResult::Ok("Left collaboration session");
}

// ============================================================================
// File sharing
// ============================================================================

ToolCallResult CollabToolHandlers::ShareFile(const nlohmann::json& args)
{
    CHECK_MGR();
    std::string path = args.value("path", "");
    std::string content = args.value("content", "");
    std::string lang = args.value("languageId", "");
    if (path.empty())
        return ToolCallResult::Error("path is required", ToolOutcome::ValidationFailed);

    if (s_manager->shareFile(path, content, lang))
        return ToolCallResult::Ok("Shared file: " + path);
    return ToolCallResult::Error("Failed to share file: " + path);
}

ToolCallResult CollabToolHandlers::EditSharedFile(const nlohmann::json& args)
{
    CHECK_MGR();
    std::string path = args.value("path", "");
    int position = args.value("position", 0);
    std::string text = args.value("text", "");
    int deleteLen = args.value("deleteLength", 0);
    if (path.empty())
        return ToolCallResult::Error("path is required", ToolOutcome::ValidationFailed);

    if (s_manager->editFile(path, position, text, deleteLen))
        return ToolCallResult::Ok("Edit applied to " + path);
    return ToolCallResult::Error("Failed to edit " + path);
}

// ============================================================================
// Participants
// ============================================================================

ToolCallResult CollabToolHandlers::ListParticipants(const nlohmann::json& /*args*/)
{
    CHECK_MGR();
    auto participants = s_manager->getParticipants();
    nlohmann::json arr = nlohmann::json::array();
    for (auto& p : participants)
    {
        arr.push_back({{"userId", p.userId},
                       {"displayName", p.displayName},
                       {"permission", (int)p.permission},
                       {"connected", p.connected}});
    }
    return ToolCallResult::Ok(arr.dump(2), {{"count", participants.size()}});
}

ToolCallResult CollabToolHandlers::SetPermission(const nlohmann::json& args)
{
    CHECK_MGR();
    std::string userId = args.value("userId", "");
    int perm = args.value("permission", 1);
    if (userId.empty())
        return ToolCallResult::Error("userId is required", ToolOutcome::ValidationFailed);

    if (s_manager->setPermission(userId, static_cast<CollabPermission>(perm)))
        return ToolCallResult::Ok("Permission updated for " + userId);
    return ToolCallResult::Error("Failed to set permission");
}

ToolCallResult CollabToolHandlers::KickParticipant(const nlohmann::json& args)
{
    CHECK_MGR();
    std::string userId = args.value("userId", "");
    if (userId.empty())
        return ToolCallResult::Error("userId is required", ToolOutcome::ValidationFailed);

    if (s_manager->kick(userId))
        return ToolCallResult::Ok("Kicked " + userId);
    return ToolCallResult::Error("Failed to kick " + userId);
}

// ============================================================================
// Chat
// ============================================================================

ToolCallResult CollabToolHandlers::SendChat(const nlohmann::json& args)
{
    CHECK_MGR();
    std::string message = args.value("message", "");
    if (message.empty())
        return ToolCallResult::Error("message is required", ToolOutcome::ValidationFailed);

    s_manager->sendChat(message);
    return ToolCallResult::Ok("Chat sent");
}

// ============================================================================
// Shared terminals
// ============================================================================

ToolCallResult CollabToolHandlers::CreateTerminal(const nlohmann::json& args)
{
    CHECK_MGR();
    std::string title = args.value("title", "");
    std::string shell = args.value("shellPath", "");
    std::string cwd = args.value("cwd", "");

    std::string id = s_manager->createSharedTerminal(title, shell, cwd);
    if (!id.empty())
        return ToolCallResult::Ok("Created shared terminal: " + id, {{"terminalId", id}});
    return ToolCallResult::Error("Failed to create shared terminal");
}

ToolCallResult CollabToolHandlers::TerminalInput(const nlohmann::json& args)
{
    CHECK_MGR();
    std::string id = args.value("terminalId", "");
    std::string input = args.value("input", "");
    if (id.empty())
        return ToolCallResult::Error("terminalId is required", ToolOutcome::ValidationFailed);

    if (s_manager->writeTerminalInput(id, input))
        return ToolCallResult::Ok("Input written to " + id);
    return ToolCallResult::Error("Failed to write to terminal " + id);
}

ToolCallResult CollabToolHandlers::TerminalScrollback(const nlohmann::json& args)
{
    CHECK_MGR();
    std::string id = args.value("terminalId", "");
    if (id.empty())
        return ToolCallResult::Error("terminalId is required", ToolOutcome::ValidationFailed);

    std::string sb = s_manager->getTerminalScrollback(id);
    return ToolCallResult::Ok(sb, {{"lines", std::count(sb.begin(), sb.end(), '\n')}});
}

ToolCallResult CollabToolHandlers::ListTerminals(const nlohmann::json& /*args*/)
{
    CHECK_MGR();
    auto terms = s_manager->listSharedTerminals();
    nlohmann::json arr = nlohmann::json::array();
    for (auto& t : terms)
    {
        arr.push_back(
            {{"terminalId", t.terminalId}, {"title", t.title}, {"alive", t.alive}, {"cols", t.cols}, {"rows", t.rows}});
    }
    return ToolCallResult::Ok(arr.dump(2), {{"count", terms.size()}});
}

// ============================================================================
// Pair programming
// ============================================================================

ToolCallResult CollabToolHandlers::PairStart(const nlohmann::json& /*args*/)
{
    CHECK_MGR();
    if (s_manager->startPairProgramming())
        return ToolCallResult::Ok("Pair programming mode activated");
    return ToolCallResult::Error("Failed to start pair programming (session not active?)");
}

ToolCallResult CollabToolHandlers::PairSwapRoles(const nlohmann::json& args)
{
    CHECK_MGR();
    std::string userA = args.value("userA", "");
    std::string userB = args.value("userB", "");
    if (userA.empty() || userB.empty())
        return ToolCallResult::Error("userA and userB are required", ToolOutcome::ValidationFailed);

    s_manager->swapPairRoles(userA, userB);
    return ToolCallResult::Ok("Roles swapped between " + userA + " and " + userB);
}

ToolCallResult CollabToolHandlers::AISuggest(const nlohmann::json& args)
{
    CHECK_MGR();
    std::string userId = args.value("userId", "");
    if (userId.empty())
        return ToolCallResult::Error("userId is required", ToolOutcome::ValidationFailed);

    s_manager->requestAISuggestion(userId);
    auto suggestions = s_manager->getAISuggestions(userId);

    nlohmann::json arr = nlohmann::json::array();
    for (auto& s : suggestions)
    {
        arr.push_back({{"title", s.title},
                       {"description", s.description},
                       {"codeSnippet", s.codeSnippet},
                       {"filePath", s.filePath},
                       {"line", s.line},
                       {"confidence", s.confidence}});
    }
    return ToolCallResult::Ok(arr.dump(2), {{"count", suggestions.size()}});
}

ToolCallResult CollabToolHandlers::ReviewCode(const nlohmann::json& args)
{
    CHECK_MGR();
    std::string file = args.value("filePath", "");
    int line = args.value("line", 0);
    std::string code = args.value("code", "");
    if (file.empty())
        return ToolCallResult::Error("filePath is required", ToolOutcome::ValidationFailed);

    auto review = s_manager->reviewCode(file, line, code);
    nlohmann::json result = {
        {"title", review.title}, {"description", review.description}, {"confidence", review.confidence}};
    return ToolCallResult::Ok(result.dump(2));
}

// ============================================================================
// Status / Diagnostics
// ============================================================================

ToolCallResult CollabToolHandlers::GetStatus(const nlohmann::json& /*args*/)
{
    CHECK_MGR();
    auto diag = s_manager->getDiagnostics();
    return ToolCallResult::Ok(diag.dump(2));
}

// ============================================================================
// Schema generation (OpenAI function-calling format)
// ============================================================================

nlohmann::json CollabToolHandlers::GetAllSchemas()
{
    nlohmann::json schemas = nlohmann::json::array();

    auto tool = [](const std::string& name, const std::string& desc, const nlohmann::json& params)
    {
        return nlohmann::json{{"type", "function"},
                              {"function", {{"name", name}, {"description", desc}, {"parameters", params}}}};
    };

    schemas.push_back(tool("collab_host_session", "Start hosting a Live Share collaboration session",
                           {{"type", "object"},
                            {"properties",
                             {{"port", {{"type", "integer"}, {"description", "WebSocket port (default 9876)"}}},
                              {"hostName", {{"type", "string"}, {"description", "Display name for the host"}}}}}}));

    schemas.push_back(tool("collab_join_session", "Join an existing Live Share session",
                           {{"type", "object"},
                            {"properties",
                             {{"address", {{"type", "string"}, {"description", "Host address"}}},
                              {"port", {{"type", "integer"}, {"description", "WebSocket port"}}},
                              {"userName", {{"type", "string"}, {"description", "Your display name"}}}}},
                            {"required", {"address", "port"}}}));

    schemas.push_back(tool("collab_leave_session", "Leave the current collaboration session",
                           {{"type", "object"}, {"properties", {}}}));

    schemas.push_back(
        tool("collab_share_file", "Share a file for collaborative editing in the Live Share session",
             {{"type", "object"},
              {"properties",
               {{"path", {{"type", "string"}, {"description", "Relative file path"}}},
                {"content", {{"type", "string"}, {"description", "Initial file content"}}},
                {"languageId", {{"type", "string"}, {"description", "Language identifier (e.g. cpp, python)"}}}}},
              {"required", {"path"}}}));

    schemas.push_back(
        tool("collab_edit_file", "Make an edit to a shared file (CRDT-synchronized)",
             {{"type", "object"},
              {"properties",
               {{"path", {{"type", "string"}, {"description", "Shared file path"}}},
                {"position", {{"type", "integer"}, {"description", "Character offset"}}},
                {"text", {{"type", "string"}, {"description", "Text to insert"}}},
                {"deleteLength", {{"type", "integer"}, {"description", "Characters to delete before insert"}}}}},
              {"required", {"path", "position"}}}));

    schemas.push_back(tool("collab_list_participants", "List all participants in the current session",
                           {{"type", "object"}, {"properties", {}}}));

    schemas.push_back(tool("collab_send_chat", "Send a chat message to all session participants",
                           {{"type", "object"},
                            {"properties", {{"message", {{"type", "string"}, {"description", "Chat message text"}}}}},
                            {"required", {"message"}}}));

    schemas.push_back(tool("collab_create_terminal", "Create a shared terminal visible to all participants",
                           {{"type", "object"},
                            {"properties",
                             {{"title", {{"type", "string"}, {"description", "Terminal tab title"}}},
                              {"shellPath", {{"type", "string"}, {"description", "Shell executable path"}}},
                              {"cwd", {{"type", "string"}, {"description", "Working directory"}}}}}}));

    schemas.push_back(tool("collab_terminal_input", "Write input to a shared terminal",
                           {{"type", "object"},
                            {"properties",
                             {{"terminalId", {{"type", "string"}, {"description", "Terminal identifier"}}},
                              {"input", {{"type", "string"}, {"description", "Input text/command"}}}}},
                            {"required", {"terminalId", "input"}}}));

    schemas.push_back(
        tool("collab_terminal_scrollback", "Get the scrollback buffer of a shared terminal",
             {{"type", "object"},
              {"properties", {{"terminalId", {{"type", "string"}, {"description", "Terminal identifier"}}}}},
              {"required", {"terminalId"}}}));

    schemas.push_back(tool("collab_list_terminals", "List all shared terminals in the session",
                           {{"type", "object"}, {"properties", {}}}));

    schemas.push_back(tool("collab_pair_start", "Activate pair programming mode with AI assistance",
                           {{"type", "object"}, {"properties", {}}}));

    schemas.push_back(tool("collab_pair_swap_roles", "Swap driver/navigator roles between two participants",
                           {{"type", "object"},
                            {"properties",
                             {{"userA", {{"type", "string"}, {"description", "First user ID"}}},
                              {"userB", {{"type", "string"}, {"description", "Second user ID"}}}}},
                            {"required", {"userA", "userB"}}}));

    schemas.push_back(tool("collab_ai_suggest", "Request an AI pair programming suggestion for a user",
                           {{"type", "object"},
                            {"properties", {{"userId", {{"type", "string"}, {"description", "Target user ID"}}}}},
                            {"required", {"userId"}}}));

    schemas.push_back(tool("collab_review_code", "AI-assisted code review at a specific location",
                           {{"type", "object"},
                            {"properties",
                             {{"filePath", {{"type", "string"}, {"description", "File to review"}}},
                              {"line", {{"type", "integer"}, {"description", "Line number"}}},
                              {"code", {{"type", "string"}, {"description", "Code snippet to review"}}}}},
                            {"required", {"filePath"}}}));

    schemas.push_back(tool("collab_set_permission",
                           "Set a participant's permission level (0=ReadOnly, 1=Editor, 2=Owner)",
                           {{"type", "object"},
                            {"properties",
                             {{"userId", {{"type", "string"}, {"description", "Participant user ID"}}},
                              {"permission", {{"type", "integer"}, {"description", "Permission level"}}}}},
                            {"required", {"userId", "permission"}}}));

    schemas.push_back(tool("collab_kick", "Remove a participant from the session",
                           {{"type", "object"},
                            {"properties", {{"userId", {{"type", "string"}, {"description", "User ID to remove"}}}}},
                            {"required", {"userId"}}}));

    schemas.push_back(tool("collab_status", "Get full collaboration diagnostics (session, terminals, pair programming)",
                           {{"type", "object"}, {"properties", {}}}));

    return schemas;
}

#undef CHECK_MGR

}  // namespace Agent
}  // namespace RawrXD

#endif  // RAWRXD_GOLD_BUILD
