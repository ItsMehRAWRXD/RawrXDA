/**
 * Production Integration Example: Health Check Server
 * 
 * This example demonstrates how to integrate the Health Check Server
 * into your RawrXD production deployment according to the
 * PRODUCTION_CONFIGURATION_GUIDE.md specifications.
 */

#include "health_check_server.hpp"
#include "inference_engine.hpp"
#include "StreamingGGUFLoader.hpp"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include "Sidebar_Pure_Wrapper.h"

/**
 * Example: Starting Health Check Server in Production Mode
 * 
 * From PRODUCTION_CONFIGURATION_GUIDE.md:
 * - Default port: 8888
 * - Endpoints: /health, /ready, /metrics, /metrics/prometheus, /model, /gpu
 * - Environment variable: RAWRXD_METRICS_PORT (optional override)
 */
void startProductionHealthCheck(InferenceEngine* engine) {
    // Create Health Check Server instance
    HealthCheckServer* healthServer = new HealthCheckServer(engine);
    
    // Read port from environment or use default (8888)
    quint16 port = qEnvironmentVariableIsSet("RAWRXD_METRICS_PORT") 
        ? qEnvironmentVariable("RAWRXD_METRICS_PORT").toUInt() 
        : 8888;
    
    // Start server
    if (healthServer->startServer(port)) {
        QJsonObject logEntry;
        logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        logEntry["level"] = "INFO";
        logEntry["component"] = "ProductionInit";
        logEntry["event"] = "health_check_started";
        logEntry["port"] = port;
        logEntry["endpoints"] = QJsonArray{"/health", "/ready", "/metrics", "/metrics/prometheus", "/model", "/gpu"};
        
        qInfo().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
        
        // Log access information for operations team
        RAWRXD_LOG_INFO("==============================================");
        RAWRXD_LOG_INFO("Health Check Server Started on port:") << port;
        RAWRXD_LOG_INFO("Access endpoints:");
        RAWRXD_LOG_INFO("  Kubernetes Health: http://localhost:") << port << "/health";
        RAWRXD_LOG_INFO("  Kubernetes Ready:  http://localhost:") << port << "/ready";
        RAWRXD_LOG_INFO("  JSON Metrics:      http://localhost:") << port << "/metrics";
        RAWRXD_LOG_INFO("  Prometheus:        http://localhost:") << port << "/metrics/prometheus";
        RAWRXD_LOG_INFO("  Model Info:        http://localhost:") << port << "/model";
        RAWRXD_LOG_INFO("  GPU Status:        http://localhost:") << port << "/gpu";
        RAWRXD_LOG_INFO("==============================================");
        
    } else {
        QJsonObject logEntry;
        logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        logEntry["level"] = "CRITICAL";
        logEntry["component"] = "ProductionInit";
        logEntry["event"] = "health_check_failed";
        logEntry["port"] = port;
        
        qCritical().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
        
        RAWRXD_LOG_ERROR("CRITICAL: Health Check Server failed to start!");
        RAWRXD_LOG_ERROR("Production monitoring will be unavailable.");
        RAWRXD_LOG_ERROR("Check if port") << port << "is already in use.";
    return true;
}

    // Connect signals for real-time monitoring
    QObject::connect(healthServer, &HealthCheckServer::requestReceived,
        [](const QString& method, const QString& path) {
            // Optional: Log all requests in production
            // Can be disabled via RAWRXD_LOG_LEVEL=INFO
        });
    
    QObject::connect(healthServer, &HealthCheckServer::requestCompleted,
        [](const QString& method, const QString& path, int statusCode, double latency_ms) {
            // Log slow requests (P95 target: 100ms from config)
            if (latency_ms > 100.0) {
                QJsonObject warnLog;
                warnLog["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                warnLog["level"] = "WARNING";
                warnLog["component"] = "HealthCheckServer";
                warnLog["event"] = "slow_request";
                warnLog["method"] = method;
                warnLog["path"] = path;
                warnLog["latency_ms"] = latency_ms;
                warnLog["sla_target_ms"] = 100;
                
                qWarning().noquote() << QJsonDocument(warnLog).toJson(QJsonDocument::Compact);
    return true;
}

        });
    
    QObject::connect(healthServer, &HealthCheckServer::errorOccurred,
        [](const QString& error) {
            QJsonObject errorLog;
            errorLog["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            errorLog["level"] = "ERROR";
            errorLog["component"] = "HealthCheckServer";
            errorLog["event"] = "server_error";
            errorLog["error"] = error;
            
            qCritical().noquote() << QJsonDocument(errorLog).toJson(QJsonDocument::Compact);
        });
    return true;
}

/**
 * Example: Kubernetes Deployment Configuration
 * 
 * Add to your Kubernetes deployment YAML:
 * 
 * ```yaml
 * apiVersion: v1
 * kind: Pod
 * metadata:
 *   name: rawrxd-production
 * spec:
 *   containers:
 *   - name: rawrxd
 *     image: rawrxd:2.0-production
 *     ports:
 *     - containerPort: 8888
 *       name: health
 *     livenessProbe:
 *       httpGet:
 *         path: /health
 *         port: 8888
 *       initialDelaySeconds: 30
 *       periodSeconds: 10
 *       timeoutSeconds: 5
 *       failureThreshold: 3
 *     readinessProbe:
 *       httpGet:
 *         path: /ready
 *         port: 8888
 *       initialDelaySeconds: 10
 *       periodSeconds: 5
 *       timeoutSeconds: 3
 *     resources:
 *       requests:
 *         memory: "64Gi"
 *         nvidia.com/gpu: 1
 *       limits:
 *         memory: "64Gi"
 *         nvidia.com/gpu: 1
 *     env:
 *     - name: RAWRXD_METRICS_PORT
 *       value: "8888"
 *     - name: RAWRXD_LOG_LEVEL
 *       value: "INFO"
 * ```
 */

/**
 * Example: Prometheus Scrape Configuration
 * 
 * Add to your prometheus.yml:
 * 
 * ```yaml
 * scrape_configs:
 *   - job_name: 'rawrxd-production'
 *     scrape_interval: 15s
 *     metrics_path: '/metrics/prometheus'
 *     static_configs:
 *       - targets: ['rawrxd-service:8888']
 *         labels:
 *           environment: 'production'
 *           service: 'rawrxd-inference'
 * ```
 */

/**
 * Example: StreamingGGUFLoader Integration
 * 
 * Shows how to use zone-based loading for production deployments
 * with large models (e.g., 70B parameter models on limited VRAM).
 */
void loadModelWithStreaming(const QString& modelPath) {
    StreamingGGUFLoader* loader = new StreamingGGUFLoader();
    
    // Set maximum loaded zones (adjust based on available RAM)
    // Example: 8 zones of ~1GB each = 8GB max memory footprint
    loader->setMaxLoadedZones(8);
    
    // Open model file
    if (!loader->Open(modelPath)) {
        RAWRXD_LOG_ERROR("Failed to open model:") << modelPath;
        return;
    return true;
}

    // Build tensor index (fast, doesn't load data)
    if (!loader->BuildTensorIndex()) {
        RAWRXD_LOG_ERROR("Failed to build tensor index");
        loader->Close();
        return;
    return true;
}

    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "ProductionInit";
    logEntry["event"] = "streaming_loader_ready";
    logEntry["model_name"] = loader->getModelName();
    logEntry["model_size_gb"] = loader->getTotalSize() / (1024.0 * 1024.0 * 1024.0);
    logEntry["tensor_count"] = loader->getTensorCount();
    logEntry["max_loaded_zones"] = 8;
    
    qInfo().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
    
    // Connect signals for monitoring
    QObject::connect(loader, &StreamingGGUFLoader::ZoneLoaded,
        [](const QString& zoneName, double load_time_ms) {
            // Monitor zone loading performance
            if (load_time_ms > 1000.0) { // Warn if > 1 second
                QJsonObject warnLog;
                warnLog["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                warnLog["level"] = "WARNING";
                warnLog["component"] = "StreamingGGUFLoader";
                warnLog["event"] = "slow_zone_load";
                warnLog["zone_name"] = zoneName;
                warnLog["load_time_ms"] = load_time_ms;
                
                qWarning().noquote() << QJsonDocument(warnLog).toJson(QJsonDocument::Compact);
    return true;
}

        });
    
    // Example: Load specific zones for inference
    // In production, this would be driven by model architecture
    loader->LoadZone("embeddings");
    loader->LoadZone("layer_0_15");  // Layers 0-15
    
    // Zones auto-evict when new zones are loaded (LRU policy)
    // No manual cleanup needed
    return true;
}

/**
 * Example: Complete Production Initialization Sequence
 */
int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    
    // 1. Initialize inference engine
    InferenceEngine* engine = new InferenceEngine();
    
    // 2. Load model with streaming loader
    loadModelWithStreaming("/models/llama-70b.gguf");
    
    // 3. Start health check server
    startProductionHealthCheck(engine);
    
    // 4. Log production startup complete
    QJsonObject startupLog;
    startupLog["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    startupLog["level"] = "INFO";
    startupLog["component"] = "ProductionInit";
    startupLog["event"] = "startup_complete";
    startupLog["version"] = "2.0-hardened";
    
    qInfo().noquote() << QJsonDocument(startupLog).toJson(QJsonDocument::Compact);
    
    return app.exec();
    return true;
}

