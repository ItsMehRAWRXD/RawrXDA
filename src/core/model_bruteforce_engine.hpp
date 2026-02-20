#pragma once
/**
 * @file model_bruteforce_engine.hpp
 * @brief Brute-Force Model Discovery & Compatibility Probe Engine
 *
 * Systematically enumerates EVERY loadable model from ALL sources (local GGUF,
 * Ollama blobs, HuggingFace cache, user cache) and brute-force tests each model
 * for compatibility with the full token generation pipeline across all three
 * IDE rendering modes (CLI, GUI, HTML).
 *
 * For each discovered model, the engine:
 *   1. Validates GGUF magic bytes (0x46554747)
 *   2. Parses header: version, tensor_count, metadata_kv_count
 *   3. Extracts metadata: architecture, context_length, embedding_dim, vocab_size,
 *      quantization type, layer count, head count
 *   4. Attempts full token generation (brute force probe):
 *      - CPU inference path via CPUInferenceEngine
 *      - Ollama API path via AgentOllamaClient
 *      - Native pipeline via NativeInferencePipeline
 *   5. Records per-model compatibility matrix:
 *      CLI output (raw text), GUI output (SendMessage), HTML output (JSON/SSE)
 *
 * Output Modes:
 *   CLI:  Tabular report to stdout via ctx.output()
 *   GUI:  Rich WM_COMMAND dispatch + status bar updates
 *   HTML: JSON endpoint served at /api/models/bruteforce
 *
 * NO SIMPLIFICATION of existing inference paths.
 * All existing loaders, resolvers, and engines are reused as-is.
 */

#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <functional>

namespace RawrXD {

// ============================================================================
// GGUF Header — raw on-disk format for brute-force validation
// ============================================================================
struct BruteForceGGUFHeader {
    uint32_t magic;              // Must be 0x46554747 ("GGUF")
    uint32_t version;            // GGUF version (2 or 3)
    uint64_t tensor_count;       // Number of tensors
    uint64_t metadata_kv_count;  // Number of metadata key-value pairs
};

// ============================================================================
// Model Probe Result — per-model compatibility report
// ============================================================================
struct ModelProbeResult {
    // Identity
    std::string path;                    // Filesystem path or Ollama model name
    std::string filename;                // Basename for display
    std::string source;                  // "local", "ollama_blob", "hf_cache", "user_cache"
    uint64_t    file_size_bytes = 0;

    // GGUF Header
    bool        valid_magic      = false;
    uint32_t    gguf_version     = 0;
    uint64_t    tensor_count     = 0;
    uint64_t    metadata_kv_count = 0;

    // Extracted Metadata
    std::string architecture;            // e.g., "llama", "qwen2", "phi3", "gemma"
    std::string quantization;            // e.g., "Q4_K_M", "Q8_0", "F16"
    uint32_t    context_length   = 0;
    uint32_t    embedding_dim    = 0;
    uint32_t    vocab_size       = 0;
    uint32_t    layer_count      = 0;
    uint32_t    head_count       = 0;
    uint32_t    head_count_kv    = 0;
    float       estimated_ram_gb = 0.0f;

    // Probe Results (brute force token generation test)
    bool        probe_attempted  = false;
    bool        ollama_available = false; // Ollama can serve this model
    bool        cpu_loadable     = false; // CPUInferenceEngine can open it
    bool        native_loadable  = false; // NativeInferencePipeline can open it
    bool        token_generated  = false; // At least 1 token produced
    float       tokens_per_sec   = 0.0f;
    uint32_t    tokens_produced  = 0;
    std::string probe_output;            // First N tokens of output
    std::string probe_error;             // Error message if failed

    // Compatibility Matrix
    bool        cli_compatible   = false; // Can output to CLI
    bool        gui_compatible   = false; // Can output to GUI
    bool        html_compatible  = false; // Can serve via HTML/JSON
    
    // Timing
    double      scan_time_ms     = 0.0;  // Header parse time
    double      probe_time_ms    = 0.0;  // Full inference probe time
};

// ============================================================================
// Scan Configuration
// ============================================================================
struct BruteForceScanConfig {
    // Search paths
    std::vector<std::string> local_dirs;     // Additional local dirs to scan
    bool scan_ollama_blobs   = true;         // Scan Ollama blob directories
    bool scan_hf_cache       = true;         // Scan HuggingFace cache
    bool scan_user_cache     = true;         // Scan user model cache
    bool scan_cwd            = true;         // Scan current working directory

    // Probe settings
    bool   probe_inference   = true;         // Actually try token generation
    int    probe_max_tokens  = 8;            // Tokens to generate per model
    int    probe_timeout_ms  = 15000;        // Per-model timeout
    float  probe_temperature = 0.7f;
    std::string probe_prompt = "Hello";      // Test prompt
    
    // Filtering
    uint64_t min_file_size   = 1024;         // Skip files < 1KB
    uint64_t max_file_size   = 0;            // 0 = no limit
    std::string arch_filter;                 // Empty = all architectures
    std::string quant_filter;                // Empty = all quantizations
    
    // Output
    bool   verbose           = true;         // Print per-model progress
    int    max_models        = 0;            // 0 = unlimited
};

// ============================================================================
// Scan Progress Callback
// ============================================================================
struct BruteForceScanProgress {
    int    models_found      = 0;
    int    models_scanned    = 0;
    int    models_compatible = 0;
    int    models_failed     = 0;
    int    total_estimated   = 0;            // 0 if unknown
    float  percent_complete  = 0.0f;
    std::string current_model;               // Model being probed right now
    std::string status_message;
    bool   is_complete       = false;
};

using BruteForceProgressCallback = std::function<void(const BruteForceScanProgress&)>;

// ============================================================================
// Brute-Force Model Engine — Singleton
// ============================================================================
class ModelBruteForceEngine {
public:
    static ModelBruteForceEngine& instance();

    // ---- Discovery ----
    
    /**
     * Scan all model sources and build a list of discovered models.
     * Does NOT probe inference yet — just validates headers.
     */
    std::vector<ModelProbeResult> DiscoverAllModels(
        const BruteForceScanConfig& config = {},
        BruteForceProgressCallback progress = nullptr);

    /**
     * Brute-force probe a single model for compatibility.
     * Attempts all inference backends in sequence.
     */
    ModelProbeResult ProbeModel(
        const std::string& model_path,
        const BruteForceScanConfig& config = {});

    /**
     * Full brute-force: discover + probe ALL models.
     * Returns compatibility matrix for every model found.
     */
    std::vector<ModelProbeResult> BruteForceAll(
        const BruteForceScanConfig& config = {},
        BruteForceProgressCallback progress = nullptr);

    // ---- Results ----
    
    /** Get last scan results (cached from most recent BruteForceAll). */
    const std::vector<ModelProbeResult>& GetLastResults() const;

    /** Get last scan progress (for async polling). */
    BruteForceScanProgress GetProgress() const;

    // ---- Output Formatting ----
    
    /** Format results as CLI table (ANSI art). */
    std::string FormatCLI(const std::vector<ModelProbeResult>& results) const;

    /** Format results as JSON for HTML/API consumption. */
    std::string FormatJSON(const std::vector<ModelProbeResult>& results) const;

    /** Format results as HTML table (standalone page). */
    std::string FormatHTML(const std::vector<ModelProbeResult>& results) const;

    // ---- Control ----
    
    /** Cancel an in-progress scan. */
    void Cancel();
    bool IsRunning() const;

private:
    ModelBruteForceEngine() = default;
    ~ModelBruteForceEngine() = default;
    ModelBruteForceEngine(const ModelBruteForceEngine&) = delete;
    ModelBruteForceEngine& operator=(const ModelBruteForceEngine&) = delete;

    // ---- Internal ----
    void ScanDirectory(const std::string& dir, const std::string& source,
                       std::vector<ModelProbeResult>& out,
                       const BruteForceScanConfig& config);
    void ScanOllamaBlobs(std::vector<ModelProbeResult>& out,
                         const BruteForceScanConfig& config);
    bool ParseGGUFHeader(const std::string& path, ModelProbeResult& result);
    bool ExtractMetadata(const std::string& path, ModelProbeResult& result);
    void ProbeWithOllama(ModelProbeResult& result, const BruteForceScanConfig& config);
    void ProbeWithCPU(ModelProbeResult& result, const BruteForceScanConfig& config);
    void ProbeWithNative(ModelProbeResult& result, const BruteForceScanConfig& config);
    float EstimateRAM(const ModelProbeResult& result) const;
    std::string ExtractQuantFromFilename(const std::string& filename) const;

    // ---- State ----
    mutable std::mutex              m_mutex;
    std::vector<ModelProbeResult>   m_lastResults;
    BruteForceScanProgress          m_progress;
    std::atomic<bool>               m_running{false};
    std::atomic<bool>               m_cancelRequested{false};
};

} // namespace RawrXD
