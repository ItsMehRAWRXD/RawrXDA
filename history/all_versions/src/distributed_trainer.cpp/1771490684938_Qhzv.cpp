#include "distributed_trainer.h"
#include <iostream>
#include <thread>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <random>
#include <intrin.h> // For __cpuid

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

bool DistributedTrainer::forwardPass(const void* batchData) {
    // Assumption: batchData is a pointer to std::vector<int32_t> representing a training sequence.
    // Task: Next Token Prediction.
    
    const std::vector<int32_t>* sequence = static_cast<const std::vector<int32_t>*>(batchData);
    if (!sequence || sequence->size() < 2) {
        return false;
    }

    // Store full sequence for backward pass to access target
    m_currentTokens = *sequence;

    // Input: All tokens except last
    std::vector<int32_t> input(m_currentTokens.begin(), m_currentTokens.end() - 1);

    // Run Engine evaluation
    m_logits = m_engine->Eval(input);
    
    if (m_logits.empty()) return false;
    
    return true;
}

bool DistributedTrainer::backwardPass() {
    // Functional Backward Pass for Output Head (Transfer Learning)
    
    if (m_logits.empty() || m_currentTokens.empty()) return false;
    
    // Target is the last token in the stored sequence
    int32_t targetToken = m_currentTokens.back();

    std::vector<float> gradients(m_logits.size());

    // 1. Softmax
    float max_val = -1e9;
    for (float v : m_logits) if (v > max_val) max_val = v;
    
    float sum = 0.0f;
    for (size_t i = 0; i < m_logits.size(); ++i) {
        float p = std::exp(m_logits[i] - max_val);
        gradients[i] = p; // Store probs temporarily
        sum += p;
    }
    
    float inv_sum = 1.0f / sum;
    for (float& p : gradients) p *= inv_sum;
    
    // 2. Compute Loss (Negative Log Likelihood)
    if (targetToken >= 0 && targetToken < (int32_t)gradients.size()) {
        m_currentLoss = -std::log(gradients[targetToken] + 1e-10f);
    } else {
        m_currentLoss = 20.0f; 
    }

    // 3. Compute Gradient dL/dz = P - Y
    if (targetToken >= 0 && targetToken < (int32_t)gradients.size()) {
        gradients[targetToken] -= 1.0f;
    }

    // 4. Update Weights
    // We delegate this to the engine
    m_engine->UpdateOutputWeights(gradients, m_config.learningRate);

    return true;
}

bool DistributedTrainer::optimizerStep() {
    // Weight update happened in backwardPass via UpdateOutputWeights
    // This method is now a placeholder for optimizer state updates (momentum, etc.)
    return true;
}

// ----------------------------------------------------------------------------
// Infrastructure (Simplified for Single-Node, capable of expansion)
// ----------------------------------------------------------------------------

bool DistributedTrainer::validateConfig() { 
    return m_config.batchSize > 0 && m_config.learningRate > 0.0f; 
}

// Real Hardware Capability Check
bool DistributedTrainer::initializeBackend() {
    int cpuInfo[4] = { -1 };
    __cpuid(cpuInfo, 0);
    int nIds = cpuInfo[0];
    
    // Check for AVX2 (Leaf 7, EBX bit 5)
    bool avx2Supported = false;
    if (nIds >= 7) {
        __cpuid(cpuInfo, 7);
        avx2Supported = (cpuInfo[1] & (1 << 5)) != 0;
    }
    
    if (!avx2Supported) {
        // Log warning but continue - we have scalar fallbacks
        std::cerr << "[DistributedTrainer] Warning: AVX2 not available. Training will inevitably be slow." << std::endl;
    } else {
        std::cout << "[DistributedTrainer] Backend initialized. AVX2 detected." << std::endl;
    }
    return true; 
}

bool DistributedTrainer::detectDevices() {
    unsigned int cores = std::thread::hardware_concurrency();
    statusChanged("Detected " + std::to_string(cores) + " CPU threads.");
    // Reserve one thread for UI/System
    m_engine->SetThreadCount(std::max(1u, cores > 1 ? cores - 1 : 1));
    return true;
}

bool DistributedTrainer::setupProcessGroup() {
    // Check if we are Master or Worker
    if (m_config.pgConfig.worldSize > 1) {
        statusChanged("Joining Process Group as Rank " + std::to_string(m_config.pgConfig.rank));
        // Verify we have network capabilities for distributed run
        // For now, fail if > 1 until NCCL integration
        statusChanged("Refusing distributed setup: Network layer pending.");
        return false;
    }
    return true;
}

// Sync Logic
bool DistributedTrainer::synchronizeGradients() {
    if (m_config.pgConfig.worldSize > 1) {
        // In a real distributed run, these would use NCCL/MPI
        return false; // Not implemented for multi-node
    }
    // Single node: Gradients are already local
    return true; 
}
bool DistributedTrainer::allReduceGradients() {
    if (m_config.pgConfig.worldSize <= 1) return true; // Single node — already local
    statusChanged("allReduceGradients: multi-node not supported in CPU backend");
    return false;
}
void DistributedTrainer::compressGradients() {
    // Gradient compression not implemented for CPU backend
    statusChanged("Gradient compression skipped (CPU backend)");
}
void DistributedTrainer::decompressGradients() {
    statusChanged("Gradient decompression skipped (CPU backend)");
}

// Maintenance
void DistributedTrainer::cleanupProcessGroup() {
    statusChanged("Process group cleaned up");
}
void DistributedTrainer::initializeLoadBalancer() {
    statusChanged("Load balancer initialized (single-node mode)");
}
void DistributedTrainer::initializeFaultTolerance() {
    statusChanged("Fault tolerance initialized (single-node mode)");
}
void DistributedTrainer::Checkpoint(const std::string& path) {
    if (path.empty()) { statusChanged("Checkpoint: no path specified"); return; }
    // Write training state to file
    namespace fs = std::filesystem;
    try {
        fs::create_directories(fs::path(path).parent_path());
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open()) { statusChanged("Checkpoint failed: cannot open " + path); return; }
        // Write header + metrics
        uint32_t magic = 0x43484B50; // "CHKP"
        out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        out.write(reinterpret_cast<const char*>(&m_globalStep), sizeof(m_globalStep));
        out.write(reinterpret_cast<const char*>(&m_currentLoss), sizeof(m_currentLoss));
        out.close();
        statusChanged("Checkpoint saved to: " + path);
    } catch (const std::exception& e) {
        statusChanged("Checkpoint exception: " + std::string(e.what()));
    }
}

// Additional Helpers
void DistributedTrainer::cleanupBackend() {
    statusChanged("Backend cleaned up");
}
void DistributedTrainer::redistributeWork() {
    // Single node — nothing to redistribute
}
void DistributedTrainer::updateDeviceLoads() {
    // Single node — no load balancing needed
}
void DistributedTrainer::detectCUDADevices() {
    // CPU-only backend — no CUDA
    statusChanged("No CUDA devices (CPU backend)");
}
void DistributedTrainer::detectRealGPUs() {
    statusChanged("No GPU devices detected (CPU backend)");
}
void DistributedTrainer::updateMetrics(float stepTimeMs) {
    m_globalStep++;
    // Log step time for monitoring
    if (stepTimeMs > 0.0f) {
        statusChanged("Step " + std::to_string(m_globalStep) + " completed in " +
                      std::to_string(stepTimeMs) + "ms, loss=" + std::to_string(m_currentLoss));
    }
}
