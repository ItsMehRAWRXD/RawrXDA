#ifndef AUTONOMOUS_DECISION_ENGINE_HPP
#define AUTONOMOUS_DECISION_ENGINE_HPP

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
 * @brief Struct representing a risk factor
 * 
 * This struct defines a risk factor that should be considered when
 * making autonomous decisions, including its category, description,
 * probability, impact, and mitigation strategies.
 */
struct RiskFactor {
    QString riskId;            ///< Unique identifier for the risk factor
    QString category;          ///< Category of risk (security, performance, etc.)
    QString description;       ///< Description of the risk
    double probability;        ///< Probability of occurrence (0.0 to 1.0)
    double impact;             ///< Impact severity (0.0 to 1.0)
    QStringList mitigationStrategies; ///< Strategies to mitigate this risk
    bool isMitigated;          ///< Whether this risk has been mitigated
};

/**
 * @brief Struct representing a decision context
 * 
 * This struct provides context for a decision, including the wish,
 * available actions, constraints, and environmental factors.
 */
struct DecisionContext {
    QString wish;              ///< The original wish
    QStringList availableActions; ///< Actions that could be taken
    QStringList constraints;   ///< Constraints on the decision
    QString projectContext;    ///< Information about the project context
    QString urgency;           ///< Urgency level (low, medium, high)
    QString complexity;        ///< Complexity level (low, medium, high)
};

/**
 * @brief Struct representing a decision outcome
 * 
 * This struct defines the outcome of a decision, including the chosen
 * action, confidence level, expected results, and risk assessment.
 */
struct DecisionOutcome {
    QString decisionId;        ///< Unique identifier for the decision
    QString chosenAction;      ///< The action that was chosen
    double confidenceLevel;    ///< Confidence in this decision (0.0 to 1.0)
    QString expectedOutcome;   ///< Expected outcome of the action
    QList<RiskFactor> risks;   ///< Risks associated with this decision
    QDateTime decidedAt;       ///< When the decision was made
    bool requiresApproval;     ///< Whether human approval is required
};

/**
 * @brief Struct representing an autonomy level
 * 
 * This struct defines different levels of autonomy, including the level
 * name, description, and what actions are allowed at this level.
 */
struct AutonomyLevel {
    QString levelName;         ///< Name of the autonomy level (e.g., "supervised", "semi-autonomous", "fully-autonomous")
    QString description;       ///< Description of this autonomy level
    QStringList allowedActions; ///< Actions allowed at this autonomy level
    double maxRiskThreshold;   ///< Maximum risk threshold allowed (0.0 to 1.0)
    bool requiresHumanOverride; ///< Whether human override is required for certain actions
};

/**
 * @brief Autonomous Decision Engine for Graduated Autonomy
 * 
 * The AutonomousDecisionEngine class provides a framework for making
 * autonomous decisions with graduated levels of autonomy, risk assessment,
 * and human oversight. It enables the agent to operate independently
 * within defined safety boundaries while escalating to human intervention
 * when necessary.
 * 
 * Key features:
 * - Risk assessment matrix
 * - Confidence thresholds for auto-execution
 * - Cost-benefit analysis
 * - Safety boundaries
 * - Graduated autonomy levels
 * - Decision logging and review
 * 
 * The class integrates with the AgentSelfReflection for confidence
 * adjustments and with the HierarchicalPlanner for action evaluation.
 */
class AutonomousDecisionEngine : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructor for AutonomousDecisionEngine
     * @param parent The parent QObject
     */
    explicit AutonomousDecisionEngine(QObject* parent = nullptr);

    /**
     * @brief Destructor for AutonomousDecisionEngine
     */
    ~AutonomousDecisionEngine();

    /**
     * @brief Make an autonomous decision based on context
     * @param context The decision context
     * @return The decision outcome
     */
    DecisionOutcome makeDecision(const DecisionContext& context);

    /**
     * @brief Assess risks associated with a potential action
     * @param action The action to assess
     * @param context The decision context
     * @return List of identified risk factors
     */
    QList<RiskFactor> assessRisks(const QString& action, const DecisionContext& context);

    /**
     * @brief Calculate confidence level for a decision
     * @param action The action being considered
     * @param context The decision context
     * @param risks Identified risks
     * @return Confidence level (0.0 to 1.0)
     */
    double calculateConfidence(const QString& action, 
                              const DecisionContext& context,
                              const QList<RiskFactor>& risks);

    /**
     * @brief Determine if human approval is required
     * @param decisionOutcome The decision outcome
     * @return true if human approval is required, false otherwise
     */
    bool requiresHumanApproval(const DecisionOutcome& decisionOutcome);

    /**
     * @brief Set the current autonomy level
     * @param level The autonomy level to set
     */
    void setAutonomyLevel(const QString& level);

    /**
     * @brief Get the current autonomy level
     * @return The current autonomy level
     */
    AutonomyLevel getAutonomyLevel() const;

    /**
     * @brief Get all available autonomy levels
     * @return List of autonomy levels
     */
    QList<AutonomyLevel> getAutonomyLevels() const;

    /**
     * @brief Evaluate the cost-benefit of an action
     * @param action The action to evaluate
     * @param context The decision context
     * @return JSON object with cost-benefit analysis
     */
    QJsonObject evaluateCostBenefit(const QString& action, const DecisionContext& context);

    /**
     * @brief Log a decision for review
     * @param decision The decision outcome to log
     */
    void logDecision(const DecisionOutcome& decision);

    /**
     * @brief Get decision history
     * @param limit Maximum number of decisions to retrieve
     * @return List of past decisions
     */
    QList<DecisionOutcome> getDecisionHistory(int limit = 50) const;

signals:
    /**
     * @brief Signal emitted when a decision is made
     * @param decision The decision outcome
     */
    void decisionMade(const DecisionOutcome& decision);

    /**
     * @brief Signal emitted when human approval is required
     * @param decision The decision requiring approval
     */
    void approvalRequired(const DecisionOutcome& decision);

    /**
     * @brief Signal emitted when a risk threshold is exceeded
     * @param risk The exceeded risk factor
     */
    void riskThresholdExceeded(const RiskFactor& risk);

    /**
     * @brief Signal emitted when autonomy level changes
     * @param level The new autonomy level
     */
    void autonomyLevelChanged(const AutonomyLevel& level);

private:
    /**
     * @brief Initialize autonomy levels
     */
    void initializeAutonomyLevels();

    /**
     * @brief Initialize common risk factors
     */
    void initializeRiskFactors();

    /**
     * @brief Get risk factors by category
     * @param category The risk category
     * @return List of risk factors in that category
     */
    QList<RiskFactor> getRiskFactorsByCategory(const QString& category) const;

    /**
     * @brief Calculate overall risk score
     * @param risks List of risk factors
     * @return Overall risk score (0.0 to 1.0)
     */
    double calculateRiskScore(const QList<RiskFactor>& risks) const;

    /**
     * @brief Generate unique ID for decisions
     * @return Unique ID string
     */
    QString generateUniqueId();

    /**
     * @brief Check if an action is allowed at the current autonomy level
     * @param action The action to check
     * @return true if allowed, false otherwise
     */
    bool isActionAllowed(const QString& action) const;

    /**
     * @brief Mitigate identified risks
     * @param risks List of risk factors
     * @return List of mitigated risk factors
     */
    QList<RiskFactor> mitigateRisks(const QList<RiskFactor>& risks);

    QList<AutonomyLevel> m_autonomyLevels;     ///< Available autonomy levels
    AutonomyLevel m_currentLevel;              ///< Current autonomy level
    QList<RiskFactor> m_riskFactors;           ///< Common risk factors
    QList<DecisionOutcome> m_decisionHistory;  ///< History of decisions
    int m_idCounter;                           ///< Counter for generating unique IDs
};

#endif // AUTONOMOUS_DECISION_ENGINE_HPP