#include "LanguageClientHost.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QDebug>

LanguageClientHost::LanguageClientHost(QObject* parent)
    : QObject(parent)
{
}

LanguageClientHost::~LanguageClientHost()
{
    for (auto& info : servers_) {
        if (info.process) {
            info.process->terminate();
            info.process->waitForFinished(2000);
            delete info.process;
        }
    }
}

void LanguageClientHost::startServer(const QString& language)
{
    if (servers_.contains(language)) {
        qWarning() << "LSP server for" << language << "already running";
        return;
    }

    QProcess* proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::SeparateChannels);
    
    ServerInfo info;
    info.process = proc;
    info.language = language;
    
    connect(proc, &QProcess::readyReadStandardOutput, this, &LanguageClientHost::onStdoutReady);
    connect(proc, &QProcess::readyReadStandardError, this, &LanguageClientHost::onStderrReady);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &LanguageClientHost::onProcessFinished);
    
    QStringList args;
    QString executable;
    
    if (language == "cpp" || language == "c") {
        executable = "clangd";
        args << "--background-index" << "--log=error";
    } else if (language == "python") {
        executable = "pylsp";
    } else if (language == "rust") {
        executable = "rust-analyzer";
    } else if (language == "go") {
        executable = "gopls";
    } else if (language == "typescript" || language == "javascript") {
        executable = "typescript-language-server";
        args << "--stdio";
    } else {
        emit serverFailed(language, "Unsupported language");
        return;
    }
    
    proc->start(executable, args);
    
    if (!proc->waitForStarted(5000)) {
        emit serverFailed(language, "Failed to start " + executable);
        delete proc;
        return;
    }
    
    servers_[language] = info;
    
    // Send initialize request
    QJsonObject params;
    params["processId"] = QJsonValue::Null;
    params["rootUri"] = fileToUri(QDir::currentPath());
    params["capabilities"] = QJsonObject{
        {"textDocument", QJsonObject{
            {"hover", QJsonObject{{"contentFormat", QJsonArray{"markdown", "plaintext"}}}},
            {"completion", QJsonObject{{"completionItem", QJsonObject{{"snippetSupport", true}}}}},
            {"publishDiagnostics", QJsonObject{{"relatedInformation", true}}}
        }}
    };
    
    int reqId = nextRequestId();
    servers_[language].initializeRequestId = reqId;
    sendRequest(language, "initialize", params);
    
    qDebug() << "[LSP] Started" << executable << "for" << language;
    emit serverStarted(language);
}

void LanguageClientHost::stopServer(const QString& language)
{
    if (!servers_.contains(language)) return;
    
    sendNotification(language, "shutdown", QJsonObject{});
    sendNotification(language, "exit", QJsonObject{});
    
    ServerInfo info = servers_.take(language);
    if (info.process) {
        info.process->waitForFinished(2000);
        delete info.process;
    }
    
    qDebug() << "[LSP] Stopped server for" << language;
}

void LanguageClientHost::didOpen(const QString& file, const QString& language, const QString& text)
{
    if (!servers_.contains(language)) {
        startServer(language);
    }
    
    QJsonObject params;
    params["textDocument"] = QJsonObject{
        {"uri", fileToUri(file)},
        {"languageId", language},
        {"version", 1},
        {"text", text}
    };
    
    sendNotification(language, "textDocument/didOpen", params);
}

void LanguageClientHost::didChange(const QString& file, const QString& text)
{
    // Determine language from file extension
    QString lang;
    if (file.endsWith(".cpp") || file.endsWith(".h") || file.endsWith(".c")) lang = "cpp";
    else if (file.endsWith(".py")) lang = "python";
    else if (file.endsWith(".rs")) lang = "rust";
    else if (file.endsWith(".go")) lang = "go";
    else if (file.endsWith(".ts")) lang = "typescript";
    else if (file.endsWith(".js")) lang = "javascript";
    else return;
    
    if (!servers_.contains(lang)) return;
    
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", fileToUri(file)}};
    params["contentChanges"] = QJsonArray{
        QJsonObject{{"text", text}}
    };
    
    sendNotification(lang, "textDocument/didChange", params);
}

void LanguageClientHost::didClose(const QString& file)
{
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", fileToUri(file)}};
    
    // Send to all servers (file might be multi-language project)
    for (const QString& lang : servers_.keys()) {
        sendNotification(lang, "textDocument/didClose", params);
    }
}

void LanguageClientHost::requestHover(const QString& file, int line, int col)
{
    QString lang;
    if (file.endsWith(".cpp") || file.endsWith(".h")) lang = "cpp";
    else if (file.endsWith(".py")) lang = "python";
    else if (file.endsWith(".rs")) lang = "rust";
    else return;
    
    if (!servers_.contains(lang) || !servers_[lang].initialized) return;
    
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", fileToUri(file)}};
    params["position"] = QJsonObject{{"line", line}, {"character", col}};
    
    sendRequest(lang, "textDocument/hover", params);
}

void LanguageClientHost::requestCompletion(const QString& file, int line, int col)
{
    QString lang;
    if (file.endsWith(".cpp") || file.endsWith(".h")) lang = "cpp";
    else if (file.endsWith(".py")) lang = "python";
    else if (file.endsWith(".rs")) lang = "rust";
    else return;
    
    if (!servers_.contains(lang) || !servers_[lang].initialized) return;
    
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", fileToUri(file)}};
    params["position"] = QJsonObject{{"line", line}, {"character", col}};
    
    sendRequest(lang, "textDocument/completion", params);
}

void LanguageClientHost::requestDefinition(const QString& file, int line, int col)
{
    QString lang;
    if (file.endsWith(".cpp") || file.endsWith(".h")) lang = "cpp";
    else if (file.endsWith(".py")) lang = "python";
    else if (file.endsWith(".rs")) lang = "rust";
    else return;
    
    if (!servers_.contains(lang) || !servers_[lang].initialized) return;
    
    QJsonObject params;
    params["textDocument"] = QJsonObject{{"uri", fileToUri(file)}};
    params["position"] = QJsonObject{{"line", line}, {"character", col}};
    
    sendRequest(lang, "textDocument/definition", params);
}

void LanguageClientHost::onStdoutReady()
{
    QProcess* proc = qobject_cast<QProcess*>(sender());
    if (!proc) return;
    
    // Find which server this process belongs to
    QString lang;
    for (auto it = servers_.begin(); it != servers_.end(); ++it) {
        if (it->process == proc) {
            lang = it.key();
            break;
        }
    }
    
    if (lang.isEmpty()) return;
    
    QByteArray data = proc->readAllStandardOutput();
    servers_[lang].buffer.append(data);
    
    // Parse complete JSON-RPC messages
    parseJsonRpc(servers_[lang].buffer);
}

void LanguageClientHost::onStderrReady()
{
    QProcess* proc = qobject_cast<QProcess*>(sender());
    if (!proc) return;
    
    QByteArray err = proc->readAllStandardError();
    qDebug() << "[LSP stderr]" << err;
}

void LanguageClientHost::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    QProcess* proc = qobject_cast<QProcess*>(sender());
    if (!proc) return;
    
    QString lang;
    for (auto it = servers_.begin(); it != servers_.end(); ++it) {
        if (it->process == proc) {
            lang = it.key();
            break;
        }
    }
    
    if (!lang.isEmpty()) {
        qWarning() << "[LSP]" << lang << "server exited with code" << exitCode;
        servers_.remove(lang);
        emit serverFailed(lang, QString("Exited with code %1").arg(exitCode));
    }
}

void LanguageClientHost::parseJsonRpc(const QByteArray& data)
{
    // JSON-RPC messages are prefixed with Content-Length header
    int idx = 0;
    while (idx < data.size()) {
        int headerEnd = data.indexOf("\r\n\r\n", idx);
        if (headerEnd < 0) break;
        
        QByteArray header = data.mid(idx, headerEnd - idx);
        if (header.startsWith("Content-Length: ")) {
            int len = header.mid(16).trimmed().toInt();
            int msgStart = headerEnd + 4;
            
            if (msgStart + len > data.size()) {
                break;  // Incomplete message
            }
            
            QByteArray msg = data.mid(msgStart, len);
            QJsonDocument doc = QJsonDocument::fromJson(msg);
            
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                
                if (obj.contains("method")) {
                    // Notification or request from server
                    QString method = obj["method"].toString();
                    
                    if (method == "textDocument/publishDiagnostics") {
                        QJsonObject params = obj["params"].toObject();
                        QString uri = params["uri"].toString();
                        QJsonArray diags = params["diagnostics"].toArray();
                        emit diagnosticsReceived(uriToFile(uri), diags);
                    }
                } else if (obj.contains("result")) {
                    // Response to our request
                    int reqId = obj["id"].toInt();
                    QString method = pendingRequests_.take(reqId);
                    
                    if (method == "initialize") {
                        // Find server by requestId
                        for (auto& info : servers_) {
                            if (info.initializeRequestId == reqId) {
                                info.initialized = true;
                                sendNotification(info.language, "initialized", QJsonObject{});
                                break;
                            }
                        }
                    } else if (method == "textDocument/hover") {
                        QJsonObject result = obj["result"].toObject();
                        if (result.contains("contents")) {
                            // TODO: Extract markdown and emit hoverInfo
                        }
                    } else if (method == "textDocument/completion") {
                        QJsonValue result = obj["result"];
                        if (result.isArray()) {
                            emit completionReady(result.toArray());
                        } else if (result.isObject()) {
                            emit completionReady(result.toObject()["items"].toArray());
                        }
                    }
                }
            }
            
            idx = msgStart + len;
        } else {
            idx = headerEnd + 4;
        }
    }
}

void LanguageClientHost::sendRequest(const QString& language, const QString& method, const QJsonObject& params)
{
    if (!servers_.contains(language)) return;
    
    int reqId = nextRequestId();
    pendingRequests_[reqId] = method;
    
    QJsonObject msg;
    msg["jsonrpc"] = "2.0";
    msg["id"] = reqId;
    msg["method"] = method;
    msg["params"] = params;
    
    QByteArray json = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    QByteArray header = QString("Content-Length: %1\r\n\r\n").arg(json.size()).toUtf8();
    
    servers_[language].process->write(header + json);
}

void LanguageClientHost::sendNotification(const QString& language, const QString& method, const QJsonObject& params)
{
    if (!servers_.contains(language)) return;
    
    QJsonObject msg;
    msg["jsonrpc"] = "2.0";
    msg["method"] = method;
    msg["params"] = params;
    
    QByteArray json = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    QByteArray header = QString("Content-Length: %1\r\n\r\n").arg(json.size()).toUtf8();
    
    servers_[language].process->write(header + json);
}

QString LanguageClientHost::fileToUri(const QString& filePath) const
{
    return QUrl::fromLocalFile(QFileInfo(filePath).absoluteFilePath()).toString();
}

QString LanguageClientHost::uriToFile(const QString& uri) const
{
    return QUrl(uri).toLocalFile();
}

int LanguageClientHost::nextRequestId()
{
    return requestIdCounter_++;
}
