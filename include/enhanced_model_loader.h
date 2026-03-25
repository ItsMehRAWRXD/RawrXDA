<<<<<<< HEAD
#pragma once

// C++20, no Qt. Multi-format model loader; callbacks replace signals.

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <set>
#include "format_router.h"
#include "hf_downloader.h"
#include "ollama_proxy.h"

class GGUFServer;
class InferenceEngine;

class EnhancedModelLoader
{
public:
    enum class LocalBackendMode {
        CPU_ONLY,
        GPU_ONLY,
        AUTO_WITH_VERIFIED_FALLBACK,
    };

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

    // Runtime hardening controls
    void setLocalBackendMode(LocalBackendMode mode) { m_backendMode = mode; }
    void setAllowRemoteFallback(bool allow) { m_allowRemoteFallback = allow; }
    void setPinnedLocalEndpoint(const std::string& endpoint) { m_pinnedLocalEndpoint = endpoint; }
    void setTokenizerConfigStrict(bool strict) { m_requireTokenizerConfigPair = strict; }
    void setMemoryHeadroom(float ramReserveFrac, float vramReserveFrac);
    void setQuantAllowlist(const std::set<std::string>& allowlist) { m_quantAllowlist = allowlist; }

    LocalBackendMode getLocalBackendMode() const { return m_backendMode; }
    std::string getResolvedRuntimeLane() const { return m_resolvedRuntimeLane; }

    bool startServer(uint16_t port = 11434);
    void stopServer();
    bool isServerRunning() const;

    std::string getModelInfo() const;
    uint16_t getServerPort() const { return m_port; }
    std::string getServerUrl() const;
    std::string getLastError() const { return m_lastError; }
    ModelFormat getLoadedFormat() const { return m_loadedFormat; }

private:
    bool validateModelFormatAndPermissions(const std::string& modelPath, std::string& reason) const;
    bool validateQuantizationAllowlist(const std::string& modelPath, std::string& reason) const;
    bool validateTokenizerConfigPair(const std::string& modelPath, std::string& reason) const;
    bool preflightMemory(const std::string& modelPath, std::string& reason, bool& gpuUsable) const;
    bool endpointAllowed(const std::string& endpoint) const;

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
    std::string m_resolvedRuntimeLane = "cpu-only";
    ModelFormat m_loadedFormat = ModelFormat::UNKNOWN;
    uint16_t m_port = 11434;
    std::string m_tempDirectory;
    std::vector<std::string> m_tempFiles;

    LocalBackendMode m_backendMode = LocalBackendMode::AUTO_WITH_VERIFIED_FALLBACK;
    bool m_allowRemoteFallback = false;
    std::string m_pinnedLocalEndpoint = "http://localhost:11434";
    bool m_requireTokenizerConfigPair = true;
    float m_ramReserveFraction = 0.20f;
    float m_vramReserveFraction = 0.15f;
    std::set<std::string> m_quantAllowlist = {
        "q2_k", "q3_k", "q4_0", "q4_1", "q4_k", "q4_k_s", "q4_k_m",
        "q5_0", "q5_1", "q5_k", "q5_k_s", "q5_k_m", "q6_k", "q8_0"
    };

    ModelLoadedFn    m_onModelLoaded;
    LoadingProgressFn m_onLoadingProgress;
    LoadingStageFn   m_onLoadingStage;
    ServerStartedFn  m_onServerStarted;
    ServerStoppedFn  m_onServerStopped;
    ErrorFn          m_onError;
};
=======
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
>>>>>>> origin/main
