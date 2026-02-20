// =============================================================================
// Win32IDE_TranscendencePanel.cpp — Transcendence Architecture Panel (E → Ω)
// =============================================================================
//
// Wires all Transcendence Architecture subsystems into the Win32 IDE:
//   - Phase E: SelfHost Engine (Self-Compilation)
//   - Phase F: Hardware Synthesizer (FPGA/ASIC)
//   - Phase G: Mesh Brain (Distributed P2P)
//   - Phase H: Speciator Engine (Metamorphic Evolution)
//   - Phase I: Neural Bridge (Direct Cortex Interface)
//   - Phase Ω: Omega Orchestrator (Autonomous Pipeline)
//   - Transcendence Coordinator (Master Controller)
//
// Contents:
//   1. Lifecycle (initTranscendence / shutdownTranscendence)
//   2. Command handlers (IDM_TRANSCEND_* 6000-6099)
//   3. HTTP endpoint handlers (/api/transcendence/*)
//
// Pattern:  PatchResult-style results, no exceptions
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#include "Win32IDE.h"
#include "transcendence_coordinator.hpp"
#include <sstream>
#include <iomanip>

// ── Local copies of utility functions ────────────────────────────────────────
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

static std::string extractJsonString(const std::string& body, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t pos = body.find(searchKey);
    if (pos == std::string::npos) return "";
    pos = body.find(':', pos + searchKey.size());
    if (pos == std::string::npos) return "";
    pos = body.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    pos++;
    size_t end = body.find('"', pos);
    if (end == std::string::npos) return "";
    return body.substr(pos, end - pos);
}

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

} // namespace LocalServerUtil

// =============================================================================
//  Helpers: phase name and health level
// =============================================================================
static const char* phaseToString(rawrxd::TranscendencePhase phase) {
    switch (phase) {
        case rawrxd::TranscendencePhase::SelfHost:  return "E:SelfHost";
        case rawrxd::TranscendencePhase::HWSynth:   return "F:HWSynth";
        case rawrxd::TranscendencePhase::MeshBrain: return "G:MeshBrain";
        case rawrxd::TranscendencePhase::Speciator: return "H:Speciator";
        case rawrxd::TranscendencePhase::Neural:    return "I:Neural";
        case rawrxd::TranscendencePhase::Omega:     return "\xCE\xA9:Omega";
        default: return "None";
    }
}

static const char* healthToString(rawrxd::HealthLevel level) {
    switch (level) {
        case rawrxd::HealthLevel::Nominal:   return "NOMINAL";
        case rawrxd::HealthLevel::Degraded:  return "DEGRADED";
        case rawrxd::HealthLevel::Critical:  return "CRITICAL";
        case rawrxd::HealthLevel::Emergency: return "EMERGENCY";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// 1. LIFECYCLE
// =============================================================================

/// Initialize all Transcendence Architecture subsystems
void Win32IDE::initTranscendence() {
    auto& coordinator = rawrxd::TranscendenceCoordinator::instance();

    PatchResult r = coordinator.initializeAll();

    if (r.success) {
        logMessage("Transcendence", "[INIT] All phases E-\xCE\xA9 initialized");

        auto health = coordinator.checkHealth();
        std::ostringstream oss;
        oss << "[HEALTH] " << healthToString(health.level)
            << " — Active phases: " << health.activePhasesCount << "/6"
            << " — Score: " << std::fixed << std::setprecision(1)
            << (health.overallScore * 100.0f) << "%";
        logMessage("Transcendence", oss.str());
    } else {
        logMessage("Transcendence", std::string("[INIT] ") + (r.detail ? r.detail : "init failed"));
    }
}

/// Shutdown all Transcendence Architecture subsystems
void Win32IDE::shutdownTranscendence() {
    auto& coordinator = rawrxd::TranscendenceCoordinator::instance();
    coordinator.shutdownAll();
    logMessage("Transcendence", "[SHUTDOWN] All phases shut down");
}

// =============================================================================
// 2. COMMAND HANDLERS — IDM_TRANSCEND_* (6000-6099)
// =============================================================================

// IDM_TRANSCEND_INIT_ALL          = 6000
// IDM_TRANSCEND_SHUTDOWN_ALL      = 6001
// IDM_TRANSCEND_HEALTH_CHECK      = 6002
// IDM_TRANSCEND_EMERGENCY_STOP    = 6003
// IDM_TRANSCEND_RUN_CYCLE         = 6004
// IDM_TRANSCEND_DIAGNOSTICS       = 6005
// IDM_TRANSCEND_INIT_SELFHOST     = 6010
// IDM_TRANSCEND_INIT_HWSYNTH      = 6011
// IDM_TRANSCEND_INIT_MESHBRAIN    = 6012
// IDM_TRANSCEND_INIT_SPECIATOR    = 6013
// IDM_TRANSCEND_INIT_NEURAL       = 6014
// IDM_TRANSCEND_INIT_OMEGA        = 6015

bool Win32IDE::handleTranscendenceCommand(int cmdId) {
    auto& coord = rawrxd::TranscendenceCoordinator::instance();

    switch (cmdId) {
    case 6000: { // Init all
        initTranscendence();
        return true;
    }
    case 6001: { // Shutdown all
        shutdownTranscendence();
        return true;
    }
    case 6002: { // Health check
        auto health = coord.checkHealth();
        std::ostringstream oss;
        oss << "{\"level\":\"" << healthToString(health.level) << "\","
            << "\"activePhasesCount\":" << health.activePhasesCount << ","
            << "\"overallScore\":" << std::fixed << std::setprecision(4)
            << health.overallScore << ","
            << "\"totalOperations\":" << health.totalOperations << ","
            << "\"totalErrors\":" << health.totalErrors << ","
            << "\"phases\":[";
        for (uint32_t i = 0; i < health.activePhasesCount && i < 6; i++) {
            if (i > 0) oss << ",";
            oss << "{\"phase\":\"" << phaseToString(health.phases[i].phase) << "\","
                << "\"active\":" << (health.phases[i].active ? "true" : "false") << ","
                << "\"errors\":" << health.phases[i].errorCount << ","
                << "\"health\":" << std::fixed << std::setprecision(2)
                << health.phases[i].healthScore << "}";
        }
        oss << "]}";
        logMessage("Transcendence", "[HEALTH] " + oss.str());
        return true;
    }
    case 6003: { // Emergency stop
        PatchResult r = coord.emergencyStop();
        logMessage("Transcendence", std::string("[EMERGENCY] ") + (r.detail ? r.detail : "stopped"));
        return true;
    }
    case 6004: { // Run autonomous cycle
        const char* defaultReq = "Optimize inference pipeline for 4-bit quantization";
        auto result = coord.runAutonomousCycle(defaultReq, 50);
        std::ostringstream oss;
        oss << "[CYCLE] Created=" << result.tasksCreated
            << " Completed=" << result.tasksCompleted
            << " Failed=" << result.tasksFailed
            << " AvgScore=" << result.avgScore
            << " AllPassed=" << (result.allPassed ? "yes" : "no");
        logMessage("Transcendence", oss.str());
        return true;
    }
    case 6005: { // Diagnostics
        coord.dumpDiagnostics();
        logMessage("Transcendence", "[DIAG] Full diagnostics dumped to log");
        return true;
    }

    // Individual phase init
    case 6010: coord.initializePhase(rawrxd::TranscendencePhase::SelfHost);  return true;
    case 6011: coord.initializePhase(rawrxd::TranscendencePhase::HWSynth);   return true;
    case 6012: coord.initializePhase(rawrxd::TranscendencePhase::MeshBrain); return true;
    case 6013: coord.initializePhase(rawrxd::TranscendencePhase::Speciator); return true;
    case 6014: coord.initializePhase(rawrxd::TranscendencePhase::Neural);    return true;
    case 6015: coord.initializePhase(rawrxd::TranscendencePhase::Omega);     return true;

    default:
        return false;
    }
}

// =============================================================================
// 3. HTTP ENDPOINT HANDLERS — /api/transcendence/*
// =============================================================================

std::string Win32IDE::handleTranscendenceEndpoint(
    const std::string& method, const std::string& path, const std::string& body)
{
    auto& coord = rawrxd::TranscendenceCoordinator::instance();

    // GET /api/transcendence/status
    if (method == "GET" && path == "/api/transcendence/status") {
        auto health = coord.checkHealth();
        auto stats = coord.getStats();
        auto level = coord.getCurrentLevel();

        std::ostringstream oss;
        oss << "{\"active\":" << (coord.isActive() ? "true" : "false") << ","
            << "\"currentLevel\":\"" << phaseToString(level) << "\","
            << "\"health\":\"" << healthToString(health.level) << "\","
            << "\"activePhasesCount\":" << health.activePhasesCount << ","
            << "\"overallScore\":" << std::fixed << std::setprecision(4)
            << health.overallScore << ","
            << "\"stats\":{"
            << "\"eventsRouted\":" << stats.eventsRouted << ","
            << "\"phaseInits\":" << stats.phaseInits << ","
            << "\"phaseShutdowns\":" << stats.phaseShutdowns << ","
            << "\"healthChecks\":" << stats.healthChecks << ","
            << "\"emergencyStops\":" << stats.emergencyStops << ","
            << "\"autonomousCycles\":" << stats.autonomousCycles << ","
            << "\"crossPhaseOps\":" << stats.crossPhaseOps
            << "}}";

        return LocalServerUtil::buildHttpResponse(200, oss.str());
    }

    // POST /api/transcendence/init
    if (method == "POST" && path == "/api/transcendence/init") {
        std::string phase = LocalServerUtil::extractJsonString(body, "phase");
        PatchResult r;

        if (phase.empty() || phase == "all") {
            r = coord.initializeAll();
        } else if (phase == "selfhost")  { r = coord.initializePhase(rawrxd::TranscendencePhase::SelfHost); }
          else if (phase == "hwsynth")   { r = coord.initializePhase(rawrxd::TranscendencePhase::HWSynth); }
          else if (phase == "meshbrain") { r = coord.initializePhase(rawrxd::TranscendencePhase::MeshBrain); }
          else if (phase == "speciator") { r = coord.initializePhase(rawrxd::TranscendencePhase::Speciator); }
          else if (phase == "neural")    { r = coord.initializePhase(rawrxd::TranscendencePhase::Neural); }
          else if (phase == "omega")     { r = coord.initializePhase(rawrxd::TranscendencePhase::Omega); }
          else {
            return LocalServerUtil::buildHttpResponse(400,
                "{\"error\":\"Unknown phase: " + LocalServerUtil::escapeJson(phase) + "\"}");
        }

        std::string detail = r.detail ? r.detail : "";
        return LocalServerUtil::buildHttpResponse(200,
            "{\"success\":" + std::string(r.success ? "true" : "false") +
            ",\"detail\":\"" + LocalServerUtil::escapeJson(detail) + "\"}");
    }

    // POST /api/transcendence/shutdown
    if (method == "POST" && path == "/api/transcendence/shutdown") {
        PatchResult r = coord.shutdownAll();
        std::string detail = r.detail ? r.detail : "";
        return LocalServerUtil::buildHttpResponse(200,
            "{\"success\":" + std::string(r.success ? "true" : "false") +
            ",\"detail\":\"" + LocalServerUtil::escapeJson(detail) + "\"}");
    }

    // POST /api/transcendence/emergency-stop
    if (method == "POST" && path == "/api/transcendence/emergency-stop") {
        PatchResult r = coord.emergencyStop();
        std::string detail = r.detail ? r.detail : "";
        return LocalServerUtil::buildHttpResponse(200,
            "{\"success\":" + std::string(r.success ? "true" : "false") +
            ",\"detail\":\"" + LocalServerUtil::escapeJson(detail) + "\"}");
    }

    // POST /api/transcendence/run-cycle
    if (method == "POST" && path == "/api/transcendence/run-cycle") {
        std::string req = LocalServerUtil::extractJsonString(body, "requirement");
        if (req.empty()) req = "Optimize inference pipeline";

        auto result = coord.runAutonomousCycle(req.c_str(),
            static_cast<uint32_t>(req.size()));

        std::ostringstream oss;
        oss << "{\"tasksCreated\":" << result.tasksCreated
            << ",\"tasksCompleted\":" << result.tasksCompleted
            << ",\"tasksFailed\":" << result.tasksFailed
            << ",\"avgScore\":" << result.avgScore
            << ",\"allPassed\":" << (result.allPassed ? "true" : "false")
            << "}";

        return LocalServerUtil::buildHttpResponse(200, oss.str());
    }

    // GET /api/transcendence/health
    if (method == "GET" && path == "/api/transcendence/health") {
        auto health = coord.checkHealth();

        std::ostringstream oss;
        oss << "{\"level\":\"" << healthToString(health.level) << "\","
            << "\"activePhasesCount\":" << health.activePhasesCount << ","
            << "\"overallScore\":" << std::fixed << std::setprecision(4)
            << health.overallScore << ","
            << "\"phases\":[";

        for (int i = 0; i < 6; i++) {
            if (i > 0) oss << ",";
            oss << "{\"phase\":\"" << phaseToString(health.phases[i].phase) << "\","
                << "\"active\":" << (health.phases[i].active ? "true" : "false") << ","
                << "\"errors\":" << health.phases[i].errorCount << ","
                << "\"health\":" << std::fixed << std::setprecision(2)
                << health.phases[i].healthScore << "}";
        }
        oss << "]}";

        return LocalServerUtil::buildHttpResponse(200, oss.str());
    }

    // GET /api/transcendence/phase/:name
    if (method == "GET" && path.rfind("/api/transcendence/phase/", 0) == 0) {
        std::string phaseName = path.substr(25);  // After "/api/transcendence/phase/"
        rawrxd::TranscendencePhase phase = rawrxd::TranscendencePhase::None;

        if      (phaseName == "selfhost")  phase = rawrxd::TranscendencePhase::SelfHost;
        else if (phaseName == "hwsynth")   phase = rawrxd::TranscendencePhase::HWSynth;
        else if (phaseName == "meshbrain") phase = rawrxd::TranscendencePhase::MeshBrain;
        else if (phaseName == "speciator") phase = rawrxd::TranscendencePhase::Speciator;
        else if (phaseName == "neural")    phase = rawrxd::TranscendencePhase::Neural;
        else if (phaseName == "omega")     phase = rawrxd::TranscendencePhase::Omega;
        else {
            return LocalServerUtil::buildHttpResponse(404,
                "{\"error\":\"Unknown phase\"}");
        }

        auto status = coord.getPhaseStatus(phase);

        std::ostringstream oss;
        oss << "{\"phase\":\"" << phaseToString(status.phase) << "\","
            << "\"active\":" << (status.active ? "true" : "false") << ","
            << "\"errorCount\":" << status.errorCount << ","
            << "\"healthScore\":" << std::fixed << std::setprecision(2)
            << status.healthScore << "}";

        return LocalServerUtil::buildHttpResponse(200, oss.str());
    }

    // GET /api/transcendence/omega/stats
    if (method == "GET" && path == "/api/transcendence/omega/stats") {
        auto stats = coord.omega().getStats();

        std::ostringstream oss;
        oss << "{\"tasksCreated\":" << stats.tasksCreated
            << ",\"tasksCompleted\":" << stats.tasksCompleted
            << ",\"tasksFailed\":" << stats.tasksFailed
            << ",\"agentsActive\":" << stats.agentsActive
            << ",\"pipelinesRun\":" << stats.pipelinesRun
            << ",\"requirementsIngested\":" << stats.requirementsIngested
            << ",\"codeGenerated\":" << stats.codeGenerated
            << ",\"testsPassed\":" << stats.testsPassed
            << ",\"deployments\":" << stats.deployments
            << ",\"evolutions\":" << stats.evolutions
            << ",\"avgScoreBP\":" << stats.avgScoreBP
            << "}";

        return LocalServerUtil::buildHttpResponse(200, oss.str());
    }

    // GET /api/transcendence/omega/world
    if (method == "GET" && path == "/api/transcendence/omega/world") {
        auto world = coord.omega().getWorldModel();

        std::ostringstream oss;
        oss << "{\"codeUnits\":" << world.codeUnits
            << ",\"testUnits\":" << world.testUnits
            << ",\"deployCount\":" << world.deployCount
            << ",\"errorRate\":" << world.errorRate
            << ",\"fitness\":" << world.fitness
            << "}";

        return LocalServerUtil::buildHttpResponse(200, oss.str());
    }

    // POST /api/transcendence/omega/spawn-agent
    if (method == "POST" && path == "/api/transcendence/omega/spawn-agent") {
        int roleInt = LocalServerUtil::extractJsonInt(body, "role");
        auto role = static_cast<rawrxd::AgentRole>(roleInt);

        int32_t agentId = coord.omega().spawnAgent(role);

        return LocalServerUtil::buildHttpResponse(200,
            "{\"agentId\":" + std::to_string(agentId) + "}");
    }

    // GET /api/transcendence/neural/stats
    if (method == "GET" && path == "/api/transcendence/neural/stats") {
        auto stats = coord.neural().getStats();

        std::ostringstream oss;
        oss << "{\"samplesAcquired\":" << stats.samplesAcquired
            << ",\"fftsDone\":" << stats.fftsDone
            << ",\"intentsClassified\":" << stats.intentsClassified
            << ",\"eventsDetected\":" << stats.eventsDetected
            << ",\"commandsEncoded\":" << stats.commandsEncoded
            << ",\"phosphenesGenerated\":" << stats.phosphenesGenerated
            << ",\"hapticsGenerated\":" << stats.hapticsGenerated
            << ",\"calibrations\":" << stats.calibrations
            << ",\"adaptations\":" << stats.adaptations
            << "}";

        return LocalServerUtil::buildHttpResponse(200, oss.str());
    }

    return LocalServerUtil::buildHttpResponse(404,
        "{\"error\":\"Unknown transcendence endpoint\"}");
}
