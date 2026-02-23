// ============================================================================
// orchestrator_cli_main.cpp — Standalone CLI Entry Point
// ============================================================================
#include "orchestrator_cli_handler.hpp"
#include "autonomous_orchestrator.hpp"
#include <iostream>
#include <fstream>

void print_banner() {
    std::cout << R"(
    ___        __                                               
   /   | __  _/ /_____  ____  ____  ____ ___  ____  __  _______
  / /| |/ / / / __/ __ \/ __ \/ __ \/ __ \`__ \/ __ \/ / / / ___/
 / ___ / /_/ / /_/ /_/ / / / / /_/ / / / / / / /_/ / /_/ (__  ) 
/_/  |_\__,_/\__/\____/_/ /_/\____/_/ /_/ /_/\____/\__,_/____/  
                                                                  
   ___           __              __            __                
  / _ \________/ /  ___ ___ ___/ /________ __/ /____ ____       
 / // / __/ __/ _ \/ -_|_-<_-< _ / __/ _ \`/ __/ __ \/ __/       
/____/_/  \__/_//_/\__/___/___/_/ \__/\_,_/\__/\___/_/          

         RawrXD Ultimate Autonomous Orchestrator v1.0
         Multi-Agent | Self-Adjusting | Production-Ready
)" << std::endl;
}

void print_help() {
    std::cout << R"(
USAGE:
  orchestrator_cli [--orchestrator <command>] [--config <file>] [OPTIONS]

COMMANDS:
  audit:<path>:<deep>        Audit codebase (deep = true/false)
  execute:<mode>             Execute todos (mode = all, top-priority, etc.)
  execute-top-difficult:<n>  Execute top N difficult tasks
  execute-top-priority:<n>   Execute top N priority tasks
  status                     Show current status
  auto-optimize              Analyze and optimize settings

OPTIONS:
  --orchestrator <cmd>       Orchestrator command (see above)
  --config <file>            Load configuration from JSON file
  --help                     Show this help

INTERACTIVE COMMANDS (without --orchestrator):
  audit [path] [--deep]      Interactive audit
  execute [mode]             Interactive execution
  status                     Show status
  help                       Show help
  exit                       Exit shell

EXAMPLES:
  # Audit with config
  orchestrator_cli --orchestrator audit:.:true --config config.json

  # Execute top 20 difficult tasks
  orchestrator_cli --orchestrator execute-top-difficult:20 --config config.json

  # Interactive mode
  orchestrator_cli
  > audit . --deep
  > execute-top-difficult 20
  > status

See AUTONOMOUS_ORCHESTRATOR_GUIDE.md for complete documentation.
)" << std::endl;
}

int main(int argc, char** argv) {
    // Check for --help
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_banner();
            print_help();
            return 0;
        }
    }

    // Create orchestrator
    RawrXD::OrchestratorConfig config;
    RawrXD::AutonomousOrchestrator orchestrator(config);
    RawrXD::OrchestratorCLI cli(&orchestrator);

    // Check for --orchestrator mode (PowerShell integration)
    std::string orchestratorCmd;
    std::string configFile;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--orchestrator" && i + 1 < argc) {
            orchestratorCmd = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            configFile = argv[++i];
        }
    }

    // PowerShell integration mode
    if (!orchestratorCmd.empty()) {
        std::string result = cli.parseOrchestratorCommand(orchestratorCmd, configFile);
        std::cout << result << std::endl;
        return 0;
    }

    // Interactive mode
    print_banner();
    std::cout << "\nType 'help' for commands, 'exit' to quit.\n" << std::endl;

    std::string line;
    while (true) {
        std::cout << "orchestrator> ";
        std::cout.flush();

        if (!std::getline(std::cin, line)) {
            break;
        }

        if (line.empty()) continue;

        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start, end - start + 1);

        // Check for exit
        if (line == "exit" || line == "quit" || line == "q") {
            std::cout << "Goodbye!" << std::endl;
            break;
        }

        // Process command
        std::string result = cli.processCommand(line);
        std::cout << result << std::endl;
    }

    return 0;
}
