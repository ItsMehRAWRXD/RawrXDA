#include "AgenticRouterBridge.h"
#include "agentic_executor.h"
#include "UnifiedInferenceRouter.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace RawrXD {
namespace Agentic {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

AgenticRouterBridge::AgenticRouterBridge() = default;

AgenticRouterBridge::~AgenticRouterBridge() {
    shutdown();
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool AgenticRouterBridge::initialize(UnifiedInferenceRouter* router, AgenticExecutor* executor) {
    if (!router || !executor) return false;
    if (m_running.exchange(true)) return true; // Already running

    m_router = router;
    m_executor = executor;

    // Start background observation thread
    m_observerThread = std::thread(&AgenticRouterBridge::observationLoop, this);

    log("AgenticRouterBridge initialized - Mode: Passive");
    return true;
}

void AgenticRouterBridge::shutdown() {
    if (!m_running.exchange(false)) return;

    // Wake up observer thread
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_observationQueue.push([]{}); // Sentinel
    }

    if (m_observerThread.joinable()) {
        m_observerThread.join();
    }

    m_router = nullptr;
    m_executor = nullptr;

    log("AgenticRouterBridge shutdown");
}

// ============================================================================
// MODE CONTROL
// ============================================================================

void AgenticRouterBridge::setMode(AgenticMode mode) {
    AgenticMode old = m_mode.exchange(mode);
    if (old != mode && m_modeChangeCb) {
        m_modeChangeCb(old, mode);
    }
    log(std::string("Mode changed to: ") + getModeString());
}

const char* AgenticRouterBridge::getModeString() const {
    switch (m_mode.load()) {
        case AgenticMode::Passive: return "Passive";
        case AgenticMode::Suggestive: return "Suggestive";
        case AgenticMode::Autonomous: return "Autonomous";
        default: return "Unknown";
    }
}

// ============================================================================
// MANUAL TRIGGERS
// ============================================================================

void AgenticRouterBridge::triggerFileAnalysis(const std::string& filePath) {
    if (!m_running) return;

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_observationQueue.push([this, filePath]() {
        analyzeCodePatterns(filePath, "");
    });
}

void AgenticRouterBridge::triggerProjectAnalysis(const std::string& projectPath) {
    if (!m_running) return;

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_observationQueue.push([this, projectPath]() {
        log("Project analysis triggered: " + projectPath);
        // Scan for common issues across project
        for (const auto& entry : std::filesystem::directory_iterator(projectPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".cpp") {
                analyzeCodePatterns(entry.path().string(), "");
            }
        }
    });
}

void AgenticRouterBridge::triggerErrorRecovery(const std::string& errorContext) {
    if (!m_running) return;

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_observationQueue.push([this, errorContext]() {
        log("Error recovery triggered: " + errorContext);
        
        auto action = createAction(
            "Auto-recover from error: " + errorContext,
            "analyzeError",
            errorContext,
            0.2f,  // Low risk
            0.7f   // Moderate confidence
        );
        action.trigger = AgenticTrigger::ErrorRecovery;
        
        if (shouldExecute(action)) {
            executeAction(action);
        } else {
            std::lock_guard<std::mutex> sugLock(m_suggestionMutex);
            m_pendingActions[action.id] = action;
        }
    });
}

// ============================================================================
// ACTION QUEUE
// ============================================================================

void AgenticRouterBridge::approveAction(const std::string& actionId) {
    std::lock_guard<std::mutex> lock(m_suggestionMutex);
    auto it = m_pendingActions.find(actionId);
    if (it != m_pendingActions.end()) {
        executeAction(it->second);
        m_pendingActions.erase(it);
    }
}

void AgenticRouterBridge::rejectAction(const std::string& actionId) {
    std::lock_guard<std::mutex> lock(m_suggestionMutex);
    m_pendingActions.erase(actionId);
    log("Action rejected: " + actionId);
}

void AgenticRouterBridge::executeActionNow(const AgenticAction& action) {
    executeAction(action);
}

// ============================================================================
// STATUS
// ============================================================================

bool AgenticRouterBridge::hasPendingSuggestions() const {
    std::lock_guard<std::mutex> lock(m_suggestionMutex);
    return !m_pendingSuggestions.empty() || !m_pendingActions.empty();
}

std::vector<AgenticSuggestion> AgenticRouterBridge::getPendingSuggestions() const {
    std::lock_guard<std::mutex> lock(m_suggestionMutex);
    return m_pendingSuggestions;
}

void AgenticRouterBridge::clearSuggestions() {
    std::lock_guard<std::mutex> lock(m_suggestionMutex);
    m_pendingSuggestions.clear();
}

// ============================================================================
// BACKGROUND THREAD
// ============================================================================

void AgenticRouterBridge::observationLoop() {
    while (m_running) {
        processObservationQueue();
        
        // Periodic analysis of active file
        auto now = std::chrono::steady_clock::now();
        if (now - m_lastAnalysisTime > ANALYSIS_COOLDOWN) {
            analyzeActiveFile();
            m_lastAnalysisTime = now;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void AgenticRouterBridge::processObservationQueue() {
    std::queue<std::function<void()>> tasks;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        tasks = std::move(m_observationQueue);
        while (!m_observationQueue.empty()) m_observationQueue.pop();
    }

    while (!tasks.empty()) {
        tasks.front()();
        tasks.pop();
    }
}

// ============================================================================
// ANALYSIS
// ============================================================================

void AgenticRouterBridge::analyzeActiveFile() {
    // In real implementation, this would get the currently open file from IDE
    // For now, use last analyzed file or skip
    if (m_lastAnalyzedFile.empty()) return;
    
    std::ifstream file(m_lastAnalyzedFile);
    if (!file) return;
    
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();
    
    analyzeCodePatterns(m_lastAnalyzedFile, content);
    detectIssues(m_lastAnalyzedFile, content);
}

void AgenticRouterBridge::analyzeCodePatterns(const std::string& filePath, const std::string& content) {
    // Pattern 1: Unused includes
    static const std::regex includeRegex(R"(#include\s+<([^\u003e]+)\u003e)");
    std::smatch match;
    std::string::const_iterator searchStart(content.cbegin());
    
    while (std::regex_search(searchStart, content.cend(), match, includeRegex)) {
        std::string header = match[1].str();
        // Check if header symbols are actually used
        // Simplified: just check if header name appears elsewhere
        if (content.find(header) == match.position()) {
            auto action = createAction(
                "Potentially unused include: " + header,
                "suggestRemoveInclude",
                header,
                0.1f,  // Very low risk
                0.6f   // Moderate confidence
            );
            
            if (shouldExecute(action)) {
                executeAction(action);
            } else {
                AgenticSuggestion sug;
                sug.id = action.id;
                sug.title = "Unused Include";
                sug.description = "Header '" + header + "' may be unused";
                sug.filePath = filePath;
                sug.confidence = 0.6f;
                sug.autoApplicable = true;
                queueSuggestion(sug);
            }
        }
        searchStart = match.suffix().first;
    }

    // Pattern 2: Missing error handling
    static const std::regex fopenRegex(R"(\bfopen\s*\()");
    if (std::regex_search(content, fopenRegex)) {
        static const std::regex nullCheckRegex(R"(if\s*\(\s*[^=]*==\s*nullptr)");
        if (!std::regex_search(content, nullCheckRegex)) {
            auto action = createAction(
                "Missing null check after fopen",
                "suggestErrorHandling",
                "fopen",
                0.15f,
                0.85f
            );
            
            AgenticSuggestion sug;
            sug.id = action.id;
            sug.title = "Missing Error Handling";
            sug.description = "File operations should check for null";
            sug.filePath = filePath;
            sug.confidence = 0.85f;
            sug.autoApplicable = false;
            queueSuggestion(sug);
        }
    }

    // Pattern 3: Memory leaks (simplified)
    static const std::regex newRegex(R"(\bnew\s+\w+)");
    static const std::regex deleteRegex(R"(\bdelete\b)");
    auto newCount = std::distance(
        std::sregex_iterator(content.begin(), content.end(), newRegex),
        std::sregex_iterator()
    );
    auto deleteCount = std::distance(
        std::sregex_iterator(content.begin(), content.end(), deleteRegex),
        std::sregex_iterator()
    );
    
    if (newCount > deleteCount) {
        auto action = createAction(
            "Potential memory leak: " + std::to_string(newCount - deleteCount) + " unmatched new/delete",
            "suggestSmartPointers",
            filePath,
            0.25f,
            0.7f
        );
        
        AgenticSuggestion sug;
        sug.id = action.id;
        sug.title = "Memory Leak Risk";
        sug.description = "Found " + std::to_string(newCount) + " new but only " + 
                         std::to_string(deleteCount) + " delete";
        sug.filePath = filePath;
        sug.confidence = 0.7f;
        sug.autoApplicable = false;
        queueSuggestion(sug);
    }
}

void AgenticRouterBridge::detectIssues(const std::string& filePath, const std::string& content) {
    // Detect TODO/FIXME comments
    static const std::regex todoRegex(R"((TODO|FIXME|XXX|HACK)\s*[:\s]\s*(.*)$)");
    std::smatch match;
    std::string::const_iterator searchStart(content.cbegin());
    int lineNum = 1;
    size_t lastPos = 0;
    
    while (std::regex_search(searchStart, content.cend(), match, todoRegex)) {
        // Calculate line number
        size_t pos = match.position();
        for (size_t i = lastPos; i < pos; ++i) {
            if (content[i] == '\n') ++lineNum;
        }
        lastPos = pos;
        
        AgenticSuggestion sug;
        sug.id = "todo_" + std::to_string(lineNum);
        sug.title = "Task: " + match[1].str();
        sug.description = match[3].str();
        sug.filePath = filePath;
        sug.lineStart = lineNum;
        sug.confidence = 0.95f;
        sug.autoApplicable = false;
        queueSuggestion(sug);
        
        searchStart = match.suffix().first;
    }
}

// ============================================================================
// DECISION
// ============================================================================

bool AgenticRouterBridge::shouldExecute(const AgenticAction& action) const {
    AgenticMode mode = m_mode.load();
    
    switch (mode) {
        case AgenticMode::Passive:
            return false; // Never auto-execute
            
        case AgenticMode::Suggestive:
            return false; // Always queue for approval
            
        case AgenticMode::Autonomous:
            return action.isAutoExecutable(); // Only safe actions
            
        default:
            return false;
    }
}

AgenticAction AgenticRouterBridge::createAction(const std::string& desc, const std::string& tool,
                                                   const std::string& params, float risk, float confidence) {
    static int actionCounter = 0;
    AgenticAction action;
    action.id = "agentic_" + std::to_string(++actionCounter) + "_" + std::to_string(GetTickCount());
    action.description = desc;
    action.toolName = tool;
    action.toolParams = params;
    action.riskScore = risk;
    action.confidence = confidence;
    action.createdAt = std::chrono::steady_clock::now();
    return action;
}

// ============================================================================
// EXECUTION
// ============================================================================

void AgenticRouterBridge::executeAction(const AgenticAction& action) {
    if (!m_executor) return;

    log("Executing action: " + action.description);

    // Route through AgenticExecutor
    std::string result = m_executor->callTool(action.toolName, action.toolParams);

    bool success = result.find("\"success\":true") != std::string::npos ||
                   result.find("\"success\": true") != std::string::npos;

    if (m_actionCb) {
        m_actionCb(action, success);
    }

    log("Action " + action.id + " completed: " + (success ? "success" : "failed"));
}

void AgenticRouterBridge::queueSuggestion(const AgenticSuggestion& suggestion) {
    std::lock_guard<std::mutex> lock(m_suggestionMutex);
    
    // Avoid duplicates
    auto it = std::find_if(m_pendingSuggestions.begin(), m_pendingSuggestions.end(),
        [&suggestion](const AgenticSuggestion& existing) {
            return existing.filePath == suggestion.filePath && 
                   existing.title == suggestion.title;
        });
    
    if (it == m_pendingSuggestions.end()) {
        m_pendingSuggestions.push_back(suggestion);
        
        if (m_suggestionCb) {
            m_suggestionCb(suggestion);
        }
        
        log("New suggestion: " + suggestion.title + " (" + suggestion.filePath + ")");
    }
}

// ============================================================================
// LOGGING
// ============================================================================

void AgenticRouterBridge::log(const std::string& msg) {
    if (m_logCb) {
        m_logCb(msg);
    }
    OutputDebugStringA(("[AgenticRouterBridge] " + msg + "\n").c_str());
}

} // namespace Agentic
} // namespace RawrXD
