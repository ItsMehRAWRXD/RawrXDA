#include "agentic/AgenticNavigator.h"
#include "win32app/Win32IDE.h"
#include <windows.h>
#include <chrono>
#include <thread>
#include <algorithm>
#include <random>

namespace RawrXD {
namespace Agentic {

AgenticNavigator::AgenticNavigator() 
    : m_currentStrategy(NavigationStrategy::AutoDetect) {
    m_fallbackConfig = FallbackConfig{};
}

AgenticNavigator::~AgenticNavigator() {
}

NavigationResult AgenticNavigator::navigateToFileExplorer() {
    auto start = std::chrono::high_resolution_clock::now();
    
    NavigationResult result;
    switch (m_currentStrategy) {
        case NavigationStrategy::DirectAPI:
            result = navigateDirectAPI("FileExplorer");
            break;
        case NavigationStrategy::IDECommands:
            result = navigateIDECommands("FileExplorer");
            break;
        case NavigationStrategy::Hybrid:
            result = navigateHybrid("FileExplorer");
            break;
        case NavigationStrategy::AutoDetect:
            result = navigateHybrid("FileExplorer");
            break;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    learnFromResult(result);
    return result;
}

NavigationResult AgenticNavigator::navigateToEditor() {
    auto start = std::chrono::high_resolution_clock::now();
    
    NavigationResult result;
    switch (m_currentStrategy) {
        case NavigationStrategy::DirectAPI:
            result = navigateDirectAPI("Editor");
            break;
        case NavigationStrategy::IDECommands:
            result = navigateIDECommands("Editor");
            break;
        case NavigationStrategy::Hybrid:
            result = navigateHybrid("Editor");
            break;
        case NavigationStrategy::AutoDetect:
            result = navigateHybrid("Editor");
            break;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    learnFromResult(result);
    return result;
}

NavigationResult AgenticNavigator::navigateToTerminal() {
    auto start = std::chrono::high_resolution_clock::now();
    
    NavigationResult result;
    switch (m_currentStrategy) {
        case NavigationStrategy::DirectAPI:
            result = navigateDirectAPI("Terminal");
            break;
        case NavigationStrategy::IDECommands:
            result = navigateIDECommands("Terminal");
            break;
        case NavigationStrategy::Hybrid:
            result = navigateHybrid("Terminal");
            break;
        case NavigationStrategy::AutoDetect:
            result = navigateHybrid("Terminal");
            break;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    learnFromResult(result);
    return result;
}

NavigationResult AgenticNavigator::navigateToCopilotChat() {
    auto start = std::chrono::high_resolution_clock::now();
    
    NavigationResult result;
    switch (m_currentStrategy) {
        case NavigationStrategy::DirectAPI:
            result = navigateDirectAPI("CopilotChat");
            break;
        case NavigationStrategy::IDECommands:
            result = navigateIDECommands("CopilotChat");
            break;
        case NavigationStrategy::Hybrid:
            result = navigateHybrid("CopilotChat");
            break;
        case NavigationStrategy::AutoDetect:
            result = navigateHybrid("CopilotChat");
            break;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    learnFromResult(result);
    return result;
}

NavigationResult AgenticNavigator::navigateToSidebar(SidebarView view) {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string target = "Sidebar_" + std::to_string(static_cast<int>(view));
    NavigationResult result;
    switch (m_currentStrategy) {
        case NavigationStrategy::DirectAPI:
            result = navigateDirectAPI(target);
            break;
        case NavigationStrategy::IDECommands:
            result = navigateIDECommands(target);
            break;
        case NavigationStrategy::Hybrid:
            result = navigateHybrid(target);
            break;
        case NavigationStrategy::AutoDetect:
            result = navigateHybrid(target);
            break;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    learnFromResult(result);
    return result;
}

NavigationResult AgenticNavigator::navigateToPanel(PanelTab tab) {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string target = "Panel_" + std::to_string(static_cast<int>(tab));
    NavigationResult result;
    switch (m_currentStrategy) {
        case NavigationStrategy::DirectAPI:
            result = navigateDirectAPI(target);
            break;
        case NavigationStrategy::IDECommands:
            result = navigateIDECommands(target);
            break;
        case NavigationStrategy::Hybrid:
            result = navigateHybrid(target);
            break;
        case NavigationStrategy::AutoDetect:
            result = navigateHybrid(target);
            break;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    learnFromResult(result);
    return result;
}

NavigationResult AgenticNavigator::navigateAndExecute(const std::string& target, int commandId) {
    // First navigate to the target
    auto navResult = navigateDirectAPI(target);
    if (!navResult.success) {
        return navResult;
    }
    
    // Then execute the command if provided
    if (commandId > 0) {
        return executeCommand(commandId);
    }
    
    return navResult;
}

std::vector<UIElement> AgenticNavigator::detectUIElements(const std::string& filter) {
    return detectElementsDirectAPI();
}

UIElement AgenticNavigator::findElementByName(const std::string& name) {
    auto elements = detectUIElements();
    for (const auto& element : elements) {
        if (element.name.find(name) != std::string::npos) {
            return element;
        }
    }
    return UIElement{};
}

UIElement AgenticNavigator::findElementByClass(const std::string& className) {
    auto elements = detectUIElements();
    for (const auto& element : elements) {
        if (element.className.find(className) != std::string::npos) {
            return element;
        }
    }
    return UIElement{};
}

UIElement AgenticNavigator::findElementByText(const std::string& text) {
    auto elements = detectUIElements();
    for (const auto& element : elements) {
        if (element.text.find(text) != std::string::npos) {
            return element;
        }
    }
    return UIElement{};
}

NavigationResult AgenticNavigator::clickElement(const UIElement& element) {
    if (!isElementValid(element)) {
        return createResult(false, "Invalid UI element");
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    NavigationResult result;
    
    // Try direct API first
    result = clickElementDirectAPI(element);
    
    // If direct API fails, fallback to IDE commands
    if (!result.success && m_fallbackConfig.maxRetries > 0) {
        result = clickElementIDECommands(element);
        result.fallbackUsed = "IDECommands";
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    learnFromResult(result);
    return result;
}

NavigationResult AgenticNavigator::sendText(const UIElement& element, const std::string& text) {
    if (!isElementValid(element)) {
        return createResult(false, "Invalid UI element");
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    NavigationResult result;
    
    // Try direct API first
    result = sendTextDirectAPI(element, text);
    
    // If direct API fails, fallback to IDE commands
    if (!result.success && m_fallbackConfig.maxRetries > 0) {
        result = sendTextIDECommands(element, text);
        result.fallbackUsed = "IDECommands";
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    learnFromResult(result);
    return result;
}

NavigationResult AgenticNavigator::focusElement(const UIElement& element) {
    if (!isElementValid(element)) {
        return createResult(false, "Invalid UI element");
    }
    
    if (SetFocus(element.handle) != nullptr) {
        return createResult(true, "Element focused successfully", element.handle);
    }
    
    return createResult(false, "Failed to focus element", element.handle);
}

NavigationResult AgenticNavigator::executeCommand(int commandId) {
    // This uses IDE command infrastructure directly
    auto start = std::chrono::high_resolution_clock::now();
    
    // replaced simulation with WM_COMMAND
#ifdef _WIN32
    // Assuming we have a cached handle to the main window or can find it.
    // For now, let's look for "RawrXD" or generic IDE window if not stored.
    HWND hIDE = FindWindowA("RawrXD_IDE_Window", NULL); // Adjust class name as needed
    if (!hIDE) hIDE = GetForegroundWindow(); // Fallback to active window for context actions

    bool success = false;
    std::string message;

    if (hIDE) {
        // Send WM_COMMAND with the specific Command ID
        // Note: Command IDs must match the resource.h or internal mapping
        PostMessage(hIDE, WM_COMMAND, (WPARAM)commandId, 0);
        success = true;
        message = "Command dispatched via Win32 API";
    } else {
        message = "IDE Window not found for command execution";
    }
#else
    bool success = true; 
    std::string message = "Platform specific command execution not implemented (Non-Windows)";
#endif
    
    auto end = std::chrono::high_resolution_clock::now();
    double executionTime = std::chrono::duration<double, std::milli>(end - start).count();
    
    NavigationResult result = createResult(success, message);
    result.executionTimeMs = executionTime;
    
    learnFromResult(result);
    return result;
}

void AgenticNavigator::setStrategy(NavigationStrategy strategy) {
    m_currentStrategy = strategy;
}

NavigationStrategy AgenticNavigator::getCurrentStrategy() const {
    return m_currentStrategy;
}

void AgenticNavigator::setFallbackConfig(const FallbackConfig& config) {
    m_fallbackConfig = config;
}

FallbackConfig AgenticNavigator::getFallbackConfig() const {
    return m_fallbackConfig;
}

double AgenticNavigator::getSuccessRate(NavigationStrategy strategy) const {
    auto it = m_strategySuccessRates.find(std::to_string(static_cast<int>(strategy)));
    return it != m_strategySuccessRates.end() ? it->second : 0.0;
}

double AgenticNavigator::getAverageTime(NavigationStrategy strategy) const {
    auto it = m_strategyAverageTimes.find(std::to_string(static_cast<int>(strategy)));
    return it != m_strategyAverageTimes.end() ? it->second : 0.0;
}

void AgenticNavigator::resetStatistics() {
    m_history.clear();
    m_strategySuccessRates.clear();
    m_strategyAverageTimes.clear();
    m_learnedPatterns.clear();
    m_patternCounts.clear();
}

void AgenticNavigator::learnFromResult(const NavigationResult& result) {
    // Update history
    m_history[m_currentStrategy].push_back(result);
    
    // Update success rates and average times
    std::string strategyKey = std::to_string(static_cast<int>(m_currentStrategy));
    
    // Calculate success rate
    int total = m_history[m_currentStrategy].size();
    int successes = 0;
    double totalTime = 0.0;
    
    for (const auto& res : m_history[m_currentStrategy]) {
        if (res.success) successes++;
        totalTime += res.executionTimeMs;
    }
    
    m_strategySuccessRates[strategyKey] = static_cast<double>(successes) / total;
    m_strategyAverageTimes[strategyKey] = totalTime / total;
    
    // Pattern learning
    if (m_fallbackConfig.enableLearning) {
        std::string operationPattern = result.targetName + "_" + std::to_string(result.success);
        m_learnedPatterns[operationPattern] = m_currentStrategy;
        m_patternCounts[operationPattern]++;
    }
}

NavigationStrategy AgenticNavigator::recommendStrategy(const std::string& operation) const {
    // Look for learned patterns
    for (const auto& pattern : m_learnedPatterns) {
        if (pattern.first.find(operation) != std::string::npos) {
            return pattern.second;
        }
    }
    
    // Default to hybrid for unknown operations
    return NavigationStrategy::Hybrid;
}

// Direct Win32 API implementation
NavigationResult AgenticNavigator::navigateDirectAPI(const std::string& target) {
    // Real implementation: Find window by class/title and bring to foreground
    HWND hwnd = FindWindowA(NULL, target.c_str());
    if (!hwnd) {
        // Try loose search if exact match fails
        hwnd = FindWindowExA(NULL, NULL, NULL, target.c_str());
    }

    if (hwnd) {
        if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
        return createResult(true, "Direct API navigation successful", hwnd);
    }
    
    return createResult(false, "Window not found: " + target);
}

// Callback for EnumChildWindows
static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam) {
    std::vector<UIElement>* elements = (std::vector<UIElement>*)lParam;
    
    char className[256];
    char windowText[256];
    GetClassNameA(hwnd, className, sizeof(className));
    GetWindowTextA(hwnd, windowText, sizeof(windowText));
    
    if (IsWindowVisible(hwnd)) {
        UIElement element;
        element.handle = hwnd;
        element.name = windowText;
        element.type = className;
        
        RECT rect;
        GetWindowRect(hwnd, &rect);
        element.x = rect.left;
        element.y = rect.top;
        element.width = rect.right - rect.left;
        element.height = rect.bottom - rect.top;
        
        elements->push_back(element);
    }
    return TRUE;
}

std::vector<UIElement> AgenticNavigator::detectElementsDirectAPI() {
    std::vector<UIElement> elements;
    
    // Scan all top level windows or current process windows
    // For specific IDE targeting, we might want to restrict to specific HWND
    EnumChildWindows(GetDesktopWindow(), EnumChildProc, (LPARAM)&elements);
    
    return elements;
}

NavigationResult AgenticNavigator::clickElementDirectAPI(const UIElement& element) {
    if (!IsWindow(element.handle)) {
        return createResult(false, "Invalid handle for click", element.handle);
    }

    // Post real click messages
    RECT rect;
    GetClientRect(element.handle, &rect);
    int x = (rect.right - rect.left) / 2;
    int y = (rect.bottom - rect.top) / 2;
    LPARAM lParam = MAKELPARAM(x, y);

    SendMessage(element.handle, WM_LBUTTONDOWN, MK_LBUTTON, lParam);
    SendMessage(element.handle, WM_LBUTTONUP, 0, lParam);
    
    return createResult(true, "Direct API click sent", element.handle);
}

NavigationResult AgenticNavigator::sendTextDirectAPI(const UIElement& element, const std::string& text) {
    if (!IsWindow(element.handle)) {
        return createResult(false, "Invalid handle for text", element.handle);
    }

    // Send text via WM_SETTEXT
    SendMessageA(element.handle, WM_SETTEXT, 0, (LPARAM)text.c_str());
    
    // Explicit Logic: If text ends with newline, send Enter key
    if (!text.empty() && text.back() == '\n') {
        SendMessage(element.handle, WM_KEYDOWN, VK_RETURN, 0);
        SendMessage(element.handle, WM_KEYUP, VK_RETURN, 0);
    }
    
    return createResult(true, "Direct API sent text", element.handle);
}

// IDE Command implementation
NavigationResult AgenticNavigator::navigateIDECommands(const std::string& target) {
    // REAL IMPLEMENTATION: Send WM_COPYDATA with command payload
    HWND mainHwnd = FindWindowA(NULL, "RawrXD"); // Try exact window title first
    if (!mainHwnd) mainHwnd = GetForegroundWindow(); // Fallback
    
    if (!mainHwnd) return createResult(false, "No accessible IDE window");

    // Command structure: "COMMAND:<target>"
    std::string commandPayload = "COMMAND:" + target;
    
    COPYDATASTRUCT cds;
    cds.dwData = 0xAA55; // Magic signature for IDE commands
    cds.cbData = (DWORD)commandPayload.size() + 1;
    cds.lpData = (PVOID)commandPayload.c_str();
    
    LRESULT res = SendMessageA(mainHwnd, WM_COPYDATA, 0, (LPARAM)&cds);
    
    if (res == 1) { // Assuming IDE returns 1 on success
        return createResult(true, "IDE Command dispatched via WM_COPYDATA");
    }
    
    return createResult(false, "IDE Command dispatch failed or window ignored message");
}


NavigationResult AgenticNavigator::clickElementIDECommands(const UIElement& element) {
    // Explicit Logic: Attempt real interaction via handle if available
    if (element.handle && IsWindow((HWND)element.handle)) {
        SendMessage((HWND)element.handle, BM_CLICK, 0, 0);
        return createResult(true, "Clicked element via handle", element.handle);
    }
    return createResult(false, "Cannot click: Invalid Window Handle", element.handle);
}

NavigationResult AgenticNavigator::sendTextIDECommands(const UIElement& element, const std::string& text) {
     // Explicit Logic: Attempt real interaction via handle if available
    if (element.handle && IsWindow((HWND)element.handle)) {
        SendMessageA((HWND)element.handle, WM_SETTEXT, 0, (LPARAM)text.c_str());
         return createResult(true, "Sent text via handle", element.handle);
    }
    return createResult(false, "Cannot send text: Invalid Window Handle", element.handle);
}

// Hybrid implementation
NavigationResult AgenticNavigator::navigateHybrid(const std::string& target) {
    if (!m_fallbackConfig.enableCrossValidation) {
        // Try direct API first, fallback to commands
        auto directResult = navigateDirectAPI(target);
        if (directResult.success) {
            return directResult;
        }
        
        auto commandResult = navigateIDECommands(target);
        commandResult.fallbackUsed = "IDECommands";
        return commandResult;
    }
    
    // Cross-validation mode
    auto directResult = navigateDirectAPI(target);
    auto commandResult = navigateIDECommands(target);
    
    return validateResults(directResult, commandResult);
}

NavigationResult AgenticNavigator::validateResults(const NavigationResult& directResult, 
                                                 const NavigationResult& commandResult) {
    if (directResult.success && commandResult.success) {
        // Both methods agree - use faster one
        if (directResult.executionTimeMs <= commandResult.executionTimeMs) {
            return directResult;
        } else {
            NavigationResult result = commandResult;
            result.fallbackUsed = "IDECommands (cross-validated)";
            return result;
        }
    } else if (directResult.success) {
        return directResult;
    } else if (commandResult.success) {
        NavigationResult result = commandResult;
        result.fallbackUsed = "IDECommands (fallback)";
        return result;
    } else {
        return createResult(false, "Both navigation methods failed");
    }
}

// Utility functions
bool AgenticNavigator::isElementValid(const UIElement& element) const {
    return element.handle != nullptr && IsWindow(element.handle);
}

std::string AgenticNavigator::getElementInfo(const UIElement& element) const {
    return "Class: " + element.className + ", Name: " + element.name + ", Text: " + element.text;
}

NavigationResult AgenticNavigator::createResult(bool success, const std::string& message, 
                                              HWND target, const std::string& fallback) {
    NavigationResult result;
    result.success = success;
    result.message = message;
    result.targetWindow = target;
    result.fallbackUsed = fallback;
    result.errorCode = success ? 0 : -1;
    return result;
}

// Global navigator instance
std::unique_ptr<AgenticNavigator> CreateAgenticNavigator() {
    return std::make_unique<AgenticNavigator>();
}

} // namespace Agentic
} // namespace RawrXD