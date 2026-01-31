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
        void* logEntry;
        logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
        logEntry["level"] = "INFO";
        logEntry["component"] = "ProductionInit";
        logEntry["event"] = "health_check_started";
        logEntry["port"] = port;
        logEntry["endpoints"] = void*{"/health", "/ready", "/metrics", "/metrics/prometheus", "/model", "/gpu"};
        
        
        // Log access information for operations team
        
    } else {
        void* logEntry;
        logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
        logEntry["level"] = "CRITICAL";
        logEntry["component"] = "ProductionInit";
        logEntry["event"] = "health_check_failed";
        logEntry["port"] = port;
        
        
    }
    
    // Connect signals for real-time monitoring
    void {
            // Optional: Log all requests in production
            // Can be disabled via RAWRXD_LOG_LEVEL=INFO
        });
    
    void {
            // Log slow requests (P95 target: 100ms from config)
            if (latency_ms > 100.0) {
                void* warnLog;
                warnLog["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
                warnLog["level"] = "WARNING";
                warnLog["component"] = "HealthCheckServer";
                warnLog["event"] = "slow_request";
                warnLog["method"] = method;
                warnLog["path"] = path;
                warnLog["latency_ms"] = latency_ms;
                warnLog["sla_target_ms"] = 100;
                
            }
        });
    
    void {
            void* errorLog;
            errorLog["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
            errorLog["level"] = "ERROR";
            errorLog["component"] = "HealthCheckServer";
            errorLog["event"] = "server_error";
            errorLog["error"] = error;
            
        });
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
void loadModelWithStreaming(const std::string& modelPath) {
    StreamingGGUFLoader* loader = new StreamingGGUFLoader();
    
    // Set maximum loaded zones (adjust based on available RAM)
    // Example: 8 zones of ~1GB each = 8GB max memory footprint
    loader->setMaxLoadedZones(8);
    
    // Open model file
    if (!loader->Open(modelPath)) {
        return;
    }
    
    // Build tensor index (fast, doesn't load data)
    if (!loader->BuildTensorIndex()) {
        loader->Close();
        return;
    }
    
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "ProductionInit";
    logEntry["event"] = "streaming_loader_ready";
    logEntry["model_name"] = loader->getModelName();
    logEntry["model_size_gb"] = loader->getTotalSize() / (1024.0 * 1024.0 * 1024.0);
    logEntry["tensor_count"] = loader->getTensorCount();
    logEntry["max_loaded_zones"] = 8;
    
    
    // Connect signals for monitoring
    void {
            // Monitor zone loading performance
            if (load_time_ms > 1000.0) { // Warn if > 1 second
                void* warnLog;
                warnLog["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
                warnLog["level"] = "WARNING";
                warnLog["component"] = "StreamingGGUFLoader";
                warnLog["event"] = "slow_zone_load";
                warnLog["zone_name"] = zoneName;
                warnLog["load_time_ms"] = load_time_ms;
                
            }
        });
    
    // Example: Load specific zones for inference
    // In production, this would be driven by model architecture
    loader->LoadZone("embeddings");
    loader->LoadZone("layer_0_15");  // Layers 0-15
    
    // Zones auto-evict when new zones are loaded (LRU policy)
    // No manual cleanup needed
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
    void* startupLog;
    startupLog["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    startupLog["level"] = "INFO";
    startupLog["component"] = "ProductionInit";
    startupLog["event"] = "startup_complete";
    startupLog["version"] = "2.0-hardened";
    
    
    return app.exec();
}

