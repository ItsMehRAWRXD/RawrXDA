#include <iostream>
#include <QString>
#include <QCoreApplication>
#include <QThread>
#include <QTimer>
#include <memory>
#include "src/qtapp/inference_engine.hpp"

class ChatTester : public QObject {
    Q_OBJECT
public:
    ChatTester() : m_engine(nullptr), m_testComplete(false) {}
    
    bool runTest(const QString& modelPath) {
        // Create engine
        m_engine = new InferenceEngine();
        
        // Connect signals
        connect(m_engine, &InferenceEngine::modelLoadedChanged, this, &ChatTester::onModelLoaded);
        connect(m_engine, &InferenceEngine::resultReady, this, &ChatTester::onResultReady);
        connect(m_engine, &InferenceEngine::error, this, &ChatTester::onError);
        
        std::cout << "[Test] Loading model: " << modelPath.toStdString() << std::endl;
        m_engine->loadModel(modelPath);
        
        // Wait for test to complete
        QTimer::singleShot(5000, this, &ChatTester::onTimeout);
        
        return true;
    }
    
private slots:
    void onModelLoaded(bool loaded, const QString& modelName) {
        std::cout << "[Test] Model loaded: " << loaded << " - " << modelName.toStdString() << std::endl;
        
        if (loaded) {
            std::cout << "[Test] Sending chat message..." << std::endl;
            qint64 reqId = 12345;
            QString prompt = "Hello, what is 2+2?";
            
            m_engine->request(prompt, reqId);
            
            // Set timeout for response
            QTimer::singleShot(3000, this, &ChatTester::onTimeout);
        }
    }
    
    void onResultReady(qint64 reqId, const QString& result) {
        std::cout << "[Test] Got response (ID: " << reqId << "): " << result.toStdString() << std::endl;
        m_testComplete = true;
        QCoreApplication::quit();
    }
    
    void onError(qint64 reqId, const QString& errorMsg) {
        std::cout << "[Test] Error (ID: " << reqId << "): " << errorMsg.toStdString() << std::endl;
        m_testComplete = true;
        QCoreApplication::quit();
    }
    
    void onTimeout() {
        if (!m_testComplete) {
            std::cout << "[Test] Timeout waiting for response" << std::endl;
            m_testComplete = true;
            QCoreApplication::quit();
        }
    }
    
private:
    InferenceEngine* m_engine;
    bool m_testComplete;
};

#include "test_chat_integration.moc"

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_gguf_model>" << std::endl;
        return 1;
    }
    
    QString modelPath = QString::fromUtf8(argv[1]);
    
    ChatTester tester;
    tester.runTest(modelPath);
    
    return app.exec();
}
