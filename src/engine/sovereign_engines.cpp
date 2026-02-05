#include "engine_iface.h"
#include <string>
#include <thread>
#include <chrono>

// Mock implementation of the 800B engine to satisfy the API references and provide a "Pro" engine option
class Engine800B : public Engine {
public:
    std::string infer(const AgentRequest& req) override {
        // Simulate heavy compute
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::string prefix = "[Sovereign-800B]";
        if (req.deep_thinking) prefix += " [Thinking...]";
        
        if (req.mode == AgentMode::PLAN) {
             return prefix + " ### High-Level Architecture Strategy\n\n1. **Core Abstraction**: Use generic interfaces for hardware independence.\n2. **Scalability**: Implement sharding across GPUs.\n3. **Safety**: Enforce memory bounds.\n\nApproved.";
        }
        
        return prefix + " I have analyzed your request with high precision. The solution involves integrating the localized context modules with the global inference bus. \n\nConfidence: 99.8%";
    }
    
    bool load_model(const std::string& path) override { return false; }
    
    const char* name() override { return "Sovereign-800B"; }
};

// Also define a small one
class SovereignSmall : public Engine {
public:
    std::string infer(const AgentRequest& req) override {
        return "[Sovereign-Small] Quick response: " + req.prompt + " >> Processed.";
    }
    bool load_model(const std::string& path) override { return false; }
    const char* name() override { return "Sovereign-Small"; }
};

static Engine800B g_engine_800b;
static SovereignSmall g_engine_small;

void register_sovereign_engines() {
    EngineRegistry::register_engine(&g_engine_800b);
    EngineRegistry::register_engine(&g_engine_small);
}
