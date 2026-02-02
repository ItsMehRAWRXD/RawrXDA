#pragma once

#include <mutex>
#include <memory>
#include <string>
#include <vector>

namespace RawrXD {

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
class InferenceSettingsManager
{

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
    std::string getPresetName(Preset preset) const;

    // Model settings
    void setCurrentModelPath(const std::string& modelPath);
    std::string getCurrentModelPath() const { return m_currentModelPath; }
    
    // Recent models
    std::vector<std::string> getRecentModels(int maxCount = 10) const;
    void addRecentModel(const std::string& modelPath);
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
    void setOllamaModelTag(const std::string& tag);
    std::string getOllamaModelTag() const { return m_ollamaModelTag; }
    
    void setUseOllama(bool use);
    bool getUseOllama() const { return m_useOllama; }

    // Validation
    bool validateSettings() const;
    
    // Persistence
    void save();
    void load();
    
    // Export/Import
    void* exportToJSON() const;
    void importFromJSON(const void*& json);


    void settingsChanged();
    void presetChanged(Preset preset);
    void modelPathChanged(const std::string& modelPath);
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
    std::string m_currentModelPath;
    std::vector<std::string> m_recentModels;
    
    // Generation parameters
    double m_temperature = 0.7;
    double m_topP = 0.9;
    int m_topK = 40;
    int m_maxTokens = 2048;
    double m_repetitionPenalty = 1.1;
    
    // Ollama settings
    std::string m_ollamaModelTag = "llama2";
    bool m_useOllama = false;
    
    // Prevent copying
    InferenceSettingsManager(const InferenceSettingsManager&) = delete;
    InferenceSettingsManager& operator=(const InferenceSettingsManager&) = delete;
};

}
