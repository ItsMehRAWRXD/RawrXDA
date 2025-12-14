#pragma once

#include <thread>
#include <atomic>
#include <string>
#include <functional>
#include <memory>

class InferenceEngine;

/**
 * @brief Pure C++ standard library thread for loading GGUF models
 * 
 * No Qt threading - uses std::thread for reliable, cancellable loading.
 * This will actually work without freezing or hanging.
 */
class ModelLoaderThread
{
public:
    using ProgressCallback = std::function<void(const std::string&)>;
    using CompleteCallback = std::function<void(bool, const std::string&)>;

    explicit ModelLoaderThread(InferenceEngine* engine, const std::string& modelPath);
    ~ModelLoaderThread();

    // Start the loading thread
    void start();
    
    // Request cancellation (thread will stop at next checkpoint)
    void cancel();
    
    // Check if canceled
    bool isCanceled() const { return m_canceled.load(); }
    
    // Wait for thread to complete (with timeout)
    bool wait(int timeoutMs = 5000);
    
    // Check if thread is running
    bool isRunning() const { return m_running.load(); }

    // Set callbacks for progress and completion
    void setProgressCallback(ProgressCallback cb) { m_progressCallback = cb; }
    void setCompleteCallback(CompleteCallback cb) { m_completeCallback = cb; }

private:
    void threadFunction();

    InferenceEngine* m_engine;
    std::string m_modelPath;
    std::atomic<bool> m_canceled;
    std::atomic<bool> m_running;
    std::unique_ptr<std::thread> m_thread;
    
    ProgressCallback m_progressCallback;
    CompleteCallback m_completeCallback;
};
