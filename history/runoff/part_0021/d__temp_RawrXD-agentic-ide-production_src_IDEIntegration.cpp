#include "IDEIntegration.h"
#include <QDebug>

IDEIntegration* IDEIntegration::s_instance = nullptr;
QMutex IDEIntegration::s_mutex;

IDEIntegration* IDEIntegration::getInstance() {
    QMutexLocker lock(&s_mutex);
    if (!s_instance) {
        s_instance = new IDEIntegration();
    }
    return s_instance;
}

IDEIntegration::IDEIntegration()
    : m_loader(new ModelLoaderBridge(this)),
      m_tokenRouter(new TokenStreamRouter(this)),
      m_performanceMonitor(nullptr),
      m_initialized(false) {
    qInfo() << "IDEIntegration constructed";
}

IDEIntegration::~IDEIntegration() {
    shutdown();
}

bool IDEIntegration::initialize() {
    if (m_initialized) {
        qWarning() << "IDEIntegration already initialized";
        return true;
    }
    
    // Initialize Sovereign Loader bridge
    if (!m_loader->initialize(8, 4096)) {
        qCritical() << "Failed to initialize ModelLoaderBridge";
        return false;
    }
    
    // Initialize performance monitor
    m_performanceMonitor = new PerformanceMonitor(nullptr);
    
    m_initialized = true;
    emit systemInitialized();
    qInfo() << "IDEIntegration system initialized";
    
    return true;
}

void IDEIntegration::shutdown() {
    if (!m_initialized) return;
    
    // Shut down all active sessions
    for (auto* session : m_activeSessions) {
        session->endSession();
        delete session;
    }
    m_activeSessions.clear();
    
    // Shut down loader
    m_loader->shutdown();
    
    m_initialized = false;
    emit systemShutdown();
    qInfo() << "IDEIntegration system shutdown";
}

void IDEIntegration::loadModel(const QString& path) {
    m_loader->loadModelAsync(path);
}

void IDEIntegration::unloadModel(const QString& modelName) {
    auto models = m_loader->getLoadedModels();
    for (const auto& model : models) {
        if (model.name == modelName && model.isLoaded) {
            // Mark as unloaded (implementation in ModelLoaderBridge)
            emit modelUnloaded(modelName);
            break;
        }
    }
}

QVector<ModelInfo> IDEIntegration::getLoadedModels() const {
    return m_loader->getLoadedModels();
}

InferenceSession* IDEIntegration::createInferenceSession(const QString& modelName) {
    auto* session = new InferenceSession(m_loader, this);
    if (session->startSession(modelName)) {
        m_activeSessions.append(session);
        emit inferenceStarted(modelName);
        return session;
    } else {
        delete session;
        return nullptr;
    }
}

void IDEIntegration::deleteInferenceSession(InferenceSession* session) {
    if (session) {
        session->endSession();
        m_activeSessions.removeAll(session);
        delete session;
    }
}

TokenStreamRouter* IDEIntegration::getTokenRouter() const {
    return m_tokenRouter;
}

PerformanceMonitor* IDEIntegration::getPerformanceMonitor() const {
    return m_performanceMonitor;
}

bool IDEIntegration::isInitialized() const {
    return m_initialized;
}

QString IDEIntegration::getSystemStatus() const {
    QString status = QString("Initialized: %1\n")
        .arg(m_initialized ? "Yes" : "No");
    
    auto models = m_loader->getLoadedModels();
    status += QString("Loaded Models: %1\n").arg(models.size());
    status += QString("Active Sessions: %1\n").arg(m_activeSessions.size());
    
    return status;
}
