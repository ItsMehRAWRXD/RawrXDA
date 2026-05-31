// slash_command_processor.h - /fix, /explain, /tests, /doc, /optimize command router
// Feature 9/15 (Copilot parity).
#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace rawrxd::parity {

struct SlashInvocation {
    std::string command;           // e.g. "fix"
    std::string argument;          // free-form argument string
    std::vector<std::string> mentions; // @-references extracted (e.g. @workspace)
    std::string original;          // raw original line
};

struct SlashResult {
    bool        ok{false};
    std::string body;              // rendered response
    std::string error;
};

using SlashHandlerFn = std::function<SlashResult(const SlashInvocation&)>;

class SlashCommandProcessor {
public:
    SlashCommandProcessor();

    // Register / override a handler. Returns true on success.
    bool register_command(const std::string& name, SlashHandlerFn fn,
                          const std::string& help = "");

    bool has_command(const std::string& name) const;
    std::vector<std::pair<std::string, std::string>> list_commands() const;

    // Parse a line beginning with `/` into an invocation. Returns nullopt for non-slash lines.
    static std::optional<SlashInvocation> parse(std::string_view line);

    // Execute an invocation against the registered handler.
    SlashResult dispatch(const SlashInvocation& inv) const;

    // Convenience: parse + dispatch in one call.
    SlashResult execute(std::string_view line) const;

private:
    struct Entry { SlashHandlerFn fn; std::string help; };

    mutable std::mutex mu_;
    std::unordered_map<std::string, Entry> handlers_;
};

} // namespace rawrxd::parity
