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
        "edit", "terminal", "search", "read", "write", "memory", "refactor", "git",
        "explain", "fix", "test", "optimize", "taskframe", "framework", "harden", "audit",
        "language", "lang", "context", "help",
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
    /taskframe <job text>       Build deterministic execution framework for a task
    /framework <job text>       Alias for /taskframe
    /harden <module_name>       Generate reliability audit harness scaffold
    /audit <module_name>        Alias for /harden
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
    } else if (command == "taskframe" || command == "framework") {
        return "/taskframe <job/task text>\nGenerate a deterministic phased execution framework including assertions, logging plan, and pass/fail gates.";
    } else if (command == "harden" || command == "audit") {
        return "/harden <module_name>\nGenerate deterministic audit harness scaffolding (PromptAuditHarness + module-specific _Audit.cpp template) with boundary purge assertions.";
    } else if (command == "language" || command == "lang") {
        return "/language <id> [modifiers]\nSwitch language context. Alias: /lang\nExamples:\n  /language cpp\n  /language rust gpu\n  /language python ml\n  /lang ts\n  /language detect  (infer from active file extension)";
    } else if (command == "context") {
        return "/context <profile> [modifiers]\nFull execution context switch bundling language, runtime, toolchain, target, and inference routing.\nExamples:\n  /context rust\n  /context cpp +cuda +vulkan\n  /context python ml gpu\n  /context wasm systems\n  /context detect";
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
    if (cmdName == "taskframe" || cmdName == "framework") return ParseTaskframe(args);
    if (cmdName == "harden" || cmdName == "audit") return ParseHarden(args);
    if (cmdName == "language" || cmdName == "lang") return ParseLanguage(args);
    if (cmdName == "context") return ParseContext(args);
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

ParsedCommand SlashCommandParser::ParseTaskframe(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "taskframe";
    if (args.empty()) {
        cmd.error = "taskframe requires job/task text. Example: /taskframe context firewall stress harness";
        return cmd;
    }
    cmd.args = args;
    cmd.valid = true;
    return cmd;
}

ParsedCommand SlashCommandParser::ParseHarden(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "harden";
    if (args.empty()) {
        cmd.error = "harden requires a module name. Example: /harden AgenticBridge";
        return cmd;
    }
    cmd.args = args;
    cmd.valid = true;
    return cmd;
}

std::string SlashCommandParser::BuildTaskFramework(const std::vector<std::string>& args) {
    std::string objective;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            objective += " ";
        }
        objective += args[i];
    }

    std::string text;
    text += "Execution Framework\n";
    text += "Objective: " + objective + "\n\n";
    text += "Phase 1 - Deterministic Harness\n";
    text += "1. Capture initial bridge snapshot (toggle state, active file, language, session id).\n";
    text += "2. Execute sequence: open file -> query -> disable context -> query -> enable context -> query.\n";
    text += "3. Record per-dispatch audit entry (sequence, context-enabled, injected byte counters, prompt hash).\n\n";
    text += "Phase 2 - Assertions\n";
    text += "1. Disabled state: openTabsBytes == 0 and activeFile/language are empty.\n";
    text += "2. Re-enable state: context bytes increase and active file context returns without restart.\n";
    text += "3. Boundary purge: no stale prompt hash reuse across disable transition.\n\n";
    text += "Phase 3 - Evidence Collection\n";
    text += "1. Save compact prompt audit logs (sizes + hashes, no raw prompt body).\n";
    text += "2. Save bridge state snapshots before/after each transition.\n";
    text += "3. Emit pass/fail summary with first offending sequence id on failure.\n\n";
    text += "Phase 4 - Hardening Extensions\n";
    text += "1. Rapid toggle spam test (20 cycles).\n";
    text += "2. Concurrent request test (2 overlapping agent requests).\n";
    text += "3. Headless override test (--no-current-file-context).\n";
    text += "4. Workspace switch and panel reopen lifecycle test.\n\n";
    text += "Definition of Done\n";
    text += "1. All assertions pass across GUI and headless lanes.\n";
    text += "2. No stale context after disable transitions.\n";
    text += "3. Evidence artifacts are reproducible and deterministic.\n";
    return text;
}

std::string SlashCommandParser::BuildHardenHarness(const std::vector<std::string>& args) {
    std::string module;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            module += "_";
        }
        module += args[i];
    }

    std::string objective;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            objective += " ";
        }
        objective += args[i];
    }

    std::string out;
    out += "Harden Scaffold\n";
    out += "Module: " + objective + "\n";
    out += "Generated files:\n";
    out += "1. PromptAuditHarness.h\n";
    out += "2. " + module + "_Audit.cpp\n\n";

    out += "PromptAuditHarness.h template\n";
    out += "--------------------------------\n";
    out += "#pragma once\n";
    out += "#include <cstdint>\n";
    out += "#include <functional>\n";
    out += "#include <string>\n";
    out += "#include <vector>\n\n";
    out += "struct PromptAuditEntry {\n";
    out += "    uint64_t sequence = 0;\n";
    out += "    bool currentFileContextEnabled = false;\n";
    out += "    bool cliOverridePresent = false;\n";
    out += "    size_t injectedBytes = 0;\n";
    out += "    size_t openTabsBytes = 0;\n";
    out += "    size_t workspaceBytes = 0;\n";
    out += "    size_t activeFileBytes = 0;\n";
    out += "    size_t languageBytes = 0;\n";
    out += "    size_t userPromptBytes = 0;\n";
    out += "    std::string activeFile;\n";
    out += "    std::string language;\n";
    out += "    uint64_t promptHash = 0;\n";
    out += "    std::string sources;\n";
    out += "};\n\n";
    out += "class PromptAuditHarness {\n";
    out += "public:\n";
    out += "    using Step = std::function<void()>;\n";
    out += "    explicit PromptAuditHarness(std::string moduleName);\n";
    out += "    void AddStep(const std::string& name, Step fn);\n";
    out += "    void Snapshot(const PromptAuditEntry& entry);\n";
    out += "    bool AssertBoundaryPurge(const PromptAuditEntry& entry, std::string& reason) const;\n";
    out += "    bool AssertHashShift(uint64_t previousHash, uint64_t currentHash, std::string& reason) const;\n";
    out += "    int Run(std::string& reportOut);\n";
    out += "private:\n";
    out += "    std::string module_;\n";
    out += "    std::vector<std::pair<std::string, Step>> steps_;\n";
    out += "    std::vector<PromptAuditEntry> entries_;\n";
    out += "};\n\n";

    out += module + "_Audit.cpp template\n";
    out += "--------------------------------\n";
    out += "#include \"PromptAuditHarness.h\"\n";
    out += "#include <string>\n\n";
    out += "int Run" + module + "HardenAudit() {\n";
    out += "    PromptAuditHarness harness(\"" + objective + "\");\n";
    out += "    harness.AddStep(\"Open Source\", []() { /* open target file */ });\n";
    out += "    harness.AddStep(\"Enable Context\", []() { /* toggle ON */ });\n";
    out += "    harness.AddStep(\"Verify Injection\", []() { /* assert openTabsBytes > 0 */ });\n";
    out += "    harness.AddStep(\"Disable Context\", []() { /* toggle OFF */ });\n";
    out += "    harness.AddStep(\"Assert Purge\", []() {\n";
    out += "        // assert openTabsBytes == 0, activeFile empty, language empty\n";
    out += "    });\n";
    out += "    harness.AddStep(\"Re-enable Context\", []() { /* toggle ON */ });\n";
    out += "    harness.AddStep(\"Verify Reinjection\", []() { /* assert context returns */ });\n\n";
    out += "    std::string report;\n";
    out += "    return harness.Run(report);\n";
    out += "}\n\n";

    out += "Deterministic validation sequence\n";
    out += "--------------------------------\n";
    out += "1. open file -> query -> snapshot\n";
    out += "2. disable context -> query -> snapshot\n";
    out += "3. enable context -> query -> snapshot\n";
    out += "4. assert OFF snapshot has zero context bytes\n";
    out += "5. assert hash changes after file switch and after re-enable\n";
    out += "6. run rapid-toggle and concurrent-request stress lanes\n";
    return out;
}

// ---------------------------------------------------------------------------
// Language / Context registry and resolver
// ---------------------------------------------------------------------------

namespace {

struct LanguageEntry {
    const char* id;
    const char* mode;
    const char* runtime;
    const char* toolchain;
    const char* target;
    const char* ext;    // canonical file extension for auto-detect
};

// Full coverage registry — maps every alias to a canonical profile.
static const LanguageEntry kLangRegistry[] = {
    // Systems
    {"c",          "C",        "native",  "gcc",     "cpu",      ".c"},
    {"cpp",        "CPP",      "native",  "msvc",    "cpu",      ".cpp"},
    {"c++",        "CPP",      "native",  "msvc",    "cpu",      ".cpp"},
    {"c/c++",      "CPP",      "native",  "msvc",    "cpu",      ".cpp"},
    {"rust",       "RUST",     "native",  "cargo",   "cpu",      ".rs"},
    {"rs",         "RUST",     "native",  "cargo",   "cpu",      ".rs"},
    {"zig",        "ZIG",      "native",  "zig",     "cpu",      ".zig"},
    {"asm",        "ASM",      "native",  "masm64",  "cpu",      ".asm"},
    {"assembly",   "ASM",      "native",  "masm64",  "cpu",      ".asm"},
    {"nasm",       "ASM",      "native",  "nasm",    "cpu",      ".asm"},
    {"fortran",    "FORTRAN",  "native",  "gfortran","cpu",      ".f90"},
    // Web
    {"html",       "HTML",     "browser", "none",    "wasm",     ".html"},
    {"css",        "CSS",      "browser", "none",    "wasm",     ".css"},
    {"javascript", "JS",       "browser", "node",    "wasm",     ".js"},
    {"js",         "JS",       "browser", "node",    "wasm",     ".js"},
    {"typescript", "TS",       "browser", "node",    "wasm",     ".ts"},
    {"ts",         "TS",       "browser", "node",    "wasm",     ".ts"},
    {"js/ts",      "TS",       "browser", "node",    "wasm",     ".ts"},
    {"html/css",   "WEB",      "browser", "none",    "wasm",     ".html"},
    {"react",      "REACT",    "browser", "node",    "wasm",     ".tsx"},
    {"vue",        "VUE",      "browser", "node",    "wasm",     ".vue"},
    {"svelte",     "SVELTE",   "browser", "node",    "wasm",     ".svelte"},
    {"node",       "JS",       "node",    "node",    "cpu",      ".js"},
    {"deno",       "TS",       "deno",    "deno",    "cpu",      ".ts"},
    {"wasm",       "WASM",     "wasm",    "emcc",    "wasm",     ".wat"},
    // JVM
    {"java",       "JAVA",     "jvm",     "maven",   "cpu",      ".java"},
    {"kotlin",     "KOTLIN",   "jvm",     "gradle",  "cpu",      ".kt"},
    {"scala",      "SCALA",    "jvm",     "sbt",     "cpu",      ".scala"},
    {"groovy",     "GROOVY",   "jvm",     "gradle",  "cpu",      ".groovy"},
    // .NET
    {"csharp",     "CSHARP",   "clr",     "msbuild", "cpu",      ".cs"},
    {"c#",         "CSHARP",   "clr",     "msbuild", "cpu",      ".cs"},
    {"fsharp",     "FSHARP",   "clr",     "msbuild", "cpu",      ".fs"},
    {"f#",         "FSHARP",   "clr",     "msbuild", "cpu",      ".fs"},
    {"vbnet",      "VBNET",    "clr",     "msbuild", "cpu",      ".vb"},
    // Scripting / AI
    {"python",     "PYTHON",   "cpython", "pip",     "cpu",      ".py"},
    {"py",         "PYTHON",   "cpython", "pip",     "cpu",      ".py"},
    {"r",          "R",        "r",       "rscript", "cpu",      ".r"},
    {"julia",      "JULIA",    "julia",   "julia",   "cpu",      ".jl"},
    {"lua",        "LUA",      "lua",     "lua",     "cpu",      ".lua"},
    {"perl",       "PERL",     "perl",    "perl",    "cpu",      ".pl"},
    {"bash",       "SHELL",    "bash",    "bash",    "cpu",      ".sh"},
    {"shell",      "SHELL",    "bash",    "bash",    "cpu",      ".sh"},
    {"zsh",        "SHELL",    "zsh",     "zsh",     "cpu",      ".zsh"},
    {"powershell", "PWSH",     "pwsh",    "pwsh",    "cpu",      ".ps1"},
    // Functional
    {"haskell",    "HASKELL",  "ghc",     "cabal",   "cpu",      ".hs"},
    {"elixir",     "ELIXIR",   "beam",    "mix",     "cpu",      ".ex"},
    {"erlang",     "ERLANG",   "beam",    "rebar3",  "cpu",      ".erl"},
    {"ocaml",      "OCAML",    "ocaml",   "dune",    "cpu",      ".ml"},
    {"lisp",       "LISP",     "lisp",    "sbcl",    "cpu",      ".lisp"},
    {"clojure",    "CLOJURE",  "jvm",     "lein",    "cpu",      ".clj"},
    // Data / AI / ML
    {"python-ml",  "PYTHON_ML","cpython", "pip",     "gpu",      ".py"},
    {"pytorch",    "PYTHON_ML","cpython", "pip",     "gpu",      ".py"},
    {"tensorflow", "PYTHON_ML","cpython", "pip",     "gpu",      ".py"},
    {"jax",        "PYTHON_ML","cpython", "pip",     "gpu",      ".py"},
    {"sql",        "SQL",      "none",    "none",    "cpu",      ".sql"},
    {"nosql",      "NOSQL",    "none",    "none",    "cpu",      ""},
    // GPU / Compute
    {"cuda",       "CUDA",     "nvcc",    "nvcc",    "gpu",      ".cu"},
    {"opencl",     "OPENCL",   "ocl",     "gcc",     "gpu",      ".cl"},
    {"vulkan",     "VULKAN",   "vulkan",  "glslc",   "gpu",      ".glsl"},
    {"metal",      "METAL",    "metal",   "xcrun",   "gpu",      ".metal"},
    {"hlsl",       "HLSL",     "dx12",    "fxc",     "gpu",      ".hlsl"},
    // Embedded
    {"arm",        "ARM",      "native",  "arm-gcc", "embedded", ".s"},
    {"riscv",      "RISCV",    "native",  "riscv-gcc","embedded",".s"},
    {"embedded-c", "C",        "none",    "arm-gcc", "embedded", ".c"},
    // Meta / Tooling
    {"cmake",      "CMAKE",    "none",    "cmake",   "cpu",      ".cmake"},
    {"dockerfile", "DOCKER",   "docker",  "docker",  "cpu",      "Dockerfile"},
    {"go",         "GO",       "go",      "go",      "cpu",      ".go"},
    {"golang",     "GO",       "go",      "go",      "cpu",      ".go"},
    {"ruby",       "RUBY",     "ruby",    "gem",     "cpu",      ".rb"},
    {"php",        "PHP",      "php",     "php",     "cpu",      ".php"},
    {"swift",      "SWIFT",    "swift",   "xcode",   "cpu",      ".swift"},
    {"dart",       "DART",     "dart",    "flutter", "cpu",      ".dart"},
    // Compound profiles
    {"rust/cpp",   "SYSTEMS",  "native",  "msvc",    "cpu",      ""},
    {"systems",    "SYSTEMS",  "native",  "msvc",    "cpu",      ""},
};

const LanguageEntry* FindLangEntry(const std::string& id) {
    std::string lower = id;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const auto& e : kLangRegistry) {
        if (lower == e.id) return &e;
    }
    return nullptr;
}

// Extension → mode (for /language detect)
const char* LangFromExtension(const std::string& ext) {
    for (const auto& e : kLangRegistry) {
        if (*e.ext && ext == e.ext) return e.mode;
    }
    return nullptr;
}

struct ContextModifiers {
    bool gpu = false;
    bool wasm = false;
    bool ml = false;
    bool cuda = false;
    bool vulkan = false;
    bool embedded = false;
};

ContextModifiers ParseModifiers(const std::vector<std::string>& tokens, size_t start) {
    ContextModifiers m;
    for (size_t i = start; i < tokens.size(); ++i) {
        std::string t = tokens[i];
        std::transform(t.begin(), t.end(), t.begin(), ::tolower);
        // Strip leading +/-
        if (!t.empty() && (t[0] == '+' || t[0] == '-')) t = t.substr(1);
        if (t == "gpu") m.gpu = true;
        else if (t == "wasm") m.wasm = true;
        else if (t == "ml") m.ml = true;
        else if (t == "cuda") m.cuda = true;
        else if (t == "vulkan") m.vulkan = true;
        else if (t == "embedded") m.embedded = true;
    }
    return m;
}

} // anonymous namespace

std::string SlashCommandParser::ResolveLanguageMode(const std::string& id) {
    const LanguageEntry* e = FindLangEntry(id);
    return e ? e->mode : "";
}

ParsedCommand SlashCommandParser::ParseLanguage(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "language";
    if (args.empty()) {
        cmd.error = "language requires an id or 'detect'. Example: /language cpp";
        return cmd;
    }
    cmd.args = args;
    cmd.valid = true;
    return cmd;
}

ParsedCommand SlashCommandParser::ParseContext(const std::vector<std::string>& args) {
    ParsedCommand cmd;
    cmd.command = "context";
    if (args.empty()) {
        cmd.error = "context requires a profile. Example: /context cpp +cuda";
        return cmd;
    }
    cmd.args = args;
    cmd.valid = true;
    return cmd;
}

std::string SlashCommandParser::BuildContextSwitchResponse(const std::vector<std::string>& args) {
    if (args.empty()) {
        return "Error: context requires a profile. Example: /context cpp +cuda\nTry /help context for usage.";
    }

    std::string id = args[0];
    std::transform(id.begin(), id.end(), id.begin(), ::tolower);

    // Detect sub-command
    if (id == "detect") {
        return "Language detect: not yet wired to active editor extension — open a file and re-run.";
    }

    const LanguageEntry* entry = FindLangEntry(id);
    ContextModifiers mods = ParseModifiers(args, 1);

    std::string mode       = entry ? entry->mode      : id;
    std::string runtime    = entry ? entry->runtime   : "native";
    std::string toolchain  = entry ? entry->toolchain : "?";
    std::string target     = entry ? entry->target    : "cpu";

    // Modifier overrides
    if (mods.gpu || mods.cuda)    target = "gpu";
    if (mods.wasm)                target = "wasm";
    if (mods.embedded)            target = "embedded";
    if (mods.ml)                  { mode = "PYTHON_ML"; runtime = "cpython"; toolchain = "pip"; target = target == "cpu" ? "gpu" : target; }
    if (mods.cuda)                toolchain += "+nvcc";
    if (mods.vulkan)              toolchain += "+glslc";

    // Build active modifier tag list
    std::string tags = "[LANG:" + mode + "][TARGET:" + std::string(target) + "]";
    if (mods.cuda)   tags += "[CTX:CUDA]";
    if (mods.vulkan) tags += "[CTX:VULKAN]";
    if (mods.ml)     tags += "[CTX:ML]";
    if (mods.wasm)   tags += "[CTX:WASM]";

    std::string out;
    out += "Context switched\n";
    out += "Mode:      " + mode + "\n";
    out += "Runtime:   " + runtime + "\n";
    out += "Toolchain: " + toolchain + "\n";
    out += "Target:    " + target + "\n";
    out += "Tags:      " + tags + "\n\n";
    out += "Activated:\n";
    out += "  Syntax:      " + mode + " highlight\n";
    out += "  Autocomplete: " + mode + " profile\n";
    out += "  Prompt tags: " + tags + "\n";
    out += "  Diagnostics: " + toolchain + " errors\n";
    if (mods.ml) {
        out += "  Inference routing: GPU-accelerated (PyTorch/TF)\n";
        out += "  Memory planner:    GPU expert load enabled\n";
    }
    if (mods.cuda || mods.vulkan) {
        out += "  Compute bridge:    GPU shader hooks active\n";
    }
    out += "\nPrompt diff logger will now tag dispatches with: " + tags;
    return out;
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
