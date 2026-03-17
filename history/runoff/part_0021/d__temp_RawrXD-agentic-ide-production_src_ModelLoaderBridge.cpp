#include "ModelLoaderBridge.h"
#include <QFileInfo>
#include <QDebug>
#include <QtConcurrent>

ModelLoaderBridge::ModelLoaderBridge(QObject* parent) 
    : QObject(parent) {
}

ModelLoaderBridge::~ModelLoaderBridge() {
    shutdown();
}

bool ModelLoaderBridge::initialize(size_t ramGB, size_t vramMB) {
    QMutexLocker lock(&m_mutex);
    
    if (m_initialized) return true;
    
    if (sovereign_loader_init(ramGB, vramMB) != 0) {
        qCritical() << "Failed to initialize Sovereign Loader";
        return false;
    }
    
    m_initialized = true;
    qInfo() << "ModelLoaderBridge: Initialized with" << ramGB << "GB RAM," << vramMB << "MB VRAM";
    return true;
}

void ModelLoaderBridge::shutdown() {
    QMutexLocker lock(&m_mutex);
    
    if (!m_initialized) return;
    
    // Unload all models
    for (const auto& model : m_loadedModels) {
        if (model.isLoaded && model.handle) {
            sovereign_loader_unload_model(model.handle);
        }
    }
    m_loadedModels.clear();
    
    sovereign_loader_shutdown();
    m_initialized = false;
}

void ModelLoaderBridge::loadModelAsync(const QString& modelPath) {
    if (!m_initialized) {
        emit modelLoadFailed(modelPath, "Loader not initialized");
        return;
    }
    
    // Run in thread pool to avoid UI blocking
    QtConcurrent::run([this, modelPath]() {
        loadModelWorker(modelPath);
    });
}

void ModelLoaderBridge::loadModelWorker(const QString& path) {
    QFileInfo info(path);
    if (!info.exists()) {
        emit modelLoadFailed(path, "File does not exist");
        return;
    }
    
    uint64_t size = 0;
    void* handle = sovereign_loader_load_model(path.toStdString().c_str(), &size);
    
    {
        QMutexLocker lock(&m_mutex);
        
        if (handle) {
            ModelInfo mi;
            mi.name = info.fileName();
            mi.path = path;
            mi.sizeBytes = size;
            mi.format = "GGUF";
            mi.isLoaded = true;
            mi.handle = handle;
            
            m_loadedModels.push_back(mi);
            
            emit modelLoaded(mi.name, mi.sizeBytes);
            qInfo() << "Loaded model:" << mi.name << "(" << size << "bytes)";
        } else {
            emit modelLoadFailed(path, "Failed to load via Sovereign Loader");
            qCritical() << "Failed to load model:" << path;
        }
    }
    
    emit metricsUpdated(getMetrics());
}

void ModelLoaderBridge::unloadModel(const QString& modelName) {
    QMutexLocker lock(&m_mutex);
    
    for (auto it = m_loadedModels.begin(); it != m_loadedModels.end(); ++it) {
        if (it->name == modelName && it->isLoaded) {
            sovereign_loader_unload_model(it->handle);
            m_loadedModels.erase(it);
            emit modelUnloaded(modelName);
            qInfo() << "Unloaded model:" << modelName;
            break;
        }
    }
}

QVector<ModelInfo> ModelLoaderBridge::getLoadedModels() const {
    QMutexLocker lock(&m_mutex);
    return m_loadedModels;
}

QVector<ModelInfo> ModelLoaderBridge::getAvailableModels() const {
    // In production, scan model directories (Ollama, HF, local)
    // For now, return loaded models
    return getLoadedModels();
}

LoaderMetrics ModelLoaderBridge::getMetrics() const {
    QMutexLocker lock(&m_mutex);
    LoaderMetrics metrics;
    sovereign_loader_get_metrics(&metrics);
    return metrics;
}
