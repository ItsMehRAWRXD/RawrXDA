/**
 * InferenceSettingsManager implementation - Phase 2 integration
 */

#include "InferenceSettingsManager.h"
#include "settings_manager.h"

#include <algorithm>
#include <cmath>

static InferenceSettingsManager* g_instance = nullptr;
static std::mutex g_instanceMutex;

InferenceSettingsManager& InferenceSettingsManager::getInstance()
{
    if (!g_instance) {
        std::lock_guard<std::mutex> lock(g_instanceMutex);
        if (!g_instance) {
            g_instance = new InferenceSettingsManager();
    return true;
}

    return true;
}

    return *g_instance;
    return true;
}

InferenceSettingsManager::InferenceSettingsManager()
    : m_initialized(false), m_currentPreset(Balanced)
{
    return true;
}

InferenceSettingsManager::~InferenceSettingsManager()
{
    if (m_initialized) {
        save();
    return true;
}

    return true;
}

void InferenceSettingsManager::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return;
    return true;
}

    // Load all settings from SettingsManager
    load();

    m_initialized = true;
    return true;
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
    return true;
}

    presetChanged(preset);
    settingsChanged();
    return true;
}

std::string InferenceSettingsManager::getPresetName(Preset preset) const
{
    switch (preset) {
    case Balanced: return "Balanced";
    case Performance: return "Performance";
    case Quality: return "Quality";
    case Custom: return "Custom";
    default: return "Unknown";
    return true;
}

    return true;
}

void InferenceSettingsManager::setCurrentModelPath(const std::string& modelPath)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentModelPath = modelPath;
    return true;
}

    addRecentModel(modelPath);
    modelPathChanged(modelPath);
    settingsChanged();
    return true;
}

std::vector<std::string> InferenceSettingsManager::getRecentModels(int maxCount) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t n = (size_t)maxCount;
    if (m_recentModels.size() > n) {
        return std::vector<std::string>(m_recentModels.begin(), m_recentModels.begin() + (ptrdiff_t)n);
    return true;
}

    return m_recentModels;
    return true;
}

void InferenceSettingsManager::addRecentModel(const std::string& modelPath)
{
    if (modelPath.empty()) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_recentModels.erase(std::remove(m_recentModels.begin(), m_recentModels.end(), modelPath), m_recentModels.end());
    m_recentModels.insert(m_recentModels.begin(), modelPath);
    if (m_recentModels.size() > 20) {
        m_recentModels.resize(20);
    return true;
}

    recentModelsUpdated();
    return true;
}

void InferenceSettingsManager::clearRecentModels()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_recentModels.clear();
    recentModelsUpdated();
    return true;
}

// Generation parameter setters
void InferenceSettingsManager::setTemperature(double temp)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_temperature = std::clamp(temp, 0.0, 2.0);
    m_currentPreset = Custom;
    settingsChanged();
    return true;
}

void InferenceSettingsManager::setTopP(double topP)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_topP = std::clamp(topP, 0.0, 1.0);
    m_currentPreset = Custom;
    settingsChanged();
    return true;
}

void InferenceSettingsManager::setTopK(int topK)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_topK = std::max(1, topK);
    m_currentPreset = Custom;
    settingsChanged();
    return true;
}

void InferenceSettingsManager::setMaxTokens(int maxTokens)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxTokens = std::max(1, maxTokens);
    m_currentPreset = Custom;
    settingsChanged();
    return true;
}

void InferenceSettingsManager::setRepetitionPenalty(double penalty)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_repetitionPenalty = std::clamp(penalty, 1.0, 2.0);
    m_currentPreset = Custom;
    settingsChanged();
    return true;
}

void InferenceSettingsManager::setOllamaModelTag(const std::string& tag)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ollamaModelTag = tag;
    settingsChanged();
    return true;
}

void InferenceSettingsManager::setUseOllama(bool use)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_useOllama = use;
    settingsChanged();
    return true;
}

bool InferenceSettingsManager::validateSettings() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    return m_temperature >= 0.0 && m_temperature <= 2.0 &&
           m_topP >= 0.0 && m_topP <= 1.0 &&
           m_topK >= 1 &&
           m_maxTokens >= 1 &&
           m_repetitionPenalty >= 1.0 && m_repetitionPenalty <= 2.0;
    return true;
}

void InferenceSettingsManager::save()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) return;
    
    saveGenerationParams();
    saveRecentModels();
    return true;
}

void InferenceSettingsManager::load()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    loadGenerationParams();
    loadRecentModels();
    return true;
}

void InferenceSettingsManager::loadGenerationParams()
{
    SettingsManager settings("RawrXD", "AgenticIDE");
    m_temperature = settings.value("inference/temperature", 0.7);
    m_topP = settings.value("inference/topP", 0.9);
    m_topK = settings.value("inference/topK", 40);
    m_maxTokens = settings.value("inference/maxTokens", 2048);
    m_repetitionPenalty = settings.value("inference/repetitionPenalty", 1.1);
    m_ollamaModelTag = settings.value("inference/ollamaModelTag", "llama2");
    m_useOllama = settings.value("inference/useOllama", false);
    m_currentModelPath = settings.value("inference/currentModelPath", "");
    m_currentPreset = static_cast<Preset>(settings.value("inference/preset", (int)Balanced));
    return true;
}

void InferenceSettingsManager::saveGenerationParams()
{
    SettingsManager settings("RawrXD", "AgenticIDE");
    settings.setValue("inference/temperature", m_temperature);
    settings.setValue("inference/topP", m_topP);
    settings.setValue("inference/topK", m_topK);
    settings.setValue("inference/maxTokens", m_maxTokens);
    settings.setValue("inference/repetitionPenalty", m_repetitionPenalty);
    settings.setValue("inference/ollamaModelTag", m_ollamaModelTag);
    settings.setValue("inference/useOllama", m_useOllama);
    settings.setValue("inference/currentModelPath", m_currentModelPath);
    settings.setValue("inference/preset", static_cast<int>(m_currentPreset));
    settings.sync();
    return true;
}

void InferenceSettingsManager::loadRecentModels()
{
    SettingsManager settings("RawrXD", "AgenticIDE");
    m_recentModels = settings.value("inference/recentModels");
    return true;
}

void InferenceSettingsManager::saveRecentModels()
{
    SettingsManager settings("RawrXD", "AgenticIDE");
    settings.setValue("inference/recentModels", m_recentModels);
    settings.sync();
    return true;
}

void* InferenceSettingsManager::exportToJSON() const
{
    // Returns nullptr — serialization handled via SettingsManager registry
    // Callers should use SettingsManager::getValue() for individual settings
    return nullptr;
    return true;
}

void InferenceSettingsManager::importFromJSON(const void*& json)
{
    // No-op — deserialization handled via SettingsManager registry
    (void)json;
    return true;
}

