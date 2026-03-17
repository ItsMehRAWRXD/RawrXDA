#include <iostream>
#include <string>
#include <csignal>
#include <vector>
#include <thread>
#include <chrono>
#include "memory_core.h"
#include "cpu_inference_engine.h"
#include "complete_server.h"
#include "agentic_engine.h"
#include "subagent_core.h"
#include "agent_history.h"

void SignalHandler(int signal) {
    std::cout << "\n[ENGINE] Exiting...\n";
    exit(0);
}

int main(int argc, char** argv) {
    std::signal(SIGINT, SignalHandler);
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║              RawrXD Engine v7.2 — Agentic Core               ║
║  Subagents • Chaining • HexMag Swarm • History & Replay     ║
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;

    std::string model_path;
    uint16_t port = 8080;
    bool enable_http = true;
    bool enable_repl = true;
    std::string history_dir = "./history";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc) {
            model_path = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--no-http") {
            enable_http = false;
        } else if (arg == "--no-repl") {
            enable_repl = false;
        } else if (arg == "--history-dir" && i + 1 < argc) {
            history_dir = argv[++i];
        } else if (arg == "--help") {
            std::cout << R"(
Usage: RawrEngine [options]
  --model <path>    Path to GGUF model file
  --port <port>     HTTP server port (default: 8080)
  --no-http         Disable HTTP server
  --no-repl         Disable interactive REPL
  --history-dir <d> Directory for history JSONL (default: ./history)
  --help            Show this help

HTTP API Endpoints:
  GET  /status                Server & model status
  POST /complete              Single completion
  POST /complete/stream       SSE streaming completion
  POST /api/chat              Agentic chat
  POST /api/subagent          Spawn a sub-agent
  POST /api/chain             Execute a prompt chain
  POST /api/swarm             Launch a HexMag swarm
  GET  /api/agents            List all sub-agents
  GET  /api/agents/status     Sub-agent manager status
  GET  /api/agents/history    Query event history (Phase 5)
  POST /api/agents/replay     Replay a recorded session (Phase 5)

REPL Commands:
  /subagent <prompt>        Spawn a sub-agent
  /chain <step1> | <step2>  Run a prompt chain
  /swarm <p1> | <p2> ...    Run a HexMag swarm
  /agents                   List active agents
  /status                   Show system status
  /history [agent_id]       Show event history (Phase 5)
  /replay <agent_id>        Replay an agent run (Phase 5)
  exit                      Quit
)" << std::endl;
            return 0;
        }
    }

    RawrXD::CPUInferenceEngine engine;
    if (!model_path.empty()) {
        std::cout << "[SYSTEM] Loading model: " << model_path << "\n";
        if (!engine.LoadModel(model_path)) {
            std::cout << "[SYSTEM] Model load failed. /complete will return empty results.\n";
        } else {
            std::cout << "[SYSTEM] Model loaded.\n";
        }
    } else {
        std::cout << "[SYSTEM] No model specified. Use --model <path> to load a GGUF.\n";
    }

    // Initialize agentic engine
    AgenticEngine agentEngine;
    agentEngine.setInferenceEngine(&engine);

    // Initialize sub-agent manager with console logging
    SubAgentManager subAgentMgr(&agentEngine);
    subAgentMgr.setLogCallback([](int level, const std::string& msg) {
        const char* prefix[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
        if (level >= 0 && level <= 3) {
            std::cout << prefix[level] << " " << msg << "\n";
        }
    });

    // Initialize event history recorder (Phase 5)
    AgentHistoryRecorder historyRecorder(history_dir);
    historyRecorder.setLogCallback([](int level, const std::string& msg) {
        const char* prefix[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
        if (level >= 0 && level <= 3) {
            std::cout << prefix[level] << " " << msg << "\n";
        }
    });
    subAgentMgr.setHistoryRecorder(&historyRecorder);
    std::cout << "[SYSTEM] Event history: session=" << historyRecorder.sessionId()
              << " dir=" << history_dir << "\n";

    // Start HTTP server with agentic support
    RawrXD::CompletionServer server;
    if (enable_http) {
        server.SetAgenticEngine(&agentEngine);
        server.SetSubAgentManager(&subAgentMgr);
        server.SetHistoryRecorder(&historyRecorder);
        server.Start(port, &engine, model_path);
    }

    if (enable_repl) {
        std::cout << "[SYSTEM] Interactive REPL ready. Type 'exit' to quit, /help for commands.\n\n";
        std::string input;
        while (true) {
            std::cout << "RawrXD> ";
            std::getline(std::cin, input);
            if (input == "exit" || input == "/exit") break;

            if (input == "/help") {
                std::cout << "Commands:\n"
                          << "  /chat <message>         Chat with the agentic engine\n"
                          << "  /subagent <prompt>      Spawn a sub-agent\n"
                          << "  /chain <s1> | <s2> ...  Run a sequential chain\n"
                          << "  /swarm <p1> | <p2> ...  Run a HexMag swarm\n"
                          << "  /agents                 List all sub-agents\n"
                          << "  /status                 System status\n"
                          << "  /todo                   Show todo list\n"
                          << "  /history [agent_id]     Event history (Phase 5)\n"
                          << "  /replay <agent_id>      Replay an agent run (Phase 5)\n"
                          << "  /stats                  History statistics (Phase 5)\n"
                          << "  exit                    Quit\n\n";
            }
            else if (input.substr(0, 5) == "/chat" || (input[0] != '/' && !input.empty())) {
                std::string msg = (input.substr(0, 5) == "/chat") ? input.substr(6) : input;
                if (agentEngine.isModelLoaded()) {
                    std::string response = agentEngine.chat(msg);
                    std::cout << response << "\n";
                    // Auto-dispatch any tool calls in the response
                    std::string toolResult;
                    if (subAgentMgr.dispatchToolCall("repl", response, toolResult)) {
                        std::cout << "\n[Tool Result]\n" << toolResult << "\n";
                    }
                } else {
                    std::cout << "[ERROR] No model loaded.\n";
                }
            }
            else if (input.substr(0, 10) == "/subagent ") {
                std::string prompt = input.substr(10);
                std::cout << "[SubAgent] Spawning...\n";
                std::string id = subAgentMgr.spawnSubAgent("repl", "REPL subagent", prompt);
                bool ok = subAgentMgr.waitForSubAgent(id, 120000);
                std::string result = subAgentMgr.getSubAgentResult(id);
                std::cout << "[SubAgent " << (ok ? "✅" : "❌") << "] " << result << "\n";
            }
            else if (input.substr(0, 7) == "/chain ") {
                std::string rest = input.substr(7);
                // Split on " | "
                std::vector<std::string> steps;
                size_t pos = 0;
                while (true) {
                    size_t delim = rest.find(" | ", pos);
                    if (delim == std::string::npos) {
                        steps.push_back(rest.substr(pos));
                        break;
                    }
                    steps.push_back(rest.substr(pos, delim - pos));
                    pos = delim + 3;
                }
                std::cout << "[Chain] Running " << steps.size() << " steps...\n";
                std::string result = subAgentMgr.executeChain("repl", steps);
                std::cout << "[Chain Result]\n" << result << "\n";
            }
            else if (input.substr(0, 7) == "/swarm ") {
                std::string rest = input.substr(7);
                std::vector<std::string> prompts;
                size_t pos = 0;
                while (true) {
                    size_t delim = rest.find(" | ", pos);
                    if (delim == std::string::npos) {
                        prompts.push_back(rest.substr(pos));
                        break;
                    }
                    prompts.push_back(rest.substr(pos, delim - pos));
                    pos = delim + 3;
                }
                std::cout << "[Swarm] Launching " << prompts.size() << " tasks...\n";
                std::string result = subAgentMgr.executeSwarm("repl", prompts);
                std::cout << "[Swarm Result]\n" << result << "\n";
            }
            else if (input == "/agents") {
                auto agents = subAgentMgr.getAllSubAgents();
                if (agents.empty()) {
                    std::cout << "No sub-agents.\n";
                } else {
                    for (const auto& a : agents) {
                        std::cout << "  " << a.id << " [" << a.stateString() << "] "
                                  << a.description << " (" << a.elapsedMs() << "ms)\n";
                    }
                }
            }
            else if (input == "/status") {
                std::cout << "Model loaded: " << (agentEngine.isModelLoaded() ? "yes" : "no") << "\n";
                std::cout << subAgentMgr.getStatusSummary() << "\n";
            }
            else if (input == "/todo") {
                auto todos = subAgentMgr.getTodoList();
                if (todos.empty()) {
                    std::cout << "Todo list empty.\n";
                } else {
                    for (const auto& t : todos) {
                        std::string icon = "⬜";
                        if (t.status == TodoItem::Status::InProgress) icon = "🔄";
                        else if (t.status == TodoItem::Status::Completed) icon = "✅";
                        else if (t.status == TodoItem::Status::Failed) icon = "❌";
                        std::cout << "  " << icon << " [" << t.id << "] " << t.title << "\n";
                    }
                }
            }
            else {
                std::cout << "Unknown command. Type /help for commands.\n";
            }
        }
    } else {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    server.Stop();
    return 0;
}
