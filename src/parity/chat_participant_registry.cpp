// chat_participant_registry.cpp - Implementation + stock participants
#include "chat_participant_registry.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace rawrxd::parity {

namespace {
std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower((unsigned char)c));
    return s;
}
} // namespace

ChatParticipantRegistry::ChatParticipantRegistry() {
    // Default (empty) participant: echoes text with basic context framing.
    register_participant("", [](const ChatTurn& t) {
        ChatReply r; r.ok = true; r.text = t.user_text; return r;
    }, "default conversation");

    register_participant("workspace", [](const ChatTurn& t) {
        ChatReply r; r.ok = true;
        std::ostringstream os;
        os << "[@workspace] root=" << (t.workspace_root.empty() ? "<none>" : t.workspace_root)
           << " active=" << (t.active_file.empty() ? "<none>" : t.active_file)
           << "\n" << t.user_text;
        r.text = os.str();
        r.follow_ups = { "Search for TODOs", "Show recently changed files", "Run tests" };
        return r;
    }, "workspace-scoped Q&A");

    register_participant("vscode", [](const ChatTurn& t) {
        ChatReply r; r.ok = true;
        r.text = "[@vscode] editor-help: " + t.user_text;
        r.follow_ups = { "How do I change theme?", "How do I configure keybindings?" };
        return r;
    }, "editor / IDE help");

    register_participant("terminal", [](const ChatTurn& t) {
        ChatReply r; r.ok = true;
        r.text = "[@terminal] shell-help: " + t.user_text;
        r.follow_ups = { "Explain last command", "Fix last error" };
        return r;
    }, "shell + terminal help");

    register_participant("git", [](const ChatTurn& t) {
        ChatReply r; r.ok = true;
        r.text = "[@git] " + t.user_text;
        r.follow_ups = { "Summarize staged changes", "Write a commit message" };
        return r;
    }, "git operations");
}

bool ChatParticipantRegistry::register_participant(const std::string& name,
                                                   ChatParticipantFn fn,
                                                   const std::string& description) {
    if (!fn) return false;
    std::lock_guard lk(mu_);
    participants_[lower(name)] = Entry{ std::move(fn), description };
    return true;
}

bool ChatParticipantRegistry::has(const std::string& name) const {
    std::lock_guard lk(mu_);
    return participants_.find(lower(name)) != participants_.end();
}

std::pair<std::string, std::string> ChatParticipantRegistry::extract_participant(std::string_view line) {
    std::size_t i = 0;
    while (i < line.size() && std::isspace((unsigned char)line[i])) ++i;
    if (i >= line.size() || line[i] != '@') return { "", std::string(line) };
    std::size_t s = ++i;
    while (i < line.size() && (std::isalnum((unsigned char)line[i]) || line[i] == '_' || line[i] == '-')) ++i;
    std::string name(line.substr(s, i - s));
    while (i < line.size() && std::isspace((unsigned char)line[i])) ++i;
    return { lower(std::move(name)), std::string(line.substr(i)) };
}

ChatReply ChatParticipantRegistry::dispatch(const ChatTurn& turn) const {
    ChatParticipantFn fn;
    {
        std::lock_guard lk(mu_);
        auto it = participants_.find(lower(turn.participant));
        if (it == participants_.end()) {
            ChatReply r; r.ok = false;
            r.error = "unknown participant: @" + turn.participant;
            return r;
        }
        fn = it->second.fn;
    }
    try { return fn(turn); }
    catch (const std::exception& e) { ChatReply r; r.ok = false; r.error = e.what(); return r; }
    catch (...) { ChatReply r; r.ok = false; r.error = "participant exception"; return r; }
}

std::vector<std::pair<std::string, std::string>> ChatParticipantRegistry::list() const {
    std::lock_guard lk(mu_);
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(participants_.size());
    for (const auto& [n, e] : participants_) out.emplace_back(n, e.description);
    std::sort(out.begin(), out.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    return out;
}

} // namespace rawrxd::parity
