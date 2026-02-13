#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

struct EngineInfo {
    std::string id;
    std::string name;
    std::string description;
    std::string path;
    bool loaded;
    bool supports_streaming;
    size_t max_model_size; // in GB
    std::vector<std::string> supported_formats; // gguf, safetensors, etc.
};

class EngineManager {
private:
    std::unordered_map<std::string, std::unique_ptr<EngineInfo>> engines_;
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
    EngineInfo* GetEngine(const std::string& engine_id);
    
    // 800B model support
    bool Load800BModel(const std::string& model_name);
    bool Setup5DriveLayout(const std::string& base_dir);
    bool VerifyDriveSetup();
    std::vector<std::string> GetDrivePaths();
    
    // Engine-specific features
    bool EnableStreaming(const std::string& engine_id);
    bool DisableStreaming(const std::string& engine_id);
    size_t GetOptimalContextSize(const std::string& engine_id);
    
    // Compiler integration
    bool RegisterCompiler(const std::string& compiler_id, const std::string& compiler_path);
    bool CompileWithEngine(const std::string& engine_id, const std::string& source_file);
    
    // Help system
    std::string GetEngineHelp(const std::string& engine_id) const;
    std::string GetAllEnginesHelp() const;
};

// Global engine manager
extern std::unique_ptr<EngineManager> g_engine_manager;
