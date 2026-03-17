/**
 * @file TaskProposalWidget.h
 * @brief Complete AI-Powered Task Proposal Widget
 * 
 * Provides an intelligent interface for:
 * - Task proposal generation from natural language descriptions
 * - AI-powered task breakdown and complexity analysis
 * - Dependency detection and resource estimation
 * - Approval workflow with modification support
 * - Proposal history and analytics
 * - Integration with planning and execution systems
 */

#pragma once

#include <QWidget>
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
#include <memory>
#include <functional>
#include <queue>

// Forward declarations
class InferenceEngine;
class AdvancedPlanningEngine;
class AgenticToolExecutor;
class UnifiedBackend;
class MetaPlanner;

namespace RawrXD {
namespace Tasks {

/**
 * @enum TaskPriority
 * @brief Priority levels for task proposals
 */
enum class TaskPriority {
    Critical = 0,   ///< Immediate attention required
    High = 1,       ///< Important, should be done soon
    Medium = 2,     ///< Normal priority
    Low = 3,        ///< Can be deferred
    Optional = 4    ///< Nice to have
};

/**
 * @enum TaskComplexity
 * @brief AI-assessed complexity levels
 */
enum class TaskComplexity {
    Trivial = 0,    ///< Simple, single-step task
    Simple = 1,     ///< Few steps, minimal dependencies
    Moderate = 2,   ///< Multiple steps, some dependencies
    Complex = 3,    ///< Many steps, multiple dependencies
    VeryComplex = 4 ///< Extensive work, needs decomposition
};

/**
 * @enum ProposalStatus
 * @brief Current status of a task proposal
 */
enum class ProposalStatus {
    Draft = 0,          ///< Being edited by user
    Analyzing = 1,      ///< AI is analyzing the proposal
    PendingReview = 2,  ///< Ready for user review
    Approved = 3,       ///< User approved, ready for execution
    Rejected = 4,       ///< User rejected
    InProgress = 5,     ///< Being executed
    Completed = 6,      ///< Successfully completed
    Failed = 7,         ///< Execution failed
    Archived = 8        ///< Moved to archive
};

/**
 * @struct SubTask
 * @brief Individual subtask within a proposal
 */
struct SubTask {
    QString id;                      ///< Unique identifier
    QString title;                   ///< Short title
    QString description;             ///< Detailed description
    QString rationale;               ///< Why this subtask is needed
    QStringList requiredFiles;       ///< Files needed for this subtask
    QStringList outputFiles;         ///< Files this subtask will create/modify
    QStringList tools;               ///< Tools/commands to use
    QStringList dependencies;        ///< IDs of subtasks this depends on
    TaskComplexity complexity;       ///< Complexity assessment
    int estimatedMinutes;            ///< Time estimate in minutes
    int actualMinutes;               ///< Actual time taken
    bool completed;                  ///< Completion status
    bool skipped;                    ///< Whether this was skipped
    QString completionNotes;         ///< Notes on completion/skip
    QJsonObject metrics;             ///< Performance metrics
    
    SubTask() 
        : id(QUuid::createUuid().toString(QUuid::WithoutBraces))
        , complexity(TaskComplexity::Moderate)
        , estimatedMinutes(15)
        , actualMinutes(0)
        , completed(false)
        , skipped(false)
    {}
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["title"] = title;
        obj["description"] = description;
        obj["rationale"] = rationale;
        obj["requiredFiles"] = QJsonArray::fromStringList(requiredFiles);
        obj["outputFiles"] = QJsonArray::fromStringList(outputFiles);
        obj["tools"] = QJsonArray::fromStringList(tools);
        obj["dependencies"] = QJsonArray::fromStringList(dependencies);
        obj["complexity"] = static_cast<int>(complexity);
        obj["estimatedMinutes"] = estimatedMinutes;
        obj["actualMinutes"] = actualMinutes;
        obj["completed"] = completed;
        obj["skipped"] = skipped;
        obj["completionNotes"] = completionNotes;
        obj["metrics"] = metrics;
        return obj;
    }
    
    static SubTask fromJson(const QJsonObject& obj) {
        SubTask st;
        st.id = obj["id"].toString();
        st.title = obj["title"].toString();
        st.description = obj["description"].toString();
        st.rationale = obj["rationale"].toString();
        for (const auto& v : obj["requiredFiles"].toArray())
            st.requiredFiles.append(v.toString());
        for (const auto& v : obj["outputFiles"].toArray())
            st.outputFiles.append(v.toString());
        for (const auto& v : obj["tools"].toArray())
            st.tools.append(v.toString());
        for (const auto& v : obj["dependencies"].toArray())
            st.dependencies.append(v.toString());
        st.complexity = static_cast<TaskComplexity>(obj["complexity"].toInt());
        st.estimatedMinutes = obj["estimatedMinutes"].toInt();
        st.actualMinutes = obj["actualMinutes"].toInt();
        st.completed = obj["completed"].toBool();
        st.skipped = obj["skipped"].toBool();
        st.completionNotes = obj["completionNotes"].toString();
        st.metrics = obj["metrics"].toObject();
        return st;
    }
};

/**
 * @struct RiskAssessment
 * @brief Risk analysis for a proposal
 */
struct RiskAssessment {
    QString id;                      ///< Risk identifier
    QString category;                ///< Category (technical, resource, scope, etc.)
    QString description;             ///< Risk description
    QString mitigation;              ///< Mitigation strategy
    int likelihood;                  ///< 1-10 scale
    int impact;                      ///< 1-10 scale
    int riskScore;                   ///< likelihood * impact
    
    RiskAssessment() : likelihood(5), impact(5), riskScore(25) {}
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["category"] = category;
        obj["description"] = description;
        obj["mitigation"] = mitigation;
        obj["likelihood"] = likelihood;
        obj["impact"] = impact;
        obj["riskScore"] = riskScore;
        return obj;
    }
    
    static RiskAssessment fromJson(const QJsonObject& obj) {
        RiskAssessment ra;
        ra.id = obj["id"].toString();
        ra.category = obj["category"].toString();
        ra.description = obj["description"].toString();
        ra.mitigation = obj["mitigation"].toString();
        ra.likelihood = obj["likelihood"].toInt();
        ra.impact = obj["impact"].toInt();
        ra.riskScore = obj["riskScore"].toInt();
        return ra;
    }
};

/**
 * @struct ResourceEstimate
 * @brief Resource requirements for a proposal
 */
struct ResourceEstimate {
    int cpuMinutes;                  ///< Estimated CPU time
    int memoryMB;                    ///< Peak memory usage estimate
    int diskMB;                      ///< Disk space needed
    int networkCalls;                ///< External API calls expected
    int tokensEstimate;              ///< AI tokens likely to be consumed
    double costEstimate;             ///< Estimated cost in units
    
    ResourceEstimate() 
        : cpuMinutes(0), memoryMB(0), diskMB(0)
        , networkCalls(0), tokensEstimate(0), costEstimate(0.0)
    {}
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["cpuMinutes"] = cpuMinutes;
        obj["memoryMB"] = memoryMB;
        obj["diskMB"] = diskMB;
        obj["networkCalls"] = networkCalls;
        obj["tokensEstimate"] = tokensEstimate;
        obj["costEstimate"] = costEstimate;
        return obj;
    }
    
    static ResourceEstimate fromJson(const QJsonObject& obj) {
        ResourceEstimate re;
        re.cpuMinutes = obj["cpuMinutes"].toInt();
        re.memoryMB = obj["memoryMB"].toInt();
        re.diskMB = obj["diskMB"].toInt();
        re.networkCalls = obj["networkCalls"].toInt();
        re.tokensEstimate = obj["tokensEstimate"].toInt();
        re.costEstimate = obj["costEstimate"].toDouble();
        return re;
    }
};

/**
 * @struct TaskProposal
 * @brief Complete task proposal with AI analysis
 */
struct TaskProposal {
    // Identification
    QString id;                              ///< Unique proposal ID
    QString title;                           ///< Short title
    QString originalDescription;             ///< User's original input
    QString refinedDescription;              ///< AI-refined description
    
    // Classification
    TaskPriority priority;                   ///< Priority level
    TaskComplexity complexity;               ///< Overall complexity
    ProposalStatus status;                   ///< Current status
    QString category;                        ///< Task category
    QStringList tags;                        ///< User/AI assigned tags
    
    // Breakdown
    QVector<SubTask> subtasks;               ///< Decomposed subtasks
    QString executionStrategy;               ///< Recommended execution approach
    QStringList assumptions;                 ///< Assumptions made
    QStringList prerequisites;               ///< Prerequisites that must be met
    
    // Analysis
    QVector<RiskAssessment> risks;           ///< Identified risks
    ResourceEstimate resources;              ///< Resource estimates
    double confidenceScore;                  ///< AI confidence (0-100)
    QString analysisNotes;                   ///< AI analysis notes
    
    // Timing
    int estimatedTotalMinutes;               ///< Total time estimate
    int actualTotalMinutes;                  ///< Actual time taken
    QDateTime createdAt;                     ///< Creation timestamp
    QDateTime analyzedAt;                    ///< Analysis completion time
    QDateTime approvedAt;                    ///< Approval timestamp
    QDateTime completedAt;                   ///< Completion timestamp
    
    // User interaction
    QString userFeedback;                    ///< User's feedback
    QString rejectionReason;                 ///< Reason if rejected
    QStringList modifications;               ///< User modifications history
    
    // Metrics
    int aiRequestCount;                      ///< Number of AI requests made
    int tokensUsed;                          ///< Total tokens consumed
    double processingTimeMs;                 ///< Processing time
    QJsonObject executionMetrics;            ///< Detailed execution metrics
    
    TaskProposal()
        : id(QUuid::createUuid().toString(QUuid::WithoutBraces))
        , priority(TaskPriority::Medium)
        , complexity(TaskComplexity::Moderate)
        , status(ProposalStatus::Draft)
        , confidenceScore(0.0)
        , estimatedTotalMinutes(0)
        , actualTotalMinutes(0)
        , createdAt(QDateTime::currentDateTime())
        , aiRequestCount(0)
        , tokensUsed(0)
        , processingTimeMs(0.0)
    {}
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["title"] = title;
        obj["originalDescription"] = originalDescription;
        obj["refinedDescription"] = refinedDescription;
        obj["priority"] = static_cast<int>(priority);
        obj["complexity"] = static_cast<int>(complexity);
        obj["status"] = static_cast<int>(status);
        obj["category"] = category;
        obj["tags"] = QJsonArray::fromStringList(tags);
        
        QJsonArray subtasksArr;
        for (const auto& st : subtasks)
            subtasksArr.append(st.toJson());
        obj["subtasks"] = subtasksArr;
        
        obj["executionStrategy"] = executionStrategy;
        obj["assumptions"] = QJsonArray::fromStringList(assumptions);
        obj["prerequisites"] = QJsonArray::fromStringList(prerequisites);
        
        QJsonArray risksArr;
        for (const auto& r : risks)
            risksArr.append(r.toJson());
        obj["risks"] = risksArr;
        
        obj["resources"] = resources.toJson();
        obj["confidenceScore"] = confidenceScore;
        obj["analysisNotes"] = analysisNotes;
        obj["estimatedTotalMinutes"] = estimatedTotalMinutes;
        obj["actualTotalMinutes"] = actualTotalMinutes;
        obj["createdAt"] = createdAt.toString(Qt::ISODate);
        obj["analyzedAt"] = analyzedAt.toString(Qt::ISODate);
        obj["approvedAt"] = approvedAt.toString(Qt::ISODate);
        obj["completedAt"] = completedAt.toString(Qt::ISODate);
        obj["userFeedback"] = userFeedback;
        obj["rejectionReason"] = rejectionReason;
        obj["modifications"] = QJsonArray::fromStringList(modifications);
        obj["aiRequestCount"] = aiRequestCount;
        obj["tokensUsed"] = tokensUsed;
        obj["processingTimeMs"] = processingTimeMs;
        obj["executionMetrics"] = executionMetrics;
        
        return obj;
    }
    
    static TaskProposal fromJson(const QJsonObject& obj) {
        TaskProposal tp;
        tp.id = obj["id"].toString();
        tp.title = obj["title"].toString();
        tp.originalDescription = obj["originalDescription"].toString();
        tp.refinedDescription = obj["refinedDescription"].toString();
        tp.priority = static_cast<TaskPriority>(obj["priority"].toInt());
        tp.complexity = static_cast<TaskComplexity>(obj["complexity"].toInt());
        tp.status = static_cast<ProposalStatus>(obj["status"].toInt());
        tp.category = obj["category"].toString();
        for (const auto& v : obj["tags"].toArray())
            tp.tags.append(v.toString());
        
        for (const auto& v : obj["subtasks"].toArray())
            tp.subtasks.append(SubTask::fromJson(v.toObject()));
        
        tp.executionStrategy = obj["executionStrategy"].toString();
        for (const auto& v : obj["assumptions"].toArray())
            tp.assumptions.append(v.toString());
        for (const auto& v : obj["prerequisites"].toArray())
            tp.prerequisites.append(v.toString());
        
        for (const auto& v : obj["risks"].toArray())
            tp.risks.append(RiskAssessment::fromJson(v.toObject()));
        
        tp.resources = ResourceEstimate::fromJson(obj["resources"].toObject());
        tp.confidenceScore = obj["confidenceScore"].toDouble();
        tp.analysisNotes = obj["analysisNotes"].toString();
        tp.estimatedTotalMinutes = obj["estimatedTotalMinutes"].toInt();
        tp.actualTotalMinutes = obj["actualTotalMinutes"].toInt();
        tp.createdAt = QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
        tp.analyzedAt = QDateTime::fromString(obj["analyzedAt"].toString(), Qt::ISODate);
        tp.approvedAt = QDateTime::fromString(obj["approvedAt"].toString(), Qt::ISODate);
        tp.completedAt = QDateTime::fromString(obj["completedAt"].toString(), Qt::ISODate);
        tp.userFeedback = obj["userFeedback"].toString();
        tp.rejectionReason = obj["rejectionReason"].toString();
        for (const auto& v : obj["modifications"].toArray())
            tp.modifications.append(v.toString());
        tp.aiRequestCount = obj["aiRequestCount"].toInt();
        tp.tokensUsed = obj["tokensUsed"].toInt();
        tp.processingTimeMs = obj["processingTimeMs"].toDouble();
        tp.executionMetrics = obj["executionMetrics"].toObject();
        
        return tp;
    }
    
    int completedSubtaskCount() const {
        int count = 0;
        for (const auto& st : subtasks) {
            if (st.completed || st.skipped)
                ++count;
        }
        return count;
    }
    
    double progressPercent() const {
        if (subtasks.isEmpty()) return 0.0;
        return (static_cast<double>(completedSubtaskCount()) / subtasks.size()) * 100.0;
    }
    
    int overallRiskScore() const {
        if (risks.isEmpty()) return 0;
        int total = 0;
        for (const auto& r : risks)
            total += r.riskScore;
        return total / risks.size();
    }
};

/**
 * @struct ProposalAnalyticsData
 * @brief Analytics and metrics for proposals
 */
struct ProposalAnalyticsData {
    int totalProposals;
    int approvedProposals;
    int rejectedProposals;
    int completedProposals;
    int failedProposals;
    double averageConfidence;
    double averageAccuracy;    // Estimated vs actual time
    int totalTokensUsed;
    double totalProcessingTime;
    QMap<QString, int> categoryDistribution;
    QMap<int, int> complexityDistribution;
    QMap<int, int> priorityDistribution;
    
    ProposalAnalyticsData()
        : totalProposals(0), approvedProposals(0), rejectedProposals(0)
        , completedProposals(0), failedProposals(0), averageConfidence(0.0)
        , averageAccuracy(0.0), totalTokensUsed(0), totalProcessingTime(0.0)
    {}
};

} // namespace Tasks
} // namespace RawrXD

/**
 * @class TaskProposalWidget
 * @brief Complete AI-Powered Task Proposal Widget
 * 
 * This widget provides a full-featured interface for creating, analyzing,
 * and managing AI-powered task proposals. It integrates with the inference
 * engine for intelligent task decomposition and the planning system for
 * execution workflow generation.
 */
class TaskProposalWidget : public QWidget {
    Q_OBJECT

public:
    explicit TaskProposalWidget(QWidget* parent = nullptr);
    ~TaskProposalWidget() override;

    // ========== Configuration ==========
    
    /**
     * @brief Set the inference engine for AI-powered analysis
     */
    void setInferenceEngine(InferenceEngine* engine);
    
    /**
     * @brief Set the planning engine for plan generation
     */
    void setPlanningEngine(AdvancedPlanningEngine* engine);
    
    /**
     * @brief Set the tool executor for validation
     */
    void setToolExecutor(AgenticToolExecutor* executor);
    
    /**
     * @brief Set the unified backend for API access
     */
    void setUnifiedBackend(UnifiedBackend* backend);
    
    /**
     * @brief Configure AI model to use for analysis
     */
    void setAnalysisModel(const QString& modelName);
    
    /**
     * @brief Configure API endpoint for cloud analysis
     */
    void setCloudEndpoint(const QString& endpoint, const QString& apiKey);
    
    /**
     * @brief Enable/disable cloud fallback
     */
    void setCloudFallbackEnabled(bool enabled);
    
    /**
     * @brief Set workspace context for file-aware analysis
     */
    void setWorkspaceContext(const QString& workspacePath, const QStringList& files);
    
    // ========== Proposal Management ==========
    
    /**
     * @brief Create a new proposal from description
     */
    void createProposal(const QString& description);
    
    /**
     * @brief Load an existing proposal for review
     */
    void loadProposal(const RawrXD::Tasks::TaskProposal& proposal);
    
    /**
     * @brief Get the current proposal being edited
     */
    const RawrXD::Tasks::TaskProposal& currentProposal() const;
    
    /**
     * @brief Get all proposals in history
     */
    QVector<RawrXD::Tasks::TaskProposal> proposalHistory() const;
    
    /**
     * @brief Clear the current proposal
     */
    void clearCurrentProposal();
    
    /**
     * @brief Archive a proposal
     */
    void archiveProposal(const QString& proposalId);
    
    // ========== Analysis ==========
    
    /**
     * @brief Trigger AI analysis of current proposal
     */
    void analyzeCurrentProposal();
    
    /**
     * @brief Re-analyze with different parameters
     */
    void reanalyzeProposal(const QJsonObject& parameters);
    
    /**
     * @brief Get complexity breakdown
     */
    QJsonObject getComplexityBreakdown() const;
    
    /**
     * @brief Get resource estimates
     */
    QJsonObject getResourceEstimates() const;
    
    // ========== Workflow ==========
    
    /**
     * @brief Approve the current proposal
     */
    void approveProposal(const QString& notes = QString());
    
    /**
     * @brief Reject the current proposal
     */
    void rejectProposal(const QString& reason);
    
    /**
     * @brief Request modifications to the proposal
     */
    void requestModifications(const QStringList& modifications);
    
    /**
     * @brief Convert approved proposal to executable plan
     */
    void convertToPlan();
    
    // ========== Analytics ==========
    
    /**
     * @brief Get analytics data
     */
    RawrXD::Tasks::ProposalAnalyticsData getAnalytics() const;
    
    /**
     * @brief Export analytics report
     */
    QJsonObject exportAnalyticsReport() const;
    
    // ========== State Queries ==========
    
    bool isAnalyzing() const { return m_isAnalyzing; }
    bool hasUnsavedChanges() const { return m_hasUnsavedChanges; }

signals:
    // Proposal lifecycle
    void proposalCreated(const QString& proposalId);
    void proposalAnalysisStarted(const QString& proposalId);
    void proposalAnalysisProgress(const QString& proposalId, int percent, const QString& status);
    void proposalAnalysisCompleted(const QString& proposalId, const RawrXD::Tasks::TaskProposal& proposal);
    void proposalAnalysisFailed(const QString& proposalId, const QString& error);
    
    // User actions
    void proposalApproved(const QString& proposalId);
    void proposalRejected(const QString& proposalId, const QString& reason);
    void proposalModified(const QString& proposalId);
    void proposalArchived(const QString& proposalId);
    
    // Conversion
    void planGenerationRequested(const RawrXD::Tasks::TaskProposal& proposal);
    void planGenerated(const QString& proposalId, const QJsonObject& plan);
    
    // Subtask events
    void subtaskSelected(const RawrXD::Tasks::SubTask& subtask);
    void subtaskModified(const QString& subtaskId);
    void subtaskCompleted(const QString& subtaskId);
    
    // Analytics
    void analyticsUpdated();
    
    // Error handling
    void errorOccurred(const QString& error);
    void warningOccurred(const QString& warning);

public slots:
    // UI actions
    void onNewProposalClicked();
    void onAnalyzeClicked();
    void onApproveClicked();
    void onRejectClicked();
    void onConvertToPlanClicked();
    void onExportClicked();
    void onSaveClicked();
    void onClearClicked();
    
    // Subtask management
    void onSubtaskSelected(int index);
    void onSubtaskEdited(int index);
    void onSubtaskMoved(int from, int to);
    void onSubtaskDeleted(int index);
    void onAddSubtask();
    
    // History management
    void onHistoryItemSelected(int index);
    void onHistoryItemDeleted(int index);
    void onClearHistory();
    
    // Settings
    void onSettingsClicked();
    void onRefreshAnalysis();

private slots:
    // AI response handlers
    void onAnalysisStreamToken(qint64 requestId, const QString& token);
    void onAnalysisStreamFinished(qint64 requestId);
    void onAnalysisError(qint64 requestId, const QString& error);
    
    // Network handlers
    void onNetworkReplyFinished(QNetworkReply* reply);
    
    // Timer handlers
    void onAnalysisTimeout();
    void onAutoSaveTimer();
    
    // Internal updates
    void updateUIFromProposal();
    void updateProposalFromUI();
    void validateCurrentProposal();

private:
    // ========== UI Setup ==========
    void setupUI();
    void setupInputPanel();
    void setupAnalysisPanel();
    void setupSubtaskPanel();
    void setupRiskPanel();
    void setupResourcePanel();
    void setupHistoryPanel();
    void setupAnalyticsPanel();
    void setupToolbar();
    void setupContextMenus();
    void applyStylesheet();
    void connectSignals();
    
    // ========== Analysis Methods ==========
    void startLocalAnalysis();
    void startCloudAnalysis();
    void processAnalysisResponse(const QString& response);
    void parseSubtasksFromResponse(const QString& response);
    void parseRisksFromResponse(const QString& response);
    void parseResourcesFromResponse(const QString& response);
    void calculateComplexity();
    void estimateTime();
    void detectDependencies();
    void generateExecutionStrategy();
    double calculateConfidenceScore();
    
    // ========== UI Helpers ==========
    void populateSubtaskList();
    void populateRiskTable();
    void populateResourcePanel();
    void populateHistoryList();
    void updateProgressIndicators();
    void updateStatusDisplay();
    void showAnalysisProgress(int percent, const QString& status);
    void hideAnalysisProgress();
    void setUIEnabled(bool enabled);
    QString priorityToString(RawrXD::Tasks::TaskPriority priority) const;
    QString complexityToString(RawrXD::Tasks::TaskComplexity complexity) const;
    QString statusToString(RawrXD::Tasks::ProposalStatus status) const;
    QColor priorityColor(RawrXD::Tasks::TaskPriority priority) const;
    QColor complexityColor(RawrXD::Tasks::TaskComplexity complexity) const;
    QColor statusColor(RawrXD::Tasks::ProposalStatus status) const;
    
    // ========== Persistence ==========
    void loadHistory();
    void saveHistory();
    void loadSettings();
    void saveSettings();
    void autoSave();
    
    // ========== Validation ==========
    bool validateDescription(const QString& description);
    bool validateSubtasks();
    bool validateRisks();
    QStringList checkPrerequisites();
    
    // ========== Analytics ==========
    void updateAnalytics();
    void recordAnalysisMetrics(qint64 requestId, double processingTime, int tokens);
    
    // ========== Logging ==========
    void logInfo(const QString& event, const QString& message, const QJsonObject& data = QJsonObject());
    void logWarn(const QString& event, const QString& message, const QJsonObject& data = QJsonObject());
    void logError(const QString& event, const QString& message, const QJsonObject& data = QJsonObject());

    // ========== Member Variables ==========
    
    // Current state
    RawrXD::Tasks::TaskProposal m_currentProposal;
    QVector<RawrXD::Tasks::TaskProposal> m_proposalHistory;
    RawrXD::Tasks::ProposalAnalyticsData m_analytics;
    
    // External components
    InferenceEngine* m_inferenceEngine;
    AdvancedPlanningEngine* m_planningEngine;
    AgenticToolExecutor* m_toolExecutor;
    UnifiedBackend* m_unifiedBackend;
    QNetworkAccessManager* m_networkManager;
    
    // Configuration
    QString m_analysisModel;
    QString m_cloudEndpoint;
    QString m_cloudApiKey;
    QString m_workspacePath;
    QStringList m_workspaceFiles;
    bool m_cloudFallbackEnabled;
    int m_analysisTimeoutMs;
    int m_maxSubtasks;
    int m_maxRisks;
    
    // UI - Main Layout
    QVBoxLayout* m_mainLayout;
    QSplitter* m_mainSplitter;
    QTabWidget* m_tabWidget;
    
    // UI - Toolbar
    QWidget* m_toolbarWidget;
    QHBoxLayout* m_toolbarLayout;
    QPushButton* m_newButton;
    QPushButton* m_analyzeButton;
    QPushButton* m_approveButton;
    QPushButton* m_rejectButton;
    QPushButton* m_convertButton;
    QPushButton* m_exportButton;
    QPushButton* m_saveButton;
    QPushButton* m_settingsButton;
    
    // UI - Input Panel
    QWidget* m_inputPanel;
    QVBoxLayout* m_inputLayout;
    QLabel* m_titleLabel;
    QLineEdit* m_titleEdit;
    QLabel* m_descriptionLabel;
    QTextEdit* m_descriptionEdit;
    QLabel* m_priorityLabel;
    QComboBox* m_priorityCombo;
    QLabel* m_categoryLabel;
    QLineEdit* m_categoryEdit;
    QLabel* m_tagsLabel;
    QLineEdit* m_tagsEdit;
    
    // UI - Analysis Panel
    QWidget* m_analysisPanel;
    QVBoxLayout* m_analysisLayout;
    QGroupBox* m_complexityGroup;
    QLabel* m_complexityLabel;
    QLabel* m_complexityValueLabel;
    QSlider* m_complexitySlider;
    QGroupBox* m_confidenceGroup;
    QLabel* m_confidenceLabel;
    QProgressBar* m_confidenceBar;
    QGroupBox* m_estimateGroup;
    QLabel* m_timeEstimateLabel;
    QLabel* m_timeEstimateValue;
    QTextEdit* m_analysisNotesEdit;
    QTextEdit* m_refinedDescriptionEdit;
    QListWidget* m_assumptionsList;
    QListWidget* m_prerequisitesList;
    
    // UI - Subtask Panel
    QWidget* m_subtaskPanel;
    QVBoxLayout* m_subtaskLayout;
    QTreeWidget* m_subtaskTree;
    QWidget* m_subtaskDetailPanel;
    QLineEdit* m_subtaskTitleEdit;
    QTextEdit* m_subtaskDescriptionEdit;
    QListWidget* m_subtaskFilesList;
    QListWidget* m_subtaskToolsList;
    QListWidget* m_subtaskDependenciesList;
    QSpinBox* m_subtaskTimeSpinBox;
    QPushButton* m_addSubtaskButton;
    QPushButton* m_removeSubtaskButton;
    QPushButton* m_moveUpButton;
    QPushButton* m_moveDownButton;
    
    // UI - Risk Panel
    QWidget* m_riskPanel;
    QVBoxLayout* m_riskLayout;
    QTableWidget* m_riskTable;
    QLabel* m_overallRiskLabel;
    QProgressBar* m_overallRiskBar;
    QPushButton* m_addRiskButton;
    QPushButton* m_removeRiskButton;
    
    // UI - Resource Panel
    QWidget* m_resourcePanel;
    QGridLayout* m_resourceLayout;
    QLabel* m_cpuLabel;
    QLabel* m_cpuValue;
    QLabel* m_memoryLabel;
    QLabel* m_memoryValue;
    QLabel* m_diskLabel;
    QLabel* m_diskValue;
    QLabel* m_networkLabel;
    QLabel* m_networkValue;
    QLabel* m_tokensLabel;
    QLabel* m_tokensValue;
    QLabel* m_costLabel;
    QLabel* m_costValue;
    
    // UI - History Panel
    QWidget* m_historyPanel;
    QVBoxLayout* m_historyLayout;
    QListWidget* m_historyList;
    QPushButton* m_clearHistoryButton;
    QLineEdit* m_historySearchEdit;
    QComboBox* m_historyFilterCombo;
    
    // UI - Analytics Panel
    QWidget* m_analyticsPanel;
    QVBoxLayout* m_analyticsLayout;
    QLabel* m_totalProposalsLabel;
    QLabel* m_approvalRateLabel;
    QLabel* m_avgConfidenceLabel;
    QLabel* m_avgAccuracyLabel;
    QLabel* m_totalTokensLabel;
    QTableWidget* m_categoryStatsTable;
    
    // UI - Progress
    QWidget* m_progressWidget;
    QProgressBar* m_progressBar;
    QLabel* m_progressLabel;
    QPushButton* m_cancelButton;
    
    // UI - Status
    QLabel* m_statusLabel;
    
    // Timers
    QTimer* m_analysisTimeoutTimer;
    QTimer* m_autoSaveTimer;
    QElapsedTimer* m_analysisTimer;
    
    // State
    bool m_isAnalyzing;
    bool m_hasUnsavedChanges;
    qint64 m_currentRequestId;
    QString m_accumulatedResponse;
    int m_selectedSubtaskIndex;
    
    // Thread safety
    mutable QMutex m_dataMutex;
    
    // Context menus
    QMenu* m_subtaskContextMenu;
    QMenu* m_riskContextMenu;
    QMenu* m_historyContextMenu;
};

#endif // TASKPROPOSALWIDGET_H
