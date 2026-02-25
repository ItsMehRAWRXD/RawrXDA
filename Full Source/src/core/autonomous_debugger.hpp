// =============================================================================
// autonomous_debugger.hpp
// RawrXD v14.2.1 Cathedral — Tier 1.4: Autonomous Debugging & Root Cause Analysis
//
// Agent-driven debugging without human direction:
//   - Auto-set breakpoints/watchpoints on suspicious variables
//   - Automated bisect debugging (binary search through execution)
//   - Core dump analysis
//   - Automatic bug reproduction from stack traces
//   - Root cause analysis via backwards taint analysis
//
// Extends NativeDebuggerEngine with autonomous capabilities.
// =============================================================================
#pragma once
#ifndef RAWRXD_AUTONOMOUS_DEBUGGER_HPP
#define RAWRXD_AUTONOMOUS_DEBUGGER_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#endif

namespace RawrXD {
namespace Debugging {

// =============================================================================
// Result Types
// =============================================================================

struct DebugResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static DebugResult ok(const char* msg) {
        DebugResult r;
        r.success   = true;
        r.detail    = msg;
        r.errorCode = 0;
        return r;
    }

    static DebugResult error(const char* msg, int code = -1) {
        DebugResult r;
        r.success   = false;
        r.detail    = msg;
        r.errorCode = code;
        return r;
    }
};

// =============================================================================
// Enums
// =============================================================================

enum class BugClassification : uint8_t {
    NullPointerDeref,
    UseAfterFree,
    BufferOverflow,
    IntegerOverflow,
    RaceCondition,
    Deadlock,
    ResourceLeak,
    LogicError,
    AssertionFailure,
    StackOverflow,
    DivideByZero,
    AccessViolation,
    Unknown
};

enum class DebugStrategy : uint8_t {
    Bisect,              // Binary search through execution history
    TaintAnalysis,       // Track data propagation backwards
    WatchpointTriangle,  // Set watchpoints around suspicious area
    DeltaDebugging,      // Minimize failing input
    SymbolicExecution,   // Explore all paths symbolically
    ReplayDivergence     // Compare good vs bad execution traces
};

enum class WatchpointAction : uint8_t {
    Break,               // Break when value changes
    Log,                 // Log value changes, don't break
    TraceHistory,        // Record all values over time
    Alert                // Notify when value meets condition
};

// =============================================================================
// Core Data Types
// =============================================================================

struct StackFrame {
    uint64_t    address;
    uint64_t    returnAddress;
    char        functionName[256];
    char        fileName[260];
    int         lineNumber;
    uint64_t    framePointer;
    uint64_t    stackPointer;
};

struct CrashReport {
    uint64_t          crashId;
    BugClassification classification;
    char              exceptionMessage[512];
    uint64_t          faultAddress;
    uint64_t          instructionPointer;
    StackFrame        callStack[64];
    int               frameCount;
    uint64_t          timestamp;

    // Register state at crash
    uint64_t          rax, rbx, rcx, rdx;
    uint64_t          rsi, rdi, rsp, rbp;
    uint64_t          r8, r9, r10, r11;
    uint64_t          r12, r13, r14, r15;
    uint64_t          rflags;
};

struct AutoWatchpoint {
    uint64_t        watchId;
    uint64_t        address;
    int             size;           // 1, 2, 4, or 8 bytes
    char            variableName[256];
    char            file[260];
    int             line;
    WatchpointAction action;
    char            condition[256]; // e.g. "value > 100" or "value == 0"
    float           suspicion;     // 0.0 – 1.0: how suspicious this variable is
    bool            active;

    // Value tracking
    uint64_t        lastValue;
    uint64_t        changeCount;
    uint64_t        hitCount;
};

struct RootCauseAnalysis {
    uint64_t          analysisId;
    BugClassification classification;

    // The identified root cause
    char              rootCauseFile[260];
    int               rootCauseLine;
    char              rootCauseFunction[256];
    char              rootCauseDescription[1024];
    float             confidence;     // 0.0 – 1.0

    // The propagation chain (how the bug manifested)
    struct PropagationStep {
        char file[260];
        int  line;
        char description[256];
    };
    PropagationStep   propagation[32];
    int               propagationLen;

    // Suggested fix
    char              suggestedFix[2048];
    bool              autoFixable;
};

struct BisectState {
    uint64_t    bisectId;
    uint64_t    goodCommit;      // Known-good state index
    uint64_t    badCommit;       // Known-bad state index
    uint64_t    currentTest;     // Currently testing
    int         totalSteps;
    int         currentStep;
    bool        completed;
    uint64_t    foundCulprit;    // The index where the bug was introduced
};

// =============================================================================
// Callbacks
// =============================================================================

typedef void (*CrashDetectedCallback)(const CrashReport* crash, void* userData);
typedef void (*RootCauseFoundCallback)(const RootCauseAnalysis* analysis, void* userData);
typedef void (*WatchpointHitCallback)(const AutoWatchpoint* wp, uint64_t oldValue, uint64_t newValue, void* userData);
typedef void (*BisectProgressCallback)(const BisectState* state, void* userData);
typedef bool (*AutoReproducerFn)(const CrashReport* crash, void* userData);  // Returns true if crash reproduced

// =============================================================================
// Configuration
// =============================================================================

struct DebugConfig {
    int          maxWatchpoints;         // Default: 32
    int          maxStackDepth;          // Default: 64
    int          maxBisectSteps;         // Default: 30
    bool         autoClassifyCrashes;    // Default: true
    bool         autoSetWatchpoints;     // Default: true
    bool         autoAnalyzeRootCause;   // Default: true
    bool         autoReproduceBugs;      // Default: false (requires test harness)
    double       watchpointTimeoutMs;    // Default: 60000
    float        suspicionThreshold;     // Default: 0.6 (auto-watch if above)

    static DebugConfig defaults() {
        DebugConfig c;
        c.maxWatchpoints       = 32;
        c.maxStackDepth        = 64;
        c.maxBisectSteps       = 30;
        c.autoClassifyCrashes  = true;
        c.autoSetWatchpoints   = true;
        c.autoAnalyzeRootCause = true;
        c.autoReproduceBugs    = false;
        c.watchpointTimeoutMs  = 60000.0;
        c.suspicionThreshold   = 0.6f;
        return c;
    }
};

// =============================================================================
// Core Class: AutonomousDebugger
// =============================================================================

class AutonomousDebugger {
public:
    static AutonomousDebugger& instance();

    // ── Lifecycle ──────────────────────────────────────────────────────────
    DebugResult initialize(const DebugConfig& config);
    DebugResult initialize();
    void shutdown();
    bool isActive() const { return m_active.load(); }

    // ── Crash Analysis ─────────────────────────────────────────────────────
    // Analyze a crash and attempt root-cause identification
    RootCauseAnalysis analyzeCrash(const CrashReport& crash);

    // Classify crash type from exception info
    BugClassification classifyCrash(const CrashReport& crash);

    // Parse minidump / core dump
    DebugResult analyzeCoreDump(const char* dumpPath);
    DebugResult analyzeMiniDump(const char* dumpPath);

    // Reproduce a crash from a stack trace
    DebugResult reproduceCrash(const CrashReport& crash);

    // ── Autonomous Breakpoint Management ───────────────────────────────────
    // Analyze code and auto-set watchpoints on suspicious variables
    DebugResult autoSetWatchpoints(const char* file);
    DebugResult autoSetWatchpoints(const char* functionName, const char* file, int line);

    // Manual watchpoint management
    uint64_t addWatchpoint(const char* variable, const char* file, int line,
                            WatchpointAction action = WatchpointAction::Break,
                            const char* condition = nullptr);
    DebugResult removeWatchpoint(uint64_t watchId);
    DebugResult removeAllWatchpoints();

    std::vector<AutoWatchpoint> getActiveWatchpoints() const;
    std::vector<AutoWatchpoint> getTriggeredWatchpoints() const;

    // ── Bisect Debugging ───────────────────────────────────────────────────
    // Binary search through execution to find where bug was introduced
    uint64_t startBisect(uint64_t goodState, uint64_t badState);
    DebugResult markBisectGood(uint64_t bisectId);
    DebugResult markBisectBad(uint64_t bisectId);
    BisectState getBisectState(uint64_t bisectId) const;

    // Git bisect integration
    DebugResult gitBisect(const char* goodCommit, const char* badCommit,
                           const char* testCommand);

    // ── Taint Analysis ─────────────────────────────────────────────────────
    // Track where a suspicious value came from
    struct TaintChain {
        struct Link {
            char file[260];
            int  line;
            char operation[256]; // "assigned from ...", "passed to ...", etc.
        };
        Link  chain[64];
        int   chainLength;
        char  originVariable[256];
        char  originFile[260];
        int   originLine;
    };

    TaintChain traceBackwards(const char* variable, const char* file, int line);

    // ── Sub-system Injection ───────────────────────────────────────────────
    // LLM-based root cause analyzer
    typedef RootCauseAnalysis (*RCAAnalyzerFn)(const CrashReport* crash,
                                                const char* sourceCode, void* ud);
    void setRootCauseAnalyzer(RCAAnalyzerFn fn, void* ud);

    // Source code reader
    typedef std::string (*SourceReaderFn)(const char* file, int startLine, int endLine, void* ud);
    void setSourceReader(SourceReaderFn fn, void* ud);

    // Test runner for bisect
    typedef bool (*BisectTestFn)(uint64_t stateId, void* ud);
    void setBisectTester(BisectTestFn fn, void* ud);

    // Debugger engine bridge
    typedef bool (*SetHWBreakpointFn)(uint64_t addr, int size, void* ud);
    void setBreakpointEngine(SetHWBreakpointFn fn, void* ud);

    // ── Callbacks ──────────────────────────────────────────────────────────
    void setCrashCallback(CrashDetectedCallback cb, void* ud);
    void setRootCauseCallback(RootCauseFoundCallback cb, void* ud);
    void setWatchpointCallback(WatchpointHitCallback cb, void* ud);
    void setBisectProgressCallback(BisectProgressCallback cb, void* ud);

    // ── Statistics ──────────────────────────────────────────────────────────
    struct Stats {
        uint64_t totalCrashesAnalyzed;
        uint64_t rootCausesFound;
        uint64_t watchpointsSet;
        uint64_t watchpointsTriggered;
        uint64_t bisectsCompleted;
        uint64_t autoFixesGenerated;
        double   avgAnalysisTimeMs;
    };

    Stats getStats() const;
    std::string statsToJson() const;
    std::string crashReportToJson(const CrashReport& crash) const;
    std::string rootCauseToJson(const RootCauseAnalysis& rca) const;

private:
    AutonomousDebugger();
    ~AutonomousDebugger();
    AutonomousDebugger(const AutonomousDebugger&) = delete;
    AutonomousDebugger& operator=(const AutonomousDebugger&) = delete;

    // Internal analysis methods
    BugClassification classifyFromAddress(uint64_t faultAddr, uint64_t ip);
    BugClassification classifyFromCallStack(const StackFrame* frames, int count);
    float calculateSuspicion(const char* variableName, const char* functionName);
    void generateAutoWatchpoints(const char* functionName, const char* file, int line);

#ifdef _WIN32
    // Windows-specific minidump parsing
    DebugResult parseMinidump(const char* path, CrashReport& out);
#endif

    uint64_t nextWatchId();
    uint64_t nextBisectId();
    uint64_t nextCrashId();

    // ── State ──────────────────────────────────────────────────────────────
    mutable std::mutex m_mutex;
    std::atomic<bool>     m_active{false};
    std::atomic<uint64_t> m_nextWatch{1};
    std::atomic<uint64_t> m_nextBisect{1};
    std::atomic<uint64_t> m_nextCrash{1};

    DebugConfig m_config;

    std::vector<AutoWatchpoint>  m_watchpoints;
    std::vector<CrashReport>     m_crashes;
    std::vector<RootCauseAnalysis> m_rootCauses;
    std::unordered_map<uint64_t, BisectState> m_bisects;

    // Injected subsystems
    RCAAnalyzerFn        m_rcaAnalyzer;      void* m_rcaUD;
    SourceReaderFn       m_sourceReader;     void* m_sourceUD;
    BisectTestFn         m_bisectTester;     void* m_bisectTestUD;
    SetHWBreakpointFn    m_bpEngine;         void* m_bpUD;

    // Callbacks
    CrashDetectedCallback  m_crashCb;        void* m_crashCbUD;
    RootCauseFoundCallback m_rootCauseCb;    void* m_rootCauseCbUD;
    WatchpointHitCallback  m_watchpointCb;   void* m_watchpointCbUD;
    BisectProgressCallback m_bisectCb;       void* m_bisectCbUD;

    alignas(64) Stats m_stats;
};

} // namespace Debugging
} // namespace RawrXD

#endif // RAWRXD_AUTONOMOUS_DEBUGGER_HPP
