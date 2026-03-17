#pragma once
#include "cpu_inference_engine.h"
#include <vector>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <iostream>
#include <future>

namespace RawrXD {

// Multi-Engine System for 800B Model Support
class MultiEngineSystem {
private:
    std::vector<std::unique_ptr<CPUInference::CPUInferenceEngine>> m_engines;
    std::vector<std::string> m_modelPaths;
    std::atomic<bool> m_running{false};
    std::thread m_coordinationThread;
    
    // 5-Drive Setup Configuration
    struct DriveConfig {
        std::string drivePath;
        size_t availableSpace;
        bool isActive;
    };
    std::vector<DriveConfig> m_drives;
    
public:
    MultiEngineSystem() {
        // Initialize 5-drive setup
        m_drives = {
            {"C:\\models", 500ULL * 1024 * 1024 * 1024, true},  // 500GB
            {"D:\\models", 1000ULL * 1024 * 1024 * 1024, true}, // 1TB
            {"E:\\models", 2000ULL * 1024 * 1024 * 1024, true}, // 2TB
            {"F:\\models", 1500ULL * 1024 * 1024 * 1024, true}, // 1.5TB
            {"G:\\models", 800ULL * 1024 * 1024 * 1024, true}   // 800GB
        };
    }
    
    ~MultiEngineSystem() {
        Stop();
    }
    
    // Load 800B model across multiple engines distributed on 5 drives
    bool Load800BModel(const std::string& modelFolderName) {
        std::cout << "[MultiEngine] Loading 800B model (distributed shards)..." << std::endl;
        
        m_engines.clear();
        m_modelPaths.clear();
        
        // In a real 800B setup, we expect shards to be distributed across the 5 high-speed drives
        // Drive 1: Shards 0-1
        // Drive 2: Shark 2-3
        // Drive 3: Shards 4-5
        // Drive 4: Shards 6-7
        // Drive 5: Reserved for KV Cache / Context expansion
        
        int shardsLoaded = 0;
        for (int i = 0; i < 8; ++i) {
            std::string shardName = "shard_" + std::to_string(i) + ".gguf";
            std::string foundPath = "";
            bool found = false;

            // Search across all configured drives for this shard
            for (const auto& drive : m_drives) {
                if (!drive.isActive) continue;
                
                std::string fullPath = drive.drivePath + "\\" + modelFolderName + "\\" + shardName;
                if (std::filesystem::exists(fullPath)) {
                    foundPath = fullPath;
                    found = true;
                    break;
                }
            }

            if (found) {
                auto engine = std::make_unique<CPUInference::CPUInferenceEngine>();
                if (engine->LoadModel(foundPath)) {
                    m_engines.push_back(std::move(engine));
                    m_modelPaths.push_back(foundPath);
                    std::cout << "[MultiEngine] Shard " << i << " loaded from " << foundPath << std::endl;
                    shardsLoaded++;
                } else {
                    std::cerr << "[MultiEngine] Failed to load shard " << i << " from " << foundPath << std::endl;
                }
            } else {
                std::cerr << "[MultiEngine] Shard " << i << " (" << shardName << ") not found on any drive!" << std::endl;
            }
        }

        if (shardsLoaded == 8) {
            std::cout << "[MultiEngine] SUCCESS: 800B Model fully distributed and loaded across 5 drives." << std::endl;
            StartCoordination();
            return true;
        }

        std::cerr << "[MultiEngine] FAILURE: Only " << shardsLoaded << "/8 shards loaded." << std::endl;
        return false;
    }
    
    // Start engine coordination for parallel inference
    void StartCoordination() {
        m_running = true;
        m_coordinationThread = std::thread([this]() {
            while (m_running) {
                // Monitor engine health and redistribute load
                for (size_t i = 0; i < m_engines.size(); ++i) {
                    auto& engine = m_engines[i];
                    size_t memoryUsage = engine->GetMemoryUsage();
                    
                    // If engine is overloaded, redistribute
                    if (memoryUsage > 8ULL * 1024 * 1024 * 1024) { // 8GB threshold
                        RedistributeLoad(i);
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        });
    }
    
    void Stop() {
        m_running = false;
        if (m_coordinationThread.joinable()) {
            m_coordinationThread.join();
        }
    }
    
    // Parallel inference across multiple engines
    std::string GenerateParallel(const std::vector<int32_t>& inputTokens, int maxTokens) {
        if (m_engines.empty()) return "[Error: No engines loaded]";
        
        std::vector<std::future<std::vector<float>>> futures;
        
        // Split input across engines
        size_t tokensPerEngine = inputTokens.size() / m_engines.size();
        
        for (size_t i = 0; i < m_engines.size(); ++i) {
            size_t start = i * tokensPerEngine;
            size_t end = (i == m_engines.size() - 1) ? inputTokens.size() : (i + 1) * tokensPerEngine;
            
            std::vector<int32_t> engineTokens(inputTokens.begin() + start, inputTokens.begin() + end);
            
            futures.push_back(std::async(std::launch::async, [this, i, engineTokens, maxTokens]() {
                return m_engines[i]->Generate(engineTokens, maxTokens / m_engines.size());
            }));
        }
        
        // Collect results
        std::vector<float> allLogits;
        for (auto& future : futures) {
            auto result = future.get();
            allLogits.insert(allLogits.end(), result.begin(), result.end());
        }
        
        // Convert logits to tokens (simplified)
        std::vector<int32_t> tokens;
        for (size_t i = 0; i < allLogits.size(); i += m_engines[0]->GetVocabSize()) {
            if (i < allLogits.size()) {
                // Simple argmax for demonstration
                float maxVal = -1e9;
                int32_t bestToken = 0;
                for (int j = 0; j < m_engines[0]->GetVocabSize() && i + j < allLogits.size(); ++j) {
                    if (allLogits[i + j] > maxVal) {
                        maxVal = allLogits[i + j];
                        bestToken = j;
                    }
                }
                tokens.push_back(bestToken);
            }
        }
        
        return m_engines[0]->Detokenize(tokens);
    }
    
    // Get drive information for model distribution
    std::vector<DriveConfig> GetDriveInfo() const {
        return m_drives;
    }
    
    // Distribute model parts across drives
    bool DistributeModel(const std::string& modelName) {
        std::cout << "[MultiEngine] Distributing model " << modelName << " across 5 drives" << std::endl;
        
        for (size_t i = 0; i < m_drives.size(); ++i) {
            std::string targetPath = m_drives[i].drivePath + "\\" + modelName + "_part_" + std::to_string(i);
            std::cout << "  Drive " << i << ": " << targetPath << std::endl;
            
            // In a real implementation, this would copy model parts
            // For now, we just simulate the distribution
        }
        
        return true;
    }
    
private:
    void RedistributeLoad(size_t overloadedEngine) {
        std::cout << "[MultiEngine] Redistributing load from engine " << overloadedEngine << std::endl;
        
        // Simple load balancing: move some tokens to the least loaded engine
        size_t leastLoaded = FindLeastLoadedEngine();
        if (leastLoaded != overloadedEngine) {
            // In a real implementation, this would involve KV cache transfer
            // For now, we just log the redistribution
            std::cout << "[MultiEngine] Moving load from engine " << overloadedEngine 
                      << " to engine " << leastLoaded << std::endl;
        }
    }
    
    size_t FindLeastLoadedEngine() {
        size_t leastLoaded = 0;
        size_t minMemory = m_engines[0]->GetMemoryUsage();
        
        for (size_t i = 1; i < m_engines.size(); ++i) {
            size_t memory = m_engines[i]->GetMemoryUsage();
            if (memory < minMemory) {
                minMemory = memory;
                leastLoaded = i;
            }
        }
        
        return leastLoaded;
    }
};

} // namespace RawrXD