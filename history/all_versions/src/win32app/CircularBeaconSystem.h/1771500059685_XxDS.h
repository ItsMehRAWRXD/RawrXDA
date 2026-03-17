#pragma once
/*
 * RawrXD Circular Beacon System - Hot Reload & Agentic Connectivity
 * Implements omnidirectional pane communication with autonomous capabilities
 */

#include <windows.h>
#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>
#include <string>
#include <mutex>
#include <atomic>

// Forward declarations
class BeaconNode;
class AgenticExecutor;
class HotPatchEngine;

enum class BeaconDirection {
    BIDIRECTIONAL = 0,   // Both send and receive
    OUTBOUND_ONLY = 1,   // Only send signals
    INBOUND_ONLY = 2,    // Only receive signals
    MIDDLE_HUB = 3       // Central distribution point
};

enum class BeaconType {
    ENCRYPTION_PANE = 1,
    IDE_CORE = 2,
    JAVA_ENGINE = 3,
    ASM_KERNEL = 4,
    GPU_TUNER = 5,
    DEBUGGER = 6,
    AGENTIC_CORE = 7,
    HOTPATCH_ENGINE = 8,
    LSP_SERVER = 9,
    MONACO_EDITOR = 10,
    TERMINAL = 11,
    PROJECT_EXPLORER = 12,
    SWARM_ORCHESTRATOR = 13,
    TELEMETRY = 14,
    MARKET_PLACE = 15,
    VOICE_ENGINE = 16,
    TRANSCENDENCE = 17
};

struct BeaconSignal {
    BeaconType sourceType;
    BeaconType targetType;
    std::string command;
    std::string payload;
    uint32_t priority;
    uint64_t timestamp;
    bool isHotReload;
    bool requiresAgentic;
    void* userData;
};

using BeaconCallback = std::function<void(const BeaconSignal&)>;

class CircularBeaconSystem {
private:
    static CircularBeaconSystem* s_instance;
    static std::mutex s_mutex;
    
    std::unordered_map<BeaconType, std::unique_ptr<BeaconNode>> m_nodes;
    std::vector<std::pair<BeaconType, BeaconType>> m_connections;
    std::shared_ptr<AgenticExecutor> m_agenticCore;
    std::shared_ptr<HotPatchEngine> m_hotPatchEngine;
    
    std::atomic<bool> m_circularMode{false};
    std::atomic<bool> m_agenticEnabled{true};
    std::atomic<bool> m_hotReloadEnabled{true};
    
    HWND m_parentHwnd{nullptr};
    
public:
    static CircularBeaconSystem& getInstance();
    
    // Core initialization
    void initialize(HWND parentHwnd);
    void shutdown();
    
    // Node management
    void registerBeacon(BeaconType type, BeaconDirection direction, BeaconCallback callback);
    void unregisterBeacon(BeaconType type);
    
    // Circular connectivity
    void enableCircularMode(bool enable);
    void createDirectionalLink(BeaconType from, BeaconType to);
    void createBidirectionalLink(BeaconType a, BeaconType b);
    void setMiddleHub(BeaconType hubType);
    
    // Signal routing
    void broadcastSignal(const BeaconSignal& signal);
    void sendDirectSignal(BeaconType from, BeaconType to, const std::string& command, const std::string& payload);
    void sendAgenticCommand(BeaconType target, const std::string& autonomousAction);
    
    // Hot reload capabilities
    void enableHotReload(BeaconType type, bool enable);
    void triggerHotReload(BeaconType type, const std::string& newModulePath = "");
    void swapKernelEngine(BeaconType type, const std::string& newKernelPath);
    
    // Tuning and monitoring
    void enableNativeTuning(BeaconType type, bool enable);
    std::vector<BeaconType> getConnectedNodes();
    std::string getSystemStatus();
    void dumpBeaconGraph();
    
private:
    CircularBeaconSystem() = default;
    void routeSignal(const BeaconSignal& signal);
    void processAgenticRequest(const BeaconSignal& signal);
    void handleHotReload(const BeaconSignal& signal);
};

class BeaconNode {
private:
    BeaconType m_type;
    BeaconDirection m_direction;
    BeaconCallback m_callback;
    std::atomic<bool> m_hotReloadEnabled{false};
    std::atomic<bool> m_nativeTuningEnabled{false};
    std::atomic<bool> m_agenticEnabled{true};
    
    HMODULE m_currentModule{nullptr};
    std::string m_modulePath;
    
public:
    BeaconNode(BeaconType type, BeaconDirection direction, BeaconCallback callback);
    ~BeaconNode();
    
    // Signal processing
    void receiveSignal(const BeaconSignal& signal);
    void sendSignal(const BeaconSignal& signal);
    
    // Hot reload functionality
    void enableHotReload(bool enable);
    bool performHotReload(const std::string& newModulePath);
    void swapKernel(const std::string& newKernelPath);
    
    // Agentic integration
    void enableAgentic(bool enable);
    void processAutonomousCommand(const std::string& command, const std::string& context);
    
    // Native tuning
    void enableNativeTuning(bool enable);
    void performTuningOperation(const std::string& operation);
    
    // Getters
    BeaconType getType() const { return m_type; }
    BeaconDirection getDirection() const { return m_direction; }
    bool isHotReloadEnabled() const { return m_hotReloadEnabled; }
    bool isAgenticEnabled() const { return m_agenticEnabled; }
    bool isNativeTuningEnabled() const { return m_nativeTuningEnabled; }
};

// Convenience macros for IDE panes
#define REGISTER_BEACON_PANE(type, direction, callback) \
    CircularBeaconSystem::getInstance().registerBeacon(type, direction, callback)

#define SEND_BEACON_COMMAND(from, to, cmd, payload) \
    CircularBeaconSystem::getInstance().sendDirectSignal(from, to, cmd, payload)

#define SEND_AGENTIC_COMMAND(target, action) \
    CircularBeaconSystem::getInstance().sendAgenticCommand(target, action)

#define ENABLE_HOT_RELOAD(type) \
    CircularBeaconSystem::getInstance().enableHotReload(type, true)

#define SWAP_KERNEL_ENGINE(type, path) \
    CircularBeaconSystem::getInstance().swapKernelEngine(type, path)

// Standard beacon commands
#define BEACON_CMD_REFRESH "REFRESH"
#define BEACON_CMD_UPDATE "UPDATE"
#define BEACON_CMD_HOTRELOAD "HOTRELOAD"
#define BEACON_CMD_AGENTIC_REQUEST "AGENTIC_REQUEST"
#define BEACON_CMD_TUNE_ENGINE "TUNE_ENGINE"
#define BEACON_CMD_SWITCH_KERNEL "SWITCH_KERNEL"
#define BEACON_CMD_AUTONOMOUS_SCAN "AUTONOMOUS_SCAN"
#define BEACON_CMD_EXECUTE_PLAN "EXECUTE_PLAN"
#define BEACON_CMD_CIRCULAR_BROADCAST "CIRCULAR_BROADCAST"