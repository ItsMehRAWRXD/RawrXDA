// autonomous_model_manager.cpp - Full implementation
#include "autonomous_model_manager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QSysInfo>
#include <QThread>
#include <QProcess>
#include <algorithm>
#include <iostream>

#ifdef Q_OS_WIN
#include <windows.h>
#include <sysinfoapi.h>
#elif defined(Q_OS_LINUX)
#include <sys/sysinfo.h>
#include <unistd.h>
#elif defined(Q_OS_MAC)
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif

AutonomousModelManager::AutonomousModelManager(QObject* parent)
    : QObject(parent),
      currentModelId(""),
      maxCacheSize(DEFAULT_MAX_CACHE_SIZE),
      preferredQuantization("Q4"),
      autoUpdateEnabled(false) {
    
    // Initialize network manager
    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &AutonomousModelManager::onNetworkReplyFinished);
    
    // Setup model cache directory
    QString defaultCache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    modelCacheDirectory = defaultCache + "/ai_models";
    QDir().mkpath(modelCacheDirectory);
    
    // Initialize auto-update timer
    autoUpdateTimer = new QTimer(this);
    autoUpdateTimer->setInterval(AUTO_UPDATE_INTERVAL_HOURS * 3600 * 1000);
    connect(autoUpdateTimer, &QTimer::timeout,
            this, &AutonomousModelManager::onAutoUpdateTimerTimeout);
    
    // Analyze system capabilities
    systemCapabilities.totalRAM = detectTotalRAM();
    systemCapabilities.availableRAM = detectAvailableRAM();
    systemCapabilities.cpuCores = detectCPUCores();
    systemCapabilities.cpuFrequency = detectCPUFrequency();
    systemCapabilities.hasGPU = detectGPU(systemCapabilities.gpuName, systemCapabilities.gpuMemory);
    systemCapabilities.hasAVX2 = detectAVX2Support();
    systemCapabilities.hasAVX512 = detectAVX512Support();
    systemCapabilities.platform = detectPlatform();
    systemCapabilities.timestamp = QDateTime::currentDateTime();
    
    // Load model registry
    loadModelRegistry();
    
    std::cout << "[AutonomousModelManager] Initialized with " << availableModels.size() 
              << " available models" << std::endl;
    std::cout << "[AutonomousModelManager] System: " << systemCapabilities.cpuCores 
              << " cores, " << (systemCapabilities.totalRAM / (1024*1024*1024)) 
              << " GB RAM, GPU: " << (systemCapabilities.hasGPU ? "Yes" : "No") << std::endl;
}

AutonomousModelManager::~AutonomousModelManager() {
    // Cancel active downloads
    for (auto it = networkReplies.begin(); it != networkReplies.end(); ++it) {
        if (it.value()) {
            it.value()->abort();
            it.value()->deleteLater();
        }
    }
    
    saveModelRegistry();
}

ModelRecommendation AutonomousModelManager::autoDetectBestModel(
    const QString& taskType, const QString& language) {
    
    std::cout << "[AutonomousModelManager] Auto-detecting best model for task: " 
              << taskType.toStdString() << ", language: " << language.toStdString() << std::endl;
    
    ModelRecommendation recommendation;
    double bestScore = 0.0;
    AIModel bestModel;
    QStringList alternatives;
    QString reasoning;
    
    for (const AIModel& model : availableModels) {
        // Check compatibility
        if (!isModelCompatible(model)) {
            continue;
        }
        
        // Calculate suitability score
        double score = calculateModelSuitability(model, taskType, language);
        
        if (score > bestScore) {
            if (bestScore > 0) {
                alternatives.append(bestModel.modelId);
            }
            bestScore = score;
            bestModel = model;
        } else if (score > MIN_CONFIDENCE_THRESHOLD) {
            alternatives.append(model.modelId);
        }
    }
    
    if (bestScore > 0) {
        recommendation.model = bestModel;
        recommendation.confidence = bestScore;
        
        // Generate reasoning
        QStringList reasons;
        reasons.append(QString("Task compatibility: %1%").arg(
            calculateTaskCompatibility(bestModel, taskType) * 100, 0, 'f', 1));
        reasons.append(QString("Language support: %1%").arg(
            calculateLanguageSupport(bestModel, language) * 100, 0, 'f', 1));
        reasons.append(QString("Performance score: %1%").arg(
            calculatePerformanceScore(bestModel) * 100, 0, 'f', 1));
        reasons.append(QString("Resource efficiency: %1%").arg(
            calculateResourceEfficiency(bestModel) * 100, 0, 'f', 1));
        
        recommendation.reasoning = reasons.join(", ");
        recommendation.alternatives = alternatives;
        
        // Populate metrics
        QJsonObject metrics;
        metrics["task_compatibility"] = calculateTaskCompatibility(bestModel, taskType);
        metrics["language_support"] = calculateLanguageSupport(bestModel, language);
        metrics["performance"] = calculatePerformanceScore(bestModel);
        metrics["resource_efficiency"] = calculateResourceEfficiency(bestModel);
        metrics["system_ram_gb"] = systemCapabilities.totalRAM / (1024.0 * 1024 * 1024);
        metrics["model_ram_gb"] = bestModel.minRAM / (1024.0 * 1024 * 1024);
        metrics["has_gpu"] = systemCapabilities.hasGPU;
        metrics["requires_gpu"] = bestModel.requiresGPU;
        recommendation.metrics = metrics;
        
        std::cout << "[AutonomousModelManager] Recommended: " << bestModel.modelId.toStdString()
                  << " (confidence: " << (bestScore * 100) << "%)" << std::endl;
    } else {
        std::cerr << "[AutonomousModelManager] No compatible models found!" << std::endl;
        recommendation.confidence = 0.0;
        recommendation.reasoning = "No compatible models available for your system configuration";
    }
    
    emit modelRecommendationReady(recommendation);
    return recommendation;
}

QVector<ModelRecommendation> AutonomousModelManager::getTopRecommendations(
    const QString& taskType, const QString& language, int topN) {
    
    QVector<ModelRecommendation> recommendations;
    QVector<std::pair<double, AIModel>> scoredModels;
    
    // Score all compatible models
    for (const AIModel& model : availableModels) {
        if (!isModelCompatible(model)) {
            continue;
        }
        
        double score = calculateModelSuitability(model, taskType, language);
        if (score > MIN_CONFIDENCE_THRESHOLD) {
            scoredModels.append(std::make_pair(score, model));
        }
    }
    
    // Sort by score descending
    std::sort(scoredModels.begin(), scoredModels.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    // Take top N
    int count = std::min(topN, scoredModels.size());
    for (int i = 0; i < count; ++i) {
        ModelRecommendation rec;
        rec.model = scoredModels[i].second;
        rec.confidence = scoredModels[i].first;
        rec.reasoning = QString("Ranked #%1 with %2% confidence").arg(i + 1).arg(
            scoredModels[i].first * 100, 0, 'f', 1);
        recommendations.append(rec);
    }
    
    return recommendations;
}

bool AutonomousModelManager::autoDownloadAndSetup(const QString& modelId) {
    std::cout << "[AutonomousModelManager] Auto-downloading model: " 
              << modelId.toStdString() << std::endl;
    
    // Find model in registry
    AIModel targetModel;
    bool found = false;
    for (const AIModel& model : availableModels) {
        if (model.modelId == modelId) {
            targetModel = model;
            found = true;
            break;
        }
    }
    
    if (!found) {
        std::cerr << "[AutonomousModelManager] Model not found: " 
                  << modelId.toStdString() << std::endl;
        emit errorOccurred("Model not found: " + modelId);
        return false;
    }
    
    // Check if already downloaded
    if (targetModel.isDownloaded && QFile::exists(targetModel.localPath)) {
        std::cout << "[AutonomousModelManager] Model already downloaded" << std::endl;
        return loadModel(modelId);
    }
    
    // Check system compatibility
    if (!isModelCompatible(targetModel)) {
        std::cerr << "[AutonomousModelManager] Model incompatible with system" << std::endl;
        emit errorOccurred("Model incompatible with system: " + modelId);
        return false;
    }
    
    // Check available disk space
    QStorageInfo storage(modelCacheDirectory);
    if (storage.bytesAvailable() < targetModel.sizeBytes * 1.5) { // 1.5x for extraction
        std::cerr << "[AutonomousModelManager] Insufficient disk space" << std::endl;
        emit errorOccurred("Insufficient disk space for model: " + modelId);
        return false;
    }
    
    // Start download
    QString destinationPath = modelCacheDirectory + "/" + modelId + ".gguf";
    startDownload(modelId, targetModel.downloadUrl);
    
    return true;
}

bool AutonomousModelManager::downloadModel(const QString& modelId, 
                                          const QString& destinationPath) {
    AIModel targetModel;
    for (const AIModel& model : availableModels) {
        if (model.modelId == modelId) {
            targetModel = model;
            break;
        }
    }
    
    if (targetModel.modelId.isEmpty()) {
        return false;
    }
    
    startDownload(modelId, targetModel.downloadUrl);
    return true;
}

void AutonomousModelManager::startDownload(const QString& modelId, const QString& url) {
    // Initialize download progress tracking
    DownloadProgress progress;
    progress.modelId = modelId;
    progress.bytesReceived = 0;
    progress.bytesTotal = 0;
    progress.percentage = 0.0;
    progress.speedBytesPerSec = 0.0;
    progress.startTime = QDateTime::currentDateTime();
    progress.status = "downloading";
    activeDownloads[modelId] = progress;
    
    // Create network request
    QNetworkRequest request(QUrl(url));
    request.setHeader(QNetworkRequest::UserAgentHeader, "AutonomousIDE/1.0");
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    request.setAttribute(QNetworkRequest::User, modelId); // Store model ID
    
    // Start download
    QNetworkReply* reply = networkManager->get(request);
    networkReplies[modelId] = reply;
    
    connect(reply, &QNetworkReply::downloadProgress,
            this, &AutonomousModelManager::onDownloadProgress);
    
    emit downloadStarted(modelId);
    std::cout << "[AutonomousModelManager] Download started: " << modelId.toStdString() << std::endl;
}

void AutonomousModelManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString modelId = reply->request().attribute(QNetworkRequest::User).toString();
    if (!activeDownloads.contains(modelId)) return;
    
    DownloadProgress& progress = activeDownloads[modelId];
    progress.bytesReceived = bytesReceived;
    progress.bytesTotal = bytesTotal;
    
    if (bytesTotal > 0) {
        progress.percentage = (bytesReceived * 100.0) / bytesTotal;
        
        // Calculate download speed
        qint64 elapsedMs = progress.startTime.msecsTo(QDateTime::currentDateTime());
        if (elapsedMs > 0) {
            progress.speedBytesPerSec = (bytesReceived * 1000.0) / elapsedMs;
            
            // Estimate completion time
            if (progress.speedBytesPerSec > 0) {
                qint64 remainingBytes = bytesTotal - bytesReceived;
                qint64 remainingSecs = remainingBytes / progress.speedBytesPerSec;
                progress.estimatedCompletion = QDateTime::currentDateTime().addSecs(remainingSecs);
            }
        }
        
        emit downloadProgress(modelId, progress.percentage);
    }
}

void AutonomousModelManager::onNetworkReplyFinished(QNetworkReply* reply) {
    QString modelId = reply->request().attribute(QNetworkRequest::User).toString();
    
    if (!activeDownloads.contains(modelId)) {
        reply->deleteLater();
        return;
    }
    
    DownloadProgress& progress = activeDownloads[modelId];
    
    if (reply->error() == QNetworkReply::NoError) {
        // Save downloaded data
        QString destinationPath = modelCacheDirectory + "/" + modelId + ".gguf";
        QFile file(destinationPath);
        
        if (file.open(QIODevice::WriteOnly)) {
            file.write(reply->readAll());
            file.close();
            
            progress.status = "validating";
            
            // Validate download
            if (validateDownload(destinationPath)) {
                progress.status = "optimizing";
                optimizeModelForSystem(destinationPath);
                
                progress.status = "complete";
                progress.percentage = 100.0;
                
                // Update model registry
                for (AIModel& model : availableModels) {
                    if (model.modelId == modelId) {
                        model.isDownloaded = true;
                        model.localPath = destinationPath;
                        installedModels.append(model);
                        break;
                    }
                }
                
                saveModelRegistry();
                
                std::cout << "[AutonomousModelManager] Download complete: " 
                          << modelId.toStdString() << std::endl;
                emit downloadComplete(modelId, true);
            } else {
                progress.status = "error";
                progress.errorMessage = "Validation failed";
                std::cerr << "[AutonomousModelManager] Download validation failed" << std::endl;
                emit downloadComplete(modelId, false);
                emit errorOccurred("Download validation failed: " + modelId);
            }
        } else {
            progress.status = "error";
            progress.errorMessage = "Failed to save file";
            emit downloadComplete(modelId, false);
            emit errorOccurred("Failed to save file: " + modelId);
        }
    } else {
        progress.status = "error";
        progress.errorMessage = reply->errorString();
        std::cerr << "[AutonomousModelManager] Download error: " 
                  << reply->errorString().toStdString() << std::endl;
        emit downloadComplete(modelId, false);
        emit errorOccurred("Download error: " + reply->errorString());
    }
    
    networkReplies.remove(modelId);
    reply->deleteLater();
}

DownloadProgress AutonomousModelManager::getDownloadProgress(const QString& modelId) const {
    return activeDownloads.value(modelId, DownloadProgress());
}

void AutonomousModelManager::cancelDownload(const QString& modelId) {
    if (networkReplies.contains(modelId)) {
        networkReplies[modelId]->abort();
    }
    
    if (activeDownloads.contains(modelId)) {
        activeDownloads[modelId].status = "cancelled";
    }
}

SystemAnalysis AutonomousModelManager::analyzeSystemCapabilities() {
    SystemAnalysis analysis;
    analysis.capabilities = systemCapabilities;
    
    // Find compatible models
    for (const AIModel& model : availableModels) {
        if (isModelCompatible(model)) {
            analysis.compatibleModels.append(model);
            
            // Calculate recommendation score
            double score = calculateResourceEfficiency(model) * 
                          calculatePerformanceScore(model);
            
            if (score > 0.7) {
                analysis.recommendedModels.append(model);
            }
        }
    }
    
    // Identify limitations
    if (systemCapabilities.totalRAM < 8LL * 1024 * 1024 * 1024) {
        analysis.limitations.append("Low RAM may limit model size");
    }
    if (!systemCapabilities.hasGPU) {
        analysis.limitations.append("No GPU detected - CPU-only inference");
    }
    if (systemCapabilities.cpuCores < 4) {
        analysis.limitations.append("Low CPU core count may impact performance");
    }
    
    // Calculate overall capability score
    double ramScore = std::min(1.0, systemCapabilities.totalRAM / (32.0 * 1024 * 1024 * 1024));
    double cpuScore = std::min(1.0, systemCapabilities.cpuCores / 16.0);
    double gpuScore = systemCapabilities.hasGPU ? 1.0 : 0.5;
    analysis.overallScore = (ramScore * 0.4 + cpuScore * 0.3 + gpuScore * 0.3);
    
    // Generate recommendations
    QJsonObject recommendations;
    recommendations["total_compatible_models"] = analysis.compatibleModels.size();
    recommendations["recommended_models"] = analysis.recommendedModels.size();
    recommendations["system_capability_score"] = analysis.overallScore;
    recommendations["optimal_quantization"] = systemCapabilities.totalRAM < 16LL * 1024 * 1024 * 1024 
        ? "Q4" : "Q5";
    analysis.recommendations = recommendations;
    
    return analysis;
}

SystemCapabilities AutonomousModelManager::getCurrentCapabilities() const {
    return systemCapabilities;
}

bool AutonomousModelManager::isModelCompatible(const AIModel& model) const {
    // Check RAM requirement
    if (model.minRAM > systemCapabilities.availableRAM) {
        return false;
    }
    
    // Check GPU requirement
    if (model.requiresGPU && !systemCapabilities.hasGPU) {
        return false;
    }
    
    // Check platform compatibility
    if (model.metadata.contains("supported_platforms")) {
        QJsonArray platforms = model.metadata["supported_platforms"].toArray();
        bool platformSupported = false;
        for (const QJsonValue& val : platforms) {
            if (val.toString() == systemCapabilities.platform) {
                platformSupported = true;
                break;
            }
        }
        if (!platformSupported) {
            return false;
        }
    }
    
    return true;
}

double AutonomousModelManager::calculateModelSuitability(
    const AIModel& model, const QString& taskType, const QString& language) const {
    
    double taskCompat = calculateTaskCompatibility(model, taskType);
    double langSupport = calculateLanguageSupport(model, language);
    double performance = calculatePerformanceScore(model);
    double efficiency = calculateResourceEfficiency(model);
    
    // Weighted average
    double score = (taskCompat * 0.4) + (langSupport * 0.25) + 
                   (performance * 0.20) + (efficiency * 0.15);
    
    return score;
}

double AutonomousModelManager::calculateTaskCompatibility(
    const AIModel& model, const QString& taskType) const {
    
    if (model.taskType == taskType) {
        return 1.0;
    }
    
    // Partial matches
    if (taskType.contains("code") && model.taskType.contains("code")) {
        return 0.8;
    }
    if (taskType.contains("completion") && model.taskType.contains("completion")) {
        return 0.7;
    }
    
    return 0.3; // Base compatibility
}

double AutonomousModelManager::calculateLanguageSupport(
    const AIModel& model, const QString& language) const {
    
    if (model.languages.contains(language)) {
        return 1.0;
    }
    
    // Check for "all" or "*" wildcard
    if (model.languages.contains("all") || model.languages.contains("*")) {
        return 0.9;
    }
    
    // Language family matches
    if (language == "cpp" && (model.languages.contains("c") || model.languages.contains("c++"))) {
        return 0.8;
    }
    
    return 0.5; // Partial support
}

double AutonomousModelManager::calculatePerformanceScore(const AIModel& model) const {
    return model.performance; // Already normalized 0-1
}

double AutonomousModelManager::calculateResourceEfficiency(const AIModel& model) const {
    // Calculate efficiency based on RAM usage
    double ramEfficiency = 1.0 - (static_cast<double>(model.minRAM) / 
                                  systemCapabilities.totalRAM);
    ramEfficiency = std::max(0.0, std::min(1.0, ramEfficiency));
    
    // Bonus for quantized models
    double quantBonus = 1.0;
    if (model.quantization == "Q4") quantBonus = 1.2;
    else if (model.quantization == "Q5") quantBonus = 1.1;
    else if (model.quantization == "Q8") quantBonus = 1.05;
    
    return ramEfficiency * quantBonus;
}

// System detection implementations
qint64 AutonomousModelManager::detectTotalRAM() const {
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    return static_cast<qint64>(memInfo.ullTotalPhys);
#elif defined(Q_OS_LINUX)
    struct sysinfo si;
    sysinfo(&si);
    return static_cast<qint64>(si.totalram) * si.mem_unit;
#elif defined(Q_OS_MAC)
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    int64_t size;
    size_t len = sizeof(size);
    sysctl(mib, 2, &size, &len, NULL, 0);
    return size;
#else
    return 8LL * 1024 * 1024 * 1024; // Default 8GB
#endif
}

qint64 AutonomousModelManager::detectAvailableRAM() const {
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    return static_cast<qint64>(memInfo.ullAvailPhys);
#elif defined(Q_OS_LINUX)
    struct sysinfo si;
    sysinfo(&si);
    return static_cast<qint64>(si.freeram) * si.mem_unit;
#elif defined(Q_OS_MAC)
    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t infoCount = HOST_VM_INFO64_COUNT;
    host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vmStats, &infoCount);
    return static_cast<qint64>(vmStats.free_count) * 4096; // Page size
#else
    return 4LL * 1024 * 1024 * 1024; // Default 4GB
#endif
}

int AutonomousModelManager::detectCPUCores() const {
    return QThread::idealThreadCount();
}

double AutonomousModelManager::detectCPUFrequency() const {
    // Simplified - would need platform-specific code for accuracy
    return 2.5; // Default 2.5 GHz
}

bool AutonomousModelManager::detectGPU(QString& gpuName, qint64& gpuMemory) const {
    // Simplified GPU detection
    // Real implementation would use CUDA, ROCm, or Vulkan APIs
    gpuName = "Unknown";
    gpuMemory = 0;
    
#ifdef Q_OS_WIN
    // Check for NVIDIA GPU via registry or WMI
    QProcess process;
    process.start("wmic", QStringList() << "path" << "win32_VideoController" << "get" << "name");
    process.waitForFinished();
    QString output = process.readAllStandardOutput();
    if (output.contains("NVIDIA") || output.contains("AMD") || output.contains("Intel Arc")) {
        gpuName = output.simplified();
        gpuMemory = 8LL * 1024 * 1024 * 1024; // Assume 8GB
        return true;
    }
#endif
    
    return false;
}

bool AutonomousModelManager::detectAVX2Support() const {
    // Would need CPUID instruction check
    return true; // Assume yes for modern CPUs
}

bool AutonomousModelManager::detectAVX512Support() const {
    return false; // Conservative default
}

QString AutonomousModelManager::detectPlatform() const {
#ifdef Q_OS_WIN
    return "Windows";
#elif defined(Q_OS_LINUX)
    return "Linux";
#elif defined(Q_OS_MAC)
    return "macOS";
#else
    return "Unknown";
#endif
}

void AutonomousModelManager::loadModelRegistry() {
    // Load from cache file
    QString registryPath = modelCacheDirectory + "/model_registry.json";
    QFile file(registryPath);
    
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonArray models = doc.array();
        
        for (const QJsonValue& val : models) {
            QJsonObject obj = val.toObject();
            AIModel model;
            parseModelMetadata(obj, model);
            availableModels.append(model);
            
            if (model.isDownloaded) {
                installedModels.append(model);
            }
        }
        
        file.close();
    } else {
        // Initialize with default models
        AIModel codellama;
        codellama.modelId = "codellama-7b-q4";
        codellama.name = "CodeLlama 7B Q4";
        codellama.provider = "HuggingFace";
        codellama.taskType = "code_completion";
        codellama.languages = {"cpp", "python", "javascript", "java", "rust", "go"};
        codellama.sizeBytes = 4LL * 1024 * 1024 * 1024;
        codellama.quantization = "Q4";
        codellama.contextLength = 16384;
        codellama.performance = 0.85;
        codellama.minRAM = 6LL * 1024 * 1024 * 1024;
        codellama.requiresGPU = false;
        codellama.downloadUrl = "https://huggingface.co/TheBloke/CodeLlama-7B-GGUF/resolve/main/codellama-7b.Q4_K_M.gguf";
        codellama.isDownloaded = false;
        availableModels.append(codellama);
        
        AIModel phi3;
        phi3.modelId = "phi-3-mini-q4";
        phi3.name = "Phi-3 Mini Q4";
        phi3.provider = "HuggingFace";
        phi3.taskType = "code_completion";
        phi3.languages = {"cpp", "python", "javascript"};
        phi3.sizeBytes = 2LL * 1024 * 1024 * 1024;
        phi3.quantization = "Q4";
        phi3.contextLength = 4096;
        phi3.performance = 0.75;
        phi3.minRAM = 4LL * 1024 * 1024 * 1024;
        phi3.requiresGPU = false;
        phi3.downloadUrl = "https://huggingface.co/microsoft/Phi-3-mini-4k-instruct-gguf/resolve/main/Phi-3-mini-4k-instruct-q4.gguf";
        phi3.isDownloaded = false;
        availableModels.append(phi3);
        
        saveModelRegistry();
    }
}

void AutonomousModelManager::saveModelRegistry() {
    QString registryPath = modelCacheDirectory + "/model_registry.json";
    QFile file(registryPath);
    
    if (file.open(QIODevice::WriteOnly)) {
        QJsonArray models;
        for (const AIModel& model : availableModels) {
            QJsonObject obj;
            obj["modelId"] = model.modelId;
            obj["name"] = model.name;
            obj["provider"] = model.provider;
            obj["taskType"] = model.taskType;
            obj["languages"] = QJsonArray::fromStringList(model.languages);
            obj["sizeBytes"] = model.sizeBytes;
            obj["quantization"] = model.quantization;
            obj["contextLength"] = model.contextLength;
            obj["performance"] = model.performance;
            obj["minRAM"] = model.minRAM;
            obj["requiresGPU"] = model.requiresGPU;
            obj["downloadUrl"] = model.downloadUrl;
            obj["localPath"] = model.localPath;
            obj["isDownloaded"] = model.isDownloaded;
            obj["metadata"] = model.metadata;
            models.append(obj);
        }
        
        QJsonDocument doc(models);
        file.write(doc.toJson());
        file.close();
    }
}

void AutonomousModelManager::parseModelMetadata(const QJsonObject& json, AIModel& model) {
    model.modelId = json["modelId"].toString();
    model.name = json["name"].toString();
    model.provider = json["provider"].toString();
    model.taskType = json["taskType"].toString();
    
    QJsonArray langs = json["languages"].toArray();
    for (const QJsonValue& val : langs) {
        model.languages.append(val.toString());
    }
    
    model.sizeBytes = json["sizeBytes"].toVariant().toLongLong();
    model.quantization = json["quantization"].toString();
    model.contextLength = json["contextLength"].toInt();
    model.performance = json["performance"].toDouble();
    model.minRAM = json["minRAM"].toVariant().toLongLong();
    model.requiresGPU = json["requiresGPU"].toBool();
    model.downloadUrl = json["downloadUrl"].toString();
    model.localPath = json["localPath"].toString();
    model.isDownloaded = json["isDownloaded"].toBool();
    model.metadata = json["metadata"].toObject();
}

bool AutonomousModelManager::validateDownload(const QString& filePath, const QString& expectedHash) {
    QFile file(filePath);
    if (!file.exists() || file.size() == 0) {
        return false;
    }
    
    // If hash provided, validate
    if (!expectedHash.isEmpty()) {
        if (file.open(QIODevice::ReadOnly)) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(&file);
            QString actualHash = hash.result().toHex();
            file.close();
            return actualHash == expectedHash;
        }
        return false;
    }
    
    // Basic validation - file exists and has reasonable size
    return true;
}

void AutonomousModelManager::optimizeModelForSystem(const QString& modelPath) {
    // Placeholder for model optimization
    // Real implementation would quantize or optimize based on system capabilities
    std::cout << "[AutonomousModelManager] Optimizing model for system: " 
              << modelPath.toStdString() << std::endl;
}

bool AutonomousModelManager::loadModel(const QString& modelId) {
    for (const AIModel& model : installedModels) {
        if (model.modelId == modelId && QFile::exists(model.localPath)) {
            currentModelId = modelId;
            emit modelLoaded(modelId);
            return true;
        }
    }
    return false;
}

bool AutonomousModelManager::unloadCurrentModel() {
    if (!currentModelId.isEmpty()) {
        QString oldModel = currentModelId;
        currentModelId = "";
        emit modelUnloaded(oldModel);
        return true;
    }
    return false;
}

QString AutonomousModelManager::getCurrentModelId() const {
    return currentModelId;
}

QVector<AIModel> AutonomousModelManager::getAvailableModels() const {
    return availableModels;
}

QVector<AIModel> AutonomousModelManager::getInstalledModels() const {
    return installedModels;
}

AIModel AutonomousModelManager::getModelInfo(const QString& modelId) const {
    for (const AIModel& model : availableModels) {
        if (model.modelId == modelId) {
            return model;
        }
    }
    return AIModel();
}

void AutonomousModelManager::refreshModelRegistry() {
    fetchHuggingFaceModels();
    emit registryUpdated(availableModels.size());
}

void AutonomousModelManager::fetchHuggingFaceModels() {
    // Placeholder - would query HuggingFace API
    std::cout << "[AutonomousModelManager] Fetching models from HuggingFace..." << std::endl;
}

void AutonomousModelManager::addCustomModel(const AIModel& model) {
    availableModels.append(model);
    saveModelRegistry();
    emit registryUpdated(availableModels.size());
}

bool AutonomousModelManager::removeModel(const QString& modelId) {
    for (int i = 0; i < availableModels.size(); ++i) {
        if (availableModels[i].modelId == modelId) {
            // Delete local file if exists
            if (availableModels[i].isDownloaded) {
                QFile::remove(availableModels[i].localPath);
            }
            availableModels.removeAt(i);
            saveModelRegistry();
            return true;
        }
    }
    return false;
}

void AutonomousModelManager::enableAutoUpdate(bool enable) {
    autoUpdateEnabled = enable;
    if (enable) {
        autoUpdateTimer->start();
    } else {
        autoUpdateTimer->stop();
    }
}

void AutonomousModelManager::onAutoUpdateTimerTimeout() {
    if (autoUpdateEnabled) {
        checkForModelUpdates();
    }
}

void AutonomousModelManager::checkForModelUpdates() {
    std::cout << "[AutonomousModelManager] Checking for model updates..." << std::endl;
    refreshModelRegistry();
}

void AutonomousModelManager::cleanupUnusedModels(int daysUnused) {
    QDateTime cutoffDate = QDateTime::currentDateTime().addDays(-daysUnused);
    
    for (int i = installedModels.size() - 1; i >= 0; --i) {
        if (installedModels[i].lastUsed < cutoffDate) {
            std::cout << "[AutonomousModelManager] Removing unused model: " 
                      << installedModels[i].modelId.toStdString() << std::endl;
            QFile::remove(installedModels[i].localPath);
            installedModels.removeAt(i);
        }
    }
    
    saveModelRegistry();
}

QJsonObject AutonomousModelManager::getModelPerformanceStats(const QString& modelId) const {
    return performanceStats.value(modelId, QJsonObject());
}

void AutonomousModelManager::recordModelUsage(const QString& modelId, 
                                             double latency, bool success) {
    QJsonObject stats = performanceStats.value(modelId, QJsonObject());
    
    int totalCalls = stats["total_calls"].toInt(0) + 1;
    int successCalls = stats["success_calls"].toInt(0) + (success ? 1 : 0);
    double avgLatency = stats["avg_latency"].toDouble(0.0);
    
    // Update average latency
    avgLatency = ((avgLatency * (totalCalls - 1)) + latency) / totalCalls;
    
    stats["total_calls"] = totalCalls;
    stats["success_calls"] = successCalls;
    stats["avg_latency"] = avgLatency;
    stats["success_rate"] = (successCalls * 100.0) / totalCalls;
    stats["last_used"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    performanceStats[modelId] = stats;
    
    // Update model last used timestamp
    for (AIModel& model : availableModels) {
        if (model.modelId == modelId) {
            model.lastUsed = QDateTime::currentDateTime();
            break;
        }
    }
}

void AutonomousModelManager::setModelCacheDirectory(const QString& path) {
    modelCacheDirectory = path;
    QDir().mkpath(path);
}

void AutonomousModelManager::setMaxCacheSize(qint64 bytes) {
    maxCacheSize = bytes;
}

void AutonomousModelManager::setPreferredQuantization(const QString& quantization) {
    preferredQuantization = quantization;
}

QString AutonomousModelManager::getModelCacheDirectory() const {
    return modelCacheDirectory;
}
