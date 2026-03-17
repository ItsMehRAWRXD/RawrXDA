/**
 * InferenceSettingsManager implementation - Phase 2 integration
 */

#include "InferenceSettingsManager.h"
//#include "settings_manager.h"

#include <algorithm>
#include <cmath>

using namespace RawrXD;

static InferenceSettingsManager* g_instance = nullptr;
static std::mutex g_instanceMutex;

InferenceSettingsManager& InferenceSettingsManager::getInstance()
{
    if (!g_instance) {
        std::lock_guard<std::mutex> lock(g_instanceMutex);
        if (!g_instance) {
            g_instance = new InferenceSettingsManager();
        }
    }
    return *g_instance;
}

InferenceSettingsManager::InferenceSettingsManager()
    : m_initialized(false), m_currentPreset(Balanced)
{
}

InferenceSettingsManager::~InferenceSettingsManager()
{
    if (m_initialized) {
        save();
    }
}

void InferenceSettingsManager::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return;
    }

    // Load all settings from SettingsManager
    load();

    m_initialized = true;
}

void InferenceSettingsManager::applyPreset(Preset preset)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_currentPreset = preset;
    
    switch (preset) {
    case Balanced:
        m_temperature = 0.7;
        m_topP = 0.9;
        m_topK = 40;
        m_maxTokens = 2048;
        m_repetitionPenalty = 1.1;
        break;
    case Performance:
        m_temperature = 0.3;
        m_topP = 0.5;
        m_topK = 20;
        m_maxTokens = 1024;
        m_repetitionPenalty = 1.05;
        break;
    case Quality:
        m_temperature = 0.9;
        m_topP = 0.95;
        m_topK = 60;
        m_maxTokens = 4096;
        m_repetitionPenalty = 1.15;
        break;
    case Custom:
        // Keep current custom settings
        break;
    }
    
    presetChanged(preset);
    settingsChanged();
}

std::string InferenceSettingsManager::getPresetName(Preset preset) const
{
    switch (preset) {
    case Balanced: return "Balanced";
    case Performance: return "Performance";
    case Quality: return "Quality";
    case Custom: return "Custom";
    default: return "Unknown";
    }
}

void InferenceSettingsManager::setCurrentModelPath(const std::string& modelPath)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentModelPath = modelPath;
    }
    
    addRecentModel(modelPath);
    modelPathChanged(modelPath);
    settingsChanged();
}

std::vector<std::string> InferenceSettingsManager::getRecentModels(int maxCount) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_recentModels.size() > static_cast<size_t>(maxCount)) {
        return std::vector<std::string>(m_recentModels.begin(), m_recentModels.begin() + maxCount);
    }
    return m_recentModels;
}

void InferenceSettingsManager::addRecentModel(const std::string& modelPath)
{
    if (modelPath.empty()) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Remove if already exists
    m_recentModels.erase(std::remove(m_recentModels.begin(), m_recentModels.end(), modelPath), m_recentModels.end());
    
    // Add to front
    m_recentModels.insert(m_recentModels.begin(), modelPath);
    
    // Limit to 20 recent models
    if (m_recentModels.size() > 20) {
        m_recentModels.resize(20);
    }
    
    recentModelsUpdated();
}

void InferenceSettingsManager::clearRecentModels()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_recentModels.clear();
    recentModelsUpdated();
}

// Generation parameter setters
void InferenceSettingsManager::setTemperature(double temp)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_temperature = std::clamp(temp, 0.0, 2.0);
    m_currentPreset = Custom;
    settingsChanged();
}

void InferenceSettingsManager::setTopP(double topP)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_topP = std::clamp(topP, 0.0, 1.0);
    m_currentPreset = Custom;
    settingsChanged();
}

void InferenceSettingsManager::setTopK(int topK)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_topK = std::max(1, topK);
    m_currentPreset = Custom;
    settingsChanged();
}

void InferenceSettingsManager::setMaxTokens(int maxTokens)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxTokens = std::max(1, maxTokens);
    m_currentPreset = Custom;
    settingsChanged();
}

void InferenceSettingsManager::setRepetitionPenalty(double penalty)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_repetitionPenalty = std::clamp(penalty, 1.0, 2.0);
    m_currentPreset = Custom;
    settingsChanged();
}

void InferenceSettingsManager::setOllamaModelTag(const std::string& tag)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ollamaModelTag = tag;
    settingsChanged();
}

void InferenceSettingsManager::setUseOllama(bool use)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_useOllama = use;
    settingsChanged();
}

bool InferenceSettingsManager::validateSettings() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    return m_temperature >= 0.0 && m_temperature <= 2.0 &&
           m_topP >= 0.0 && m_topP <= 1.0 &&
           m_topK >= 1 &&
           m_maxTokens >= 1 &&
           m_repetitionPenalty >= 1.0 && m_repetitionPenalty <= 2.0;
}

void InferenceSettingsManager::save()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) return;
    
    saveGenerationParams();
    saveRecentModels();
}

void InferenceSettingsManager::load()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    loadGenerationParams();
    loadRecentModels();
}

void InferenceSettingsManager::loadGenerationParams()
{
    // Defaults for Verification Mode without Qt
    m_temperature = 0.7;
    m_topP = 0.9;
    m_topK = 40;
    m_maxTokens = 2048;
    m_repetitionPenalty = 1.1;
    m_ollamaModelTag = "llama2";
    m_useOllama = false;
    m_currentModelPath = "";
    m_currentPreset = Balanced;
}

void InferenceSettingsManager::saveGenerationParams()
{
    // No-op for Verification
}

void InferenceSettingsManager::loadRecentModels()
{
    // No-op
}

void InferenceSettingsManager::saveRecentModels()
{
    // No-op
}

void* InferenceSettingsManager::exportToJSON() const
{
    // Stub
    return nullptr;
}

void InferenceSettingsManager::importFromJSON(const void*& json)
{
    // Stub
}

void InferenceSettingsManager::settingsChanged() {}
void InferenceSettingsManager::presetChanged(Preset preset) {}
void InferenceSettingsManager::modelPathChanged(const std::string& modelPath) {}
void InferenceSettingsManager::recentModelsUpdated() {}

