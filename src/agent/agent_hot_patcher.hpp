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

/**
 * @struct HallucinationDetection
 * @brief Information about detected hallucination
 */
struct HallucinationDetection {
    QString detectionId;              ///< Unique detection ID
    QString hallucinationType;        ///< Type: invalid_path, fabricated_path, logic_contradiction, etc.
    double confidence = 0.0;          ///< Confidence (0.0-1.0)
    QString detectedContent;          ///< What was detected
    QString expectedContent;          ///< What it should be
    QString correctionStrategy;       ///< How to fix it
    QDateTime detectedAt;             ///< When detected
    bool correctionApplied = false;   ///< Was correction applied
};

/**
 * @struct NavigationFix
 * @brief Information about fixed navigation error
 */
struct NavigationFix {
    QString fixId;                    ///< Unique fix ID
    QString incorrectPath;            ///< Original path
    QString correctPath;              ///< Normalized path
    QString reasoning;                ///< Why it was wrong
    double effectiveness = 0.0;       ///< How effective (0.0-1.0)
};

/**
 * @struct BehaviorPatch
 * @brief Behavior modification for model outputs
 */
struct BehaviorPatch {
    QString patchId;                  ///< Unique patch ID
    QString patchType;                ///< Type: prompt_modifier, output_filter, validator
    QString condition;                ///< When to apply
    QString action;                   ///< What to do
    QStringList affectedModels;       ///< Models to apply to
    double successRate = 0.0;         ///< Success rate
    bool enabled = true;              ///< Is patch enabled
    QDateTime createdAt;              ///< When created
};

/**
 * @class AgentHotPatcher
 * @brief Real-time hallucination detection and correction
 * 
 * Features:
 * - Detects 6 types of hallucinations
 * - Applies real-time corrections
 * - Learns from corrections
 * - Thread-safe operation (atomic stats, mutex-protected vectors)
 * - Full statistics tracking
 * - Non-copyable (Q_DISABLE_COPY)
 */
class AgentHotPatcher : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(AgentHotPatcher)

public:
    explicit AgentHotPatcher(QObject* parent = nullptr);
    ~AgentHotPatcher();

    /**
     * Initialize hot patcher
     * @param ggufLoaderPath Path to GGUF loader executable
     * @param flags Reserved for future use
     */
    void initialize(const QString& ggufLoaderPath, int flags);

    /**
     * Intercept and patch model output
     * @param modelOutput Raw model output
     * @param context Execution context
     * @return Corrected output as JSON
     */
    QJsonObject interceptModelOutput(const QString& modelOutput, const QJsonObject& context);

    /**
     * Detect hallucinations in content
     * @param content Content to analyze
     * @param context Execution context
     * @return Detection info (empty if no hallucination)
     */
    HallucinationDetection detectHallucination(const QString& content, const QJsonObject& context);

    /**
     * Apply correction to detected hallucination
     * @param detection Hallucination detection
     * @return Corrected content
     */
    QString correctHallucination(const HallucinationDetection& detection);

    /**
     * Fix navigation error in path
     * @param path Incorrect path
     * @param context Execution context
     * @return Navigation fix info
     */
    NavigationFix fixNavigationError(const QString& path, const QJsonObject& context);

    /**
     * Apply behavior patches to output
     * @param output Original output
     * @return Patched output
     */
    QString applyBehaviorPatches(const QString& output);

    /**
     * Register a correction pattern
     * @param pattern Pattern to register
     */
    void registerCorrectionPattern(const HallucinationDetection& pattern);

    /**
     * Register navigation fix pattern
     * @param fix Fix to register
     */
    void registerNavigationFix(const NavigationFix& fix);

    /**
     * Create behavior patch
     * @param patch Patch to create
     */
    void createBehaviorPatch(const BehaviorPatch& patch);

    /**
     * Convenience wrapper: add correction pattern (bridge compatibility)
     */
    inline void addCorrectionPattern(const HallucinationDetection& p) { registerCorrectionPattern(p); }

    /**
     * Convenience wrapper: add navigation fix (bridge compatibility)
     */
    inline void addNavigationFix(const NavigationFix& f) { registerNavigationFix(f); }

    /**
     * Convenience wrapper: add behavior patch (bridge compatibility)
     */
    inline void addBehaviorPatch(const BehaviorPatch& p) { createBehaviorPatch(p); }

    /**
     * Enable/disable hot patching
     * @param enabled True to enable
     */
    void setHotPatchingEnabled(bool enabled);

    /**
     * Check if hot patching is enabled
     */
    bool isHotPatchingEnabled() const;

    /**
     * Enable debug logging
     * @param enabled True to enable
     */
    void setDebugLogging(bool enabled);

    /**
     * Get correction statistics
     * @return Statistics JSON
     */
    QJsonObject getCorrectionStatistics() const;

    /**
     * Get count of correction patterns
     */
    int getCorrectionPatternCount() const;

signals:
    void hallucinationDetected(const HallucinationDetection& detection);
    void hallucinationCorrected(const HallucinationDetection& correction);
    void navigationErrorFixed(const NavigationFix& fix);
    void behaviorPatchApplied(const BehaviorPatch& patch);
    void statisticsUpdated(const QJsonObject& stats);

private:
    // ====== Statistics (lock-free atomic counters) ======
    std::atomic<int> m_totalHallucinationsDetected{0};
    std::atomic<int> m_hallucinationsCorrected{0};
    std::atomic<int> m_navigationFixesApplied{0};

    // ====== Runtime state (protected by m_mutex) ======
    bool m_hotPatchingEnabled = false;
    bool m_debugLogging = false;
    mutable QMutex m_mutex;
    
    // Patterns
    std::vector<HallucinationDetection> m_correctionPatterns;
    std::vector<NavigationFix> m_navigationPatterns;
    std::vector<BehaviorPatch> m_behaviorPatches;
    
    // Helper methods
    HallucinationDetection detectPathHallucination(const QString& content);
    HallucinationDetection detectLogicContradiction(const QString& content);
    HallucinationDetection detectIncompleteReasoning(const QString& content);
    QString normalizePathInContent(const QString& content, const QString& validPath);
    bool isValidPath(const QString& path) const;
};

// Qt meta-type registration (must be outside class definition)
Q_DECLARE_METATYPE(HallucinationDetection)
Q_DECLARE_METATYPE(NavigationFix)
Q_DECLARE_METATYPE(BehaviorPatch)

#endif // AGENT_HOT_PATCHER_HPP
