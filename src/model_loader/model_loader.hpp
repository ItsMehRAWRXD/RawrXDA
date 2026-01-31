#pragma once


#include <memory>

class GGUFServer;
class InferenceEngine;
class EnhancedModelLoader;

/**
 * @class ModelLoader
 * @brief Facade for GGUF model loading and inference
 * 
 * Provides simplified interface to GGUFServer for test integration.
 * Handles model discovery, server startup, and HTTP inference.
 */
class ModelLoader : public void {

public:
    explicit ModelLoader(void* parent = nullptr);
    ~ModelLoader() override;

    // Model loading
    bool loadModel(const std::string& modelPath);
    bool initializeInference();
    bool startServer(quint16 port = 11434);
    void stopServer();
    bool isServerRunning() const;
    
    // Server info
    std::string getModelInfo() const;
    quint16 getServerPort() const;
    std::string getServerUrl() const;

    void modelLoaded(const std::string& path);
    void loadingProgress(int percent);
    void serverStarted(quint16 port);
    void serverStopped();
    void error(const std::string& message);

private:
    std::unique_ptr<InferenceEngine> m_engine;
    std::unique_ptr<GGUFServer> m_server;
    std::unique_ptr<EnhancedModelLoader> m_enhancedLoader;
    std::string m_modelPath;
    quint16 m_port = 11434;
};

