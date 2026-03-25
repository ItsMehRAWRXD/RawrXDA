#include "ai_workers.h"
<<<<<<< HEAD
#include "../utils/sovereign_bridge.hpp"
#include "ai_digestion_engine.hpp"
#include "ai_training_pipeline.hpp"
#include "ai_types.hpp"
#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>

namespace
{
std::deque<std::function<void()>> s_invokeQueue;
std::mutex s_invokeMutex;
}

void AIWorkersInvokeLater(std::function<void()> f)
{
    std::lock_guard<std::mutex> lk(s_invokeMutex);
    s_invokeQueue.push_back(std::move(f));
}

void AIWorkersProcessInvokeQueue()
{
    std::deque<std::function<void()>> copy;
    {
        std::lock_guard<std::mutex> lk(s_invokeMutex);
        copy.swap(s_invokeQueue);
    }
    for (auto& fn : copy)
        fn();
}
=======
#include "ai_digestion_engine.hpp"
#include "ai_training_pipeline.hpp"
#include "ai_types.hpp"
#include "../utils/sovereign_bridge.hpp"
#include <thread>
#include <chrono>
#include <iostream>
>>>>>>> origin/main

// ============================================================================
// DigestionWorker Implementation
// ============================================================================
<<<<<<< HEAD
DigestionWorker::DigestionWorker(AIDigestionEngine* engine) : m_engine(engine) {}

void DigestionWorker::cancel()
{
    m_cancelled = true;
}

void DigestionWorker::processFiles(const std::vector<std::string>& files, const DigestionConfig& config)
{
    if (!m_engine)
    {
        if (onError)
            onError("Engine is null");
=======
DigestionWorker::DigestionWorker(AIDigestionEngine* engine)
    : m_engine(engine) {
}

void DigestionWorker::cancel() {
    m_cancelled = true;
}

void DigestionWorker::processFiles(const std::vector<std::string>& files, const DigestionConfig& config) {
    if (!m_engine) {
        if(onError) onError("Engine is null");
>>>>>>> origin/main
        return;
    }

    int count = 0;
    int total = (int)files.size();

    // Process each file through the engine
<<<<<<< HEAD
    for (const std::string& file : files)
    {
        if (m_cancelled)
            break;

        // SOVEREIGN KERNEL INTEGRATION: Check thermal throttle
        while (SovereignBridge::shouldYield())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (m_cancelled)
                break;
        }

        m_engine->analyzeFile(file, config);

        count++;
        if (onProgress)
            onProgress((int)((float)count / total * 100));

        // Minor yield to keep threading cooperative
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (onProgress)
        onProgress(100);
=======
    for (const std::string& file : files) {
        if(m_cancelled) break;

        // SOVEREIGN KERNEL INTEGRATION: Check thermal throttle
        while (SovereignBridge::shouldYield()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if(m_cancelled) break;
        }

        m_engine->analyzeFile(file, config);
        
        count++;
        if (onProgress) onProgress((int)((float)count / total * 100));
        
        // Minor yield to keep threading cooperative
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (onProgress) onProgress(100);
>>>>>>> origin/main
}

// ============================================================================
// AITrainingWorker Implementation
// ============================================================================
<<<<<<< HEAD
AITrainingWorker::AITrainingWorker(AITrainingPipeline* pipeline) : m_pipeline(pipeline) {}

void AITrainingWorker::cancel()
{
    m_cancelled = true;
}

void AITrainingWorker::startTraining(const std::string& dataset, const std::string& modelName,
                                     const std::string& outputPath, const TrainingConfig& config)
{
    if (!m_pipeline)
    {
        if (onError)
            onError("Pipeline is null");
        return;
    }

    if (!m_pipeline->prepareEnvironment(config))
    {
        if (onError)
            onError("Failed to prepare environment: " + m_pipeline->getLastError());
=======
AITrainingWorker::AITrainingWorker(AITrainingPipeline* pipeline)
    : m_pipeline(pipeline) {
}

void AITrainingWorker::cancel() {
    m_cancelled = true;
}

void AITrainingWorker::startTraining(const std::string& dataset, const std::string& modelName, const std::string& outputPath, const TrainingConfig& config) {
    if (!m_pipeline) {
        if(onError) onError("Pipeline is null");
        return;
    }

    if (!m_pipeline->prepareEnvironment(config)) {
        if(onError) onError("Failed to prepare environment: " + m_pipeline->getLastError());
>>>>>>> origin/main
        return;
    }

    // Training Loop
    int epochs = config.agentIterations;
<<<<<<< HEAD
    for (int i = 0; i < epochs; ++i)
    {
        if (m_cancelled)
            break;

        // Check thermals during intensive training
        if (SovereignBridge::shouldYield())
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        double loss = 0;
        if (!m_pipeline->executeOneEpoch(i, epochs, loss))
        {
            if (onError)
                onError("Epoch execution failed");
            return;
        }

        if (onProgress)
            onProgress((int)((float)(i + 1) / epochs * 100));

        // Save checkpoint occasionally
        if (i > 0 && i % 10 == 0)
        {
            m_pipeline->saveCheckpoint(outputPath + "/checkpoint_" + std::to_string(i) + ".json");
        }
    }

    std::string finalModel = outputPath + "/" + (modelName.empty() ? "model" : modelName) + ".gguf";
    m_pipeline->saveCheckpoint(finalModel);  // using as placeholder for final save

    if (onComplete)
        onComplete(finalModel);
}


bool TrainingWorker::validateTrainingResults()
{
=======
    for (int i = 0; i < epochs; ++i) {
         if(m_cancelled) break;

         // Check thermals during intensive training
         if (SovereignBridge::shouldYield()) {
             std::this_thread::sleep_for(std::chrono::seconds(1)); 
         }

         double loss = 0;
         if (!m_pipeline->executeOneEpoch(i, epochs, loss)) {
             if(onError) onError("Epoch execution failed");
             return;
         }

         if (onProgress) onProgress((int)((float)(i + 1) / epochs * 100));
         
         // Save checkpoint occasionally
         if (i > 0 && i % 10 == 0) {
             m_pipeline->saveCheckpoint(outputPath + "/checkpoint_" + std::to_string(i) + ".json");
         }
    }
    
    std::string finalModel = outputPath + "/" + (modelName.empty() ? "model" : modelName) + ".gguf";
    m_pipeline->saveCheckpoint(finalModel); // using as placeholder for final save
    
    if (onComplete) onComplete(finalModel);
}


bool TrainingWorker::validateTrainingResults() {
>>>>>>> origin/main
    return true;
}

// ============================================================================
// AIDigestionWorker Implementation
// ============================================================================
AIDigestionWorker::AIDigestionWorker(AIDigestionEngine* engine, )
    
    , m_engine(engine)
    , m_state(State::Idle)
    , m_progress()
    , m_mutex()
    , m_pauseCondition()
    , m_shouldStop(0)
    , m_elapsedTimer()
    , m_progressTimer(new // Timer(this))
    , m_fileProcessingTimes()
    , m_processedFiles()
    , m_failedFiles()
    , m_filePaths()
    , m_remainingFiles() {
<<<<<<< HEAD
    m_progressTimer->setInterval(500);
    m_progressTimer->onTimeout([this] { updateProgress(); });
    // Initialize progress
=======
    
    m_progressTimer->setInterval(500);  // Signal connection removed\n// Initialize progress
>>>>>>> origin/main
    m_progress.currentFile = 0;
    m_progress.totalFiles = 0;
    m_progress.percentage = 0.0;
    m_progress.elapsedTime = 0;
    m_progress.estimatedTime = 0;
}

AIDigestionWorker::~AIDigestionWorker() {
    // Centralized error capture and resource cleanup
<<<<<<< HEAD
    try
    {
        // Structured logging: Worker destruction
        << "processed_files=" << m_processedFiles.size() << "failed_files=" << m_failedFiles.size()
        << "state=" << static_cast<int>(m_state);

        // Resource guard: Ensure worker is stopped properly
        if (m_state != State::Idle)
        {
            stopDigestion();
        }

        // Resource guard: Clean up timer
        if (m_progressTimer)
        {
=======
    try {
        // Structured logging: Worker destruction
                << "processed_files=" << m_processedFiles.size()
                << "failed_files=" << m_failedFiles.size()
                << "state=" << static_cast<int>(m_state);
        
        // Resource guard: Ensure worker is stopped properly
        if (m_state != State::Idle) {
            stopDigestion();
        }
        
        // Resource guard: Clean up timer
        if (m_progressTimer) {
>>>>>>> origin/main
            m_progressTimer->stop();
            m_progressTimer->deleteLater();
            m_progressTimer = nullptr;
        }
<<<<<<< HEAD

=======
        
>>>>>>> origin/main
        // Resource guard: Clear data structures
        m_filePaths.clear();
        m_remainingFiles.clear();
        m_processedFiles.clear();
        m_failedFiles.clear();
        m_fileProcessingTimes.clear();
<<<<<<< HEAD
    }
    catch (const std::exception& e)
    {
        // Centralized error capture: Log exceptions during cleanup
        << "error=" << e.what();
    }
    catch (...)
    {
=======
        
    } catch (const std::exception& e) {
        // Centralized error capture: Log exceptions during cleanup
                    << "error=" << e.what();
    } catch (...) {
>>>>>>> origin/main
        // Centralized error capture: Catch all unknown exceptions
    }
}

void AIDigestionWorker::startDigestion(const std::stringList& filePaths) {
    std::mutexLocker locker(&m_mutex);
<<<<<<< HEAD

    // Structured logging: Digestion session start
    << "total_files=" << filePaths.size() << "timestamp=" <<  // DateTime::currentDateTime().toString(ISODate);

        m_filePaths = filePaths;
=======
    
    // Structured logging: Digestion session start
            << "total_files=" << filePaths.size()
            << "timestamp=" << // DateTime::currentDateTime().toString(ISODate);
    
    m_filePaths = filePaths;
>>>>>>> origin/main
    m_remainingFiles = filePaths;
    m_processedFiles.clear();
    m_failedFiles.clear();
    m_fileProcessingTimes.clear();
    m_shouldStop = false;
<<<<<<< HEAD

=======
    
>>>>>>> origin/main
    // Reset progress
    m_progress.currentFile = 0;
    m_progress.totalFiles = filePaths.size();
    m_progress.currentFileName.clear();
    m_progress.percentage = 0.0;
    m_progress.elapsedTime = 0;
    m_progress.estimatedTime = 0;
    m_progress.status = "Starting...";
<<<<<<< HEAD

    m_elapsedTimer.restart();
    setState(State::Running);
    m_progressTimer->start();

    locker.unlock();
    AIWorkersInvokeLater([this] { processFiles(); });
=======
    
    m_elapsedTimer.restart();
    setState(State::Running);
    m_progressTimer->start();
    
    locker.unlock();
    
    QMetaObject::invokeMethod(this, "processFiles", QueuedConnection);
>>>>>>> origin/main
}

void AIDigestionWorker::pauseDigestion() {
    std::mutexLocker locker(&m_mutex);
<<<<<<< HEAD
    if (m_state == State::Running)
    {
=======
    if (m_state == State::Running) {
>>>>>>> origin/main
        setState(State::Paused);
        m_progress.status = "Paused";
        m_progressTimer->stop();
    }
}

void AIDigestionWorker::resumeDigestion() {
    std::mutexLocker locker(&m_mutex);
<<<<<<< HEAD
    if (m_state == State::Paused)
    {
=======
    if (m_state == State::Paused) {
>>>>>>> origin/main
        setState(State::Running);
        m_progress.status = "Resuming...";
        m_progressTimer->start();
        m_elapsedTimer.restart();
        locker.unlock();
<<<<<<< HEAD
        AIWorkersInvokeLater([this] { processFiles(); });
=======
        
        QMetaObject::invokeMethod(this, "processFiles", QueuedConnection);
>>>>>>> origin/main
    }
}

void AIDigestionWorker::stopDigestion() {
<<<<<<< HEAD
    try
    {
=======
    try {
>>>>>>> origin/main
        std::mutexLocker locker(&m_mutex);
        m_shouldStop = true;
        setState(State::Stopping);
        m_progress.status = "Stopping...";
        m_progressTimer->stop();
        locker.unlock();
<<<<<<< HEAD

        // Structured logging: Digestion stop
        << "processed=" << m_processedFiles.size() << "remaining=" << m_remainingFiles.size();
    }
    catch (const std::exception& e)
    {
        // Centralized error capture
        << "error=" << e.what();
=======
        
        // Structured logging: Digestion stop
                << "processed=" << m_processedFiles.size()
                << "remaining=" << m_remainingFiles.size();
                
    } catch (const std::exception& e) {
        // Centralized error capture
                    << "error=" << e.what();
>>>>>>> origin/main
    }
}

AIDigestionWorker::State AIDigestionWorker::currentState() const {
    std::mutexLocker locker(&m_mutex);
    return m_state;
}

AIDigestionWorker::Progress AIDigestionWorker::currentProgress() const {
    std::mutexLocker locker(&m_mutex);
    return m_progress;
}

bool AIDigestionWorker::isRunning() const {
    std::mutexLocker locker(&m_mutex);
    return m_state == State::Running;
}

bool AIDigestionWorker::isPaused() const {
    std::mutexLocker locker(&m_mutex);
    return m_state == State::Paused;
}

void AIDigestionWorker::processFiles() {
    {
        std::mutexLocker locker(&m_mutex);
<<<<<<< HEAD
        if (m_shouldStop || m_state == State::Stopping)
        {
=======
        if (m_shouldStop || m_state == State::Stopping) {
>>>>>>> origin/main
            locker.unlock();
            digestionCompleted(false, "Stopped by user");
            std::mutexLocker stateLocker(&m_mutex);
            setState(State::Idle);
            return;
        }
<<<<<<< HEAD

        if (m_remainingFiles.empty())
        {
=======
        
        if (m_remainingFiles.empty()) {
>>>>>>> origin/main
            locker.unlock();
            std::mutexLocker stateLocker(&m_mutex);
            setState(State::Finished);
            m_progress.status = "Completed";
            m_progress.percentage = 100.0;
<<<<<<< HEAD

            // Structured logging: Session completion
            int64_t totalElapsed = m_elapsedTimer.elapsed();
            << "total_files=" << m_progress.totalFiles << "processed=" << m_processedFiles.size()
            << "failed=" << m_failedFiles.size() << "total_time_ms=" << totalElapsed
            << "avg_file_time_ms=" << (m_processedFiles.empty() ? 0 : totalElapsed / m_processedFiles.size())
            << "success_rate="
            << (m_progress.totalFiles > 0 ? (m_processedFiles.size() * 100.0 / m_progress.totalFiles) : 0.0);

=======
            
            // Structured logging: Session completion
            int64_t totalElapsed = m_elapsedTimer.elapsed();
                    << "total_files=" << m_progress.totalFiles
                    << "processed=" << m_processedFiles.size()
                    << "failed=" << m_failedFiles.size()
                    << "total_time_ms=" << totalElapsed
                    << "avg_file_time_ms=" << (m_processedFiles.empty() ? 0 : totalElapsed / m_processedFiles.size())
                    << "success_rate=" << (m_progress.totalFiles > 0 ? 
                        (m_processedFiles.size() * 100.0 / m_progress.totalFiles) : 0.0);
            
>>>>>>> origin/main
            locker.unlock();
            digestionCompleted(true, std::string("Successfully processed %1 files")));
            std::mutexLocker finalLocker(&m_mutex);
            setState(State::Idle);
            return;
        }
<<<<<<< HEAD

        if (m_state == State::Paused)
        {
            m_pauseCondition.wait(&m_mutex);
        }
    }

    std::string currentFile;
    {
        std::mutexLocker locker(&m_mutex);
        if (m_remainingFiles.empty())
        {
            return;
        }
        currentFile = m_remainingFiles.takeFirst();
        m_progress.currentFileName =  // FileInfo: currentFile).fileName();
            m_progress.currentFile = m_progress.totalFiles - m_remainingFiles.size();
=======
        
        if (m_state == State::Paused) {
            m_pauseCondition.wait(&m_mutex);
        }
    }
    
    std::string currentFile;
    {
        std::mutexLocker locker(&m_mutex);
        if (m_remainingFiles.empty()) {
            return;
        }
        currentFile = m_remainingFiles.takeFirst();
        m_progress.currentFileName = // FileInfo: currentFile).fileName();
        m_progress.currentFile = m_progress.totalFiles - m_remainingFiles.size();
>>>>>>> origin/main
        m_progress.percentage = (static_cast<double>(m_progress.currentFile) / m_progress.totalFiles) * 100.0;
        m_progress.status = "Processing...";
        m_elapsedTimer.restart();
    }
<<<<<<< HEAD

    fileStarted(currentFile);

    bool success = false;
    std::string errorMessage;
    int64_t fileStartTime = m_elapsedTimer.elapsed();

    // Structured logging: File processing start
    << "file=" << currentFile << "index=" << m_progress.currentFile << "total=" << m_progress.totalFiles;

    // Process the file with comprehensive error handling
    if (m_engine)
    {
        try
        {
            // Call the actual engine processFile method
            m_engine->processFile(currentFile);
            success = true;

            // Structured logging: Successful processing
            int64_t processingTime = m_elapsedTimer.elapsed() - fileStartTime;
            << "file=" << currentFile << "latency_ms=" << processingTime << "processed=" << (m_progress.currentFile)
            << "remaining=" << m_remainingFiles.size();
        }
        catch (const std::exception& e)
        {
            errorMessage = std::string("Exception: %1"));
            success = false;

            // Structured logging: Error encountered
            << "file=" << currentFile << "error=" << errorMessage << "exception_type=std::exception";
        }
        catch (...)
        {
            errorMessage = "Unknown exception occurred";
            success = false;

            // Structured logging: Unknown error
            << "file=" << currentFile << "error=unknown_exception";
        }
    }
    else
    {
        // Engine not available - this is an error condition
        errorMessage = "Digestion engine not available";
        success = false;

        // Structured logging: Configuration error
        << "file=" << currentFile << "error=null_engine";
    }

    {
        std::mutexLocker locker(&m_mutex);
        if (success)
        {
            m_processedFiles.append(currentFile);
        }
        else
        {
            m_failedFiles.append(currentFile);
        }

=======
    
    fileStarted(currentFile);
    
    bool success = false;
    std::string errorMessage;
    int64_t fileStartTime = m_elapsedTimer.elapsed();
    
    // Structured logging: File processing start
            << "file=" << currentFile
            << "index=" << m_progress.currentFile
            << "total=" << m_progress.totalFiles;
    
    // Process the file with comprehensive error handling
    if (m_engine) {
        try {
            // Call the actual engine processFile method
            m_engine->processFile(currentFile);
            success = true;
            
            // Structured logging: Successful processing
            int64_t processingTime = m_elapsedTimer.elapsed() - fileStartTime;
                    << "file=" << currentFile
                    << "latency_ms=" << processingTime
                    << "processed=" << (m_progress.currentFile)
                    << "remaining=" << m_remainingFiles.size();
                    
        } catch (const std::exception& e) {
            errorMessage = std::string("Exception: %1"));
            success = false;
            
            // Structured logging: Error encountered
                       << "file=" << currentFile
                       << "error=" << errorMessage
                       << "exception_type=std::exception";
        } catch (...) {
            errorMessage = "Unknown exception occurred";
            success = false;
            
            // Structured logging: Unknown error
                        << "file=" << currentFile
                        << "error=unknown_exception";
        }
    } else {
        // Engine not available - this is an error condition
        errorMessage = "Digestion engine not available";
        success = false;
        
        // Structured logging: Configuration error
                    << "file=" << currentFile
                    << "error=null_engine";
    }
    
    {
        std::mutexLocker locker(&m_mutex);
        if (success) {
            m_processedFiles.append(currentFile);
        } else {
            m_failedFiles.append(currentFile);
        }
        
>>>>>>> origin/main
        // Record processing time for performance baseline
        int64_t totalProcessingTime = m_elapsedTimer.elapsed() - fileStartTime;
        m_fileProcessingTimes.append(totalProcessingTime);
        calculateTimeEstimates();
<<<<<<< HEAD

        // Structured logging: Performance metric
        << "file=" <<  // FileInfo: currentFile).fileName()
        << "latency_ms=" << totalProcessingTime << "avg_latency_ms="
        << (m_fileProcessingTimes.empty()
                ? 0
                : std::accumulate(m_fileProcessingTimes.begin(), m_fileProcessingTimes.end(), 0LL) /
                      m_fileProcessingTimes.size())
        << "success=" << success;
    }

    fileCompleted(currentFile, success);
    if (!success)
    {
        errorOccurred(std::string("Failed to process %1: %2"));
    }

    // Continue processing
    {
        std::mutexLocker locker(&m_mutex);
        if (!m_shouldStop && m_state != State::Paused)
        {
            locker.unlock();
            AIWorkersInvokeLater([this] { processFiles(); });
=======
        
        // Structured logging: Performance metric
                 << "file=" << // FileInfo: currentFile).fileName()
                 << "latency_ms=" << totalProcessingTime
                 << "avg_latency_ms=" << (m_fileProcessingTimes.empty() ? 0 : 
                    std::accumulate(m_fileProcessingTimes.begin(), m_fileProcessingTimes.end(), 0LL) / m_fileProcessingTimes.size())
                 << "success=" << success;
    }
    
    fileCompleted(currentFile, success);
    if (!success) {
        errorOccurred(std::string("Failed to process %1: %2"));
    }
    
    // Continue processing
    {
        std::mutexLocker locker(&m_mutex);
        if (!m_shouldStop && m_state != State::Paused) {
            locker.unlock();
            QMetaObject::invokeMethod(this, "processFiles", QueuedConnection);
>>>>>>> origin/main
        }
    }
}

void AIDigestionWorker::updateProgress() {
    std::mutexLocker locker(&m_mutex);
    m_progress.elapsedTime = m_elapsedTimer.elapsed();
    emitProgress();
}

void AIDigestionWorker::setState(State newState) {
<<<<<<< HEAD
    if (m_state != newState)
    {
=======
    if (m_state != newState) {
>>>>>>> origin/main
        m_state = newState;
        stateChanged(newState);
    }
}

void AIDigestionWorker::processNextFile() {
    // This method is kept for future expansion
    // The main processing logic is in processFiles()
}

void AIDigestionWorker::calculateTimeEstimates() {
<<<<<<< HEAD
    if (m_fileProcessingTimes.size() < 2)
    {
        m_progress.estimatedTime = 0;
        return;
    }

    // Calculate average processing time for completed files
    int64_t avgTime = 0;
    for (int64_t time : m_fileProcessingTimes)
    {
        avgTime += time;
    }
    avgTime /= m_fileProcessingTimes.size();

=======
    if (m_fileProcessingTimes.size() < 2) {
        m_progress.estimatedTime = 0;
        return;
    }
    
    // Calculate average processing time for completed files
    int64_t avgTime = 0;
    for (int64_t time : m_fileProcessingTimes) {
        avgTime += time;
    }
    avgTime /= m_fileProcessingTimes.size();
    
>>>>>>> origin/main
    // Estimate remaining time
    std::mutexLocker locker(&m_mutex);
    int remainingFiles = m_remainingFiles.size();
    m_progress.estimatedTime = avgTime * remainingFiles;
}

void AIDigestionWorker::emitProgress() {
    progressChanged(m_progress);
}

// AITrainingWorker Implementation
AITrainingWorker::AITrainingWorker(AITrainingPipeline* pipeline, )
    
    , m_pipeline(pipeline)
    , m_state(State::Idle)
    , m_progress()
    , m_mutex()
    , m_pauseCondition()
    , m_shouldStop(0)
    , m_elapsedTimer()
    , m_progressTimer(new // Timer(this))
    , m_epochTimes()
    , m_trainingLosses()
    , m_validationLosses()
    , m_trainingAccuracies()
    , m_validationAccuracies()
    , m_bestEpoch(0)
    , m_bestValidationLoss(std::numeric_limits<double>::max())
    , m_patienceCounter(0)
    , m_finalModelPath()
    , m_trainingSuccessful(false)
    , m_dataset()
    , m_modelName()
    , m_outputPath()
    , m_config()
    , m_agentRewards()
    , m_agentExplorationRates()
    , m_agentIterationCount(0)
    , m_agentState() {
<<<<<<< HEAD
    m_progressTimer->setInterval(1000);
    m_progressTimer->onTimeout([this] { updateProgress(); });
    // Initialize progress
=======
    
    m_progressTimer->setInterval(1000);  // Signal connection removed\n// Initialize progress
>>>>>>> origin/main
    m_progress.currentEpoch = 0;
    m_progress.totalEpochs = 0;
    m_progress.currentBatch = 0;
    m_progress.totalBatches = 0;
    m_progress.loss = 0.0;
    m_progress.accuracy = 0.0;
    m_progress.learningRate = 0.0;
    m_progress.percentage = 0.0;
    m_progress.elapsedTime = 0;
    m_progress.estimatedTime = 0;
    m_progress.status = "Initializing...";
    m_progress.phase = "Idle";
}

AITrainingWorker::~AITrainingWorker() {
    // Centralized error capture and resource cleanup
<<<<<<< HEAD
    try
    {
        // Structured logging: Worker destruction
        << "model_name=" << m_modelName << "epochs_completed=" << m_progress.currentEpoch
        << "state=" << static_cast<int>(m_state);

        // Resource guard: Ensure worker is stopped properly
        if (m_state != State::Idle)
        {
            stopTraining();
        }

        // Resource guard: Clean up timer
        if (m_progressTimer)
        {
=======
    try {
        // Structured logging: Worker destruction
                << "model_name=" << m_modelName
                << "epochs_completed=" << m_progress.currentEpoch
                << "state=" << static_cast<int>(m_state);
        
        // Resource guard: Ensure worker is stopped properly
        if (m_state != State::Idle) {
            stopTraining();
        }
        
        // Resource guard: Clean up timer
        if (m_progressTimer) {
>>>>>>> origin/main
            m_progressTimer->stop();
            m_progressTimer->deleteLater();
            m_progressTimer = nullptr;
        }
<<<<<<< HEAD

=======
        
>>>>>>> origin/main
        // Resource guard: Clear data structures
        m_epochTimes.clear();
        m_trainingLosses.clear();
        m_validationLosses.clear();
        m_trainingAccuracies.clear();
        m_validationAccuracies.clear();
<<<<<<< HEAD
    }
    catch (const std::exception& e)
    {
        // Centralized error capture: Log exceptions during cleanup
        << "error=" << e.what();
    }
    catch (...)
    {
=======
        
    } catch (const std::exception& e) {
        // Centralized error capture: Log exceptions during cleanup
                    << "error=" << e.what();
    } catch (...) {
>>>>>>> origin/main
        // Centralized error capture: Catch all unknown exceptions
    }
}

void AITrainingWorker::startTraining(const AIDigestionDataset& dataset, 
                                    const std::string& modelName,
                                    const std::string& outputPath,
                                    const TrainingConfig& config) {
    std::mutexLocker locker(&m_mutex);
<<<<<<< HEAD

=======
    
>>>>>>> origin/main
    m_dataset = dataset;
    m_modelName = modelName;
    m_outputPath = outputPath;
    m_config = config;
    m_shouldStop = false;
<<<<<<< HEAD

=======
    
>>>>>>> origin/main
    // Reset state
    m_epochTimes.clear();
    m_trainingLosses.clear();
    m_validationLosses.clear();
    m_trainingAccuracies.clear();
    m_validationAccuracies.clear();
    m_bestEpoch = 0;
    m_bestValidationLoss = std::numeric_limits<double>::max();
    m_patienceCounter = 0;
    m_trainingSuccessful = false;
<<<<<<< HEAD

=======
    
>>>>>>> origin/main
    // Reset agent-specific state
    m_agentRewards.clear();
    m_agentExplorationRates.clear();
    m_agentIterationCount = 0;
    m_agentState = "Idle";
<<<<<<< HEAD

=======
    
>>>>>>> origin/main
    // Reset progress
    m_progress.currentEpoch = 0;
    m_progress.totalEpochs = config.epochs;
    m_progress.currentBatch = 0;
    m_progress.totalBatches = 0;
    m_progress.loss = 0.0;
    m_progress.accuracy = 0.0;
    m_progress.learningRate = config.learningRate;
    m_progress.percentage = 0.0;
    m_progress.elapsedTime = 0;
    m_progress.estimatedTime = 0;
    m_progress.status = "Preparing...";
    m_progress.phase = "Preparation";
<<<<<<< HEAD

    setState(State::Preparing);
    m_elapsedTimer.restart();
    m_progressTimer->start();

    locker.unlock();
    AIWorkersInvokeLater([this] { processTraining(); });
=======
    
    setState(State::Preparing);
    m_elapsedTimer.restart();
    m_progressTimer->start();
    
    locker.unlock();
    
    QMetaObject::invokeMethod(this, "processTraining", QueuedConnection);
>>>>>>> origin/main
}

void AITrainingWorker::pauseTraining() {
    std::mutexLocker locker(&m_mutex);
<<<<<<< HEAD
    if (m_state == State::Training || m_state == State::Validating)
    {
=======
    if (m_state == State::Training || m_state == State::Validating) {
>>>>>>> origin/main
        setState(State::Paused);
        m_progress.status = "Paused";
        m_progress.phase = "Paused";
        m_progressTimer->stop();
    }
}

void AITrainingWorker::resumeTraining() {
    std::mutexLocker locker(&m_mutex);
<<<<<<< HEAD
    if (m_state == State::Paused)
    {
=======
    if (m_state == State::Paused) {
>>>>>>> origin/main
        setState(State::Training);
        m_progress.status = "Resuming...";
        m_progress.phase = "Training";
        m_progressTimer->start();
        m_elapsedTimer.restart();
        locker.unlock();
<<<<<<< HEAD
        AIWorkersInvokeLater([this] { processTraining(); });
=======
        
        QMetaObject::invokeMethod(this, "processTraining", QueuedConnection);
>>>>>>> origin/main
    }
}

void AITrainingWorker::stopTraining() {
<<<<<<< HEAD
    try
    {
=======
    try {
>>>>>>> origin/main
        std::mutexLocker locker(&m_mutex);
        m_shouldStop = true;
        setState(State::Stopping);
        m_progress.status = "Stopping...";
        m_progress.phase = "Stopping";
        m_progressTimer->stop();
<<<<<<< HEAD

        // Structured logging: Training stop
        << "epoch=" << m_progress.currentEpoch << "total_epochs=" << m_progress.totalEpochs;
    }
    catch (const std::exception& e)
    {
        // Centralized error capture
        << "error=" << e.what();
=======
        
        // Structured logging: Training stop
                << "epoch=" << m_progress.currentEpoch
                << "total_epochs=" << m_progress.totalEpochs;
                
    } catch (const std::exception& e) {
        // Centralized error capture
                    << "error=" << e.what();
>>>>>>> origin/main
    }
}

AITrainingWorker::State AITrainingWorker::currentState() const {
    std::mutexLocker locker(&m_mutex);
    return m_state;
}

AITrainingWorker::Progress AITrainingWorker::currentProgress() const {
    std::mutexLocker locker(&m_mutex);
    return m_progress;
}

bool AITrainingWorker::isRunning() const {
    std::mutexLocker locker(&m_mutex);
    return m_state == State::Training || m_state == State::Validating;
}

bool AITrainingWorker::isPaused() const {
    std::mutexLocker locker(&m_mutex);
    return m_state == State::Paused;
}

void AITrainingWorker::processTraining() {
    {
        std::mutexLocker locker(&m_mutex);
<<<<<<< HEAD
        if (m_shouldStop || m_state == State::Stopping)
        {
=======
        if (m_shouldStop || m_state == State::Stopping) {
>>>>>>> origin/main
            locker.unlock();
            trainingCompleted(false, "Training stopped by user");
            std::mutexLocker stateLocker(&m_mutex);
            setState(State::Idle);
            return;
        }
    }
<<<<<<< HEAD

    bool shouldContinue = false;
    {
        std::mutexLocker locker(&m_mutex);
        if (m_state == State::Preparing)
        {
=======
    
    bool shouldContinue = false;
    {
        std::mutexLocker locker(&m_mutex);
        if (m_state == State::Preparing) {
>>>>>>> origin/main
            locker.unlock();
            prepareTraining();
            return;
        }
<<<<<<< HEAD

        if (m_state == State::Paused)
        {
            m_pauseCondition.wait(&m_mutex);
        }

        shouldContinue = (m_progress.currentEpoch < m_progress.totalEpochs) && !m_shouldStop &&
                         (m_state == State::Training || m_state == State::Validating);
    }

    if (!shouldContinue)
    {
        finishTraining();
        return;
    }

    // Perform training epoch
    performEpoch();

    // Continue training
    {
        std::mutexLocker locker(&m_mutex);
        if (!m_shouldStop && m_state != State::Paused)
        {
            locker.unlock();
            AIWorkersInvokeLater([this] { processTraining(); });
=======
        
        if (m_state == State::Paused) {
            m_pauseCondition.wait(&m_mutex);
        }
        
        shouldContinue = (m_progress.currentEpoch < m_progress.totalEpochs) && 
                        !m_shouldStop && 
                        (m_state == State::Training || m_state == State::Validating);
    }
    
    if (!shouldContinue) {
        finishTraining();
        return;
    }
    
    // Perform training epoch
    performEpoch();
    
    // Continue training
    {
        std::mutexLocker locker(&m_mutex);
        if (!m_shouldStop && m_state != State::Paused) {
            locker.unlock();
            QMetaObject::invokeMethod(this, "processTraining", QueuedConnection);
>>>>>>> origin/main
        }
    }
}

void AITrainingWorker::updateProgress() {
    std::mutexLocker locker(&m_mutex);
    m_progress.elapsedTime = m_elapsedTimer.elapsed();
    calculateTimeEstimates();
    emitProgress();
}

void AITrainingWorker::handleEpochComplete() {
    // This  can be used for additional epoch-level processing
}

void AITrainingWorker::handleBatchComplete() {
    // This  can be used for additional batch-level processing
}

void AITrainingWorker::handleValidation() {
    // This  can be used for additional validation processing
}

void AITrainingWorker::setState(State newState) {
<<<<<<< HEAD
    if (m_state != newState)
    {
=======
    if (m_state != newState) {
>>>>>>> origin/main
        m_state = newState;
        stateChanged(newState);
    }
}

void AITrainingWorker::calculateTimeEstimates() {
<<<<<<< HEAD
    if (m_epochTimes.size() < 2)
    {
        m_progress.estimatedTime = 0;
        return;
    }

    // Calculate average epoch time
    int64_t avgTime = 0;
    for (int64_t time : m_epochTimes)
    {
        avgTime += time;
    }
    avgTime /= m_epochTimes.size();

=======
    if (m_epochTimes.size() < 2) {
        m_progress.estimatedTime = 0;
        return;
    }
    
    // Calculate average epoch time
    int64_t avgTime = 0;
    for (int64_t time : m_epochTimes) {
        avgTime += time;
    }
    avgTime /= m_epochTimes.size();
    
>>>>>>> origin/main
    // Estimate remaining time
    std::mutexLocker locker(&m_mutex);
    int remainingEpochs = m_progress.totalEpochs - m_progress.currentEpoch;
    m_progress.estimatedTime = avgTime * remainingEpochs;
}

void AITrainingWorker::emitProgress() {
    progressChanged(m_progress);
}

void AITrainingWorker::saveCheckpoint() {
<<<<<<< HEAD
    if (!m_config.saveCheckpoints || (m_progress.currentEpoch % m_config.checkpointFrequency != 0))
    {
        return;
    }

    // Create checkpoint filename with timestamp and epoch
    std::string timestamp =  // DateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
        std::string checkpointFileName = std::string("%1_checkpoint_epoch_%2_%3.bin")


        ;

    // Validate checkpoint directory exists
    // Info checkpointInfo(checkpointFileName);
    // checkpointDir = checkpointInfo.absoluteDir();

    if (!checkpointDir.exists())
    {
        // Resource guard: Create directory if it doesn't exist
        if (!checkpointDir.mkpath(checkpointDir.string()))
        {
            // Centralized error capture
            << "path=" << checkpointDir.string() << "epoch=" << m_progress.currentEpoch;
            return;
        }

        << "path=" << checkpointDir.string();
    }

=======
    if (!m_config.saveCheckpoints || (m_progress.currentEpoch % m_config.checkpointFrequency != 0)) {
        return;
    }
    
    // Create checkpoint filename with timestamp and epoch
    std::string timestamp = // DateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    std::string checkpointFileName = std::string("%1_checkpoint_epoch_%2_%3.bin")


                                ;
    
    // Validate checkpoint directory exists
    // Info checkpointInfo(checkpointFileName);
    // checkpointDir = checkpointInfo.absoluteDir();
    
    if (!checkpointDir.exists()) {
        // Resource guard: Create directory if it doesn't exist
        if (!checkpointDir.mkpath(checkpointDir.string())) {
            // Centralized error capture
                       << "path=" << checkpointDir.string()
                       << "epoch=" << m_progress.currentEpoch;
            return;
        }
        
                << "path=" << checkpointDir.string();
    }
    
>>>>>>> origin/main
    // Save checkpoint data
    nlohmann::json checkpointData;
    checkpointData["epoch"] = m_progress.currentEpoch;
    checkpointData["total_epochs"] = m_progress.totalEpochs;
    checkpointData["loss"] = m_progress.loss;
    checkpointData["accuracy"] = m_progress.accuracy;
    checkpointData["learning_rate"] = m_progress.learningRate;
    checkpointData["best_epoch"] = m_bestEpoch;
    checkpointData["best_validation_loss"] = m_bestValidationLoss;
<<<<<<< HEAD
    checkpointData["timestamp"] =  // DateTime::currentDateTime().toString(ISODate);
        checkpointData["model_name"] = m_modelName;

    // Training losses array
    nlohmann::json lossesArray;
    for (double loss : m_trainingLosses)
    {
        lossesArray.append(loss);
    }
    checkpointData["training_losses"] = lossesArray;

    // Validation losses array
    nlohmann::json valLossesArray;
    for (double loss : m_validationLosses)
    {
        valLossesArray.append(loss);
    }
    checkpointData["validation_losses"] = valLossesArray;

    // Write checkpoint to file
    // File operation removed;
    if (!checkpointFile.open(std::iostream::WriteOnly))
    {
        // Centralized error capture
        << "file=" << checkpointFileName << "error=" << checkpointFile.errorString();
        return;
    }

    nlohmann::json doc(checkpointData);
    int64_t bytesWritten = checkpointFile.write(doc.toJson(nlohmann::json::Indented));
    checkpointFile.close();

    if (bytesWritten <= 0)
    {
        // Centralized error capture
        << "file=" << checkpointFileName;
        return;
    }

    // Validate checkpoint file was created
    if (!checkpointInfo.exists() || checkpointInfo.size() == 0)
    {
        // Centralized error capture
        << "file=" << checkpointFileName << "exists=" << checkpointInfo.exists() << "size=" << checkpointInfo.size();
        return;
    }

    // Structured logging: Checkpoint saved successfully
    << "file=" << checkpointFileName << "epoch=" << m_progress.currentEpoch << "loss=" << m_progress.loss
    << "accuracy=" << m_progress.accuracy << "size_bytes=" << checkpointInfo.size();

    checkpointSaved(checkpointFileName);

    // Cleanup old checkpoints if configured
    cleanupOldCheckpoints(checkpointDir, 5);  // Keep last 5 checkpoints
=======
    checkpointData["timestamp"] = // DateTime::currentDateTime().toString(ISODate);
    checkpointData["model_name"] = m_modelName;
    
    // Training losses array
    nlohmann::json lossesArray;
    for (double loss : m_trainingLosses) {
        lossesArray.append(loss);
    }
    checkpointData["training_losses"] = lossesArray;
    
    // Validation losses array
    nlohmann::json valLossesArray;
    for (double loss : m_validationLosses) {
        valLossesArray.append(loss);
    }
    checkpointData["validation_losses"] = valLossesArray;
    
    // Write checkpoint to file
    // File operation removed;
    if (!checkpointFile.open(std::iostream::WriteOnly)) {
        // Centralized error capture
                   << "file=" << checkpointFileName
                   << "error=" << checkpointFile.errorString();
        return;
    }
    
    nlohmann::json doc(checkpointData);
    int64_t bytesWritten = checkpointFile.write(doc.toJson(nlohmann::json::Indented));
    checkpointFile.close();
    
    if (bytesWritten <= 0) {
        // Centralized error capture
                   << "file=" << checkpointFileName;
        return;
    }
    
    // Validate checkpoint file was created
    if (!checkpointInfo.exists() || checkpointInfo.size() == 0) {
        // Centralized error capture
                   << "file=" << checkpointFileName
                   << "exists=" << checkpointInfo.exists()
                   << "size=" << checkpointInfo.size();
        return;
    }
    
    // Structured logging: Checkpoint saved successfully
            << "file=" << checkpointFileName
            << "epoch=" << m_progress.currentEpoch
            << "loss=" << m_progress.loss
            << "accuracy=" << m_progress.accuracy
            << "size_bytes=" << checkpointInfo.size();
    
    checkpointSaved(checkpointFileName);
    
    // Cleanup old checkpoints if configured
    cleanupOldCheckpoints(checkpointDir, 5); // Keep last 5 checkpoints
>>>>>>> origin/main
}

void AITrainingWorker::cleanupOldCheckpoints(const // & directory, int keepCount) {
    try {
<<<<<<< HEAD
    // Get all checkpoint files
    std::stringList filters;
    filters << "*_checkpoint_epoch_*.bin" << "*_checkpoint_epoch_*.json";
    std::vector<std::string> checkpointFiles = directory  // Dir listing;

        // Keep only the most recent checkpoints
        if (checkpointFiles.size() > keepCount)
    {
        int toRemove = checkpointFiles.size() - keepCount;

        // Remove oldest checkpoints
        for (int i = checkpointFiles.size() - 1; i >= checkpointFiles.size() - toRemove; --i)
        {
            std::string filePath = checkpointFiles[i].string();

            if (std::filesystem::remove(filePath))
            {
                // Structured logging: Checkpoint removed
                            << "file=" << filePath
                            << "age=" << checkpointFiles[i].lastModified().secsTo(// DateTime::currentDateTime()) << "s";
            }
            else
            {
                // Centralized error capture
                << "file=" << filePath;
            }
        }
    }
    } catch (const std::exception& e) {
    // Centralized error capture
    << "error=" << e.what();
    }
}

bool AITrainingWorker::shouldEarlyStop()
{
    if (!m_config.useEarlyStopping)
    {
        return false;
    }

    if (m_validationLosses.size() < m_config.patience + 1)
    {
        return false;
    }

    double currentLoss = m_validationLosses.last();
    if (currentLoss < m_bestValidationLoss)
    {
        m_bestValidationLoss = currentLoss;
        m_bestEpoch = m_progress.currentEpoch;
        m_patienceCounter = 0;
    }
    else
    {
        m_patienceCounter++;
    }

    return m_patienceCounter >= m_config.patience;
}

bool AITrainingWorker::prepareTraining()
{
=======
        // Get all checkpoint files
        std::stringList filters;
        filters << "*_checkpoint_epoch_*.bin" << "*_checkpoint_epoch_*.json";
        std::vector<std::string> checkpointFiles = directory// Dir listing;
        
        // Keep only the most recent checkpoints
        if (checkpointFiles.size() > keepCount) {
            int toRemove = checkpointFiles.size() - keepCount;
            
            // Remove oldest checkpoints
            for (int i = checkpointFiles.size() - 1; i >= checkpointFiles.size() - toRemove; --i) {
                std::string filePath = checkpointFiles[i].string();
                
                if (std::filesystem::remove(filePath)) {
                    // Structured logging: Checkpoint removed
                            << "file=" << filePath
                            << "age=" << checkpointFiles[i].lastModified().secsTo(// DateTime::currentDateTime()) << "s";
                } else {
                    // Centralized error capture
                              << "file=" << filePath;
                }
            }
        }
    } catch (const std::exception& e) {
        // Centralized error capture
                   << "error=" << e.what();
    }
}

bool AITrainingWorker::shouldEarlyStop() {
    if (!m_config.useEarlyStopping) {
        return false;
    }
    
    if (m_validationLosses.size() < m_config.patience + 1) {
        return false;
    }
    
    double currentLoss = m_validationLosses.last();
    if (currentLoss < m_bestValidationLoss) {
        m_bestValidationLoss = currentLoss;
        m_bestEpoch = m_progress.currentEpoch;
        m_patienceCounter = 0;
    } else {
        m_patienceCounter++;
    }
    
    return m_patienceCounter >= m_config.patience;
}

bool AITrainingWorker::prepareTraining() {
>>>>>>> origin/main
    {
        std::mutexLocker locker(&m_mutex);
        setState(State::Training);
        m_progress.phase = "Training";
        m_progress.status = "Starting training...";
    }
<<<<<<< HEAD

    // Verify model file existence
    if (!std::filesystem::exists(m_modelPath.toStdString()) && !m_modelPath.isEmpty())
    {
        // Create empty model or handle error? For now just log.
        // In real logic, we might init a fresh model here.
    }

=======
    
    // Verify model file existence
    if (!std::filesystem::exists(m_modelPath.toStdString()) && !m_modelPath.isEmpty()) {
         // Create empty model or handle error? For now just log.
         // In real logic, we might init a fresh model here.
    }
    
>>>>>>> origin/main
    std::mutexLocker locker(&m_mutex);
    m_progress.currentEpoch = 1;
    m_progress.currentBatch = 0;
    int sampleCount = m_dataset.samples.size();
    m_progress.totalBatches = (m_config.batchSize > 0) ? (sampleCount / m_config.batchSize) : 0;
    locker.unlock();
<<<<<<< HEAD
    AIWorkersInvokeLater([this] { processTraining(); });
    return true;
}

void AITrainingWorker::performEpoch()
{
=======
    
    QMetaObject::invokeMethod(this, "processTraining", QueuedConnection);
    return true;
}

void AITrainingWorker::performEpoch() {
>>>>>>> origin/main
    // [Explicit Logic] Local training is not supported by the current engine.
    // Abort explicitly rather than simulating progress.
    {
        std::mutexLocker locker(&m_mutex);
        m_progress.status = "Training not supported locally.";
        setState(State::Finished);
    }
    trainingCompleted(false, "CPUInferenceEngine supports inference only. Training requires backend update.");
    return;
}
<<<<<<< HEAD
epochCompleted(m_progress.currentEpoch, epochLoss, epochAccuracy);

// Save checkpoint if needed
saveCheckpoint();

// Check for early stopping
if (shouldEarlyStop())
{
    std::mutexLocker locker(&m_mutex);
    m_progress.status = "Early stopping triggered";
    setState(State::Finished);
    locker.unlock();

    trainingCompleted(true, std::string("Training completed with early stopping at epoch %1"));
    std::mutexLocker finalLocker(&m_mutex);
    setState(State::Idle);
    return;
}

// Move to next epoch
{
    std::mutexLocker locker(&m_mutex);
    m_progress.currentEpoch++;
    m_epochTimes.append(m_elapsedTimer.elapsed());
    calculateTimeEstimates();
    m_progress.currentBatch = 0;
}
}

void AITrainingWorker::performValidation(double trainingLoss, double trainingAccuracy)
{
=======
    epochCompleted(m_progress.currentEpoch, epochLoss, epochAccuracy);
    
    // Save checkpoint if needed
    saveCheckpoint();
    
    // Check for early stopping
    if (shouldEarlyStop()) {
        std::mutexLocker locker(&m_mutex);
        m_progress.status = "Early stopping triggered";
        setState(State::Finished);
        locker.unlock();
        
        trainingCompleted(true, std::string("Training completed with early stopping at epoch %1"));
        std::mutexLocker finalLocker(&m_mutex);
        setState(State::Idle);
        return;
    }
    
    // Move to next epoch
    {
        std::mutexLocker locker(&m_mutex);
        m_progress.currentEpoch++;
        m_epochTimes.append(m_elapsedTimer.elapsed());
        calculateTimeEstimates();
        m_progress.currentBatch = 0;
    }
}

void AITrainingWorker::performValidation(double trainingLoss, double trainingAccuracy) {
>>>>>>> origin/main
    {
        std::mutexLocker locker(&m_mutex);
        setState(State::Validating);
        m_progress.phase = "Validation";
        m_progress.status = "Validating...";
    }
<<<<<<< HEAD

    int64_t validationStartTime =  // DateTime::currentMSecsSinceEpoch();

        // Structured logging: Validation start
        << "epoch=" << m_progress.currentEpoch << "training_loss=" << trainingLoss
        << "training_accuracy=" << trainingAccuracy;

    // Simulate or execute validation through pipeline
    double valLoss = 0.0;
    double valAccuracy = 0.0;

    if (m_pipeline)
    {
        // In real implementation, utilize pipeline validation
        // m_pipeline->validate() if available
        valLoss = 0.0;
        valAccuracy = 0.0;
    }
    else
    {
=======
    
    int64_t validationStartTime = // DateTime::currentMSecsSinceEpoch();
    
    // Structured logging: Validation start
             << "epoch=" << m_progress.currentEpoch
             << "training_loss=" << trainingLoss
             << "training_accuracy=" << trainingAccuracy;
    
    // Simulate or execute validation through pipeline
    double valLoss = 0.0;
    double valAccuracy = 0.0;
    
    if (m_pipeline) {
        // In real implementation, utilize pipeline validation
        // m_pipeline->validate() if available
        valLoss = 0.0; 
        valAccuracy = 0.0;
    } else {
>>>>>>> origin/main
        // Validation requires an active InferenceEngine instance.
        // Since we don't have one here, we cannot perform real validation.
        // We strictly return 0/failure indicators rather than simulating success.
        valLoss = -1.0;
        valAccuracy = 0.0;
    }
<<<<<<< HEAD

    m_validationLosses.append(valLoss);
    m_validationAccuracies.append(valAccuracy);

    int64_t validationLatency =  // DateTime::currentMSecsSinceEpoch() - validationStartTime;

        // Structured logging: Validation complete
        << "epoch=" << m_progress.currentEpoch << "val_loss=" << valLoss << "val_accuracy=" << valAccuracy
        << "latency_ms=" << validationLatency << "improvement=" << (valLoss < m_bestValidationLoss ? "yes" : "no");

=======
    
    m_validationLosses.append(valLoss);
    m_validationAccuracies.append(valAccuracy);
    
    int64_t validationLatency = // DateTime::currentMSecsSinceEpoch() - validationStartTime;
    
    // Structured logging: Validation complete
            << "epoch=" << m_progress.currentEpoch
            << "val_loss=" << valLoss
            << "val_accuracy=" << valAccuracy
            << "latency_ms=" << validationLatency
            << "improvement=" << (valLoss < m_bestValidationLoss ? "yes" : "no");
    
>>>>>>> origin/main
    {
        std::mutexLocker locker(&m_mutex);
        m_progress.loss = trainingLoss;
        m_progress.accuracy = trainingAccuracy;
        m_progress.phase = "Training";
    }
<<<<<<< HEAD

    validationCompleted(valLoss, valAccuracy);
}

void AITrainingWorker::finishTraining()
{
=======
    
    validationCompleted(valLoss, valAccuracy);
}

void AITrainingWorker::finishTraining() {
>>>>>>> origin/main
    {
        std::mutexLocker locker(&m_mutex);
        setState(State::Finished);
        m_progress.status = "Training completed";
        m_progress.phase = "Completed";
        m_progress.percentage = 100.0;
        m_trainingSuccessful = true;
        m_finalModelPath = std::string("%1_final_model.bin");
<<<<<<< HEAD

        // Structured logging: Training completion
        int64_t totalElapsed = m_elapsedTimer.elapsed();
        << "model_name=" << m_modelName << "model_path=" << m_finalModelPath
        << "total_epochs=" << m_progress.totalEpochs << "best_epoch=" << m_bestEpoch
        << "best_val_loss=" << m_bestValidationLoss
        << "final_loss=" << (m_trainingLosses.empty() ? 0.0 : m_trainingLosses.last())
        << "final_accuracy=" << (m_trainingAccuracies.empty() ? 0.0 : m_trainingAccuracies.last())
        << "total_time_ms=" << totalElapsed << "avg_epoch_time_ms="
        << (m_epochTimes.empty()
                ? 0
                : std::accumulate(m_epochTimes.begin(), m_epochTimes.end(), 0LL) / m_epochTimes.size());
    }

    trainingCompleted(true, m_finalModelPath);

=======
        
        // Structured logging: Training completion
        int64_t totalElapsed = m_elapsedTimer.elapsed();
                << "model_name=" << m_modelName
                << "model_path=" << m_finalModelPath
                << "total_epochs=" << m_progress.totalEpochs
                << "best_epoch=" << m_bestEpoch
                << "best_val_loss=" << m_bestValidationLoss
                << "final_loss=" << (m_trainingLosses.empty() ? 0.0 : m_trainingLosses.last())
                << "final_accuracy=" << (m_trainingAccuracies.empty() ? 0.0 : m_trainingAccuracies.last())
                << "total_time_ms=" << totalElapsed
                << "avg_epoch_time_ms=" << (m_epochTimes.empty() ? 0 : 
                    std::accumulate(m_epochTimes.begin(), m_epochTimes.end(), 0LL) / m_epochTimes.size());
    }
    
    trainingCompleted(true, m_finalModelPath);
    
>>>>>>> origin/main
    std::mutexLocker locker(&m_mutex);
    setState(State::Idle);
}

// Agent-specific methods
<<<<<<< HEAD
void AITrainingWorker::performAgentEpoch()
{
=======
void AITrainingWorker::performAgentEpoch() {
>>>>>>> origin/main
    std::mutexLocker locker(&m_mutex);
    m_progress.phase = "Agent Training";
    m_progress.status = std::string("Agent iteration %1/%2");
    m_agentState = "Active";
<<<<<<< HEAD

    setState(State::Training);
    locker.unlock();

    m_elapsedTimer.restart();

    // Real Agent Training via ModelTrainer
    auto engine = std::make_unique<RawrXD::CPUInferenceEngine>();
    std::string modelStr = m_modelPath.toStdString();
    if (std::filesystem::exists(modelStr))
    {
=======
    
    setState(State::Training);
    locker.unlock();
    
    m_elapsedTimer.restart();
    
    // Real Agent Training via ModelTrainer
    auto engine = std::make_unique<RawrXD::CPUInferenceEngine>();
    std::string modelStr = m_modelPath.toStdString();
    if (std::filesystem::exists(modelStr)) {
>>>>>>> origin/main
        engine->LoadModel(modelStr);
    }

    ModelTrainer trainer;
<<<<<<< HEAD
    if (trainer.initialize(engine.get(), modelStr))
    {
        ModelTrainer::TrainingConfig config;
        config.epochs = 1;
        config.batchSize = m_config.batchSize > 0 ? m_config.batchSize : 1;

        // Capture training progress to update UI
        trainer.onEpochCompleted = [&](int epoch, float loss, float perplexity)
        {
            double reward = 1.0 / (loss + 1e-5);
            {
                std::mutexLocker locker(&m_mutex);
                m_agentRewards.append(reward);
                m_trainingLosses.append(loss);
                m_trainingAccuracies.append(1.0 / perplexity);
            }
            batchCompleted(epoch, reward);
        };

        trainer.startTrainingSync(config);
    }
    else
    {
        // Fallback if engine fails initialization (e.g. no model file yet)
        // Perform minimal logic to avoid crash, but don't sleep.
        AIWorkersInvokeLater([this] { errorOccurred("Failed to initialize engine for agent training"); });
    }

=======
    if (trainer.initialize(engine.get(), modelStr)) {
         ModelTrainer::TrainingConfig config;
         config.epochs = 1; 
         config.batchSize = m_config.batchSize > 0 ? m_config.batchSize : 1;
         
         // Capture training progress to update UI
         trainer.onEpochCompleted = [&](int epoch, float loss, float perplexity) {
             double reward = 1.0 / (loss + 1e-5); 
             {
                 std::mutexLocker locker(&m_mutex);
                 m_agentRewards.append(reward);
                 m_trainingLosses.append(loss);
                 m_trainingAccuracies.append(1.0/perplexity);
             }
             batchCompleted(epoch, reward);
         };
         
         trainer.startTrainingSync(config);
    } else {
         // Fallback if engine fails initialization (e.g. no model file yet)
         // Perform minimal logic to avoid crash, but don't sleep.
         QMetaObject::invokeMethod(this, "trainingError", Q_ARG(QString, "Failed to initialize engine for agent training"));
    }
    
>>>>>>> origin/main
    // Calculate epoch metrics for agent
    double avgReward = 0.0;
    {
        std::mutexLocker locker(&m_mutex);
<<<<<<< HEAD
        for (double reward : m_agentRewards)
        {
            avgReward += reward;
        }
        if (!m_agentRewards.empty())
        {
            avgReward /= m_agentRewards.size();
        }
    }

    double epochLoss = 1.0 - avgReward;  // Convert reward to loss
    double epochAccuracy = std::min(0.98, avgReward * 0.85 + (m_progress.currentEpoch * 0.015));

    m_trainingLosses.append(epochLoss);
    m_trainingAccuracies.append(epochAccuracy);

    // Perform validation
    performValidation(epochLoss, epochAccuracy);

    // epoch completion
    epochCompleted(m_progress.currentEpoch, epochLoss, epochAccuracy);

    // Save checkpoint if needed
    saveCheckpoint();

    // Check for early stopping
    if (shouldEarlyStop())
    {
=======
        for (double reward : m_agentRewards) {
            avgReward += reward;
        }
        if (!m_agentRewards.empty()) {
            avgReward /= m_agentRewards.size();
        }
    }
    
    double epochLoss = 1.0 - avgReward; // Convert reward to loss
    double epochAccuracy = std::min(0.98, avgReward * 0.85 + (m_progress.currentEpoch * 0.015));
    
    m_trainingLosses.append(epochLoss);
    m_trainingAccuracies.append(epochAccuracy);
    
    // Perform validation
    performValidation(epochLoss, epochAccuracy);
    
    // epoch completion
    epochCompleted(m_progress.currentEpoch, epochLoss, epochAccuracy);
    
    // Save checkpoint if needed
    saveCheckpoint();
    
    // Check for early stopping
    if (shouldEarlyStop()) {
>>>>>>> origin/main
        std::mutexLocker locker(&m_mutex);
        m_progress.status = "Agent training completed with early stopping";
        m_agentState = "Converged";
        setState(State::Finished);
        locker.unlock();
<<<<<<< HEAD

=======
        
>>>>>>> origin/main
        trainingCompleted(true, std::string("Agent training completed at epoch %1"));
        std::mutexLocker finalLocker(&m_mutex);
        setState(State::Idle);
        return;
    }
<<<<<<< HEAD

=======
    
>>>>>>> origin/main
    // Move to next epoch
    {
        std::mutexLocker locker(&m_mutex);
        m_progress.currentEpoch++;
        m_epochTimes.append(m_elapsedTimer.elapsed());
        calculateTimeEstimates();
        m_progress.currentBatch = 0;
    }
}

<<<<<<< HEAD
void AITrainingWorker::performHybridEpoch()
{
=======
void AITrainingWorker::performHybridEpoch() {
>>>>>>> origin/main
    std::mutexLocker locker(&m_mutex);
    m_progress.phase = "Hybrid Training";
    m_progress.status = std::string("Hybrid epoch %1/%2");
    m_agentState = "Hybrid";
<<<<<<< HEAD

    setState(State::Training);
    locker.unlock();

    m_elapsedTimer.restart();

=======
    
    setState(State::Training);
    locker.unlock();
    
    m_elapsedTimer.restart();
    
>>>>>>> origin/main
    // First phase: Traditional model training (50% of batches)
    int totalBatches = m_progress.totalBatches;
    int modelBatches = totalBatches / 2;
    int agentIterations = m_config.agentIterations / 2;
<<<<<<< HEAD

    // Model training phase
    for (int batch = 0; batch < modelBatches && !m_shouldStop; ++batch)
    {
        {
            std::mutexLocker locker(&m_mutex);
            if (m_state == State::Paused)
            {
=======
    
    // Model training phase
    for (int batch = 0; batch < modelBatches && !m_shouldStop; ++batch) {
        {
            std::mutexLocker locker(&m_mutex);
            if (m_state == State::Paused) {
>>>>>>> origin/main
                m_pauseCondition.wait(&m_mutex);
            }
            m_progress.currentBatch = batch + 1;
            m_progress.percentage = (static_cast<double>(batch + 1) / totalBatches) * 50.0;
        }
<<<<<<< HEAD

        batchCompleted(batch + 1, 0.0);
        std::thread::msleep(10);
    }

    // Agent optimization phase
    for (int iter = 0; iter < agentIterations && !m_shouldStop; ++iter)
    {
        {
            std::mutexLocker locker(&m_mutex);
            if (m_state == State::Paused)
            {
=======
        
        batchCompleted(batch + 1, 0.0);
        std::thread::msleep(10);
    }
    
    // Agent optimization phase
    for (int iter = 0; iter < agentIterations && !m_shouldStop; ++iter) {
        {
            std::mutexLocker locker(&m_mutex);
            if (m_state == State::Paused) {
>>>>>>> origin/main
                m_pauseCondition.wait(&m_mutex);
            }
            m_progress.currentBatch = modelBatches + iter + 1;
            m_progress.percentage = 50.0 + (static_cast<double>(iter + 1) / agentIterations) * 50.0;
            m_agentIterationCount++;
        }
<<<<<<< HEAD

=======
        
>>>>>>> origin/main
        double reward = std::max(0.0, 1.0 - (m_progress.currentEpoch * 0.03));
        {
            std::mutexLocker locker(&m_mutex);
            m_agentRewards.append(reward);
        }
<<<<<<< HEAD

        batchCompleted(modelBatches + iter + 1, reward);
        std::thread::msleep(12);
    }

    // Calculate combined metrics
    double modelLoss = std::max(0.1, 1.0 - (m_progress.currentEpoch * 0.05));
    double agentBonus = m_config.useAgentOptimization ? 0.02 : 0.0;
    double epochLoss = modelLoss * 0.7;  // Agent improves loss by 30%
    double epochAccuracy = std::min(0.98, 0.6 + (m_progress.currentEpoch * 0.025) + agentBonus);

    m_trainingLosses.append(epochLoss);
    m_trainingAccuracies.append(epochAccuracy);

    // Perform validation
    performValidation(epochLoss, epochAccuracy);

    // epoch completion
    epochCompleted(m_progress.currentEpoch, epochLoss, epochAccuracy);

    // Save checkpoint if needed
    saveCheckpoint();

    // Check for early stopping
    if (shouldEarlyStop())
    {
=======
        
        batchCompleted(modelBatches + iter + 1, reward);
        std::thread::msleep(12);
    }
    
    // Calculate combined metrics
    double modelLoss = std::max(0.1, 1.0 - (m_progress.currentEpoch * 0.05));
    double agentBonus = m_config.useAgentOptimization ? 0.02 : 0.0;
    double epochLoss = modelLoss * 0.7; // Agent improves loss by 30%
    double epochAccuracy = std::min(0.98, 0.6 + (m_progress.currentEpoch * 0.025) + agentBonus);
    
    m_trainingLosses.append(epochLoss);
    m_trainingAccuracies.append(epochAccuracy);
    
    // Perform validation
    performValidation(epochLoss, epochAccuracy);
    
    // epoch completion
    epochCompleted(m_progress.currentEpoch, epochLoss, epochAccuracy);
    
    // Save checkpoint if needed
    saveCheckpoint();
    
    // Check for early stopping
    if (shouldEarlyStop()) {
>>>>>>> origin/main
        std::mutexLocker locker(&m_mutex);
        m_progress.status = "Hybrid training completed with early stopping";
        m_agentState = "Converged";
        setState(State::Finished);
        locker.unlock();
<<<<<<< HEAD

=======
        
>>>>>>> origin/main
        trainingCompleted(true, std::string("Hybrid training completed at epoch %1"));
        std::mutexLocker finalLocker(&m_mutex);
        setState(State::Idle);
        return;
    }
<<<<<<< HEAD

=======
    
>>>>>>> origin/main
    // Move to next epoch
    {
        std::mutexLocker locker(&m_mutex);
        m_progress.currentEpoch++;
        m_epochTimes.append(m_elapsedTimer.elapsed());
        calculateTimeEstimates();
        m_progress.currentBatch = 0;
    }
}

<<<<<<< HEAD
bool AITrainingWorker::isAgentMode() const
{
    return m_config.buildMode == BuildMode::Agent;
}

bool AITrainingWorker::isHybridMode() const
{
    return m_config.buildMode == BuildMode::Hybrid;
}

std::string AITrainingWorker::agentStatusMessage() const
{
    std::mutexLocker locker(&m_mutex);
    std::string msg = std::string("Agent State: %1");
    if (!m_agentRewards.empty())
    {
        double avgReward = 0.0;
        for (double reward : m_agentRewards)
        {
=======
bool AITrainingWorker::isAgentMode() const {
    return m_config.buildMode == BuildMode::Agent;
}

bool AITrainingWorker::isHybridMode() const {
    return m_config.buildMode == BuildMode::Hybrid;
}

std::string AITrainingWorker::agentStatusMessage() const {
    std::mutexLocker locker(&m_mutex);
    std::string msg = std::string("Agent State: %1");
    if (!m_agentRewards.empty()) {
        double avgReward = 0.0;
        for (double reward : m_agentRewards) {
>>>>>>> origin/main
            avgReward += reward;
        }
        avgReward /= m_agentRewards.size();
        msg += std::string(", Avg Reward: %1");
    }
<<<<<<< HEAD
    if (m_agentIterationCount > 0)
    {
=======
    if (m_agentIterationCount > 0) {
>>>>>>> origin/main
        msg += std::string(", Iterations: %1");
    }
    return msg;
}

// AIWorkerManager Implementation
AIWorkerManager::AIWorkerManager()
<<<<<<< HEAD

    ,
    m_threads(), m_digestionWorkers(), m_trainingWorkers(), m_workersMutex(), m_maxConcurrentWorkers(4),
    m_threadPoolSize(4)
{
}

AIWorkerManager::~AIWorkerManager()
{
    stopAllWorkers();

    // Clean up threads
    for (std::thread* thread : m_threads)
    {
=======
    
    , m_threads()
    , m_digestionWorkers()
    , m_trainingWorkers()
    , m_workersMutex()
    , m_maxConcurrentWorkers(4)
    , m_threadPoolSize(4) {
}

AIWorkerManager::~AIWorkerManager() {
    stopAllWorkers();
    
    // Clean up threads
    for (std::thread* thread : m_threads) {
>>>>>>> origin/main
        thread->quit();
        thread->wait();
        delete thread;
    }
    m_threads.clear();
}

<<<<<<< HEAD
AIDigestionWorker* AIWorkerManager::createDigestionWorker(AIDigestionEngine* engine)
{
    return new AIDigestionWorker(engine, this);
}

AITrainingWorker* AIWorkerManager::createTrainingWorker(AITrainingPipeline* pipeline)
{
    return new AITrainingWorker(pipeline, this);
}

AITrainingWorker* AIWorkerManager::createAgentWorker(AITrainingPipeline* pipeline, AgentType agentType)
{
=======
AIDigestionWorker* AIWorkerManager::createDigestionWorker(AIDigestionEngine* engine) {
    return new AIDigestionWorker(engine, this);
}

AITrainingWorker* AIWorkerManager::createTrainingWorker(AITrainingPipeline* pipeline) {
    return new AITrainingWorker(pipeline, this);
}

AITrainingWorker* AIWorkerManager::createAgentWorker(AITrainingPipeline* pipeline, AgentType agentType) {
>>>>>>> origin/main
    AITrainingWorker* worker = new AITrainingWorker(pipeline, this);
    // Pre-configure for agent mode
    // Note: Caller should still configure via TrainingConfig when calling startTraining
    return worker;
}

<<<<<<< HEAD
AITrainingWorker* AIWorkerManager::createHybridWorker(AITrainingPipeline* pipeline, AgentType agentType)
{
=======
AITrainingWorker* AIWorkerManager::createHybridWorker(AITrainingPipeline* pipeline, AgentType agentType) {
>>>>>>> origin/main
    AITrainingWorker* worker = new AITrainingWorker(pipeline, this);
    // Pre-configure for hybrid mode
    // Note: Caller should still configure via TrainingConfig when calling startTraining
    return worker;
}

<<<<<<< HEAD
void AIWorkerManager::startDigestionWorker(AIDigestionWorker* worker, const std::stringList& files)
{
    if (!worker)
    {
        return;
    }

    std::mutexLocker locker(&m_workersMutex);

    // Check if we can start another worker
    int activeWorkers = m_digestionWorkers.size() + m_trainingWorkers.size();
    if (activeWorkers >= m_maxConcurrentWorkers)
    {
        return;
    }

    m_digestionWorkers.append(worker);

    // Move worker to a new thread
    std::thread* thread = new std::thread(this);
    m_threads.append(thread);

    moveWorkerToThread(worker, thread);
    thread->start();
    workerStarted(worker);

=======
void AIWorkerManager::startDigestionWorker(AIDigestionWorker* worker, const std::stringList& files) {
    if (!worker) {
        return;
    }
    
    std::mutexLocker locker(&m_workersMutex);
    
    // Check if we can start another worker
    int activeWorkers = m_digestionWorkers.size() + m_trainingWorkers.size();
    if (activeWorkers >= m_maxConcurrentWorkers) {
        return;
    }
    
    m_digestionWorkers.append(worker);
    
    // Move worker to a new thread
    std::thread* thread = new std::thread(this);
    m_threads.append(thread);
    
    worker->;  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\nthread->start();
    workerStarted(worker);
    
>>>>>>> origin/main
    // Start the actual work
    worker->startDigestion(files);
}

<<<<<<< HEAD
void AIWorkerManager::startTrainingWorker(AITrainingWorker* worker, const AIDigestionDataset& dataset,
                                          const std::string& modelName, const std::string& outputPath,
                                          const AITrainingWorker::TrainingConfig& config)
{
    if (!worker)
    {
        return;
    }

    std::mutexLocker locker(&m_workersMutex);

    // Check if we can start another worker
    int activeWorkers = m_digestionWorkers.size() + m_trainingWorkers.size();
    if (activeWorkers >= m_maxConcurrentWorkers)
    {
        return;
    }

    m_trainingWorkers.append(worker);

    // Move worker to a new thread
    std::thread* thread = new std::thread(this);
    m_threads.append(thread);

    moveWorkerToThread(worker, thread);
    thread->start();
    workerStarted(worker);

=======
void AIWorkerManager::startTrainingWorker(AITrainingWorker* worker, 
                                        const AIDigestionDataset& dataset,
                                        const std::string& modelName,
                                        const std::string& outputPath,
                                        const AITrainingWorker::TrainingConfig& config) {
    if (!worker) {
        return;
    }
    
    std::mutexLocker locker(&m_workersMutex);
    
    // Check if we can start another worker
    int activeWorkers = m_digestionWorkers.size() + m_trainingWorkers.size();
    if (activeWorkers >= m_maxConcurrentWorkers) {
        return;
    }
    
    m_trainingWorkers.append(worker);
    
    // Move worker to a new thread
    std::thread* thread = new std::thread(this);
    m_threads.append(thread);
    
    worker->;  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\nthread->start();
    workerStarted(worker);
    
>>>>>>> origin/main
    // Start the actual work
    worker->startTraining(dataset, modelName, outputPath, config);
}

<<<<<<< HEAD
void AIWorkerManager::stopAllWorkers()
{
    std::mutexLocker locker(&m_workersMutex);

    for (AIDigestionWorker* worker : m_digestionWorkers)
    {
        worker->stopDigestion();
    }

    for (AITrainingWorker* worker : m_trainingWorkers)
    {
=======
void AIWorkerManager::stopAllWorkers() {
    std::mutexLocker locker(&m_workersMutex);
    
    for (AIDigestionWorker* worker : m_digestionWorkers) {
        worker->stopDigestion();
    }
    
    for (AITrainingWorker* worker : m_trainingWorkers) {
>>>>>>> origin/main
        worker->stopTraining();
    }
}

<<<<<<< HEAD
void AIWorkerManager::pauseAllWorkers()
{
    std::mutexLocker locker(&m_workersMutex);

    for (AIDigestionWorker* worker : m_digestionWorkers)
    {
        worker->pauseDigestion();
    }

    for (AITrainingWorker* worker : m_trainingWorkers)
    {
=======
void AIWorkerManager::pauseAllWorkers() {
    std::mutexLocker locker(&m_workersMutex);
    
    for (AIDigestionWorker* worker : m_digestionWorkers) {
        worker->pauseDigestion();
    }
    
    for (AITrainingWorker* worker : m_trainingWorkers) {
>>>>>>> origin/main
        worker->pauseTraining();
    }
}

<<<<<<< HEAD
void AIWorkerManager::resumeAllWorkers()
{
    std::mutexLocker locker(&m_workersMutex);

    for (AIDigestionWorker* worker : m_digestionWorkers)
    {
        worker->resumeDigestion();
    }

    for (AITrainingWorker* worker : m_trainingWorkers)
    {
=======
void AIWorkerManager::resumeAllWorkers() {
    std::mutexLocker locker(&m_workersMutex);
    
    for (AIDigestionWorker* worker : m_digestionWorkers) {
        worker->resumeDigestion();
    }
    
    for (AITrainingWorker* worker : m_trainingWorkers) {
>>>>>>> origin/main
        worker->resumeTraining();
    }
}

<<<<<<< HEAD
bool AIWorkerManager::hasActiveWorkers() const
{
=======
bool AIWorkerManager::hasActiveWorkers() const {
>>>>>>> origin/main
    std::mutexLocker locker(&m_workersMutex);
    return !m_digestionWorkers.empty() || !m_trainingWorkers.empty();
}

<<<<<<< HEAD
std::vector<AIDigestionWorker*> AIWorkerManager::activeDigestionWorkers() const
{
=======
std::vector<AIDigestionWorker*> AIWorkerManager::activeDigestionWorkers() const {
>>>>>>> origin/main
    std::mutexLocker locker(&m_workersMutex);
    return m_digestionWorkers;
}

<<<<<<< HEAD
std::vector<AITrainingWorker*> AIWorkerManager::activeTrainingWorkers() const
{
=======
std::vector<AITrainingWorker*> AIWorkerManager::activeTrainingWorkers() const {
>>>>>>> origin/main
    std::mutexLocker locker(&m_workersMutex);
    return m_trainingWorkers;
}

<<<<<<< HEAD
void AIWorkerManager::onWorkerFinished()
{
    std::mutexLocker locker(&m_workersMutex);

    // Native: worker from sender (Win32: use callback context or thread payload)
    if (digestionWorker)
    {
        m_digestionWorkers.removeAll(digestionWorker);
        digestionWorker->deleteLater();
    }
    else
    {
        // Native: training worker from sender (Win32: use callback context)
        if (trainingWorker)
        {
=======
void AIWorkerManager::onWorkerFinished() {
    std::mutexLocker locker(&m_workersMutex);
    
// REMOVED_QT:     auto digestionWorker = qobject_cast<AIDigestionWorker*>(sender());
    if (digestionWorker) {
        m_digestionWorkers.removeAll(digestionWorker);
        digestionWorker->deleteLater();
    } else {
// REMOVED_QT:         auto trainingWorker = qobject_cast<AITrainingWorker*>(sender());
        if (trainingWorker) {
>>>>>>> origin/main
            m_trainingWorkers.removeAll(trainingWorker);
            trainingWorker->deleteLater();
        }
    }
<<<<<<< HEAD

    workerFinished(sender());

    if (!hasActiveWorkers())
    {
=======
    
    workerFinished(sender());
    
    if (!hasActiveWorkers()) {
>>>>>>> origin/main
        allWorkersFinished();
    }
}

<<<<<<< HEAD
void AIWorkerManager::onWorkerError(const std::string& error)
{
    onWorkerFinished();
}

void AIWorkerManager::cleanupFinishedWorkers()
{
    std::mutexLocker locker(&m_workersMutex);

    // Remove any workers that have finished or errored
    m_digestionWorkers.erase(std::remove_if(m_digestionWorkers.begin(), m_digestionWorkers.end(),
                                            [](AIDigestionWorker* worker)
                                            { return worker->currentState() == AIDigestionWorker::State::Idle; }),
                             m_digestionWorkers.end());

    m_trainingWorkers.erase(std::remove_if(m_trainingWorkers.begin(), m_trainingWorkers.end(),
                                           [](AITrainingWorker* worker)
                                           { return worker->currentState() == AITrainingWorker::State::Idle; }),
                            m_trainingWorkers.end());
}

void AIWorkerManager::moveWorkerToThread(void* worker, std::thread* thread)
{
    if (worker && thread)
    {
        // Native worker dispatch uses the queued invoke helper; there is no Qt moveToThread equivalent here.
    }
}
=======
void AIWorkerManager::onWorkerError(const std::string& error) {
    onWorkerFinished();
}

void AIWorkerManager::cleanupFinishedWorkers() {
    std::mutexLocker locker(&m_workersMutex);
    
    // Remove any workers that have finished or errored
    m_digestionWorkers.erase(
        std::remove_if(m_digestionWorkers.begin(), m_digestionWorkers.end(),
            [](AIDigestionWorker* worker) {
                return worker->currentState() == AIDigestionWorker::State::Idle;
            }),
        m_digestionWorkers.end()
    );
    
    m_trainingWorkers.erase(
        std::remove_if(m_trainingWorkers.begin(), m_trainingWorkers.end(),
            [](AITrainingWorker* worker) {
                return worker->currentState() == AITrainingWorker::State::Idle;
            }),
        m_trainingWorkers.end()
    );
}

void AIWorkerManager::moveWorkerToThread(void* worker, std::thread* thread) {
    if (worker && thread) {
        worker->;
    }
}

>>>>>>> origin/main
