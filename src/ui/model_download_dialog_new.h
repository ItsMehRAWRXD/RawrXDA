// Model Download Dialog - User-friendly interface for auto-downloading models
#pragma once

#include "auto_model_downloader.h"


namespace RawrXD {

class ModelDownloadDialog : public void {

public:
    explicit ModelDownloadDialog(void* parent = nullptr);
    virtual ~ModelDownloadDialog();

private:
    void onDownloadClicked();
    void onSkipClicked();
    void onDownloadProgress(const std::string& modelName, int64_t bytesReceived, int64_t bytesTotal);
    void onDownloadCompleted(const std::string& modelName, const std::string& filePath);
    void onDownloadFailed(const std::string& modelName, const std::string& error);

private:
    void setupUI();
    void setupConnections();
    void loadRecommendedModels();

    QListWidget* m_modelList{nullptr};
    void* m_progressBar{nullptr};
    void* m_statusLabel{nullptr};
    void* m_downloadButton{nullptr};
    void* m_skipButton{nullptr};
    
    AutoModelDownloader* m_downloader{nullptr};
    std::vector<ModelDownloadInfo> m_models;
};

} // namespace RawrXD


