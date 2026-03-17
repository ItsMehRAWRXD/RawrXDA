/**
 * InferenceSettingsManager - Specialized settings manager for AI inference parameters
 * 
 * Extends SettingsManager to provide type-safe access to inference-specific settings
 * including model selection, generation parameters, and performance options.
 * 
 * Features:
 * - Model path persistence and MRU (Most Recently Used) tracking
 * - Generation parameters (temperature, top_k, top_p, repetition penalty)
 * - Token limits and context window settings
 * - Performance options (batch size, threading, GPU settings)
 * - Presets for common inference scenarios (creative, precise, balanced)
 */

#pragma once

#include "SettingsManager.h"
#include <QString>
#include <QStringList>
#include <vector>
#include <string>

class InferenceSettingsManager
{
public:
    /**
     * Inference generation parameters
     */
    struct GenerationParams {
        float temperature = 0.8f;        // Randomness (0.0-2.0, default 0.8)
        int top_k = 40;                  // Top-K sampling (0-100, default 40)
        float top_p = 0.9f;              // Nucleus sampling (0.0-1.0, default 0.9)
        float repetition_penalty = 1.1f; // Repetition penalty (1.0-2.0, default 1.1)
        int max_tokens = 512;            // Maximum generation tokens
        int context_length = 2048;       // Context window size
        bool stop_at_newline = false;    // Stop generation at newline
        std::string stop_sequence = "";  // Custom stop sequence
    };

    /**
     * Performance and optimization settings
     */
    struct PerformanceSettings {
        int batch_size = 128;            // Batch size for processing
        int thread_count = 0;            // CPU threads (0 = auto)
        bool use_gpu = true;             // Enable GPU acceleration
        bool use_gpu_attention = true;   // GPU for attention computation
        bool use_gpu_matmul = true;      // GPU for matrix multiplication
        bool use_flash_attention = false; // Flash attention optimization
        int gpu_layers = -1;             // GPU offload layers (-1 = all)
        bool use_mmap = true;            // Memory-mapped file access
        bool use_mlock = false;          // Lock model in RAM
    };

    /**
     * Model configuration
     */
    struct ModelConfig {
        std::string model_path;          // Current model path
        std::string model_name;          // Display name
        std::string architecture;        // Model architecture (llama, gpt, etc.)
        int vocab_size = 0;              // Vocabulary size
        int embedding_dim = 0;           // Embedding dimension
        int layer_count = 0;             // Number of layers
        std::string quantization;        // Quantization type (Q4_K_M, Q8_0, etc.)
    };

    /**
     * Preset configurations
     */
    enum class Preset {
        Creative,      // High temperature, diverse outputs
        Balanced,      // Default balanced settings
        Precise,       // Low temperature, deterministic
        Fast,          // Performance-optimized
        Quality,       // Quality-optimized
        Custom         // User-defined
    };

    /**
     * Get singleton instance
     */
    static InferenceSettingsManager& getInstance();

    /**
     * Initialize with base SettingsManager
     */
    void initialize();

    // ========== Generation Parameters ==========

    /**
     * Get current generation parameters
     */
    GenerationParams getGenerationParams() const;

    /**
     * Set generation parameters
     */
    void setGenerationParams(const GenerationParams& params);

    /**
     * Apply preset configuration
     */
    void applyPreset(Preset preset);

    /**
     * Get current preset
     */
    Preset getCurrentPreset() const;

    /**
     * Individual parameter getters
     */
    float getTemperature() const;
    int getTopK() const;
    float getTopP() const;
    float getRepetitionPenalty() const;
    int getMaxTokens() const;
    int getContextLength() const;

    /**
     * Individual parameter setters
     */
    void setTemperature(float value);
    void setTopK(int value);
    void setTopP(float value);
    void setRepetitionPenalty(float value);
    void setMaxTokens(int value);
    void setContextLength(int value);

    // ========== Performance Settings ==========

    /**
     * Get performance settings
     */
    PerformanceSettings getPerformanceSettings() const;

    /**
     * Set performance settings
     */
    void setPerformanceSettings(const PerformanceSettings& settings);

    /**
     * Enable/disable GPU acceleration
     */
    void setGPUEnabled(bool enabled);
    bool isGPUEnabled() const;

    /**
     * Set GPU layer offload count
     */
    void setGPULayers(int layers);
    int getGPULayers() const;

    /**
     * Set batch size
     */
    void setBatchSize(int size);
    int getBatchSize() const;

    // ========== Model Configuration ==========

    /**
     * Get current model configuration
     */
    ModelConfig getCurrentModelConfig() const;

    /**
     * Set current model configuration
     */
    void setCurrentModelConfig(const ModelConfig& config);

    /**
     * Get/set current model path
     */
    std::string getCurrentModelPath() const;
    void setCurrentModelPath(const std::string& path);

    /**
     * Get recently used models (MRU list)
     */
    QStringList getRecentModels() const;

    /**
     * Add model to recent list
     */
    void addRecentModel(const QString& path);

    /**
     * Clear recent models
     */
    void clearRecentModels();

    /**
     * Get last used model path
     */
    QString getLastModelPath() const;

    // ========== Ollama Integration ==========

    /**
     * Get/set Ollama server URL
     */
    std::string getOllamaURL() const;
    void setOllamaURL(const std::string& url);

    /**
     * Get/set Ollama model tag
     */
    std::string getOllamaModelTag() const;
    void setOllamaModelTag(const std::string& tag);

    // ========== Validation ==========

    /**
     * Validate generation parameters
     * Returns error message if invalid, empty string if valid
     */
    std::string validateGenerationParams(const GenerationParams& params) const;

    /**
     * Validate performance settings
     */
    std::string validatePerformanceSettings(const PerformanceSettings& settings) const;

    /**
     * Clamp parameters to valid ranges
     */
    static GenerationParams clampParams(const GenerationParams& params);

    // ========== Presets Data ==========

    /**
     * Get preset display name
     */
    static std::string getPresetName(Preset preset);

    /**
     * Get preset description
     */
    static std::string getPresetDescription(Preset preset);

    /**
     * Get default parameters for preset
     */
    static GenerationParams getPresetParams(Preset preset);

    // ========== Persistence ==========

    /**
     * Save all inference settings
     */
    void save();

    /**
     * Load all inference settings
     */
    void load();

    /**
     * Export settings to JSON
     */
    void exportToJSON(const std::string& filePath);

    /**
     * Import settings from JSON
     */
    void importFromJSON(const std::string& filePath);

    // Delete copy/move
    InferenceSettingsManager(const InferenceSettingsManager&) = delete;
    InferenceSettingsManager& operator=(const InferenceSettingsManager&) = delete;

private:
    InferenceSettingsManager();
    ~InferenceSettingsManager();

    mutable std::mutex m_mutex;
    bool m_initialized = false;
    Preset m_currentPreset = Preset::Balanced;

    // Cached settings
    GenerationParams m_generationParams;
    PerformanceSettings m_performanceSettings;
    ModelConfig m_modelConfig;

    // Helper methods
    void loadGenerationParams();
    void saveGenerationParams();
    void loadPerformanceSettings();
    void savePerformanceSettings();
    void loadModelConfig();
    void saveModelConfig();
};
