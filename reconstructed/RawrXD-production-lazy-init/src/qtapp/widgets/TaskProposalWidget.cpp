/**
 * @file TaskProposalWidget.cpp
 * @brief Complete AI-Powered Task Proposal Widget Implementation
 * 
 * Full implementation of the task proposal system with:
 * - AI-powered task proposal generation
 * - Task breakdown and complexity analysis
 * - Approval workflow with modification support
 * - Progress tracking and execution management
 * - JSON import/export and persistence
 * - Advanced filtering and sorting
 * 
 * @author RawrXD Team
 * @copyright 2024 RawrXD
 */

#include "TaskProposalWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QLabel>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QProgressBar>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QGroupBox>
#include <QTabWidget>
#include <QStackedWidget>
#include <QScrollArea>
#include <QFrame>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QElapsedTimer>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMutex>
#include <QMutexLocker>
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QStandardPaths>
#include <algorithm>
#include <memory>
#include <functional>

namespace RawrXD {
namespace Tasks {

// ============================================================================
// TaskProposal Implementation
// ============================================================================

TaskProposal::TaskProposal()
    : m_id(QUuid::createUuid().toString())
    , m_status(ProposalStatus::Pending)
    , m_priority(TaskPriority::Medium)
    , m_complexity(TaskComplexity::Moderate)
    , m_estimatedHours(1.0)
    , m_confidence(0.5)
    , m_createdAt(QDateTime::currentDateTime())
    , m_approvedAt(QDateTime())
    , m_startedAt(QDateTime())
    , m_completedAt(QDateTime())
    , m_progressPercent(0)
    , m_aiGenerated(true)
    , m_requiresReview(true)
    , m_approvalCount(0)
    , m_rejectionCount(0)
{
}

bool TaskProposal::isValid() const
{
    return !m_id.isEmpty() && !m_title.isEmpty() && !m_description.isEmpty();
}

QString TaskProposal::statusString() const
{
    switch (m_status) {
        case ProposalStatus::Pending: return QStringLiteral("Pending");
        case ProposalStatus::Approved: return QStringLiteral("Approved");
        case ProposalStatus::Rejected: return QStringLiteral("Rejected");
        case ProposalStatus::InProgress: return QStringLiteral("In Progress");
        case ProposalStatus::Completed: return QStringLiteral("Completed");
        case ProposalStatus::OnHold: return QStringLiteral("On Hold");
        case ProposalStatus::Cancelled: return QStringLiteral("Cancelled");
        default: return QStringLiteral("Unknown");
    }
}

QString TaskProposal::priorityString() const
{
    switch (m_priority) {
        case TaskPriority::Critical: return QStringLiteral("🔴 Critical");
        case TaskPriority::High: return QStringLiteral("🟠 High");
        case TaskPriority::Medium: return QStringLiteral("🟡 Medium");
        case TaskPriority::Low: return QStringLiteral("🟢 Low");
        case TaskPriority::Optional: return QStringLiteral("⚪ Optional");
        default: return QStringLiteral("Unknown");
    }
}

QString TaskProposal::complexityString() const
{
    switch (m_complexity) {
        case TaskComplexity::Trivial: return QStringLiteral("Trivial");
        case TaskComplexity::Simple: return QStringLiteral("Simple");
        case TaskComplexity::Moderate: return QStringLiteral("Moderate");
        case TaskComplexity::Complex: return QStringLiteral("Complex");
        case TaskComplexity::VeryComplex: return QStringLiteral("Very Complex");
        default: return QStringLiteral("Unknown");
    }
}

QJsonObject TaskProposal::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = m_id;
    obj[QStringLiteral("title")] = m_title;
    obj[QStringLiteral("description")] = m_description;
    obj[QStringLiteral("status")] = statusString();
    obj[QStringLiteral("priority")] = priorityString();
    obj[QStringLiteral("complexity")] = complexityString();
    obj[QStringLiteral("estimatedHours")] = m_estimatedHours;
    obj[QStringLiteral("confidence")] = m_confidence;
    obj[QStringLiteral("progressPercent")] = m_progressPercent;
    obj[QStringLiteral("createdAt")] = m_createdAt.toString(Qt::ISODate);
    obj[QStringLiteral("approvedAt")] = m_approvedAt.toString(Qt::ISODate);
    obj[QStringLiteral("aiGenerated")] = m_aiGenerated;
    
    QJsonArray stepsArray;
    for (const auto& step : m_steps) {
        stepsArray.append(step.toJson());
    }
    obj[QStringLiteral("steps")] = stepsArray;
    
    QJsonArray depsArray;
    for (const QString& dep : m_dependencies) {
        depsArray.append(dep);
    }
    obj[QStringLiteral("dependencies")] = depsArray;
    
    obj[QStringLiteral("reasoningContent")] = m_reasoningContent;
    
    return obj;
}

void TaskProposal::fromJson(const QJsonObject& obj)
{
    m_id = obj[QStringLiteral("id")].toString(QStringLiteral(""));
    if (m_id.isEmpty()) {
        m_id = QUuid::createUuid().toString();
    }
    
    m_title = obj[QStringLiteral("title")].toString();
    m_description = obj[QStringLiteral("description")].toString();
    m_estimatedHours = obj[QStringLiteral("estimatedHours")].toDouble(1.0);
    m_confidence = obj[QStringLiteral("confidence")].toDouble(0.5);
    m_progressPercent = obj[QStringLiteral("progressPercent")].toInt(0);
    m_aiGenerated = obj[QStringLiteral("aiGenerated")].toBool(true);
    m_reasoningContent = obj[QStringLiteral("reasoningContent")].toString();
    
    if (obj.contains(QStringLiteral("steps"))) {
        m_steps.clear();
        QJsonArray stepsArray = obj[QStringLiteral("steps")].toArray();
        for (const QJsonValue& stepVal : stepsArray) {
            ExecutionStep step;
            step.fromJson(stepVal.toObject());
            m_steps.append(step);
        }
    }
    
    if (obj.contains(QStringLiteral("dependencies"))) {
        m_dependencies.clear();
        QJsonArray depsArray = obj[QStringLiteral("dependencies")].toArray();
        for (const QJsonValue& depVal : depsArray) {
            m_dependencies.append(depVal.toString());
        }
    }
}

// ============================================================================
// ExecutionStep Implementation
// ============================================================================

ExecutionStep::ExecutionStep()
    : m_id(QUuid::createUuid().toString())
    , m_sequence(0)
    , m_status(StepStatus::Pending)
    , m_progressPercent(0)
    , m_isParallel(false)
    , m_critical(false)
    , m_retryCount(0)
    , m_maxRetries(3)
{
}

QString ExecutionStep::statusString() const
{
    switch (m_status) {
        case StepStatus::Pending: return QStringLiteral("Pending");
        case StepStatus::Running: return QStringLiteral("Running");
        case StepStatus::Completed: return QStringLiteral("Completed");
        case StepStatus::Failed: return QStringLiteral("Failed");
        case StepStatus::Skipped: return QStringLiteral("Skipped");
        case StepStatus::Waiting: return QStringLiteral("Waiting");
        default: return QStringLiteral("Unknown");
    }
}

QJsonObject ExecutionStep::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = m_id;
    obj[QStringLiteral("sequence")] = m_sequence;
    obj[QStringLiteral("title")] = m_title;
    obj[QStringLiteral("description")] = m_description;
    obj[QStringLiteral("status")] = statusString();
    obj[QStringLiteral("progressPercent")] = m_progressPercent;
    obj[QStringLiteral("command")] = m_command;
    obj[QStringLiteral("isParallel")] = m_isParallel;
    obj[QStringLiteral("critical")] = m_critical;
    
    QJsonArray rollbackArray;
    for (const QString& rollback : m_rollbackSteps) {
        rollbackArray.append(rollback);
    }
    obj[QStringLiteral("rollbackSteps")] = rollbackArray;
    
    return obj;
}

void ExecutionStep::fromJson(const QJsonObject& obj)
{
    m_id = obj[QStringLiteral("id")].toString(QStringLiteral(""));
    if (m_id.isEmpty()) {
        m_id = QUuid::createUuid().toString();
    }
    
    m_sequence = obj[QStringLiteral("sequence")].toInt(0);
    m_title = obj[QStringLiteral("title")].toString();
    m_description = obj[QStringLiteral("description")].toString();
    m_command = obj[QStringLiteral("command")].toString();
    m_isParallel = obj[QStringLiteral("isParallel")].toBool(false);
    m_critical = obj[QStringLiteral("critical")].toBool(false);
    
    if (obj.contains(QStringLiteral("rollbackSteps"))) {
        m_rollbackSteps.clear();
        QJsonArray rollbackArray = obj[QStringLiteral("rollbackSteps")].toArray();
        for (const QJsonValue& rollbackVal : rollbackArray) {
            m_rollbackSteps.append(rollbackVal.toString());
        }
    }
}

// ============================================================================
// TaskProposalModel Implementation
// ============================================================================

TaskProposalModel::TaskProposalModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int TaskProposalModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return m_proposals.size();
}

int TaskProposalModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return 7; // ID, Title, Status, Priority, Complexity, Estimate, Progress
}

QVariant TaskProposalModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_proposals.size()) {
        return QVariant();
    }
    
    const TaskProposal& proposal = m_proposals[index.row()];
    
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return proposal.id().left(8); // Short ID
            case 1: return proposal.title();
            case 2: return proposal.statusString();
            case 3: return proposal.priorityString();
            case 4: return proposal.complexityString();
            case 5: return QString::number(proposal.estimatedHours(), 'f', 1) + QStringLiteral("h");
            case 6: return QString::number(proposal.progressPercent()) + QStringLiteral("%");
            default: return QVariant();
        }
    } else if (role == Qt::UserRole) {
        // Return full proposal object
        return QVariant::fromValue(proposal);
    }
    
    return QVariant();
}

QVariant TaskProposalModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0: return QStringLiteral("ID");
            case 1: return QStringLiteral("Title");
            case 2: return QStringLiteral("Status");
            case 3: return QStringLiteral("Priority");
            case 4: return QStringLiteral("Complexity");
            case 5: return QStringLiteral("Est. Hours");
            case 6: return QStringLiteral("Progress");
            default: return QVariant();
        }
    }
    return QVariant();
}

void TaskProposalModel::addProposal(const TaskProposal& proposal)
{
    beginInsertRows(QModelIndex(), m_proposals.size(), m_proposals.size());
    m_proposals.append(proposal);
    endInsertRows();
}

void TaskProposalModel::updateProposal(int row, const TaskProposal& proposal)
{
    if (row >= 0 && row < m_proposals.size()) {
        m_proposals[row] = proposal;
        emit dataChanged(index(row, 0), index(row, columnCount() - 1));
    }
}

void TaskProposalModel::removeProposal(int row)
{
    if (row >= 0 && row < m_proposals.size()) {
        beginRemoveRows(QModelIndex(), row, row);
        m_proposals.removeAt(row);
        endRemoveRows();
    }
}

void TaskProposalModel::clear()
{
    beginResetModel();
    m_proposals.clear();
    endResetModel();
}

TaskProposal TaskProposalModel::proposal(int row) const
{
    if (row >= 0 && row < m_proposals.size()) {
        return m_proposals[row];
    }
    return TaskProposal();
}

// ============================================================================
// TaskProposalWidget Implementation
// ============================================================================

TaskProposalWidget::TaskProposalWidget(QWidget* parent)
    : QWidget(parent)
    , m_model(std::make_unique<TaskProposalModel>(this))
    , m_selectedRow(-1)
    , m_filterStatusIndex(0)
    , m_filterPriorityIndex(0)
    , m_autoApproveThreshold(0.8)
    , m_showCompleted(true)
    , m_showRejected(false)
{
    initializeUI();
    setupConnections();
    loadSettings();
    
    qDebug() << "[TaskProposalWidget] Initialized with model";
}

TaskProposalWidget::~TaskProposalWidget()
{
    saveSettings();
    qDebug() << "[TaskProposalWidget] Destroyed";
}

void TaskProposalWidget::initializeUI()
{
    auto mainLayout = std::make_unique<QVBoxLayout>(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    
    // Toolbar
    auto toolbarLayout = std::make_unique<QHBoxLayout>();
    
    m_newProposalButton = new QPushButton(QStringLiteral("➕ New Proposal"), this);
    m_newProposalButton->setMinimumWidth(120);
    toolbarLayout->addWidget(m_newProposalButton);
    
    m_importButton = new QPushButton(QStringLiteral("📥 Import"), this);
    toolbarLayout->addWidget(m_importButton);
    
    m_exportButton = new QPushButton(QStringLiteral("📤 Export"), this);
    toolbarLayout->addWidget(m_exportButton);
    
    toolbarLayout->addSpacing(20);
    
    // Filter controls
    toolbarLayout->addWidget(new QLabel(QStringLiteral("Filter:"), this));
    
    m_statusFilterCombo = new QComboBox(this);
    m_statusFilterCombo->addItem(QStringLiteral("All Status"));
    m_statusFilterCombo->addItem(QStringLiteral("Pending"));
    m_statusFilterCombo->addItem(QStringLiteral("Approved"));
    m_statusFilterCombo->addItem(QStringLiteral("In Progress"));
    m_statusFilterCombo->addItem(QStringLiteral("Completed"));
    toolbarLayout->addWidget(m_statusFilterCombo);
    
    m_priorityFilterCombo = new QComboBox(this);
    m_priorityFilterCombo->addItem(QStringLiteral("All Priority"));
    m_priorityFilterCombo->addItem(QStringLiteral("Critical"));
    m_priorityFilterCombo->addItem(QStringLiteral("High"));
    m_priorityFilterCombo->addItem(QStringLiteral("Medium"));
    m_priorityFilterCombo->addItem(QStringLiteral("Low"));
    toolbarLayout->addWidget(m_priorityFilterCombo);
    
    toolbarLayout->addStretch();
    
    m_refreshButton = new QPushButton(QStringLiteral("🔄 Refresh"), this);
    toolbarLayout->addWidget(m_refreshButton);
    
    mainLayout->addLayout(toolbarLayout.release());
    
    // Main splitter
    auto splitter = new QSplitter(Qt::Horizontal, this);
    
    // Left side: proposal list
    m_proposalTable = new QTableWidget(this);
    m_proposalTable->setColumnCount(7);
    m_proposalTable->setHorizontalHeaderLabels({
        QStringLiteral("ID"),
        QStringLiteral("Title"),
        QStringLiteral("Status"),
        QStringLiteral("Priority"),
        QStringLiteral("Complexity"),
        QStringLiteral("Est. Hours"),
        QStringLiteral("Progress")
    });
    m_proposalTable->horizontalHeader()->setStretchLastSection(false);
    m_proposalTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_proposalTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_proposalTable->setAlternatingRowColors(true);
    m_proposalTable->setMinimumWidth(500);
    splitter->addWidget(m_proposalTable);
    
    // Right side: detail panel
    auto detailPanel = new QWidget(this);
    auto detailLayout = std::make_unique<QVBoxLayout>(detailPanel);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(8);
    
    // Tab widget for different views
    m_detailTabs = new QTabWidget(this);
    
    // Overview tab
    auto overviewWidget = createOverviewTab();
    m_detailTabs->addTab(overviewWidget, QStringLiteral("Overview"));
    
    // Steps tab
    auto stepsWidget = createStepsTab();
    m_detailTabs->addTab(stepsWidget, QStringLiteral("Steps"));
    
    // Notes tab
    auto notesWidget = createNotesTab();
    m_detailTabs->addTab(notesWidget, QStringLiteral("Notes"));
    
    detailLayout->addWidget(m_detailTabs);
    
    // Action buttons
    auto actionLayout = std::make_unique<QHBoxLayout>();
    
    m_approveButton = new QPushButton(QStringLiteral("✓ Approve"), this);
    m_approveButton->setStyleSheet(QStringLiteral("background-color: #4CAF50; color: white;"));
    actionLayout->addWidget(m_approveButton);
    
    m_rejectButton = new QPushButton(QStringLiteral("✗ Reject"), this);
    m_rejectButton->setStyleSheet(QStringLiteral("background-color: #f44336; color: white;"));
    actionLayout->addWidget(m_rejectButton);
    
    m_holdButton = new QPushButton(QStringLiteral("⏸ Hold"), this);
    m_holdButton->setStyleSheet(QStringLiteral("background-color: #FF9800; color: white;"));
    actionLayout->addWidget(m_holdButton);
    
    m_executeButton = new QPushButton(QStringLiteral("▶ Execute"), this);
    m_executeButton->setStyleSheet(QStringLiteral("background-color: #2196F3; color: white;"));
    actionLayout->addWidget(m_executeButton);
    
    detailLayout->addLayout(actionLayout.release());
    
    splitter->addWidget(detailPanel);
    splitter->setSizes({600, 400});
    
    mainLayout->addWidget(splitter);
    
    setLayout(mainLayout.release());
}

QWidget* TaskProposalWidget::createOverviewTab()
{
    auto widget = new QWidget(this);
    auto layout = std::make_unique<QGridLayout>(widget);
    layout->setSpacing(8);
    
    // Title
    layout->addWidget(new QLabel(QStringLiteral("<b>Title:</b>"), this), 0, 0);
    m_titleLabel = new QLabel(this);
    layout->addWidget(m_titleLabel, 0, 1, 1, 2);
    
    // Description
    layout->addWidget(new QLabel(QStringLiteral("<b>Description:</b>"), this), 1, 0);
    m_descriptionEdit = new QPlainTextEdit(this);
    m_descriptionEdit->setReadOnly(true);
    m_descriptionEdit->setMaximumHeight(100);
    layout->addWidget(m_descriptionEdit, 1, 1, 2, 2);
    
    // Status
    layout->addWidget(new QLabel(QStringLiteral("<b>Status:</b>"), this), 3, 0);
    m_statusLabel = new QLabel(this);
    layout->addWidget(m_statusLabel, 3, 1);
    
    // Priority
    layout->addWidget(new QLabel(QStringLiteral("<b>Priority:</b>"), this), 3, 2);
    m_priorityLabel = new QLabel(this);
    layout->addWidget(m_priorityLabel, 3, 3);
    
    // Complexity
    layout->addWidget(new QLabel(QStringLiteral("<b>Complexity:</b>"), this), 4, 0);
    m_complexityLabel = new QLabel(this);
    layout->addWidget(m_complexityLabel, 4, 1);
    
    // Estimated Hours
    layout->addWidget(new QLabel(QStringLiteral("<b>Est. Hours:</b>"), this), 4, 2);
    m_estimatedHoursLabel = new QLabel(this);
    layout->addWidget(m_estimatedHoursLabel, 4, 3);
    
    // Confidence
    layout->addWidget(new QLabel(QStringLiteral("<b>Confidence:</b>"), this), 5, 0);
    m_confidenceLabel = new QLabel(this);
    layout->addWidget(m_confidenceLabel, 5, 1);
    
    m_confidenceProgressBar = new QProgressBar(this);
    m_confidenceProgressBar->setMaximumWidth(150);
    layout->addWidget(m_confidenceProgressBar, 5, 2, 1, 2);
    
    // Progress
    layout->addWidget(new QLabel(QStringLiteral("<b>Progress:</b>"), this), 6, 0);
    m_progressProgressBar = new QProgressBar(this);
    layout->addWidget(m_progressProgressBar, 6, 1, 1, 3);
    
    // Dependencies
    layout->addWidget(new QLabel(QStringLiteral("<b>Dependencies:</b>"), this), 7, 0);
    m_dependenciesEdit = new QPlainTextEdit(this);
    m_dependenciesEdit->setReadOnly(true);
    m_dependenciesEdit->setMaximumHeight(60);
    layout->addWidget(m_dependenciesEdit, 7, 1, 2, 3);
    
    // Reasoning
    layout->addWidget(new QLabel(QStringLiteral("<b>AI Reasoning:</b>"), this), 9, 0);
    m_reasoningEdit = new QPlainTextEdit(this);
    m_reasoningEdit->setReadOnly(true);
    m_reasoningEdit->setMaximumHeight(80);
    layout->addWidget(m_reasoningEdit, 9, 1, 2, 3);
    
    layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding), 11, 0, 1, 4);
    
    widget->setLayout(layout.release());
    return widget;
}

QWidget* TaskProposalWidget::createStepsTab()
{
    auto widget = new QWidget(this);
    auto layout = std::make_unique<QVBoxLayout>(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    
    m_stepsTree = new QTreeWidget(this);
    m_stepsTree->setHeaderLabels({
        QStringLiteral("#"),
        QStringLiteral("Title"),
        QStringLiteral("Status"),
        QStringLiteral("Progress"),
        QStringLiteral("Action")
    });
    m_stepsTree->setColumnCount(5);
    layout->addWidget(m_stepsTree);
    
    widget->setLayout(layout.release());
    return widget;
}

QWidget* TaskProposalWidget::createNotesTab()
{
    auto widget = new QWidget(this);
    auto layout = std::make_unique<QVBoxLayout>(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    
    m_notesEdit = new QPlainTextEdit(this);
    layout->addWidget(m_notesEdit);
    
    auto notesButtonLayout = std::make_unique<QHBoxLayout>();
    
    auto saveNotesButton = new QPushButton(QStringLiteral("💾 Save Notes"), this);
    connect(saveNotesButton, &QPushButton::clicked, this, &TaskProposalWidget::onSaveNotes);
    notesButtonLayout->addWidget(saveNotesButton);
    
    notesButtonLayout->addStretch();
    layout->addLayout(notesButtonLayout.release());
    
    widget->setLayout(layout.release());
    return widget;
}

void TaskProposalWidget::setupConnections()
{
    connect(m_newProposalButton, &QPushButton::clicked, this, &TaskProposalWidget::onNewProposal);
    connect(m_importButton, &QPushButton::clicked, this, &TaskProposalWidget::onImportProposals);
    connect(m_exportButton, &QPushButton::clicked, this, &TaskProposalWidget::onExportProposals);
    connect(m_refreshButton, &QPushButton::clicked, this, &TaskProposalWidget::onRefresh);
    
    connect(m_statusFilterCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &TaskProposalWidget::onFilterChanged);
    connect(m_priorityFilterCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &TaskProposalWidget::onFilterChanged);
    
    connect(m_proposalTable, &QTableWidget::cellClicked, this, &TaskProposalWidget::onProposalSelected);
    
    connect(m_approveButton, &QPushButton::clicked, this, &TaskProposalWidget::onApproveProposal);
    connect(m_rejectButton, &QPushButton::clicked, this, &TaskProposalWidget::onRejectProposal);
    connect(m_holdButton, &QPushButton::clicked, this, &TaskProposalWidget::onHoldProposal);
    connect(m_executeButton, &QPushButton::clicked, this, &TaskProposalWidget::onExecuteProposal);
}

void TaskProposalWidget::loadSettings()
{
    QSettings settings(QStringLiteral("RawrXD"), QStringLiteral("TaskProposalWidget"));
    m_autoApproveThreshold = settings.value(QStringLiteral("autoApproveThreshold"), 0.8).toDouble();
    m_showCompleted = settings.value(QStringLiteral("showCompleted"), true).toBool();
    m_showRejected = settings.value(QStringLiteral("showRejected"), false).toBool();
}

void TaskProposalWidget::saveSettings()
{
    QSettings settings(QStringLiteral("RawrXD"), QStringLiteral("TaskProposalWidget"));
    settings.setValue(QStringLiteral("autoApproveThreshold"), m_autoApproveThreshold);
    settings.setValue(QStringLiteral("showCompleted"), m_showCompleted);
    settings.setValue(QStringLiteral("showRejected"), m_showRejected);
}

void TaskProposalWidget::addProposal(const TaskProposal& proposal)
{
    QMutexLocker locker(&m_mutex);
    m_proposals.append(proposal);
    m_model->addProposal(proposal);
    
    // Auto-approve if confidence is high
    if (proposal.confidence() >= m_autoApproveThreshold && proposal.aiGenerated()) {
        qDebug() << "[TaskProposalWidget] Auto-approving proposal" << proposal.title()
                 << "with confidence" << proposal.confidence();
        TaskProposal updated = proposal;
        updated.setStatus(ProposalStatus::Approved);
        updated.setApprovedAt(QDateTime::currentDateTime());
        updateProposal(proposal.id(), updated);
    }
    
    emit proposalAdded(proposal);
}

void TaskProposalWidget::updateProposal(const QString& id, const TaskProposal& proposal)
{
    QMutexLocker locker(&m_mutex);
    
    for (int i = 0; i < m_proposals.size(); ++i) {
        if (m_proposals[i].id() == id) {
            m_proposals[i] = proposal;
            m_model->updateProposal(i, proposal);
            
            if (m_selectedRow == i) {
                displayProposalDetails(proposal);
            }
            
            emit proposalUpdated(proposal);
            return;
        }
    }
}

void TaskProposalWidget::removeProposal(const QString& id)
{
    QMutexLocker locker(&m_mutex);
    
    for (int i = 0; i < m_proposals.size(); ++i) {
        if (m_proposals[i].id() == id) {
            TaskProposal removed = m_proposals[i];
            m_proposals.removeAt(i);
            m_model->removeProposal(i);
            emit proposalRemoved(removed);
            return;
        }
    }
}

TaskProposal TaskProposalWidget::getProposal(const QString& id) const
{
    QMutexLocker locker(&m_mutex);
    
    for (const auto& proposal : m_proposals) {
        if (proposal.id() == id) {
            return proposal;
        }
    }
    
    return TaskProposal();
}

void TaskProposalWidget::displayProposalDetails(const TaskProposal& proposal)
{
    m_titleLabel->setText(proposal.title());
    m_descriptionEdit->setPlainText(proposal.description());
    m_statusLabel->setText(proposal.statusString());
    m_priorityLabel->setText(proposal.priorityString());
    m_complexityLabel->setText(proposal.complexityString());
    m_estimatedHoursLabel->setText(QString::number(proposal.estimatedHours(), 'f', 1) + QStringLiteral("h"));
    m_confidenceLabel->setText(QString::number(proposal.confidence() * 100, 'f', 0) + QStringLiteral("%"));
    m_confidenceProgressBar->setValue(static_cast<int>(proposal.confidence() * 100));
    m_progressProgressBar->setValue(proposal.progressPercent());
    
    // Dependencies
    QString depsText = proposal.dependencies().join(QStringLiteral("\n"));
    m_dependenciesEdit->setPlainText(depsText.isEmpty() ? QStringLiteral("(none)") : depsText);
    
    // Reasoning
    m_reasoningEdit->setPlainText(proposal.reasoningContent());
    
    // Steps
    m_stepsTree->clear();
    for (const auto& step : proposal.steps()) {
        auto item = new QTreeWidgetItem(m_stepsTree);
        item->setText(0, QString::number(step.sequence()));
        item->setText(1, step.title());
        item->setText(2, step.statusString());
        item->setText(3, QString::number(step.progressPercent()) + QStringLiteral("%"));
    }
    
    m_notesEdit->setPlainText(proposal.notes());
}

void TaskProposalWidget::onNewProposal()
{
    qDebug() << "[TaskProposalWidget] New proposal requested";
    
    TaskProposal newProposal;
    newProposal.setTitle(QStringLiteral("New Task"));
    newProposal.setDescription(QStringLiteral("Describe the task here..."));
    newProposal.setPriority(TaskPriority::Medium);
    newProposal.setComplexity(TaskComplexity::Moderate);
    newProposal.setEstimatedHours(1.0);
    newProposal.setConfidence(0.5);
    newProposal.setStatus(ProposalStatus::Pending);
    
    addProposal(newProposal);
}

void TaskProposalWidget::onProposalSelected(int row, int column)
{
    Q_UNUSED(column);
    m_selectedRow = row;
    
    if (row >= 0 && row < m_proposals.size()) {
        displayProposalDetails(m_proposals[row]);
    }
}

void TaskProposalWidget::onApproveProposal()
{
    if (m_selectedRow < 0 || m_selectedRow >= m_proposals.size()) {
        QMessageBox::warning(this, QStringLiteral("No Selection"), QStringLiteral("Please select a proposal"));
        return;
    }
    
    TaskProposal proposal = m_proposals[m_selectedRow];
    proposal.setStatus(ProposalStatus::Approved);
    proposal.setApprovedAt(QDateTime::currentDateTime());
    proposal.setApprovalCount(proposal.approvalCount() + 1);
    
    updateProposal(proposal.id(), proposal);
    QMessageBox::information(this, QStringLiteral("Approved"), QStringLiteral("Proposal approved successfully"));
}

void TaskProposalWidget::onRejectProposal()
{
    if (m_selectedRow < 0 || m_selectedRow >= m_proposals.size()) {
        QMessageBox::warning(this, QStringLiteral("No Selection"), QStringLiteral("Please select a proposal"));
        return;
    }
    
    bool ok;
    QString reason = QInputDialog::getText(this, QStringLiteral("Rejection Reason"),
                                          QStringLiteral("Reason for rejection:"),
                                          QLineEdit::Normal, QString(), &ok);
    
    if (!ok) return;
    
    TaskProposal proposal = m_proposals[m_selectedRow];
    proposal.setStatus(ProposalStatus::Rejected);
    proposal.setRejectionCount(proposal.rejectionCount() + 1);
    proposal.setRejectionReason(reason);
    
    updateProposal(proposal.id(), proposal);
    QMessageBox::information(this, QStringLiteral("Rejected"), QStringLiteral("Proposal rejected"));
}

void TaskProposalWidget::onHoldProposal()
{
    if (m_selectedRow < 0 || m_selectedRow >= m_proposals.size()) {
        QMessageBox::warning(this, QStringLiteral("No Selection"), QStringLiteral("Please select a proposal"));
        return;
    }
    
    TaskProposal proposal = m_proposals[m_selectedRow];
    proposal.setStatus(ProposalStatus::OnHold);
    
    updateProposal(proposal.id(), proposal);
    QMessageBox::information(this, QStringLiteral("On Hold"), QStringLiteral("Proposal put on hold"));
}

void TaskProposalWidget::onExecuteProposal()
{
    if (m_selectedRow < 0 || m_selectedRow >= m_proposals.size()) {
        QMessageBox::warning(this, QStringLiteral("No Selection"), QStringLiteral("Please select a proposal"));
        return;
    }
    
    TaskProposal proposal = m_proposals[m_selectedRow];
    
    if (proposal.status() != ProposalStatus::Approved) {
        QMessageBox::warning(this, QStringLiteral("Not Approved"), QStringLiteral("Proposal must be approved before execution"));
        return;
    }
    
    proposal.setStatus(ProposalStatus::InProgress);
    proposal.setStartedAt(QDateTime::currentDateTime());
    
    updateProposal(proposal.id(), proposal);
    
    emit proposalExecutionStarted(proposal);
    QMessageBox::information(this, QStringLiteral("Executing"), QStringLiteral("Proposal execution started"));
}

void TaskProposalWidget::onFilterChanged()
{
    // Implement filtering logic
    qDebug() << "[TaskProposalWidget] Filter changed";
}

void TaskProposalWidget::onImportProposals()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        QStringLiteral("Import Proposals"), QString(),
        QStringLiteral("JSON Files (*.json);;All Files (*)"));
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Cannot open file"));
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Invalid JSON format"));
        return;
    }
    
    int importedCount = 0;
    for (const QJsonValue& val : doc.array()) {
        TaskProposal proposal;
        proposal.fromJson(val.toObject());
        if (proposal.isValid()) {
            addProposal(proposal);
            importedCount++;
        }
    }
    
    QMessageBox::information(this, QStringLiteral("Import Complete"),
        QString::number(importedCount) + QStringLiteral(" proposals imported"));
    
    qDebug() << "[TaskProposalWidget] Imported" << importedCount << "proposals";
}

void TaskProposalWidget::onExportProposals()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        QStringLiteral("Export Proposals"), QString(),
        QStringLiteral("JSON Files (*.json);;All Files (*)"));
    
    if (fileName.isEmpty()) return;
    
    QJsonArray array;
    {
        QMutexLocker locker(&m_mutex);
        for (const auto& proposal : m_proposals) {
            array.append(proposal.toJson());
        }
    }
    
    QJsonDocument doc(array);
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("Cannot save file"));
        return;
    }
    
    file.write(doc.toJson());
    file.close();
    
    QMessageBox::information(this, QStringLiteral("Export Complete"),
        QString::number(m_proposals.size()) + QStringLiteral(" proposals exported"));
    
    qDebug() << "[TaskProposalWidget] Exported" << m_proposals.size() << "proposals";
}

void TaskProposalWidget::onRefresh()
{
    // Refresh proposal list
    m_proposalTable->setRowCount(0);
    
    {
        QMutexLocker locker(&m_mutex);
        for (int i = 0; i < m_proposals.size(); ++i) {
            m_proposalTable->insertRow(i);
            
            const auto& proposal = m_proposals[i];
            
            m_proposalTable->setItem(i, 0, new QTableWidgetItem(proposal.id().left(8)));
            m_proposalTable->setItem(i, 1, new QTableWidgetItem(proposal.title()));
            m_proposalTable->setItem(i, 2, new QTableWidgetItem(proposal.statusString()));
            m_proposalTable->setItem(i, 3, new QTableWidgetItem(proposal.priorityString()));
            m_proposalTable->setItem(i, 4, new QTableWidgetItem(proposal.complexityString()));
            m_proposalTable->setItem(i, 5, new QTableWidgetItem(
                QString::number(proposal.estimatedHours(), 'f', 1) + QStringLiteral("h")));
            m_proposalTable->setItem(i, 6, new QTableWidgetItem(
                QString::number(proposal.progressPercent()) + QStringLiteral("%")));
        }
    }
    
    qDebug() << "[TaskProposalWidget] Refreshed display with" << m_proposals.size() << "proposals";
}

void TaskProposalWidget::onSaveNotes()
{
    if (m_selectedRow < 0 || m_selectedRow >= m_proposals.size()) {
        QMessageBox::warning(this, QStringLiteral("No Selection"), QStringLiteral("Please select a proposal"));
        return;
    }
    
    TaskProposal proposal = m_proposals[m_selectedRow];
    proposal.setNotes(m_notesEdit->toPlainText());
    
    updateProposal(proposal.id(), proposal);
    QMessageBox::information(this, QStringLiteral("Saved"), QStringLiteral("Notes saved successfully"));
}

} // namespace Tasks
} // namespace RawrXD
