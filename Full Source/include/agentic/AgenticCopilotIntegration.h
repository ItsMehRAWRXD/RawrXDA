#pragma once

#include "agentic/AgenticNavigator.h"
#include "win32app/Win32IDE.h"
#include <string>
#include <memory>

namespace RawrXD {
namespace Agentic {

// Enhanced Copilot integration with autonomous navigation
class AgenticCopilotIntegration {
public:
    AgenticCopilotIntegration(std::unique_ptr<AgenticNavigator> navigator);
    ~AgenticCopilotIntegration();
    
    // Enhanced Copilot methods with autonomous capabilities
    void handleCopilotSendWithNavigation();
    void handleCopilotClearWithNavigation();
    void appendCopilotResponseWithContext(const std::string& response);
    void handleCopilotStreamUpdateWithNavigation(const std::string& token);
    
    // Autonomous task execution
    NavigationResult executeAutonomousTask(const std::string& taskDescription);
    NavigationResult navigateAndExecute(const std::string& target, const std::string& action);
    
    // Context-aware operations
    void setCurrentContext(const std::string& context);
    std::string getCurrentContext() const;
    
    // Safety and validation
    bool validateNavigationTarget(const std::string& target);
    bool confirmCriticalAction(const std::string& action);
    
    // Performance monitoring
    void logNavigationPerformance(const NavigationResult& result);
    void optimizeNavigationStrategy();
    
private:
    std::unique_ptr<AgenticNavigator> m_navigator;
    std::string m_currentContext;
    Win32IDE* m_ideInstance;
    
    // Task execution helpers
    NavigationResult executeFileOperation(const std::string& operation);
    NavigationResult executeCodeGeneration(const std::string& prompt);
    NavigationResult executeDebugOperation(const std::string& operation);
    NavigationResult executeTerminalCommand(const std::string& command);
    
    // Context management
    void updateContextFromIDE();
    std::string buildContextAwarePrompt(const std::string& userPrompt);
    
    // Safety mechanisms
    bool isSafeOperation(const std::string& operation);
    bool requiresConfirmation(const std::string& operation);
};

// Global agentic copilot instance
std::unique_ptr<AgenticCopilotIntegration> CreateAgenticCopilotIntegration(Win32IDE* ide);

} // namespace Agentic
} // namespace RawrXD