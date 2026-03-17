// Model Download Dialog - User-friendly interface for auto-downloading models
#pragma once

#include "auto_model_downloader.h"
#include <QDialog>
#include <QVector>

class QListWidget;
class QProgressBar;
class QLabel;
class QPushButton;
class QListWidgetItem;

namespace RawrXD {

class ModelDownloadDialog : public QDialog {
    Q_OBJECT

public:
    explicit ModelDownloadDialog(QWidget* parent = nullptr);
    virtual ~ModelDownloadDialog();

private slots:
    void onDownloadClicked();
    void onSkipClicked();
    void onDownloadProgress(const QString& modelName, qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadCompleted(const QString& modelName, const QString& filePath);
    void onDownloadFailed(const QString& modelName, const QString& error);

private:
    void setupUI();
    void setupConnections();
    void loadRecommendedModels();

    QListWidget* m_modelList{nullptr};
    QProgressBar* m_progressBar{nullptr};
    QLabel* m_statusLabel{nullptr};
    QPushButton* m_downloadButton{nullptr};
    QPushButton* m_skipButton{nullptr};
    
    AutoModelDownloader* m_downloader{nullptr};
    QVector<ModelDownloadInfo> m_models;
};

} // namespace RawrXD
