/**
 * InferenceSettingsManager implementation
 */

#include "InferenceSettingsManager.h"
#include "IDELogger.h"
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
        }
    }
    return *g_instance;
}

InferenceSettingsManager::InferenceSettingsManager()
    : m_initialized(false), m_currentPreset(Preset::Balanced)
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
    LOG_INFO("InferenceSettingsManager initialized");
}

// ========== Generation Parameters ==========

InferenceSettingsManager::GenerationParams InferenceSettingsManager::getGenerationParams() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_generationParams;
}

void InferenceSettingsManager::setGenerationParams(const GenerationParams& params)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Validate and clamp
    m_generationParams = clampParams(params);
    m_currentPreset = Preset::Custom;  // Mark as custom when manually set
    
    saveGenerationParams();
    LOG_INFO("Generation parameters updated");
}

void InferenceSettingsManager::applyPreset(Preset preset)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_currentPreset = preset;
    m_generationParams = getPresetParams(preset);
    
    saveGenerationParams();
    
    SettingsManager& settings = SettingsManager::getInstance();
    settings.setInt("inference.preset", static_cast<int>(preset));
    settings.save();
    
    LOG_INFO(std::string("Applied preset: ") + getPresetName(preset));
}

InferenceSettingsManager::Preset InferenceSettingsManager::getCurrentPreset() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentPreset;
}

float InferenceSettingsManager::getTemperature() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_generationParams.temperature;
}

int InferenceSettingsManager::getTopK() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_generationParams.top_k;
}

float InferenceSettingsManager::getTopP() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_generationParams.top_p;
}

float InferenceSettingsManager::getRepetitionPenalty() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_generationParams.repetition_penalty;
}

int InferenceSettingsManager::getMaxTokens() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_generationParams.max_tokens;
}

int InferenceSettingsManager::getContextLength() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_generationParams.context_length;
}

void InferenceSettingsManager::setTemperature(float value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_generationParams.temperature = std::max(0.0f, std::min(2.0f, value));
    m_currentPreset = Preset::Custom;
    saveGenerationParams();
}

void InferenceSettingsManager::setTopK(int value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_generationParams.top_k = std::max(0, std::min(100, value));
    m_currentPreset = Preset::Custom;
    saveGenerationParams();
}

void InferenceSettingsManager::setTopP(float value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_generationParams.top_p = std::max(0.0f, std::min(1.0f, value));
    m_currentPreset = Preset::Custom;
    saveGenerationParams();
}

void InferenceSettingsManager::setRepetitionPenalty(float value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_generationParams.repetition_penalty = std::max(1.0f, std::min(2.0f, value));
    m_currentPreset = Preset::Custom;
    saveGenerationParams();
}

void InferenceSettingsManager::setMaxTokens(int value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_generationParams.max_tokens = std::max(1, std::min(8192, value));
    m_currentPreset = Preset::Custom;
    saveGenerationParams();
}

void InferenceSettingsManager::setContextLength(int value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_generationParams.context_length = std::max(128, std::min(32768, value));
    m_currentPreset = Preset::Custom;
    saveGenerationParams();
}

// ========== Performance Settings ==========

InferenceSettingsManager::PerformanceSettings InferenceSettingsManager::getPerformanceSettings() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_performanceSettings;
}

void InferenceSettingsManager::setPerformanceSettings(const PerformanceSettings& settings)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_performanceSettings = settings;
    savePerformanceSettings();
}

void InferenceSettingsManager::setGPUEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_performanceSettings.use_gpu = enabled;
    savePerformanceSettings();
}

bool InferenceSettingsManager::isGPUEnabled() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_performanceSettings.use_gpu;
}

void InferenceSettingsManager::setGPULayers(int layers)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_performanceSettings.gpu_layers = layers;
    savePerformanceSettings();
}

int InferenceSettingsManager::getGPULayers() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_performanceSettings.gpu_layers;
}

void InferenceSettingsManager::setBatchSize(int size)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_performanceSettings.batch_size = std::max(1, std::min(2048, size));
    savePerformanceSettings();
}

int InferenceSettingsManager::getBatchSize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_performanceSettings.batch_size;
}

// ========== Model Configuration ==========

InferenceSettingsManager::ModelConfig InferenceSettingsManager::getCurrentModelConfig() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_modelConfig;
}

void InferenceSettingsManager::setCurrentModelConfig(const ModelConfig& config)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_modelConfig = config;
    saveModelConfig();
}

std::string InferenceSettingsManager::getCurrentModelPath() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_modelConfig.model_path;
}

void InferenceSettingsManager::setCurrentModelPath(const std::string& path)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_modelConfig.model_path = path;
        saveModelConfig();
    }
    
    // Add to recent models (outside lock to avoid deadlock)
    addRecentModel(QString::fromStdString(path));
}

QStringList InferenceSettingsManager::getRecentModels() const
{
    SettingsManager& settings = SettingsManager::getInstance();
    
    QStringList recent;
    for (int i = 0; i < 10; ++i) {
        std::string key = "inference.recentModel" + std::to_string(i);
        std::string path = settings.getString(key, "");
        if (!path.empty()) {
            recent.append(QString::fromStdString(path));
        }
    }
    
    return recent;
}

void InferenceSettingsManager::addRecentModel(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }

    SettingsManager& settings = SettingsManager::getInstance();
    
    // Get existing recent list
    QStringList recent = getRecentModels();
    
    // Remove duplicates
    recent.removeAll(path);
    
    // Add to front
    recent.prepend(path);
    
    // Limit to 10 entries
    while (recent.size() > 10) {
        recent.removeLast();
    }
    
    // Save back
    for (int i = 0; i < recent.size(); ++i) {
        std::string key = "inference.recentModel" + std::to_string(i);
        settings.setString(key, recent[i].toStdString());
    }
    
    settings.save();
}

void InferenceSettingsManager::clearRecentModels()
{
    SettingsManager& settings = SettingsManager::getInstance();
    
    for (int i = 0; i < 10; ++i) {
        std::string key = "inference.recentModel" + std::to_string(i);
        settings.remove(key);
    }
    
    settings.save();
}

QString InferenceSettingsManager::getLastModelPath() const
{
    QStringList recent = getRecentModels();
    return recent.isEmpty() ? QString() : recent.first();
}

// ========== Ollama Integration ==========

std::string InferenceSettingsManager::getOllamaURL() const
{
    SettingsManager& settings = SettingsManager::getInstance();
    return settings.getString("inference.ollamaUrl", "http://localhost:11434");
}

void InferenceSettingsManager::setOllamaURL(const std::string& url)
{
    SettingsManager& settings = SettingsManager::getInstance();
    settings.setString("inference.ollamaUrl", url);
    settings.save();
}

std::string InferenceSettingsManager::getOllamaModelTag() const
{
    SettingsManager& settings = SettingsManager::getInstance();
    return settings.getString("inference.ollamaModelTag", "");
}

void InferenceSettingsManager::setOllamaModelTag(const std::string& tag)
{
    SettingsManager& settings = SettingsManager::getInstance();
    settings.setString("inference.ollamaModelTag", tag);
    settings.save();
}

// ========== Validation ==========

std::string InferenceSettingsManager::validateGenerationParams(const GenerationParams& params) const
{
    if (params.temperature < 0.0f || params.temperature > 2.0f) {
        return "Temperature must be between 0.0 and 2.0";
    }
    if (params.top_k < 0 || params.top_k > 100) {
        return "Top-K must be between 0 and 100";
    }
    if (params.top_p < 0.0f || params.top_p > 1.0f) {
        return "Top-P must be between 0.0 and 1.0";
    }
    if (params.repetition_penalty < 1.0f || params.repetition_penalty > 2.0f) {
        return "Repetition penalty must be between 1.0 and 2.0";
    }
    if (params.max_tokens < 1 || params.max_tokens > 8192) {
        return "Max tokens must be between 1 and 8192";
    }
    if (params.context_length < 128 || params.context_length > 32768) {
        return "Context length must be between 128 and 32768";
    }
    return "";  // Valid
}

std::string InferenceSettingsManager::validatePerformanceSettings(const PerformanceSettings& settings) const
{
    if (settings.batch_size < 1 || settings.batch_size > 2048) {
        return "Batch size must be between 1 and 2048";
    }
    if (settings.thread_count < 0 || settings.thread_count > 128) {
        return "Thread count must be between 0 (auto) and 128";
    }
    if (settings.gpu_layers < -1) {
        return "GPU layers must be -1 (all) or positive";
    }
    return "";  // Valid
}

InferenceSettingsManager::GenerationParams InferenceSettingsManager::clampParams(const GenerationParams& params)
{
    GenerationParams clamped = params;
    clamped.temperature = std::max(0.0f, std::min(2.0f, params.temperature));
    clamped.top_k = std::max(0, std::min(100, params.top_k));
    clamped.top_p = std::max(0.0f, std::min(1.0f, params.top_p));
    clamped.repetition_penalty = std::max(1.0f, std::min(2.0f, params.repetition_penalty));
    clamped.max_tokens = std::max(1, std::min(8192, params.max_tokens));
    clamped.context_length = std::max(128, std::min(32768, params.context_length));
    return clamped;
}

// ========== Presets ==========

std::string InferenceSettingsManager::getPresetName(Preset preset)
{
    switch (preset) {
        case Preset::Creative: return "Creative";
        case Preset::Balanced: return "Balanced";
        case Preset::Precise: return "Precise";
        case Preset::Fast: return "Fast";
        case Preset::Quality: return "Quality";
        case Preset::Custom: return "Custom";
        default: return "Unknown";
    }
}

std::string InferenceSettingsManager::getPresetDescription(Preset preset)
{
    switch (preset) {
        case Preset::Creative:
            return "High temperature, diverse and creative outputs";
        case Preset::Balanced:
            return "Balanced settings for general use";
        case Preset::Precise:
            return "Low temperature, deterministic and focused outputs";
        case Preset::Fast:
            return "Optimized for speed with smaller context";
        case Preset::Quality:
            return "Optimized for quality with larger context";
        case Preset::Custom:
            return "User-customized parameters";
        default:
            return "";
    }
}

InferenceSettingsManager::GenerationParams InferenceSettingsManager::getPresetParams(Preset preset)
{
    GenerationParams params;
    
    switch (preset) {
        case Preset::Creative:
            params.temperature = 1.2f;
            params.top_k = 60;
            params.top_p = 0.95f;
            params.repetition_penalty = 1.05f;
            params.max_tokens = 1024;
            params.context_length = 2048;
            break;
            
        case Preset::Balanced:
            params.temperature = 0.8f;
            params.top_k = 40;
            params.top_p = 0.9f;
            params.repetition_penalty = 1.1f;
            params.max_tokens = 512;
            params.context_length = 2048;
            break;
            
        case Preset::Precise:
            params.temperature = 0.3f;
            params.top_k = 20;
            params.top_p = 0.7f;
            params.repetition_penalty = 1.2f;
            params.max_tokens = 256;
            params.context_length = 2048;
            break;
            
        case Preset::Fast:
            params.temperature = 0.7f;
            params.top_k = 30;
            params.top_p = 0.85f;
            params.repetition_penalty = 1.1f;
            params.max_tokens = 256;
            params.context_length = 1024;
            break;
            
        case Preset::Quality:
            params.temperature = 0.8f;
            params.top_k = 50;
            params.top_p = 0.92f;
            params.repetition_penalty = 1.15f;
            params.max_tokens = 2048;
            params.context_length = 4096;
            break;
            
        case Preset::Custom:
        default:
            // Return default balanced params
            params.temperature = 0.8f;
            params.top_k = 40;
            params.top_p = 0.9f;
            params.repetition_penalty = 1.1f;
            params.max_tokens = 512;
            params.context_length = 2048;
            break;
    }
    
    return params;
}

// ========== Persistence ==========

void InferenceSettingsManager::save()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    saveGenerationParams();
    savePerformanceSettings();
    saveModelConfig();
}

void InferenceSettingsManager::load()
{
    loadGenerationParams();
    loadPerformanceSettings();
    loadModelConfig();
}

void InferenceSettingsManager::loadGenerationParams()
{
    SettingsManager& settings = SettingsManager::getInstance();
    
    m_generationParams.temperature = static_cast<float>(settings.getDouble("inference.temperature", 0.8));
    m_generationParams.top_k = settings.getInt("inference.topK", 40);
    m_generationParams.top_p = static_cast<float>(settings.getDouble("inference.topP", 0.9));
    m_generationParams.repetition_penalty = static_cast<float>(settings.getDouble("inference.repetitionPenalty", 1.1));
    m_generationParams.max_tokens = settings.getInt("inference.maxTokens", 512);
    m_generationParams.context_length = settings.getInt("inference.contextLength", 2048);
    m_generationParams.stop_at_newline = settings.getBool("inference.stopAtNewline", false);
    m_generationParams.stop_sequence = settings.getString("inference.stopSequence", "");
    
    int presetInt = settings.getInt("inference.preset", static_cast<int>(Preset::Balanced));
    m_currentPreset = static_cast<Preset>(presetInt);
}

void InferenceSettingsManager::saveGenerationParams()
{
    SettingsManager& settings = SettingsManager::getInstance();
    
    settings.setDouble("inference.temperature", m_generationParams.temperature);
    settings.setInt("inference.topK", m_generationParams.top_k);
    settings.setDouble("inference.topP", m_generationParams.top_p);
    settings.setDouble("inference.repetitionPenalty", m_generationParams.repetition_penalty);
    settings.setInt("inference.maxTokens", m_generationParams.max_tokens);
    settings.setInt("inference.contextLength", m_generationParams.context_length);
    settings.setBool("inference.stopAtNewline", m_generationParams.stop_at_newline);
    settings.setString("inference.stopSequence", m_generationParams.stop_sequence);
    settings.setInt("inference.preset", static_cast<int>(m_currentPreset));
    
    settings.save();
}

void InferenceSettingsManager::loadPerformanceSettings()
{
    SettingsManager& settings = SettingsManager::getInstance();
    
    m_performanceSettings.batch_size = settings.getInt("performance.batchSize", 128);
    m_performanceSettings.thread_count = settings.getInt("performance.threadCount", 0);
    m_performanceSettings.use_gpu = settings.getBool("performance.useGPU", true);
    m_performanceSettings.use_gpu_attention = settings.getBool("performance.useGPUAttention", true);
    m_performanceSettings.use_gpu_matmul = settings.getBool("performance.useGPUMatmul", true);
    m_performanceSettings.use_flash_attention = settings.getBool("performance.useFlashAttention", false);
    m_performanceSettings.gpu_layers = settings.getInt("performance.gpuLayers", -1);
    m_performanceSettings.use_mmap = settings.getBool("performance.useMmap", true);
    m_performanceSettings.use_mlock = settings.getBool("performance.useMlock", false);
}

void InferenceSettingsManager::savePerformanceSettings()
{
    SettingsManager& settings = SettingsManager::getInstance();
    
    settings.setInt("performance.batchSize", m_performanceSettings.batch_size);
    settings.setInt("performance.threadCount", m_performanceSettings.thread_count);
    settings.setBool("performance.useGPU", m_performanceSettings.use_gpu);
    settings.setBool("performance.useGPUAttention", m_performanceSettings.use_gpu_attention);
    settings.setBool("performance.useGPUMatmul", m_performanceSettings.use_gpu_matmul);
    settings.setBool("performance.useFlashAttention", m_performanceSettings.use_flash_attention);
    settings.setInt("performance.gpuLayers", m_performanceSettings.gpu_layers);
    settings.setBool("performance.useMmap", m_performanceSettings.use_mmap);
    settings.setBool("performance.useMlock", m_performanceSettings.use_mlock);
    
    settings.save();
}

void InferenceSettingsManager::loadModelConfig()
{
    SettingsManager& settings = SettingsManager::getInstance();
    
    m_modelConfig.model_path = settings.getString("model.path", "");
    m_modelConfig.model_name = settings.getString("model.name", "");
    m_modelConfig.architecture = settings.getString("model.architecture", "");
    m_modelConfig.vocab_size = settings.getInt("model.vocabSize", 0);
    m_modelConfig.embedding_dim = settings.getInt("model.embeddingDim", 0);
    m_modelConfig.layer_count = settings.getInt("model.layerCount", 0);
    m_modelConfig.quantization = settings.getString("model.quantization", "");
}

void InferenceSettingsManager::saveModelConfig()
{
    SettingsManager& settings = SettingsManager::getInstance();
    
    settings.setString("model.path", m_modelConfig.model_path);
    settings.setString("model.name", m_modelConfig.model_name);
    settings.setString("model.architecture", m_modelConfig.architecture);
    settings.setInt("model.vocabSize", m_modelConfig.vocab_size);
    settings.setInt("model.embeddingDim", m_modelConfig.embedding_dim);
    settings.setInt("model.layerCount", m_modelConfig.layer_count);
    settings.setString("model.quantization", m_modelConfig.quantization);
    
    settings.save();
}

void InferenceSettingsManager::exportToJSON(const std::string& filePath)
{
    // Delegate to base SettingsManager
    SettingsManager& settings = SettingsManager::getInstance();
    settings.exportToJSON(filePath);
}

void InferenceSettingsManager::importFromJSON(const std::string& filePath)
{
    // Delegate to base SettingsManager
    SettingsManager& settings = SettingsManager::getInstance();
    settings.importFromJSON(filePath);
    
    // Reload our cached settings
    load();
}
