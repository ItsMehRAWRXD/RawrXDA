#ifndef ENTERPRISE_AI_REASONING_ENGINE_HPP
#define ENTERPRISE_AI_REASONING_ENGINE_HPP

#include <QObject>
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>

struct ReasoningStep {
    QString id;
    QString type; // perception | analysis | decision
    QJsonObject input;
    QJsonObject output;
    QString explanation;
    QDateTime timestamp;
    double confidence = 0.0;
};

struct Decision {
    QString action;
    QJsonObject parameters;
    double confidence = 0.0;
    QString reasoning;
    QJsonArray alternatives;       // evaluated actions with scores
    QJsonObject expectedOutcomes;  // simulated outcome summary
};

struct ReasoningContext {
    QString missionId;
    QJsonObject currentState;
    QJsonObject goals;
    double confidenceThreshold = 0.0;
    int maxReasoningDepth = 0;
};

// Forward declaration of the private implementation struct
struct ReasoningEnginePrivate;

class EnterpriseAIReasoningEngine : public QObject {
    Q_OBJECT
public:
    explicit EnterpriseAIReasoningEngine(QObject* parent = nullptr);
    ~EnterpriseAIReasoningEngine();

    Decision makeAutonomousDecision(const ReasoningContext& context);
    QVector<ReasoningStep> getReasoningProcess(const QString& missionId);

    // Ready-to-use reasoning entry points
    Decision performStrategicReasoning(const QJsonObject& strategicGoals);
    Decision performTacticalReasoning(const QJsonObject& tacticalSituation);
    Decision performAdaptiveReasoning(const QJsonObject& changingConditions);

    // Learning & model updates
    void learnFromOutcome(const QString& missionId, bool success, const QJsonObject& actualOutcomes);
    void updateReasoningModels(const QJsonObject& learningData);

    // Higher-level utilities for enterprise workflows
    QJsonObject analyzeCodebaseStrategy(const QString& projectPath);
    QJsonObject determineOptimalDevelopmentApproach(const QJsonObject& requirements);
    QJsonObject predictDevelopmentChallenges(const QJsonObject& projectContext);

signals:
    void reasoningProcessUpdated(const QString& missionId, const QVector<ReasoningStep>& steps);
    void decisionMade(const QString& missionId, const Decision& decision);
    void learningOccurred(const QString& pattern, double learningGain);

private:
    Decision generateOptimalDecision(const QJsonObject& analysis, const ReasoningContext& context);

    QScopedPointer<ReasoningEnginePrivate> d_ptr;
};

#endif // ENTERPRISE_AI_REASONING_ENGINE_HPP
