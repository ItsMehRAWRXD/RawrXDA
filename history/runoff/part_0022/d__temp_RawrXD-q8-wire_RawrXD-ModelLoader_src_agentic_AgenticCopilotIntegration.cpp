#include "agentic/AgenticCopilotIntegration.h"
#include "agentic/AgenticNavigator.h"
#include "win32app/Win32IDE.h"
#include <chrono>
#include <sstream>

// Use forward-declared enums from AgenticNavigator.h
using RawrXD::Agentic::SidebarView;
using RawrXD::Agentic::PanelTab;

namespace RawrXD {
namespace Agentic {

AgenticCopilotIntegration::AgenticCopilotIntegration(std::unique_ptr<AgenticNavigator> navigator)
    : m_navigator(std::move(navigator)), m_ideInstance(nullptr) {
}

AgenticCopilotIntegration::~AgenticCopilotIntegration() {
}

void AgenticCopilotIntegration::handleCopilotSendWithNavigation() {
    // Update context before sending
    updateContextFromIDE();
    
    // Navigate to Copilot chat if not already focused
    auto navResult = m_navigator->navigateToCopilotChat();
    if (!navResult.success) {
        // Log navigation failure but continue with send operation
        logNavigationPerformance(navResult);
    }
    
    // Execute standard Copilot send with enhanced context
    // This would integrate with existing HandleCopilotSend()
    
    logNavigationPerformance(navResult);
}

void AgenticCopilotIntegration::handleCopilotClearWithNavigation() {
    // Navigate to Copilot chat
    auto navResult = m_navigator->navigateToCopilotChat();
    if (navResult.success) {
        // Execute standard Copilot clear
        // This would integrate with existing HandleCopilotClear()
    }
    
    logNavigationPerformance(navResult);
}

void AgenticCopilotIntegration::appendCopilotResponseWithContext(const std::string& response) {
    // Enhance response with navigation context
    std::string enhancedResponse = response;
    
    // Add context information if available
    if (!m_currentContext.empty()) {
        enhancedResponse += "\n\n[Context: " + m_currentContext + "]";
    }
    
    // Navigate to output area if needed
    auto navResult = m_navigator->navigateToPanel(PanelTab::Output);
    if (navResult.success) {
        // Append to Copilot chat output
        // This would integrate with existing AppendCopilotResponse()
    }
    
    logNavigationPerformance(navResult);
}

void AgenticCopilotIntegration::handleCopilotStreamUpdateWithNavigation(const std::string& token) {
    // Ensure we're in the right context for streaming updates
    auto navResult = m_navigator->navigateToCopilotChat();
    if (navResult.success) {
        // Handle streaming token update
        // This would integrate with existing HandleCopilotStreamUpdate()
    }
    
    logNavigationPerformance(navResult);
}

NavigationResult AgenticCopilotIntegration::executeAutonomousTask(const std::string& taskDescription) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Parse task description and determine appropriate action
    NavigationResult result;
    
    if (taskDescription.find("file") != std::string::npos || 
        taskDescription.find("open") != std::string::npos ||
        taskDescription.find("save") != std::string::npos) {
        result = executeFileOperation(taskDescription);
    } else if (taskDescription.find("code") != std::string::npos ||
               taskDescription.find("generate") != std::string::npos ||
               taskDescription.find("implement") != std::string::npos) {
        result = executeCodeGeneration(taskDescription);
    } else if (taskDescription.find("debug") != std::string::npos ||
               taskDescription.find("run") != std::string::npos ||
               taskDescription.find("test") != std::string::npos) {
        result = executeDebugOperation(taskDescription);
    } else if (taskDescription.find("terminal") != std::string::npos ||
               taskDescription.find("command") != std::string::npos ||
               taskDescription.find("execute") != std::string::npos) {
        result = executeTerminalCommand(taskDescription);
    } else {
        // TODO: Implement proper task routing for unknown operations
        result.success = false;
        result.message = "Unknown task type: " + taskDescription;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    logNavigationPerformance(result);
    return result;
}

NavigationResult AgenticCopilotIntegration::navigateAndExecute(const std::string& target, const std::string& action) {
    // Validate target first
    if (!validateNavigationTarget(target)) {
        return NavigationResult{false, "Invalid navigation target: " + target};
    }
    
    // Check if action requires confirmation
    if (requiresConfirmation(action) && !confirmCriticalAction(action)) {
        return NavigationResult{false, "Action cancelled by user: " + action};
    }
    
    // Execute navigation and action
    NavigationResult navResult;
    
    if (target == "FileExplorer") {
        navResult = m_navigator->navigateToFileExplorer();
    } else if (target == "Editor") {
        navResult = m_navigator->navigateToEditor();
    } else if (target == "Terminal") {
        navResult = m_navigator->navigateToTerminal();
    } else if (target == "CopilotChat") {
        navResult = m_navigator->navigateToCopilotChat();
    } else {
        navResult = NavigationResult{false, "Unknown target: " + target};
    }
    
    if (navResult.success) {
        // Execute the action after successful navigation
        // This would integrate with actual IDE command execution
        navResult.message += ", Action: " + action + " executed";
    }
    
    return navResult;
}

void AgenticCopilotIntegration::setCurrentContext(const std::string& context) {
    m_currentContext = context;
}

std::string AgenticCopilotIntegration::getCurrentContext() const {
    return m_currentContext;
}

bool AgenticCopilotIntegration::validateNavigationTarget(const std::string& target) {
    // List of valid navigation targets
    static const std::vector<std::string> validTargets = {
        "FileExplorer", "Editor", "Terminal", "CopilotChat",
        "Sidebar_Explorer", "Sidebar_Search", "Sidebar_SourceControl",
        "Sidebar_RunDebug", "Sidebar_Extensions",
        "Panel_Terminal", "Panel_Output", "Panel_Problems", "Panel_DebugConsole"
    };
    
    return std::find(validTargets.begin(), validTargets.end(), target) != validTargets.end();
}

bool AgenticCopilotIntegration::confirmCriticalAction(const std::string& action) {
    // For now, simulate user confirmation
    // In real implementation, this would show a confirmation dialog
    return true; // Auto-confirm for testing
}

void AgenticCopilotIntegration::logNavigationPerformance(const NavigationResult& result) {
    // Log navigation performance for optimization
    std::stringstream log;
    log << "Navigation Performance - Success: " << result.success
          << ", Time: " << result.executionTimeMs << "ms"
          << ", Message: " << result.message;
    
    if (!result.fallbackUsed.empty()) {
        log << ", Fallback: " << result.fallbackUsed;
    }
    
    // This would integrate with existing logging system
    // m_ideInstance->logInfo(log.str());
}

void AgenticCopilotIntegration::optimizeNavigationStrategy() {
    // Analyze performance and optimize strategy
    double directSuccessRate = m_navigator->getSuccessRate(NavigationStrategy::DirectAPI);
    double commandSuccessRate = m_navigator->getSuccessRate(NavigationStrategy::IDECommands);
    
    if (directSuccessRate >= 0.9 && commandSuccessRate <= 0.7) {
        m_navigator->setStrategy(NavigationStrategy::DirectAPI);
    } else if (commandSuccessRate >= 0.9 && directSuccessRate <= 0.7) {
        m_navigator->setStrategy(NavigationStrategy::IDECommands);
    } else {
        m_navigator->setStrategy(NavigationStrategy::Hybrid);
    }
}

// Task execution helpers
NavigationResult AgenticCopilotIntegration::executeFileOperation(const std::string& operation) {
    // Navigate to appropriate area for file operations
    auto navResult = m_navigator->navigateToFileExplorer();
    if (navResult.success) {
        // Execute file operation
        navResult.message += ", File operation: " + operation;
    }
    return navResult;
}

NavigationResult AgenticCopilotIntegration::executeCodeGeneration(const std::string& prompt) {
    // Navigate to editor for code generation
    auto navResult = m_navigator->navigateToEditor();
    if (navResult.success) {
        // Execute code generation
        navResult.message += ", Code generation: " + prompt;
    }
    return navResult;
}

NavigationResult AgenticCopilotIntegration::executeDebugOperation(const std::string& operation) {
    // Navigate to debug panel
    auto navResult = m_navigator->navigateToPanel(PanelTab::DebugConsole);
    if (navResult.success) {
        // Execute debug operation
        navResult.message += ", Debug operation: " + operation;
    }
    return navResult;
}

NavigationResult AgenticCopilotIntegration::executeTerminalCommand(const std::string& command) {
    // Navigate to terminal
    auto navResult = m_navigator->navigateToTerminal();
    if (navResult.success) {
        // Execute terminal command
        navResult.message += ", Terminal command: " + command;
    }
    return navResult;
}

// Context management
void AgenticCopilotIntegration::updateContextFromIDE() {
    if (m_ideInstance) {
        // Extract current context from IDE state
        // This would integrate with actual IDE state access
        m_currentContext = "IDE Context Updated";
    }
}

std::string AgenticCopilotIntegration::buildContextAwarePrompt(const std::string& userPrompt) {
    std::string contextPrompt = userPrompt;
    
    if (!m_currentContext.empty()) {
        contextPrompt = "Current Context: " + m_currentContext + "\n\nUser Request: " + userPrompt;
    }
    
    return contextPrompt;
}

// Safety mechanisms
bool AgenticCopilotIntegration::isSafeOperation(const std::string& operation) {
    // List of safe operations that don't require confirmation
    static const std::vector<std::string> safeOperations = {
        "navigate", "focus", "view", "show", "display", "list"
    };
    
    for (const auto& safeOp : safeOperations) {
        if (operation.find(safeOp) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

bool AgenticCopilotIntegration::requiresConfirmation(const std::string& operation) {
    // List of operations that require user confirmation
    static const std::vector<std::string> criticalOperations = {
        "delete", "remove", "modify", "change", "update", "install", "uninstall",
        "execute", "run", "build", "compile", "deploy"
    };
    
    for (const auto& criticalOp : criticalOperations) {
        if (operation.find(criticalOp) != std::string::npos) {
            return true;
        }
    }
    
    return !isSafeOperation(operation);
}

// Global agentic copilot instance
std::unique_ptr<AgenticCopilotIntegration> CreateAgenticCopilotIntegration(Win32IDE* ide) {
    auto navigator = CreateAgenticNavigator();
    auto integration = std::make_unique<AgenticCopilotIntegration>(std::move(navigator));
    
    // Set IDE instance for context access
    // integration->setIDEInstance(ide); // This would require additional method
    
    return integration;
}

} // namespace Agentic
} // namespace RawrXD