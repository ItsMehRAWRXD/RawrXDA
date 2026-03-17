// ════════════════════════════════════════════════════════════════════════════════
// MULTI-MODEL AGENT PANEL - Qt Widget for Managing Multi-Agent System
// ════════════════════════════════════════════════════════════════════════════════

#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QSplitter>
#include <QCheckBox>
#include <QScrollArea>
#include <QFrame>
#include <QTimer>
#include <QJsonObject>
#include <QMap>
#include <QListWidget>
#include <QStackedWidget>

class MultiModelAgentCoordinator;

namespace RawrXD {
namespace IDE {

class MultiModelAgentPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MultiModelAgentPanel(MultiModelAgentCoordinator* coordinator, QWidget* parent = nullptr);
    ~MultiModelAgentPanel();

    void setupUI();
    void setupConnections();
    void refreshAgentList();

signals:
    void agentCreated(const QString& agentId);
    void parallelQueryExecuted(const QString& sessionId);

public slots:
    void onAgentCreated(const QString& agentId, const QString& provider, const QString& model);
    void onAgentRemoved(const QString& agentId);
    void onParallelExecutionStarted(const QString& sessionId, int agentCount);
    void onParallelExecutionCompleted(const QString& sessionId,
                                    const QMap<QString, QString>& responses,
                                    qint64 totalTimeMs,
                                    double averageQuality,
                                    qint64 fastestTimeMs,
                                    qint64 slowestTimeMs);
    void onAgentResponseReceived(const QString& agentId,
                               const QString& response,
                               qint64 responseTimeMs,
                               float qualityScore);

private slots:
    void createNewAgent();
    void removeSelectedAgent();
    void executeParallelQuery();
    void switchAgentModel();
    void toggleBrowserMode(bool enabled);
    void onAgentSelectionChanged();
    void updateAgentStatus();

private:
    void setupAgentManagementSection();
    void setupQueryExecutionSection();
    void setupResultsDisplaySection();
    void setupBrowserModeSection();

    void addAgentToList(const QString& agentId);
    void removeAgentFromList(const QString& agentId);
    void updateAgentDisplay(const QString& agentId);
    void displayParallelResults(const QString& sessionId,
                              const QMap<QString, QString>& responses,
                              qint64 totalTimeMs,
                              double averageQuality);

    QString getSelectedAgentId() const;
    void clearResultsDisplay();

    // UI Components
    MultiModelAgentCoordinator* m_coordinator;

    // Main layout
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;

    // Agent Management Section
    QGroupBox* m_agentGroup;
    QVBoxLayout* m_agentLayout;
    QHBoxLayout* m_agentControlsLayout;

    QComboBox* m_providerCombo;
    QComboBox* m_modelCombo;
    QLineEdit* m_roleEdit;
    QPushButton* m_createAgentBtn;
    QPushButton* m_removeAgentBtn;
    QPushButton* m_switchModelBtn;

    QListWidget* m_agentList;
    QTableWidget* m_agentStatusTable;

    // Query Execution Section
    QGroupBox* m_queryGroup;
    QVBoxLayout* m_queryLayout;

    QTextEdit* m_queryInput;
    QListWidget* m_selectedAgentsList;
    QPushButton* m_addToQueryBtn;
    QPushButton* m_removeFromQueryBtn;
    QPushButton* m_executeQueryBtn;

    QCheckBox* m_browserModeCheck;
    QLabel* m_browserStatusLabel;

    // Results Display Section
    QGroupBox* m_resultsGroup;
    QVBoxLayout* m_resultsLayout;
    QStackedWidget* m_resultsStack;

    // Progress display
    QWidget* m_progressWidget;
    QVBoxLayout* m_progressLayout;
    QProgressBar* m_executionProgress;
    QLabel* m_progressLabel;
    QLabel* m_statsLabel;

    // Results display
    QWidget* m_resultsWidget;
    QVBoxLayout* m_resultsDisplayLayout;
    QTableWidget* m_resultsTable;
    QTextEdit* m_summaryText;

    // Status update timer
    QTimer* m_statusTimer;

    // Current session tracking
    QString m_currentSessionId;
    QMap<QString, QString> m_currentResponses;
};

} // namespace IDE
} // namespace RawrXD