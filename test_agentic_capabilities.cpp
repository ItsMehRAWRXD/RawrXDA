// Test file for Ollama model provider agentic capabilities
#include "src/extensions/ollama_model_provider.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace RawrXD::Extensions::Ollama;

int main() {
    // Create provider instance
    OllamaModelProvider* provider = CreateOllamaProvider();
    
    // Initialize with default config
    provider->Initialize("{}");
    
    // Test multi-file operation
    MultiFileRequest multiFileReq;
    multiFileReq.operation = "analyze_and_refactor";
    multiFileReq.instruction = "Analyze these files for performance issues and suggest refactoring";
    multiFileReq.context = "C++ codebase with performance optimization needs";
    multiFileReq.filePaths = {"src/main.cpp", "src/utils.cpp", "include/utils.h"};
    
    std::string result = provider->ProcessMultiFileOperation(multiFileReq);
    std::cout << "Multi-file operation result: " << result << std::endl;
    
    // Test autonomous task
    AutonomousTask task;
    task.goal = "Refactor the entire codebase for better performance";
    task.maxSteps = 10;
    task.timeoutMs = 30000;
    
    AgenticSession session = provider->StartAutonomousTask(task);
    std::cout << "Started autonomous session: " << session.sessionId << std::endl;
    
    // Test tool execution
    ToolExecutionResult toolResult = provider->ExecuteTool("file_read", "src/main.cpp");
    if (toolResult.success) {
        std::cout << "File read successful, content length: " << toolResult.output.length() << std::endl;
    } else {
        std::cout << "File read failed: " << toolResult.error << std::endl;
    }
    
    // Test context management
    provider->AddToContext("project_type", "C++ performance optimization");
    std::string context = provider->GetFromContext("project_type");
    std::cout << "Context value: " << context << std::endl;
    
    // Test codebase analysis
    std::string analysis = provider->AnalyzeCodebase("src", "performance");
    std::cout << "Codebase analysis: " << analysis << std::endl;
    
    // Test documentation generation
    std::vector<std::string> files = {"src/main.cpp", "include/utils.h"};
    std::string docs = provider->GenerateDocumentation(files);
    std::cout << "Generated documentation: " << docs << std::endl;
    
    // Test refactoring
    std::string refactored = provider->RefactorCode(files, "optimize_performance");
    std::cout << "Refactored code: " << refactored << std::endl;
    
    // Get available tools
    auto tools = provider->GetAvailableTools();
    std::cout << "Available tools: ";
    for (const auto& tool : tools) {
        std::cout << tool << " ";
    }
    std::cout << std::endl;
    
    // Get active sessions
    auto sessions = provider->GetActiveSessions();
    std::cout << "Active sessions: " << sessions.size() << std::endl;
    
    // Cleanup
    DestroyOllamaProvider(provider);
    
    return 0;
}