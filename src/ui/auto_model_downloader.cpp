// Auto Model Downloader - Implementation
#include "auto_model_downloader.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>
#include <QDateTime>
#include <QProcess>
#include <QEventLoop>

namespace RawrXD {

AutoModelDownloader::AutoModelDownloader(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    qDebug() << "[AutoModelDownloader] Initialized at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Find Ollama directory
    m_modelsDirectory = findOllamaDirectory();
    
    // Check for existing models
    checkLocalModels();
}

AutoModelDownloader::~AutoModelDownloader() {
    cancelDownload();
}

QString AutoModelDownloader::findOllamaDirectory() const {
    // Common Ollama model locations
    QStringList searchPaths = {
        "D:/OllamaModels",
        QDir::homePath() + "/.ollama/models",
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/ollama/models",
        "C:/Users/" + qgetenv("USERNAME") + "/.ollama/models"
    };
    
    for (const QString& path : searchPaths) {
        QDir dir(path);
        if (dir.exists()) {
            qDebug() << "[AutoModelDownloader] Found Ollama directory:" << path;
            return path;
        }
    }
    
    // Default to D:/OllamaModels if nothing found
    QString defaultPath = "D:/OllamaModels";
    qDebug() << "[AutoModelDownloader] No existing directory found, using:" << defaultPath;
    return defaultPath;
}

void AutoModelDownloader::setModelsDirectory(const QString& path) {
    m_modelsDirectory = path;
    checkLocalModels();
}

void AutoModelDownloader::checkLocalModels() {
    QDir modelsDir(m_modelsDirectory);
    if (!modelsDir.exists()) {
        qDebug() << "[AutoModelDownloader] Models directory does not exist:" << m_modelsDirectory;
        emit noModelsDetected();
        return;
    }
    
    // Check for .gguf files
    QStringList ggufFiles = modelsDir.entryList(QStringList() << "*.gguf", QDir::Files);
    
    if (ggufFiles.isEmpty()) {
        qDebug() << "[AutoModelDownloader] No .gguf models found in" << m_modelsDirectory;
        emit noModelsDetected();
    } else {
        qDebug() << "[AutoModelDownloader] Found" << ggufFiles.size() << "local models";
        emit modelsAvailable(ggufFiles.size());
    }
}

bool AutoModelDownloader::hasLocalModels() const {
    QDir modelsDir(m_modelsDirectory);
    if (!modelsDir.exists()) return false;
    
    QStringList ggufFiles = modelsDir.entryList(QStringList() << "*.gguf", QDir::Files);
    return !ggufFiles.isEmpty();
}

QVector<ModelDownloadInfo> AutoModelDownloader::getRecommendedModels() const {
    QVector<ModelDownloadInfo> models;
    
    // TinyLlama 1.1B - Excellent for testing and low-resource environments
    ModelDownloadInfo tinyLlama;
    tinyLlama.name = "tinyllama-1.1b";
    tinyLlama.displayName = "TinyLlama 1.1B (Recommended)";
    tinyLlama.url = "https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf";
    tinyLlama.sizeBytes = 669000000;  // ~669 MB
    tinyLlama.description = "Ultra-fast, low memory (~1GB VRAM). Perfect for code completion and chat.";
    tinyLlama.isDefault = true;
    models.append(tinyLlama);
    
    // Phi-2 2.7B - Better quality, still small
    ModelDownloadInfo phi2;
    phi2.name = "phi-2-2.7b";
    phi2.displayName = "Microsoft Phi-2 2.7B";
    phi2.url = "https://huggingface.co/TheBloke/phi-2-GGUF/resolve/main/phi-2.Q4_K_M.gguf";
    phi2.sizeBytes = 1600000000;  // ~1.6 GB
    phi2.description = "High quality, optimized for reasoning. Needs ~3GB VRAM.";
    phi2.isDefault = false;
    models.append(phi2);
    
    // Gemma 2B - Google's efficient model
    ModelDownloadInfo gemma;
    gemma.name = "gemma-2b";
    gemma.displayName = "Google Gemma 2B";
    gemma.url = "https://huggingface.co/google/gemma-2b-it-GGUF/resolve/main/gemma-2b-it.Q4_K_M.gguf";
    gemma.sizeBytes = 1500000000;  // ~1.5 GB
    gemma.description = "Google's efficient instruction-tuned model. ~2.5GB VRAM.";
    gemma.isDefault = false;
    models.append(gemma);
    
    return models;
}

void AutoModelDownloader::downloadModel(const ModelDownloadInfo& model, const QString& destinationPath) {
    if (m_downloadInProgress) {
        qWarning() << "[AutoModelDownloader] Download already in progress";
        return;
    }
    
    qDebug() << "[AutoModelDownloader] Starting download of" << model.displayName;
    qDebug() << "  URL:" << model.url;
    qDebug() << "  Destination:" << destinationPath;
    
    m_currentModelName = model.name;
    m_currentDestination = destinationPath;
    m_downloadInProgress = true;
    
    // Ensure destination directory exists
    QFileInfo destInfo(destinationPath);
    QDir().mkpath(destInfo.absolutePath());
    
    // Start download
    QNetworkRequest request(model.url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    m_currentReply = m_networkManager->get(request);
    
    connect(m_currentReply, &QNetworkReply::downloadProgress,
            this, &AutoModelDownloader::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished,
            this, &AutoModelDownloader::onDownloadFinished);
    connect(m_currentReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &AutoModelDownloader::onDownloadError);
    
    emit downloadStarted(model.displayName);
}

void AutoModelDownloader::downloadDefaultModel(const QString& destinationPath) {
    QVector<ModelDownloadInfo> models = getRecommendedModels();
    
    for (const auto& model : models) {
        if (model.isDefault) {
            QString fullPath = QDir(destinationPath).filePath(model.name + ".gguf");
            downloadModel(model, fullPath);
            return;
        }
    }
    
    qWarning() << "[AutoModelDownloader] No default model found in recommendations";
}

void AutoModelDownloader::cancelDownload() {
    if (m_currentReply && m_downloadInProgress) {
        qDebug() << "[AutoModelDownloader] Cancelling download of" << m_currentModelName;
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        m_downloadInProgress = false;
    }
}

void AutoModelDownloader::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    emit downloadProgress(m_currentModelName, bytesReceived, bytesTotal);
    
    // Log progress every 10%
    if (bytesTotal > 0) {
        int progress = static_cast<int>((bytesReceived * 100) / bytesTotal);
        static int lastLoggedProgress = -1;
        if (progress / 10 != lastLoggedProgress / 10) {
            qDebug() << "[AutoModelDownloader]" << m_currentModelName 
                     << "download progress:" << progress << "%"
                     << "(" << (bytesReceived / 1024 / 1024) << "/" << (bytesTotal / 1024 / 1024) << "MB)";
            lastLoggedProgress = progress;
        }
    }
}

void AutoModelDownloader::onDownloadFinished() {
    if (!m_currentReply) return;
    
    if (m_currentReply->error() == QNetworkReply::NoError) {
        // Save downloaded data
        QFile file(m_currentDestination);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(m_currentReply->readAll());
            file.close();
            
            qDebug() << "[AutoModelDownloader] ✓ Download completed:" << m_currentModelName;
            qDebug() << "  Saved to:" << m_currentDestination;
            
            emit downloadCompleted(m_currentModelName, m_currentDestination);
        } else {
            QString error = QString("Failed to save file: %1").arg(file.errorString());
            qWarning() << "[AutoModelDownloader] ✗" << error;
            emit downloadFailed(m_currentModelName, error);
        }
    }
    
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    m_downloadInProgress = false;
    
    // Recheck for local models
    checkLocalModels();
}

void AutoModelDownloader::onDownloadError(QNetworkReply::NetworkError error) {
    QString errorString = m_currentReply ? m_currentReply->errorString() : "Unknown error";
    
    qWarning() << "[AutoModelDownloader] ✗ Download failed:" << m_currentModelName;
    qWarning() << "  Error code:" << error;
    qWarning() << "  Error message:" << errorString;
    
    emit downloadFailed(m_currentModelName, errorString);
    
    m_downloadInProgress = false;
}

} // namespace RawrXD
