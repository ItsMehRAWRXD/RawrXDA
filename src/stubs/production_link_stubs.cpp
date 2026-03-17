// ================================================
// RawrXD Production Link Stubs
// Temporary implementations to resolve linker errors
// Replace with real implementations as features mature
// ================================================
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <array>
#include <windows.h>
#include "../win32app/Win32IDE.h"
#include "../../include/agentic_autonomous_config.h"
#include "../../include/feature_registry.h"

// ModelRegistry, UniversalModelRouter, CheckpointManager, ProjectContext, UI stubs:
// Provided by real sources (model_registry.cpp, universal_model_router, etc.) when built.
#if defined(RAWRXD_NEED_PRODUCTION_STUBS)
// ================================================
// Model Registry / Router / Checkpoint / UI stubs
// ================================================
struct ModelVersion {
    std::string name;
    std::string version;
    int id;
};

class ModelRegistry {
private:
    std::vector<ModelVersion> m_models;
    int m_nextId = 1;
    
public:
    ModelRegistry(void*) {
        // Initialize with some default models
        m_models.push_back({"GPT-2", "1.0.0", m_nextId++});
        m_models.push_back({"LLaMA", "2.0.0", m_nextId++});
        m_models.push_back({"CodeLlama", "7b", m_nextId++});
    }
    ~ModelRegistry() = default;
    
    void initialize() {
        // Load models from disk or registry
    }
    
    std::vector<ModelVersion> getAllModels() const { 
        return m_models; 
    }
    
    bool setActiveModel(int id) { 
        for (const auto& model : m_models) {
            if (model.id == id) {
                return true;
            }
        }
        return false;
    }
    
    bool registerModel(const std::string& name, const std::string& version) {
        m_models.push_back({name, version, m_nextId++});
        return true;
    }
    
    bool unregisterModel(int id) {
        auto it = std::find_if(m_models.begin(), m_models.end(), 
            [id](const ModelVersion& m) { return m.id == id; });
        if (it != m_models.end()) {
            m_models.erase(it);
            return true;
        }
        return false;
    }
};

// ================================================
// Universal Model Router Stubs
// ================================================
namespace RawrXD { class ProjectContext; }

class UniversalModelRouter {
private:
    std::vector<std::string> m_backends;
    std::string m_activeBackend;
    
public:
    UniversalModelRouter() : m_activeBackend("local") {
        m_backends = {"local", "ollama", "openai", "anthropic"};
    }
    ~UniversalModelRouter() = default;
    
    void initializeLocalEngine(const std::string& modelPath) {
        m_activeBackend = "local";
        // Initialize local inference engine
    }
    
    void routeRequest(const std::string& prompt, const std::string& context, 
                      const RawrXD::ProjectContext& projectCtx,
                      std::function<void(const std::string&, bool)> callback) {
        // Route to appropriate backend based on context and availability
        if (m_activeBackend == "local") {
            // Use local inference
            std::string response = "Local inference response for: " + prompt.substr(0, 50);
            callback(response, true);
        } else if (m_activeBackend == "ollama") {
            // Use Ollama API
            std::string response = "Ollama response for: " + prompt.substr(0, 50);
            callback(response, true);
        } else {
            callback("Backend not available", false);
        }
    }
    
    std::vector<std::string> getAvailableBackends() const { 
        return m_backends; 
    }
    
    bool setActiveBackend(const std::string& backend) {
        if (std::find(m_backends.begin(), m_backends.end(), backend) != m_backends.end()) {
            m_activeBackend = backend;
            return true;
        }
        return false;
    }
    
    std::string getActiveBackend() const {
        return m_activeBackend;
    }
};

// ================================================
// Checkpoint Manager Stubs
// ================================================
class CheckpointManager {
private:
    std::string m_basePath;
    bool m_initialized;
    
public:
    CheckpointManager(void*) : m_initialized(false) {}
    ~CheckpointManager() = default;
    
    bool initialize(const std::string& basePath, int maxCheckpoints) { 
        m_basePath = basePath;
        m_initialized = true;
        // Create directory structure
        CreateDirectoryA(basePath.c_str(), nullptr);
        return true; 
    }
    
    bool isInitialized() const { return m_initialized; }
    
    std::string saveCheckpoint(const CheckpointMetadata& metadata, 
                              const CheckpointState& state, 
                              CompressionLevel level) { 
        if (!m_initialized) return "";
        
        std::string checkpointId = generateCheckpointId();
        std::string filename = m_basePath + "\\" + checkpointId + ".checkpoint";
        
        // Save to file
        return writeCheckpointToDisk(filename, state, level) ? checkpointId : "";
    }
    
    std::string quickSaveCheckpoint(const CheckpointMetadata& metadata, 
                                   const CheckpointState& state) { 
        return saveCheckpoint(metadata, state, CompressionLevel::FAST);
    }
    
    std::string saveModelWeights(const CheckpointMetadata& metadata, 
                                const std::vector<uint8_t>& weights, 
                                CompressionLevel level) { 
        if (!m_initialized) return "";
        
        std::string checkpointId = generateCheckpointId();
        std::string filename = m_basePath + "\\" + checkpointId + "_weights.bin";
        
        // Compress and save weights
        auto compressed = compressState(weights, level);
        // Write to file...
        return checkpointId;
    }
    
    bool loadCheckpoint(const std::string& checkpointId, CheckpointState& state) { 
        if (!m_initialized) return false;
        
        std::string filename = m_basePath + "\\" + checkpointId + ".checkpoint";
        return readCheckpointFromDisk(filename, state);
    }
    
    std::string loadLatestCheckpoint(CheckpointState& state) { 
        auto checkpoints = listCheckpoints();
        if (checkpoints.empty()) return "";
        
        // Find latest by timestamp
        std::sort(checkpoints.begin(), checkpoints.end(), 
            [](const CheckpointIndex& a, const CheckpointIndex& b) {
                return a.timestamp > b.timestamp;
            });
        
        if (loadCheckpoint(checkpoints[0].id, state)) {
            return checkpoints[0].id;
        }
        return "";
    }
    
    std::string loadBestCheckpoint(CheckpointState& state) { 
        auto checkpoints = listCheckpoints();
        if (checkpoints.empty()) return "";
        
        // Find best by validation accuracy (simplified)
        std::sort(checkpoints.begin(), checkpoints.end(), 
            [](const CheckpointIndex& a, const CheckpointIndex& b) {
                return a.metadata.validationAccuracy > b.metadata.validationAccuracy;
            });
        
        if (loadCheckpoint(checkpoints[0].id, state)) {
            return checkpoints[0].id;
        }
        return "";
    }
    
    std::string loadCheckpointFromEpoch(int epoch, CheckpointState& state) { 
        auto checkpoints = listCheckpoints();
        
        for (const auto& cp : checkpoints) {
            if (cp.metadata.epoch == epoch) {
                if (loadCheckpoint(cp.id, state)) {
                    return cp.id;
                }
            }
        }
        return "";
    }
    
    CheckpointManager::CheckpointMetadata getCheckpointMetadata(const std::string& checkpointId) const { 
        CheckpointState state;
        if (loadCheckpoint(checkpointId, state)) {
            return state.metadata;
        }
        return {};
    }
    
    std::vector<CheckpointManager::CheckpointIndex> listCheckpoints() const { 
        std::vector<CheckpointIndex> checkpoints;
        if (!m_initialized) return checkpoints;
        
        WIN32_FIND_DATAA findData;
        std::string searchPath = m_basePath + "\\*.checkpoint";
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    CheckpointIndex idx;
                    idx.id = std::string(findData.cFileName).substr(0, 
                        std::string(findData.cFileName).find_last_of('.'));
                    idx.timestamp = 0; // Would parse from filename or metadata
                    // Load metadata...
                    checkpoints.push_back(idx);
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        
        return checkpoints;
    }
    
    std::vector<CheckpointManager::CheckpointIndex> getCheckpointHistory(int modelId) const { 
        auto all = listCheckpoints();
        std::vector<CheckpointIndex> filtered;
        
        for (const auto& cp : all) {
            if (cp.metadata.modelId == modelId) {
                filtered.push_back(cp);
            }
        }
        
        return filtered;
    }
    
    bool deleteCheckpoint(const std::string& checkpointId) { 
        if (!m_initialized) return false;
        
        std::string filename = m_basePath + "\\" + checkpointId + ".checkpoint";
        return DeleteFileA(filename.c_str()) != 0;
    }
    
    int pruneOldCheckpoints(int keepCount) { 
        auto checkpoints = listCheckpoints();
        if (checkpoints.size() <= (size_t)keepCount) return 0;
        
        // Sort by timestamp, keep newest
        std::sort(checkpoints.begin(), checkpoints.end(), 
            [](const CheckpointIndex& a, const CheckpointIndex& b) {
                return a.timestamp > b.timestamp;
            });
        
        int deleted = 0;
        for (size_t i = keepCount; i < checkpoints.size(); ++i) {
            if (deleteCheckpoint(checkpoints[i].id)) {
                deleted++;
            }
        }
        
        return deleted;
    }
    
    CheckpointManager::CheckpointMetadata getBestCheckpointInfo() const { 
        auto checkpoints = listCheckpoints();
        if (checkpoints.empty()) return {};
        
        // Find best by validation accuracy
        const CheckpointIndex* best = nullptr;
        for (const auto& cp : checkpoints) {
            if (!best || cp.metadata.validationAccuracy > best->metadata.validationAccuracy) {
                best = &cp;
            }
        }
        
        return best ? best->metadata : CheckpointMetadata{};
    }
    
    bool updateCheckpointMetadata(const std::string& checkpointId, const CheckpointMetadata& metadata) { 
        // Load, update, save
        CheckpointState state;
        if (!loadCheckpoint(checkpointId, state)) return false;
        
        state.metadata = metadata;
        return saveCheckpoint(metadata, state, CompressionLevel::FAST) == checkpointId;
    }
    
    bool setCheckpointNote(const std::string& checkpointId, const std::string& note) { 
        CheckpointState state;
        if (!loadCheckpoint(checkpointId, state)) return false;
        
        state.metadata.notes = note;
        return updateCheckpointMetadata(checkpointId, state.metadata);
    }
    
    bool enableAutoCheckpointing(int intervalEpochs, int maxCheckpoints) { 
        // Set up automatic checkpointing
        return true;
    }
    
    void disableAutoCheckpointing() {}
    
    bool shouldCheckpoint(int currentEpoch, int totalEpochs) const { 
        // Checkpoint every 10 epochs or at end
        return (currentEpoch % 10 == 0) || (currentEpoch == totalEpochs - 1);
    }
    
    bool validateCheckpoint(const std::string& checkpointId) const { 
        CheckpointState state;
        return loadCheckpoint(checkpointId, state);
    }
    
    std::map<std::string, bool> validateAllCheckpoints() const { 
        std::map<std::string, bool> results;
        auto checkpoints = listCheckpoints();
        
        for (const auto& cp : checkpoints) {
            results[cp.id] = validateCheckpoint(cp.id);
        }
        
        return results;
    }
    
    bool repairCheckpoint(const std::string& checkpointId) { 
        if (!m_initialized || checkpointId.empty()) return false;

        CheckpointState state;
        if (loadCheckpoint(checkpointId, state)) {
            return true;
        }

        auto checkpoints = listCheckpoints();
        std::sort(checkpoints.begin(), checkpoints.end(),
            [](const CheckpointIndex& a, const CheckpointIndex& b) {
                return a.timestamp > b.timestamp;
            });

        for (const auto& cp : checkpoints) {
            if (cp.id == checkpointId) continue;
            if (!loadCheckpoint(cp.id, state)) continue;

            const std::filesystem::path src = std::filesystem::path(m_basePath) / (cp.id + ".checkpoint");
            const std::filesystem::path dst = std::filesystem::path(m_basePath) / (checkpointId + ".checkpoint");
            std::error_code ec;
            std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) continue;

            CheckpointState verify;
            if (loadCheckpoint(checkpointId, verify)) {
                return true;
            }
        }

        return false;
    }
    
    uint64_t getTotalCheckpointSize() const { 
        uint64_t total = 0;
        auto checkpoints = listCheckpoints();
        
        for (const auto& cp : checkpoints) {
            total += getCheckpointSize(cp.id);
        }
        
        return total;
    }
    
    uint64_t getCheckpointSize(const std::string& checkpointId) const { 
        std::string filename = m_basePath + "\\" + checkpointId + ".checkpoint";
        HANDLE hFile = CreateFileA(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, 
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        
        if (hFile == INVALID_HANDLE_VALUE) return 0;
        
        LARGE_INTEGER size;
        GetFileSizeEx(hFile, &size);
        CloseHandle(hFile);
        
        return size.QuadPart;
    }
    
    std::string generateCheckpointReport() const { 
        auto checkpoints = listCheckpoints();
        std::string report = "Checkpoint Report\n";
        report += "Total checkpoints: " + std::to_string(checkpoints.size()) + "\n";
        report += "Total size: " + std::to_string(getTotalCheckpointSize()) + " bytes\n";
        
        for (const auto& cp : checkpoints) {
            report += "- " + cp.id + " (" + std::to_string(getCheckpointSize(cp.id)) + " bytes)\n";
        }
        
        return report;
    }
    
    std::string compareCheckpoints(const std::string& id1, const std::string& id2) const { 
        CheckpointMetadata m1 = getCheckpointMetadata(id1);
        CheckpointMetadata m2 = getCheckpointMetadata(id2);
        
        std::string comparison = "Comparing " + id1 + " vs " + id2 + "\n";
        comparison += "Epoch: " + std::to_string(m1.epoch) + " vs " + std::to_string(m2.epoch) + "\n";
        comparison += "Validation Accuracy: " + std::to_string(m1.validationAccuracy) + 
                     " vs " + std::to_string(m2.validationAccuracy) + "\n";
        
        return comparison;
    }
    
    void setDistributedInfo(int rank, int worldSize) {}
    
    bool synchronizeDistributedCheckpoints() { return true; }
    
    std::string exportConfiguration() const { 
        return "{\"basePath\":\"" + m_basePath + "\",\"initialized\":" + 
               (m_initialized ? "true" : "false") + "}";
    }
    
    bool importConfiguration(const std::string& config) { 
        // Parse JSON and apply
        return true;
    }
    
    bool saveConfigurationToFile(const std::string& filename) const { 
        std::string config = exportConfiguration();
        std::ofstream file(filename);
        if (!file.is_open()) return false;
        file << config;
        return true;
    }
    
    bool loadConfigurationFromFile(const std::string& filename) { 
        std::ifstream file(filename);
        if (!file.is_open()) return false;
        
        std::string config((std::istreambuf_iterator<char>(file)), 
                          std::istreambuf_iterator<char>());
        return importConfiguration(config);
    }
    
    std::string generateCheckpointId() { 
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        return "cp_" + std::to_string(timestamp);
    }
    
    std::vector<uint8_t> compressState(const std::vector<uint8_t>& data, CompressionLevel level) { 
        // Simple compression - just return data for now
        return data;
    }
    
    std::vector<uint8_t> decompressState(const std::vector<uint8_t>& data) { 
        return data;
    }
    
    bool writeCheckpointToDisk(const std::string& filename, const CheckpointState& state, CompressionLevel level) { 
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Write metadata
        file.write(reinterpret_cast<const char*>(&state.metadata), sizeof(state.metadata));
        
        // Write compressed state data
        auto compressed = compressState(state.modelState, level);
        uint64_t size = compressed.size();
        file.write(reinterpret_cast<const char*>(&size), sizeof(size));
        file.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
        
        return file.good();
    }
    
    bool readCheckpointFromDisk(const std::string& filename, CheckpointState& state) { 
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) return false;
        
        // Read metadata
        file.read(reinterpret_cast<char*>(&state.metadata), sizeof(state.metadata));
        
        // Read compressed state data
        uint64_t size;
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        state.modelState.resize(size);
        file.read(reinterpret_cast<char*>(state.modelState.data()), size);
        
        // Decompress
        state.modelState = decompressState(state.modelState);
        
        return file.good();
    }
};

// ================================================
// Project Context Stub
// ================================================
namespace RawrXD {
class ProjectContext {
private:
    std::string m_projectPath;
    std::string m_projectType;
    std::unordered_map<std::string, std::string> m_metadata;
    
public:
    ProjectContext() = default;
    
    void setProjectPath(const std::string& path) { m_projectPath = path; }
    std::string getProjectPath() const { return m_projectPath; }
    
    void setProjectType(const std::string& type) { m_projectType = type; }
    std::string getProjectType() const { return m_projectType; }
    
    void setMetadata(const std::string& key, const std::string& value) { 
        m_metadata[key] = value; 
    }
    
    std::string getMetadata(const std::string& key) const { 
        auto it = m_metadata.find(key);
        return it != m_metadata.end() ? it->second : "";
    }
    
    std::vector<std::string> getAllMetadataKeys() const {
        std::vector<std::string> keys;
        for (const auto& pair : m_metadata) {
            keys.push_back(pair.first);
        }
        return keys;
    }
};
}

// ================================================
// UI Component Stubs
// ================================================
class MultiFileSearchWidget {
private:
    std::vector<std::string> m_searchResults;
    std::string m_currentQuery;
    
public:
    ~MultiFileSearchWidget() = default;
    
    void setQuery(const std::string& query) { m_currentQuery = query; }
    std::string getQuery() const { return m_currentQuery; }
    
    void performSearch() {
        m_searchResults.clear();
        // Simulate search results
        m_searchResults.push_back("file1.txt: Found '" + m_currentQuery + "'");
        m_searchResults.push_back("file2.cpp: Found '" + m_currentQuery + "'");
    }
    
    const std::vector<std::string>& getResults() const { return m_searchResults; }
    
    void clearResults() { m_searchResults.clear(); }
};

class BenchmarkMenu {
private:
    HWND m_parentWindow;
    
public:
    BenchmarkMenu(HWND parent) : m_parentWindow(parent) {}
    ~BenchmarkMenu() = default;
    
    void initialize() {
        // Create benchmark menu items
    }
    
    void openBenchmarkDialog() {
        // Show benchmark configuration dialog
        MessageBoxA(m_parentWindow, "Benchmark Dialog", "RawrXD IDE", MB_OK);
    }
    
    void runBenchmark(const std::string& benchmarkType) {
        // Execute benchmark
    }
    
    std::vector<std::string> getAvailableBenchmarks() const {
        return {"inference_speed", "memory_usage", "accuracy"};
    }
};

class InterpretabilityPanel {
private:
    HWND m_parentWindow;
    
public:
    void setParent(HWND parent) { m_parentWindow = parent; }
    
    void initialize() {
        // Initialize interpretability UI components
    }
    
    void show() {
        // Show interpretability panel
        if (m_parentWindow) {
            MessageBoxA(m_parentWindow, "Interpretability Panel", "RawrXD IDE", MB_OK);
        }
    }
    
    void updateModelVisualization(const std::string& modelData) {
        // Update visualization with model data
    }
    
    void showAttentionWeights(const std::vector<float>& weights) {
        // Display attention weights
    }
};

class FeatureRegistryPanel {
private:
    std::vector<std::string> m_registeredFeatures;
    
public:
    ~FeatureRegistryPanel() = default;
    
    void registerFeature(const std::string& featureName) {
        m_registeredFeatures.push_back(featureName);
    }
    
    void unregisterFeature(const std::string& featureName) {
        auto it = std::find(m_registeredFeatures.begin(), m_registeredFeatures.end(), featureName);
        if (it != m_registeredFeatures.end()) {
            m_registeredFeatures.erase(it);
        }
    }
    
    const std::vector<std::string>& getRegisteredFeatures() const {
        return m_registeredFeatures;
    }
    
    bool isFeatureRegistered(const std::string& featureName) const {
        return std::find(m_registeredFeatures.begin(), m_registeredFeatures.end(), featureName) 
               != m_registeredFeatures.end();
    }
};
#endif // RAWRXD_NEED_PRODUCTION_STUBS

// ================================================
// Checkpoint Manager (include/checkpoint_manager.h) — no other .cpp provides this class
// ================================================
#include "../../include/checkpoint_manager.h"

namespace {
int g_checkpointDistributedRank = 0;
int g_checkpointDistributedWorldSize = 1;
}

CheckpointManager::CheckpointManager(void*) {}
CheckpointManager::~CheckpointManager() = default;
bool CheckpointManager::initialize(const std::string& checkpointDir, int maxCheckpoints) {
    m_checkpointDir = checkpointDir;
    m_maxCheckpoints = std::max(1, maxCheckpoints);
    std::error_code ec;
    std::filesystem::create_directories(m_checkpointDir, ec);
    m_checkpointIndex = listCheckpoints();
    return !m_checkpointDir.empty();
}

bool CheckpointManager::isInitialized() const { return !m_checkpointDir.empty(); }

std::string CheckpointManager::saveCheckpoint(const CheckpointMetadata& metadata,
                                              const CheckpointState& state,
                                              CompressionLevel compress) {
    if (!isInitialized()) return {};
    CheckpointMetadata md = metadata;
    if (md.checkpointId.empty()) md.checkpointId = generateCheckpointId();
    if (md.timestamp == 0) {
        md.timestamp = static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    }

    if (!writeCheckpointToDisk(md.checkpointId, state, compress)) return {};

    CheckpointIndex idx;
    idx.checkpointId = md.checkpointId;
    idx.filePath = (std::filesystem::path(m_checkpointDir) / (md.checkpointId + ".ckpt")).string();
    idx.metadata = md;
    idx.checkpointNumber = ++m_checkpointCounter;

    auto it = std::find_if(m_checkpointIndex.begin(), m_checkpointIndex.end(),
                           [&](const CheckpointIndex& c) { return c.checkpointId == idx.checkpointId; });
    if (it != m_checkpointIndex.end()) {
        *it = idx;
    } else {
        m_checkpointIndex.push_back(idx);
    }

    if (md.isBestModel || m_bestCheckpointId.empty()) {
        m_bestCheckpointId = md.checkpointId;
    }
    if (m_savedCb) m_savedCb(m_savedCtx, md.checkpointId.c_str(), md.epoch, md.step);
    return md.checkpointId;
}

std::string CheckpointManager::quickSaveCheckpoint(const CheckpointMetadata& metadata, const CheckpointState& state) {
    return saveCheckpoint(metadata, state, CompressionLevel::Low);
}

std::string CheckpointManager::saveModelWeights(const CheckpointMetadata& metadata,
                                                const std::vector<uint8_t>& modelWeights,
                                                CompressionLevel compress) {
    CheckpointState state;
    state.modelWeights = modelWeights;
    return saveCheckpoint(metadata, state, compress);
}

bool CheckpointManager::loadCheckpoint(const std::string& checkpointId, CheckpointState& state) {
    const bool ok = readCheckpointFromDisk(checkpointId, state);
    if (ok && m_loadedCb) m_loadedCb(m_loadedCtx, checkpointId.c_str());
    return ok;
}

std::string CheckpointManager::loadLatestCheckpoint(CheckpointState& state) {
    auto list = listCheckpoints();
    if (list.empty()) return {};
    std::sort(list.begin(), list.end(), [](const CheckpointIndex& a, const CheckpointIndex& b) {
        return a.metadata.timestamp > b.metadata.timestamp;
    });
    return loadCheckpoint(list.front().checkpointId, state) ? list.front().checkpointId : std::string{};
}

std::string CheckpointManager::loadBestCheckpoint(CheckpointState& state) {
    if (!m_bestCheckpointId.empty() && loadCheckpoint(m_bestCheckpointId, state)) {
        return m_bestCheckpointId;
    }
    auto list = listCheckpoints();
    if (list.empty()) return {};
    std::sort(list.begin(), list.end(), [](const CheckpointIndex& a, const CheckpointIndex& b) {
        return a.metadata.accuracy > b.metadata.accuracy;
    });
    return loadCheckpoint(list.front().checkpointId, state) ? list.front().checkpointId : std::string{};
}

std::string CheckpointManager::loadCheckpointFromEpoch(int epoch, CheckpointState& state) {
    const auto list = listCheckpoints();
    for (const auto& cp : list) {
        if (cp.metadata.epoch == epoch && loadCheckpoint(cp.checkpointId, state)) return cp.checkpointId;
    }
    return {};
}

CheckpointManager::CheckpointMetadata CheckpointManager::getCheckpointMetadata(const std::string& checkpointId) const {
    const auto list = listCheckpoints();
    for (const auto& cp : list) {
        if (cp.checkpointId == checkpointId) return cp.metadata;
    }
    return {};
}

std::vector<CheckpointManager::CheckpointIndex> CheckpointManager::listCheckpoints() const {
    std::vector<CheckpointIndex> list;
    if (!isInitialized()) return list;

    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(m_checkpointDir, ec)) {
        if (ec || !entry.is_regular_file()) continue;
        if (entry.path().extension() != ".ckpt") continue;

        CheckpointIndex idx;
        idx.checkpointId = entry.path().stem().string();
        idx.filePath = entry.path().string();
        idx.metadata.checkpointId = idx.checkpointId;
        idx.metadata.timestamp = static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                entry.last_write_time().time_since_epoch()).count());
        list.push_back(std::move(idx));
    }
    return list;
}

std::vector<CheckpointManager::CheckpointIndex> CheckpointManager::getCheckpointHistory(int limit) const {
    auto list = listCheckpoints();
    std::sort(list.begin(), list.end(), [](const CheckpointIndex& a, const CheckpointIndex& b) {
        return a.metadata.timestamp > b.metadata.timestamp;
    });
    if (limit > 0 && static_cast<size_t>(limit) < list.size()) {
        list.resize(static_cast<size_t>(limit));
    }
    return list;
}
bool CheckpointManager::deleteCheckpoint(const std::string& checkpointId) {
    if (!isInitialized() || checkpointId.empty()) return false;

    const auto path = std::filesystem::path(m_checkpointDir) / (checkpointId + ".ckpt");
    std::error_code ec;
    const bool removed = std::filesystem::remove(path, ec);
    if (ec || !removed) return false;

    m_checkpointIndex.erase(
        std::remove_if(m_checkpointIndex.begin(), m_checkpointIndex.end(),
            [&](const CheckpointIndex& idx) { return idx.checkpointId == checkpointId; }),
        m_checkpointIndex.end());

    if (m_bestCheckpointId == checkpointId) {
        m_bestCheckpointId.clear();
    }
    return true;
}
int CheckpointManager::pruneOldCheckpoints(int keepCount) {
    if (!isInitialized()) return 0;
    if (keepCount < 0) keepCount = 0;

    auto list = listCheckpoints();
    std::sort(list.begin(), list.end(), [](const CheckpointIndex& a, const CheckpointIndex& b) {
        return a.metadata.timestamp > b.metadata.timestamp;
    });

    if (static_cast<size_t>(keepCount) >= list.size()) return 0;

    int deleted = 0;
    for (size_t i = static_cast<size_t>(keepCount); i < list.size(); ++i) {
        if (deleteCheckpoint(list[i].checkpointId)) {
            ++deleted;
        }
    }
    return deleted;
}
CheckpointManager::CheckpointMetadata CheckpointManager::getBestCheckpointInfo() const {
    if (!m_bestCheckpointId.empty()) return getCheckpointMetadata(m_bestCheckpointId);
    auto list = listCheckpoints();
    if (list.empty()) return {};
    std::sort(list.begin(), list.end(), [](const CheckpointIndex& a, const CheckpointIndex& b) {
        return a.metadata.accuracy > b.metadata.accuracy;
    });
    return list.front().metadata;
}
bool CheckpointManager::updateCheckpointMetadata(const std::string& checkpointId, const CheckpointMetadata& metadata) {
    if (!isInitialized() || checkpointId.empty() || !validateCheckpoint(checkpointId)) return false;

    bool updated = false;
    for (auto& idx : m_checkpointIndex) {
        if (idx.checkpointId == checkpointId) {
            idx.metadata = metadata;
            idx.metadata.checkpointId = checkpointId;
            updated = true;
            break;
        }
    }

    if (!updated) {
        CheckpointIndex idx;
        idx.checkpointId = checkpointId;
        idx.filePath = (std::filesystem::path(m_checkpointDir) / (checkpointId + ".ckpt")).string();
        idx.metadata = metadata;
        idx.metadata.checkpointId = checkpointId;
        m_checkpointIndex.push_back(std::move(idx));
    }

    if (metadata.isBestModel) {
        m_bestCheckpointId = checkpointId;
    }
    return true;
}
bool CheckpointManager::setCheckpointNote(const std::string& checkpointId, const std::string& note) {
    if (!isInitialized() || checkpointId.empty()) return false;
    auto metadata = getCheckpointMetadata(checkpointId);
    metadata.checkpointId = checkpointId;
    metadata.notes = note;
    return updateCheckpointMetadata(checkpointId, metadata);
}
bool CheckpointManager::enableAutoCheckpointing(int intervalEpochs, int maxCheckpoints) {
    if (intervalEpochs <= 0) return false;
    m_autoCheckpointEnabled = true;
    m_autoCheckpointEpochInterval = intervalEpochs;
    if (maxCheckpoints > 0) {
        m_maxCheckpoints = maxCheckpoints;
    }
    return true;
}
void CheckpointManager::disableAutoCheckpointing() {}
bool CheckpointManager::shouldCheckpoint(int currentEpoch, int totalEpochs) const {
    if (!m_autoCheckpointEnabled || currentEpoch < 0) return false;
    if (totalEpochs > 0 && currentEpoch >= (totalEpochs - 1)) return true;
    const int interval = std::max(1, m_autoCheckpointEpochInterval);
    return (currentEpoch == 0) || ((currentEpoch % interval) == 0);
}
bool CheckpointManager::validateCheckpoint(const std::string& checkpointId) const {
    std::error_code ec;
    const auto p = std::filesystem::path(m_checkpointDir) / (checkpointId + ".ckpt");
    return std::filesystem::exists(p, ec) && std::filesystem::is_regular_file(p, ec) &&
           std::filesystem::file_size(p, ec) > 0;
}
std::map<std::string, bool> CheckpointManager::validateAllCheckpoints() const {
    std::map<std::string, bool> out;
    for (const auto& cp : listCheckpoints()) out[cp.checkpointId] = validateCheckpoint(cp.checkpointId);
    return out;
}
bool CheckpointManager::repairCheckpoint(const std::string& checkpointId) {
    if (checkpointId.empty()) return false;
    if (validateCheckpoint(checkpointId)) return true;

    const auto dst = std::filesystem::path(m_checkpointDir) / (checkpointId + ".ckpt");
    auto list = listCheckpoints();
    std::sort(list.begin(), list.end(), [](const CheckpointIndex& a, const CheckpointIndex& b) {
        return a.metadata.timestamp > b.metadata.timestamp;
    });

    for (const auto& cp : list) {
        if (cp.checkpointId == checkpointId) continue;
        if (!validateCheckpoint(cp.checkpointId)) continue;

        const auto src = std::filesystem::path(m_checkpointDir) / (cp.checkpointId + ".ckpt");
        std::error_code ec;
        std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec && validateCheckpoint(checkpointId)) {
            return true;
        }
    }
    return false;
}
uint64_t CheckpointManager::getTotalCheckpointSize() const {
    uint64_t totalBytes = 0;
    for (const auto& checkpoint : listCheckpoints()) {
        totalBytes += getCheckpointSize(checkpoint.checkpointId);
    }
    return totalBytes;
}
uint64_t CheckpointManager::getCheckpointSize(const std::string& checkpointId) const {
    if (!isInitialized() || checkpointId.empty()) return 0;

    const auto checkpointPath = std::filesystem::path(m_checkpointDir) / (checkpointId + ".ckpt");
    std::error_code ec;
    if (!std::filesystem::exists(checkpointPath, ec) || !std::filesystem::is_regular_file(checkpointPath, ec)) {
        return 0;
    }
    const uint64_t fileSize = std::filesystem::file_size(checkpointPath, ec);
    return ec ? 0 : fileSize;
}
std::string CheckpointManager::generateCheckpointReport() const {
    std::ostringstream oss;
    const auto cps = listCheckpoints();
    oss << "Checkpoint Report\n";
    oss << "count=" << cps.size() << "\n";
    for (const auto& cp : cps) {
        oss << cp.checkpointId << " acc=" << cp.metadata.accuracy
            << " epoch=" << cp.metadata.epoch << "\n";
    }
    return oss.str();
}
std::string CheckpointManager::compareCheckpoints(const std::string& checkpointId1,
                                                  const std::string& checkpointId2) const {
    const auto a = getCheckpointMetadata(checkpointId1);
    const auto b = getCheckpointMetadata(checkpointId2);
    std::ostringstream oss;
    oss << "compare " << checkpointId1 << " vs " << checkpointId2 << "\n";
    oss << "epoch: " << a.epoch << " vs " << b.epoch << "\n";
    oss << "accuracy: " << a.accuracy << " vs " << b.accuracy << "\n";
    oss << "val_loss: " << a.validationLoss << " vs " << b.validationLoss << "\n";
    return oss.str();
}
void CheckpointManager::setDistributedInfo(int rank, int worldSize) {
    g_checkpointDistributedRank = std::max(0, rank);
    g_checkpointDistributedWorldSize = std::max(1, worldSize);
}
bool CheckpointManager::synchronizeDistributedCheckpoints() {
    if (!isInitialized()) return false;

    std::error_code ec;
    const auto syncRoot = std::filesystem::path(m_checkpointDir) / "distributed_sync";
    const auto localRankDir = syncRoot / ("rank_" + std::to_string(g_checkpointDistributedRank));
    std::filesystem::create_directories(localRankDir, ec);
    if (ec) return false;

    bool wroteLocalReplica = false;
    const auto checkpoints = listCheckpoints();
    for (const auto& cp : checkpoints) {
        const auto src = std::filesystem::path(m_checkpointDir) / (cp.checkpointId + ".ckpt");
        const auto dst = localRankDir / (cp.checkpointId + ".ckpt");
        std::error_code copyEc;
        std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, copyEc);
        if (!copyEc) {
            wroteLocalReplica = true;
        }
    }

    bool importedReplica = false;
    for (const auto& rankDir : std::filesystem::directory_iterator(syncRoot, ec)) {
        if (ec || !rankDir.is_directory()) continue;
        if (rankDir.path() == localRankDir) continue;

        for (const auto& file : std::filesystem::directory_iterator(rankDir.path(), ec)) {
            if (ec || !file.is_regular_file()) continue;
            if (file.path().extension() != ".ckpt") continue;

            const auto dst = std::filesystem::path(m_checkpointDir) / file.path().filename();
            if (std::filesystem::exists(dst, ec)) continue;
            std::error_code importEc;
            std::filesystem::copy_file(file.path(), dst, std::filesystem::copy_options::overwrite_existing, importEc);
            if (!importEc) {
                importedReplica = true;
            }
        }
    }

    const auto manifest = localRankDir / "manifest.txt";
    std::ofstream manifestOut(manifest, std::ios::trunc);
    if (!manifestOut) return false;
    manifestOut << "rank=" << g_checkpointDistributedRank << "\n";
    manifestOut << "world_size=" << g_checkpointDistributedWorldSize << "\n";
    manifestOut << "checkpoint_count=" << checkpoints.size() << "\n";
    manifestOut << "wrote_local_replica=" << (wroteLocalReplica ? "true" : "false") << "\n";
    manifestOut << "imported_replica=" << (importedReplica ? "true" : "false") << "\n";

    m_checkpointIndex = listCheckpoints();
    return manifestOut.good();
}
std::string CheckpointManager::exportConfiguration() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"checkpointDir\":\"" << m_checkpointDir << "\",";
    oss << "\"maxCheckpoints\":" << m_maxCheckpoints << ",";
    oss << "\"autoEnabled\":" << (m_autoCheckpointEnabled ? "true" : "false") << ",";
    oss << "\"autoInterval\":" << m_autoCheckpointInterval << ",";
    oss << "\"autoEpochInterval\":" << m_autoCheckpointEpochInterval;
    oss << "}";
    return oss.str();
}
bool CheckpointManager::importConfiguration(const std::string& config) {
    if (config.empty()) return false;

    auto extractString = [&](const char* key, std::string& out) -> bool {
        const std::string pattern = std::string("\"") + key + "\":\"";
        const size_t start = config.find(pattern);
        if (start == std::string::npos) return false;
        const size_t valueStart = start + pattern.size();
        const size_t valueEnd = config.find('"', valueStart);
        if (valueEnd == std::string::npos) return false;
        out = config.substr(valueStart, valueEnd - valueStart);
        return true;
    };

    auto extractInt = [&](const char* key, int& out) -> bool {
        const std::string pattern = std::string("\"") + key + "\":";
        const size_t start = config.find(pattern);
        if (start == std::string::npos) return false;
        const size_t valueStart = start + pattern.size();
        std::stringstream ss(config.substr(valueStart));
        int v = 0;
        ss >> v;
        if (ss.fail()) return false;
        out = v;
        return true;
    };

    auto extractBool = [&](const char* key, bool& out) -> bool {
        const std::string pattern = std::string("\"") + key + "\":";
        const size_t start = config.find(pattern);
        if (start == std::string::npos) return false;
        const size_t valueStart = start + pattern.size();
        if (config.compare(valueStart, 4, "true") == 0) {
            out = true;
            return true;
        }
        if (config.compare(valueStart, 5, "false") == 0) {
            out = false;
            return true;
        }
        return false;
    };

    bool parsedAny = false;

    std::string checkpointDir;
    if (extractString("checkpointDir", checkpointDir)) {
        m_checkpointDir = checkpointDir;
        std::error_code ec;
        std::filesystem::create_directories(m_checkpointDir, ec);
        parsedAny = true;
    }

    int maxCheckpoints = 0;
    if (extractInt("maxCheckpoints", maxCheckpoints) && maxCheckpoints > 0) {
        m_maxCheckpoints = maxCheckpoints;
        parsedAny = true;
    }

    bool autoEnabled = false;
    if (extractBool("autoEnabled", autoEnabled)) {
        m_autoCheckpointEnabled = autoEnabled;
        parsedAny = true;
    }

    int autoInterval = 0;
    if (extractInt("autoInterval", autoInterval)) {
        m_autoCheckpointInterval = autoInterval;
        parsedAny = true;
    }

    int autoEpochInterval = 0;
    if (extractInt("autoEpochInterval", autoEpochInterval) && autoEpochInterval > 0) {
        m_autoCheckpointEpochInterval = autoEpochInterval;
        parsedAny = true;
    }

    if (!m_checkpointDir.empty()) {
        m_checkpointIndex = listCheckpoints();
    }
    return parsedAny;
}
bool CheckpointManager::saveConfigurationToFile(const std::string& filename) const {
    if (filename.empty()) return false;
    const std::filesystem::path path(filename);
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << exportConfiguration();
    return out.good();
}
bool CheckpointManager::loadConfigurationFromFile(const std::string& filename) {
    if (filename.empty()) return false;
    std::ifstream in(filename, std::ios::binary);
    if (!in) return false;
    std::string config((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (config.empty()) return false;
    return importConfiguration(config);
}
std::string CheckpointManager::generateCheckpointId() {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream oss;
    oss << "ckpt_" << now << "_" << (++m_checkpointCounter);
    return oss.str();
}
std::vector<uint8_t> CheckpointManager::compressState(const std::vector<uint8_t>& data, CompressionLevel) { return data; }
std::vector<uint8_t> CheckpointManager::decompressState(const std::vector<uint8_t>& data) { return data; }
bool CheckpointManager::writeCheckpointToDisk(const std::string& checkpointId,
                                              const CheckpointState& state,
                                              CompressionLevel compress) {
    if (!isInitialized() || checkpointId.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(m_checkpointDir, ec);
    const auto path = std::filesystem::path(m_checkpointDir) / (checkpointId + ".ckpt");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    auto writeBlob = [&](const std::vector<uint8_t>& blob) {
        const auto compressed = compressState(blob, compress);
        uint64_t sz = static_cast<uint64_t>(compressed.size());
        out.write(reinterpret_cast<const char*>(&sz), sizeof(sz));
        if (sz) out.write(reinterpret_cast<const char*>(compressed.data()), static_cast<std::streamsize>(sz));
    };

    writeBlob(state.modelWeights);
    writeBlob(state.optimizerState);
    writeBlob(state.schedulerState);
    writeBlob(state.trainingState);
    uint64_t cfgLen = static_cast<uint64_t>(state.config.size());
    out.write(reinterpret_cast<const char*>(&cfgLen), sizeof(cfgLen));
    if (cfgLen) out.write(state.config.data(), static_cast<std::streamsize>(cfgLen));
    return out.good();
}
bool CheckpointManager::readCheckpointFromDisk(const std::string& checkpointId, CheckpointState& state) {
    if (!isInitialized() || checkpointId.empty()) return false;
    const auto path = std::filesystem::path(m_checkpointDir) / (checkpointId + ".ckpt");
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    auto readBlob = [&](std::vector<uint8_t>& blob) -> bool {
        uint64_t sz = 0;
        in.read(reinterpret_cast<char*>(&sz), sizeof(sz));
        if (!in.good()) return false;
        blob.resize(static_cast<size_t>(sz));
        if (sz) in.read(reinterpret_cast<char*>(blob.data()), static_cast<std::streamsize>(sz));
        if (!in.good()) return false;
        blob = decompressState(blob);
        return true;
    };

    if (!readBlob(state.modelWeights)) return false;
    if (!readBlob(state.optimizerState)) return false;
    if (!readBlob(state.schedulerState)) return false;
    if (!readBlob(state.trainingState)) return false;

    uint64_t cfgLen = 0;
    in.read(reinterpret_cast<char*>(&cfgLen), sizeof(cfgLen));
    if (!in.good()) return false;
    state.config.resize(static_cast<size_t>(cfgLen));
    if (cfgLen) in.read(state.config.data(), static_cast<std::streamsize>(cfgLen));
    return in.good();
}

// ================================================
// Agentic Config Stubs
// ================================================
// Superseded by src/core/agentic_autonomous_config.cpp. Keep this block disabled
// to avoid conflicting/obsolete enums and behavior.
#if 0
namespace RawrXD {
AgenticAutonomousConfig::AgenticAutonomousConfig() = default;
AgenticAutonomousConfig& AgenticAutonomousConfig::instance() {
    static AgenticAutonomousConfig inst;
    return inst;
}
bool AgenticAutonomousConfig::setOperationModeFromString(const std::string& mode) { 
    if (mode == "agent") m_operationMode = AgenticOperationMode::Agent;
    else if (mode == "autonomous") m_operationMode = AgenticOperationMode::Autonomous;
    else if (mode == "hybrid") m_operationMode = AgenticOperationMode::Hybrid;
    else return false;
    return true;
}
bool AgenticAutonomousConfig::setModelSelectionModeFromString(const std::string& mode) { 
    if (mode == "manual") m_modelSelectionMode = ModelSelectionMode::Manual;
    else if (mode == "automatic") m_modelSelectionMode = ModelSelectionMode::Automatic;
    else if (mode == "adaptive") m_modelSelectionMode = ModelSelectionMode::Adaptive;
    else return false;
    return true;
}
void AgenticAutonomousConfig::setPerModelInstanceCount(int count) { 
    m_perModelInstanceCount = count; 
}
void AgenticAutonomousConfig::setMaxModelsInParallel(int count) { 
    m_maxModelsInParallel = count; 
}
void AgenticAutonomousConfig::setCycleAgentCounter(int count) { 
    m_cycleAgentCounter = count; 
}
bool AgenticAutonomousConfig::setQualitySpeedBalanceFromString(const std::string& balance) { 
    if (balance == "quality") m_qualitySpeedBalance = QualitySpeedBalance::Quality;
    else if (balance == "balanced") m_qualitySpeedBalance = QualitySpeedBalance::Balanced;
    else if (balance == "speed") m_qualitySpeedBalance = QualitySpeedBalance::Speed;
    else return false;
    return true;
}
bool AgenticAutonomousConfig::fromJson(const std::string& json) { 
    return true;
}
AgenticOperationMode AgenticAutonomousConfig::getOperationMode() const { 
    return m_operationMode; 
}
std::string AgenticAutonomousConfig::getRecommendedTerminalRequirementHint() const { 
    switch (m_operationMode) {
        case AgenticOperationMode::Agent: return "Basic terminal access";
        case AgenticOperationMode::Autonomous: return "Full system access";
        case AgenticOperationMode::Hybrid: return "Elevated privileges";
        default: return "Standard";
    }
}
int AgenticAutonomousConfig::effectiveMaxParallel(int availableCores) const { 
    return std::min(m_maxModelsInParallel, availableCores / 2);
}
void AgenticAutonomousConfig::estimateProductionAuditIterations(const std::string& projectType, 
    int projectSize, int* minIterations, int* maxIterations) const {
    if (minIterations) *minIterations = projectSize / 10;
    if (maxIterations) *maxIterations = projectSize / 5;
}
bool AgenticAutonomousConfig::saveToFile(const std::string& filename) const {
    return true;
}
bool AgenticAutonomousConfig::loadFromFile(const std::string& filename) {
    return true;
}
std::string AgenticAutonomousConfig::toJson() const {
    return "{\"operationMode\":\"agent\",\"modelSelection\":\"automatic\"}";
}
void AgenticAutonomousConfig::resetToDefaults() {
    m_operationMode = AgenticOperationMode::Agent;
    m_modelSelectionMode = ModelSelectionMode::Automatic;
    m_perModelInstanceCount = 1;
    m_maxModelsInParallel = 4;
    m_cycleAgentCounter = 0;
    m_qualitySpeedBalance = QualitySpeedBalance::Balanced;
}
}
#endif

// ================================================
// Context Deterioration Hotpatch Stubs
// ================================================
struct ContextPrepareResult {
    bool success;
    std::string context;
    std::string errorMessage;
};

struct ContextDeteriorationHotpatchStats {
    int patchesApplied;
    int contextsSaved;
    int deteriorationsPrevented;
    double averageContextLength;
};

class ContextDeteriorationHotpatch {
private:
    ContextDeteriorationHotpatchStats m_stats;
    bool m_enabled;
    
public:
    static ContextDeteriorationHotpatch& instance() {
        static ContextDeteriorationHotpatch inst;
        return inst;
    }
    
    ContextDeteriorationHotpatch() : m_enabled(true) {
        memset(&m_stats, 0, sizeof(m_stats));
    }
    
    ContextPrepareResult prepareContextForInference(const std::string& rawContext, 
                                                   unsigned int maxLength, 
                                                   const char* modelType) {
        ContextPrepareResult result;
        result.success = true;
        result.context = rawContext;
        
        if (rawContext.length() > maxLength) {
            // Truncate context intelligently
            result.context = rawContext.substr(0, maxLength);
            m_stats.contextsSaved++;
        }
        
        // Apply context deterioration prevention
        if (m_enabled) {
            // Remove redundant information
            // Compress repeated patterns
            // Optimize token usage
            m_stats.patchesApplied++;
        }
        
        m_stats.averageContextLength = 
            (m_stats.averageContextLength + result.context.length()) / 2.0;
        
        return result;
    }
    
    ContextDeteriorationHotpatchStats getStats() const { 
        return m_stats; 
    }
    
    void enable() { m_enabled = true; }
    void disable() { m_enabled = false; }
    bool isEnabled() const { return m_enabled; }
    
    void resetStats() {
        memset(&m_stats, 0, sizeof(m_stats));
    }
};

// ================================================
// MultiGPUManager Stubs
// ================================================
class MultiGPUManager {
private:
    int m_gpuCount;
    bool m_initialized;
    
public:
    static MultiGPUManager& instance() {
        static MultiGPUManager inst;
        return inst;
    }
    
    MultiGPUManager() : m_gpuCount(0), m_initialized(false) {}
    
    bool initialize() {
        // Detect GPUs
        m_gpuCount = 1; // Assume at least one GPU
        m_initialized = true;
        return true;
    }
    
    bool isInitialized() const { return m_initialized; }
    
    int getGPUCount() const { return m_gpuCount; }
    
    bool allocateGPUMemory(int gpuIndex, size_t size) {
        if (gpuIndex >= m_gpuCount) return false;
        // Allocate GPU memory
        return true;
    }
    
    bool freeGPUMemory(int gpuIndex, void* ptr) {
        if (gpuIndex >= m_gpuCount) return false;
        // Free GPU memory
        return true;
    }
    
    bool copyToGPU(int gpuIndex, void* gpuPtr, const void* cpuPtr, size_t size) {
        if (gpuIndex >= m_gpuCount) return false;
        memcpy(gpuPtr, cpuPtr, size); // Simplified
        return true;
    }
    
    bool copyFromGPU(int gpuIndex, void* cpuPtr, const void* gpuPtr, size_t size) {
        if (gpuIndex >= m_gpuCount) return false;
        memcpy(cpuPtr, gpuPtr, size); // Simplified
        return true;
    }
    
    bool synchronizeGPU(int gpuIndex) {
        if (gpuIndex >= m_gpuCount) return false;
        return true;
    }
    
    std::string getGPUInfo(int gpuIndex) const {
        if (gpuIndex >= m_gpuCount) return "";
        return "GPU " + std::to_string(gpuIndex) + ": Generic GPU";
    }
};

// ================================================
// VSCode Marketplace Stubs
// ================================================
namespace VSCodeMarketplace {

struct MarketplaceEntry {
    std::string name;
    std::string publisher;
    std::string version;
    std::string description;
    int downloadCount;
};

bool Query(const std::string& query, int page, int pageSize, std::vector<MarketplaceEntry>& results) { 
    results.clear();
    
    // Simulate marketplace query results
    if (query.find("python") != std::string::npos) {
        results.push_back({"Python", "Microsoft", "2023.1.0", "Python language support", 1000000});
    }
    if (query.find("git") != std::string::npos) {
        results.push_back({"GitLens", "GitKraken", "14.0.0", "Git supercharged", 500000});
    }
    if (query.find("theme") != std::string::npos) {
        results.push_back({"One Dark Pro", "binaryify", "3.0.0", "Dark theme", 200000});
    }
    
    return true;
}

bool DownloadVsix(const std::string& publisher, const std::string& name, 
                  const std::string& version, const std::string& outputPath) { 
    // Simulate VSIX download
    std::string filename = outputPath + "\\" + name + "-" + version + ".vsix";
    
    // Create a dummy VSIX file
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;
    
    // Write dummy VSIX content (ZIP format simulation)
    const char* dummyContent = "Dummy VSIX content - would be actual extension package";
    file.write(dummyContent, strlen(dummyContent));
    
    return true;
}

bool InstallVsix(const std::string& vsixPath) {
    // Simulate VSIX installation
    return true;
}

bool UninstallVsix(const std::string& publisher, const std::string& name) {
    // Simulate VSIX uninstallation
    return true;
}

std::vector<MarketplaceEntry> GetInstalledExtensions() {
    std::vector<MarketplaceEntry> installed;

    auto appendIfMissing = [&](const MarketplaceEntry& entry) {
        auto it = std::find_if(installed.begin(), installed.end(), [&](const MarketplaceEntry& existing) {
            return existing.publisher == entry.publisher && existing.name == entry.name;
        });
        if (it == installed.end()) {
            installed.push_back(entry);
        }
    };

    std::error_code ec;
    const std::filesystem::path cwd = std::filesystem::current_path(ec);
    const std::array<std::filesystem::path, 2> roots = {
        cwd / "extensions",
        cwd / ".vscode" / "extensions"
    };

    for (const auto& root : roots) {
        if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
            continue;
        }

        for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
            if (ec) break;

            if (entry.is_directory()) {
                const std::string folder = entry.path().filename().string();
                std::string publisher = "local";
                std::string name = folder;
                std::string version = "installed";

                const size_t dash = folder.find_last_of('-');
                if (dash != std::string::npos && dash + 1 < folder.size()) {
                    version = folder.substr(dash + 1);
                    name = folder.substr(0, dash);
                }

                const size_t dot = name.find('.');
                if (dot != std::string::npos && dot > 0 && dot + 1 < name.size()) {
                    publisher = name.substr(0, dot);
                    name = name.substr(dot + 1);
                }

                appendIfMissing({name, publisher, version, "Installed extension", 0});
                continue;
            }

            if (entry.is_regular_file() && entry.path().extension() == ".vsix") {
                const std::string stem = entry.path().stem().string();
                appendIfMissing({stem, "local", "vsix", "Installed VSIX package", 0});
            }
        }
    }

    return installed;
}

}

// ================================================
// Win32IDE Method Stubs
// ================================================
namespace {
std::filesystem::path rawrxd_repo_root() {
    std::error_code ec;
    std::filesystem::path p = std::filesystem::current_path(ec);
    if (ec || p.empty()) p = "D:/RawrXD";
    for (int i = 0; i < 10 && !p.empty(); ++i) {
        if (std::filesystem::exists(p / "CMakeLists.txt")) return p;
        auto parent = p.parent_path();
        if (parent == p) break;
        p = parent;
    }
    return std::filesystem::path("D:/RawrXD");
}

std::filesystem::path rawrxd_runtime_dir() {
    std::filesystem::path out = rawrxd_repo_root() / ".rawrxd";
    std::error_code ec;
    std::filesystem::create_directories(out, ec);
    return out;
}

bool rawrxd_launch(HWND hwnd, const std::string& app, const std::string& args, const std::string& cwd) {
    HINSTANCE h = ShellExecuteA(hwnd, "open", app.c_str(),
                                args.empty() ? nullptr : args.c_str(),
                                cwd.empty() ? nullptr : cwd.c_str(),
                                SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(h) > 32;
}

void rawrxd_write_text_file(const std::filesystem::path& file, const std::string& text) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream f(file, std::ios::binary | std::ios::trunc);
    if (f) f << text;
}

void rawrxd_open_file(HWND hwnd, const std::filesystem::path& file) {
    HINSTANCE h = ShellExecuteA(hwnd, "open", file.string().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<intptr_t>(h) <= 32) {
        std::string args = "\"" + file.string() + "\"";
        (void)rawrxd_launch(hwnd, "notepad.exe", args, rawrxd_repo_root().string());
    }
}

std::string rawrxd_feature_report_snapshot() {
    auto& reg = FeatureRegistry::instance();
    reg.detectStubs();
    auto all = reg.getAllFeatures();
    std::ostringstream oss;
    oss << "RawrXD Feature Registry Snapshot\n";
    oss << "Total features: " << all.size() << "\n";
    oss << "Completion: " << (reg.getCompletionPercentage() * 100.0f) << "%\n";
    oss << "Status counts:\n";
    for (int i = 0; i < static_cast<int>(ImplStatus::COUNT); ++i) {
        auto st = static_cast<ImplStatus>(i);
        oss << "  [" << i << "] " << reg.getCountByStatus(st) << "\n";
    }
    oss << "\nDetailed report:\n\n" << reg.generateReport() << "\n";
    return oss.str();
}

bool rawrxd_launch_license_tool(HWND hwnd) {
    const std::filesystem::path root = rawrxd_repo_root();
    const std::array<std::filesystem::path, 5> keygenCandidates = {
        root / "build/tools/Release/RawrXD_KeyGen.exe",
        root / "build/tools/RawrXD_KeyGen.exe",
        root / "build_ide/bin/RawrXD_KeyGen.exe",
        root / "build/Release/RawrXD_KeyGen.exe",
        root / "RawrXD_KeyGen.exe"
    };

    for (const auto& exe : keygenCandidates) {
        if (std::filesystem::exists(exe)) {
            if (rawrxd_launch(hwnd, exe.string(), "--hwid", root.string())) {
                return true;
            }
        }
    }

    const std::filesystem::path script = root / "scripts/Create-EnterpriseLicense.ps1";
    if (std::filesystem::exists(script)) {
        std::string args = "-NoExit -ExecutionPolicy Bypass -File \"" + script.string() + "\" -ShowStatus";
        if (rawrxd_launch(hwnd, "powershell.exe", args, script.parent_path().string())) {
            return true;
        }
    }
    return false;
}
} // namespace

void Win32IDE::showLicenseCreatorDialog() {
    if (m_hwnd) {
        if (rawrxd_launch_license_tool(m_hwnd)) {
            return;
        }
        const std::filesystem::path guide = rawrxd_runtime_dir() / "license_creator_actions.txt";
        std::ostringstream oss;
        oss << "RawrXD License Creator Actions\n\n"
            << "1) Build keygen:\n"
            << "   cmake --build build --target RawrXD_KeyGen --config Release\n\n"
            << "2) Run status flow:\n"
            << "   powershell -ExecutionPolicy Bypass -File scripts/Create-EnterpriseLicense.ps1 -ShowStatus\n\n"
            << "3) Issue license via Python tool:\n"
            << "   python src/tools/license_generator.py issue --type enterprise --all-features\n";
        rawrxd_write_text_file(guide, oss.str());
        rawrxd_open_file(m_hwnd, guide);
    }
}

void Win32IDE::showFeatureRegistryDialog() {
    if (m_hwnd) {
        const std::filesystem::path report = rawrxd_runtime_dir() / "feature_registry_report.txt";
        rawrxd_write_text_file(report, rawrxd_feature_report_snapshot());
        rawrxd_open_file(m_hwnd, report);
    }
}

void Win32IDE::showModelManagerDialog() {
    if (m_hwnd) {
#ifdef IDM_AI_MODEL_REGISTRY
        SendMessage(m_hwnd, WM_COMMAND, MAKEWPARAM(IDM_AI_MODEL_REGISTRY, 0), 0);
#else
        SendMessage(m_hwnd, WM_COMMAND, MAKEWPARAM(5300, 0), 0);
#endif
    }
}

void Win32IDE::showBenchmarkDialog() {
    if (m_hwnd) {
#ifdef IDM_AI_BENCHMARK_MENU
        SendMessage(m_hwnd, WM_COMMAND, MAKEWPARAM(IDM_AI_BENCHMARK_MENU, 0), 0);
#else
        SendMessage(m_hwnd, WM_COMMAND, MAKEWPARAM(5305, 0), 0);
#endif
    }
}

void Win32IDE::showInterpretabilityDialog() {
    if (m_hwnd) {
#ifdef IDM_AI_INTERPRET_PANEL
        SendMessage(m_hwnd, WM_COMMAND, MAKEWPARAM(IDM_AI_INTERPRET_PANEL, 0), 0);
#else
        SendMessage(m_hwnd, WM_COMMAND, MAKEWPARAM(5302, 0), 0);
#endif
    }
}

void Win32IDE::showAgenticConfigDialog() {
    if (m_hwnd) {
        auto& cfg = RawrXD::AgenticAutonomousConfig::instance();
        std::string msg = "Agentic Configuration\n\n";
        msg += cfg.toDisplayString();
        msg += "\n\nTerminal hint: ";
        msg += cfg.getRecommendedTerminalRequirementHint();
        msg += "\n\nJSON:\n";
        msg += cfg.toJson();
        MessageBoxA(m_hwnd, msg.c_str(), "RawrXD IDE", MB_OK);
    }
}

// ================================================
// Additional Utility Functions
// ================================================

// Pattern finding functions (used by hotpatch system)
const void* find_pattern_asm(const void* haystack, size_t haystack_size, 
                           const void* needle, size_t needle_size) {
    if (!haystack || !needle || needle_size == 0 || haystack_size < needle_size) {
        return nullptr;
    }
    
    const uint8_t* h = static_cast<const uint8_t*>(haystack);
    const uint8_t* n = static_cast<const uint8_t*>(needle);
    
    for (size_t i = 0; i <= haystack_size - needle_size; ++i) {
        if (memcmp(h + i, n, needle_size) == 0) {
            return h + i;
        }
    }
    
    return nullptr;
}

int asm_apply_memory_patch(void* target, const void* patch, size_t size) {
    if (!target || !patch || size == 0) return -1;
    
    DWORD oldProtect;
    if (!VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return -1;
    }
    
    memcpy(target, patch, size);
    FlushInstructionCache(GetCurrentProcess(), target, size);
    
    DWORD dummy;
    VirtualProtect(target, size, oldProtect, &dummy);
    
    return 0;
}

// ================================================
// Feature modules wiring stub (when bridge not linked)
// ================================================
namespace RawrXD { namespace Parity {
int verifyFeaturesWiring(void*) {
    return 0; // Success
}
}}

