#include "health_check_server.hpp"
#include "inference_engine.hpp"
#include <QTcpServer>
#include <QTcpSocket>
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <algorithm>
#include <cmath>

HealthCheckServer::HealthCheckServer(InferenceEngine* engine, QObject* parent)
    : QObject(parent), m_engine(engine) {
    Q_ASSERT(m_engine);
    
    // Structured logging with timestamp
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "HealthCheckServer";
    logEntry["event"] = "initialized";
    logEntry["engine_ptr"] = QString::number(reinterpret_cast<quintptr>(m_engine), 16);
    
    qInfo().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
}

HealthCheckServer::~HealthCheckServer() {
    stopServer();
    
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "HealthCheckServer";
    logEntry["event"] = "destroyed";
    logEntry["total_requests_served"] = (qint64)m_metrics.total_requests;
    
    qInfo().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
}

bool HealthCheckServer::startServer(quint16 port) {
    QElapsedTimer timer;
    timer.start();
    
    m_server = std::make_unique<QTcpServer>(this);
    
    if (!m_server->listen(QHostAddress::Any, port)) {
        QJsonObject logEntry;
        logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        logEntry["level"] = "ERROR";
        logEntry["component"] = "HealthCheckServer";
        logEntry["event"] = "server_start_failed";
        logEntry["port"] = port;
        logEntry["error"] = m_server->errorString();
        
        qCritical().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
        
        m_server.reset();
        return false;
    }
    
    connect(m_server.get(), &QTcpServer::newConnection, this, &HealthCheckServer::onNewConnection);
    
    qint64 startup_time_ms = timer.elapsed();
    
    // Production startup log
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = "INFO";
    logEntry["component"] = "HealthCheckServer";
    logEntry["event"] = "server_started";
    logEntry["port"] = port;
    logEntry["startup_time_ms"] = startup_time_ms;
    
    QJsonArray endpoints;
    endpoints.append(QJsonObject{{"method", "GET"}, {"path", "/health"}, {"description", "Server health status"}});
    endpoints.append(QJsonObject{{"method", "GET"}, {"path", "/ready"}, {"description", "Readiness probe"}});
    endpoints.append(QJsonObject{{"method", "GET"}, {"path", "/metrics"}, {"description", "JSON performance metrics"}});
    endpoints.append(QJsonObject{{"method", "GET"}, {"path", "/metrics/prometheus"}, {"description", "Prometheus format metrics"}});
    endpoints.append(QJsonObject{{"method", "GET"}, {"path", "/model"}, {"description", "Model information"}});
    endpoints.append(QJsonObject{{"method", "GET"}, {"path", "/gpu"}, {"description", "GPU status"}});
    
    logEntry["endpoints"] = endpoints;
    
    qInfo().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
    
    return true;
}

void HealthCheckServer::stopServer() {
    if (m_server) {
        m_server->close();
        m_server.reset();
        
        QJsonObject logEntry;
        logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        logEntry["level"] = "INFO";
        logEntry["component"] = "HealthCheckServer";
        logEntry["event"] = "server_stopped";
        logEntry["total_requests"] = (qint64)m_metrics.total_requests;
        logEntry["success_rate"] = m_metrics.total_requests > 0 ? 
            (100.0 * m_metrics.successful_requests / m_metrics.total_requests) : 0.0;
        
        qInfo().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
    }
}

void HealthCheckServer::onNewConnection() {
    while (QTcpSocket* socket = m_server->nextPendingConnection()) {
        connect(socket, &QTcpSocket::readyRead, this, &HealthCheckServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &HealthCheckServer::onDisconnected);
    }
}

void HealthCheckServer::onReadyRead() {
    QElapsedTimer requestTimer;
    requestTimer.start();
    
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    QString request = QString::fromUtf8(socket->readAll());
    QString method = extractMethod(request);
    QString path = extractPath(request);
    QString requestId = generateRequestId();
    
    logRequest(requestId, method, path);
    emit requestReceived(method, path);
    
    QString responseBody;
    int statusCode = 200;
    QString contentType = "application/json";
    
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
            responseBody = QString("{\"error\": \"Not found\", \"path\": \"%1\"}").arg(path);
        }
    } catch (const std::exception& ex) {
        statusCode = 500;
        responseBody = QString("{\"error\": \"Internal server error\", \"message\": \"%1\"}").arg(ex.what());
        
        QJsonObject errorLog;
        errorLog["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        errorLog["level"] = "ERROR";
        errorLog["component"] = "HealthCheckServer";
        errorLog["event"] = "request_exception";
        errorLog["request_id"] = requestId;
        errorLog["exception"] = ex.what();
        
        qCritical().noquote() << QJsonDocument(errorLog).toJson(QJsonDocument::Compact);
    }
    
    QString response = buildHttpResponse(statusCode, contentType, responseBody);
    socket->write(response.toUtf8());
    socket->disconnectFromHost();
    
    double latency_ms = requestTimer.elapsed();
    logResponse(requestId, statusCode, latency_ms);
    updateMetrics(statusCode >= 200 && statusCode < 300, latency_ms);
    
    emit requestCompleted(method, path, statusCode, latency_ms);
}

void HealthCheckServer::onDisconnected() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        socket->deleteLater();
    }
}

QString HealthCheckServer::createHealthJson() {
    auto health = m_engine->getHealthStatus();
    
    QJsonObject json;
    json["status"] = health.inference_ready ? "healthy" : "degraded";
    json["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonObject system;
    system["model_loaded"] = health.model_loaded;
    system["gpu_available"] = health.gpu_available;
    system["inference_ready"] = health.inference_ready;
    json["system"] = system;
    
    QJsonObject memory;
    memory["total_vram_mb"] = (int)health.total_vram_mb;
    memory["used_vram_mb"] = (int)health.used_vram_mb;
    memory["available_vram_mb"] = (int)(health.total_vram_mb - health.used_vram_mb);
    memory["utilization_percent"] = health.total_vram_mb > 0 ? 
        (100.0 * health.used_vram_mb / health.total_vram_mb) : 0.0;
    json["memory"] = memory;
    
    QJsonObject latency;
    latency["avg_ms"] = health.avg_latency_ms;
    latency["p50_ms"] = health.avg_latency_ms; // Approximation
    latency["p95_ms"] = health.p95_latency_ms;
    latency["p99_ms"] = health.p99_latency_ms;
    json["latency"] = latency;
    
    QJsonObject queue;
    queue["pending_requests"] = health.pending_requests;
    queue["total_processed"] = (qint64)health.total_requests_processed;
    json["queue"] = queue;
    
    if (!health.last_error.isEmpty()) {
        json["last_error"] = health.last_error;
    }
    
    // SLA compliance (from PRODUCTION_CONFIGURATION_GUIDE.md)
    QJsonObject sla;
    sla["p50_target_ms"] = 50;
    sla["p95_target_ms"] = 100;
    sla["p99_target_ms"] = 200;
    sla["p50_met"] = health.avg_latency_ms < 50;
    sla["p95_met"] = health.p95_latency_ms < 100;
    sla["p99_met"] = health.p99_latency_ms < 200;
    json["sla"] = sla;
    
    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

QString HealthCheckServer::createReadyJson() {
    auto health = m_engine->getHealthStatus();
    
    QJsonObject json;
    json["ready"] = health.model_loaded && health.inference_ready;
    json["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonArray checks;
    checks.append(QJsonObject{{"name", "model_loaded"}, {"passed", health.model_loaded}});
    checks.append(QJsonObject{{"name", "inference_ready"}, {"passed", health.inference_ready}});
    checks.append(QJsonObject{{"name", "gpu_available"}, {"passed", health.gpu_available}});
    
    json["checks"] = checks;
    
    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

QString HealthCheckServer::createMetricsJson() {
    auto health = m_engine->getHealthStatus();
    double tps = m_engine->getTokensPerSecond();
    
    QJsonObject json;
    json["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonObject performance;
    performance["avg_latency_ms"] = health.avg_latency_ms;
    performance["p50_latency_ms"] = m_metrics.avg_response_time_ms;
    performance["p95_latency_ms"] = m_metrics.p95_response_time_ms;
    performance["p99_latency_ms"] = m_metrics.p99_response_time_ms;
    performance["tokens_per_second"] = tps;
    performance["throughput_rps"] = m_metrics.total_requests > 0 ? 
        (m_metrics.total_requests / 60.0) : 0.0; // Rough estimate
    json["performance"] = performance;
    
    QJsonObject requests;
    requests["total_processed"] = (qint64)m_metrics.total_requests;
    requests["successful"] = (qint64)m_metrics.successful_requests;
    requests["failed"] = (qint64)m_metrics.failed_requests;
    requests["error_rate"] = m_metrics.total_requests > 0 ? 
        (100.0 * m_metrics.failed_requests / m_metrics.total_requests) : 0.0;
    requests["pending"] = health.pending_requests;
    json["requests"] = requests;
    
    QJsonObject gpu;
    gpu["total_vram_mb"] = (int)health.total_vram_mb;
    gpu["used_vram_mb"] = (int)health.used_vram_mb;
    gpu["utilization_percent"] = health.total_vram_mb > 0 ? 
        (100.0 * health.used_vram_mb / health.total_vram_mb) : 0.0;
    json["gpu"] = gpu;
    
    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

QString HealthCheckServer::createPrometheusMetrics() {
    auto health = m_engine->getHealthStatus();
    double tps = m_engine->getTokensPerSecond();
    
    QString metrics;
    metrics += "# HELP rawrxd_requests_total Total number of requests\n";
    metrics += "# TYPE rawrxd_requests_total counter\n";
    metrics += QString("rawrxd_requests_total %1\n").arg(m_metrics.total_requests);
    
    metrics += "# HELP rawrxd_requests_successful Successful requests\n";
    metrics += "# TYPE rawrxd_requests_successful counter\n";
    metrics += QString("rawrxd_requests_successful %1\n").arg(m_metrics.successful_requests);
    
    metrics += "# HELP rawrxd_requests_failed Failed requests\n";
    metrics += "# TYPE rawrxd_requests_failed counter\n";
    metrics += QString("rawrxd_requests_failed %1\n").arg(m_metrics.failed_requests);
    
    metrics += "# HELP rawrxd_latency_ms Request latency in milliseconds\n";
    metrics += "# TYPE rawrxd_latency_ms summary\n";
    metrics += QString("rawrxd_latency_ms{quantile=\"0.5\"} %1\n").arg(m_metrics.avg_response_time_ms);
    metrics += QString("rawrxd_latency_ms{quantile=\"0.95\"} %1\n").arg(m_metrics.p95_response_time_ms);
    metrics += QString("rawrxd_latency_ms{quantile=\"0.99\"} %1\n").arg(m_metrics.p99_response_time_ms);
    
    metrics += "# HELP rawrxd_gpu_memory_used_bytes GPU memory used in bytes\n";
    metrics += "# TYPE rawrxd_gpu_memory_used_bytes gauge\n";
    metrics += QString("rawrxd_gpu_memory_used_bytes %1\n").arg(health.used_vram_mb * 1024 * 1024);
    
    metrics += "# HELP rawrxd_gpu_memory_total_bytes Total GPU memory in bytes\n";
    metrics += "# TYPE rawrxd_gpu_memory_total_bytes gauge\n";
    metrics += QString("rawrxd_gpu_memory_total_bytes %1\n").arg(health.total_vram_mb * 1024 * 1024);
    
    metrics += "# HELP rawrxd_tokens_per_second Token generation rate\n";
    metrics += "# TYPE rawrxd_tokens_per_second gauge\n";
    metrics += QString("rawrxd_tokens_per_second %1\n").arg(tps);
    
    metrics += "# HELP rawrxd_pending_requests Number of pending requests\n";
    metrics += "# TYPE rawrxd_pending_requests gauge\n";
    metrics += QString("rawrxd_pending_requests %1\n").arg(health.pending_requests);
    
    return metrics;
}

QString HealthCheckServer::createModelJson() {
    auto health = m_engine->getHealthStatus();
    
    QJsonObject json;
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
    
    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

QString HealthCheckServer::createGPUJson() {
    auto health = m_engine->getHealthStatus();
    
    QJsonObject json;
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
    
    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

QString HealthCheckServer::buildHttpResponse(int statusCode, const QString& contentType,
                                             const QString& body) {
    QString statusText;
    switch (statusCode) {
        case 200: statusText = "OK"; break;
        case 404: statusText = "Not Found"; break;
        case 500: statusText = "Internal Server Error"; break;
        default: statusText = "Unknown"; break;
    }
    
    QString response = QString("HTTP/1.1 %1 %2\r\n")
        .arg(statusCode)
        .arg(statusText);
    
    response += QString("Content-Type: %1\r\n").arg(contentType);
    response += QString("Content-Length: %1\r\n").arg(body.length());
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "X-RawrXD-Version: 2.0\r\n";
    response += "\r\n";
    response += body;
    
    return response;
}

QString HealthCheckServer::extractPath(const QString& request) {
    QStringList lines = request.split("\r\n");
    if (lines.isEmpty()) return "/";
    
    QStringList parts = lines[0].split(" ");
    if (parts.size() < 2) return "/";
    
    return parts[1];
}

QString HealthCheckServer::extractMethod(const QString& request) {
    QStringList lines = request.split("\r\n");
    if (lines.isEmpty()) return "GET";
    
    QStringList parts = lines[0].split(" ");
    if (parts.isEmpty()) return "GET";
    
    return parts[0];
}

QString HealthCheckServer::generateRequestId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

void HealthCheckServer::logRequest(const QString& requestId, const QString& method, const QString& path) {
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = "DEBUG";
    logEntry["component"] = "HealthCheckServer";
    logEntry["event"] = "request_received";
    logEntry["request_id"] = requestId;
    logEntry["method"] = method;
    logEntry["path"] = path;
    
    qDebug().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
}

void HealthCheckServer::logResponse(const QString& requestId, int statusCode, double latency_ms) {
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["level"] = (statusCode >= 400) ? "ERROR" : "DEBUG";
    logEntry["component"] = "HealthCheckServer";
    logEntry["event"] = "request_completed";
    logEntry["request_id"] = requestId;
    logEntry["status_code"] = statusCode;
    logEntry["latency_ms"] = latency_ms;
    
    if (statusCode >= 400) {
        qWarning().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
    } else {
        qDebug().noquote() << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
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
