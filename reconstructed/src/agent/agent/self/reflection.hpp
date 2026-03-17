#ifndef AGENT_SELF_REFLECTION_HPP
#define AGENT_SELF_REFLECTION_HPP

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <memory>

/**
 * @brief Struct representing an error analysis
 * 
 * This struct contains detailed analysis of why an action failed,
 * including the error type, root cause, impact assessment, and
 * recommended corrective actions.
 */
struct ErrorAnalysis {
    QString errorId;           ///< Unique identifier for the error analysis
    QString actionType;        ///< Type of action that failed
    QString errorMessage;      ///< The error message
    QString rootCause;         ///< Identified root cause of the failure
    QString impact;            ///< Impact of the failure (low, medium, high)
    QStringList recommendations; ///< Recommended corrective actions
    QDateTime analyzedAt;      ///< When the analysis was performed
    bool requiresHumanInput;   ///< Whether human input is needed to resolve
};

/**
 * @brief Struct representing an alternative approach
 * 
 * This struct defines an alternative approach to accomplish a task
 * when the original approach failed, including the approach description,
 * confidence level, and expected success probability.
 */
struct AlternativeApproach {
    QString approachId;        ///< Unique identifier for the approach
    QString description;       ///< Description of the alternative approach
    double confidenceLevel;    ///< Confidence in this approach (0.0 to 1.0)
    double expectedSuccess;    ///< Expected success probability (0.0 to 1.0)
    QStringList requiredSteps; ///< Steps needed to implement this approach
    QString estimatedTime;     ///< Estimated time to implement
};

/**
 * @brief Struct representing a confidence adjustment
 * 
 * This struct defines how the agent's confidence in its abilities
 * should be adjusted based on recent performance.
 */
struct ConfidenceAdjustment {
    QString component;         ///< Component affected (planning, execution, etc.)
    double adjustment;         ///< Adjustment to confidence (-1.0 to 1.0)
    QString reason;            ///< Reason for the adjustment
    QDateTime adjustedAt;      ///< When the adjustment was made
};

/**
 * @brief Agent Self-Reflection System for Meta-Cognition and Error Recovery
 * 
 * The AgentSelfReflection class provides meta-cognitive capabilities for the
 * autonomous agent, enabling it to analyze failures, generate alternative
 * approaches, adjust its confidence levels, and determine when to escalate
 * to human intervention.
 * 
 * Key features:
 * - Error analysis and root cause identification
 * - Alternative approach generation
 * - Confidence level adjustment based on performance
 * - Escalation decision making
 * - Learning from execution patterns
 * - Self-improvement mechanisms
 * 
 * The class integrates with the ActionExecutor to analyze failures and with
 * the HierarchicalPlanner to generate alternative approaches.
 */
class AgentSelfReflection : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructor for AgentSelfReflection
     * @param parent The parent QObject
     */
    explicit AgentSelfReflection(QObject* parent = nullptr);

    /**
     * @brief Destructor for AgentSelfReflection
     */
    ~AgentSelfReflection();

    /**
     * @brief Analyze an error and determine root cause
     * @param actionType Type of action that failed
     * @param errorMessage The error message
     * @param context Additional context about the failure
     * @return Detailed error analysis
     */
    ErrorAnalysis analyzeError(const QString& actionType, 
                              const QString& errorMessage,
                              const QString& context);

    /**
     * @brief Generate alternative approaches for a failed action
     * @param actionType Type of action that failed
     * @param errorAnalysis The error analysis
     * @param context Additional context
     * @return List of alternative approaches
     */
    QList<AlternativeApproach> generateAlternatives(const QString& actionType,
                                                   const ErrorAnalysis& errorAnalysis,
                                                   const QString& context);

    /**
     * @brief Adjust confidence levels based on recent performance
     * @param success Whether the recent action was successful
     * @param actionType Type of action performed
     * @param executionTime Time taken for execution
     * @return Confidence adjustment recommendation
     */
    ConfidenceAdjustment adjustConfidence(bool success, 
                                         const QString& actionType,
                                         int executionTime);

    /**
     * @brief Determine if human intervention is needed
     * @param errorAnalysis The error analysis
     * @param alternatives Available alternative approaches
     * @return true if human intervention is recommended, false otherwise
     */
    bool shouldEscalateToHuman(const ErrorAnalysis& errorAnalysis,
                              const QList<AlternativeApproach>& alternatives);

    /**
     * @brief Learn from execution patterns
     * @param actionType Type of action performed
     * @param success Whether the action was successful
     * @param executionTime Time taken for execution
     * @param errorMessage Error message if failed
     */
    void learnFromExecution(const QString& actionType, 
                           bool success, 
                           int executionTime,
                           const QString& errorMessage = "");

    /**
     * @brief Get confidence level for a specific action type
     * @param actionType Type of action
     * @return Confidence level (0.0 to 1.0)
     */
    double getActionConfidence(const QString& actionType) const;

    /**
     * @brief Get overall agent confidence
     * @return Overall confidence level (0.0 to 1.0)
     */
    double getOverallConfidence() const;

    /**
     * @brief Reset confidence levels
     */
    void resetConfidence();

signals:
    /**
     * @brief Signal emitted when an error is analyzed
     * @param analysis The error analysis
     */
    void errorAnalyzed(const ErrorAnalysis& analysis);

    /**
     * @brief Signal emitted when alternatives are generated
     * @param alternatives The alternative approaches
     */
    void alternativesGenerated(const QList<AlternativeApproach>& alternatives);

    /**
     * @brief Signal emitted when confidence is adjusted
     * @param adjustment The confidence adjustment
     */
    void confidenceAdjusted(const ConfidenceAdjustment& adjustment);

    /**
     * @brief Signal emitted when escalation is recommended
     * @param analysis The error analysis that triggered escalation
     */
    void escalationRecommended(const ErrorAnalysis& analysis);

private:
    /**
     * @brief Initialize default confidence levels
     */
    void initializeConfidenceLevels();

    /**
     * @brief Identify root cause based on error message
     * @param errorMessage The error message
     * @return Identified root cause
     */
    QString identifyRootCause(const QString& errorMessage) const;

    /**
     * @brief Generate recommendations based on root cause
     * @param rootCause The identified root cause
     * @return List of recommendations
     */
    QStringList generateRecommendations(const QString& rootCause) const;

    /**
     * @brief Calculate impact level based on error
     * @param errorMessage The error message
     * @return Impact level (low, medium, high)
     */
    QString calculateImpact(const QString& errorMessage) const;

    /**
     * @brief Determine if human input is required
     * @param rootCause The identified root cause
     * @param impact The impact level
     * @return true if human input is required, false otherwise
     */
    bool requiresHumanInput(const QString& rootCause, const QString& impact) const;

    /**
     * @brief Generate a unique ID for error analysis
     * @return Unique ID string
     */
    QString generateUniqueId();

    /**
     * @brief Update confidence level for a component
     * @param component The component to update
     * @param adjustment The adjustment to apply
     */
    void updateConfidence(const QString& component, double adjustment);

    QMap<QString, double> m_confidenceLevels;  ///< Confidence levels for different components
    QMap<QString, int> m_executionCounts;      ///< Execution counts for different action types
    QMap<QString, int> m_successCounts;        ///< Success counts for different action types
    int m_idCounter;                           ///< Counter for generating unique IDs
};

#endif // AGENT_SELF_REFLECTION_HPP