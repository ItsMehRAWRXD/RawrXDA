// ═════════════════════════════════════════════════════════════════════════════
// Win32_InferenceEngine_Integration.hpp - Usage Examples and Integration Guide
// Pure Win32 Replacement for Qt-based InferenceEngine
// ═════════════════════════════════════════════════════════════════════════════

#pragma once

#ifndef RAWRXD_WIN32_INFERENCE_ENGINE_INTEGRATION_HPP
#define RAWRXD_WIN32_INFERENCE_ENGINE_INTEGRATION_HPP

#include "Win32_InferenceEngine.hpp"
#include "agent_kernel_main.hpp"

namespace RawrXD {
namespace Win32 {

// ═════════════════════════════════════════════════════════════════════════════
// Inference Engine Manager - Singleton wrapper for global access
// ═════════════════════════════════════════════════════════════════════════════

class InferenceEngineManager {
public:
    static InferenceEngineManager& GetInstance() {
        static InferenceEngineManager instance;
        return instance;
    }
    
    InferenceEngine& GetEngine() { return engine_; }
    
    // High-level API
    bool Initialize(const String& modelPath, const String& tokenizerPath) {
        if (!engine_.LoadModel(modelPath, tokenizerPath)) {
            return false;
        }
        engine_.Start();
        return true;
    }
    
    String InferAsync(const String& prompt, int maxTokens = 256, float temperature = 0.8f) {
        return engine_.QueueInferenceRequest(prompt, maxTokens, temperature);
    }
    
    String InferSync(const String& prompt, int maxTokens = 256, float temperature = 0.8f) {
        return engine_.Infer(prompt, maxTokens, temperature);
    }
    
    bool IsReady() const {
        return engine_.IsModelLoaded();
    }
    
    HealthStatus GetStatus() const {
        return engine_.GetHealthStatus();
    }
    
    void Shutdown() {
        engine_.Shutdown();
    }
    
private:
    InferenceEngineManager() = default;
    ~InferenceEngineManager() { Shutdown(); }
    
    InferenceEngine engine_;
};

// ═════════════════════════════════════════════════════════════════════════════
// Event Handler - Aggregates all engine callbacks into single interface
// ═════════════════════════════════════════════════════════════════════════════

class InferenceEventHandler {
public:
    using EventCallback = std::function<void(const String& event, const String& data)>;
    
    InferenceEventHandler(EventCallback handler = nullptr) : handler_(handler) {}
    
    void AttachToEngine(InferenceEngine& engine) {
        engine.SetOnModelLoaded([this]() {
            OnModelLoaded();
        });
        
        engine.SetOnModelLoadFailed([this](const String& reason) {
            OnModelLoadFailed(reason);
        });
        
        engine.SetOnInferenceProgress([this](int current, int total) {
            OnInferenceProgress(current, total);
        });
        
        engine.SetOnInferenceComplete([this](const InferenceResult& result) {
            OnInferenceComplete(result);
        });
        
        engine.SetOnError([this](InferenceErrorCode code, const String& msg) {
            OnError(code, msg);
        });
        
        engine.SetOnHealthStatusChanged([this](const HealthStatus& status) {
            OnHealthStatusChanged(status);
        });
    }
    
    void SetEventHandler(EventCallback handler) {
        handler_ = handler;
    }
    
private:
    void EmitEvent(const String& eventType, const String& data) {
        if (handler_) {
            handler_(eventType, data);
        }
    }
    
    void OnModelLoaded() {
        EmitEvent(L"ModelLoaded", L"");
    }
    
    void OnModelLoadFailed(const String& reason) {
        EmitEvent(L"ModelLoadFailed", reason);
    }
    
    void OnInferenceProgress(int current, int total) {
        EmitEvent(L"InferenceProgress", std::to_wstring(current) + L"/" + std::to_wstring(total));
    }
    
    void OnInferenceComplete(const InferenceResult& result) {
        EmitEvent(L"InferenceComplete", L"ID:" + result.requestId + L" Latency:" + 
                  std::to_wstring(static_cast<int>(result.latencyMs)) + L"ms");
    }
    
    void OnError(InferenceErrorCode code, const String& msg) {
        EmitEvent(L"Error", L"Code:" + std::to_wstring(static_cast<int>(code)) + L" Msg:" + msg);
    }
    
    void OnHealthStatusChanged(const HealthStatus& status) {
        EmitEvent(L"HealthStatusChanged", 
                  L"Ready:" + std::wstring(status.inferenceReady ? L"Y" : L"N") +
                  L" Pending:" + std::to_wstring(status.pendingRequests) +
                  L" AvgLatency:" + std::to_wstring(static_cast<int>(status.avgLatencyMs)) + L"ms");
    }
    
    EventCallback handler_;
};

// ═════════════════════════════════════════════════════════════════════════════
// Result Aggregator - Collects async results for batch processing
// ═════════════════════════════════════════════════════════════════════════════

class InferenceResultAggregator {
public:
    void AttachToEngine(InferenceEngine& engine) {
        engine.SetOnResultReady([this](const String& requestId, const String& result) {
            std::lock_guard<std::mutex> lock(mutex_);
            results_[requestId] = result;
            completedCount_++;
        });
    }
    
    Optional<String> GetResult(const String& requestId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = results_.find(requestId);
        if (it != results_.end()) {
            String result = it->second;
            results_.erase(it);
            return result;
        }
        return std::nullopt;
    }
    
    bool HasResult(const String& requestId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return results_.find(requestId) != results_.end();
    }
    
    size_t GetCompletedCount() const {
        return completedCount_.load();
    }
    
    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        results_.clear();
    }
    
private:
    mutable std::mutex mutex_;
    Map<String, String> results_;
    std::atomic<size_t> completedCount_{0};
};

// ═════════════════════════════════════════════════════════════════════════════
// Usage Examples
// ═════════════════════════════════════════════════════════════════════════════

class InferenceExamples {
public:
    // Example 1: Basic synchronous usage
    static void SynchronousInference() {
        auto& manager = InferenceEngineManager::GetInstance();
        
        if (!manager.Initialize(L"D:\\model.bin", L"D:\\tokenizer.bin")) {
            // Handle error
            return;
        }
        
        String result = manager.InferSync(L"What is AI?", 256, 0.8f);
        // Use result
        
        manager.Shutdown();
    }
    
    // Example 2: Asynchronous inference with callbacks
    static void AsynchronousInference() {
        auto& engine = InferenceEngineManager::GetInstance().GetEngine();
        InferenceResultAggregator aggregator;
        aggregator.AttachToEngine(engine);
        
        // Queue multiple requests
        String reqId1 = engine.QueueInferenceRequest(L"Request 1", 100);
        String reqId2 = engine.QueueInferenceRequest(L"Request 2", 100);
        String reqId3 = engine.QueueInferenceRequest(L"Request 3", 100);
        
        // Later, check for results
        while (!aggregator.HasResult(reqId1)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        if (auto result = aggregator.GetResult(reqId1)) {
            // Process result
        }
    }
    
    // Example 3: Event-driven architecture
    static void EventDrivenInference() {
        auto& engine = InferenceEngineManager::GetInstance().GetEngine();
        InferenceEventHandler handler;
        
        handler.SetEventHandler([](const String& eventType, const String& data) {
            if (eventType == L"InferenceComplete") {
                // Handle completion
            } else if (eventType == L"Error") {
                // Handle error
            }
        });
        
        handler.AttachToEngine(engine);
    }
    
    // Example 4: Health monitoring
    static void HealthMonitoring() {
        auto& engine = InferenceEngineManager::GetInstance().GetEngine();
        
        engine.SetOnHealthStatusChanged([](const HealthStatus& status) {
            if (!status.inferenceReady) {
                // Handle engine not ready
            }
            if (status.pendingRequests > 100) {
                // Handle queue buildup
            }
        });
    }
    
    // Example 5: Error handling
    static void ErrorHandling() {
        auto& engine = InferenceEngineManager::GetInstance().GetEngine();
        
        engine.SetOnError([](InferenceErrorCode code, const String& msg) {
            switch (code) {
                case InferenceErrorCode::MODEL_LOAD_FAILED:
                    // Handle model load error
                    break;
                case InferenceErrorCode::INSUFFICIENT_MEMORY:
                    // Handle memory error
                    break;
                case InferenceErrorCode::REQUEST_TIMEOUT:
                    // Handle timeout
                    break;
                default:
                    // Handle other errors
                    break;
            }
        });
    }
};

} // namespace Win32
} // namespace RawrXD

#endif // RAWRXD_WIN32_INFERENCE_ENGINE_INTEGRATION_HPP
