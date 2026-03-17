#pragma once
#include <windows.h>
#include <string>
#include "agentic/ide_agent_bridge.hpp"
#include "tools/tool_registry.hpp"
#include "config/config_manager.hpp"

class Engine {
public:
    Engine();
    ~Engine();

    // Initializes subsystems (logging, config, UI, etc.)
    bool initialize(HINSTANCE hInst);

    // Main message loop
    int run();
    
    // Agentic functionality
    void executeWish(const std::string& wish);
    
    // Tool registry access
    ToolRegistry& getToolRegistry() { return toolRegistry_; }
    
    // Configuration access
    ConfigManager& getConfigManager() { return configManager_; }

private:
    HINSTANCE hInstance_;
    IDEAgentBridge agentBridge_;
    ToolRegistry toolRegistry_;
    ConfigManager configManager_;
    
    void initializeAgenticSystem();
    void loadConfiguration();
};