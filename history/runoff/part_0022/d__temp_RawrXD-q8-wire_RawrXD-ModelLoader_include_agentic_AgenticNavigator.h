#pragma once

#include <windows.h>
#include <string>
#include <functional>
#include <map>
#include <vector>
#include <memory>

namespace RawrXD {
namespace Agentic {

// Forward-declare UI types (Win32 implementation details)
enum class SidebarView { FileExplorer, Search, SourceControl, Debug, Extensions };
enum class PanelTab { Terminal, Output, Problems, DebugConsole };

// Navigation result structure
struct NavigationResult {
    bool success;
    std::string message;
    HWND targetWindow;
    std::string targetName;
    int errorCode;
    double executionTimeMs;
    std::string fallbackUsed;
};

// UI Element detection structure
struct UIElement {
    HWND handle;
    std::string className;
    std::string name;
    std::string text;
    RECT bounds;
    bool visible;
    bool enabled;
    int controlId;
};

// Navigation strategy enum
enum class NavigationStrategy {
    DirectAPI = 0,    // Primary: Direct Win32 API calls
    IDECommands = 1,  // Fallback: IDE command infrastructure
    Hybrid = 2,       // Both methods with cross-validation
    AutoDetect = 3    // Automatic strategy selection
};

// Fallback detection configuration
struct FallbackConfig {
    int timeoutMs = 2000;           // Timeout for direct API operations
    int maxRetries = 3;             // Maximum retry attempts
    double reliabilityThreshold = 0.8; // Minimum success rate for strategy
    bool enableLearning = true;     // Enable pattern learning
    bool enableCrossValidation = true; // Enable result verification
};

// Agentic Navigation Interface
class AgenticNavigator {
public:
    AgenticNavigator();
    ~AgenticNavigator();
    
    // Primary navigation methods
    NavigationResult navigateToFileExplorer();
    NavigationResult navigateToEditor();
    NavigationResult navigateToTerminal();
    NavigationResult navigateToCopilotChat();
    NavigationResult navigateToSidebar(SidebarView view);
    NavigationResult navigateToPanel(PanelTab tab);
    
    // UI element detection
    std::vector<UIElement> detectUIElements(const std::string& filter = "");
    UIElement findElementByName(const std::string& name);
    UIElement findElementByClass(const std::string& className);
    UIElement findElementByText(const std::string& text);
    
    // Interaction methods
    NavigationResult clickElement(const UIElement& element);
    NavigationResult sendText(const UIElement& element, const std::string& text);
    NavigationResult focusElement(const UIElement& element);
    NavigationResult executeCommand(int commandId);
    
    // Strategy management
    void setStrategy(NavigationStrategy strategy);
    NavigationStrategy getCurrentStrategy() const;
    void setFallbackConfig(const FallbackConfig& config);
    FallbackConfig getFallbackConfig() const;
    
    // Performance tracking
    double getSuccessRate(NavigationStrategy strategy) const;
    double getAverageTime(NavigationStrategy strategy) const;
    void resetStatistics();
    
    // Learning and adaptation
    void learnFromResult(const NavigationResult& result);
    NavigationStrategy recommendStrategy(const std::string& operation) const;
    
private:
    // Direct Win32 API implementation (Primary)
    NavigationResult navigateDirectAPI(const std::string& target);
    std::vector<UIElement> detectElementsDirectAPI();
    NavigationResult clickElementDirectAPI(const UIElement& element);
    NavigationResult sendTextDirectAPI(const UIElement& element, const std::string& text);
    
    // IDE Command implementation (Fallback)
    NavigationResult navigateIDECommands(const std::string& target);
    NavigationResult clickElementIDECommands(const UIElement& element);
    NavigationResult sendTextIDECommands(const UIElement& element, const std::string& text);
    
    // Hybrid implementation
    NavigationResult navigateHybrid(const std::string& target);
    NavigationResult validateResults(const NavigationResult& directResult, 
                                   const NavigationResult& commandResult);
    
    // Utility functions
    bool isElementValid(const UIElement& element) const;
    std::string getElementInfo(const UIElement& element) const;
    NavigationResult createResult(bool success, const std::string& message, 
                                HWND target = nullptr, const std::string& fallback = "");
    
    // Internal state
    NavigationStrategy m_currentStrategy;
    FallbackConfig m_fallbackConfig;
    std::map<NavigationStrategy, std::vector<NavigationResult>> m_history;
    std::map<std::string, double> m_strategySuccessRates;
    std::map<std::string, double> m_strategyAverageTimes;
    
    // Pattern learning
    std::map<std::string, NavigationStrategy> m_learnedPatterns;
    std::map<std::string, int> m_patternCounts;
};

// Global navigator instance
std::unique_ptr<AgenticNavigator> CreateAgenticNavigator();

} // namespace Agentic
} // namespace RawrXD