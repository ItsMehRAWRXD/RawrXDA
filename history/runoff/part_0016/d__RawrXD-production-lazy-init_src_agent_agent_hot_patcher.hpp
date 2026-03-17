// ============================================================================
// File: src/agent/agent_hot_patcher.hpp
// 
// Purpose: Real-time hallucination and navigation correction interface
// This system intercepts model outputs and corrects them in real-time
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#pragma once

#include <QString>
#include <QStringList>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QMutex>
#include <QMetaType>
#include <QUuid>
#include <vector>
#include <atomic>
#include <QList>

/**
 * @struct HallucinationDetection
 * @brief Information about detected hallucination
 */
struct HallucinationDetection {
    QString detectionId;
    QString hallucationType;
    QString originalContent;
    QString suggestedCorrection;
    QString detectedContent;  // Added for bridge integration
    QString expectedContent;  // Added for bridge integration
    double confidence;
    bool detected;
};

/**
 * @struct NavigationFix
 * @brief Information about a navigation path correction
 */
struct NavigationFix {
    QString navigationId;
    QString originalPath;
    QString correctedPath;
    QString incorrectPath;    // Added for bridge integration
    QString correctPath;      // Added for bridge integration
    QString reasoning;        // Added for bridge integration
    double effectiveness;
    QDateTime lastApplied;
    int timesCorrected;
};

/**
 * @struct BehaviorPatch
 * @brief Information about a behavioral model patch
 */
struct BehaviorPatch {
    QString patchId;
    QString description;
    QString patchType;        // Added for bridge integration
    double successRate;       // Added for bridge integration
    bool enabled;
};

class AgentHotPatcher : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(AgentHotPatcher)

public:
    explicit AgentHotPatcher(QObject* parent = nullptr);
    ~AgentHotPatcher();

    void initialize(const QString& ggufLoaderPath, int flags);
    QJsonObject interceptModelOutput(const QString& modelOutput, const QJsonObject& context);
    HallucinationDetection detectHallucination(const QString& content, const QJsonObject& context);
    QString correctHallucination(const HallucinationDetection& detection);
    NavigationFix fixNavigationError(const QString& path, const QJsonObject& context);
    QString applyBehaviorPatches(const QString& output);
    void registerCorrectionPattern(const HallucinationDetection& pattern);
    void registerNavigationFix(const NavigationFix& fix);
    void createBehaviorPatch(const BehaviorPatch& patch);
    
    QJsonObject getCorrectionStatistics() const;
    int getCorrectionPatternCount() const;

    void setHotPatchingEnabled(bool enabled);
    bool isHotPatchingEnabled() const;

    HallucinationDetection analyzeForHallucinations(const QString& content);
    bool validateNavigationPath(const QString& path);
    QString extractReasoningChain(const QString& content);
    QString extractReasoningChain(const QJsonObject& content);
    bool validateReasoningLogic(const QString& content);

signals:
    void hallucinationDetected(const HallucinationDetection& detection);
    void hallucinationCorrected(const HallucinationDetection& correction);
    void navigationErrorFixed(const NavigationFix& fix);
    void behaviorPatchApplied(const BehaviorPatch& patch);
    void statisticsUpdated(const QJsonObject& stats);

private:
    std::atomic<int> m_totalHallucinationsDetected{0};
    std::atomic<int> m_hallucinationsCorrected{0};
    std::atomic<int> m_navigationFixesApplied{0};

    bool m_enabled;
    int m_idCounter;
    int m_interceptionPort;
    QString m_ggufLoaderPath;
    bool m_debugLogging = false;
    mutable QMutex m_mutex;
    
    QList<HallucinationDetection> m_correctionPatterns;
    QList<NavigationFix> m_navigationPatterns;
    QList<BehaviorPatch> m_behaviorPatches;
    QList<HallucinationDetection> m_detectedHallucinations;
    QList<NavigationFix> m_navigationFixes;
    QList<HallucinationDetection> m_hallucationPatterns;

    bool startInterceptorServer(int port);
    bool loadCorrectionPatterns();
    bool saveCorrectionPatterns();
    QString generateUniqueId();
    QJsonObject processInterceptedResponse(const QJsonObject& response);
};

Q_DECLARE_METATYPE(HallucinationDetection)
Q_DECLARE_METATYPE(NavigationFix)
Q_DECLARE_METATYPE(BehaviorPatch)



