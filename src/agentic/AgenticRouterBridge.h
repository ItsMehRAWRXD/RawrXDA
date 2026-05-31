#pragma once

#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <chrono>
#include <string>
#include <vector>
#include <map>

// Forward declarations
class UnifiedInferenceRouter;
class AgenticExecutor;
class AgenticEngine;

namespace RawrXD {
namespace Agentic {

enum class AgenticMode {
    Passive = 0,      // Only respond to explicit user requests
    Suggestive = 1,   // Propose changes but require approval
    Autonomous = 2    // Execute low-risk actions automatically
};

enum class AgenticTrigger {
    UserKeystroke = 0,
    AgenticObservation = 1,
    BackgroundAnalysis = 2,
    ToolResult = 3,
    ErrorRecovery = 4
};

struct AgenticAction {
    std::string id;
    std::string description;
    std::string targetFile;
    std::string toolName;
    std::string toolParams;
    float riskScore = 0.5f;        // 0.0 = safe, 1.0 = dangerous
    float confidence = 0.5f;         // 0.0 = unsure, 1.0 = certain
    AgenticTrigger trigger = AgenticTrigger::AgenticObservation;
    std::chrono::steady_clock::time_point createdAt;
    
    bool isAutoExecutable() const {
        return riskScore < 0.3f && confidence > 0.8f;
    }
};

struct AgenticSuggestion {
    std::string id;
    std::string title;
    std::string description;
    std::string codeChange;
    std::string filePath;
    int lineStart = 0;
    int lineEnd = 0;
    float confidence = 0.0f;
    bool autoApplicable = false;
};

class AgenticRouterBridge {
public:
    using SuggestionCallback = std::function<void(const AgenticSuggestion&)>;
    using ActionCallback = std::function<void(const AgenticAction&, bool executed)>;
    using ModeChangeCallback = std::function<void(AgenticMode oldMode, AgenticMode newMode)>;
    using LogCallback = std::function<void(const std::string&)>;

    AgenticRouterBridge();
    ~AgenticRouterBridge();

    // Lifecycle
    bool initialize(UnifiedInferenceRouter* router, AgenticExecutor* executor);
    void shutdown();
    bool isRunning() const { return m_running.load(); }

    // Mode control
    void setMode(AgenticMode mode);
    AgenticMode getMode() const { return m_mode.load(); }
    const char* getModeString() const;

    // Event registration
    void onSuggestion(SuggestionCallback cb) { m_suggestionCb = cb; }
    void onAction(ActionCallback cb) { m_actionCb = cb; }
    void onModeChange(ModeChangeCallback cb) { m_modeChangeCb = cb; }
    void onLog(LogCallback cb) { m_logCb = cb; }

    // Manual triggers
    void triggerFileAnalysis(const std::string& filePath);
    void triggerProjectAnalysis(const std::string& projectPath);
    void triggerErrorRecovery(const std::string& errorContext);

    // Action queue
    void approveAction(const std::string& actionId);
    void rejectAction(const std::string& actionId);
    void executeActionNow(const AgenticAction& action);

    // Status
    bool hasPendingSuggestions() const;
    std::vector<AgenticSuggestion> getPendingSuggestions() const;
    void clearSuggestions();

private:
    // Background thread
    void observationLoop();
    void processObservationQueue();
    
    // Analysis
    void analyzeActiveFile();
    void analyzeCodePatterns(const std::string& filePath, const std::string& content);
    void detectIssues(const std::string& filePath, const std::string& content);
    
    // Decision
    bool shouldExecute(const AgenticAction& action) const;
    AgenticAction createAction(const std::string& desc, const std::string& tool, 
                               const std::string& params, float risk, float confidence);
    
    // Execution
    void executeAction(const AgenticAction& action);
    void queueSuggestion(const AgenticSuggestion& suggestion);
    
    // Logging
    void log(const std::string& msg);

    std::atomic<bool> m_running{false};
    std::atomic<AgenticMode> m_mode{AgenticMode::Passive};
    
    UnifiedInferenceRouter* m_router = nullptr;
    AgenticExecutor* m_executor = nullptr;
    
    std::thread m_observerThread;
    mutable std::mutex m_queueMutex;
    std::queue<std::function<void()>> m_observationQueue;
    
    mutable std::mutex m_suggestionMutex;
    std::vector<AgenticSuggestion> m_pendingSuggestions;
    std::map<std::string, AgenticAction> m_pendingActions;
    
    SuggestionCallback m_suggestionCb;
    ActionCallback m_actionCb;
    ModeChangeCallback m_modeChangeCb;
    LogCallback m_logCb;
    
    std::string m_lastAnalyzedFile;
    std::chrono::steady_clock::time_point m_lastAnalysisTime;
    static constexpr auto ANALYSIS_COOLDOWN = std::chrono::seconds(5);
};

} // namespace Agentic
} // namespace RawrXD
