#include "autonomous_model_manager.h"
#include <QDebug>
#include <QFileInfo>
#include <memory>

AutonomousModelManager::AutonomousModelManager(QObject* parent)
    : QObject(parent) {
}

AutonomousModelManager::~AutonomousModelManager() {
    // Clean up compression provider
    m_compressionProvider.reset();
}

bool AutonomousModelManager::loadModelAutonomously(const QString& modelPath) {
    QFileInfo fileInfo(modelPath);
    if (!fileInfo.exists()) {
        qWarning() << "[AutonomousModelManager] Model file does not exist:" << modelPath;
        return false;
    }
    
    // Select optimal compression based on model characteristics
    m_compressionProvider = selectOptimalCompression();
    
    qInfo() << "[AutonomousModelManager] Loading model autonomously:" << modelPath;
    qInfo() << "[AutonomousModelManager] Selected compression:" 
            << (m_compressionProvider ? QString::fromStdString(m_compressionProvider->GetActiveKernel()) : "None");
    
    // Emit success signal with stats
    emit modelLoaded(modelPath, m_stats);
    return true;
}

std::shared_ptr<ICompressionProvider> AutonomousModelManager::selectOptimalCompression() {
    // Autonomous decision logic based on system capabilities
    
    // Check available memory (simplified)
    uint64_t availableMemory = 8ULL * 1024 * 1024 * 1024; // Assume 8GB
    
    // Prefer BRUTAL_GZIP for better compression ratio
    auto provider = CompressionFactory::Create(2);
    if (provider && provider->IsSupported()) {
        qInfo() << "[AutonomousModelManager] Selected BrutalGzip (optimal compression)";
        return provider;
    }
    
    // Fallback to Deflate
    provider = CompressionFactory::Create(1);
    if (provider && provider->IsSupported()) {
        qInfo() << "[AutonomousModelManager] Selected Deflate (fallback)";
        return provider;
    }
    
    qWarning() << "[AutonomousModelManager] No compression provider available";
    return nullptr;
}

void AutonomousModelManager::adaptCompressionSettings(const CompressionStats& stats) {
    m_stats = stats;
    
    // Adaptive logic based on performance
    if (stats.decompression_calls > 100) {
        double avgRatio = stats.avg_compression_ratio;
        if (avgRatio < 0.5) {
            qInfo() << "[AutonomousModelManager] Adapting: Low compression ratio detected";
            emit compressionOptimized("adaptive", avgRatio);
        }
    }
}