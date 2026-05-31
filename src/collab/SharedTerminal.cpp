// ============================================================================
// SharedTerminal.cpp — Shared Terminal Multiplexer Implementation
// ============================================================================

#include "SharedTerminal.h"
#include "websocket_hub.h"
#include "LiveShareSession.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>
#include <chrono>

namespace RawrXD {

// ============================================================================
// Construction / Destruction
// ============================================================================

SharedTerminal::SharedTerminal() = default;

SharedTerminal::~SharedTerminal() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_terminals.clear();
}

void SharedTerminal::setTransport(WebSocketHub* hub) { m_hub = hub; }
void SharedTerminal::setSession(LiveShareSession* session) { m_session = session; }

// ============================================================================
// Terminal ID generation
// ============================================================================

std::string SharedTerminal::generateTerminalId() {
    return "term_" + std::to_string(m_nextId++);
}

// ============================================================================
// Terminal management
// ============================================================================

std::string SharedTerminal::createTerminal(const std::string& ownerId, const std::string& title,
                                            const std::string& shellPath, const std::string& cwd) {
    std::lock_guard<std::mutex> lk(m_mutex);

    auto id = generateTerminalId();
    auto term = std::make_unique<InternalTerminal>();
    term->info.terminalId = id;
    term->info.title = title.empty() ? ("Terminal " + std::to_string(m_nextId - 1)) : title;
    term->info.shellPath = shellPath.empty() ? "cmd.exe" : shellPath;
    term->info.cwd = cwd;
    term->info.ownerId = ownerId;
    term->info.alive = true;

    // Owner gets admin access
    term->userAccess[ownerId] = TerminalAccessLevel::Admin;

    m_terminals[id] = std::move(term);

    // Broadcast creation
    if (m_hub) {
        nlohmann::json msg = {
            {"type", "terminal_created"},
            {"terminalId", id},
            {"title", m_terminals[id]->info.title},
            {"shellPath", m_terminals[id]->info.shellPath},
            {"cwd", m_terminals[id]->info.cwd},
            {"ownerId", ownerId},
            {"cols", m_terminals[id]->info.cols},
            {"rows", m_terminals[id]->info.rows}
        };
        m_hub->broadcastMessage(msg.dump());
    }

    if (onTerminalCreated) onTerminalCreated(m_terminals[id]->info);
    return id;
}

bool SharedTerminal::destroyTerminal(const std::string& terminalId, const std::string& requesterId) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_terminals.find(terminalId);
    if (it == m_terminals.end()) return false;

    // Only owner or admin can destroy
    auto accessIt = it->second->userAccess.find(requesterId);
    bool isOwner = (it->second->info.ownerId == requesterId);
    bool isAdmin = (accessIt != it->second->userAccess.end() && accessIt->second == TerminalAccessLevel::Admin);
    if (!isOwner && !isAdmin) return false;

    it->second->info.alive = false;
    m_terminals.erase(it);

    if (m_hub) {
        nlohmann::json msg = {
            {"type", "terminal_destroyed"},
            {"terminalId", terminalId}
        };
        m_hub->broadcastMessage(msg.dump());
    }

    if (onTerminalDestroyed) onTerminalDestroyed(terminalId);
    return true;
}

std::vector<SharedTerminalInfo> SharedTerminal::listTerminals() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<SharedTerminalInfo> out;
    out.reserve(m_terminals.size());
    for (auto& [_, t] : m_terminals)
        out.push_back(t->info);
    return out;
}

bool SharedTerminal::terminalExists(const std::string& terminalId) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_terminals.count(terminalId) > 0;
}

// ============================================================================
// I/O
// ============================================================================

bool SharedTerminal::writeInput(const std::string& terminalId, const std::string& userId, const std::string& input) {
    if (input.empty() || input.size() > kMaxInputLength) return false;

    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_terminals.find(terminalId);
    if (it == m_terminals.end() || !it->second->info.alive) return false;

    // Check access
    auto access = getUserAccess(terminalId, userId);
    if (access == TerminalAccessLevel::None) return false;

    // Broadcast input to all observers
    if (m_hub) {
        nlohmann::json msg = {
            {"type", "terminal_input"},
            {"terminalId", terminalId},
            {"userId", userId},
            {"input", input}
        };
        m_hub->broadcastMessage(msg.dump());
    }

    // Invoke local handler to feed stdin of actual process
    if (onLocalInput) onLocalInput(terminalId, input);
    return true;
}

void SharedTerminal::feedOutput(const std::string& terminalId, const std::string& data) {
    if (data.empty()) return;

    TerminalOutputChunk chunk;
    chunk.terminalId = terminalId;
    chunk.data = data;
    chunk.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_terminals.find(terminalId);
        if (it == m_terminals.end()) return;

        // Append to scrollback (line-split for storage)
        std::istringstream iss(data);
        std::string line;
        while (std::getline(iss, line)) {
            it->second->scrollback.push_back(line);
            while (it->second->scrollback.size() > kMaxScrollbackLines)
                it->second->scrollback.pop_front();
        }
    }

    // Broadcast to all peers
    if (m_hub) {
        nlohmann::json msg = {
            {"type", "terminal_output"},
            {"terminalId", terminalId},
            {"data", data},
            {"timestamp", chunk.timestamp}
        };
        m_hub->broadcastMessage(msg.dump());
    }

    if (onOutputReceived) onOutputReceived(chunk);
}

std::string SharedTerminal::getScrollback(const std::string& terminalId, size_t maxLines) const {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_terminals.find(terminalId);
    if (it == m_terminals.end()) return {};

    std::ostringstream oss;
    auto& sb = it->second->scrollback;
    size_t start = (sb.size() > maxLines) ? sb.size() - maxLines : 0;
    for (size_t i = start; i < sb.size(); ++i) {
        if (i > start) oss << '\n';
        oss << sb[i];
    }
    return oss.str();
}

// ============================================================================
// Access control
// ============================================================================

bool SharedTerminal::setUserAccess(const std::string& terminalId, const std::string& userId, TerminalAccessLevel level) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_terminals.find(terminalId);
    if (it == m_terminals.end()) return false;
    it->second->userAccess[userId] = level;
    return true;
}

TerminalAccessLevel SharedTerminal::getUserAccess(const std::string& terminalId, const std::string& userId) const {
    // Note: caller must hold m_mutex if calling internally, or this is a standalone query
    auto it = m_terminals.find(terminalId);
    if (it == m_terminals.end()) return TerminalAccessLevel::None;

    auto ait = it->second->userAccess.find(userId);
    if (ait != it->second->userAccess.end()) return ait->second;

    // Fall back to terminal default
    return it->second->info.defaultAccess;
}

// ============================================================================
// Resize
// ============================================================================

bool SharedTerminal::resizeTerminal(const std::string& terminalId, const std::string& userId, int cols, int rows) {
    if (cols <= 0 || rows <= 0 || cols > 500 || rows > 200) return false;

    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_terminals.find(terminalId);
    if (it == m_terminals.end()) return false;

    auto access = getUserAccess(terminalId, userId);
    if (access != TerminalAccessLevel::Admin) return false;

    it->second->info.cols = cols;
    it->second->info.rows = rows;

    if (m_hub) {
        nlohmann::json msg = {
            {"type", "terminal_resize"},
            {"terminalId", terminalId},
            {"cols", cols},
            {"rows", rows}
        };
        m_hub->broadcastMessage(msg.dump());
    }
    return true;
}

// ============================================================================
// Sync to new peer
// ============================================================================

void SharedTerminal::syncAllTerminals(void* clientCtx) {
    if (!m_hub) return;
    std::lock_guard<std::mutex> lk(m_mutex);

    for (auto& [id, term] : m_terminals) {
        // Send terminal info
        nlohmann::json info = {
            {"type", "terminal_sync"},
            {"terminalId", term->info.terminalId},
            {"title", term->info.title},
            {"shellPath", term->info.shellPath},
            {"cwd", term->info.cwd},
            {"ownerId", term->info.ownerId},
            {"cols", term->info.cols},
            {"rows", term->info.rows},
            {"alive", term->info.alive}
        };
        m_hub->sendTextToClient(clientCtx, info.dump());

        // Send scrollback
        std::ostringstream sb;
        for (auto& line : term->scrollback)
            sb << line << '\n';
        if (!term->scrollback.empty()) {
            nlohmann::json scrollMsg = {
                {"type", "terminal_scrollback"},
                {"terminalId", id},
                {"data", sb.str()}
            };
            m_hub->sendTextToClient(clientCtx, scrollMsg.dump());
        }
    }
}

// ============================================================================
// Wire protocol handler
// ============================================================================

void SharedTerminal::handleMessage(const std::string& json, void* clientCtx) {
    nlohmann::json msg;
    try { msg = nlohmann::json::parse(json); }
    catch (...) { return; }

    std::string type = msg.value("type", "");

    if (type == "terminal_input") {
        std::string termId = msg.value("terminalId", "");
        std::string userId = msg.value("userId", "");
        std::string input = msg.value("input", "");
        writeInput(termId, userId, input);
    }
    else if (type == "terminal_create_request") {
        std::string userId = msg.value("userId", "");
        std::string title = msg.value("title", "");
        std::string shell = msg.value("shellPath", "");
        std::string cwd = msg.value("cwd", "");
        createTerminal(userId, title, shell, cwd);
    }
    else if (type == "terminal_destroy_request") {
        std::string termId = msg.value("terminalId", "");
        std::string userId = msg.value("userId", "");
        destroyTerminal(termId, userId);
    }
    else if (type == "terminal_resize") {
        std::string termId = msg.value("terminalId", "");
        std::string userId = msg.value("userId", "");
        int cols = msg.value("cols", 120);
        int rows = msg.value("rows", 30);
        resizeTerminal(termId, userId, cols, rows);
    }
}

// ============================================================================
// Diagnostics
// ============================================================================

nlohmann::json SharedTerminal::getDiagnostics() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    nlohmann::json diag;
    diag["terminalCount"] = m_terminals.size();
    nlohmann::json arr = nlohmann::json::array();
    for (auto& [id, t] : m_terminals) {
        arr.push_back({
            {"terminalId", t->info.terminalId},
            {"title", t->info.title},
            {"alive", t->info.alive},
            {"scrollbackLines", t->scrollback.size()},
            {"cols", t->info.cols},
            {"rows", t->info.rows}
        });
    }
    diag["terminals"] = arr;
    return diag;
}

} // namespace RawrXD
