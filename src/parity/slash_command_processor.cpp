// slash_command_processor.cpp - Implementation with sensible default handlers
#include "slash_command_processor.h"

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

SlashCommandProcessor::SlashCommandProcessor() {
    // Built-in help: returns the list of registered commands.
    register_command("help",
        [this](const SlashInvocation&) {
            SlashResult r; r.ok = true;
            std::ostringstream os;
            os << "Available slash commands:\n";
            for (const auto& [n, h] : list_commands())
                os << "  /" << n << (h.empty() ? "" : "  — " + h) << "\n";
            r.body = os.str();
            return r;
        }, "list commands");
    // Well-known defaults return a prompt template; callers can override with LLM-backed ones.
    auto templ = [](const char* intent) {
        return [intent](const SlashInvocation& inv) {
            SlashResult r; r.ok = true;
            std::ostringstream os;
            os << "[intent=" << intent << "] " << inv.argument;
            if (!inv.mentions.empty()) {
                os << " | mentions:";
                for (const auto& m : inv.mentions) os << " @" << m;
            }
            r.body = os.str();
            return r;
        };
    };
    register_command("fix",      templ("fix"),      "fix errors in current selection/file");
    register_command("explain",  templ("explain"),  "explain the selected code");
    register_command("tests",    templ("tests"),    "generate unit tests");
    register_command("doc",      templ("doc"),      "add documentation");
    register_command("optimize", templ("optimize"), "suggest performance optimizations");
    register_command("refactor", templ("refactor"), "refactor the selection");
    register_command("review",   templ("review"),   "perform a code review");
}

bool SlashCommandProcessor::register_command(const std::string& name,
                                             SlashHandlerFn fn,
                                             const std::string& help) {
    if (name.empty() || !fn) return false;
    std::lock_guard lk(mu_);
    handlers_[lower(name)] = Entry{ std::move(fn), help };
    return true;
}

bool SlashCommandProcessor::has_command(const std::string& name) const {
    std::lock_guard lk(mu_);
    return handlers_.find(lower(name)) != handlers_.end();
}

std::vector<std::pair<std::string, std::string>> SlashCommandProcessor::list_commands() const {
    std::lock_guard lk(mu_);
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(handlers_.size());
    for (const auto& [n, e] : handlers_) out.emplace_back(n, e.help);
    std::sort(out.begin(), out.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    return out;
}

std::optional<SlashInvocation> SlashCommandProcessor::parse(std::string_view line) {
    std::size_t i = 0;
    while (i < line.size() && std::isspace((unsigned char)line[i])) ++i;
    if (i >= line.size() || line[i] != '/') return std::nullopt;
    ++i;
    std::size_t cs = i;
    while (i < line.size() && (std::isalnum((unsigned char)line[i]) || line[i] == '_' || line[i] == '-'))
        ++i;
    if (i == cs) return std::nullopt;
    SlashInvocation inv;
    inv.command = lower(std::string(line.substr(cs, i - cs)));
    while (i < line.size() && std::isspace((unsigned char)line[i])) ++i;
    std::string rest(line.substr(i));
    inv.argument = rest;
    inv.original = std::string(line);
    // Extract @mentions.
    for (std::size_t k = 0; k < rest.size(); ++k) {
        if (rest[k] != '@') continue;
        std::size_t s = k + 1;
        while (s < rest.size() && (std::isalnum((unsigned char)rest[s]) || rest[s] == '_' || rest[s] == '-'))
            ++s;
        if (s > k + 1) inv.mentions.emplace_back(rest.substr(k + 1, s - k - 1));
        k = s;
    }
    return inv;
}

SlashResult SlashCommandProcessor::dispatch(const SlashInvocation& inv) const {
    SlashHandlerFn fn;
    {
        std::lock_guard lk(mu_);
        auto it = handlers_.find(lower(inv.command));
        if (it == handlers_.end()) {
            SlashResult r; r.ok = false;
            r.error = "unknown command: /" + inv.command;
            return r;
        }
        fn = it->second.fn;
    }
    try { return fn(inv); }
    catch (const std::exception& e) { SlashResult r; r.ok = false; r.error = e.what(); return r; }
    catch (...)                     { SlashResult r; r.ok = false; r.error = "handler exception"; return r; }
}

SlashResult SlashCommandProcessor::execute(std::string_view line) const {
    auto inv = parse(line);
    if (!inv) { SlashResult r; r.ok = false; r.error = "not a slash command"; return r; }
    return dispatch(*inv);
}

} // namespace rawrxd::parity
