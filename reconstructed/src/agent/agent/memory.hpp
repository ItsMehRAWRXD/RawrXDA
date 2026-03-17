#ifndef AGENT_MEMORY_HPP
#define AGENT_MEMORY_HPP

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <memory>
#include <vector>

/**
 * @brief Struct representing an execution record in the agent's memory
 * 
 * This struct stores information about a completed execution, including:
 * - The original wish that triggered the execution
 * - The generated plan
 * - Whether the execution was successful
 * - Execution time metrics
 * - Any error messages if the execution failed
 * - Timestamp of when the execution occurred
 */
struct ExecutionRecord {
    int id;                    ///< Unique identifier for the record
    QString wish;              ///< The original wish that triggered execution
    QString plan;              ///< The plan that was executed
    bool success;              ///< Whether the execution was successful
    int executionTime;         ///< Time taken for execution in milliseconds
    QString errorMessage;      ///< Error message if execution failed
    QDateTime timestamp;       ///< When the execution occurred
};

/**
 * @brief Struct representing a learned pattern
 * 
 * This struct stores patterns that the agent has learned from previous executions,
 * including the type of pattern, the data associated with it, the project context,
 * success rate, and when it was last used.
 */
struct LearnedPattern {
    int id;                    ///< Unique identifier for the pattern
    QString patternType;       ///< Type of pattern (e.g., "file_operation", "build_pattern")
    QString patternData;       ///< Data associated with the pattern
    QString projectContext;    ///< Context in which the pattern was observed
    double successRate;        ///< Success rate of this pattern (0.0 to 1.0)
    QDateTime lastUsed;        ///< When this pattern was last used
};

/**
 * @brief Struct representing user preferences
 * 
 * This struct stores inferred user preferences, including the type of preference,
 * the value, confidence level in the preference, and when it was last updated.
 */
struct UserPreference {
    int id;                    ///< Unique identifier for the preference
    QString preferenceType;    ///< Type of preference (e.g., "coding_style", "tool_choice")
    QString preferenceValue;   ///< The preferred value
    double confidenceLevel;    ///< Confidence in this preference (0.0 to 1.0)
    QDateTime lastUpdated;     ///< When this preference was last updated
};

/**
 * @brief Agent Memory System for Persistent Learning
 * 
 * The AgentMemory class provides a persistent memory system for the autonomous agent,
 * enabling it to learn from past executions, remember user preferences, and identify
 * patterns in project behavior.
 * 
 * Key features:
 * - Persistent storage using SQLite
 * - Execution history tracking
 * - Pattern learning and recognition
 * - User preference inference
 * - Performance metrics collection
 * 
 * The class is designed to be thread-safe and integrates with the Qt signal/slot system
 * for real-time updates.
 */
class AgentMemory : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructor for AgentMemory
     * @param parent The parent QObject
     */
    explicit AgentMemory(QObject* parent = nullptr);

    /**
     * @brief Destructor for AgentMemory
     */
    ~AgentMemory();

    /**
     * @brief Initialize the memory system
     * @param databasePath Path to the SQLite database file
     * @return true if initialization was successful, false otherwise
     */
    bool initialize(const QString& databasePath);

    /**
     * @brief Record an execution in the agent's memory
     * @param wish The original wish that triggered the execution
     * @param plan The plan that was executed
     * @param success Whether the execution was successful
     * @param executionTime Time taken for execution in milliseconds
     * @param errorMessage Error message if execution failed
     */
    void recordExecution(const QString& wish, const QString& plan, bool success, 
                        int executionTime, const QString& errorMessage = "");

    /**
     * @brief Retrieve execution history
     * @param limit Maximum number of records to retrieve (default: 100)
     * @return List of execution records
     */
    QList<ExecutionRecord> getExecutionHistory(int limit = 100) const;

    /**
     * @brief Record a learned pattern
     * @param patternType Type of pattern
     * @param patternData Data associated with the pattern
     * @param projectContext Context in which the pattern was observed
     * @param success Whether the pattern led to success
     */
    void recordPattern(const QString& patternType, const QString& patternData,
                      const QString& projectContext, bool success);

    /**
     * @brief Retrieve learned patterns
     * @param patternType Optional filter by pattern type
     * @return List of learned patterns
     */
    QList<LearnedPattern> getLearnedPatterns(const QString& patternType = "") const;

    /**
     * @brief Update or create a user preference
     * @param preferenceType Type of preference
     * @param preferenceValue The preferred value
     * @param confidence Confidence level in this preference (0.0 to 1.0)
     */
    void updateUserPreference(const QString& preferenceType, const QString& preferenceValue,
                             double confidence);

    /**
     * @brief Retrieve user preferences
     * @param preferenceType Optional filter by preference type
     * @return List of user preferences
     */
    QList<UserPreference> getUserPreferences(const QString& preferenceType = "") const;

    /**
     * @brief Get success rate for a specific pattern type
     * @param patternType Type of pattern
     * @return Success rate (0.0 to 1.0)
     */
    double getPatternSuccessRate(const QString& patternType) const;

    /**
     * @brief Get average execution time for successful executions
     * @return Average execution time in milliseconds
     */
    double getAverageExecutionTime() const;

    /**
     * @brief Get failure rate of executions
     * @return Failure rate (0.0 to 1.0)
     */
    double getFailureRate() const;

signals:
    /**
     * @brief Signal emitted when a new execution is recorded
     * @param record The execution record that was added
     */
    void executionRecorded(const ExecutionRecord& record);

    /**
     * @brief Signal emitted when a new pattern is learned
     * @param pattern The learned pattern
     */
    void patternLearned(const LearnedPattern& pattern);

    /**
     * @brief Signal emitted when a user preference is updated
     * @param preference The updated preference
     */
    void preferenceUpdated(const UserPreference& preference);

private:
    /**
     * @brief Create the necessary database tables
     * @return true if tables were created successfully, false otherwise
     */
    bool createTables();

    /**
     * @brief Update the success rate of a pattern based on a new outcome
     * @param patternId ID of the pattern to update
     * @param success Whether the pattern led to success
     */
    void updatePatternSuccessRate(int patternId, bool success);

    QSqlDatabase m_database;                    ///< SQLite database connection
    QString m_databasePath;                     ///< Path to the database file
    mutable QMutex m_mutex;                     ///< Mutex for thread safety
};

#endif // AGENT_MEMORY_HPP