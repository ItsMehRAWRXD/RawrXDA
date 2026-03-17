#pragma once
#include "../qtapp/integration/ProdIntegration.h"
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QPointer>
#include <vector>
#include <functional>

namespace RawrXD {
namespace Integration {
namespace ML {

// ML-specific metrics and observability
class MLMetrics {
public:
    static MLMetrics& instance() {
        static MLMetrics metrics;
        return metrics;
    }

    void recordInference(const QString& modelId, qint64 durationMs, bool success = true) {
        if (!Config::metricsEnabled()) return;
        
        const QString metricName = success ? QStringLiteral("ml_inference_success") 
                                         : QStringLiteral("ml_inference_failure");
        recordMetric(metricName, 1);
        
        if (success) {
            recordMetric(QStringLiteral("ml_inference_duration_ms"), durationMs);
        }
        
        if (Config::loggingEnabled()) {
            logInfo(QStringLiteral("MLMetrics"), QStringLiteral("inference"),
                    QStringLiteral("%1: %2ms (%3)").arg(modelId).arg(durationMs).arg(success ? "success" : "failure"));
        }
    }

    void recordTraining(const QString& modelId, qint64 durationMs, int sampleCount, double accuracy) {
        if (!Config::metricsEnabled()) return;
        
        recordMetric(QStringLiteral("ml_training_completed"), 1);
        recordMetric(QStringLiteral("ml_training_duration_ms"), durationMs);
        recordMetric(QStringLiteral("ml_training_samples"), sampleCount);
        
        if (Config::loggingEnabled()) {
            logInfo(QStringLiteral("MLMetrics"), QStringLiteral("training"),
                    QStringLiteral("%1: %2 samples in %3ms, accuracy=%4")
                    .arg(modelId).arg(sampleCount).arg(durationMs).arg(accuracy));
        }
    }

    void recordFeatureExtraction(const QString& featureName, qint64 durationMs) {
        if (!Config::metricsEnabled()) return;
        
        recordMetric(QStringLiteral("ml_feature_extraction"), 1);
        recordMetric(QStringLiteral("ml_feature_duration_ms"), durationMs);
        
        if (Config::loggingEnabled()) {
            logDebug(QStringLiteral("MLMetrics"), QStringLiteral("feature_extraction"),
                    QStringLiteral("%1: %2ms").arg(featureName).arg(durationMs));
        }
    }

    void recordModelLoad(const QString& modelId, qint64 durationMs, bool success) {
        if (!Config::metricsEnabled()) return;
        
        const QString metricName = success ? QStringLiteral("ml_model_load_success") 
                                         : QStringLiteral("ml_model_load_failure");
        recordMetric(metricName, 1);
        
        if (success) {
            recordMetric(QStringLiteral("ml_model_load_duration_ms"), durationMs);
        }
        
        if (Config::loggingEnabled()) {
            logInfo(QStringLiteral("MLMetrics"), QStringLiteral("model_load"),
                    QStringLiteral("%1: %2ms (%3)").arg(modelId).arg(durationMs).arg(success ? "success" : "failure"));
        }
    }

    void recordPredictionConfidence(const QString& modelId, double confidence) {
        if (!Config::metricsEnabled()) return;
        
        // Bucket confidence into ranges for histogram
        int bucket = static_cast<int>(confidence * 10); // 0-10 buckets
        recordMetric(QStringLiteral("ml_prediction_confidence_bucket"), bucket);
        
        if (Config::loggingEnabled()) {
            logDebug(QStringLiteral("MLMetrics"), QStringLiteral("prediction_confidence"),
                    QStringLiteral("%1: confidence=%2").arg(modelId).arg(confidence));
        }
    }

private:
    MLMetrics() = default;
};

// ML model lifecycle tracker
class ModelLifecycleTracker {
public:
    struct ModelInfo {
        QString id;
        QString type;
        QString version;
        QDateTime loadedAt;
        qint64 loadDurationMs;
        int inferenceCount;
        double avgInferenceMs;
        bool isHealthy;
        QString healthReason;
    };

    static ModelLifecycleTracker& instance() {
        static ModelLifecycleTracker tracker;
        return tracker;
    }

    void registerModel(const QString& modelId, const QString& modelType, 
                      const QString& version = QStringLiteral("1.0")) {
        QMutexLocker lock(&m_mutex);
        
        ModelInfo info;
        info.id = modelId;
        info.type = modelType;
        info.version = version;
        info.loadedAt = QDateTime::currentDateTime();
        info.isHealthy = true;
        info.healthReason = QStringLiteral("Model registered");
        
        m_models[modelId] = info;
        
        if (Config::loggingEnabled()) {
            logInfo(QStringLiteral("ModelLifecycleTracker"), QStringLiteral("register"),
                    QStringLiteral("%1 (%2 v%3) registered").arg(modelId, modelType, version));
        }
    }

    void recordModelLoad(const QString& modelId, qint64 durationMs, bool success) {
        QMutexLocker lock(&m_mutex);
        
        auto it = m_models.find(modelId);
        if (it != m_models.end()) {
            it->loadDurationMs = durationMs;
            it->isHealthy = success;
            it->healthReason = success ? QStringLiteral("Load successful") : QStringLiteral("Load failed");
        }
        
        MLMetrics::instance().recordModelLoad(modelId, durationMs, success);
    }

    void recordInference(const QString& modelId, qint64 durationMs, bool success) {
        QMutexLocker lock(&m_mutex);
        
        auto it = m_models.find(modelId);
        if (it != m_models.end()) {
            it->inferenceCount++;
            it->avgInferenceMs = ((it->avgInferenceMs * (it->inferenceCount - 1)) + durationMs) / it->inferenceCount;
            it->isHealthy = success;
            it->healthReason = success ? QStringLiteral("Inference successful") : QStringLiteral("Inference failed");
        }
        
        MLMetrics::instance().recordInference(modelId, durationMs, success);
    }

    void markModelUnhealthy(const QString& modelId, const QString& reason) {
        QMutexLocker lock(&m_mutex);
        
        auto it = m_models.find(modelId);
        if (it != m_models.end()) {
            it->isHealthy = false;
            it->healthReason = reason;
            
            if (Config::loggingEnabled()) {
                logWarn(QStringLiteral("ModelLifecycleTracker"), QStringLiteral("unhealthy"),
                        QStringLiteral("%1: %2").arg(modelId, reason));
            }
        }
    }

    QJsonArray getModelStatus() const {
        QMutexLocker lock(&m_mutex);
        
        QJsonArray result;
        for (const auto& model : m_models) {
            QJsonObject obj;
            obj.insert(QStringLiteral("id"), model.id);
            obj.insert(QStringLiteral("type"), model.type);
            obj.insert(QStringLiteral("version"), model.version);
            obj.insert(QStringLiteral("loaded_at"), model.loadedAt.toString(Qt::ISODate));
            obj.insert(QStringLiteral("load_duration_ms"), model.loadDurationMs);
            obj.insert(QStringLiteral("inference_count"), model.inferenceCount);
            obj.insert(QStringLiteral("avg_inference_ms"), model.avgInferenceMs);
            obj.insert(QStringLiteral("is_healthy"), model.isHealthy);
            obj.insert(QStringLiteral("health_reason"), model.healthReason);
            result.append(obj);
        }
        
        return result;
    }

    int healthyModelCount() const {
        QMutexLocker lock(&m_mutex);
        
        int count = 0;
        for (const auto& model : m_models) {
            if (model.isHealthy) count++;
        }
        
        return count;
    }

private:
    ModelLifecycleTracker() = default;
    
    mutable QMutex m_mutex;
    QMap<QString, ModelInfo> m_models;
};

// ML inference wrapper with observability
template <typename InputT, typename OutputT>
class InferenceWrapper {
public:
    using InferenceFn = std::function<OutputT(const InputT&)>;

    InferenceWrapper(const QString& modelId, InferenceFn fn)
        : m_modelId(modelId), m_inferenceFn(std::move(fn)) {
        ModelLifecycleTracker::instance().registerModel(modelId, QStringLiteral("custom"));
    }

    OutputT operator()(const InputT& input) {
        ScopedTimer timer(QStringLiteral("InferenceWrapper"), m_modelId.toUtf8().constData(), QStringLiteral("inference"));
        
        QElapsedTimer inferenceTimer;
        inferenceTimer.start();
        
        try {
            OutputT result = m_inferenceFn(input);
            qint64 durationMs = inferenceTimer.elapsed();
            
            ModelLifecycleTracker::instance().recordInference(m_modelId, durationMs, true);
            
            if (Config::loggingEnabled()) {
                logDebug(QStringLiteral("InferenceWrapper"), QStringLiteral("success"),
                        QStringLiteral("%1: inference completed in %2ms").arg(m_modelId).arg(durationMs));
            }
            
            return result;
        } catch (const std::exception& ex) {
            qint64 durationMs = inferenceTimer.elapsed();
            
            ModelLifecycleTracker::instance().recordInference(m_modelId, durationMs, false);
            ModelLifecycleTracker::instance().markModelUnhealthy(m_modelId, QString::fromUtf8(ex.what()));
            
            if (Config::loggingEnabled()) {
                logError(QStringLiteral("InferenceWrapper"), QStringLiteral("failure"),
                        QStringLiteral("%1: inference failed after %2ms: %3")
                        .arg(m_modelId).arg(durationMs).arg(QString::fromUtf8(ex.what())));
            }
            
            throw;
        }
    }

private:
    QString m_modelId;
    InferenceFn m_inferenceFn;
};

// ML training pipeline observability
class TrainingPipelineTracker {
public:
    static TrainingPipelineTracker& instance() {
        static TrainingPipelineTracker tracker;
        return tracker;
    }

    void startTraining(const QString& pipelineId, int sampleCount) {
        QMutexLocker lock(&m_mutex);
        
        m_activeTrainings[pipelineId] = {
            QDateTime::currentDateTime(),
            sampleCount,
            0, // progress
            QStringLiteral("Training started")
        };
        
        if (Config::loggingEnabled()) {
            logInfo(QStringLiteral("TrainingPipelineTracker"), QStringLiteral("start"),
                    QStringLiteral("%1: started with %2 samples").arg(pipelineId).arg(sampleCount));
        }
    }

    void updateProgress(const QString& pipelineId, int progressPercent, const QString& status) {
        QMutexLocker lock(&m_mutex);
        
        auto it = m_activeTrainings.find(pipelineId);
        if (it != m_activeTrainings.end()) {
            it->progress = progressPercent;
            it->status = status;
            
            if (Config::loggingEnabled()) {
                logDebug(QStringLiteral("TrainingPipelineTracker"), QStringLiteral("progress"),
                        QStringLiteral("%1: %2% - %3").arg(pipelineId).arg(progressPercent).arg(status));
            }
        }
    }

    void completeTraining(const QString& pipelineId, double accuracy, qint64 durationMs) {
        QMutexLocker lock(&m_mutex);
        
        auto it = m_activeTrainings.find(pipelineId);
        if (it != m_activeTrainings.end()) {
            m_activeTrainings.erase(it);
            
            MLMetrics::instance().recordTraining(pipelineId, durationMs, it->sampleCount, accuracy);
            
            if (Config::loggingEnabled()) {
                logInfo(QStringLiteral("TrainingPipelineTracker"), QStringLiteral("complete"),
                        QStringLiteral("%1: completed in %2ms, accuracy=%3")
                        .arg(pipelineId).arg(durationMs).arg(accuracy));
            }
        }
    }

    void failTraining(const QString& pipelineId, const QString& error) {
        QMutexLocker lock(&m_mutex);
        
        auto it = m_activeTrainings.find(pipelineId);
        if (it != m_activeTrainings.end()) {
            m_activeTrainings.erase(it);
            
            if (Config::loggingEnabled()) {
                logError(QStringLiteral("TrainingPipelineTracker"), QStringLiteral("failure"),
                        QStringLiteral("%1: failed: %2").arg(pipelineId).arg(error));
            }
        }
    }

    QJsonArray getActiveTrainings() const {
        QMutexLocker lock(&m_mutex);
        
        QJsonArray result;
        for (const auto& training : m_activeTrainings) {
            QJsonObject obj;
            obj.insert(QStringLiteral("pipeline_id"), training.pipelineId);
            obj.insert(QStringLiteral("started_at"), training.startedAt.toString(Qt::ISODate));
            obj.insert(QStringLiteral("sample_count"), training.sampleCount);
            obj.insert(QStringLiteral("progress"), training.progress);
            obj.insert(QStringLiteral("status"), training.status);
            result.append(obj);
        }
        
        return result;
    }

private:
    struct TrainingInfo {
        QString pipelineId;
        QDateTime startedAt;
        int sampleCount;
        int progress; // 0-100
        QString status;
    };

    TrainingPipelineTracker() = default;
    
    mutable QMutex m_mutex;
    QMap<QString, TrainingInfo> m_activeTrainings;
};

// ML feature store integration helpers
class FeatureStoreHelper {
public:
    static FeatureStoreHelper& instance() {
        static FeatureStoreHelper helper;
        return helper;
    }

    void recordFeatureUsage(const QString& featureName, qint64 extractionTimeMs) {
        MLMetrics::instance().recordFeatureExtraction(featureName, extractionTimeMs);
    }

    void recordFeatureQuality(const QString& featureName, double qualityScore) {
        if (!Config::metricsEnabled()) return;
        
        recordMetric(QStringLiteral("ml_feature_quality"), static_cast<qint64>(qualityScore * 100));
        
        if (Config::loggingEnabled()) {
            logDebug(QStringLiteral("FeatureStoreHelper"), QStringLiteral("quality"),
                    QStringLiteral("%1: quality=%2").arg(featureName).arg(qualityScore));
        }
    }

    void recordFeatureCacheHit(const QString& featureName, bool hit) {
        if (!Config::metricsEnabled()) return;
        
        const QString metricName = hit ? QStringLiteral("ml_feature_cache_hit") 
                                     : QStringLiteral("ml_feature_cache_miss");
        recordMetric(metricName, 1);
        
        if (Config::loggingEnabled()) {
            logDebug(QStringLiteral("FeatureStoreHelper"), QStringLiteral("cache"),
                    QStringLiteral("%1: cache %2").arg(featureName).arg(hit ? "hit" : "miss"));
        }
    }

private:
    FeatureStoreHelper() = default;
};

} // namespace ML
} // namespace Integration
} // namespace RawrXD
