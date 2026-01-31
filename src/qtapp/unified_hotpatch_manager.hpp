// unified_hotpatch_manager.hpp - Coordinates all three hotpatch systems
// Provides single interface for memory, byte-level, and server hotpatching

#pragma once

#include "model_memory_hotpatch.hpp"
#include "byte_level_hotpatcher.hpp"
#include "gguf_server_hotpatch.hpp"


#include <memory>


enum class PatchLayer {
    System,
    Memory,
    Byte,
    Server
};

struct UnifiedResult {
    bool success = false;
    PatchLayer layer = PatchLayer::System;
    std::string operationName;
    std::string errorDetail;
    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::time_point::currentDateTime();
    int errorCode = 0;

    static UnifiedResult successResult(const std::string& op, PatchLayer layer = PatchLayer::System, const std::string& detail = "OK") {
        UnifiedResult r; r.success = true; r.operationName = op; r.layer = layer; r.errorDetail = detail; return r;
    }
    static UnifiedResult failureResult(const std::string& op, const std::string& detail, PatchLayer layer = PatchLayer::System, int code = -1) {
        UnifiedResult r; r.success = false; r.operationName = op; r.layer = layer; r.errorDetail = detail; r.errorCode = code; return r;
    }
};

class UnifiedHotpatchManager : public void {

public:
    explicit UnifiedHotpatchManager(void* parent = nullptr);
    ~UnifiedHotpatchManager();

    UnifiedResult initialize();
    bool isInitialized() const;
    
    ModelMemoryHotpatch* memoryHotpatcher() const;
    ByteLevelHotpatcher* byteHotpatcher() const;
    GGUFServerHotpatch* serverHotpatcher() const;
    
    UnifiedResult attachToModel(void* modelPtr, size_t modelSize, const std::string& modelPath);
    UnifiedResult detachAll();
    
    // Memory-level operations
    PatchResult applyMemoryPatch(const std::string& name, const MemoryPatch& patch);
    PatchResult scaleWeights(const std::string& tensorName, double factor);
    PatchResult bypassLayer(int layerIndex);
    
    // Byte-level operations
    UnifiedResult applyBytePatch(const std::string& name, const BytePatch& patch);
    UnifiedResult savePatchedModel(const std::string& outputPath);
    UnifiedResult patchGGUFMetadata(const std::string& key, const std::any& value);
    
    // Server-level operations
    UnifiedResult addServerHotpatch(const std::string& name, const ServerHotpatch& patch);
    UnifiedResult enableSystemPromptInjection(const std::string& prompt);
    UnifiedResult setTemperatureOverride(double temperature);
    UnifiedResult enableResponseCaching(bool enable);
    
    // Coordinated operations
    std::vector<UnifiedResult> optimizeModel();
    std::vector<UnifiedResult> applySafetyFilters();
    std::vector<UnifiedResult> boostInferenceSpeed();
    
    struct UnifiedStats {
        ModelMemoryHotpatch::MemoryPatchStats memoryStats;
        quint64 totalPatchesApplied = 0;
        quint64 totalBytesModified = 0;
        std::chrono::system_clock::time_point sessionStarted;
        std::chrono::system_clock::time_point lastCoordinatedAction;
        quint64 coordinatedActionsCompleted = 0;
    };
    
    UnifiedStats getStatistics() const;
    void resetStatistics();
    
    UnifiedResult savePreset(const std::string& name);
    UnifiedResult loadPreset(const std::string& name);
    UnifiedResult deletePreset(const std::string& name);
    std::vector<std::string> listPresets() const;
    
    UnifiedResult exportConfiguration(const std::string& filePath);
    UnifiedResult importConfiguration(const std::string& filePath);

    void initialized();
    void modelAttached(const std::string& modelPath, size_t modelSize);
    void modelDetached();
    void patchApplied(const std::string& name, PatchLayer layer);
    void optimizationComplete(const std::string& type, int improvementPercent);
    void errorOccurred(const UnifiedResult& error);

public:
    void setMemoryHotpatchEnabled(bool enabled);
    void setByteHotpatchEnabled(bool enabled);
    void setServerHotpatchEnabled(bool enabled);
    void enableAllLayers();
    void disableAllLayers();
    void resetAllLayers();

private:
    std::unique_ptr<ModelMemoryHotpatch> m_memoryHotpatch;
    std::unique_ptr<ByteLevelHotpatcher> m_byteHotpatch;
    std::unique_ptr<GGUFServerHotpatch> m_serverHotpatch;
    
    bool m_initialized = false;
    std::string m_currentModelPath;
    void* m_currentModelPtr = nullptr;
    size_t m_currentModelSize = 0;
    
    bool m_memoryEnabled = true;
    bool m_byteEnabled = true;
    bool m_serverEnabled = true;
    
    UnifiedStats m_stats;
    std::chrono::system_clock::time_point m_sessionStart;
    std::unordered_map<std::string, void*> m_presets;
    mutable std::mutex m_mutex;
    
    void connectSignals();
    void updateStatistics(const UnifiedResult& result);
    std::vector<UnifiedResult> logCoordinatedResults(const std::string& operation, const std::vector<UnifiedResult>& results);
};

