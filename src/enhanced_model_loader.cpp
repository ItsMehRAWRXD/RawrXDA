#include "enhanced_model_loader.h"
#include "bridge/UnifiedModelMetadata.h"
#include "cpu_inference_engine.h"
#include "model_metadata_hotpatch.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <set>
#include <cctype>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")
#endif

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

static std::string ToLowerAscii(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

static bool StartsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

static bool EndsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool IsLoopbackEndpoint(const std::string& endpoint) {
    std::string lower = ToLowerAscii(endpoint);
    std::string hostPort = lower;
    const size_t scheme = lower.find("://");
    if (scheme != std::string::npos) {
        hostPort = lower.substr(scheme + 3);
    }
    const size_t pathPos = hostPort.find('/');
    if (pathPos != std::string::npos) {
        hostPort = hostPort.substr(0, pathPos);
    }
    return StartsWith(hostPort, "localhost") || StartsWith(hostPort, "127.0.0.1") ||
           StartsWith(hostPort, "[::1]") || StartsWith(hostPort, "::1");
}

static bool HasReadPermission(const std::string& modelPath) {
    std::ifstream f(modelPath, std::ios::binary);
    return f.good();
}

static bool HasGgufMagic(const std::string& modelPath) {
    std::ifstream f(modelPath, std::ios::binary);
    if (!f.is_open()) {
        return false;
    }
    uint32_t magic = 0;
    f.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    return f.good() && magic == 0x46554747U;
}

static std::string ExtractQuantTag(const std::string& modelPath) {
    const std::string name = ToLowerAscii(std::filesystem::path(modelPath).filename().string());
    const std::vector<std::string> known = {
        "q2_k", "q3_k", "q4_0", "q4_1", "q4_k_s", "q4_k_m", "q4_k",
        "q5_0", "q5_1", "q5_k_s", "q5_k_m", "q5_k", "q6_k", "q8_0"
    };
    for (const std::string& tag : known) {
        if (name.find(tag) != std::string::npos) {
            return tag;
        }
    }
    return "";
}

} // namespace

EnhancedModelLoader::~EnhancedModelLoader() = default;

void EnhancedModelLoader::setMemoryHeadroom(float ramReserveFrac, float vramReserveFrac) {
    m_ramReserveFraction = std::max(0.0f, std::min(0.95f, ramReserveFrac));
    m_vramReserveFraction = std::max(0.0f, std::min(0.95f, vramReserveFrac));
}

bool EnhancedModelLoader::endpointAllowed(const std::string& endpoint) const {
    if (m_allowRemoteFallback) {
        return true;
    }
    return IsLoopbackEndpoint(endpoint);
}

bool EnhancedModelLoader::validateModelFormatAndPermissions(const std::string& modelPath, std::string& reason) const {
    const std::string lower = ToLowerAscii(modelPath);
    if (!EndsWith(lower, ".gguf") || !HasGgufMagic(modelPath)) {
        reason = "Model format rejected: only valid GGUF files are accepted";
        return false;
    }
    if (!HasReadPermission(modelPath)) {
        reason = "permission denied for runtime user";
        return false;
    }
    return true;
}

bool EnhancedModelLoader::validateQuantizationAllowlist(const std::string& modelPath, std::string& reason) const {
    const std::string quantTag = ExtractQuantTag(modelPath);
    if (quantTag.empty() || m_quantAllowlist.find(quantTag) == m_quantAllowlist.end()) {
        reason = "unsupported quant: rejected at model load";
        return false;
    }
    return true;
}

bool EnhancedModelLoader::validateTokenizerConfigPair(const std::string& modelPath, std::string& reason) const {
    if (!m_requireTokenizerConfigPair) {
        return true;
    }

    const std::filesystem::path p(modelPath);
    const std::filesystem::path dir = p.parent_path();
    const std::string stem = p.stem().string();
    const std::filesystem::path tokenizer = dir / (stem + ".tokenizer.json");
    const std::filesystem::path config = dir / (stem + ".config.json");

    if (!std::filesystem::exists(tokenizer) || !std::filesystem::exists(config)) {
        reason = "Tokenizer/config mismatch: tokenizer/config pair missing for model stem";
        return false;
    }

    std::ifstream t(tokenizer, std::ios::binary);
    std::ifstream c(config, std::ios::binary);
    if (!t.good() || !c.good()) {
        reason = "Tokenizer/config mismatch: tokenizer/config not readable";
        return false;
    }

    std::string tBlob((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    std::string cBlob((std::istreambuf_iterator<char>(c)), std::istreambuf_iterator<char>());
    if (tBlob.find("version") == std::string::npos || cBlob.find("version") == std::string::npos) {
        reason = "Tokenizer/config mismatch: tokenizer/config version field missing";
        return false;
    }
    return true;
}

bool EnhancedModelLoader::preflightMemory(const std::string& modelPath, std::string& reason, bool& gpuUsable) const {
    reason.clear();
    gpuUsable = false;
    const uint64_t modelBytes = static_cast<uint64_t>(std::filesystem::file_size(modelPath));

#ifdef _WIN32
    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    if (!GlobalMemoryStatusEx(&mem)) {
        reason = "unable to query system memory";
        return false;
    }

    const uint64_t availRam = mem.ullAvailPhys;
    const uint64_t ramLimit = static_cast<uint64_t>(static_cast<double>(availRam) * (1.0 - m_ramReserveFraction));
    if (modelBytes > ramLimit) {
        reason = "insufficient RAM headroom";
        return false;
    }

    IDXGIFactory1* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
        IDXGIAdapter1* adapter = nullptr;
        if (SUCCEEDED(factory->EnumAdapters1(0, &adapter))) {
            DXGI_ADAPTER_DESC1 desc = {};
            if (SUCCEEDED(adapter->GetDesc1(&desc)) && desc.DedicatedVideoMemory > 0) {
                const uint64_t vramLimit = static_cast<uint64_t>(
                    static_cast<double>(desc.DedicatedVideoMemory) * (1.0 - m_vramReserveFraction));
                gpuUsable = modelBytes <= vramLimit;
            }
            adapter->Release();
        }
        factory->Release();
    }
#else
    (void)modelPath;
#endif

    return true;
}

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

    std::string reason;
    if (!validateModelFormatAndPermissions(modelPath, reason)) {
        m_lastError = reason;
        if (m_onError) m_onError(m_lastError);
        return false;
    }

    if (!validateQuantizationAllowlist(modelPath, reason)) {
        m_lastError = reason;
        if (m_onError) m_onError(m_lastError);
        return false;
    }

    if (!validateTokenizerConfigPair(modelPath, reason)) {
        m_lastError = reason;
        if (m_onError) m_onError(m_lastError);
        return false;
    }

    bool gpuUsable = false;
    if (!preflightMemory(modelPath, reason, gpuUsable)) {
        m_lastError = "Memory preflight failed: " + reason;
        if (m_onError) m_onError(m_lastError);
        return false;
    }

    if (m_backendMode == LocalBackendMode::GPU_ONLY) {
        // No GPU inference engine is wired in this lane yet; fail explicit instead of silent fallback.
        m_lastError = gpuUsable
            ? "backend mismatch: gpu-only selected but GPU backend is not available in this build lane"
            : "backend mismatch: gpu-only selected and VRAM preflight failed";
        if (m_onError) m_onError(m_lastError);
        std::cout << "[EnhancedModelLoader] backend=gpu-only result=fail reason=" << m_lastError << "\n";
        return false;
    }

    if (m_backendMode == LocalBackendMode::CPU_ONLY) {
        m_resolvedRuntimeLane = "cpu-only";
    } else {
        m_resolvedRuntimeLane = gpuUsable ? "gpu-only" : "cpu-only";
    }

    std::cout << "[EnhancedModelLoader] backend=" << m_resolvedRuntimeLane
              << " result=ok reason=" << (gpuUsable ? "gpu preflight passed" : "cpu fallback verified")
              << "\n";

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

    if (!endpointAllowed(m_pinnedLocalEndpoint)) {
        m_lastError = "Endpoint rejected: local runtime is pinned to localhost unless remote fallback is enabled";
        if (m_onError) m_onError(m_lastError);
        return false;
    }

    m_ollamaProxy->setBaseUrl(m_pinnedLocalEndpoint);

    if (!m_ollamaProxy->isOllamaAvailable()) {
        m_lastError = "Ollama not available at " + m_pinnedLocalEndpoint;
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
    std::cout << "[EnhancedModelLoader] endpoint=" << m_pinnedLocalEndpoint << " local=true\n";

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

