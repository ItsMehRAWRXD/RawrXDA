#pragma once


#include <chrono>
#include <vector>
#include <memory>

/**
 * @class Profiler
 * @brief Performance profiler for training monitoring and optimization
 *
 * Tracks:
 * - CPU time per training phase (dataset loading, tokenization, forward pass, backward pass, weight update)
 * - Memory allocation/deallocation patterns
 * - GPU utilization and memory (if available)
 * - Cache hit rates and efficiency metrics
 * - Throughput (samples/sec, tokens/sec)
 * - Latency percentiles (P50, P95, P99)
 *
 * Emits signals for real-time dashboard visualization.
 */
class Profiler : public void
{

public:
    /**
     * @brief Profiling metric snapshot
     */
    struct ProfileSnapshot {
        qint64 timestamp = 0;           ///< Unix timestamp (ms)
        float cpuUsagePercent = 0.0f;   ///< CPU usage 0-100%
        float memoryUsageMB = 0.0f;     ///< Memory in MB
        float gpuUsagePercent = 0.0f;   ///< GPU usage 0-100%
        float gpuMemoryMB = 0.0f;       ///< GPU memory in MB
        float throughputSamples = 0.0f; ///< Samples processed per second
        float throughputTokens = 0.0f;  ///< Tokens processed per second
        float batchLatencyMs = 0.0f;    ///< Average batch processing time (ms)
        
        // Phase-specific latencies
        float loadLatencyMs = 0.0f;     ///< Dataset loading time (ms)
        float tokenizeLatencyMs = 0.0f; ///< Tokenization time (ms)
        float forwardPassMs = 0.0f;     ///< Forward pass time (ms)
        float backwardPassMs = 0.0f;    ///< Backward pass time (ms)
        float optimizerStepMs = 0.0f;   ///< Optimizer step time (ms)
    };

    /**
     * @brief Constructor
     * @param parent Qt parent object
     */
    explicit Profiler(void* parent = nullptr);
    ~Profiler() override = default;

    /**
     * @brief Start profiling session
     */
    void startProfiling();

    /**
     * @brief Stop profiling session and collect final statistics
     */
    void stopProfiling();

    /**
     * @brief Check if profiling is active
     * @return true if profiling
     */
    bool isProfiling() const { return m_isProfiling; }

    /**
     * @brief Mark the start of a training phase
     * @param phaseName Name of the phase (e.g., "forwardPass", "backwardPass")
     */
    void markPhaseStart(const std::string& phaseName);

    /**
     * @brief Mark the end of a training phase and record duration
     * @param phaseName Name of the phase
     */
    void markPhaseEnd(const std::string& phaseName);

    /**
     * @brief Record batch processing with sample/token counts
     * @param sampleCount Number of samples processed
     * @param tokenCount Number of tokens processed
     */
    void recordBatchCompleted(int sampleCount, int tokenCount);

    /**
     * @brief Record memory allocation
     * @param bytes Amount allocated
     */
    void recordMemoryAllocation(size_t bytes);

    /**
     * @brief Record memory deallocation
     * @param bytes Amount deallocated
     */
    void recordMemoryDeallocation(size_t bytes);

    /**
     * @brief Update GPU metrics (from external source if available)
     * @param gpuUsagePercent GPU utilization 0-100%
     * @param gpuMemoryMB GPU memory used in MB
     */
    void updateGpuMetrics(float gpuUsagePercent, float gpuMemoryMB);

    /**
     * @brief Get current performance snapshot
     * @return Current metrics snapshot
     */
    ProfileSnapshot getCurrentSnapshot() const;

    /**
     * @brief Get full profiling report
     * @return JSON object with detailed metrics and statistics
     */
    void* getProfilingReport() const;

    /**
     * @brief Export profiling data to file
     * @param filePath Path to export to (JSON format)
     * @return true if export successful
     */
    bool exportReport(const std::string& filePath) const;


    /**
     * @brief Emitted when new metrics snapshot available
     * @param snapshot Current metrics snapshot
     */
    void metricsUpdated(const ProfileSnapshot& snapshot);

    /**
     * @brief Emitted when profiling session completes
     * @param report Full profiling report as JSON
     */
    void profilingCompleted(const void*& report);

    /**
     * @brief Emitted for performance warnings
     * @param warning Warning message
     */
    void performanceWarning(const std::string& warning);

private:
    /**
     * @brief Periodic collection of system metrics
     */
    void collectSystemMetrics();

private:
    /**
     * @brief Get current CPU usage percentage
     * @return CPU usage 0-100%
     */
    float getCpuUsagePercent() const;

    /**
     * @brief Get current process memory usage in MB
     * @return Memory usage in MB
     */
    float getMemoryUsageMB() const;

    /**
     * @brief Analyze collected metrics and detect issues
     */
    void analyzeMetrics();

    // ===== State =====
    bool m_isProfiling = false;
    std::chrono::high_resolution_clock::time_point m_profilingStart;
    std::chrono::high_resolution_clock::time_point m_batchStart;

    // ===== Phase Tracking =====
    struct PhaseData {
        std::chrono::high_resolution_clock::time_point startTime;
        std::vector<qint64> durations; // milliseconds
        qint64 totalMs = 0;
    };
    std::map<std::string, PhaseData> m_phases;

    // ===== Memory Tracking =====
    size_t m_totalAllocated = 0;
    size_t m_currentAllocated = 0;
    size_t m_peakAllocated = 0;

    // ===== Batch/Throughput Tracking =====
    int m_totalSamplesProcessed = 0;
    int m_totalTokensProcessed = 0;
    std::vector<qint64> m_batchLatencies; // milliseconds
    std::chrono::high_resolution_clock::time_point m_lastMetricsCollection;

    // ===== GPU Metrics =====
    float m_lastGpuUsagePercent = 0.0f;
    float m_lastGpuMemoryMB = 0.0f;

    // ===== Periodic Update =====
    void** m_metricsTimer = nullptr;
    std::vector<ProfileSnapshot> m_snapshots; // Historical data

    // ===== Thresholds for Warnings =====
    float m_cpuThresholdPercent = 95.0f;
    float m_memoryThresholdPercent = 85.0f;
    float m_gpuThresholdPercent = 95.0f;
};

