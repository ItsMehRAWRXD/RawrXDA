#include "linker_stubs.h"

// HotPatcher implementation
HotPatcher::HotPatcher() {}
HotPatcher::~HotPatcher() {}
bool HotPatcher::ApplyPatch(const std::string&, void*, const std::vector<unsigned char>&) { return false; }

// Real implementations are compiled from their respective source files:
// - MemoryCore -> memory_core.cpp
// - AgenticEngine -> agentic_engine.cpp  
// - VSIXLoader -> vsix_loader.cpp

// MemoryManager implementation (stub)
MemoryManager::MemoryManager() {}
MemoryManager::~MemoryManager() {}
void MemoryManager::SetContextSize(unsigned long long) {}
std::vector<unsigned long long> MemoryManager::GetAvailableSizes() { return {}; }
std::string MemoryManager::GetModule(unsigned long long) { return ""; }
void MemoryManager::ProcessWithContext(const std::string&) {}

// AdvancedFeatures implementation
bool AdvancedFeatures::ApplyHotPatch(const std::string&, const std::string&, const std::string&) { return true; }

// ToolRegistry stub
void ToolRegistry::inject_tools(AgentRequest&) {}

// ReactServerGenerator stubs
namespace RawrXD {
    bool ReactServerGenerator::Generate(const std::string& path, const ReactServerConfig& config) { 
        return true; 
    }
}

// MemorySystem stub
void MemorySystem::PushContext(const std::string&) {}

// External C functions
extern "C" {
    MemorySystem* g_memory_system = nullptr;
    
    void init_runtime() {}
    void memory_system_init(unsigned long long size) {}
    void register_rawr_inference(void) {}
    void register_sovereign_engines(void) {}
}
