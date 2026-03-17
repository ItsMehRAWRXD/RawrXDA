#ifndef AGENTIC_LEARNING_SYSTEM_H
#define AGENTIC_LEARNING_SYSTEM_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QMap>
#include <QJsonObject>

/**
 * @class AgenticLearningSystem
 * @brief Advanced machine learning system for IDE optimization and user behavior learning
 */
class AgenticLearningSystem : public QObject {
    Q_OBJECT

public:
    explicit AgenticLearningSystem(QObject* parent = nullptr);
    ~AgenticLearningSystem();

    /**
     * @brief Initialize the learning system
     */
    void initialize();

    /**
     * @brief Record user action for learning
     */
    void recordUserAction(const QString& action, const QJsonObject& context = QJsonObject());

    /**
     * @brief Analyze current IDE state for optimization opportunities
     */
    QJsonObject analyzeForOptimizations();

    /**
     * @brief Get learning statistics
     */
    QJsonObject getLearningStatistics() const;

signals:
    /**
     * @brief Emitted when optimization is suggested
     */
    void optimizationSuggested(const QString& change, double improvement);

    /**
     * @brief Learning progress update
     */
    void learningProgress(int current, int total);

    /**
     * @brief Learning system status
     */
    void statusUpdate(const QString& message);

public slots:
    /**
     * @brief Start learning session
     */
    void startLearning();

    /**
     * @brief Stop learning session
     */
    void stopLearning();

    /**
     * @brief Process optimization suggestion
     */
    void processOptimizationSuggestion(const QString& change, double improvement);

private slots:
    void onTimerTimeout();
    void onAnalysisComplete();
    void onOptimizationApplied(const QString& change, double improvement);

private:
    void initializeModel();
    void processUserBehavior();
    void updateOptimizationSuggestions();
    void generateInsights();

    // Member variables
    bool m_initialized;
    QTimer* m_learningTimer;
    QTimer* m_analysisTimer;
    QMap<QString, int> m_actionCounts;
    QJsonObject m_behaviorData;
    QJsonObject m_optimizationSuggestions;
    int m_learningSessionCount;
    int m_optimizationCount;
    double m_averageImprovement;

    // Internal state
    QString m_currentState;
    QJsonObject m_analyticsData;
};

#endif // AGENTIC_LEARNING_SYSTEM_H