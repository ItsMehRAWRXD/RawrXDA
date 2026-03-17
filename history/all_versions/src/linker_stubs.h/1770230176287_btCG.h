#pragma once
#include <string>
#include <vector>
#include <functional>

// Forward declaration for nlohmann::json
namespace nlohmann {
    class json {};
}

// HotPatcher stub
class HotPatcher {
public:
    HotPatcher();
    ~HotPatcher();
    bool ApplyPatch(const std::string&, void*, const std::vector<unsigned char>&);
};

// Forward declaration for types
enum class ContextTier {
    TIER_512,
    TIER_2K,
    TIER_8K,
    TIER_32K,
    TIER_128K
};

// Real implementations (not stubs):
// - MemoryCore is in memory_core.h/cpp
// - AgenticEngine is in agentic_engine.h/cpp  
// - VSIXLoader is in vsix_loader.h/cpp

// MemoryManager stub (no real implementation)
class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager();
    void SetContextSize(unsigned long long);
    std::vector<unsigned long long> GetAvailableSizes();
    std::string GetModule(unsigned long long);
    void ProcessWithContext(const std::string&);
};

// AdvancedFeatures stub
class AdvancedFeatures {
public:
    static bool ApplyHotPatch(const std::string&, const std::string&, const std::string&);
};

// Tool Registry stub
struct AgentRequest {
    std::string prompt;
    std::string mode;
};
class ToolRegistry {
public:
    static void inject_tools(AgentRequest&);
};

// Memory system stub
class MemorySystem {
public:
    void PushContext(const std::string&);
};

// ReactServerGenerator stub
namespace RawrXD {
    struct DiagnosticEvent {
        std::string type;
        std::string message;
        int severity;
    };
    
    class IDEDiagnosticSystem {
    public:
        void RegisterDiagnosticListener(std::function<void(const DiagnosticEvent&)> listener);
        float GetHealthScore() const;
        int CountErrors() const;
        int CountWarnings() const;
        std::string GetPerformanceReport() const;
    };
    
    struct ReactServerConfig {
        std::string name;
        std::string description;
        bool include_ide_features = false;
        bool include_monaco_editor = false;
        bool include_agent_modes = false;
        bool include_hotpatch_controls = false;
    };

    class ReactServerGenerator {
    public:
        static bool Generate(const std::string& path, const ReactServerConfig& config);
    };
}
extern "C" {
    void init_runtime();
    void memory_system_init(unsigned long long size);
    void register_rawr_inference();
    void register_sovereign_engines();
    extern struct MemorySystem* g_memory_system;
}
