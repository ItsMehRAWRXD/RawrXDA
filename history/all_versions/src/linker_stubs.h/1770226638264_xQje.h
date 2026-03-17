#pragma once
#include <string>
#include <vector>

// HotPatcher stub
class HotPatcher {
public:
    HotPatcher();
    ~HotPatcher();
    bool ApplyPatch(const std::string&, void*, const std::vector<unsigned char>&);
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
    std::string chat(const std::string&);
    std::string planTask(const std::string&);
    std::string analyzeCode(const std::string&);
    std::string performCompleteCodeAudit(const std::string&);
    std::string getSecurityAssessment(const std::string&);
    std::string getPerformanceRecommendations(const std::string&);
    std::string getIDEHealthReport();
};

// VSIXLoader stub
class VSIXLoader {
public:
    bool LoadEngine(const std::string&, const std::string&);
    bool UnloadEngine(const std::string&);
    std::vector<std::string> GetAvailableEngines();
    std::string GetCurrentEngine();
    std::vector<std::string> GetAvailableMemoryModules();
    bool LoadMemoryModule(const std::string&, unsigned long long);
    bool UnloadMemoryModule(unsigned long long);
    bool UnloadPlugin(const std::string&);
    bool LoadPlugin(const std::string&);
    bool EnablePlugin(const std::string&);
    bool DisablePlugin(const std::string&);
    bool ReloadPlugin(const std::string&);
    std::vector<std::string> GetLoadedPlugins();
    std::string GetPluginHelp(const std::string&);
    void ConfigurePlugin(const std::string&, const void*);
};

// MemoryManager stub
class MemoryManager {
public:
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
struct AgentRequest {};
class ToolRegistry {
public:
    void inject_tools(AgentRequest&);
};

// Memory system stub
class MemorySystem {
public:
    void PushContext(const std::string&);
};

extern "C" {
    extern MemorySystem* g_memory_system;
    void memory_system_init(unsigned long long size);
    void register_rawr_inference();
    void register_sovereign_engines();
}
