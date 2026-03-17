#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QMutex>
#include <QThread>
#include "sovereign_loader.h"

struct ModelInfo {
    QString name;
    QString path;
    uint64_t sizeBytes;
    QString format; // "GGUF", "ONNX", etc.
    int parameterCount;
    bool isLoaded;
    void* handle;
};

class ModelLoaderBridge : public QObject {
    Q_OBJECT
public:
    explicit ModelLoaderBridge(QObject* parent = nullptr);
    ~ModelLoaderBridge();
    
    bool initialize(size_t ramGB, size_t vramMB);
    void shutdown();
    
    // Async model loading
    void loadModelAsync(const QString& modelPath);
    void unloadModel(const QString& modelName);
    
    // Model cache management
    QVector<ModelInfo> getAvailableModels() const;
    QVector<ModelInfo> getLoadedModels() const;
    
    // Metrics
    LoaderMetrics getMetrics() const;
    
signals:
    void modelLoaded(const QString& name, uint64_t sizeBytes);
    void modelLoadFailed(const QString& name, const QString& error);
    void modelUnloaded(const QString& name);
    void metricsUpdated(const LoaderMetrics& metrics);

private:
    void loadModelWorker(const QString& path);
    
    mutable QMutex m_mutex;
    QVector<ModelInfo> m_loadedModels;
    bool m_initialized = false;
};
