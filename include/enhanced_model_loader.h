#pragma once

#include <QString>
#include <QObject>
#include <memory>
#include <optional>
#include <vector>
#include "format_router.h"
#include "hf_downloader.h"
#include "ollama_proxy.h"

class GGUFServer;
class InferenceEngine;

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
class EnhancedModelLoader : public QObject {
    Q_OBJECT

public:
    explicit EnhancedModelLoader(QObject* parent = nullptr);
    ~EnhancedModelLoader() override;

    // Main loading interface (supports all formats)
    bool loadModel(const QString& modelInput);
    bool loadModelAsync(const QString& modelInput);
    
    // Format-specific loaders
    bool loadGGUFLocal(const QString& modelPath);
    bool loadHFModel(const QString& repoId);
    bool loadOllamaModel(const QString& modelName);
    bool loadCompressedModel(const QString& compressedPath);

    // Server management
    bool startServer(quint16 port = 11434);
    void stopServer();
    bool isServerRunning() const;

    // Status queries
    QString getModelInfo() const;
    quint16 getServerPort() const;
    QString getServerUrl() const;
    QString getLastError() const { return m_lastError; }
    ModelFormat getLoadedFormat() const { return m_loadedFormat; }

signals:
    void modelLoaded(const QString& path);
    void loadingProgress(int percent);
    void loadingStage(const QString& stage);
    void serverStarted(quint16 port);
    void serverStopped();
    void error(const QString& message);

private:
    bool decompressAndLoad(const QString& compressedPath, CompressionType compression);
    
    void logLoadStart(const QString& input, ModelFormat format);
    void logLoadSuccess(const QString& input, ModelFormat format, qint64 durationMs);
    void logLoadError(const QString& input, ModelFormat format, const QString& error);
    
    bool setupTempDirectory();
    void cleanupTempFiles();

    std::unique_ptr<InferenceEngine> m_engine;
    std::unique_ptr<GGUFServer> m_server;
    std::unique_ptr<FormatRouter> m_formatRouter;
    std::unique_ptr<HFDownloader> m_hfDownloader;
    std::unique_ptr<OllamaProxy> m_ollamaProxy;

    QString m_modelPath;
    QString m_lastError;
    ModelFormat m_loadedFormat = ModelFormat::UNKNOWN;
    quint16 m_port = 11434;
    
    std::string m_tempDirectory;
    std::vector<std::string> m_tempFiles;
};
