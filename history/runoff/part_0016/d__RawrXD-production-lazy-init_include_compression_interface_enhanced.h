#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <string>
#include <functional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <QObject>
#include <QTimer>
#include "brutal_gzip.h"
#include "deflate_brutal_qt.hpp"

/**
 * @file compression_interface_enhanced.h
 * @brief Enhanced MASM-optimized compression interface with multiple algorithms and telemetry
 * 
 * This header provides wrapper classes that integrate with:
 * - brutal_gzip.lib (MASM-optimized GZIP decompression)
 * - deflate_brutal_qt.hpp (MASM-optimized deflate with Qt integration)
 * - LZ4 (Ultra-fast compression for real-time scenarios)
 * - ZSTD (High compression ratio for storage optimization)
 * - Brotli (Web-optimized compression)
 * - Snappy (Google's fast compression)
 * - LZMA (Maximum compression ratio)
 * 
 * Features:
 * - Multi-algorithm support with automatic selection
 * - Real-time performance monitoring and telemetry
 * - SIMD optimization with CPU feature detection
 * - Parallel processing with configurable thread pools
 * - Comprehensive statistics and performance metrics
 */

// Forward declarations
class IGZIPCompressor;
class IDeflateCompressor;

/**
 * @enum CompressionAlgorithm
 * @brief Supported compression algorithms with performance characteristics
 */
enum class CompressionAlgorithm : uint32_t {
    BRUTAL_GZIP = 0,    // MASM-optimized GZIP (~200MB/s compression, ~500MB/s decompression)
    DEFLATE = 1,        // Standard deflate with MASM wrappers (~150MB/s, ~450MB/s)
    LZ4_FAST = 2,       // Ultra-fast compression (~400MB/s compression, ~2000MB/s decompression)
    LZ4_HC = 3,         // LZ4 High Compression (~100MB/s compression, ~1800MB/s decompression)
    ZSTD = 4,           // High compression ratio (~80MB/s compression, ~800MB/s decompression)
    BROTLI = 5,         // Web-optimized compression (~20MB/s compression, ~300MB/s decompression)
    SNAPPY = 6,         // Google's fast compression (~300MB/s compression, ~1000MB/s decompression)
    LZMA = 7,           // Maximum compression ratio (~10MB/s compression, ~50MB/s decompression)
    AUTO_SELECT = 255   // Automatic algorithm selection based on content analysis
};

/**
 * @enum CompressionLevel
 * @brief Compression levels with speed/ratio trade-offs
 */
enum class CompressionLevel : uint32_t {
    FASTEST = 1,        // Prioritize speed over compression ratio
    FAST = 3,           // Good balance favoring speed
    BALANCED = 6,       // Optimal balance of speed and compression
    GOOD = 8,           // Good compression with acceptable speed
    BEST = 9            // Maximum compression ratio
};

/**
 * @enum StreamingMode
 * @brief Streaming compression modes for large datasets
 */
enum class StreamingMode : uint32_t {
    BLOCK = 0,          // Fixed block size compression
    ADAPTIVE = 1,       // Dynamic block sizing based on content
    SLIDING_WINDOW = 2  // Overlapping compression windows
};

/**
 * @struct CompressionConfig
 * @brief Configuration parameters for compression operations
 */
struct CompressionConfig {
    CompressionAlgorithm algorithm = CompressionAlgorithm::BRUTAL_GZIP;
    CompressionLevel level = CompressionLevel::BALANCED;
    StreamingMode streaming_mode = StreamingMode::BLOCK;
    
    // Performance tuning
    bool enable_parallel = true;
    bool enable_simd = true;
    bool enable_dictionary = false;
    bool enable_caching = true;
    
    uint32_t thread_count = 0;                // 0 = auto-detect
    uint32_t block_size = 1024 * 1024;        // 1MB default
    uint32_t dict_size = 32 * 1024;           // 32KB dictionary
    
    // Memory management
    uint32_t memory_limit_mb = 0;             // 0 = no limit
    bool prefer_speed = false;
    bool verify_integrity = false;
    
    // Telemetry
    bool enable_telemetry = true;
    bool detailed_metrics = false;
};

/**
 * @struct CompressionMetrics
 * @brief Real-time compression performance metrics
 */
struct CompressionMetrics {
    // Basic metrics
    std::atomic<uint64_t> compression_calls{0};
    std::atomic<uint64_t> decompression_calls{0};
    std::atomic<uint64_t> total_input_bytes{0};
    std::atomic<uint64_t> total_output_bytes{0};
    
    // Performance metrics
    std::atomic<double> avg_compression_ratio{0.0};
    std::atomic<double> avg_compression_time_ms{0.0};
    std::atomic<double> avg_decompression_time_ms{0.0};
    std::atomic<double> throughput_mb_per_sec{0.0};
    
    // Advanced metrics
    std::atomic<double> simd_efficiency{0.0};      // SIMD utilization percentage
    std::atomic<double> cache_hit_ratio{0.0};      // Dictionary cache hits
    std::atomic<double> parallel_efficiency{0.0};  // Multi-threading efficiency
    std::atomic<double> memory_usage_peak_mb{0.0}; // Peak memory usage
    
    // Hardware information
    std::string simd_features;
    std::string active_kernel;
    bool hardware_acceleration = false;
    
    // Algorithm specifics
    std::string active_algorithm;
    uint32_t active_level = 0;
    std::atomic<uint64_t> parallel_operations{0};
    
    // Error tracking
    std::atomic<uint64_t> compression_errors{0};
    std::atomic<uint64_t> decompression_errors{0};
    
    void Reset() {
        compression_calls = 0;
        decompression_calls = 0;
        total_input_bytes = 0;
        total_output_bytes = 0;
        avg_compression_ratio = 0.0;
        avg_compression_time_ms = 0.0;
        avg_decompression_time_ms = 0.0;
        throughput_mb_per_sec = 0.0;
        simd_efficiency = 0.0;
        cache_hit_ratio = 0.0;
        parallel_efficiency = 0.0;
        memory_usage_peak_mb = 0.0;
        parallel_operations = 0;
        compression_errors = 0;
        decompression_errors = 0;
    }
    
    std::string ToString() const;
};

/**
 * @class ICompressionProvider
 * @brief Enhanced abstract interface for compression algorithms with telemetry
 */
class ICompressionProvider {
public:
    virtual ~ICompressionProvider() = default;
    
    // Basic compression interface
    virtual bool Compress(const std::vector<uint8_t>& raw, std::vector<uint8_t>& compressed) = 0;
    virtual bool Decompress(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& raw) = 0;
    virtual bool IsSupported() const = 0;
    
    // Advanced compression with configuration
    virtual bool CompressAdvanced(const std::vector<uint8_t>& raw,
                                 std::vector<uint8_t>& compressed,
                                 const CompressionConfig& config) = 0;
    
    // Streaming compression for large datasets
    virtual bool CompressStream(const void* input, size_t input_size,
                               void* output, size_t* output_size,
                               const CompressionConfig& config) = 0;
    
    // Dictionary-based compression for repetitive data
    virtual bool CompressWithDict(const std::vector<uint8_t>& raw,
                                 std::vector<uint8_t>& compressed,
                                 const std::vector<uint8_t>& dictionary) = 0;
    
    // Performance and telemetry
    virtual CompressionMetrics GetMetrics() const = 0;
    virtual void ResetMetrics() = 0;
    virtual std::string GetSupportedFeatures() const = 0;
    virtual CompressionAlgorithm GetAlgorithm() const = 0;
    
    // Configuration
    virtual void SetConfig(const CompressionConfig& config) = 0;
    virtual CompressionConfig GetConfig() const = 0;
};

/**
 * @class EnhancedBrutalGzipWrapper
 * @brief Enhanced wrapper around brutal_gzip.lib with comprehensive telemetry
 */
class EnhancedBrutalGzipWrapper : public QObject, public ICompressionProvider {
    Q_OBJECT
    
public:
    explicit EnhancedBrutalGzipWrapper(QObject* parent = nullptr);
    ~EnhancedBrutalGzipWrapper();
    
    // ICompressionProvider implementation
    bool Compress(const std::vector<uint8_t>& raw, std::vector<uint8_t>& compressed) override;
    bool Decompress(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& raw) override;
    bool IsSupported() const override;
    
    bool CompressAdvanced(const std::vector<uint8_t>& raw,
                         std::vector<uint8_t>& compressed,
                         const CompressionConfig& config) override;
    
    bool CompressStream(const void* input, size_t input_size,
                       void* output, size_t* output_size,
                       const CompressionConfig& config) override;
                       
    bool CompressWithDict(const std::vector<uint8_t>& raw,
                         std::vector<uint8_t>& compressed,
                         const std::vector<uint8_t>& dictionary) override;
    
    CompressionMetrics GetMetrics() const override;
    void ResetMetrics() override;
    std::string GetSupportedFeatures() const override;
    CompressionAlgorithm GetAlgorithm() const override { return CompressionAlgorithm::BRUTAL_GZIP; }
    
    void SetConfig(const CompressionConfig& config) override;
    CompressionConfig GetConfig() const override;
    
    // Enhanced functionality
    bool Initialize();
    std::string GetActiveKernel() const;
    void SetThreadCount(uint32_t num_threads);
    
signals:
    void compressionProgress(int percentage);
    void compressionCompleted(uint64_t input_size, uint64_t output_size, double time_ms);
    void compressionFailed(const QString& error_message);
    void metricsUpdated(const CompressionMetrics& metrics);

private slots:
    void onTelemetryTimer();

private:
    void InitializeKernels();
    void UpdateMetrics(bool is_compression, uint64_t input_size, uint64_t output_size, double time_ms);
    void DetectCPUFeatures();
    
    CompressionConfig config_;
    mutable CompressionMetrics metrics_;
    mutable std::mutex metrics_mutex_;
    QTimer* telemetry_timer_;
    
    bool is_initialized_ = false;
    uint32_t thread_count_ = 0;
    bool has_avx512_ = false;
    bool has_avx2_ = false;
    bool has_sse42_ = false;
    bool has_bmi2_ = false;
    
    std::string active_kernel_;
    std::chrono::high_resolution_clock::time_point last_operation_time_;
};

/**
 * @class LZ4Wrapper
 * @brief Ultra-fast LZ4 compression provider with real-time performance monitoring
 */
class LZ4Wrapper : public QObject, public ICompressionProvider {
    Q_OBJECT
    
public:
    explicit LZ4Wrapper(QObject* parent = nullptr);
    ~LZ4Wrapper();
    
    // ICompressionProvider implementation
    bool Compress(const std::vector<uint8_t>& raw, std::vector<uint8_t>& compressed) override;
    bool Decompress(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& raw) override;
    bool IsSupported() const override;
    
    bool CompressAdvanced(const std::vector<uint8_t>& raw,
                         std::vector<uint8_t>& compressed,
                         const CompressionConfig& config) override;
    
    bool CompressStream(const void* input, size_t input_size,
                       void* output, size_t* output_size,
                       const CompressionConfig& config) override;
                       
    bool CompressWithDict(const std::vector<uint8_t>& raw,
                         std::vector<uint8_t>& compressed,
                         const std::vector<uint8_t>& dictionary) override;
    
    CompressionMetrics GetMetrics() const override;
    void ResetMetrics() override;
    std::string GetSupportedFeatures() const override;
    CompressionAlgorithm GetAlgorithm() const override { return CompressionAlgorithm::LZ4_FAST; }
    
    void SetConfig(const CompressionConfig& config) override;
    CompressionConfig GetConfig() const override;

signals:
    void compressionCompleted(uint64_t input_size, uint64_t output_size, double time_ms);
    void metricsUpdated(const CompressionMetrics& metrics);

private:
    CompressionConfig config_;
    mutable CompressionMetrics metrics_;
    mutable std::mutex metrics_mutex_;
    bool is_initialized_ = false;
};

/**
 * @class ZSTDWrapper
 * @brief High-ratio ZSTD compression provider with adaptive compression levels
 */
class ZSTDWrapper : public QObject, public ICompressionProvider {
    Q_OBJECT
    
public:
    explicit ZSTDWrapper(QObject* parent = nullptr);
    ~ZSTDWrapper();
    
    // ICompressionProvider implementation
    bool Compress(const std::vector<uint8_t>& raw, std::vector<uint8_t>& compressed) override;
    bool Decompress(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& raw) override;
    bool IsSupported() const override;
    
    bool CompressAdvanced(const std::vector<uint8_t>& raw,
                         std::vector<uint8_t>& compressed,
                         const CompressionConfig& config) override;
    
    bool CompressStream(const void* input, size_t input_size,
                       void* output, size_t* output_size,
                       const CompressionConfig& config) override;
                       
    bool CompressWithDict(const std::vector<uint8_t>& raw,
                         std::vector<uint8_t>& compressed,
                         const std::vector<uint8_t>& dictionary) override;
    
    CompressionMetrics GetMetrics() const override;
    void ResetMetrics() override;
    std::string GetSupportedFeatures() const override;
    CompressionAlgorithm GetAlgorithm() const override { return CompressionAlgorithm::ZSTD; }
    
    void SetConfig(const CompressionConfig& config) override;
    CompressionConfig GetConfig() const override;

signals:
    void compressionCompleted(uint64_t input_size, uint64_t output_size, double time_ms);
    void metricsUpdated(const CompressionMetrics& metrics);

private:
    CompressionConfig config_;
    mutable CompressionMetrics metrics_;
    mutable std::mutex metrics_mutex_;
    bool is_initialized_ = false;
    void* zstd_cctx_ = nullptr;  // ZSTD compression context
    void* zstd_dctx_ = nullptr;  // ZSTD decompression context
};

/**
 * @class BrotliWrapper
 * @brief Web-optimized Brotli compression provider
 */
class BrotliWrapper : public QObject, public ICompressionProvider {
    Q_OBJECT
    
public:
    explicit BrotliWrapper(QObject* parent = nullptr);
    ~BrotliWrapper();
    
    // ICompressionProvider implementation
    bool Compress(const std::vector<uint8_t>& raw, std::vector<uint8_t>& compressed) override;
    bool Decompress(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& raw) override;
    bool IsSupported() const override;
    
    bool CompressAdvanced(const std::vector<uint8_t>& raw,
                         std::vector<uint8_t>& compressed,
                         const CompressionConfig& config) override;
    
    bool CompressStream(const void* input, size_t input_size,
                       void* output, size_t* output_size,
                       const CompressionConfig& config) override;
                       
    bool CompressWithDict(const std::vector<uint8_t>& raw,
                         std::vector<uint8_t>& compressed,
                         const std::vector<uint8_t>& dictionary) override;
    
    CompressionMetrics GetMetrics() const override;
    void ResetMetrics() override;
    std::string GetSupportedFeatures() const override;
    CompressionAlgorithm GetAlgorithm() const override { return CompressionAlgorithm::BROTLI; }
    
    void SetConfig(const CompressionConfig& config) override;
    CompressionConfig GetConfig() const override;

signals:
    void compressionCompleted(uint64_t input_size, uint64_t output_size, double time_ms);
    void metricsUpdated(const CompressionMetrics& metrics);

private:
    CompressionConfig config_;
    mutable CompressionMetrics metrics_;
    mutable std::mutex metrics_mutex_;
    bool is_initialized_ = false;
};

/**
 * @struct CompressionJob
 * @brief Batch compression job specification
 */
struct CompressionJob {
    uint64_t job_id;
    CompressionAlgorithm algorithm;
    CompressionConfig config;
    std::vector<uint8_t> input_data;
    std::string description;
};

/**
 * @struct CompressionResult
 * @brief Batch compression job result
 */
struct CompressionResult {
    uint64_t job_id;
    bool success;
    std::vector<uint8_t> compressed_data;
    CompressionMetrics metrics;
    std::string error_message;
    double processing_time_ms;
};

/**
 * @struct CompressionBenchmarkResult
 * @brief Algorithm benchmark result
 */
struct CompressionBenchmarkResult {
    CompressionAlgorithm algorithm;
    bool success;
    double avg_compression_ratio;
    double avg_compression_time_ms;
    double avg_decompression_time_ms;
    double throughput_mb_per_sec;
    std::string performance_profile;
};

/**
 * @class CompressionManager
 * @brief High-level manager for batch processing and algorithm selection
 */
class CompressionManager : public QObject {
    Q_OBJECT
    
public:
    explicit CompressionManager(QObject* parent = nullptr);
    ~CompressionManager();
    
    // Batch processing
    bool CompressBatch(const std::vector<CompressionJob>& jobs,
                      std::vector<CompressionResult>& results);
    
    // Algorithm selection and optimization
    CompressionAlgorithm SelectOptimalAlgorithm(const std::vector<uint8_t>& sample_data);
    
    // Performance benchmarking
    std::vector<CompressionBenchmarkResult> BenchmarkAlgorithms(
        const std::vector<uint8_t>& test_data,
        const std::vector<CompressionAlgorithm>& algorithms);
    
    // Global metrics aggregation
    CompressionMetrics GetAggregatedMetrics() const;
    void ResetAllMetrics();
    
    // Configuration management
    void SetGlobalConfig(const CompressionConfig& config);
    CompressionConfig GetGlobalConfig() const;

signals:
    void batchProgressUpdated(int percentage);
    void batchCompleted(size_t successful_jobs, size_t total_jobs);
    void algorithmBenchmarkCompleted(CompressionAlgorithm algorithm, double score);
    void globalMetricsUpdated(const CompressionMetrics& metrics);

private slots:
    void onProviderMetricsUpdated(const CompressionMetrics& metrics);

private:
    void InitializeProviders();
    double CalculateAlgorithmScore(const CompressionBenchmarkResult& result);
    
    std::vector<std::shared_ptr<ICompressionProvider>> providers_;
    CompressionConfig global_config_;
    mutable CompressionMetrics aggregated_metrics_;
    mutable std::mutex metrics_mutex_;
};

/**
 * @class EnhancedCompressionFactory
 * @brief Enhanced factory for creating compression providers with telemetry
 */
class EnhancedCompressionFactory {
public:
    /**
     * @brief Create a compression provider for specified algorithm
     * @param algorithm Desired compression algorithm
     * @return Shared pointer to the created provider (nullptr if not available)
     */
    static std::shared_ptr<ICompressionProvider> CreateProvider(CompressionAlgorithm algorithm);
    
    /**
     * @brief Create optimal provider based on sample data analysis
     * @param sample_data Sample data for algorithm selection
     * @return Shared pointer to the optimal provider
     */
    static std::shared_ptr<ICompressionProvider> CreateOptimalProvider(const std::vector<uint8_t>& sample_data);
    
    /**
     * @brief Get all supported algorithms on this system
     * @return Vector of supported algorithms
     */
    static std::vector<CompressionAlgorithm> GetSupportedAlgorithms();
    
    /**
     * @brief Check if a specific algorithm is supported
     * @param algorithm Algorithm to check
     * @return true if supported, false otherwise
     */
    static bool IsSupported(CompressionAlgorithm algorithm);
    
    /**
     * @brief Get algorithm performance characteristics
     * @param algorithm Algorithm to query
     * @return String describing performance characteristics
     */
    static std::string GetAlgorithmInfo(CompressionAlgorithm algorithm);
    
    /**
     * @brief Initialize all supported compression libraries
     * @return true if initialization successful
     */
    static bool InitializeLibraries();
    
    /**
     * @brief Get system capabilities and recommendations
     * @return String with system analysis and recommendations
     */
    static std::string GetSystemCapabilities();
};

/**
 * @class CompressionTelemetry
 * @brief Centralized telemetry collection and monitoring system
 */
class CompressionTelemetry : public QObject {
    Q_OBJECT
    
public:
    static CompressionTelemetry& Instance();
    
    // Telemetry collection
    void RecordOperation(CompressionAlgorithm algorithm, const CompressionMetrics& metrics);
    void RecordError(CompressionAlgorithm algorithm, const std::string& error);
    void RecordSystemEvent(const std::string& event, const std::string& details);
    
    // Monitoring and alerting
    void SetPerformanceThreshold(double threshold_mb_per_sec);
    void SetErrorRateThreshold(double threshold_percent);
    void EnableRealTimeMonitoring(bool enabled);
    
    // Reporting
    std::string GenerateReport() const;
    std::string GeneratePerformanceProfile() const;
    void ExportMetrics(const std::string& filename) const;
    
    // Historical data
    std::vector<CompressionMetrics> GetHistoricalMetrics(
        CompressionAlgorithm algorithm,
        const std::chrono::time_point<std::chrono::system_clock>& start_time,
        const std::chrono::time_point<std::chrono::system_clock>& end_time) const;

signals:
    void performanceAlert(const QString& message);
    void errorAlert(const QString& algorithm, const QString& error);
    void systemAlert(const QString& event, const QString& details);

private:
    CompressionTelemetry() = default;
    
    mutable std::mutex telemetry_mutex_;
    std::map<CompressionAlgorithm, std::vector<CompressionMetrics>> historical_metrics_;
    std::map<CompressionAlgorithm, std::vector<std::string>> error_log_;
    double performance_threshold_ = 100.0;  // MB/s
    double error_rate_threshold_ = 5.0;     // %
    bool real_time_monitoring_ = false;
};

#include "compression_interface_enhanced.moc"

#endif // COMPRESSION_INTERFACE_ENHANCED_H