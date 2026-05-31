#include "model_loader_thread.hpp"
#include "inference_engine.hpp"
#include <QString>
#include <QDebug>
#include <chrono>
#include <iostream>
#include <sstream>

ModelLoaderThread::ModelLoaderThread(InferenceEngine* engine, const std::string& modelPath)
    : m_engine(engine)
    , m_modelPath(modelPath)
    , m_canceled(false)
    , m_running(false)
    , m_thread(nullptr)
    , m_progressCallback(nullptr)
    , m_completeCallback(nullptr)
{
}

ModelLoaderThread::~ModelLoaderThread()
{
    cancel();
    wait(5000);
}

void ModelLoaderThread::start()
{
    if (m_running.load()) {
        qWarning() << "[ModelLoaderThread] Already running!";
        return;
    }

    m_canceled.store(false);
    m_running.store(true);
    
    // Create and start the thread
    m_thread = std::make_unique<std::thread>(&ModelLoaderThread::threadFunction, this);
}

void ModelLoaderThread::cancel()
{
    qWarning() << "[ModelLoaderThread] Cancellation requested";
    m_canceled.store(true);
}

bool ModelLoaderThread::wait(int timeoutMs)
{
    if (!m_thread || !m_thread->joinable()) {
        return true;
    }

    auto start = std::chrono::steady_clock::now();
    
    while (m_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        if (elapsed > timeoutMs) {
            qCritical() << "[ModelLoaderThread] Wait timeout after" << timeoutMs << "ms";
            // Detach the thread to prevent blocking
            if (m_thread->joinable()) {
                m_thread->detach();
            }
            return false;
        }
    }
    
    if (m_thread->joinable()) {
        m_thread->join();
    }
    
    return true;
}

void ModelLoaderThread::threadFunction()
{
    qInfo() << "[ModelLoaderThread] Starting model load:" << QString::fromStdString(m_modelPath);
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    qInfo() << "[ModelLoaderThread] Thread ID:" << QString::fromStdString(oss.str());

    if (!m_engine) {
        qCritical() << "[ModelLoaderThread] No inference engine provided!";
        if (m_completeCallback) {
            m_completeCallback(false, "Internal error: No inference engine");
        }
        m_running.store(false);
        return;
    }

    if (m_canceled.load()) {
        qWarning() << "[ModelLoaderThread] Canceled before starting";
        if (m_completeCallback) {
            m_completeCallback(false, "Canceled by user");
        }
        m_running.store(false);
        return;
    }

    try {
        if (m_progressCallback) {
            m_progressCallback("Opening GGUF file...");
        }
        
        if (m_canceled.load()) {
            if (m_completeCallback) {
                m_completeCallback(false, "Canceled by user");
            }
            m_running.store(false);
            return;
        }

        if (m_progressCallback) {
            m_progressCallback("Parsing GGUF metadata...");
        }
        
        // Setup progress callback for the engine to use
        m_engine->setLoadProgressCallback([this](const QString& msg) {
            if (m_progressCallback) {
                m_progressCallback(msg.toStdString());
            }
        });
        
        // Call the engine's loadModel - it will handle all the heavy lifting
        qInfo() << "[ModelLoaderThread] Calling InferenceEngine::loadModel";
        bool success = m_engine->loadModel(QString::fromStdString(m_modelPath));

        if (m_canceled.load()) {
            qWarning() << "[ModelLoaderThread] Canceled after load attempt";
            if (m_completeCallback) {
                m_completeCallback(false, "Canceled by user");
            }
            m_running.store(false);
            return;
        }

        if (success) {
            qInfo() << "[ModelLoaderThread] Model loaded successfully!";
            if (m_completeCallback) {
                m_completeCallback(true, "");
            }
        } else {
            qCritical() << "[ModelLoaderThread] Model loading failed";
            if (m_completeCallback) {
                m_completeCallback(false, "Failed to load model. Check console for details.");
            }
        }

    } catch (const std::bad_alloc& e) {
        qCritical() << "[ModelLoaderThread] OUT OF MEMORY:" << e.what();
        if (m_completeCallback) {
            m_completeCallback(false, std::string("Out of memory: ") + e.what());
        }
    } catch (const std::exception& e) {
        qCritical() << "[ModelLoaderThread] EXCEPTION:" << e.what();
        if (m_completeCallback) {
            m_completeCallback(false, std::string("Exception: ") + e.what());
        }
    } catch (...) {
        qCritical() << "[ModelLoaderThread] UNKNOWN EXCEPTION";
        if (m_completeCallback) {
            m_completeCallback(false, "Unknown exception during model loading");
        }
    }

    qInfo() << "[ModelLoaderThread] Thread finishing";
    m_running.store(false);
}
