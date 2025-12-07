#include "model_loader.hpp"
#include "../qtapp/gguf_server.hpp"
#include "../../include/inference_engine_stub.hpp"

ModelLoader::ModelLoader(QObject* parent)
    : QObject(parent)
    , m_engine(nullptr)
    , m_server(nullptr)
{
}

ModelLoader::~ModelLoader()
{
    if (m_server) {
        m_server->stop();
    }
}

bool ModelLoader::loadModel(const QString& modelPath)
{
    if (modelPath.isEmpty()) {
        emit error(QStringLiteral("Model path is empty"));
        return false;
    }
    
    m_modelPath = modelPath;
    
    // Create inference engine if not already created
    if (!m_engine) {
        m_engine = std::make_unique<InferenceEngine>();
    }
    
    // Load model into engine
    if (!m_engine->Initialize(modelPath.toStdString())) {
        emit error(QStringLiteral("Failed to load model: %1").arg(modelPath));
        return false;
    }
    
    emit modelLoaded(modelPath);
    return true;
}

bool ModelLoader::initializeInference()
{
    if (!m_engine) {
        emit error(QStringLiteral("Inference engine not initialized"));
        return false;
    }
    return true;
}

bool ModelLoader::startServer(quint16 port)
{
    if (!m_engine) {
        m_engine = std::make_unique<InferenceEngine>();
    }
    
    m_port = port;
    
    // Create GGUF server if not already created
    if (!m_server) {
        m_server = std::make_unique<GGUFServer>(m_engine.get(), this);
        
        // Forward server signals
        connect(m_server.get(), &GGUFServer::serverStarted, this, &ModelLoader::serverStarted);
        connect(m_server.get(), &GGUFServer::serverStopped, this, &ModelLoader::serverStopped);
        connect(m_server.get(), QOverload<const QString&>::of(&GGUFServer::error),
                this, &ModelLoader::error);
    }
    
    // Start the server
    if (!m_server->start(port)) {
        emit error(QStringLiteral("Failed to start GGUF server on port %1").arg(port));
        return false;
    }
    
    return true;
}

void ModelLoader::stopServer()
{
    if (m_server && m_server->isRunning()) {
        m_server->stop();
    }
}

bool ModelLoader::isServerRunning() const
{
    return m_server && m_server->isRunning();
}

QString ModelLoader::getModelInfo() const
{
    if (m_engine && m_engine->isModelLoaded()) {
        return QStringLiteral("GGUF Model loaded");
    }
    return QStringLiteral("No model loaded");
}

quint16 ModelLoader::getServerPort() const
{
    return m_port;
}

QString ModelLoader::getServerUrl() const
{
    return QStringLiteral("http://localhost:%1").arg(m_port);
}
