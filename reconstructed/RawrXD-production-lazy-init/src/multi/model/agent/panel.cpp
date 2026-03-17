// ════════════════════════════════════════════════════════════════════════════════
// MULTI-MODEL AGENT PANEL IMPLEMENTATION
// ════════════════════════════════════════════════════════════════════════════════

#include "multi_model_agent_panel.h"
#include "multi_model_agent_coordinator.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QApplication>
#include <QStyle>
#include <QGroupBox>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QTableWidgetItem>
#include <QTextDocument>
#include <QScrollBar>

namespace RawrXD {
namespace IDE {

MultiModelAgentPanel::MultiModelAgentPanel(MultiModelAgentCoordinator* coordinator, QWidget* parent)
    : QWidget(parent),
      m_coordinator(coordinator),
      m_mainLayout(new QVBoxLayout(this)),
      m_mainSplitter(new QSplitter(Qt::Vertical, this)),
      m_statusTimer(new QTimer(this))
{
    setupUI();
    setupConnections();

    // Setup status update timer (every 2 seconds)
    connect(m_statusTimer, &QTimer::timeout, this, &MultiModelAgentPanel::updateAgentStatus);
    m_statusTimer->start(2000);

    refreshAgentList();
}

MultiModelAgentPanel::~MultiModelAgentPanel()
{
    m_statusTimer->stop();
}

void MultiModelAgentPanel::setupUI()
{
    setWindowTitle("Multi-Model Agent Manager");
    setMinimumSize(800, 600);

    // Main splitter for sections
    m_mainLayout->addWidget(m_mainSplitter);

    // Setup different sections
    setupAgentManagementSection();
    setupQueryExecutionSection();
    setupResultsDisplaySection();

    // Set splitter proportions
    m_mainSplitter->setStretchFactor(0, 1); // Agent management
    m_mainSplitter->setStretchFactor(1, 1); // Query execution
    m_mainSplitter->setStretchFactor(2, 2); // Results display
}

void MultiModelAgentPanel::setupAgentManagementSection()
{
    m_agentGroup = new QGroupBox("Agent Management", this);
    m_agentLayout = new QVBoxLayout(m_agentGroup);

    // Agent creation controls
    m_agentControlsLayout = new QHBoxLayout();

    // Provider selection
    m_providerCombo = new QComboBox();
    m_providerCombo->addItems(m_coordinator->getSupportedProviders());
    m_providerCombo->setCurrentText("openai");
    m_agentControlsLayout->addWidget(new QLabel("Provider:"));
    m_agentControlsLayout->addWidget(m_providerCombo);

    // Model selection
    m_modelCombo = new QComboBox();
    updateModelCombo(); // Populate based on selected provider
    m_agentControlsLayout->addWidget(new QLabel("Model:"));
    m_agentControlsLayout->addWidget(m_modelCombo);

    // Role input
    m_roleEdit = new QLineEdit("general");
    m_roleEdit->setPlaceholderText("Agent role (e.g., coder, reviewer)");
    m_agentControlsLayout->addWidget(new QLabel("Role:"));
    m_agentControlsLayout->addWidget(m_roleEdit);

    // Control buttons
    m_createAgentBtn = new QPushButton("Create Agent");
    m_createAgentBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
    m_agentControlsLayout->addWidget(m_createAgentBtn);

    m_removeAgentBtn = new QPushButton("Remove Selected");
    m_removeAgentBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
    m_agentControlsLayout->addWidget(m_removeAgentBtn);

    m_switchModelBtn = new QPushButton("Switch Model");
    m_switchModelBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_BrowserReload));
    m_agentControlsLayout->addWidget(m_switchModelBtn);

    m_agentLayout->addLayout(m_agentControlsLayout);

    // Agent list and status
    QHBoxLayout* listLayout = new QHBoxLayout();

    // Agent list
    m_agentList = new QListWidget();
    m_agentList->setMaximumWidth(300);
    m_agentList->setSelectionMode(QAbstractItemView::SingleSelection);
    listLayout->addWidget(m_agentList);

    // Agent status table
    m_agentStatusTable = new QTableWidget();
    m_agentStatusTable->setColumnCount(5);
    m_agentStatusTable->setHorizontalHeaderLabels({"Agent ID", "Model", "Role", "Status", "Quality"});
    m_agentStatusTable->horizontalHeader()->setStretchLastSection(true);
    m_agentStatusTable->setAlternatingRowColors(true);
    m_agentStatusTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    listLayout->addWidget(m_agentStatusTable);

    m_agentLayout->addLayout(listLayout);

    m_mainSplitter->addWidget(m_agentGroup);

    // Connect provider change to update models
    connect(m_providerCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            this, &MultiModelAgentPanel::updateModelCombo);
}

void MultiModelAgentPanel::setupQueryExecutionSection()
{
    m_queryGroup = new QGroupBox("Parallel Query Execution", this);
    m_queryLayout = new QVBoxLayout(m_queryGroup);

    // Query input
    QLabel* queryLabel = new QLabel("Query:");
    m_queryInput = new QTextEdit();
    m_queryInput->setPlaceholderText("Enter your query here. Multiple agents will respond in parallel...");
    m_queryInput->setMaximumHeight(100);

    m_queryLayout->addWidget(queryLabel);
    m_queryLayout->addWidget(m_queryInput);

    // Agent selection for query
    QHBoxLayout* selectionLayout = new QHBoxLayout();
    QLabel* selectLabel = new QLabel("Selected Agents for Query:");

    m_selectedAgentsList = new QListWidget();
    m_selectedAgentsList->setMaximumWidth(300);
    m_selectedAgentsList->setSelectionMode(QAbstractItemView::MultiSelection);

    m_addToQueryBtn = new QPushButton("Add Selected →");
    m_removeFromQueryBtn = new QPushButton("← Remove Selected");

    selectionLayout->addWidget(selectLabel);
    selectionLayout->addWidget(m_selectedAgentsList);
    selectionLayout->addWidget(m_addToQueryBtn);
    selectionLayout->addWidget(m_removeFromQueryBtn);

    m_queryLayout->addLayout(selectionLayout);

    // Browser mode and execution controls
    QHBoxLayout* controlLayout = new QHBoxLayout();

    m_browserModeCheck = new QCheckBox("Enable Browser Mode");
    m_browserModeCheck->setChecked(false);
    m_browserStatusLabel = new QLabel("Browser mode disabled");
    m_browserStatusLabel->setStyleSheet("color: gray;");

    m_executeQueryBtn = new QPushButton("Execute Parallel Query");
    m_executeQueryBtn->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaPlay));
    m_executeQueryBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; "
                                    "border: none; padding: 8px 16px; border-radius: 4px; }"
                                    "QPushButton:hover { background-color: #45a049; }");

    controlLayout->addWidget(m_browserModeCheck);
    controlLayout->addWidget(m_browserStatusLabel);
    controlLayout->addStretch();
    controlLayout->addWidget(m_executeQueryBtn);

    m_queryLayout->addLayout(controlLayout);

    m_mainSplitter->addWidget(m_queryGroup);
}

void MultiModelAgentPanel::setupResultsDisplaySection()
{
    m_resultsGroup = new QGroupBox("Parallel Execution Results", this);
    m_resultsLayout = new QVBoxLayout(m_resultsGroup);

    m_resultsStack = new QStackedWidget();

    // Progress widget
    m_progressWidget = new QWidget();
    m_progressLayout = new QVBoxLayout(m_progressWidget);

    m_executionProgress = new QProgressBar();
    m_executionProgress->setRange(0, 100);
    m_executionProgress->setValue(0);

    m_progressLabel = new QLabel("Ready to execute queries...");
    m_progressLabel->setAlignment(Qt::AlignCenter);

    m_statsLabel = new QLabel("");
    m_statsLabel->setAlignment(Qt::AlignCenter);

    m_progressLayout->addWidget(m_executionProgress);
    m_progressLayout->addWidget(m_progressLabel);
    m_progressLayout->addWidget(m_statsLabel);

    m_resultsStack->addWidget(m_progressWidget);

    // Results widget
    m_resultsWidget = new QWidget();
    m_resultsDisplayLayout = new QVBoxLayout(m_resultsWidget);

    m_resultsTable = new QTableWidget();
    m_resultsTable->setColumnCount(4);
    m_resultsTable->setHorizontalHeaderLabels({"Agent", "Model", "Response Time", "Response"});
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);
    m_resultsTable->setAlternatingRowColors(true);
    m_resultsTable->verticalHeader()->setDefaultSectionSize(100);

    m_summaryText = new QTextEdit();
    m_summaryText->setMaximumHeight(150);
    m_summaryText->setReadOnly(true);

    m_resultsDisplayLayout->addWidget(m_resultsTable);
    m_resultsDisplayLayout->addWidget(m_summaryText);

    m_resultsStack->addWidget(m_resultsWidget);

    m_resultsLayout->addWidget(m_resultsStack);

    m_mainSplitter->addWidget(m_resultsGroup);

    // Start with progress widget visible
    m_resultsStack->setCurrentWidget(m_progressWidget);
}

void MultiModelAgentPanel::setupConnections()
{
    // Agent management connections
    connect(m_createAgentBtn, &QPushButton::clicked, this, &MultiModelAgentPanel::createNewAgent);
    connect(m_removeAgentBtn, &QPushButton::clicked, this, &MultiModelAgentPanel::removeSelectedAgent);
    connect(m_switchModelBtn, &QPushButton::clicked, this, &MultiModelAgentPanel::switchAgentModel);
    connect(m_agentList, &QListWidget::itemSelectionChanged, this, &MultiModelAgentPanel::onAgentSelectionChanged);

    // Query execution connections
    connect(m_addToQueryBtn, &QPushButton::clicked, [this]() {
        auto selectedItems = m_agentList->selectedItems();
        for (auto item : selectedItems) {
            QString agentId = item->data(Qt::UserRole).toString();
            if (m_selectedAgentsList->findItems(agentId, Qt::MatchExactly).isEmpty()) {
                new QListWidgetItem(agentId, m_selectedAgentsList);
            }
        }
    });

    connect(m_removeFromQueryBtn, &QPushButton::clicked, [this]() {
        auto selectedItems = m_selectedAgentsList->selectedItems();
        for (auto item : selectedItems) {
            delete item;
        }
    });

    connect(m_executeQueryBtn, &QPushButton::clicked, this, &MultiModelAgentPanel::executeParallelQuery);
    connect(m_browserModeCheck, &QCheckBox::toggled, this, &MultiModelAgentPanel::toggleBrowserMode);

    // Coordinator connections
    connect(m_coordinator, &MultiModelAgentCoordinator::agentCreated,
            this, &MultiModelAgentPanel::onAgentCreated);
    connect(m_coordinator, &MultiModelAgentCoordinator::agentRemoved,
            this, &MultiModelAgentPanel::onAgentRemoved);
    connect(m_coordinator, &MultiModelAgentCoordinator::parallelExecutionStarted,
            this, &MultiModelAgentPanel::onParallelExecutionStarted);
    connect(m_coordinator, &MultiModelAgentCoordinator::parallelExecutionCompleted,
            this, &MultiModelAgentPanel::onParallelExecutionCompleted);
    connect(m_coordinator, &MultiModelAgentCoordinator::agentResponseReceived,
            this, &MultiModelAgentPanel::onAgentResponseReceived);
}

void MultiModelAgentPanel::createNewAgent()
{
    QString provider = m_providerCombo->currentText();
    QString model = m_modelCombo->currentText();
    QString role = m_roleEdit->text().trimmed();

    if (role.isEmpty()) {
        role = "general";
    }

    QString agentId = m_coordinator->createAgent(provider, model, role);
    if (agentId.isEmpty()) {
        QMessageBox::warning(this, "Agent Creation Failed",
                           "Failed to create agent. Maximum number of agents reached or invalid configuration.");
    }
}

void MultiModelAgentPanel::removeSelectedAgent()
{
    QString agentId = getSelectedAgentId();
    if (!agentId.isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Remove Agent",
            "Are you sure you want to remove this agent?",
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            m_coordinator->removeAgent(agentId);
        }
    }
}

void MultiModelAgentPanel::switchAgentModel()
{
    QString agentId = getSelectedAgentId();
    if (agentId.isEmpty()) return;

    QString newProvider = m_providerCombo->currentText();
    QString newModel = m_modelCombo->currentText();

    m_coordinator->switchAgentModel(agentId, newProvider, newModel);
}

void MultiModelAgentPanel::executeParallelQuery()
{
    QString query = m_queryInput->toPlainText().trimmed();
    if (query.isEmpty()) {
        QMessageBox::warning(this, "Empty Query", "Please enter a query to execute.");
        return;
    }

    // Get selected agents
    QStringList selectedAgents;
    for (int i = 0; i < m_selectedAgentsList->count(); ++i) {
        selectedAgents.append(m_selectedAgentsList->item(i)->text());
    }

    if (selectedAgents.isEmpty()) {
        QMessageBox::warning(this, "No Agents Selected", "Please select at least one agent for the query.");
        return;
    }

    bool browserMode = m_browserModeCheck->isChecked();

    QString sessionId = m_coordinator->executeParallelQuery(query, selectedAgents, browserMode);
    emit parallelQueryExecuted(sessionId);
}

void MultiModelAgentPanel::toggleBrowserMode(bool enabled)
{
    m_coordinator->setBrowserModeEnabled(enabled);
    m_browserStatusLabel->setText(enabled ? "Browser mode enabled" : "Browser mode disabled");
    m_browserStatusLabel->setStyleSheet(enabled ? "color: green;" : "color: gray;");
}

void MultiModelAgentPanel::onAgentSelectionChanged()
{
    // Update controls based on selection
    bool hasSelection = !m_agentList->selectedItems().isEmpty();
    m_removeAgentBtn->setEnabled(hasSelection);
    m_switchModelBtn->setEnabled(hasSelection);
}

void MultiModelAgentPanel::updateAgentStatus()
{
    // Update status table with current agent information
    m_agentStatusTable->setRowCount(0);

    QStringList agentIds = m_coordinator->getAllAgentIds();
    for (const QString& agentId : agentIds) {
        QJsonObject status = m_coordinator->getAgentStatus(agentId);
        if (status.isEmpty()) continue;

        int row = m_agentStatusTable->rowCount();
        m_agentStatusTable->insertRow(row);

        m_agentStatusTable->setItem(row, 0, new QTableWidgetItem(status["agentId"].toString()));
        m_agentStatusTable->setItem(row, 1, new QTableWidgetItem(
            QString("%1/%2").arg(status["modelProvider"].toString(), status["modelName"].toString())));
        m_agentStatusTable->setItem(row, 2, new QTableWidgetItem(status["role"].toString()));
        m_agentStatusTable->setItem(row, 3, new QTableWidgetItem(
            status["isActive"].toBool() ? "Active" : "Inactive"));
        m_agentStatusTable->setItem(row, 4, new QTableWidgetItem(
            QString::number(status["qualityScore"].toDouble(), 'f', 2)));
    }

    m_agentStatusTable->resizeColumnsToContents();
}

void MultiModelAgentPanel::onAgentCreated(const QString& agentId, const QString& provider, const QString& model)
{
    addAgentToList(agentId);
    updateAgentStatus();
}

void MultiModelAgentPanel::onAgentRemoved(const QString& agentId)
{
    removeAgentFromList(agentId);
    updateAgentStatus();
}

void MultiModelAgentPanel::onParallelExecutionStarted(const QString& sessionId, int agentCount)
{
    m_currentSessionId = sessionId;
    m_currentResponses.clear();

    m_resultsStack->setCurrentWidget(m_progressWidget);
    m_executionProgress->setValue(0);
    m_progressLabel->setText(QString("Executing query on %1 agents...").arg(agentCount));
    m_statsLabel->setText("");

    m_executeQueryBtn->setEnabled(false);
    m_executeQueryBtn->setText("Executing...");
}

void MultiModelAgentPanel::onParallelExecutionCompleted(const QString& sessionId,
                                                      const QMap<QString, QString>& responses,
                                                      qint64 totalTimeMs,
                                                      double averageQuality,
                                                      qint64 fastestTimeMs,
                                                      qint64 slowestTimeMs)
{
    displayParallelResults(sessionId, responses, totalTimeMs, averageQuality);

    m_executeQueryBtn->setEnabled(true);
    m_executeQueryBtn->setText("Execute Parallel Query");
}

void MultiModelAgentPanel::onAgentResponseReceived(const QString& agentId,
                                                 const QString& response,
                                                 qint64 responseTimeMs,
                                                 float qualityScore)
{
    if (m_currentSessionId.isEmpty()) return;

    m_currentResponses[agentId] = response;

    // Update progress
    int completed = m_currentResponses.size();
    int total = m_selectedAgentsList->count();
    int percentage = total > 0 ? (completed * 100) / total : 0;

    m_executionProgress->setValue(percentage);
    m_progressLabel->setText(QString("Received %1/%2 responses...").arg(completed).arg(total));
}

void MultiModelAgentPanel::displayParallelResults(const QString& sessionId,
                                                const QMap<QString, QString>& responses,
                                                qint64 totalTimeMs,
                                                double averageQuality)
{
    m_resultsStack->setCurrentWidget(m_resultsWidget);

    // Clear previous results
    m_resultsTable->setRowCount(0);

    // Populate results table
    int row = 0;
    for (auto it = responses.begin(); it != responses.end(); ++it) {
        m_resultsTable->insertRow(row);

        // Get agent info
        QJsonObject agentStatus = m_coordinator->getAgentStatus(it.key());
        QString modelInfo = QString("%1/%2").arg(
            agentStatus["modelProvider"].toString(),
            agentStatus["modelName"].toString());

        m_resultsTable->setItem(row, 0, new QTableWidgetItem(it.key())); // Agent ID
        m_resultsTable->setItem(row, 1, new QTableWidgetItem(modelInfo)); // Model

        // Response time (we'll need to get this from the session data)
        QJsonObject sessionStatus = m_coordinator->getSessionStatus(sessionId);
        QString responseTime = "Unknown";
        if (sessionStatus.contains("responseTimes")) {
            QJsonObject times = sessionStatus["responseTimes"].toObject();
            if (times.contains(it.key())) {
                responseTime = QString("%1 ms").arg(times[it.key()].toInt());
            }
        }
        m_resultsTable->setItem(row, 2, new QTableWidgetItem(responseTime));

        // Response text (truncated for display)
        QString responseText = it.value();
        if (responseText.length() > 200) {
            responseText = responseText.left(200) + "...";
        }
        QTableWidgetItem* responseItem = new QTableWidgetItem(responseText);
        responseItem->setToolTip(it.value()); // Full text on hover
        m_resultsTable->setItem(row, 3, responseItem);

        row++;
    }

    m_resultsTable->resizeColumnsToContents();
    m_resultsTable->resizeRowsToContents();

    // Update summary
    QString summary = QString(
        "Parallel Execution Summary:\n"
        "• Total Time: %1 ms\n"
        "• Average Quality Score: %2\n"
        "• Agents: %3\n"
        "• Session ID: %4"
    ).arg(totalTimeMs).arg(averageQuality, 0, 'f', 2).arg(responses.size()).arg(sessionId);

    m_summaryText->setPlainText(summary);
}

void MultiModelAgentPanel::refreshAgentList()
{
    m_agentList->clear();

    QStringList agentIds = m_coordinator->getAllAgentIds();
    for (const QString& agentId : agentIds) {
        addAgentToList(agentId);
    }
}

void MultiModelAgentPanel::addAgentToList(const QString& agentId)
{
    QJsonObject status = m_coordinator->getAgentStatus(agentId);
    if (status.isEmpty()) return;

    QString displayText = QString("%1 (%2/%3) - %4")
        .arg(agentId.left(8) + "...")
        .arg(status["modelProvider"].toString())
        .arg(status["modelName"].toString())
        .arg(status["role"].toString());

    QListWidgetItem* item = new QListWidgetItem(displayText);
    item->setData(Qt::UserRole, agentId);
    m_agentList->addItem(item);
}

void MultiModelAgentPanel::removeAgentFromList(const QString& agentId)
{
    for (int i = 0; i < m_agentList->count(); ++i) {
        QListWidgetItem* item = m_agentList->item(i);
        if (item->data(Qt::UserRole).toString() == agentId) {
            delete m_agentList->takeItem(i);
            break;
        }
    }
}

void MultiModelAgentPanel::updateAgentDisplay(const QString& agentId)
{
    // Update the display text for the agent in the list
    for (int i = 0; i < m_agentList->count(); ++i) {
        QListWidgetItem* item = m_agentList->item(i);
        if (item->data(Qt::UserRole).toString() == agentId) {
            QJsonObject status = m_coordinator->getAgentStatus(agentId);
            QString displayText = QString("%1 (%2/%3) - %4")
                .arg(agentId.left(8) + "...")
                .arg(status["modelProvider"].toString())
                .arg(status["modelName"].toString())
                .arg(status["role"].toString());
            item->setText(displayText);
            break;
        }
    }
}

void MultiModelAgentPanel::updateModelCombo()
{
    QString provider = m_providerCombo->currentText();
    m_modelCombo->clear();
    m_modelCombo->addItems(m_coordinator->getModelsForProvider(provider));
}

QString MultiModelAgentPanel::getSelectedAgentId() const
{
    auto selectedItems = m_agentList->selectedItems();
    if (!selectedItems.isEmpty()) {
        return selectedItems.first()->data(Qt::UserRole).toString();
    }
    return QString();
}

void MultiModelAgentPanel::clearResultsDisplay()
{
    m_resultsTable->setRowCount(0);
    m_summaryText->clear();
    m_resultsStack->setCurrentWidget(m_progressWidget);
    m_executionProgress->setValue(0);
    m_progressLabel->setText("Ready to execute queries...");
    m_statsLabel->clear();
}

} // namespace IDE
} // namespace RawrXD