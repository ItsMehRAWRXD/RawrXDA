#pragma once

#include <QObject>
#include <QString>
#include <memory>
#include <vector>

#include "format_router.h"

class InferenceEngine;
class GGUFServer;
class HFDownloader;
class OllamaProxy;

class EnhancedModelLoader : public QObject {
    Q_OBJECT
public:
    explicit EnhancedModelLoader(QObject* parent = nullptr);
    ~EnhancedModelLoader() override;

    bool loadModel(const QString& modelInput);
    bool loadGGUFLocal(const QString& modelPath);
    bool loadHFModel(const QString& repoId);
    bool loadOllamaModel(const QString& modelName);
    bool loadCompressedModel(const QString& compressedPath);
    bool decompressAndLoad(const QString& compressedPath, int compressionType);

    bool startServer(quint16 port);
    void stopServer();
    bool isServerRunning() const;

    QString getModelInfo() const;
    quint16 getServerPort() const;
    QString getServerUrl() const;

    QString getLastError() const { return m_lastError; }

signals:
    void modelLoaded(const QString& path);
    void loadingStage(const QString& stage);
    void loadingProgress(int percent);
    void serverStarted(quint16 port);
    void serverStopped();
    void error(const QString& message);

private:
    void logLoadStart(const QString& input, ModelFormat format);
    void logLoadSuccess(const QString& input, ModelFormat format, qint64 durationMs);
    void logLoadError(const QString& input, ModelFormat format, const QString& error);
    bool setupTempDirectory();
    void cleanupTempFiles();

private:
    std::unique_ptr<InferenceEngine> m_engine{nullptr};
    std::unique_ptr<GGUFServer> m_server{nullptr};
    std::unique_ptr<HFDownloader> m_hfDownloader{nullptr};
    std::unique_ptr<OllamaProxy> m_ollamaProxy{nullptr};
    std::string m_tempDirectory;
    std::vector<std::string> m_tempFiles;
    QString m_lastError;
    quint16 m_port{11434};
    ModelFormat m_loadedFormat{ModelFormat::UNKNOWN};
    QString m_modelPath;
};
