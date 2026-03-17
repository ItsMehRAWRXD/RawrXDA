#pragma once

#include "RawrXD_Foundation.h"
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include "format_router.h"
#include "hf_downloader.h"
#include "ollama_proxy.h"
#include "RawrXD_SignalSlot.h"

using RawrXD::String;
using RawrXD::Signal;

class GGUFServer;
namespace RawrXD { class InferenceEngine; }

/**
 * @class EnhancedModelLoader
 * @brief Multi-format model loader with routing, caching, and error handling
 * 
 * Supports:
 * - Local GGUF files
 * - HuggingFace Hub downloads
 * - Ollama remote inference
 * - MASM-compressed models with automatic decompression
 */
class EnhancedModelLoader {
public:
    explicit EnhancedModelLoader(void* parent = nullptr);
    virtual ~EnhancedModelLoader();

    // Main loading interface (supports all formats)
    bool loadModel(const String& modelInput);
    bool loadModelAsync(const String& modelInput);
    
    // Format-specific loaders
    bool loadGGUFLocal(const String& modelPath);
    bool loadHFModel(const String& repoId);
    bool loadOllamaModel(const String& modelName);
    bool loadCompressedModel(const String& compressedPath);

    // Server management
    bool startServer(uint16_t port = 11434);
    void stopServer();
    bool isServerRunning() const;

    // Status queries
    String getModelInfo() const;
    uint16_t getServerPort() const;
    String getServerUrl() const;
    String getLastError() const { return m_lastError; }
    ModelFormat getLoadedFormat() const { return m_loadedFormat; }

    // Signals
    Signal<const String&> modelLoaded;
    Signal<int> loadingProgress;
    Signal<const String&> loadingStage;
    Signal<uint16_t> serverStarted;
    Signal<> serverStopped;
    Signal<const String&> error;

private:
    bool decompressAndLoad(const String& compressedPath, CompressionType compression);
    
    void logLoadStart(const String& input, ModelFormat format);
    void logLoadSuccess(const String& input, ModelFormat format, int64_t durationMs);
    void logLoadError(const String& input, ModelFormat format, const String& error);
    
    bool setupTempDirectory();
    void cleanupTempFiles();

    std::unique_ptr<RawrXD::InferenceEngine> m_engine;
    std::unique_ptr<GGUFServer> m_server;
    std::unique_ptr<FormatRouter> m_formatRouter;
    std::unique_ptr<HFDownloader> m_hfDownloader;
    std::unique_ptr<OllamaProxy> m_ollamaProxy;

    String m_modelPath;
    String m_lastError;
    ModelFormat m_loadedFormat = ModelFormat::UNKNOWN;
    uint16_t m_port = 11434;
    
    std::string m_tempDirectory;
    std::vector<std::string> m_tempFiles;
};
