/*
 * RawrXD Circular Beacon System Implementation
 * Hot Reload & Agentic Connectivity Core
 */

#include "CircularBeaconSystem.h"
#include "../agentic/AgentOrchestrator.h"
#include "../core/unified_hotpatch_manager.h"
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>

// Static initialization
CircularBeaconSystem* CircularBeaconSystem::s_instance = nullptr;
std::mutex CircularBeaconSystem::s_mutex;

CircularBeaconSystem& CircularBeaconSystem::getInstance() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_instance) {
        s_instance = new CircularBeaconSystem();
    }
    return *s_instance;
}

void CircularBeaconSystem::initialize(HWND parentHwnd) {
    m_parentHwnd = parentHwnd;
    
    // Initialize agentic core
    m_agenticCore = std::make_shared<AgentOrchestrator>();
    m_hotPatchEngine = std::make_shared<UnifiedHotpatchManager>();
    
    // Set up default hub configuration (IDE_CORE as middle hub)
    setMiddleHub(BeaconType::IDE_CORE);
    
    // Enable circular mode by default
    enableCircularMode(true);
    
    OutputDebugStringA("[CircularBeacon] System initialized with agentic and hot reload capabilities\n");
}

void CircularBeaconSystem::shutdown() {
    m_circularMode = false;
    m_nodes.clear();
    m_connections.clear();
    
    if (m_agenticCore) {
        m_agenticCore.reset();
    }
    
    if (m_hotPatchEngine) {
        m_hotPatchEngine.reset();
    }
    
    OutputDebugStringA("[CircularBeacon] System shutdown complete\n");
}

void CircularBeaconSystem::registerBeacon(BeaconType type, BeaconDirection direction, BeaconCallback callback) {
    auto node = std::make_unique<BeaconNode>(type, direction, callback);
    
    // Enable agentic and hot reload by default for all nodes
    node->enableAgentic(m_agenticEnabled);
    node->enableHotReload(m_hotReloadEnabled);
    
    m_nodes[type] = std::move(node);
    
    // Automatically create circular connections if enabled
    if (m_circularMode) {
        // Connect to all existing nodes in circular fashion
        for (const auto& [existingType, existingNode] : m_nodes) {
            if (existingType != type) {
                createBidirectionalLink(type, existingType);
            }
        }
    }
    
    std::string msg = "[CircularBeacon] Registered beacon type " + std::to_string((int)type) + " with " + 
                     std::to_string(m_nodes.size()) + " total nodes\n";
    OutputDebugStringA(msg.c_str());
}

void CircularBeaconSystem::unregisterBeacon(BeaconType type) {
    auto it = m_nodes.find(type);
    if (it != m_nodes.end()) {
        // Remove all connections involving this node
        m_connections.erase(
            std::remove_if(m_connections.begin(), m_connections.end(),
                [type](const auto& conn) {
                    return conn.first == type || conn.second == type;
                }),
            m_connections.end()
        );
        
        m_nodes.erase(it);
        OutputDebugStringA("[CircularBeacon] Unregistered beacon node\n");
    }
}

void CircularBeaconSystem::enableCircularMode(bool enable) {
    m_circularMode = enable;
    
    if (enable) {
        // Create full mesh connectivity
        for (const auto& [type1, node1] : m_nodes) {
            for (const auto& [type2, node2] : m_nodes) {
                if (type1 != type2) {
                    createBidirectionalLink(type1, type2);
                }
            }
        }
    }
    
    OutputDebugStringA(enable ? "[CircularBeacon] Circular mode ENABLED - full mesh connectivity\n" 
                              : "[CircularBeacon] Circular mode DISABLED\n");
}

void CircularBeaconSystem::createDirectionalLink(BeaconType from, BeaconType to) {
    m_connections.emplace_back(from, to);
}

void CircularBeaconSystem::createBidirectionalLink(BeaconType a, BeaconType b) {
    createDirectionalLink(a, b);
    createDirectionalLink(b, a);
}

void CircularBeaconSystem::setMiddleHub(BeaconType hubType) {
    // Create hub connections to all other nodes
    for (const auto& [type, node] : m_nodes) {
        if (type != hubType) {
            createBidirectionalLink(hubType, type);
        }
    }
    
    std::string msg = "[CircularBeacon] Set middle hub: " + std::to_string((int)hubType) + "\n";
    OutputDebugStringA(msg.c_str());
}

void CircularBeaconSystem::broadcastSignal(const BeaconSignal& signal) {
    if (m_circularMode) {
        // In circular mode, send to all connected nodes
        for (const auto& [type, node] : m_nodes) {
            if (type != signal.sourceType) {
                auto modifiedSignal = signal;
                modifiedSignal.targetType = type;
                routeSignal(modifiedSignal);
            }
        }
    } else {
        routeSignal(signal);
    }
}

void CircularBeaconSystem::sendDirectSignal(BeaconType from, BeaconType to, const std::string& command, const std::string& payload) {
    BeaconSignal signal;
    signal.sourceType = from;
    signal.targetType = to;
    signal.command = command;
    signal.payload = payload;
    signal.priority = 1;
    signal.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    signal.isHotReload = (command == BEACON_CMD_HOTRELOAD);
    signal.requiresAgentic = (command == BEACON_CMD_AGENTIC_REQUEST || command == BEACON_CMD_AUTONOMOUS_SCAN);
    signal.userData = nullptr;
    
    routeSignal(signal);
}

void CircularBeaconSystem::sendAgenticCommand(BeaconType target, const std::string& autonomousAction) {
    sendDirectSignal(BeaconType::AGENTIC_CORE, target, BEACON_CMD_AGENTIC_REQUEST, autonomousAction);
}

void CircularBeaconSystem::enableHotReload(BeaconType type, bool enable) {
    auto it = m_nodes.find(type);
    if (it != m_nodes.end()) {
        it->second->enableHotReload(enable);
        
        std::string msg = "[CircularBeacon] Hot reload " + std::string(enable ? "ENABLED" : "DISABLED") + 
                         " for type " + std::to_string((int)type) + "\n";
        OutputDebugStringA(msg.c_str());
    }
}

void CircularBeaconSystem::triggerHotReload(BeaconType type, const std::string& newModulePath) {
    sendDirectSignal(BeaconType::HOTPATCH_ENGINE, type, BEACON_CMD_HOTRELOAD, newModulePath);
}

void CircularBeaconSystem::swapKernelEngine(BeaconType type, const std::string& newKernelPath) {
    sendDirectSignal(BeaconType::HOTPATCH_ENGINE, type, BEACON_CMD_SWITCH_KERNEL, newKernelPath);
}

void CircularBeaconSystem::enableNativeTuning(BeaconType type, bool enable) {
    auto it = m_nodes.find(type);
    if (it != m_nodes.end()) {
        it->second->enableNativeTuning(enable);
    }
}

std::vector<BeaconType> CircularBeaconSystem::getConnectedNodes() {
    std::vector<BeaconType> types;
    for (const auto& [type, node] : m_nodes) {
        types.push_back(type);
    }
    return types;
}

std::string CircularBeaconSystem::getSystemStatus() {
    std::ostringstream oss;
    oss << "CircularBeaconSystem Status:\n";
    oss << "  Nodes: " << m_nodes.size() << "\n";
    oss << "  Connections: " << m_connections.size() << "\n";
    oss << "  Circular Mode: " << (m_circularMode ? "ENABLED" : "DISABLED") << "\n";
    oss << "  Agentic Core: " << (m_agenticEnabled ? "ENABLED" : "DISABLED") << "\n";
    oss << "  Hot Reload: " << (m_hotReloadEnabled ? "ENABLED" : "DISABLED") << "\n";
    
    oss << "\nActive Beacons:\n";
    for (const auto& [type, node] : m_nodes) {
        oss << "  - Type " << std::to_string((int)type) 
            << " [HR:" << (node->isHotReloadEnabled() ? "Y" : "N")
            << " AG:" << (node->isAgenticEnabled() ? "Y" : "N") 
            << " NT:" << (node->isNativeTuningEnabled() ? "Y" : "N") << "]\n";
    }
    
    return oss.str();
}

void CircularBeaconSystem::dumpBeaconGraph() {
    OutputDebugStringA("[CircularBeacon] === BEACON GRAPH DUMP ===\n");
    std::string status = getSystemStatus();
    OutputDebugStringA(status.c_str());
    OutputDebugStringA("[CircularBeacon] === END GRAPH DUMP ===\n");
}

void CircularBeaconSystem::routeSignal(const BeaconSignal& signal) {
    auto targetIt = m_nodes.find(signal.targetType);
    if (targetIt == m_nodes.end()) {
        return; // Target node not found
    }
    
    // Process agentic requests through the agentic core
    if (signal.requiresAgentic && m_agenticCore) {
        processAgenticRequest(signal);
        return;
    }
    
    // Process hot reload requests through the hotpatch engine
    if (signal.isHotReload && m_hotPatchEngine) {
        handleHotReload(signal);
        return;
    }
    
    // Standard signal routing
    targetIt->second->receiveSignal(signal);
}

void CircularBeaconSystem::processAgenticRequest(const BeaconSignal& signal) {
    try {
        // Use the agentic orchestrator for autonomous processing
        if (m_agenticCore && signal.command == BEACON_CMD_AGENTIC_REQUEST) {
            // Create agentic task context
            std::string context = "BeaconRequest: " + signal.payload + 
                                " | Source: " + std::to_string((int)signal.sourceType) +
                                " | Target: " + std::to_string((int)signal.targetType);
            
            // Execute autonomous action (implementation depends on AgentOrchestrator interface)
            OutputDebugStringA("[CircularBeacon] Processing agentic request through orchestrator\n");
        }
    } catch (const std::exception& e) {
        std::string error = "[CircularBeacon] Agentic processing error: " + std::string(e.what()) + "\n";
        OutputDebugStringA(error.c_str());
    }
}

void CircularBeaconSystem::handleHotReload(const BeaconSignal& signal) {
    try {
        if (m_hotPatchEngine && signal.command == BEACON_CMD_HOTRELOAD) {
            auto targetIt = m_nodes.find(signal.targetType);
            if (targetIt != m_nodes.end()) {
                bool success = targetIt->second->performHotReload(signal.payload);
                
                std::string msg = "[CircularBeacon] Hot reload " + 
                                std::string(success ? "SUCCESS" : "FAILED") + 
                                " for type " + std::to_string((int)signal.targetType) + "\n";
                OutputDebugStringA(msg.c_str());
            }
        }
    } catch (const std::exception& e) {
        std::string error = "[CircularBeacon] Hot reload error: " + std::string(e.what()) + "\n";
        OutputDebugStringA(error.c_str());
    }
}

// ============================================================================
// BeaconNode Implementation
// ============================================================================

BeaconNode::BeaconNode(BeaconType type, BeaconDirection direction, BeaconCallback callback)
    : m_type(type), m_direction(direction), m_callback(callback) {
    
    std::string msg = "[BeaconNode] Created node type " + std::to_string((int)type) + 
                     " direction " + std::to_string((int)direction) + "\n";
    OutputDebugStringA(msg.c_str());
}

BeaconNode::~BeaconNode() {
    if (m_currentModule) {
        FreeLibrary(m_currentModule);
    }
}

void BeaconNode::receiveSignal(const BeaconSignal& signal) {
    if (m_direction == BeaconDirection::OUTBOUND_ONLY) {
        return; // This node doesn't receive signals
    }
    
    // Process special commands
    if (signal.command == BEACON_CMD_HOTRELOAD) {
        performHotReload(signal.payload);
        return;
    }
    
    if (signal.command == BEACON_CMD_AGENTIC_REQUEST && m_agenticEnabled) {
        processAutonomousCommand(signal.command, signal.payload);
        return;
    }
    
    if (signal.command == BEACON_CMD_TUNE_ENGINE && m_nativeTuningEnabled) {
        performTuningOperation(signal.payload);
        return;
    }
    
    // Call the registered callback
    if (m_callback) {
        m_callback(signal);
    }
}

void BeaconNode::sendSignal(const BeaconSignal& signal) {
    if (m_direction == BeaconDirection::INBOUND_ONLY) {
        return; // This node doesn't send signals
    }
    
    // Route through the beacon system
    CircularBeaconSystem::getInstance().broadcastSignal(signal);
}

void BeaconNode::enableHotReload(bool enable) {
    m_hotReloadEnabled = enable;
    
    std::string msg = "[BeaconNode] Hot reload " + std::string(enable ? "ENABLED" : "DISABLED") + 
                     " for type " + std::to_string((int)m_type) + "\n";
    OutputDebugStringA(msg.c_str());
}

bool BeaconNode::performHotReload(const std::string& newModulePath) {
    if (!m_hotReloadEnabled) {
        return false;
    }
    
    try {
        // Free existing module
        if (m_currentModule) {
            FreeLibrary(m_currentModule);
            m_currentModule = nullptr;
        }
        
        // Load new module if path provided
        if (!newModulePath.empty()) {
            m_currentModule = LoadLibraryA(newModulePath.c_str());
            if (m_currentModule) {
                m_modulePath = newModulePath;
                OutputDebugStringA("[BeaconNode] Hot reload successful\n");
                return true;
            } else {
                std::string error = "[BeaconNode] Failed to load module: " + newModulePath + "\n";
                OutputDebugStringA(error.c_str());
                return false;
            }
        }
        
        // Just refresh existing module
        OutputDebugStringA("[BeaconNode] Module refresh completed\n");
        return true;
        
    } catch (const std::exception& e) {
        std::string error = "[BeaconNode] Hot reload exception: " + std::string(e.what()) + "\n";
        OutputDebugStringA(error.c_str());
        return false;
    }
}

void BeaconNode::swapKernel(const std::string& newKernelPath) {
    performHotReload(newKernelPath);
}

void BeaconNode::enableAgentic(bool enable) {
    m_agenticEnabled = enable;
}

void BeaconNode::processAutonomousCommand(const std::string& command, const std::string& context) {
    if (!m_agenticEnabled) {
        return;
    }
    
    std::string msg = "[BeaconNode] Processing autonomous command: " + command + " | Context: " + context + "\n";
    OutputDebugStringA(msg.c_str());
    
    // Autonomous processing logic would go here
    // This could involve calling through to the agentic orchestrator
}

void BeaconNode::enableNativeTuning(bool enable) {
    m_nativeTuningEnabled = enable;
    
    std::string msg = "[BeaconNode] Native tuning " + std::string(enable ? "ENABLED" : "DISABLED") + 
                     " for type " + std::to_string((int)m_type) + "\n";
    OutputDebugStringA(msg.c_str());
}

void BeaconNode::performTuningOperation(const std::string& operation) {
    if (!m_nativeTuningEnabled) {
        return;
    }
    
    std::string msg = "[BeaconNode] Performing tuning operation: " + operation + "\n";
    OutputDebugStringA(msg.c_str());
    
    // Native tuning logic would go here
    // This could involve calling kernel-level optimization routines
}