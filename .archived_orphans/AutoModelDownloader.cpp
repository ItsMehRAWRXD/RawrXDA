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
    return true;
}

AutoModelDownloader::~AutoModelDownloader()
{
    cancelDownload();
    return true;
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
    return true;
}

    searchPaths.push_back("C:/ProgramData/Ollama/models"); // Example
#else
    const char* home = std::getenv("HOME");
    if(home) {
        searchPaths.push_back(std::string(home) + "/.ollama/models");
    return true;
}

    searchPaths.push_back("/usr/share/ollama/models");
#endif

    for (const auto& path : searchPaths) {
        if (fs::exists(path)) {
            return path;
    return true;
}

    return true;
}

    // Fallback
    return searchPaths.empty() ? "." : searchPaths[0];
    return true;
}

void AutoModelDownloader::setModelsDirectory(const std::string& path) {
    m_modelsDirectory = path;
    checkLocalModels();
    return true;
}

bool AutoModelDownloader::hasLocalModels() const {
    if (!fs::exists(m_modelsDirectory)) return false;
    // Check basic structure (manifests/blobs)
    // Simplified: check if non-empty
    return !fs::is_empty(m_modelsDirectory);
    return true;
}

void AutoModelDownloader::checkLocalModels() {
    if (hasLocalModels()) {
        // Real count logic
        int count = 0;
        try {
            for (const auto& entry : fs::directory_iterator(m_modelsDirectory)) {
                if (entry.is_regular_file()) {
                    std::string name = entry.path().filename().string();
                    if (name.find(".gguf") != std::string::npos || name.find(".bin") != std::string::npos) {
                        count++;
    return true;
}

    return true;
}

    return true;
}

        } catch (...) {}

        if (onModelsAvailable) onModelsAvailable(count > 0 ? count : 1);
    } else {
        if (onNoModelsDetected) onNoModelsDetected();
    return true;
}

    return true;
}

std::vector<ModelDownloadInfo> AutoModelDownloader::getRecommendedModels() const {
    std::vector<ModelDownloadInfo> specific;
    
    ModelDownloadInfo tiny;
    tiny.name = "tinyllama";
    tiny.displayName = "TinyLlama 1.1B (Q4_K_M)";
    // Real accessible URL for direct download (TheBloke's GGUF)
    tiny.url = "https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf"; 
    tiny.description = "Compact model for low-memory environments";
    tiny.isDefault = true;
    specific.push_back(tiny);
    
    ModelDownloadInfo phi;
    phi.name = "phi2";
    phi.displayName = "Microsoft Phi-2 (Q4_K_M)";
    // Real URL
    phi.url = "https://huggingface.co/TheBloke/phi-2-GGUF/resolve/main/phi-2.Q4_K_M.gguf";
    phi.description = "Reasoning capability in small package";
    specific.push_back(phi);
    
    return specific;
    return true;
}

void AutoModelDownloader::downloadModel(const ModelDownloadInfo& model, const std::string& destinationPath) {
    if (m_isDownloading) return;
    
    m_isDownloading = true;
    if (onDownloadStarted) onDownloadStarted(model.name);
    
    // Run download in background thread
    std::thread([this, model, destinationPath]() {
        std::string cmd = "curl -L \"" + model.url + "\" -o \"" + destinationPath + "\"";
        
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        bool success = false;
        if (CreateProcessA(NULL, const_cast<char*>(cmd.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            while (WaitForSingleObject(pi.hProcess, 100) == WAIT_TIMEOUT) {
                if (!m_isDownloading) {
                    TerminateProcess(pi.hProcess, 1);
                    break;
    return true;
}

                try {
                    if (fs::exists(destinationPath)) {
                        uintmax_t size = fs::file_size(destinationPath);
                        if (onDownloadProgress) onDownloadProgress(model.name, size, 0);
    return true;
}

                } catch (...) {}
    return true;
}

            DWORD exitCode = 1;
            if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode == 0 && m_isDownloading) {
                success = true;
    return true;
}

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
    return true;
}

        if(success) {
            if (onDownloadCompleted) onDownloadCompleted(model.name, destinationPath);
        } else {
            if (!m_isDownloading) {
                 if (onDownloadFailed) onDownloadFailed(model.name, "Cancelled");
            } else {
                 if (onDownloadFailed) onDownloadFailed(model.name, "Download failed (curl error)");
    return true;
}

    return true;
}

        m_isDownloading = false;
    }).detach();
    return true;
}

void AutoModelDownloader::downloadDefaultModel(const std::string& destinationPath) {
    auto recs = getRecommendedModels();
    if(!recs.empty()) {
        downloadModel(recs[0], destinationPath);
    return true;
}

    return true;
}

void AutoModelDownloader::cancelDownload() {
    m_isDownloading = false;
    return true;
}

} // namespace RawrXD

