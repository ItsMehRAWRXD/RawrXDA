#include <iostream>
#include <string>
#include <cstdlib>
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
#include "agent_policy.h"
#include "agent_explainability.h"
#include "ai_backend.h"
#include "dml_inference_engine.h"

#include "logging/logger.h"
static Logger s_logger("main");

// Phase 20-25: New subsystem headers
#include "swarm_decision_bridge.h"
#include "universal_model_hotpatcher.h"
#include "production_release.h"
#include "webrtc_signaling.h"
#include "gpu_kernel_autotuner.h"
#include "sandbox_integration.h"
#include "amd_gpu_accelerator.h"

// Phase 21: Distributed Swarm Inference Orchestrator
#include "swarm_orchestrator.h"

// Phase 20: CLI headless systems (for ! commands dispatch)
#include "cli_headless_systems.h"

// Phase 26: ReverseEngineered MASM Kernel — Scheduler, Heartbeat, Deadlock, GPU DMA, Tensor
#include "../include/reverse_engineered_bridge.h"

// Phase 33: Voice Chat Engine
#include "core/voice_chat.hpp"

void SignalHandler(int signal) {
    s_logger.info("\n[ENGINE] Exiting...\n");
    exit(0);
}

int main(int argc, char** argv) {
    std::signal(SIGINT, SignalHandler);
    s_logger.info( R"(
╔══════════════════════════════════════════════════════════════╗
║              RawrXD Engine v7.5 — Agentic Core               ║
║  Subagents • Chaining • HexMag Swarm • History & Replay     ║
║  Phase 25: AMD GPU • Sandbox • 800B Streaming Quant          ║
║  Phase 21: Distributed Swarm Inference • AVX-512 Requant     ║
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;

    // Command registration is automatic via static AutoRegistrar in
    // unified_command_dispatch.cpp — reads COMMAND_TABLE at startup.

    std::string model_path;
    uint16_t port = 23959;
    bool enable_http = true;
    bool enable_repl = true;
    std::string history_dir = "./history";
    std::string policy_dir = "./policies";
    std::string engine_type = "cpu";   // "cpu" or "dml"

    if (const char* envPort = std::getenv("RAWRXD_PORT")) {
        try {
            int p = std::stoi(envPort);
            if (p > 0 && p < 65536) port = static_cast<uint16_t>(p);
        } catch (...) {
            // ignore invalid env var
        }
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc) {
            model_path = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--engine" && i + 1 < argc) {
            engine_type = argv[++i];
        } else if (arg == "--no-http") {
            enable_http = false;
        } else if (arg == "--no-repl") {
            enable_repl = false;
        } else if (arg == "--history-dir" && i + 1 < argc) {
            history_dir = argv[++i];
        } else if (arg == "--policy-dir" && i + 1 < argc) {
            policy_dir = argv[++i];
        } else if (arg == "--help") {
            s_logger.info( R"(
Usage: RawrEngine [options]
  --model <path>    Path to GGUF model file
  --port <port>     HTTP server port (default: 23959)
  --engine <type>   Inference engine: cpu (default) or dml (DirectML GPU)
  --no-http         Disable HTTP server
  --no-repl         Disable interactive REPL
  --history-dir <d> Directory for history JSONL (default: ./history)
  --policy-dir <d>  Directory for policy storage (default: ./policies)
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
  GET  /api/policies          List all policies (Phase 7)
  GET  /api/policies/suggestions  Get policy suggestions (Phase 7)
  POST /api/policies/apply    Accept a suggestion (Phase 7)
  POST /api/policies/reject   Reject a suggestion (Phase 7)
  GET  /api/policies/export   Export policies (Phase 7)
  POST /api/policies/import   Import policies (Phase 7)
  GET  /api/policies/heuristics  Compute heuristics (Phase 7)
  GET  /api/policies/stats    Policy engine stats (Phase 7)
  GET  /api/backends          List all backends (Phase 8B)
  GET  /api/backends/status   Active backend status (Phase 8B)
  POST /api/backends/use      Switch active backend (Phase 8B)
  GET  /api/gpu/status          GPU acceleration status (Phase 25)
  POST /api/gpu/toggle          Toggle GPU on/off (Phase 25)
  GET  /api/gpu/features        AMD GPU features (Phase 25)
  GET  /api/gpu/memory          GPU memory usage (Phase 25)
  GET  /api/tuner/status        Kernel auto-tuner status (Phase 23)
  POST /api/tuner/run           Run kernel auto-tuner (Phase 23)
  GET  /api/swarm/bridge        Swarm decision bridge status (Phase 21)
  GET  /api/hotpatch/model      Universal model hotpatcher (Phase 21)
  GET  /api/webrtc/status       WebRTC signaling status (Phase 20)
  GET  /api/sandbox/list        List sandboxes (Phase 24)
  POST /api/sandbox/create      Create sandbox (Phase 24)
  GET  /api/release/status      Production release status (Phase 22)

REPL Commands:
  /subagent <prompt>        Spawn a sub-agent
  /chain <step1> | <step2>  Run a prompt chain
  /swarm <p1> | <p2> ...    Run a HexMag swarm
  /agents                   List active agents
  /status                   Show system status
  /history [agent_id]       Show event history (Phase 5)
  /replay <agent_id>        Replay an agent run (Phase 5)
  /policies                 List active policies (Phase 7)
  /suggest                  Generate policy suggestions (Phase 7)
  /policy accept <id>       Accept a suggestion (Phase 7)
  /policy reject <id>       Reject a suggestion (Phase 7)
  /policy export <file>     Export policies to file (Phase 7)
  /policy import <file>     Import policies from file (Phase 7)
  /heuristics               Compute & show heuristics (Phase 7)
  /explain <agent_id>       Explain agent decision trace (Phase 8A)
  /explain last             Explain most recent agent (Phase 8A)
  /explain session          Explain full session (Phase 8A)
  /explain snapshot <file>  Export explainability snapshot (Phase 8A)
  /backend list             List all backends (Phase 8B)
  /backend use <id>         Switch active backend (Phase 8B)
  /backend status           Show active backend status (Phase 8B)
  /gpu                     GPU acceleration status (Phase 25)
  /gpu on|off|toggle       Enable/disable GPU (Phase 25)
  /gpu features            AMD GPU features (Phase 25)
  /gpu memory              GPU memory usage (Phase 25)
  /tune                    Run GPU kernel auto-tuner (Phase 23)
  /swarm bridge            Swarm decision bridge status (Phase 21)
  /hotpatch status         Model hotpatcher status (Phase 21)
  /webrtc                  WebRTC signaling status (Phase 20)
  /sandbox list|create     Manage sandboxes (Phase 24)
  /release                 Production release status (Phase 22)
  exit                      Quit
)" << std::endl;
            return 0;
        }
    }

    // Create inference engine based on --engine flag
    RawrXD::CPUInferenceEngine cpuEngine;
    RawrXD::DMLInferenceEngine dmlEngine;
    RawrXD::InferenceEngine* engine = nullptr;

    if (engine_type == "dml" || engine_type == "directml" || engine_type == "gpu") {
        s_logger.info("[SYSTEM] Using DirectML GPU inference engine (AMD RX 7800 XT)\n");
        engine = &dmlEngine;
    } else {
        s_logger.info("[SYSTEM] Using CPU inference engine\n");
        engine = &cpuEngine;
    }

    if (!model_path.empty()) {
        s_logger.info("[SYSTEM] Loading model: ");
        if (!engine->LoadModel(model_path)) {
            s_logger.info("[SYSTEM] Model load failed. /complete will return empty results.\n");
        } else {
            s_logger.info("[SYSTEM] Model loaded via ");
        }
    } else {
        s_logger.info("[SYSTEM] No model specified. Use --model <path> to load a GGUF.\n");
    }

    // Initialize agentic engine
    AgenticEngine agentEngine;
    agentEngine.setInferenceEngine(engine);

    // Initialize sub-agent manager with console logging
    SubAgentManager subAgentMgr(&agentEngine);
    subAgentMgr.setLogCallback([](int level, const std::string& msg) {
        const char* prefix[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
        if (level >= 0 && level <= 3) {
            s_logger.info( prefix[level] << " " << msg << "\n";
        }
    });

    // Initialize event history recorder (Phase 5)
    AgentHistoryRecorder historyRecorder(history_dir);
    historyRecorder.setLogCallback([](int level, const std::string& msg) {
        const char* prefix[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
        if (level >= 0 && level <= 3) {
            s_logger.info( prefix[level] << " " << msg << "\n";
        }
    });
    subAgentMgr.setHistoryRecorder(&historyRecorder);
    s_logger.info("[SYSTEM] Event history: session=");

    // Initialize policy engine (Phase 7)
    PolicyEngine policyEngine(policy_dir);
    policyEngine.setLogCallback([](int level, const std::string& msg) {
        const char* prefix[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
        if (level >= 0 && level <= 3) {
            s_logger.info( prefix[level] << " " << msg << "\n";
        }
    });
    policyEngine.setHistoryRecorder(&historyRecorder);
    subAgentMgr.setPolicyEngine(&policyEngine);
    s_logger.info("[SYSTEM] Policy engine: ");

    // Initialize explainability engine (Phase 8A — read-only)
    ExplainabilityEngine explainEngine;
    explainEngine.setHistoryRecorder(&historyRecorder);
    explainEngine.setPolicyEngine(&policyEngine);
    explainEngine.setLogCallback([](int level, const std::string& msg) {
        const char* prefix[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
        if (level >= 0 && level <= 3) {
            s_logger.info( prefix[level] << " " << msg << "\n";
        }
    });
    s_logger.info("[SYSTEM] Explainability engine: ready (Phase 8A)\n");

    // Initialize AI backend manager (Phase 8B)
    AIBackendManager backendMgr;
    backendMgr.setLogCallback([](int level, const std::string& msg) {
        const char* prefix[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
        if (level >= 0 && level <= 3) {
            s_logger.info( prefix[level] << " " << msg << "\n";
        }
    });
    // Pre-register Ollama as a common second backend
    {
        AIBackendConfig ollama;
        ollama.id          = "ollama";
        ollama.displayName = "Ollama";
        ollama.type        = AIBackendType::Ollama;
        ollama.endpoint    = "http://localhost:11434";
        ollama.model       = "llama3";
        ollama.enabled     = true;
        backendMgr.addBackend(ollama);
    }
    s_logger.info("[SYSTEM] Backend manager: ");

    // ═══════════════════════════════════════════════════════════════════
    // Phase 25: AMD/ATI GPU Acceleration (initialize FIRST — others depend on it)
    // ═══════════════════════════════════════════════════════════════════
    auto& gpuAccel = AMDGPUAccelerator::instance();
    {
        auto r = gpuAccel.initialize(GPUBackend::Auto);
        s_logger.info("[SYSTEM] AMD GPU Accelerator: ");
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 23: GPU Kernel Auto-Tuner
    // ═══════════════════════════════════════════════════════════════════
    auto& kernelTuner = GPUKernelAutoTuner::instance();
    {
        auto r = kernelTuner.initialize();
        s_logger.info("[SYSTEM] GPU Kernel Auto-Tuner: ");
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 21: Swarm Decision Bridge (wires AgenticDecisionTree → Swarm)
    // ═══════════════════════════════════════════════════════════════════
    auto& swarmBridge = SwarmDecisionBridge::instance();
    {
        bool ok = swarmBridge.initialize();
        s_logger.info("[SYSTEM] Swarm Decision Bridge: ");
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 21: Universal Model Hotpatcher (120B-800B streaming quant)
    // ═══════════════════════════════════════════════════════════════════
    auto& modelHotpatcher = UniversalModelHotpatcher::instance();
    {
        bool ok = modelHotpatcher.initialize();
        s_logger.info("[SYSTEM] Universal Model Hotpatcher: ");
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 22: Production Release Engine
    // ═══════════════════════════════════════════════════════════════════
    auto& releaseEngine = ProductionReleaseEngine::instance();
    {
        s_logger.info("[SYSTEM] Production Release Engine: Loaded");
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 20: WebRTC P2P Signaling
    // ═══════════════════════════════════════════════════════════════════
    auto& webrtc = WebRTCSignaling::instance();
    {
        auto r = webrtc.initialize("wss://signal.rawrxd.local:8443");
        s_logger.info("[SYSTEM] WebRTC P2P Signaling: ");
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 24: Windows Sandbox Manager
    // ═══════════════════════════════════════════════════════════════════
    auto& sandboxMgr = SandboxManager::instance();
    {
        auto r = sandboxMgr.initialize();
        s_logger.info("[SYSTEM] Windows Sandbox Manager: ");
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 21: Distributed Swarm Inference Orchestrator
    // ═══════════════════════════════════════════════════════════════════
    auto& swarmOrchestrator = RawrXD::Swarm::SwarmOrchestrator::instance();
    s_logger.info("[SYSTEM] Swarm Orchestrator: ready (use !swarm_join to start)\n");

    // ═══════════════════════════════════════════════════════════════════
    // Phase 26: ReverseEngineered MASM Kernel Subsystems
    // Work-Stealing Scheduler + Conflict Detection + Heartbeat + INFINITY I/O
    // ═══════════════════════════════════════════════════════════════════
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
    {
        auto reResult = RawrXD::ReverseEngineered::InitializeAllSubsystems(
            0,      // workerCount: auto-detect from CPU cores
            0,      // heartbeatPort: 0 = disabled for single-node CLI
            256     // maxResources for conflict detection
        );
        s_logger.info("[SYSTEM] ReverseEngineered Kernel: ");

        if (reResult.success) {
            // Report high-res tick capability
            uint64_t tick = GetHighResTick();
            uint64_t us = TicksToMicroseconds(1);
            s_logger.info("[SYSTEM]   Timer resolution: ");
        }
    }
#else
    s_logger.info("[SYSTEM] ReverseEngineered Kernel: DISABLED (no MASM)\n");
#endif

    // Start HTTP server with agentic support
    RawrXD::CompletionServer server;
    if (enable_http) {
        server.SetAgenticEngine(&agentEngine);
        server.SetSubAgentManager(&subAgentMgr);
        server.SetHistoryRecorder(&historyRecorder);
        server.SetPolicyEngine(&policyEngine);
        server.SetExplainabilityEngine(&explainEngine);
        server.SetBackendManager(&backendMgr);
        server.Start(port, engine, model_path);
    }

    if (enable_repl) {
        s_logger.info("[SYSTEM] Interactive REPL ready. Type 'exit' to quit, /help for commands.\n\n");
        std::string input;
        while (true) {
            s_logger.info("RawrXD> ");
            std::getline(std::cin, input);
            if (input == "exit" || input == "/exit") break;

            if (input == "/help") {
                s_logger.info("Commands:\n");
            }
            else if (input.substr(0, 5) == "/chat" || (input[0] != '/' && !input.empty())) {
                std::string msg = (input.substr(0, 5) == "/chat") ? input.substr(6) : input;
                if (agentEngine.isModelLoaded()) {
                    historyRecorder.recordChatRequest(msg);
                    auto chatStart = std::chrono::steady_clock::now();
                    std::string response = agentEngine.chat(msg);
                    auto chatEnd = std::chrono::steady_clock::now();
                    int chatMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(chatEnd - chatStart).count();
                    historyRecorder.recordChatResponse(response, chatMs);
                    s_logger.info( response << "\n";
                    // Auto-dispatch any tool calls in the response
                    std::string toolResult;
                    if (subAgentMgr.dispatchToolCall("repl", response, toolResult)) {
                        s_logger.info("\n[Tool Result]\n");
                    }
                } else {
                    s_logger.info("[ERROR] No model loaded.\n");
                }
            }
            else if (input.substr(0, 10) == "/subagent ") {
                std::string prompt = input.substr(10);
                s_logger.info("[SubAgent] Spawning...\n");
                std::string id = subAgentMgr.spawnSubAgent("repl", "REPL subagent", prompt);
                bool ok = subAgentMgr.waitForSubAgent(id, 120000);
                std::string result = subAgentMgr.getSubAgentResult(id);
                s_logger.info("[SubAgent ");
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
                s_logger.info("[Chain] Running ");
                std::string result = subAgentMgr.executeChain("repl", steps);
                s_logger.info("[Chain Result]\n");
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
                s_logger.info("[Swarm] Launching ");
                std::string result = subAgentMgr.executeSwarm("repl", prompts);
                s_logger.info("[Swarm Result]\n");
            }
            else if (input == "/agents") {
                auto agents = subAgentMgr.getAllSubAgents();
                if (agents.empty()) {
                    s_logger.info("No sub-agents.\n");
                } else {
                    for (const auto& a : agents) {
                        s_logger.info("  ");
                    }
                }
            }
            else if (input == "/status") {
                s_logger.info("Model loaded: ");
                s_logger.info( subAgentMgr.getStatusSummary() << "\n";
            }
            else if (input == "/todo") {
                auto todos = subAgentMgr.getTodoList();
                if (todos.empty()) {
                    s_logger.info("Todo list empty.\n");
                } else {
                    for (const auto& t : todos) {
                        std::string icon = "⬜";
                        if (t.status == TodoItem::Status::InProgress) icon = "🔄";
                        else if (t.status == TodoItem::Status::Completed) icon = "✅";
                        else if (t.status == TodoItem::Status::Failed) icon = "❌";
                        s_logger.info("  ");
                    }
                }
            }
            // ── Phase 5: History, Timeline & Replay ──
            else if (input == "/history" || input.substr(0, 9) == "/history ") {
                std::string agentFilter;
                if (input.size() > 9) agentFilter = input.substr(9);
                
                std::vector<AgentEvent> events;
                if (!agentFilter.empty()) {
                    events = historyRecorder.getAgentTimeline(agentFilter);
                    s_logger.info("[History] Events for agent ");
                } else {
                    HistoryQuery q;
                    q.limit = 25;
                    events = historyRecorder.query(q);
                    s_logger.info("[History] Last ");
                }
                
                if (events.empty()) {
                    s_logger.info("  (no events)\n");
                } else {
                    for (const auto& e : events) {
                        std::string icon = e.success ? "✅" : "❌";
                        s_logger.info("  ");
                        if (!e.agentId.empty()) s_logger.info(" agent=");
                        s_logger.info(" ");
                        if (e.durationMs > 0) s_logger.info(" (");
                        s_logger.info("\n");
                    }
                }
            }
            else if (input.substr(0, 8) == "/replay ") {
                std::string agentId = input.substr(8);
                s_logger.info("[Replay] Replaying agent ");
                
                ReplayRequest req;
                req.originalAgentId = agentId;
                req.dryRun = false;
                
                ReplayResult result = historyRecorder.replay(req, &subAgentMgr);
                if (result.success) {
                    s_logger.info("[Replay ✅] ");
                    s_logger.info("[Replay Result]\n");
                    if (!result.originalResult.empty()) {
                        s_logger.info("[Original Result]\n");
                    }
                } else {
                    s_logger.info("[Replay ❌] ");
                }
            }
            else if (input == "/stats") {
                s_logger.info("[History Stats] ");
                s_logger.info("Total events: ");
            }
            // ── Phase 7: Policy Engine Commands ──
            else if (input == "/policies") {
                auto policies = policyEngine.getAllPolicies();
                if (policies.empty()) {
                    s_logger.info("[Policies] No policies defined.\n");
                } else {
                    s_logger.info("[Policies] ");
                    for (const auto& p : policies) {
                        std::string icon = p.enabled ? "🟢" : "⚪";
                        s_logger.info("  ");
                        if (!p.description.empty()) {
                            s_logger.info("      ");
                        }
                    }
                }
            }
            else if (input == "/suggest") {
                s_logger.info("[Policy] Analyzing history and generating suggestions...\n");
                auto suggestions = policyEngine.generateSuggestions();
                auto pending = policyEngine.getPendingSuggestions();
                
                if (pending.empty()) {
                    s_logger.info("[Policy] No suggestions at this time. Need more history data.\n");
                } else {
                    s_logger.info("[Policy] ");
                    for (const auto& s : pending) {
                        s_logger.info("\n  📋 Suggestion [");
                        s_logger.info("     Policy: ");
                        s_logger.info("     Rationale: ");
                        s_logger.info("     Estimated improvement: ");
                        s_logger.info("     Supporting events: ");
                        s_logger.info("     → /policy accept ");
                        s_logger.info("     → /policy reject ");
                    }
                }
            }
            else if (input.substr(0, 15) == "/policy accept ") {
                std::string id = input.substr(15);
                if (policyEngine.acceptSuggestion(id)) {
                    s_logger.info("[Policy ✅] Suggestion accepted and policy activated.\n");
                    policyEngine.save();
                } else {
                    s_logger.info("[Policy ❌] Suggestion not found or already decided.\n");
                }
            }
            else if (input.substr(0, 15) == "/policy reject ") {
                std::string id = input.substr(15);
                if (policyEngine.rejectSuggestion(id)) {
                    s_logger.info("[Policy ✅] Suggestion rejected.\n");
                    policyEngine.save();
                } else {
                    s_logger.info("[Policy ❌] Suggestion not found or already decided.\n");
                }
            }
            else if (input.substr(0, 15) == "/policy export ") {
                std::string filePath = input.substr(15);
                if (policyEngine.exportToFile(filePath)) {
                    s_logger.info("[Policy ✅] Policies exported to ");
                } else {
                    s_logger.info("[Policy ❌] Export failed.\n");
                }
            }
            else if (input.substr(0, 15) == "/policy import ") {
                std::string filePath = input.substr(15);
                int count = policyEngine.importFromFile(filePath);
                s_logger.info("[Policy] Imported ");
                if (count > 0) policyEngine.save();
            }
            else if (input == "/heuristics") {
                s_logger.info("[Policy] Computing heuristics from event history...\n");
                policyEngine.computeHeuristics();
                auto heuristics = policyEngine.getAllHeuristics();
                
                if (heuristics.empty()) {
                    s_logger.info("[Policy] No heuristics computed. Need event history.\n");
                } else {
                    s_logger.info("[Policy] ");
                    for (const auto& h : heuristics) {
                        s_logger.info("  📊 ");
                        if (h.avgDurationMs > 0) {
                            s_logger.info(" avg=");
                        }
                        s_logger.info("\n");
                        for (const auto& reason : h.topFailureReasons) {
                            s_logger.info("      ⚠ ");
                        }
                    }
                }
            }
            // ── Phase 8A: Explainability Commands ──
            else if (input == "/explain last") {
                // Find the most recent agent_spawn event
                HistoryQuery lastQ;
                lastQ.limit = 1;
                auto recent = historyRecorder.query(lastQ);
                if (recent.empty()) {
                    s_logger.info("[Explain] No events recorded yet.\n");
                } else {
                    // Walk backwards to find last agent
                    auto all = historyRecorder.allEvents();
                    std::string lastAgent;
                    for (auto it = all.rbegin(); it != all.rend(); ++it) {
                        if (!it->agentId.empty()) { lastAgent = it->agentId; break; }
                    }
                    if (lastAgent.empty()) {
                        s_logger.info("[Explain] No agent events found.\n");
                    } else {
                        auto trace = explainEngine.traceAuto(lastAgent);
                        s_logger.info("[Explain] ");
                        for (const auto& node : trace.nodes) {
                            std::string icon = node.success ? "✅" : "❌";
                            s_logger.info("  ");
                            if (!node.policyId.empty())
                                s_logger.info(" 🛡️ ");
                            if (node.durationMs > 0)
                                s_logger.info(" (");
                            s_logger.info("\n");
                        }
                    }
                }
            }
            else if (input == "/explain session") {
                auto session = explainEngine.explainSession();
                s_logger.info("[Explain] Session narrative:\n");

                if (!session.failureAttributions.empty()) {
                    s_logger.info("Failure attributions:\n");
                    for (const auto& fa : session.failureAttributions) {
                        s_logger.info("  ❌ ");
                        if (fa.wasRetried)
                            s_logger.info(" → retried (");
                        if (!fa.policyId.empty())
                            s_logger.info(" [policy: ");
                        s_logger.info("\n");
                    }
                }

                if (!session.policyAttributions.empty()) {
                    s_logger.info("\nPolicy attributions:\n");
                    for (const auto& pa : session.policyAttributions) {
                        s_logger.info("  🛡️ ");
                    }
                }
            }
            else if (input.substr(0, 18) == "/explain snapshot ") {
                std::string filePath = input.substr(18);
                if (explainEngine.exportSnapshot(filePath)) {
                    s_logger.info("[Explain ✅] Snapshot exported to ");
                } else {
                    s_logger.info("[Explain ❌] Failed to export snapshot.\n");
                }
            }
            else if (input.substr(0, 9) == "/explain ") {
                std::string agentId = input.substr(9);
                auto trace = explainEngine.traceAuto(agentId);
                s_logger.info("[Explain] ");
                for (const auto& node : trace.nodes) {
                    std::string icon = node.success ? "✅" : "❌";
                    s_logger.info("  ");
                    if (!node.policyId.empty())
                        s_logger.info(" 🛡️ ");
                    if (node.durationMs > 0)
                        s_logger.info(" (");
                    s_logger.info("\n");
                }
                if (trace.failureCount > 0) {
                    s_logger.info("\n");
                }
            }
            // ── Phase 8B: Backend Switcher Commands ──
            else if (input == "/backend list") {
                auto backends = backendMgr.listBackends();
                std::string activeId = backendMgr.getActiveId();
                s_logger.info("[Backends] ");
                for (const auto& b : backends) {
                    std::string icon = (b.id == activeId) ? "🟢" : "⚪";
                    if (!b.enabled) icon = "⛔";
                    s_logger.info("  ");
                    if (!b.endpoint.empty()) s_logger.info(" → ");
                    if (!b.model.empty()) s_logger.info(" model=");
                    s_logger.info("\n");
                }
            }
            else if (input.substr(0, 13) == "/backend use ") {
                std::string id = input.substr(13);
                if (backendMgr.setActiveBackend(id)) {
                    s_logger.info("[Backend ✅] Active backend → ");
                } else {
                    s_logger.info("[Backend ❌] Backend '");
                }
            }
            else if (input == "/backend status") {
                s_logger.info("[Backend Status] ");
                auto active = backendMgr.getActiveBackend();
                s_logger.info("  Active: ");
                if (!active.endpoint.empty())
                    s_logger.info("  Endpoint: ");
                if (!active.model.empty())
                    s_logger.info("  Model: ");
                s_logger.info("  MaxTokens: ");
            }
            // ── Phase 25: AMD GPU Acceleration Commands ──
            else if (input == "/gpu") {
                s_logger.info("[GPU] ");
            }
            else if (input == "/gpu on") {
                auto r = gpuAccel.enableGPU();
                s_logger.info("[GPU] ");
            }
            else if (input == "/gpu off") {
                auto r = gpuAccel.disableGPU();
                s_logger.info("[GPU] ");
            }
            else if (input == "/gpu toggle") {
                auto r = gpuAccel.toggleGPU();
                s_logger.info("[GPU] ");
            }
            else if (input == "/gpu features") {
                s_logger.info("[GPU Features] ");
            }
            else if (input == "/gpu memory") {
                s_logger.info("[GPU Memory] ");
            }
            // ── Phase 23: GPU Kernel Auto-Tuner Commands ──
            else if (input == "/tune") {
                s_logger.info("[Tuner] Running kernel auto-tuner...\n");
                auto r = kernelTuner.tuneAllKernels();
                s_logger.info("[Tuner] ");
                s_logger.info("[Tuner] ");
            }
            else if (input == "/tune cache") {
                s_logger.info("[Tuner Cache] ");
            }
            // ── Phase 21: Swarm Decision Bridge Commands ──
            else if (input == "/swarm bridge") {
                s_logger.info("[Swarm Bridge] ");
            }
            // ── Phase 21: Universal Model Hotpatcher Commands ──
            else if (input == "/hotpatch status") {
                s_logger.info("[Model Hotpatcher] ");
            }
            // ── Phase 20: WebRTC Commands ──
            else if (input == "/webrtc") {
                s_logger.info("[WebRTC] ");
                s_logger.info("[WebRTC Peers] ");
            }
            // ── Phase 24: Sandbox Commands ──
            else if (input == "/sandbox list") {
                auto sandboxes = sandboxMgr.listSandboxes();
                if (sandboxes.empty()) {
                    s_logger.info("[Sandbox] No active sandboxes.\n");
                } else {
                    for (const auto& id : sandboxes) {
                        s_logger.info("  ");
                    }
                }
            }
            else if (input == "/sandbox create") {
                SandboxConfig cfg;
                cfg.type = SandboxType::JobObject;
                std::string id;
                auto r = sandboxMgr.createSandbox(cfg, id);
                s_logger.info("[Sandbox] ");
            }
            // ── Phase 22: Production Release Commands ──
            else if (input == "/release") {
                s_logger.info("[Release] ");
            }
            // ── Phase 20: Model Hotpatcher ! Commands ──
            else if (input.substr(0, 12) == "!model_load ") {
                cmd_model("load " + input.substr(12));
            }
            else if (input == "!model_plan") {
                cmd_model("plan");
            }
            else if (input == "!model_surgery") {
                cmd_model("surgery");
            }
            else if (input == "!pressure_auto") {
                cmd_model("pressure");
            }
            else if (input == "!model_status") {
                cmd_model("status");
            }
            else if (input == "!model_layers") {
                cmd_model("layers");
            }
            else if (input.substr(0, 7) == "!model " && input.size() > 7) {
                cmd_model(input.substr(7));
            }
            // ── Phase 21: Swarm Orchestrator ! Commands ──
            else if (input == "!swarm_join" || input.substr(0, 12) == "!swarm_join ") {
                std::string arg = input.size() > 12 ? input.substr(12) : "";
                cmd_swarm_orchestrator("join " + arg);
            }
            else if (input == "!swarm_status") {
                cmd_swarm_orchestrator("status");
            }
            else if (input.substr(0, 17) == "!swarm_distribute") {
                std::string arg = input.size() > 18 ? input.substr(18) : "";
                cmd_swarm_orchestrator("distribute " + arg);
            }
            else if (input == "!swarm_rebalance") {
                cmd_swarm_orchestrator("rebalance");
            }
            else if (input == "!swarm_nodes") {
                cmd_swarm_orchestrator("nodes");
            }
            else if (input == "!swarm_shards") {
                cmd_swarm_orchestrator("shards");
            }
            else if (input == "!swarm_leave") {
                cmd_swarm_orchestrator("leave");
            }
            else if (input == "!swarm_stats") {
                cmd_swarm_orchestrator("stats");
            }
            // ── Phase 33: Voice Chat Commands ──
            else if (input == "/voice record") {
                static VoiceChat cliVoiceChat;
                static bool cliVoiceInit = false;
                if (!cliVoiceInit) {
                    VoiceChatConfig vcfg;
                    vcfg.sampleRate = 16000;
                    vcfg.channels = 1;
                    vcfg.bitsPerSample = 16;
                    vcfg.enableVAD = true;
                    cliVoiceChat.configure(vcfg);
                    cliVoiceInit = true;
                }
                if (cliVoiceChat.isRecording()) {
                    auto r = cliVoiceChat.stopRecording();
                    s_logger.info("[Voice] Recording stopped: ");
                } else {
                    auto r = cliVoiceChat.startRecording();
                    s_logger.info("[Voice] ");
                    s_logger.info("[Voice] Type '/voice record' again to stop.\n");
                }
            }
            else if (input == "/voice play") {
                static VoiceChat cliVoiceChat;
                auto& rec = cliVoiceChat.getLastRecording();
                if (rec.empty()) {
                    s_logger.info("[Voice] No recording to play. Use '/voice record' first.\n");
                } else {
                    s_logger.info("[Voice] Playing ");
                    auto r = cliVoiceChat.playAudio(rec);
                    s_logger.info("[Voice] ");
                }
            }
            else if (input == "/voice transcribe") {
                static VoiceChat cliVoiceChat;
                std::string transcript;
                auto r = cliVoiceChat.transcribeLastRecording(transcript);
                if (r.success) {
                    s_logger.info("[Voice STT] ");
                } else {
                    s_logger.info("[Voice] Transcription failed: ");
                }
            }
            else if (input.substr(0, 13) == "/voice speak ") {
                static VoiceChat cliVoiceChat;
                std::string text = input.substr(13);
                auto r = cliVoiceChat.speak(text);
                s_logger.info("[Voice TTS] ");
            }
            else if (input == "/voice devices") {
                auto inputs = VoiceChat::enumerateInputDevices();
                auto outputs = VoiceChat::enumerateOutputDevices();
                s_logger.info("=== Input Devices ===\n");
                for (auto& d : inputs) {
                    s_logger.info("  [");
                    if (d.isDefault) s_logger.info(" (default)");
                    s_logger.info("\n");
                }
                s_logger.info("=== Output Devices ===\n");
                for (auto& d : outputs) {
                    s_logger.info("  [");
                    if (d.isDefault) s_logger.info(" (default)");
                    s_logger.info("\n");
                }
            }
            else if (input.substr(0, 12) == "/voice mode ") {
                static VoiceChat cliVoiceChat;
                std::string mode = input.substr(12);
                VoiceChatMode m = VoiceChatMode::PushToTalk;
                if (mode == "vad" || mode == "continuous") m = VoiceChatMode::Continuous;
                else if (mode == "off" || mode == "disabled") m = VoiceChatMode::Disabled;
                cliVoiceChat.setMode(m);
                s_logger.info("[Voice] Mode set to: ");
            }
            else if (input.substr(0, 12) == "/voice room ") {
                static VoiceChat cliVoiceChat;
                std::string room = input.substr(12);
                if (cliVoiceChat.isInRoom()) {
                    cliVoiceChat.leaveRoom();
                    s_logger.info("[Voice] Left room\n");
                }
                auto r = cliVoiceChat.joinRoom(room);
                s_logger.info("[Voice] ");
            }
            else if (input == "/voice status") {
                static VoiceChat cliVoiceChat;
                s_logger.info("[Voice] Recording: ");
            }
            else if (input == "/voice metrics") {
                static VoiceChat cliVoiceChat;
                auto m = cliVoiceChat.getMetrics();
                s_logger.info("[Voice Metrics]\n");
            }
            // ── Phase 26: ReverseEngineered Kernel Commands ──
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
            else if (input == "/scheduler status") {
                auto& state = RawrXD::ReverseEngineered::GetState();
                bool initialized = state.schedulerInit.load();
                s_logger.info("[Scheduler] Work-stealing scheduler: ");
                if (initialized) {
                    // Live probe: submit a real task and verify worker execution
                    uint64_t probeUs = 0;
                    bool probeOk = RawrXD::ReverseEngineered::ProbeScheduler(&probeUs);
                    s_logger.info("  Probe task: ");
                    s_logger.info("  Workers: ");
                }
            }
            else if (input == "/scheduler submit") {
                // Submit a benchmark task that writes a sentinel to verify execution
                auto& state = RawrXD::ReverseEngineered::GetState();
                volatile uint64_t sentinel = 0;
                auto testFn = [](void* arg) -> void {
                    if (arg) *static_cast<volatile uint64_t*>(arg) = 0xDEADC0DEULL;
                };
                uint64_t t0 = GetHighResTick();
                int64_t taskId = Scheduler_SubmitTask(
                    reinterpret_cast<void*>(+testFn),
                    const_cast<uint64_t*>(&sentinel), 0, 0, nullptr);
                state.tasksSubmitted.fetch_add(1);
                if (taskId >= 0) {
                    s_logger.info("[Scheduler] Task submitted: id=");
                    void* result = Scheduler_WaitForTask(taskId, 5000);
                    uint64_t elapsedUs = TicksToMicroseconds(GetHighResTick() - t0);
                    bool workerRan = (sentinel == 0xDEADC0DEULL);
                    if (workerRan) state.tasksCompleted.fetch_add(1);
                    s_logger.info("[Scheduler] Worker executed: ");
                } else {
                    s_logger.info("[Scheduler] Task submit failed: ");
                }
            }
            else if (input == "/conflict status") {
                auto& state = RawrXD::ReverseEngineered::GetState();
                bool initialized = state.conflictDetectorInit.load();
                s_logger.info("[ConflictDetector] Deadlock detection: ");
                if (initialized) {
                    // Live probe: register + lock + unlock a test resource
                    uint64_t probeUs = 0;
                    int probeRc = RawrXD::ReverseEngineered::ProbeConflictDetector(&probeUs);
                    const char* probeMsg = "error";
                    if (probeRc == 0) probeMsg = "OK";
                    else if (probeRc == 1) probeMsg = "DEADLOCK_DETECTED";
                    else if (probeRc == -2) probeMsg = "TABLE_FULL";
                    s_logger.info("  Probe: ");
                    s_logger.info("  Max resources: ");
                }
            }
            else if (input == "/heartbeat status") {
                auto& state = RawrXD::ReverseEngineered::GetState();
                bool initialized = state.heartbeatInit.load();
                s_logger.info("[Heartbeat] UDP gossip monitor: ");
                if (initialized) {
                    s_logger.info("  Listen port: ");
                } else {
                    s_logger.info("  Port not configured (heartbeatPort=0 at init)\n");
                }
            }
            else if (input.substr(0, 15) == "/heartbeat add ") {
                std::string args = input.substr(15);
                size_t sp = args.find(' ');
                if (sp != std::string::npos) {
                    std::string ip = args.substr(0, sp);
                    uint16_t hp = static_cast<uint16_t>(std::stoul(args.substr(sp + 1)));
                    static uint32_t nextNodeId = 1;
                    int rc = Heartbeat_AddNode(nextNodeId++, ip.c_str(), hp);
                    if (rc == 0) {
                        RawrXD::ReverseEngineered::GetState().heartbeatNodesAdded.fetch_add(1);
                    }
                    s_logger.info("[Heartbeat] Add node ");
                } else {
                    s_logger.info("[Heartbeat] Usage: /heartbeat add <ip> <port>\n");
                }
            }
            else if (input == "/gpu dma status") {
                auto& state = RawrXD::ReverseEngineered::GetState();
                s_logger.info("[GPU DMA] Running live probe...\n");
                uint64_t probeUs = 0;
                bool allocOk = false, transferOk = false, verifyOk = false;
                RawrXD::ReverseEngineered::ProbeDMA(&probeUs, &allocOk, &transferOk, &verifyOk);
                s_logger.info("  Alloc: ");
                s_logger.info("  Total DMA transfers: ");
            }
            else if (input == "/tensor bench") {
                // Quick benchmark: 64x64 INT8 matmul
                const uint32_t M = 64, N = 64, K = 64;
                void* A = AllocateDMABuffer(M * K);
                void* B = AllocateDMABuffer(K * N);
                float* C = (float*)AllocateDMABuffer(M * N * sizeof(float));
                if (A && B && C) {
                    memset(A, 1, M * K);
                    memset(B, 1, K * N);
                    uint64_t t0 = GetHighResTick();
                    Tensor_QuantizedMatMul(A, B, C, M, N, K, QUANT_Q8_0);
                    uint64_t t1 = GetHighResTick();
                    uint64_t elapsed = TicksToMicroseconds(t1 - t0);
                    RawrXD::ReverseEngineered::GetState().tensorOps.fetch_add(1);
                    double gflops = 0.0;
                    if (elapsed > 0) {
                        gflops = (2.0 * M * N * K) / (static_cast<double>(elapsed) * 1000.0);
                    }
                    s_logger.info("[Tensor] 64x64 Q8_0 matmul: ");
                    VirtualFree(A, 0, MEM_RELEASE);
                    VirtualFree(B, 0, MEM_RELEASE);
                    VirtualFree(C, 0, MEM_RELEASE);
                } else {
                    s_logger.info("[Tensor] DMA buffer allocation failed\n");
                }
            }
            else if (input == "/timer") {
                uint64_t t0 = GetHighResTick();
                Sleep(100);
                uint64_t t1 = GetHighResTick();
                uint64_t ms = TicksToMilliseconds(t1 - t0);
                uint64_t us = TicksToMicroseconds(t1 - t0);
                s_logger.info("[Timer] 100ms sleep measured: ");
                // CRC test
                const char* testData = "RawrXD-ReverseEngineered-Kernel";
                uint32_t crc = CalculateCRC32(testData, strlen(testData));
                s_logger.info("[CRC32] \");
            }
            else if (input.rfind("/crc32 ", 0) == 0) {
                std::string payload = input.substr(7);
                if (payload.empty()) {
                    s_logger.info("[CRC32] Usage: /crc32 <text>\n");
                } else {
                    auto& st = RawrXD::ReverseEngineered::GetState();
                    uint64_t t0 = GetHighResTick();
                    uint32_t crc = CalculateCRC32(payload.c_str(),
                                                  static_cast<uint32_t>(payload.size()));
                    uint64_t t1 = GetHighResTick();
                    uint64_t us = TicksToMicroseconds(t1 - t0);
                    s_logger.info("[CRC32] Input : ");
                }
            }
#endif
            else {
                s_logger.info("Unknown command. Type /help for commands.\n");
            }
        }
    } else {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 26: ReverseEngineered Kernel Shutdown
    // ═══════════════════════════════════════════════════════════════════
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
    {
        s_logger.info("[SYSTEM] Shutting down ReverseEngineered kernel...\n");
        RawrXD::ReverseEngineered::ShutdownAllSubsystems();
        s_logger.info("[SYSTEM] ReverseEngineered kernel shutdown complete\n");
    }
#endif

    server.Stop();
    return 0;
}
