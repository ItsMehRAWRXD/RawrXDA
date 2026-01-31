#include "health_check_server.hpp"
#include "inference_engine.hpp"


#include <algorithm>
#include <cmath>

HealthCheckServer::HealthCheckServer(InferenceEngine* engine, void* parent)
    : void(parent), m_engine(engine) {
    (m_engine);
    
    // Structured logging with timestamp
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "HealthCheckServer";
    logEntry["event"] = "initialized";
    logEntry["engine_ptr"] = std::string::number(reinterpret_cast<quintptr>(m_engine), 16);
    
}

HealthCheckServer::~HealthCheckServer() {
    stopServer();
    
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "HealthCheckServer";
    logEntry["event"] = "destroyed";
    logEntry["total_requests_served"] = (qint64)m_metrics.total_requests;
    
}

bool HealthCheckServer::startServer(quint16 port) {
    std::chrono::steady_clock timer;
    timer.start();
    
    m_server = std::make_unique<void*>(this);
    
    if (!m_server->listen(QHostAddress::Any, port)) {
        void* logEntry;
        logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
        logEntry["level"] = "ERROR";
        logEntry["component"] = "HealthCheckServer";
        logEntry["event"] = "server_start_failed";
        logEntry["port"] = port;
        logEntry["error"] = m_server->errorString();
        
        
        m_server.reset();
        return false;
    }
// Qt connect removed
    qint64 startup_time_ms = timer.elapsed();
    
    // Production startup log
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "HealthCheckServer";
    logEntry["event"] = "server_started";
    logEntry["port"] = port;
    logEntry["startup_time_ms"] = startup_time_ms;
    
    void* endpoints;
    endpoints.append(void*{{"method", "GET"}, {"path", "/health"}, {"description", "Server health status"}});
    endpoints.append(void*{{"method", "GET"}, {"path", "/ready"}, {"description", "Readiness probe"}});
    endpoints.append(void*{{"method", "GET"}, {"path", "/metrics"}, {"description", "JSON performance metrics"}});
    endpoints.append(void*{{"method", "GET"}, {"path", "/metrics/prometheus"}, {"description", "Prometheus format metrics"}});
    endpoints.append(void*{{"method", "GET"}, {"path", "/model"}, {"description", "Model information"}});
    endpoints.append(void*{{"method", "GET"}, {"path", "/gpu"}, {"description", "GPU status"}});
    
    logEntry["endpoints"] = endpoints;
    
    
    return true;
}

void HealthCheckServer::stopServer() {
    if (m_server) {
        m_server->close();
        m_server.reset();
        
        void* logEntry;
        logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
        logEntry["level"] = "INFO";
        logEntry["component"] = "HealthCheckServer";
        logEntry["event"] = "server_stopped";
        logEntry["total_requests"] = (qint64)m_metrics.total_requests;
        logEntry["success_rate"] = m_metrics.total_requests > 0 ? 
            (100.0 * m_metrics.successful_requests / m_metrics.total_requests) : 0.0;
        
    }
}

void HealthCheckServer::onNewConnection() {
    while (void** socket = m_server->nextPendingConnection()) {
// Qt connect removed
// Qt connect removed
    }
}

void HealthCheckServer::onReadyRead() {
    std::chrono::steady_clock requestTimer;
    requestTimer.start();
    
    void** socket = qobject_cast<void**>(sender());
    if (!socket) return;
    
    std::string request = std::string::fromUtf8(socket->readAll());
    std::string method = extractMethod(request);
    std::string path = extractPath(request);
    std::string requestId = generateRequestId();
    
    logRequest(requestId, method, path);
    requestReceived(method, path);
    
    std::string responseBody;
    int statusCode = 200;
    std::string contentType = "application/json";
    
    // Route requests
    try {
        if (method == "GET" && path == "/health") {
            responseBody = createHealthJson();
        } else if (method == "GET" && path == "/ready") {
            responseBody = createReadyJson();
        } else if (method == "GET" && path == "/metrics") {
            responseBody = createMetricsJson();
        } else if (method == "GET" && path == "/metrics/prometheus") {
            responseBody = createPrometheusMetrics();
            contentType = "text/plain; version=0.0.4";
        } else if (method == "GET" && path == "/model") {
            responseBody = createModelJson();
        } else if (method == "GET" && path == "/gpu") {
            responseBody = createGPUJson();
        } else {
            statusCode = 404;
            responseBody = std::string("{\"error\": \"Not found\", \"path\": \"%1\"}");
        }
    } catch (const std::exception& ex) {
        statusCode = 500;
        responseBody = std::string("{\"error\": \"Internal server error\", \"message\": \"%1\"}"));
        
        void* errorLog;
        errorLog["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
        errorLog["level"] = "ERROR";
        errorLog["component"] = "HealthCheckServer";
        errorLog["event"] = "request_exception";
        errorLog["request_id"] = requestId;
        errorLog["exception"] = ex.what();
        
    }
    
    std::string response = buildHttpResponse(statusCode, contentType, responseBody);
    socket->write(response.toUtf8());
    socket->disconnectFromHost();
    
    double latency_ms = requestTimer.elapsed();
    logResponse(requestId, statusCode, latency_ms);
    updateMetrics(statusCode >= 200 && statusCode < 300, latency_ms);
    
    requestCompleted(method, path, statusCode, latency_ms);
}

void HealthCheckServer::onDisconnected() {
    void** socket = qobject_cast<void**>(sender());
    if (socket) {
        socket->deleteLater();
    }
}

std::string HealthCheckServer::createHealthJson() {
    auto health = m_engine->getHealthStatus();
    
    void* json;
    json["status"] = health.inference_ready ? "healthy" : "degraded";
    json["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    
    void* system;
    system["model_loaded"] = health.model_loaded;
    system["gpu_available"] = health.gpu_available;
    system["inference_ready"] = health.inference_ready;
    json["system"] = system;
    
    void* memory;
    memory["total_vram_mb"] = (int)health.total_vram_mb;
    memory["used_vram_mb"] = (int)health.used_vram_mb;
    memory["available_vram_mb"] = (int)(health.total_vram_mb - health.used_vram_mb);
    memory["utilization_percent"] = health.total_vram_mb > 0 ? 
        (100.0 * health.used_vram_mb / health.total_vram_mb) : 0.0;
    json["memory"] = memory;
    
    void* latency;
    latency["avg_ms"] = health.avg_latency_ms;
    latency["p50_ms"] = health.avg_latency_ms; // Approximation
    latency["p95_ms"] = health.p95_latency_ms;
    latency["p99_ms"] = health.p99_latency_ms;
    json["latency"] = latency;
    
    void* queue;
    queue["pending_requests"] = health.pending_requests;
    queue["total_processed"] = (qint64)health.total_requests_processed;
    json["queue"] = queue;
    
    if (!health.last_error.isEmpty()) {
        json["last_error"] = health.last_error;
    }
    
    // SLA compliance (from PRODUCTION_CONFIGURATION_GUIDE.md)
    void* sla;
    sla["p50_target_ms"] = 50;
    sla["p95_target_ms"] = 100;
    sla["p99_target_ms"] = 200;
    sla["p50_met"] = health.avg_latency_ms < 50;
    sla["p95_met"] = health.p95_latency_ms < 100;
    sla["p99_met"] = health.p99_latency_ms < 200;
    json["sla"] = sla;
    
    return void*(json).toJson(void*::Compact);
}

std::string HealthCheckServer::createReadyJson() {
    auto health = m_engine->getHealthStatus();
    
    void* json;
    json["ready"] = health.model_loaded && health.inference_ready;
    json["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    
    void* checks;
    checks.append(void*{{"name", "model_loaded"}, {"passed", health.model_loaded}});
    checks.append(void*{{"name", "inference_ready"}, {"passed", health.inference_ready}});
    checks.append(void*{{"name", "gpu_available"}, {"passed", health.gpu_available}});
    
    json["checks"] = checks;
    
    return void*(json).toJson(void*::Compact);
}

std::string HealthCheckServer::createMetricsJson() {
    auto health = m_engine->getHealthStatus();
    double tps = m_engine->getTokensPerSecond();
    
    void* json;
    json["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    
    void* performance;
    performance["avg_latency_ms"] = health.avg_latency_ms;
    performance["p50_latency_ms"] = m_metrics.avg_response_time_ms;
    performance["p95_latency_ms"] = m_metrics.p95_response_time_ms;
    performance["p99_latency_ms"] = m_metrics.p99_response_time_ms;
    performance["tokens_per_second"] = tps;
    performance["throughput_rps"] = m_metrics.total_requests > 0 ? 
        (m_metrics.total_requests / 60.0) : 0.0; // Rough estimate
    json["performance"] = performance;
    
    void* requests;
    requests["total_processed"] = (qint64)m_metrics.total_requests;
    requests["successful"] = (qint64)m_metrics.successful_requests;
    requests["failed"] = (qint64)m_metrics.failed_requests;
    requests["error_rate"] = m_metrics.total_requests > 0 ? 
        (100.0 * m_metrics.failed_requests / m_metrics.total_requests) : 0.0;
    requests["pending"] = health.pending_requests;
    json["requests"] = requests;
    
    void* gpu;
    gpu["total_vram_mb"] = (int)health.total_vram_mb;
    gpu["used_vram_mb"] = (int)health.used_vram_mb;
    gpu["utilization_percent"] = health.total_vram_mb > 0 ? 
        (100.0 * health.used_vram_mb / health.total_vram_mb) : 0.0;
    json["gpu"] = gpu;
    
    return void*(json).toJson(void*::Compact);
}

std::string HealthCheckServer::createPrometheusMetrics() {
    auto health = m_engine->getHealthStatus();
    double tps = m_engine->getTokensPerSecond();
    
    std::string metrics;
    metrics += "# HELP rawrxd_requests_total Total number of requests\n";
    metrics += "# TYPE rawrxd_requests_total counter\n";
    metrics += std::string("rawrxd_requests_total %1\n");
    
    metrics += "# HELP rawrxd_requests_successful Successful requests\n";
    metrics += "# TYPE rawrxd_requests_successful counter\n";
    metrics += std::string("rawrxd_requests_successful %1\n");
    
    metrics += "# HELP rawrxd_requests_failed Failed requests\n";
    metrics += "# TYPE rawrxd_requests_failed counter\n";
    metrics += std::string("rawrxd_requests_failed %1\n");
    
    metrics += "# HELP rawrxd_latency_ms Request latency in milliseconds\n";
    metrics += "# TYPE rawrxd_latency_ms summary\n";
    metrics += std::string("rawrxd_latency_ms{quantile=\"0.5\"} %1\n");
    metrics += std::string("rawrxd_latency_ms{quantile=\"0.95\"} %1\n");
    metrics += std::string("rawrxd_latency_ms{quantile=\"0.99\"} %1\n");
    
    metrics += "# HELP rawrxd_gpu_memory_used_bytes GPU memory used in bytes\n";
    metrics += "# TYPE rawrxd_gpu_memory_used_bytes gauge\n";
    metrics += std::string("rawrxd_gpu_memory_used_bytes %1\n");
    
    metrics += "# HELP rawrxd_gpu_memory_total_bytes Total GPU memory in bytes\n";
    metrics += "# TYPE rawrxd_gpu_memory_total_bytes gauge\n";
    metrics += std::string("rawrxd_gpu_memory_total_bytes %1\n");
    
    metrics += "# HELP rawrxd_tokens_per_second Token generation rate\n";
    metrics += "# TYPE rawrxd_tokens_per_second gauge\n";
    metrics += std::string("rawrxd_tokens_per_second %1\n");
    
    metrics += "# HELP rawrxd_pending_requests Number of pending requests\n";
    metrics += "# TYPE rawrxd_pending_requests gauge\n";
    metrics += std::string("rawrxd_pending_requests %1\n");
    
    return metrics;
}

std::string HealthCheckServer::createModelJson() {
    auto health = m_engine->getHealthStatus();
    
    void* json;
    json["loaded"] = health.model_loaded;
    json["inference_ready"] = health.inference_ready;
    
    // Production model metadata (should come from actual model config)
    json["name"] = "RawrXD-Production";
    json["version"] = "2.0-hardened";
    json["architecture"] = "transformer";
    json["layers"] = 32;
    json["embedding_dim"] = 4096;
    json["vocab_size"] = 32000;
    json["context_length"] = 2048;
    json["quantization"] = "int8";
    
    return void*(json).toJson(void*::Compact);
}

std::string HealthCheckServer::createGPUJson() {
    auto health = m_engine->getHealthStatus();
    
    void* json;
    json["available"] = health.gpu_available;
    json["total_vram_mb"] = (int)health.total_vram_mb;
    json["used_vram_mb"] = (int)health.used_vram_mb;
    json["available_vram_mb"] = (int)(health.total_vram_mb - health.used_vram_mb);
    
    double utilization = (health.total_vram_mb > 0) ? 
        (100.0 * health.used_vram_mb / health.total_vram_mb) : 0.0;
    json["utilization_percent"] = utilization;
    
    // Production target: 80-95% GPU utilization
    json["utilization_target_min"] = 80;
    json["utilization_target_max"] = 95;
    json["utilization_optimal"] = (utilization >= 80 && utilization <= 95);
    
    // GPU information (should come from actual hardware detection)
    json["backend"] = "Vulkan";
    json["device_type"] = "NVIDIA";
    json["compute_capability"] = "8.0";
    json["driver_version"] = "unknown";
    
    return void*(json).toJson(void*::Compact);
}

std::string HealthCheckServer::buildHttpResponse(int statusCode, const std::string& contentType,
                                             const std::string& body) {
    std::string statusText;
    switch (statusCode) {
        case 200: statusText = "OK"; break;
        case 404: statusText = "Not Found"; break;
        case 500: statusText = "Internal Server Error"; break;
        default: statusText = "Unknown"; break;
    }
    
    std::string response = std::string("HTTP/1.1 %1 %2\r\n")
        
        ;
    
    response += std::string("Content-Type: %1\r\n");
    response += std::string("Content-Length: %1\r\n"));
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "X-RawrXD-Version: 2.0\r\n";
    response += "\r\n";
    response += body;
    
    return response;
}

std::string HealthCheckServer::extractPath(const std::string& request) {
    std::vector<std::string> lines = request.split("\r\n");
    if (lines.isEmpty()) return "/";
    
    std::vector<std::string> parts = lines[0].split(" ");
    if (parts.size() < 2) return "/";
    
    return parts[1];
}

std::string HealthCheckServer::extractMethod(const std::string& request) {
    std::vector<std::string> lines = request.split("\r\n");
    if (lines.isEmpty()) return "GET";
    
    std::vector<std::string> parts = lines[0].split(" ");
    if (parts.isEmpty()) return "GET";
    
    return parts[0];
}

std::string HealthCheckServer::generateRequestId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

void HealthCheckServer::logRequest(const std::string& requestId, const std::string& method, const std::string& path) {
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = "DEBUG";
    logEntry["component"] = "HealthCheckServer";
    logEntry["event"] = "request_received";
    logEntry["request_id"] = requestId;
    logEntry["method"] = method;
    logEntry["path"] = path;
    
}

void HealthCheckServer::logResponse(const std::string& requestId, int statusCode, double latency_ms) {
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = (statusCode >= 400) ? "ERROR" : "DEBUG";
    logEntry["component"] = "HealthCheckServer";
    logEntry["event"] = "request_completed";
    logEntry["request_id"] = requestId;
    logEntry["status_code"] = statusCode;
    logEntry["latency_ms"] = latency_ms;
    
    if (statusCode >= 400) {
    } else {
    }
}

void HealthCheckServer::updateMetrics(bool success, double latency_ms) {
    m_metrics.total_requests++;
    if (success) {
        m_metrics.successful_requests++;
    } else {
        m_metrics.failed_requests++;
    }
    
    m_latency_samples.push_back(latency_ms);
    
    // Keep last 1000 samples for percentile calculation
    if (m_latency_samples.size() > 1000) {
        m_latency_samples.erase(m_latency_samples.begin());
    }
    
    calculatePercentiles();
    
    m_metrics.last_request_time = std::chrono::system_clock::now();
}

void HealthCheckServer::calculatePercentiles() {
    if (m_latency_samples.empty()) return;
    
    std::vector<double> sorted = m_latency_samples;
    std::sort(sorted.begin(), sorted.end());
    
    size_t n = sorted.size();
    
    // Calculate average
    double sum = 0.0;
    for (double val : sorted) {
        sum += val;
    }
    m_metrics.avg_response_time_ms = sum / n;
    
    // Calculate P95
    size_t p95_idx = static_cast<size_t>(std::ceil(0.95 * n)) - 1;
    if (p95_idx < n) {
        m_metrics.p95_response_time_ms = sorted[p95_idx];
    }
    
    // Calculate P99
    size_t p99_idx = static_cast<size_t>(std::ceil(0.99 * n)) - 1;
    if (p99_idx < n) {
        m_metrics.p99_response_time_ms = sorted[p99_idx];
    }
}

