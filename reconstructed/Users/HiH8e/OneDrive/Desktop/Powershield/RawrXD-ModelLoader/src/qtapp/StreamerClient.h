#pragma once

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

struct StreamerRequest {
    QString model;      // e.g., "quantumide-feature"
    QString prompt;     // goal or command
    bool stream{true};  // request streaming chunks
};

class StreamerClient : public QObject {
    Q_OBJECT
public:
    explicit StreamerClient(const QUrl& baseUrl, QObject* parent = nullptr);

    void invoke(const StreamerRequest& req);
    // New: start generation with an associated taskId for orchestrator tracking
    void startGeneration(const QString& model, const QString& prompt, const QString& taskId);
    // Overload: start without taskId (non-orchestrated streams)
    void startGeneration(const QString& model, const QString& prompt);

signals:
    void chunkReceived(const QString& text);
    void completed();
    void taskCompleted(bool success, const QString& taskId, const QString& modelName, const QString& fullOutput);
    void errorOccurred(const QString& message);

private:
    QUrl baseUrl_;
    QNetworkAccessManager* nam_{};
};
