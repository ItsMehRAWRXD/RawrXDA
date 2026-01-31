// Auto Model Downloader - Downloads tiny default models if none found
// Production-ready with progress tracking and error handling
#pragma once


#include <functional>

namespace RawrXD {

struct ModelDownloadInfo {
    std::string name;
    std::string displayName;
    std::string url;
    int64_t sizeBytes{0};
    std::string description;
    bool isDefault{false};
};

class AutoModelDownloader : public void {

public:
    explicit AutoModelDownloader(void* parent = nullptr);
    ~AutoModelDownloader() override;

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


    void downloadStarted(const std::string& modelName);
    void downloadProgress(const std::string& modelName, int64_t bytesReceived, int64_t bytesTotal);
    void downloadCompleted(const std::string& modelName, const std::string& filePath);
    void downloadFailed(const std::string& modelName, const std::string& error);
    
    void noModelsDetected();
    void modelsAvailable(int count);

private:
    void onDownloadProgress(int64_t bytesReceived, int64_t bytesTotal);
    void onDownloadFinished();
    void onDownloadError(void*::NetworkError error);

private:
    void checkLocalModels();
    std::string findOllamaDirectory() const;
    
    void** m_networkManager{nullptr};
    void** m_currentReply{nullptr};
    
    std::string m_modelsDirectory;
    std::string m_currentModelName;
    std::string m_currentDestination;
    
    bool m_downloadInProgress{false};
};

} // namespace RawrXD


