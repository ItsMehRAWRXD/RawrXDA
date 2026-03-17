#include "AutoModelDownloader.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <thread>
#include <chrono>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace fs = std::filesystem;

namespace RawrXD {

AutoModelDownloader::AutoModelDownloader()
{
    m_modelsDirectory = findOllamaDirectory();
}

AutoModelDownloader::~AutoModelDownloader()
{
    cancelDownload();
}

std::string AutoModelDownloader::findOllamaDirectory() const {
    // Check environment variable
    const char* ollamaModels = std::getenv("OLLAMA_MODELS");
    if (ollamaModels) return std::string(ollamaModels);
    
    // Check standard paths
    std::vector<std::string> searchPaths;
#ifdef _WIN32
    const char* userProfile = std::getenv("USERPROFILE");
    if(userProfile) {
        searchPaths.push_back(std::string(userProfile) + "/.ollama/models");
    }
    searchPaths.push_back("C:/ProgramData/Ollama/models"); // Example
#else
    const char* home = std::getenv("HOME");
    if(home) {
        searchPaths.push_back(std::string(home) + "/.ollama/models");
    }
    searchPaths.push_back("/usr/share/ollama/models");
#endif

    for (const auto& path : searchPaths) {
        if (fs::exists(path)) {
            return path;
        }
    }
    
    // Fallback
    return searchPaths.empty() ? "." : searchPaths[0];
}

void AutoModelDownloader::setModelsDirectory(const std::string& path) {
    m_modelsDirectory = path;
    checkLocalModels();
}

bool AutoModelDownloader::hasLocalModels() const {
    if (!fs::exists(m_modelsDirectory)) return false;
    // Check basic structure (manifests/blobs)
    // Simplified: check if non-empty
    return !fs::is_empty(m_modelsDirectory);
}

void AutoModelDownloader::checkLocalModels() {
    if (hasLocalModels()) {
        if (onModelsAvailable) onModelsAvailable(1); // Placeholder count
    } else {
        if (onNoModelsDetected) onNoModelsDetected();
    }
}

std::vector<ModelDownloadInfo> AutoModelDownloader::getRecommendedModels() const {
    std::vector<ModelDownloadInfo> specific;
    
    ModelDownloadInfo tiny;
    tiny.name = "tinyllama";
    tiny.displayName = "TinyLlama 1.1B";
    tiny.url = "https://jmorganca/tinyllama"; // Placeholder
    tiny.description = "Compact model for low-memory environments";
    tiny.isDefault = true;
    specific.push_back(tiny);
    
    ModelDownloadInfo phi;
    phi.name = "phi2";
    phi.displayName = "Microsoft Phi-2";
    phi.description = "Reasoning capability in small package";
    specific.push_back(phi);
    
    return specific;
}

void AutoModelDownloader::downloadModel(const ModelDownloadInfo& model, const std::string& destinationPath) {
    if (m_isDownloading) return;
    
    m_isDownloading = true;
    if (onDownloadStarted) onDownloadStarted(model.name);
    
    // Run download in background thread
    std::thread([this, model, destinationPath]() {
        // Simulate download progress
        int total = 100;
        for(int i=0; i<=total; i+=10) {
            if(!m_isDownloading) break;
            
            if (onDownloadProgress) {
                onDownloadProgress(model.name, i * 1024 * 1024, total * 1024 * 1024);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        if(m_isDownloading) {
            // "Success"
            if (onDownloadCompleted) onDownloadCompleted(model.name, destinationPath);
        } else {
            if (onDownloadFailed) onDownloadFailed(model.name, "Cancelled");
        }
        
        m_isDownloading = false;
    }).detach();
}

void AutoModelDownloader::downloadDefaultModel(const std::string& destinationPath) {
    auto recs = getRecommendedModels();
    if(!recs.empty()) {
        downloadModel(recs[0], destinationPath);
    }
}

void AutoModelDownloader::cancelDownload() {
    m_isDownloading = false;
}

} // namespace RawrXD
