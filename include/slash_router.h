// === RAWRXD SLASH COMMAND ROUTER ===
// Single parser, 25 commands, zero deps

#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

enum class SlashCmd : uint8_t {
    NONE = 0,
    EXPLAIN, FIX, TEST, DOC, OPTIMIZE,
    REFACTOR, REVIEW, COMMIT, DIFF, MERGE,
    SEARCH, SYMBOL, TYPE, IMPORT, DEBUG,
    LINT, FORMAT, RENAME, EXTRACT, INLINE,
    SHELL, GIT, BUILD, DEPLOY, AGENT
};

struct SlashDispatch {
    SlashCmd    cmd;
    std::string args;
    std::string ctx;  // editor selection / file / project
};

class SlashRouter {
public:
    using Handler = std::function<void(const SlashDispatch&)>;
    
    SlashRouter();
    
    // Parse "/command args" → dispatch struct
    SlashDispatch Parse(const std::string& input);
    
    // Execute registered handler
    void Execute(const SlashDispatch& d);
    
    // Register custom handler
    void On(SlashCmd cmd, Handler h);
    
    // Get help text for all commands
    std::string Help() const;

private:
    std::unordered_map<SlashCmd, Handler> m_handlers;
    std::unordered_map<std::string, SlashCmd> m_aliases;
    
    void RegisterDefaults();
};