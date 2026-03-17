// model_loader.cpp — Qt-free facade around EnhancedModelLoader
// Uses std::function callbacks, std::string, std::cout/cerr

#include "model_loader.hpp"
#include "../../include/enhanced_model_loader.h"
#include "../../include/inference_engine_stub.hpp"
#include <chrono>
#include <iostream>

#define FIRE(cb, ...) do { if (cb) cb(__VA_ARGS__); } while(0)

ModelLoader::ModelLoader()
    : m_engine(nullptr)
    , m_enhancedLoader(std::make_unique<EnhancedModelLoader>())
{
    // Wire enhanced loader callbacks to our facade callbacks
    m_enhancedLoader->onModelLoaded([this](const std::string& path) {
        FIRE(m_onModelLoaded, path);
    });
    m_enhancedLoader->onError([this](const std::string& msg) {
        FIRE(m_onError, msg);
    });
    m_enhancedLoader->onLoadingProgress([this](int pct) {
        FIRE(m_onLoadingProgress, pct);
    });
}

ModelLoader::~ModelLoader()
{
    stopServer();
}

bool ModelLoader::loadModel(const std::string& modelPath)
{
    const auto start = std::chrono::steady_clock::now();

    if (modelPath.empty()) {
        FIRE(m_onError, std::string("Model path is empty"));
        return false;
    }

    std::cout << "[ModelLoader] Loading model: " << modelPath << "\n";
    m_modelPath = modelPath;

    // Use enhanced loader for multi-format support (GGUF/HF/Ollama/MASM)
    bool success = m_enhancedLoader->loadModel(modelPath);

    if (success) {
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        std::cout << "[ModelLoader] Model loaded successfully in " << duration << " ms\n";
    } else {
        std::cerr << "[ModelLoader] Model load failed: " << m_enhancedLoader->getLastError() << "\n";
    }

    return success;
}

bool ModelLoader::initializeInference()
{
    if (!m_engine) {
        FIRE(m_onError, std::string("Inference engine not initialized"));
        return false;
    }
    return true;
}

bool ModelLoader::startServer(uint16_t port)
{
    if (!m_engine) {
        m_engine = std::make_unique<RawrXD::CPUInferenceEngine>();
    }
    m_port = port;

    // Server integration pending — GGUFServer needs Qt-free rewrite
    // For now, delegate to enhanced loader
    bool ok = m_enhancedLoader->startServer(port);
    if (ok) {
        FIRE(m_onServerStarted, port);
    } else {
        FIRE(m_onError, std::string("Failed to start server on port ") + std::to_string(port));
    }
    return ok;
}

void ModelLoader::stopServer()
{
    m_enhancedLoader->stopServer();
}

bool ModelLoader::isServerRunning() const
{
    return m_enhancedLoader->isServerRunning();
}

std::string ModelLoader::getModelInfo() const
{
    return m_enhancedLoader->getModelInfo();
}

uint16_t ModelLoader::getServerPort() const
{
    return m_port;
}

std::string ModelLoader::getServerUrl() const
{
    return "http://localhost:" + std::to_string(m_port);
}
