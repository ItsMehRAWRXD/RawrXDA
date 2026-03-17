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
    // Dispatch IDE command via WM_COMMAND to the main window
    auto start = std::chrono::high_resolution_clock::now();
    
    HWND mainWnd = GetForegroundWindow();
    bool success = false;
    std::string message;
    
    if (mainWnd) {
        LRESULT lr = SendMessageA(mainWnd, WM_COMMAND, MAKEWPARAM(commandId, 0), 0);
        success = true;
        message = "Command " + std::to_string(commandId) + " dispatched (result=" + std::to_string(lr) + ")";
    } else {
        message = "No foreground window available for command dispatch";
    }
    
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
    // Find target window/control by name or class using Win32 API
    HWND mainWnd = GetForegroundWindow();
    if (!mainWnd) {
        return createResult(false, "No foreground window for navigation");
    }
    
    // Search for child window matching target name or class
    HWND found = FindWindowExA(mainWnd, nullptr, nullptr, target.c_str());
    if (!found) {
        // Try by class name
        found = FindWindowExA(mainWnd, nullptr, target.c_str(), nullptr);
    }
    
    if (found) {
        SetFocus(found);
        return createResult(true, "Direct API navigated to " + target, found);
    }
    
    return createResult(false, "Target '" + target + "' not found via direct API");
}

std::vector<UIElement> AgenticNavigator::detectElementsDirectAPI() {
    std::vector<UIElement> elements;

    // Get the foreground window as the root for element detection
    HWND foreground = GetForegroundWindow();
    if (!foreground) {
        return elements;
    }

    // Enumerate all child windows
    struct EnumContext {
        std::vector<UIElement>* elements;
    };
    EnumContext ctx{&elements};

    EnumChildWindows(foreground, [](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* ctx = reinterpret_cast<EnumContext*>(lParam);

        if (!IsWindowVisible(hwnd)) return TRUE; // Skip hidden windows

        UIElement elem;
        elem.handle = hwnd;

        // Get window class name
        char className[256] = {};
        GetClassNameA(hwnd, className, sizeof(className));
        elem.className = className;

        // Get window text
        char windowText[512] = {};
        GetWindowTextA(hwnd, windowText, sizeof(windowText));
        elem.text = windowText;
        elem.name = elem.text.empty() ? elem.className : elem.text;

        // Get bounding rectangle
        RECT rect;
        if (GetWindowRect(hwnd, &rect)) {
            elem.x = rect.left;
            elem.y = rect.top;
            elem.width = rect.right - rect.left;
            elem.height = rect.bottom - rect.top;
        }

        // Determine element type from class name
        if (elem.className == "Button" || elem.className == "BUTTON") {
            elem.type = "button";
        } else if (elem.className == "Edit" || elem.className == "EDIT" ||
                   elem.className == "Scintilla") {
            elem.type = "textbox";
        } else if (elem.className == "SysTreeView32") {
            elem.type = "treeview";
        } else if (elem.className == "SysTabControl32") {
            elem.type = "tabcontrol";
        } else if (elem.className == "SysListView32") {
            elem.type = "listview";
        } else if (elem.className == "Static" || elem.className == "STATIC") {
            elem.type = "label";
        } else if (elem.className == "ComboBox" || elem.className == "COMBOBOX") {
            elem.type = "combobox";
        } else {
            elem.type = "generic";
        }

        // Check if element is enabled
        elem.enabled = IsWindowEnabled(hwnd) ? true : false;
        elem.visible = true; // Already filtered above

        ctx->elements->push_back(elem);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    return elements;
}

NavigationResult AgenticNavigator::clickElementDirectAPI(const UIElement& element) {
    // Click via WM_LBUTTONDOWN/UP at the center of the element
    if (!element.handle || !IsWindow(element.handle)) {
        return createResult(false, "Invalid window handle for click", element.handle);
    }
    
    RECT rect;
    if (!GetClientRect(element.handle, &rect)) {
        return createResult(false, "Cannot get client rect for " + element.name, element.handle);
    }
    
    int cx = (rect.right - rect.left) / 2;
    int cy = (rect.bottom - rect.top) / 2;
    LPARAM lParam = MAKELPARAM(cx, cy);
    
    // Bring to front and focus
    SetForegroundWindow(GetAncestor(element.handle, GA_ROOT));
    SetFocus(element.handle);
    
    // Send click messages
    SendMessageA(element.handle, WM_LBUTTONDOWN, MK_LBUTTON, lParam);
    SendMessageA(element.handle, WM_LBUTTONUP, 0, lParam);
    
    return createResult(true, "Clicked " + element.name + " at center (" + std::to_string(cx) + "," + std::to_string(cy) + ")", element.handle);
}

NavigationResult AgenticNavigator::sendTextDirectAPI(const UIElement& element, const std::string& text) {
    // Send text via WM_SETTEXT for edit controls, WM_CHAR for others
    if (!element.handle || !IsWindow(element.handle)) {
        return createResult(false, "Invalid window handle for text send", element.handle);
    }
    
    SetFocus(element.handle);
    
    // Try WM_SETTEXT first (works for Edit, RichEdit, ComboBox controls)
    if (element.type == "textbox" || element.type == "combobox" ||
        element.className == "Edit" || element.className == "EDIT") {
        SendMessageA(element.handle, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()));
    } else {
        // Fallback: send individual WM_CHAR messages
        for (char c : text) {
            SendMessageA(element.handle, WM_CHAR, static_cast<WPARAM>(c), 0);
        }
    }
    
    return createResult(true, "Sent " + std::to_string(text.size()) + " chars to " + element.name, element.handle);
}

// IDE Command implementation
NavigationResult AgenticNavigator::navigateIDECommands(const std::string& target) {
    // Navigate using IDE command palette dispatch
    HWND mainWnd = GetForegroundWindow();
    if (!mainWnd) {
        return createResult(false, "No foreground window for IDE command navigation");
    }
    
    // Post a user-defined message that the IDE message loop handles for command routing
    // WM_APP + 0x100 = IDE command dispatch, wParam = hash of command name
    unsigned int cmdHash = 0;
    for (char c : target) cmdHash = cmdHash * 31 + static_cast<unsigned char>(c);
    
    PostMessageA(mainWnd, WM_APP + 0x100, cmdHash, 0);
    
    return createResult(true, "IDE command dispatched: " + target);
}

NavigationResult AgenticNavigator::clickElementIDECommands(const UIElement& element) {
    // Click via BN_CLICKED notification for buttons, BM_CLICK for others
    if (!element.handle || !IsWindow(element.handle)) {
        return createResult(false, "Invalid handle for IDE command click", element.handle);
    }
    
    if (element.type == "button") {
        // Send BM_CLICK message which triggers WM_COMMAND BN_CLICKED in parent
        SendMessageA(element.handle, BM_CLICK, 0, 0);
    } else {
        // Generic: send WM_LBUTTONDOWN/UP at center
        RECT rect;
        GetClientRect(element.handle, &rect);
        LPARAM center = MAKELPARAM((rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2);
        SendMessageA(element.handle, WM_LBUTTONDOWN, MK_LBUTTON, center);
        SendMessageA(element.handle, WM_LBUTTONUP, 0, center);
    }
    
    return createResult(true, "IDE command clicked " + element.name, element.handle);
}

NavigationResult AgenticNavigator::sendTextIDECommands(const UIElement& element, const std::string& text) {
    // Send text via EM_REPLACESEL for edit controls (preserves undo), WM_SETTEXT as fallback
    if (!element.handle || !IsWindow(element.handle)) {
        return createResult(false, "Invalid handle for IDE command text", element.handle);
    }
    
    SetFocus(element.handle);
    
    if (element.type == "textbox" || element.className == "Edit" || 
        element.className == "EDIT" || element.className == "Scintilla") {
        // Use EM_REPLACESEL to insert at cursor position (supports undo)
        SendMessageA(element.handle, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(text.c_str()));
    } else {
        SendMessageA(element.handle, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }
    
    return createResult(true, "IDE command sent " + std::to_string(text.size()) + " chars to " + element.name, element.handle);
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