// Auto Model Downloader - Downloads tiny default models if none found
// Production-ready with progress tracking and error handling
#pragma once

/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */
#include <functional>

namespace RawrXD {

struct ModelDownloadInfo {
    std::wstring name;
    std::wstring displayName;
    std::wstring url;
    qint64 sizeBytes{0};
    std::wstring description;
    bool isDefault{false};
};

class AutoModelDownloader  {
    /* /* Q_OBJECT */ */

public:
    explicit AutoModelDownloader(void* parent = nullptr);
    ~AutoModelDownloader() override;

    // Check if any models are available locally
    bool hasLocalModels() const;
    
    // Get list of recommended tiny models
    std::vector<ModelDownloadInfo> getRecommendedModels() const;
    
    // Download a specific model
    void downloadModel(const ModelDownloadInfo& model, const std::wstring& destinationPath);
    
    // Download default tiny model
    void downloadDefaultModel(const std::wstring& destinationPath);
    
    // Cancel active download
    void cancelDownload();
    
    // Set Ollama models directory to check
    void setModelsDirectory(const std::wstring& path);

/* signals */ public:
    void downloadStarted(const std::wstring& modelName);
    void downloadProgress(const std::wstring& modelName, qint64 bytesReceived, qint64 bytesTotal);
    void downloadCompleted(const std::wstring& modelName, const std::wstring& filePath);
    void downloadFailed(const std::wstring& modelName, const std::wstring& error);
    
    void noModelsDetected();
    void modelsAvailable(int count);

private /* slots */ public:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onDownloadError(QNetworkReply::NetworkError error);

private:
    void checkLocalModels();
    std::wstring findOllamaDirectory() const;
    
    void* m_networkManager{nullptr};
    void* m_currentReply{nullptr};
    
    std::wstring m_modelsDirectory;
    std::wstring m_currentModelName;
    std::wstring m_currentDestination;
    
    bool m_downloadInProgress{false};
};

} // namespace RawrXD
