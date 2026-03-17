#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QVariant>
#include <mutex>
#include <memory>

/**
 * @brief InferenceSettingsManager - Phase 2 integration
 * 
 * Thread-safe singleton for managing inference settings with:
 * - Preset configurations (Balanced, Performance, Quality, Custom)
 * - Model-specific settings persistence
 * - Recent models tracking
 * - Settings validation and fallbacks
 * - Integration with Qt SettingsManager
 */
class InferenceSettingsManager : public QObject
{
    Q_OBJECT

public:
    enum Preset {
        Balanced = 0,
        Performance = 1,
        Quality = 2,
        Custom = 3
    };

    // Singleton access
    static InferenceSettingsManager& getInstance();
    
    // Initialization
    void initialize();
    bool isInitialized() const { return m_initialized; }

    // Preset management
    void applyPreset(Preset preset);
    Preset getCurrentPreset() const { return m_currentPreset; }
    QString getPresetName(Preset preset) const;

    // Model settings
    void setCurrentModelPath(const QString& modelPath);
    QString getCurrentModelPath() const { return m_currentModelPath; }
    
    // Recent models
    QStringList getRecentModels(int maxCount = 10) const;
    void addRecentModel(const QString& modelPath);
    void clearRecentModels();

    // Generation parameters
    void setTemperature(double temp);
    double getTemperature() const { return m_temperature; }
    
    void setTopP(double topP);
    double getTopP() const { return m_topP; }
    
    void setTopK(int topK);
    int getTopK() const { return m_topK; }
    
    void setMaxTokens(int maxTokens);
    int getMaxTokens() const { return m_maxTokens; }
    
    void setRepetitionPenalty(double penalty);
    double getRepetitionPenalty() const { return m_repetitionPenalty; }

    // Ollama integration
    void setOllamaModelTag(const QString& tag);
    QString getOllamaModelTag() const { return m_ollamaModelTag; }
    
    void setUseOllama(bool use);
    bool getUseOllama() const { return m_useOllama; }

    // Validation
    bool validateSettings() const;
    
    // Persistence
    void save();
    void load();
    
    // Export/Import
    QJsonObject exportToJSON() const;
    void importFromJSON(const QJsonObject& json);

signals:
    void settingsChanged();
    void presetChanged(Preset preset);
    void modelPathChanged(const QString& modelPath);
    void recentModelsUpdated();

private:
    InferenceSettingsManager();
    ~InferenceSettingsManager();
    
    void loadGenerationParams();
    void saveGenerationParams();
    void loadRecentModels();
    void saveRecentModels();
    
    mutable std::mutex m_mutex;
    bool m_initialized = false;
    Preset m_currentPreset = Balanced;
    QString m_currentModelPath;
    QStringList m_recentModels;
    
    // Generation parameters
    double m_temperature = 0.7;
    double m_topP = 0.9;
    int m_topK = 40;
    int m_maxTokens = 2048;
    double m_repetitionPenalty = 1.1;
    
    // Ollama settings
    QString m_ollamaModelTag = "llama2";
    bool m_useOllama = false;
    
    // Prevent copying
    InferenceSettingsManager(const InferenceSettingsManager&) = delete;
    InferenceSettingsManager& operator=(const InferenceSettingsManager&) = delete;
};