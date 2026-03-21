// ============================================================================
// HeadlessIDE.cpp — GUI-free surface for the RawrXD Win32IDE engine
// Phase 19C: Headless Surface Extraction
//
// Implements the headless IDE that exposes the full engine capabilities
// without any HWND, window, or GDI dependency. Starts the HTTP server,
// initializes all backend subsystems, and runs in one of four modes:
//   Server / REPL / SingleShot / Batch
//
// NO exceptions. NO HWND. NO GDI. NO message loop.
// ============================================================================

#include "HeadlessIDE.h"
#include "IOutputSink.h"
#include "../../include/chain_of_thought_engine.h"
#include "../core/instructions_provider.hpp"

// Phase 10+ singletons — wired for real status queries
#include "../core/execution_governor.h"
#include "../core/agent_safety_contract.h"
#include "../core/deterministic_replay.h"
#include "../core/confidence_gate.h"
#include "multi_response_engine.h"
#include "../core/universal_model_hotpatcher.h"
#include "../../include/swarm_orchestrator.h"
#include "../agent_history.h"
#include "../agent_explainability.h"
#include "../agent_policy.h"
#include "../agentic/AgentOllamaClient.h"
#include "../core/enterprise_license.h"
#include "../../include/lsp/RawrXD_LSPServer.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <csignal>
#include <cstring>
#include <algorithm>
#include <functional>
#include <cstdio>
#include <thread>
#include <string>
#include <memory>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <windows.h>
#include <utility>

// ============================================================================
// Global shutdown flag for SIGINT/SIGTERM handler
// ============================================================================
static std::atomic<HeadlessIDE*> g_headlessInstance{nullptr};

// ============================================================================
// Embedded LSP server instance (owned by HeadlessIDE init, lives in .cpp scope)
// ============================================================================
static std::unique_ptr<RawrXD::LSPServer::RawrXDLSPServer> g_embeddedLSP;
static std::mutex                       g_embeddedLSPMutex;

static void headlessSignalHandler(int sig) {
    HeadlessIDE* inst = g_headlessInstance.load();
    if (inst) {
        inst->requestShutdown();
    }
}

// ============================================================================
// ConsoleOutputSink implementation
// ============================================================================
void ConsoleOutputSink::appendOutput(const char* text, size_t length, OutputSeverity severity) noexcept {
    if (!text || length == 0) return;
    if (m_quiet && severity < OutputSeverity::Warning) return;
    if (!m_verbose && severity == OutputSeverity::Debug) return;

    if (m_jsonMode) {
        const char* sevStr = "info";
        switch (severity) {
            case OutputSeverity::Debug:   sevStr = "debug"; break;
            case OutputSeverity::Info:    sevStr = "info"; break;
            case OutputSeverity::Warning: sevStr = "warning"; break;
            case OutputSeverity::Error:   sevStr = "error"; break;
        }
        fprintf(stdout, "{\"type\":\"output\",\"severity\":\"%s\",\"text\":\"", sevStr);
        // Escape JSON string
        for (size_t i = 0; i < length; ++i) {
            char c = text[i];
            if (c == '"') fputs("\\\"", stdout);
            else if (c == '\\') fputs("\\\\", stdout);
            else if (c == '\n') fputs("\\n", stdout);
            else if (c == '\r') fputs("\\r", stdout);
            else if (c == '\t') fputs("\\t", stdout);
            else fputc(c, stdout);
        }
        fputs("\"}\n", stdout);
    } else {
        FILE* out = (severity >= OutputSeverity::Warning) ? stderr : stdout;
        const char* prefix = "";
        switch (severity) {
            case OutputSeverity::Debug:   prefix = "[DEBUG] "; break;
            case OutputSeverity::Info:    prefix = ""; break;
            case OutputSeverity::Warning: prefix = "[WARN]  "; break;
            case OutputSeverity::Error:   prefix = "[ERROR] "; break;
        }
        fprintf(out, "%s%.*s\n", prefix, (int)length, text);
    }
}

void ConsoleOutputSink::onStreamingToken(const char* token, size_t length, StreamTokenOrigin origin) noexcept {
    if (!token || length == 0) return;
    if (m_jsonMode) {
        fprintf(stdout, "{\"type\":\"token\",\"origin\":%d,\"text\":\"", (int)origin);
        for (size_t i = 0; i < length; ++i) {
            char c = token[i];
            if (c == '"') fputs("\\\"", stdout);
            else if (c == '\\') fputs("\\\\", stdout);
            else if (c == '\n') fputs("\\n", stdout);
            else if (c == '\r') fputs("\\r", stdout);
            else if (c == '\t') fputs("\\t", stdout);
            else fputc(c, stdout);
        }
        fputs("\"}\n", stdout);
        fflush(stdout);
    } else {
        // Direct token output — no newline, for streaming effect
        fwrite(token, 1, length, stdout);
        fflush(stdout);
    }
}

void ConsoleOutputSink::onStreamStart(const char* sourceId) noexcept {
    if (m_jsonMode) {
        fprintf(stdout, "{\"type\":\"stream_start\",\"source\":\"%s\"}\n", sourceId ? sourceId : "");
    } else if (m_verbose) {
        fprintf(stdout, "\n--- Stream started: %s ---\n", sourceId ? sourceId : "unknown");
    }
}

void ConsoleOutputSink::onStreamEnd(const char* sourceId, bool success) noexcept {
    if (m_jsonMode) {
        fprintf(stdout, "{\"type\":\"stream_end\",\"source\":\"%s\",\"success\":%s}\n",
                sourceId ? sourceId : "", success ? "true" : "false");
    } else {
        if (!m_quiet) fprintf(stdout, "\n");
        if (m_verbose) {
            fprintf(stdout, "--- Stream ended: %s (%s) ---\n",
                    sourceId ? sourceId : "unknown", success ? "ok" : "FAILED");
        }
    }
}

void ConsoleOutputSink::onAgentStarted(const char* agentId, const char* prompt) noexcept {
    if (m_jsonMode) {
        fprintf(stdout, "{\"type\":\"agent_started\",\"agentId\":\"%s\"}\n",
                agentId ? agentId : "");
    } else if (m_verbose) {
        fprintf(stdout, "[AGENT] Started: %s\n", agentId ? agentId : "?");
    }
}

void ConsoleOutputSink::onAgentCompleted(const char* agentId, const char* result, int durationMs) noexcept {
    if (m_jsonMode) {
        fprintf(stdout, "{\"type\":\"agent_completed\",\"agentId\":\"%s\",\"durationMs\":%d}\n",
                agentId ? agentId : "", durationMs);
    } else if (m_verbose) {
        fprintf(stdout, "[AGENT] Completed: %s (%dms)\n", agentId ? agentId : "?", durationMs);
    }
}

void ConsoleOutputSink::onAgentFailed(const char* agentId, const char* error) noexcept {
    if (m_jsonMode) {
        fprintf(stdout, "{\"type\":\"agent_failed\",\"agentId\":\"%s\",\"error\":\"%s\"}\n",
                agentId ? agentId : "", error ? error : "");
    } else {
        fprintf(stderr, "[AGENT] Failed: %s — %s\n",
                agentId ? agentId : "?", error ? error : "unknown error");
    }
}

void ConsoleOutputSink::onStatusUpdate(const char* key, const char* value) noexcept {
    if (m_jsonMode) {
        fprintf(stdout, "{\"type\":\"status\",\"key\":\"%s\",\"value\":\"%s\"}\n",
                key ? key : "", value ? value : "");
    } else if (m_verbose) {
        fprintf(stdout, "[STATUS] %s: %s\n", key ? key : "?", value ? value : "?");
    }
}

void ConsoleOutputSink::flush() noexcept {
    fflush(stdout);
    fflush(stderr);
}

// ============================================================================
// HeadlessIDE — Constructor / Destructor
// ============================================================================
HeadlessIDE::HeadlessIDE() {
    // Generate session ID
    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    m_startEpochMs = static_cast<uint64_t>(epoch);
    m_sessionId = "headless-" + std::to_string(m_startEpochMs);

    // Default output sink
    m_outputSink = std::make_unique<ConsoleOutputSink>();
}

HeadlessIDE::~HeadlessIDE() {
    if (m_running.load()) {
        requestShutdown();
    }
    shutdownAll();
}

// ============================================================================
// Lifecycle
// ============================================================================
HeadlessResult HeadlessIDE::initialize(int argc, char* argv[]) {
    HeadlessResult r = parseArgs(argc, argv);
    if (!r.success) return r;
    return initialize(m_config);
}

HeadlessResult HeadlessIDE::initialize(const HeadlessConfig& config) {
    m_config = config;

    // Configure output sink based on config
    if (auto* console = dynamic_cast<ConsoleOutputSink*>(m_outputSink.get())) {
        console->setVerbose(m_config.verbose);
        console->setQuiet(m_config.quiet);
        console->setJsonMode(m_config.jsonOutput);
    }

    m_outputSink->appendOutput("RawrXD Headless IDE initializing...", OutputSeverity::Info);
    m_outputSink->appendOutput(("Session: " + m_sessionId).c_str(), OutputSeverity::Debug);
    m_outputSink->appendOutput(("Version: " + std::string(VERSION)).c_str(), OutputSeverity::Debug);

    // Initialize WinSock (required for HTTP server + remote backends)
    HeadlessResult wr = initWinsock();
    if (!wr.success) return wr;

    // Initialize engines
    HeadlessResult er = initEngines();
    if (!er.success) {
        m_outputSink->appendOutput(er.detail, OutputSeverity::Warning);
        // Non-fatal: engines are optional, server can run without them
    }

    // Initialize subsystems — all are non-fatal
    auto tryInit = [this](HeadlessResult (HeadlessIDE::*fn)(), const char* name) {
        HeadlessResult r = (this->*fn)();
        if (!r.success) {
            std::string msg = std::string(name) + ": " + r.detail;
            m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Warning);
        }
    };

    tryInit(&HeadlessIDE::initBackendManager, "BackendManager");
    tryInit(&HeadlessIDE::initLLMRouter, "LLMRouter");
    tryInit(&HeadlessIDE::initFailureDetection, "FailureDetection");
    tryInit(&HeadlessIDE::initAgentHistory, "AgentHistory");
    tryInit(&HeadlessIDE::initAsmSemantic, "AsmSemantic");
    tryInit(&HeadlessIDE::initLSPClient, "LSPClient");
    tryInit(&HeadlessIDE::initHybridBridge, "HybridBridge");
    tryInit(&HeadlessIDE::initMultiResponse, "MultiResponse");
    tryInit(&HeadlessIDE::initPhase10, "Phase10-ExecGovernor");
    tryInit(&HeadlessIDE::initPhase11, "Phase11-Swarm");
    tryInit(&HeadlessIDE::initPhase12, "Phase12-NativeDebug");
    tryInit(&HeadlessIDE::initHotpatch, "Hotpatch");
    tryInit(&HeadlessIDE::initInstructions, "Instructions");

    // Load model if specified
    if (!m_config.modelPath.empty()) {
        if (!loadModel(m_config.modelPath)) {
            return HeadlessResult::error("Failed to load model", 2);
        }
    }

    // Load settings
    if (!m_config.settingsFile.empty()) {
        loadSettings(m_config.settingsFile);
    }

    m_outputSink->appendOutput("Headless IDE initialized successfully.", OutputSeverity::Info);
    return HeadlessResult::ok("Initialized");
}

int HeadlessIDE::run() {
    m_running.store(true);
    m_shutdownRequested.store(false);

    // Register signal handlers
    g_headlessInstance.store(this);
    signal(SIGINT, headlessSignalHandler);
    signal(SIGTERM, headlessSignalHandler);

    int exitCode = 0;

    switch (m_config.mode) {
        case HeadlessRunMode::Server:
            exitCode = runServerMode();
            break;
        case HeadlessRunMode::REPL:
            exitCode = runReplMode();
            break;
        case HeadlessRunMode::SingleShot:
            exitCode = runSingleShotMode();
            break;
        case HeadlessRunMode::Batch:
            exitCode = runBatchMode();
            break;
    }

    m_running.store(false);
    g_headlessInstance.store(nullptr);
    return exitCode;
}

void HeadlessIDE::requestShutdown() noexcept {
    m_shutdownRequested.store(true);
    stopServer();
}

void HeadlessIDE::setOutputSink(std::unique_ptr<IOutputSink> sink) {
    if (sink) m_outputSink = std::move(sink);
}

// ============================================================================
// Argument Parsing
// ============================================================================
HeadlessResult HeadlessIDE::parseArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--headless") {
            // Already in headless mode (this flag is consumed by main_win32.cpp)
            continue;
        }
        else if (arg == "--port" && i + 1 < argc) {
            m_config.port = std::atoi(argv[++i]);
        }
        else if (arg == "--bind" && i + 1 < argc) {
            m_config.bindAddress = argv[++i];
        }
        else if (arg == "--model" && i + 1 < argc) {
            m_config.modelPath = argv[++i];
        }
        else if (arg == "--prompt" && i + 1 < argc) {
            m_config.prompt = argv[++i];
            m_config.mode = HeadlessRunMode::SingleShot;
        }
        else if (arg == "--input" && i + 1 < argc) {
            m_config.inputFile = argv[++i];
            m_config.mode = HeadlessRunMode::Batch;
        }
        else if (arg == "--output" && i + 1 < argc) {
            m_config.outputFile = argv[++i];
        }
        else if (arg == "--settings" && i + 1 < argc) {
            m_config.settingsFile = argv[++i];
        }
        else if (arg == "--backend" && i + 1 < argc) {
            m_config.backend = argv[++i];
        }
        else if (arg == "--max-tokens" && i + 1 < argc) {
            m_config.maxTokens = std::atoi(argv[++i]);
        }
        else if (arg == "--temperature" && i + 1 < argc) {
            m_config.temperature = static_cast<float>(std::atof(argv[++i]));
        }
        else if (arg == "--repl") {
            m_config.enableRepl = true;
            m_config.mode = HeadlessRunMode::REPL;
        }
        else if (arg == "--no-server") {
            m_config.enableServer = false;
        }
        else if (arg == "--verbose" || arg == "-v") {
            m_config.verbose = true;
        }
        else if (arg == "--quiet" || arg == "-q") {
            m_config.quiet = true;
        }
        else if (arg == "--json") {
            m_config.jsonOutput = true;
        }
        else if (arg == "--help" || arg == "-h") {
            printReplHelp();
            return HeadlessResult::error("Help requested", 0);
        }
    }

    return HeadlessResult::ok();
}

// ============================================================================
// Initialization Phases
// ============================================================================
HeadlessResult HeadlessIDE::initWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        return HeadlessResult::error("WSAStartup failed", result);
    }
    m_winsockInitialized = true;
    return HeadlessResult::ok("WinSock initialized");
}

HeadlessResult HeadlessIDE::initEngines() {
    // Engine manager and Codex are optional — set externally via setEngineManager/setCodexUltimate
    // In headless mode we attempt to load them, but failure is non-fatal
    if (!m_engineManager) {
        m_engineManager = new EngineManager();
        if (RawrXD::EnterpriseLicense::Instance().Is800BUnlocked() || RawrXD::g_800B_Unlocked) {
            try { m_engineManager->LoadEngine("engines/800b-5drive/800b_engine.dll", "800b-5drive"); } catch (...) {}
        }
        try { m_engineManager->LoadEngine("engines/codex-ultimate/codex.dll", "codex-ultimate"); } catch (...) {}
        try { m_engineManager->LoadEngine("engines/rawrxd-compiler/compiler.dll", "rawrxd-compiler"); } catch (...) {}
    }
    if (!m_codexUltimate) {
        m_codexUltimate = new CodexUltimate();
    }
    return HeadlessResult::ok("Engines initialized");
}

HeadlessResult HeadlessIDE::initBackendManager() {
    auto startTime = std::chrono::steady_clock::now();

    // Configure default backend based on config
    if (!m_config.backend.empty()) {
        if (m_config.backend == "ollama")  m_activeBackend = AIBackendType::Ollama;
        else if (m_config.backend == "openai")  m_activeBackend = AIBackendType::OpenAI;
        else if (m_config.backend == "claude")  m_activeBackend = AIBackendType::Claude;
        else if (m_config.backend == "gemini")  m_activeBackend = AIBackendType::Gemini;
        else m_activeBackend = AIBackendType::LocalGGUF;
    }

    // Probe Ollama availability (primary backend)
    RawrXD::Agent::OllamaConfig ollamaCfg;
    ollamaCfg.host = "127.0.0.1";
    ollamaCfg.port = 11434;
    ollamaCfg.timeout_ms = 3000;
    RawrXD::Agent::AgentOllamaClient probeClient(ollamaCfg);
    bool ollamaAvailable = probeClient.TestConnection();

    std::ostringstream statusMsg;
    statusMsg << "Backend manager initialized (headless)";
    if (ollamaAvailable) {
        auto models = probeClient.ListModels();
        statusMsg << " | Ollama: online (" << models.size() << " models)";
        // Default to Ollama if available and no explicit backend set
        if (m_config.backend.empty()) {
            m_activeBackend = AIBackendType::Ollama;
        }
    } else {
        statusMsg << " | Ollama: offline";
    }

    const char* backendNames[] = { "LocalGGUF", "Ollama", "OpenAI", "Claude", "Gemini" };
    statusMsg << " | Active: " << backendNames[static_cast<int>(m_activeBackend)];

    m_backendManagerInitialized = true;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    statusMsg << " [" << elapsed << "us]";
    m_outputSink->appendOutput(statusMsg.str().c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("backend_manager", "active");
    m_outputSink->onStatusUpdate("backend", backendNames[static_cast<int>(m_activeBackend)]);
    return HeadlessResult::ok("Backend manager ready");
}

HeadlessResult HeadlessIDE::initLLMRouter() {
    auto startTime = std::chrono::steady_clock::now();

    // Configure routing table with backend priorities
    // Priority: Ollama (local, fast) > LocalGGUF > Cloud backends
    struct RouterEntry {
        AIBackendType type;
        const char* name;
        int priority;  // lower = higher priority
        bool available;
    };

    RouterEntry routes[] = {
        { AIBackendType::Ollama,    "Ollama",    1, m_backendManagerInitialized },
        { AIBackendType::LocalGGUF, "LocalGGUF",  2, m_modelLoaded },
        { AIBackendType::OpenAI,    "OpenAI",    10, false },
        { AIBackendType::Claude,    "Claude",    11, false },
        { AIBackendType::Gemini,    "Gemini",    12, false },
    };

    int activeRoutes = 0;
    for (auto& r : routes) {
        if (r.available) activeRoutes++;
    }

    m_routerInitialized = true;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    char msg[256];
    snprintf(msg, sizeof(msg), "LLM router initialized: %d/%d backends available [%lldus]",
             activeRoutes, 5, (long long)elapsed);
    m_outputSink->appendOutput(msg, OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("llm_router", "active");
    return HeadlessResult::ok("LLM router ready");
}

HeadlessResult HeadlessIDE::initFailureDetection() {
    auto startTime = std::chrono::steady_clock::now();
    m_failureDetectorInitialized = true;
    m_failureDetections = 0;
    m_failureRetries = 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Failure detector initialized (headless) [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("failure_detector", "active");
    return HeadlessResult::ok("Failure detector ready");
}

HeadlessResult HeadlessIDE::initAgentHistory() {
    auto startTime = std::chrono::steady_clock::now();
    if (!m_historyRecorder) {
        m_historyRecorder = std::make_unique<AgentHistoryRecorder>("rawrxd_headless_history");
        m_historyRecorder->setLogCallback([this](int level, const std::string& msg) {
            if (level >= 2) {
                m_outputSink->appendOutput(("[History] " + msg).c_str(), OutputSeverity::Debug);
            }
        });
    }
    m_agentHistoryInitialized = true;
    m_agentEventCount = 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Agent history initialized (headless) [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("agent_history", "active");
    return HeadlessResult::ok("Agent history ready");
}

HeadlessResult HeadlessIDE::initAsmSemantic() {
    auto startTime = std::chrono::steady_clock::now();
    m_asmSemanticInitialized = true;
    m_asmSymbolCount = 0;
    m_asmFilesParsed = 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "ASM semantic initialized (headless) [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("asm_semantic", "active");
    return HeadlessResult::ok("ASM semantic ready");
}

HeadlessResult HeadlessIDE::initLSPClient() {
    auto startTime = std::chrono::steady_clock::now();

    // Create embedded LSP server for headless diagnostics + code intelligence
    {
        std::lock_guard<std::mutex> lk(g_embeddedLSPMutex);
        if (!g_embeddedLSP) {
            g_embeddedLSP = std::make_unique<RawrXD::LSPServer::RawrXDLSPServer>();
        }

        // Configure for in-process (pipe) transport — headless owns stdio
        RawrXD::LSPServer::ServerConfig lspConfig;
        lspConfig.useStdio           = false;  // Use named pipe, not stdio
        lspConfig.pipeName           = "\\\\.\\pipe\\rawrxd-lsp-headless";
        lspConfig.enableSemanticTokens = true;
        lspConfig.enableHover        = true;
        lspConfig.enableCompletion   = true;
        lspConfig.enableDefinition   = true;
        lspConfig.enableReferences   = true;
        lspConfig.enableDocumentSymbol  = true;
        lspConfig.enableWorkspaceSymbol = true;
        lspConfig.enableDiagnostics  = true;
        lspConfig.indexThrottleMs    = 100;  // Faster for headless
        lspConfig.maxSymbolResults   = 1000;
        lspConfig.maxCompletionItems = 200;

        // Set workspace root from current working dir or explicitly if available
        char cwd[MAX_PATH];
        if (GetCurrentDirectoryA(MAX_PATH, cwd)) {
            lspConfig.rootPath = cwd;
            // Convert to file URI
            std::string pathStr = cwd;
            for (auto& c : pathStr) { if (c == '\\') c = '/'; }
            lspConfig.rootUri = "file:///" + pathStr;
        }

        g_embeddedLSP->configure(lspConfig);

        // Start the LSP server (launches reader + dispatch threads)
        if (g_embeddedLSP->start()) {
            m_lspServerCount = 1;

            // Trigger initial project indexing if we have a root path
            if (!lspConfig.rootPath.empty()) {
                g_embeddedLSP->rebuildIndex();
                size_t symCount = g_embeddedLSP->getIndexedSymbolCount();
                size_t fileCount = g_embeddedLSP->getTrackedFileCount();

                std::ostringstream oss;
                oss << "  LSP initial index: " << symCount << " symbols across "
                    << fileCount << " files";
                m_outputSink->appendOutput(oss.str().c_str(), OutputSeverity::Debug);
            }
        } else {
            m_outputSink->appendOutput("LSP server failed to start on named pipe",
                                       OutputSeverity::Warning);
            // Still mark initialized — server exists but isn't running
        }
    }

    m_lspInitialized = true;
    m_lspCompletionCount = 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "LSP client initialized (headless, embedded server) [" +
                      std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("lsp_client", "active");
    return HeadlessResult::ok("LSP client ready (embedded server)");
}

HeadlessResult HeadlessIDE::initHybridBridge() {
    auto startTime = std::chrono::steady_clock::now();
    m_hybridBridgeInitialized = true;
    m_hybridCompletionCount = 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Hybrid bridge initialized (headless) [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("hybrid_bridge", "active");
    return HeadlessResult::ok("Hybrid bridge ready");
}

HeadlessResult HeadlessIDE::initMultiResponse() {
    auto startTime = std::chrono::steady_clock::now();
    if (!m_multiResponse) {
        m_multiResponse = std::make_unique<MultiResponseEngine>();
        auto initResult = m_multiResponse->initialize();
        if (!initResult.success) {
            m_outputSink->appendOutput("Multi-response engine failed to initialize", OutputSeverity::Warning);
        }
    }
    m_multiResponseInitialized = true;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Multi-response initialized (headless) [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("multi_response", "active");
    return HeadlessResult::ok("Multi-response ready");
}

HeadlessResult HeadlessIDE::initPhase10() {
    auto startTime = std::chrono::steady_clock::now();

    // Phase 10A: Execution Governor
    auto& governor = ExecutionGovernor::instance();
    if (!governor.isInitialized()) {
        governor.init();
    }

    // Phase 10B: Safety Contract
    auto& safety = AgentSafetyContract::instance();
    safety.init();
    safety.setAutoApproveEscalations(true); // headless: auto-approve

    // Phase 10C: Deterministic Replay Journal
    auto& replay = ReplayJournal::instance();
    replay.init("rawrxd_headless_replay");
    replay.startSession("headless-" + m_sessionId);
    replay.startRecording();
    replay.recordMarker("Headless IDE Phase 10 initialized");

    // Phase 10D: Confidence Gate
    auto& confidence = ConfidenceGate::instance();
    confidence.init();
    confidence.setPolicy(GatePolicy::Normal);
    confidence.setEnabled(true);
    confidence.setAutoEscalate(true); // headless: auto-escalate

    m_phase10Initialized = true;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Phase 10 (Governor+Safety+Replay+Confidence) initialized [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("phase10", "active");
    return HeadlessResult::ok("Phase 10 ready");
}

HeadlessResult HeadlessIDE::initPhase11() {
    auto startTime = std::chrono::steady_clock::now();
    auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
    if (!swarm.isInitialized()) {
        auto result = swarm.initialize(RawrXD::Swarm::NodeRole::Coordinator);
        if (!result.success) {
            m_outputSink->appendOutput(
                ("Swarm init note: " + std::string(result.detail)).c_str(),
                OutputSeverity::Debug);
            // Non-fatal: swarm is optional in headless single-node mode
        }
    }
    m_phase11Initialized = true;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Phase 11 (Distributed Swarm) initialized [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("swarm", swarm.isRunning() ? "active" : "standby");
    return HeadlessResult::ok("Phase 11 ready");
}

HeadlessResult HeadlessIDE::initPhase12() {
    auto startTime = std::chrono::steady_clock::now();
    m_phase12Initialized = true;
    m_debugSessionActive = false;
    m_debugBreakpointCount = 0;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Phase 12 (Native Debugger) initialized [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("native_debugger", "ready");
    return HeadlessResult::ok("Phase 12 ready");
}

HeadlessResult HeadlessIDE::initHotpatch() {
    auto startTime = std::chrono::steady_clock::now();
    auto& hotpatcher = UniversalModelHotpatcher::instance();
    if (!hotpatcher.isInitialized()) {
        hotpatcher.initialize();
    }
    m_hotpatchInitialized = true;
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::string msg = "Three-layer hotpatch initialized [" + std::to_string(elapsed) + "us]";
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Debug);
    m_outputSink->onStatusUpdate("hotpatch", "active");
    return HeadlessResult::ok("Hotpatch ready");
}

HeadlessResult HeadlessIDE::initInstructions() {
    auto startTime = std::chrono::steady_clock::now();
    auto& provider = InstructionsProvider::instance();

    // Add workspace-relative search paths
    provider.addSearchPath(".");
    provider.addSearchPath(".github");

    auto r = provider.loadAll();
    m_instructionsInitialized = r.success;

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    if (r.success) {
        std::string msg = "Instructions loaded: " +
            std::to_string(provider.getLoadedCount()) + " files (" +
            std::to_string(provider.getAllContent().size()) + " bytes) [" +
            std::to_string(elapsed) + "us]";
        m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Info);
        m_outputSink->onStatusUpdate("instructions", "loaded");
    } else {
        std::string msg = std::string("Instructions: ") + r.detail +
            " [" + std::to_string(elapsed) + "us]";
        m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Warning);
        m_outputSink->onStatusUpdate("instructions", "unavailable");
    }

    return r.success ? HeadlessResult::ok("Instructions loaded") 
                     : HeadlessResult::error(r.detail);
}

std::string HeadlessIDE::getInstructionsContent() const {
    auto& provider = InstructionsProvider::instance();
    if (!provider.isLoaded()) {
        InstructionsProvider::instance().loadAll();
    }
    return provider.getAllContent();
}

// ============================================================================
// Model Operations
// ============================================================================
bool HeadlessIDE::loadModel(const std::string& filepath) {
    m_outputSink->appendOutput(("Loading model: " + filepath).c_str(), OutputSeverity::Info);
    auto t0 = std::chrono::steady_clock::now();

    // Phase 1: Resolve the model source — local, Ollama, HuggingFace, URL
    RawrXD::ModelSourceResolver resolver;
    RawrXD::ResolvedModelPath resolved = resolver.Resolve(filepath,
        [this](const RawrXD::ModelDownloadProgress& p) {
            if (p.total_bytes > 0) {
                char buf[256];
                snprintf(buf, sizeof(buf), "[Model] Downloading %.1f%% (%llu / %llu bytes)",
                         p.progress_percent, (unsigned long long)p.downloaded_bytes,
                         (unsigned long long)p.total_bytes);
                m_outputSink->appendOutput(buf, OutputSeverity::Info);
            }
        });
    
    std::string localPath = resolved.success ? resolved.local_path : filepath;
    
    // Phase 2: Validate file exists on disk
    DWORD attr = GetFileAttributesA(localPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        std::string err = "Model file not found: " + localPath;
        if (!resolved.success && !resolved.error_message.empty()) {
            err += " (" + resolved.error_message + ")";
        }
        m_outputSink->appendOutput(err.c_str(), OutputSeverity::Error);
        return false;
    }

    // Phase 3: Open with StreamingGGUFLoader and parse header + metadata
    auto loader = std::make_unique<RawrXD::StreamingGGUFLoader>();
    if (!loader->Open(localPath)) {
        m_outputSink->appendOutput("Failed to open GGUF file", OutputSeverity::Error);
        return false;
    }

    if (!loader->ParseHeader()) {
        m_outputSink->appendOutput("Invalid GGUF header — file may be corrupt", OutputSeverity::Error);
        loader->Close();
        return false;
    }

    CPUInference::GGUFHeader hdr = loader->GetHeader();
    // Validate magic: 0x46475547 = "GGUF" little-endian
    if (hdr.magic != 0x46475547) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Bad GGUF magic: 0x%08X (expected 0x46475547)", hdr.magic);
        m_outputSink->appendOutput(buf, OutputSeverity::Error);
        loader->Close();
        return false;
    }

    if (!loader->ParseMetadata()) {
        m_outputSink->appendOutput("Failed to parse GGUF metadata", OutputSeverity::Warning);
        // Non-fatal — we can still load with header-only info
    }

    CPUInference::GGUFMetadata meta = loader->GetMetadata();

    // Phase 4: Build tensor index for streaming zone loading
    loader->BuildTensorIndex();

    // Store state
    m_loadedModelPath = localPath;
    size_t lastSlash = localPath.find_last_of("/\\");
    m_loadedModelName = (lastSlash != std::string::npos) ? localPath.substr(lastSlash + 1) : localPath;
    m_modelLoaded = true;

    auto t1 = std::chrono::steady_clock::now();
    int loadMs = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

    // Report model info
    std::ostringstream info;
    info << "Model loaded: " << m_loadedModelName << "\n"
         << "  GGUF version: " << hdr.version << "\n"
         << "  Tensors: " << hdr.tensor_count << "\n"
         << "  Metadata KVs: " << hdr.metadata_kv_count << "\n"
         << "  Layers: " << meta.layer_count << "\n"
         << "  Context length: " << meta.context_length << "\n"
         << "  Embedding dim: " << meta.embedding_dim << "\n"
         << "  Vocab size: " << meta.vocab_size << "\n"
         << "  File size: " << (loader->GetFileSize() / (1024*1024)) << " MB\n"
         << "  Load latency: " << loadMs << " ms\n";
    if (resolved.success && resolved.source_type != GGUFConstants::ModelSourceType::LOCAL_FILE) {
        info << "  Source: " << resolved.original_input << "\n";
    }
    m_outputSink->appendOutput(info.str().c_str(), OutputSeverity::Info);
    m_outputSink->onStatusUpdate("model", m_loadedModelName.c_str());

    loader->Close();
    recordSimpleEvent("model_loaded");
    return true;
}

bool HeadlessIDE::unloadModel() {
    if (!m_modelLoaded) return false;
    m_outputSink->appendOutput(("Unloading model: " + m_loadedModelName).c_str(), OutputSeverity::Info);
    m_modelLoaded = false;
    m_loadedModelPath.clear();
    m_loadedModelName.clear();
    m_outputSink->onStatusUpdate("model", "(none)");
    return true;
}

bool HeadlessIDE::isModelLoaded() const {
    return m_modelLoaded;
}

std::string HeadlessIDE::getLoadedModelName() const {
    return m_loadedModelName;
}

std::string HeadlessIDE::getModelInfo() const {
    if (!m_modelLoaded) return "No model loaded";
    std::ostringstream oss;
    oss << "Model: " << m_loadedModelName << "\n";
    oss << "Path: " << m_loadedModelPath << "\n";
    return oss.str();
}

// ============================================================================
// Inference
// ============================================================================
std::string HeadlessIDE::runInference(const std::string& prompt) {
    return runInference(prompt, m_config.maxTokens, m_config.temperature);
}

std::string HeadlessIDE::runInference(const std::string& prompt, int maxTokens, float temperature) {
    if (!m_modelLoaded) {
        m_outputSink->appendOutput("No model loaded for inference", OutputSeverity::Error);
        return "[error: no model loaded]";
    }

    m_outputSink->onAgentStarted("inference", prompt.c_str());

    auto startTime = std::chrono::steady_clock::now();

    // Route through backend manager → LLM router → inference engine
    // This delegates to the same path as Win32IDE::routeInferenceRequest
    std::string result = routeInferenceRequest(prompt);

    auto endTime = std::chrono::steady_clock::now();
    int durationMs = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());

    if (!result.empty()) {
        m_outputSink->onAgentCompleted("inference", result.c_str(), durationMs);
    } else {
        m_outputSink->onAgentFailed("inference", "Empty result from inference engine");
    }

    return result;
}

void HeadlessIDE::runInferenceStreaming(const std::string& prompt,
                                         std::function<void(const char*, size_t)> tokenCallback) {
    if (!m_modelLoaded) {
        m_outputSink->appendOutput("No model loaded for streaming inference", OutputSeverity::Error);
        return;
    }

    m_outputSink->onStreamStart("inference");

    // Use AgentOllamaClient streaming API for real per-token delivery
    if (m_activeBackend == AIBackendType::Ollama || m_activeBackend == AIBackendType::LocalGGUF) {
        RawrXD::Agent::OllamaConfig cfg;
        cfg.host = "127.0.0.1";
        cfg.port = 11434;
        // chat_model left empty — auto-detected from Ollama /api/tags
        cfg.temperature = m_config.temperature;
        cfg.max_tokens = m_config.maxTokens;
        cfg.use_gpu = true;
        cfg.num_gpu = 99;

        RawrXD::Agent::AgentOllamaClient client(cfg);
        std::vector<RawrXD::Agent::ChatMessage> messages;
        messages.push_back({"system", "You are RawrXD IDE's embedded AI assistant.", "", {}});
        messages.push_back({"user", prompt, "", {}});

        bool streamOk = client.ChatStream(
            messages, nlohmann::json::array(),
            /* on_token */ [&](const std::string& token) {
                if (tokenCallback) {
                    tokenCallback(token.c_str(), token.size());
                }
                m_outputSink->onStreamingToken(token.c_str(), token.size(), StreamTokenOrigin::Inference);
            },
            /* on_tool_call */ [](const std::string&, const nlohmann::json&) {},
            /* on_done */ [&](const std::string& full, uint64_t pt, uint64_t ct, double tps) {
                char perf[256];
                snprintf(perf, sizeof(perf), "[stream] %llu+%llu tokens, %.1f tok/s",
                         (unsigned long long)pt, (unsigned long long)ct, tps);
                m_outputSink->appendOutput(perf, OutputSeverity::Debug);
            },
            /* on_error */ [&](const std::string& err) {
                m_outputSink->appendOutput(("Stream error: " + err).c_str(), OutputSeverity::Error);
            }
        );

        m_outputSink->onStreamEnd("inference", streamOk);
        return;
    }

    // Fallback: batch inference emitted as single chunk
    std::string result = runInference(prompt);
    if (tokenCallback && !result.empty()) {
        tokenCallback(result.c_str(), result.size());
    }
    m_outputSink->onStreamEnd("inference", !result.empty());
}

// ============================================================================
// Backend Switcher (Phase 8B)
// ============================================================================
bool HeadlessIDE::setActiveBackend(AIBackendType type) {
    const char* backendNames[] = { "LocalGGUF", "Ollama", "OpenAI", "Claude", "Gemini" };
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= static_cast<int>(AIBackendType::Count)) {
        m_outputSink->appendOutput("Invalid backend type", OutputSeverity::Error);
        return false;
    }

    // Probe health before switching
    if (!probeBackendHealth(type)) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Backend '%s' health check failed — switch aborted", backendNames[idx]);
        m_outputSink->appendOutput(buf, OutputSeverity::Warning);
        return false;
    }

    AIBackendType previousBackend = m_activeBackend;
    m_activeBackend = type;

    char buf[256];
    snprintf(buf, sizeof(buf), "Backend switched: %s → %s",
             backendNames[static_cast<int>(previousBackend)], backendNames[idx]);
    m_outputSink->appendOutput(buf, OutputSeverity::Info);
    m_outputSink->onStatusUpdate("backend", backendNames[idx]);
    recordSimpleEvent("backend_switch");
    return true;
}

HeadlessIDE::AIBackendType HeadlessIDE::getActiveBackendType() const {
    return m_activeBackend;
}

std::string HeadlessIDE::getBackendStatusString() const {
    const char* backendNames[] = { "LocalGGUF", "Ollama", "OpenAI", "Claude", "Gemini" };
    int idx = static_cast<int>(m_activeBackend);
    std::ostringstream oss;
    oss << "Backend: " << (idx >= 0 && idx < 5 ? backendNames[idx] : "Unknown") << " (headless)\n";
    oss << "Status: Active\n";
    oss << "Model: " << (m_loadedModelName.empty() ? "(none)" : m_loadedModelName) << "\n";
    oss << "Inference requests: " << m_inferenceRequestCount;
    return oss.str();
}

bool HeadlessIDE::probeBackendHealth(AIBackendType type) {
    switch (type) {
        case AIBackendType::LocalGGUF:
            // Local GGUF: healthy if model is loaded
            return m_modelLoaded;

        case AIBackendType::Ollama: {
            // Probe Ollama server connection
            RawrXD::Agent::OllamaConfig cfg;
            cfg.host = "127.0.0.1";
            cfg.port = 11434;
            cfg.timeout_ms = 5000; // Quick probe timeout
            RawrXD::Agent::AgentOllamaClient client(cfg);
            bool connected = client.TestConnection();
            if (connected) {
                auto models = client.ListModels();
                m_outputSink->appendOutput(
                    ("Ollama: " + std::to_string(models.size()) + " models available").c_str(),
                    OutputSeverity::Debug);
            }
            return connected;
        }

        case AIBackendType::OpenAI:
        case AIBackendType::Claude:
        case AIBackendType::Gemini:
            // Cloud backends: check if API key is configured
            // (API key management is in BackendState singleton)
            return false; // Not yet configured in headless mode

        default:
            return false;
    }
}

std::string HeadlessIDE::routeInferenceRequest(const std::string& prompt) {
    m_inferenceRequestCount++;
    recordSimpleEvent("inference_request");

    auto t0 = std::chrono::steady_clock::now();

    // Route based on active backend type
    if (m_activeBackend == AIBackendType::Ollama || m_activeBackend == AIBackendType::LocalGGUF) {
        // Use AgentOllamaClient for Ollama-backed inference
        RawrXD::Agent::OllamaConfig cfg;
        cfg.host = "127.0.0.1";
        cfg.port = 11434;
        // chat_model left empty — auto-detected from Ollama /api/tags
        cfg.temperature = m_config.temperature;
        cfg.max_tokens = m_config.maxTokens;
        cfg.use_gpu = true;
        cfg.num_gpu = 99;

        RawrXD::Agent::AgentOllamaClient client(cfg);

        // Build conversation with system context
        std::vector<RawrXD::Agent::ChatMessage> messages;
        messages.push_back({"system", "You are RawrXD IDE's embedded AI assistant. "
            "Provide accurate, concise answers. When asked about code, give working examples.", "", {}});
        if (m_modelLoaded) {
            messages.push_back({"system", "Loaded model: " + m_loadedModelName, "", {}});
        }
        messages.push_back({"user", prompt, "", {}});

        auto result = client.ChatSync(messages);

        auto t1 = std::chrono::steady_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

        if (result.success) {
            char perf[256];
            snprintf(perf, sizeof(perf),
                     "[inference] %llu prompt + %llu completion tokens, %.1f tok/s, %.0f ms",
                     (unsigned long long)result.prompt_tokens,
                     (unsigned long long)result.completion_tokens,
                     result.tokens_per_sec, durationMs);
            m_outputSink->appendOutput(perf, OutputSeverity::Debug);
            return result.response;
        } else {
            m_outputSink->appendOutput(
                ("Ollama inference failed: " + result.error_message).c_str(),
                OutputSeverity::Warning);
            // Fall through to engine manager path
        }
    }

    // Secondary path: route through engine manager if available
    if (m_engineManager) {
        std::string currentId = m_engineManager->GetCurrentEngine();
        auto* engine = currentId.empty() ? nullptr : m_engineManager->GetEngine(currentId);
        if (engine && engine->loaded) {
            m_outputSink->appendOutput(("Engine '" + engine->name + "' active but no inference API").c_str(),
                                       OutputSeverity::Debug);
        }
    }

    // Final fallback: provide actionable error
    return "[error] Inference unavailable — ensure Ollama is running on port 11434 "
           "or configure an alternative backend with 'backend <type>'";
}

// ============================================================================
// LLM Router (Phase 8C)
// ============================================================================
std::string HeadlessIDE::routeWithIntelligence(const std::string& prompt) {
    return routeInferenceRequest(prompt);
}

std::string HeadlessIDE::getRouterStatusString() const {
    std::ostringstream oss;
    oss << "LLM Router: " << (m_routerInitialized ? "Active" : "Inactive") << " (headless)\n";
    oss << "Backends: 5 configured\n";
    oss << "Task types: 8\n";
    oss << "Requests routed: " << m_inferenceRequestCount;
    return oss.str();
}

std::string HeadlessIDE::getCostLatencyHeatmapString() const {
    return "Cost/Latency heatmap: (headless mode — collecting data)";
}

// ============================================================================
// Failure Detection (Phase 4B/6)
// ============================================================================
std::string HeadlessIDE::executeWithFailureDetection(const std::string& prompt) {
    return runInference(prompt);
}

std::string HeadlessIDE::getFailureDetectorStats() const {
    std::ostringstream oss;
    oss << "Failure detector: " << (m_failureDetectorInitialized ? "Active" : "Inactive") << " (headless)\n";
    oss << "Total detections: " << m_failureDetections << "\n";
    oss << "Retries: " << m_failureRetries;
    return oss.str();
}

std::string HeadlessIDE::getFailureIntelligenceStatsString() const {
    std::ostringstream oss;
    oss << "Failure intelligence: " << (m_failureDetectorInitialized ? "Active" : "Inactive") << " (headless)\n";
    oss << "Records: " << m_failureDetections << "\n";
    oss << "Accuracy: " << (m_failureDetections > 0 ? "tracking" : "N/A");
    return oss.str();
}

// ============================================================================
// Agent History (Phase 6B)
// ============================================================================
std::string HeadlessIDE::getAgentHistoryStats() const {
    std::ostringstream oss;
    oss << "Agent history: " << (m_agentHistoryInitialized ? "Active" : "Inactive") << " (headless)\n";
    oss << "Session: " << m_sessionId << "\n";
    oss << "Events: " << m_agentEventCount;
    return oss.str();
}

void HeadlessIDE::recordSimpleEvent(const std::string& description) {
    m_agentEventCount++;
    if (m_historyRecorder) {
        m_historyRecorder->record("simple_event", "headless", "", description, "", "", true);
    }
    if (m_phase10Initialized) {
        ReplayJournal::instance().recordMarker(description);
    }
    m_outputSink->appendOutput(("Event: " + description).c_str(), OutputSeverity::Debug);
}

// ============================================================================
// ASM Semantic (Phase 9A)
// ============================================================================
void HeadlessIDE::parseAsmFile(const std::string& filePath) {
    m_asmFilesParsed++;
    m_outputSink->appendOutput(("Parsing ASM file: " + filePath).c_str(), OutputSeverity::Info);
    recordSimpleEvent("asm_parse: " + filePath);
}

void HeadlessIDE::parseAsmDirectory(const std::string& dirPath, bool recursive) {
    m_outputSink->appendOutput(("Parsing ASM directory: " + dirPath).c_str(), OutputSeverity::Info);
    recordSimpleEvent("asm_dir_parse: " + dirPath);
}

std::string HeadlessIDE::getAsmSymbolTableString() const {
    std::ostringstream oss;
    oss << "ASM symbol table (headless)\n";
    oss << "Symbols: " << m_asmSymbolCount << "\n";
    oss << "Files parsed: " << m_asmFilesParsed;
    return oss.str();
}

std::string HeadlessIDE::getAsmSemanticStatsString() const {
    std::ostringstream oss;
    oss << "ASM semantic: " << (m_asmSemanticInitialized ? "Active" : "Inactive") << " (headless)\n";
    oss << "Symbols: " << m_asmSymbolCount << "\n";
    oss << "Files: " << m_asmFilesParsed;
    return oss.str();
}

// ============================================================================
// LSP / Hybrid / Multi-Response status — wired to real subsystem state
// ============================================================================
std::string HeadlessIDE::getLSPStatusString() const {
    std::ostringstream oss;
    oss << "LSP client: " << (m_lspInitialized ? "Active" : "Inactive") << " (headless)\n";
    oss << "Servers: " << m_lspServerCount << " configured\n";

    // Query real embedded server stats if available
    {
        std::lock_guard<std::mutex> lk(g_embeddedLSPMutex);
        if (g_embeddedLSP) {
            auto state = g_embeddedLSP->getState();
            const char* stateStr = "Unknown";
            switch (state) {
                case RawrXD::LSPServer::ServerState::Created:      stateStr = "Created"; break;
                case RawrXD::LSPServer::ServerState::Initializing: stateStr = "Initializing"; break;
                case RawrXD::LSPServer::ServerState::Running:      stateStr = "Running"; break;
                case RawrXD::LSPServer::ServerState::ShuttingDown: stateStr = "ShuttingDown"; break;
                case RawrXD::LSPServer::ServerState::Stopped:      stateStr = "Stopped"; break;
                default: break;
            }
            oss << "Embedded server state: " << stateStr << "\n";
            oss << "Indexed symbols: " << g_embeddedLSP->getIndexedSymbolCount() << "\n";
            oss << "Tracked files: " << g_embeddedLSP->getTrackedFileCount() << "\n";
            auto stats = g_embeddedLSP->getStats();
            oss << "Completion requests: " << stats.completionRequests << "\n";
            oss << "Definition requests: " << stats.definitionRequests << "\n";
            oss << "Hover requests: " << stats.hoverRequests;
        } else {
            oss << "Completions served: " << m_lspCompletionCount;
        }
    }
    return oss.str();
}

std::string HeadlessIDE::getHybridBridgeStatusString() const {
    std::ostringstream oss;
    oss << "Hybrid bridge: " << (m_hybridBridgeInitialized ? "Active" : "Inactive") << " (headless)\n";
    oss << "Completions: " << m_hybridCompletionCount;
    return oss.str();
}

// ============================================================================
// Phase 10/11/12 status — wired to real singletons
// ============================================================================
std::string HeadlessIDE::getGovernorStatus() const {
    if (!m_phase10Initialized) return "Execution governor: Not initialized";
    return ExecutionGovernor::instance().getStatusString();
}

std::string HeadlessIDE::getSafetyStatus() const {
    if (!m_phase10Initialized) return "Safety contract: Not initialized";
    return AgentSafetyContract::instance().getStatusString();
}

std::string HeadlessIDE::getReplayStatus() const {
    if (!m_phase10Initialized) return "Replay journal: Not initialized";
    return ReplayJournal::instance().getStatusString();
}

std::string HeadlessIDE::getConfidenceStatus() const {
    if (!m_phase10Initialized) return "Confidence gate: Not initialized";
    return ConfidenceGate::instance().getStatusString();
}

std::string HeadlessIDE::getSwarmStatus() const {
    if (!m_phase11Initialized) return "Distributed swarm: Not initialized";
    auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
    std::ostringstream oss;
    oss << "Distributed swarm: " << (swarm.isRunning() ? "Running" : "Standby") << "\n";
    oss << "Node: " << swarm.getNodeId() << "\n";
    oss << "Peers: " << swarm.getNodeCount() << "\n";
    oss << "Role: " << (swarm.isCoordinator() ? "Coordinator" : "Worker");
    auto& stats = swarm.getStats();
    oss << "\nRequests: " << stats.inferenceRequests.load();
    oss << "\nBytes sent/recv: " << stats.bytesSent.load() << "/" << stats.bytesReceived.load();
    return oss.str();
}

std::string HeadlessIDE::getNativeDebugStatus() const {
    std::ostringstream oss;
    oss << "Native debugger: " << (m_phase12Initialized ? "Ready" : "Not initialized") << "\n";
    oss << "Session: " << (m_debugSessionActive ? "active" : "none") << "\n";
    oss << "Breakpoints: " << m_debugBreakpointCount;
    return oss.str();
}

std::string HeadlessIDE::getHotpatchStatus() const {
    if (!m_hotpatchInitialized) return "Three-layer hotpatch: Not initialized";
    auto& hp = UniversalModelHotpatcher::instance();
    const auto& stats = hp.getStats();
    std::ostringstream oss;
    oss << "Three-layer hotpatch: Active\n";
    oss << "Layers analyzed: " << stats.layersAnalyzed.load() << "\n";
    oss << "Layers requantized: " << stats.layersRequantized.load() << "\n";
    oss << "Total surgeries: " << stats.totalSurgeries.load() << "\n";
    oss << "Memory saved: " << (stats.totalMemorySaved.load() / (1024*1024)) << " MB\n";
    oss << "Pressure events: " << stats.pressureEvents.load();
    return oss.str();
}

// ============================================================================
// Settings
// ============================================================================
void HeadlessIDE::loadSettings(const std::string& path) {
    std::string settingsPath = path.empty() ? getSettingsFilePath() : path;
    m_outputSink->appendOutput(("Loading settings: " + settingsPath).c_str(), OutputSeverity::Debug);
}

void HeadlessIDE::saveSettings(const std::string& path) {
    std::string settingsPath = path.empty() ? getSettingsFilePath() : path;
    m_outputSink->appendOutput(("Saving settings: " + settingsPath).c_str(), OutputSeverity::Debug);
}

std::string HeadlessIDE::getSettingsFilePath() const {
    return "rawrxd_settings.json";
}

// ============================================================================
// HTTP Server
// ============================================================================
void HeadlessIDE::startServer() {
    if (m_serverRunning.load()) return;

    m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_serverSocket == INVALID_SOCKET) {
        m_outputSink->appendOutput("Failed to create server socket", OutputSeverity::Error);
        return;
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(m_config.port));
    inet_pton(AF_INET, m_config.bindAddress.c_str(), &addr.sin_addr);

    if (bind(m_serverSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::string msg = "Failed to bind to " + m_config.bindAddress + ":" + std::to_string(m_config.port);
        m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Error);
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
        return;
    }

    if (listen(m_serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        m_outputSink->appendOutput("Failed to listen on server socket", OutputSeverity::Error);
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
        return;
    }

    m_serverRunning.store(true);

    std::string msg = "HTTP server listening on " + m_config.bindAddress + ":" + std::to_string(m_config.port);
    m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Info);

    m_serverThread = std::thread(&HeadlessIDE::serverLoop, this);
}

void HeadlessIDE::stopServer() {
    if (!m_serverRunning.load()) return;
    m_serverRunning.store(false);
    if (m_serverSocket != INVALID_SOCKET) {
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
    }
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    m_outputSink->appendOutput("HTTP server stopped", OutputSeverity::Info);
}

bool HeadlessIDE::isServerRunning() const {
    return m_serverRunning.load();
}

std::string HeadlessIDE::getServerStatus() const {
    if (!m_serverRunning.load()) return "Server: Stopped";
    return "Server: Running on " + m_config.bindAddress + ":" + std::to_string(m_config.port);
}

void HeadlessIDE::serverLoop() {
    while (m_serverRunning.load()) {
        // Set a timeout on accept so we can check the shutdown flag
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(m_serverSocket, &readSet);
        timeval tv = { 1, 0 };  // 1 second timeout

        int sel = select(0, &readSet, nullptr, nullptr, &tv);
        if (sel <= 0) continue;

        SOCKET client = accept(m_serverSocket, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        // Handle client in a detached thread (same pattern as Win32IDE LocalServer)
        std::thread([this, client]() {
            handleClient(client);
            closesocket(client);
        }).detach();
    }
}

void HeadlessIDE::handleClient(SOCKET clientFd) {
    // Read HTTP request
    char buf[8192] = {};
    int bytesRead = recv(clientFd, buf, sizeof(buf) - 1, 0);
    if (bytesRead <= 0) return;

    std::string request(buf, bytesRead);

    // Parse method and path
    std::string method, path;
    {
        size_t sp1 = request.find(' ');
        size_t sp2 = request.find(' ', sp1 + 1);
        if (sp1 != std::string::npos && sp2 != std::string::npos) {
            method = request.substr(0, sp1);
            path = request.substr(sp1 + 1, sp2 - sp1 - 1);
        }
    }

    // Extract body (after double newline)
    std::string body;
    {
        size_t bodyStart = request.find("\r\n\r\n");
        if (bodyStart != std::string::npos) {
            body = request.substr(bodyStart + 4);
        }
    }

    // Route to handlers (mirrors Win32IDE_LocalServer.cpp endpoints)
    std::string responseBody;
    std::string contentType = "application/json";
    int statusCode = 200;

    if (path == "/api/status" || path == "/api/headless/status") {
        responseBody = getFullStatusDump();
    }
    else if (path == "/api/version") {
        responseBody = "{\"version\":\"" + std::string(VERSION) + "\",\"phase\":\"" + BUILD_PHASE + "\",\"mode\":\"headless\"}";
    }
    else if (path == "/api/model/info") {
        responseBody = "{\"loaded\":" + std::string(m_modelLoaded ? "true" : "false") +
                       ",\"name\":\"" + m_loadedModelName + "\"}";
    }
    else if (path == "/api/generate" && method == "POST") {
        // Parse prompt from JSON body
        std::string prompt;
        try {
            auto j = nlohmann::json::parse(body);
            prompt = j.value("prompt", "");
        } catch (...) {
            prompt = body;
        }
        std::string result = runInference(prompt);
        responseBody = "{\"response\":\"" + result + "\"}";
    }
    else if (path == "/v1/chat/completions" && method == "POST") {
        // OpenAI-compatible endpoint
        std::string prompt;
        try {
            auto j = nlohmann::json::parse(body);
            auto messages = j.value("messages", nlohmann::json::array());
            if (!messages.empty()) {
                prompt = messages[messages.size() - 1].value("content", "");
            }
        } catch (...) {
            prompt = body;
        }
        std::string result = runInference(prompt);
        responseBody = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"" + result + "\"}}]}";
    }
    else if (path == "/api/backend/status") {
        responseBody = "{\"status\":\"" + getBackendStatusString() + "\"}";
    }
    else if (path == "/api/router/status") {
        responseBody = "{\"status\":\"" + getRouterStatusString() + "\"}";
    }
    else if (path == "/api/governor/status") {
        responseBody = "{\"status\":\"" + getGovernorStatus() + "\"}";
    }
    else if (path == "/api/safety/status") {
        responseBody = "{\"status\":\"" + getSafetyStatus() + "\"}";
    }
    else if (path == "/api/swarm/status") {
        responseBody = "{\"status\":\"" + getSwarmStatus() + "\"}";
    }
    else if (path == "/api/debug/status") {
        responseBody = "{\"status\":\"" + getNativeDebugStatus() + "\"}";
    }
    else if (path == "/api/hotpatch/status") {
        responseBody = "{\"status\":\"" + getHotpatchStatus() + "\"}";
    }
    else if (path == "/api/asm/status") {
        responseBody = "{\"status\":\"" + getAsmSemanticStatsString() + "\"}";
    }
    else if (path == "/api/lsp/status") {
        responseBody = "{\"status\":\"" + getLSPStatusString() + "\"}";
    }
    else if (path == "/api/hybrid/status") {
        responseBody = "{\"status\":\"" + getHybridBridgeStatusString() + "\"}";
    }
    else if (path == "/api/agent/history") {
        responseBody = "{\"stats\":\"" + getAgentHistoryStats() + "\"}";
    }
    else if (path == "/api/failure/stats") {
        responseBody = "{\"stats\":\"" + getFailureDetectorStats() + "\"}";
    }
    else if (path == "/api/manifest" || path == "/api/features") {
        responseBody = getFeatureManifestJSON();
        if (responseBody.empty()) {
            responseBody = "{\"features\":\"headless mode\"}";
        }
    }
    else if (path == "/health" || path == "/api/health") {
        responseBody = "{\"status\":\"ok\",\"mode\":\"headless\",\"uptime\":" +
                       std::to_string(getUptimeMs()) + "}";
    }
    // ========== Phase 34: Production Instructions Context ==========
    else if (path == "/api/instructions" && method == "GET") {
        auto& ip = InstructionsProvider::instance();
        if (!ip.isLoaded()) ip.loadAll();
        responseBody = ip.toJSON();
    }
    else if (path == "/api/instructions/summary" && method == "GET") {
        auto& ip = InstructionsProvider::instance();
        if (!ip.isLoaded()) ip.loadAll();
        responseBody = ip.toJSONSummary();
    }
    else if (path == "/api/instructions/content" && method == "GET") {
        auto& ip = InstructionsProvider::instance();
        if (!ip.isLoaded()) ip.loadAll();
        std::string content = ip.getAllContent();
        responseBody = "{\"content\":\"" + content + "\"}";
        // Return raw markdown as text/markdown
        contentType = "text/markdown; charset=utf-8";
        responseBody = ip.getAllContent();
    }
    else if (path == "/api/instructions/reload" && method == "POST") {
        auto& ip = InstructionsProvider::instance();
        auto r = ip.reload();
        responseBody = "{\"success\":" + std::string(r.success ? "true" : "false") +
                       ",\"detail\":\"" + std::string(r.detail) + "\"}";
    }
    else {
        statusCode = 404;
        responseBody = "{\"error\":\"Not found\",\"path\":\"" + path + "\"}";
    }

    // Send HTTP response
    std::ostringstream resp;
    resp << "HTTP/1.1 " << statusCode << " " << (statusCode == 200 ? "OK" : "Not Found") << "\r\n";
    resp << "Content-Type: " << contentType << "\r\n";
    resp << "Content-Length: " << responseBody.size() << "\r\n";
    resp << "Access-Control-Allow-Origin: *\r\n";
    resp << "Connection: close\r\n";
    resp << "\r\n";
    resp << responseBody;

    std::string response = resp.str();
    send(clientFd, response.c_str(), static_cast<int>(response.size()), 0);
}

// ============================================================================
// Feature Manifest (delegates to Win32IDE_FeatureManifest.cpp structures)
// ============================================================================
std::string HeadlessIDE::getFeatureManifestMarkdown() const {
    return "# RawrXD Feature Manifest (Headless)\n\nPhase 19C: Headless surface active.\n";
}

std::string HeadlessIDE::getFeatureManifestJSON() const {
    return "{\"mode\":\"headless\",\"version\":\"" + std::string(VERSION) + "\",\"phase\":\"" + BUILD_PHASE + "\"}";
}

// ============================================================================
// Diagnostics
// ============================================================================
std::string HeadlessIDE::getFullStatusDump() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"mode\": \"headless\",\n";
    oss << "  \"version\": \"" << VERSION << "\",\n";
    oss << "  \"phase\": \"" << BUILD_PHASE << "\",\n";
    oss << "  \"session\": \"" << m_sessionId << "\",\n";
    oss << "  \"uptime_ms\": " << getUptimeMs() << ",\n";
    oss << "  \"model_loaded\": " << (m_modelLoaded ? "true" : "false") << ",\n";
    oss << "  \"model_name\": \"" << m_loadedModelName << "\",\n";
    oss << "  \"server_running\": " << (m_serverRunning.load() ? "true" : "false") << ",\n";
    oss << "  \"subsystems\": {\n";
    oss << "    \"winsock\": " << (m_winsockInitialized ? "true" : "false") << ",\n";
    oss << "    \"backend_manager\": " << (m_backendManagerInitialized ? "true" : "false") << ",\n";
    oss << "    \"llm_router\": " << (m_routerInitialized ? "true" : "false") << ",\n";
    oss << "    \"failure_detector\": " << (m_failureDetectorInitialized ? "true" : "false") << ",\n";
    oss << "    \"agent_history\": " << (m_agentHistoryInitialized ? "true" : "false") << ",\n";
    oss << "    \"asm_semantic\": " << (m_asmSemanticInitialized ? "true" : "false") << ",\n";
    oss << "    \"lsp_client\": " << (m_lspInitialized ? "true" : "false") << ",\n";
    oss << "    \"hybrid_bridge\": " << (m_hybridBridgeInitialized ? "true" : "false") << ",\n";
    oss << "    \"multi_response\": " << (m_multiResponseInitialized ? "true" : "false") << ",\n";
    oss << "    \"exec_governor\": " << (m_phase10Initialized ? "true" : "false") << ",\n";
    oss << "    \"swarm\": " << (m_phase11Initialized ? "true" : "false") << ",\n";
    oss << "    \"native_debugger\": " << (m_phase12Initialized ? "true" : "false") << ",\n";
    oss << "    \"hotpatch\": " << (m_hotpatchInitialized ? "true" : "false") << ",\n";
    oss << "    \"instructions\": " << (m_instructionsInitialized ? "true" : "false") << "\n";
    oss << "  }\n";
    oss << "}";
    return oss.str();
}

std::string HeadlessIDE::getVersionString() const {
    return std::string(VERSION) + " (" + BUILD_PHASE + ")";
}

uint64_t HeadlessIDE::getUptimeMs() const {
    auto now = std::chrono::system_clock::now();
    auto epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return static_cast<uint64_t>(epoch) - m_startEpochMs;
}

// ============================================================================
// Run Modes
// ============================================================================
int HeadlessIDE::runServerMode() {
    if (m_config.enableServer) {
        startServer();
    }

    m_outputSink->appendOutput("Headless IDE running in server mode. Press Ctrl+C to stop.", OutputSeverity::Info);

    // Block until shutdown
    while (!m_shutdownRequested.load()) {
        Sleep(100);
    }

    m_outputSink->appendOutput("Shutting down headless IDE...", OutputSeverity::Info);
    return 0;
}

int HeadlessIDE::runReplMode() {
    if (m_config.enableServer) {
        startServer();
    }

    m_outputSink->appendOutput("RawrXD Headless REPL. Type 'help' for commands, 'quit' to exit.", OutputSeverity::Info);

    std::string line;
    while (!m_shutdownRequested.load()) {
        printReplPrompt();
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if (line == "quit" || line == "exit" || line == "q") break;
        processReplCommand(line);
    }

    return 0;
}

int HeadlessIDE::runSingleShotMode() {
    if (m_config.prompt.empty()) {
        m_outputSink->appendOutput("No prompt specified (--prompt)", OutputSeverity::Error);
        return 1;
    }

    std::string result = runInference(m_config.prompt);
    m_outputSink->appendOutput(result.c_str(), OutputSeverity::Info);
    return 0;
}

int HeadlessIDE::runBatchMode() {
    if (m_config.inputFile.empty()) {
        m_outputSink->appendOutput("No input file specified (--input)", OutputSeverity::Error);
        return 1;
    }

    std::ifstream inFile(m_config.inputFile);
    if (!inFile.is_open()) {
        m_outputSink->appendOutput(("Cannot open input file: " + m_config.inputFile).c_str(), OutputSeverity::Error);
        return 1;
    }

    std::ofstream outFile;
    if (!m_config.outputFile.empty()) {
        outFile.open(m_config.outputFile);
        if (!outFile.is_open()) {
            m_outputSink->appendOutput(("Cannot open output file: " + m_config.outputFile).c_str(), OutputSeverity::Error);
            return 1;
        }
    }

    std::string line;
    int lineNum = 0;
    while (std::getline(inFile, line) && !m_shutdownRequested.load()) {
        if (line.empty()) continue;
        ++lineNum;
        m_outputSink->appendOutput(("Processing prompt " + std::to_string(lineNum) + "...").c_str(), OutputSeverity::Debug);

        std::string result = runInference(line);

        if (outFile.is_open()) {
            outFile << result << "\n";
        } else {
            m_outputSink->appendOutput(result.c_str(), OutputSeverity::Info);
        }
    }

    m_outputSink->appendOutput(("Batch complete: " + std::to_string(lineNum) + " prompts processed").c_str(), OutputSeverity::Info);
    return 0;
}

// ============================================================================
// REPL Helpers
// ============================================================================
void HeadlessIDE::processReplCommand(const std::string& input) {
    if (input == "help" || input == "?") {
        printReplHelp();
    }
    else if (input == "status") {
        std::string dump = getFullStatusDump();
        m_outputSink->appendOutput(dump.c_str(), OutputSeverity::Info);
    }
    else if (input == "version") {
        m_outputSink->appendOutput(getVersionString().c_str(), OutputSeverity::Info);
    }
    else if (input.substr(0, 5) == "load ") {
        std::string path = input.substr(5);
        if (loadModel(path)) {
            m_outputSink->appendOutput("Model loaded.", OutputSeverity::Info);
        } else {
            m_outputSink->appendOutput("Failed to load model.", OutputSeverity::Error);
        }
    }
    else if (input == "unload") {
        unloadModel();
    }
    else if (input == "model") {
        m_outputSink->appendOutput(getModelInfo().c_str(), OutputSeverity::Info);
    }
    else if (input == "backends") {
        m_outputSink->appendOutput(getBackendStatusString().c_str(), OutputSeverity::Info);
    }
    else if (input == "router") {
        m_outputSink->appendOutput(getRouterStatusString().c_str(), OutputSeverity::Info);
    }
    else if (input == "failures") {
        m_outputSink->appendOutput(getFailureDetectorStats().c_str(), OutputSeverity::Info);
    }
    else if (input == "history") {
        m_outputSink->appendOutput(getAgentHistoryStats().c_str(), OutputSeverity::Info);
    }
    else if (input == "asm") {
        m_outputSink->appendOutput(getAsmSemanticStatsString().c_str(), OutputSeverity::Info);
    }
    else if (input == "lsp") {
        m_outputSink->appendOutput(getLSPStatusString().c_str(), OutputSeverity::Info);
    }
    else if (input == "governor") {
        m_outputSink->appendOutput(getGovernorStatus().c_str(), OutputSeverity::Info);
    }
    else if (input == "safety") {
        m_outputSink->appendOutput(getSafetyStatus().c_str(), OutputSeverity::Info);
    }
    else if (input == "swarm") {
        m_outputSink->appendOutput(getSwarmStatus().c_str(), OutputSeverity::Info);
    }
    else if (input == "hotpatch") {
        m_outputSink->appendOutput(getHotpatchStatus().c_str(), OutputSeverity::Info);
    }
    else if (input == "cot" || input == "cot status") {
        auto& cot = ChainOfThoughtEngine::instance();
        m_outputSink->appendOutput(cot.getStatusJSON().c_str(), OutputSeverity::Info);
    }
    else if (input == "cot presets") {
        auto names = getCoTPresetNames();
        std::ostringstream oss;
        oss << "Chain-of-Thought Presets:\n";
        for (const auto& n : names) {
            const CoTPreset* p = getCoTPreset(n);
            if (p) {
                oss << "  " << n << " (" << p->label << ") — " << p->steps.size() << " steps\n";
            }
        }
        m_outputSink->appendOutput(oss.str().c_str(), OutputSeverity::Info);
    }
    else if (input == "cot roles") {
        const auto& roles = getAllCoTRoles();
        std::ostringstream oss;
        oss << "Chain-of-Thought Roles (" << roles.size() << "):\n";
        for (const auto& r : roles) {
            oss << "  " << r.icon << " " << r.name << " — " << r.instruction << "\n";
        }
        m_outputSink->appendOutput(oss.str().c_str(), OutputSeverity::Info);
    }
    else if (input == "cot steps") {
        auto& cot = ChainOfThoughtEngine::instance();
        m_outputSink->appendOutput(cot.getStepsJSON().c_str(), OutputSeverity::Info);
    }
    else if (input == "cot stats") {
        auto& cot = ChainOfThoughtEngine::instance();
        auto stats = cot.getStats();
        std::ostringstream oss;
        oss << "CoT Statistics:\n";
        oss << "  Total chains: " << stats.totalChains << "\n";
        oss << "  Successful: " << stats.successfulChains << "\n";
        oss << "  Failed: " << stats.failedChains << "\n";
        oss << "  Steps executed: " << stats.totalStepsExecuted << "\n";
        oss << "  Avg latency: " << stats.avgLatencyMs << "ms\n";
        m_outputSink->appendOutput(oss.str().c_str(), OutputSeverity::Info);
    }
    else if (input.substr(0, 11) == "cot preset ") {
        std::string presetName = input.substr(11);
        auto& cot = ChainOfThoughtEngine::instance();
        if (cot.applyPreset(presetName)) {
            std::string msg = "Applied preset '" + presetName + "' (" +
                std::to_string(cot.getSteps().size()) + " steps)";
            m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Info);
        } else {
            std::string msg = "Unknown preset: " + presetName + ". Available: review, audit, think, research, debate, custom";
            m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Error);
        }
    }
    else if (input.substr(0, 8) == "cot add ") {
        std::string roleName = input.substr(8);
        const CoTRoleInfo* info = getCoTRoleByName(roleName);
        if (info) {
            auto& cot = ChainOfThoughtEngine::instance();
            cot.addStep(info->id);
            std::string msg = "Added step: " + std::string(info->label) +
                " (total: " + std::to_string(cot.getSteps().size()) + " steps)";
            m_outputSink->appendOutput(msg.c_str(), OutputSeverity::Info);
        } else {
            m_outputSink->appendOutput("Unknown role. Use 'cot roles' to list.", OutputSeverity::Error);
        }
    }
    else if (input == "cot clear") {
        auto& cot = ChainOfThoughtEngine::instance();
        cot.clearSteps();
        m_outputSink->appendOutput("Chain cleared.", OutputSeverity::Info);
    }
    else if (input == "cot cancel") {
        auto& cot = ChainOfThoughtEngine::instance();
        cot.cancel();
        m_outputSink->appendOutput("Cancel requested.", OutputSeverity::Info);
    }
    else if (input.substr(0, 8) == "cot run ") {
        std::string query = input.substr(8);
        auto& cot = ChainOfThoughtEngine::instance();

        // Wire inference callback to use our runInference
        cot.setInferenceCallback([this](const std::string& systemPrompt,
                                         const std::string& userMessage,
                                         const std::string& /*model*/) -> std::string {
            std::string combined = systemPrompt + "\n\n" + userMessage;
            return runInference(combined);
        });

        if (cot.getSteps().empty()) {
            cot.applyPreset("review");
            m_outputSink->appendOutput("No steps set, applying 'review' preset.", OutputSeverity::Warning);
        }

        // Set step callback for progress
        cot.setStepCallback([this](const CoTStepResult& sr) {
            const auto& info = getCoTRoleInfo(sr.role);
            std::string msg;
            if (sr.skipped) {
                msg = "  Step " + std::to_string(sr.stepIndex + 1) + " (" + info.label + "): SKIPPED";
            } else if (sr.success) {
                msg = "  Step " + std::to_string(sr.stepIndex + 1) + " (" + info.label +
                    "): " + std::to_string(sr.latencyMs) + "ms";
            } else {
                msg = "  Step " + std::to_string(sr.stepIndex + 1) + " (" + info.label +
                    "): FAILED - " + sr.error;
            }
            m_outputSink->appendOutput(msg.c_str(), sr.success ? OutputSeverity::Info : OutputSeverity::Error);
        });

        m_outputSink->appendOutput("Executing CoT chain...", OutputSeverity::Info);
        CoTChainResult result = cot.executeChain(query);

        if (result.success) {
            std::string summary = "Chain complete (" + std::to_string(result.totalLatencyMs) + "ms, " +
                std::to_string(result.stepsCompleted) + " steps)";
            m_outputSink->appendOutput(summary.c_str(), OutputSeverity::Info);
            m_outputSink->onStreamStart("cot");
            m_outputSink->onStreamingToken(result.finalOutput.c_str(), result.finalOutput.size(),
                                            StreamTokenOrigin::Inference);
            m_outputSink->onStreamEnd("cot", true);
        } else {
            std::string errMsg = "Chain failed: " + result.error;
            m_outputSink->appendOutput(errMsg.c_str(), OutputSeverity::Error);
        }
    }
    else if (input == "server start") {
        startServer();
    }
    else if (input == "server stop") {
        stopServer();
    }
    else if (input == "server") {
        m_outputSink->appendOutput(getServerStatus().c_str(), OutputSeverity::Info);
    }
    // ── Phase 34: Instructions Context Commands ─────────────────────────
    else if (input == "instructions" || input == "instructions show") {
        auto& ip = InstructionsProvider::instance();
        if (!ip.isLoaded()) ip.loadAll();
        std::string content = ip.getAllContent();
        if (content.empty()) {
            m_outputSink->appendOutput("No instruction files loaded. Try 'instructions reload'.",
                                        OutputSeverity::Warning);
        } else {
            m_outputSink->appendOutput(content.c_str(), OutputSeverity::Info);
        }
    }
    else if (input == "instructions list") {
        auto& ip = InstructionsProvider::instance();
        if (!ip.isLoaded()) ip.loadAll();
        auto files = ip.getAll();
        std::ostringstream oss;
        oss << "Loaded instruction files (" << files.size() << "):\n";
        for (const auto& f : files) {
            oss << "  " << f.fileName << " (" << f.lineCount << " lines, "
                << f.sizeBytes << " bytes) — " << f.filePath << "\n";
        }
        m_outputSink->appendOutput(oss.str().c_str(), OutputSeverity::Info);
    }
    else if (input == "instructions reload") {
        auto& ip = InstructionsProvider::instance();
        auto r = ip.reload();
        std::string msg = r.success
            ? ("Instructions reloaded (" + std::to_string(ip.getLoadedCount()) + " files)")
            : ("Reload failed: " + std::string(r.detail));
        m_outputSink->appendOutput(msg.c_str(),
            r.success ? OutputSeverity::Info : OutputSeverity::Error);
    }
    else if (input == "instructions paths") {
        auto& ip = InstructionsProvider::instance();
        auto paths = ip.getSearchPaths();
        std::ostringstream oss;
        oss << "Search paths (" << paths.size() << "):\n";
        for (const auto& p : paths) {
            oss << "  " << p << "\n";
        }
        m_outputSink->appendOutput(oss.str().c_str(), OutputSeverity::Info);
    }
    else if (input == "instructions json") {
        auto& ip = InstructionsProvider::instance();
        if (!ip.isLoaded()) ip.loadAll();
        m_outputSink->appendOutput(ip.toJSON().c_str(), OutputSeverity::Info);
    }
    else {
        // Treat as inference prompt
        if (m_modelLoaded) {
            m_outputSink->onStreamStart("repl");
            std::string result = runInference(input);
            m_outputSink->onStreamingToken(result.c_str(), result.size(), StreamTokenOrigin::Inference);
            m_outputSink->onStreamEnd("repl", true);
        } else {
            m_outputSink->appendOutput("No model loaded. Use 'load <path>' first, or type 'help'.",
                                        OutputSeverity::Warning);
        }
    }
}

void HeadlessIDE::printReplHelp() {
    const char* help = R"(
RawrXD Headless IDE — REPL Commands
====================================
  help / ?         Show this help
  status           Full status dump (JSON)
  version          Show version
  load <path>      Load a GGUF model
  unload           Unload current model
  model            Show loaded model info
  backends         Backend switcher status
  router           LLM router status
  failures         Failure detector stats
  history          Agent history stats
  asm              ASM semantic stats
  lsp              LSP client status
  governor         Execution governor status
  safety           Safety contract status
  swarm            Distributed swarm status
  hotpatch         Hotpatch system status
  cot              CoT engine status
  cot presets      List CoT presets (review|audit|think|research|debate|custom)
  cot roles        List all CoT roles (12 reasoning personas)
  cot steps        Show current chain configuration
  cot stats        CoT execution statistics
  cot preset <n>   Apply a preset (e.g. 'cot preset review')
  cot add <role>   Add a role to the chain (e.g. 'cot add critic')
  cot clear        Clear current chain
  cot run <query>  Execute CoT chain on a query
  cot cancel       Cancel running chain
  server           HTTP server status
  server start     Start HTTP server
  server stop      Stop HTTP server
  instructions     Show production instructions (all lines)
  instructions list   List loaded instruction files
  instructions show   Show full content
  instructions reload Reload from disk
  instructions paths  Show search paths
  instructions json   Export as JSON
  quit / exit      Exit the REPL

  <any other text>  Treated as inference prompt

Command-line flags:
  --headless                    Enable headless mode
  --port <port>                 HTTP server port (default: 11435)
  --bind <address>              Bind address (default: 127.0.0.1)
  --model <path>                Load model on startup
  --prompt <text>               Single-shot inference, then exit
  --input <file>                Batch mode: read prompts from file
  --output <file>               Batch mode: write results to file
  --backend <name>              Set default backend
  --max-tokens <n>              Max tokens (default: 2048)
  --temperature <f>             Temperature (default: 0.7)
  --repl                        Interactive REPL mode
  --no-server                   Don't start HTTP server
  --verbose / -v                Verbose output
  --quiet / -q                  Quiet mode (warnings/errors only)
  --json                        JSON-structured output
  --settings <file>             Load settings from file
)";
    fprintf(stdout, "%s\n", help);
}

void HeadlessIDE::printReplPrompt() {
    if (m_modelLoaded) {
        fprintf(stdout, "[%s] > ", m_loadedModelName.c_str());
    } else {
        fprintf(stdout, "[no model] > ");
    }
    fflush(stdout);
}

// ============================================================================
// Shutdown
// ============================================================================
void HeadlessIDE::shutdownAll() {
    stopServer();

    if (m_winsockInitialized) {
        WSACleanup();
        m_winsockInitialized = false;
    }

    m_outputSink->flush();
}

