#include "agentic_ide.h"
#include <iostream>
#include <string>

// SCALAR-ONLY: Main entry point for autonomous agentic IDE

using namespace RawrXD;

int main(int argc, char* argv[]) {
    std::cout << "=======================" << std::endl;
    std::cout << "  SCALAR AGENTIC IDE   " << std::endl;
    std::cout << "  Fully Autonomous     " << std::endl;
    std::cout << "  100% Scalar Only     " << std::endl;
    std::cout << "=======================" << std::endl << std::endl;

    // Default root directory
    std::string root_directory = "C:\\Users\\HiH8e\\OneDrive\\Desktop\\Powershield";

    // Allow override from command line
    if (argc > 1) {
        root_directory = argv[1];
    }

    std::cout << "Root directory: " << root_directory << std::endl;

    // Create and initialize IDE
    AgenticIDE ide;

    if (!ide.Initialize(root_directory)) {
        std::cerr << "Failed to initialize IDE" << std::endl;
        return 1;
    }

    std::cout << std::endl;
    std::cout << "IDE Features:" << std::endl;
    std::cout << "  ✓ Scalar HTTP/WebSocket Server (port 8080)" << std::endl;
    std::cout << "  ✓ File Browser Tree (recursive)" << std::endl;
    std::cout << "  ✓ Two-Way AI Chat Interface" << std::endl;
    std::cout << "  ✓ Autonomous Agentic Engine" << std::endl;
    std::cout << "  ✓ No Threading (100% Scalar)" << std::endl;
    std::cout << "  ✓ No GPU/SIMD (Pure CPU)" << std::endl;
    std::cout << std::endl;

    std::cout << "Available Commands:" << std::endl;
    std::cout << "  /agent <task>   - Execute autonomous agent task" << std::endl;
    std::cout << "  /search <query> - Search project files" << std::endl;
    std::cout << "  /quit           - Exit IDE" << std::endl;
    std::cout << std::endl;

    std::cout << "API Endpoints:" << std::endl;
    std::cout << "  POST http://localhost:8080/api/chat" << std::endl;
    std::cout << "  GET  http://localhost:8080/api/files" << std::endl;
    std::cout << "  POST http://localhost:8080/api/agent" << std::endl;
    std::cout << "  GET  http://localhost:8080/api/status" << std::endl;
    std::cout << std::endl;

    // Demonstrate autonomous capabilities
    std::cout << "Demonstrating autonomous capabilities..." << std::endl;

    auto* chat = ide.GetChatInterface();
    auto* agent = ide.GetAgenticEngine();
    auto* browser = ide.GetFileBrowser();

    // Show file tree
    std::cout << "\nFile Browser initialized with root: " << browser->GetRootPath() << std::endl;

    // Demonstrate chat
    chat->SendUserMessage("Hello, I need help with my project");
    chat->AddAssistantMessage("I'm your autonomous agentic assistant. I can help you with code generation, file editing, debugging, and more!");

    // Demonstrate agent task
    AgentTask demo_task;
    demo_task.id = "demo_1";
    demo_task.type = AgentTaskType::CODE_GENERATION;
    demo_task.description = "Generate a scalar matrix multiplication function";
    demo_task.status = "pending";

    std::cout << "\nQueuing demo agent task..." << std::endl;
    agent->QueueTask(demo_task);

    std::cout << "\nStarting IDE event loop..." << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl << std::endl;

    // Run IDE (scalar event loop)
    ide.Run();

    std::cout << "IDE shutdown complete" << std::endl;
    return 0;
}
