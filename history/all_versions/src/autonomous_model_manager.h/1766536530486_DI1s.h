#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <memory>
#include "compression_interface.h"
#include "autonomous_resource_manager.h"
#include "agentic_learning_system.h"

class ModelLoader;
class StreamingGGUFLoader;

/**
 * @enum ModelMode
 * @brief Operating modes for model inference
 */
enum class ModelMode {
    Standard,      // Normal inference
    Search,        // Web search augmented
    DeepResearch,  // Multi-step research with citations
    Thinking,      // Chain-of-thought reasoning
    Max            // Maximum quality (slower, more thorough)
};

/**
 * @class AutonomousModelManager
 * @brief Autonomous model loading with intelligent compression optimization
 * 
 * This manager:
 * - Automatically selects optimal compression based on resources
 * - Monitors compression performance
 * - Learns from past model loads
 * - Adapts compression settings over time
 * - Provides autonomous decision-making for model loading
 */
class AutonomousModelManager : public QObject {
    Q_OBJECT

public:
    explicit AutonomousModelManager(QObject* parent = nullptr);
    ~AutonomousModelManager();

    /**
     * @brief Initialize the manager with required dependencies
     * @param resourceManager Resource manager for system monitoring
     * @param learningSystem Learning system for performance optimization
     */
    void initialize(
        AutonomousResourceManager* resourceManager,
        AgenticLearningSystem* learningSystem
    );

    /**
     * @brief Load model autonomously with optimal compression settings
     * @param modelPath Path to GGUF model file
     * @param forceReload Force reload even if already loaded
     * @return true if loaded successfully
     */
    bool loadModelAutonomously(const QString& modelPath, bool forceReload = false);

    /**
     * @brief Select optimal compression provider based on current conditions
     * @return Shared pointer to compression provider, or nullptr if none available
     */
    std::shared_ptr<ICompressionProvider> selectOptimalCompression();

    /**
     * @brief Adapt compression settings based on performance statistics
     * @param stats Compression statistics from recent operations
     */
    void adaptCompressionSettings(const CompressionStats& stats);

    /**
     * @brief Get current compression provider
     * @return Current compression provider or nullptr
     */
    std::shared_ptr<ICompressionProvider> getCurrentCompressionProvider() const;

    /**
     * @brief Check if a model is currently loaded
     * @return true if model is loaded
     */
    bool isModelLoaded() const;

    /**
     * @brief Get path of currently loaded model
     * @return Model path or empty string
     */
    QString getLoadedModelPath() const;

    /**
     * @brief Unload current model
     */
    void unloadModel();

    /**
     * @brief Get compression statistics for current session
     * @return Compression statistics
     */
    CompressionStats getCompressionStats() const;

    /**
     * @brief Get list of available models
     * @return List of available model paths
     */
    QStringList getAvailableModels() const;

    /**
     * @brief Set model operating mode
     * @param mode Operating mode (Standard, Search, DeepResearch, Thinking, Max)
     */
    void setModelMode(ModelMode mode);

    /**
     * @brief Get current model operating mode
     * @return Current operating mode
     */
    ModelMode getModelMode() const;

    /**
     * @brief Enable search augmentation for queries
     * @param enabled true to enable web search
     */
    void setSearchEnabled(bool enabled);

    /**
     * @brief Enable deep research mode with citations
     * @param enabled true to enable deep research
     */
    void setDeepResearchEnabled(bool enabled);

    /**
     * @brief Enable chain-of-thought thinking mode
     * @param enabled true to enable thinking mode
     */
    void setThinkingModeEnabled(bool enabled);

    /**
     * @brief Get model capabilities based on current mode
     * @return Map of capability name to enabled status
     */
    QMap<QString, bool> getModelCapabilities() const;

signals:
    /**
     * @brief Emitted when model is loaded
     * @param modelPath Path to loaded model
     * @param stats Compression statistics from loading
     */
    void modelLoaded(const QString& modelPath, const CompressionStats& stats);

    /**
     * @brief Emitted when compression is optimized
     * @param method Compression method selected
     * @param ratio Compression ratio achieved
     */
    void compressionOptimized(const QString& method, double ratio);

    /**
     * @brief Emitted when model loading fails
     * @param modelPath Path to model that failed
     * @param error Error message
     */
    void modelLoadFailed(const QString& modelPath, const QString& error);

    /**
     * @brief Emitted during model loading progress
     * @param progress Progress 0.0-1.0
     * @param message Status message
     */
    void loadingProgress(double progress, const QString& message);

    /**
     * @brief Emitted during model download progress
     * @param progress Progress 0.0-1.0
     * @param message Status message
     */
    void downloadProgress(double progress, const QString& message);

    /**
     * @brief Emitted when model download completes
     * @param modelPath Path to downloaded model
     */
    void downloadCompleted(const QString& modelPath);

private:
    /**
     * @brief Analyze model and determine optimal compression strategy
     * @param modelPath Path to model file
     * @return Compression method to use ("brutal_gzip", "deflate", or "none")
     */
    QString analyzeModelForCompression(const QString& modelPath) const;

    /**
     * @brief Apply compression settings to provider
     * @param provider Compression provider
     * @param method Compression method
     */
    void applyCompressionSettings(
        std::shared_ptr<ICompressionProvider> provider,
        const QString& method
    );

    // Model mode state
    ModelMode m_currentMode{ModelMode::Standard};
    bool m_searchEnabled{false};
    bool m_deepResearchEnabled{false};
    bool m_thinkingModeEnabled{false};
    
    /**
     * @brief Record compression performance for learning
     * @param method Compression method used
     * @param inputSize Input size in bytes
     * @param outputSize Output size in bytes
     * @param timeMs Time taken in milliseconds
     */
    void recordPerformance(
        const QString& method,
        size_t inputSize,
        size_t outputSize,
        qint64 timeMs
    );

    AutonomousResourceManager* resource_manager_;
    AgenticLearningSystem* learning_system_;
    std::shared_ptr<ICompressionProvider> compression_provider_;
    std::unique_ptr<ModelLoader> model_loader_;
    CompressionStats compression_stats_;
    QString loaded_model_path_;
    bool model_loaded_;
};
