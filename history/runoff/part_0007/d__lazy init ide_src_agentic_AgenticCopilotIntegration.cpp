#include "agentic/AgenticCopilotIntegration.h"
#include "win32app/Win32IDE.h"
#include <chrono>
#include <sstream>

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

        #include "agentic/AgenticCopilotIntegration.h"
        #include "win32app/Win32IDE.h"
        #include <chrono>
        #include <sstream>

        namespace RawrXD {
        namespace Agentic {

        AgenticCopilotIntegration::AgenticCopilotIntegration(std::unique_ptr<AgenticNavigator> navigator)
            : m_navigator(std::move(navigator)), m_ideInstance(nullptr) {
        }

        AgenticCopilotIntegration::~AgenticCopilotIntegration() {
        }

        void AgenticCopilotIntegration::handleCopilotSendWithNavigation() {
            updateContextFromIDE();
            auto navResult = m_navigator->navigateToCopilotChat();
            if (!navResult.success) logNavigationPerformance(navResult);
            logNavigationPerformance(navResult);
        }

        void AgenticCopilotIntegration::handleCopilotClearWithNavigation() {
            auto navResult = m_navigator->navigateToCopilotChat();
            logNavigationPerformance(navResult);
        }

        void AgenticCopilotIntegration::appendCopilotResponseWithContext(const std::string& response) {
            std::string enhancedResponse = response;
            if (!m_currentContext.empty()) {
                enhancedResponse += "\n\n[Context: " + m_currentContext + "]";
            }
            auto navResult = m_navigator->navigateToPanel(PanelTab::Output);
            logNavigationPerformance(navResult);
        }

        void AgenticCopilotIntegration::handleCopilotStreamUpdateWithNavigation(const std::string& token) {
            auto navResult = m_navigator->navigateToCopilotChat();
            logNavigationPerformance(navResult);
        }

        NavigationResult AgenticCopilotIntegration::executeAutonomousTask(const std::string& taskDescription) {
            auto start = std::chrono::high_resolution_clock::now();
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
                result = navigateAndExecute("Unknown", taskDescription);
            }
            auto end = std::chrono::high_resolution_clock::now();
            result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
            logNavigationPerformance(result);
            return result;
        }

        NavigationResult AgenticCopilotIntegration::navigateAndExecute(const std::string& target, const std::string& action) {
            if (!validateNavigationTarget(target)) {
                return NavigationResult{false, "Invalid navigation target: " + target};
            }
            if (requiresConfirmation(action) && !confirmCriticalAction(action)) {
                return NavigationResult{false, "Action cancelled by user: " + action};
            }
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
            static const std::vector<std::string> validTargets = {
                "FileExplorer", "Editor", "Terminal", "CopilotChat",
                "Sidebar_Explorer", "Sidebar_Search", "Sidebar_SourceControl",
                "Sidebar_RunDebug", "Sidebar_Extensions",
                "Panel_Terminal", "Panel_Output", "Panel_Problems", "Panel_DebugConsole"
            };
            return std::find(validTargets.begin(), validTargets.end(), target) != validTargets.end();
        }

        bool AgenticCopilotIntegration::confirmCriticalAction(const std::string& action) {
            return true;
        }

        void AgenticCopilotIntegration::logNavigationPerformance(const NavigationResult& result) {
            std::stringstream log;
            log << "Navigation Performance - Success: " << result.success
                << ", Time: " << result.executionTimeMs << "ms"
                << ", Message: " << result.message;
            if (!result.fallbackUsed.empty()) {
                log << ", Fallback: " << result.fallbackUsed;
            }
        }

        void AgenticCopilotIntegration::optimizeNavigationStrategy() {
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

        NavigationResult AgenticCopilotIntegration::executeFileOperation(const std::string& operation) {
            auto navResult = m_navigator->navigateToFileExplorer();
            if (navResult.success) {
                navResult.message += ", File operation: " + operation;
            }
            return navResult;
        }

        NavigationResult AgenticCopilotIntegration::executeCodeGeneration(const std::string& prompt) {
            auto navResult = m_navigator->navigateToEditor();
            if (navResult.success) {
                navResult.message += ", Code generation: " + prompt;
            }
            return navResult;
        }

        NavigationResult AgenticCopilotIntegration::executeDebugOperation(const std::string& operation) {
            auto navResult = m_navigator->navigateToPanel(PanelTab::DebugConsole);
            if (navResult.success) {
                navResult.message += ", Debug operation: " + operation;
            }
            return navResult;
        }

        NavigationResult AgenticCopilotIntegration::executeTerminalCommand(const std::string& command) {
            auto navResult = m_navigator->navigateToTerminal();
            if (navResult.success) {
                navResult.message += ", Terminal command: " + command;
            }
            return navResult;
        }

        void AgenticCopilotIntegration::updateContextFromIDE() {
            if (m_ideInstance) {
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

        bool AgenticCopilotIntegration::isSafeOperation(const std::string& operation) {
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

        std::unique_ptr<AgenticCopilotIntegration> CreateAgenticCopilotIntegration(Win32IDE* ide) {
            auto navigator = CreateAgenticNavigator();
            auto integration = std::make_unique<AgenticCopilotIntegration>(std::move(navigator));
            return integration;
        }

        } // namespace Agentic
        } // namespace RawrXD
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