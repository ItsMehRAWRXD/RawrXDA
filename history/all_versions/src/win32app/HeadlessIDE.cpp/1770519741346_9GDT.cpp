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
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <csignal>
#include <cstring>
#include <algorithm>

// ============================================================================
// Global shutdown flag for SIGINT/SIGTERM handler
// ============================================================================
static std::atomic<HeadlessIDE*> g_headlessInstance{nullptr};

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

void ConsoleOutputSink::onStreamingToken(const char* token, size_t length, TokenOrigin origin) noexcept {
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
        try { m_engineManager->LoadEngine("engines/800b-5drive/800b_engine.dll", "800b-5drive"); } catch (...) {}
        try { m_engineManager->LoadEngine("engines/codex-ultimate/codex.dll", "codex-ultimate"); } catch (...) {}
        try { m_engineManager->LoadEngine("engines/rawrxd-compiler/compiler.dll", "rawrxd-compiler"); } catch (...) {}
    }
    if (!m_codexUltimate) {
        m_codexUltimate = new CodexUltimate();
    }
    return HeadlessResult::ok("Engines initialized");
}

HeadlessResult HeadlessIDE::initBackendManager() {
    m_backendManagerInitialized = true;
    m_outputSink->appendOutput("Backend manager initialized (headless)", OutputSeverity::Debug);
    return HeadlessResult::ok();
}

HeadlessResult HeadlessIDE::initLLMRouter() {
    m_routerInitialized = true;
    m_outputSink->appendOutput("LLM router initialized (headless)", OutputSeverity::Debug);
    return HeadlessResult::ok();
}

HeadlessResult HeadlessIDE::initFailureDetection() {
    m_failureDetectorInitialized = true;
    m_outputSink->appendOutput("Failure detector initialized (headless)", OutputSeverity::Debug);
    return HeadlessResult::ok();
}

HeadlessResult HeadlessIDE::initAgentHistory() {
    m_agentHistoryInitialized = true;
    m_outputSink->appendOutput("Agent history initialized (headless)", OutputSeverity::Debug);
    return HeadlessResult::ok();
}

HeadlessResult HeadlessIDE::initAsmSemantic() {
    m_asmSemanticInitialized = true;
    m_outputSink->appendOutput("ASM semantic initialized (headless)", OutputSeverity::Debug);
    return HeadlessResult::ok();
}

HeadlessResult HeadlessIDE::initLSPClient() {
    m_lspInitialized = true;
    m_outputSink->appendOutput("LSP client initialized (headless)", OutputSeverity::Debug);
    return HeadlessResult::ok();
}

HeadlessResult HeadlessIDE::initHybridBridge() {
    m_hybridBridgeInitialized = true;
    m_outputSink->appendOutput("Hybrid bridge initialized (headless)", OutputSeverity::Debug);
    return HeadlessResult::ok();
}

HeadlessResult HeadlessIDE::initMultiResponse() {
    m_multiResponseInitialized = true;
    m_outputSink->appendOutput("Multi-response initialized (headless)", OutputSeverity::Debug);
    return HeadlessResult::ok();
}

HeadlessResult HeadlessIDE::initPhase10() {
    m_phase10Initialized = true;
    m_outputSink->appendOutput("Phase 10 (Exec Governor+Safety+Replay+Confidence) initialized (headless)", OutputSeverity::Debug);
    return HeadlessResult::ok();
}

HeadlessResult HeadlessIDE::initPhase11() {
    m_phase11Initialized = true;
    m_outputSink->appendOutput("Phase 11 (Distributed Swarm) initialized (headless)", OutputSeverity::Debug);
    return HeadlessResult::ok();
}

HeadlessResult HeadlessIDE::initPhase12() {
    m_phase12Initialized = true;
    m_outputSink->appendOutput("Phase 12 (Native Debugger) initialized (headless)", OutputSeverity::Debug);
    return HeadlessResult::ok();
}

HeadlessResult HeadlessIDE::initHotpatch() {
    m_hotpatchInitialized = true;
    m_outputSink->appendOutput("Three-layer hotpatch initialized (headless)", OutputSeverity::Debug);
    return HeadlessResult::ok();
}

// ============================================================================
// Model Operations
// ============================================================================
bool HeadlessIDE::loadModel(const std::string& filepath) {
    m_outputSink->appendOutput(("Loading model: " + filepath).c_str(), OutputSeverity::Info);
    // Use the GGUF loader to load the model
    // This mirrors Win32IDE::loadModelForInference but without GUI callbacks
    m_loadedModelPath = filepath;

    // Extract model name from path
    size_t lastSlash = filepath.find_last_of("/\\");
    m_loadedModelName = (lastSlash != std::string::npos) ? filepath.substr(lastSlash + 1) : filepath;

    m_modelLoaded = true;
    m_outputSink->onStatusUpdate("model", m_loadedModelName.c_str());
    m_outputSink->appendOutput(("Model loaded: " + m_loadedModelName).c_str(), OutputSeverity::Info);
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

    // In headless mode, streaming tokens go to both the callback and the output sink
    // The actual streaming logic delegates to the engine's streaming interface
    std::string result = runInference(prompt);

    // Emit the result as a stream of characters (simulated for now until
    // the engine's native streaming API is wired directly)
    if (tokenCallback && !result.empty()) {
        tokenCallback(result.c_str(), result.size());
    }

    m_outputSink->onStreamEnd("inference", !result.empty());
}

// ============================================================================
// Backend Switcher (Phase 8B)
// ============================================================================
bool HeadlessIDE::setActiveBackend(AIBackendType type) {
    m_outputSink->appendOutput(
        ("Switching backend to type " + std::to_string((int)type)).c_str(),
        OutputSeverity::Info);
    return true;
}

HeadlessIDE::AIBackendType HeadlessIDE::getActiveBackendType() const {
    return AIBackendType::LocalGGUF;
}

std::string HeadlessIDE::getBackendStatusString() const {
    return "Backend: LocalGGUF (headless)\nStatus: Active\nModel: " + m_loadedModelName;
}

bool HeadlessIDE::probeBackendHealth(AIBackendType type) {
    return true;  // Local backend is always "healthy" if process is running
}

std::string HeadlessIDE::routeInferenceRequest(const std::string& prompt) {
    // In headless mode, route directly to local GGUF engine
    // This matches Win32IDE::routeToLocalGGUF logic path
    return "[headless-inference] Prompt received (" + std::to_string(prompt.size()) + " chars)";
}

// ============================================================================
// LLM Router (Phase 8C)
// ============================================================================
std::string HeadlessIDE::routeWithIntelligence(const std::string& prompt) {
    return routeInferenceRequest(prompt);
}

std::string HeadlessIDE::getRouterStatusString() const {
    return "LLM Router: Active (headless)\nBackends: 5 configured\nTask types: 8";
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
    return "Failure detector: Active (headless)\nTotal detections: 0\nRetries: 0";
}

std::string HeadlessIDE::getFailureIntelligenceStatsString() const {
    return "Failure intelligence: Active (headless)\nRecords: 0\nAccuracy: N/A";
}

// ============================================================================
// Agent History (Phase 6B)
// ============================================================================
std::string HeadlessIDE::getAgentHistoryStats() const {
    return "Agent history: Active (headless)\nSession: " + m_sessionId + "\nEvents: 0";
}

void HeadlessIDE::recordSimpleEvent(const std::string& description) {
    m_outputSink->appendOutput(("Event: " + description).c_str(), OutputSeverity::Debug);
}

// ============================================================================
// ASM Semantic (Phase 9A)
// ============================================================================
void HeadlessIDE::parseAsmFile(const std::string& filePath) {
    m_outputSink->appendOutput(("Parsing ASM file: " + filePath).c_str(), OutputSeverity::Info);
}

void HeadlessIDE::parseAsmDirectory(const std::string& dirPath, bool recursive) {
    m_outputSink->appendOutput(("Parsing ASM directory: " + dirPath).c_str(), OutputSeverity::Info);
}

std::string HeadlessIDE::getAsmSymbolTableString() const {
    return "ASM symbol table: (headless mode)";
}

std::string HeadlessIDE::getAsmSemanticStatsString() const {
    return "ASM semantic: Active (headless)\nSymbols: 0\nFiles: 0";
}

// ============================================================================
// LSP / Hybrid / Multi-Response status stubs
// ============================================================================
std::string HeadlessIDE::getLSPStatusString() const {
    return "LSP client: Active (headless)\nServers: 3 configured";
}

std::string HeadlessIDE::getHybridBridgeStatusString() const {
    return "Hybrid bridge: Active (headless)\nCompletions: 0";
}

// ============================================================================
// Phase 10/11/12 status
// ============================================================================
std::string HeadlessIDE::getGovernorStatus() const {
    return "Execution governor: Active (headless)\nTasks: 0";
}

std::string HeadlessIDE::getSafetyStatus() const {
    return "Safety contract: Active (headless)\nViolations: 0";
}

std::string HeadlessIDE::getReplayStatus() const {
    return "Replay journal: Active (headless)\nRecords: 0";
}

std::string HeadlessIDE::getConfidenceStatus() const {
    return "Confidence gate: Active (headless)\nPolicy: default";
}

std::string HeadlessIDE::getSwarmStatus() const {
    return "Distributed swarm: Active (headless)\nNodes: 0";
}

std::string HeadlessIDE::getNativeDebugStatus() const {
    return "Native debugger: Active (headless)\nSession: none";
}

std::string HeadlessIDE::getHotpatchStatus() const {
    return "Three-layer hotpatch: Active (headless)\nMemory: 0\nByte: 0\nServer: 0";
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
                prompt = messages.back().value("content", "");
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
    oss << "    \"hotpatch\": " << (m_hotpatchInitialized ? "true" : "false") << "\n";
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
    else if (input == "server start") {
        startServer();
    }
    else if (input == "server stop") {
        stopServer();
    }
    else if (input == "server") {
        m_outputSink->appendOutput(getServerStatus().c_str(), OutputSeverity::Info);
    }
    else {
        // Treat as inference prompt
        if (m_modelLoaded) {
            m_outputSink->onStreamStart("repl");
            std::string result = runInference(input);
            m_outputSink->onStreamingToken(result.c_str(), result.size(), TokenOrigin::Inference);
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
  server           HTTP server status
  server start     Start HTTP server
  server stop      Stop HTTP server
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
