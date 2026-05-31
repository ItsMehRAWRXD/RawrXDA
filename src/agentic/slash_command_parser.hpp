#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

/**
 * Slash Command Parser for RawrXD Agent
 * 
 * Parses user input starting with "/" and converts to tool calls.
 * NO EXTERNAL DEPENDENCIES — Uses only C++ stdlib and existing nlohmann/json
 * 
 * Supported commands:
 * /edit <file1> [file2] [file3]...    → multi-file editing
 * /terminal <command>                  → execute terminal command
 * /search <pattern>                    → semantic code search
 * /read <file>                         → read file
 * /write <file> <content>              → write file
 * /memory <command> [path] [text]      → persistent agent memory files
 * /refactor <type> [selection]         → refactoring (extract, rename, etc)
 * /git <action> [args]                 → git operations
 * /explain [code|selection]            → explain code or selection
 * /fix [file|selection]                → fix issues in file or selection
 * /test [file|pattern]                 → generate/run tests
 * /optimize [file|selection]           → optimize code performance
 * /help [command]                      → show command help
 */

namespace RawrXD::Agentic {

struct ParsedCommand {
    std::string command;           // "edit", "terminal", "search", etc
    std::vector<std::string> args; // remaining arguments after command
    bool valid = false;
    std::string error;             // error message if invalid
    
    // Convert to tool call JSON for AgentToolHandlers::Execute
    nlohmann::json ToToolCall() const;
};

class SlashCommandParser {
public:
    /**
     * Parse slash command string. Returns ParsedCommand with valid flag.
     * 
     * Usage:
     *   auto cmd = SlashCommandParser::Parse("/edit src/file.cpp src/other.h");
     *   if (cmd.valid) {
     *       auto toolCall = cmd.ToToolCall();
     *       // Pass to AgentToolHandlers::Execute(toolCall)
     *   }
     */
    static ParsedCommand Parse(const std::string& input);
    
    /**
     * Check if input starts with slash command
     */
    static bool IsSlashCommand(const std::string& input);
    
    /**
     * Get list of available slash commands
     */
    static std::vector<std::string> AvailableCommands();
    
    /**
     * Get help text for command (or all if command empty)
     */
    static std::string GetHelp(const std::string& command = "");

private:
    static ParsedCommand ParseEdit(const std::vector<std::string>& args);
    static ParsedCommand ParseTerminal(const std::vector<std::string>& args);
    static ParsedCommand ParseSearch(const std::vector<std::string>& args);
    static ParsedCommand ParseRead(const std::vector<std::string>& args);
    static ParsedCommand ParseWrite(const std::vector<std::string>& args);
    static ParsedCommand ParseMemory(const std::vector<std::string>& args);
    static ParsedCommand ParseRefactor(const std::vector<std::string>& args);
    static ParsedCommand ParseGit(const std::vector<std::string>& args);
    static ParsedCommand ParseExplain(const std::vector<std::string>& args);
    static ParsedCommand ParseFix(const std::vector<std::string>& args);
    static ParsedCommand ParseTest(const std::vector<std::string>& args);
    static ParsedCommand ParseOptimize(const std::vector<std::string>& args);
    
    // Streaming control commands (NEW)
    static ParsedCommand ParseStreaming(const std::vector<std::string>& args);
    static ParsedCommand ParseStreamingStatus(const std::vector<std::string>& args);
    static ParsedCommand ParseStreamingAutopatch(const std::vector<std::string>& args);
    static ParsedCommand ParseStreamingThrottle(const std::vector<std::string>& args);
    
    static std::vector<std::string> Tokenize(const std::string& input);
};

} // namespace RawrXD::Agentic
