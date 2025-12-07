#pragma once

#include <QString>
#include <QObject>
#include <memory>

class GGUFServer;
class InferenceEngine;

/**
 * @class ModelLoader
 * @brief Facade for GGUF model loading and inference
 * 
 * Provides simplified interface to GGUFServer for test integration.
 * Handles model discovery, server startup, and HTTP inference.
 */
class ModelLoader : public QObject {
    Q_OBJECT

public:
    explicit ModelLoader(QObject* parent = nullptr);
    ~ModelLoader() override;

    // Model loading
    bool loadModel(const QString& modelPath);
    bool initializeInference();
    bool startServer(quint16 port = 11434);
    void stopServer();
    bool isServerRunning() const;
    
    // Server info
    QString getModelInfo() const;
    quint16 getServerPort() const;
    QString getServerUrl() const;

signals:
    void modelLoaded(const QString& path);
    void loadingProgress(int percent);
    void serverStarted(quint16 port);
    void serverStopped();
    void error(const QString& message);

private:
    std::unique_ptr<InferenceEngine> m_engine;
    std::unique_ptr<GGUFServer> m_server;
    QString m_modelPath;
    quint16 m_port = 11434;
};
