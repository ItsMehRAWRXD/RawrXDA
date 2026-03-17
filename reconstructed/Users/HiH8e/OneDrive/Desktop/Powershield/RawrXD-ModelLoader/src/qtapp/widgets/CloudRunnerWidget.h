#pragma once
/*  CloudRunnerWidget.h
    Week 1: One-click cloud builds on GitHub Actions / AWS / GCP
    Ship the headline feature: "64 cores for 3¢/min" button              */

#include <QWidget>
#include <QPointer>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QWebSocket>
#include <QElapsedTimer>

QT_BEGIN_NAMESPACE
class QComboBox;
class QPushButton;
class QProgressBar;
class QPlainTextEdit;
class QLabel;
class QNetworkReply;
QT_END_NAMESPACE

class CloudRunnerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CloudRunnerWidget(QWidget* parent = nullptr);
    ~CloudRunnerWidget();

signals:
    void jobStarted(const QString& jobId);
    void jobProgress(int percent);
    void jobCompleted(bool success, const QString& artifactUrl);
    void logChunk(const QString& text);
    void costUpdated(double dollars);

public slots:
    void startJob(const QString& provider, const QString& instanceType);
    void cancelJob();
    void downloadArtifacts(const QString& runId);

private slots:
    void onZipReady(const QString& zipPath);
    void onUploadFinished(QNetworkReply* reply);
    void onDispatchFinished(QNetworkReply* reply);
    void onPollFinished(QNetworkReply* reply);
    void onArtifactDownloaded(QNetworkReply* reply);
    void onWebSocketMessage(const QString& msg);
    void updateCostEstimate();

private:
    void setupUi();
    QString zipWorkspace(const QString& projectRoot);
    void uploadZip(const QString& zipPath);
    void dispatchWorkflow(const QString& instanceType, const QString& zipUrl);
    void startPolling(const QString& runId);
    void connectWebSocket(const QString& jobId);
    double getSpotPrice(const QString& instanceType) const;

private:
    QComboBox* providerCombo_{};
    QComboBox* instanceCombo_{};
    QPushButton* startBtn_{};
    QPushButton* cancelBtn_{};
    QProgressBar* progressBar_{};
    QPlainTextEdit* logOutput_{};
    QLabel* costLabel_{};
    QLabel* statusLabel_{};

    QNetworkAccessManager* network_{};
    QWebSocket* logSocket_{};
    QTimer* pollTimer_{};
    
    QString currentJobId_{};
    QString currentRunId_{};
    QString currentArtifactId_{};
    QString githubToken_{};
    QString awsAccessKey_{};
    QElapsedTimer jobTimer_{};
    QElapsedTimer costTimer_{};
    
    bool jobRunning_{false};
    int pollIntervalMs_{5000};
};
