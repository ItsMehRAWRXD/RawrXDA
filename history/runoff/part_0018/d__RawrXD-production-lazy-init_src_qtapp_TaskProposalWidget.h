/**
 * @file TaskProposalWidget.h
 * @brief Complete Task Proposal Widget for RawrXD Agentic IDE
 * 
 * Provides UI for displaying AI-proposed tasks and managing approval workflow:
 * - Task proposal display with details and rationale
 * - Interactive approval/rejection interface
 * - Task prioritization and scheduling
 * - Progress tracking and notifications
 * - Batch task processing capabilities
 * - Custom task templates and automation
 * 
 * @author RawrXD Team
 * @copyright 2024 RawrXD
 */

#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QSlider>
#include <QProgressBar>
#include <QGroupBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QMenuBar>
#include <QToolBar>
#include <QToolButton>
#include <QDateEdit>
#include <QTimeEdit>
#include <QDateTimeEdit>
#include <QFont>
#include <QFontComboBox>
#include <QColor>
#include <QColorDialog>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QApplication>
#include <QClipboard>
#include <QDrag>
#include <QDropEvent>
#include <QMimeData>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QMap>
#include <QSet>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <memory>

namespace RawrXD {

/**
 * @brief Task proposal types
 */
enum class ProposalType {
    Refactoring,         ///< Code refactoring suggestions
    Optimization,       ///< Performance optimizations
    Security,           ///< Security improvements
    Documentation,      ///< Documentation updates
    Testing,           ///< Test generation
    BugFix,           ///< Bug fix proposals
    Feature,           ///< New feature suggestions
    Migration,         ///< Code migration
    Cleanup,          ///< Code cleanup tasks
    Review,           ///< Code review tasks
    Custom            ///< Custom task types
};

/**
 * @brief Task proposal status
 */
enum class ProposalStatus {
    Pending,            ///< Awaiting review
    Approved,          ///< Approved for execution
    Rejected,          ///< Rejected by user
    InProgress,        Currently executing
    Completed,         ///< Successfully completed
    Failed,           Execution failed
    Cancelled,        Execution cancelled
    OnHold            Temporarily on hold
};

/**
 * @brief Task priority levels
 */
enum class TaskPriority {
    Low,              ///< Low priority
    Normal,          Normal priority
    High,            High priority
    Critical,        Critical priority
    Emergency        Emergency priority
};

/**
 * @brief Task category
 */
enum class TaskCategory {
    CodeQuality,      Code quality improvements
    Performance,      Performance optimization
    Security,         Security enhancements
    Documentation,    Documentation updates
    Testing,         Test-related tasks
    Maintenance,      Maintenance tasks
    NewFeature,       New feature development
    BugFix,          Bug fixes
    Refactoring,      Code refactoring
    Custom            Custom category
};

/**
 * @brief Represents a task proposal from AI
 */
class TaskProposal {
public:
    TaskProposal();
    TaskProposal(const QString& id, const QString& title, ProposalType type);
    
    QString id() const { return m_id; }
    void setId(const QString& id) { m_id = id; }
    
    QString title() const { return m_title; }
    void setTitle(const QString& title) { m_title = title; }
    
    QString description() const { return m_description; }
    void setDescription(const QString& desc) { m_description = desc; }
    
    QString rationale() const { return m_rationale; }
    void setRationale(const QString& rationale) { m_rationale = rationale; }
    
    QStringList affectedFiles() const { return m_affectedFiles; }
    void setAffectedFiles(const QStringList& files) { m_affectedFiles = files; }
    void addAffectedFile(const QString& file) { m_affectedFiles.append(file); }
    
    ProposalType type() const { return m_type; }
    void setType(ProposalType type) { m_type = type; }
    
    ProposalStatus status() const { return m_status; }
    void setStatus(ProposalStatus status) { m_status = status; }
    
    TaskPriority priority() const { return m_priority; }
    void setPriority(TaskPriority priority) { m_priority = priority; }
    
    TaskCategory category() const { return m_category; }
    void setCategory(TaskCategory category) { m_category = category; }
    
    QString proposer() const { return m_proposer; }
    void setProposer(const QString& proposer) { m_proposer = proposer; }
    
    QDateTime createdAt() const { return m_createdAt; }
    void setCreatedAt(const QDateTime& time) { m_createdAt = time; }
    
    QDateTime reviewedAt() const { return m_reviewedAt; }
    void setReviewedAt(const QDateTime& time) { m_reviewedAt = time; }
    
    QString reviewer() const { return m_reviewer; }
    void setReviewer(const QString& reviewer) { m_reviewer = reviewer; }
    
    QString reviewNotes() const { return m_reviewNotes; }
    void setReviewNotes(const QString& notes) { m_reviewNotes = notes; }
    
    int estimatedEffort() const { return m_estimatedEffort; }
    void setEstimatedEffort(int effort) { m_estimatedEffort = effort; }
    
    double confidence() const { return m_confidence; }
    void setConfidence(double confidence) { m_confidence = confidence; }
    
    QJsonObject metadata() const { return m_metadata; }
    void setMetadata(const QJsonObject& metadata) { m_metadata = metadata; }
    
    QStringList tags() const { return m_tags; }
    void setTags(const QStringList& tags) { m_tags = tags; }
    void addTag(const QString& tag) { m_tags.append(tag); }
    
    bool isAutoApplicable() const { return m_autoApplicable; }
    void setAutoApplicable(bool autoApplicable) { m_autoApplicable = autoApplicable; }
    
    QString impact() const { return m_impact; }
    void setImpact(const QString& impact) { m_impact = impact; }
    
    QString risk() const { return m_risk; }
    void setRisk(const QString& risk) { m_risk = risk; }
    
    bool isValid() const;
    
    QString toDisplayString() const;
    
private:
    QString m_id;
    QString m_title;
    QString m_description;
    QString m_rationale;
    QStringList m_affectedFiles;
    ProposalType m_type;
    ProposalStatus m_status;
    TaskPriority m_priority;
    TaskCategory m_category;
    QString m_proposer;
    QDateTime m_createdAt;
    QDateTime m_reviewedAt;
    QString m_reviewer;
    QString m_reviewNotes;
    int m_estimatedEffort; // in hours
    double m_confidence; // 0.0 to 1.0
    QJsonObject m_metadata;
    QStringList m_tags;
    bool m_autoApplicable;
    QString m_impact;
    QString m_risk;
};

/**
 * @brief Represents a task execution plan
 */
class ExecutionPlan {
public:
    ExecutionPlan();
    ExecutionPlan(const QString& proposalId);
    
    QString proposalId() const { return m_proposalId; }
    void setProposalId(const QString& id) { m_proposalId = id; }
    
    QStringList steps() const { return m_steps; }
    void setSteps(const QStringList& steps) { m_steps = steps; }
    void addStep(const QString& step) { m_steps.append(step); }
    
    QMap<QString, QString> stepDetails() const { return m_stepDetails; }
    void setStepDetails(const QMap<QString, QString>& details) { m_stepDetails = details; }
    void setStepDetail(const QString& step, const QString& detail) { m_stepDetails[step] = detail; }
    
    QStringList prerequisites() const { return m_prerequisites; }
    void setPrerequisites(const QStringList& prereqs) { m_prerequisites = prereqs; }
    void addPrerequisite(const QString& prereq) { m_prerequisites.append(prereq); }
    
    QStringList rollbackSteps() const { return m_rollbackSteps; }
    void setRollbackSteps(const QStringList& steps) { m_rollbackSteps = steps; }
    void addRollbackStep(const QString& step) { m_rollbackSteps.append(step); }
    
    int estimatedDuration() const { return m_estimatedDuration; }
    void setEstimatedDuration(int duration) { m_estimatedDuration = duration; }
    
    QStringList affectedResources() const { return m_affectedResources; }
    void setAffectedResources(const QStringList& resources) { m_affectedResources = resources; }
    void addAffectedResource(const QString& resource) { m_affectedResources.append(resource); }
    
    bool hasBackup() const { return m_hasBackup; }
    void setHasBackup(bool backup) { m_hasBackup = backup; }
    
    QString backupLocation() const { return m_backupLocation; }
    void setBackupLocation(const QString& location) { m_backupLocation = location; }
    
private:
    QString m_proposalId;
    QStringList m_steps;
    QMap<QString, QString> m_stepDetails;
    QStringList m_prerequisites;
    QStringList m_rollbackSteps;
    int m_estimatedDuration; // in minutes
    QStringList m_affectedResources;
    bool m_hasBackup;
    QString m_backupLocation;
};

/**
 * @brief Widget for displaying and managing AI task proposals
 */
class TaskProposalWidget : public QWidget {
    Q_OBJECT
    
public:
    /**
     * @brief Construct TaskProposalWidget
     * @param parent Parent widget
     */
    explicit TaskProposalWidget(QWidget* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~TaskProposalWidget() override;
    
    /**
     * @brief Add a new task proposal
     * @param proposal Task proposal to add
     * @return true if added successfully
     */
    bool addProposal(const TaskProposal& proposal);
    
    /**
     * @brief Update an existing task proposal
     * @param proposalId ID of proposal to update
     * @param proposal Updated proposal data
     * @return true if updated successfully
     */
    bool updateProposal(const QString& proposalId, const TaskProposal& proposal);
    
    /**
     * @brief Remove a task proposal
     * @param proposalId ID of proposal to remove
     * @return true if removed successfully
     */
    bool removeProposal(const QString& proposalId);
    
    /**
     * @brief Get proposal by ID
     * @param proposalId Proposal ID
     * @return TaskProposal object
     */
    TaskProposal getProposal(const QString& proposalId) const;
    
    /**
     * @brief Get all proposals
     * @return List of all proposals
     */
    QList<TaskProposal> getProposals() const;
    
    /**
     * @brief Get proposals by status
     * @param status Proposal status
     * @return List of proposals with the specified status
     */
    QList<TaskProposal> getProposalsByStatus(ProposalStatus status) const;
    
    /**
     * @brief Get proposals by type
     * @param type Proposal type
     * @return List of proposals of the specified type
     */
    QList<TaskProposal> getProposalsByType(ProposalType type) const;
    
    /**
     * @brief Approve a proposal
     * @param proposalId ID of proposal to approve
     * @param reviewer Reviewer name
     * @param notes Review notes
     * @return true if approved successfully
     */
    bool approveProposal(const QString& proposalId, const QString& reviewer, const QString& notes = QString());
    
    /**
     * @brief Reject a proposal
     * @param proposalId ID of proposal to reject
     * @param reviewer Reviewer name
     * @param reason Rejection reason
     * @return true if rejected successfully
     */
    bool rejectProposal(const QString& proposalId, const QString& reviewer, const QString& reason);
    
    /**
     * @brief Start execution of an approved proposal
     * @param proposalId ID of proposal to execute
     * @param plan Execution plan
     * @return true if execution started successfully
     */
    bool startExecution(const QString& proposalId, const ExecutionPlan& plan);
    
    /**
     * @brief Cancel execution of a proposal
     * @param proposalId ID of proposal to cancel
     * @param reason Cancellation reason
     * @return true if cancelled successfully
     */
    bool cancelExecution(const QString& proposalId, const QString& reason = QString());
    
    /**
     * @brief Complete execution of a proposal
     * @param proposalId ID of proposal to complete
     * @param success Whether execution was successful
     * @param results Execution results
     * @return true if completion processed successfully
     */
    bool completeExecution(const QString& proposalId, bool success, const QJsonObject& results = QJsonObject());
    
    /**
     * @brief Import proposals from JSON
     * @param jsonData JSON data containing proposals
     * @return Number of proposals imported
     */
    int importProposals(const QByteArray& jsonData);
    
    /**
     * @brief Export proposals to JSON
     * @param proposalIds IDs of proposals to export (empty for all)
     * @return JSON data containing proposals
     */
    QByteArray exportProposals(const QStringList& proposalIds = QStringList()) const;
    
    /**
     * @brief Clear all proposals
     */
    void clearProposals();
    
    /**
     * @brief Filter proposals
     * @param filter Filter criteria
     */
    void filterProposals(const QJsonObject& filter);
    
    /**
     * @brief Sort proposals
     * @param criteria Sort criteria
     * @param order Sort order
     */
    void sortProposals(const QString& criteria, Qt::SortOrder order = Qt::AscendingOrder);
    
    /**
     * @brief Refresh the display
     */
    void refresh();
    
    /**
     * @brief Set auto-apply for applicable proposals
     * @param enabled Whether to enable auto-apply
     */
    void setAutoApplyEnabled(bool enabled);
    
    /**
     * @brief Check if auto-apply is enabled
     * @return true if enabled
     */
    bool isAutoApplyEnabled() const;
    
    /**
     * @brief Set maximum proposals to display
     * @param max Maximum number of proposals
     */
    void setMaxDisplayCount(int max);
    
    /**
     * @brief Get maximum proposals to display
     * @return Maximum proposals
     */
    int maxDisplayCount() const;
    
signals:
    /**
     * @brief Emitted when proposal status changes
     * @param proposalId Proposal ID
     * @param oldStatus Previous status
     * @param newStatus New status
     */
    void proposalStatusChanged(const QString& proposalId, ProposalStatus oldStatus, ProposalStatus newStatus);
    
    /**
     * @brief Emitted when proposal is approved
     * @param proposalId Approved proposal ID
     * @param reviewer Reviewer name
     * @param notes Review notes
     */
    void proposalApproved(const QString& proposalId, const QString& reviewer, const QString& notes);
    
    /**
     * @brief Emitted when proposal is rejected
     * @param proposalId Rejected proposal ID
     * @param reviewer Reviewer name
     * @param reason Rejection reason
     */
    void proposalRejected(const QString& proposalId, const QString& reviewer, const QString& reason);
    
    /**
     * @brief Emitted when proposal execution starts
     * @param proposalId Proposal ID
     * @param plan Execution plan
     */
    void executionStarted(const QString& proposalId, const ExecutionPlan& plan);
    
    /**
     * @brief Emitted when proposal execution completes
     * @param proposalId Proposal ID
     * @param success Whether execution was successful
     * @param results Execution results
     */
    void executionCompleted(const QString& proposalId, bool success, const QJsonObject& results);
    
    /**
     * @brief Emitted when proposal is selected
     * @param proposalId Selected proposal ID
     */
    void proposalSelected(const QString& proposalId);
    
    /**
     * @brief Emitted when proposal details are requested
     * @param proposalId Proposal ID for details
     */
    void proposalDetailsRequested(const QString& proposalId);
    
    /**
     * @brief Emitted when bulk action is performed
     * @param action Action performed
     * @param proposalIds Affected proposal IDs
     */
    void bulkActionPerformed(const QString& action, const QStringList& proposalIds);
    
protected:
    /**
     * @brief Initialize the widget UI
     */
    void initializeUI();
    
    /**
     * @brief Create the main layout
     */
    void createMainLayout();
    
    /**
     * @brief Create the toolbar
     */
    void createToolbar();
    
    /**
     * @brief Create the proposal list
     */
    void createProposalList();
    
    /**
     * @brief Create the proposal details panel
     */
    void createDetailsPanel();
    
    /**
     * @brief Create the filtering controls
     */
    void createFilteringControls();
    
    /**
     * @brief Update proposal list display
     */
    void updateProposalList();
    
    /**
     * @brief Update proposal details display
     * @param proposalId ID of proposal to show details for
     */
    void updateProposalDetails(const QString& proposalId);
    
    /**
     * @brief Create context menu for proposal list
     * @param position Menu position
     */
    void createContextMenu(const QPoint& position);
    
    /**
     * @brief Handle proposal list item selection
     */
    void onProposalSelectionChanged();
    
    /**
     * @brief Handle approval button click
     */
    void onApproveClicked();
    
    /**
     * @brief Handle rejection button click
     */
    void onRejectClicked();
    
    /**
     * @brief Handle execute button click
     */
    void onExecuteClicked();
    
    /**
     * @brief Handle cancel button click
     */
    void onCancelClicked();
    
    /**
     * @brief Handle details button click
     */
    void onDetailsClicked();
    
    /**
     * @brief Handle bulk approval
     */
    void onBulkApprove();
    
    /**
     * @brief Handle bulk rejection
     */
    void onBulkReject();
    
    /**
     * @brief Handle import action
     */
    void onImport();
    
    /**
     * @brief Handle export action
     */
    void onExport();
    
    /**
     * @brief Handle refresh action
     */
    void onRefresh();
    
    /**
     * @brief Handle filter changes
     */
    void onFilterChanged();
    
    /**
     * @brief Handle sort changes
     */
    void onSortChanged();
    
    /**
     * @brief Get color for proposal type
     * @param type Proposal type
     * @return Color for the type
     */
    QColor getColorForType(ProposalType type) const;
    
    /**
     * @brief Get icon for proposal status
     * @param status Proposal status
     * @return Icon for the status
     */
    QIcon getIconForStatus(ProposalStatus status) const;
    
    /**
     * @brief Format proposal display text
     * @param proposal Proposal to format
     * @return Formatted display string
     */
    QString formatProposalDisplay(const TaskProposal& proposal) const;
    
    /**
     * @brief Validate proposal
     * @param proposal Proposal to validate
     * @return true if valid
     */
    bool validateProposal(const TaskProposal& proposal) const;
    
    /**
     * @brief Create execution plan dialog
     * @param proposal Proposal to create plan for
     * @return ExecutionPlan object
     */
    ExecutionPlan createExecutionPlan(const TaskProposal& proposal) const;
    
private:
    /**
     * @brief Populate the widget with sample data
     */
    void populateSampleData();
    
private slots:
    void onListItemDoubleClicked(QListWidgetItem* item);
    void onListItemContextMenu(const QPoint& position);
    void onToolbarActionTriggered();
    void onFilterComboChanged();
    void onSearchTextChanged();
    void onStatusFilterChanged();
    void onTypeFilterChanged();
    void onPriorityFilterChanged();
    void onAutoRefreshToggled();
    void onRefreshTimerTimeout();
    
private:
    // UI Components
    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_toolbarLayout;
    QSplitter* m_mainSplitter;
    
    // Toolbar
    QToolBar* m_toolbar;
    QToolButton* m_importButton;
    QToolButton* m_exportButton;
    QToolButton* m_refreshButton;
    QToolButton* m_approveButton;
    QToolButton* m_rejectButton;
    QToolButton* m_executeButton;
    QToolButton* m_cancelButton;
    QToolButton* m_bulkApproveButton;
    QToolButton* m_bulkRejectButton;
    QCheckBox* m_autoApplyCheckBox;
    
    // Filtering controls
    QGroupBox* m_filterGroup;
    QHBoxLayout* m_filterLayout;
    QComboBox* m_statusFilterCombo;
    QComboBox* m_typeFilterCombo;
    QComboBox* m_priorityFilterCombo;
    QLineEdit* m_searchEdit;
    QSpinBox* m_maxCountSpinBox;
    QCheckBox* m_autoRefreshCheckBox;
    
    // Proposal list
    QListWidget* m_proposalList;
    QMenu* m_contextMenu;
    QAction* m_approveAction;
    QAction* m_rejectAction;
    QAction* m_executeAction;
    QAction* m_cancelAction;
    QAction* m_detailsAction;
    QAction* m_deleteAction;
    
    // Details panel
    QWidget* m_detailsPanel;
    QVBoxLayout* m_detailsLayout;
    QLabel* m_detailsTitleLabel;
    QTextEdit* m_detailsDescriptionEdit;
    QTextEdit* m_detailsRationaleEdit;
    QLabel* m_detailsTypeLabel;
    QLabel* m_detailsPriorityLabel;
    QLabel* m_detailsCategoryLabel;
    QLabel* m_detailsStatusLabel;
    QLabel* m_detailsProposerLabel;
    QLabel* m_detailsCreatedLabel;
    QLabel* m_detailsReviewedLabel;
    QLabel* m_detailsReviewerLabel;
    QLabel* m_detailsEffortLabel;
    QLabel* m_detailsConfidenceLabel;
    QTextEdit* m_detailsAffectedFilesEdit;
    QTextEdit* m_detailsReviewNotesEdit;
    QTextEdit* m_detailsMetadataEdit;
    QListWidget* m_detailsTagsList;
    QLabel* m_detailsImpactLabel;
    QLabel* m_detailsRiskLabel;
    
    // Progress tracking
    QProgressBar* m_executionProgressBar;
    QLabel* m_executionStatusLabel;
    QTextEdit* m_executionLogEdit;
    
    // Data
    QMap<QString, TaskProposal> m_proposals;
    QMap<QString, ExecutionPlan> m_executionPlans;
    QStringList m_filteredProposalIds;
    
    // State
    QString m_selectedProposalId;
    QString m_currentSortCriteria;
    Qt::SortOrder m_currentSortOrder;
    bool m_autoApplyEnabled;
    int m_maxDisplayCount;
    bool m_autoRefresh;
    
    // Timers
    QTimer* m_refreshTimer;
    
    // Thread safety
    mutable QMutex m_mutex;
};

} // namespace RawrXD
