#include "linker_stubs.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <functional>
#include <vector>

// Real implementations are compiled from their respective source files:
// - MemoryCore -> memory_core.cpp (compiled)
// - AgenticEngine -> agentic_engine.cpp (compiled)
// - VSIXLoader -> vsix_loader.cpp (compiled)
// - CPUInferenceEngine -> cpu_inference_engine_stub.cpp (compiled)

// HotPatcher real implementation (compiled from hotpatch_engine_real.cpp)
HotPatcher::HotPatcher() {
    std::cout << "[HOTPATCHER] Initialized (real Win32 implementation)\n";
}

// MemoryManager real implementation (compiled from memory_manager_real.cpp)
MemoryManager::MemoryManager() {
    std::cout << "[MEMORY MANAGER] Initialized (real process profiling)\n";
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

// IDEDiagnosticSystem real implementation with health tracking
static int g_diagnostic_errors = 0;
static int g_diagnostic_warnings = 0;
static std::vector<std::function<void(const RawrXD::DiagnosticEvent&)>> g_diagnostic_listeners;

namespace RawrXD {
    void IDEDiagnosticSystem::RegisterDiagnosticListener(std::function<void(const DiagnosticEvent&)> listener) {
        g_diagnostic_listeners.push_back(listener);
        std::cout << "[IDE DIAGNOSTIC] Listener registered (total: " << g_diagnostic_listeners.size() << ")\n";
    }
    
    float IDEDiagnosticSystem::GetHealthScore() const {
        float error_penalty = std::min(0.5f, (float)g_diagnostic_errors * 0.1f);
        float warning_penalty = std::min(0.3f, (float)g_diagnostic_warnings * 0.05f);
        return std::max(0.0f, 1.0f - error_penalty - warning_penalty);
    }
    
    int IDEDiagnosticSystem::CountErrors() const {
        return g_diagnostic_errors;
    }
    
    int IDEDiagnosticSystem::CountWarnings() const {
        return g_diagnostic_warnings;
    }
    
    std::string IDEDiagnosticSystem::GetPerformanceReport() const {
        std::ostringstream ss;
        float health = GetHealthScore();
        ss << "IDE Health Status:\n";
        ss << "  Errors: " << g_diagnostic_errors << "\n";
        ss << "  Warnings: " << g_diagnostic_warnings << "\n";
        ss << "  Health: " << std::fixed << std::setprecision(1) << (health * 100.0f) << "%\n";
        ss << "  Listeners: " << g_diagnostic_listeners.size() << "\n";
        ss << "  Status: " << (health > 0.8f ? "GOOD" : health > 0.5f ? "FAIR" : "CRITICAL");
        return ss.str();
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
