#include "StreamingInferenceEngine.h"
#include "IDEIntegration.h"
#include <QDebug>
#include <QElapsedTimer>

StreamingInferenceEngine::StreamingInferenceEngine(QObject* parent)
    : QObject(parent),
      m_session(nullptr),
      m_tokenRouter(IDEIntegration::getInstance()->getTokenRouter()),
      m_tokenCount(0),
      m_isRunning(false) {
    qInfo() << "StreamingInferenceEngine initialized";
}

StreamingInferenceEngine::~StreamingInferenceEngine() {
    cancelInference();
}

void StreamingInferenceEngine::runInference(const InferenceConfig& config) {
    if (m_isRunning) {
        qWarning() << "Inference already running";
        return;
    }
    
    if (config.modelName.isEmpty()) {
        emit inferenceFailed("Model name is empty");
        return;
    }
    
    m_isRunning = true;
    m_tokenCount = 0;
    m_currentText.clear();
    
    QElapsedTimer timer;
    timer.start();
    
    emit inferenceStarted(config.modelName);
    
    // Create inference session
    IDEIntegration* ide = IDEIntegration::getInstance();
    m_session = ide->createInferenceSession(config.modelName);
    
    if (!m_session) {
        emit inferenceFailed(QString("Failed to create session for model: %1").arg(config.modelName));
        m_isRunning = false;
        return;
    }
    
    // Setup token callback
    if (m_tokenCallback) {
        setTokenCallback(m_tokenCallback);
    }
    
    // Start inference
    m_session->inferStreaming(config.prompt, [this](const std::string& token) {
        QString tokenStr = QString::fromStdString(token);
        m_currentText += tokenStr;
        m_tokenCount++;
        
        if (m_tokenCallback) {
            m_tokenCallback(tokenStr);
        }
        
        emit tokenReceived(tokenStr);
    });
    
    // Record completion metrics
    connect(m_session, &InferenceSession::inferenceComplete,
            this, &StreamingInferenceEngine::onInferenceComplete);
}

void StreamingInferenceEngine::cancelInference() {
    if (m_session) {
        m_session->endSession();
        IDEIntegration::getInstance()->deleteInferenceSession(m_session);
        m_session = nullptr;
    }
    
    m_isRunning = false;
    emit inferenceCancelled();
}

void StreamingInferenceEngine::setTokenCallback(std::function<void(const QString&)> callback) {
    m_tokenCallback = callback;
}

void StreamingInferenceEngine::onTokenReceived(const QString& token) {
    m_currentText += token;
}

void StreamingInferenceEngine::onInferenceComplete() {
    if (!m_isRunning) return;
    
    m_isRunning = false;
    
    InferenceResult result;
    result.fullText = m_currentText;
    result.tokensGenerated = m_tokenCount;
    result.executionTimeMs = 0.0; // Should be measured from actual inference
    
    if (m_tokenCount > 0) {
        result.tokensPerSecond = (m_tokenCount * 1000.0) / (result.executionTimeMs > 0 ? result.executionTimeMs : 1);
    }
    
    // Cleanup session
    if (m_session) {
        m_session->endSession();
        IDEIntegration::getInstance()->deleteInferenceSession(m_session);
        m_session = nullptr;
    }
    
    emit inferenceCompleted(result);
    
    qInfo() << "Inference completed:"
            << "Tokens:" << m_tokenCount
            << "TPS:" << result.tokensPerSecond;
}
