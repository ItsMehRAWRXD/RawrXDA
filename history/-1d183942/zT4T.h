#pragma once

#include <QWidget>
#include <QString>
#include <QMap>

class QTextEdit;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;
class QProgressBar;
class QTimer;
class QPushButton;
class QListWidget;
class AgenticEngine;
class ZeroDayAgenticEngine;
class AgenticBrowser;
class EnterpriseMetricsCollector;

namespace RawrXD {
    class PlanOrchestrator;
}

// Agent workflow breadcrumb states
enum class AgentWorkflowState {
    Agent,      // Initial agent state
    Ask,        // Asking/clarifying phase
    Plan,       // Planning/analyzing phase
    Edit,       // Code editing phase
    Configure   // Custom agent configuration
};

class ChatInterface : public QWidget {
    Q_OBJECT
public:
    explicit ChatInterface(QWidget* parent = nullptr);
    
    // Two-phase initialization: call after QApplication exists
    void initialize();
    
    void setAgenticEngine(AgenticEngine* engine) { m_agenticEngine = engine; }
    void setPlanOrchestrator(RawrXD::PlanOrchestrator* orchestrator) { m_planOrchestrator = orchestrator; }
    void setZeroDayAgent(ZeroDayAgenticEngine* agent) { m_zeroDayAgent = agent; wireAgentSignals(); }
    void setBrowser(AgenticBrowser* browser) { m_browser = browser; }
    void setMetrics(EnterpriseMetricsCollector* metrics) { m_metrics = metrics; }
    
    void addMessage(const QString& sender, const QString& message);
    QString selectedModel() const;
    bool isMaxMode() const;
    void sendMessageProgrammatically(const QString& message);
    
    // Agent tool commands
    // Updated to accept optional arguments string for future extensions
    void executeAgentCommand(const QString& command, const QString& args = "");
    bool isAgentCommand(const QString& message) const;
    
    // Autonomous mission control
    void startAutonomousMission(const QString& goal);
    void abortAutonomousMission();
    
    // Command suggestions and discovery
    QStringList getAvailableCommands() const;
    QStringList getSuggestedCommands() const;
    
    // Agent stream handlers (wired from agentic engines)
    void onAgentStreamToken(const QString& token);
    void onAgentComplete(const QString& summary);
    void onAgentError(const QString& error);
    void onBrowserNavigated(const QUrl& url, bool success, int httpStatus);
    
    // Get current agent workflow state
    AgentWorkflowState currentWorkflowState() const { return m_workflowState; }
    
public slots:
    void displayResponse(const QString& response);
    void focusInput();
    void sendMessage();
    void refreshModels();
    void onModelChanged(int index);
    void onModel2Changed(int index);
    void onMaxModeToggled(bool enabled);
    void setCanSendMessage(bool enabled);
    
    // Phase 2: Streaming token progress
    void onTokenGenerated(int delta);
    void hideProgress();
    
    // Breadcrumb workflow navigation
    void onWorkflowStateChanged(int state);
    void onAutoModelSelected();
    
signals:
    void messageSent(const QString& message);
    void modelSelected(const QString& modelPath);
    void model2Selected(const QString& modelPath);
    void maxModeChanged(bool enabled);
    void messageReceived(const QString& reply);
    void workflowStateChanged(AgentWorkflowState state);
    
private:
    void loadAvailableModels();
    void loadAvailableModelsForSecond();
    QString resolveGgufPath(const QString& modelName);  // Resolve Ollama model name to GGUF file
    
    // Helper to select best model based on task
    QString selectBestModelForTask(AgentWorkflowState task);
    
    QTextEdit* message_history_;
    QLineEdit* message_input_;
    QComboBox* modelSelector_;
    QComboBox* modelSelector2_;
    QComboBox* workflowBreadcrumb_;  // Agent/Ask/Plan/Edit/Configure dropdown
    QComboBox* modelAutoSelector_;   // Auto-selecting model dropdown
    QPushButton* autoModelButton_;   // Button to trigger auto-selection
    QCheckBox* maxModeToggle_;
    QLabel* statusLabel_;
    bool maxMode_;
    bool m_busy = false;  // Prevent re-entrancy during inference
    QString m_lastPrompt;  // Store last user message
    AgenticEngine* m_agenticEngine = nullptr;
    RawrXD::PlanOrchestrator* m_planOrchestrator = nullptr;
    ZeroDayAgenticEngine* m_zeroDayAgent = nullptr;
    AgentWorkflowState m_workflowState = AgentWorkflowState::Agent;
    
    // Available models map for smart selection
    QMap<QString, QString> m_modelDescriptions;
    QMap<AgentWorkflowState, QString> m_modelPreferences;
    
    // Phase 2: Streaming token progress bar
    QProgressBar* m_tokenProgress{nullptr};
    QTimer* m_hideTimer{nullptr};
    
    // Autonomous and agentic features
    AgenticBrowser* m_browser = nullptr;
    EnterpriseMetricsCollector* m_metrics = nullptr;
    QListWidget* m_commandSuggestions = nullptr;
    QTimer* m_missionMonitorTimer = nullptr;
    bool m_missionActive = false;
    QString m_currentMissionGoal;
    
    // Command history for suggestions
    QStringList m_commandHistory;
    
    // Helper methods
    void updateCommandSuggestions(const QString& filter = "");
    void wireAgentSignals();
    void enhanceMessageWithContext(QString& message);
};
