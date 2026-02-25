#pragma once

// C++20, no Qt. Multi-format model loader; callbacks replace signals.

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "format_router.h"
#include "hf_downloader.h"
#include "ollama_proxy.h"

class GGUFServer;
class InferenceEngine;

class EnhancedModelLoader
{
public:
    using ModelLoadedFn    = std::function<void(const std::string& path)>;
    using LoadingProgressFn = std::function<void(int percent)>;
    using LoadingStageFn   = std::function<void(const std::string& stage)>;
    using ServerStartedFn  = std::function<void(uint16_t port)>;
    using ServerStoppedFn  = std::function<void()>;
    using ErrorFn          = std::function<void(const std::string&)>;

    EnhancedModelLoader() = default;
    ~EnhancedModelLoader();

    void setOnModelLoaded(ModelLoadedFn f)      { m_onModelLoaded = std::move(f); }
    void setOnLoadingProgress(LoadingProgressFn f) { m_onLoadingProgress = std::move(f); }
    void setOnLoadingStage(LoadingStageFn f)   { m_onLoadingStage = std::move(f); }
    void setOnServerStarted(ServerStartedFn f) { m_onServerStarted = std::move(f); }
    void setOnServerStopped(ServerStoppedFn f) { m_onServerStopped = std::move(f); }
    void setOnError(ErrorFn f)                 { m_onError = std::move(f); }

    bool loadModel(const std::string& modelInput);
    bool loadModelAsync(const std::string& modelInput);

    bool loadGGUFLocal(const std::string& modelPath);
    bool loadHFModel(const std::string& repoId);
    bool loadOllamaModel(const std::string& modelName);
    bool loadCompressedModel(const std::string& compressedPath);

    bool startServer(uint16_t port = 11434);
    void stopServer();
    bool isServerRunning() const;

    std::string getModelInfo() const;
    uint16_t getServerPort() const { return m_port; }
    std::string getServerUrl() const;
    std::string getLastError() const { return m_lastError; }
    ModelFormat getLoadedFormat() const { return m_loadedFormat; }

private:
    bool decompressAndLoad(const std::string& compressedPath, CompressionType compression);
    void logLoadStart(const std::string& input, ModelFormat format);
    void logLoadSuccess(const std::string& input, ModelFormat format, int64_t durationMs);
    void logLoadError(const std::string& input, ModelFormat format, const std::string& error);
    bool setupTempDirectory();
    void cleanupTempFiles();

    std::unique_ptr<InferenceEngine> m_engine;
    std::unique_ptr<GGUFServer> m_server;
    std::unique_ptr<FormatRouter> m_formatRouter;
    std::unique_ptr<HFDownloader> m_hfDownloader;
    std::unique_ptr<OllamaProxy> m_ollamaProxy;

    std::string m_modelPath;
    std::string m_lastError;
    ModelFormat m_loadedFormat = ModelFormat::UNKNOWN;
    uint16_t m_port = 11434;
    std::string m_tempDirectory;
    std::vector<std::string> m_tempFiles;

    ModelLoadedFn    m_onModelLoaded;
    LoadingProgressFn m_onLoadingProgress;
    LoadingStageFn   m_onLoadingStage;
    ServerStartedFn  m_onServerStarted;
    ServerStoppedFn  m_onServerStopped;
    ErrorFn          m_onError;
};
