#include "InferenceSession.h"
#include <QDebug>

class InferenceWorker : public QObject {
    Q_OBJECT
public:
    InferenceWorker(const QString& modelName, void* modelHandle) 
        : m_modelName(modelName), m_handle(modelHandle) {}
    
public slots:
    void processInference(const QString& prompt) {
        // This is where you call your actual inference engine
        // For now, using Sovereign Loader's quantization
        qInfo() << "InferenceWorker: Processing prompt for model:" << m_modelName;
        
        emit tokenGenerated("Generated token");
        emit inferenceComplete();
    }
    
signals:
    void tokenGenerated(const QString& token);
    void inferenceComplete();
    
private:
    QString m_modelName;
    void* m_handle;
};

#include "InferenceSession.moc"

InferenceSession::InferenceSession(ModelLoaderBridge* loader, QObject* parent)
    : QObject(parent), m_loader(loader), m_workerThread(nullptr), m_worker(nullptr) {
}

InferenceSession::~InferenceSession() {
    endSession();
}

bool InferenceSession::startSession(const QString& modelName) {
    if (m_sessionActive) return false;
    
    // Verify model is loaded
    auto models = m_loader->getLoadedModels();
    for (const auto& model : models) {
        if (model.name == modelName && model.isLoaded) {
            m_activeModel = modelName;
            m_sessionActive = true;
            
            // Create worker in separate thread
            m_workerThread = new QThread(this);
            m_worker = new InferenceWorker(modelName, model.handle);
            m_worker->moveToThread(m_workerThread);
            
                // Route prompt to worker via signal-slot, ensure queued connection
                connect(this, &InferenceSession::inferenceStarted,
                    m_worker, &InferenceWorker::processInference,
                    Qt::QueuedConnection);
            connect(m_worker, &InferenceWorker::tokenGenerated,
                    this, &InferenceSession::tokenGenerated);
            connect(m_worker, &InferenceWorker::inferenceComplete,
                    this, &InferenceSession::inferenceComplete);
            
            return true;
        }
    }
    
    qWarning() << "Model not loaded:" << modelName;
    return false;
}

void InferenceSession::inferStreaming(const QString& prompt, TokenCallback callback) {
    if (!m_sessionActive || !m_workerThread) return;
    
    connect(this, &InferenceSession::tokenGenerated, 
            [callback](const QString& token) {
                callback(token.toStdString());
            });
    
    emit inferenceStarted(prompt);
    m_workerThread->start();
}

void InferenceSession::endSession() {
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
    }
    
    m_sessionActive = false;
    m_activeModel.clear();
}

double InferenceSession::getTokensPerSecond() const {
    return 0.0; // Implement based on actual metrics
}

size_t InferenceSession::getTotalTokensGenerated() const {
    return 0; // Implement based on actual metrics
}
