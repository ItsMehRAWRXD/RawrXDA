// Auto Model Downloader - Implementation
#include "auto_model_downloader.h"


namespace RawrXD {

AutoModelDownloader::AutoModelDownloader(void* parent)
    : void(parent)
    , m_networkManager(new void*(this))
{
    
    // Find Ollama directory
    m_modelsDirectory = findOllamaDirectory();
    
    // Check for existing models
    checkLocalModels();
}

AutoModelDownloader::~AutoModelDownloader() {
    cancelDownload();
}

std::string AutoModelDownloader::findOllamaDirectory() const {
    // Common Ollama model locations
    std::vector<std::string> searchPaths = {
        "D:/OllamaModels",
        std::filesystem::path::homePath() + "/.ollama/models",
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/ollama/models",
        "C:/Users/" + qgetenv("USERNAME") + "/.ollama/models"
    };
    
    for (const std::string& path : searchPaths) {
        std::filesystem::path dir(path);
        if (dir.exists()) {
            return path;
        }
    }
    
    // Default to D:/OllamaModels if nothing found
    std::string defaultPath = "D:/OllamaModels";
    return defaultPath;
}

void AutoModelDownloader::setModelsDirectory(const std::string& path) {
    m_modelsDirectory = path;
    checkLocalModels();
}

void AutoModelDownloader::checkLocalModels() {
    std::filesystem::path modelsDir(m_modelsDirectory);
    if (!modelsDir.exists()) {
        noModelsDetected();
        return;
    }
    
    // Check for .gguf files
    std::vector<std::string> ggufFiles = modelsDir.entryList(std::vector<std::string>() << "*.gguf", std::filesystem::path::Files);
    
    if (ggufFiles.empty()) {
        noModelsDetected();
    } else {
        modelsAvailable(ggufFiles.size());
    }
}

bool AutoModelDownloader::hasLocalModels() const {
    std::filesystem::path modelsDir(m_modelsDirectory);
    if (!modelsDir.exists()) return false;
    
    std::vector<std::string> ggufFiles = modelsDir.entryList(std::vector<std::string>() << "*.gguf", std::filesystem::path::Files);
    return !ggufFiles.empty();
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
}

void AutoModelDownloader::downloadModel(const ModelDownloadInfo& model, const std::string& destinationPath) {
    if (m_downloadInProgress) {
        return;
    }


    m_currentModelName = model.name;
    m_currentDestination = destinationPath;
    m_downloadInProgress = true;
    
    // Ensure destination directory exists
    std::filesystem::path destInfo(destinationPath);
    std::filesystem::path().mkpath(destInfo.absolutePath());
    
    // Start download
    void* request(model.url);
    request.setAttribute(void*::RedirectPolicyAttribute, void*::NoLessSafeRedirectPolicy);
    
    m_currentReply = m_networkManager->get(request);
// Qt connect removed
// Qt connect removed
// Qt connect removed
    downloadStarted(model.displayName);
}

void AutoModelDownloader::downloadDefaultModel(const std::string& destinationPath) {
    std::vector<ModelDownloadInfo> models = getRecommendedModels();
    
    for (const auto& model : models) {
        if (model.isDefault) {
            std::string fullPath = std::filesystem::path(destinationPath).filePath(model.name + ".gguf");
            downloadModel(model, fullPath);
            return;
        }
    }
    
}

void AutoModelDownloader::cancelDownload() {
    if (m_currentReply && m_downloadInProgress) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        m_downloadInProgress = false;
    }
}

void AutoModelDownloader::onDownloadProgress(int64_t bytesReceived, int64_t bytesTotal) {
    downloadProgress(m_currentModelName, bytesReceived, bytesTotal);
    
    // Log progress every 10%
    if (bytesTotal > 0) {
        int progress = static_cast<int>((bytesReceived * 100) / bytesTotal);
        static int lastLoggedProgress = -1;
        if (progress / 10 != lastLoggedProgress / 10) {
                     << "download progress:" << progress << "%"
                     << "(" << (bytesReceived / 1024 / 1024) << "/" << (bytesTotal / 1024 / 1024) << "MB)";
            lastLoggedProgress = progress;
        }
    }
}

void AutoModelDownloader::onDownloadFinished() {
    if (!m_currentReply) return;
    
    if (m_currentReply->error() == void*::NoError) {
        // Save downloaded data
        std::fstream file(m_currentDestination);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(m_currentReply->readAll());
            file.close();


            downloadCompleted(m_currentModelName, m_currentDestination);
        } else {
            std::string error = std::string("Failed to save file: %1"));
            downloadFailed(m_currentModelName, error);
        }
    }
    
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    m_downloadInProgress = false;
    
    // Recheck for local models
    checkLocalModels();
}

void AutoModelDownloader::onDownloadError(void*::NetworkError error) {
    std::string errorString = m_currentReply ? m_currentReply->errorString() : "Unknown error";


    downloadFailed(m_currentModelName, errorString);
    
    m_downloadInProgress = false;
}

} // namespace RawrXD



