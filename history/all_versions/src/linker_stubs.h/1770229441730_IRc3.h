#pragma once
#include <string>
#include <vector>

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

// MemoryCore stub
class MemoryCore {
public:
    MemoryCore();
    ~MemoryCore();
    std::string GetStatsString();
    void PushContext(const std::string&);
    void SetContextSize(unsigned long long);
    std::vector<unsigned long long> GetAvailableSizes();
    std::string GetModule(unsigned long long);
    void ProcessWithContext(const std::string&);
    bool Allocate(ContextTier tier);
    void Wipe();
};

// CPU Inference stubs
namespace CPUInference {
    class CPUInferenceEngine {
    public:
        bool LoadModel(const std::string&);
    };
}

// AgenticEngine stub
class AgenticEngine {
public:
    AgenticEngine();
    ~AgenticEngine();
    void initialize();
    std::string chat(const std::string&);
    std::string planTask(const std::string&);
    std::string analyzeCode(const std::string&);
    std::string performCompleteCodeAudit(const std::string&);
    std::string getSecurityAssessment(const std::string&);
    std::string getPerformanceRecommendations(const std::string&);
    std::string getIDEHealthReport();
};

// VSIXLoader is in vsix_loader.h - real implementation compiled
// MemoryManager stub
class MemoryManager {
public:
    MemoryManager();
    ~MemoryManager();
    void SetContextSize(unsigned long long);
    std::vector<unsigned long long> GetAvailableSizes();
    std::string GetModule(unsigned long long);
    void ProcessWithContext(const std::string&);
};
    void SetContextSize(unsigned long long);
    std::vector<unsigned long long> GetAvailableSizes();
    std::string GetModule(unsigned long long);
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
    extern MemorySystem* g_memory_system;
    void memory_system_init(unsigned long long size);
    void register_rawr_inference();
    void register_sovereign_engines();
}
