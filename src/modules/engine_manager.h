#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>

struct EngineInfo {
    std::string id;
    std::string name;
    std::string description;
    std::string path;
    void* module_handle = nullptr;
    bool loaded = false;
    bool supports_streaming = false;
    size_t max_model_size = 0; // in GB
    std::vector<std::string> supported_formats; // gguf, safetensors, etc.
};

class EngineManager {
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<EngineInfo>> engines_;
    std::unordered_map<std::string, std::string> registered_compilers_;
    std::string current_engine_id_;
    std::string models_dir_; // Directory for 5-drive setup
    
public:
    EngineManager();
    ~EngineManager();
    
    // Engine management
    bool LoadEngine(const std::string& engine_path, const std::string& engine_id);
    bool UnloadEngine(const std::string& engine_id);
    bool SwitchEngine(const std::string& engine_id);
    std::vector<std::string> GetAvailableEngines() const;
    std::string GetCurrentEngine() const;
    const EngineInfo* GetEngine(const std::string& engine_id) const;
    EngineInfo* GetEngineMut(const std::string& engine_id);
    
    // 800B model support
    bool Load800BModel(const std::string& model_name);
    bool Setup5DriveLayout(const std::string& base_dir);
    bool VerifyDriveSetup();
    std::vector<std::string> GetDrivePaths();
    
    // Engine-specific features
    bool EnableStreaming(const std::string& engine_id);
    bool DisableStreaming(const std::string& engine_id);
    size_t GetOptimalContextSize(const std::string& engine_id) const;
    
    // Compiler integration
    bool RegisterCompiler(const std::string& compiler_id, const std::string& compiler_path);
    bool CompileWithEngine(const std::string& engine_id, const std::string& source_file);
    
    // Help system
    std::string GetEngineHelp(const std::string& engine_id) const;
    std::string GetAllEnginesHelp() const;
};

// Global engine manager
extern std::unique_ptr<EngineManager> g_engine_manager;
