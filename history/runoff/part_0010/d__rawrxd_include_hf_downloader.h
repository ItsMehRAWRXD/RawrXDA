#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <functional>
#include "common_types.h"  // DownloadProgress definition

struct ModelInfo {
    std::string repo_id;
    std::string model_name;
    std::string description;
    std::vector<std::string> files;
    uint64_t total_size_bytes = 0;
};

using ProgressCallback = std::function<void(const DownloadProgress&)>;

/**
 * @class HFDownloader
 * @brief Downloads models from Hugging Face Hub
 * Supports GGUF and other formats; can detect available models and download to cache
 */
class HFDownloader {
public:
    HFDownloader();
    ~HFDownloader();

    // Model discovery
    bool SearchModels(const std::string& query, std::vector<ModelInfo>& results, 
                      const std::string& token = "");
    bool GetModelInfo(const std::string& repo_id, ModelInfo& info,
                      const std::string& token = "");
    
    // Model download
    bool DownloadModel(const std::string& repo_id, const std::string& filename,
                       const std::string& output_dir, ProgressCallback callback = nullptr,
                       const std::string& token = "");
    
    bool DownloadModelAsync(const std::string& repo_id, const std::string& filename,
                            const std::string& output_dir, ProgressCallback callback = nullptr,
                            const std::string& token = "");
    
    bool CancelDownload();
    
    // Token validation
    bool ValidateHFToken(const std::string& token);
    
    // Format detection
    std::vector<std::string> ParseAvailableFormats(const std::string& repo_id,
                                                    const std::string& token = "");
    
    // Query helpers
    bool IsDownloading() const { return is_downloading_.load(); }
    DownloadProgress GetProgress() const { return current_progress_; }

private:
    bool FetchJSON(const std::string& url, std::string& response,
                   const std::string& token = "");
    
    bool DownloadFile(const std::string& url, const std::string& output_path,
                      ProgressCallback callback = nullptr, const std::string& token = "");
    
    bool ParseModelMetadata(const std::string& response, ModelInfo& info);
    
    std::string BuildHFUrl(const std::string& repo_id, const std::string& filename) const;
    std::string GetAuthHeader(const std::string& token) const;
    
    std::atomic<bool> is_downloading_{false};
    bool cancel_requested_ = false;
    DownloadProgress current_progress_;
    ProgressCallback progress_callback_;
    std::unique_ptr<std::thread> download_thread_;
};
