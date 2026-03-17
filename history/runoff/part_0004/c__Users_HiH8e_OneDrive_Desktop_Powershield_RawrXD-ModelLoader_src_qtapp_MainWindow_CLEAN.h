#pragma once

#include <QMainWindow>
#include <QUrl>
#include <QHash>
#include <QString>

class QLineEdit;
class QPushButton;
class QProgressBar;
class QListWidget;
class QListWidgetItem;
class QTabWidget;
class QTextEdit;
class QLabel;
class QJsonDocument;
class StreamerClient;
class AISuggestionOverlay;
class AgentOrchestrator;
class TaskProposalWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void onGoalSubmitted(const QString& goal);

private slots:
    void handleGoalSubmit();
    void handleAgentMockProgress();
    void updateSuggestion(const QString& chunk);
    void handleGenerationFinished();
    void handleQShellReturn();
    void handleArchitectChunk(const QString& chunk);
    void handleArchitectFinished();
    void handleTaskStatusUpdate(const QString& taskId, const QString& status, const QString& agentType);
    void handleTaskCompleted(const QString& agentType, const QString& summary);
    void handleWorkflowFinished(bool success);
    void handleTaskStreaming(const QString& taskId, const QString& chunk);

private:
    QWidget* createGoalBar();
    QWidget* createAgentPanel();
    QWidget* createProposalReview();
    QWidget* createEditorArea();
    QWidget* createQShellTab();
    QJsonDocument getMockArchitectJson() const;

    // Goal Bar
    QLineEdit* goalInput_{};
    QPushButton* micButton_{};
    QLabel* mockStatusBadge_{};

    // Agent Panel
    QProgressBar* goalProgress_{};
    QLabel* activeGoalLabel_{};
    QLabel* featureStatus_{};
    QLabel* securityStatus_{};
    QLabel* performanceStatus_{};

    // Proposal Review
    QListWidget* proposalList_{};

    // Center Editor
    QTabWidget* editorTabs_{};
    QTextEdit* codeView_{};
    AISuggestionOverlay* overlay_{};
    QString suggestionBuffer_{};
    QString architectBuffer_{};
    bool suggestionEnabled_{true};
    bool forceMockArchitect_{false};
    bool architectRunning_{false};
    QHash<QString, QListWidgetItem*> proposalItemMap_{};
    QHash<QString, TaskProposalWidget*> proposalWidgetMap_{};

    // QShell
    QTextEdit* qshellOutput_{};
    QLineEdit* qshellInput_{};

    // Streaming client
    StreamerClient* streamer_{};
    QUrl streamerUrl_{QStringLiteral("http://localhost:11434")};

    // Orchestrator
    AgentOrchestrator* orchestrator_{};
};
