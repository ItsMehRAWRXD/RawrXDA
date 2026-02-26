#pragma once
// model_loader.hpp — Qt-free facade around EnhancedModelLoader
// Uses std::function callbacks instead of Qt signals/slots

#include <string>
#include <memory>
#include <functional>
#include <cstdint>

class EnhancedModelLoader;

namespace RawrXD { class CPUInferenceEngine; }

/**
 * @class ModelLoader
 * @brief Facade for GGUF model loading and inference
 *
 * Provides simplified interface for test integration and CLI usage.
 * Handles model discovery, server startup, and HTTP inference.
 * Qt-free: uses std::function callbacks instead of signals.
 */
class ModelLoader {
public:
    // Callback types (replace Qt signals)
    using ModelLoadedFn    = std::function<void(const std::string& path)>;
    using LoadingProgressFn = std::function<void(int percent)>;
    using ServerStartedFn  = std::function<void(uint16_t port)>;
    using ServerStoppedFn  = std::function<void()>;
    using ErrorFn          = std::function<void(const std::string& message)>;

    ModelLoader();
    ~ModelLoader();

    // Non-copyable
    ModelLoader(const ModelLoader&) = delete;
    ModelLoader& operator=(const ModelLoader&) = delete;

    // Model loading
    bool loadModel(const std::string& modelPath);
    bool initializeInference();
    bool startServer(uint16_t port = 11434);
    void stopServer();
    bool isServerRunning() const;

    // Server info
    std::string getModelInfo() const;
    uint16_t getServerPort() const;
    std::string getServerUrl() const;

    // Callback setters (replace connect())
    void onModelLoaded(ModelLoadedFn fn)       { m_onModelLoaded = std::move(fn); }
    void onLoadingProgress(LoadingProgressFn fn) { m_onLoadingProgress = std::move(fn); }
    void onServerStarted(ServerStartedFn fn)   { m_onServerStarted = std::move(fn); }
    void onServerStopped(ServerStoppedFn fn)    { m_onServerStopped = std::move(fn); }
    void onError(ErrorFn fn)                   { m_onError = std::move(fn); }

private:
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_engine;
    std::unique_ptr<EnhancedModelLoader> m_enhancedLoader;
    std::string m_modelPath;
    uint16_t m_port = 11434;

    // Callbacks
    ModelLoadedFn     m_onModelLoaded;
    LoadingProgressFn m_onLoadingProgress;
    ServerStartedFn   m_onServerStarted;
    ServerStoppedFn   m_onServerStopped;
    ErrorFn           m_onError;
};
