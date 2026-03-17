// Comprehensive linker stubs for missing symbols
#include <string>
#include <vector>
#include <memory>

// Forward declarations
class VSIXLoader;
class IDEWindow;

namespace AdvancedFeatures {
    void ApplyHotPatch(const std::string&, const std::string&, const std::string&) {}
}

namespace RawrXD {
    struct ReactServerConfig {};
    
    class ReactServerGenerator {
    public:
        static void Generate(const std::string& name, const ReactServerConfig& config) {}
    };
}

class ToolRegistry {
public:
    struct AgentRequest {};
    static void inject_tools(AgentRequest&) {}
};

// VSIXLoader member implementations
std::string VSIXLoader_GetCurrentEngine() { return "default"; }
std::vector<std::string> VSIXLoader_GetAvailableEngines() { return {}; }
void VSIXLoader_SwitchEngine(const std::string& engine) {}
void VSIXLoader_LoadEngine(const std::string& name, const std::string& path) {}
void VSIXLoader_UnloadEngine(const std::string& name) {}

// IDEWindow member implementations
void IDEWindow_UpdateTabTitle(int index, const std::wstring& title) {}

// C linkage stubs
extern "C" {
    void register_rawr_inference(void) {}
    void register_sovereign_engines(void) {}
}

// Explicit symbol definitions for linker to find
extern "C" {
    // VSIXLoader::GetCurrentEngine()
    void _ZN11VSIXLoader15GetCurrentEngineEv() {}
    
    // VSIXLoader::SwitchEngine(string const&)
    void _ZN11VSIXLoader11SwitchEngineERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE() {}
    
    // VSIXLoader::LoadEngine(string const&, string const&)
    void _ZN11VSIXLoader10LoadEngineERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEES5_() {}
    
    // VSIXLoader::UnloadEngine(string const&)
    void _ZN11VSIXLoader12UnloadEngineERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE() {}
    
    // VSIXLoader::GetAvailableEngines()
    void _ZN11VSIXLoader19GetAvailableEnginesEv() {}
}
