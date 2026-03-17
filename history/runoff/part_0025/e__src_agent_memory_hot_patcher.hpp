#ifndef MEMORY_HOT_PATCHER_HPP
#define MEMORY_HOT_PATCHER_HPP

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutex>
#include <memory>
#include <functional>

/**
 * @brief Struct representing a hallucination pattern
 * 
 * Identifies recurring hallucinations that can be automatically corrected
 */
struct HallucinationPattern {
    QString patternId;         ///< Unique identifier
    QString triggerKeywords;   ///< Keywords that trigger the hallucination
    QString hallucينatedOutput; ///< Expected hallucinated response
    QString correctOutput;     ///< Corrected response to use instead
    double confidence;         ///< Confidence of this pattern (0.0-1.0)
    int occurrenceCount;       ///< Number of times this pattern was detected
    bool enabled;              ///< Whether this pattern is active
};

/**
 * @brief Struct representing a navigation correction
 * 
 * Stores navigation path corrections for the agent
 */
struct NavigationCorrection {
    QString correctionId;      ///< Unique identifier
    QString attemptedPath;     ///< Path the agent tried
    QString correctPath;       ///< Correct path to use
    QString failureReason;     ///< Why the attempted path failed
    int successfulCorrections; ///< Count of successful corrections
    QDateTime lastApplied;     ///< When this correction was last applied
};

/**
 * @brief Struct representing a memory intercept rule
 * 
 * Rule for intercepting and modifying agent memory/output before execution
 */
struct MemoryInterceptRule {
    QString ruleId;            ///< Unique identifier
    QString matchPattern;      ///< Regex pattern to match
    QString replacementPattern; ///< Regex replacement pattern
    QString description;       ///< Description of what this rule fixes
    int priority;              ///< Priority level (higher = applied first)
    bool enabled;              ///< Whether this rule is active
    QDateTime createdAt;       ///< When the rule was created
    int timesApplied;          ///< Count of times applied
};

/**
 * @brief Struct representing corrected execution context
 * 
 * Tracks corrections made to an execution for auditing
 */
struct CorrectedExecutionContext {
    QString executionId;       ///< Unique execution identifier
    QString originalWish;      ///< Original wish before corrections
    QString correctedWish;     ///< Wish after corrections
    QList<QString> appliedRules; ///< Rules that were applied
    QList<QString> correctedPaths; ///< Navigation paths that were corrected
    QList<QString> fixedHallucinations; ///< Hallucinations that were fixed
    int totalCorrections;      ///< Total number of corrections made
    double correctionConfidence; ///< Overall confidence of corrections
};

/**
 * @brief Memory Hot Patcher for Agent Hallucination and Navigation Correction
 * 
 * The MemoryHotPatcher intercepts agent memory and output, detecting and correcting:
 * - Hallucinations (incorrect outputs based on learned patterns)
 * - Navigation failures (incorrect path following)
 * - Logic errors (faulty reasoning patterns)
 * - State inconsistencies (contradictory memory updates)
 * 
 * Acts as a transparent proxy that improves agent reliability without requiring
 * retraining or model modifications.
 * 
 * Key features:
 * - Real-time hallucination detection and correction
 * - Navigation path validation and correction
 * - Memory intercept rules for pattern-based fixes
 * - Proxy server integration for GGUF model loader interception
 * - Learning-based pattern accumulation
 * - Audit trail of all corrections
 * - Confidence scoring for corrections
 */
class MemoryHotPatcher : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructor for MemoryHotPatcher
     * @param parent The parent QObject
     */
    explicit MemoryHotPatcher(QObject* parent = nullptr);

    /**
     * @brief Destructor for MemoryHotPatcher
     */
    ~MemoryHotPatcher();

    /**
     * @brief Initialize the hot patcher with storage path
     * @param storagePath Path where correction rules are stored
     * @return true if initialization succeeded
     */
    bool initialize(const QString& storagePath);

    /**
     * @brief Intercept and correct agent output
     * @param output The raw agent output to potentially correct
     * @param context Additional context for correction
     * @return Corrected output
     */
    QString interceptAndCorrectOutput(const QString& output, const QString& context);

    /**
     * @brief Validate and correct navigation path
     * @param attemptedPath The path the agent wants to follow
     * @param projectContext Current project context
     * @return Corrected path if issues found, otherwise original path
     */
    QString validateAndCorrectPath(const QString& attemptedPath, const QString& projectContext);

    /**
     * @brief Process JSON plan and correct hallucinations
     * @param plan The JSON plan from the LLM
     * @return Corrected JSON plan
     */
    QJsonObject correctPlanHallucinations(const QJsonObject& plan);

    /**
     * @brief Register a detected hallucination pattern
     * @param keywords Keywords that trigger the hallucination
     * @param hallucinated The hallucinated output
     * @param correct The correct output
     * @param confidence Confidence level of this pattern
     */
    void registerHallucinationPattern(const QString& keywords, 
                                     const QString& hallucinated,
                                     const QString& correct,
                                     double confidence);

    /**
     * @brief Register a navigation correction
     * @param attemptedPath The path that failed
     * @param correctPath The correct path
     * @param failureReason Why the original path failed
     */
    void registerNavigationCorrection(const QString& attemptedPath,
                                     const QString& correctPath,
                                     const QString& failureReason);

    /**
     * @brief Add a memory intercept rule
     * @param matchPattern Regex pattern to match
     * @param replacement Replacement text or pattern
     * @param description What this rule fixes
     * @param priority Priority for rule application
     */
    void addInterceptRule(const QString& matchPattern,
                         const QString& replacement,
                         const QString& description,
                         int priority = 5);

    /**
     * @brief Enable/disable a hallucination pattern
     * @param patternId ID of the pattern
     * @param enabled Whether to enable or disable
     */
    void setPatternEnabled(const QString& patternId, bool enabled);

    /**
     * @brief Get all active hallucination patterns
     * @return List of active patterns
     */
    QList<HallucinationPattern> getActivePatterns() const;

    /**
     * @brief Get all navigation corrections
     * @return List of navigation corrections
     */
    QList<NavigationCorrection> getNavigationCorrections() const;

    /**
     * @brief Get execution correction history
     * @param limit Maximum number of records to retrieve
     * @return List of corrected execution contexts
     */
    QList<CorrectedExecutionContext> getCorrectionHistory(int limit = 100) const;

    /**
     * @brief Enable hot patching globally
     */
    void enableHotPatching();

    /**
     * @brief Disable hot patching globally
     */
    void disableHotPatching();

    /**
     * @brief Check if hot patching is enabled
     * @return true if enabled
     */
    bool isHotPatchingEnabled() const { return m_enabled; }

    /**
     * @brief Get correction statistics
     * @return JSON object with statistics
     */
    QJsonObject getCorrectionStatistics() const;

    /**
     * @brief Save corrections to persistent storage
     */
    void saveCorrections();

    /**
     * @brief Load corrections from persistent storage
     */
    void loadCorrections();

signals:
    /**
     * @brief Signal emitted when a hallucination is detected and corrected
     * @param original Original hallucinated output
     * @param corrected Corrected output
     * @param pattern The pattern that was matched
     */
    void hallucinationCorrected(const QString& original, const QString& corrected, const QString& pattern);

    /**
     * @brief Signal emitted when a navigation path is corrected
     * @param attempted The attempted path
     * @param corrected The corrected path
     */
    void navigationCorrected(const QString& attempted, const QString& corrected);

    /**
     * @brief Signal emitted when an intercept rule is applied
     * @param ruleId ID of the applied rule
     * @param result Result of the correction
     */
    void ruleApplied(const QString& ruleId, const QString& result);

    /**
     * @brief Signal emitted when a new pattern is learned
     * @param pattern The newly learned pattern
     */
    void patternLearned(const HallucinationPattern& pattern);

    /**
     * @brief Signal emitted when hot patching status changes
     * @param enabled Whether hot patching is now enabled
     */
    void hotPatchingStatusChanged(bool enabled);

private:
    /**
     * @brief Apply all enabled hallucination patterns
     * @param text Text to process
     * @return Corrected text
     */
    QString applyHallucinationPatterns(const QString& text);

    /**
     * @brief Apply all enabled intercept rules
     * @param text Text to process
     * @return Corrected text
     */
    QString applyInterceptRules(const QString& text);

    /**
     * @brief Detect common hallucination types
     * @param output Output to check
     * @return List of detected hallucination patterns
     */
    QList<HallucinationPattern> detectHallucinations(const QString& output);

    /**
     * @brief Validate file path existence and correctness
     * @param path Path to validate
     * @return Validation result with correction suggestion
     */
    QString validateFilePath(const QString& path);

    /**
     * @brief Check for circular dependencies in navigation
     * @param path Path to check
     * @return true if circular dependency detected
     */
    bool hasCircularDependency(const QString& path);

    /**
     * @brief Load rules from file
     */
    void loadRulesFromFile();

    /**
     * @brief Save rules to file
     */
    void saveRulesToFile();

    /**
     * @brief Generate unique ID for patterns/rules
     */
    QString generateUniqueId();

    QList<HallucinationPattern> m_hallucinationPatterns; ///< Learned hallucination patterns
    QList<NavigationCorrection> m_navigationCorrections; ///< Learned navigation corrections
    QList<MemoryInterceptRule> m_interceptRules;        ///< Intercept rules
    QList<CorrectedExecutionContext> m_correctionHistory; ///< Audit trail
    QString m_storagePath;                              ///< Path for persistent storage
    bool m_enabled;                                     ///< Whether hot patching is active
    mutable QMutex m_mutex;                             ///< Thread safety
    int m_idCounter;                                    ///< For generating unique IDs
    int m_totalCorrections;                             ///< Total corrections made
    int m_totalHallucinations;                          ///< Total hallucinations corrected
};

#endif // MEMORY_HOT_PATCHER_HPP
