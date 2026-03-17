#include "autonomous_model_manager.h"
#include "model_loader/ModelLoader.hpp"
#include <QFileInfo>
#include <QElapsedTimer>
#include <QDebug>
#include <QDir>

AutonomousModelManager::AutonomousModelManager(QObject* parent)
    : QObject(parent)
    , resource_manager_(nullptr)
    , learning_system_(nullptr)
    , model_loaded_(false)
{
    compression_stats_.Reset();
}

AutonomousModelManager::~AutonomousModelManager()
{
    unloadModel();
}

void AutonomousModelManager::initialize(
    AutonomousResourceManager* resourceManager,
    AgenticLearningSystem* learningSystem)
{
    resource_manager_ = resourceManager;
    learning_system_ = learningSystem;
    
    if (!model_loader_) {
        model_loader_ = std::make_unique<ModelLoader>();
    }
    
    qInfo() << "[AutonomousModelManager] Initialized with resource manager and learning system";
}

bool AutonomousModelManager::loadModelAutonomously(const QString& modelPath, bool forceReload)
{
    if (model_loaded_ && !forceReload && loaded_model_path_ == modelPath) {
        qInfo() << "[AutonomousModelManager] Model already loaded:" << modelPath;
        return true;
    }

    emit loadingProgress(0.0, "Analyzing model and system resources...");

    // Step 1: Check if model can be loaded with current resources
    if (resource_manager_) {
        AutonomousResourceManager::SystemResources resources = resource_manager_->getCurrentResources();
        if (!resource_manager_->canLoadModel(modelPath, resources)) {
            QString error = "Insufficient resources to load model";
            emit modelLoadFailed(modelPath, error);
            return false;
        }
    }

    emit loadingProgress(0.1, "Selecting optimal compression...");

    // Step 2: Select optimal compression
    compression_provider_ = selectOptimalCompression();
    if (!compression_provider_) {
        qWarning() << "[AutonomousModelManager] No compression provider available, proceeding without compression";
    } else {
        QString method = analyzeModelForCompression(modelPath);
        applyCompressionSettings(compression_provider_, method);
        qInfo() << "[AutonomousModelManager] Selected compression:" << method;
    }

    emit loadingProgress(0.2, "Loading model...");

    // Step 3: Load model using ModelLoader
    QElapsedTimer timer;
    timer.start();

    if (!model_loader_) {
        model_loader_ = std::make_unique<ModelLoader>();
    }

    std::string modelPathStd = modelPath.toStdString();
    bool success = model_loader_->loadModel(modelPathStd, true); // Use streaming

    qint64 loadTime = timer.elapsed();

    if (!success) {
        QString error = "Failed to load model file";
        emit modelLoadFailed(modelPath, error);
        return false;
    }
    
    emit loadingProgress(0.8, "Finalizing...");

    // Step 4: Record performance for learning
    if (learning_system_ && compression_provider_) {
        QFileInfo fileInfo(modelPath);
        qint64 fileSize = fileInfo.size();
        
        // Estimate compression performance (we don't have exact numbers here)
        // In real implementation, this would come from actual decompression stats
        recordPerformance(
            compression_provider_->IsSupported() ? "brutal_gzip" : "deflate",
            fileSize,
            fileSize, // Assume no compression for now (would be actual decompressed size)
            loadTime
        );
    }

    // Step 5: Update state
    loaded_model_path_ = modelPath;
    model_loaded_ = true;

    emit loadingProgress(1.0, "Model loaded successfully");
    emit modelLoaded(modelPath, compression_stats_);

    qInfo() << "[AutonomousModelManager] Model loaded successfully:" << modelPath 
            << "in" << loadTime << "ms";
    
    return true;
}

std::shared_ptr<ICompressionProvider> AutonomousModelManager::selectOptimalCompression()
{
    if (!resource_manager_ || !learning_system_) {
        // Fallback: just create default provider
        return CompressionFactory::Create(2); // Prefer BRUTAL_GZIP
    }

    AutonomousResourceManager::SystemResources resources = resource_manager_->getCurrentResources();

    // Decision logic:
    // 1. Check if compression should be used at all
    if (!resource_manager_->shouldUseCompression(resources)) {
        qInfo() << "[AutonomousModelManager] Compression disabled due to resource constraints";
        return nullptr;
    }

    // 2. Use learning system to predict optimal method
    // Estimate data size (would be actual model size in real scenario)
    size_t estimatedSize = 100 * 1024 * 1024; // 100MB default
    QString predictedMethod = learning_system_->predictOptimalCompression(estimatedSize, "model");

    // 3. Create provider based on prediction
    uint32_t preferType = (predictedMethod == "brutal_gzip") ? 2 : 1;
    auto provider = CompressionFactory::Create(preferType);

    if (provider && provider->IsSupported()) {
        // Apply optimal settings
        uint32_t compressionLevel = resource_manager_->getRecommendedCompressionLevel(resources);
        if (auto deflate = std::dynamic_pointer_cast<DeflateWrapper>(provider)) {
            deflate->SetCompressionLevel(compressionLevel);
        }

        uint32_t threadCount = resource_manager_->getOptimalThreadCount(resources);
        if (auto gzip = std::dynamic_pointer_cast<BrutalGzipWrapper>(provider)) {
            gzip->SetThreadCount(threadCount);
        }

        qInfo() << "[AutonomousModelManager] Selected compression:" << predictedMethod
                << "level:" << compressionLevel << "threads:" << threadCount;
        
        return provider;
    }

    // Fallback
    qWarning() << "[AutonomousModelManager] Predicted method not available, using fallback";
    return CompressionFactory::Create(1); // DEFLATE fallback
}

void AutonomousModelManager::adaptCompressionSettings(const CompressionStats& stats)
{
    if (!learning_system_ || !compression_provider_) {
        return;
    }

    // Analyze stats and adapt
    double avgRatio = 0.0;
    if (stats.compression_calls > 0 && stats.total_compressed_bytes > 0) {
        avgRatio = 1.0 - ((double)stats.total_decompressed_bytes / (double)stats.total_compressed_bytes);
    }

    // Record for learning
    if (stats.compression_calls > 0) {
        QString method = compression_provider_->IsSupported() ? "brutal_gzip" : "deflate";
        
        // Estimate performance (would use actual timing in real implementation)
        learning_system_->recordCompressionPerformance(
            method,
            stats.total_compressed_bytes,
            stats.total_decompressed_bytes,
            0, // Would be actual time
            "model"
        );
    }

    // Update compression stats
    compression_stats_ = stats;

    qInfo() << "[AutonomousModelManager] Adapted compression settings, avg ratio:" 
            << QString::number(avgRatio, 'f', 3);
}

std::shared_ptr<ICompressionProvider> AutonomousModelManager::getCurrentCompressionProvider() const
{
    return compression_provider_;
}

bool AutonomousModelManager::isModelLoaded() const
{
    return model_loaded_;
}

QString AutonomousModelManager::getLoadedModelPath() const
{
    return loaded_model_path_;
}

void AutonomousModelManager::unloadModel()
{
    if (model_loader_) {
        model_loader_->closeModel();
    }
    
    model_loaded_ = false;
    loaded_model_path_.clear();
    compression_provider_.reset();
    
    qInfo() << "[AutonomousModelManager] Model unloaded";
}

CompressionStats AutonomousModelManager::getCompressionStats() const
{
    return compression_stats_;
}

QStringList AutonomousModelManager::getAvailableModels() const
{
    QStringList models;
    
    // Search common model directories
    QStringList searchPaths = {
        QDir::homePath() + "/OllamaModels",
        QDir::homePath() + "/.ollama/models",
        QDir::currentPath() + "/models",
        "D:/OllamaModels"  // Default path from Win32IDE
    };
    
    for (const QString& path : searchPaths) {
        QDir dir(path);
        if (dir.exists()) {
            QStringList filters = {"*.gguf", "*.bin", "*.safetensors"};
            QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
            for (const QFileInfo& file : files) {
                if (!models.contains(file.absoluteFilePath())) {
                    models.append(file.absoluteFilePath());
                }
            }
        }
    }
    
    return models;
}

QString AutonomousModelManager::analyzeModelForCompression(const QString& modelPath) const
{
    QFileInfo fileInfo(modelPath);
    qint64 fileSize = fileInfo.size();

    // Simple heuristic:
    // - Small models (< 1GB): Fast compression
    // - Medium models (1-10GB): Balanced
    // - Large models (> 10GB): Best compression

    if (fileSize < 1024ULL * 1024 * 1024) {
        return "deflate"; // Fast for small models
    } else if (fileSize < 10ULL * 1024 * 1024 * 1024) {
        return "brutal_gzip"; // Balanced for medium models
    } else {
        return "brutal_gzip"; // Best for large models
    }
}

void AutonomousModelManager::applyCompressionSettings(
    std::shared_ptr<ICompressionProvider> provider,
    const QString& method)
{
    if (!provider || !resource_manager_) {
        return;
    }

    AutonomousResourceManager::SystemResources resources = resource_manager_->getCurrentResources();
    uint32_t level = resource_manager_->getRecommendedCompressionLevel(resources);
    uint32_t threads = resource_manager_->getOptimalThreadCount(resources);

    if (auto deflate = std::dynamic_pointer_cast<DeflateWrapper>(provider)) {
        deflate->SetCompressionLevel(level);
        qDebug() << "[AutonomousModelManager] Set compression level:" << level;
    }

    if (auto gzip = std::dynamic_pointer_cast<BrutalGzipWrapper>(provider)) {
        gzip->SetThreadCount(threads);
        qDebug() << "[AutonomousModelManager] Set thread count:" << threads;
    }
}

void AutonomousModelManager::recordPerformance(
    const QString& method,
    size_t inputSize,
    size_t outputSize,
    qint64 timeMs)
{
    if (learning_system_) {
        learning_system_->recordCompressionPerformance(method, inputSize, outputSize, timeMs, "model");
    }
}

void AutonomousModelManager::setModelMode(ModelMode mode)
{
    m_currentMode = mode;
    
    // Update individual mode flags based on selected mode
    m_searchEnabled = (mode == ModelMode::Search || mode == ModelMode::DeepResearch);
    m_deepResearchEnabled = (mode == ModelMode::DeepResearch);
    m_thinkingModeEnabled = (mode == ModelMode::Thinking || mode == ModelMode::Max);
    
    qInfo() << "[AutonomousModelManager] Model mode set to:" << static_cast<int>(mode);
}

ModelMode AutonomousModelManager::getModelMode() const
{
    return m_currentMode;
}

void AutonomousModelManager::setSearchEnabled(bool enabled)
{
    m_searchEnabled = enabled;
    if (enabled && m_currentMode == ModelMode::Standard) {
        m_currentMode = ModelMode::Search;
    }
}

void AutonomousModelManager::setDeepResearchEnabled(bool enabled)
{
    m_deepResearchEnabled = enabled;
    if (enabled) {
        m_searchEnabled = true; // Deep research requires search
        m_currentMode = ModelMode::DeepResearch;
    }
}

void AutonomousModelManager::setThinkingModeEnabled(bool enabled)
{
    m_thinkingModeEnabled = enabled;
    if (enabled && m_currentMode == ModelMode::Standard) {
        m_currentMode = ModelMode::Thinking;
    }
}

QMap<QString, bool> AutonomousModelManager::getModelCapabilities() const
{
    QMap<QString, bool> capabilities;
    capabilities["search"] = m_searchEnabled;
    capabilities["deep_research"] = m_deepResearchEnabled;
    capabilities["thinking"] = m_thinkingModeEnabled;
    capabilities["max_mode"] = (m_currentMode == ModelMode::Max);
    capabilities["standard"] = (m_currentMode == ModelMode::Standard);
    return capabilities;
}
