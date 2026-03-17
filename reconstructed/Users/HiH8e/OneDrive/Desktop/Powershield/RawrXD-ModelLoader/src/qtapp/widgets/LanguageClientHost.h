#pragma once
/*  LanguageClientHost.h
    Week 3: LSP client for clangd/pylsp/rust-analyzer/gopls
    Spawns language servers, handles JSON-RPC, emits diagnostics      */

#include <QObject>
#include <QProcess>
#include <QHash>
#include <QJsonArray>
#include <QPointer>

class LanguageClientHost : public QObject
{
    Q_OBJECT
public:
    explicit LanguageClientHost(QObject* parent = nullptr);
    ~LanguageClientHost();

signals:
    void diagnosticsReceived(const QString& file, const QJsonArray& diagnostics);
    void hoverInfo(const QString& file, int line, int col, const QString& markdown);
    void completionReady(const QJsonArray& items);
    void definitionFound(const QString& file, int line, int col);
    void serverStarted(const QString& language);
    void serverFailed(const QString& language, const QString& error);

public slots:
    void startServer(const QString& language);
    void stopServer(const QString& language);
    void didOpen(const QString& file, const QString& language, const QString& text);
    void didChange(const QString& file, const QString& text);
    void didClose(const QString& file);
    void requestHover(const QString& file, int line, int col);
    void requestCompletion(const QString& file, int line, int col);
    void requestDefinition(const QString& file, int line, int col);

private slots:
    void onStdoutReady();
    void onStderrReady();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void parseJsonRpc(const QByteArray& data);
    void sendRequest(const QString& language, const QString& method, const QJsonObject& params);
    void sendNotification(const QString& language, const QString& method, const QJsonObject& params);
    QString fileToUri(const QString& filePath) const;
    QString uriToFile(const QString& uri) const;
    int nextRequestId();

private:
    struct ServerInfo {
        QProcess* process{nullptr};
        QString language{};
        QByteArray buffer{};  // JSON-RPC message buffer
        int initializeRequestId{0};
        bool initialized{false};
    };

    QHash<QString, ServerInfo> servers_;  // language -> server
    int requestIdCounter_{1};
    QHash<int, QString> pendingRequests_;  // requestId -> method
};
