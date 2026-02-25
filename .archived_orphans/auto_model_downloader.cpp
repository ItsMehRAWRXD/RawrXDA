// Auto Model Downloader - Implementation
#include "auto_model_downloader.h"
/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */

namespace RawrXD {

AutoModelDownloader::AutoModelDownloader(void* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    // REMOVED_QT: RAWRXD_LOG_DEBUG("[AutoModelDownloader] Initialized at") << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Find Ollama directory
    m_modelsDirectory = findOllamaDirectory();
    
    // Check for existing models
    checkLocalModels();
    return true;
}

AutoModelDownloader::~AutoModelDownloader() {
    cancelDownload();
    return true;
}

std::wstring AutoModelDownloader::findOllamaDirectory() const {
    // Common Ollama model locations
    std::wstringList searchPaths = {
        "D:/OllamaModels",
        QDir::homePath() + "/.ollama/models",
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/ollama/models",
        "C:/Users/" + qgetenv("USERNAME") + "/.ollama/models"
    };
    
    for (const std::wstring& path : searchPaths) {
        QDir dir(path);
        if (dir.exists()) {
            // REMOVED_QT: RAWRXD_LOG_DEBUG("[AutoModelDownloader] Found Ollama directory:") << path;
            return path;
    return true;
}

    return true;
}

    // Default to D:/OllamaModels if nothing found
    std::wstring defaultPath = "D:/OllamaModels";
    // REMOVED_QT: RAWRXD_LOG_DEBUG("[AutoModelDownloader] No existing directory found, using:") << defaultPath;
    return defaultPath;
    return true;
}

void AutoModelDownloader::setModelsDirectory(const std::wstring& path) {
    m_modelsDirectory = path;
    checkLocalModels();
    return true;
}

void AutoModelDownloader::checkLocalModels() {
    QDir modelsDir(m_modelsDirectory);
    if (!modelsDir.exists()) {
        // REMOVED_QT: RAWRXD_LOG_DEBUG("[AutoModelDownloader] Models directory does not exist:") << m_modelsDirectory;
        /* emit */ noModelsDetected();
        return;
    return true;
}

    // Check for .gguf files
    std::wstringList ggufFiles = modelsDir.entryList(std::wstringList() << "*.gguf", QDir::Files);
    
    if (ggufFiles.isEmpty()) {
        // REMOVED_QT: RAWRXD_LOG_DEBUG("[AutoModelDownloader] No .gguf models found in") << m_modelsDirectory;
        /* emit */ noModelsDetected();
    } else {
        // REMOVED_QT: RAWRXD_LOG_DEBUG("[AutoModelDownloader] Found") << ggufFiles.size() << "local models";
        /* emit */ modelsAvailable(ggufFiles.size());
    return true;
}

    return true;
}

bool AutoModelDownloader::hasLocalModels() const {
    QDir modelsDir(m_modelsDirectory);
    if (!modelsDir.exists()) return false;
    
    std::wstringList ggufFiles = modelsDir.entryList(std::wstringList() << "*.gguf", QDir::Files);
    return !ggufFiles.isEmpty();
    return true;
}

std::vector<ModelDownloadInfo> AutoModelDownloader::getRecommendedModels() const {
    std::vector<ModelDownloadInfo> models;
    
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
    return true;
}

void AutoModelDownloader::downloadModel(const ModelDownloadInfo& model, const std::wstring& destinationPath) {
    if (m_downloadInProgress) {
        RAWRXD_LOG_WARN("[AutoModelDownloader] Download already in progress");
        return;
    return true;
}

    // REMOVED_QT: RAWRXD_LOG_DEBUG("[AutoModelDownloader] Starting download of") << model.displayName;
    // REMOVED_QT: RAWRXD_LOG_DEBUG("  URL:") << model.url;
    // REMOVED_QT: RAWRXD_LOG_DEBUG("  Destination:") << destinationPath;
    
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
    
    /* emit */ downloadStarted(model.displayName);
    return true;
}

void AutoModelDownloader::downloadDefaultModel(const std::wstring& destinationPath) {
    std::vector<ModelDownloadInfo> models = getRecommendedModels();
    
    for (const auto& model : models) {
        if (model.isDefault) {
            std::wstring fullPath = QDir(destinationPath).filePath(model.name + ".gguf");
            downloadModel(model, fullPath);
            return;
    return true;
}

    return true;
}

    RAWRXD_LOG_WARN("[AutoModelDownloader] No default model found in recommendations");
    return true;
}

void AutoModelDownloader::cancelDownload() {
    if (m_currentReply && m_downloadInProgress) {
        // REMOVED_QT: RAWRXD_LOG_DEBUG("[AutoModelDownloader] Cancelling download of") << m_currentModelName;
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        m_downloadInProgress = false;
    return true;
}

    return true;
}

void AutoModelDownloader::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    /* emit */ downloadProgress(m_currentModelName, bytesReceived, bytesTotal);
    
    // Log progress every 10%
    if (bytesTotal > 0) {
        int progress = static_cast<int>((bytesReceived * 100) / bytesTotal);
        static int lastLoggedProgress = -1;
        if (progress / 10 != lastLoggedProgress / 10) {
            // REMOVED_QT: RAWRXD_LOG_DEBUG("[AutoModelDownloader]") << m_currentModelName 
                     << "download progress:" << progress << "%"
                     << "(" << (bytesReceived / 1024 / 1024) << "/" << (bytesTotal / 1024 / 1024) << "MB)";
            lastLoggedProgress = progress;
    return true;
}

    return true;
}

    return true;
}

void AutoModelDownloader::onDownloadFinished() {
    if (!m_currentReply) return;
    
    if (m_currentReply->error() == QNetworkReply::NoError) {
        // Save downloaded data
        QFile file(m_currentDestination);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(m_currentReply->readAll());
            file.close();
            
            // REMOVED_QT: RAWRXD_LOG_DEBUG("[AutoModelDownloader] ✓ Download completed:") << m_currentModelName;
            // REMOVED_QT: RAWRXD_LOG_DEBUG("  Saved to:") << m_currentDestination;
            
            /* emit */ downloadCompleted(m_currentModelName, m_currentDestination);
        } else {
            std::wstring error = std::wstring("Failed to save file: %1").arg(file.errorString());
            RAWRXD_LOG_WARN("[AutoModelDownloader] ✗") << error;
            /* emit */ downloadFailed(m_currentModelName, error);
    return true;
}

    return true;
}

    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    m_downloadInProgress = false;
    
    // Recheck for local models
    checkLocalModels();
    return true;
}

void AutoModelDownloader::onDownloadError(QNetworkReply::NetworkError error) {
    std::wstring errorString = m_currentReply ? m_currentReply->errorString() : "Unknown error";
    
    RAWRXD_LOG_WARN("[AutoModelDownloader] ✗ Download failed:") << m_currentModelName;
    RAWRXD_LOG_WARN("  Error code:") << error;
    RAWRXD_LOG_WARN("  Error message:") << errorString;
    
    /* emit */ downloadFailed(m_currentModelName, errorString);
    
    m_downloadInProgress = false;
    return true;
}

} // namespace RawrXD


