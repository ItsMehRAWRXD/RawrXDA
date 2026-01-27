#include "unified_backend.hpp"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMetaObject>

UnifiedBackend::UnifiedBackend(QObject* parent) 
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void UnifiedBackend::submit(const UnifiedRequest& req)
{
    if (req.backend == "local") {
        // Forward to existing InferenceEngine (worker thread)
        if (m_localEngine) {
            QMetaObject::invokeMethod(m_localEngine, "request", Qt::QueuedConnection,
                                      Q_ARG(QString, req.prompt), 
                                      Q_ARG(qint64, req.reqId));
        } else {
            emit error(req.reqId, "Local engine not initialized");
        }
    } else if (req.backend == "llama") {
        submitLlamaCpp(req);
    } else if (req.backend == "openai") {
        submitOpenAI(req);
    } else if (req.backend == "claude") {
        submitClaude(req);
    } else if (req.backend == "gemini") {
        submitGemini(req);
    } else {
        emit error(req.reqId, "Unknown backend: " + req.backend);
    }
}

void UnifiedBackend::submitLlamaCpp(const UnifiedRequest& req)
{
    QUrl url("http://localhost:8080/completion");
    QJsonObject body{
        {"prompt", req.prompt},
        {"stream", true},
        {"n_predict", 100}
    };
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply* reply = m_nam->post(request, QJsonDocument(body).toJson());
    
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, req]() {
        while (reply->canReadLine()) {
            QByteArray line = reply->readLine();
            auto doc = QJsonDocument::fromJson(line);
            QString token = doc["content"].toString();
            if (!token.isEmpty()) {
                emit streamToken(req.reqId, token);
            }
        }
    });
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, req]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit error(req.reqId, "llama.cpp error: " + reply->errorString());
        }
        emit streamFinished(req.reqId);
        reply->deleteLater();
    });
}

void UnifiedBackend::submitOpenAI(const UnifiedRequest& req)
{
    QUrl url("https://api.openai.com/v1/chat/completions");
    QJsonObject body{
        {"model", "gpt-3.5-turbo"},
        {"messages", QJsonArray{
            QJsonObject{{"role", "user"}, {"content", req.prompt}}
        }},
        {"stream", true}
    };
    
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + req.apiKey).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply* reply = m_nam->post(request, QJsonDocument(body).toJson());
    
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, req]() {
        while (reply->canReadLine()) {
            QByteArray line = reply->readLine().trimmed();
            if (!line.startsWith("data: ")) continue;
            
            line = line.mid(6); // Remove "data: " prefix
            if (line == "[DONE]") {
                emit streamFinished(req.reqId);
                continue;
            }
            
            auto doc = QJsonDocument::fromJson(line);
            auto choices = doc["choices"].toArray();
            if (!choices.isEmpty()) {
                QString token = choices[0].toObject()["delta"].toObject()["content"].toString();
                if (!token.isEmpty()) {
                    emit streamToken(req.reqId, token);
                }
            }
        }
    });
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, req]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit error(req.reqId, "OpenAI error: " + reply->errorString());
        }
        emit streamFinished(req.reqId);
        reply->deleteLater();
    });
}

void UnifiedBackend::submitClaude(const UnifiedRequest& req)
{
    QUrl url("https://api.anthropic.com/v1/messages");
    QJsonObject body{
        {"model", "claude-3-sonnet-20240229"},
        {"max_tokens", 1000},
        {"messages", QJsonArray{
            QJsonObject{{"role", "user"}, {"content", req.prompt}}
        }},
        {"stream", true}
    };
    
    QNetworkRequest request(url);
    request.setRawHeader("x-api-key", req.apiKey.toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply* reply = m_nam->post(request, QJsonDocument(body).toJson());
    
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, req]() {
        while (reply->canReadLine()) {
            QByteArray line = reply->readLine().trimmed();
            if (!line.startsWith("data: ")) continue;
            
            line = line.mid(6); // Remove "data: " prefix
            auto doc = QJsonDocument::fromJson(line);
            
            // Claude streaming format: {"type":"content_block_delta","delta":{"text":"..."}}
            QString type = doc["type"].toString();
            if (type == "content_block_delta") {
                QString token = doc["delta"].toObject()["text"].toString();
                if (!token.isEmpty()) {
                    emit streamToken(req.reqId, token);
                }
            } else if (type == "message_stop") {
                emit streamFinished(req.reqId);
            }
        }
    });
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, req]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit error(req.reqId, "Claude error: " + reply->errorString());
        }
        emit streamFinished(req.reqId);
        reply->deleteLater();
    });
}

void UnifiedBackend::submitGemini(const UnifiedRequest& req)
{
    QUrl url("https://generativelanguage.googleapis.com/v1beta/models/gemini-pro:streamGenerateContent?alt=sse&key=" + req.apiKey);
    QJsonObject body{
        {"contents", QJsonArray{
            QJsonObject{{"parts", QJsonArray{
                QJsonObject{{"text", req.prompt}}
            }}}
        }},
        {"generationConfig", QJsonObject{
            {"temperature", 0.8}
        }}
    };
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QNetworkReply* reply = m_nam->post(request, QJsonDocument(body).toJson());
    
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, req]() {
        while (reply->canReadLine()) {
            QByteArray line = reply->readLine().trimmed();
            if (!line.startsWith("data: ")) continue;
            
            line = line.mid(6); // Remove "data: " prefix
            auto doc = QJsonDocument::fromJson(line);
            auto candidates = doc["candidates"].toArray();
            
            if (!candidates.isEmpty()) {
                auto content = candidates[0].toObject()["content"].toObject();
                auto parts = content["parts"].toArray();
                if (!parts.isEmpty()) {
                    QString token = parts[0].toObject()["text"].toString();
                    if (!token.isEmpty()) {
                        emit streamToken(req.reqId, token);
                    }
                }
            }
        }
    });
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, req]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit error(req.reqId, "Gemini error: " + reply->errorString());
        }
        emit streamFinished(req.reqId);
        reply->deleteLater();
    });
}

void UnifiedBackend::onLocalDone(qint64 id, const QString& answer)
{
    // Local engine doesn't stream by default - emit as single token
    emit streamToken(id, answer);
    emit streamFinished(id);
}
