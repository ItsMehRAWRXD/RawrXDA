#include "enhanced_model_loader.h"
#include "inference_engine.h"
#include <iostream>

// Stub definition for GGUFServer to satisfy unique_ptr
class GGUFServer {
public:
    GGUFServer() = default;
    ~GGUFServer() = default;
};

EnhancedModelLoader::EnhancedModelLoader(void* parent) {
    // Stubs
    // m_server = std::make_unique<GGUFServer>(); // Optional, only if needed by stub logic
}

EnhancedModelLoader::~EnhancedModelLoader() {
    // unique_ptr destructors run here, GGUFServer and InferenceEngine must be complete types
}

bool EnhancedModelLoader::loadModel(const String& modelInput) {
    // Stub
    return true;
}

bool EnhancedModelLoader::loadModelAsync(const String& modelInput) {
    return true;
}

bool EnhancedModelLoader::loadGGUFLocal(const String& modelPath) {
    return true;
}

bool EnhancedModelLoader::loadHFModel(const String& repoId) {
    return true;
}

bool EnhancedModelLoader::loadOllamaModel(const String& modelName) {
    return true;
}

bool EnhancedModelLoader::loadCompressedModel(const String& compressedPath) {
    return true;
}

bool EnhancedModelLoader::startServer(uint16_t port) {
    return true;
}

void EnhancedModelLoader::stopServer() {
}

bool EnhancedModelLoader::isServerRunning() const {
    return false;
}

String EnhancedModelLoader::getModelInfo() const {
    return "Stub Model Info";
}

uint16_t EnhancedModelLoader::getServerPort() const {
    return m_port;
}

String EnhancedModelLoader::getServerUrl() const {
    return "http://localhost:11434";
}

bool EnhancedModelLoader::decompressAndLoad(const String& compressedPath, CompressionType compression) {
    return true;
}

void EnhancedModelLoader::logLoadStart(const String& input, ModelFormat format) {
}

void EnhancedModelLoader::logLoadSuccess(const String& input, ModelFormat format, int64_t durationMs) {
}

void EnhancedModelLoader::logLoadError(const String& input, ModelFormat format, const String& error) {
}

bool EnhancedModelLoader::setupTempDirectory() {
    return true;
}

void EnhancedModelLoader::cleanupTempFiles() {
}
