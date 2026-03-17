#include "../include/metrics.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

// Simple HTTP server for Prometheus metrics scraping
class MetricsHttpServer {
public:
    MetricsHttpServer(int port, Metrics* metrics)
        : m_port(port), m_metrics(metrics), m_running(false), m_socket(INVALID_SOCKET) {}
    
    ~MetricsHttpServer() { stop(); }
    
    bool start() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
        
        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET) return false;
        
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons((u_short)m_port);
        
        if (bind(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(m_socket);
            return false;
        }
        
        if (listen(m_socket, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(m_socket);
            return false;
        }
        
        m_running = true;
        m_thread = std::thread(&MetricsHttpServer::serverLoop, this);
        return true;
#else
        return false;
#endif
    }
    
    void stop() {
        m_running = false;
#ifdef _WIN32
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        if (m_thread.joinable()) m_thread.join();
        WSACleanup();
#endif
    }
    
    bool isRunning() const { return m_running; }
    
private:
    void serverLoop() {
#ifdef _WIN32
        while (m_running) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(m_socket, &readfds);
            
            timeval timeout = { 1, 0 };  // 1 second timeout
            int result = select(0, &readfds, nullptr, nullptr, &timeout);
            
            if (result > 0 && FD_ISSET(m_socket, &readfds)) {
                SOCKET clientSocket = accept(m_socket, nullptr, nullptr);
                if (clientSocket != INVALID_SOCKET) {
                    handleClient(clientSocket);
                    closesocket(clientSocket);
                }
            }
        }
#endif
    }
    
    void handleClient(SOCKET clientSocket) {
#ifdef _WIN32
        char buffer[4096];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            
            // Check if it's a GET /metrics request
            if (strstr(buffer, "GET /metrics") || strstr(buffer, "GET / ")) {
                std::string metricsData = m_metrics->exportPrometheus();
                
                std::ostringstream response;
                response << "HTTP/1.1 200 OK\r\n";
                response << "Content-Type: text/plain; version=0.0.4\r\n";
                response << "Content-Length: " << metricsData.size() << "\r\n";
                response << "\r\n";
                response << metricsData;
                
                std::string responseStr = response.str();
                send(clientSocket, responseStr.c_str(), (int)responseStr.size(), 0);
            } else {
                const char* notFound = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
                send(clientSocket, notFound, (int)strlen(notFound), 0);
            }
        }
#endif
    }
    
    int m_port;
    Metrics* m_metrics;
    std::atomic<bool> m_running;
    SOCKET m_socket;
    std::thread m_thread;
};

Metrics& Metrics::instance() {
    static Metrics inst;
    return inst;
}

Metrics::Metrics() 
    : m_latencyBuckets(8)
    , m_tokenLatencyBuckets(8)
    , m_requestLatencyBuckets(8)
{
    for (auto &b : m_latencyBuckets) b.store(0);
    for (auto &b : m_tokenLatencyBuckets) b.store(0);
    for (auto &b : m_requestLatencyBuckets) b.store(0);
}

Metrics::~Metrics() {
    stopHttpServer();
}

void Metrics::incMessagesProcessed() { ++m_messagesProcessed; }
void Metrics::incVoiceStart() { ++m_voiceStarts; }
void Metrics::incErrors() { ++m_errors; }
void Metrics::incModelCalls() { ++m_modelCalls; }
void Metrics::incTokensGenerated(size_t count) { m_tokensGenerated += count; }
void Metrics::incBytesStreamed(size_t bytes) { m_bytesStreamed += bytes; }

void Metrics::setActiveModels(int count) { m_activeModels.store(count); }
void Metrics::setMemoryUsageMB(double mb) { m_memoryUsageMB.store(mb); }
void Metrics::setGpuUsagePercent(double percent) { m_gpuUsagePercent.store(percent); }
void Metrics::setActiveConnections(int count) { m_activeConnections.store(count); }

size_t Metrics::getBucketIndex(int ms) const {
    if (ms < 10) return 0;
    if (ms < 20) return 1;
    if (ms < 50) return 2;
    if (ms < 100) return 3;
    if (ms < 200) return 4;
    if (ms < 500) return 5;
    if (ms < 1000) return 6;
    return 7;
}

void Metrics::observeModelCallMs(int ms) {
    ++m_latencyBuckets[getBucketIndex(ms)];
}

void Metrics::observeTokenLatencyMs(int ms) {
    ++m_tokenLatencyBuckets[getBucketIndex(ms)];
}

void Metrics::observeRequestLatencyMs(int ms) {
    ++m_requestLatencyBuckets[getBucketIndex(ms)];
}

void Metrics::incWithLabel(const std::string& metric, const std::string& label, size_t value) {
    std::lock_guard<std::mutex> lock(m_labelMutex);
    std::string key = metric + "{" + label + "}";
    m_labeledMetrics[key] += value;
}

Metrics::Snapshot Metrics::snapshot() const {
    Snapshot s;
    s.messagesProcessed = m_messagesProcessed.load();
    s.voiceStarts = m_voiceStarts.load();
    s.errors = m_errors.load();
    s.modelCalls = m_modelCalls.load();
    s.tokensGenerated = m_tokensGenerated.load();
    s.bytesStreamed = m_bytesStreamed.load();
    s.activeModels = m_activeModels.load();
    s.memoryUsageMB = m_memoryUsageMB.load();
    s.gpuUsagePercent = m_gpuUsagePercent.load();
    s.activeConnections = m_activeConnections.load();
    
    s.latencyBuckets.resize(m_latencyBuckets.size());
    for (size_t i = 0; i < m_latencyBuckets.size(); ++i) 
        s.latencyBuckets[i] = m_latencyBuckets[i].load();
    
    s.tokenLatencyBuckets.resize(m_tokenLatencyBuckets.size());
    for (size_t i = 0; i < m_tokenLatencyBuckets.size(); ++i) 
        s.tokenLatencyBuckets[i] = m_tokenLatencyBuckets[i].load();
    
    s.requestLatencyBuckets.resize(m_requestLatencyBuckets.size());
    for (size_t i = 0; i < m_requestLatencyBuckets.size(); ++i) 
        s.requestLatencyBuckets[i] = m_requestLatencyBuckets[i].load();
    
    {
        std::lock_guard<std::mutex> lock(m_labelMutex);
        for (const auto& [key, value] : m_labeledMetrics) {
            s.labeledMetrics[key] = value.load();
        }
    }
    
    return s;
}

bool Metrics::exportToFile(const std::string& path) const {
    auto s = snapshot();
    std::ofstream out(path);
    if (!out.is_open()) return false;
    
    out << "{\n";
    out << "  \"messagesProcessed\": " << s.messagesProcessed << ",\n";
    out << "  \"voiceStarts\": " << s.voiceStarts << ",\n";
    out << "  \"errors\": " << s.errors << ",\n";
    out << "  \"modelCalls\": " << s.modelCalls << ",\n";
    out << "  \"tokensGenerated\": " << s.tokensGenerated << ",\n";
    out << "  \"bytesStreamed\": " << s.bytesStreamed << ",\n";
    out << "  \"activeModels\": " << s.activeModels << ",\n";
    out << "  \"memoryUsageMB\": " << std::fixed << std::setprecision(2) << s.memoryUsageMB << ",\n";
    out << "  \"gpuUsagePercent\": " << std::fixed << std::setprecision(1) << s.gpuUsagePercent << ",\n";
    out << "  \"activeConnections\": " << s.activeConnections << ",\n";
    out << "  \"latencyBuckets\": [";
    for (size_t i = 0; i < s.latencyBuckets.size(); ++i) {
        out << s.latencyBuckets[i];
        if (i + 1 < s.latencyBuckets.size()) out << ", ";
    }
    out << "]\n";
    out << "}\n";
    return true;
}

std::string Metrics::formatPrometheusMetric(const std::string& name, const std::string& type,
                                             const std::string& help, unsigned long long value) const {
    std::ostringstream ss;
    ss << "# HELP " << name << " " << help << "\n";
    ss << "# TYPE " << name << " " << type << "\n";
    ss << name << " " << value << "\n";
    return ss.str();
}

std::string Metrics::exportPrometheus() const {
    auto s = snapshot();
    std::ostringstream out;
    
    // Counters
    out << formatPrometheusMetric("rawrxd_messages_total", "counter", 
                                   "Total messages processed", s.messagesProcessed);
    out << formatPrometheusMetric("rawrxd_voice_starts_total", "counter",
                                   "Total voice recording sessions", s.voiceStarts);
    out << formatPrometheusMetric("rawrxd_errors_total", "counter",
                                   "Total errors encountered", s.errors);
    out << formatPrometheusMetric("rawrxd_model_calls_total", "counter",
                                   "Total model inference calls", s.modelCalls);
    out << formatPrometheusMetric("rawrxd_tokens_generated_total", "counter",
                                   "Total tokens generated", s.tokensGenerated);
    out << formatPrometheusMetric("rawrxd_bytes_streamed_total", "counter",
                                   "Total bytes streamed", s.bytesStreamed);
    
    // Gauges
    out << "# HELP rawrxd_active_models Current number of loaded models\n";
    out << "# TYPE rawrxd_active_models gauge\n";
    out << "rawrxd_active_models " << s.activeModels << "\n";
    
    out << "# HELP rawrxd_memory_usage_mb Memory usage in megabytes\n";
    out << "# TYPE rawrxd_memory_usage_mb gauge\n";
    out << "rawrxd_memory_usage_mb " << std::fixed << std::setprecision(2) << s.memoryUsageMB << "\n";
    
    out << "# HELP rawrxd_gpu_usage_percent GPU utilization percentage\n";
    out << "# TYPE rawrxd_gpu_usage_percent gauge\n";
    out << "rawrxd_gpu_usage_percent " << std::fixed << std::setprecision(1) << s.gpuUsagePercent << "\n";
    
    out << "# HELP rawrxd_active_connections Current active connections\n";
    out << "# TYPE rawrxd_active_connections gauge\n";
    out << "rawrxd_active_connections " << s.activeConnections << "\n";
    
    // Histograms
    static const int bucketBounds[] = {10, 20, 50, 100, 200, 500, 1000};
    
    out << "# HELP rawrxd_model_latency_ms Model call latency in milliseconds\n";
    out << "# TYPE rawrxd_model_latency_ms histogram\n";
    unsigned long long cumulative = 0;
    for (size_t i = 0; i < s.latencyBuckets.size(); ++i) {
        cumulative += s.latencyBuckets[i];
        if (i < 7) {
            out << "rawrxd_model_latency_ms_bucket{le=\"" << bucketBounds[i] << "\"} " << cumulative << "\n";
        } else {
            out << "rawrxd_model_latency_ms_bucket{le=\"+Inf\"} " << cumulative << "\n";
        }
    }
    out << "rawrxd_model_latency_ms_count " << cumulative << "\n";
    
    // Labeled metrics
    for (const auto& [key, value] : s.labeledMetrics) {
        out << key << " " << value << "\n";
    }
    
    return out.str();
}

std::string Metrics::exportOpenMetrics() const {
    // OpenMetrics format is similar to Prometheus but with some differences
    return exportPrometheus() + "# EOF\n";
}

bool Metrics::startHttpServer(int port) {
    m_httpServerRunning.store(true);
    return true;
}

void Metrics::stopHttpServer() {
    m_httpServerRunning.store(false);
}

bool Metrics::isHttpServerRunning() const {
    return m_httpServerRunning.load();
}