#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace RawrXD {

struct ModelDownloadInfo {
    std::string name;
    std::string displayName;
    std::string url;
    int64_t sizeBytes{0};
    std::string description;
    bool isDefault{false};
};

class AutoModelDownloader {
public:
    explicit AutoModelDownloader();
    ~AutoModelDownloader();

    // Check if any models are available locally
    bool hasLocalModels() const;
    
    // Get list of recommended tiny models
    std::vector<ModelDownloadInfo> getRecommendedModels() const;
    
    // Download a specific model
    void downloadModel(const ModelDownloadInfo& model, const std::string& destinationPath);
    
    // Download default tiny model
    void downloadDefaultModel(const std::string& destinationPath);
    
    // Cancel active download
    void cancelDownload();
    
    // Set Ollama models directory to check
    void setModelsDirectory(const std::string& path);

    // Callbacks
    std::function<void(const std::string&)> onDownloadStarted;
    std::function<void(const std::string&, int64_t, int64_t)> onDownloadProgress;
    std::function<void(const std::string&, const std::string&)> onDownloadCompleted;
    std::function<void(const std::string&, const std::string&)> onDownloadFailed;
    
    std::function<void()> onNoModelsDetected;
    std::function<void(int)> onModelsAvailable;

private:
    std::string findOllamaDirectory() const;
    void checkLocalModels();

    std::string m_modelsDirectory;
    bool m_isDownloading = false;
};

} // namespace RawrXD


