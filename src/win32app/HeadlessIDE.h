#pragma once
// ============================================================================
// HeadlessIDE — GUI-free surface for the RawrXD Win32IDE engine
// Phase 19C: Headless Surface Extraction
//
// This class exposes the full engine capabilities of Win32IDE without any
// HWND, window, or GDI dependency. It starts the LocalServer, initializes
// all backend subsystems (inference, LLM router, failure detection, agent
// history, ASM semantic, LSP, hybrid bridge, multi-response, execution
// governor, safety contract, replay journal, confidence gate, swarm,
// native debugger, hotpatch), and runs an event loop that services:
//
//   1. HTTP API on port 11435 (60+ endpoints — identical to Win32IDE)
//   2. Console REPL (optional stdin command processing)
//   3. stdin → prompt → stdout streaming inference
//
// Lifecycle:
//   HeadlessIDE ide;
//   if (!ide.initialize(argc, argv)) return 1;
//   return ide.run();   // blocks until shutdown signal
//
// NO exceptions. Returns PatchResult-style structured results.
// NO Qt. NO HWND. NO GDI. NO message loop.
// ============================================================================

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#ifdef ERROR
#undef ERROR
#endif

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <map>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <climits>

#include "IOutputSink.h"
#include "Win32IDE_AgenticBridge.h"
#include "Win32IDE_Autonomy.h"
#include "Win32IDE_SubAgent.h"
#include "../gguf_loader.h"
#include "../streaming_gguf_loader.h"
#include "../model_source_resolver.h"
#include "../modules/engine_manager.h"
#include "../modules/codex_ultimate.h"
#include "../modules/ExtensionLoader.hpp"
#include <nlohmann/json.hpp>

// Forward declarations
class MultiResponseEngine;
class SubAgentManager;
class AgentHistoryRecorder;
class AgenticEngine;

// ============================================================================
// Headless initialization result
// ============================================================================
struct HeadlessResult {
    bool success;
    const char* detail;
    int errorCode;

    static HeadlessResult ok(const char* msg = "OK") {
        return { true, msg, 0 };
    }
    static HeadlessResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// ============================================================================
// Headless run mode (how the main loop behaves)
// ============================================================================
enum class HeadlessRunMode {
    Server,      // Start HTTP server, block until SIGINT/SIGTERM (default)
    REPL,        // Interactive console REPL with command processing
    SingleShot,  // Process one prompt from --prompt arg, print result, exit
    Batch        // Read prompts from --input file, write results to --output
};

// ============================================================================
// Headless configuration (parsed from argc/argv)
// ============================================================================
struct HeadlessConfig {
    HeadlessRunMode mode           = HeadlessRunMode::Server;
    int             port           = 11435;
    std::string     bindAddress    = "127.0.0.1";
    std::string     modelPath;            // --model <path>
    std::string     prompt;               // --prompt <text> (SingleShot mode)
    std::string     inputFile;            // --input <file>  (Batch mode)
    std::string     outputFile;           // --output <file> (Batch mode)
    std::string     settingsFile;         // --settings <file>
    std::string     backend;              // --backend local|ollama|openai|claude|gemini
    bool            verbose        = false;
    bool            quiet          = false;
    bool            jsonOutput     = false;  // --json: emit structured JSON
    bool            enableRepl     = false;  // --repl: interactive mode
    bool            enableServer   = true;   // --no-server: disable HTTP
    int             maxTokens      = 2048;
    float           temperature    = 0.7f;
    std::string     ollamaHost     = "127.0.0.1";
    int             ollamaPort     = 11434;
    std::string     workingDir;             // --dir <path>
    bool            listModelsOnly = false; // --list: list Ollama models and exit
};

// ============================================================================
// HeadlessIDE — the headless surface class
// ============================================================================
class HeadlessIDE {
public:
    HeadlessIDE();
    ~HeadlessIDE();

    // ---- Lifecycle ----
    HeadlessResult initialize(int argc, char* argv[]);
    HeadlessResult initialize(const HeadlessConfig& config);
    int run();                        // Main blocking event loop
    void requestShutdown() noexcept;  // Signal the run loop to exit

    // ---- Configuration ----
    const HeadlessConfig& getConfig() const { return m_config; }
    bool isRunning() const { return m_running.load(); }

    // ---- Output sink ----
    IOutputSink* getOutputSink() const { return m_outputSink.get(); }
    void setOutputSink(std::unique_ptr<IOutputSink> sink);

    // ---- Engine access (same API surface as Win32IDE) ----
    void setEngineManager(EngineManager* mgr) { m_engineManager = mgr; }
    void setCodexUltimate(CodexUltimate* codex) { m_codexUltimate = codex; }

    // ---- Model operations ----
    bool loadModel(const std::string& filepath);
    bool unloadModel();
    bool isModelLoaded() const;
    std::string getLoadedModelName() const;
    std::string getModelInfo() const;

    // ---- Inference ----
    std::string runInference(const std::string& prompt);
    std::string runInference(const std::string& prompt, int maxTokens, float temperature);
    void runInferenceStreaming(const std::string& prompt,
                               std::function<void(const char*, size_t)> tokenCallback);

    // ---- Backend switcher (Phase 8B) ----
    enum class AIBackendType {
        LocalGGUF  = 0,
        Ollama     = 1,
        OpenAI     = 2,
        Claude     = 3,
        Gemini     = 4,
        Count      = 5
    };
    bool setActiveBackend(AIBackendType type);
    AIBackendType getActiveBackendType() const;
    std::string getBackendStatusString() const;
    bool probeBackendHealth(AIBackendType type);
    std::string routeInferenceRequest(const std::string& prompt);

    // ---- LLM Router (Phase 8C) ----
    std::string routeWithIntelligence(const std::string& prompt);
    std::string getRouterStatusString() const;
    std::string getCostLatencyHeatmapString() const;

    // ---- Failure Detection (Phase 4B/6) ----
    std::string executeWithFailureDetection(const std::string& prompt);
    std::string getFailureDetectorStats() const;
    std::string getFailureIntelligenceStatsString() const;

    // ---- Agent History (Phase 6B) ----
    std::string getAgentHistoryStats() const;
    void recordSimpleEvent(const std::string& description);

    // ---- ASM Semantic (Phase 9A) ----
    void parseAsmFile(const std::string& filePath);
    void parseAsmDirectory(const std::string& dirPath, bool recursive = true);
    std::string getAsmSymbolTableString() const;
    std::string getAsmSemanticStatsString() const;

    // ---- LSP Client (Phase 9A) ----
    std::string getLSPStatusString() const;

    // ---- Hybrid Bridge (Phase 9B) ----
    std::string getHybridBridgeStatusString() const;

    // ---- Multi-Response (Phase 9C) ----
    // Generates multiple responses from different backends/temperatures

    // ---- Execution Governor (Phase 10A) ----
    std::string getGovernorStatus() const;

    // ---- Safety Contract (Phase 10B) ----
    std::string getSafetyStatus() const;

    // ---- Replay Journal (Phase 10C) ----
    std::string getReplayStatus() const;

    // ---- Confidence Gate (Phase 10D) ----
    std::string getConfidenceStatus() const;

    // ---- Swarm (Phase 11) ----
    std::string getSwarmStatus() const;

    // ---- Native Debugger (Phase 12) ----
    std::string getNativeDebugStatus() const;

    // ---- Hotpatch (Phase 14.2) ----
    std::string getHotpatchStatus() const;

    // ---- Settings ----
    void loadSettings(const std::string& path = "");
    void saveSettings(const std::string& path = "");
    std::string getSettingsFilePath() const;

    // ---- Local HTTP Server ----
    void startServer();
    void stopServer();
    bool isServerRunning() const;
    std::string getServerStatus() const;

    // ---- Feature Manifest ----
    std::string getFeatureManifestMarkdown() const;
    std::string getFeatureManifestJSON() const;

    // ---- Diagnostics ----
    std::string getFullStatusDump() const;
    std::string getVersionString() const;
    uint64_t getUptimeMs() const;

    // ---- Instructions Context (Phase 34 — persistent across session) ----
    std::string getInstructionsContent() const;
    bool isInstructionsLoaded() const { return m_instructionsInitialized; }

private:
    // ---- Argument parsing ----
    HeadlessResult parseArgs(int argc, char* argv[]);

    // ---- Initialization phases ----
    HeadlessResult initWinsock();
    HeadlessResult initEngines();
    HeadlessResult initBackendManager();
    HeadlessResult initLLMRouter();
    HeadlessResult initFailureDetection();
    HeadlessResult initAgentHistory();
    HeadlessResult initAsmSemantic();
    HeadlessResult initLSPClient();
    HeadlessResult initHybridBridge();
    HeadlessResult initMultiResponse();
    HeadlessResult initPhase10();
    HeadlessResult initPhase11();
    HeadlessResult initPhase12();
    HeadlessResult initHotpatch();
    HeadlessResult initInstructions();
    HeadlessResult initAgentic();

    // ---- Run modes ----
    int runServerMode();
    int runReplMode();
    int runSingleShotMode();
    int runBatchMode();

    // ---- REPL helpers ----
    void processReplCommand(const std::string& input);
    void printReplHelp();
    void printReplPrompt();

    // ---- Tool execution (parity with Win32 Agent > Run Tool; used by /api/tool and /run-tool) ----
    bool executeToolRepl(const std::string& toolName, const std::string& argsJson, std::string& outResult);

    // ---- HTTP server (delegating to Win32IDE_LocalServer logic) ----
    void serverLoop();
    void handleClient(SOCKET clientFd);

    // ---- Shutdown ----
    void shutdownAll();

    // ---- State ----
    HeadlessConfig                    m_config;
    std::unique_ptr<IOutputSink>      m_outputSink;
    std::atomic<bool>                 m_running{false};
    std::atomic<bool>                 m_shutdownRequested{false};

    // Engine pointers (not owned — same pattern as Win32IDE)
    EngineManager*                    m_engineManager   = nullptr;
    CodexUltimate*                    m_codexUltimate   = nullptr;

    // Subsystem initialized flags
    bool m_winsockInitialized         = false;
    bool m_backendManagerInitialized  = false;
    bool m_routerInitialized          = false;
    bool m_failureDetectorInitialized = false;
    bool m_agentHistoryInitialized    = false;
    bool m_asmSemanticInitialized     = false;
    bool m_lspInitialized             = false;
    bool m_hybridBridgeInitialized    = false;
    bool m_multiResponseInitialized   = false;
    bool m_phase10Initialized         = false;
    bool m_phase11Initialized         = false;
    bool m_phase12Initialized         = false;
    bool m_hotpatchInitialized        = false;
    bool m_instructionsInitialized  = false;

    // HTTP server state
    std::atomic<bool>                 m_serverRunning{false};
    std::thread                       m_serverThread;
    SOCKET                            m_serverSocket = INVALID_SOCKET;

    // GGUF model state
    bool                              m_modelLoaded     = false;
    std::string                       m_loadedModelPath;
    std::string                       m_loadedModelName;

    // Active backend state
    AIBackendType                     m_activeBackend   = AIBackendType::LocalGGUF;
    uint64_t                          m_inferenceRequestCount = 0;

    // Real subsystem instances (owned by HeadlessIDE)
    std::unique_ptr<MultiResponseEngine> m_multiResponse;
    std::unique_ptr<AgentHistoryRecorder> m_historyRecorder;

    // Agentic stack (101% parity with Win32 — chat, tool dispatch, subagent, chain, swarm)
    std::unique_ptr<AgenticEngine>     m_agenticEngine;
    std::unique_ptr<SubAgentManager>   m_subAgentManager;

    // Failure detection counters
    uint64_t                          m_failureDetections = 0;
    uint64_t                          m_failureRetries    = 0;

    // Agent history counters
    uint64_t                          m_agentEventCount   = 0;

    // ASM semantic counters
    uint32_t                          m_asmSymbolCount    = 0;
    uint32_t                          m_asmFilesParsed    = 0;

    // LSP counters
    uint32_t                          m_lspServerCount    = 0;
    uint64_t                          m_lspCompletionCount = 0;

    // Hybrid bridge counters
    uint64_t                          m_hybridCompletionCount = 0;

    // Native debugger state
    bool                              m_debugSessionActive    = false;
    uint32_t                          m_debugBreakpointCount  = 0;

    // Session
    std::string                       m_sessionId;
    uint64_t                          m_startEpochMs    = 0;

    // Version
    static constexpr const char*      VERSION = "20.0.0-headless-enterprise";
    static constexpr const char*      BUILD_PHASE = "Phase 20 Enterprise";
};
