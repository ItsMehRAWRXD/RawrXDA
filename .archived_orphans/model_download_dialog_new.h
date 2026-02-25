// Model Download Dialog - User-friendly interface for auto-downloading models
#pragma once

#include "auto_model_downloader.h"
/* Qt removed */
/* Qt removed */

class QListWidget;
class QProgressBar;
class QLabel;
class QPushButton;
class QListWidgetItem;

namespace RawrXD {

class ModelDownloadDialog : public QDialog {
    /* Q_OBJECT */

public:
    explicit ModelDownloadDialog(void* parent = nullptr);
    virtual ~ModelDownloadDialog();

private /* slots */ public:
    void onDownloadClicked();
    void onSkipClicked();
    void onDownloadProgress(const std::wstring& modelName, qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadCompleted(const std::wstring& modelName, const std::wstring& filePath);
    void onDownloadFailed(const std::wstring& modelName, const std::wstring& error);

private:
    void setupUI();
    void setupConnections();
    void loadRecommendedModels();

    void* m_modelList{nullptr};
    void* m_progressBar{nullptr};
    void* m_statusLabel{nullptr};
    void* m_downloadButton{nullptr};
    void* m_skipButton{nullptr};
    
    AutoModelDownloader* m_downloader{nullptr};
    std::vector<ModelDownloadInfo> m_models;
};

} // namespace RawrXD
