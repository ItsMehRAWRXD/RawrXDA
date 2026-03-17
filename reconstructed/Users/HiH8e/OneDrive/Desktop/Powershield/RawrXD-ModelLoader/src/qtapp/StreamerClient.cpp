#include "StreamerClient.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTextStream>

StreamerClient::StreamerClient(const QUrl& baseUrl, QObject* parent)
    : QObject(parent), baseUrl_(baseUrl), nam_(new QNetworkAccessManager(this)) {}

void StreamerClient::invoke(const StreamerRequest& req) {
    // Endpoint: POST {base}/api/generate
    QUrl url = baseUrl_;
    url.setPath("/api/generate");

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["model"] = req.model;
    body["prompt"] = req.prompt;
    body["stream"] = req.stream;

    QJsonDocument doc(body);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    QNetworkReply* reply = nam_->post(request, payload);

    connect(reply, &QNetworkReply::readyRead, [this, reply]() {
        const QByteArray data = reply->readAll();
        // Many local streamers send JSONL. We attempt to parse lines; else emit raw.
        QList<QByteArray> lines = data.split('\n');
        for (const QByteArray& line : lines) {
            if (line.trimmed().isEmpty()) continue;
            // Try JSON parse
            QJsonParseError err;
            QJsonDocument j = QJsonDocument::fromJson(line, &err);
            if (err.error == QJsonParseError::NoError && j.isObject()) {
                auto obj = j.object();
                QString out = obj.value("response").toString();
                if (!out.isEmpty()) emit chunkReceived(out);
            } else {
                emit chunkReceived(QString::fromUtf8(line));
            }
        }
    });

    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
        } else {
            emit completed();
        }
        reply->deleteLater();
    });
}

void StreamerClient::startGeneration(const QString& model, const QString& prompt, const QString& taskId) {
    // Endpoint: POST {base}/api/generate
    QUrl url = baseUrl_;
    url.setPath("/api/generate");

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["model"] = model;
    body["prompt"] = prompt;
    body["stream"] = true;

    QJsonDocument doc(body);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    QNetworkReply* reply = nam_->post(request, payload);
    
    // Store metadata as reply properties to avoid dangerous lambda captures
    reply->setProperty("taskId", taskId);
    reply->setProperty("model", model);
    reply->setProperty("accumulator", QString());

    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        // Validate sender to prevent use-after-free
        if (!reply || reply != qobject_cast<QNetworkReply*>(sender())) {
            return;
        }
        
        const QByteArray data = reply->readAll();
        QList<QByteArray> lines = data.split('\n');
        QString currentAccumulator = reply->property("accumulator").toString();
        
        for (const QByteArray& line : lines) {
            if (line.trimmed().isEmpty()) continue;
            QJsonParseError err;
            QJsonDocument j = QJsonDocument::fromJson(line, &err);
            QString out;
            if (err.error == QJsonParseError::NoError && j.isObject()) {
                auto obj = j.object();
                out = obj.value("response").toString();
            } else {
                out = QString::fromUtf8(line);
            }
            if (!out.isEmpty()) {
                currentAccumulator += out;
                emit chunkReceived(out);
            }
        }
        reply->setProperty("accumulator", currentAccumulator);
    });

    // Connect error signal explicitly for better diagnostics
    connect(reply, &QNetworkReply::errorOccurred, this, [this, reply](QNetworkReply::NetworkError code) {
        if (reply && code != QNetworkReply::NoError) {
            emit errorOccurred(QString("Network error %1: %2").arg(code).arg(reply->errorString()));
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // Strict sender validation
        if (!reply || reply != qobject_cast<QNetworkReply*>(sender())) {
            return;
        }
        
        const QString taskId = reply->property("taskId").toString();
        const QString model = reply->property("model").toString();
        const QString accumulator = reply->property("accumulator").toString();
        const bool ok = (reply->error() == QNetworkReply::NoError);
        
        if (!ok && reply->error() != QNetworkReply::OperationCanceledError) {
            emit errorOccurred(reply->errorString());
        }
        
        emit taskCompleted(ok, taskId, model, accumulator);
        emit completed();
        
        // Safe cleanup
        reply->deleteLater();
    });
}

void StreamerClient::startGeneration(const QString& model, const QString& prompt) {
    startGeneration(model, prompt, QString());
}
