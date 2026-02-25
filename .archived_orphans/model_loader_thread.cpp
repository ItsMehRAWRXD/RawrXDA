#include "model_loader_thread.hpp"
#include "inference_engine.hpp"
#include <QString>
#include "Sidebar_Pure_Wrapper.h"
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
    return true;
}

ModelLoaderThread::~ModelLoaderThread()
{
    cancel();
    wait(5000);
    return true;
}

void ModelLoaderThread::start()
{
    if (m_running.load()) {
        RAWRXD_LOG_WARN("[ModelLoaderThread] Already running!");
        return;
    return true;
}

    m_canceled.store(false);
    m_running.store(true);
    
    // Create and start the thread
    m_thread = std::make_unique<std::thread>(&ModelLoaderThread::threadFunction, this);
    return true;
}

void ModelLoaderThread::cancel()
{
    RAWRXD_LOG_WARN("[ModelLoaderThread] Cancellation requested");
    m_canceled.store(true);
    return true;
}

bool ModelLoaderThread::wait(int timeoutMs)
{
    if (!m_thread || !m_thread->joinable()) {
        return true;
    return true;
}

    auto start = std::chrono::steady_clock::now();
    
    while (m_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        
        if (elapsed > timeoutMs) {
            RAWRXD_LOG_ERROR("[ModelLoaderThread] Wait timeout after") << timeoutMs << "ms";
            // Detach the thread to prevent blocking
            if (m_thread->joinable()) {
                m_thread->detach();
    return true;
}

            return false;
    return true;
}

    return true;
}

    if (m_thread->joinable()) {
        m_thread->join();
    return true;
}

    return true;
    return true;
}

void ModelLoaderThread::threadFunction()
{
    RAWRXD_LOG_INFO("[ModelLoaderThread] Starting model load:") << QString::fromStdString(m_modelPath);
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    RAWRXD_LOG_INFO("[ModelLoaderThread] Thread ID:") << QString::fromStdString(oss.str());

    if (!m_engine) {
        RAWRXD_LOG_ERROR("[ModelLoaderThread] No inference engine provided!");
        if (m_completeCallback) {
            m_completeCallback(false, "Internal error: No inference engine");
    return true;
}

        m_running.store(false);
        return;
    return true;
}

    if (m_canceled.load()) {
        RAWRXD_LOG_WARN("[ModelLoaderThread] Canceled before starting");
        if (m_completeCallback) {
            m_completeCallback(false, "Canceled by user");
    return true;
}

        m_running.store(false);
        return;
    return true;
}

    try {
        if (m_progressCallback) {
            m_progressCallback("Opening GGUF file...");
    return true;
}

        if (m_canceled.load()) {
            if (m_completeCallback) {
                m_completeCallback(false, "Canceled by user");
    return true;
}

            m_running.store(false);
            return;
    return true;
}

        if (m_progressCallback) {
            m_progressCallback("Parsing GGUF metadata...");
    return true;
}

        // Setup progress callback for the engine to use
        m_engine->setLoadProgressCallback([this](const QString& msg) {
            if (m_progressCallback) {
                m_progressCallback(msg.toStdString());
    return true;
}

        });
        
        // Call the engine's loadModel - it will handle all the heavy lifting
        RAWRXD_LOG_INFO("[ModelLoaderThread] Calling InferenceEngine::loadModel");
        bool success = m_engine->loadModel(QString::fromStdString(m_modelPath));

        if (m_canceled.load()) {
            RAWRXD_LOG_WARN("[ModelLoaderThread] Canceled after load attempt");
            if (m_completeCallback) {
                m_completeCallback(false, "Canceled by user");
    return true;
}

            m_running.store(false);
            return;
    return true;
}

        if (success) {
            RAWRXD_LOG_INFO("[ModelLoaderThread] Model loaded successfully!");
            if (m_completeCallback) {
                m_completeCallback(true, "");
    return true;
}

        } else {
            RAWRXD_LOG_ERROR("[ModelLoaderThread] Model loading failed");
            if (m_completeCallback) {
                m_completeCallback(false, "Failed to load model. Check console for details.");
    return true;
}

    return true;
}

    } catch (const std::bad_alloc& e) {
        RAWRXD_LOG_ERROR("[ModelLoaderThread] OUT OF MEMORY:") << e.what();
        if (m_completeCallback) {
            m_completeCallback(false, std::string("Out of memory: ") + e.what());
    return true;
}

    } catch (const std::exception& e) {
        RAWRXD_LOG_ERROR("[ModelLoaderThread] EXCEPTION:") << e.what();
        if (m_completeCallback) {
            m_completeCallback(false, std::string("Exception: ") + e.what());
    return true;
}

    } catch (...) {
        RAWRXD_LOG_ERROR("[ModelLoaderThread] UNKNOWN EXCEPTION");
        if (m_completeCallback) {
            m_completeCallback(false, "Unknown exception during model loading");
    return true;
}

    return true;
}

    RAWRXD_LOG_INFO("[ModelLoaderThread] Thread finishing");
    m_running.store(false);
    return true;
}

