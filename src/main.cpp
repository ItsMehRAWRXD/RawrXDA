#include <chrono>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "agent_explainability.h"
#include "agent_history.h"
#include "agent_policy.h"
#include "agentic_engine.h"
#include "ai_backend.h"
#include "complete_server.h"
#include "cpu_inference_engine.h"
#include "dml_inference_engine.h"
#include "memory_core.h"
#include "subagent_core.h"

// Phase 20-25: New subsystem headers
#include "amd_gpu_accelerator.h"
#include "gpu_kernel_autotuner.h"
#include "production_release.h"
#include "sandbox_integration.h"
#include "swarm_decision_bridge.h"
#include "universal_model_hotpatcher.h"
#include "webrtc_signaling.h"

// Phase 21: Distributed Swarm Inference Orchestrator
#include "swarm_orchestrator.h"

// Phase 20: CLI headless systems (for ! commands dispatch)
#include "cli_headless_systems.h"

// Phase 19: CLI Autonomy Loop — headless autonomous agentic loop (same command set as Win32 IDE)
#include "cli/cli_autonomy_loop.h"
#include "cli/deep_iteration_engine.h"

// Phase 26: ReverseEngineered MASM Kernel — Scheduler, Heartbeat, Deadlock, GPU DMA, Tensor
#include "../include/reverse_engineered_bridge.h"

// Phase 51: Security — Dork Scanner + Universal Dorker
#include "security/RawrXD_GoogleDork_Scanner.h"
#include "security/RawrXD_Universal_Dorker.h"

// Phase 33: Voice Chat Engine
#include "core/shared_feature_dispatch.h"
#include "core/voice_chat.hpp"


// Enterprise License & Feature Manager
#include "core/enterprise_license.h"
#include "enterprise/multi_gpu.h"
#include "enterprise/support_tier.h"
#include "enterprise_feature_manager.hpp"

// Agentic Autonomous: Operation mode + Model selection + parallel cap
#include "agentic_autonomous_config.h"

namespace
{
std::atomic<bool> g_shutdownRequested{false};

bool tryParseIntInRange(const std::string& text, int minValue, int maxValue, int& outValue)
{
    try
    {
        size_t idx = 0;
        int parsed = std::stoi(text, &idx);
        if (idx != text.size())
            return false;
        if (parsed < minValue || parsed > maxValue)
            return false;
        outValue = parsed;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void parseHostPort(const std::string& input, std::string& hostOut, int& portOut)
{
    if (input.empty())
        return;
    std::string endpoint = input;
    size_t schemePos = endpoint.find("://");
    if (schemePos != std::string::npos)
        endpoint = endpoint.substr(schemePos + 3);

    size_t slashPos = endpoint.find('/');
    if (slashPos != std::string::npos)
        endpoint = endpoint.substr(0, slashPos);

    size_t colonPos = endpoint.rfind(':');
    if (colonPos != std::string::npos && colonPos + 1 < endpoint.size())
    {
        int parsedPort = portOut;
        if (tryParseIntInRange(endpoint.substr(colonPos + 1), 1, 65535, parsedPort))
        {
            hostOut = endpoint.substr(0, colonPos);
            portOut = parsedPort;
            return;
        }
    }
    hostOut = endpoint;
}

bool ensureDirectoryExists(const std::string& path, const char* label)
{
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec)
    {
        std::cerr << "[SYSTEM] Failed to create " << label << " directory: " << path << " (" << ec.message() << ")\n";
        return false;
    }
    if (!std::filesystem::exists(path, ec) || ec)
    {
        std::cerr << "[SYSTEM] " << label << " directory is not accessible: " << path << "\n";
        return false;
    }
    return true;
}
}  // namespace

void SignalHandler(int signal)
{
    (void)signal;
    g_shutdownRequested.store(true);
    std::cout << "\n[ENGINE] Exiting...\n";
}

static void cliRegistryOutput(const char* text, void* userData)
{
    (void)userData;
    if (text)
        std::cout << text;
}

static bool dispatchProfileBangCommand(const std::string& input)
{
    if (input.empty() || input[0] != '!')
        return false;

    size_t split = input.find_first_of(" \t");
    std::string cmd = (split == std::string::npos) ? input : input.substr(0, split);
    std::string args = (split == std::string::npos) ? "" : input.substr(split + 1);

    // Accept both canonical profile commands and tool-prefixed aliases.
    if (cmd == "!tools_profile_start")
        cmd = "!profile_start";
    else if (cmd == "!tools_profile_stop")
        cmd = "!profile_stop";
    else if (cmd == "!tools_profile_results")
        cmd = "!profile_results";

    if (!(cmd == "!profile_start" || cmd == "!profile_stop" || cmd == "!profile_results"))
        return false;

    CommandContext ctx{};
    ctx.rawInput = input.c_str();
    ctx.args = args.c_str();
    ctx.idePtr = nullptr;
    ctx.cliStatePtr = nullptr;
    ctx.commandId = 0;
    ctx.isGui = false;
    ctx.isHeadless = true;
    ctx.hwnd = nullptr;
    ctx.emitEvent = nullptr;
    ctx.outputFn = &cliRegistryOutput;
    ctx.outputUserData = nullptr;

    CommandResult result = SharedFeatureRegistry::instance().dispatchByCli(cmd.c_str(), ctx);
    if (!result.success && result.detail)
    {
        std::cout << "[Profile] " << result.detail << "\n";
    }
    return true;
}

// Build guard: compile this entry point only for the CLI target.
// Default Win32 IDE uses win32app/Win32IDE_Main.cpp.
#ifdef RAWRXD_STANDALONE_MAIN
// Match cli_shell.cpp: feed model (output, prompt) to CLIAutonomyLoop when /autonomy start is active.
void rawrxdStandaloneMaybeEnqueueAutonomy(const std::string& userLine, const std::string& modelOutput)
{
    if (modelOutput.empty())
        return;
    auto& loop = CLIAutonomyLoop::instance();
    if (loop.getState() == AutonomyLoopState::Running)
        loop.enqueueOutput(modelOutput, userLine);
}
}  // namespace

int MainNoCRuntime(int argc, char** argv)
{
    std::signal(SIGINT, SignalHandler);
#ifdef RAWRXD_PURE_CLI
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║           RawrXD CLI — Same command set as Win32 IDE         ║
║  Full chat + agentic autonomous • Same engine as Win32 GUI   ║
║  Default port 23959 — Win32 IDE connects to this server      ║
║  /chat • /agent • /wish • /subagent • /chain • /swarm        ║
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;
#else
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║              RawrXD Engine v7.5 — Agentic Core               ║
║  Subagents • Chaining • HexMag Swarm • History & Replay     ║
║  Phase 25: AMD GPU • Sandbox • 800B Streaming Quant          ║
║  Phase 21: Distributed Swarm Inference • AVX-512 Requant     ║
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;
#endif

    // Command registration is automatic via static AutoRegistrar in
    // unified_command_dispatch.cpp — reads COMMAND_TABLE at startup.

    std::string model_path;
#if defined(RAWRXD_PURE_CLI)
    uint16_t port = 23959;  // Win32 IDE expects this port for /api/chat and /api/tool
#else
    uint16_t port = 8080;
#endif
    bool enable_http = true;
    bool enable_repl = true;
    std::string history_dir = "./history";
    std::string policy_dir = "./policies";
    std::string engine_type = "cpu";  // "cpu" or "dml"
    bool list_models_only = false;
    std::string work_dir;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc)
        {
            model_path = argv[++i];
        }
        else if (arg == "--list" || arg == "-l")
        {
            list_models_only = true;
        }
        else if (arg == "--dir" && i + 1 < argc)
        {
            work_dir = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            int parsedPort = 0;
            if (!tryParseIntInRange(argv[++i], 1, 65535, parsedPort))
            {
                std::cerr << "[ERROR] Invalid --port value. Expected 1-65535.\n";
                return 2;
            }
            port = static_cast<uint16_t>(parsedPort);
        }
        else if (arg == "--engine" && i + 1 < argc)
        {
            engine_type = argv[++i];
        }
        else if (arg == "--no-http")
        {
            enable_http = false;
        }
        else if (arg == "--no-repl")
        {
            enable_repl = false;
        }
        else if (arg == "--history-dir" && i + 1 < argc)
        {
            history_dir = argv[++i];
        }
        else if (arg == "--policy-dir" && i + 1 < argc)
        {
            policy_dir = argv[++i];
        }
        else if (arg == "--help")
        {
            std::cout << R"HELP(
Usage: RawrEngine [options]  (or RawrXD_CLI for pure CLI build)
  --model <path>    Path to GGUF model file
  --port <port>     HTTP port (default 23959 for RawrXD_CLI, 8080 for RawrEngine)
  --engine <type>   Inference engine: cpu (default) or dml (DirectML GPU)
  --no-http         Disable HTTP server
  --no-repl         Disable interactive REPL
  --history-dir <d> Directory for history JSONL (default: ./history)
  --policy-dir <d>  Directory for policy storage (default: ./policies)
  --help            Show this help

  Pure CLI (RawrXD_CLI): port 23959 by default - Win32 IDE connects for chat + Agent > Run Tool.

HTTP API Endpoints:
  GET  /status                Server & model status
  POST /complete              Single completion
  POST /complete/stream       SSE streaming completion
  POST /api/chat              Agentic chat (Win32 IDE AI Chat panel)
  POST /api/tool              Run tool (Win32 IDE Agent > Run Tool)
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
  GET  /api/agentic/config    Agentic operation + model selection config
  POST /api/agentic/config   Set operationMode, modelSelectionMode, perModelInstances, maxModelsInParallel, cycleAgentCounter
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
  GET  /api/security/dork/status   Dork scanner + Universal Dorker status (Phase 51)
  POST /api/security/dork/scan    Run Google Dork scan (body: dork or file) (Phase 51)
  POST /api/security/dork/universal  Universal PHP dorks + hotpatch markers (Phase 51)
  GET  /api/security/dashboard    Security dashboard summary (Phase 51)

REPL Commands (chat + agentic — same as Win32 IDE):
  /chat <message>           Chat (GGUF or Ollama)
  /agent <prompt> [N]       Agentic loop (chat + tools, max N cycles)
  /wish <natural lang>      Execute user wish
  /subagent <prompt>        Spawn a sub-agent
  /chain <step1> | <step2>  Run a prompt chain
  /swarm <p1> | <p2> ...    Run a HexMag swarm
  /agents                   List active agents
  /tools                    List available agent tools (same as GUI)
  /run-tool <name> [json]   Execute tool by name (e.g. /run-tool list_dir {})
  /smoke                    Run agentic smoke test (same as GUI)
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

  Enterprise License:
  /license                 License dashboard (edition, features, limits)
  /license audit           Full enterprise feature audit report
  /license unlock          Dev unlock (needs RAWRXD_ENTERPRISE_DEV=1)
  /license hwid            Show hardware ID for this machine
  /license install <file>  Install a .rawrlic license file
  /license features        List all enterprise features with status
  exit                      Quit
)HELP" << std::endl;
            return 0;
        }
        else
        {
            std::cerr << "[ERROR] Unknown argument: " << arg << "\n";
            std::cerr << "Use --help to see available options.\n";
            return 2;
        }
    }

    if (!(engine_type == "cpu" || engine_type == "dml" || engine_type == "directml" || engine_type == "gpu"))
    {
        std::cerr << "[ERROR] Invalid --engine value: " << engine_type << ". Use cpu|dml|directml|gpu.\n";
        return 2;
    }

    // --list: list Ollama models and exit (production CLI)
    if (list_models_only)
    {
        std::string host = "localhost";
        int ollama_port = 11434;
        const char* e = std::getenv("OLLAMA_HOST");
        if (e && e[0])
        {
            parseHostPort(e, host, ollama_port);
        }
        std::vector<std::string> names;
        if (OllamaListModelsSync(host, ollama_port, names))
        {
            std::cout << "Ollama models at " << host << ":" << ollama_port << ":\n";
            if (names.empty())
                std::cout << "  (none — run 'ollama pull <model>' to add models)\n";
            else
                for (const auto& n : names)
                    std::cout << "  " << n << "\n";
        }
        else
        {
            std::cout << "Could not reach Ollama at " << host << ":" << ollama_port
                      << ". Start Ollama or set OLLAMA_HOST.\n";
        }
        return 0;
    }

    // --dir: set working directory for tool execution (production CLI)
    if (!work_dir.empty())
    {
#ifdef _WIN32
        if (_chdir(work_dir.c_str()) != 0)
        {
            std::cerr << "[CLI] Failed to change directory to: " << work_dir << "\n";
        }
        else
        {
            std::cout << "[CLI] Working directory: " << work_dir << "\n";
        }
#else
        if (chdir(work_dir.c_str()) != 0)
        {
            std::cerr << "[CLI] Failed to change directory to: " << work_dir << "\n";
        }
        else
        {
            std::cout << "[CLI] Working directory: " << work_dir << "\n";
        }
#endif
    }

    if (!ensureDirectoryExists(history_dir, "history") || !ensureDirectoryExists(policy_dir, "policy"))
    {
        return 2;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Enterprise License System — initialize FIRST (gates engine registration)
    // ═══════════════════════════════════════════════════════════════════
    {
        auto& license = RawrXD::EnterpriseLicense::Instance();
        license.Initialize();

        auto& featureMgr = EnterpriseFeatureManager::Instance();
        featureMgr.Initialize();

        std::cout << "[SYSTEM] License: " << license.GetEditionName() << " | Max Model: " << license.GetMaxModelSizeGB()
                  << "GB"
                  << " | Max Context: " << license.GetMaxContextLength() << " tokens"
                  << " | 800B Dual-Engine: "
                  << (license.Is800BUnlocked() ? "UNLOCKED" : "locked (requires Enterprise license)") << "\n";
    }

    // Create inference engine based on --engine flag
    RawrXD::CPUInferenceEngine cpuEngine;
    RawrXD::DMLInferenceEngine dmlEngine;
    RawrXD::InferenceEngine* engine = nullptr;

    if (engine_type == "dml" || engine_type == "directml" || engine_type == "gpu")
    {
        std::cout << "[SYSTEM] Using DirectML GPU inference engine (AMD RX 7800 XT)\n";
        engine = &dmlEngine;
    }
    else
    {
        std::cout << "[SYSTEM] Using CPU inference engine\n";
        engine = &cpuEngine;
    }

    // Initialize AI backend manager early for auto-downgrade support
    AIBackendManager backendMgr;
    backendMgr.setLogCallback(
        [](int level, const std::string& msg)
        {
            const char* prefix[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
            if (level >= 0 && level <= 3)
            {
                std::cout << prefix[level] << " " << msg << "\n";
            }
        });

    // Pre-register Ollama as a common second backend for auto-fallback
    {
        AIBackendConfig ollama;
        ollama.id = "ollama";
        ollama.displayName = "Ollama";
        ollama.type = AIBackendType::Ollama;
        ollama.endpoint = "http://localhost:11434";
        ollama.model = "llama3";
        ollama.enabled = true;
        backendMgr.addBackend(ollama);
    }

    if (!model_path.empty())
    {
        std::cout << "[SYSTEM] Loading model: " << model_path << "\n";

        // Track the model path for auto-downgrade functionality
        backendMgr.setLoadedModelPath(model_path);

        if (!engine->LoadModel(model_path))
        {
            std::cout << "[SYSTEM] Initial model load failed. Attempting auto-downgrade...\n";

            // Attempt auto-downgrade to smaller quantized siblings
            std::string downgradedPath;
            std::string downgradeFailure;
            bool downgraded = backendMgr.tryAutoDowngradeLocalModel(
                [engine](const std::string& candidatePath) -> bool
                {
                    std::cout << "[SYSTEM] Trying candidate: " << candidatePath << "\n";
                    return engine->LoadModel(candidatePath);
                },
                &downgradedPath, &downgradeFailure);

            if (downgraded)
            {
                std::cout << "[SYSTEM] Auto-downgrade succeeded: " << downgradedPath << "\n";
                std::cout << "[SYSTEM] Model loaded via " << engine->GetEngineName() << " engine (downgraded).\n";
            }
            else
            {
                std::cout << "[SYSTEM] Model load failed and auto-downgrade failed: " << downgradeFailure << "\n";
                std::cout << "[SYSTEM] /complete will return empty results or fallback to Ollama if available.\n";

                // Set metadata-only blocking state for potential Ollama fallback
                std::string blockReason = "Model loading failed. " + downgradeFailure;
                backendMgr.setLocalMetadataOnlyBlocked(blockReason);
            }
        }
        else
        {
            std::cout << "[SYSTEM] Model loaded via " << engine->GetEngineName() << " engine.\n";
        }
    }
    else
    {
        std::cout << "[SYSTEM] No model specified. Use --model <path> to load a GGUF.\n";
    }

    // Initialize agentic engine
    AgenticEngine agentEngine;
    agentEngine.setInferenceEngine(engine);

    // Initialize sub-agent manager with console logging
    SubAgentManager subAgentMgr(&agentEngine);
    subAgentMgr.setLogCallback(
        [](int level, const std::string& msg)
        {
            const char* prefix[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
            if (level >= 0 && level <= 3)
            {
                std::cout << prefix[level] << " " << msg << "\n";
            }
        });

    // Initialize event history recorder (Phase 5)
    AgentHistoryRecorder historyRecorder(history_dir);
    historyRecorder.setLogCallback(
        [](int level, const std::string& msg)
        {
            const char* prefix[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
            if (level >= 0 && level <= 3)
            {
                std::cout << prefix[level] << " " << msg << "\n";
            }
        });
    subAgentMgr.setHistoryRecorder(&historyRecorder);
    std::cout << "[SYSTEM] Event history: session=" << historyRecorder.sessionId() << " dir=" << history_dir << "\n";

    // Initialize policy engine (Phase 7)
    PolicyEngine policyEngine(policy_dir);
    policyEngine.setLogCallback(
        [](int level, const std::string& msg)
        {
            const char* prefix[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
            if (level >= 0 && level <= 3)
            {
                std::cout << prefix[level] << " " << msg << "\n";
            }
        });
    policyEngine.setHistoryRecorder(&historyRecorder);
    subAgentMgr.setPolicyEngine(&policyEngine);
    std::cout << "[SYSTEM] Policy engine: " << policyEngine.policyCount() << " policies loaded from " << policy_dir
              << "\n";

    // Initialize explainability engine (Phase 8A — read-only)
    ExplainabilityEngine explainEngine;
    explainEngine.setHistoryRecorder(&historyRecorder);
    explainEngine.setPolicyEngine(&policyEngine);
    explainEngine.setLogCallback(
        [](int level, const std::string& msg)
        {
            const char* prefix[] = {"[DEBUG]", "[INFO]", "[WARN]", "[ERROR]"};
            if (level >= 0 && level <= 3)
            {
                std::cout << prefix[level] << " " << msg << "\n";
            }
        });
    std::cout << "[SYSTEM] Explainability engine: ready (Phase 8A)\n";

    // Initialize headless CLI subsystems (safety, confidence, replay, governor, multi-response)
    if (cli_headless_init(&agentEngine, &subAgentMgr))
    {
        std::cout << "[SYSTEM] Headless CLI subsystems: initialized (Phase 10)\n";
    }
    else
    {
        std::cout << "[SYSTEM] Headless CLI subsystems: init failed (optional)\n";
    }

    // Wire CLIAutonomyLoop for headless autonomous agentic (same as Win32 IDE)
    {
        auto& autonomyLoop = CLIAutonomyLoop::instance();
        autonomyLoop.setAgenticEngine(&agentEngine);
        autonomyLoop.setSubAgentManager(&subAgentMgr);
        auto& deepIter = DeepIterationEngine::instance();
        deepIter.setAgenticEngine(&agentEngine);
        deepIter.setSubAgentManager(&subAgentMgr);
        deepIter.setChatProvider([&agentEngine](const std::string& msg, const std::string&)
                                 { return agentEngine.chat(msg); });
        std::cout << "[SYSTEM] Autonomy Loop: ready (use /autonomy start for autonomous agentic mode)\n";
        std::cout << "[SYSTEM] Deep Iteration: ready (use /deep-iterate <path> [max] for audit→code cycles)\n";
    }

    std::cout << "[SYSTEM] Backend manager: " << backendMgr.backendCount()
              << " backends, active=" << backendMgr.getActiveBackendName() << " (Phase 8B)\n";

    // ═══════════════════════════════════════════════════════════════════
    // Phase 25: AMD/ATI GPU Acceleration (initialize FIRST — others depend on it)
    // ═══════════════════════════════════════════════════════════════════
    auto& gpuAccel = AMDGPUAccelerator::instance();
    {
        auto r = gpuAccel.initialize(GPUBackend::Auto);
        std::cout << "[SYSTEM] AMD GPU Accelerator: " << r.detail << " GPU=" << (gpuAccel.isGPUEnabled() ? "ON" : "OFF")
                  << " backend=" << gpuAccel.getBackendName() << "\n";
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 23: GPU Kernel Auto-Tuner
    // ═══════════════════════════════════════════════════════════════════
    auto& kernelTuner = GPUKernelAutoTuner::instance();
    {
        auto r = kernelTuner.initialize();
        std::cout << "[SYSTEM] GPU Kernel Auto-Tuner: " << r.detail << " cache=" << kernelTuner.getCacheSize()
                  << " entries\n";
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 21: Swarm Decision Bridge (wires AgenticDecisionTree → Swarm)
    // ═══════════════════════════════════════════════════════════════════
    auto& swarmBridge = SwarmDecisionBridge::instance();
    {
        bool ok = swarmBridge.initialize();
        std::cout << "[SYSTEM] Swarm Decision Bridge: " << (ok ? "Initialized" : "Failed") << "\n";
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 21: Universal Model Hotpatcher (120B-800B streaming quant)
    // ═══════════════════════════════════════════════════════════════════
    auto& modelHotpatcher = UniversalModelHotpatcher::instance();
    {
        bool ok = modelHotpatcher.initialize();
        std::cout << "[SYSTEM] Universal Model Hotpatcher: " << (ok ? "Initialized" : "Failed") << "\n";
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 22: Production Release Engine
    // ═══════════════════════════════════════════════════════════════════
    auto& releaseEngine = ProductionReleaseEngine::instance();
    {
        std::cout << "[SYSTEM] Production Release Engine: Loaded"
                  << " version=" << releaseEngine.getCurrentVersion() << "\n";
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 20: WebRTC P2P Signaling
    // ═══════════════════════════════════════════════════════════════════
    auto& webrtc = WebRTCSignaling::instance();
    {
        auto r = webrtc.initialize("wss://signal.rawrxd.local:8443");
        std::cout << "[SYSTEM] WebRTC P2P Signaling: " << r.detail << " peers=" << webrtc.getConnectedPeerCount()
                  << "\n";
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 24: Windows Sandbox Manager
    // ═══════════════════════════════════════════════════════════════════
    auto& sandboxMgr = SandboxManager::instance();
    {
        auto r = sandboxMgr.initialize();
        std::cout << "[SYSTEM] Windows Sandbox Manager: " << r.detail << "\n";
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 21: Distributed Swarm Inference Orchestrator
    // ═══════════════════════════════════════════════════════════════════
    RawrXD::SwarmOrchestrator swarmOrchestrator;
    std::cout << "[SYSTEM] Swarm Orchestrator: ready (use !swarm_join to start)\n";

    // ═══════════════════════════════════════════════════════════════════
    // Phase 26: ReverseEngineered MASM Kernel Subsystems
    // Work-Stealing Scheduler + Conflict Detection + Heartbeat + INFINITY I/O
    // ═══════════════════════════════════════════════════════════════════
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
    {
        auto reResult =
            RawrXD::ReverseEngineered::InitializeAllSubsystems(0,   // workerCount: auto-detect from CPU cores
                                                               0,   // heartbeatPort: 0 = disabled for single-node CLI
                                                               256  // maxResources for conflict detection
            );
        std::cout << "[SYSTEM] ReverseEngineered Kernel: " << reResult.detail
                  << " (scheduler=" << (reResult.success ? "ON" : "OFF") << ", deadlock_detect=ON, GPU_DMA=ready)\n";

        if (reResult.success)
        {
            // Report high-res tick capability
            uint64_t tick = GetHighResTick();
            uint64_t us = TicksToMicroseconds(1);
            std::cout << "[SYSTEM]   Timer resolution: " << (us > 0 ? "sub-microsecond" : "millisecond") << "\n";
        }
    }
#else
    std::cout << "[SYSTEM] ReverseEngineered Kernel: DISABLED (no MASM)\n";
#endif

    // Start HTTP server with agentic support
    RawrXD::CompletionServer server;
    if (enable_http)
    {
        server.SetAgenticEngine(&agentEngine);
        server.SetSubAgentManager(&subAgentMgr);
        server.SetHistoryRecorder(&historyRecorder);
        server.SetPolicyEngine(&policyEngine);
        server.SetExplainabilityEngine(&explainEngine);
        server.SetBackendManager(&backendMgr);
        server.Start(port, engine, model_path);
    }

    if (enable_repl)
    {
        std::cout << "[SYSTEM] Interactive REPL ready. Type 'exit' to quit, /help for commands.\n\n";
        std::string input;
        while (true)
        {
            std::cout << "RawrXD> ";
            std::getline(std::cin, input);
            if (input == "exit" || input == "/exit")
                break;
            if (input.empty())
                continue;

            if (input == "/help")
            {
                std::cout
                    << "Commands (same as Win32 IDE — chat + agentic autonomous):\n"
                    << "  /chat <message>         Chat (GGUF or Ollama if no model loaded)\n"
                    << "  /chat /model:<name> <m> Chat using Ollama model <name>\n"
                    << "  /wish <natural lang>    Execute user wish (same as API /api/agent/wish)\n"
                    << "  /agent <prompt> [N]     Agentic loop: chat + tools until done (max N cycles, default 10)\n"
                    << "  /subagent <prompt>      Spawn a sub-agent\n"
                    << "  /chain <s1> | <s2> ...  Run a sequential chain\n"
                    << "  /swarm <p1> | <p2> ...  Run a HexMag swarm\n"
                    << "  /agents                 List all sub-agents\n"
                    << "  /tools                  List available agent tools (same as GUI)\n"
                    << "  /run-tool <name> [json]  Execute tool by name (e.g. /run-tool list_dir {})\n"
                    << "  /smoke                  Run agentic smoke test (same as GUI)\n"
                    << "  /status                 System status\n"
                    << "  /todo                   Show todo list\n"
                    << "  /history [agent_id]     Event history (Phase 5)\n"
                    << "  /replay <agent_id>      Replay an agent run (Phase 5)\n"
                    << "  /stats                  History statistics (Phase 5)\n"
                    << "  /policies               List active policies (Phase 7)\n"
                    << "  /suggest                Generate policy suggestions (Phase 7)\n"
                    << "  /policy accept <id>     Accept a suggestion (Phase 7)\n"
                    << "  /policy reject <id>     Reject a suggestion (Phase 7)\n"
                    << "  /policy export <file>   Export policies to file (Phase 7)\n"
                    << "  /policy import <file>   Import policies from file (Phase 7)\n"
                    << "  /heuristics             Compute & show heuristics (Phase 7)\n"
                    << "  /explain <agent_id>     Explain agent decision (Phase 8A)\n"
                    << "  /explain last           Explain most recent agent (Phase 8A)\n"
                    << "  /explain session        Explain full session (Phase 8A)\n"
                    << "  /explain snapshot <f>   Export snapshot to file (Phase 8A)\n"
                    << "  /backend list           List all backends (Phase 8B)\n"
                    << "  /backend use <id>       Switch active backend (Phase 8B)\n"
                    << "  /backend status         Show active backend status (Phase 8B)\n"
                    << "  /gpu                    GPU acceleration status (Phase 25)\n"
                    << "  /gpu on                 Enable GPU acceleration (Phase 25)\n"
                    << "  /gpu off                Disable GPU acceleration (Phase 25)\n"
                    << "  /gpu toggle             Toggle GPU on/off (Phase 25)\n"
                    << "  /gpu features           Show AMD GPU features (Phase 25)\n"
                    << "  /gpu memory             Show GPU memory usage (Phase 25)\n"
                    << "  /tune                   Run GPU kernel auto-tuner (Phase 23)\n"
                    << "  /tune cache             Show tuning cache (Phase 23)\n"
                    << "  /swarm bridge           Swarm decision bridge status (Phase 21)\n"
                    << "  /hotpatch status        Universal model hotpatcher (Phase 21)\n"
                    << "  /webrtc                 WebRTC signaling status (Phase 20)\n"
                    << "  /sandbox list           List active sandboxes (Phase 24)\n"
                    << "  /sandbox create         Create a new sandbox (Phase 24)\n"
                    << "  /release                 Production release status (Phase 22)\n"
                    << "  /security                Security dashboard (Phase 51)\n"
                    << "  /dork status             Dork scanner + Universal Dorker status (Phase 51)\n"
                    << "\n  Autonomy (background agentic loop — same as Win32 IDE):\n"
                    << "  /autonomy start          Start autonomous loop (poll→detect→decide→act→verify)\n"
                    << "  /autonomy stop           Stop autonomous loop\n"
                    << "  /autonomy pause          Pause autonomy loop\n"
                    << "  /autonomy resume         Resume paused autonomy loop\n"
                    << "  /autonomy status         Show autonomy loop state and stats\n"
                    << "\n  Deep Iteration (audit→code cycles, beyond Copilot/Cursor):\n"
                    << "  /deep-iterate <path> [max] [--write]  Run audit→code cycles; --write saves result to file\n"
                    << "  /deep-status                Show deep iteration stats\n"
                    << "  /deep-config [key=value]    Get/set: max_iterations, convergence_window, min_complexity, "
                       "max_tokens\n"
                    << "\n  Agentic Autonomous:\n"
                    << "  /agentic                 Show operation + model config\n"
                    << "  /agentic mode <Agent|Plan|Debug|Ask>  Set operation mode\n"
                    << "  /models                  Show model selection + cap + cycle\n"
                    << "  /models mode <Auto|MAX|multiple>  Set model selection mode\n"
                    << "  /models per-model <1-4>  Instances per model (when multiple)\n"
                    << "  /models cap <1-40>       Max models in parallel (cap 40)\n"
                    << "  /models cycle <1-4>      Cycle agent counter (1x-4x)\n"
                    << "\n  Enterprise License:\n"
                    << "  /license                 License dashboard (edition + features)\n"
                    << "  /license audit           Full enterprise feature audit\n"
                    << "  /license unlock          Dev unlock (RAWRXD_ENTERPRISE_DEV=1)\n"
                    << "  /license hwid            Show hardware ID\n"
                    << "  /license install <file>  Install .rawrlic license file\n"
                    << "  /license features        List all 8 enterprise features\n"
                    << "\n  Phase 20 — Model Surgery:\n"
                    << "  !model_load <path>       Analyze GGUF model (Phase 20)\n"
                    << "  !model_plan              Compute optimal quant plan (Phase 20)\n"
                    << "  !model_surgery           Apply streaming requantization (Phase 20)\n"
                    << "  !pressure_auto           Enable auto VRAM pressure response (Phase 20)\n"
                    << "  !model_status            Show model hotpatcher status (Phase 20)\n"
                    << "  !model_layers            List loaded model layers (Phase 20)\n"
                    << "\n  Phase 21 — Distributed Swarm Inference:\n"
                    << "  !swarm_join [ip]         Join swarm or become coordinator (Phase 21)\n"
                    << "  !swarm_status            Show swarm topology (Phase 21)\n"
                    << "  !swarm_distribute <m> <n> Distribute model layers (Phase 21)\n"
                    << "  !swarm_rebalance         Rebalance by VRAM pressure (Phase 21)\n"
                    << "  !swarm_nodes             List swarm nodes (Phase 21)\n"
                    << "  !swarm_shards            List layer shards (Phase 21)\n"
                    << "  !swarm_leave             Leave the swarm (Phase 21)\n"
                    << "  !swarm_stats             Show swarm network stats (Phase 21)\n"
                    << "\n  Phase 33 — Voice Chat:\n"
                    << "  /voice record            Start/stop audio recording (Phase 33)\n"
                    << "  /voice play              Play last recording (Phase 33)\n"
                    << "  /voice transcribe        Transcribe last recording (Phase 33)\n"
                    << "  /voice speak <text>      Text-to-speech (Phase 33)\n"
                    << "  /voice devices           List audio devices (Phase 33)\n"
                    << "  /voice mode <ptt|vad|off> Set voice mode (Phase 33)\n"
                    << "  /voice room <name>       Join/leave voice room (Phase 33)\n"
                    << "  /voice status            Voice chat status (Phase 33)\n"
                    << "  /voice metrics           Voice chat metrics (Phase 33)\n"
                    << "\n  Phase 26 — ReverseEngineered Kernel:\n"
                    << "  /scheduler status       Work-stealing scheduler status\n"
                    << "  /scheduler submit       Submit a test task\n"
                    << "  /conflict status        Conflict detector / deadlock stats\n"
                    << "  /heartbeat status       Heartbeat monitor node health\n"
                    << "  /heartbeat add <ip> <p> Add heartbeat peer node\n"
                    << "  /gpu dma status         GPU DMA transfer status\n"
                    << "  /tensor bench           Run quantized matmul benchmark\n"
                    << "  /timer                  High-res timer test\n"
                    << "  /crc32 <text>           CRC32 of arbitrary text\n"
                    << "  exit                    Quit\n\n";
            }
            else if (input.substr(0, 5) == "/chat" || (input[0] != '/' && !input.empty()))
            {
                std::string raw = (input.substr(0, 5) == "/chat") ? input.substr(5) : input;
                while (!raw.empty() && (raw[0] == ' ' || raw[0] == '\t'))
                    raw.erase(0, 1);
                std::string modelOverride;
                if (raw.size() >= 7 && raw.substr(0, 7) == "/model:")
                {
                    size_t sp = raw.find(' ', 7);
                    if (sp != std::string::npos)
                    {
                        modelOverride = raw.substr(7, sp - 7);
                        raw = raw.substr(sp);
                        while (!raw.empty() && (raw[0] == ' ' || raw[0] == '\t'))
                            raw.erase(0, 1);
                    }
                    else
                    {
                        modelOverride = raw.substr(7);
                        raw.clear();
                    }
                }
                std::string msg = raw;
                if (msg.empty())
                {
                    std::cout << "[ERROR] Empty message. Use: /chat <message> or /chat /model:llama3.2 <message>\n";
                }
                else
                {
                    auto doChat = [&](const std::string& message) -> std::string
                    {
                        if (agentEngine.isModelLoaded() && modelOverride.empty())
                        {
                            return agentEngine.chat(message);
                        }
                        if (!modelOverride.empty() || backendMgr.getActiveId() == "ollama")
                        {
                            std::string host = "localhost";
                            int port = 11434;
                            const char* envHost = std::getenv("OLLAMA_HOST");
                            if (envHost && envHost[0])
                            {
                                parseHostPort(envHost, host, port);
                            }
                            else if (backendMgr.getActiveId() == "ollama")
                            {
                                auto cfg = backendMgr.getActiveBackend();
                                parseHostPort(cfg.endpoint, host, port);
                            }
                            std::string ollamaModel =
                                modelOverride.empty() ? backendMgr.getActiveBackend().model : modelOverride;
                            if (ollamaModel.empty())
                                ollamaModel = "llama3.2";
                            std::string response;
                            if (OllamaGenerateSync(host, port, ollamaModel, message, response))
                            {
                                return response;
                            }
                            return "[Ollama error: start Ollama or check OLLAMA_HOST / backend]";
                        }
                        if (agentEngine.isModelLoaded())
                        {
                            return agentEngine.chat(message);
                        }
                        return "";
                    };
                    historyRecorder.recordChatRequest(msg);
                    auto chatStart = std::chrono::steady_clock::now();
                    std::string response = doChat(msg);
                    auto chatEnd = std::chrono::steady_clock::now();
                    int chatMs =
                        (int)std::chrono::duration_cast<std::chrono::milliseconds>(chatEnd - chatStart).count();
                    historyRecorder.recordChatResponse(response, chatMs);
                    if (response.empty())
                    {
                        std::cout << "[ERROR] No model loaded. Use --model <gguf> or /chat /model:llama3.2 <msg> or "
                                     "/backend use ollama\n";
                    }
                    else
                    {
                        std::cout << response << "\n";
                        rawrxdStandaloneMaybeEnqueueAutonomy(input, response);
                        auto operationMode = RawrXD::AgenticAutonomousConfig::instance().getOperationMode();
                        bool allowTools = (operationMode != RawrXD::AgenticOperationMode::Ask &&
                                           operationMode != RawrXD::AgenticOperationMode::Plan);
                        if (allowTools)
                        {
                            std::string toolResult;
                            if (subAgentMgr.dispatchToolCall("repl", response, toolResult))
                            {
                                std::cout << "\n[Tool Result]\n" << toolResult << "\n";
                            }
                        }
                    }
                }
            }
            else if (input.substr(0, 6) == "/wish ")
            {
                std::string wish = input.substr(6);
                std::string msg = "Execute the following user wish. Respond with a clear plan or result: " + wish;
                if (agentEngine.isModelLoaded())
                {
                    historyRecorder.recordChatRequest(msg);
                    auto chatStart = std::chrono::steady_clock::now();
                    std::string response = agentEngine.chat(msg);
                    auto chatEnd = std::chrono::steady_clock::now();
                    historyRecorder.recordChatResponse(
                        response,
                        (int)std::chrono::duration_cast<std::chrono::milliseconds>(chatEnd - chatStart).count());
                    std::cout << response << "\n";
                    rawrxdStandaloneMaybeEnqueueAutonomy(input, response);
                    std::string toolResult;
                    if (subAgentMgr.dispatchToolCall("repl", response, toolResult))
                    {
                        std::cout << "\n[Tool Result]\n" << toolResult << "\n";
                    }
                }
                else
                {
                    std::string host = "localhost";
                    int port = 11434;
                    const char* envHost = std::getenv("OLLAMA_HOST");
                    if (envHost && envHost[0])
                    {
                        parseHostPort(envHost, host, port);
                    }
                    else
                    {
                        auto cfg = backendMgr.getActiveBackend();
                        if (cfg.type == AIBackendType::Ollama && !cfg.endpoint.empty())
                        {
                            parseHostPort(cfg.endpoint, host, port);
                        }
                    }
                    std::string model = backendMgr.getActiveBackend().model;
                    if (model.empty())
                        model = "llama3.2";
                    std::string response;
                    if (OllamaGenerateSync(host, port, model, msg, response))
                    {
                        std::cout << response << "\n";
                        rawrxdStandaloneMaybeEnqueueAutonomy(input, response);
                    }
                    else
                    {
                        std::cout << "[ERROR] Ollama unavailable. Start Ollama or use /backend use ollama and pull a "
                                     "model.\n";
                    }
                }
            }
            else if (input.substr(0, 7) == "/agent " || input == "/agent")
            {
                std::string prompt;
                int maxIter = 10;
                if (input.size() > 7)
                {
                    prompt = input.substr(7);
                    size_t lastSpace = prompt.rfind(' ');
                    if (lastSpace != std::string::npos)
                    {
                        try
                        {
                            int n = std::stoi(prompt.substr(lastSpace + 1));
                            if (n >= 1 && n <= 50)
                            {
                                maxIter = n;
                                prompt = prompt.substr(0, lastSpace);
                            }
                        }
                        catch (...)
                        {
                        }
                    }
                }
                if (prompt.empty())
                {
                    std::cout << "Usage: /agent <prompt> [max_iterations]. Runs agentic loop (chat + tool dispatch "
                                 "until done, like Win32 Agent menu).\n";
                }
                else
                {
                    auto operationMode = RawrXD::AgenticAutonomousConfig::instance().getOperationMode();
                    bool allowTools = (operationMode != RawrXD::AgenticOperationMode::Ask &&
                                       operationMode != RawrXD::AgenticOperationMode::Plan);
                    std::cout << "[Agent] Loop for up to " << maxIter << " iterations (tools "
                              << (allowTools ? "ON" : "off") << ")...\n";
                    std::string currentPrompt = prompt;
                    for (int i = 0; i < maxIter; i++)
                    {
                        std::string response;
                        if (agentEngine.isModelLoaded())
                        {
                            response = agentEngine.chat(currentPrompt);
                        }
                        else
                        {
                            std::string host = "localhost";
                            int port = 11434;
                            const char* e = std::getenv("OLLAMA_HOST");
                            if (e && e[0])
                            {
                                parseHostPort(e, host, port);
                            }
                            else
                            {
                                auto cfg = backendMgr.getActiveBackend();
                                if (cfg.type == AIBackendType::Ollama && !cfg.endpoint.empty())
                                {
                                    parseHostPort(cfg.endpoint, host, port);
                                }
                            }
                            std::string model = backendMgr.getActiveBackend().model;
                            if (model.empty())
                                model = "llama3.2";
                            if (!OllamaGenerateSync(host, port, model, currentPrompt, response))
                                response = "[Ollama error]";
                        }
                        std::cout << "\n[Cycle " << (i + 1) << "]\n" << response << "\n";
                        rawrxdStandaloneMaybeEnqueueAutonomy(currentPrompt, response);
                        if (!allowTools)
                            break;
                        std::string toolResult;
                        if (!subAgentMgr.dispatchToolCall("repl", response, toolResult))
                            break;
                        std::cout << "\n[Tool Result]\n" << toolResult << "\n";
                        currentPrompt = "Observation from tool execution:\n" + toolResult +
                                        "\n\nContinue toward the goal: " + prompt;
                    }
                    std::cout << "[Agent] Done.\n";
                }
            }
            else if (input.substr(0, 10) == "/subagent ")
            {
                std::string prompt = input.substr(10);
                std::cout << "[SubAgent] Spawning...\n";
                std::string id = subAgentMgr.spawnSubAgent("repl", "REPL subagent", prompt);
                bool ok = subAgentMgr.waitForSubAgent(id, 120000);
                std::string result = subAgentMgr.getSubAgentResult(id);
                std::cout << "[SubAgent " << (ok ? "✅" : "❌") << "] " << result << "\n";
            }
            else if (input.substr(0, 7) == "/chain ")
            {
                std::string rest = input.substr(7);
                // Split on " | "
                std::vector<std::string> steps;
                size_t pos = 0;
                while (true)
                {
                    size_t delim = rest.find(" | ", pos);
                    if (delim == std::string::npos)
                    {
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
            else if (input.substr(0, 7) == "/swarm ")
            {
                std::string rest = input.substr(7);
                std::vector<std::string> prompts;
                size_t pos = 0;
                while (true)
                {
                    size_t delim = rest.find(" | ", pos);
                    if (delim == std::string::npos)
                    {
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
            else if (input == "/tools")
            {
                std::cout << "Available Agent Tools (same as GUI):\n\n";
                std::cout << "  • shell, powershell     — Run terminal commands\n";
                std::cout << "  • read_file, write_file, list_dir — File operations\n";
                std::cout << "  • runSubagent           — Spawn sub-agent\n";
                std::cout << "  • manage_todo_list      — Todo list management\n";
                std::cout << "  • chain                 — Sequential prompt chain\n";
                std::cout << "  • hexmag_swarm          — Parallel swarm execution\n";
                std::cout << "  • bulk_fix              — Bulk autonomous fix\n\n";
                std::cout
                    << "Model outputs TOOL:<name>:{...} to invoke. Example: TOOL:read_file:{\"path\":\"file.txt\"}\n";
                std::cout << "HTTP API: POST /api/tool with {\"tool\":\"read_file\"|\"run_command\"|...}\n\n";
            }
            else if ((input.size() > 10 && input.substr(0, 10) == "/run-tool ") ||
                     (input.size() > 9 && input.substr(0, 9) == "run-tool "))
            {
                size_t nameStart = (input[0] == '/') ? 10 : 9;
                size_t sp = input.find(' ', nameStart);
                std::string toolName =
                    (sp != std::string::npos) ? input.substr(nameStart, sp - nameStart) : input.substr(nameStart);
                std::string jsonArgs = (sp != std::string::npos && sp + 1 < input.size()) ? input.substr(sp + 1) : "{}";
                if (toolName.empty())
                {
                    std::cout << "Usage: /run-tool <name> [json]. Example: /run-tool list_dir {}\n";
                }
                else
                {
                    std::string fakeOutput = "TOOL:" + toolName + ":" + jsonArgs;
                    std::string toolResult;
                    if (subAgentMgr.dispatchToolCall("repl-run-tool", fakeOutput, toolResult))
                    {
                        std::cout << "[Tool " << toolName << "] " << toolResult << "\n";
                    }
                    else
                    {
                        std::cout << "[Tool " << toolName
                                  << "] Not dispatched. Try: read_file, write_file, list_dir, shell, powershell.\n";
                    }
                }
            }
            else if (input == "/smoke")
            {
                std::string smokePrompt =
                    "SMOKE TEST: Create a file named 'agent_smoke_test.txt' in the current working directory. "
                    "Content should be 'RawrXD Agentic Smoke Test Successful at ' + [Current Timestamp]. "
                    "After creating the file, run 'dir' or 'ls' to verify it exists and output the result. "
                    "Finally, read the file back to confirm content integrity.";
                std::cout << "[Smoke] Running agentic smoke test (same as Win32 Agent > Run Smoke Test)...\n";
                std::cout << "[Smoke] Goal: Verify multi-step tools (filesystem + shell).\n\n";
                if (!agentEngine.isModelLoaded())
                {
                    std::cout << "[Smoke] Hint: No GGUF loaded — using Ollama. Ensure 'ollama serve' is running.\n\n";
                }
                auto operationMode = RawrXD::AgenticAutonomousConfig::instance().getOperationMode();
                bool allowTools = (operationMode != RawrXD::AgenticOperationMode::Ask &&
                                   operationMode != RawrXD::AgenticOperationMode::Plan);
                std::string currentPrompt = smokePrompt;
                for (int i = 0; i < 10; i++)
                {
                    std::string response;
                    if (agentEngine.isModelLoaded())
                    {
                        response = agentEngine.chat(currentPrompt);
                    }
                    else
                    {
                        std::string host = "localhost";
                        int port = 11434;
                        const char* e = std::getenv("OLLAMA_HOST");
                        if (e && e[0])
                        {
                            parseHostPort(e, host, port);
                        }
                        else
                        {
                            auto cfg = backendMgr.getActiveBackend();
                            if (cfg.type == AIBackendType::Ollama && !cfg.endpoint.empty())
                            {
                                parseHostPort(cfg.endpoint, host, port);
                            }
                        }
                        std::string model = backendMgr.getActiveBackend().model;
                        if (model.empty())
                            model = "llama3.2";
                        if (!OllamaGenerateSync(host, port, model, currentPrompt, response))
                            response = "[Ollama error: start ollama serve]";
                    }
                    std::cout << "\n[Cycle " << (i + 1) << "]\n" << response << "\n";
                    rawrxdStandaloneMaybeEnqueueAutonomy(currentPrompt, response);
                    if (!allowTools)
                        break;
                    std::string toolResult;
                    if (!subAgentMgr.dispatchToolCall("smoke", response, toolResult))
                        break;
                    std::cout << "\n[Tool Result]\n" << toolResult << "\n";
                    currentPrompt = "Observation from tool execution:\n" + toolResult +
                                    "\n\nContinue toward the goal: " + smokePrompt;
                }
                std::cout << "[Smoke] Done. Check agent_smoke_test.txt for verification.\n\n";
            }
            else if (input == "/agents")
            {
                auto agents = subAgentMgr.getAllSubAgents();
                if (agents.empty())
                {
                    std::cout << "No sub-agents.\n";
                }
                else
                {
                    for (const auto& a : agents)
                    {
                        std::cout << "  " << a.id << " [" << a.stateString() << "] " << a.description << " ("
                                  << a.elapsedMs() << "ms)\n";
                    }
                }
            }
            else if (input == "/status")
            {
                std::cout << "Model loaded: " << (agentEngine.isModelLoaded() ? "yes" : "no") << "\n";
                std::cout << subAgentMgr.getStatusSummary() << "\n";
            }
            // ── Autonomy Loop (same as Win32 IDE: Start/Stop) ──
            else if (input == "/autonomy start")
            {
                CLIAutonomyLoop::instance().start();
            }
            else if (input == "/autonomy stop")
            {
                CLIAutonomyLoop::instance().stop();
            }
            else if (input == "/autonomy pause")
            {
                CLIAutonomyLoop::instance().pause();
            }
            else if (input == "/autonomy resume")
            {
                CLIAutonomyLoop::instance().resume();
            }
            else if (input == "/autonomy status")
            {
                std::cout << CLIAutonomyLoop::instance().getStatusString() << "\n";
                std::cout << CLIAutonomyLoop::instance().getDetailedStatus() << "\n";
            }
            // Deep Iteration: audit→code→audit cycles (beyond Copilot/Cursor)
            else if (input.substr(0, 14) == "/deep-iterate ")
            {
                std::string args = input.substr(14);
                std::string path;
                int maxIter = 10;
                bool writeBack = false;
                std::istringstream argStream(args);
                std::string tok;
                while (argStream >> tok)
                {
                    if (tok == "--write")
                        writeBack = true;
                    else if (!tok.empty() && tok[0] >= '0' && tok[0] <= '9')
                        maxIter = std::max(1, std::min(50, std::stoi(tok)));
                    else if (path.empty())
                        path = tok;
                }
                if (path.empty())
                {
                    std::cout << "[ERROR] Usage: /deep-iterate <path> [max_iter] [--write]\n";
                }
                else
                {
                    if (writeBack)
                        std::cout << "[Deep Iteration] Write-back enabled: results will be saved to " << path << "\n";
                    std::cout << "[Deep Iteration] Running " << maxIter << " audit→code cycles on " << path << "...\n";
                    auto cfg = DeepIterationEngine::instance().getConfig();
                    cfg.maxIterations = maxIter;
                    cfg.writeResultToFile = writeBack;
                    DeepIterationEngine::instance().setConfig(cfg);
                    std::string finalCode;
                    bool ok = DeepIterationEngine::instance().run(path, "", &finalCode);
                    std::cout << "[Deep Iteration] " << (ok ? "Completed" : "Stopped") << ". "
                              << DeepIterationEngine::instance().getStatusString() << "\n";
                }
            }
            else if (input == "/deep-status")
            {
                std::cout << DeepIterationEngine::instance().getStatusString() << "\n";
                const auto& st = DeepIterationEngine::instance().getStats();
                std::cout << "  Iterations: " << st.totalIterations.load() << " | Audits: " << st.auditsRun.load()
                          << " | Code phases: " << st.codePhasesRun.load()
                          << " | Verifications: " << st.verificationsRun.load()
                          << " | Findings: " << st.findingsTotal.load() << " | Changes: " << st.changesApplied.load()
                          << " | Converged: " << st.convergenceCount.load()
                          << " | Complexity rejects: " << st.complexityRejects.load() << "\n";
            }
            else if (input.substr(0, 12) == "/deep-config ")
            {
                std::string rest = input.substr(12);
                while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t'))
                    rest.erase(0, 1);
                auto cfg = DeepIterationEngine::instance().getConfig();
                if (rest.empty())
                {
                    std::cout << "Deep Iteration Config:\n"
                              << "  max_iterations=" << cfg.maxIterations
                              << " convergence_window=" << cfg.convergenceWindow
                              << " min_complexity=" << cfg.minComplexityPreservation
                              << " max_tokens_per_phase=" << cfg.maxTokensPerPhase
                              << " write_result=" << (cfg.writeResultToFile ? "1" : "0") << "\n";
                }
                else
                {
                    size_t eq = rest.find('=');
                    if (eq != std::string::npos)
                    {
                        std::string key = rest.substr(0, eq);
                        std::string val = rest.substr(eq + 1);
                        while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
                            key.pop_back();
                        while (!val.empty() && (val[0] == ' ' || val[0] == '\t'))
                            val.erase(0, 1);
                        try
                        {
                            if (key == "max_iterations")
                                cfg.maxIterations = std::max(1, std::min(50, std::stoi(val)));
                            else if (key == "convergence_window")
                                cfg.convergenceWindow = std::max(1, std::min(20, std::stoi(val)));
                            else if (key == "min_complexity")
                                cfg.minComplexityPreservation = std::max(0.0f, std::min(1.0f, std::stof(val)));
                            else if (key == "max_tokens")
                                cfg.maxTokensPerPhase = std::max(256, std::min(32768, std::stoi(val)));
                            else if (key == "write_result")
                                cfg.writeResultToFile = (val == "1" || val == "true" || val == "yes");
                            else
                            {
                                std::cout << "[ERROR] Unknown deep-config key: " << key << "\n";
                                continue;
                            }
                            DeepIterationEngine::instance().setConfig(cfg);
                            std::cout << "[Deep Config] Set " << key << " = " << val << "\n";
                        }
                        catch (...)
                        {
                            std::cout << "[ERROR] Invalid value for key '" << key << "': " << val << "\n";
                        }
                    }
                    else
                    {
                        std::cout << "[ERROR] Usage: /deep-config [key=value]\n";
                    }
                }
            }
            else if (input == "/todo")
            {
                auto todos = subAgentMgr.getTodoList();
                if (todos.empty())
                {
                    std::cout << "Todo list empty.\n";
                }
                else
                {
                    for (const auto& t : todos)
                    {
                        std::string icon = "⬜";
                        if (t.status == TodoItem::Status::InProgress)
                            icon = "🔄";
                        else if (t.status == TodoItem::Status::Completed)
                            icon = "✅";
                        else if (t.status == TodoItem::Status::Failed)
                            icon = "❌";
                        std::cout << "  " << icon << " [" << t.id << "] " << t.title << "\n";
                    }
                }
            }
            // ── Phase 5: History, Timeline & Replay ──
            else if (input == "/history" || input.substr(0, 9) == "/history ")
            {
                std::string agentFilter;
                if (input.size() > 9)
                    agentFilter = input.substr(9);

                std::vector<AgentEvent> events;
                if (!agentFilter.empty())
                {
                    events = historyRecorder.getAgentTimeline(agentFilter);
                    std::cout << "[History] Events for agent " << agentFilter << ":\n";
                }
                else
                {
                    HistoryQuery q;
                    q.limit = 25;
                    events = historyRecorder.query(q);
                    std::cout << "[History] Last " << events.size() << " events (session "
                              << historyRecorder.sessionId() << "):\n";
                }

                if (events.empty())
                {
                    std::cout << "  (no events)\n";
                }
                else
                {
                    for (const auto& e : events)
                    {
                        std::string icon = e.success ? "✅" : "❌";
                        std::cout << "  " << icon << " #" << e.id << " [" << e.eventType << "]";
                        if (!e.agentId.empty())
                            std::cout << " agent=" << e.agentId;
                        std::cout << " " << e.description;
                        if (e.durationMs > 0)
                            std::cout << " (" << e.durationMs << "ms)";
                        std::cout << "\n";
                    }
                }
            }
            else if (input.substr(0, 8) == "/replay ")
            {
                std::string agentId = input.substr(8);
                std::cout << "[Replay] Replaying agent " << agentId << "...\n";

                ReplayRequest req;
                req.originalAgentId = agentId;
                req.dryRun = false;

                ReplayResult result = historyRecorder.replay(req, &subAgentMgr);
                if (result.success)
                {
                    std::cout << "[Replay ✅] " << result.eventsReplayed << " events replayed in " << result.durationMs
                              << "ms\n";
                    std::cout << "[Replay Result]\n" << result.result << "\n";
                    if (!result.originalResult.empty())
                    {
                        std::cout << "[Original Result]\n" << result.originalResult << "\n";
                    }
                }
                else
                {
                    std::cout << "[Replay ❌] " << result.result << "\n";
                }
            }
            else if (input == "/stats")
            {
                std::cout << "[History Stats] " << historyRecorder.getStatsSummary() << "\n";
                std::cout << "Total events: " << historyRecorder.eventCount() << "\n";
            }
            // ── Phase 7: Policy Engine Commands ──
            else if (input == "/policies")
            {
                auto policies = policyEngine.getAllPolicies();
                if (policies.empty())
                {
                    std::cout << "[Policies] No policies defined.\n";
                }
                else
                {
                    std::cout << "[Policies] " << policies.size() << " policies:\n";
                    for (const auto& p : policies)
                    {
                        std::string icon = p.enabled ? "🟢" : "⚪";
                        std::cout << "  " << icon << " [" << p.id << "] " << p.name << " (pri=" << p.priority << " v"
                                  << p.version << " applied=" << p.appliedCount << ")\n";
                        if (!p.description.empty())
                        {
                            std::cout << "      " << p.description << "\n";
                        }
                    }
                }
            }
            else if (input == "/suggest")
            {
                std::cout << "[Policy] Analyzing history and generating suggestions...\n";
                auto suggestions = policyEngine.generateSuggestions();
                auto pending = policyEngine.getPendingSuggestions();

                if (pending.empty())
                {
                    std::cout << "[Policy] No suggestions at this time. Need more history data.\n";
                }
                else
                {
                    std::cout << "[Policy] " << pending.size() << " pending suggestions:\n";
                    for (const auto& s : pending)
                    {
                        std::cout << "\n  📋 Suggestion [" << s.id << "]\n";
                        std::cout << "     Policy: " << s.proposedPolicy.name << "\n";
                        std::cout << "     Rationale: " << s.rationale << "\n";
                        std::cout << "     Estimated improvement: " << (int)(s.estimatedImprovement * 100.0f) << "%\n";
                        std::cout << "     Supporting events: " << s.supportingEvents << "\n";
                        std::cout << "     → /policy accept " << s.id << "\n";
                        std::cout << "     → /policy reject " << s.id << "\n";
                    }
                }
            }
            else if (input.substr(0, 15) == "/policy accept ")
            {
                std::string id = input.substr(15);
                if (policyEngine.acceptSuggestion(id))
                {
                    std::cout << "[Policy ✅] Suggestion accepted and policy activated.\n";
                    policyEngine.save();
                }
                else
                {
                    std::cout << "[Policy ❌] Suggestion not found or already decided.\n";
                }
            }
            else if (input.substr(0, 15) == "/policy reject ")
            {
                std::string id = input.substr(15);
                if (policyEngine.rejectSuggestion(id))
                {
                    std::cout << "[Policy ✅] Suggestion rejected.\n";
                    policyEngine.save();
                }
                else
                {
                    std::cout << "[Policy ❌] Suggestion not found or already decided.\n";
                }
            }
            else if (input.substr(0, 15) == "/policy export ")
            {
                std::string filePath = input.substr(15);
                if (policyEngine.exportToFile(filePath))
                {
                    std::cout << "[Policy ✅] Policies exported to " << filePath << "\n";
                }
                else
                {
                    std::cout << "[Policy ❌] Export failed.\n";
                }
            }
            else if (input.substr(0, 15) == "/policy import ")
            {
                std::string filePath = input.substr(15);
                int count = policyEngine.importFromFile(filePath);
                std::cout << "[Policy] Imported " << count << " policies from " << filePath << "\n";
                if (count > 0)
                    policyEngine.save();
            }
            else if (input == "/heuristics")
            {
                std::cout << "[Policy] Computing heuristics from event history...\n";
                policyEngine.computeHeuristics();
                auto heuristics = policyEngine.getAllHeuristics();

                if (heuristics.empty())
                {
                    std::cout << "[Policy] No heuristics computed. Need event history.\n";
                }
                else
                {
                    std::cout << "[Policy] " << heuristics.size() << " heuristics:\n";
                    for (const auto& h : heuristics)
                    {
                        std::cout << "  📊 " << h.key << " — events=" << h.totalEvents
                                  << " success=" << (int)(h.successRate * 100.0f) << "%"
                                  << " fail=" << h.failCount;
                        if (h.avgDurationMs > 0)
                        {
                            std::cout << " avg=" << (int)h.avgDurationMs << "ms"
                                      << " p95=" << (int)h.p95DurationMs << "ms";
                        }
                        std::cout << "\n";
                        for (const auto& reason : h.topFailureReasons)
                        {
                            std::cout << "      ⚠ " << reason << "\n";
                        }
                    }
                }
            }
            // ── Phase 8A: Explainability Commands ──
            else if (input == "/explain last")
            {
                // Find the most recent agent_spawn event
                HistoryQuery lastQ;
                lastQ.limit = 1;
                auto recent = historyRecorder.query(lastQ);
                if (recent.empty())
                {
                    std::cout << "[Explain] No events recorded yet.\n";
                }
                else
                {
                    // Walk backwards to find last agent
                    auto all = historyRecorder.allEvents();
                    std::string lastAgent;
                    for (auto it = all.rbegin(); it != all.rend(); ++it)
                    {
                        if (!it->agentId.empty())
                        {
                            lastAgent = it->agentId;
                            break;
                        }
                    }
                    if (lastAgent.empty())
                    {
                        std::cout << "[Explain] No agent events found.\n";
                    }
                    else
                    {
                        auto trace = explainEngine.traceAuto(lastAgent);
                        std::cout << "[Explain] " << trace.summary << "\n";
                        for (const auto& node : trace.nodes)
                        {
                            std::string icon = node.success ? "✅" : "❌";
                            std::cout << "  " << icon << " #" << node.eventId << " [" << node.eventType << "] "
                                      << node.trigger;
                            if (!node.policyId.empty())
                                std::cout << " 🛡️ " << node.policyName << ": " << node.policyEffect;
                            if (node.durationMs > 0)
                                std::cout << " (" << node.durationMs << "ms)";
                            std::cout << "\n";
                        }
                    }
                }
            }
            else if (input == "/explain session")
            {
                auto session = explainEngine.explainSession();
                std::cout << "[Explain] Session narrative:\n" << session.narrative << "\n\n";

                if (!session.failureAttributions.empty())
                {
                    std::cout << "Failure attributions:\n";
                    for (const auto& fa : session.failureAttributions)
                    {
                        std::cout << "  ❌ " << fa.agentId << " [" << fa.failureType << "]: " << fa.errorMessage;
                        if (fa.wasRetried)
                            std::cout << " → retried (" << (fa.retrySucceeded ? "success" : "failed") << ")";
                        if (!fa.policyId.empty())
                            std::cout << " [policy: " << fa.policyName << "]";
                        std::cout << "\n";
                    }
                }

                if (!session.policyAttributions.empty())
                {
                    std::cout << "\nPolicy attributions:\n";
                    for (const auto& pa : session.policyAttributions)
                    {
                        std::cout << "  🛡️ " << pa.policyName << " (applied " << pa.policyAppliedCount
                                  << "x): " << pa.effectDescription << "\n";
                    }
                }
            }
            else if (input.substr(0, 18) == "/explain snapshot ")
            {
                std::string filePath = input.substr(18);
                if (explainEngine.exportSnapshot(filePath))
                {
                    std::cout << "[Explain ✅] Snapshot exported to " << filePath << "\n";
                }
                else
                {
                    std::cout << "[Explain ❌] Failed to export snapshot.\n";
                }
            }
            else if (input.substr(0, 9) == "/explain ")
            {
                std::string agentId = input.substr(9);
                auto trace = explainEngine.traceAuto(agentId);
                std::cout << "[Explain] " << trace.summary << "\n";
                for (const auto& node : trace.nodes)
                {
                    std::string icon = node.success ? "✅" : "❌";
                    std::cout << "  " << icon << " #" << node.eventId << " [" << node.eventType << "] " << node.trigger;
                    if (!node.policyId.empty())
                        std::cout << " 🛡️ " << node.policyName << ": " << node.policyEffect;
                    if (node.durationMs > 0)
                        std::cout << " (" << node.durationMs << "ms)";
                    std::cout << "\n";
                }
                if (trace.failureCount > 0)
                {
                    std::cout << "\n" << explainEngine.summarizeFailures();
                }
            }
            // ── Phase 8B: Backend Switcher Commands ──
            else if (input == "/backend list")
            {
                auto backends = backendMgr.listBackends();
                std::string activeId = backendMgr.getActiveId();
                std::cout << "[Backends] " << backends.size() << " configured:\n";
                for (const auto& b : backends)
                {
                    std::string icon = (b.id == activeId) ? "🟢" : "⚪";
                    if (!b.enabled)
                        icon = "⛔";
                    std::cout << "  " << icon << " [" << b.id << "] " << b.displayName << " ("
                              << aiBackendTypeName(b.type) << ")";
                    if (!b.endpoint.empty())
                        std::cout << " → " << b.endpoint;
                    if (!b.model.empty())
                        std::cout << " model=" << b.model;
                    std::cout << "\n";
                }
            }
            else if (input.substr(0, 13) == "/backend use ")
            {
                std::string id = input.substr(13);
                if (backendMgr.setActiveBackend(id))
                {
                    std::cout << "[Backend ✅] Active backend → " << id << " (" << backendMgr.getActiveBackendName()
                              << ")\n";
                }
                else
                {
                    std::cout << "[Backend ❌] Backend '" << id << "' not found or disabled. Use /backend list.\n";
                }
            }
            else if (input == "/backend status")
            {
                std::cout << "[Backend Status] " << backendMgr.getStatsJSON() << "\n";
                auto active = backendMgr.getActiveBackend();
                std::cout << "  Active: " << active.displayName << " (" << aiBackendTypeName(active.type) << ")\n";
                if (!active.endpoint.empty())
                    std::cout << "  Endpoint: " << active.endpoint << "\n";
                if (!active.model.empty())
                    std::cout << "  Model: " << active.model << "\n";
                std::cout << "  MaxTokens: " << active.maxTokens << "  Temperature: " << active.temperature
                          << "  Timeout: " << active.timeoutMs << "ms\n";
            }
            // ── Phase 25: AMD GPU Acceleration Commands ──
            else if (input == "/gpu")
            {
                std::cout << "[GPU] " << gpuAccel.toJson() << "\n";
            }
            else if (input == "/gpu on")
            {
                auto r = gpuAccel.enableGPU();
                std::cout << "[GPU] " << r.detail << "\n";
            }
            else if (input == "/gpu off")
            {
                auto r = gpuAccel.disableGPU();
                std::cout << "[GPU] " << r.detail << "\n";
            }
            else if (input == "/gpu toggle")
            {
                auto r = gpuAccel.toggleGPU();
                std::cout << "[GPU] " << r.detail << " — now " << (gpuAccel.isGPUEnabled() ? "ENABLED" : "DISABLED")
                          << "\n";
            }
            else if (input == "/gpu features")
            {
                std::cout << "[GPU Features] " << gpuAccel.featuresToJson() << "\n";
            }
            else if (input == "/gpu memory")
            {
                std::cout << "[GPU Memory] " << gpuAccel.memoryToJson() << "\n";
            }
            // ── Phase 23: GPU Kernel Auto-Tuner Commands ──
            else if (input == "/tune")
            {
                std::cout << "[Tuner] Running kernel auto-tuner...\n";
                auto r = kernelTuner.tuneAllKernels();
                std::cout << "[Tuner] " << r.detail << " configs=" << r.configsTested << "\n";
                std::cout << "[Tuner] " << kernelTuner.toJson() << "\n";
            }
            else if (input == "/tune cache")
            {
                std::cout << "[Tuner Cache] " << kernelTuner.tuneCacheToJson() << "\n";
            }
            // ── Phase 21: Swarm Decision Bridge Commands ──
            else if (input == "/swarm bridge")
            {
                std::cout << "[Swarm Bridge] " << swarmBridge.toJson() << "\n";
            }
            // ── Phase 21: Universal Model Hotpatcher Commands ──
            else if (input == "/hotpatch status")
            {
                std::cout << "[Model Hotpatcher] " << modelHotpatcher.toJson() << "\n";
            }
            // ── Phase 20: WebRTC Commands ──
            else if (input == "/webrtc")
            {
                std::cout << "[WebRTC] " << webrtc.toJson() << "\n";
                std::cout << "[WebRTC Peers] " << webrtc.peersToJson() << "\n";
            }
            // ── Phase 24: Sandbox Commands ──
            else if (input == "/sandbox list")
            {
                auto sandboxes = sandboxMgr.listSandboxes();
                if (sandboxes.empty())
                {
                    std::cout << "[Sandbox] No active sandboxes.\n";
                }
                else
                {
                    for (const auto& id : sandboxes)
                    {
                        std::cout << "  " << sandboxMgr.sandboxToJson(id) << "\n";
                    }
                }
            }
            else if (input == "/sandbox create")
            {
                SandboxConfig cfg;
                cfg.type = SandboxType::JobObject;
                std::string id;
                auto r = sandboxMgr.createSandbox(cfg, id);
                std::cout << "[Sandbox] " << r.detail << " id=" << id << "\n";
            }
            // ── Phase 22: Production Release Commands ──
            else if (input == "/release")
            {
                std::cout << "[Release] " << releaseEngine.toJson() << "\n";
            }
            // ── Phase 51: Security (Dork Scanner + Universal Dorker) ──
            else if (input == "/security")
            {
                using namespace RawrXD::Security;
                int db = 0, ub = 0;
                {
                    RawrXD::Security::GoogleDorkScanner sc(DorkScannerConfig{});
                    if (sc.initialize())
                        db = sc.getBuiltinDorkCount();
                }
                ub = RawrXD::Security::UniversalPhpDorker::GetBuiltinCount();
                std::cout << "[Security] phase=51 dorkScanner.builtinDorkCount=" << db
                          << " universalDorker.builtinDorkCount=" << ub
                          << " endpoints=/api/security/dork/status,/api/security/dork/scan,/api/security/dork/"
                             "universal,/api/security/dashboard\n";
            }
            else if (input == "/dork status")
            {
                using namespace RawrXD::Security;
                DorkScannerConfig def = {};
                def.threadCount = 4;
                def.delayMs = 1500;
                def.maxIterations = 100;
                int builtin = 0;
                {
                    RawrXD::Security::GoogleDorkScanner sc(def);
                    if (sc.initialize())
                        builtin = sc.getBuiltinDorkCount();
                }
                int universalCount = RawrXD::Security::UniversalPhpDorker::GetBuiltinCount();
                std::cout << "[Dork] builtinDorkCount=" << builtin << " universalBuiltinDorkCount=" << universalCount
                          << " defaultThreadCount=4 delayMs=1500 maxIterations=100\n";
            }
            // ── Agentic Autonomous: Operation mode (Agent/Plan/Debug/Ask) + Model selection ──
            else if (input == "/agentic")
            {
                auto& cfg = RawrXD::AgenticAutonomousConfig::instance();
                std::cout << "[Agentic] " << cfg.toDisplayString() << "\n";
                std::cout << "  Operation: Agent (full) | Plan | Debug | Ask\n";
                std::cout << "  /agentic mode <Agent|Plan|Debug|Ask>\n";
            }
            else if (input.substr(0, 12) == "/agentic mode ")
            {
                std::string modeStr = input.substr(12);
                auto& cfg = RawrXD::AgenticAutonomousConfig::instance();
                cfg.setOperationModeFromString(modeStr);
                std::cout << "[Agentic] Operation mode set to: " << RawrXD::to_string(cfg.getOperationMode()) << "\n";
            }
            else if (input == "/models")
            {
                auto& cfg = RawrXD::AgenticAutonomousConfig::instance();
                std::cout << "[Models] " << cfg.toDisplayString() << "\n";
                std::cout << "  Mode: Auto | MAX | UseMultipleModels\n";
                std::cout << "  Per-model instances: 1-4, global cap: 1-40, cycle: 1x-4x\n";
                std::cout
                    << "  /models mode <Auto|MAX|multiple> | /models per-model <1-4> | cap <1-40> | cycle <1-4>\n";
            }
            else if (input.substr(0, 12) == "/models mode ")
            {
                std::string modeStr = input.substr(12);
                auto& cfg = RawrXD::AgenticAutonomousConfig::instance();
                cfg.setModelSelectionModeFromString(modeStr);
                std::cout << "[Models] Model selection set to: " << RawrXD::to_string(cfg.getModelSelectionMode())
                          << "\n";
            }
            else if (input.substr(0, 17) == "/models per-model ")
            {
                try
                {
                    int n = std::stoi(input.substr(17));
                    auto& cfg = RawrXD::AgenticAutonomousConfig::instance();
                    cfg.setPerModelInstanceCount(n);
                    std::cout << "[Models] Per-model instances: " << cfg.getPerModelInstanceCount() << "\n";
                }
                catch (...)
                {
                    std::cout << "[Models] Usage: /models per-model <1-4>\n";
                }
            }
            else if (input.substr(0, 11) == "/models cap ")
            {
                try
                {
                    int n = std::stoi(input.substr(11));
                    auto& cfg = RawrXD::AgenticAutonomousConfig::instance();
                    cfg.setMaxModelsInParallel(n);
                    std::cout << "[Models] Max models in parallel: " << cfg.getMaxModelsInParallel() << "\n";
                }
                catch (...)
                {
                    std::cout << "[Models] Usage: /models cap <1-40>\n";
                }
            }
            else if (input.substr(0, 13) == "/models cycle ")
            {
                try
                {
                    int n = std::stoi(input.substr(13));
                    auto& cfg = RawrXD::AgenticAutonomousConfig::instance();
                    cfg.setCycleAgentCounter(n);
                    std::cout << "[Models] Cycle agent counter: " << cfg.getCycleAgentCounter() << "x\n";
                }
                catch (...)
                {
                    std::cout << "[Models] Usage: /models cycle <1-4>\n";
                }
            }
            else if (dispatchProfileBangCommand(input))
            {
                // Profile command handled via shared feature dispatcher hotpatch path.
            }
            // ── Phase 20: Model Hotpatcher ! Commands ──
            else if (input.substr(0, 12) == "!model_load ")
            {
                cmd_model("load " + input.substr(12));
            }
            else if (input == "!model_plan")
            {
                cmd_model("plan");
            }
            else if (input == "!model_surgery")
            {
                cmd_model("surgery");
            }
            else if (input == "!pressure_auto")
            {
                cmd_model("pressure");
            }
            else if (input == "!model_status")
            {
                cmd_model("status");
            }
            else if (input == "!model_layers")
            {
                cmd_model("layers");
            }
            else if (input.substr(0, 7) == "!model " && input.size() > 7)
            {
                cmd_model(input.substr(7));
            }
            // ── Phase 21: Swarm Orchestrator ! Commands ──
            else if (input == "!swarm_join" || input.substr(0, 12) == "!swarm_join ")
            {
                std::string arg = input.size() > 12 ? input.substr(12) : "";
                cmd_swarm_orchestrator("join " + arg);
            }
            else if (input == "!swarm_status")
            {
                cmd_swarm_orchestrator("status");
            }
            else if (input.substr(0, 17) == "!swarm_distribute")
            {
                std::string arg = input.size() > 18 ? input.substr(18) : "";
                cmd_swarm_orchestrator("distribute " + arg);
            }
            else if (input == "!swarm_rebalance")
            {
                cmd_swarm_orchestrator("rebalance");
            }
            else if (input == "!swarm_nodes")
            {
                cmd_swarm_orchestrator("nodes");
            }
            else if (input == "!swarm_shards")
            {
                cmd_swarm_orchestrator("shards");
            }
            else if (input == "!swarm_leave")
            {
                cmd_swarm_orchestrator("leave");
            }
            else if (input == "!swarm_stats")
            {
                cmd_swarm_orchestrator("stats");
            }
            // ── Phase 33: Voice Chat Commands ──
            else if (input == "/voice record")
            {
                static VoiceChat cliVoiceChat;
                static bool cliVoiceInit = false;
                if (!cliVoiceInit)
                {
                    VoiceChatConfig vcfg;
                    vcfg.sampleRate = 16000;
                    vcfg.channels = 1;
                    vcfg.bitsPerSample = 16;
                    vcfg.enableVAD = true;
                    cliVoiceChat.configure(vcfg);
                    cliVoiceInit = true;
                }
                if (cliVoiceChat.isRecording())
                {
                    auto r = cliVoiceChat.stopRecording();
                    std::cout << "[Voice] Recording stopped: " << r.detail << " ("
                              << cliVoiceChat.getRecordedSampleCount() << " samples)\n";
                }
                else
                {
                    auto r = cliVoiceChat.startRecording();
                    std::cout << "[Voice] " << r.detail << "\n";
                    std::cout << "[Voice] Type '/voice record' again to stop.\n";
                }
            }
            else if (input == "/voice play")
            {
                static VoiceChat cliVoiceChat;
                auto& rec = cliVoiceChat.getLastRecording();
                if (rec.empty())
                {
                    std::cout << "[Voice] No recording to play. Use '/voice record' first.\n";
                }
                else
                {
                    std::cout << "[Voice] Playing " << rec.size() << " samples...\n";
                    auto r = cliVoiceChat.playAudio(rec);
                    std::cout << "[Voice] " << r.detail << "\n";
                }
            }
            else if (input == "/voice transcribe")
            {
                static VoiceChat cliVoiceChat;
                std::string transcript;
                auto r = cliVoiceChat.transcribeLastRecording(transcript);
                if (r.success)
                {
                    std::cout << "[Voice STT] " << transcript << "\n";
                }
                else
                {
                    std::cout << "[Voice] Transcription failed: " << r.detail << "\n";
                }
            }
            else if (input.substr(0, 13) == "/voice speak ")
            {
                static VoiceChat cliVoiceChat;
                std::string text = input.substr(13);
                auto r = cliVoiceChat.speak(text);
                std::cout << "[Voice TTS] " << r.detail << "\n";
            }
            else if (input == "/voice devices")
            {
                auto inputs = VoiceChat::enumerateInputDevices();
                auto outputs = VoiceChat::enumerateOutputDevices();
                std::cout << "=== Input Devices ===\n";
                for (auto& d : inputs)
                {
                    std::cout << "  [" << d.id << "] " << d.name;
                    if (d.isDefault)
                        std::cout << " (default)";
                    std::cout << "\n";
                }
                std::cout << "=== Output Devices ===\n";
                for (auto& d : outputs)
                {
                    std::cout << "  [" << d.id << "] " << d.name;
                    if (d.isDefault)
                        std::cout << " (default)";
                    std::cout << "\n";
                }
            }
            else if (input.substr(0, 12) == "/voice mode ")
            {
                static VoiceChat cliVoiceChat;
                std::string mode = input.substr(12);
                VoiceChatMode m = VoiceChatMode::PushToTalk;
                if (mode == "vad" || mode == "continuous")
                    m = VoiceChatMode::Continuous;
                else if (mode == "off" || mode == "disabled")
                    m = VoiceChatMode::Disabled;
                cliVoiceChat.setMode(m);
                std::cout << "[Voice] Mode set to: " << mode << "\n";
            }
            else if (input.substr(0, 12) == "/voice room ")
            {
                static VoiceChat cliVoiceChat;
                std::string room = input.substr(12);
                if (cliVoiceChat.isInRoom())
                {
                    cliVoiceChat.leaveRoom();
                    std::cout << "[Voice] Left room\n";
                }
                auto r = cliVoiceChat.joinRoom(room);
                std::cout << "[Voice] " << r.detail << " (" << room << ")\n";
            }
            else if (input == "/voice status")
            {
                static VoiceChat cliVoiceChat;
                std::cout << "[Voice] Recording: " << (cliVoiceChat.isRecording() ? "YES" : "no")
                          << "  Playing: " << (cliVoiceChat.isPlaying() ? "YES" : "no")
                          << "  Room: " << (cliVoiceChat.isInRoom() ? cliVoiceChat.getRoomName() : "(none)")
                          << "  RMS: " << cliVoiceChat.getCurrentRMS()
                          << "  Samples: " << cliVoiceChat.getRecordedSampleCount() << "\n";
            }
            else if (input == "/voice metrics")
            {
                static VoiceChat cliVoiceChat;
                auto m = cliVoiceChat.getMetrics();
                std::cout << "[Voice Metrics]\n"
                          << "  Recordings:    " << m.recordingCount << "\n"
                          << "  Playbacks:     " << m.playbackCount << "\n"
                          << "  Transcriptions:" << m.transcriptionCount << "\n"
                          << "  TTS Calls:     " << m.ttsCount << "\n"
                          << "  Errors:        " << m.errorCount << "\n"
                          << "  Bytes Recorded:" << m.bytesRecorded << "\n"
                          << "  VAD Events:    " << m.vadSpeechEvents << "\n";
            }
            // ── Phase 26: ReverseEngineered Kernel Commands ──
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
            else if (input == "/scheduler status")
            {
                auto& state = RawrXD::ReverseEngineered::GetState();
                bool initialized = state.schedulerInit.load();
                std::cout << "[Scheduler] Work-stealing scheduler: "
                          << (initialized ? "INITIALIZED" : "NOT INITIALIZED") << "\n";
                if (initialized)
                {
                    // Live probe: submit a real task and verify worker execution
                    uint64_t probeUs = 0;
                    bool probeOk = RawrXD::ReverseEngineered::ProbeScheduler(&probeUs);
                    std::cout << "  Probe task: " << (probeOk ? "OK" : "FAILED") << " (" << probeUs << " us)\n";
                    std::cout << "  Workers: " << state.workerCount.load() << "  NUMA: enabled\n"
                              << "  Tasks submitted: " << state.tasksSubmitted.load()
                              << "  completed: " << state.tasksCompleted.load() << "\n";
                }
            }
            else if (input == "/scheduler submit")
            {
                // Submit a benchmark task that writes a sentinel to verify execution
                auto& state = RawrXD::ReverseEngineered::GetState();
                volatile uint64_t sentinel = 0;
                auto testFn = [](void* arg) -> void
                {
                    if (arg)
                        *static_cast<volatile uint64_t*>(arg) = 0xDEADC0DEULL;
                };
                uint64_t t0 = GetHighResTick();
                int64_t taskId = Scheduler_SubmitTask(reinterpret_cast<void*>(+testFn),
                                                      const_cast<uint64_t*>(&sentinel), 0, 0, nullptr);
                state.tasksSubmitted.fetch_add(1);
                if (taskId >= 0)
                {
                    std::cout << "[Scheduler] Task submitted: id=" << taskId << "\n";
                    void* result = Scheduler_WaitForTask(taskId, 5000);
                    uint64_t elapsedUs = TicksToMicroseconds(GetHighResTick() - t0);
                    bool workerRan = (sentinel == 0xDEADC0DEULL);
                    if (workerRan)
                        state.tasksCompleted.fetch_add(1);
                    std::cout << "[Scheduler] Worker executed: " << (workerRan ? "YES" : "NO")
                              << "  Wait result: " << (result ? "OK" : "timeout") << "  Latency: " << elapsedUs
                              << " us\n";
                }
                else
                {
                    std::cout << "[Scheduler] Task submit failed: " << taskId << "\n";
                }
            }
            else if (input == "/conflict status")
            {
                auto& state = RawrXD::ReverseEngineered::GetState();
                bool initialized = state.conflictDetectorInit.load();
                std::cout << "[ConflictDetector] Deadlock detection: "
                          << (initialized ? "INITIALIZED" : "NOT INITIALIZED") << "\n";
                if (initialized)
                {
                    // Live probe: register + lock + unlock a test resource
                    uint64_t probeUs = 0;
                    int probeRc = RawrXD::ReverseEngineered::ProbeConflictDetector(&probeUs);
                    const char* probeMsg = "error";
                    if (probeRc == 0)
                        probeMsg = "OK";
                    else if (probeRc == 1)
                        probeMsg = "DEADLOCK_DETECTED";
                    else if (probeRc == -2)
                        probeMsg = "TABLE_FULL";
                    std::cout << "  Probe: " << probeMsg << " (" << probeUs << " us)\n";
                    std::cout << "  Max resources: " << state.maxResources.load()
                              << "  Scan interval: " << state.conflictScanIntervalMs.load() << "ms\n"
                              << "  Lock ops: " << state.conflictLocks.load()
                              << "  Unlock ops: " << state.conflictUnlocks.load() << "\n";
                }
            }
            else if (input == "/heartbeat status")
            {
                auto& state = RawrXD::ReverseEngineered::GetState();
                bool initialized = state.heartbeatInit.load();
                std::cout << "[Heartbeat] UDP gossip monitor: " << (initialized ? "ACTIVE" : "DISABLED") << "\n";
                if (initialized)
                {
                    std::cout << "  Listen port: " << state.heartbeatPort.load()
                              << "  Interval: " << state.heartbeatIntervalMs.load() << "ms\n"
                              << "  Nodes added: " << state.heartbeatNodesAdded.load() << "\n";
                }
                else
                {
                    std::cout << "  Port not configured (heartbeatPort=0 at init)\n"
                              << "  Use /heartbeat add <ip> <port> to register nodes\n";
                }
            }
            else if (input.substr(0, 15) == "/heartbeat add ")
            {
                std::string args = input.substr(15);
                size_t sp = args.find(' ');
                if (sp != std::string::npos)
                {
                    std::string ip = args.substr(0, sp);
                    int parsedHp = 0;
                    if (!tryParseIntInRange(args.substr(sp + 1), 1, 65535, parsedHp))
                    {
                        std::cout << "[Heartbeat] Invalid port. Usage: /heartbeat add <ip> <port>\n";
                        continue;
                    }
                    uint16_t hp = static_cast<uint16_t>(parsedHp);
                    static uint32_t nextNodeId = 1;
                    int rc = Heartbeat_AddNode(nextNodeId++, ip.c_str(), hp);
                    if (rc == 0)
                    {
                        RawrXD::ReverseEngineered::GetState().heartbeatNodesAdded.fetch_add(1);
                    }
                    std::cout << "[Heartbeat] Add node " << ip << ":" << hp << " -> " << (rc == 0 ? "OK" : "FAILED")
                              << "\n";
                }
                else
                {
                    std::cout << "[Heartbeat] Usage: /heartbeat add <ip> <port>\n";
                }
            }
            else if (input == "/gpu dma status")
            {
                auto& state = RawrXD::ReverseEngineered::GetState();
                std::cout << "[GPU DMA] Running live probe...\n";
                uint64_t probeUs = 0;
                bool allocOk = false, transferOk = false, verifyOk = false;
                RawrXD::ReverseEngineered::ProbeDMA(&probeUs, &allocOk, &transferOk, &verifyOk);
                std::cout << "  Alloc: " << (allocOk ? "OK" : "FAILED")
                          << "  Transfer: " << (transferOk ? "OK" : "FAILED")
                          << "  Verify: " << (verifyOk ? "OK" : "MISMATCH") << " (" << probeUs << " us)\n";
                std::cout << "  Total DMA transfers: " << state.dmaTransfers.load() << "\n";
            }
            else if (input == "/tensor bench")
            {
                // Quick benchmark: 64x64 INT8 matmul
                const uint32_t M = 64, N = 64, K = 64;
                void* A = AllocateDMABuffer(M * K);
                void* B = AllocateDMABuffer(K * N);
                float* C = (float*)AllocateDMABuffer(M * N * sizeof(float));
                if (A && B && C)
                {
                    memset(A, 1, M * K);
                    memset(B, 1, K * N);
                    uint64_t t0 = GetHighResTick();
                    Tensor_QuantizedMatMul(A, B, C, M, N, K, QUANT_Q8_0);
                    uint64_t t1 = GetHighResTick();
                    uint64_t elapsed = TicksToMicroseconds(t1 - t0);
                    RawrXD::ReverseEngineered::GetState().tensorOps.fetch_add(1);
                    double gflops = 0.0;
                    if (elapsed > 0)
                    {
                        gflops = (2.0 * M * N * K) / (static_cast<double>(elapsed) * 1000.0);
                    }
                    std::cout << "[Tensor] 64x64 Q8_0 matmul: " << elapsed << " us"
                              << "  (" << gflops << " GFLOPS)\n"
                              << "  C[0,0]=" << C[0] << " C[63,63]=" << C[63 * 64 + 63] << "\n"
                              << "  Total tensor ops: " << RawrXD::ReverseEngineered::GetState().tensorOps.load()
                              << "\n";
                    VirtualFree(A, 0, MEM_RELEASE);
                    VirtualFree(B, 0, MEM_RELEASE);
                    VirtualFree(C, 0, MEM_RELEASE);
                }
                else
                {
                    std::cout << "[Tensor] DMA buffer allocation failed\n";
                }
            }
            else if (input == "/timer")
            {
                uint64_t t0 = GetHighResTick();
                Sleep(100);
                uint64_t t1 = GetHighResTick();
                uint64_t ms = TicksToMilliseconds(t1 - t0);
                uint64_t us = TicksToMicroseconds(t1 - t0);
                std::cout << "[Timer] 100ms sleep measured: " << ms << " ms (" << us << " us)\n";
                // CRC test
                const char* testData = "RawrXD-ReverseEngineered-Kernel";
                uint32_t crc = CalculateCRC32(testData, strlen(testData));
                std::cout << "[CRC32] \"" << testData << "\" -> 0x" << std::hex << crc << std::dec << "\n";
            }
            else if (input.rfind("/crc32 ", 0) == 0)
            {
                std::string payload = input.substr(7);
                if (payload.empty())
                {
                    std::cout << "[CRC32] Usage: /crc32 <text>\n";
                }
                else
                {
                    auto& st = RawrXD::ReverseEngineered::GetState();
                    uint64_t t0 = GetHighResTick();
                    uint32_t crc = CalculateCRC32(payload.c_str(), static_cast<uint32_t>(payload.size()));
                    uint64_t t1 = GetHighResTick();
                    uint64_t us = TicksToMicroseconds(t1 - t0);
                    std::cout << "[CRC32] Input : " << payload << "\n"
                              << "[CRC32] Length: " << payload.size() << " bytes\n"
                              << "[CRC32] Hash  : 0x" << std::hex << crc << std::dec << "\n"
                              << "[CRC32] Time  : " << us << " us\n";
                }
            }
#endif
            // ═══════════════════════════════════════════════════════════
            // Enterprise License REPL Commands
            // ═══════════════════════════════════════════════════════════
            else if (input == "/license" || input == "/lic")
            {
                std::cout << EnterpriseFeatureManager::Instance().GenerateDashboard();
            }
            else if (input == "/license audit" || input == "/lic audit")
            {
                std::cout << EnterpriseFeatureManager::Instance().GenerateAuditReport();
            }
            else if (input == "/license unlock" || input == "/lic unlock")
            {
                bool ok = EnterpriseFeatureManager::Instance().DevUnlock();
                if (ok)
                {
                    std::cout << "[License] All enterprise features unlocked!\n";
                    std::cout << EnterpriseFeatureManager::Instance().GenerateDashboard();
                }
                else
                {
                    std::cout << "[License] Dev unlock failed. Set RAWRXD_ENTERPRISE_DEV=1 and restart.\n";
                }
            }
            else if (input == "/license hwid" || input == "/lic hwid")
            {
                std::cout << "[License] HWID: " << EnterpriseFeatureManager::Instance().GetHWIDString() << "\n";
            }
            else if (input.rfind("/license install ", 0) == 0)
            {
                std::string path = input.substr(17);
                bool ok = EnterpriseFeatureManager::Instance().InstallLicenseFromFile(path);
                if (ok)
                {
                    std::cout << "[License] License installed successfully!\n";
                    std::cout << EnterpriseFeatureManager::Instance().GenerateDashboard();
                }
                else
                {
                    std::cout << "[License] Install failed. Check file path and format.\n";
                }
            }
            else if (input == "/license features" || input == "/lic features")
            {
                auto statuses = EnterpriseFeatureManager::Instance().GetFeatureStatuses();
                std::cout << "\nEnterprise Features (" << EnterpriseFeatureManager::Instance().GetEditionName()
                          << "):\n";
                for (const auto& s : statuses)
                {
                    std::cout << "  [" << (s.active ? "ACTIVE" : "LOCKED") << "] " << s.name << " (0x" << std::hex
                              << s.mask << std::dec << ")\n";
                }
                std::cout << "\n";
            }
            // ═══════════════════════════════════════════════════════════
            // Per-Feature Status Commands
            // ═══════════════════════════════════════════════════════════
            else if (input == "/800b")
            {
                bool active = RawrXD::EnterpriseLicense::isFeatureEnabled(0x01);
                std::cout << "[800B Dual-Engine] Status: " << (active ? "ACTIVE" : "LOCKED") << "\n";
                if (active)
                {
                    std::cout << "  Max model size: " << RawrXD::EnterpriseLicense::Instance().GetMaxModelSizeGB()
                              << " GB\n";
                    std::cout << "  800B unlocked: "
                              << (RawrXD::EnterpriseLicense::Instance().Is800BUnlocked() ? "Yes" : "No") << "\n";
                }
                else
                {
                    std::cout << "  Requires: Enterprise license\n";
                }
            }
            else if (input == "/avx512")
            {
                bool active = RawrXD::EnterpriseLicense::isFeatureEnabled(0x02);
                std::cout << "[AVX-512 Premium] Status: " << (active ? "ACTIVE" : "LOCKED") << "\n";
                if (active)
                {
                    std::cout << "  Premium AVX-512 kernels enabled for inference and quantization\n";
                }
                else
                {
                    std::cout << "  Requires: Pro or Enterprise license\n";
                }
            }
            else if (input == "/swarm")
            {
                bool active = RawrXD::EnterpriseLicense::isFeatureEnabled(0x04);
                std::cout << "[Distributed Swarm] Status: " << (active ? "ACTIVE" : "LOCKED") << "\n";
                if (active)
                {
                    std::cout << "  Distributed inference orchestrator is available\n";
                }
                else
                {
                    std::cout << "  Requires: Enterprise license\n";
                }
            }
            else if (input == "/gpuquant")
            {
                bool active = RawrXD::EnterpriseLicense::isFeatureEnabled(0x08);
                std::cout << "[GPU Quant 4-bit] Status: " << (active ? "ACTIVE" : "LOCKED") << "\n";
                if (active)
                {
                    std::cout << "  GPU-accelerated Q4_0/Q4_K quantization enabled\n";
                }
                else
                {
                    std::cout << "  Requires: Pro or Enterprise license\n";
                }
            }
            else if (input == "/support")
            {
                bool active = RawrXD::EnterpriseLicense::isFeatureEnabled(0x10);
                std::cout << "[Enterprise Support] Status: " << (active ? "ACTIVE" : "LOCKED") << "\n";
                if (active)
                {
                    std::cout << RawrXD::Enterprise::SupportTierManager::Instance().GenerateStatusReport();
                }
                else
                {
                    std::cout << "  Requires: Enterprise license\n";
                }
            }
            else if (input == "/context")
            {
                bool active = RawrXD::EnterpriseLicense::isFeatureEnabled(0x20);
                std::cout << "[Unlimited Context] Status: " << (active ? "ACTIVE" : "LOCKED") << "\n";
                std::cout << "  Max context: " << RawrXD::EnterpriseLicense::Instance().GetMaxContextLength()
                          << " tokens\n";
                if (!active)
                {
                    std::cout << "  (Community limit: 32K — upgrade for 200K)\n";
                }
            }
            else if (input == "/flashattn")
            {
                bool active = RawrXD::EnterpriseLicense::isFeatureEnabled(0x40);
                std::cout << "[Flash Attention] Status: " << (active ? "ACTIVE" : "LOCKED") << "\n";
                if (active)
                {
                    std::cout << "  Flash Attention v2 MASM kernels enabled\n";
                }
                else
                {
                    std::cout << "  Requires: Pro or Enterprise license\n";
                }
            }
            else if (input == "/multigpu")
            {
                bool active = RawrXD::EnterpriseLicense::isFeatureEnabled(0x80);
                std::cout << "[Multi-GPU] Status: " << (active ? "ACTIVE" : "LOCKED") << "\n";
                auto& mgr = RawrXD::Enterprise::MultiGPUManager::Instance();
                std::cout << mgr.GenerateStatusReport();
                if (mgr.GetDeviceCount() > 1)
                {
                    std::cout << mgr.GenerateTopologyReport();
                }
            }
            else
            {
                std::cout << "Unknown command. Type /help for commands.\n";
            }
        }
    }
    else
    {
        while (true)
        {
            if (g_shutdownRequested.load())
                break;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // Phase 26: ReverseEngineered Kernel Shutdown
    // ═══════════════════════════════════════════════════════════════════
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
    {
        std::cout << "[SYSTEM] Shutting down ReverseEngineered kernel...\n";
        RawrXD::ReverseEngineered::ShutdownAllSubsystems();
        std::cout << "[SYSTEM] ReverseEngineered kernel shutdown complete\n";
    }
#endif

    // Stop autonomy loop before shutdown
    CLIAutonomyLoop::instance().stop();

    // Shutdown headless CLI subsystems
    cli_headless_shutdown();

    // Shutdown enterprise subsystems
    EnterpriseFeatureManager::Instance().Shutdown();
    RawrXD::EnterpriseLicense::Instance().Shutdown();

    server.Stop();
    return 0;
}
#endif  // RAWRXD_STANDALONE_MAIN
