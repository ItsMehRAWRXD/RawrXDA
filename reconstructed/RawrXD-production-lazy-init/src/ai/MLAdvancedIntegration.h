#pragma once
#include "MLIntegration.h"
#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>

namespace RawrXD {
namespace Integration {
namespace ML {

// ML-specific health checks
class MLHealthChecker {
public:
    static MLHealthChecker& instance() {
        static MLHealthChecker checker;
        return checker;
    }

    struct HealthStatus {
        bool isHealthy;
        QString status;
        QJsonObject details;
        QDateTime lastChecked;
    };

    HealthStatus checkMLSystemHealth() {
        HealthStatus status;
        status.lastChecked = QDateTime::currentDateTime();
        
        QJsonObject details;
        
        // Check model health
        auto modelStatus = ModelLifecycleTracker::instance().getModelStatus();
        int healthyModels = ModelLifecycleTracker::instance().healthyModelCount();
        int totalModels = modelStatus.size();
        
        details.insert(QStringLiteral("total_models"), totalModels);
        details.insert(QStringLiteral("healthy_models"), healthyModels);
        details.insert(QStringLiteral("model_health_ratio"), 
                      totalModels > 0 ? static_cast<double>(healthyModels) / totalModels : 1.0);
        
        // Check training pipelines
        auto activeTrainings = TrainingPipelineTracker::instance().getActiveTrainings();
        details.insert(QStringLiteral("active_trainings"), activeTrainings.size());
        
        // Check inference latency
        double avgLatency = calculateAverageInferenceLatency();
        details.insert(QStringLiteral("avg_inference_latency_ms"), avgLatency);
        
        // Determine overall health
        bool modelsHealthy = (totalModels == 0) || (healthyModels >= totalModels * 0.8); // 80% threshold
        bool latencyHealthy = avgLatency < 1000; // 1 second threshold
        
        status.isHealthy = modelsHealthy && latencyHealthy;
        status.status = status.isHealthy ? QStringLiteral("Healthy") : QStringLiteral("Degraded");
        status.details = details;
        
        if (Config::loggingEnabled()) {
            logInfo(QStringLiteral("MLHealthChecker"), QStringLiteral("health_check"),
                    QStringLiteral("ML system health: %1 (models: %2/%3, latency: %4ms)")
                    .arg(status.status).arg(healthyModels).arg(totalModels).arg(avgLatency));
        }
        
        return status;
    }

    void registerCustomHealthCheck(const QString& checkName, std::function<bool(QJsonObject&)> checkFn) {
        QMutexLocker lock(&m_mutex);
        m_customChecks[checkName] = std::move(checkFn);
    }

private:
    MLHealthChecker() = default;
    
    double calculateAverageInferenceLatency() const {
        // Simple average calculation - in production, use more sophisticated metrics
        auto modelStatus = ModelLifecycleTracker::instance().getModelStatus();
        double totalLatency = 0.0;
        int count = 0;
        
        for (const auto& model : modelStatus) {
            if (model.toObject().contains(QStringLiteral("avg_inference_ms"))) {
                totalLatency += model.toObject()[QStringLiteral("avg_inference_ms")].toDouble();
                count++;
            }
        }
        
        return count > 0 ? totalLatency / count : 0.0;
    }
    
    mutable QMutex m_mutex;
    QMap<QString, std::function<bool(QJsonObject&)>> m_customChecks;
};

// ML configuration management
class MLConfigManager {
public:
    static MLConfigManager& instance() {
        static MLConfigManager manager;
        return manager;
    }

    bool isFeatureEnabled(const QString& featureName, bool defaultEnabled = false) {
        const QString envKey = QStringLiteral("RAWRXD_ML_FEATURE_") + featureName;
        return featureEnabled(envKey.toUtf8().constData(), defaultEnabled);
    }

    int getIntConfig(const QString& configName, int defaultValue = 0) {
        const QString envKey = QStringLiteral("RAWRXD_ML_") + configName;
        const QByteArray envVal = qgetenv(envKey.toUtf8().constData());
        
        if (!envVal.isEmpty()) {
            bool ok;
            int value = QString::fromUtf8(envVal).toInt(&ok);
            if (ok) return value;
        }
        
        return defaultValue;
    }

    double getDoubleConfig(const QString& configName, double defaultValue = 0.0) {
        const QString envKey = QStringLiteral("RAWRXD_ML_") + configName;
        const QByteArray envVal = qgetenv(envKey.toUtf8().constData());
        
        if (!envVal.isEmpty()) {
            bool ok;
            double value = QString::fromUtf8(envVal).toDouble(&ok);
            if (ok) return value;
        }
        
        return defaultValue;
    }

    QString getStringConfig(const QString& configName, const QString& defaultValue = QString()) {
        const QString envKey = QStringLiteral("RAWRXD_ML_") + configName;
        const QByteArray envVal = qgetenv(envKey.toUtf8().constData());
        
        return envVal.isEmpty() ? defaultValue : QString::fromUtf8(envVal);
    }

    QJsonObject getMLConfig() const {
        QJsonObject config;
        
        // Model configuration
        config.insert(QStringLiteral("enable_ml_metrics"), Config::metricsEnabled());
        config.insert(QStringLiteral("enable_ml_logging"), Config::loggingEnabled());
        config.insert(QStringLiteral("enable_model_lifecycle_tracking"), 
                      isFeatureEnabled(QStringLiteral("MODEL_LIFECYCLE_TRACKING"), true));
        config.insert(QStringLiteral("enable_training_pipeline_tracking"), 
                      isFeatureEnabled(QStringLiteral("TRAINING_PIPELINE_TRACKING"), true));
        config.insert(QStringLiteral("enable_feature_store_tracking"), 
                      isFeatureEnabled(QStringLiteral("FEATURE_STORE_TRACKING"), true));
        
        // Performance thresholds
        config.insert(QStringLiteral("inference_latency_threshold_ms"), 
                      getIntConfig(QStringLiteral("INFERENCE_LATENCY_THRESHOLD"), 1000));
        config.insert(QStringLiteral("model_health_threshold"), 
                      getDoubleConfig(QStringLiteral("MODEL_HEALTH_THRESHOLD"), 0.8));
        config.insert(QStringLiteral("training_timeout_ms"), 
                      getIntConfig(QStringLiteral("TRAINING_TIMEOUT"), 3600000)); // 1 hour
        
        return config;
    }

private:
    MLConfigManager() = default;
};

// ML error pattern analyzer
class MLErrorPatternAnalyzer {
public:
    static MLErrorPatternAnalyzer& instance() {
        static MLErrorPatternAnalyzer analyzer;
        return analyzer;
    }

    void analyzeErrorPattern(const QString& component, const QString& operation, 
                            const QString& error, const QJsonObject& context) {
        if (!MLConfigManager::instance().isFeatureEnabled("ERROR_ANALYSIS", false)) return;

        ScopedTimer timer("MLErrorPatternAnalyzer", "MLErrorPatternAnalyzer", "analyzeErrorPattern");
        
        QJsonObject analysis;
        analysis.insert(QStringLiteral("component"), component);
        analysis.insert(QStringLiteral("operation"), operation);
        analysis.insert(QStringLiteral("error"), error);
        analysis.insert(QStringLiteral("context"), context);
        analysis.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(Qt::ISODate));
        
        // Simple pattern detection
        if (error.contains(QStringLiteral("timeout"), Qt::CaseInsensitive)) {
            analysis.insert(QStringLiteral("pattern"), QStringLiteral("timeout"));
            analysis.insert(QStringLiteral("suggested_action"), QStringLiteral("increase timeout or retry"));
        } else if (error.contains(QStringLiteral("memory"), Qt::CaseInsensitive)) {
            analysis.insert(QStringLiteral("pattern"), QStringLiteral("memory_issue"));
            analysis.insert(QStringLiteral("suggested_action"), QStringLiteral("reduce batch size or increase memory"));
        } else if (error.contains(QStringLiteral("model"), Qt::CaseInsensitive)) {
            analysis.insert(QStringLiteral("pattern"), QStringLiteral("model_issue"));
            analysis.insert(QStringLiteral("suggested_action"), QStringLiteral("check model file or retrain"));
        } else {
            analysis.insert(QStringLiteral("pattern"), QStringLiteral("unknown"));
            analysis.insert(QStringLiteral("suggested_action"), QStringLiteral("investigate logs"));
        }
        
        logInfo(QStringLiteral("MLErrorPatternAnalyzer"), QStringLiteral("analysis"),
                QStringLiteral("Error pattern detected: %1").arg(analysis[QStringLiteral("pattern")].toString()), analysis);
        
        if (MLConfigManager::instance().isFeatureEnabled("METRICS", false)) {
            recordMetric(QStringLiteral("ml_error_pattern_") + analysis[QStringLiteral("pattern")].toString(), 1);
        }
    }

    QJsonArray getErrorPatternSummary() const {
        QJsonArray summary;
        
        // In production, this would aggregate patterns from a database
        // For now, return a simple summary
        QJsonObject timeoutPattern;
        timeoutPattern.insert(QStringLiteral("pattern"), QStringLiteral("timeout"));
        timeoutPattern.insert(QStringLiteral("count"), 0);
        timeoutPattern.insert(QStringLiteral("last_occurrence"), QDateTime::currentDateTime().toString(Qt::ISODate));
        summary.append(timeoutPattern);
        
        QJsonObject memoryPattern;
        memoryPattern.insert(QStringLiteral("pattern"), QStringLiteral("memory_issue"));
        memoryPattern.insert(QStringLiteral("count"), 0);
        memoryPattern.insert(QStringLiteral("last_occurrence"), QDateTime::currentDateTime().toString(Qt::ISODate));
        summary.append(memoryPattern);
        
        QJsonObject modelPattern;
        modelPattern.insert(QStringLiteral("pattern"), QStringLiteral("model_issue"));
        modelPattern.insert(QStringLiteral("count"), 0);
        modelPattern.insert(QStringLiteral("last_occurrence"), QDateTime::currentDateTime().toString(Qt::ISODate));
        summary.append(modelPattern);
        
        return summary;
    }

private:
    MLErrorPatternAnalyzer() = default;
};

// ML performance profiler
class MLPerformanceProfiler {
public:
    static MLPerformanceProfiler& instance() {
        static MLPerformanceProfiler profiler;
        return profiler;
    }

    void startProfiling(const QString& profileName) {
        QMutexLocker lock(&m_mutex);
        
        ProfileInfo info;
        info.name = profileName;
        info.startTime = QDateTime::currentDateTime();
        info.timer.start();
        
        m_activeProfiles[profileName] = info;
        
        if (Config::loggingEnabled()) {
            logDebug(QStringLiteral("MLPerformanceProfiler"), QStringLiteral("start"),
                    QStringLiteral("Profiling started: %1").arg(profileName));
        }
    }

    void stopProfiling(const QString& profileName) {
        QMutexLocker lock(&m_mutex);
        
        auto it = m_activeProfiles.find(profileName);
        if (it != m_activeProfiles.end()) {
            qint64 durationMs = it->timer.elapsed();
            
            if (Config::metricsEnabled()) {
                recordMetric(QStringLiteral("ml_profile_") + profileName, durationMs);
            }
            
            if (Config::loggingEnabled()) {
                logDebug(QStringLiteral("MLPerformanceProfiler"), QStringLiteral("stop"),
                        QStringLiteral("Profiling stopped: %1 took %2ms").arg(profileName).arg(durationMs));
            }
            
            m_activeProfiles.erase(it);
        }
    }

    QJsonArray getProfileSummary() const {
        QMutexLocker lock(&m_mutex);
        
        QJsonArray summary;
        for (const auto& profile : m_activeProfiles) {
            QJsonObject obj;
            obj.insert(QStringLiteral("name"), profile.name);
            obj.insert(QStringLiteral("start_time"), profile.startTime.toString(Qt::ISODate));
            obj.insert(QStringLiteral("duration_ms"), profile.timer.elapsed());
            summary.append(obj);
        }
        
        return summary;
    }

private:
    struct ProfileInfo {
        QString name;
        QDateTime startTime;
        QElapsedTimer timer;
    };

    MLPerformanceProfiler() = default;
    
    mutable QMutex m_mutex;
    QMap<QString, ProfileInfo> m_activeProfiles;
};

} // namespace ML
} // namespace Integration
} // namespace RawrXD
