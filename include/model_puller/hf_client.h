#pragma once
// ============================================================================
// hf_client.h — Hugging Face Hub API Client
// ============================================================================
// REST client for Hugging Face model discovery and download.
//   - List GGUF files in any HF repo
//   - Auto-detect quantization from filename
//   - Download with resume + progress
//   - Optional Bearer token for gated models
// ============================================================================

#ifndef RAWRXD_HF_CLIENT_H
#define RAWRXD_HF_CLIENT_H

#include "model_puller/download_manager.h"
#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {

// -------------------------------------------------------
// HF file info from repo tree API
// -------------------------------------------------------
struct HFFileInfo {
    std::string filename;
    std::string rfilename;   // relative path in repo
    uint64_t    sizeBytes = 0;
    std::string sha256;
    std::string quantization; // parsed from filename "Q4_K_M", "Q5_K_S" etc.
};

// -------------------------------------------------------
// HF repo metadata
// -------------------------------------------------------
struct HFRepoInfo {
    std::string repoId;       // e.g. "bartowski/Qwen2.5-Coder-32B-Instruct-GGUF"
    std::string modelName;    // derived from repoId
    std::string author;
    std::string description;
    int64_t     downloads = 0;
    int64_t     likes     = 0;
    std::vector<HFFileInfo> ggufFiles;
};

// -------------------------------------------------------
// HuggingFaceClient
// -------------------------------------------------------
class HuggingFaceClient {
public:
    HuggingFaceClient();
    ~HuggingFaceClient();

    // Set optional auth token for gated model access
    void SetToken(const std::string& hfToken);

    // List all GGUF files in a repo, with size + quantization info
    bool ListGGUFFiles(const std::string& repoId, std::vector<HFFileInfo>& filesOut);

    // Get repo metadata
    bool GetRepoInfo(const std::string& repoId, HFRepoInfo& infoOut);

    // Search HF for GGUF repos
    bool SearchModels(const std::string& query, std::vector<HFRepoInfo>& resultsOut,
                      int limit = 20);

    // Download a specific file from a repo
    //   repo: "bartowski/Qwen2.5-Coder-32B-Instruct-GGUF"
    //   filename: "Qwen2.5-Coder-32B-Instruct-Q4_K_M.gguf"
    //   destPath: full local path to save to
    bool Download(const std::string& repoId,
                  const std::string& filename,
                  const std::string& destPath,
                  ProgressCallback onProgress = nullptr,
                  bool resume = true);

    // Cancel active download
    void Cancel();

    // Build the download URL for a file in a repo
    static std::string BuildDownloadUrl(const std::string& repoId, const std::string& filename);

    // Extract quantization tag from filename like "Q4_K_M" from "model-Q4_K_M.gguf"
    static std::string ExtractQuantization(const std::string& filename);

private:
    std::string MakeAuthHeader() const;

    DownloadManager m_downloader;
    std::string     m_token;
};

} // namespace RawrXD

#endif // RAWRXD_HF_CLIENT_H
