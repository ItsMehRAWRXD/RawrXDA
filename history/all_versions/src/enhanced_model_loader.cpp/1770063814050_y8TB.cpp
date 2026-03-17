#include "enhanced_model_loader.h"
#include "cpu_inference_engine.h"
#include "hf_downloader.h"
#include "ollama_proxy.h"
#include <iostream>
#include <filesystem>

// Stub definition for GGUFServer to satisfy unique_ptr if used in header (it's forward declared)
class GGUFServer {
public:
    GGUFServer() = default;
    ~GGUFServer() = default;
};

EnhancedModelLoader::EnhancedModelLoader(void* parent) {
    // For now, this is a wrapper around CPUInferenceEngine
    // m_server is unique_ptr<GGUFServer>, so we need complete type here if we initialize it
    // But we don't initialize it here based on user snippet
}

EnhancedModelLoader::~EnhancedModelLoader() = default;

bool EnhancedModelLoader::loadModel(const String& modelInput) {
    // Determine model type and route accordingly
    // Check for suffix
    std::string input = modelInput;
    if (input.length() >= 5 && input.substr(input.length() - 5) == ".gguf") {
        return loadGGUFLocal(modelInput);
    }
    if (input.length() >= 4 && input.substr(input.length() - 4) == ".bin") {
        return loadGGUFLocal(modelInput);
    }
    
    if (modelInput.find('/') != std::string::npos) {
        return loadHFModel(modelInput);  // e.g., "microsoft/Phi-3-mini-4k-instruct"
    } else {
        return loadOllamaModel(modelInput);  // e.g., "llama3"
    }
}

bool EnhancedModelLoader::loadModelAsync(const String& modelInput) {
    // Basic async implementation
    // In production this would spawn a thread and emit signals
    // For now we do sync
    return loadModel(modelInput);
}

bool EnhancedModelLoader::loadGGUFLocal(const String& modelPath) {
    try {
        // Use the global CPU inference engine
        auto engine = RawrXD::CPUInferenceEngine::getInstance();
        return engine->loadModel(modelPath).has_value();
    } catch (...) {
        return false;
    }
}

bool EnhancedModelLoader::loadHFModel(const String& repoId) {
    // Stub: would download from HuggingFace Hub
    std::cout << "[EnhancedModelLoader] HF Model load requested: " << repoId << "\n";
    std::cout << "[EnhancedModelLoader] Not implemented (Stub)\n";
    return false;
}

bool EnhancedModelLoader::loadOllamaModel(const String& modelName) {
    // Stub: would call Ollama API
    std::cout << "[EnhancedModelLoader] Ollama Model load requested: " << modelName << "\n";
    std::cout << "[EnhancedModelLoader] Not implemented (Stub)\n";
    return false;
}

bool EnhancedModelLoader::loadCompressedModel(const String& compressedPath) {
    // Stub: would decompress and load
    return false;
}

bool EnhancedModelLoader::startServer(uint16_t port) {
    return true; // Stub
}

void EnhancedModelLoader::stopServer() {
}

bool EnhancedModelLoader::isServerRunning() const {
    return false;
}

String EnhancedModelLoader::getModelInfo() const {
    auto engine = RawrXD::CPUInferenceEngine::getInstance();
    if (engine->isModelLoaded()) {
        auto status = engine->getStatus();
        if (status.contains("model_path")) {
             return "GGUF Model Loaded: " + status["model_path"].get<std::string>();
        }
        return "GGUF Model Loaded";
    }
    return "No Model Loaded";
}

// Stub getters if needed by header
uint16_t EnhancedModelLoader::getServerPort() const { return 11434; }
String EnhancedModelLoader::getServerUrl() const { return "http://localhost:11434"; }

// Logic for logging etc.
void EnhancedModelLoader::logLoadStart(const String& input, ModelFormat format) {}
void EnhancedModelLoader::logLoadSuccess(const String& input, ModelFormat format, int64_t durationMs) {}
void EnhancedModelLoader::logLoadError(const String& input, ModelFormat format, const String& error) {}
bool EnhancedModelLoader::setupTempDirectory() { return true; }
void EnhancedModelLoader::cleanupTempFiles() {}
bool EnhancedModelLoader::decompressAndLoad(const String& compressedPath, CompressionType compression) { return false; }

