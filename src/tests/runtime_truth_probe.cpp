#include "runtime/SovereignMCPBridge.h"
#include "runtime/RawrXDNLShell.h"
#include "runtime/RawrXDVectorIndex.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <io.h>
#include <windows.h>
// Note: keep MCP checks bounded so probe cannot hang indefinitely.

using RawrXD::Runtime::RawrXDNLShell;
using RawrXD::Runtime::RawrXDVectorIndex;
using RawrXD::Runtime::SovereignMCPBridge;

extern "C" float Vector_DotProduct(const float* a, const float* b, uint32_t dims);
extern "C" float Vector_MagnitudeSq(const float* v, uint32_t dims);
extern "C" float Vector_CosineSimilarity(const float* a, const float* b, uint32_t dims);

namespace {

bool print_result(const std::string& label, bool pass, const std::string& detail) {
    std::cout << "[" << (pass ? "PASS" : "FAIL") << "] " << label << " - " << detail << "\n";
    return pass;
}

bool read_mcp_message(nlohmann::json& out) {
    std::string line;
    size_t contentLength = 0;

    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            break;
        }

        constexpr const char* kHeader = "Content-Length:";
        if (line.rfind(kHeader, 0) == 0) {
            std::string value = line.substr(std::char_traits<char>::length(kHeader));
            std::stringstream ss(value);
            ss >> contentLength;
        }
    }

    if (contentLength == 0 || contentLength > (8u * 1024u * 1024u)) {
        return false;
    }

    std::string payload(contentLength, '\0');
    std::cin.read(payload.data(), static_cast<std::streamsize>(contentLength));
    if (std::cin.gcount() != static_cast<std::streamsize>(contentLength)) {
        return false;
    }

    try {
        out = nlohmann::json::parse(payload);
        return true;
    } catch (...) {
        return false;
    }
}

void write_mcp_json(const nlohmann::json& obj) {
    const std::string payload = obj.dump();
    std::cout << "Content-Length: " << payload.size() << "\r\n\r\n";
    std::cout << payload;
    std::cout.flush();
}

int run_embedded_mcp_server() {
    while (true) {
        nlohmann::json msg;
        if (!read_mcp_message(msg)) {
            return 0;
        }

        const std::string method = msg.value("method", "");
        const bool hasId = msg.contains("id");

        if (method == "initialize" && hasId) {
            nlohmann::json resp;
            resp["jsonrpc"] = "2.0";
            resp["id"] = msg["id"];
            resp["result"]["protocolVersion"] = "2024-11-05";
            resp["result"]["capabilities"]["tools"] = nlohmann::json::object();
            resp["result"]["serverInfo"]["name"] = "rawrxd-embedded-mcp";
            resp["result"]["serverInfo"]["version"] = "1.0";
            write_mcp_json(resp);
            continue;
        }

        if (method == "notifications/initialized") {
            continue;
        }

        if (method == "tools/list" && hasId) {
            nlohmann::json resp;
            resp["jsonrpc"] = "2.0";
            resp["id"] = msg["id"];
            resp["result"]["tools"] = nlohmann::json::array({
                {
                    {"name", "echo"},
                    {"description", "Echoes input text"},
                    {"inputSchema", {
                        {"type", "object"},
                        {"properties", {
                            {"text", {{"type", "string"}}}
                        }}
                    }}
                }
            });
            write_mcp_json(resp);
            continue;
        }

        if (method == "tools/call" && hasId) {
            const auto params = msg.value("params", nlohmann::json::object());
            const std::string name = params.value("name", "");
            const auto args = params.value("arguments", nlohmann::json::object());
            if (name == "echo") {
                nlohmann::json resp;
                resp["jsonrpc"] = "2.0";
                resp["id"] = msg["id"];
                resp["result"]["content"] = nlohmann::json::array({
                    {
                        {"type", "text"},
                        {"text", args.value("text", "")}
                    }
                });
                write_mcp_json(resp);
            } else {
                nlohmann::json err;
                err["jsonrpc"] = "2.0";
                err["id"] = msg["id"];
                err["error"]["code"] = -32601;
                err["error"]["message"] = "unknown tool";
                write_mcp_json(err);
            }
            continue;
        }

        if (hasId) {
            nlohmann::json err;
            err["jsonrpc"] = "2.0";
            err["id"] = msg["id"];
            err["error"]["code"] = -32601;
            err["error"]["message"] = "unknown method";
            write_mcp_json(err);
        }
    }
}

bool run_vector_truth_test() {
    std::vector<float> a(768, 1.0f);
    std::vector<float> b(768, 1.0f);
    std::vector<float> c(768, -1.0f);

    auto& idx = RawrXDVectorIndex::instance();
    const float sim_aa = idx.computeSimilarity(a, b);
    const float sim_ac = idx.computeSimilarity(a, c);
    const float dot_aa = Vector_DotProduct(a.data(), b.data(), 768);
    const float dot_ac = Vector_DotProduct(a.data(), c.data(), 768);
    const float mag_a = Vector_MagnitudeSq(a.data(), 768);
    const float mag_b = Vector_MagnitudeSq(b.data(), 768);
    const float cos_aa_direct = Vector_CosineSimilarity(a.data(), b.data(), 768);
    const float cos_ac_direct = Vector_CosineSimilarity(a.data(), c.data(), 768);

    const bool bounded = (sim_aa <= 1.001f && sim_aa >= -1.001f && sim_ac <= 1.001f && sim_ac >= -1.001f);
    const bool ordered = sim_aa > sim_ac;
    const bool strong = sim_aa > 0.95f;

    bool ok = true;
    ok &= print_result("vector.similarity_bounds", bounded,
                       "sim(a,a)=" + std::to_string(sim_aa) + ", sim(a,c)=" + std::to_string(sim_ac));
    ok &= print_result("vector.similarity_order", ordered,
                       "sim(a,a) should be greater than sim(a,c)");
    ok &= print_result("vector.self_similarity", strong,
                       "sim(a,a) should be near 1");
    std::cout << "[INFO] vector.raw dot(a,a)=" << dot_aa
              << " dot(a,c)=" << dot_ac
              << " mag2(a)=" << mag_a
              << " mag2(b)=" << mag_b
              << " cos(a,a)=" << cos_aa_direct
              << " cos(a,c)=" << cos_ac_direct << "\n";
    return ok;
}

bool run_nlshell_truth_test() {
    auto& shell = RawrXDNLShell::instance();

    const uint32_t risk_ls = shell.validateCommand("ls");
    const uint32_t risk_mkdir = shell.validateCommand("mkdir test");
    const uint32_t risk_rm = shell.validateCommand("rm -rf /");

    bool ok = true;
    ok &= print_result("nlshell.readonly", risk_ls == 0,
                       "ls risk=" + std::to_string(risk_ls));
    ok &= print_result("nlshell.write", risk_mkdir == 1,
                       "mkdir test risk=" + std::to_string(risk_mkdir));
    ok &= print_result("nlshell.block", risk_rm >= 100,
                       "rm -rf / risk=" + std::to_string(risk_rm));
    return ok;
}

bool run_mcp_truth_test() {
    struct McpResult {
        bool spawned = false;
        bool toolsArray = false;
        bool hasEcho = false;
        bool echoCallOk = false;
        std::string detail;
    };

    std::atomic<int> stage{0};
    const bool probeDebug = (std::getenv("RAWR_PROBE_DEBUG") != nullptr);

    auto setStage = [&](int value, const char* label) {
        stage.store(value);
        if (probeDebug) {
            std::cerr << "[PROBE_STAGE] " << value << " " << label << std::endl;
        }
    };

    auto worker = [&]() -> McpResult {
        McpResult result{};
        auto& bridge = SovereignMCPBridge::instance();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(14);

        auto remainingMs = [&]() -> DWORD {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                return 0;
            }
            return static_cast<DWORD>(
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
        };

        auto findOnPath = [](const char* exeName) -> std::string {
            char resolved[MAX_PATH] = {};
            DWORD len = SearchPathA(nullptr, exeName, nullptr, MAX_PATH, resolved, nullptr);
            if (len == 0 || len >= MAX_PATH) {
                return {};
            }
            return std::string(resolved, resolved + len);
        };

        auto quote = [](const std::string& s) -> std::string {
            return std::string("\"") + s + "\"";
        };

        char exeBuf[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
        const std::filesystem::path exePath(exeBuf);
        const std::filesystem::path nativeMcpExe = exePath.parent_path() / "RawrXD-MCPServer.exe";

        std::vector<std::string> commands;
        if (std::filesystem::exists(nativeMcpExe)) {
            commands.push_back(quote(nativeMcpExe.string()));
        } else {
            // Fall back to embedded server only when native MASM server is unavailable.
            commands.push_back(quote(exePath.string()) + " --mcp-min-server");
        }

        for (const auto& command : commands) {
            setStage(10, "spawn_attempt");
            DWORD spawnBudget = remainingMs();
            if (spawnBudget == 0) {
                result.detail = "deadline_exceeded before spawn";
                break;
            }
            if (spawnBudget > 6000) {
                spawnBudget = 6000;
            }

            if (!bridge.spawnServer(command, spawnBudget)) {
                result.detail = "spawn failed for: " + command + " | " + bridge.lastError();
                continue;
            }
            result.spawned = true;
            result.detail = "spawned via: " + command;
            break;
        }

        if (!result.spawned) {
            if (result.detail.empty()) {
                result.detail = "failed to spawn MCP test server: " + bridge.lastError();
            }
            setStage(11, "spawn_failed");
            bridge.shutdown();
            return result;
        }

        setStage(20, "tools_list");
        DWORD toolsBudget = remainingMs();
        if (toolsBudget == 0) {
            result.detail = "deadline_exceeded before tools/list";
            bridge.shutdown();
            return result;
        }
        toolsBudget = (toolsBudget > 3000) ? 3000 : toolsBudget;
        const auto tools = bridge.listTools(toolsBudget);
        result.toolsArray = tools.is_object() && tools.contains("tools") && tools["tools"].is_array();
        result.hasEcho = result.toolsArray && std::any_of(tools["tools"].begin(), tools["tools"].end(), [](const auto& t) {
            return t.is_object() && t.value("name", "") == "echo";
        });

        setStage(30, "tool_call_echo");
        DWORD callBudget = remainingMs();
        if (callBudget == 0) {
            result.detail = "deadline_exceeded before tools/call";
            bridge.shutdown();
            return result;
        }
        callBudget = (callBudget > 3000) ? 3000 : callBudget;
        const auto callResp = bridge.callTool("echo", nlohmann::json{{"text", "probe-ok"}}, callBudget);
        if (callResp.is_object() && callResp.contains("content") && callResp["content"].is_array() && !callResp["content"].empty()) {
            const auto& item = callResp["content"][static_cast<size_t>(0)];
            result.echoCallOk = item.is_object() && item.value("text", "") == "probe-ok";
        }

        if (!result.echoCallOk) {
            result.detail = "tools=" + tools.dump() + " call=" + callResp.dump();
        }

        setStage(40, "bridge_shutdown");
        bridge.shutdown();
        setStage(41, "bridge_shutdown_done");
        return result;
    };

    bool ok = true;
    std::mutex m;
    std::condition_variable cv;
    bool done = false;
    McpResult result;
    std::thread th([&]() {
        setStage(1, "worker_started");
        McpResult local = worker();
        setStage(2, "worker_finished");
        {
            std::lock_guard<std::mutex> lock(m);
            result = std::move(local);
            done = true;
        }
        setStage(3, "worker_notified");
        cv.notify_one();
    });
    th.detach();

    {
        std::unique_lock<std::mutex> lock(m);
        if (!cv.wait_for(lock, std::chrono::seconds(15), [&]() { return done; })) {
            ok &= print_result("mcp.timeout", false,
                               "MCP truth test exceeded 15s timeout (stage=" + std::to_string(stage.load()) + ")");
            return false;
        }
    }

    if (!done) {
        ok &= print_result("mcp.timeout", false, "MCP truth test exceeded 15s timeout");
        return false;
    }

    ok &= print_result("mcp.spawn", result.spawned, result.spawned ? result.detail : result.detail);
    if (!result.spawned) {
        return false;
    }

    ok &= print_result("mcp.tools_list", result.toolsArray,
                       result.toolsArray ? "tools array present" : result.detail);
    ok &= print_result("mcp.tools_list_echo", result.hasEcho,
                       result.hasEcho ? "echo tool discovered" : result.detail);
    ok &= print_result("mcp.call_tool_echo", result.echoCallOk,
                       result.echoCallOk ? "echo response validated" : result.detail);

    return ok;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && argv != nullptr && std::string(argv[1]) == "--mcp-min-server") {
#ifdef _WIN32
        // Ensure immediate pipe-visible output with no text translation artifacts.
        setvbuf(stdout, nullptr, _IONBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);
        _setmode(_fileno(stdout), _O_BINARY);
        _setmode(_fileno(stdin), _O_BINARY);
#endif
        return run_embedded_mcp_server();
    }

    bool ok = true;

    std::cout << "=== RawrXD Runtime Truth Probe ===\n";
    ok &= run_vector_truth_test();
    ok &= run_nlshell_truth_test();
    ok &= run_mcp_truth_test();

    std::cout << (ok ? "=== PROBE RESULT: PASS ===\n" : "=== PROBE RESULT: FAIL ===\n");
    return ok ? 0 : 1;
}
