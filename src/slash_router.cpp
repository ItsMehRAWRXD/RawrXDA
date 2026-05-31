// RAWRXD Slash Command Router Implementation
// 25 Command Parity Set with Zero Dependencies

#include "../include/slash_router.h"
#include <algorithm>
#include <cctype>

SlashRouter::SlashRouter() { 
    RegisterDefaults(); 
}

SlashDispatch SlashRouter::Parse(const std::string& input) {
    SlashDispatch d{SlashCmd::NONE, "", ""};
    if (input.empty() || input[0] != '/') return d;
    
    size_t space = input.find(' ');
    std::string cmdStr = (space == std::string::npos) 
        ? input.substr(1) 
        : input.substr(1, space - 1);
    
    // Normalize: lowercase
    for (auto& c : cmdStr) c = static_cast<char>(tolower(c));
    
    auto it = m_aliases.find(cmdStr);
    if (it != m_aliases.end()) {
        d.cmd = it->second;
        d.args = (space == std::string::npos) ? "" : input.substr(space + 1);
    }
    return d;
}

void SlashRouter::Execute(const SlashDispatch& d) {
    auto it = m_handlers.find(d.cmd);
    if (it != m_handlers.end()) it->second(d);
}

void SlashRouter::On(SlashCmd cmd, Handler h) {
    m_handlers[cmd] = std::move(h);
}

void SlashRouter::RegisterDefaults() {
    // Core 8 (Cursor/Copilot baseline)
    m_aliases["explain"]   = SlashCmd::EXPLAIN;
    m_aliases["fix"]       = SlashCmd::FIX;
    m_aliases["test"]      = SlashCmd::TEST;
    m_aliases["doc"]       = SlashCmd::DOC;
    m_aliases["optimize"]  = SlashCmd::OPTIMIZE;
    m_aliases["refactor"]  = SlashCmd::REFACTOR;
    m_aliases["review"]    = SlashCmd::REVIEW;
    m_aliases["commit"]    = SlashCmd::COMMIT;
    
    // Git 3
    m_aliases["diff"]      = SlashCmd::DIFF;
    m_aliases["merge"]     = SlashCmd::MERGE;
    m_aliases["git"]       = SlashCmd::GIT;
    
    // Navigation 3
    m_aliases["search"]    = SlashCmd::SEARCH;
    m_aliases["symbol"]    = SlashCmd::SYMBOL;
    m_aliases["type"]      = SlashCmd::TYPE;
    
    // Editor 4
    m_aliases["import"]    = SlashCmd::IMPORT;
    m_aliases["lint"]      = SlashCmd::LINT;
    m_aliases["format"]    = SlashCmd::FORMAT;
    m_aliases["rename"]    = SlashCmd::RENAME;
    
    // Refactor 2
    m_aliases["extract"]   = SlashCmd::EXTRACT;
    m_aliases["inline"]    = SlashCmd::INLINE;
    
    // DevOps 3
    m_aliases["shell"]     = SlashCmd::SHELL;
    m_aliases["build"]     = SlashCmd::BUILD;
    m_aliases["deploy"]    = SlashCmd::DEPLOY;
    
    // Advanced 2
        m_aliases["debug"]     = SlashCmd::DEBUG;
    m_aliases["agent"]     = SlashCmd::AGENT;
}

std::string SlashRouter::Help() const {
    return R"(
/explain  <code>   Explain selected code
/fix      <code>   Fix errors in selection
/test     <func>   Generate unit tests
/doc      <symbol> Generate documentation
/optimize <code>   Performance optimize
/refactor <code>   Restructure code
/review   <file>   Code review
/commit   <msg>    Generate commit message

/diff     <branch> Show git diff
/merge    <branch> Merge conflict help
/git      <cmd>    Run git command

/search   <query>  Search codebase
/symbol   <name>   Find symbol
/type     <name>   Find type definition

/import   <module> Add import/include
/lint     <file>   Run linter
/format   <file>   Format code
/rename   <old>    Rename symbol

/extract  <func>   Extract method
/inline   <func>   Inline method

/shell    <cmd>    Execute shell command
/build    <target> Build project
/deploy   <env>    Deploy to environment

/debug    <var>    Debug variable
/agent    <task>   Autonomous agent task
)";
}