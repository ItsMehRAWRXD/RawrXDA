#include "enhanced_model_loader.h"
#include "bridge/UnifiedModelMetadata.h"
#include "cpu_inference_engine.h"
#include "model_metadata_hotpatch.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

namespace {

class GGUFServer {
public:
    explicit GGUFServer(RawrXD::InferenceEngine* /*engine*/) {}

    bool start(uint16_t port) {
        running_ = true;
        port_ = port;
        return true;
    }

    void stop() { running_ = false; }
    bool isRunning() const { return running_; }
    uint16_t port() const { return port_; }

private:
    bool running_ = false;
    uint16_t port_ = 11434;
};

static std::string SanitizePathComponent(std::string s) {
    for (auto& ch : s) {
        const bool ok =
            (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_' || ch == '-' || ch == '.';
        if (!ok) ch = '_';
    }
    return s;
}

} // namespace

EnhancedModelLoader::~EnhancedModelLoader() = default;

bool EnhancedModelLoader::loadModel(const std::string& modelInput) {
    if (!m_formatRouter) m_formatRouter = std::make_unique<FormatRouter>();
    if (!m_hfDownloader) m_hfDownloader = std::make_unique<HFDownloader>();
    if (!m_ollamaProxy) m_ollamaProxy = std::make_unique<OllamaProxy>();

    if (m_onLoadingStage) m_onLoadingStage("Detecting model format...");

    const auto src_opt = m_formatRouter->route(modelInput);
    if (!src_opt) {
        m_lastError = "Could not detect model format for input: " + modelInput;
        if (m_onError) m_onError(m_lastError);
        return false;
    }

    const auto& src = *src_opt;
    return loadModelAsync(src.path); // route again inside; keeps single path
}

bool EnhancedModelLoader::loadModelAsync(const std::string& modelInput) {
    // Current codebase doesn't expose a consistent background task runner here yet.
    // Keep behavior deterministic: load synchronously, but preserve API.
    if (!m_formatRouter) m_formatRouter = std::make_unique<FormatRouter>();
    if (!m_hfDownloader) m_hfDownloader = std::make_unique<HFDownloader>();
    if (!m_ollamaProxy) m_ollamaProxy = std::make_unique<OllamaProxy>();

    const auto start = std::chrono::steady_clock::now();
    const auto src_opt = m_formatRouter->route(modelInput);
    if (!src_opt) {
        m_lastError = "Could not route model input: " + modelInput;
        if (m_onError) m_onError(m_lastError);
        return false;
    }

    const auto& src = *src_opt;
    logLoadStart(modelInput, src.format);

    bool ok = false;
    switch (src.format) {
        case ModelFormat::GGUF_LOCAL:
            ok = loadGGUFLocal(src.path);
            break;
        case ModelFormat::HF_REPO:
        case ModelFormat::HF_FILE:
            ok = loadHFModel(src.path);
            break;
        case ModelFormat::OLLAMA_REMOTE:
            ok = loadOllamaModel(src.path);
            break;
        case ModelFormat::MASM_COMPRESSED:
            ok = loadCompressedModel(src.path);
            break;
        default:
            m_lastError = "Unsupported model format: " + FormatRouter::formatToString(src.format);
            if (m_onError) m_onError(m_lastError);
            ok = false;
            break;
    }

    const auto end = std::chrono::steady_clock::now();
    const auto ms = (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (ok) logLoadSuccess(modelInput, src.format, ms);
    else logLoadError(modelInput, src.format, m_lastError);

    return ok;
}

bool EnhancedModelLoader::loadGGUFLocal(const std::string& modelPath) {
    if (m_onLoadingStage) m_onLoadingStage("Loading local GGUF...");

    if (!std::filesystem::exists(modelPath)) {
        m_lastError = "GGUF path not found: " + modelPath;
        if (m_onError) m_onError(m_lastError);
        return false;
    }

    if (!m_engine) {
        m_engine = std::make_unique<RawrXD::CPUInferenceEngine>();
    }

    if (!m_engine->LoadModel(modelPath)) {
        m_lastError = "InferenceEngine failed to load model: " + modelPath;
        if (m_onError) m_onError(m_lastError);
        return false;
    }

    m_modelPath = modelPath;
    m_loadedFormat = ModelFormat::GGUF_LOCAL;

    if (m_onLoadingProgress) m_onLoadingProgress(100);
    if (m_onModelLoaded) m_onModelLoaded(m_modelPath);
    return true;
}

bool EnhancedModelLoader::loadHFModel(const std::string& repoId) {
    if (m_onLoadingStage) m_onLoadingStage("Resolving Hugging Face model...");
    if (!m_hfDownloader) m_hfDownloader = std::make_unique<HFDownloader>();

    ModelInfo info;
    if (!m_hfDownloader->GetModelInfo(repoId, info)) {
        m_lastError = "Failed to fetch HF model info: " + repoId;
        if (m_onError) m_onError(m_lastError);
        return false;
    }

    // Prefer a .gguf artifact if present.
    std::string gguf_file;
    for (const auto& f : info.files) {
        if (f.size() >= 5 && f.substr(f.size() - 5) == ".gguf") {
            gguf_file = f;
            break;
        }
    }
    if (gguf_file.empty()) {
        m_lastError = "HF repo has no .gguf artifacts: " + repoId;
        if (m_onError) m_onError(m_lastError);
        return false;
    }

    const auto out_dir = (std::filesystem::path("cache") / "hf" / SanitizePathComponent(repoId)).string();
    std::filesystem::create_directories(out_dir);
    const auto out_path = (std::filesystem::path(out_dir) / gguf_file).string();

    if (m_onLoadingStage) m_onLoadingStage("Downloading HF artifact: " + gguf_file);
    const bool dl_ok = m_hfDownloader->DownloadModel(repoId, gguf_file, out_dir,
        [this](const DownloadProgress& p) {
            if (!m_onLoadingProgress) return;
            const int pct = (int)p.progress_percent;
            m_onLoadingProgress(std::max(0, std::min(100, pct)));
        });

    if (!dl_ok || !std::filesystem::exists(out_path)) {
        m_lastError = "HF download failed: " + repoId + " :: " + gguf_file;
        if (m_onError) m_onError(m_lastError);
        return false;
    }

    return loadGGUFLocal(out_path);
}

bool EnhancedModelLoader::loadOllamaModel(const std::string& modelName) {
    if (m_onLoadingStage) m_onLoadingStage("Connecting to Ollama...");
    if (!m_ollamaProxy) m_ollamaProxy = std::make_unique<OllamaProxy>();

    if (!m_ollamaProxy->isOllamaAvailable()) {
        m_lastError = "Ollama not available at http://localhost:11434";
        if (m_onError) m_onError(m_lastError);
        return false;
    }
    if (!m_ollamaProxy->isModelAvailable(modelName)) {
        m_lastError = "Ollama model not found: " + modelName;
        if (m_onError) m_onError(m_lastError);
        return false;
    }

    m_ollamaProxy->setModel(modelName);
    m_modelPath = "ollama:" + modelName;
    m_loadedFormat = ModelFormat::OLLAMA_REMOTE;

    // --- Metadata hotpatch: populate metadata so Ollama models display
    //     family/parameter_size/quantization in the agent tab like GitHub models ---
    if (m_onLoadingStage) m_onLoadingStage("Injecting model metadata...");

    // Fetch actual metadata from Ollama API
    auto meta = m_ollamaProxy->getModelMetadata(modelName);
    RawrXD::ModelMetadataBuffer metaBuf =
        RawrXD::ModelMetadataHotpatch::fromOllamaModel(
            modelName,
            meta.found ? meta.family : "",
            meta.found ? meta.parameter_size : "",
            meta.found ? meta.quantization_level : ""
        );

    RawrXD::ModelMetadataHotpatch::forceAgentCapable(&metaBuf);

    // Commit to Unified Metadata Bridge
    RawrXD::Bridge::UnifiedModelMetadata unifiedMeta;
    unifiedMeta.source = "ollama";
    unifiedMeta.family = meta.found ? meta.family : "";
    unifiedMeta.quantization = meta.found ? meta.quantization_level : "";
    unifiedMeta.parameter_count = 0; // Populate if available
    unifiedMeta.supports_tools = true; // Ollama models often support tools via their API
    RawrXD::Bridge::MetadataRegistry::commit(unifiedMeta);

    const bool metaComplete = RawrXD::ModelMetadataHotpatch::isComplete(&metaBuf);
    std::cout << "[EnhancedModelLoader] metadata hotpatch for '" << modelName
              << "': complete=" << (metaComplete ? "yes" : "no")
              << " flags=0x" << std::hex << metaBuf.flags << std::dec
              << " agent=" << (int)metaBuf.agent_flag << "\n";

    // TODO: store as m_lastMetadata member once enhanced_model_loader.h is updated
    (void)metaBuf;

    if (m_onLoadingProgress) m_onLoadingProgress(100);
    if (m_onModelLoaded) m_onModelLoaded(m_modelPath);
    return true;
}

bool EnhancedModelLoader::loadCompressedModel(const std::string& compressedPath) {
    if (!m_formatRouter) m_formatRouter = std::make_unique<FormatRouter>();
    const auto det = m_formatRouter->detectFormat(compressedPath);
    return decompressAndLoad(compressedPath, det.compression);
}

bool EnhancedModelLoader::startServer(uint16_t port) {
    if (!m_engine || !m_engine->IsModelLoaded()) {
        m_lastError = "Cannot start server: no model loaded";
        if (m_onError) m_onError(m_lastError);
        return false;
    }

    if (!m_server) m_server = std::make_unique<GGUFServer>(m_engine.get());
    if (!m_server->start(port)) {
        m_lastError = "Server failed to start";
        if (m_onError) m_onError(m_lastError);
        return false;
    }

    m_port = port;
    if (m_onServerStarted) m_onServerStarted(port);
    return true;
}

void EnhancedModelLoader::stopServer() {
    if (m_server) m_server->stop();
    if (m_onServerStopped) m_onServerStopped();
}

bool EnhancedModelLoader::isServerRunning() const {
    return m_server && m_server->isRunning();
}

std::string EnhancedModelLoader::getModelInfo() const {
    std::string s;
    s += "format=" + FormatRouter::formatToString(m_loadedFormat);
    s += " path=" + (m_modelPath.empty() ? "<none>" : m_modelPath);
    if (m_engine) s += " engine=" + std::string(m_engine->GetEngineName());
    return s;
}

std::string EnhancedModelLoader::getServerUrl() const {
    return "http://localhost:" + std::to_string(m_port);
}

bool EnhancedModelLoader::decompressAndLoad(const std::string& compressedPath, CompressionType compression) {
    if (compression == CompressionType::NONE) {
        return loadGGUFLocal(compressedPath);
    }
    m_lastError = "Compressed model loading not implemented for compression=" + FormatRouter::compressionToString(compression);
    if (m_onError) m_onError(m_lastError);
    return false;
}

void EnhancedModelLoader::logLoadStart(const std::string& input, ModelFormat format) {
    std::cout << "[EnhancedModelLoader] load start: " << input
              << " (" << FormatRouter::formatToString(format) << ")\n";
}

void EnhancedModelLoader::logLoadSuccess(const std::string& input, ModelFormat format, int64_t durationMs) {
    std::cout << "[EnhancedModelLoader] load ok: " << input
              << " (" << FormatRouter::formatToString(format) << ")"
              << " in " << durationMs << "ms\n";
}

void EnhancedModelLoader::logLoadError(const std::string& input, ModelFormat format, const std::string& error) {
    std::cerr << "[EnhancedModelLoader] load failed: " << input
              << " (" << FormatRouter::formatToString(format) << "): "
              << error << "\n";
}

bool EnhancedModelLoader::setupTempDirectory() {
    m_tempDirectory = (std::filesystem::temp_directory_path() / "rawrxd_tmp").string();
    std::filesystem::create_directories(m_tempDirectory);
    return true;
}

void EnhancedModelLoader::cleanupTempFiles() {
    for (const auto& f : m_tempFiles) {
        std::error_code ec;
        std::filesystem::remove(f, ec);
    }
    m_tempFiles.clear();
}

