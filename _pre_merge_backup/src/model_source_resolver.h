#pragma once
/**
 * @file model_source_resolver.h
 * @brief Unified Model Source Resolution for GGUF models
 *
 * Supports loading GGUF models from multiple sources:
 *   - Local filesystem (.gguf files)
 *   - HuggingFace Hub (model ID like "TheBloke/Llama-2-7B-GGUF")
 *   - Ollama blobs (model names like "llama3.2:3b", resolves sha256-* blobs)
 *   - Direct HTTP/HTTPS URLs to .gguf files
 *
 * All sources are resolved to a local filesystem path that StreamingGGUFLoader
 * can open. Downloads go to a local cache directory. Ollama blobs are validated
 * for GGUF magic bytes before being returned.
 *
 * Uses WinHTTP for network operations (no curl dependency for Win32 build).
 *
 * This module preserves all existing streaming/zone-based logic for 800B+ models.
 * It only adds source resolution on top — NO SIMPLIFICATION of existing loaders.
 */

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <mutex>
#include <atomic>
#include "model_loader/GGUFConstants.hpp"
#include <functional>
#include <thread>
#include <string>
#include <vector>

namespace RawrXD {

// ============================================================================
// Download progress tracking
// ============================================================================
struct ModelDownloadProgress {
    std::string source_url;          // Original URL or model ID
    std::string local_path;          // Destination file path
    std::string filename;            // Filename being downloaded
    uint64_t total_bytes = 0;        // Total expected size (0 if unknown)
    uint64_t downloaded_bytes = 0;   // Bytes downloaded so far
    float progress_percent = 0.0f;   // 0.0 - 100.0
    bool is_completed = false;
    bool is_cancelled = false;
    bool has_error = false;
    std::string error_message;
};

using DownloadProgressCallback = std::function<void(const ModelDownloadProgress&)>;

// ============================================================================
// HuggingFace model file info
// ============================================================================
struct HFModelFileInfo {
    std::string filename;             // e.g., "model-Q4_K_M.gguf"
    uint64_t size_bytes = 0;         // File size
    std::string quantization;        // Extracted quant type (e.g., "Q4_K_M")
};

struct HFModelInfo {
    std::string repo_id;             // e.g., "TheBloke/Llama-2-7B-GGUF"
    std::string model_name;          // e.g., "Llama-2-7B-GGUF"
    std::string description;
    uint64_t downloads = 0;
    std::vector<HFModelFileInfo> gguf_files;  // Available .gguf files in the repo
};

// ============================================================================
// Ollama blob resolution result
// ============================================================================
struct OllamaBlobInfo {
    std::string model_name;          // e.g., "llama3.2:3b"
    std::string blob_path;           // Full path to the blob file
    uint64_t size_bytes = 0;
    bool is_valid_gguf = false;      // Magic bytes validated
};

// ============================================================================
// Resolved model path result
// ============================================================================
struct ResolvedModelPath {
    bool success = false;
    std::string local_path;              // Final local filesystem path for StreamingGGUFLoader
    GGUFConstants::ModelSourceType source_type = GGUFConstants::ModelSourceType::UNKNOWN;
    std::string original_input;          // What the user originally provided
    std::string error_message;
    
    // For HF models — which specific GGUF file was selected
    std::string hf_repo_id;
    std::string hf_filename;
    
    // For Ollama blobs
    std::string ollama_model_name;
};

// ============================================================================
// ModelSourceResolver — Unified model source detection and resolution
// ============================================================================
class ModelSourceResolver {
public:
    ModelSourceResolver();
    ~ModelSourceResolver();

    // ---- Source Detection ----
    
    /**
     * Detect what kind of model source the input string represents.
     * 
     * Detection rules:
     *   - Ends with .gguf and exists on disk → LOCAL_FILE
     *   - Contains "/" and matches "owner/repo" pattern → HUGGINGFACE_REPO
     *   - Starts with "hf://" or "huggingface://" → HUGGINGFACE_REPO
     *   - Starts with "http://" or "https://" → HTTP_URL
     *   - Contains ":" (like "llama3.2:3b") or no "/" → OLLAMA_BLOB
     *   - Otherwise → attempts local file, then Ollama, then HF search
     */
    GGUFConstants::ModelSourceType DetectSourceType(const std::string& input) const;

    // ---- Unified Resolution ----
    
    /**
     * Resolve any model source to a local filesystem path.
     * 
     * This is the main entry point. It:
     *   1. Detects the source type
     *   2. For LOCAL_FILE: validates the file exists and has GGUF magic
     *   3. For HUGGINGFACE_REPO: downloads to cache, returns cache path
     *   4. For OLLAMA_BLOB: searches blob directories, validates GGUF magic
     *   5. For HTTP_URL: downloads to cache, returns cache path
     * 
     * @param input Model path/URL/ID/name
     * @param progress Optional progress callback for downloads
     * @return ResolvedModelPath with local_path set on success
     */
    ResolvedModelPath Resolve(const std::string& input, 
                              DownloadProgressCallback progress = nullptr);

    // ---- HuggingFace Operations ----
    
    /**
     * Search HuggingFace Hub for GGUF models.
     * Uses WinHTTP for the API call.
     */
    std::vector<HFModelInfo> SearchHuggingFace(const std::string& query, int limit = 10);
    
    /**
     * Get info about a specific HuggingFace repo, including available GGUF files.
     */
    HFModelInfo GetHuggingFaceModelInfo(const std::string& repo_id);
    
    /**
     * Get available GGUF files from a HuggingFace repo.
     */
    std::vector<HFModelFileInfo> GetHuggingFaceGGUFFiles(const std::string& repo_id);
    
    /**
     * Download a GGUF file from HuggingFace to the local cache.
     * Returns the local cache path on success.
     */
    std::string DownloadFromHuggingFace(const std::string& repo_id,
                                        const std::string& filename,
                                        DownloadProgressCallback progress = nullptr);

    // ---- Ollama Blob Operations ----
    
    /**
     * Search all known Ollama blob directories for GGUF models.
     * Validates each found blob with GGUF magic bytes.
     */
    std::vector<OllamaBlobInfo> FindOllamaBlobs();
    
    /**
     * Resolve an Ollama model name to a local blob path.
     * Searches standard Ollama directories, validates GGUF magic.
     */
    OllamaBlobInfo ResolveOllamaBlob(const std::string& model_name);

    // ---- HTTP Download Operations ----
    
    /**
     * Download a file from an HTTP/HTTPS URL using WinHTTP.
     * Returns the local cache path on success.
     */
    std::string DownloadFromURL(const std::string& url,
                                DownloadProgressCallback progress = nullptr);

    // ---- Configuration ----
    
    /**
     * Set the local cache directory for downloaded models.
     * Default: %USERPROFILE%/.cache/rawrxd/models
     */
    void SetCacheDirectory(const std::string& path);
    std::string GetCacheDirectory() const;
    
    /**
     * Set HuggingFace API token for private model access.
     */
    void SetHuggingFaceToken(const std::string& token);
    
    /**
     * Cancel any active download operation.
     */
    void CancelDownload();
    bool IsDownloading() const;

    // ---- Validation ----
    
    /**
     * Validate that a file has valid GGUF magic bytes (0x46554747).
     */
    static bool ValidateGGUFMagic(const std::string& filepath);

    /**
     * Get a human-readable description of a model source type.
     */
    static std::string SourceTypeToString(GGUFConstants::ModelSourceType type);

private:
    // ---- WinHTTP Helpers ----
    std::string WinHTTPGet(const std::string& url, const std::string& auth_token = "");
    bool WinHTTPDownload(const std::string& url, const std::string& output_path,
                         DownloadProgressCallback progress = nullptr,
                         const std::string& auth_token = "");
    
    // ---- Path Helpers ----
    std::string GetDefaultCacheDir() const;
    std::string GetUserHomeDir() const;
    std::string SanitizeFilename(const std::string& name) const;
    std::string ExtractFilenameFromURL(const std::string& url) const;
    
    // ---- Ollama Helpers ----
    std::vector<std::string> GetOllamaSearchPaths() const;
    std::vector<std::string> GetOllamaBlobPaths() const;
    
    // ---- JSON Parsing Helpers (minimal, no nlohmann dependency) ----
    std::string ExtractJSONString(const std::string& json, const std::string& key) const;
    uint64_t ExtractJSONNumber(const std::string& json, const std::string& key) const;
    std::vector<std::string> SplitJSONObjects(const std::string& json) const;
    
    // ---- State ----
    std::string m_cacheDir;
    std::string m_hfToken;
    std::atomic<bool> m_downloading{false};
    std::atomic<bool> m_cancelRequested{false};
    std::mutex m_downloadMutex;
};

} // namespace RawrXD

