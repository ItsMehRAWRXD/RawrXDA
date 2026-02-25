import os

file_path = r'e:\RawrXD\src\interactive_shell.cpp'

with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()

start_marker = 'suggestions.push_back("!memory " + cmd);'
end_marker = '// Global shell instance implementation'

start_idx = content.find(start_marker)
end_idx = content.rfind(end_marker)

if start_idx == -1 or end_idx == -1:
    print(f"Error: Markers not found. Start: {start_idx}, End: {end_idx}")
    exit(1)

# Adjust start_idx to include the closing braces of the previous block
# We want to keep the start_marker line and the braces closing the memory loop
# The memory loop closes with:
#             }
#         }
#     }

# Searching for the closing brace after suggestions.push_back
idx = content.find('}', start_idx) # closes if
idx = content.find('}', idx + 1)   # closes for
idx = content.find('}', idx + 1)   # closes if (input.rfind...)

if idx == -1:
    print("Error: Could not find closing braces for memory block")
    exit(1)
    
split_point = idx + 1 # After the closing brace of memory block

new_middle = '''
    else if (input.rfind("!engine ", 0) == 0) {
        // Engine command autocomplete
        std::vector<std::string> engine_commands = {
            "list", "switch", "load", "unload", "help"
        };
        std::string after_engine = input.substr(8);
        for (const auto& cmd : engine_commands) {
            if (cmd.find(after_engine) == 0) {
                suggestions.push_back("!engine " + cmd);
            }
        }
    }

    return suggestions;
}

std::string InteractiveShell::GetHelp() const {
    return R"(
╔══════════════════════════════════════════════════════════════╗
║                    RawrXD AI Shell v6.0                      ║
║                                                              ║
║  CORE COMMANDS:                                              ║
║  /plan <task>            - Create execution plan             ║
║  /react-server <name>    - Generate React + Express project  ║
║  /bugreport <target>     - Analyze for bugs                  ║
║  /suggest <code>         - Get AI code suggestions           ║
║  /hotpatch f old new     - Apply code hotpatch               ║
║  /analyze <target>       - Deep code analysis                ║
║  /optimize <target>      - Performance optimization          ║
║  /security <target>      - Security vulnerability scan       ║
║                                                              ║
║  MODE TOGGLES:                                               ║
║  /maxmode <on|off>       - Toggle 32K+ context               ║
║  /deepthinking <on|off>  - Enable chain-of-thought           ║
║  /deepresearch <on|off>  - Enable workspace scanning         ║
║  /norefusal <on|off>     - Bypass safety filters             ║
║  /autocorrect <on|off>   - Auto-fix hallucinations           ║
║                                                              ║
║  CONTEXT & MEMORY:                                           ║
║  /context <size>         - Set context (4k/32k...1m)         ║
║  /context+               - Increase context size             ║
║  /context-               - Decrease context size             ║
║  /memory-status          - Show memory usage                 ║
║                                                              ║
║  PLUGIN COMMANDS:                                            ║
║  !plugin load <path>     - Load VSIX plugin                  ║
║  !plugin unload <id>     - Unload plugin                     ║
║  !plugin enable <id>     - Enable plugin                     ║
║  !plugin disable <id>    - Disable plugin                    ║
║  !plugin reload <id>     - Reload plugin                     ║
║  !plugin help <id>       - Show plugin help                  ║
║  !plugin list            - List all plugins                  ║
║  !plugin config <id>     - Configure plugin                  ║
║                                                              ║
║  MEMORY COMMANDS:                                            ║
║  !memory load <size>     - Load memory module                ║
║  !memory unload <size>   - Unload memory module              ║
║  !memory list            - List available modules            ║
║  !memory current         - Show current context              ║
║                                                              ║
║  ENGINE COMMANDS:                                            ║
║  !engine list            - List available engines            ║
║  !engine switch <id>     - Switch to engine                  ║
║  !engine load <path> <id>- Load new engine                   ║
║  !engine unload <id>     - Unload engine                     ║
║  !engine help <id>       - Show engine help                  ║
║                                                              ║
║  SHELL COMMANDS:                                             ║
║  /clear                  - Clear history                     ║
║  /save <filename>        - Save conversation                 ║
║  /load <filename>        - Load conversation                 ║
║  /status                 - Show system status                ║
║  /help                   - Show this help                    ║
║  /exit                   - Exit shell                        ║
║                                                              ║
║  For more help: https://github.com/ItsMehRAWRXD/RawrXD/wiki  ║
╚══════════════════════════════════════════════════════════════╝
)";
}

std::string InteractiveShell::GetPluginHelp() const {
    if (!vsix_loader_) return "VSIX loader not initialized\\n";
    return vsix_loader_->GetAllPluginsHelp();
}

std::string InteractiveShell::GetMemoryHelp() const {
    if (!memory_) return "Memory manager not initialized\\n";
    std::string help = "Memory Modules:\\n";
    auto sizes = memory_->GetAvailableSizes();
    for (auto size : sizes) {
        MemoryModule* module = memory_->GetModule(static_cast<size_t>(size));
        if (module) {
             help += "  " + module->GetName() + " - " + std::to_string(module->GetMaxTokens()) + " tokens\\n";
        }
    }
    help += "\\nCurrent context: " + std::to_string(memory_->GetCurrentContextSize() / 1024) + "K\\n";
    return help;
}

std::string InteractiveShell::GetEngineHelp() const {
    if (!vsix_loader_) return "VSIX loader not initialized\\n";
    std::string help = "Available Engines:\\n";
    auto engines = vsix_loader_->GetAvailableEngines();
    for (const auto& engine : engines) {
        help += "  " + engine + "\\n";
        if (engine == "800b-5drive") {
            help += "    - Supports 800B models\\n";
            help += "    - 5-drive setup for distributed loading\\n";
            help += "    - Streaming loader for memory efficiency\\n";
        }
        else if (engine == "codex-ultimate") {
            help += "    - Reverse engineering suite\\n";
            help += "    - Includes disassembler, dumpbin, compiler\\n";
        }
        else if (engine == "rawrxd-compiler") {
            help += "    - MASM64 compiler\\n";
            help += "    - AVX-512 optimization\\n";
        }
    }
    help += "\\nCurrent engine: " + vsix_loader_->GetCurrentEngine() + "\\n";
    return help;
}

'''

new_content = content[:split_point] + new_middle + content[end_idx:]

with open(file_path, 'w', encoding='utf-8') as f:
    f.write(new_content)

print(f"Successfully patched {file_path}")
