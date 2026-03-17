#pragma once
#include <QObject>
#include <QByteArray>
#include <QString>
#include <QMutex>
#include "gguf_loader.hpp"

/**
 * @brief Inference engine for GGUF models with brutal_gzip compression support
 * 
 * This class runs in a worker thread and handles model loading, tensor decompression,
 * and inference requests. It integrates with the existing brutal_gzip MASM/NEON
 * deflate implementation for fast compression/decompression.
 */
class InferenceEngine : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Construct an inference engine
     * @param ggufPath Path to the GGUF model file (can be empty, loaded later)
     * @param parent QObject parent
     */
    explicit InferenceEngine(const QString& ggufPath = QString(), QObject* parent = nullptr);
    
    /**
     * @brief Load a GGUF model file
     * @param path Path to the GGUF file
     * @return true if loaded successfully
     */
    bool loadModel(const QString& path);
    
    /**
     * @brief Check if a model is currently loaded
     */
    bool isModelLoaded() const;
    
    /**
     * @brief Get the current model path
     */
    QString modelPath() const;

public slots:
    /**
     * @brief Process an inference request
     * @param prompt User input text
     * @param reqId Request ID for correlation
     */
    void request(const QString& prompt, qint64 reqId);
    
    /**
     * @brief Unload the current model
     */
    void unloadModel();

signals:
    /**
     * @brief Emitted when inference completes successfully
     * @param reqId Request ID
     * @param answer Generated response
     */
    void resultReady(qint64 reqId, const QString& answer);
    
    /**
     * @brief Emitted when an error occurs
     * @param reqId Request ID
     * @param errorMsg Error description
     */
    void error(qint64 reqId, const QString& errorMsg);
    
    /**
     * @brief Emitted when model loading status changes
     * @param loaded true if model is loaded
     * @param modelName Name of the loaded model
     */
    void modelLoadedChanged(bool loaded, const QString& modelName);
    
    /**
     * @brief Emitted for each token during streaming inference
     * @param reqId Request ID
     * @param token Single token string
     */
    void streamToken(qint64 reqId, const QString& token);
    
    /**
     * @brief Emitted when streaming inference completes
     * @param reqId Request ID
     */
    void streamFinished(qint64 reqId);

private:
    QString m_modelPath;
    GGUFLoader* m_loader;
    mutable QMutex m_mutex;
    
    // Helper to extract model name from path
    QString extractModelName(const QString& path) const;
};
