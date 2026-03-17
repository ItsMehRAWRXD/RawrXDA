// inference_engine.hpp — Pure C++17 inference engine
// Converted from Qt (QObject, QMutex, QQueue, QHash, QElapsedTimer, QUuid, signals)
// to std::mutex, std::deque, std::unordered_map, std::chrono, custom UUID

#pragma once

#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <numeric>
#include <random>
#include <cstring>
#include <sstream>
#include <iomanip>

// ======================== Error Codes ========================
enum class InferenceErrorCode : int {
    Success              = 0,
    ModelNotLoaded       = 4001,
    InvalidInput         = 4002,
    InferenceFailed      = 4003,
    QueueFull            = 4004,
    Timeout              = 4005,
    OutOfMemory          = 4006,
    BackendError         = 4007,
    InvalidModelFormat   = 4008,
    GPUError             = 4009,
    CancelledByUser      = 4010,
    InternalError        = 4099
};

static const char* inferenceErrorString(InferenceErrorCode code) {
    switch (code) {
        case InferenceErrorCode::Success:           return "Success";
        case InferenceErrorCode::ModelNotLoaded:     return "Model not loaded";
        case InferenceErrorCode::InvalidInput:       return "Invalid input";
        case InferenceErrorCode::InferenceFailed:    return "Inference failed";
        case InferenceErrorCode::QueueFull:          return "Queue full";
        case InferenceErrorCode::Timeout:            return "Timeout";
        case InferenceErrorCode::OutOfMemory:        return "Out of memory";
        case InferenceErrorCode::BackendError:       return "Backend error";
        case InferenceErrorCode::InvalidModelFormat: return "Invalid model format";
        case InferenceErrorCode::GPUError:           return "GPU error";
        case InferenceErrorCode::CancelledByUser:    return "Cancelled by user";
        case InferenceErrorCode::InternalError:      return "Internal error";
        default: return "Unknown error";
    }
}

// ======================== Request / Response ========================
struct InferenceRequest {
    std::string id;
    std::string prompt;
    int maxTokens           = 256;
    float temperature       = 0.7f;
    float topP              = 0.9f;
    int topK                = 40;
    float repeatPenalty     = 1.1f;
    std::vector<std::string> stopSequences;
    int priority            = 0; // Higher = more urgent
    std::chrono::steady_clock::time_point submitTime;
};

struct InferenceResponse {
    std::string requestId;
    bool success                = false;
    InferenceErrorCode error    = InferenceErrorCode::Success;
    std::string output;
    int tokensGenerated         = 0;
    double latencyMs            = 0.0;
    double tokensPerSecond      = 0.0;
    double queueWaitMs          = 0.0;
};

// ======================== Health Status ========================
struct HealthStatus {
    bool modelLoaded        = false;
    int queueDepth          = 0;
    int totalRequestsServed = 0;
    double avgLatencyMs     = 0.0;
    double uptime           = 0.0;
    size_t memoryUsedBytes  = 0;
    size_t gpuMemoryBytes   = 0;
    std::string modelName;
    std::string modelVersion;
    std::string status;         // "healthy", "degraded", "error"
};

// ======================== UUID Generator ========================
static std::string generateUUID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

    uint32_t parts[4] = { dist(gen), dist(gen), dist(gen), dist(gen) };

    // Set version 4 and variant bits
    parts[1] = (parts[1] & 0xFFFF0FFF) | 0x00004000;
    parts[2] = (parts[2] & 0x3FFFFFFF) | 0x80000000;

    char buf[37];
    snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%04x%08x",
             parts[0],
             (parts[1] >> 16) & 0xFFFF,
             parts[1] & 0xFFFF,
             (parts[2] >> 16) & 0xFFFF,
             parts[2] & 0xFFFF,
             parts[3]);
    return std::string(buf);
}

// ======================== InferenceEngine ========================
class InferenceEngine {
public:
    // Callback types (replacing Qt signals)
    using ResponseCallback  = std::function<void(const InferenceResponse&)>;
    using HealthCallback    = std::function<void(const HealthStatus&)>;
    using ErrorCallback     = std::function<void(InferenceErrorCode, const std::string&)>;
    using TokenCallback     = std::function<void(const std::string& requestId, int tokenId)>;

    InferenceEngine();
    ~InferenceEngine();

    // Non-copyable
    InferenceEngine(const InferenceEngine&) = delete;
    InferenceEngine& operator=(const InferenceEngine&) = delete;

    // Model management
    bool loadModel(const std::string& modelPath);
    bool isModelLoaded() const { return m_modelLoaded; }
    void unloadModel();
    std::string modelName() const { return m_modelName; }

    // Synchronous inference
    InferenceResponse infer(const InferenceRequest& request);

    // Async queue
    std::string queueInferenceRequest(const InferenceRequest& request);
    bool cancelRequest(const std::string& requestId);
    int queueDepth() const;

    // Process next queued request
    InferenceResponse processNextRequest();

    // Health & metrics
    HealthStatus getHealthStatus() const;

    // Performance metrics
    struct PerformanceMetrics {
        double avgLatencyMs    = 0.0;
        double p50LatencyMs    = 0.0;
        double p95LatencyMs    = 0.0;
        double p99LatencyMs    = 0.0;
        int totalRequests      = 0;
        int successfulRequests = 0;
        int failedRequests     = 0;
        double totalTokensGen  = 0;
    };
    PerformanceMetrics getPerformanceMetrics() const;

    // GPU memory tracking
    size_t gpuMemoryUsed() const { return m_gpuMemUsed; }
    size_t gpuMemoryTotal() const { return m_gpuMemTotal; }

    // Callbacks
    void onResponse(ResponseCallback cb)   { m_responseCb = std::move(cb); }
    void onHealth(HealthCallback cb)        { m_healthCb = std::move(cb); }
    void onError(ErrorCallback cb)          { m_errorCb = std::move(cb); }
    void onToken(TokenCallback cb)          { m_tokenCb = std::move(cb); }

private:
    void emitError(InferenceErrorCode code, const std::string& msg);
    void emitResponse(const InferenceResponse& resp);
    void emitHealth(const HealthStatus& status);
    void recordLatency(double ms);

    // State
    std::atomic<bool> m_modelLoaded{false};
    std::string m_modelName;
    std::string m_modelPath;

    // Tensor cache (replaces QHash<QString,QByteArray>)
    std::unordered_map<std::string, std::vector<uint8_t>> m_tensorCache;

    // Request queue (replaces QQueue)
    mutable std::mutex m_queueMutex;
    std::deque<InferenceRequest> m_requestQueue;
    static constexpr int MAX_QUEUE_SIZE = 100;

    // Latency tracking
    mutable std::mutex m_metricsMutex;
    std::vector<double> m_latencyHistory;
    int m_totalRequests     = 0;
    int m_successRequests   = 0;
    int m_failedRequests    = 0;
    double m_totalTokens    = 0;

    // GPU tracking
    size_t m_gpuMemUsed     = 0;
    size_t m_gpuMemTotal    = 0;

    // Timing
    std::chrono::steady_clock::time_point m_startTime;

    // Callbacks
    ResponseCallback  m_responseCb;
    HealthCallback    m_healthCb;
    ErrorCallback     m_errorCb;
    TokenCallback     m_tokenCb;
};
