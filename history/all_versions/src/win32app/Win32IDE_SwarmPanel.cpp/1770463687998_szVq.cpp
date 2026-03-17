// =============================================================================
// Win32IDE_SwarmPanel.cpp — Phase 11: Swarm IDE Integration
// =============================================================================
//
// Wires all Phase 11 Distributed Swarm Compilation subsystems into Win32IDE:
//   - 11A: SwarmCoordinator (Leader)
//   - 11B: SwarmWorker (Compute Node)
//   - Discovery, Scheduling, Consensus, Object Cache
//
// Contents:
//   1. Lifecycle (initPhase11 / shutdownPhase11)
//   2. Command handlers (IDM_SWARM_* 5132-5155)
//   3. HTTP endpoint handlers (/api/swarm/*)
//
// Pattern:  PatchResult-style results, no exceptions
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#include "Win32IDE.h"
#include "../core/swarm_coordinator.h"
#include "../core/swarm_worker.h"
#include "../core/enterprise_license.h"
#include <sstream>
#include <iomanip>

// ── Local copies of utility functions (same pattern as Phase 10) ──────────
// Duplicated because LocalServerUtil is static per-TU. DO NOT SIMPLIFY.
namespace LocalServerUtil {

static std::string escapeJson(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out.push_back(c); break;
        }
    }
    return out;
}

static std::string buildHttpResponse(int status, const std::string& body,
                                      const std::string& contentType = "application/json") {
    std::ostringstream oss;
    switch (status) {
        case 200: oss << "HTTP/1.1 200 OK\r\n"; break;
        case 204: oss << "HTTP/1.1 204 No Content\r\n"; break;
        case 400: oss << "HTTP/1.1 400 Bad Request\r\n"; break;
        case 403: oss << "HTTP/1.1 403 Forbidden\r\n"; break;
        case 404: oss << "HTTP/1.1 404 Not Found\r\n"; break;
        default:  oss << "HTTP/1.1 500 Internal Server Error\r\n"; break;
    }
    oss << "Content-Type: " << contentType << "\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    oss << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    oss << "Connection: close\r\n\r\n";
    oss << body;
    return oss.str();
}

// Extract a JSON string value by key from a flat JSON body
static std::string extractJsonString(const std::string& body, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = body.find(searchKey);
    if (pos == std::string::npos) return "";
    pos = body.find(':', pos + searchKey.size());
    if (pos == std::string::npos) return "";
    pos = body.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    pos++; // skip opening quote
    size_t end = body.find('"', pos);
    if (end == std::string::npos) return "";
    return body.substr(pos, end - pos);
}

// Extract a JSON integer value by key
static int extractJsonInt(const std::string& body, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = body.find(searchKey);
    if (pos == std::string::npos) return 0;
    pos = body.find(':', pos + searchKey.size());
    if (pos == std::string::npos) return 0;
    pos++;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) pos++;
    return std::atoi(body.c_str() + pos);
}

// Extract a JSON boolean value by key
static bool extractJsonBool(const std::string& body, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = body.find(searchKey);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + searchKey.size());
    if (pos == std::string::npos) return false;
    return body.find("true", pos) < body.find("false", pos);
}

} // namespace LocalServerUtil

// =============================================================================
// Helper: Check enterprise license for DistributedSwarm feature
// =============================================================================
static bool checkSwarmLicense() {
    // Check if DistributedSwarm feature is enabled in the enterprise license
    return EnterpriseLicense::isFeatureEnabled(LicenseFeature::DistributedSwarm);
}

// =============================================================================
// Swarm event callback → IDE output panel
// =============================================================================
static void swarmEventToOutput(const SwarmEvent& event, void* userData) {
    auto* ide = static_cast<Win32IDE*>(userData);
    if (!ide) return;

    const char* typeStr = "?";
    switch (event.type) {
        case SwarmEventType::NodeJoined:        typeStr = "NODE+"; break;
        case SwarmEventType::NodeLeft:          typeStr = "NODE-"; break;
        case SwarmEventType::NodeBlacklisted:   typeStr = "BAN"; break;
        case SwarmEventType::TaskAssigned:      typeStr = "ASSIGN"; break;
        case SwarmEventType::TaskCompleted:     typeStr = "DONE"; break;
        case SwarmEventType::TaskFailed:        typeStr = "FAIL"; break;
        case SwarmEventType::TaskRetried:       typeStr = "RETRY"; break;
        case SwarmEventType::BuildStarted:      typeStr = "BUILD▶"; break;
        case SwarmEventType::BuildCompleted:    typeStr = "BUILD✓"; break;
        case SwarmEventType::BuildFailed:       typeStr = "BUILD✗"; break;
        case SwarmEventType::ConsensusReached:  typeStr = "CONSENSUS✓"; break;
        case SwarmEventType::ConsensusRejected: typeStr = "CONSENSUS✗"; break;
        case SwarmEventType::HeartbeatTimeout:  typeStr = "TIMEOUT"; break;
        case SwarmEventType::AttestationSuccess: typeStr = "ATTEST✓"; break;
        case SwarmEventType::AttestationFailure: typeStr = "ATTEST✗"; break;
        case SwarmEventType::SwarmStarted:      typeStr = "START"; break;
        case SwarmEventType::SwarmStopped:      typeStr = "STOP"; break;
        default: break;
    }

    std::string msg = "[Swarm:" + std::string(typeStr) + "] " + event.message;
    ide->logMessage("Swarm", msg);
}

// Swarm log callback → IDE output panel
static void swarmLogToOutput(uint64_t taskId, uint32_t nodeSlot,
                              const char* logLine, void* userData) {
    auto* ide = static_cast<Win32IDE*>(userData);
    if (!ide) return;

    std::string msg = "[Swarm:LOG] Node " + std::to_string(nodeSlot) +
                      " Task " + std::to_string(taskId) + ": " + logLine;
    ide->logMessage("Swarm", msg);
}

// =============================================================================
// 1. LIFECYCLE
// =============================================================================

void Win32IDE::initPhase11() {
    if (!checkSwarmLicense()) {
        logMessage("Phase11", "DistributedSwarm feature not licensed — skipping init");
        m_phase11Initialized = false;
        return;
    }

    // Register callbacks
    SwarmCoordinator::instance().setEventCallback(swarmEventToOutput, this);
    SwarmCoordinator::instance().setLogCallback(swarmLogToOutput, this);

    m_phase11Initialized = true;
    logMessage("Phase11", "Swarm subsystem initialized (not yet started — use Swarm Start)");
}

void Win32IDE::shutdownPhase11() {
    if (!m_phase11Initialized) return;

    // Stop worker first, then coordinator
    if (SwarmWorker::instance().isRunning()) {
        SwarmWorker::instance().stop();
    }
    if (SwarmCoordinator::instance().isRunning()) {
        SwarmCoordinator::instance().stop();
    }

    m_phase11Initialized = false;
    logMessage("Phase11", "Swarm subsystem shut down");
}

// =============================================================================
// 2. COMMAND HANDLERS — Swarm Control (5132-5135)
// =============================================================================

void Win32IDE::cmdSwarmStatus() {
    auto& coord = SwarmCoordinator::instance();
    auto& worker = SwarmWorker::instance();

    std::ostringstream oss;
    oss << "════════════════════════════════════════════\n"
        << "  Phase 11: Distributed Swarm Status\n"
        << "════════════════════════════════════════════\n"
        << "  Licensed:       " << (checkSwarmLicense() ? "YES" : "NO") << "\n"
        << "  Coordinator:    " << (coord.isRunning() ? "RUNNING" : "STOPPED") << "\n"
        << "  Worker:         " << (worker.isRunning() ? "RUNNING" : "STOPPED") << "\n"
        << "  Mode:           " << static_cast<int>(coord.getMode()) << "\n"
        << "  Online Nodes:   " << coord.getOnlineNodeCount() << "\n"
        << "  Build Active:   " << (coord.isBuildRunning() ? "YES" : "NO") << "\n"
        << "  Build Progress: " << std::fixed << std::setprecision(1)
                                << (coord.getBuildProgress() * 100.0) << "%\n"
        << "  Discovery:      " << (coord.isDiscoveryEnabled() ? "ON" : "OFF") << "\n";

    auto stats = coord.getStats();
    oss << "  ── Statistics ──────────────────────────\n"
        << "  Total Tasks:    " << stats.totalTasks << "\n"
        << "  Completed:      " << stats.completedTasks << "\n"
        << "  Failed:         " << stats.failedTasks << "\n"
        << "  Running:        " << stats.runningTasks << "\n"
        << "  Pending:        " << stats.pendingTasks << "\n"
        << "  Parallel Speed: " << std::fixed << std::setprecision(2)
                                << stats.parallelSpeedup << "x\n"
        << "  Packets TX/RX:  " << stats.totalPacketsSent << "/"
                                << stats.totalPacketsRecv << "\n"
        << "  Cache Hits:     " << stats.objectCacheHits << "\n"
        << "════════════════════════════════════════════";
    appendToOutput(oss.str());
}

void Win32IDE::cmdSwarmStartLeader() {
    if (!checkSwarmLicense()) {
        appendToOutput("[Swarm] ERROR: DistributedSwarm not licensed");
        return;
    }

    if (SwarmCoordinator::instance().isRunning()) {
        appendToOutput("[Swarm] Already running — stop first");
        return;
    }

    DscConfig config;
    config.mode = SwarmMode::Leader;
    config.requireAttestation = true;

    if (SwarmCoordinator::instance().start(config)) {
        appendToOutput("[Swarm] Leader started on port " +
                       std::to_string(config.swarmPort));
    } else {
        appendToOutput("[Swarm] ERROR: Failed to start leader");
    }
}

void Win32IDE::cmdSwarmStartWorker() {
    if (!checkSwarmLicense()) {
        appendToOutput("[Swarm] ERROR: DistributedSwarm not licensed");
        return;
    }

    if (SwarmWorker::instance().isRunning()) {
        appendToOutput("[Swarm] Worker already running");
        return;
    }

    DscConfig config;
    config.mode = SwarmMode::Worker;

    if (SwarmWorker::instance().start(config)) {
        appendToOutput("[Swarm] Worker started (fitness: " +
                       std::to_string(SwarmWorker::instance().getFitnessScore()) + ")");
    } else {
        appendToOutput("[Swarm] ERROR: Failed to start worker");
    }
}

void Win32IDE::cmdSwarmStartHybrid() {
    if (!checkSwarmLicense()) {
        appendToOutput("[Swarm] ERROR: DistributedSwarm not licensed");
        return;
    }

    DscConfig config;
    config.mode = SwarmMode::Hybrid;
    config.requireAttestation = false; // Relaxed for single-machine testing

    if (SwarmCoordinator::instance().start(config)) {
        SwarmWorker::instance().start(config);
        SwarmWorker::instance().connectToLeader("127.0.0.1", config.swarmPort);
        appendToOutput("[Swarm] Hybrid mode started (leader + local worker)");
    } else {
        appendToOutput("[Swarm] ERROR: Failed to start hybrid mode");
    }
}

void Win32IDE::cmdSwarmStop() {
    SwarmWorker::instance().stop();
    SwarmCoordinator::instance().stop();
    appendToOutput("[Swarm] Stopped");
}

// =============================================================================
// 2. COMMAND HANDLERS — Node Management (5136-5139)
// =============================================================================

void Win32IDE::cmdSwarmListNodes() {
    auto nodes = SwarmCoordinator::instance().getNodes();

    std::ostringstream oss;
    oss << "════════════════════════════════════════════\n"
        << "  Swarm Nodes (" << nodes.size() << " total)\n"
        << "════════════════════════════════════════════\n";

    if (nodes.empty()) {
        oss << "  (no nodes)\n";
    } else {
        oss << "  Slot  State    Hostname         IP               Fitness  Tasks  CPU\n"
            << "  ────  ─────    ────────         ──               ───────  ─────  ───\n";
        for (const auto& n : nodes) {
            const char* stateStr = "?";
            switch (n.state) {
                case SwarmNodeState::Discovered: stateStr = "DISC"; break;
                case SwarmNodeState::Attesting:  stateStr = "AUTH"; break;
                case SwarmNodeState::Online:     stateStr = "OK  "; break;
                case SwarmNodeState::Busy:       stateStr = "BUSY"; break;
                case SwarmNodeState::Draining:   stateStr = "DRAIN"; break;
                case SwarmNodeState::Offline:    stateStr = "OFF "; break;
                case SwarmNodeState::Blacklisted: stateStr = "BAN "; break;
                default: break;
            }
            oss << "  " << std::setw(4) << n.slotIndex
                << "  " << stateStr
                << "    " << std::setw(16) << std::left << n.hostname
                << " " << std::setw(16) << std::left << n.ipAddress
                << " " << std::setw(7) << n.fitnessScore
                << "  " << n.activeTasks << "/" << n.maxConcurrentTasks
                << "  " << n.cpuLoadPercent << "%\n";
        }
    }
    oss << "════════════════════════════════════════════";
    appendToOutput(oss.str());
}

void Win32IDE::cmdSwarmAddNode() {
    // Read IP:port from clipboard
    std::string input;
    if (OpenClipboard(m_hwndMain)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* text = static_cast<char*>(GlobalLock(hData));
            if (text) { input = text; GlobalUnlock(hData); }
        }
        CloseClipboard();
    }

    if (input.empty()) {
        appendToOutput("[Swarm] Copy IP:PORT to clipboard, then run this command");
        return;
    }

    // Parse IP:port
    size_t colonPos = input.find(':');
    if (colonPos == std::string::npos) {
        appendToOutput("[Swarm] Invalid format. Expected IP:PORT (e.g. 192.168.1.100:11437)");
        return;
    }

    std::string ip = input.substr(0, colonPos);
    uint16_t port = static_cast<uint16_t>(std::atoi(input.substr(colonPos + 1).c_str()));

    if (SwarmCoordinator::instance().addNodeManual(ip.c_str(), port)) {
        appendToOutput("[Swarm] Node added: " + ip + ":" + std::to_string(port));
    } else {
        appendToOutput("[Swarm] Failed to add node: " + ip + ":" + std::to_string(port));
    }
}

void Win32IDE::cmdSwarmRemoveNode() {
    std::string input;
    if (OpenClipboard(m_hwndMain)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* text = static_cast<char*>(GlobalLock(hData));
            if (text) { input = text; GlobalUnlock(hData); }
        }
        CloseClipboard();
    }

    if (input.empty()) {
        appendToOutput("[Swarm] Copy node slot number to clipboard, then run this command");
        return;
    }

    uint32_t slot = static_cast<uint32_t>(std::atoi(input.c_str()));
    if (SwarmCoordinator::instance().removeNode(slot)) {
        appendToOutput("[Swarm] Node " + std::to_string(slot) + " removed");
    } else {
        appendToOutput("[Swarm] Failed to remove node " + std::to_string(slot));
    }
}

void Win32IDE::cmdSwarmBlacklistNode() {
    std::string input;
    if (OpenClipboard(m_hwndMain)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* text = static_cast<char*>(GlobalLock(hData));
            if (text) { input = text; GlobalUnlock(hData); }
        }
        CloseClipboard();
    }

    uint32_t slot = static_cast<uint32_t>(std::atoi(input.c_str()));
    if (SwarmCoordinator::instance().blacklistNode(slot, "Manual blacklist from IDE")) {
        appendToOutput("[Swarm] Node " + std::to_string(slot) + " blacklisted");
    } else {
        appendToOutput("[Swarm] Failed to blacklist node " + std::to_string(slot));
    }
}

// =============================================================================
// 2. COMMAND HANDLERS — Build (5140-5143)
// =============================================================================

void Win32IDE::cmdSwarmBuildFromSources() {
    // Use the current project's source files
    // Read file list from clipboard (one file per line)
    std::string input;
    if (OpenClipboard(m_hwndMain)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* text = static_cast<char*>(GlobalLock(hData));
            if (text) { input = text; GlobalUnlock(hData); }
        }
        CloseClipboard();
    }

    if (input.empty()) {
        appendToOutput("[Swarm] Copy source file list to clipboard (one per line), then run");
        return;
    }

    // Parse file list
    std::vector<std::string> files;
    std::istringstream iss(input);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line[0] != '#') {
            // Trim whitespace
            size_t start = line.find_first_not_of(" \t\r\n");
            size_t end = line.find_last_not_of(" \t\r\n");
            if (start != std::string::npos) {
                files.push_back(line.substr(start, end - start + 1));
            }
        }
    }

    if (files.empty()) {
        appendToOutput("[Swarm] No source files found in clipboard");
        return;
    }

    // Build DAG
    std::string args = "-O3 -std=c++20 -mavx2 -mfma";
    if (SwarmCoordinator::instance().buildDagFromSources(files, args, ".\\build")) {
        appendToOutput("[Swarm] DAG built: " + std::to_string(files.size()) + " source files");
    } else {
        appendToOutput("[Swarm] Failed to build DAG");
    }
}

void Win32IDE::cmdSwarmBuildFromCMake() {
    if (SwarmCoordinator::instance().buildDagFromCMake(".\\build")) {
        appendToOutput("[Swarm] DAG built from compile_commands.json");
    } else {
        appendToOutput("[Swarm] Failed: compile_commands.json not found in .\\build");
    }
}

void Win32IDE::cmdSwarmStartBuild() {
    if (SwarmCoordinator::instance().startBuild()) {
        appendToOutput("[Swarm] Build started — distributing tasks to " +
                       std::to_string(SwarmCoordinator::instance().getOnlineNodeCount()) +
                       " nodes");
    } else {
        appendToOutput("[Swarm] Failed to start build (no nodes or no tasks?)");
    }
}

void Win32IDE::cmdSwarmCancelBuild() {
    SwarmCoordinator::instance().cancelAllTasks();
    appendToOutput("[Swarm] Build cancelled");
}

// =============================================================================
// 2. COMMAND HANDLERS — Cache & Config (5144-5147)
// =============================================================================

void Win32IDE::cmdSwarmCacheStatus() {
    auto& coord = SwarmCoordinator::instance();
    uint32_t size = coord.objectCacheSize();
    appendToOutput("[Swarm] Object cache: " + std::to_string(size) + " entries");
}

void Win32IDE::cmdSwarmCacheClear() {
    SwarmCoordinator::instance().objectCacheClear();
    appendToOutput("[Swarm] Object cache cleared");
}

void Win32IDE::cmdSwarmShowConfig() {
    std::string json = SwarmCoordinator::instance().configToJson();
    appendToOutput("[Swarm] Configuration:\n" + json);
}

void Win32IDE::cmdSwarmToggleDiscovery() {
    auto& coord = SwarmCoordinator::instance();
    bool newState = !coord.isDiscoveryEnabled();
    coord.enableDiscovery(newState);
    appendToOutput("[Swarm] Discovery " + std::string(newState ? "enabled" : "disabled"));
}

// =============================================================================
// 2. COMMAND HANDLERS — Task Graph & Events (5148-5151)
// =============================================================================

void Win32IDE::cmdSwarmShowTaskGraph() {
    std::string json = SwarmCoordinator::instance().taskGraphToJson();
    appendToOutput("[Swarm] Task Graph:\n" + json);
}

void Win32IDE::cmdSwarmShowEvents() {
    auto events = SwarmCoordinator::instance().getRecentEvents(30);
    std::ostringstream oss;
    oss << "════════════════════════════════════════════\n"
        << "  Recent Swarm Events (" << events.size() << ")\n"
        << "════════════════════════════════════════════\n";
    for (const auto& e : events) {
        oss << "  [" << e.timestampMs << "] " << e.message << "\n";
    }
    oss << "════════════════════════════════════════════";
    appendToOutput(oss.str());
}

void Win32IDE::cmdSwarmShowStats() {
    std::string json = SwarmCoordinator::instance().statsToJson();
    appendToOutput("[Swarm] Statistics:\n" + json);
}

void Win32IDE::cmdSwarmResetStats() {
    SwarmCoordinator::instance().resetStats();
    appendToOutput("[Swarm] Statistics reset");
}

// =============================================================================
// 2. COMMAND HANDLERS — Worker Control (5152-5155)
// =============================================================================

void Win32IDE::cmdSwarmWorkerStatus() {
    appendToOutput(SwarmWorker::instance().getStatusString());
}

void Win32IDE::cmdSwarmWorkerConnect() {
    std::string input;
    if (OpenClipboard(m_hwndMain)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* text = static_cast<char*>(GlobalLock(hData));
            if (text) { input = text; GlobalUnlock(hData); }
        }
        CloseClipboard();
    }

    if (input.empty()) {
        appendToOutput("[Swarm] Copy leader IP:PORT to clipboard, then run");
        return;
    }

    size_t colonPos = input.find(':');
    std::string ip = (colonPos != std::string::npos) ? input.substr(0, colonPos) : input;
    uint16_t port = (colonPos != std::string::npos) ?
        static_cast<uint16_t>(std::atoi(input.substr(colonPos + 1).c_str())) :
        SWARM_DEFAULT_PORT;

    if (SwarmWorker::instance().connectToLeader(ip.c_str(), port)) {
        appendToOutput("[Swarm] Worker connected to " + ip + ":" + std::to_string(port));
    } else {
        appendToOutput("[Swarm] Worker failed to connect to " + ip + ":" + std::to_string(port));
    }
}

void Win32IDE::cmdSwarmWorkerDisconnect() {
    SwarmWorker::instance().disconnect();
    appendToOutput("[Swarm] Worker disconnected");
}

// Forward declaration for MASM fitness function
extern "C" uint32_t Swarm_ComputeNodeFitness(void);

void Win32IDE::cmdSwarmFitnessTest() {
    uint32_t fitness = Swarm_ComputeNodeFitness();
    appendToOutput("[Swarm] Local node fitness score: " + std::to_string(fitness) +
                   " (CPUID: cores*100 + AVX2*500 + AVX512*1000 + AESNI*200)");
}

// =============================================================================
// 3. HTTP ENDPOINT HANDLERS — /api/swarm/*
// =============================================================================

void Win32IDE::handleSwarmStatusEndpoint(SOCKET client) {
    std::string json = SwarmCoordinator::instance().toJson();
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleSwarmNodesEndpoint(SOCKET client) {
    std::string json = SwarmCoordinator::instance().nodesToJson();
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleSwarmTaskGraphEndpoint(SOCKET client) {
    std::string json = SwarmCoordinator::instance().taskGraphToJson();
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleSwarmStatsEndpoint(SOCKET client) {
    std::string json = SwarmCoordinator::instance().statsToJson();
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleSwarmEventsEndpoint(SOCKET client) {
    std::string json = SwarmCoordinator::instance().eventsToJson(100);
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleSwarmConfigEndpoint(SOCKET client) {
    std::string json = SwarmCoordinator::instance().configToJson();
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleSwarmWorkerEndpoint(SOCKET client) {
    std::string json = SwarmWorker::instance().toJson();
    std::string resp = LocalServerUtil::buildHttpResponse(200, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleSwarmStartEndpoint(SOCKET client, const std::string& body) {
    std::string mode = LocalServerUtil::extractJsonString(body, "mode");

    DscConfig config;
    if (mode == "leader") {
        config.mode = SwarmMode::Leader;
    } else if (mode == "worker") {
        config.mode = SwarmMode::Worker;
    } else {
        config.mode = SwarmMode::Hybrid;
    }

    config.requireAttestation = LocalServerUtil::extractJsonBool(body, "attestation");

    int port = LocalServerUtil::extractJsonInt(body, "port");
    if (port > 0) config.swarmPort = static_cast<uint16_t>(port);

    bool ok = false;
    if (config.mode == SwarmMode::Worker) {
        ok = SwarmWorker::instance().start(config);
        std::string leaderIp = LocalServerUtil::extractJsonString(body, "leaderIp");
        int leaderPort = LocalServerUtil::extractJsonInt(body, "leaderPort");
        if (ok && !leaderIp.empty()) {
            SwarmWorker::instance().connectToLeader(
                leaderIp.c_str(),
                leaderPort > 0 ? static_cast<uint16_t>(leaderPort) : SWARM_DEFAULT_PORT);
        }
    } else {
        ok = SwarmCoordinator::instance().start(config);
        if (ok && config.mode == SwarmMode::Hybrid) {
            SwarmWorker::instance().start(config);
            SwarmWorker::instance().connectToLeader("127.0.0.1", config.swarmPort);
        }
    }

    std::string json = ok ? "{\"success\":true,\"mode\":\"" + mode + "\"}" :
                            "{\"success\":false,\"error\":\"Failed to start\"}";
    std::string resp = LocalServerUtil::buildHttpResponse(ok ? 200 : 500, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleSwarmStopEndpoint(SOCKET client) {
    SwarmWorker::instance().stop();
    SwarmCoordinator::instance().stop();
    std::string resp = LocalServerUtil::buildHttpResponse(200, "{\"success\":true}");
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleSwarmAddNodeEndpoint(SOCKET client, const std::string& body) {
    std::string ip = LocalServerUtil::extractJsonString(body, "ip");
    int port = LocalServerUtil::extractJsonInt(body, "port");
    if (port <= 0) port = SWARM_DEFAULT_PORT;

    bool ok = SwarmCoordinator::instance().addNodeManual(
        ip.c_str(), static_cast<uint16_t>(port));

    std::string json = ok ?
        "{\"success\":true,\"ip\":\"" + ip + "\",\"port\":" + std::to_string(port) + "}" :
        "{\"success\":false,\"error\":\"Failed to add node\"}";
    std::string resp = LocalServerUtil::buildHttpResponse(ok ? 200 : 500, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleSwarmBuildEndpoint(SOCKET client, const std::string& body) {
    std::string buildDir = LocalServerUtil::extractJsonString(body, "buildDir");
    if (buildDir.empty()) buildDir = ".\\build";

    bool dagOk = SwarmCoordinator::instance().buildDagFromCMake(buildDir.c_str());
    bool buildOk = dagOk && SwarmCoordinator::instance().startBuild();

    std::string json = buildOk ?
        "{\"success\":true,\"message\":\"Build started\"}" :
        "{\"success\":false,\"error\":\"" +
            std::string(dagOk ? "No nodes available" : "Failed to build DAG") + "\"}";
    std::string resp = LocalServerUtil::buildHttpResponse(buildOk ? 200 : 500, json);
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleSwarmCancelEndpoint(SOCKET client) {
    SwarmCoordinator::instance().cancelAllTasks();
    std::string resp = LocalServerUtil::buildHttpResponse(200, "{\"success\":true}");
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleSwarmCacheClearEndpoint(SOCKET client) {
    SwarmCoordinator::instance().objectCacheClear();
    std::string resp = LocalServerUtil::buildHttpResponse(200,
        "{\"success\":true,\"message\":\"Cache cleared\"}");
    send(client, resp.c_str(), (int)resp.size(), 0);
}

// Unified Phase 11 status endpoint
void Win32IDE::handlePhase11StatusEndpoint(SOCKET client) {
    std::ostringstream oss;
    oss << "{"
        << "\"phase\":11,"
        << "\"name\":\"Distributed Swarm Compilation\","
        << "\"initialized\":" << (m_phase11Initialized ? "true" : "false") << ","
        << "\"coordinator\":" << SwarmCoordinator::instance().toJson() << ","
        << "\"worker\":" << SwarmWorker::instance().toJson() << ","
        << "\"stats\":" << SwarmCoordinator::instance().statsToJson()
        << "}";
    std::string resp = LocalServerUtil::buildHttpResponse(200, oss.str());
    send(client, resp.c_str(), (int)resp.size(), 0);
}
