#pragma once
//=============================================================================
// SovereignCoreWrapper.hpp
// C++ interface for RawrXD_Sovereign_Core.asm x64 MASM routines
// Embeds autonomous agentic core into IDE
//=============================================================================

#include <cstdint>
#include <functional>
#include <vector>
#include <string>
#include <chrono>

namespace RawrXD::Sovereign {

//-----------------------------------------------------------------------------
// MASM Function Declarations (RawrXD_Sovereign_Core.obj exports)
//-----------------------------------------------------------------------------
extern "C" {
    // Agentic cycle execution
    void Sovereign_Pipeline_Cycle(void);
    
    // Lock management
    void AcquireSovereignLock(void);
    void ReleaseSovereignLock(void);
    
    // Agent coordination
    void CoordinateAgents(void);
    
    // Token observation (agent idx as 1st arg in RCX for x64)
    void ObserveTokenStream(uint64_t agentIdx);
    
    // Self-healing
    void HealSymbolResolution(const char* symbolName);
    
    // DMA validation
    uint64_t ValidateDMAAlignment(void);  // Returns 0=OK, 1=drift
    
    // Trigger full pipeline
    void RawrXD_Trigger_Chat(void);
    
    // Global state (resolve from .data section)
    extern uint64_t g_CycleCounter;
    extern uint64_t g_SovereignStatus;
    extern uint64_t g_SymbolHealCount;
    extern uint32_t g_ActiveAgentCount;
    extern uint64_t g_AgentRegistry[];  // [32] agents
}

//-----------------------------------------------------------------------------
// C++ Wrapper Class
//-----------------------------------------------------------------------------
class SovereignCore {
public:
    enum class Status : uint64_t {
        IDLE        = 0x00,
        COMPILING   = 0x02,
        FIXING      = 0x04,
        SYNCING     = 0x08
    };
    
    struct CycleStats {
        uint64_t cycleCount;
        uint64_t healCount;
        Status status;
        std::chrono::milliseconds elapsed;
    };
    
    struct AgentState {
        uint32_t agentIdx;
        uint64_t address;
        uint64_t lastHeartbeat;
        bool isAlive;
        bool hasError;
    };

    //--- Singleton access ---
    static SovereignCore& getInstance();
    
    //--- Initialization ---
    void initialize(uint32_t numAgents = 1);
    void shutdown();
    bool isInitialized() const;
    
    //--- Main cycle control ---
    void runCycle();                           // Execute one pipeline cycle
    void startAutonomousLoop();                // Launch background thread
    void stopAutonomousLoop();                 // Graceful shutdown
    bool isRunning() const;
    
    //--- Query state ---
    CycleStats getStats() const;
    Status getCurrentStatus() const;
    std::vector<AgentState> getAgentStates() const;
    
    //--- Manual pipeline stages ---
    void triggerFullChatPipeline();
    void triggerSelfHeal(const std::string& symbol);
    void validateAlignment();
    
    //--- Callbacks (fired after each cycle) ---
    using CycleCallback = std::function<void(const CycleStats&)>;
    void setOnCycleComplete(CycleCallback cb);
    
    //--- Thread control ---
    uint32_t getCycleIntervalMs() const;
    void setCycleIntervalMs(uint32_t ms);

private:
    SovereignCore();
    ~SovereignCore();
    
    bool m_initialized;
    bool m_running;
    void* m_loopThread;           // HANDLE (opaque to avoid Windows.h)
    uint32_t m_cycleIntervalMs;
    CycleCallback m_onCycleComplete;
    
    static SovereignCore* s_instance;
    
    void autonomousLoopProc();    // Thread entry
};

//-----------------------------------------------------------------------------
// IDE Integration Hook
//-----------------------------------------------------------------------------
class SovereignIDEBridge {
public:
    static SovereignIDEBridge& getInstance();
    
    // Called by agentic_engine.cpp on each engine cycle
    void onEngineCycle(const std::string& chatInput);
    
    // Query UI status display
    std::string getStatusDisplayLine() const;  // "Cycle: 42 | Status: SYNCING | Heals: 15"
    
    // Get latest tokens for rendering
    std::vector<uint64_t> getLatestTokens(uint32_t count = 5) const;
    
    // Register IDE UI callback for live updates
    using UIUpdateCallback = std::function<void(const std::string&)>;
    void setUIUpdateCallback(UIUpdateCallback cb);

private:
    SovereignIDEBridge();
    ~SovereignIDEBridge();
    
    SovereignCore& m_core;
    UIUpdateCallback m_uiCallback;
    std::vector<uint64_t> m_lastTokens;
    
    static SovereignIDEBridge* s_instance;
};

} // namespace RawrXD::Sovereign
