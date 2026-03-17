#pragma once
//=============================================================================
// AgenticEngineSovereignHook.h
// Integrates SovereignCore into agentic_engine.cpp chat loop
// Called from agentic_engine::chat() to inject autonomous cycles
//=============================================================================

#include "SovereignCoreWrapper.hpp"
#include <string>
#include <functional>

namespace RawrXD {

class AgenticEngineSovereignHook {
public:
    static AgenticEngineSovereignHook& getInstance();
    
    //--- Initialization ---
    void initialize();
    
    //--- Main integration point (call from agentic_engine::chat) ---
    // Returns augmented response with sovereign status inline
    std::string processWithSovereign(
        const std::string& userPrompt,
        std::function<std::string(const std::string&)> originalChatFn
    );
    
    //--- UI Update Sink (called by SovereignIDEBridge on cycle complete) ---
    void updateUI(const std::string& statusLine);
    
    //--- IDE State Query ---
    bool isSovereignEnabled() const;
    void setSovereignEnabled(bool enabled);
    
    //--- Diagnostic/Debug ---
    std::string getSovereignDiagnostics() const;

private:
    AgenticEngineSovereignHook();
    ~AgenticEngineSovereignHook();
    
    Sovereign::SovereignCore& m_core;
    Sovereign::SovereignIDEBridge& m_bridge;
    bool m_enabled;
    std::string m_lastStatus;
    
    static AgenticEngineSovereignHook* s_instance;
};

} // namespace RawrXD
