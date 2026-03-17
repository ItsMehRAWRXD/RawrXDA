// cli_main.cpp - Command-line interface for Orchestra task execution
// ============================================================================
// This file provides a complete CLI for submitting, executing, and monitoring
// tasks through the production agent orchestration system.
// ============================================================================

#include "orchestra_integration.h"
#include <iostream>
#include <iomanip>

/**
 * @brief Interactive mode for CLI orchestra
 * Allows users to submit multiple tasks and execute them interactively
 */
void runInteractiveMode() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║     AI Orchestra CLI - Interactive Mode                   ║\n";
    std::cout << "║     Type 'help' for available commands                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    auto& manager = OrchestraManager::getInstance();
    std::string input;

    while (true) {
        std::cout << "orchestra> ";
        std::getline(std::cin, input);

        if (input.empty()) continue;

        // Parse command
        std::istringstream iss(input);
        std::string command;
        iss >> command;

        if (command == "help") {
            std::cout << "\nAvailable Commands:\n";
            std::cout << "  add <id> <goal> [file]   - Add a task\n";
            std::cout << "  list                     - List pending tasks\n";
            std::cout << "  execute [num_agents]     - Execute all pending tasks\n";
            std::cout << "  result <id>              - Show task result\n";
            std::cout << "  results                  - Show all results\n";
            std::cout << "  progress                 - Show execution progress\n";
            std::cout << "  clear                    - Clear all tasks\n";
            std::cout << "  quit                     - Exit\n\n";
        }
        else if (command == "add") {
            std::string id, goal, file;
            
            if (!(iss >> id >> goal)) {
                std::cout << "✗ Usage: add <id> <goal> [file]\n";
                continue;
            }
            
            iss >> file;
            
            manager.submitTask(id, goal, file);
            std::cout << "✓ Task added: " << id << "\n";
            std::cout << "  Goal: " << goal << "\n";
            if (!file.empty()) {
                std::cout << "  File: " << file << "\n";
            }
        }
        else if (command == "list") {
            size_t count = manager.getPendingTaskCount();
            std::cout << "Pending tasks: " << count << "\n";
        }
        else if (command == "execute") {
            int num_agents = 4;
            iss >> num_agents;
            
            std::cout << "\nExecuting tasks with " << num_agents << " agents...\n";
            auto metrics = manager.executeAllTasks(num_agents);
            CLIOrchestraHelper::printMetrics(metrics);
        }
        else if (command == "result") {
            std::string task_id;
            if (!(iss >> task_id)) {
                std::cout << "✗ Usage: result <task_id>\n";
                continue;
            }
            
            auto result = manager.getTaskResult(task_id);
            if (result) {
                CLIOrchestraHelper::printTaskResult(*result);
            } else {
                std::cout << "✗ Task not found: " << task_id << "\n";
            }
        }
        else if (command == "results") {
            CLIOrchestraHelper::printAllResults();
        }
        else if (command == "progress") {
            double progress = manager.getProgressPercentage();
            size_t pending = manager.getPendingTaskCount();
            
            std::cout << "\nProgress: " << std::fixed << std::setprecision(1) 
                     << progress << "%\n";
            std::cout << "Pending tasks: " << pending << "\n\n";
        }
        else if (command == "clear") {
            manager.clearAllTasks();
            std::cout << "✓ All tasks cleared\n";
        }
        else if (command == "quit" || command == "exit") {
            std::cout << "\nGoodbye!\n";
            break;
        }
        else {
            std::cout << "✗ Unknown command: " << command << "\n";
            std::cout << "Type 'help' for available commands.\n";
        }
    }
}

/**
 * @brief Batch mode for CLI orchestra
 * Executes commands from command-line arguments
 */
void runBatchMode(int argc, char* argv[]) {
    if (!CLIOrchestraHelper::parseAndExecute(argc, argv)) {
        CLIOrchestraHelper::printUsage(argv[0]);
    }
}

/**
 * @brief Demo mode - shows orchestra capabilities
 */
void runDemoMode() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║     AI Orchestra CLI - Demo Mode                          ║\n";
    std::cout << "║     Demonstrating parallel task execution                 ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    auto& manager = OrchestraManager::getInstance();

    // Submit sample tasks
    std::cout << "[1] Submitting sample tasks...\n";
    manager.submitTask("task-001", "Analyze security vulnerabilities", "main.cpp");
    manager.submitTask("task-002", "Review performance metrics", "performance.cpp");
    manager.submitTask("task-003", "Check code maintainability", "utils.cpp");
    manager.submitTask("task-004", "Validate error handling", "error_handler.cpp");
    manager.submitTask("task-005", "Audit dependency graph", "dependencies.txt");

    std::cout << "✓ 5 tasks submitted\n\n";

    // Execute with 4 agents
    std::cout << "[2] Executing with 4 parallel agents...\n\n";
    auto metrics = manager.executeAllTasks(4);

    // Display results
    CLIOrchestraHelper::printMetrics(metrics);

    // Show detailed results
    std::cout << "[3] Detailed task results:\n\n";
    CLIOrchestraHelper::printAllResults();
}

/**
 * @brief Print help information
 */
void printHelpInfo() {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║     AI Orchestra CLI - Task Execution System              ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "USAGE:\n";
    std::cout << "  orchestra-cli [MODE] [ARGUMENTS]\n\n";

    std::cout << "MODES:\n";
    std::cout << "  interactive         - Interactive shell mode (default)\n";
    std::cout << "  demo                - Run demonstration with sample tasks\n";
    std::cout << "  batch               - Execute single command from arguments\n";
    std::cout << "  help                - Show this help message\n\n";

    std::cout << "BATCH COMMANDS:\n";
    std::cout << "  submit-task <id> <goal> [file]  - Submit a task\n";
    std::cout << "  execute <num_agents>            - Execute all tasks\n";
    std::cout << "  result <task_id>                - Get task result\n";
    std::cout << "  list                            - List all results\n";
    std::cout << "  clear                           - Clear all tasks\n\n";

    std::cout << "EXAMPLES:\n";
    std::cout << "  orchestra-cli\n";
    std::cout << "    -> Starts interactive mode\n\n";

    std::cout << "  orchestra-cli demo\n";
    std::cout << "    -> Runs demo with 5 sample tasks\n\n";

    std::cout << "  orchestra-cli batch submit-task task1 \"Analyze code\" main.cpp\n";
    std::cout << "    -> Submits a single task in batch mode\n\n";

    std::cout << "  orchestra-cli batch execute 4\n";
    std::cout << "    -> Executes all tasks with 4 agents\n\n";

    std::cout << "  orchestra-cli batch result task1\n";
    std::cout << "    -> Shows result for task1\n\n";
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    try {
        // Default to interactive mode if no arguments
        if (argc == 1) {
            runInteractiveMode();
        }
        else {
            std::string mode = argv[1];

            if (mode == "interactive" || mode == "-i") {
                runInteractiveMode();
            }
            else if (mode == "demo" || mode == "-d") {
                runDemoMode();
            }
            else if (mode == "batch" || mode == "-b") {
                // Shift arguments for batch mode
                int new_argc = argc - 2;
                char** new_argv = argv + 2;
                
                if (new_argc >= 2) {
                    runBatchMode(new_argc, new_argv);
                } else {
                    std::cout << "✗ Batch mode requires at least 2 arguments\n";
                    std::cout << "Usage: orchestra-cli batch <command> [args...]\n";
                }
            }
            else if (mode == "help" || mode == "-h" || mode == "--help") {
                printHelpInfo();
            }
            else {
                // Try to interpret as batch command
                runBatchMode(argc, argv);
            }
        }

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\n✗ Fatal Error: " << e.what() << "\n";
        return 1;
    }
}
