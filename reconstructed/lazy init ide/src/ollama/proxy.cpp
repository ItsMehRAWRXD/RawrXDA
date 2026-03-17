// OllamaProxy - Fallback to Ollama REST API for unsupported models
#include "ollama_proxy.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QUrlQuery>
#include <QThread>

OllamaProxy::OllamaProxy(QObject* parent)
    : QObject(parent)
    , m_ollamaUrl("http://localhost:11434")
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
{
    qDebug() << "[OllamaProxy] Initialized with endpoint:" << m_ollamaUrl;
}

OllamaProxy::~OllamaProxy()
{
    stopGeneration();
}

void OllamaProxy::setModel(const QString& modelName)
{
    m_modelName = modelName;
    qInfo() << "[OllamaProxy] Model set to:" << modelName;
}

bool OllamaProxy::isOllamaAvailable()
{
    // Quick sync check if Ollama is running
    QNetworkRequest request(QUrl(m_ollamaUrl + "/api/tags"));
    QNetworkReply* reply = m_networkManager->get(request);
    
    // Wait up to 1 second for response
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(1000, &loop, &QEventLoop::quit);
    loop.exec();
    
    bool available = (reply->error() == QNetworkReply::NoError);
    reply->deleteLater();
    
    qDebug() << "[OllamaProxy] Ollama available:" << available;
    return available;
}

bool OllamaProxy::isModelAvailable(const QString& modelName)
{
    // Check if model exists in Ollama registry
    QNetworkRequest request(QUrl(m_ollamaUrl + "/api/tags"));
    QNetworkReply* reply = m_networkManager->get(request);
    
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();
    
    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return false;
    }
    
    QByteArray data = reply->readAll();
    reply->deleteLater();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonArray models = doc.object()["models"].toArray();
    
    for (const QJsonValue& val : models) {
        QString name = val.toObject()["name"].toString();
        if (name == modelName || name.startsWith(modelName + ":")) {
            qDebug() << "[OllamaProxy] Model found:" << name;
            return true;
        }
    }
    
    qWarning() << "[OllamaProxy] Model not found in Ollama:" << modelName;
    return false;
}

void OllamaProxy::generateResponse(const QString& prompt, float temperature, int maxTokens)
{
    if (m_modelName.isEmpty()) {
        emit error("No model selected");
        return;
    }
    
    // Stop any ongoing generation
    stopGeneration();
    
    qInfo() << "[OllamaProxy] Generating response for prompt:" << prompt.left(50) << "...";
    // Debug: report thread affinity to help diagnose cross-thread network issues
    qDebug() << "[OllamaProxy] generateResponse threads - this thread:" << QThread::currentThread()
             << "network manager thread:" << (m_networkManager ? m_networkManager->thread() : nullptr);
    
    // Build JSON request for Ollama API
    QJsonObject request;
    request["model"] = m_modelName;
    request["prompt"] = prompt;
    request["stream"] = true;  // Enable streaming
    
    QJsonObject options;
    options["temperature"] = temperature;
    options["num_predict"] = maxTokens;
    request["options"] = options;
    
    QJsonDocument doc(request);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    
    // Send POST request to /api/generate
    QNetworkRequest netRequest(QUrl(m_ollamaUrl + "/api/generate"));
    netRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    m_currentReply = m_networkManager->post(netRequest, jsonData);
    m_buffer.clear();
    
    // Connect signals for streaming response
    connect(m_currentReply, &QNetworkReply::readyRead, this, &OllamaProxy::onNetworkReply);
    connect(m_currentReply, &QNetworkReply::finished, this, [this]() {
        qDebug() << "[OllamaProxy] Generation finished";
        emit generationComplete();
        if (m_currentReply) {
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }
    });
    connect(m_currentReply, &QNetworkReply::errorOccurred, this, &OllamaProxy::onNetworkError);
}

void OllamaProxy::stopGeneration()
{
    if (m_currentReply) {
        qDebug() << "[OllamaProxy] Stopping generation";
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
}

void OllamaProxy::onNetworkReply()
{
    if (!m_currentReply) return;
    
    // Read available data
    QByteArray newData = m_currentReply->readAll();
    m_buffer.append(newData);
    
    // Process complete JSON lines (Ollama sends newline-delimited JSON)
    while (m_buffer.contains('\n')) {
        int newlinePos = m_buffer.indexOf('\n');
        QByteArray line = m_buffer.left(newlinePos);
        m_buffer.remove(0, newlinePos + 1);
        
        if (line.trimmed().isEmpty()) continue;
        
        // Parse JSON response
        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isNull()) {
            qWarning() << "[OllamaProxy] Failed to parse JSON:" << line;
            continue;
        }
        
        QJsonObject obj = doc.object();
        
        // Check for errors
        if (obj.contains("error")) {
            QString errMsg = obj["error"].toString();
            qWarning() << "[OllamaProxy] Server error:" << errMsg;
            emit error(errMsg);
            continue;
        }
        
        // Extract token from response
        if (obj.contains("response")) {
            QString token = obj["response"].toString();
            if (!token.isEmpty()) {
                emit tokenArrived(token);
            }
        }
        
        // Check if done
        if (obj["done"].toBool()) {
            qDebug() << "[OllamaProxy] Stream complete";
            break;
        }
    }
}

void OllamaProxy::onNetworkError(QNetworkReply::NetworkError code)
{
    QString errorMsg = QString("Network error: %1").arg(code);
    if (m_currentReply) {
        errorMsg += " - " + m_currentReply->errorString();
    }
    
    qWarning() << "[OllamaProxy]" << errorMsg;
    emit error(errorMsg);
}
