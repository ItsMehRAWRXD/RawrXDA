#pragma once
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>

struct ModelInfo {
    std::string repo_id;
    std::string model_name;
    std::string description;
    uint64_t model_size;
    std::string url;
    std::vector<std::string> available_formats;
};

struct DownloadProgress {
    uint64_t downloaded_bytes;
    uint64_t total_bytes;
    float progress_percent;
    std::string current_file;
    bool is_completed;
    bool has_error;
    std::string error_message;
};

using ProgressCallback = std::function<void(const DownloadProgress&)>;

class HFDownloader {
public:
    HFDownloader();
    ~HFDownloader();

    // Search operations
    bool SearchModels(const std::string& query, std::vector<ModelInfo>& results, 
                     const std::string& token = "");
    bool GetModelInfo(const std::string& repo_id, ModelInfo& info, 
                     const std::string& token = "");
    
    // Download operations
    bool DownloadModel(const std::string& repo_id, const std::string& filename,
                      const std::string& output_dir, ProgressCallback callback,
                      const std::string& token = "");
    bool DownloadModelAsync(const std::string& repo_id, const std::string& filename,
                           const std::string& output_dir, ProgressCallback callback,
                           const std::string& token = "");
    
    // Control
    bool CancelDownload();
    DownloadProgress GetProgress() const { return current_progress_; }
    bool IsDownloading() const { return is_downloading_.load(); }
    
    // Utility
    bool ValidateHFToken(const std::string& token);
    std::vector<std::string> ParseAvailableFormats(const std::string& repo_id,
                                                   const std::string& token = "");

private:
    std::atomic<bool> is_downloading_;
    std::atomic<bool> cancel_requested_;
    DownloadProgress current_progress_;
    std::unique_ptr<std::thread> download_thread_;
    ProgressCallback progress_callback_;
    
    // HTTP operations
    bool FetchJSON(const std::string& url, std::string& response, 
                  const std::string& token = "");
    bool DownloadFile(const std::string& url, const std::string& output_path,
                     ProgressCallback callback, const std::string& token = "");
    
    // Helper functions
    std::string BuildHFUrl(const std::string& repo_id, const std::string& filename) const;
    std::string GetAuthHeader(const std::string& token) const;
    bool ParseModelMetadata(const std::string& json_response, ModelInfo& info);
};
