// ============================================================================
// Win32IDE_DualAgentPanel.cpp — Phase 41: Dual-Agent Orchestrator Endpoints
// ============================================================================
//
// HTTP endpoint handlers for the Architect + Coder dual-agent system.
// Routes: /api/agent/dual/{init,shutdown,status,handoff,submit}
//         /api/phase41/status
//
// Backend: RawrXD_DualAgent_Orchestrator.asm via model_bridge_x64.asm
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include "../core/dual_agent_session.hpp"
#include <winsock2.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>

// Re-use the buildHttpResponse and extractJson* helpers from Win32IDE_LocalServer.cpp
// They are file-static there, so we define minimal local versions here.

namespace DualAgentUtil {

static std::string buildHttpResponse(int status, const std::string& body,
                                      const std::string& contentType = "application/json") {
    std::ostringstream oss;
    switch (status) {
        case 200: oss << "HTTP/1.1 200 OK\r\n"; break;
        case 400: oss << "HTTP/1.1 400 Bad Request\r\n"; break;
        case 404: oss << "HTTP/1.1 404 Not Found\r\n"; break;
        case 409: oss << "HTTP/1.1 409 Conflict\r\n"; break;
        case 500: oss << "HTTP/1.1 500 Internal Server Error\r\n"; break;
        case 507: oss << "HTTP/1.1 507 Insufficient Storage\r\n"; break;
        default:  oss << "HTTP/1.1 " << status << " Error\r\n"; break;
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

static bool extractString(const std::string& body, const std::string& key, std::string& out) {
    std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < body.size() && std::isspace((unsigned char)body[pos])) pos++;
    if (pos >= body.size() || body[pos] != '"') return false;
    pos++;
    std::string result;
    while (pos < body.size()) {
        char c = body[pos++];
        if (c == '\\' && pos < body.size()) {
            char esc = body[pos++];
            switch (esc) {
                case '"':  result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case 'n':  result.push_back('\n'); break;
                case 'r':  result.push_back('\r'); break;
                default:   result.push_back(esc); break;
            }
            continue;
        }
        if (c == '"') break;
        result.push_back(c);
    }
    out = result;
    return true;
}

static bool extractInt(const std::string& body, const std::string& key, int& out) {
    std::string pattern = "\"" + key + "\"";
    auto pos = body.find(pattern);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < body.size() && std::isspace((unsigned char)body[pos])) pos++;
    size_t start = pos;
    while (pos < body.size() && (std::isdigit((unsigned char)body[pos]) || body[pos] == '-')) pos++;
    if (start == pos) return false;
    try { out = std::stoi(body.substr(start, pos - start)); } catch (...) { return false; }
    return true;
}

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
            default:   out += c; break;
        }
    }
    return out;
}

} // namespace DualAgentUtil

// ============================================================================
// POST /api/agent/dual/init — Initialize dual-agent swarm
// Body: { "architect_profile": 20, "coder_profile": 5 }
// ============================================================================
void Win32IDE::handleDualAgentInitEndpoint(SOCKET client, const std::string& body) {
    using namespace DualAgentUtil;

    if (m_dualAgentInitialized) {
        std::string resp = buildHttpResponse(409,
            R"({"error":"Dual-agent swarm already initialized","hint":"POST /api/agent/dual/shutdown first"})");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    // Parse profile indices (defaults: architect=20 [800B DualEngine], coder=5 [llama3.1:8b])
    int archIdx = 20, coderIdx = 5;
    extractInt(body, "architect_profile", archIdx);
    extractInt(body, "coder_profile", coderIdx);

    IDELogger::info("[Phase41] Dual-agent init: architect_profile=%d, coder_profile=%d", archIdx, coderIdx);

#ifdef RAWR_HAS_MASM
    // Initialize model bridge if needed
    static bool s_bridgeInit = false;
    if (!s_bridgeInit) {
        s_bridgeInit = (ModelBridge_Init() == 0);
        if (!s_bridgeInit) {
            IDELogger::error("[Phase41] ModelBridge_Init failed");
            std::string resp = buildHttpResponse(500,
                R"({"error":"ModelBridge_Init failed","detail":"Cannot initialize MASM model bridge"})");
            send(client, resp.c_str(), (int)resp.size(), 0);
            return;
        }
    }

    uint64_t result = Swarm_Init(static_cast<uint32_t>(archIdx), static_cast<uint32_t>(coderIdx));
    if (result != SWARM_OK) {
        IDELogger::error("[Phase41] Swarm_Init failed: code=%llu (%s)",
                        result, GetSwarmErrorName(result));
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            R"({"error":"Swarm init failed","code":%llu,"reason":"%s"})",
            result, GetSwarmErrorName(result));
        std::string resp = buildHttpResponse(500, buf);
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    m_dualAgentInitialized = true;

    // Build success response with swarm details
    MasmSwarmState* state = reinterpret_cast<MasmSwarmState*>(Swarm_GetState());

    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        R"({"success":true,"message":"Dual-agent swarm initialized",)"
        R"("architect":{"profile_id":%d,"agent_id":0},)"
        R"("coder":{"profile_id":%d,"agent_id":1},)"
        R"("ring_size_mb":%llu,"bridge":"masm-x64-swarm"})",
        archIdx, coderIdx,
        state ? state->ring_size / (1024ULL * 1024ULL) : 0ULL);

    IDELogger::info("[Phase41] Swarm initialized: ring=%lluMB", state ? state->ring_size / (1024ULL * 1024ULL) : 0ULL);
    std::string resp = buildHttpResponse(200, buf);
    send(client, resp.c_str(), (int)resp.size(), 0);
#else
    m_dualAgentInitialized = true;
    IDELogger::info("[Phase41] Dual-agent init (cpp fallback)");
    std::string resp = buildHttpResponse(200,
        R"({"success":true,"message":"Dual-agent swarm initialized (cpp fallback)","bridge":"cpp-fallback"})");
    send(client, resp.c_str(), (int)resp.size(), 0);
#endif
}

// ============================================================================
// POST /api/agent/dual/shutdown — Shutdown dual-agent swarm
// ============================================================================
void Win32IDE::handleDualAgentShutdownEndpoint(SOCKET client) {
    using namespace DualAgentUtil;

    if (!m_dualAgentInitialized) {
        std::string resp = buildHttpResponse(409,
            R"({"error":"Swarm not initialized"})");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    IDELogger::info("[Phase41] Dual-agent shutdown requested");

#ifdef RAWR_HAS_MASM
    uint64_t result = Swarm_Shutdown();
    m_dualAgentInitialized = false;

    if (result != SWARM_OK) {
        IDELogger::error("[Phase41] Swarm_Shutdown failed: code=%llu (%s)",
                        result, GetSwarmErrorName(result));
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            R"({"error":"Swarm shutdown failed","code":%llu,"reason":"%s"})",
            result, GetSwarmErrorName(result));
        std::string resp = buildHttpResponse(500, buf);
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    IDELogger::info("[Phase41] Swarm shut down cleanly");
    std::string resp = buildHttpResponse(200,
        R"({"success":true,"message":"Dual-agent swarm shut down cleanly"})");
    send(client, resp.c_str(), (int)resp.size(), 0);
#else
    m_dualAgentInitialized = false;
    std::string resp = buildHttpResponse(200,
        R"({"success":true,"message":"Swarm shut down (cpp fallback)"})");
    send(client, resp.c_str(), (int)resp.size(), 0);
#endif
}

// ============================================================================
// GET /api/agent/dual/status — Get swarm + per-agent status
// ============================================================================
void Win32IDE::handleDualAgentStatusEndpoint(SOCKET client) {
    using namespace DualAgentUtil;

#ifdef RAWR_HAS_MASM
    if (!m_dualAgentInitialized) {
        std::string resp = buildHttpResponse(200,
            R"({"initialized":false,"architect":{"state":"offline"},"coder":{"state":"offline"}})");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    MasmSwarmState* state = reinterpret_cast<MasmSwarmState*>(Swarm_GetState());
    MasmAgentCtx* archCtx = state ? reinterpret_cast<MasmAgentCtx*>(state->architect_ctx) : nullptr;
    MasmAgentCtx* coderCtx = state ? reinterpret_cast<MasmAgentCtx*>(state->coder_ctx) : nullptr;

    char buf[2048];
    std::snprintf(buf, sizeof(buf),
        R"({"initialized":true,)"
        R"("agent_count":%u,)"
        R"("ring_size_mb":%llu,)"
        R"("total_handoffs":%llu,)"
        R"("total_tasks":%llu,)"
        R"("architect":{"agent_id":0,"profile":%u,"state":"%s",)"
        R"("tasks_processed":%llu,"errors":%llu,)"
        R"("ring_head":%llu,"ring_tail":%llu,)"
        R"("queue_depth":%u},)"
        R"("coder":{"agent_id":1,"profile":%u,"state":"%s",)"
        R"("tasks_processed":%llu,"errors":%llu,)"
        R"("ring_head":%llu,"ring_tail":%llu,)"
        R"("queue_depth":%u},)"
        R"("bridge":"masm-x64-swarm"})",
        state ? state->agent_count : 0u,
        state ? state->ring_size / (1024ULL * 1024ULL) : 0ULL,
        state ? state->total_handoffs : 0ULL,
        state ? state->total_tasks : 0ULL,
        archCtx ? archCtx->model_profile : 0u,
        archCtx ? GetAgentStateName(archCtx->state_flags) : "unknown",
        archCtx ? archCtx->task_count : 0ULL,
        archCtx ? archCtx->error_count : 0ULL,
        archCtx ? archCtx->ring_head : 0ULL,
        archCtx ? archCtx->ring_tail : 0ULL,
        archCtx ? ((archCtx->queue_head - archCtx->queue_tail) & 0xFF) : 0u,
        coderCtx ? coderCtx->model_profile : 0u,
        coderCtx ? GetAgentStateName(coderCtx->state_flags) : "unknown",
        coderCtx ? coderCtx->task_count : 0ULL,
        coderCtx ? coderCtx->error_count : 0ULL,
        coderCtx ? coderCtx->ring_head : 0ULL,
        coderCtx ? coderCtx->ring_tail : 0ULL,
        coderCtx ? ((coderCtx->queue_head - coderCtx->queue_tail) & 0xFF) : 0u
    );

    std::string resp = buildHttpResponse(200, buf);
    send(client, resp.c_str(), (int)resp.size(), 0);
#else
    std::string resp = buildHttpResponse(200,
        R"({"initialized":false,"architect":{"state":"offline"},"coder":{"state":"offline"},"bridge":"cpp-fallback"})");
    send(client, resp.c_str(), (int)resp.size(), 0);
#endif
}

// ============================================================================
// POST /api/agent/dual/handoff — Context handoff Architect → Coder
// Body: { "context": "...architect output text..." }
// ============================================================================
void Win32IDE::handleDualAgentHandoffEndpoint(SOCKET client, const std::string& body) {
    using namespace DualAgentUtil;

    if (!m_dualAgentInitialized) {
        std::string resp = buildHttpResponse(409,
            R"({"error":"Swarm not initialized","hint":"POST /api/agent/dual/init first"})");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    // Extract context from body
    std::string context;
    if (!extractString(body, "context", context) || context.empty()) {
        std::string resp = buildHttpResponse(400,
            R"({"error":"Missing or empty 'context' field in request body"})");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    IDELogger::info("[Phase41] Handoff: %zu bytes context", context.size());

#ifdef RAWR_HAS_MASM
    // Pad to 64-byte alignment for ring buffer (required by MASM fencing)
    size_t dataLen = context.size();
    size_t alignedLen = (dataLen + 63) & ~63ULL;
    std::vector<char> aligned(alignedLen, 0);
    std::memcpy(aligned.data(), context.data(), dataLen);

    uint64_t result = Swarm_Handoff(aligned.data(), static_cast<uint32_t>(alignedLen));
    if (result != SWARM_OK) {
        IDELogger::warn("[Phase41] Handoff failed: ring_full (code=%llu)", result);
        std::string resp = buildHttpResponse(507,
            R"({"error":"Ring buffer full","detail":"Backpressure active, retry after coder drains"})");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    char buf[256];
    std::snprintf(buf, sizeof(buf),
        R"({"success":true,"message":"Context handed off to Coder",)"
        R"("bytes_written":%zu,"aligned_bytes":%zu})",
        dataLen, alignedLen);

    IDELogger::info("[Phase41] Handoff OK: %zu → %zu bytes (aligned)", dataLen, alignedLen);
    std::string resp = buildHttpResponse(200, buf);
    send(client, resp.c_str(), (int)resp.size(), 0);
#else
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        R"({"success":true,"message":"Handoff acknowledged (cpp fallback)","bytes":%zu})",
        context.size());
    std::string resp = buildHttpResponse(200, buf);
    send(client, resp.c_str(), (int)resp.size(), 0);
#endif
}

// ============================================================================
// POST /api/agent/dual/submit — Submit a task to a specific agent
// Body: { "agent": "architect"|"coder", "task_type": 0-3, "data": "..." }
// ============================================================================
void Win32IDE::handleDualAgentSubmitEndpoint(SOCKET client, const std::string& body) {
    using namespace DualAgentUtil;

    if (!m_dualAgentInitialized) {
        std::string resp = buildHttpResponse(409,
            R"({"error":"Swarm not initialized","hint":"POST /api/agent/dual/init first"})");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    // Parse agent target
    std::string agentName;
    if (!extractString(body, "agent", agentName)) {
        std::string resp = buildHttpResponse(400,
            R"({"error":"Missing 'agent' field (expected 'architect' or 'coder')"})");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    int taskTypeInt = 0;
    extractInt(body, "task_type", taskTypeInt);

    std::string data;
    extractString(body, "data", data);

    IDELogger::info("[Phase41] Submit: agent=%s, task_type=%d, data=%zu bytes",
                   agentName.c_str(), taskTypeInt, data.size());

#ifdef RAWR_HAS_MASM
    // Determine target agent
    uint32_t agentId = (agentName == "coder") ? AGENT_ID_CODER : AGENT_ID_ARCHITECT;

    MasmSwarmState* state = reinterpret_cast<MasmSwarmState*>(Swarm_GetState());
    if (!state) {
        std::string resp = buildHttpResponse(500, R"({"error":"Swarm state unavailable"})");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    MasmAgentCtx* targetCtx = (agentId == AGENT_ID_ARCHITECT)
        ? reinterpret_cast<MasmAgentCtx*>(state->architect_ctx)
        : reinterpret_cast<MasmAgentCtx*>(state->coder_ctx);

    if (!targetCtx) {
        std::string resp = buildHttpResponse(500, R"({"error":"Agent context not available"})");
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    // Build a task packet
    MasmTaskPkt task = {};
    task.task_id = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    task.task_type = static_cast<uint32_t>(taskTypeInt);
    task.priority = 128;  // Normal priority
    task.source_agent = AGENT_ID_ARCHITECT;  // IDE acts as architect by default
    task.target_agent = agentId;

    // If data provided, write to ring and set offset
    if (!data.empty()) {
        size_t dataLen = data.size();
        size_t alignedLen = (dataLen + 63) & ~63ULL;
        std::vector<char> aligned(alignedLen, 0);
        std::memcpy(aligned.data(), data.data(), dataLen);

        uint64_t handoffResult = Swarm_Handoff(aligned.data(), static_cast<uint32_t>(alignedLen));
        if (handoffResult != SWARM_OK) {
            std::string resp = buildHttpResponse(507,
                R"({"error":"Ring buffer full while submitting task data"})");
            send(client, resp.c_str(), (int)resp.size(), 0);
            return;
        }
        task.data_length = static_cast<uint32_t>(alignedLen);
    }

    uint64_t result = Swarm_SubmitTask(targetCtx, &task);
    if (result != SWARM_OK) {
        IDELogger::error("[Phase41] SubmitTask failed: code=%llu", result);
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            R"({"error":"Task submission failed","code":%llu})", result);
        std::string resp = buildHttpResponse(500, buf);
        send(client, resp.c_str(), (int)resp.size(), 0);
        return;
    }

    char buf[512];
    std::snprintf(buf, sizeof(buf),
        R"({"success":true,"message":"Task submitted to %s",)"
        R"("task_id":%llu,"task_type":"%s","agent":"%s"})",
        agentName.c_str(),
        task.task_id,
        GetTaskTypeName(static_cast<uint32_t>(taskTypeInt)),
        agentName.c_str());

    IDELogger::info("[Phase41] Task submitted: id=%llu → %s", task.task_id, agentName.c_str());
    std::string resp = buildHttpResponse(200, buf);
    send(client, resp.c_str(), (int)resp.size(), 0);
#else
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        R"({"success":true,"message":"Task submitted (cpp fallback)","agent":"%s"})",
        agentName.c_str());
    std::string resp = buildHttpResponse(200, buf);
    send(client, resp.c_str(), (int)resp.size(), 0);
#endif
}

// ============================================================================
// GET /api/phase41/status — Phase 41 integration status
// ============================================================================
void Win32IDE::handlePhase41StatusEndpoint(SOCKET client) {
    using namespace DualAgentUtil;

    char buf[512];
    std::snprintf(buf, sizeof(buf),
        R"({"phase":41,"name":"Dual-Agent Orchestrator",)"
        R"("status":"%s",)"
        R"("components":{)"
        R"("asm_orchestrator":true,)"
        R"("model_bridge":true,)"
        R"("agent_loops":true,)"
        R"("ide_routes":true,)"
        R"("frontend_ui":true},)"
        R"("endpoints":[)"
        R"("POST /api/agent/dual/init",)"
        R"("POST /api/agent/dual/shutdown",)"
        R"("GET /api/agent/dual/status",)"
        R"("POST /api/agent/dual/handoff",)"
        R"("POST /api/agent/dual/submit"]})",
        m_dualAgentInitialized ? "active" : "ready");

    std::string resp = buildHttpResponse(200, buf);
    send(client, resp.c_str(), (int)resp.size(), 0);
}
