//=============================================================================
// SovereignCoreWrapper.cpp
// Implementation of C++ wrapper for RawrXD_Sovereign_Core.asm
// Thread-safe singleton + IDE integration
//=============================================================================

#include "SovereignCoreWrapper.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <cstring>

namespace RawrXD::Sovereign {

//=============================================================================
// SovereignCore — Singleton Implementation
//=============================================================================

SovereignCore* SovereignCore::s_instance = nullptr;
static std::mutex s_sovereignMutex;

SovereignCore& SovereignCore::getInstance() {
    if (!s_instance) {
        std::lock_guard<std::mutex> lock(s_sovereignMutex);
        if (!s_instance) {
            s_instance = new SovereignCore();
        }
    }
    return *s_instance;
}

SovereignCore::SovereignCore()
    : m_initialized(false),
      m_running(false),
      m_loopThread(nullptr),
      m_cycleIntervalMs(200),
      m_onCycleComplete(nullptr)
{
}

SovereignCore::~SovereignCore() {
    if (m_running) {
        stopAutonomousLoop();
    }
}

void SovereignCore::initialize(uint32_t numAgents) {
    std::lock_guard<std::mutex> lock(s_sovereignMutex);
    
    if (m_initialized) return;
    
    // Set agent count in MASM global
    g_ActiveAgentCount = (numAgents > 0) ? numAgents : 1;
    
    // Initialize agent registry (0 = no error)
    for (uint32_t i = 0; i < numAgents && i < 32; ++i) {
        g_AgentRegistry[i] = 0;  // Empty slot
    }
    
    m_initialized = true;
}

void SovereignCore::shutdown() {
    std::lock_guard<std::mutex> lock(s_sovereignMutex);
    stopAutonomousLoop();
    m_initialized = false;
}

bool SovereignCore::isInitialized() const {
    return m_initialized;
}

void SovereignCore::runCycle() {
    if (!m_initialized) {
        throw std::runtime_error("SovereignCore not initialized");
    }
    
    // Call MASM routine
    Sovereign_Pipeline_Cycle();
    
    // Capture stats
    CycleStats stats{
        g_CycleCounter,
        g_SymbolHealCount,
        static_cast<Status>(g_SovereignStatus & 0x0F),
        std::chrono::milliseconds(0)
    };
    
    // Fire callback if registered
    if (m_onCycleComplete) {
        m_onCycleComplete(stats);
    }
}

void SovereignCore::autonomousLoopProc() {
    while (m_running) {
        try {
            runCycle();
        } catch (const std::exception& e) {
            // Log error, continue
            // TODO: Wire to IDE error panel
        }
        
        // Sleep between cycles
        std::this_thread::sleep_for(
            std::chrono::milliseconds(m_cycleIntervalMs)
        );
    }
}

void SovereignCore::startAutonomousLoop() {
    std::lock_guard<std::mutex> lock(s_sovereignMutex);
    
    if (m_running || !m_initialized) return;
    
    m_running = true;
    
    // Launch worker thread
    m_loopThread = new std::thread(
        [this]() { this->autonomousLoopProc(); }
    );
    
    // Detach so destructor doesn't block
    static_cast<std::thread*>(m_loopThread)->detach();
}

void SovereignCore::stopAutonomousLoop() {
    m_running = false;
    
    if (m_loopThread) {
        // Give thread time to exit gracefully
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        delete static_cast<std::thread*>(m_loopThread);
        m_loopThread = nullptr;
    }
}

bool SovereignCore::isRunning() const {
    return m_running;
}

SovereignCore::CycleStats SovereignCore::getStats() const {
    return CycleStats{
        g_CycleCounter,
        g_SymbolHealCount,
        static_cast<Status>(g_SovereignStatus & 0x0F),
        std::chrono::milliseconds(0)
    };
}

SovereignCore::Status SovereignCore::getCurrentStatus() const {
    return static_cast<Status>(g_SovereignStatus & 0x0F);
}

std::vector<SovereignCore::AgentState> SovereignCore::getAgentStates() const {
    std::vector<AgentState> states;
    
    for (uint32_t i = 0; i < g_ActiveAgentCount && i < 32; ++i) {
        AgentState st{
            i,
            g_AgentRegistry[i],
            0,  // TODO: Extract heartbeat
            g_AgentRegistry[i] != 0,
            false  // TODO: Extract error flag
        };
        states.push_back(st);
    }
    
    return states;
}

void SovereignCore::triggerFullChatPipeline() {
    RawrXD_Trigger_Chat();
}

void SovereignCore::triggerSelfHeal(const std::string& symbol) {
    HealSymbolResolution(symbol.c_str());
}

void SovereignCore::validateAlignment() {
    ValidateDMAAlignment();
}

void SovereignCore::setOnCycleComplete(CycleCallback cb) {
    m_onCycleComplete = cb;
}

uint32_t SovereignCore::getCycleIntervalMs() const {
    return m_cycleIntervalMs;
}

void SovereignCore::setCycleIntervalMs(uint32_t ms) {
    m_cycleIntervalMs = (ms > 0) ? ms : 200;
}

//=============================================================================
// SovereignIDEBridge — IDE Integration
//=============================================================================

SovereignIDEBridge* SovereignIDEBridge::s_instance = nullptr;
static std::mutex s_bridgeMutex;

SovereignIDEBridge& SovereignIDEBridge::getInstance() {
    if (!s_instance) {
        std::lock_guard<std::mutex> lock(s_bridgeMutex);
        if (!s_instance) {
            s_instance = new SovereignIDEBridge();
        }
    }
    return *s_instance;
}

SovereignIDEBridge::SovereignIDEBridge()
    : m_core(SovereignCore::getInstance()),
      m_uiCallback(nullptr)
{
    // Register cycle callback
    m_core.setOnCycleComplete(
        [this](const SovereignCore::CycleStats& stats) {
            if (m_uiCallback) {
                m_uiCallback(this->getStatusDisplayLine());
            }
        }
    );
}

SovereignIDEBridge::~SovereignIDEBridge() {
}

void SovereignIDEBridge::onEngineCycle(const std::string& chatInput) {
    // Hook from agentic_engine.cpp:
    // When the IDE receives a chat message, dispatch to sovereign core
    if (!m_core.isInitialized()) {
        m_core.initialize(1);
    }
    
    // Run one sovereign cycle
    m_core.runCycle();
    
    // Capture latest token states
    m_lastTokens.clear();
    auto states = m_core.getAgentStates();
    for (const auto& st : states) {
        m_lastTokens.push_back(st.address);  // Token proxy
    }
}

std::string SovereignIDEBridge::getStatusDisplayLine() const {
    auto stats = m_core.getStats();
    
    char buf[256];
    const char* statusStr = "IDLE";
    switch (stats.status) {
        case SovereignCore::Status::COMPILING: statusStr = "COMPILING"; break;
        case SovereignCore::Status::FIXING: statusStr = "FIXING"; break;
        case SovereignCore::Status::SYNCING: statusStr = "SYNCING"; break;
        default: break;
    }
    
    snprintf(buf, sizeof(buf),
        "Cycle: %llu | Status: %s | Heals: %llu",
        stats.cycleCount,
        statusStr,
        stats.healCount
    );
    
    return std::string(buf);
}

std::vector<uint64_t> SovereignIDEBridge::getLatestTokens(uint32_t count) const {
    std::vector<uint64_t> result;
    uint32_t n = std::min(count, static_cast<uint32_t>(m_lastTokens.size()));
    
    for (uint32_t i = 0; i < n; ++i) {
        result.push_back(m_lastTokens[i]);
    }
    
    return result;
}

void SovereignIDEBridge::setUIUpdateCallback(UIUpdateCallback cb) {
    m_uiCallback = cb;
}

} // namespace RawrXD::Sovereign
