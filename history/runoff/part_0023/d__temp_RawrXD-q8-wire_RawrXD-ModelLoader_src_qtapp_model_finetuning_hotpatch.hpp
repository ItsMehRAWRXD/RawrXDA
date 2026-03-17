// model_finetuning_hotpatch.hpp - Real-time model fine-tuning via response hotpatching
// Automatically corrects model outputs during inference without retraining
// Supports custom presets and live adjustments while model is running

#pragma once

#include <QObject>
#include <QString>
#include <QHash>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QMutex>
#include <functional>

/**
 * @brief Fine-tuning adjustment types that can be applied in real-time
 */
enum class FineTuningType {
    TemperatureBoost,      // Increase/decrease randomness
    TopKAdjustment,        // Modify top-k sampling
    TopPAdjustment,        // Modify nucleus sampling
    RepetitionPenalty,     // Reduce repetitive outputs
    LengthControl,         // Enforce min/max response length
    ToneAdjustment,        // Professional/casual/technical tone
    FactualityEnhancement, // Reduce hallucinations
    CreativityBoost,       // More creative responses
    CodeOptimization,      // Better code formatting/quality
    SafetyFilter,          // Content filtering
    BiasCorrection,        // Reduce biased outputs
    CustomRule             // User-defined transformation
};

/**
 * @brief Single fine-tuning rule that modifies model output
 */
struct FineTuningRule {
    QString name;
    FineTuningType type;
    bool enabled = true;
    double strength = 1.0;  // 0.0 to 2.0, 1.0 = normal
    int priority = 0;       // Higher = applied first
    
    // Pattern-based rules
    QRegularExpression triggerPattern;
    QString replacement;
    
    // Function-based rules
    std::function<QString(const QString&)> transformFunction;
    
    // Statistics
    int timesApplied = 0;
    int successfulApplications = 0;
    double averageImprovement = 0.0;
    QDateTime lastUsed;
    
    // MASM compression for efficient storage
    bool compressedStorage = false;
    QByteArray compressedData;
};

/**
 * @brief Preset collection of fine-tuning rules
 */
struct FineTuningPreset {
    QString name;
    QString description;
    QList<FineTuningRule> rules;
    QHash<QString, QVariant> parameters;  // temperature, top_p, etc.
    bool autoActivate = false;            // Apply on model load
    QDateTime created;
    QDateTime lastModified;
    int timesUsed = 0;
    double averageRating = 0.0;
};

/**
 * @brief Real-time model fine-tuning engine
 * 
 * Applies corrections to model outputs during inference without retraining.
 * Supports live adjustments, custom presets, and automatic learning.
 */
class ModelFineTuningHotpatch : public QObject {
    Q_OBJECT

public:
    explicit ModelFineTuningHotpatch(QObject* parent = nullptr);
    ~ModelFineTuningHotpatch();

    /**
     * @brief Apply fine-tuning to model output in real-time
     * @param modelOutput Raw model response
     * @param context Request context (prompt, parameters, etc.)
     * @return Fine-tuned response
     */
    QString applyFineTuning(const QString& modelOutput, const QJsonObject& context);

    /**
     * @brief Add a fine-tuning rule
     */
    void addRule(const FineTuningRule& rule);
    
    /**
     * @brief Remove a rule by name
     */
    void removeRule(const QString& name);
    
    /**
     * @brief Enable/disable a rule
     */
    void setRuleEnabled(const QString& name, bool enabled);
    
    /**
     * @brief Adjust rule strength (0.0 to 2.0)
     */
    void setRuleStrength(const QString& name, double strength);
    
    /**
     * @brief Get all active rules
     */
    QList<FineTuningRule> getActiveRules() const;

    // Preset management
    void savePreset(const QString& name, const QString& description);
    void loadPreset(const QString& name);
    void deletePreset(const QString& name);
    QStringList listPresets() const;
    FineTuningPreset getPreset(const QString& name) const;
    
    // Hot-reload configuration
    void reloadConfiguration();
    void exportConfiguration(const QString& filePath);
    void importConfiguration(const QString& filePath);

    // Live parameter adjustment
    void setParameter(const QString& key, const QVariant& value);
    QVariant getParameter(const QString& key) const;
    QHash<QString, QVariant> getAllParameters() const;

    // Auto-correction based on feedback
    void recordFeedback(const QString& output, bool positive, const QString& correction = QString());
    void learnFromCorrection(const QString& original, const QString& corrected);
    
    // Statistics
    struct FineTuningStats {
        quint64 totalApplications = 0;
        quint64 successfulCorrections = 0;
        quint64 failedCorrections = 0;
        QHash<FineTuningType, int> typeUsage;
        double averageImprovement = 0.0;
        QDateTime lastUpdate;
    };
    FineTuningStats getStatistics() const;
    void resetStatistics();
    
    // ===== DIRECT MEMORY MANIPULATION (for model weights integration) =====
    /**
     * @brief Attach to model memory for direct weight manipulation during fine-tuning
     * @param modelPtr Pointer to loaded model data
     * @param modelSize Total size of model in bytes
     * @return true if successfully attached
     */
    bool attachToModel(void* modelPtr, size_t modelSize);
    
    /**
     * @brief Detach from model memory
     */
    void detachFromModel();
    
    /**
     * @brief Directly modify weight values based on fine-tuning feedback
     * @param tensorName Name of tensor to modify
     * @param adjustments Vector of (index, delta) pairs for weight adjustments
     * @return true if successful
     */
    struct WeightAdjustment {
        size_t index;
        float delta;  // Amount to add to weight
    };
    bool directWeightAdjustment(const QString& tensorName, const QVector<WeightAdjustment>& adjustments);
    
    /**
     * @brief Apply learned corrections directly to model memory
     * @param correctionData Compiled correction patterns
     * @return true if successful
     */
    bool applyDirectCorrection(const QByteArray& correctionData);
    
    /**
     * @brief Extract weight region for analysis
     * @param tensorName Tensor to extract from
     * @param startIndex Starting weight index
     * @param count Number of weights to extract
     * @param outWeights Output vector for weight values
     * @return true if successful
     */
    bool extractWeights(const QString& tensorName, size_t startIndex, size_t count, QVector<float>& outWeights);
    
    /**
     * @brief Inject weights directly (bypass normal loading)
     * @param tensorName Tensor to inject into
     * @param startIndex Starting weight index
     * @param weights Weight values to inject
     * @return true if successful
     */
    bool injectWeights(const QString& tensorName, size_t startIndex, const QVector<float>& weights);
    
    /**
     * @brief Scale weights in a tensor region
     * @param tensorName Tensor to scale
     * @param startIndex Starting weight index
     * @param count Number of weights to scale
     * @param scaleFactor Multiplication factor
     * @return true if successful
     */
    bool scaleWeights(const QString& tensorName, size_t startIndex, size_t count, float scaleFactor);
    
    /**
     * @brief Apply fine-tuning delta to model memory
     * @param deltaData Binary delta from fine-tuning run
     * @return true if successful
     */
    bool applyFineTuningDelta(const QByteArray& deltaData);
    
    /**
     * @brief Read raw bytes from model memory
     * @param offset Byte offset
     * @param size Number of bytes to read
     * @return Byte array of data
     */
    QByteArray readModelMemory(size_t offset, size_t size);
    
    /**
     * @brief Write raw bytes to model memory
     * @param offset Byte offset
     * @param data Data to write
     * @return true if successful
     */
    bool writeModelMemory(size_t offset, const QByteArray& data);

signals:
    void fineTuningApplied(const QString& original, const QString& finetuned, const QStringList& rulesApplied);
    void ruleAdded(const QString& ruleName);
    void ruleRemoved(const QString& ruleName);
    void presetLoaded(const QString& presetName);
    void parameterChanged(const QString& key, const QVariant& value);
    void feedbackReceived(bool positive, const QString& output);

private:
    // Rule application
    QString applyRule(const QString& text, const FineTuningRule& rule, const QJsonObject& context);
    QList<FineTuningRule> selectApplicableRules(const QString& text, const QJsonObject& context);
    void sortRulesByPriority(QList<FineTuningRule>& rules);
    
    // Built-in transformations
    QString applyTemperatureBoost(const QString& text, double strength);
    QString applyRepetitionPenalty(const QString& text, double strength);
    QString applyLengthControl(const QString& text, int minLength, int maxLength);
    QString applyToneAdjustment(const QString& text, const QString& targetTone, double strength);
    QString applyFactualityEnhancement(const QString& text, double strength);
    QString applyCreativityBoost(const QString& text, double strength);
    QString applyCodeOptimization(const QString& text, double strength);
    QString applySafetyFilter(const QString& text, double strength);
    QString applyBiasCorrection(const QString& text, double strength);
    
    // Learning system integration
    void updateRuleStatistics(const QString& ruleName, bool success, double improvement);
    void optimizeRules();  // Remove low-performing rules
    void suggestNewRules();  // Based on observed patterns
    
    // MASM compression for rule storage
    QByteArray compressRule(const FineTuningRule& rule);
    FineTuningRule decompressRule(const QByteArray& compressed);

    // Data
    QHash<QString, FineTuningRule> m_rules;
    QHash<QString, FineTuningPreset> m_presets;
    QHash<QString, QVariant> m_parameters;
    FineTuningStats m_stats;
    
    // Active preset
    QString m_activePreset;
    
    // Direct memory access
    void* m_modelPtr = nullptr;
    size_t m_modelSize = 0;
    bool m_modelAttached = false;
    
    // Thread safety
    mutable QMutex m_mutex;
    
    // Configuration paths
    QString m_configDir;
    QString m_presetsFile;
    QString m_rulesFile;
    
    // Auto-learning
    QList<QPair<QString, QString>> m_feedbackHistory;  // original, corrected
    static constexpr int MAX_FEEDBACK_HISTORY = 100;
};
