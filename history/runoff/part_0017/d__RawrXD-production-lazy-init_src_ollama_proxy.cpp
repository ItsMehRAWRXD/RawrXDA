// OllamaProxy - Fallback to Ollama REST API for unsupported models
#include "../include/ollama_proxy.h"
#include <QNetworkRequest>
#include <QJsonArray>
#include <QDebug>
#include <QUrlQuery>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QEventLoop>

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

void OllamaProxy::detectBlobs(const QString& modelDir)
{
    qInfo() << "[OllamaProxy] Scanning for blobs in:" << modelDir;
    m_detectedModels.clear();
    m_blobToModel.clear();

    QDir dir(modelDir);
    if (!dir.exists()) {
        qWarning() << "[OllamaProxy] Model directory does not exist:" << modelDir;
        return;
    }

    // 1. Scan manifests to map model names to blobs
    QString manifestsPath = dir.absoluteFilePath("manifests");
    if (QFile::exists(manifestsPath)) {
        QDirIterator it(manifestsPath, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString filePath = it.next();
            QFile file(filePath);
            if (file.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
                QJsonObject obj = doc.object();
                QJsonArray layers = obj["layers"].toArray();
                
                for (const QJsonValue& layer : layers) {
                    QJsonObject layerObj = layer.toObject();
                    if (layerObj["mediaType"].toString() == "application/vnd.ollama.image.model") {
                        QString digest = layerObj["digest"].toString();
                        // digest is usually sha256:hash
                        QString hash = digest.startsWith("sha256:") ? digest.mid(7) : digest;
                        
                        // Relative path from manifests/ to model name
                        QString modelName = QDir(manifestsPath).relativeFilePath(filePath);
                        // Replace backslashes with forward slashes and handle registry prefixes
                        modelName.replace("\\", "/");
                        if (modelName.startsWith("registry.ollama.ai/library/")) {
                            modelName.remove(0, 27);
                        } else if (modelName.startsWith("registry.ollama.ai/")) {
                            modelName.remove(0, 19);
                        }

                        QString blobPath = dir.absoluteFilePath("blobs/sha256-" + hash);
                        if (!QFile::exists(blobPath)) {
                            // Try without sha256- prefix
                            blobPath = dir.absoluteFilePath("blobs/" + hash);
                        }

                        if (QFile::exists(blobPath)) {
                            m_detectedModels[modelName] = blobPath;
                            m_blobToModel[blobPath] = modelName;
                            qDebug() << "[OllamaProxy] Detected model:" << modelName << "->" << blobPath;
                        }
                    }
                }
            }
        }
    }

    // 2. Scan blobs directory for orphaned blobs (optional, but good for "detect blob files")
    QString blobsPath = dir.absoluteFilePath("blobs");
    if (QFile::exists(blobsPath)) {
        QDirIterator it(blobsPath, QDir::Files);
        while (it.hasNext()) {
            QString filePath = it.next();
            if (!m_blobToModel.contains(filePath)) {
                QFileInfo info(filePath);
                if (info.size() > 100 * 1024 * 1024) { // > 100MB likely a model
                    QString hash = info.fileName();
                    if (hash.startsWith("sha256-")) hash.remove(0, 7);
                    
                    QString pseudoName = "blob-" + hash.left(8);
                    m_detectedModels[pseudoName] = filePath;
                    m_blobToModel[filePath] = pseudoName;
                    qDebug() << "[OllamaProxy] Detected orphaned blob:" << pseudoName << "->" << filePath;
                }
            }
        }
    }

    qInfo() << "[OllamaProxy] Detection complete." << m_detectedModels.size() << "models found.";
}

bool OllamaProxy::isBlobPath(const QString& path) const
{
    return m_blobToModel.contains(path) || path.contains("/blobs/sha256-") || path.contains("\\blobs\\sha256-");
}

QString OllamaProxy::resolveBlobToModel(const QString& blobPath) const
{
    if (m_blobToModel.contains(blobPath)) {
        return m_blobToModel[blobPath];
    }
    
    // Fallback: extract hash from path
    QFileInfo info(blobPath);
    QString name = info.fileName();
    if (name.startsWith("sha256-")) {
        return "blob-" + name.mid(7, 8);
    }
    return name;
}

QString OllamaProxy::blobPathForModel(const QString& modelName) const
{
    if (m_detectedModels.contains(modelName)) {
        return m_detectedModels.value(modelName);
    }
    return QString();
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

QString OllamaProxy::generateResponseSync(const QString& prompt, float temperature, int maxTokens)
{
    if (m_modelName.isEmpty()) {
        return "Error: No model set in OllamaProxy";
    }

    QJsonObject root;
    root["model"] = m_modelName;
    root["prompt"] = prompt;
    root["stream"] = false; // Non-streaming for easier sync handling
    
    QJsonObject options;
    options["temperature"] = temperature;
    options["num_predict"] = maxTokens;
    root["options"] = options;

    QNetworkRequest request(QUrl(m_ollamaUrl + "/api/generate"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_networkManager->post(request, QJsonDocument(root).toJson());
    
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    // 30 second timeout for complex agentic tasks
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        return "Error: Ollama request timed out";
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        reply->deleteLater();
        return "Error: " + err;
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    return doc.object()["response"].toString();
}

#include "moc_ollama_proxy.cpp"
