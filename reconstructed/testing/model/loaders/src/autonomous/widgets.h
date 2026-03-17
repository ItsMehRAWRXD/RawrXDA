// autonomous_widgets.h - Autonomous Widget System (Headless Stubs)
// Converted from Qt QWidget to pure C++17 data structures
// GUI rendering removed; all state management and data structures preserved
#ifndef AUTONOMOUS_WIDGETS_H
#define AUTONOMOUS_WIDGETS_H

#include "common/callback_system.hpp"
#include "common/json_types.hpp"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

// Widget visibility/state — replaces QWidget::show/hide
enum class WidgetState {
    Hidden,
    Visible,
    Minimized,
    Maximized,
    Disabled
};

// Widget geometry — replaces QRect
struct WidgetGeometry {
    int x = 0, y = 0, width = 400, height = 300;
};

// Base widget data (replaces QWidget)
struct WidgetData {
    std::string id;
    std::string title;
    std::string tooltip;
    WidgetState state = WidgetState::Hidden;
    WidgetGeometry geometry;
    bool enabled = true;
    std::string styleClass;
};

// Model suggestion card
struct SuggestionCard {
    std::string modelName;
    std::string description;
    float matchScore = 0.0f;
    int parameterCount = 0;
    std::string quantization;
    bool isDownloaded = false;
    std::string downloadUrl;
    double sizeMB = 0.0;
};

// Training progress display data
struct TrainingProgressData {
    std::string modelName;
    int currentEpoch = 0;
    int totalEpochs = 0;
    float currentLoss = 0.0f;
    float bestLoss = 0.0f;
    float progress = 0.0f;        // 0.0 to 1.0
    double elapsedSeconds = 0.0;
    double estimatedRemaining = 0.0;
    std::vector<float> lossHistory;
    std::string statusMessage;
};

// Code analysis display data
struct CodeAnalysisDisplay {
    std::string filePath;
    int totalLines = 0;
    int issues = 0;
    float complexity = 0.0f;
    std::vector<std::string> warnings;
    std::vector<std::string> suggestions;
};

// Agent status display
struct AgentStatusDisplay {
    std::string agentName;
    std::string status;       // idle, working, error, paused
    std::string currentTask;
    float confidence = 0.0f;
    int tasksCompleted = 0;
    int tasksFailed = 0;
    double uptime = 0.0;
};

// Dashboard panel data
struct DashboardPanel {
    std::string panelId;
    std::string title;
    std::string type;         // chart, list, status, progress
    WidgetGeometry geometry;
    JsonObject data;
};

// Autonomous widget manager (headless)
class AutonomousWidgetManager {
public:
    AutonomousWidgetManager();
    ~AutonomousWidgetManager();

    // Widget management
    void addWidget(const WidgetData& widget);
    void removeWidget(const std::string& id);
    WidgetData getWidget(const std::string& id) const;
    std::vector<WidgetData> getAllWidgets() const;
    void setWidgetState(const std::string& id, WidgetState state);
    void setWidgetGeometry(const std::string& id, const WidgetGeometry& geo);

    // Suggestion cards
    void addSuggestionCard(const SuggestionCard& card);
    std::vector<SuggestionCard> getSuggestionCards() const;
    void clearSuggestionCards();

    // Training progress
    void updateTrainingProgress(const TrainingProgressData& data);
    TrainingProgressData getTrainingProgress() const;

    // Code analysis display
    void updateCodeAnalysis(const CodeAnalysisDisplay& display);
    std::vector<CodeAnalysisDisplay> getCodeAnalyses() const;

    // Agent status
    void updateAgentStatus(const AgentStatusDisplay& status);
    std::vector<AgentStatusDisplay> getAgentStatuses() const;

    // Dashboard
    void addDashboardPanel(const DashboardPanel& panel);
    void removeDashboardPanel(const std::string& panelId);
    std::vector<DashboardPanel> getDashboardPanels() const;

    // Serialization
    std::string serializeLayout() const;
    bool loadLayout(const std::string& json);

    // Callbacks
    CallbackList<const std::string&> onWidgetStateChanged;
    CallbackList<const SuggestionCard&> onSuggestionSelected;
    CallbackList<const std::string&> onPanelResized;

private:
    std::map<std::string, WidgetData> m_widgets;
    std::vector<SuggestionCard> m_suggestions;
    TrainingProgressData m_trainingProgress;
    std::vector<CodeAnalysisDisplay> m_codeAnalyses;
    std::vector<AgentStatusDisplay> m_agentStatuses;
    std::vector<DashboardPanel> m_dashboardPanels;
};

#endif // AUTONOMOUS_WIDGETS_H
