// chat_participant_registry.h - @workspace / @vscode / @terminal participants
// Feature 10/15 (Copilot parity).
#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace rawrxd::parity {

struct ChatTurn {
    std::string participant;       // "" = default
    std::string user_text;
    std::vector<std::string> mentions;
    std::string workspace_root;
    std::string active_file;
};

struct ChatReply {
    bool ok{false};
    std::string text;
    std::vector<std::string> follow_ups;  // suggested next prompts
    std::string error;
};

using ChatParticipantFn = std::function<ChatReply(const ChatTurn&)>;

class ChatParticipantRegistry {
public:
    ChatParticipantRegistry();

    bool register_participant(const std::string& name, ChatParticipantFn fn,
                              const std::string& description = "");

    bool has(const std::string& name) const;

    // Parse a user line; extracts a leading @participant if present.
    static std::pair<std::string, std::string> extract_participant(std::string_view line);

    // Dispatch based on turn.participant.
    ChatReply dispatch(const ChatTurn& turn) const;

    std::vector<std::pair<std::string, std::string>> list() const;

private:
    struct Entry { ChatParticipantFn fn; std::string description; };

    mutable std::mutex mu_;
    std::unordered_map<std::string, Entry> participants_;
};

} // namespace rawrxd::parity
