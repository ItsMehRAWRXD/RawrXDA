#include "linker_stubs.h"

// HotPatcher implementation
HotPatcher::HotPatcher() {}
HotPatcher::~HotPatcher() {}
bool HotPatcher::ApplyPatch(const std::string&, void*, const std::vector<unsigned char>&) { return false; }

// MemoryCore implementation
MemoryCore::MemoryCore() {}
MemoryCore::~MemoryCore() {}
std::string MemoryCore::GetStatsString() { return "Memory Core: Stub"; }
void MemoryCore::PushContext(const std::string&) {}
void MemoryCore::SetContextSize(unsigned long long) {}
std::vector<unsigned long long> MemoryCore::GetAvailableSizes() { return {}; }
std::string MemoryCore::GetModule(unsigned long long) { return ""; }
void MemoryCore::ProcessWithContext(const std::string&) {}
bool MemoryCore::Allocate(ContextTier) { return true; }
void MemoryCore::Wipe() {}

// CPU Inference Engine implementation
namespace CPUInference {
    bool CPUInferenceEngine::LoadModel(const std::string&) { return false; }
}

// AgenticEngine implementation
AgenticEngine::AgenticEngine() {}
AgenticEngine::~AgenticEngine() {}
void AgenticEngine::initialize() {}
std::string AgenticEngine::chat(const std::string&) { return "Agentic engine stub response"; }
std::string AgenticEngine::planTask(const std::string&) { return "Plan generated"; }
std::string AgenticEngine::analyzeCode(const std::string&) { return "Code analysis complete"; }
std::string AgenticEngine::performCompleteCodeAudit(const std::string&) { return "Audit complete"; }
std::string AgenticEngine::getSecurityAssessment(const std::string&) { return "Security: OK"; }
// VSIXLoader - using real implementation from vsix_loader.cpp
// MemoryManager implementation
MemoryManager::MemoryManager() {}
MemoryManager::~MemoryManager() {}
void MemoryManager::SetContextSize(unsigned long long) {}
std::vector<unsigned long long> MemoryManager::GetAvailableSizes() { return {}; }
std::string MemoryManager::GetModule(unsigned long long) { return ""; }
void MemoryManager::ProcessWithContext(const std::string&) {}

// AdvancedFeatures implementation
bool AdvancedFeatures::ApplyHotPatch(const std::string&, const std::string&, const std::string&) { return true; }

extern "C" {
    MemorySystem* g_memory_system = nullptr;
    
    void init_runtime() {}
    void memory_system_init(unsigned long long size) {}
    void register_rawr_inference(void) {}
    void register_sovereign_engines(void) {}
}

// ReactServerGenerator stubs
namespace RawrXD {
    bool ReactServerGenerator::Generate(const std::string& path, const ReactServerConfig& config) { 
        return true; 
    }
}

// IDEWindow stubs - removed (implementation in ide_window.cpp)


