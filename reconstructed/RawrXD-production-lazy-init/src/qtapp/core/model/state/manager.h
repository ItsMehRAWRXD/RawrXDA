#pragma once

#include <QString>
#include <QObject>
#include <memory>
#include <QJsonObject>

class InferenceEngine;

/**
 * @file model_state_manager.h
 * @brief Centralized model lifecycle management
 * 
 * Provides:
 * - Single model instance across IDE
 * - Model loading/unloading coordination
 * - State change notifications
 * - Model metrics & information
 */

class ModelStateManager : public QObject {
    Q_OBJECT

public:
    enum class ModelState {
        Unloaded,
        Loading,
        Ready,
        Unloading,
        Error
    };

    enum class ErrorType {
        FileNotFound,
        InvalidFormat,
        IncompatibleVersion,
        InsufficientMemory,
        LoadTimeout,
        UnknownError
    };

    struct ModelInfo {
        QString path;
        QString name;
        ModelState state = ModelState::Unloaded;
        qint64 fileSizeBytes = 0;
        int layerCount = 0;
        int vocabSize = 0;
        int embeddingDim = 0;
        int headCount = 0;
        QString format;  // GGUF, GGML, etc.
        QString quantization;  // Q4_0, Q8_0, etc.
        double loadTimeSeconds = 0.0;
    };

    static ModelStateManager& instance();

    /**
     * Load model from file
     */
    bool loadModel(const QString& path);

    /**
     * Unload current model
     */
    void unloadModel();

    /**
     * Check if model is loaded
     */
    bool isModelLoaded() const;

    /**
     * Get current model state
     */
    ModelState getModelState() const;

    /**
     * Get current model information
     */
    const ModelInfo& getModelInfo() const;

    /**
     * Get active inference engine (nullptr if not loaded)
     */
    InferenceEngine* getActiveModel();
    const InferenceEngine* getActiveModel() const;

    /**
     * Get last error message
     */
    QString getLastError() const;

    /**
     * Get last error type
     */
    ErrorType getLastErrorType() const;

    /**
     * Swap model (unload old, load new)
     */
    bool swapModel(const QString& newModelPath);

    /**
     * Preload model in background (for next swap)
     */
    void preloadModel(const QString& path);

    /**
     * Get preloaded model (takes ownership)
     */
    InferenceEngine* takePreloadedModel();

    /**
     * Cancel ongoing operations
     */
    void cancel();

    /**
     * Get model list (recent, cached, etc.)
     */
    QStringList getRecentModels() const;

    /**
     * Add model to recent list
     */
    void addToRecentModels(const QString& path);

    /**
     * Verify model file before loading
     */
    bool verifyModelFile(const QString& path);

    /**
     * Get model compatibility info
     */
    QJsonObject getCompatibilityInfo() const;

signals:
    /**
     * Model loading started
     */
    void modelLoadingStarted(const QString& path);

    /**
     * Model loading progress (0-100)
     */
    void modelLoadingProgress(int percentage);

    /**
     * Model loaded successfully
     */
    void modelLoaded(const ModelInfo& info);

    /**
     * Model loading failed
     */
    void modelLoadingFailed(const QString& error, ErrorType type);

    /**
     * Model unloading started
     */
    void modelUnloadingStarted();

    /**
     * Model unloaded
     */
    void modelUnloaded();

    /**
     * Model state changed
     */
    void modelStateChanged(ModelState newState);

    /**
     * Model info updated
     */
    void modelInfoUpdated(const ModelInfo& info);

private slots:
    void onModelLoadingComplete();
    void onModelLoadingError(const QString& error);

private:
    ModelStateManager();
    ~ModelStateManager() override;

    ModelState m_currentState = ModelState::Unloaded;
    ModelInfo m_modelInfo;
    std::unique_ptr<InferenceEngine> m_activeModel;
    std::unique_ptr<InferenceEngine> m_preloadedModel;
    QString m_lastError;
    ErrorType m_lastErrorType = ErrorType::UnknownError;
    QStringList m_recentModels;
    static constexpr int MAX_RECENT_MODELS = 10;

    void setState(ModelState state);
    void setError(const QString& error, ErrorType type);
};
