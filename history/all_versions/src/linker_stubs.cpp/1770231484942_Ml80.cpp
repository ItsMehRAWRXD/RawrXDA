#include "linker_stubs.h"
#include <iostream>

// Real implementations are compiled from their respective source files:
// - MemoryCore -> memory_core.cpp (compiled)
// - AgenticEngine -> agentic_engine.cpp (compiled)
// - VSIXLoader -> vsix_loader.cpp (compiled)
// - CPUInferenceEngine -> cpu_inference_engine_stub.cpp (compiled)

// HotPatcher stub (not compiled, so provide implementation here)
HotPatcher::HotPatcher() {}
HotPatcher::~HotPatcher() {}
bool HotPatcher::ApplyPatch(const std::string& name, void* addr, const std::vector<unsigned char>& bytes) { 
    std::cout << "[HOTPATCH STUB] Apply patch: " << name << " at " << addr << "\n";
    return false;
}

// MemoryManager stub (not compiled, so provide implementation here)
MemoryManager::MemoryManager() {}
MemoryManager::~MemoryManager() {}
void MemoryManager::SetContextSize(unsigned long long size) {
    std::cout << "[MEMORY MANAGER STUB] SetContextSize: " << size << "\n";
}
std::vector<unsigned long long> MemoryManager::GetAvailableSizes() { 
    return {512, 2048, 8192, 32768, 131072}; 
}
std::string MemoryManager::GetModule(unsigned long long size) { 
    return "module_" + std::to_string(size) + "_kb";
}
void MemoryManager::ProcessWithContext(const std::string& text) {
    std::cout << "[MEMORY MANAGER STUB] ProcessWithContext: " << text.substr(0, 50) << "...\n";
}

// AdvancedFeatures stub
bool AdvancedFeatures::ApplyHotPatch(const std::string& target, const std::string& bytes, const std::string& options) { 
    std::cout << "[ADVANCED FEATURES STUB] ApplyHotPatch: " << target << "\n";
    return true;
}

// ToolRegistry stub
void ToolRegistry::inject_tools(AgentRequest& req) {
    std::cout << "[TOOL REGISTRY STUB] inject_tools\n";
}

// ReactServerGenerator stub
namespace RawrXD {
    bool ReactServerGenerator::Generate(const std::string& path, const ReactServerConfig& config) { 
        std::cout << "[REACT SERVER GENERATOR STUB] Generate: " << path << "\n";
        return true; 
    }
}

// IDEDiagnosticSystem stubs
namespace RawrXD {
    void IDEDiagnosticSystem::RegisterDiagnosticListener(std::function<void(const DiagnosticEvent&)> listener) {
        std::cout << "[IDE DIAGNOSTIC STUB] RegisterDiagnosticListener\n";
    }
    
    float IDEDiagnosticSystem::GetHealthScore() const {
        return 1.0f; // Perfect health
    }
    
    int IDEDiagnosticSystem::CountErrors() const {
        return 0;
    }
    
    int IDEDiagnosticSystem::CountWarnings() const {
        return 0;
    }
    
    std::string IDEDiagnosticSystem::GetPerformanceReport() const {
        return "IDE Performance: Nominal (stub)";
    }
}

// Memory System stub
void MemorySystem::PushContext(const std::string& context) {
    std::cout << "[MEMORY SYSTEM STUB] PushContext\n";
}

// Runtime functions (not extern "C" - declared in runtime_core.h)
void init_runtime() {
    std::cout << "[RUNTIME] Initialized (stub)\n";
}

void memory_system_init(unsigned long long size) {
    std::cout << "[RUNTIME] Memory system init: " << size << " (stub)\n";
}

void register_rawr_inference(void) {
    std::cout << "[RUNTIME] Register Rawr inference (stub)\n";
}

void register_sovereign_engines(void) {
    std::cout << "[RUNTIME] Register Sovereign engines (stub)\n";
}
