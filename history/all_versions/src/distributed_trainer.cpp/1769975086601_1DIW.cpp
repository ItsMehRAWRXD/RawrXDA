#include "distributed_trainer.h"
#include <iostream>
#include <thread>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <random>

DistributedTrainer::DistributedTrainer(void* parent) : m_initialized(false) {
    m_engine = std::make_unique<RawrXD::CPUInferenceEngine>();
}
DistributedTrainer::~DistributedTrainer() { Shutdown(); }

bool DistributedTrainer::Initialize(const TrainerConfig& config) {
    m_config = config;
    if (!validateConfig()) return false;
    
    statusChanged("Initializing Distributed Trainer...");
    if (!detectDevices()) return false;
    if (!initializeBackend()) return false;
    if (!setupProcessGroup()) return false;

    if (config.enableLoadBalancing) initializeLoadBalancer();
    if (config.enableFaultTolerance) initializeFaultTolerance();

    m_initialized = true;
    return true;
}

void DistributedTrainer::Shutdown() {
    if (!m_initialized) return;
    cleanupProcessGroup();
    cleanupBackend();
    m_initialized = false;
}

bool DistributedTrainer::loadModel(const std::string& path) {
    if (!m_engine) return false;
    bool result = m_engine->LoadModel(path);
    if (result) {
        statusChanged("Model loaded for training: " + path);
    } else {
        logError("Failed to load model: " + path, 404);
    }
    return result;
}

bool DistributedTrainer::performTrainingStep(const void* batchData, float* lossOut) {
    if (!m_initialized || !m_engine) return false;

    // 1. Forward Pass (Compute Logic)
    if (!forwardPass(batchData)) {
         logError("Forward pass failed", 500);
         return false;
    }

    // 2. Backward Pass (Compute Gradients)
    if (!backwardPass()) {
        logError("Backward pass failed", 501);
        return false;
    }

    // 3. Synchronization (Distributed Ops) (Real barrier check placeholder)
    if (!synchronizeGradients()) return false;

    // 4. Optimizer Step (Weight Update)
    if (!optimizerStep()) return false;
    
    // Return real calculated loss
    if (lossOut) *lossOut = m_currentLoss;
    
    // Callback
    if (trainingStepCompleted) trainingStepCompleted(m_globalStep++, m_currentLoss);
    
    return true;
}

void DistributedTrainer::logError(const std::string& msg, int code) {
    std::cerr << "[Trainer Error] " << msg << " (" << code << ")" << std::endl;
}

void DistributedTrainer::statusChanged(const std::string& status) {
    std::cout << "[Trainer Status] " << status << std::endl;
}

// ----------------------------------------------------------------------------
// Core Implementation (No longer stubs)
// ----------------------------------------------------------------------------

bool DistributedTrainer::forwardPass(const void*& batchData) {
    // Assumption: batchData is a pointer to std::vector<int32_t> representing a training sequence.
    // Task: Next Token Prediction.
    
    const std::vector<int32_t>* sequence = static_cast<const std::vector<int32_t>*>(batchData);
    if (!sequence || sequence->size() < 2) {
        // Fallback: If no data, use internal buffer or fail.
        // For robustness in this environment, return false if no data.
        return false;
    }

    // Input: All tokens except last
    // Target: Last token
    std::vector<int32_t> input(sequence->begin(), sequence->end() - 1);
    m_lastTargetToken = sequence->back();

    // Run Engine evaluation
    // This executes the real Transformer forward pass
    m_lastLogits = m_engine->Eval(input);
    
    if (m_lastLogits.empty()) return false;
    
    return true;
}

bool DistributedTrainer::backwardPass() {
    // Functional Backward Pass for Output Head (Transfer Learning)
    // dL/dz = P - Y
    
    if (m_lastLogits.empty()) return false;
    
    size_t vocabBytes = m_lastLogits.size() * sizeof(float);
    if (m_outputGradients.size() != m_lastLogits.size()) {
        m_outputGradients.resize(m_lastLogits.size());
    }

    // 1. Softmax
    // Find max for stability
    float max_val = -1e9;
    for (float v : m_lastLogits) if (v > max_val) max_val = v;
    
    float sum = 0.0f;
    std::vector<float>& probs = m_outputGradients; // Reuse buffer for probs then grads
    
    for (size_t i = 0; i < m_lastLogits.size(); ++i) {
        float p = std::exp(m_lastLogits[i] - max_val);
        probs[i] = p;
        sum += p;
    }
    
    float inv_sum = 1.0f / sum;
    for (float& p : probs) p *= inv_sum;
    
    // 2. Compute Loss (Negative Log Likelihood)
    if (m_lastTargetToken >= 0 && m_lastTargetToken < (int32_t)probs.size()) {
        m_currentLoss = -std::log(probs[m_lastTargetToken] + 1e-10f); // epsilon
    } else {
        m_currentLoss = 20.0f; // High loss for error
    }

    // 3. Compute Gradient dL/dz
    // P_i - y_i. y_i is 1.0 for target, 0.0 otherwise.
    if (m_lastTargetToken >= 0 && m_lastTargetToken < (int32_t)probs.size()) {
        probs[m_lastTargetToken] -= 1.0f;
    }

    return true;
}

bool DistributedTrainer::optimizerStep() {
    // Apply updates to the Output Head
    // W_new = W_old - lr * gradients
    // We delegate this to the engine which holds the weights
    
    m_engine->UpdateOutputWeights(m_outputGradients, m_config.learningRate);
    return true;
}

// ----------------------------------------------------------------------------
// Infrastructure (Simplified for Single-Node, capable of expansion)
// ----------------------------------------------------------------------------

bool DistributedTrainer::validateConfig() { 
    return m_config.batchSize > 0 && m_config.learningRate > 0.0f; 
}

bool DistributedTrainer::initializeBackend() { return true; }

bool DistributedTrainer::detectDevices() {
    unsigned int cores = std::thread::hardware_concurrency();
    statusChanged("Detected " + std::to_string(cores) + " CPU threads.");
    m_engine->SetThreadCount(std::max(1u, cores - 1));
    return true;
}

bool DistributedTrainer::setupProcessGroup() {
    // Check if we are Master or Worker
    if (m_config.pgConfig.worldSize > 1) {
        statusChanged("Joining Process Group as Rank " + std::to_string(m_config.pgConfig.rank));
    }
    return true;
}

// Sync Stubs (No-ops for single node)
// In a real distributed run, these would use NCCL/MPI
bool DistributedTrainer::synchronizeGradients() { return true; }
bool DistributedTrainer::allReduceGradients() { return true; }
void DistributedTrainer::compressGradients() {}
void DistributedTrainer::decompressGradients() {}

// Maintenance
void DistributedTrainer::cleanupProcessGroup() {}
void DistributedTrainer::initializeLoadBalancer() {}
void DistributedTrainer::initializeFaultTolerance() {}
void DistributedTrainer::Checkpoint(const std::string& path) {
    statusChanged("Checkpointing not fully implemented for CPU backend.");
}

// Additional Helpers
void DistributedTrainer::cleanupBackend() {}
void DistributedTrainer::redistributeWork() {}
void DistributedTrainer::updateDeviceLoads() {}
void DistributedTrainer::detectCUDADevices() {}
void DistributedTrainer::detectRealGPUs() {}
void DistributedTrainer::updateMetrics(float stepTimeMs) {}
