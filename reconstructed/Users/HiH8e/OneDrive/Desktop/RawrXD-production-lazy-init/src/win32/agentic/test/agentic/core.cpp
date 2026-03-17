#include "win32/agentic/model_invoker.hpp"
#include "win32/agentic/action_executor.hpp"
#include "win32/agentic/ide_agent_bridge.hpp"
#include "win32/enhanced_tool_registry.hpp"
#include <iostream>
#include <memory>

int main() {
    // Create tool registry
    auto toolRegistry = std::make_shared<EnhancedToolRegistry>();
    
    // Create model invoker
    auto modelInvoker = std::make_shared<ModelInvoker>();
    
    // Create action executor
    auto actionExecutor = std::make_shared<ActionExecutor>(toolRegistry);
    
    // Create IDE agent bridge
    auto agentBridge = std::make_shared<IDEGentBridge>(modelInvoker, actionExecutor);
    
    // Test the components
    std::cout << "Testing Agentic Core Components...\n";
    
    // Test ModelInvoker
    std::cout << "1. Testing ModelInvoker...\n";
    std::string wish = "Create a new C++ file that prints 'Hello, World!'"; 
    auto plan = modelInvoker->generatePlan(wish);
    std::cout << "Generated plan: " << plan.dump(2) << "\n\n";
    
    // Test ActionExecutor with a simple action
    std::cout << "2. Testing ActionExecutor...\n";
    nlohmann::json testPlan = {
        {"plan", {
            {{"action", "write_file"}, {"description", "Create hello world file"}, 
             {"parameters", {{"path", "hello.cpp"}, {"content", "#include <iostream>\n\nint main() {\n    std::cout << \"Hello, World!\" << std::endl;\n    return 0;\n}\n"}}}}
        }}
    };
    
    auto result = actionExecutor->executePlan(testPlan);
    std::cout << "Execution result: " << (result.success ? "SUCCESS" : "FAILED") << "\n";
    if (!result.success) {
        std::cout << "Error: " << result.errorMessage << "\n";
    }
    std::cout << "\n";
    
    // Test IDEAgentBridge
    std::cout << "3. Testing IDEAgentBridge...\n";
    agentBridge->enableDryRun(true); // Enable dry run for testing
    auto bridgeResult = agentBridge->executeWish("Create a simple calculator app");
    std::cout << "Bridge execution result: " << (bridgeResult.success ? "SUCCESS" : "FAILED") << "\n";
    if (!bridgeResult.success) {
        std::cout << "Error: " << bridgeResult.errorMessage << "\n";
    }
    
    std::cout << "\nAll tests completed!\n";
    return 0;
}