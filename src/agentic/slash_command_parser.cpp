#include "slash_command_parser.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace RawrXD::Agentic {

// Tokenize input string by whitespace, respecting quoted strings
std::vector<std::string> SlashCommandParser::Tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string token;
    bool inQuote = false;
    
    for (size_t i = 0; i < input.length(); i++) {
        char c = input[i];
        
        if (c == '"' && (i == 0 || input[i-1] != '\\')) {
            inQuote = !inQuote;
        } else if ((isspace(c) || c == '\t') && !inQuote) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += c;
        }
    }
    
    if (!token.empty()) {
        tokens.push_back(token);
    }
    
    return tokens;
}

bool SlashCommandParser::IsSlashCommand(const std::string& input) {
    auto trimmed = input;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    return !trimmed.empty() && trimmed[0] == '/';
}

std::vector<std::string> SlashCommandParser::AvailableCommands() {
    return {
        "edit", "terminal", "search", "read", "write", "memory", "refactor", "git",
        "explain", "fix", "test", "optimize", "help",
        "streaming", "streaming status", "streaming autopatch", "streaming throttle"
    };
}

std::string SlashCommandParser::GetHelp(const std::string& command) {
    if (command.empty()) {
        return R"(
RawrXD Slash Commands:
  /edit <file1> [file2]...    Edit one or more files in sequence
  /terminal <cmd>             Execute terminal command and capture output
  /search <pattern>           Semantic code search in repository
  /read <file>                Read a file without editing
  /write <file> <content>     Write content to file
    /memory <cmd> [path] [text] Manage persistent /memories files via the main agent HUB
  /refactor <type> [sel]      Refactor code (extract, rename, simplify, etc)
  /git <action> [args]        Git operations (commit, diff, log, status)
  /help [command]             Show this help or help for specific command
)";
    } else if (command == "edit") {
        return "/edit FILE1 [FILE2] ...\nEdit one or more files sequentially with multi-file awareness.";
    } else if (command == "terminal") {
        return "/terminal COMMAND\nExecute terminal command. Output captured and returned.";
    } else if (command == "search") {
        return "/search PATTERN\nSemantic code search for PATTERN in repository.";
    } else if (command == "read") {
        return "/read FILE\nRead FILE without opening editor. Returns file contents.";
    } else if (command == "write") {
        return "/write FILE CONTENT\nWrite CONTENT to FILE (overwrites if exists).";
    } else if (command == "memory") {
        return "/memory COMMAND [PATH] [TEXT]\nCommands: view, create, delete, retrieve, reindex. Example: /memory view /memories/repo/";
    } else if (command == "refactor") {
        return "/refactor TYPE [SELECTION]\nRefactor TYPE: extract, rename, simplify, inline";
    } else if (command == "git") {
        return "/git ACTION [ARGS]\nGit actions: commit, diff, log, status, blame";
    } else if (command == "explain") {
        return "/explain [CODE|selection]\nExplain the selected code or current context.";
    } else if (command == "fix") {
        return "/fix [FILE|selection]\nFix issues in the specified file or selection.";
    } else if (command == "test") {
        return "/test [FILE|PATTERN]\nGenerate or run tests for the specified file or pattern.";
    } else if (command == "optimize") {
        return "/optimize [FILE|selection]\nOptimize code performance in the specified file or selection.";
    } else if (command == "streaming") {
        return R"(/streaming <subcommand> [args]
Streaming engine control commands:
  /streaming status              → Show phase-aware metrics snapshot
  /streaming autopatch on|off    → Toggle autopatch and mmap fallback
  /streaming throttle <tps>      → Cap decode TPS (0=unlimited))";
    } else if (command == "streaming status") {
        return "/streaming status\nShow JSON snapshot of phase-aware metrics (TTFT, TPS, memory, phase).";
    } else if (command == "streaming autopatch") {
        return "/streaming autopatch on|off\nToggle autopatch system and mmap fallback for large zones.";
    } else if (command == "streaming throttle") {
        return "/streaming throttle <tps>\nCap decode TPS to specified value (0=unlimited).";
    }
    return "Unknown command: " + command;
}

ParsedCommand SlashCommandParser::ParseEdit(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "edit";
    
    if (args.empty()) {
        cmd.error = "edit requires at least one file argument";
        return cmd;
    }
    
    // /edit file1 file2 file3...
    // Maps to: multi-file editing tool
    cmd.args = args;
    cmd.valid = true;
    return cmd;
}

ParsedCommand SlashCommandParser::ParseTerminal(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "terminal";
    
    if (args.empty()) {
        cmd.error = "terminal requires a command";
        return cmd;
    }
    
    // Rejoin args as single command string
    std::string fullCmd = args[0];
    for (size_t i = 1; i < args.size(); i++) {
        fullCmd += " " + args[i];
    }
    
    cmd.args.push_back(fullCmd);
    cmd.valid = true;
    return cmd;
}

ParsedCommand SlashCommandParser::ParseSearch(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "search";
    
    if (args.empty()) {
        cmd.error = "search requires a pattern";
        return cmd;
    }
    
    // /search pattern words...
    cmd.args = args;
    cmd.valid = true;
    return cmd;
}

ParsedCommand SlashCommandParser::ParseRead(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "read";
    
    if (args.empty()) {
        cmd.error = "read requires a file path";
        return cmd;
    }
    
    cmd.args.push_back(args[0]); // first arg is file
    cmd.valid = true;
    return cmd;
}

ParsedCommand SlashCommandParser::ParseWrite(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "write";
    
    if (args.size() < 2) {
        cmd.error = "write requires file and content";
        return cmd;
    }
    
    cmd.args.push_back(args[0]); // file
    
    // Rejoin remaining as content
    std::string content = args[1];
    for (size_t i = 2; i < args.size(); i++) {
        content += " " + args[i];
    }
    cmd.args.push_back(content);
    cmd.valid = true;
    return cmd;
}

ParsedCommand SlashCommandParser::ParseMemory(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "memory";

    if (args.empty()) {
        cmd.error = "memory requires a command (view, create, delete, retrieve, reindex)";
        return cmd;
    }

    cmd.args = args;
    cmd.valid = true;
    return cmd;
}

ParsedCommand SlashCommandParser::ParseRefactor(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "refactor";
    
    if (args.empty()) {
        cmd.error = "refactor requires a type (extract, rename, simplify, inline)";
        return cmd;
    }
    
    // /refactor type [selection]
    cmd.args = args;
    cmd.valid = true;
    return cmd;
}

ParsedCommand SlashCommandParser::ParseGit(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "git";
    
    if (args.empty()) {
        cmd.error = "git requires an action (commit, diff, log, status)";
        return cmd;
    }
    
    // /git action [args]...
    cmd.args = args;
    cmd.valid = true;
    return cmd;
}

ParsedCommand SlashCommandParser::Parse(const std::string& input) {
    ParsedCommand cmd;
    
    if (!IsSlashCommand(input)) {
        cmd.error = "not a slash command (must start with /)";
        return cmd;
    }
    
    // Tokenize
    auto tokens = Tokenize(input);
    if (tokens.empty()) {
        cmd.error = "empty command";
        return cmd;
    }
    
    // First token is /command
    std::string cmdToken = tokens[0];
    if (cmdToken[0] != '/' || cmdToken.length() < 2) {
        cmd.error = "invalid command format";
        return cmd;
    }
    
    std::string cmdName = cmdToken.substr(1); // Remove leading /
    
    // Rest are arguments
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());
    
    // Dispatch to appropriate parser
    if (cmdName == "edit")     return ParseEdit(args);
    if (cmdName == "terminal") return ParseTerminal(args);
    if (cmdName == "search")   return ParseSearch(args);
    if (cmdName == "read")     return ParseRead(args);
    if (cmdName == "write")    return ParseWrite(args);
    if (cmdName == "memory")   return ParseMemory(args);
    if (cmdName == "refactor") return ParseRefactor(args);
    if (cmdName == "git")      return ParseGit(args);
    if (cmdName == "explain")  return ParseExplain(args);
    if (cmdName == "fix")      return ParseFix(args);
    if (cmdName == "test")     return ParseTest(args);
    if (cmdName == "optimize") return ParseOptimize(args);
    if (cmdName == "streaming") return ParseStreaming(args);
    if (cmdName == "help") {
        cmd.command = "help";
        cmd.args = args;
        cmd.valid = true;
        return cmd;
    }
    
    cmd.error = "unknown command: " + cmdName;
    return cmd;
}

nlohmann::json ParsedCommand::ToToolCall() const {
    using json = nlohmann::json;
    
    json toolCall = json::object();
    
    // Map slash command to appropriate tool
    if (command == "edit") {
        toolCall["tool"] = "propose_multifile_edits";
        toolCall["args"] = json::object();
        json files = json::array();
        for (const auto& arg : args) {
            files.push_back(arg);
        }
        toolCall["args"]["files"] = files;
    } 
    else if (command == "terminal") {
        toolCall["tool"] = "run_terminal";
        toolCall["args"] = json::object();
        if (!args.empty()) {
            toolCall["args"]["command"] = args[0];
        }
    }
    else if (command == "search") {
        toolCall["tool"] = "semantic_search";
        toolCall["args"] = json::object();
        if (!args.empty()) {
            toolCall["args"]["query"] = args[0];
        }
    }
    else if (command == "read") {
        toolCall["tool"] = "read_file";
        toolCall["args"] = json::object();
        if (!args.empty()) {
            toolCall["args"]["path"] = args[0];
        }
    }
    else if (command == "write") {
        toolCall["tool"] = "write_file";
        toolCall["args"] = json::object();
        if (args.size() >= 2) {
            toolCall["args"]["path"] = args[0];
            toolCall["args"]["content"] = args[1];
        }
    }
    else if (command == "memory") {
        toolCall["tool"] = "memory_file";
        toolCall["args"] = json::object();
        if (!args.empty()) {
            toolCall["args"]["command"] = args[0];
        }
        if (args.size() >= 2) {
            if (args[0] == "retrieve") {
                std::string query = args[1];
                for (size_t i = 2; i < args.size(); ++i) {
                    query += " " + args[i];
                }
                toolCall["args"]["query"] = query;
            } else {
                toolCall["args"]["path"] = args[1];
            }
        }
        if (args.size() >= 3 && args[0] == "create") {
            std::string text = args[2];
            for (size_t i = 3; i < args.size(); ++i) {
                text += " " + args[i];
            }
            toolCall["args"]["file_text"] = text;
        }
    }
    else if (command == "refactor") {
        toolCall["tool"] = "propose_multifile_edits"; // Use multi-file edits as base
        toolCall["args"] = json::object();
        if (!args.empty()) {
            toolCall["args"]["refactor_type"] = args[0];
        }
    }
    else if (command == "git") {
        toolCall["tool"] = "run_terminal";
        toolCall["args"] = json::object();
        std::string gitCmd = "git";
        for (const auto& arg : args) {
            gitCmd += " " + arg;
        }
        toolCall["args"]["command"] = gitCmd;
    }
    else if (command == "help") {
        toolCall["tool"] = "help";
        toolCall["args"] = json::object();
        if (!args.empty()) {
            toolCall["args"]["command"] = args[0];
        }
    }
    else if (command == "explain") {
        toolCall["tool"] = "agentic_explain";
        toolCall["args"] = json::object();
        if (!args.empty()) {
            toolCall["args"]["target"] = args[0];
        } else {
            toolCall["args"]["target"] = "selection";
        }
    }
    else if (command == "fix") {
        toolCall["tool"] = "agentic_fix";
        toolCall["args"] = json::object();
        if (!args.empty()) {
            toolCall["args"]["target"] = args[0];
        } else {
            toolCall["args"]["target"] = "selection";
        }
    }
    else if (command == "test") {
        toolCall["tool"] = "agentic_test";
        toolCall["args"] = json::object();
        if (!args.empty()) {
            toolCall["args"]["target"] = args[0];
        } else {
            toolCall["args"]["target"] = "current_file";
        }
    }
    else if (command == "optimize") {
        toolCall["tool"] = "agentic_optimize";
        toolCall["args"] = json::object();
        if (!args.empty()) {
            toolCall["args"]["target"] = args[0];
        } else {
            toolCall["args"]["target"] = "selection";
        }
    }
    else if (command == "streaming_status") {
        toolCall["tool"] = "streaming_status";
        toolCall["args"] = json::object();
    }
    else if (command == "streaming_autopatch") {
        toolCall["tool"] = "streaming_autopatch";
        toolCall["args"] = json::object();
        if (!args.empty()) {
            toolCall["args"]["state"] = args[0];  // "on" or "off"
        }
    }
    else if (command == "streaming_throttle") {
        toolCall["tool"] = "streaming_throttle";
        toolCall["args"] = json::object();
        if (!args.empty()) {
            toolCall["args"]["tps"] = args[0];  // numeric TPS cap
        }
    }

    return toolCall;
}

// Parse /explain [code|selection]
ParsedCommand SlashCommandParser::ParseExplain(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "explain";
    cmd.args = args;
    cmd.valid = true;
    return cmd;
}

// Parse /fix [file|selection]
ParsedCommand SlashCommandParser::ParseFix(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "fix";
    cmd.args = args;
    cmd.valid = true;
    return cmd;
}

// Parse /test [file|pattern]
ParsedCommand SlashCommandParser::ParseTest(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "test";
    cmd.args = args;
    cmd.valid = true;
    return cmd;
}

// Parse /optimize [file|selection]
ParsedCommand SlashCommandParser::ParseOptimize(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "optimize";
    cmd.args = args;
    cmd.valid = true;
    return cmd;
}

// Parse /streaming <subcommand> [args]
ParsedCommand SlashCommandParser::ParseStreaming(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    
    if (args.empty()) {
        cmd.error = "streaming requires a subcommand: status, autopatch, throttle";
        return cmd;
    }
    
    std::string subcmd = args[0];
    std::vector<std::string> subArgs(args.begin() + 1, args.end());
    
    if (subcmd == "status") {
        return ParseStreamingStatus(subArgs);
    } else if (subcmd == "autopatch") {
        return ParseStreamingAutopatch(subArgs);
    } else if (subcmd == "throttle") {
        return ParseStreamingThrottle(subArgs);
    }
    
    cmd.error = "unknown streaming subcommand: " + subcmd;
    return cmd;
}

// Parse /streaming status
ParsedCommand SlashCommandParser::ParseStreamingStatus(const std::vector<std::string>& args) {
    (void)args; // No additional args needed
    ParsedCommand cmd;
    cmd.command = "streaming_status";
    cmd.valid = true;
    return cmd;
}

// Parse /streaming autopatch on|off
ParsedCommand SlashCommandParser::ParseStreamingAutopatch(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "streaming_autopatch";
    
    if (args.empty()) {
        cmd.error = "autopatch requires 'on' or 'off'";
        return cmd;
    }
    
    std::string state = args[0];
    if (state != "on" && state != "off") {
        cmd.error = "autopatch state must be 'on' or 'off', got: " + state;
        return cmd;
    }
    
    cmd.args = args;
    cmd.valid = true;
    return cmd;
}

// Parse /streaming throttle <tps>
ParsedCommand SlashCommandParser::ParseStreamingThrottle(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "streaming_throttle";
    
    if (args.empty()) {
        cmd.error = "throttle requires a TPS value (0=unlimited)";
        return cmd;
    }
    
    // Validate TPS is numeric
    try {
        double tps = std::stod(args[0]);
        if (tps < 0) {
            cmd.error = "throttle TPS must be >= 0";
            return cmd;
        }
    } catch (...) {
        cmd.error = "throttle TPS must be a number, got: " + args[0];
        return cmd;
    }
    
    cmd.args = args;
    cmd.valid = true;
    return cmd;
}

} // namespace RawrXD::Agentic
