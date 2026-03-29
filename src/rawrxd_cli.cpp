#include "advanced_features.h"
#include "agentic/AgentToolHandlers.h"
#include "agentic_engine.h"
#include "api_server.h"
#include "core/agent_capability_audit.hpp"
#include "cpu_inference_engine.h"
#include "crt_free_memory.h"
#include "diagnostics/self_diagnose.hpp"
#include "interactive_shell.h"
#include "modules/codex_ultimate.h"
#include "modules/engine_manager.h"
#include "modules/memory_manager.h"
#include "modules/react_generator.h"
#include "modules/vsix_loader.h"
#include "overclock_governor.h"
#include "overclock_vendor.h"
#include "settings.h"
#include "telemetry.h"
#include "win32app/OllamaServiceManager.h"  // Embedded Ollama service
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <conio.h>
#include <deque>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <winhttp.h>
#include <winsock2.h>


#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")

using json = nlohmann::json;
using namespace std::chrono_literals;


using GenerationConfig = AgenticEngine::GenerationConfig;
struct CLIState : public AppState
{
    std::string model_path;
    bool loaded_model = false;
    std::shared_ptr<RawrXD::CPUInferenceEngine> inference_engine;
    std::shared_ptr<AgenticEngine> agent_engine;
    std::unique_ptr<APIServer> api_server;
    std::unique_ptr<OverclockGovernor> governor;
    std::unique_ptr<VSIXLoader> vsix_loader;
    std::unique_ptr<MemoryManager> memory_manager;
    std::unique_ptr<InteractiveShell> shell;
    std::unique_ptr<EngineManager> engine_manager;
    std::unique_ptr<CodexUltimate> codex;
    std::unique_ptr<OllamaServiceManager> embedded_ollama;  // Embedded Ollama service

    // Configuration
    float temperature = 0.8f;
    float top_p = 0.9f;
    int max_tokens = 2048;
    bool enable_max_mode = false;
    bool enable_deep_thinking = false;
    bool enable_deep_research = false;
    bool enable_no_refusal = false;
    bool enable_autocorrect = false;
    bool enable_overclock_governor = false;
    bool use_embedded_ollama = true;  // Prefer embedded service by default
    size_t context_size = 4096;
    uint32_t target_cpu_temp_c = 85;
    uint32_t target_gpu_temp_c = 85;

    // missing symbols for Settings::SaveCompute
    bool is_gpu_enabled = false;
    int thread_count = 4;
    int vram_limit_mb = 2048;
};

static bool IsLikelyLocalFileModelPath(const std::string& modelRef)
{
    if (modelRef.empty())
        return false;
    if (modelRef.find('\\') != std::string::npos || modelRef.find('/') != std::string::npos)
        return true;
    const std::string lower = modelRef;
    return lower.size() > 5 &&
           (lower.rfind(".gguf") == lower.size() - 5 || lower.rfind(".bin") == lower.size() - 4);
}

static std::string NormalizeOllamaModelRef(std::string modelRef)
{
    while (!modelRef.empty() && (modelRef.front() == '"' || modelRef.front() == ' '))
        modelRef.erase(modelRef.begin());
    while (!modelRef.empty() && (modelRef.back() == '"' || modelRef.back() == ' '))
        modelRef.pop_back();
    if (modelRef.empty())
        return "llama3.1";
    return modelRef;
}

static std::string QueryOllamaAPI(const std::string& modelName, const std::string& prompt, double temperature,
                                  double topP, int maxTokens)
{
    HINTERNET hSession = WinHttpOpen(L"RawrXD-CLI/6.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        return {};
    HINTERNET hConnect = WinHttpConnect(hSession, L"localhost", 11434, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return {};
    }
    HINTERNET hRequest =
        WinHttpOpenRequest(hConnect, L"POST", L"/api/generate", NULL, WINHTTP_NO_REFERER, NULL, WINHTTP_FLAG_REFRESH);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return {};
    }
    WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
    nlohmann::json requestBody = {{"model", NormalizeOllamaModelRef(modelName)},
                                  {"prompt", prompt},
                                  {"temperature", temperature},
                                  {"top_p", topP},
                                  {"num_predict", maxTokens},
                                  {"stream", false}};
    const std::string body = requestBody.dump();
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body.c_str(), (DWORD)body.size(),
                            (DWORD)body.size(), 0) ||
        !WinHttpReceiveResponse(hRequest, NULL))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return {};
    }
    std::string responseBody;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0)
    {
        std::vector<char> buf(bytesAvailable + 1, 0);
        DWORD bytesRead = 0;
        if (!WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead))
            break;
        responseBody.append(buf.data(), bytesRead);
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    if (responseBody.empty())
        return {};
    try
    {
        auto doc = nlohmann::json::parse(responseBody);
        if (doc.contains("response") && doc["response"].is_string())
            return doc["response"].get<std::string>();
    }
    catch (...)
    {
    }
    return {};
}

void EnsureDirectories()
{
    std::filesystem::create_directories("plugins");
    std::filesystem::create_directories("memory_modules");
    std::filesystem::create_directories("models");
    std::filesystem::create_directories("sessions");
    std::filesystem::create_directories("projects");
    std::filesystem::create_directories("models/800b");
}

#if 0
    // New agent tool commands (production, non-placeholder)
    state.shell->registerCommand(
        "compact",
        [&state](const auto& args)
        {
            std::ifstream in("shell_history.txt", std::ios::binary);
            if (!in)
            {
                RawrXD::CRTFreeConsole::WriteLine("No shell history found (shell_history.txt).");
                return;
            }

            std::deque<std::string> tail;
            std::string line;
            while (std::getline(in, line))
            {
                if (!line.empty())
                {
                    tail.push_back(line);
                    if (tail.size() > 160)
                        tail.pop_front();
                }
            }

            std::filesystem::create_directories("sessions");
            json compacted;
            compacted["model"] = state.model_path;
            compacted["context_size"] = state.context_size;
            compacted["temperature"] = state.temperature;
            compacted["top_p"] = state.top_p;
            compacted["max_tokens"] = state.max_tokens;
            compacted["line_count"] = tail.size();
            compacted["conversation_tail"] = tail;

            const std::string outPath = "sessions/compacted_conversation.json";
            std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                RawrXD::CRTFreeConsole::WriteLine("Failed to write compacted conversation snapshot.");
                return;
            }
            out << compacted.dump(2);
            RawrXD::CRTFreeConsole::WriteFormat("Compacted conversation saved: %s (%zu lines)\n", outPath.c_str(),
                                                tail.size());
        });

    state.shell->registerCommand(
        "optimize",
        [&state](const auto& args)
        {
            if (args.size() < 2)
            {
                RawrXD::CRTFreeConsole::WriteLine("Usage: optimize <intent text>");
                return;
            }
            std::string intent;
            for (size_t i = 1; i < args.size(); ++i)
            {
                if (i > 1)
                    intent += " ";
                intent += args[i];
            }

            std::string low = intent;
            std::transform(low.begin(), low.end(), low.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            struct ToolScore
            {
                std::string name;
                int score;
                std::string reason;
            };
            std::vector<ToolScore> ranked = {{"read", 10, "baseline"},      {"search", 10, "baseline"},
                                             {"plan", 10, "baseline"},      {"resolve", 10, "baseline"},
                                             {"evaluate", 10, "baseline"},  {"compact", 10, "baseline"},
                                             {"checkpoint", 10, "baseline"}};
            auto contains = [&](const char* key) { return low.find(key) != std::string::npos; };
            for (auto& t : ranked)
            {
                if ((contains("file") || contains("path")) && t.name == "search")
                {
                    t.score += 35;
                    t.reason = "file discovery intent";
                }
                if ((contains("line") || contains("snippet")) && t.name == "read")
                {
                    t.score += 35;
                    t.reason = "line range intent";
                }
                if ((contains("plan") || contains("explore") || contains("audit")) && t.name == "plan")
                {
                    t.score += 35;
                    t.reason = "planning intent";
                }
                if ((contains("resolve") || contains("fix") || contains("ambigu")) && t.name == "resolve")
                {
                    t.score += 35;
                    t.reason = "resolution intent";
                }
                if (contains("checkpoint") && t.name == "checkpoint")
                {
                    t.score += 35;
                    t.reason = "checkpoint intent";
                }
                if ((contains("compact") || contains("summary")) && t.name == "compact")
                {
                    t.score += 35;
                    t.reason = "conversation compaction intent";
                }
                if ((contains("feasible") || contains("risk") || contains("integration")) && t.name == "evaluate")
                {
                    t.score += 35;
                    t.reason = "feasibility intent";
                }
            }
            std::sort(ranked.begin(), ranked.end(),
                      [](const ToolScore& a, const ToolScore& b) { return a.score > b.score; });

            RawrXD::CRTFreeConsole::WriteFormat("Top tool candidates for: %s\n", intent.c_str());
            for (size_t i = 0; i < ranked.size() && i < 5; ++i)
            {
                RawrXD::CRTFreeConsole::WriteFormat("  %zu) %s (score=%d, reason=%s)\n", i + 1, ranked[i].name.c_str(),
                                                    ranked[i].score, ranked[i].reason.c_str());
            }
        });

    state.shell->registerCommand("resolve",
                                 [&state](const auto& args)
                                 {
                                     if (args.size() < 2)
                                     {
                                         RawrXD::CRTFreeConsole::WriteLine("Usage: resolve <issue or symbol>");
                                         return;
                                     }
                                     std::string issue;
                                     for (size_t i = 1; i < args.size(); ++i)
                                     {
                                         if (i > 1)
                                             issue += " ";
                                         issue += args[i];
                                     }
                                     RawrXD::CRTFreeConsole::WriteFormat("Resolving target: %s\n", issue.c_str());
                                     RawrXD::CRTFreeConsole::WriteLine("Resolution plan:");
                                     RawrXD::CRTFreeConsole::WriteLine("  1) Confirm scope and impacted files");
                                     RawrXD::CRTFreeConsole::WriteLine("  2) Gather precise failure evidence");
                                     RawrXD::CRTFreeConsole::WriteLine("  3) Apply minimal targeted fix");
                                     RawrXD::CRTFreeConsole::WriteLine("  4) Re-run smoke validation");
                                 });

    state.shell->registerCommand(
        "read",
        [&state](const auto& args)
        {
            if (args.size() < 3)
            {
                RawrXD::CRTFreeConsole::WriteLine("Usage: read <start>-<end> <file>");
                return;
            }
            std::string range = args[1];
            std::string file = args[2];
            size_t dash = range.find('-');
            if (dash == std::string::npos)
            {
                RawrXD::CRTFreeConsole::WriteLine("Invalid range format. Use start-end");
                return;
            }
            try
            {
                int start = std::stoi(range.substr(0, dash));
                int end = std::stoi(range.substr(dash + 1));
                if (start <= 0 || end < start)
                {
                    RawrXD::CRTFreeConsole::WriteLine("Range must satisfy: start > 0 and end >= start.");
                    return;
                }
                std::ifstream in(file, std::ios::binary);
                if (!in)
                {
                    RawrXD::CRTFreeConsole::WriteFormat("Failed to open file: %s\n", file.c_str());
                    return;
                }
                int current = 0;
                int emitted = 0;
                const int maxEmit = 500;
                std::string currentLine;
                while (std::getline(in, currentLine))
                {
                    ++current;
                    if (current < start)
                        continue;
                    if (current > end)
                        break;
                    RawrXD::CRTFreeConsole::WriteFormat("%d: %s\n", current, currentLine.c_str());
                    if (++emitted >= maxEmit)
                    {
                        RawrXD::CRTFreeConsole::WriteLine("Output truncated at 500 lines.");
                        break;
                    }
                }
            }
            catch (const std::exception&)
            {
                RawrXD::CRTFreeConsole::WriteLine("Invalid line numbers.");
            }
        });

    state.shell->registerCommand(
        "plan",
        [&state](const auto& args)
        {
            std::string target = args.size() > 1 ? args[1] : "src";
            std::string focus = args.size() > 2 ? args[2] : "latest integration path";
            RawrXD::CRTFreeConsole::WriteFormat("Targeted code exploration plan for %s (focus: %s)\n", target.c_str(),
                                                focus.c_str());
            RawrXD::CRTFreeConsole::WriteLine("  1) Enumerate candidate files by pattern and recency");
            RawrXD::CRTFreeConsole::WriteLine("  2) Locate runtime entry points and API routes");
            RawrXD::CRTFreeConsole::WriteLine("  3) Identify stubs/placeholders and missing fallback logic");
            RawrXD::CRTFreeConsole::WriteLine("  4) Patch in small validated batches");
            RawrXD::CRTFreeConsole::WriteLine("  5) Re-run smoke checks and checkpoint state");
        });

    state.shell->registerCommand(
        "search",
        [&state](const auto& args)
        {
            if (args.size() < 2)
            {
                RawrXD::CRTFreeConsole::WriteLine("Usage: search <pattern> [root]");
                return;
            }
            std::string pattern = args[1];
            std::string root = args.size() > 2 ? args[2] : ".";
            RawrXD::CRTFreeConsole::WriteFormat("Searching for files matching: %s (root=%s)\n", pattern.c_str(),
                                                root.c_str());
            try
            {
                namespace fs = std::filesystem;
                size_t hits = 0;
                const size_t maxHits = 200;
                for (auto& p : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied))
                {
                    if (p.is_regular_file())
                    {
                        std::string filename = p.path().filename().string();
                        if (filename.find(pattern) != std::string::npos)
                        {
                            RawrXD::CRTFreeConsole::WriteLine(p.path().string().c_str());
                            if (++hits >= maxHits)
                            {
                                RawrXD::CRTFreeConsole::WriteLine("Search truncated at 200 matches.");
                                break;
                            }
                        }
                    }
                }
            }
            catch (const std::exception& e)
            {
                RawrXD::CRTFreeConsole::WriteFormat("Search error: %s\n", e.what());
            }
        });

    state.shell->registerCommand(
        "evaluate",
        [&state](const auto& args)
        {
            const std::string root = args.size() > 1 ? args[1] : "src";
            std::error_code ec;
            size_t fileCount = 0;
            size_t sourceCount = 0;
            uintmax_t totalBytes = 0;

            for (const auto& e : std::filesystem::recursive_directory_iterator(
                     root, std::filesystem::directory_options::skip_permission_denied, ec))
            {
                if (ec)
                    break;
                if (!e.is_regular_file())
                    continue;
                ++fileCount;
                const auto ext = e.path().extension().string();
                if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c" || ext == ".asm" || ext == ".inc")
                {
                    ++sourceCount;
                }
                totalBytes += e.file_size(ec);
                if (ec)
                    ec.clear();
            }

            const double sizeMb = static_cast<double>(totalBytes) / (1024.0 * 1024.0);
            RawrXD::CRTFreeConsole::WriteFormat("Audit feasibility for %s\n", root.c_str());
            RawrXD::CRTFreeConsole::WriteFormat("  files=%zu source_files=%zu size_mb=%.2f\n", fileCount, sourceCount,
                                                sizeMb);
            if (sourceCount > 3000 || sizeMb > 1500.0)
            {
                RawrXD::CRTFreeConsole::WriteLine("  recommendation=batch audit (7 targets per pass)");
            }
            else
            {
                RawrXD::CRTFreeConsole::WriteLine("  recommendation=single-pass targeted audit feasible");
            }
        });

    auto checkpointHandler = [&state](const auto& args)
    {
        const std::string mode = args.size() > 1 ? args[1] : "restore";
        const std::string cpPath = args.size() > 2 ? args[2] : "sessions/runtime_checkpoint.json";
        std::filesystem::create_directories("sessions");

        if (mode == "save")
        {
            json cp;
            cp["model_path"] = state.model_path;
            cp["context_size"] = state.context_size;
            cp["temperature"] = state.temperature;
            cp["top_p"] = state.top_p;
            cp["max_tokens"] = state.max_tokens;
            cp["enable_max_mode"] = state.enable_max_mode;
            cp["enable_deep_thinking"] = state.enable_deep_thinking;
            cp["enable_deep_research"] = state.enable_deep_research;
            cp["enable_no_refusal"] = state.enable_no_refusal;
            cp["enable_autocorrect"] = state.enable_autocorrect;

            std::ofstream out(cpPath, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                RawrXD::CRTFreeConsole::WriteFormat("Failed to save checkpoint: %s\n", cpPath.c_str());
                return;
            }
            out << cp.dump(2);
            RawrXD::CRTFreeConsole::WriteFormat("Checkpoint saved: %s\n", cpPath.c_str());
            return;
        }

        std::ifstream in(cpPath, std::ios::binary);
        if (!in)
        {
            RawrXD::CRTFreeConsole::WriteFormat("Checkpoint not found: %s\n", cpPath.c_str());
            return;
        }
        try
        {
            json cp = json::parse(in);
            if (cp.contains("model_path"))
                state.model_path = cp["model_path"].get<std::string>();
            if (cp.contains("context_size"))
                state.context_size = cp["context_size"].get<size_t>();
            if (cp.contains("temperature"))
                state.temperature = cp["temperature"].get<float>();
            if (cp.contains("top_p"))
                state.top_p = cp["top_p"].get<float>();
            if (cp.contains("max_tokens"))
                state.max_tokens = cp["max_tokens"].get<int>();
            if (cp.contains("enable_max_mode"))
                state.enable_max_mode = cp["enable_max_mode"].get<bool>();
            if (cp.contains("enable_deep_thinking"))
                state.enable_deep_thinking = cp["enable_deep_thinking"].get<bool>();
            if (cp.contains("enable_deep_research"))
                state.enable_deep_research = cp["enable_deep_research"].get<bool>();
            if (cp.contains("enable_no_refusal"))
                state.enable_no_refusal = cp["enable_no_refusal"].get<bool>();
            if (cp.contains("enable_autocorrect"))
                state.enable_autocorrect = cp["enable_autocorrect"].get<bool>();
            RawrXD::CRTFreeConsole::WriteFormat("Checkpoint restored: %s\n", cpPath.c_str());
        }
        catch (const std::exception& e)
        {
            RawrXD::CRTFreeConsole::WriteFormat("Checkpoint restore failed: %s\n", e.what());
        }
    };

    state.shell->registerCommand("checkpoint", checkpointHandler);
    state.shell->registerCommand("rstore", checkpointHandler);

    // Long-form aliases requested by integration audit workflow
    state.shell->registerCommand("compacted-conversation",
                                 [&state](const auto& args) { state.shell->executeCommand("compact", args); });
    state.shell->registerCommand("optimizing-tool-selection",
                                 [&state](const auto& args) { state.shell->executeCommand("optimize", args); });
    state.shell->registerCommand("resolving",
                                 [&state](const auto& args) { state.shell->executeCommand("resolve", args); });
    state.shell->registerCommand("read-lines",
                                 [&state](const auto& args) { state.shell->executeCommand("read", args); });
    state.shell->registerCommand("planning-code-exploration",
                                 [&state](const auto& args) { state.shell->executeCommand("plan", args); });
    state.shell->registerCommand("search-files",
                                 [&state](const auto& args) { state.shell->executeCommand("search", args); });
    state.shell->registerCommand("evaluating-audit-feasibility",
                                 [&state](const auto& args) { state.shell->executeCommand("evaluate", args); });
    state.shell->registerCommand("restore-checkpoint",
                                 [&state](const auto& args) { state.shell->executeCommand("checkpoint", args); });
            (DWORD)body_str.length(),
            (DWORD)body_str.length(),
            0))
            {
                std::cerr << "[HTTP] Failed to send request to embedded service" << std::endl;
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                goto fallback;
            }

            // Receive response
            if (!WinHttpReceiveResponse(hRequest, NULL))
            {
                std::cerr << "[HTTP] Failed to receive response from embedded service" << std::endl;
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                goto fallback;
            }

            // Read response body
            std::string response_body;
            DWORD bytes_available = 0;

            while (WinHttpQueryDataAvailable(hRequest, &bytes_available) && bytes_available > 0)
            {
                std::vector<char> buffer(bytes_available + 1, 0);

                DWORD bytes_read = 0;
                if (WinHttpReadData(hRequest, buffer.data(), bytes_available, &bytes_read))
                {
                    response_body.append(buffer.data(), bytes_read);
                }
                else
                {
                    break;
                }
            }

            // Parse JSON response
            if (!response_body.empty())
            {
                try
                {
                    json response = json::parse(response_body);
                    if (response.contains("response"))
                    {
                        std::string result = response["response"];
                        std::cout << "[Embedded Ollama] Query successful via managed service" << std::endl;
                        WinHttpCloseHandle(hRequest);
                        WinHttpCloseHandle(hConnect);
                        WinHttpCloseHandle(hSession);
                        return result;
                    }
                }
                catch (const std::exception& e)
                {
                    std::cerr << "[JSON] Parse error from embedded service: " << e.what() << "\n";
                }
            }

            // Cleanup
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);

            goto fallback;
}

fallback :
    // Fallback to direct external Ollama connection
    std::cout
    << "[HTTP] Using direct connection to external Ollama service"
    << std::endl;

try
{
    // Initialize WinHTTP session
    HINTERNET hSession = WinHttpOpen(L"RawrXD-CLI-Fallback/6.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession)
    {
        std::cerr << "[HTTP] Failed to create WinHTTP session\n";
        return "";
    }

    // Connect to Ollama server
    HINTERNET hConnect = WinHttpConnect(hSession, L"localhost", 11434, 0);

    if (!hConnect)
    {
        std::cerr << "[HTTP] Failed to connect to Ollama at localhost:11434\n";
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Create HTTP request
    HINTERNET hRequest =
        WinHttpOpenRequest(hConnect, L"POST", L"/api/generate", NULL, WINHTTP_NO_REFERER, NULL, WINHTTP_FLAG_REFRESH);

    if (!hRequest)
    {
        std::cerr << "[HTTP] Failed to create HTTP request\n";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Add content-type header
    if (!WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD))
    {
        std::cerr << "[HTTP] Failed to add headers\n";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Build JSON request body
    json request_body;
    request_body["model"] = NormalizeOllamaModelRef(model_name);
    request_body["prompt"] = prompt;
    request_body["temperature"] = temperature;
    request_body["top_p"] = top_p;
    request_body["num_predict"] = max_tokens;
    request_body["stream"] = false;

    std::string body_str = request_body.dump();

    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body_str.c_str(),
                            (DWORD)body_str.length(), (DWORD)body_str.length(), 0))
    {
        std::cerr << "[HTTP] Failed to send request\n";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, NULL))
    {
        std::cerr << "[HTTP] Failed to receive response\n";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Read response body
    std::string response_body;
    DWORD bytes_available = 0;

    while (WinHttpQueryDataAvailable(hRequest, &bytes_available) && bytes_available > 0)
    {
        std::vector<char> buffer(bytes_available + 1, 0);

        DWORD bytes_read = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytes_available, &bytes_read))
        {
            response_body.append(buffer.data(), bytes_read);
        }
        else
        {
            break;
        }
    }

    // Parse JSON response
    if (!response_body.empty())
    {
        try
        {
            json response = json::parse(response_body);
            if (response.contains("response"))
            {
                std::string result = response["response"];
                std::cout << "[Ollama] Query successful via fallback connection\n";
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return result;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "[JSON] Parse error: " << e.what() << "\n";
        }
    }

    // Cleanup
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return "";
}
catch (const std::exception& e)
{
    std::cerr << "[HTTP] Exception: " << e.what() << "\n";
    return "";
}
}
#endif

void PrintBanner()
{
    RawrXD::CRTFreeConsole::WriteLine(R"(
╔══════════════════════════════════════════════════════════════╗
║                    RawrXD AI Shell v6.0                      ║
║                                                              ║
║  Zero Qt • Zero Python • Pure Performance                   ║
║  Multi-Engine • 800B Support • Reverse Engineering          ║
╚══════════════════════════════════════════════════════════════╝
)");
}

int MainNoCRuntime(int argc, char** argv)
{
    RawrXD::Diagnostics::SelfDiagnoser::Install();
    RAWRXD_PHASE_SET(RawrXD::Diagnostics::InitPhase::CoreInit, "CLI Entry");

    CLIState state;
    EnsureDirectories();
    PrintBanner();
    RAWRXD_PHASE_SET(RawrXD::Diagnostics::InitPhase::Registry, "Parsed args begin");

    // Parse command line arguments
    bool chat_mode = false;
    std::string chat_prompt;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc)
        {
            state.model_path = argv[++i];
        }
        else if (arg == "--mode" && i + 1 < argc)
        {
            std::string mode = argv[++i];
            if (mode == "local")
            {
                // Already setting model via --model
            }
            else if (mode == "distributed")
            {
                state.enable_max_mode = true;
                state.enable_overclock_governor = true;
            }
        }
        else if (arg == "--chat" && i + 1 < argc)
        {
            chat_mode = true;
            chat_prompt = argv[++i];
        }
        else if (arg == "--max-mode")
        {
            state.enable_max_mode = true;
        }
        else if (arg == "--deep-thinking")
        {
            state.enable_deep_thinking = true;
        }
        else if (arg == "--deep-research")
        {
            state.enable_deep_research = true;
        }
        else if (arg == "--no-refusal")
        {
            state.enable_no_refusal = true;
        }
        else if (arg == "--autocorrect")
        {
            state.enable_autocorrect = true;
        }
        else if (arg == "--context-size" && i + 1 < argc)
        {
            std::string size = argv[++i];
            if (size == "4k")
                state.context_size = 4096;
            else if (size == "32k")
                state.context_size = 32768;
            else if (size == "64k")
                state.context_size = 65536;
            else if (size == "128k")
                state.context_size = 131072;
            else if (size == "256k")
                state.context_size = 262144;
            else if (size == "512k")
                state.context_size = 524288;
            else if (size == "1m")
                state.context_size = 1048576;
        }
        else if (arg == "--governor")
        {
            state.enable_overclock_governor = true;
        }
        else if (arg == "--temp" && i + 1 < argc)
        {
            std::string tempStr = argv[++i];
            try
            {
                state.temperature = std::stof(tempStr);
            }
            catch (const std::exception&)
            {
                std::cerr << "Warning: invalid value for --temp: '" << tempStr << "'. Using default temperature.\n";
            }
        }
        else if (arg == "--top-p" && i + 1 < argc)
        {
            std::string topPStr = argv[++i];
            try
            {
                state.top_p = std::stof(topPStr);
            }
            catch (const std::exception&)
            {
                std::cerr << "Warning: invalid value for --top-p: '" << topPStr << "'. Using default top_p.\n";
            }
        }
        else if (arg == "--max-tokens" && i + 1 < argc)
        {
            std::string maxTokensStr = argv[++i];
            try
            {
                state.max_tokens = std::stoi(maxTokensStr);
            }
            catch (const std::exception&)
            {
                std::cerr << "Warning: invalid value for --max-tokens: '" << maxTokensStr
                          << "'. Using default max_tokens.\n";
            }
        }
        else if (arg == "--engine" && i + 1 < argc)
            {
                // Pre-select engine
                std::string engine_id = argv[++i];
                state.engine_manager = std::make_unique<EngineManager>();
                state.engine_manager->LoadEngine("engines/" + engine_id, engine_id);
                state.engine_manager->SwitchEngine(engine_id);
            }
            else if (arg == "--embedded-ollama")
            {
                state.use_embedded_ollama = true;
            }
            else if (arg == "--no-embedded-ollama")
            {
                state.use_embedded_ollama = false;
            }
            else if (arg == "--help")
            {
                RawrXD::CRTFreeConsole::Write(R"(
RawrXD CLI v6.0 - Ultimate AI Shell

Usage:
  RawrXD_CLI [options]

Options:
  --model <path>          Path to GGUF model file
  --mode <local|dist>     Operating mode (local or distributed)
  --chat "<prompt>"       Run a single chat prompt and exit
  --max-mode              Enable max mode (32K+ context)
  --deep-thinking         Enable deep thinking mode
  --deep-research         Enable deep research mode
  --no-refusal            Enable no refusal mode
  --autocorrect           Enable auto-correction
  --context-size <size>   Set context size (4k/32k/64k/128k/256k/512k/1m)
  --governor              Enable overclock governor
  --temp <value>          Set temperature (default: 0.8)
  --top-p <value>         Set top-p (default: 0.9)
  --max-tokens <value>    Set max tokens (default: 2048)
  --embedded-ollama       Enable embedded Ollama service (default)
  --no-embedded-ollama    Disable embedded service, use external Ollama
  --engine <id>           Pre-select engine (800b-5drive, codex-ultimate, rawrxd-compiler)
  --help                  Show this help

ENGINE-SPECIFIC:
  800B Model Engine:
    !engine load800b <model>    - Load 800B model
    !engine setup5drive <dir>   - Setup 5-drive layout
    !engine verify              - Verify drive setup

  Codex Ultimate:
    !engine disasm <file>       - Disassemble binary
    !engine dumpbin <file>      - Dump PE/COFF info
    !engine analyze <file>      - Agentic analysis

  RawrXD Compiler:
    !engine compile <file>      - Compile with MASM64
    !engine optimize <file>     - Optimize code

INTERACTIVE COMMANDS:
  /plan <task>            - Create execution plan
  /react-server <name>    - Generate React project
  /bugreport <target>     - Analyze for bugs
  /suggest <code>         - Get code suggestions
  /hotpatch f old new     - Apply code hotpatch
  /analyze <target>       - Deep code analysis
  /optimize <target>      - Performance optimization
  /security <target>      - Security vulnerability scan

MODE TOGGLES:
  /maxmode <on|off>       - Toggle max mode
  /deepthinking <on|off>  - Toggle deep thinking
  /deepresearch <on|off>  - Toggle deep research
  /norefusal <on|off>     - Toggle no refusal
  /autocorrect <on|off>   - Toggle auto-correction

SHELL COMMANDS:
  /clear                  - Clear history
  /save <filename>        - Save conversation
  /load <filename>        - Load conversation
  /status                 - Show system status
  /ollama-status          - Show Ollama service status
  /ollama-start           - Start embedded Ollama service
  /ollama-stop            - Stop embedded Ollama service
  /ollama-models          - List available Ollama models
  /ollama-preload <model> - Preload a specific model
  /ollama-cache <model>   - Cache a model locally
  /ollama-cached <model>  - Check if model is cached
  /ollama-recommend       - Show recommended models
  /ollama-optimize <task> - Get optimal model for task (code/chat/analysis/creative/fast/general)
  /ollama-perf            - Show model performance metrics
  /ollama-test-perf <mdl> - Run performance test on model
  /ollama-best <task>     - Get best performing model for task
  /ollama-logs            - Show recent Ollama service logs
  /help                   - Show this help
  /exit                   - Exit

For more help: https://github.com/ItsMehRAWRXD/RawrXD/wiki
)");
                return 0;
            }
        }

        // Initialize engine manager
        state.engine_manager = std::make_unique<EngineManager>();

        // Load default engines
        state.engine_manager->LoadEngine("engines/800b-5drive/800b_engine.dll", "800b-5drive");
        state.engine_manager->LoadEngine("engines/codex-ultimate/codex.dll", "codex-ultimate");
        state.engine_manager->LoadEngine("engines/rawrxd-compiler/compiler.dll", "rawrxd-compiler");

        // Initialize VSIX loader
        state.vsix_loader = std::make_unique<VSIXLoader>();
        state.vsix_loader->Initialize("plugins");

        // Initialize memory manager
        state.memory_manager = std::make_unique<MemoryManager>();
        state.memory_manager->SetContextSize(state.context_size);

        // Initialize inference engine (used only for local file-backed models)
        state.inference_engine = std::make_shared<RawrXD::CPUInferenceEngine>();

        // Initialize embedded Ollama service
        if (state.use_embedded_ollama)
        {
            RawrXD::CRTFreeConsole::WriteLine("[Embedded Ollama] Initializing embedded Ollama service...");
            state.embedded_ollama = std::make_unique<OllamaServiceManager>(nullptr);

            // Set up logging callback for CLI output
            state.embedded_ollama->setLogCallback(
                [](const OllamaServiceManager::LogEntry& entry)
                { std::cout << "[Ollama] " << OllamaUtils::formatLogEntry(entry) << std::endl; });

            // Set up state callback for service status updates
            state.embedded_ollama->setStateCallback(
                [](OllamaServiceManager::ServiceState state, const std::string& message)
                {
                    std::cout << "[Ollama] Service state: " << OllamaUtils::serviceStateToString(state);
                    if (!message.empty())
                    {
                        std::cout << " - " << message;
                    }
                    std::cout << std::endl;
                });

            if (state.embedded_ollama->initialize())
            {
                RawrXD::CRTFreeConsole::WriteLine("[Embedded Ollama] Service manager initialized successfully");

                // Try to start the service
                if (state.embedded_ollama->startService())
                {
                    RawrXD::CRTFreeConsole::WriteFormat("[Embedded Ollama] Service started at %s\n",
                                                        state.embedded_ollama->getServiceEndpoint().c_str());
                }
                else
                {
                    RawrXD::CRTFreeConsole::WriteLine(
                        "[Embedded Ollama] Failed to start service, falling back to external Ollama");
                    state.use_embedded_ollama = false;
                }
            }
            else
            {
                RawrXD::CRTFreeConsole::WriteLine(
                    "[Embedded Ollama] Initialization failed, falling back to external Ollama");
                state.use_embedded_ollama = false;
            }
        }

        // Load model or select HTTP Ollama model reference
        while (!state.loaded_model)
        {
            if (state.model_path.empty())
            {
                RawrXD::CRTFreeConsole::Write("Enter model path (GGUF/Blob): ");
                state.model_path = RawrXD::CRTFreeConsole::ReadLine().c_str();
                if (state.model_path.empty())
                    continue;
            }

            // Remove quotes
            if (!state.model_path.empty() && state.model_path.front() == '"')
                state.model_path.erase(0, 1);
            if (!state.model_path.empty() && state.model_path.back() == '"')
                state.model_path.pop_back();

            if (!IsLikelyLocalFileModelPath(state.model_path))
            {
                state.model_path = NormalizeOllamaModelRef(state.model_path);
                state.loaded_model = true;
                state.model_ready = true;
                state.loaded_model_name = state.model_path;

                // Check if embedded Ollama service is available
                if (state.use_embedded_ollama && state.embedded_ollama && state.embedded_ollama->isServiceHealthy())
                {
                    RawrXD::CRTFreeConsole::WriteFormat("[HTTP] Using embedded Ollama service for model: %s\n",
                                                        state.model_path.c_str());
                    RawrXD::CRTFreeConsole::WriteFormat("[HTTP] Service endpoint: %s\n",
                                                        state.embedded_ollama->getServiceEndpoint().c_str());
                }
                else
                {
                    RawrXD::CRTFreeConsole::WriteFormat("[HTTP] Using external Ollama HTTP service for model: %s\n",
                                                        state.model_path.c_str());
                    RawrXD::CRTFreeConsole::WriteLine(
                        "[HTTP] Note: Embedded Ollama service is not available, ensure external Ollama is running");
                }
                break;
            }

            RawrXD::CRTFreeConsole::WriteFormat("[Loading] %s...\n", state.model_path.c_str());
            if (state.inference_engine->LoadModel(state.model_path))
            {
                state.loaded_model = true;
                state.loaded_model_name = state.model_path;
                state.model_ready = true;
                RawrXD::CRTFreeConsole::WriteFormat("[Success] Model loaded: %s\n", state.model_path.c_str());
            }
            else
            {
                RawrXD::CRTFreeConsole::WriteError(
                    ("[Error] Failed to load model: " + state.model_path + "\n").c_str());
                state.model_path.clear();
            }
        }

        // Initialize agentic engine
        state.agent_engine = std::make_shared<AgenticEngine>();
        if (state.inference_engine && IsLikelyLocalFileModelPath(state.model_path))
        {
            state.agent_engine->setInferenceEngine(state.inference_engine.get());
        }
        // Configure HTTP chat provider only when using an Ollama HTTP model reference
        if (!IsLikelyLocalFileModelPath(state.model_path))
        {
            const std::string httpModelRef = state.model_path;
            const double temperature = state.temperature;
            const double top_p = state.top_p;
            const int max_tokens = state.max_tokens;
            state.agent_engine->setChatProvider(
                [httpModelRef, temperature, top_p, max_tokens](const std::string& message)
                { return QueryOllamaAPI(httpModelRef, message, temperature, top_p, max_tokens); });
        }

        // Configure agent
        GenerationConfig config;
        config.temperature = state.temperature;
        config.topP = state.top_p;
        config.maxTokens = state.max_tokens;
        config.maxMode = state.enable_max_mode;
        config.deepThinking = state.enable_deep_thinking;
        config.deepResearch = state.enable_deep_research;
        config.noRefusal = state.enable_no_refusal;
        config.autoCorrect = state.enable_autocorrect;
        state.agent_engine->updateConfig(config);

        // Initialize API server
        state.api_server = std::make_unique<APIServer>(state);
        state.api_server->Start(11434);
        RawrXD::CRTFreeConsole::WriteLine("[API] Server started on http://localhost:11434");

        // Initialize governor if requested
        if (state.enable_overclock_governor)
        {
            state.governor = std::make_unique<OverclockGovernor>();
            state.governor->Start(state);
            RawrXD::CRTFreeConsole::WriteLine("[Governor] Started");
        }

        // Initialize Codex Ultimate
        state.codex = std::make_unique<CodexUltimate>();

        // Initialize interactive shell
        ShellConfig shell_config;
        shell_config.cli_mode = true;
        shell_config.auto_save_history = true;
        shell_config.history_file = "shell_history.txt";
        shell_config.max_history_size = 1000;
        shell_config.enable_suggestions = true;
        shell_config.enable_autocomplete = true;

        state.shell = std::make_unique<InteractiveShell>(shell_config);
        RAWRXD_PHASE_SET(RawrXD::Diagnostics::InitPhase::UI, "Shell initialized");

        // Register production-ready Ollama commands
        // Register shell commands — Ollama commands handle absent service internally;
        // agent tool commands have no Ollama dependency and must always be registered.
        // Backend tool dispatch helper — defined at function scope so shell-registered lambdas
        // can safely capture it by reference throughout the shell's lifetime.
        auto runBackendTool = [&](const std::string& toolName, nlohmann::json toolArgs) -> bool
        {
            auto& handlers = RawrXD::Agent::AgentToolHandlers::Instance();
            if (!handlers.HasTool(toolName))
            {
                RawrXD::CRTFreeConsole::WriteFormat("Backend tool unavailable: %s\n", toolName.c_str());
                return false;
            }
            const auto result = handlers.Execute(toolName, toolArgs);
            RawrXD::CRTFreeConsole::WriteFormat("%s\n", result.toJson().dump(2).c_str());
            return result.isSuccess();
        };
        {
            state.shell->registerCommand(
                "ollama-status",
                [&](const auto& args)
                {
                    if (!state.embedded_ollama)
                    {
                        RawrXD::CRTFreeConsole::Write("Ollama service not enabled.\n");
                        return;
                    }
                    RawrXD::CRTFreeConsole::WriteFormat(
                        "Ollama Service Status:\n  State: %s\n  Healthy: %s\n  Endpoint: %s\n  PID: %lu\n  Version: "
                        "%s\n",
                        OllamaUtils::serviceStateToString(state.embedded_ollama->getServiceState()).c_str(),
                        state.embedded_ollama->isServiceHealthy() ? "Yes" : "No",
                        state.embedded_ollama->getServiceEndpoint().c_str(), state.embedded_ollama->getServicePID(),
                        state.embedded_ollama->getOllamaVersion().c_str());
                    auto models = state.embedded_ollama->getLoadedModels();
                    RawrXD::CRTFreeConsole::WriteFormat("  Loaded Models (%zu):\n", models.size());
                    for (const auto& m : models)
                    {
                        RawrXD::CRTFreeConsole::Write("    - ");
                        RawrXD::CRTFreeConsole::Write(m.c_str());
                        RawrXD::CRTFreeConsole::Write("\n");
                    }
                });
            state.shell->registerCommand("ollama-start",
                                         [&state](const auto& args)
                                         {
                                             if (state.embedded_ollama && state.embedded_ollama->startService())
                                                 RawrXD::CRTFreeConsole::Write("Ollama service started.\n");
                                             else
                                                 RawrXD::CRTFreeConsole::Write("Failed to start Ollama service.\n");
                                         });
            state.shell->registerCommand("ollama-stop",
                                         [&state](const auto& args)
                                         {
                                             if (state.embedded_ollama && state.embedded_ollama->stopService())
                                                 RawrXD::CRTFreeConsole::Write("Ollama service stopped.\n");
                                             else
                                                 RawrXD::CRTFreeConsole::Write("Failed to stop Ollama service.\n");
                                         });
            state.shell->registerCommand("ollama-models",
                                         [&state](const auto& args)
                                         {
                                             if (!state.embedded_ollama)
                                             {
                                                 RawrXD::CRTFreeConsole::Write("Ollama service not enabled.\n");
                                                 return;
                                             }
                                             auto models = state.embedded_ollama->getAvailableModels();
                                             RawrXD::CRTFreeConsole::Write("Available Models:\n");
                                             for (const auto& m : models)
                                             {
                                                 RawrXD::CRTFreeConsole::Write("  - ");
                                                 RawrXD::CRTFreeConsole::Write(m.c_str());
                                                 RawrXD::CRTFreeConsole::Write("\n");
                                             }
                                         });
            state.shell->registerCommand(
                "ollama-pull",
                [&state](const auto& args)
                {
                    if (!state.embedded_ollama || args.size() < 2)
                    {
                        RawrXD::CRTFreeConsole::WriteLine("Usage: ollama-pull <model_name>");
                        return;
                    }
                    if (state.embedded_ollama->pullModel(args[1]))
                        RawrXD::CRTFreeConsole::WriteFormat("Model '%s' is being pulled.\n", args[1].c_str());
                    else
                        RawrXD::CRTFreeConsole::WriteFormat("Failed to pull model '%s'.\n", args[1].c_str());
                });
            state.shell->registerCommand(
                "ollama-preload",
                [&state](const auto& args)
                {
                    if (!state.embedded_ollama || args.size() < 2)
                    {
                        RawrXD::CRTFreeConsole::WriteLine("Usage: ollama-preload <model_name>");
                        return;
                    }
                    if (state.embedded_ollama->preloadModel(args[1]))
                        RawrXD::CRTFreeConsole::WriteFormat("Model '%s' preloaded.\n", args[1].c_str());
                    else
                        RawrXD::CRTFreeConsole::WriteFormat("Failed to preload model '%s'.\n", args[1].c_str());
                });
            state.shell->registerCommand("ollama-logs",
                                         [&state](const auto& args)
                                         {
                                             if (!state.embedded_ollama)
                                             {
                                                 RawrXD::CRTFreeConsole::WriteLine("Ollama service not enabled.");
                                                 return;
                                             }
                                             auto logs = state.embedded_ollama->getRecentLogs(50);
                                             RawrXD::CRTFreeConsole::WriteLine("Recent Ollama Logs:");
                                             for (const auto& l : logs)
                                                 RawrXD::CRTFreeConsole::WriteFormat(
                                                     "%s\n", OllamaUtils::formatLogEntry(l).c_str());
                                         });


            // New agent tool commands (CLI/GUI parity: same backend tools as /api/tool)
            state.shell->registerCommand("compact",
                                         [&](const auto& args)
                                         {
                                             nlohmann::json toolArgs = nlohmann::json::object();
                                             toolArgs["history_log_path"] = "shell_history.txt";
                                             toolArgs["checkpoint_path"] = "sessions/compacted_conversation.json";
                                             runBackendTool("compact_conversation", toolArgs);
                                         });

            state.shell->registerCommand("optimize",
                                         [&](const auto& args)
                                         {
                                             if (args.size() < 2)
                                             {
                                                 RawrXD::CRTFreeConsole::WriteLine("Usage: optimize <intent text>");
                                                 return;
                                             }
                                             std::string intent;
                                             for (size_t i = 1; i < args.size(); ++i)
                                             {
                                                 if (i > 1)
                                                     intent += " ";
                                                 intent += args[i];
                                             }
                                             nlohmann::json toolArgs = nlohmann::json::object();
                                             toolArgs["task"] = intent;
                                             toolArgs["context"] = "cli_shell";
                                             runBackendTool("optimize_tool_selection", toolArgs);
                                         });

            state.shell->registerCommand("resolve",
                                         [&](const auto& args)
                                         {
                                             if (args.size() < 2)
                                             {
                                                 RawrXD::CRTFreeConsole::WriteLine("Usage: resolve <symbol>");
                                                 return;
                                             }
                                             nlohmann::json toolArgs = nlohmann::json::object();
                                             toolArgs["symbol"] = args[1];
                                             runBackendTool("resolve_symbol", toolArgs);
                                         });

            state.shell->registerCommand(
                "read",
                [&state](const auto& args)
                {
                    if (args.size() < 2)
                    {
                        RawrXD::CRTFreeConsole::WriteLine("Usage: read <start>-<end> <file>");
                        return;
                    }
                    std::string range = args[1];
                    std::string file = args.size() > 2 ? args[2] : "";
                    if (file.empty())
                    {
                        RawrXD::CRTFreeConsole::WriteLine("File path required.");
                        return;
                    }
                    // Path validation
                    if (file.find("..") != std::string::npos || file.find(":") != std::string::npos)
                    {
                        RawrXD::CRTFreeConsole::WriteLine("Invalid file path: path traversal not allowed.");
                        return;
                    }
                    namespace fs = std::filesystem;
                    if (!fs::exists(file) || !fs::is_regular_file(file))
                    {
                        RawrXD::CRTFreeConsole::WriteLine("File does not exist or is not a regular file.");
                        return;
                    }
                    size_t dash = range.find('-');
                    if (dash == std::string::npos)
                    {
                        RawrXD::CRTFreeConsole::WriteLine("Invalid range format. Use start-end");
                        return;
                    }
                    try
                    {
                        int start = std::stoi(range.substr(0, dash));
                        int end = std::stoi(range.substr(dash + 1));
                        if (start < 1 || end < start)
                        {
                            RawrXD::CRTFreeConsole::WriteLine("Invalid line range.");
                            return;
                        }
                        nlohmann::json toolArgs = nlohmann::json::object();
                        toolArgs["path"] = file;
                        toolArgs["line_start"] = start;
                        toolArgs["line_end"] = end;
                        runBackendTool("read_lines", toolArgs);
                    }
                    catch (const std::exception&)
                    {
                        RawrXD::CRTFreeConsole::WriteLine("Invalid line numbers.");
                    }
                });

            state.shell->registerCommand("plan",
                                         [&](const auto& args)
                                         {
                                             nlohmann::json toolArgs = nlohmann::json::object();
                                             if (args.size() > 1)
                                             {
                                                 toolArgs["query"] = args[1];
                                             }
                                             else
                                             {
                                                 toolArgs["query"] =
                                                     "Targeted code exploration plan for current project";
                                             }
                                             runBackendTool("plan_code_exploration", toolArgs);
                                         });

            state.shell->registerCommand(
                "search",
                [&](const auto& args)
                {
                    if (args.size() < 2)
                    {
                        RawrXD::CRTFreeConsole::WriteLine("Usage: search <pattern>");
                        return;
                    }
                    std::string pattern = args[1];
                    if (pattern.find("..") != std::string::npos || pattern.find(":") != std::string::npos)
                    {
                        RawrXD::CRTFreeConsole::WriteLine("Invalid pattern: path traversal not allowed.");
                        return;
                    }
                    nlohmann::json toolArgs = nlohmann::json::object();
                    toolArgs["query"] = pattern;
                    toolArgs["max_results"] = 100;
                    runBackendTool("search_files", toolArgs);
                });

            state.shell->registerCommand("evaluate",
                                         [&](const auto& args)
                                         {
                                             nlohmann::json toolArgs = nlohmann::json::object();
                                             if (args.size() > 1)
                                             {
                                                 toolArgs["target"] = args[1];
                                             }
                                             else
                                             {
                                                 toolArgs["target"] = "src";
                                             }
                                             runBackendTool("evaluate_integration_audit_feasibility", toolArgs);
                                         });

            state.shell->registerCommand("tool-capabilities",
                                         [&state](const auto& args)
                                         {
                                             nlohmann::json out = nlohmann::json::object();
                                             out["success"] = true;
                                             out["outsideHotpatchAccessible"] = true;
                                             out["surface"] = "cli_shell";
                                             out["endpointParity"] = "/api/tool/capabilities";
                                             out["tools"] = RawrXD::Agent::AgentToolHandlers::GetAllSchemas();
                                             RawrXD::CRTFreeConsole::WriteFormat("%s\n", out.dump(2).c_str());
                                         });

            state.shell->registerCommand(
                "agent-capabilities",
                [&state](const auto& args)
                {
                    const std::string payload =
                        R"({"success":true,"surface":"cli_shell","outsideHotpatchAccessible":true,"routes":{"chat":"/api/chat","tool":"/api/tool","toolCapabilities":"/api/tool/capabilities","orchestrate":"/api/agent/orchestrate","intent":"/api/agent/intent","subagent":"/api/subagent","chain":"/api/chain","swarm":"/api/swarm","swarmStatus":"/api/swarm/status","agents":"/api/agents","agentsStatus":"/api/agents/status","agentsHistory":"/api/agents/history","agentsReplay":"/api/agents/replay","agentImplementationAudit":"/api/agent/implementation-audit","agentCursorGapAudit":"/api/agent/cursor-gap-audit","agentRuntimeFallbackMetrics":"/api/agent/runtime-fallback-metrics","agentRuntimeFallbackMetricsReset":"/api/agent/runtime-fallback-metrics/reset","agentRuntimeFallbackMetricSurfaces":"/api/agent/runtime-fallback-metrics/surfaces","agentParityMatrix":"/api/agent/parity-matrix","agentGlobalWrapperAudit":"/api/agent/global-wrapper-audit","agentWiringAudit":"/api/agent/wiring-audit"},"notes":["Use /api/tool for canonical backend tool execution","Use /api/agent/orchestrate for intent planning + speculative execution + synthesized reply","Use /api/agent/wiring-audit to verify fallback-family backend mappings","Hotpatch ops remain under /api/agent/ops/* for compatibility only"]})";
                    RawrXD::CRTFreeConsole::WriteFormat("%s\n", payload.c_str());
                });

            state.shell->registerCommand(
                "agent-parity-matrix",
                [&state](const auto& args)
                {
                    nlohmann::json out = RawrXD::Core::BuildAgentParityMatrix("cli_shell");
                    RawrXD::CRTFreeConsole::WriteFormat("%s\n", out.dump(2).c_str());
                });

            state.shell->registerCommand(
                "agent-global-wrapper-audit",
                [&state](const auto& args)
                {
                    const std::string root = RawrXD::Core::ResolveAuditRepoRoot();
                    nlohmann::json out = nlohmann::json::object();
                    out["success"] = true;
                    out["surface"] = "cli_shell";
                    out["outsideHotpatchAccessible"] = true;
                    out["globalWrapperMacroAudit"] = RawrXD::Core::BuildGlobalWrapperMacroAudit(root);
                    RawrXD::CRTFreeConsole::WriteFormat("%s\n", out.dump(2).c_str());
                });

            state.shell->registerCommand(
                "agent-wiring-audit",
                [&state](const auto& args)
                {
                    const nlohmann::json coverage =
                        RawrXD::Core::BuildCoverageSnapshotFromReport(RawrXD::Core::ResolveAuditRepoRoot());
                    nlohmann::json out = RawrXD::Core::BuildAgentCapabilityAudit("cli_shell", coverage, "report_snapshot");
                    out["orchestratorRoute"] = "/api/agent/orchestrate";
                    out["toolRoute"] = "/api/tool";
                    out["capabilitiesRoute"] = "/api/tool/capabilities";
                    out["familyMappings"] = {{"handleAI*", "optimize_tool_selection|next_edit_hint"},
                                             {"handleAgent*", "optimize_tool_selection|execute_command"},
                                             {"handleSubagent*", "plan_code_exploration|plan_tasks|load_rules"},
                                             {"handleAutonomy*", "load_rules|plan_tasks"},
                                             {"handleRouter*", "optimize_tool_selection|load_rules"},
                                             {"handleHelp*/handleEdit*", "search_code"},
                                             {"handleTools*", "run_shell"},
                                             {"handleView*", "list_dir"},
                                             {"handleTelemetry*", "load_rules|plan_tasks"},
                                             {"handleLsp*", "get_diagnostics"},
                                             {"handleModel*", "search_files"},
                                             {"handlePlugin*/handleMarketplace*/handleVscExt*/handleVscext*", "list_dir"},
                                             {"handleAsm*/handleReveng*/handleRE*", "search_code"},
                                             {"handleTheme*/handleVoice*/handleTransparency*", "plan_tasks"},
                                             {"handleReplay*", "restore_checkpoint"},
                                             {"handleGovernor*/handleGov*/handleSafety*", "load_rules|plan_tasks"},
                                             {"handleFile*", "list_dir"},
                                             {"handleMonaco*/handleTier1*/handleQw*/handleTrans*", "plan_tasks"},
                                             {"handleSwarm*", "plan_code_exploration"},
                                             {"handleHotpatch*", "load_rules|plan_tasks"},
                                             {"handleAudit*", "load_rules"},
                                             {"handleGit*", "git_status"},
                                             {"handleEditor*", "plan_tasks"},
                                             {"handleTerminal*", "run_shell"},
                                             {"handleDecomp*", "search_code"},
                                             {"handlePdb*/handleModules*", "search_files"},
                                             {"handleGauntlet*", "plan_tasks"},
                                             {"handleConfidence*", "load_rules"},
                                             {"handleBeacon*", "search_files"},
                                             {"handleVision*", "semantic_search"}};
                    out["quietByDefault"] = true;
                    out["showActionsToggle"] = "show_actions|include_actions";
                    RawrXD::CRTFreeConsole::WriteFormat("%s\n", out.dump(2).c_str());
                });

            state.shell->registerCommand("checkpoint",
                                         [&](const auto& args)
                                         {
                                             nlohmann::json toolArgs = nlohmann::json::object();
                                             if (args.size() > 1 && args[1] == "restore")
                                             {
                                                 toolArgs["checkpoint_path"] = "sessions/runtime_checkpoint.json";
                                                 runBackendTool("restore_checkpoint", toolArgs);
                                                 return;
                                             }
                                             toolArgs["checkpoint_path"] = "sessions/runtime_checkpoint.json";
                                             runBackendTool("save_checkpoint", toolArgs);
                                         });

            state.shell->registerCommand(
                "orchestrate",
                [&](const auto& args)
                {
                    nlohmann::json toolArgs = nlohmann::json::object();
                    std::string prompt;
                    for (size_t i = 1; i < args.size(); ++i)
                    {
                        if (!prompt.empty())
                            prompt += " ";
                        prompt += args[i];
                    }
                    toolArgs["prompt"] = prompt.empty() ? "general assistance" : prompt;
                    runBackendTool("orchestrate_conversation", toolArgs);
                });

            state.shell->registerCommand(
                "speculate",
                [&](const auto& args)
                {
                    nlohmann::json toolArgs = nlohmann::json::object();
                    std::string prompt;
                    for (size_t i = 1; i < args.size(); ++i)
                    {
                        if (!prompt.empty())
                            prompt += " ";
                        prompt += args[i];
                    }
                    toolArgs["prompt"] = prompt.empty() ? "predict next best actions" : prompt;
                    runBackendTool("speculate_next", toolArgs);
                });

            state.shell->registerCommand(
                "subagent",
                [&](const auto& args)
                {
                    nlohmann::json toolArgs = nlohmann::json::object();
                    std::string query;
                    for (size_t i = 1; i < args.size(); ++i)
                    {
                        if (!query.empty())
                            query += " ";
                        query += args[i];
                    }
                    toolArgs["query"] = query.empty() ? "Execute delegated subagent task" : query;
                    toolArgs["task_type"] = "analyze";
                    toolArgs["mode"] = "subagent";
                    runBackendTool("plan_code_exploration", toolArgs);
                });

            state.shell->registerCommand(
                "chain",
                [&](const auto& args)
                {
                    nlohmann::json toolArgs = nlohmann::json::object();
                    std::string goal;
                    for (size_t i = 1; i < args.size(); ++i)
                    {
                        if (!goal.empty())
                            goal += " ";
                        goal += args[i];
                    }
                    toolArgs["task"] = goal.empty() ? "Run chained multi-step analysis" : goal;
                    toolArgs["owner"] = "chain";
                    toolArgs["deadline"] = "asap";
                    runBackendTool("plan_tasks", toolArgs);
                });

            state.shell->registerCommand(
                "swarm",
                [&](const auto& args)
                {
                    nlohmann::json toolArgs = nlohmann::json::object();
                    std::string goal;
                    for (size_t i = 1; i < args.size(); ++i)
                    {
                        if (!goal.empty())
                            goal += " ";
                        goal += args[i];
                    }
                    toolArgs["goal"] = goal.empty() ? "Run swarm orchestration analysis" : goal;
                    toolArgs["mode"] = "swarm";
                    runBackendTool("plan_code_exploration", toolArgs);
                });
        }

        // Handle one-shot chat if requested
        if (chat_mode)
        {
            std::cout << "[Chat] Running prompt: " << chat_prompt << std::endl;

            std::string response = state.agent_engine->chat(chat_prompt);
            if (response.empty())
            {
                std::cerr << "[Error] HTTP Ollama request failed. Ensure Ollama is reachable at localhost:11434."
                          << std::endl;
                return 2;
            }
            std::cout << response << std::endl;
            return 0;
        }

        RAWRXD_PHASE_SET(RawrXD::Diagnostics::InitPhase::Ready, "Shell loop start");

        state.shell->Start(
            state.agent_engine.get(), state.memory_manager.get(), state.vsix_loader.get(),
            nullptr,  // React generator not needed in CLI
            [](const std::string& output) { std::cout << output; },
            [](const std::string& input)
            {
                // Input handling is done by shell
            });

        // Main loop
        std::cout << "\n=== RawrXD Interactive AI Shell v6.0 Ready ===\n";
        std::cout << "Model: " << state.model_path << "\n";
        std::cout << "Context: " << state.context_size / 1024 << "K\n";
        std::cout << "Engine: " << state.engine_manager->GetCurrentEngine() << "\n";
        std::cout << "Modes: " << (state.enable_max_mode ? "Max " : "")
                  << (state.enable_deep_thinking ? "Thinking " : "") << (state.enable_deep_research ? "Research " : "")
                  << (state.enable_no_refusal ? "NoRefusal " : "") << (state.enable_autocorrect ? "AutoCorrect " : "")
                  << "\n";
        std::cout << "Type /help for commands, /exit to quit\n\n";

        // Run shell with bounded iterations instead of infinite loop
        // Safety cutoff: 100,000 polling cycles * 100ms ≈ 2.8 hours of runtime
        const int kMaxShellPollingCycles = 100000;
        int iteration_count = 0;
        while (state.shell->IsRunning() && iteration_count < kMaxShellPollingCycles)
        {
            std::this_thread::sleep_for(100ms);
            iteration_count++;
        }

        if (iteration_count >= kMaxShellPollingCycles)
        {
            std::cout << "[Warning] Shell reached max iterations, forcing exit\n";
        }

        // Cleanup
        if (state.embedded_ollama)
        {
            std::cout << "[Embedded Ollama] Shutting down service..." << std::endl;
            state.embedded_ollama->stopService();
            state.embedded_ollama.reset();
            std::cout << "[Embedded Ollama] Service stopped" << std::endl;
        }

        if (state.governor)
        {
            state.governor->Stop();
            std::cout << "[Governor] Stopped\n";
        }

        state.api_server->Stop();
        std::cout << "[API] Stopped\n";

        std::cout << "\n[Shutdown] RawrXD terminated gracefully.\n";
        return 0;
    }
