// Auto Model Downloader - Downloads tiny default models if none found
// Production-ready with progress tracking and error handling
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <functional>

namespace RawrXD {

struct ModelDownloadInfo {
    QString name;
    QString displayName;
    QString url;
    qint64 sizeBytes{0};
    QString description;
    bool isDefault{false};
};

class AutoModelDownloader : public QObject {
    Q_OBJECT

public:
    explicit AutoModelDownloader(QObject* parent = nullptr);
    ~AutoModelDownloader() override;

    // Check if any models are available locally
    bool hasLocalModels() const;
    
    // Get list of recommended tiny models
    QVector<ModelDownloadInfo> getRecommendedModels() const;
    
    // Download a specific model
    void downloadModel(const ModelDownloadInfo& model, const QString& destinationPath);
    
    // Download default tiny model
    void downloadDefaultModel(const QString& destinationPath);
    
    // Cancel active download
    void cancelDownload();
    
    // Set Ollama models directory to check
    void setModelsDirectory(const QString& path);

signals:
    void downloadStarted(const QString& modelName);
    void downloadProgress(const QString& modelName, qint64 bytesReceived, qint64 bytesTotal);
    void downloadCompleted(const QString& modelName, const QString& filePath);
    void downloadFailed(const QString& modelName, const QString& error);
    
    void noModelsDetected();
    void modelsAvailable(int count);

private slots:
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onDownloadError(QNetworkReply::NetworkError error);

private:
    void checkLocalModels();
    QString findOllamaDirectory() const;
    
    QNetworkAccessManager* m_networkManager{nullptr};
    QNetworkReply* m_currentReply{nullptr};
    
    QString m_modelsDirectory;
    QString m_currentModelName;
    QString m_currentDestination;
    
    bool m_downloadInProgress{false};
};

} // namespace RawrXD
