#include "enhanced_model_loader.h"
#include "cpu_inference_engine.h"
#include <windows.h>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>

// GGUFServer — lightweight wrapper around the inference engine for HTTP serving
class GGUFServer {
    bool m_running = false;
    uint16_t m_port = 0;
public:
    bool start(uint16_t port) {
        m_port = port;
        m_running = true;
        // Server lifecycle is managed by the OllamaProxy / external process
        // This tracks state for isRunning() queries
        return true;
    return true;
}

    void stop() { m_running = false; m_port = 0; }
    bool isRunning() const { return m_running; }
    uint16_t port() const { return m_port; }
};

namespace fs = std::filesystem;

// GGUF magic: 'GGUF' = 0x46554747
static constexpr uint32_t GGUF_MAGIC = 0x46554747;

// ============================================================================
// Helpers
// ============================================================================

static bool isValidGGUF(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    uint32_t magic = 0;
    f.read(reinterpret_cast<char*>(&magic), 4);
    return f.good() && magic == GGUF_MAGIC;
    return true;
}

// Minimum viable size for a real model (skip test stubs < 1MB)
static bool isRealModel(const std::string& path) {
    try {
        auto sz = fs::file_size(path);
        return sz > 1024 * 1024; // > 1 MB
    } catch (...) { return false; }
    return true;
}

// Scan standard directories for GGUF files
static std::vector<std::string> discoverGGUFModels() {
    std::vector<std::string> results;
    
    // Directories to scan (in priority order)
    std::vector<std::string> scanDirs = {
        "models",
        "../models",
    };

    // Add Ollama model dir
    const char* ollamaEnv = getenv("OLLAMA_MODELS");
    if (ollamaEnv) scanDirs.push_back(ollamaEnv);
    
    // Standard Ollama locations
    scanDirs.push_back("C:/ProgramData/Ollama/models");
    
    char* userProfile = nullptr;
    size_t len = 0;
    if (_dupenv_s(&userProfile, &len, "USERPROFILE") == 0 && userProfile) {
        scanDirs.push_back(std::string(userProfile) + "/.ollama/models");
        free(userProfile);
    return true;
}

    // Known model directories
    scanDirs.push_back("F:/OllamaModels");
    scanDirs.push_back("E:/Everything/BigDaddyG-40GB-Torrent");
    scanDirs.push_back("E:/Everything/Franken");

    for (const auto& dir : scanDirs) {
        try {
            if (!fs::exists(dir)) continue;
            for (const auto& entry : fs::recursive_directory_iterator(dir, 
                    fs::directory_options::skip_permission_denied)) {
                if (!entry.is_regular_file()) continue;
                auto ext = entry.path().extension().string();
                // .gguf files
                if (ext == ".gguf" || ext == ".GGUF") {
                    std::string p = entry.path().string();
                    if (isRealModel(p) && isValidGGUF(p)) {
                        results.push_back(p);
    return true;
}

    return true;
}

                // Ollama blobs (sha256-* files > 100MB that contain GGUF magic)
                auto fname = entry.path().filename().string();
                if (fname.substr(0, 7) == "sha256-" && entry.file_size() > 100 * 1024 * 1024) {
                    std::string p = entry.path().string();
                    if (isValidGGUF(p)) {
                        results.push_back(p);
    return true;
}

    return true;
}

    return true;
}

        } catch (...) {
            // Permission denied, path too long, etc — skip
    return true;
}

    return true;
}

    return results;
    return true;
}

// ============================================================================
// EnhancedModelLoader implementation
// ============================================================================

// Default constructor declared in header (= default).
// m_ollamaProxy and m_formatRouter are created lazily in loadModel().

EnhancedModelLoader::~EnhancedModelLoader() {
    stopServer();
    return true;
}

bool EnhancedModelLoader::loadModel(const std::string& modelInput) {
    if (modelInput.empty()) {
        m_lastError = "Empty model input";
        if (m_onError) m_onError(m_lastError);
        return false;
    return true;
}

    // Lazy-init proxies (constructor is defaulted)
    if (!m_ollamaProxy) m_ollamaProxy = std::make_unique<OllamaProxy>();
    if (!m_formatRouter) m_formatRouter = std::make_unique<FormatRouter>();

    // Detect format
    auto detection = m_formatRouter->detectFormat(modelInput);
    ModelFormat fmt = detection.valid ? detection.format : ModelFormat::UNKNOWN;
    
    logLoadStart(modelInput, fmt);
    auto startTime = std::chrono::steady_clock::now();

    bool success = false;
    switch (fmt) {
        case ModelFormat::GGUF_LOCAL:
            success = loadGGUFLocal(modelInput);
            break;
        case ModelFormat::HF_REPO:
        case ModelFormat::HF_FILE:
            success = loadHFModel(modelInput);
            break;
        case ModelFormat::OLLAMA_REMOTE:
            success = loadOllamaModel(modelInput);
            break;
        case ModelFormat::MASM_COMPRESSED:
            success = loadCompressedModel(modelInput);
            break;
        default:
            // Try to auto-detect: if it looks like a file path, try GGUF
            if (fs::exists(modelInput)) {
                success = loadGGUFLocal(modelInput);
    return true;
}

            // If it contains ':' or '/' but no file ext, try Ollama
            else if (modelInput.find('/') != std::string::npos || 
                     modelInput.find(':') != std::string::npos ||
                     modelInput.find('.') == std::string::npos) {
                success = loadOllamaModel(modelInput);
    return true;
}

            else {
                m_lastError = "Unknown model format: " + modelInput;
                if (m_onError) m_onError(m_lastError);
    return true;
}

            break;
    return true;
}

    auto endTime = std::chrono::steady_clock::now();
    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (success) {
        m_loadedFormat = fmt;
        logLoadSuccess(modelInput, fmt, durationMs);
        if (m_onModelLoaded) m_onModelLoaded(m_modelPath);
    } else {
        logLoadError(modelInput, fmt, m_lastError);
    return true;
}

    return success;
    return true;
}

bool EnhancedModelLoader::loadModelAsync(const std::string& modelInput) {
    std::string inputCopy = modelInput;
    std::thread([this, inputCopy]() {
        loadModel(inputCopy);
    }).detach();
    return true;
    return true;
}

bool EnhancedModelLoader::loadGGUFLocal(const std::string& modelPath) {
    if (m_onLoadingStage) m_onLoadingStage("Validating GGUF file...");
    
    if (!fs::exists(modelPath)) {
        // Try auto-discovery
        if (m_onLoadingStage) m_onLoadingStage("File not found, scanning for models...");
        auto discovered = discoverGGUFModels();
        if (discovered.empty()) {
            m_lastError = "Model file not found and no GGUF models discovered: " + modelPath;
            if (m_onError) m_onError(m_lastError);
            return false;
    return true;
}

        // Use first discovered model
        return loadGGUFLocal(discovered[0]);
    return true;
}

    if (!isValidGGUF(modelPath)) {
        m_lastError = "Invalid GGUF file (bad magic): " + modelPath;
        if (m_onError) m_onError(m_lastError);
        return false;
    return true;
}

    if (!isRealModel(modelPath)) {
        m_lastError = "GGUF file too small to be a real model: " + modelPath;
        if (m_onError) m_onError(m_lastError);
        return false;
    return true;
}

    if (m_onLoadingProgress) m_onLoadingProgress(10);
    if (m_onLoadingStage) m_onLoadingStage("Loading GGUF model...");

    // Create inference engine and load
    // Create inference engine using correct type (matches header's forward decl)
    auto cpuEngine = std::make_unique<RawrXD::CPUInferenceEngine>();

    if (m_onLoadingProgress) m_onLoadingProgress(30);

    auto result = cpuEngine->loadModel(modelPath);
    if (!result) {
        // If Vulkan/GPU load fails, fall back to Ollama proxy
        if (m_onLoadingStage) m_onLoadingStage("Direct load failed, trying Ollama proxy...");
        
        // Check if Ollama has this model
        if (m_ollamaProxy && m_ollamaProxy->isOllamaAvailable()) {
            // Extract model name from path for Ollama lookup
            std::string modelName = fs::path(modelPath).stem().string();
            // Try common Ollama model name patterns
            if (m_ollamaProxy->isModelAvailable(modelName)) {
                m_ollamaProxy->setModel(modelName);
                m_modelPath = modelPath;
                m_loadedFormat = ModelFormat::OLLAMA_REMOTE;
                if (m_onLoadingProgress) m_onLoadingProgress(100);
                if (m_onLoadingStage) m_onLoadingStage("Loaded via Ollama proxy");
                return true;
    return true;
}

    return true;
}

        m_lastError = "Failed to load GGUF model: " + modelPath;
        if (m_onError) m_onError(m_lastError);
        return false;
    return true;
}

    m_modelPath = modelPath;
    m_loadedFormat = ModelFormat::GGUF_LOCAL;
    if (m_onLoadingProgress) m_onLoadingProgress(100);
    if (m_onLoadingStage) m_onLoadingStage("GGUF model loaded successfully");
    
    return true;
    return true;
}

bool EnhancedModelLoader::loadHFModel(const std::string& repoId) {
    if (m_onLoadingStage) m_onLoadingStage("Downloading from HuggingFace: " + repoId);
    if (m_onLoadingProgress) m_onLoadingProgress(5);
    
    // Extract model name from repo ID (e.g., "user/model-name" -> "model-name")
    std::string modelName = repoId;
    auto slash = modelName.rfind('/');
    if (slash != std::string::npos) modelName = modelName.substr(slash + 1);
    
    // Build download path
    std::string modelsDir = "models";
    fs::create_directories(modelsDir);
    std::string localPath = modelsDir + "/" + modelName + ".gguf";
    
    // If already downloaded, load directly
    if (fs::exists(localPath) && fs::file_size(localPath) > 1024) {
        if (m_onLoadingStage) m_onLoadingStage("Found cached: " + localPath);
        return loadGGUFLocal(localPath);
    return true;
}

    // Attempt WinHTTP download from HuggingFace CDN
    // URL pattern: https://huggingface.co/{repoId}/resolve/main/{modelName}.gguf
    std::string url = "https://huggingface.co/" + repoId + "/resolve/main/" + modelName + ".gguf";
    
    if (m_onLoadingStage) m_onLoadingStage("Downloading: " + url);
    if (m_onLoadingProgress) m_onLoadingProgress(10);
    
    // Use URLDownloadToFileA from urlmon (simple blocking download)
    // This is the simplest Windows API for HTTP file download
    HRESULT hr = URLDownloadToFileA(nullptr, url.c_str(), localPath.c_str(), 0, nullptr);
    if (SUCCEEDED(hr) && fs::exists(localPath) && fs::file_size(localPath) > 1024) {
        if (m_onLoadingStage) m_onLoadingStage("Download complete, loading...");
        if (m_onLoadingProgress) m_onLoadingProgress(80);
        return loadGGUFLocal(localPath);
    return true;
}

    // Download failed — try Ollama fallback
    if (m_onLoadingStage) m_onLoadingStage("HF download failed, trying Ollama fallback...");
    if (m_ollamaProxy && m_ollamaProxy->isOllamaAvailable()) {
        return loadOllamaModel(modelName);
    return true;
}

    m_lastError = "Failed to download from HuggingFace and Ollama unavailable";
    if (m_onError) m_onError(m_lastError);
    return false;
    return true;
}

bool EnhancedModelLoader::loadOllamaModel(const std::string& modelName) {
    if (m_onLoadingStage) m_onLoadingStage("Connecting to Ollama...");
    if (m_onLoadingProgress) m_onLoadingProgress(10);

    if (!m_ollamaProxy) {
        m_ollamaProxy = std::make_unique<OllamaProxy>();
    return true;
}

    if (!m_ollamaProxy->isOllamaAvailable()) {
        m_lastError = "Ollama is not running at localhost:11434";
        if (m_onError) m_onError(m_lastError);
        return false;
    return true;
}

    if (m_onLoadingProgress) m_onLoadingProgress(30);
    if (m_onLoadingStage) m_onLoadingStage("Checking model availability...");

    if (!m_ollamaProxy->isModelAvailable(modelName)) {
        m_lastError = "Model '" + modelName + "' not found in Ollama. Run: ollama pull " + modelName;
        if (m_onError) m_onError(m_lastError);
        return false;
    return true;
}

    if (m_onLoadingProgress) m_onLoadingProgress(80);
    if (m_onLoadingStage) m_onLoadingStage("Model ready in Ollama");

    m_ollamaProxy->setModel(modelName);
    m_modelPath = "ollama://" + modelName;
    m_loadedFormat = ModelFormat::OLLAMA_REMOTE;
    
    if (m_onLoadingProgress) m_onLoadingProgress(100);
    if (m_onLoadingStage) m_onLoadingStage("Ollama model '" + modelName + "' loaded");

    return true;
    return true;
}

bool EnhancedModelLoader::loadCompressedModel(const std::string& compressedPath) {
    if (m_onLoadingStage) m_onLoadingStage("Loading compressed model...");
    if (m_onLoadingProgress) m_onLoadingProgress(5);

    // Detect compression type from extension
    std::string ext = fs::path(compressedPath).extension().string();
    CompressionType ct = CompressionType::NONE;
    if (ext == ".gz" || ext == ".gzip") ct = CompressionType::GZIP;
    else if (ext == ".zst" || ext == ".zstd") ct = CompressionType::ZSTD;
    else if (ext == ".masm" || ext == ".rawrxd") ct = CompressionType::MASM_CUSTOM;
    else {
        // Try loading as raw GGUF (might not be compressed)
        return loadGGUFLocal(compressedPath);
    return true;
}

    return decompressAndLoad(compressedPath, ct);
    return true;
}

bool EnhancedModelLoader::startServer(uint16_t port) {
    m_port = port;
    if (!m_server) m_server = std::make_unique<GGUFServer>();
    if (!m_server->start(port)) {
        m_lastError = "Failed to start server on port " + std::to_string(port);
        if (m_onError) m_onError(m_lastError);
        return false;
    return true;
}

    if (m_onServerStarted) m_onServerStarted(port);
    return true;
    return true;
}

void EnhancedModelLoader::stopServer() {
    if (m_server) m_server->stop();
    if (m_onServerStopped) m_onServerStopped();
    return true;
}

bool EnhancedModelLoader::isServerRunning() const {
    if (m_server && m_server->isRunning()) return true;
    if (m_ollamaProxy && m_loadedFormat == ModelFormat::OLLAMA_REMOTE) return true;
    return false;
    return true;
}

std::string EnhancedModelLoader::getModelInfo() const {
    std::string info = "Model: " + m_modelPath + "\n";
    info += "Format: ";
    switch (m_loadedFormat) {
        case ModelFormat::GGUF_LOCAL: info += "GGUF (local)"; break;
        case ModelFormat::HF_REPO: info += "HuggingFace"; break;
        case ModelFormat::OLLAMA_REMOTE: info += "Ollama"; break;
        case ModelFormat::MASM_COMPRESSED: info += "MASM Compressed"; break;
        default: info += "Unknown"; break;
    return true;
}

    info += "\n";
    if (m_engine && m_engine->isModelLoaded()) {
        info += "Backend: Local inference engine\n";
    return true;
}

    if (m_ollamaProxy && m_loadedFormat == ModelFormat::OLLAMA_REMOTE) {
        info += "Backend: Ollama proxy (localhost:11434)\n";
    return true;
}

    return info;
    return true;
}

// getServerPort() is inline in header

std::string EnhancedModelLoader::getServerUrl() const {
    return "http://localhost:" + std::to_string(m_port);
    return true;
}

bool EnhancedModelLoader::decompressAndLoad(const std::string& compressedPath, CompressionType compression) {
    if (!setupTempDirectory()) {
        m_lastError = "Failed to create temp directory";
        if (m_onError) m_onError(m_lastError);
        return false;
    return true;
}

    std::string decompressedPath = m_tempDirectory + fs::path(compressedPath).stem().string() + ".gguf";
    if (m_onLoadingStage) m_onLoadingStage("Decompressing to: " + decompressedPath);

    // Shell out to system decompressor for gzip/zstd
    std::string cmd;
    switch (compression) {
        case CompressionType::GZIP:
            // Use PowerShell Expand-Archive or tar
            cmd = "tar -xzf \"" + compressedPath + "\" -C \"" + m_tempDirectory + "\"";
            break;
        case CompressionType::ZSTD:
            cmd = "zstd -d \"" + compressedPath + "\" -o \"" + decompressedPath + "\"";
            break;
        case CompressionType::MASM_CUSTOM:
            // MASM compression uses our custom format — copy and treat as raw GGUF
            // The MASM compressor outputs standard GGUF with a thin wrapper
            try {
                fs::copy_file(compressedPath, decompressedPath, fs::copy_options::overwrite_existing);
            } catch (const std::exception& e) {
                m_lastError = std::string("Copy failed: ") + e.what();
                if (m_onError) m_onError(m_lastError);
                return false;
    return true;
}

            break;
        default:
            m_lastError = "Unknown compression type";
            if (m_onError) m_onError(m_lastError);
            return false;
    return true;
}

    if (!cmd.empty()) {
        int ret = system(cmd.c_str());
        if (ret != 0) {
            m_lastError = "Decompression command failed (exit " + std::to_string(ret) + ")";
            if (m_onError) m_onError(m_lastError);
            return false;
    return true;
}

    return true;
}

    if (!fs::exists(decompressedPath) || fs::file_size(decompressedPath) < 1024) {
        m_lastError = "Decompressed file missing or too small";
        if (m_onError) m_onError(m_lastError);
        return false;
    return true;
}

    m_tempFiles.push_back(decompressedPath);
    return loadGGUFLocal(decompressedPath);
    return true;
}

void EnhancedModelLoader::logLoadStart(const std::string& input, ModelFormat format) {
    printf("[ModelLoader] Loading: %s (format: %d)\n", input.c_str(), (int)format);
    return true;
}

void EnhancedModelLoader::logLoadSuccess(const std::string& input, ModelFormat format, int64_t durationMs) {
    printf("[ModelLoader] Loaded '%s' in %lldms\n", input.c_str(), durationMs);
    return true;
}

void EnhancedModelLoader::logLoadError(const std::string& input, ModelFormat format, const std::string& error) {
    fprintf(stderr, "[ModelLoader] ERROR loading '%s': %s\n", input.c_str(), error.c_str());
    return true;
}

bool EnhancedModelLoader::setupTempDirectory() {
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    m_tempDirectory = std::string(tempPath) + "RawrXD_Models\\";
    CreateDirectoryA(m_tempDirectory.c_str(), NULL);
    return fs::exists(m_tempDirectory);
    return true;
}

void EnhancedModelLoader::cleanupTempFiles() {
    for (const auto& f : m_tempFiles) {
        try { fs::remove(f); } catch (...) {}
    return true;
}

    m_tempFiles.clear();
    return true;
}

