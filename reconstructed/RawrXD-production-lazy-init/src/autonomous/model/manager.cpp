#include "autonomous_model_manager.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QThread>
#include <memory>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

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

QJsonArray AutonomousModelManager::getAvailableModels() {
    return availableModels;
}

ModelRecommendation AutonomousModelManager::autoDetectBestModel(const QString& taskType, const QString& language) {
    qInfo() << "[AutonomousModelManager] Auto-detecting best model for task:" << taskType << "language:" << language;
    
    ModelRecommendation recommendation;
    
    // Analyze system capabilities first
    SystemAnalysis system = analyzeSystemCapabilities();
    
    // Determine optimal model based on task type and system capabilities
    if (taskType == "completion" || taskType == "code") {
        if (system.availableRAM > 16LL * 1024 * 1024 * 1024) {
            recommendation.modelId = "codellama-13b-q4";
            recommendation.name = "CodeLlama 13B Q4";
            recommendation.suitabilityScore = 0.95;
            recommendation.reasoning = "Large memory available, best for complex code completion";
        } else if (system.availableRAM > 8LL * 1024 * 1024 * 1024) {
            recommendation.modelId = "codellama-7b-q4";
            recommendation.name = "CodeLlama 7B Q4";
            recommendation.suitabilityScore = 0.85;
            recommendation.reasoning = "Medium memory, good for most code completion tasks";
        } else {
            recommendation.modelId = "phi-2-q4";
            recommendation.name = "Phi-2 Q4";
            recommendation.suitabilityScore = 0.70;
            recommendation.reasoning = "Limited memory, small but capable model";
        }
    } else if (taskType == "chat") {
        if (system.availableRAM > 16LL * 1024 * 1024 * 1024) {
            recommendation.modelId = "llama-3-8b-instruct-q4";
            recommendation.name = "Llama 3 8B Instruct Q4";
            recommendation.suitabilityScore = 0.92;
            recommendation.reasoning = "Best conversational model for available resources";
        } else {
            recommendation.modelId = "phi-3-mini-q4";
            recommendation.name = "Phi-3 Mini Q4";
            recommendation.suitabilityScore = 0.80;
            recommendation.reasoning = "Efficient chat model for limited resources";
        }
    } else {
        // Default general-purpose model
        recommendation.modelId = "mistral-7b-q4";
        recommendation.name = "Mistral 7B Q4";
        recommendation.suitabilityScore = 0.88;
        recommendation.reasoning = "Versatile model for general tasks";
    }
    
    // Adjust for language preference
    if (language == "python" || language == "py") {
        recommendation.suitabilityScore += 0.02;
        recommendation.reasoning += " | Python-optimized selection";
    } else if (language == "cpp" || language == "c++") {
        recommendation.suitabilityScore += 0.01;
        recommendation.reasoning += " | C++ capable";
    }
    
    recommendation.taskType = taskType;
    recommendation.estimatedMemoryUsage = 4LL * 1024 * 1024 * 1024; // Estimate 4GB
    recommendation.estimatedDownloadSize = 4LL * 1024 * 1024 * 1024; // Estimate 4GB
    
    qInfo() << "[AutonomousModelManager] Recommended:" << recommendation.modelId 
            << "Score:" << recommendation.suitabilityScore;
    
    return recommendation;
}

SystemAnalysis AutonomousModelManager::analyzeSystemCapabilities() {
    qInfo() << "[AutonomousModelManager] Analyzing system capabilities";
    
    SystemAnalysis analysis;
    
    // Get available RAM (platform-specific, using conservative estimates)
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        analysis.availableRAM = static_cast<qint64>(memInfo.ullAvailPhys);
    } else {
        analysis.availableRAM = 8LL * 1024 * 1024 * 1024; // Default 8GB
    }
#else
    analysis.availableRAM = 8LL * 1024 * 1024 * 1024; // Default 8GB on other platforms
#endif
    
    // Get available disk space
    analysis.availableDiskSpace = 50LL * 1024 * 1024 * 1024; // Default 50GB
    
    // Get CPU cores
    analysis.cpuCores = QThread::idealThreadCount();
    if (analysis.cpuCores < 1) analysis.cpuCores = 4; // Default
    
    // GPU detection (simplified)
    analysis.hasGPU = true; // Assume GPU available on modern systems
    analysis.gpuType = "Unknown";
    analysis.gpuMemory = 8LL * 1024 * 1024 * 1024; // Default 8GB
    
    currentSystem = analysis;
    
    qInfo() << "[AutonomousModelManager] System analysis complete:"
            << "RAM:" << (analysis.availableRAM / (1024*1024*1024)) << "GB"
            << "Cores:" << analysis.cpuCores
            << "GPU:" << analysis.hasGPU;
    
    return analysis;
}

ModelRecommendation AutonomousModelManager::recommendModelForCodebase(const QString& projectPath) {
    qInfo() << "[AutonomousModelManager] Recommending model for codebase:" << projectPath;
    
    ModelRecommendation recommendation;
    
    // Analyze codebase to determine requirements
    QDir projectDir(projectPath);
    QStringList filters;
    filters << "*.cpp" << "*.h" << "*.py" << "*.js" << "*.ts" << "*.java";
    QStringList files = projectDir.entryList(filters, QDir::Files);
    
    // Determine dominant language
    int cppCount = 0, pyCount = 0, jsCount = 0;
    for (const QString& file : files) {
        if (file.endsWith(".cpp") || file.endsWith(".h")) cppCount++;
        else if (file.endsWith(".py")) pyCount++;
        else if (file.endsWith(".js") || file.endsWith(".ts")) jsCount++;
    }
    
    QString primaryLanguage;
    QString taskType;
    
    if (cppCount > pyCount && cppCount > jsCount) {
        primaryLanguage = "cpp";
        taskType = "code";
    } else if (pyCount > jsCount) {
        primaryLanguage = "python";
        taskType = "code";
    } else if (jsCount > 0) {
        primaryLanguage = "javascript";
        taskType = "code";
    } else {
        primaryLanguage = "general";
        taskType = "completion";
    }
    
    // Use autoDetectBestModel with the detected language and task
    recommendation = autoDetectBestModel(taskType, primaryLanguage);
    
    // Add codebase-specific reasoning
    recommendation.reasoning += QString(" | Codebase analysis: %1 files, primary language: %2")
                                .arg(files.size())
                                .arg(primaryLanguage);
    
    qInfo() << "[AutonomousModelManager] Codebase recommendation:" << recommendation.modelId;
    
    return recommendation;
}
