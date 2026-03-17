// production_systems_bridge.hpp
// C++/Qt Bridge for MASM Production Systems Integration
// Provides C++ wrapper classes and Qt signal integration for MASM modules

#pragma once

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <functional>
#include <cstdint>

// ============================================================================
// EXTERN DECLARATIONS - Link to MASM modules
// ============================================================================

extern "C" {
    // Pipeline Executor Functions
    int32_t pipeline_executor_init();
    int64_t pipeline_create_job(const char* jobName, uint32_t stageCount, void* stagesArray);
    int32_t pipeline_queue_job(int64_t jobId);
    int32_t pipeline_execute_stage(int64_t jobId, uint32_t stageIdx);
    int32_t pipeline_get_job_status(int64_t jobId);  // Returns status code
    int32_t pipeline_cancel_job(int64_t jobId);
    int32_t pipeline_retry_job(int64_t jobId);
    int32_t pipeline_execute_vcs_stage(int64_t jobId, const char* command);
    int32_t pipeline_execute_docker_stage(int64_t jobId, const char* imageName);
    int32_t pipeline_execute_k8s_deploy(int64_t jobId, const char* manifest);
    int32_t pipeline_cleanup_artifacts();
    int32_t pipeline_check_webhook(const char* payload);
    int32_t pipeline_notify_completion(int64_t jobId, const char* notificationType);
    void* pipeline_get_metrics();  // Returns PIPELINE_METRICS*

    // Telemetry Visualization Functions
    int32_t telemetry_collector_init();
    int64_t telemetry_start_request(const char* modelName, uint32_t promptTokens);
    int32_t telemetry_end_request(int64_t requestId, uint32_t completionTokens, 
                                  int64_t latencyMs, int32_t success);
    int32_t telemetry_record_token(int64_t requestId, int32_t tokenCount);
    int32_t telemetry_record_memory(uint64_t bytesUsed);
    int32_t telemetry_record_gpu_usage(uint32_t utilizationPercent);
    int32_t telemetry_record_event(const char* eventName, const char* details);
    void* telemetry_get_metrics();  // Returns AGGREGATE_METRICS*
    int32_t telemetry_calculate_percentiles();
    int64_t telemetry_create_alert(const char* metricName, uint32_t alertLevel, 
                                   double triggerValue, int32_t enabled);
    int32_t telemetry_export_json(char* buffer, uint32_t bufferSize);
    int32_t telemetry_export_csv(char* buffer, uint32_t bufferSize);
    int32_t telemetry_export_prometheus(char* buffer, uint32_t bufferSize);
    int32_t telemetry_reset();

    // Theme Animation Functions
    int32_t animation_system_init();
    int64_t animation_create(uint32_t fromColor, uint32_t toColor, int64_t durationMs);
    int32_t animation_start(int64_t animationId);
    int32_t animation_stop(int64_t animationId);
    int32_t animation_update();  // Returns number of active animations
    uint32_t animation_interpolate_color(uint32_t fromColor, uint32_t toColor, 
                                         double progress, int32_t interpolationMode);
    int32_t animation_set_easing(int64_t animationId, int32_t easingType);
    int32_t animation_add_keyframe(int64_t animationId, int64_t timestampMs, 
                                   uint32_t keyframeColor);
    double animation_get_progress(int64_t animationId);
    int32_t animation_is_active(int64_t animationId);
    int32_t animation_destroy(int64_t animationId);

    // Unified Production Systems Interface
    int32_t production_systems_init();
    int64_t production_start_ci_job(const char* jobName, uint32_t stageCount, void* stagesArray);
    int32_t production_execute_pipeline_stage(int64_t jobId, uint32_t stageIdx);
    int64_t production_track_inference_request(const char* modelName, uint32_t promptTokens,
                                              uint32_t completionTokens, int64_t latencyMs,
                                              int32_t success);
    int32_t production_animate_theme_transition(const char* fromTheme, const char* toTheme,
                                               int64_t durationMs, uint32_t colorCount, 
                                               void* colorsArray);
    int32_t production_export_metrics(int32_t format, char* buffer, uint32_t bufferSize);
    int64_t production_set_alert(const char* metricName, uint32_t alertLevel, 
                                double triggerValue);
    void* production_get_system_status();  // Returns SYSTEM_STATUS*
    int32_t production_shutdown();
}

// ============================================================================
// C++ WRAPPER CLASSES
// ============================================================================

/**
 * @class PipelineExecutorBridge
 * @brief C++/Qt wrapper for MASM pipeline executor
 */
class PipelineExecutorBridge : public QObject {
    Q_OBJECT

public:
    enum JobStatus {
        QUEUED = 0,
        RUNNING = 1,
        COMPLETED = 2,
        FAILED = 3,
        CANCELLED = 4,
        RETRY = 5
    };

    enum StageType {
        VCS = 0,
        DOCKER = 1,
        KUBERNETES = 2,
        NOTIFICATION = 3
    };

    explicit PipelineExecutorBridge(QObject* parent = nullptr);
    ~PipelineExecutorBridge();

    // Initialize system
    bool initialize();

    // Job management
    int64_t createJob(const QString& jobName, const QStringList& stages);
    bool queueJob(int64_t jobId);
    bool executeStage(int64_t jobId, uint32_t stageIdx);
    JobStatus getJobStatus(int64_t jobId);
    bool cancelJob(int64_t jobId);
    bool retryJob(int64_t jobId);

    // VCS operations
    bool executeVcsStage(int64_t jobId, const QString& command);

    // Docker operations
    bool executeDockerStage(int64_t jobId, const QString& imageName);

    // Kubernetes operations
    bool executeK8sDeploy(int64_t jobId, const QString& manifest);

    // Notifications
    bool notifyCompletion(int64_t jobId, const QString& notificationType);

    // Artifacts
    bool cleanupArtifacts();

    // Metrics
    QJsonObject getMetrics();

signals:
    void jobCreated(int64_t jobId);
    void jobQueued(int64_t jobId);
    void stageStarted(int64_t jobId, uint32_t stageIdx);
    void stageCompleted(int64_t jobId, uint32_t stageIdx, bool success);
    void jobCompleted(int64_t jobId, bool success);
    void jobFailed(int64_t jobId, const QString& error);
    void pipelineError(const QString& errorMsg);

private:
    QString statusToString(JobStatus status) const;
};

/**
 * @class TelemetryVisualizationBridge
 * @brief C++/Qt wrapper for MASM telemetry system
 */
class TelemetryVisualizationBridge : public QObject {
    Q_OBJECT

public:
    enum AlertLevel {
        INFO = 0,
        WARNING = 1,
        CRITICAL = 2
    };

    enum ExportFormat {
        JSON_FORMAT = 0,
        CSV_FORMAT = 1,
        PROMETHEUS_FORMAT = 2
    };

    explicit TelemetryVisualizationBridge(QObject* parent = nullptr);
    ~TelemetryVisualizationBridge();

    // Initialize system
    bool initialize();

    // Request tracking
    int64_t startRequest(const QString& modelName, uint32_t promptTokens);
    bool endRequest(int64_t requestId, uint32_t completionTokens, 
                   int64_t latencyMs, bool success);
    bool recordToken(int64_t requestId, int32_t tokenCount);

    // Resource tracking
    bool recordMemory(uint64_t bytesUsed);
    bool recordGpuUsage(uint32_t utilizationPercent);
    bool recordEvent(const QString& eventName, const QString& details);

    // Metrics queries
    QJsonObject getMetrics();
    QJsonObject getPercentiles();
    QJsonArray getModelPerformance();

    // Alerting
    bool createAlert(const QString& metricName, AlertLevel level, 
                    double triggerValue, bool enabled = true);

    // Export
    QString exportMetrics(ExportFormat format);
    bool exportMetricsToFile(ExportFormat format, const QString& filePath);

    // System control
    bool reset();

signals:
    void requestStarted(int64_t requestId);
    void requestCompleted(int64_t requestId, int64_t latencyMs);
    void metricsUpdated();
    void alertTriggered(const QString& metricName, double value);
    void telemetryError(const QString& errorMsg);

private:
    QString formatToString(ExportFormat format) const;
    QString timelineToJson();
    QString timelineToCSV();
    QString timelineToPrometheus();
};

/**
 * @class ThemeAnimationBridge
 * @brief C++/Qt wrapper for MASM animation system
 */
class ThemeAnimationBridge : public QObject {
    Q_OBJECT

public:
    enum EasingType {
        LINEAR = 0,
        EASE_IN = 1,
        EASE_OUT = 2,
        EASE_IN_OUT = 3,
        QUAD = 4,
        CUBIC = 5,
        QUART = 6,
        QUINT = 7,
        SINE = 8,
        ELASTIC = 9,
        BOUNCE = 10,
        BACK = 11
    };

    enum InterpolationMode {
        RGB_MODE = 0,
        HSV_MODE = 1,
        LAB_MODE = 2,
        LRGB_MODE = 3
    };

    explicit ThemeAnimationBridge(QObject* parent = nullptr);
    ~ThemeAnimationBridge();

    // Initialize system
    bool initialize();

    // Animation creation and control
    int64_t createAnimation(uint32_t fromColor, uint32_t toColor, int64_t durationMs);
    bool startAnimation(int64_t animationId);
    bool stopAnimation(int64_t animationId);
    bool destroyAnimation(int64_t animationId);

    // Animation configuration
    bool setEasing(int64_t animationId, EasingType easingType);
    bool setInterpolationMode(int64_t animationId, InterpolationMode mode);

    // Keyframe support
    bool addKeyframe(int64_t animationId, int64_t timestampMs, uint32_t keyframeColor);

    // Animation queries
    double getProgress(int64_t animationId);
    bool isActive(int64_t animationId);

    // Update (call regularly in main loop)
    int32_t updateAnimations();

    // Color interpolation (utility)
    static uint32_t InterpolateColor(uint32_t fromColor, uint32_t toColor, 
                                     double progress, InterpolationMode mode);

    // Convenience: Animate all theme colors
    bool animateThemeTransition(const QString& fromTheme, const QString& toTheme, 
                               int64_t durationMs);

signals:
    void animationCreated(int64_t animationId);
    void animationStarted(int64_t animationId);
    void animationProgress(int64_t animationId, double progress);
    void animationCompleted(int64_t animationId);
    void animationStopped(int64_t animationId);
    void animationError(const QString& errorMsg);

public slots:
    void onMainLoopTick();  // Connect to application's main loop

private:
    void emitAnimationSignals();
};

/**
 * @class ProductionSystemsBridge
 * @brief Unified coordinator for all three production systems
 */
class ProductionSystemsBridge : public QObject {
    Q_OBJECT

public:
    explicit ProductionSystemsBridge(QObject* parent = nullptr);
    ~ProductionSystemsBridge();

    // Initialization
    bool initialize();
    bool shutdown();

    // Access to individual subsystems
    PipelineExecutorBridge* pipelineExecutor() const { return m_pipeline; }
    TelemetryVisualizationBridge* telemetry() const { return m_telemetry; }
    ThemeAnimationBridge* animations() const { return m_animations; }

    // Unified high-level operations
    int64_t startCIJob(const QString& jobName, const QStringList& stages);
    bool executeCIStage(int64_t jobId, uint32_t stageIdx);
    
    int64_t trackInferenceRequest(const QString& modelName, uint32_t promptTokens,
                                 uint32_t completionTokens, int64_t latencyMs, bool success);
    
    bool animateThemeTransition(const QString& fromTheme, const QString& toTheme, 
                               int64_t durationMs);

    QJsonObject exportAllMetrics();
    bool exportMetricsToFile(TelemetryVisualizationBridge::ExportFormat format, 
                            const QString& filePath);

    QJsonObject getSystemStatus();

signals:
    void systemInitialized();
    void systemShutdown();
    void errorOccurred(const QString& subsystem, const QString& error);
    void statusChanged(const QJsonObject& status);

private:
    PipelineExecutorBridge* m_pipeline;
    TelemetryVisualizationBridge* m_telemetry;
    ThemeAnimationBridge* m_animations;
    
    void connectSignals();
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Convert RGBA color integer to QColor
 */
inline QColor colorFromRGBA(uint32_t rgba) {
    uint8_t r = (rgba >> 16) & 0xFF;
    uint8_t g = (rgba >> 8) & 0xFF;
    uint8_t b = rgba & 0xFF;
    uint8_t a = (rgba >> 24) & 0xFF;
    return QColor(r, g, b, a);
}

/**
 * Convert QColor to RGBA color integer
 */
inline uint32_t colorToRGBA(const QColor& color) {
    uint32_t r = color.red() & 0xFF;
    uint32_t g = color.green() & 0xFF;
    uint32_t b = color.blue() & 0xFF;
    uint32_t a = color.alpha() & 0xFF;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

#endif // PRODUCTION_SYSTEMS_BRIDGE_HPP
