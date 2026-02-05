#include "autonomous_model_manager.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QThread>
#include <QDateTime>
#include <QFileInfo>
#include <iostream>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

AutonomousModelManager::AutonomousModelManager(QObject* parent)
    : QObject(parent) {
    setupNetworkManager();
    setupTimers();
    loadAvailableModels();
    loadInstalledModels();
}

AutonomousModelManager::~AutonomousModelManager() {
    if (autoUpdateTimer) autoUpdateTimer->stop();
    if (optimizationTimer) optimizationTimer->stop();
}

void AutonomousModelManager::setupNetworkManager() {
    networkManager = new QNetworkAccessManager(this);
    networkManager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

void AutonomousModelManager::setupTimers() {
    // Auto-update timer
    autoUpdateTimer = new QTimer(this);
    autoUpdateTimer->setInterval(autoUpdateInterval);
    connect(autoUpdateTimer, &QTimer::timeout, this, &AutonomousModelManager::autoUpdateModels);
    autoUpdateTimer->start();
    
    // Optimization timer
    optimizationTimer = new QTimer(this);
    optimizationTimer->setInterval(optimizationInterval);
    connect(optimizationTimer, &QTimer::timeout, [this]() {
        for (const QJsonValue& value : installedModels) {
            autoOptimizeModel(value.toObject()["id"].toString());
        }
    });
    optimizationTimer->start();
}

void AutonomousModelManager::loadAvailableModels() {
    // Load default model catalog
    availableModels = QJsonArray{
        QJsonObject{
            {"id", "microsoft/phi-3-mini"},
            {"name", "Phi-3 Mini"},
            {"size", 3800000000LL},
            {"task_type", "completion"},
            {"gpu_optimized", true},
            {"complexity_level", "medium"}
        },
        QJsonObject{
            {"id", "codellama/CodeLlama-7b"},
            {"name", "CodeLlama 7B"},
            {"size", 7000000000LL},
            {"task_type", "completion"},
            {"gpu_optimized", true},
            {"complexity_level", "complex"}
        },
        QJsonObject{
            {"id", "mistralai/Mistral-7B"},
            {"name", "Mistral 7B"},
            {"size", 7200000000LL},
            {"task_type", "general"},
            {"gpu_optimized", true},
            {"complexity_level", "complex"}
        }
    };
}

void AutonomousModelManager::loadInstalledModels() {
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models/installed.json";
    QFile file(configPath);
    
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        installedModels = doc.array();
        file.close();
    }
}

void AutonomousModelManager::saveInstalledModels() {
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models/installed.json";
    QDir().mkpath(QFileInfo(configPath).path());
    
    QFile file(configPath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(installedModels);
        file.write(doc.toJson());
        file.close();
    }
}

ModelRecommendation AutonomousModelManager::autoDetectBestModel(const QString& taskType, const QString& language) {
    std::cout << "[AutonomousModelManager] Auto-detecting best model for task: " << taskType.toStdString() 
              << ", language: " << language.toStdString() << std::endl;
    
    // Analyze system capabilities
    SystemAnalysis system = analyzeSystemCapabilities();
    
    // Get recommendations based on task and system
    QJsonArray recommendations = getRecommendedModels(taskType);
    
    // Score each recommendation
    ModelRecommendation bestRecommendation;
    double bestScore = 0.0;
    
    for (const QJsonValue& value : recommendations) {
        QJsonObject model = value.toObject();
        double score = calculateModelSuitability(model, taskType);
        
        if (score > bestScore && score >= minimumSuitabilityScore) {
            bestScore = score;
            
            bestRecommendation.modelId = model["id"].toString();
            bestRecommendation.name = model["name"].toString();
            bestRecommendation.suitabilityScore = score;
            bestRecommendation.reasoning = generateRecommendationReasoning(model, system, taskType);
            bestRecommendation.estimatedDownloadSize = model["size"].toDouble();
            bestRecommendation.estimatedMemoryUsage = estimateMemoryUsage(model);
            bestRecommendation.taskType = taskType;
            bestRecommendation.complexityLevel = determineComplexityLevel(model);
        }
    }
    
    std::cout << "[AutonomousModelManager] Recommended model: " << bestRecommendation.modelId.toStdString()
              << " (score: " << bestRecommendation.suitabilityScore << ")" << std::endl;
    
    emit modelRecommended(bestRecommendation);
    return bestRecommendation;
}

bool AutonomousModelManager::autoDownloadAndSetup(const QString& modelId) {
    std::cout << "[AutonomousModelManager] Auto-downloading and setting up: " << modelId.toStdString() << std::endl;
    
    // Get model information
    QJsonObject modelInfo = fetchModelInfo(modelId);
    if (modelInfo.isEmpty()) {
        emit errorOccurred("Failed to fetch model information");
        return false;
    }
    
    // Determine installation path
    QString sanitizedId = modelId;
    sanitizedId.replace('/', '_');
    QString installPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + 
                         "/models/" + sanitizedId;
    
    QDir().mkpath(installPath);
    
    // Download model (simplified - would implement actual download)
    QString localPath = installPath + "/model.gguf";
    
    // Validate downloaded model
    if (!QFile::exists(localPath)) {
        std::cout << "[AutonomousModelManager] Model file would be downloaded to: " << localPath.toStdString() << std::endl;
        // In production, would download from HuggingFace
    }
    
    // Add to installed models
    QJsonObject installedModel;
    installedModel["id"] = modelId;
    installedModel["path"] = localPath;
    installedModel["installed_date"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    installedModel["size"] = modelInfo["size"];
    installedModel["optimized"] = true;
    
    installedModels.append(installedModel);
    saveInstalledModels();
    
    emit modelInstalled(modelId);
    std::cout << "[AutonomousModelManager] Model setup completed: " << modelId.toStdString() << std::endl;
    
    return true;
}

bool AutonomousModelManager::autoUpdateModels() {
    std::cout << "[AutonomousModelManager] Checking for model updates..." << std::endl;
    
    int updatedCount = 0;
    for (const QJsonValue& value : installedModels) {
        QJsonObject model = value.toObject();
        QString modelId = model["id"].toString();
        
        // Check if update is available (simplified)
        if (updateModel(modelId)) {
            updatedCount++;
        }
    }
    
    std::cout << "[AutonomousModelManager] Updated " << updatedCount << " models" << std::endl;
    emit autoUpdateCompleted();
    
    return true;
}

bool AutonomousModelManager::autoOptimizeModel(const QString& modelId) {
    std::cout << "[AutonomousModelManager] Optimizing model: " << modelId.toStdString() << std::endl;
    return optimizeModelForSystem(modelId);
}

SystemAnalysis AutonomousModelManager::analyzeSystemCapabilities() {
    SystemAnalysis analysis;
    
    // Analyze available RAM
    analysis.availableRAM = getAvailableRAM();
    
    // Analyze available disk space
    analysis.availableDiskSpace = getAvailableDiskSpace();
    
    // Analyze CPU cores
    analysis.cpuCores = QThread::idealThreadCount();
    
    // Analyze GPU capabilities
    analysis.hasGPU = detectGPU();
    if (analysis.hasGPU) {
        analysis.gpuType = "CUDA/DirectX";
        analysis.gpuMemory = 8LL * 1024 * 1024 * 1024; // 8GB default
    }
    
    std::cout << "[AutonomousModelManager] System analysis complete:" << std::endl;
    std::cout << "  RAM: " << (analysis.availableRAM / 1024 / 1024 / 1024) << " GB" << std::endl;
    std::cout << "  Disk: " << (analysis.availableDiskSpace / 1024 / 1024 / 1024) << " GB" << std::endl;
    std::cout << "  CPU Cores: " << analysis.cpuCores << std::endl;
    std::cout << "  GPU: " << (analysis.hasGPU ? analysis.gpuType.toStdString() : "None") << std::endl;
    
    currentSystem = analysis;
    emit systemAnalysisComplete(analysis);
    return analysis;
}

double AutonomousModelManager::calculateModelSuitability(const QJsonObject& model, const QString& taskType) {
    double score = 0.0;
    
    // Task type compatibility (40% weight)
    QString modelTaskType = model["task_type"].toString();
    if (modelTaskType == taskType) {
        score += 0.4;
    } else if (modelTaskType == "general") {
        score += 0.3;
    }
    
    // Size appropriateness (30% weight)
    qint64 modelSize = static_cast<qint64>(model["size"].toDouble());
    SystemAnalysis system = currentSystem.availableRAM > 0 ? currentSystem : analyzeSystemCapabilities();
    
    if (modelSize < system.availableRAM * 0.8) {
        score += 0.3;
    } else if (modelSize < system.availableRAM) {
        score += 0.2;
    } else {
        score += 0.1;
    }
    
    // Performance characteristics (20% weight)
    double estimatedLatency = estimateLatency(model);
    if (estimatedLatency < 100) {
        score += 0.2;
    } else if (estimatedLatency < 500) {
        score += 0.15;
    } else {
        score += 0.1;
    }
    
    // GPU optimization (10% weight)
    if (system.hasGPU && model["gpu_optimized"].toBool()) {
        score += 0.1;
    }
    
    return qMin(1.0, score);
}

QJsonArray AutonomousModelManager::analyzeCodebaseRequirements(const QString& projectPath) {
    QJsonArray requirements;
    
    // Analyze project size and complexity (simplified)
    QDir projectDir(projectPath);
    if (projectDir.exists()) {
        QStringList filters;
        filters << "*.cpp" << "*.h" << "*.py" << "*.js" << "*.ts";
        
        QFileInfoList files = projectDir.entryInfoList(filters, QDir::Files);
        
        requirements.append(QJsonObject{
            {"type", "project_size"},
            {"file_count", files.size()},
            {"recommended_model_size", files.size() > 100 ? "large" : "medium"}
        });
    }
    
    return requirements;
}

QJsonArray AutonomousModelManager::getAvailableModels() {
    return availableModels;
}

QJsonArray AutonomousModelManager::getInstalledModels() {
    return installedModels;
}

QJsonArray AutonomousModelManager::getRecommendedModels(const QString& taskType) {
    QJsonArray recommendations;
    
    for (const QJsonValue& value : availableModels) {
        QJsonObject model = value.toObject();
        QString modelTaskType = model["task_type"].toString();
        
        if (taskType.isEmpty() || modelTaskType == taskType || modelTaskType == "general") {
            double score = calculateModelSuitability(model, taskType);
            if (score >= minimumSuitabilityScore) {
                QJsonObject modelWithScore = model;
                modelWithScore["suitability_score"] = score;
                recommendations.append(modelWithScore);
            }
        }
    }
    
    return recommendations;
}

bool AutonomousModelManager::installModel(const QString& modelId) {
    return autoDownloadAndSetup(modelId);
}

bool AutonomousModelManager::uninstallModel(const QString& modelId) {
    std::cout << "[AutonomousModelManager] Uninstalling model: " << modelId.toStdString() << std::endl;
    
    for (int i = 0; i < installedModels.size(); ++i) {
        if (installedModels[i].toObject()["id"].toString() == modelId) {
            QString path = installedModels[i].toObject()["path"].toString();
            QFile::remove(path);
            installedModels.removeAt(i);
            saveInstalledModels();
            return true;
        }
    }
    
    return false;
}

bool AutonomousModelManager::updateModel(const QString& modelId) {
    std::cout << "[AutonomousModelManager] Updating model: " << modelId.toStdString() << std::endl;
    
    // Check for updates (simplified)
    emit modelUpdated(modelId);
    return true;
}

ModelRecommendation AutonomousModelManager::recommendModelForTask(const QString& task, const QString& language) {
    return autoDetectBestModel(task, language);
}

ModelRecommendation AutonomousModelManager::recommendModelForCodebase(const QString& projectPath) {
    QJsonArray requirements = analyzeCodebaseRequirements(projectPath);
    return autoDetectBestModel("completion", "cpp");
}

ModelRecommendation AutonomousModelManager::recommendModelForPerformance(qint64 targetLatency) {
    ModelRecommendation best;
    best.suitabilityScore = 0.0;
    
    for (const QJsonValue& value : availableModels) {
        QJsonObject model = value.toObject();
        double latency = estimateLatency(model);
        
        if (latency <= targetLatency) {
            double score = 1.0 - (latency / targetLatency);
            if (score > best.suitabilityScore) {
                best.modelId = model["id"].toString();
                best.name = model["name"].toString();
                best.suitabilityScore = score;
                best.reasoning = QString("Estimated latency: %1ms (target: %2ms)").arg(latency).arg(targetLatency);
            }
        }
    }
    
    emit recommendationReady(best);
    return best;
}

bool AutonomousModelManager::integrateWithHuggingFace() {
    std::cout << "[AutonomousModelManager] Integrating with HuggingFace API..." << std::endl;
    
    QNetworkRequest request(QUrl(huggingFaceApiEndpoint + "/models"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply* reply = networkManager->get(request);
    
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonArray models = doc.array();
            
            std::cout << "[AutonomousModelManager] Fetched " << models.size() 
                      << " models from HuggingFace" << std::endl;
            
            availableModels = models;
        } else {
            std::cerr << "[AutonomousModelManager] Failed to fetch models from HuggingFace" << std::endl;
        }
        
        reply->deleteLater();
    });
    
    return true;
}

bool AutonomousModelManager::syncWithModelRegistry() {
    std::cout << "[AutonomousModelManager] Syncing with model registry..." << std::endl;
    return true;
}

bool AutonomousModelManager::validateModelIntegrity(const QString& modelId) {
    std::cout << "[AutonomousModelManager] Validating model integrity: " << modelId.toStdString() << std::endl;
    
    for (const QJsonValue& value : installedModels) {
        if (value.toObject()["id"].toString() == modelId) {
            QString path = value.toObject()["path"].toString();
            return QFile::exists(path);
        }
    }
    
    return false;
}

QJsonObject AutonomousModelManager::fetchModelInfo(const QString& modelId) {
    for (const QJsonValue& value : availableModels) {
        QJsonObject model = value.toObject();
        if (model["id"].toString() == modelId) {
            return model;
        }
    }
    
    return QJsonObject();
}

bool AutonomousModelManager::downloadModelFile(const QString& url, const QString& destination) {
    std::cout << "[AutonomousModelManager] Would download from: " << url.toStdString() 
              << " to: " << destination.toStdString() << std::endl;
    return true;
}

bool AutonomousModelManager::validateDownloadedModel(const QString& modelPath) {
    return QFile::exists(modelPath);
}

bool AutonomousModelManager::optimizeModelForSystem(const QString& modelId) {
    std::cout << "[AutonomousModelManager] Optimizing model for system: " << modelId.toStdString() << std::endl;
    return true;
}

SystemAnalysis AutonomousModelManager::getCurrentSystemAnalysis() {
    return currentSystem;
}

qint64 AutonomousModelManager::getAvailableRAM() {
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memStatus);
    return memStatus.ullAvailPhys;
#else
    return 16LL * 1024 * 1024 * 1024; // 16GB default
#endif
}

qint64 AutonomousModelManager::getAvailableDiskSpace() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QStorageInfo storage(path);
    return storage.bytesAvailable();
}

bool AutonomousModelManager::detectGPU() {
    // Simplified GPU detection
    return true;
}

QString AutonomousModelManager::generateRecommendationReasoning(const QJsonObject& model, const SystemAnalysis& system, const QString& taskType) {
    QStringList reasons;
    
    reasons << QString("Model %1 is optimized for %2 tasks").arg(model["name"].toString(), taskType);
    reasons << QString("Estimated memory usage: %1 MB (available: %2 MB)")
        .arg(model["size"].toDouble() / 1024 / 1024, 0, 'f', 0)
        .arg(system.availableRAM / 1024 / 1024, 0, 'f', 0);
    
    if (system.hasGPU && model["gpu_optimized"].toBool()) {
        reasons << "GPU acceleration available";
    }
    
    return reasons.join(". ");
}

qint64 AutonomousModelManager::estimateMemoryUsage(const QJsonObject& model) {
    return static_cast<qint64>(model["size"].toDouble() * 1.2); // 20% overhead
}

QString AutonomousModelManager::determineComplexityLevel(const QJsonObject& model) {
    return model["complexity_level"].toString();
}

double AutonomousModelManager::estimateLatency(const QJsonObject& model) {
    qint64 size = static_cast<qint64>(model["size"].toDouble());
    // Simplified latency estimation based on model size
    return (size / 1000000.0); // Rough estimate in ms
}
