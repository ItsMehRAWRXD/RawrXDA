// ============================================================================
// quickjs_extension_host.h — QuickJS VSIX JavaScript Extension Host
// ============================================================================
//
// Phase 36: VSIX JS Extension Host via QuickJS
//
// Purpose:
//   Embeds a QuickJS runtime to execute real VSIX JavaScript extensions inside
//   the RawrXD Win32IDE. Each extension gets its own isolated runtime, event
//   loop thread, and sandboxed environment. All vscode.* API calls trampoline
//   into the existing VSCodeExtensionAPI C++ singleton—zero reimplementation.
//
// Architecture:
//   VSIX (.vsix) → unzip → package.json → main JS entry → QuickJS Runtime
//     ├── vscode.* API bindings   (C++ trampolines → VSCodeExtensionAPI)
//     ├── Event loop bridge        (host-driven timer + promise pump)
//     ├── Node shims               (fs, path, os, process — sandboxed)
//     └── Extension context        (subscriptions, globalState, workspaceState)
//
// Design Constraints (Non-Negotiable):
//   - No Node.js dependency
//   - No Electron dependency
//   - No shared runtimes (one RT per extension, crash-isolated)
//   - No eval from disk, no import from network
//   - Host controls lifecycle, not JS
//   - PatchResult-style structured results (no exceptions)
//   - Function pointer callbacks (no std::function in hot path)
//   - All COM/D2D calls routed to UI thread via PostMessage
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once
#ifndef QUICKJS_EXTENSION_HOST_H_
#define QUICKJS_EXTENSION_HOST_H_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <memory>
#include <filesystem>
#include <queue>
#include <condition_variable>

// Forward-declare QuickJS types (avoids pulling quickjs.h into every translation unit)
// These are opaque in this header; the .cpp files include quickjs.h directly.
struct JSRuntime;
struct JSContext;
typedef uint64_t JSValue;  // QuickJS uses 64-bit NaN-boxed values

// Forward-declare our own types
struct VSCodeExtensionManifest;
struct VSCodeExtensionContext;
struct VSCodeAPIResult;
class  Win32IDE;

namespace vscode {
    class VSCodeExtensionAPI;
}

// ============================================================================
// Configuration — Per-Extension Sandbox Limits
// ============================================================================

struct QuickJSSandboxConfig {
    size_t      memoryLimitBytes    = 64 * 1024 * 1024;    // 64 MB default per extension
    size_t      stackSizeLimitBytes = 1  * 1024 * 1024;    // 1 MB stack
    uint64_t    maxInstructionCount = 1000000000ULL;        // 1 billion ops watchdog
    uint32_t    timerResolutionMs   = 16;                   // Timer granularity (~60fps)
    uint32_t    maxTimers           = 256;                  // Max concurrent timers
    uint32_t    maxPendingCallbacks = 4096;                 // Event queue depth limit
    bool        allowEval           = false;                // Disallow eval() by default
    bool        allowBytecodeLoad   = false;                // Disallow .qjsc loading
    bool        allowNetworkShims   = false;                // Block net/http/https requires

    // Filesystem sandbox boundaries (extensions can only read/write within these)
    std::vector<std::filesystem::path>  allowedReadPaths;
    std::vector<std::filesystem::path>  allowedWritePaths;
};

// ============================================================================
// Timer Entry — Host-Driven setTimeout/setInterval
// ============================================================================

struct QuickJSTimerEntry {
    uint32_t    id;
    uint64_t    jsCallbackHandle;       // Opaque handle to the JS function (stored in runtime)
    uint64_t    intervalMs;             // 0 = setTimeout (one-shot), >0 = setInterval (repeating)
    uint64_t    nextFireTimeMs;         // Absolute time (GetTickCount64-based)
    bool        cancelled;
    bool        isInterval;
};

// ============================================================================
// Event Queue Entry — Cross-Thread Callback Dispatch
// ============================================================================

enum class QuickJSEventType : int {
    TimerFired          = 0,
    NativeCallback      = 1,    // C++ → JS callback (e.g., command handler, event listener)
    PromiseResolve      = 2,
    PromiseReject       = 3,
    ExtensionActivate   = 4,
    ExtensionDeactivate = 5,
    Shutdown            = 6
};

struct QuickJSEvent {
    QuickJSEventType    type;
    uint64_t            callbackHandle;     // JS function handle
    std::string         payload;            // JSON-encoded arguments (for complex data)
    void*               nativeData;         // Raw pointer for native callback data
    uint32_t            timerId;            // For timer events
};

// ============================================================================
// Extension Runtime — One Per Extension (Crash-Isolated)
// ============================================================================

enum class QuickJSExtensionState : int {
    Unloaded        = 0,
    Loading         = 1,
    Loaded          = 2,
    Activating      = 3,
    Active          = 4,
    Deactivating    = 5,
    Failed          = 6,
    Crashed         = 7
};

struct QuickJSExtensionRuntime {
    // Identity
    std::string             extensionId;
    std::string             extensionPath;      // Filesystem root of unpacked VSIX
    std::string             mainScriptPath;     // Resolved from package.json "main"
    VSCodeExtensionManifest* manifest;          // Borrowed pointer (owned by VSCodeExtensionAPI)

    // QuickJS Runtime (one per extension — crash isolation)
    JSRuntime*              runtime;
    JSContext*              context;

    // Lifecycle
    QuickJSExtensionState   state;
    std::string             lastError;
    uint64_t                activateTimeMs;
    uint64_t                totalCpuTimeMs;

    // Event Loop Thread
    std::thread             eventLoopThread;
    std::atomic<bool>       running;
    std::mutex              eventMutex;
    std::condition_variable eventCv;
    std::queue<QuickJSEvent> eventQueue;

    // Timers (host-driven)
    std::vector<QuickJSTimerEntry>  timers;
    std::mutex                      timerMutex;
    uint32_t                        nextTimerId;

    // JS Object Handles (stored as opaque uint64_t to avoid exposing JSValue in header)
    uint64_t    jsGlobalVscode;         // globalThis.vscode
    uint64_t    jsExportsActivate;      // exports.activate
    uint64_t    jsExportsDeactivate;    // exports.deactivate
    uint64_t    jsExtensionContext;     // The context object passed to activate()

    // Resource Tracking
    size_t      peakMemoryUsage;
    uint64_t    totalInstructionsExecuted;
    uint64_t    apiCallCount;
    uint64_t    errorCount;

    // Sandbox Config
    QuickJSSandboxConfig    sandbox;

    QuickJSExtensionRuntime()
        : manifest(nullptr)
        , runtime(nullptr)
        , context(nullptr)
        , state(QuickJSExtensionState::Unloaded)
        , activateTimeMs(0)
        , totalCpuTimeMs(0)
        , running(false)
        , nextTimerId(1)
        , jsGlobalVscode(0)
        , jsExportsActivate(0)
        , jsExportsDeactivate(0)
        , jsExtensionContext(0)
        , peakMemoryUsage(0)
        , totalInstructionsExecuted(0)
        , apiCallCount(0)
        , errorCount(0)
    {}
};

// ============================================================================
// JS Callback Wrapper — Safe Cross-Thread Callback Dispatch
// ============================================================================

struct QuickJSCallbackRef {
    uint64_t        jsFunction;         // Opaque handle (DUP'd JSValue)
    JSContext*       ctx;                // Owning context (for thread affinity check)
    std::string     extensionId;        // For routing to correct event loop
    bool            disposed;

    QuickJSCallbackRef()
        : jsFunction(0), ctx(nullptr), disposed(false) {}
};

// ============================================================================
// Statistics
// ============================================================================

struct QuickJSHostStats {
    uint64_t    totalExtensionsLoaded;
    uint64_t    totalExtensionsActive;
    uint64_t    totalExtensionsFailed;
    uint64_t    totalJSApiCalls;
    uint64_t    totalJSErrors;
    uint64_t    totalTimersFired;
    uint64_t    totalPromisesResolved;
    uint64_t    totalCallbacksDispatched;
    uint64_t    peakMemoryAllExtensions;
    uint64_t    totalInstructionsAllExtensions;
};

// ============================================================================
// QuickJSExtensionHost — Main Host Class (Singleton)
// ============================================================================
//
// Manages all QuickJS extension runtimes, VSIX unpacking/loading, and
// coordinates with VSCodeExtensionAPI for API routing.
//
// Thread Model:
//   - One event loop thread per extension (JS execution only on that thread)
//   - Host methods are called from the Win32IDE UI thread or worker threads
//   - Cross-thread dispatch via event queue + condition variable
//   - All JS function calls happen only inside the extension's event loop thread
//

class QuickJSExtensionHost {
public:
    // Singleton access
    static QuickJSExtensionHost& instance();

    // ---- Initialization ----
    // Called once at IDE startup. Wires up to VSCodeExtensionAPI singleton.
    VSCodeAPIResult initialize(Win32IDE* host, HWND mainWindow);
    VSCodeAPIResult shutdown();
    bool isInitialized() const { return m_initialized; }

    // ---- VSIX Install & Load ----
    // Full pipeline: unzip → parse package.json → validate → create runtime → load JS
    VSCodeAPIResult installVSIX(const char* vsixPath);
    VSCodeAPIResult loadJSExtension(const char* extensionId, const char* extensionPath,
                                     const VSCodeExtensionManifest* manifest);

    // ---- Extension Lifecycle ----
    VSCodeAPIResult activateExtension(const char* extensionId);
    VSCodeAPIResult deactivateExtension(const char* extensionId);
    VSCodeAPIResult unloadExtension(const char* extensionId);
    VSCodeAPIResult reloadExtension(const char* extensionId);

    // ---- Query ----
    QuickJSExtensionState getExtensionState(const char* extensionId) const;
    bool isJSExtension(const char* extensionId) const;
    size_t getLoadedExtensionCount() const;
    size_t getActiveExtensionCount() const;

    // ---- Activation Events ----
    // Called by VSCodeExtensionAPI when activation events fire
    void onActivationEvent(const char* event);  // e.g., "onCommand:myext.hello"
    void onLanguageActivation(const char* languageId);

    // ---- Cross-Thread Callback Dispatch ----
    // Queue a callback to be executed inside a specific extension's JS thread
    VSCodeAPIResult dispatchCallback(const char* extensionId,
                                      const QuickJSCallbackRef& callback,
                                      const char* argsJson);

    // ---- Statistics ----
    QuickJSHostStats getStats() const;
    void getStatusString(char* buf, size_t maxLen) const;
    void serializeStatsToJson(char* outJson, size_t maxLen) const;

    // ---- Configuration ----
    void setDefaultSandboxConfig(const QuickJSSandboxConfig& config);
    const QuickJSSandboxConfig& getDefaultSandboxConfig() const { return m_defaultSandbox; }

    // ---- Accessors ----
    Win32IDE* getHost() const { return m_host; }
    HWND getMainWindow() const { return m_mainWindow; }

private:
    QuickJSExtensionHost();
    ~QuickJSExtensionHost();
    QuickJSExtensionHost(const QuickJSExtensionHost&) = delete;
    QuickJSExtensionHost& operator=(const QuickJSExtensionHost&) = delete;

    // ---- Internal: Runtime Management ----
    QuickJSExtensionRuntime* createRuntime(const std::string& extensionId,
                                            const QuickJSSandboxConfig& sandbox);
    void destroyRuntime(QuickJSExtensionRuntime* rt);

    // ---- Internal: JS Context Setup ----
    // Inject globalThis.vscode, console, timers, require() into a fresh context
    bool injectGlobals(QuickJSExtensionRuntime* rt);
    bool injectVSCodeAPI(QuickJSExtensionRuntime* rt);
    bool injectConsole(QuickJSExtensionRuntime* rt);
    bool injectTimers(QuickJSExtensionRuntime* rt);
    bool injectRequire(QuickJSExtensionRuntime* rt);
    bool injectExtensionContext(QuickJSExtensionRuntime* rt);

    // ---- Internal: Module Loading ----
    // Custom require() that routes to Node shims or extension-local modules
    bool loadMainScript(QuickJSExtensionRuntime* rt);
    bool resolveModulePath(const std::string& requestedModule,
                           const std::string& extensionPath,
                           std::string& outResolvedPath);

    // ---- Internal: Event Loop ----
    static void eventLoopEntry(QuickJSExtensionRuntime* rt);
    void processEvents(QuickJSExtensionRuntime* rt);
    void processTimers(QuickJSExtensionRuntime* rt);
    void pumpPromiseJobs(QuickJSExtensionRuntime* rt);

    // ---- Internal: VSIX Unpacking ----
    bool unpackVSIX(const std::string& vsixPath, std::filesystem::path& outExtDir);
    bool parsePackageJson(const std::filesystem::path& extensionDir,
                          VSCodeExtensionManifest& outManifest);
    bool validateManifest(const VSCodeExtensionManifest& manifest);

    // ---- Internal: Instruction Count Watchdog ----
    static void instructionCountCallback(JSRuntime* rt, void* opaque);

    // ---- Internal: Crash Containment ----
    void handleExtensionCrash(QuickJSExtensionRuntime* rt, const char* errorMsg);

    // ---- Internal: Logging ----
    void logInfo(const char* fmt, ...) const;
    void logError(const char* fmt, ...) const;
    void logDebug(const char* fmt, ...) const;

    // ---- State ----
    Win32IDE*       m_host;
    HWND            m_mainWindow;
    bool            m_initialized;
    mutable std::mutex m_mutex;

    // Extension runtimes (extensionId → runtime)
    std::unordered_map<std::string, std::unique_ptr<QuickJSExtensionRuntime>> m_runtimes;
    mutable std::mutex m_runtimesMutex;

    // Activation event → list of extension IDs waiting for that event
    std::unordered_map<std::string, std::vector<std::string>> m_activationListeners;
    mutable std::mutex m_activationMutex;

    // Default sandbox config (applied to new extensions unless overridden)
    QuickJSSandboxConfig m_defaultSandbox;

    // VSIX install directory
    std::filesystem::path m_extensionsDir;

    // Statistics
    mutable std::atomic<uint64_t> m_totalAPICallsJS{0};
    mutable std::atomic<uint64_t> m_totalErrorsJS{0};
    mutable std::atomic<uint64_t> m_totalTimersFired{0};
    mutable std::atomic<uint64_t> m_totalPromisesResolved{0};
    mutable std::atomic<uint64_t> m_totalCallbacksDispatched{0};
};

// ============================================================================
// Node Shim API — Declared here, implemented in quickjs_node_shims.cpp
// ============================================================================
//
// These functions register the Node.js compatibility modules (fs, path, os,
// process) into a QuickJS context. Each module is sandboxed per the extension's
// QuickJSSandboxConfig.
//

namespace quickjs_shims {

    // Register all Node.js compatibility shims into the given context
    bool registerAllShims(JSContext* ctx, const QuickJSSandboxConfig& sandbox,
                          const std::string& extensionPath);

    // Individual module registration
    bool registerFS(JSContext* ctx, const QuickJSSandboxConfig& sandbox,
                    const std::string& extensionPath);
    bool registerPath(JSContext* ctx);
    bool registerOS(JSContext* ctx);
    bool registerProcess(JSContext* ctx, const std::string& extensionPath);

    // Path sandbox validation (used by fs shims)
    bool isPathAllowed(const std::filesystem::path& candidate,
                       const std::vector<std::filesystem::path>& allowedPaths);

    // Explicitly rejected modules (hard-fail on require())
    extern const char* const REJECTED_MODULES[];
    extern const size_t REJECTED_MODULES_COUNT;

} // namespace quickjs_shims

// ============================================================================
// VS Code API Bindings — Declared here, implemented in quickjs_vscode_bindings.cpp
// ============================================================================
//
// Each function registers a namespace of JS → C++ trampolines that call through
// to VSCodeExtensionAPI::instance().
//

namespace quickjs_bindings {

    // Register the entire vscode.* API surface into globalThis.vscode
    bool registerVSCodeAPI(JSContext* ctx, QuickJSExtensionRuntime* rt);

    // Individual namespace registration (called by registerVSCodeAPI)
    bool registerCommands(JSContext* ctx, uint64_t vsCodeObj, QuickJSExtensionRuntime* rt);
    bool registerWindow(JSContext* ctx, uint64_t vsCodeObj, QuickJSExtensionRuntime* rt);
    bool registerWorkspace(JSContext* ctx, uint64_t vsCodeObj, QuickJSExtensionRuntime* rt);
    bool registerLanguages(JSContext* ctx, uint64_t vsCodeObj, QuickJSExtensionRuntime* rt);
    bool registerDebug(JSContext* ctx, uint64_t vsCodeObj, QuickJSExtensionRuntime* rt);
    bool registerTasks(JSContext* ctx, uint64_t vsCodeObj, QuickJSExtensionRuntime* rt);
    bool registerEnv(JSContext* ctx, uint64_t vsCodeObj, QuickJSExtensionRuntime* rt);
    bool registerExtensions(JSContext* ctx, uint64_t vsCodeObj, QuickJSExtensionRuntime* rt);
    bool registerSCM(JSContext* ctx, uint64_t vsCodeObj, QuickJSExtensionRuntime* rt);

    // Enum/constant injection (CompletionItemKind, DiagnosticSeverity, etc.)
    bool registerEnums(JSContext* ctx, uint64_t vsCodeObj);

    // Utility: Convert C++ VSCodeAPIResult to a JS resolved/rejected Promise
    uint64_t resultToPromise(JSContext* ctx, const VSCodeAPIResult& result);

    // Utility: Extract QuickJSExtensionRuntime* from JS function's opaque data
    QuickJSExtensionRuntime* getRuntimeFromContext(JSContext* ctx);

} // namespace quickjs_bindings

// ============================================================================
// Command IDs for QuickJS Host (Phase 36, 10020 range)
// Relocated from 9420 range to avoid PDB 9400-9500 conflict
// ============================================================================
#define IDM_QUICKJS_HOST_STATUS         10020
#define IDM_QUICKJS_HOST_INSTALL_VSIX   10021
#define IDM_QUICKJS_HOST_LIST_EXTENSIONS 10022
#define IDM_QUICKJS_HOST_ACTIVATE       10023
#define IDM_QUICKJS_HOST_DEACTIVATE     10024
#define IDM_QUICKJS_HOST_RELOAD         10025
#define IDM_QUICKJS_HOST_SANDBOX_CONFIG 10026
#define IDM_QUICKJS_HOST_KILL_RUNTIME   10027

#endif // QUICKJS_EXTENSION_HOST_H_
