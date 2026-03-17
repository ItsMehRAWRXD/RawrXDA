// =============================================================================
// autonomous_debugger.cpp
// RawrXD v14.2.1 Cathedral — Tier 1.4: Autonomous Debugging & Root Cause Analysis
//
// Implementation of autonomous crash analysis, watchpoint management,
// bisect debugging, and taint analysis.
// =============================================================================

#include "autonomous_debugger.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace RawrXD {
namespace Debugging {

// Forward declaration
static const char* bugClassificationStr(BugClassification c);

// =============================================================================
// Singleton
// =============================================================================

AutonomousDebugger& AutonomousDebugger::instance() {
    static AutonomousDebugger inst;
    return inst;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

AutonomousDebugger::AutonomousDebugger()
    : m_rcaAnalyzer(nullptr),   m_rcaUD(nullptr)
    , m_sourceReader(nullptr),  m_sourceUD(nullptr)
    , m_bisectTester(nullptr),  m_bisectTestUD(nullptr)
    , m_bpEngine(nullptr),      m_bpUD(nullptr)
    , m_crashCb(nullptr),       m_crashCbUD(nullptr)
    , m_rootCauseCb(nullptr),   m_rootCauseCbUD(nullptr)
    , m_watchpointCb(nullptr),  m_watchpointCbUD(nullptr)
    , m_bisectCb(nullptr),      m_bisectCbUD(nullptr)
{
    memset(&m_stats, 0, sizeof(m_stats));
}

AutonomousDebugger::~AutonomousDebugger() {
    shutdown();
}

// =============================================================================
// Lifecycle
// =============================================================================

DebugResult AutonomousDebugger::initialize(const DebugConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_active.load()) {
        return DebugResult::error("Already initialized");
    }

    m_config = config;
    m_watchpoints.reserve(static_cast<size_t>(config.maxWatchpoints));
    m_crashes.reserve(128);
    m_rootCauses.reserve(128);

    m_active.store(true);
    return DebugResult::ok("Autonomous debugger initialized");
}

DebugResult AutonomousDebugger::initialize() {
    return initialize(DebugConfig::defaults());
}

void AutonomousDebugger::shutdown() {
    if (!m_active.exchange(false)) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_watchpoints.clear();
    m_bisects.clear();
}

// =============================================================================
// Crash Classification
// =============================================================================

BugClassification AutonomousDebugger::classifyCrash(const CrashReport& crash) {
    // First try classifying based on exception message keywords
    const char* msg = crash.exceptionMessage;

    if (strstr(msg, "null") || strstr(msg, "NULL") || strstr(msg, "nullptr")) {
        return BugClassification::NullPointerDeref;
    }
    if (strstr(msg, "use after free") || strstr(msg, "heap-use-after-free")) {
        return BugClassification::UseAfterFree;
    }
    if (strstr(msg, "buffer overflow") || strstr(msg, "heap-buffer-overflow")
        || strstr(msg, "stack-buffer-overflow")) {
        return BugClassification::BufferOverflow;
    }
    if (strstr(msg, "integer overflow") || strstr(msg, "signed integer")) {
        return BugClassification::IntegerOverflow;
    }
    if (strstr(msg, "deadlock") || strstr(msg, "lock order")) {
        return BugClassification::Deadlock;
    }
    if (strstr(msg, "race") || strstr(msg, "data race") || strstr(msg, "TSAN")) {
        return BugClassification::RaceCondition;
    }
    if (strstr(msg, "stack overflow")) {
        return BugClassification::StackOverflow;
    }
    if (strstr(msg, "divide by zero") || strstr(msg, "division by zero")) {
        return BugClassification::DivideByZero;
    }
    if (strstr(msg, "assertion") || strstr(msg, "ASSERT")) {
        return BugClassification::AssertionFailure;
    }
    if (strstr(msg, "access violation") || strstr(msg, "SIGSEGV")
        || strstr(msg, "segfault")) {
        return BugClassification::AccessViolation;
    }

    // Fallback: classify from fault address patterns
    return classifyFromAddress(crash.faultAddress, crash.instructionPointer);
}

BugClassification AutonomousDebugger::classifyFromAddress(uint64_t faultAddr, uint64_t ip) {
    // Address 0 region: almost certainly null deref
    if (faultAddr < 0x10000) {
        return BugClassification::NullPointerDeref;
    }

    // Very high addresses on Windows may indicate stack overflow
#ifdef _WIN32
    if (faultAddr > 0x7FF000000000ULL) {
        return BugClassification::StackOverflow;
    }
#endif

    // If instruction pointer and fault address are far apart, possible buffer overflow
    if (ip > 0 && faultAddr > 0) {
        uint64_t delta = (faultAddr > ip) ? (faultAddr - ip) : (ip - faultAddr);
        if (delta > 0x100000) {
            return BugClassification::BufferOverflow;
        }
    }

    return BugClassification::Unknown;
}

BugClassification AutonomousDebugger::classifyFromCallStack(const StackFrame* frames, int count) {
    // Look for patterns in the call stack
    for (int i = 0; i < count; ++i) {
        const char* fn = frames[i].functionName;
        if (strstr(fn, "free") && i > 0 && strstr(frames[i - 1].functionName, "access")) {
            return BugClassification::UseAfterFree;
        }
        if (strstr(fn, "__alloca") || strstr(fn, "_chkstk")) {
            return BugClassification::StackOverflow;
        }
        if (strstr(fn, "RtlReportCriticalFailure")) {
            return BugClassification::BufferOverflow;
        }
        if (strstr(fn, "RtlpWaitOnCriticalSection")) {
            return BugClassification::Deadlock;
        }
    }
    return BugClassification::Unknown;
}

// =============================================================================
// Crash Analysis → Root Cause
// =============================================================================

RootCauseAnalysis AutonomousDebugger::analyzeCrash(const CrashReport& crash) {
    auto startTime = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);

    RootCauseAnalysis rca;
    memset(&rca, 0, sizeof(rca));
    rca.analysisId      = m_nextCrash.fetch_add(1);
    rca.classification  = classifyCrash(crash);

    // Step 1: Find the top user-code frame (skip system frames)
    int userFrame = -1;
    for (int i = 0; i < crash.frameCount; ++i) {
        // System frames typically don't have file info
        if (crash.callStack[i].fileName[0] != '\0'
            && crash.callStack[i].lineNumber > 0) {
            userFrame = i;
            break;
        }
    }

    if (userFrame >= 0) {
        const StackFrame& frame = crash.callStack[userFrame];
        strncpy(rca.rootCauseFile, frame.fileName, sizeof(rca.rootCauseFile) - 1);
        rca.rootCauseLine = frame.lineNumber;
        strncpy(rca.rootCauseFunction, frame.functionName, sizeof(rca.rootCauseFunction) - 1);
    }

    // Step 2: If an LLM root cause analyzer is set, use it for deep analysis
    if (m_rcaAnalyzer) {
        std::string sourceContext;
        if (m_sourceReader && userFrame >= 0) {
            const StackFrame& frame = crash.callStack[userFrame];
            int startLine = (frame.lineNumber > 20) ? frame.lineNumber - 20 : 1;
            int endLine = frame.lineNumber + 20;
            sourceContext = m_sourceReader(frame.fileName, startLine, endLine, m_sourceUD);
        }

        RootCauseAnalysis llmRCA = m_rcaAnalyzer(&crash, sourceContext.c_str(), m_rcaUD);
        // Merge LLM result into our analysis
        if (llmRCA.confidence > 0.0f) {
            strncpy(rca.rootCauseDescription, llmRCA.rootCauseDescription,
                     sizeof(rca.rootCauseDescription) - 1);
            strncpy(rca.suggestedFix, llmRCA.suggestedFix, sizeof(rca.suggestedFix) - 1);
            rca.autoFixable = llmRCA.autoFixable;
            rca.confidence  = llmRCA.confidence;
        }
    } else {
        // Heuristic-based description
        snprintf(rca.rootCauseDescription, sizeof(rca.rootCauseDescription),
                 "Crash classified as %s at %s:%d in function %s. Fault addr: 0x%llX",
                 bugClassificationStr(rca.classification),
                 rca.rootCauseFile, rca.rootCauseLine, rca.rootCauseFunction,
                 (unsigned long long)crash.faultAddress);
        rca.confidence = 0.5f;
    }

    // Step 3: Build propagation chain from call stack
    rca.propagationLen = 0;
    for (int i = userFrame; i < crash.frameCount && rca.propagationLen < 32; ++i) {
        if (crash.callStack[i].fileName[0] != '\0') {
            auto& step = rca.propagation[rca.propagationLen];
            strncpy(step.file, crash.callStack[i].fileName, sizeof(step.file) - 1);
            step.line = crash.callStack[i].lineNumber;
            snprintf(step.description, sizeof(step.description),
                     "Call through %s", crash.callStack[i].functionName);
            rca.propagationLen++;
        }
    }

    // Store for later retrieval
    m_crashes.push_back(crash);
    m_rootCauses.push_back(rca);

    // Update stats
    m_stats.totalCrashesAnalyzed++;
    m_stats.rootCausesFound++;

    auto elapsed = std::chrono::steady_clock::now() - startTime;
    double ms = std::chrono::duration<double, std::milli>(elapsed).count();
    double total = m_stats.avgAnalysisTimeMs * (m_stats.totalCrashesAnalyzed - 1) + ms;
    m_stats.avgAnalysisTimeMs = total / m_stats.totalCrashesAnalyzed;

    // Fire callbacks
    if (m_crashCb) {
        m_crashCb(&crash, m_crashCbUD);
    }
    if (m_rootCauseCb) {
        m_rootCauseCb(&rca, m_rootCauseCbUD);
    }

    return rca;
}

// Helper for classification names
static const char* bugClassificationStr(BugClassification c) {
    switch (c) {
        case BugClassification::NullPointerDeref: return "NullPointerDeref";
        case BugClassification::UseAfterFree:     return "UseAfterFree";
        case BugClassification::BufferOverflow:   return "BufferOverflow";
        case BugClassification::IntegerOverflow:  return "IntegerOverflow";
        case BugClassification::RaceCondition:    return "RaceCondition";
        case BugClassification::Deadlock:         return "Deadlock";
        case BugClassification::ResourceLeak:     return "ResourceLeak";
        case BugClassification::LogicError:       return "LogicError";
        case BugClassification::AssertionFailure: return "AssertionFailure";
        case BugClassification::StackOverflow:    return "StackOverflow";
        case BugClassification::DivideByZero:     return "DivideByZero";
        case BugClassification::AccessViolation:  return "AccessViolation";
        case BugClassification::Unknown:          return "Unknown";
        default:                                  return "Unknown";
    }
}

// =============================================================================
// Minidump / Core Dump Analysis
// =============================================================================

DebugResult AutonomousDebugger::analyzeCoreDump(const char* dumpPath) {
    if (!dumpPath || dumpPath[0] == '\0') {
        return DebugResult::error("Null dump path");
    }
    return analyzeMiniDump(dumpPath);
}

DebugResult AutonomousDebugger::analyzeMiniDump(const char* dumpPath) {
#ifdef _WIN32
    CrashReport crash;
    memset(&crash, 0, sizeof(crash));

    DebugResult parseResult = parseMinidump(dumpPath, crash);
    if (!parseResult.success) {
        return parseResult;
    }

    RootCauseAnalysis rca = analyzeCrash(crash);
    if (rca.confidence > 0.3f) {
        return DebugResult::ok("Minidump analyzed, root cause identified");
    }
    return DebugResult::ok("Minidump parsed but root cause confidence is low");
#else
    (void)dumpPath;
    return DebugResult::error("Minidump analysis not supported on this platform");
#endif
}

#ifdef _WIN32
DebugResult AutonomousDebugger::parseMinidump(const char* path, CrashReport& out) {
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return DebugResult::error("Failed to open minidump file", (int)GetLastError());
    }

    HANDLE mapping = CreateFileMappingA(file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!mapping) {
        CloseHandle(file);
        return DebugResult::error("Failed to map minidump file", (int)GetLastError());
    }

    void* base = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (!base) {
        CloseHandle(mapping);
        CloseHandle(file);
        return DebugResult::error("Failed to map view of minidump", (int)GetLastError());
    }

    // Extract exception record from minidump
    MINIDUMP_EXCEPTION_STREAM* excStream = nullptr;
    MINIDUMP_DIRECTORY* dir = nullptr;
    ULONG streamSize = 0;

    BOOL found = MiniDumpReadDumpStream(base, ExceptionStream,
                                         &dir, (PVOID*)&excStream, &streamSize);
    if (found && excStream) {
        MINIDUMP_EXCEPTION& exc = excStream->ExceptionRecord;
        out.faultAddress       = exc.ExceptionAddress;
        out.instructionPointer = excStream->ThreadContext.Rip;

        snprintf(out.exceptionMessage, sizeof(out.exceptionMessage),
                 "Exception 0x%08X at 0x%llX",
                 (unsigned)exc.ExceptionCode, (unsigned long long)exc.ExceptionAddress);
    }

    out.timestamp = (uint64_t)time(nullptr);
    out.crashId = m_nextCrash.fetch_add(1);

    UnmapViewOfFile(base);
    CloseHandle(mapping);
    CloseHandle(file);

    return DebugResult::ok("Minidump parsed successfully");
}
#endif

// =============================================================================
// Crash Reproduction
// =============================================================================

DebugResult AutonomousDebugger::reproduceCrash(const CrashReport& crash) {
    if (!m_config.autoReproduceBugs) {
        return DebugResult::error("Auto-reproduction disabled in config");
    }

    // We need the watchpoint + breakpoint engine to reproduce
    if (!m_bpEngine) {
        return DebugResult::error("No breakpoint engine configured");
    }

    // Set breakpoints at each frame in the call stack
    int bpCount = 0;
    for (int i = 0; i < crash.frameCount; ++i) {
        if (crash.callStack[i].address != 0) {
            bool set = m_bpEngine(crash.callStack[i].address, 1, m_bpUD);
            if (set) bpCount++;
        }
    }

    if (bpCount == 0) {
        return DebugResult::error("Failed to set any breakpoints for reproduction");
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "Set %d breakpoints for crash reproduction", bpCount);
    return DebugResult::ok(msg);
}

// =============================================================================
// Autonomous Watchpoint Management
// =============================================================================

DebugResult AutonomousDebugger::autoSetWatchpoints(const char* file) {
    return autoSetWatchpoints(nullptr, file, 0);
}

DebugResult AutonomousDebugger::autoSetWatchpoints(const char* functionName,
                                                     const char* file, int line) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_active.load()) {
        return DebugResult::error("Debugger not initialized");
    }

    // Read source code to find potential bug sources
    if (!m_sourceReader) {
        return DebugResult::error("No source reader configured");
    }

    int startLine = (line > 50) ? line - 50 : 1;
    int endLine   = line + 50;
    std::string source = m_sourceReader(file, startLine, endLine, m_sourceUD);

    if (source.empty()) {
        return DebugResult::error("Could not read source file");
    }

    // Heuristic: watch pointer variables, array indices, and loop counters
    // We look for patterns like ptr->, *ptr, arr[i], while/for loops
    generateAutoWatchpoints(functionName, file, line);

    char msg[256];
    snprintf(msg, sizeof(msg), "Auto-set %zu watchpoints for %s",
             m_watchpoints.size(), file);
    return DebugResult::ok(msg);
}

void AutonomousDebugger::generateAutoWatchpoints(const char* functionName,
                                                   const char* file, int line) {
    // Generate watchpoints using suspicion scoring
    // This is a heuristic; an LLM-based variant would be injected at runtime

    // Pattern: variables used in pointer dereferences get high suspicion
    AutoWatchpoint wp;
    memset(&wp, 0, sizeof(wp));
    wp.watchId = nextWatchId();
    wp.active = true;
    wp.action = WatchpointAction::TraceHistory;
    wp.line = line;

    if (file) strncpy(wp.file, file, sizeof(wp.file) - 1);
    if (functionName) {
        snprintf(wp.variableName, sizeof(wp.variableName),
                 "return_value_of_%s", functionName);
        wp.suspicion = calculateSuspicion(wp.variableName, functionName);
    }

    if (wp.suspicion >= m_config.suspicionThreshold) {
        m_watchpoints.push_back(wp);
        m_stats.watchpointsSet++;
    }
}

float AutonomousDebugger::calculateSuspicion(const char* variableName,
                                               const char* functionName) {
    float score = 0.3f; // Base suspicion

    // Pointer-related names are more suspicious
    if (strstr(variableName, "ptr") || strstr(variableName, "Ptr")
        || strstr(variableName, "pointer") || strstr(variableName, "Pointer")) {
        score += 0.3f;
    }

    // Buffer/array related
    if (strstr(variableName, "buf") || strstr(variableName, "Buf")
        || strstr(variableName, "buffer") || strstr(variableName, "array")) {
        score += 0.25f;
    }

    // Index variables
    if (strstr(variableName, "idx") || strstr(variableName, "index")
        || strstr(variableName, "offset") || strstr(variableName, "size")
        || strstr(variableName, "len") || strstr(variableName, "count")) {
        score += 0.2f;
    }

    // Return values of allocation functions
    if (functionName) {
        if (strstr(functionName, "alloc") || strstr(functionName, "malloc")
            || strstr(functionName, "new") || strstr(functionName, "create")
            || strstr(functionName, "open")) {
            score += 0.35f;
        }
        // Free-related functions are suspicious
        if (strstr(functionName, "free") || strstr(functionName, "delete")
            || strstr(functionName, "close") || strstr(functionName, "release")) {
            score += 0.3f;
        }
    }

    return (score > 1.0f) ? 1.0f : score;
}

uint64_t AutonomousDebugger::addWatchpoint(const char* variable, const char* file,
                                            int line, WatchpointAction action,
                                            const char* condition) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if ((int)m_watchpoints.size() >= m_config.maxWatchpoints) {
        return 0; // Too many watchpoints
    }

    AutoWatchpoint wp;
    memset(&wp, 0, sizeof(wp));
    wp.watchId = nextWatchId();
    strncpy(wp.variableName, variable, sizeof(wp.variableName) - 1);
    if (file)  strncpy(wp.file, file, sizeof(wp.file) - 1);
    wp.line = line;
    wp.action = action;
    wp.active = true;
    wp.suspicion = 0.5f;

    if (condition) {
        strncpy(wp.condition, condition, sizeof(wp.condition) - 1);
    }

    m_watchpoints.push_back(wp);
    m_stats.watchpointsSet++;

    return wp.watchId;
}

DebugResult AutonomousDebugger::removeWatchpoint(uint64_t watchId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_watchpoints.begin(), m_watchpoints.end(),
                            [watchId](const AutoWatchpoint& w) { return w.watchId == watchId; });
    if (it == m_watchpoints.end()) {
        return DebugResult::error("Watchpoint not found");
    }

    m_watchpoints.erase(it);
    return DebugResult::ok("Watchpoint removed");
}

DebugResult AutonomousDebugger::removeAllWatchpoints() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_watchpoints.clear();
    return DebugResult::ok("All watchpoints removed");
}

std::vector<AutoWatchpoint> AutonomousDebugger::getActiveWatchpoints() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<AutoWatchpoint> result;
    for (const auto& wp : m_watchpoints) {
        if (wp.active) result.push_back(wp);
    }
    return result;
}

std::vector<AutoWatchpoint> AutonomousDebugger::getTriggeredWatchpoints() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<AutoWatchpoint> result;
    for (const auto& wp : m_watchpoints) {
        if (wp.hitCount > 0) result.push_back(wp);
    }
    return result;
}

// =============================================================================
// Bisect Debugging
// =============================================================================

uint64_t AutonomousDebugger::startBisect(uint64_t goodState, uint64_t badState) {
    std::lock_guard<std::mutex> lock(m_mutex);

    BisectState bs;
    memset(&bs, 0, sizeof(bs));
    bs.bisectId    = nextBisectId();
    bs.goodCommit  = goodState;
    bs.badCommit   = badState;
    bs.currentTest = (goodState + badState) / 2;
    bs.completed   = false;
    bs.currentStep = 0;

    // Calculate expected steps (log2 of range)
    uint64_t range = (badState > goodState) ? (badState - goodState) : (goodState - badState);
    bs.totalSteps = 0;
    while (range > 0) {
        range >>= 1;
        bs.totalSteps++;
    }
    if (bs.totalSteps > m_config.maxBisectSteps) {
        bs.totalSteps = m_config.maxBisectSteps;
    }

    m_bisects[bs.bisectId] = bs;

    // Run bisect test on midpoint
    if (m_bisectTester) {
        bool passed = m_bisectTester(bs.currentTest, m_bisectTestUD);
        if (passed) {
            m_bisects[bs.bisectId].goodCommit = bs.currentTest;
        } else {
            m_bisects[bs.bisectId].badCommit = bs.currentTest;
        }
        // Advance to next midpoint
        auto& state = m_bisects[bs.bisectId];
        state.currentTest = (state.goodCommit + state.badCommit) / 2;
        state.currentStep++;
    }

    if (m_bisectCb) {
        m_bisectCb(&m_bisects[bs.bisectId], m_bisectCbUD);
    }

    return bs.bisectId;
}

DebugResult AutonomousDebugger::markBisectGood(uint64_t bisectId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_bisects.find(bisectId);
    if (it == m_bisects.end()) {
        return DebugResult::error("Bisect session not found");
    }

    BisectState& bs = it->second;
    if (bs.completed) {
        return DebugResult::error("Bisect already completed");
    }

    bs.goodCommit = bs.currentTest;
    bs.currentStep++;

    if (bs.badCommit - bs.goodCommit <= 1 || bs.currentStep >= bs.totalSteps) {
        bs.completed     = true;
        bs.foundCulprit  = bs.badCommit;
        m_stats.bisectsCompleted++;
        return DebugResult::ok("Bisect completed — culprit found");
    }

    bs.currentTest = (bs.goodCommit + bs.badCommit) / 2;

    if (m_bisectCb) {
        m_bisectCb(&bs, m_bisectCbUD);
    }

    return DebugResult::ok("Marked good, testing next midpoint");
}

DebugResult AutonomousDebugger::markBisectBad(uint64_t bisectId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_bisects.find(bisectId);
    if (it == m_bisects.end()) {
        return DebugResult::error("Bisect session not found");
    }

    BisectState& bs = it->second;
    if (bs.completed) {
        return DebugResult::error("Bisect already completed");
    }

    bs.badCommit = bs.currentTest;
    bs.currentStep++;

    if (bs.badCommit - bs.goodCommit <= 1 || bs.currentStep >= bs.totalSteps) {
        bs.completed     = true;
        bs.foundCulprit  = bs.badCommit;
        m_stats.bisectsCompleted++;
        return DebugResult::ok("Bisect completed — culprit found");
    }

    bs.currentTest = (bs.goodCommit + bs.badCommit) / 2;

    if (m_bisectCb) {
        m_bisectCb(&bs, m_bisectCbUD);
    }

    return DebugResult::ok("Marked bad, testing next midpoint");
}

BisectState AutonomousDebugger::getBisectState(uint64_t bisectId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_bisects.find(bisectId);
    if (it == m_bisects.end()) {
        BisectState empty;
        memset(&empty, 0, sizeof(empty));
        return empty;
    }
    return it->second;
}

DebugResult AutonomousDebugger::gitBisect(const char* goodCommit,
                                            const char* badCommit,
                                            const char* testCommand) {
    if (!goodCommit || !badCommit || !testCommand) {
        return DebugResult::error("Null argument to gitBisect");
    }

    // Shell out to git bisect
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "git bisect start %s %s && git bisect run %s && git bisect reset",
             badCommit, goodCommit, testCommand);

    int result = system(cmd);
    if (result != 0) {
        return DebugResult::error("git bisect failed", result);
    }

    m_stats.bisectsCompleted++;
    return DebugResult::ok("git bisect completed successfully");
}

// =============================================================================
// Taint Analysis
// =============================================================================

AutonomousDebugger::TaintChain AutonomousDebugger::traceBackwards(
    const char* variable, const char* file, int line) {

    TaintChain chain;
    memset(&chain, 0, sizeof(chain));

    strncpy(chain.originVariable, variable, sizeof(chain.originVariable) - 1);
    strncpy(chain.originFile, file, sizeof(chain.originFile) - 1);
    chain.originLine = line;

    if (!m_sourceReader) {
        // Without source reader, we can only report the starting point
        auto& link = chain.chain[0];
        strncpy(link.file, file, sizeof(link.file) - 1);
        link.line = line;
        snprintf(link.operation, sizeof(link.operation),
                 "Origin of '%s' (no source reader for deeper analysis)", variable);
        chain.chainLength = 1;
        return chain;
    }

    // Read source around the variable usage
    int startLine = (line > 100) ? line - 100 : 1;
    std::string source = m_sourceReader(file, startLine, line, m_sourceUD);

    // Simple heuristic: walk backward through lines looking for assignments
    // A real implementation would use the AST from StaticAnalysisEngine
    int chainIdx = 0;
    auto& firstLink = chain.chain[chainIdx++];
    strncpy(firstLink.file, file, sizeof(firstLink.file) - 1);
    firstLink.line = line;
    snprintf(firstLink.operation, sizeof(firstLink.operation),
             "Variable '%s' used here", variable);

    // Look for assignment patterns: "variable =" or "variable ="
    // This is a simplified backward scan
    std::string varStr(variable);
    size_t pos = source.rfind(varStr);
    while (pos != std::string::npos && chainIdx < 64) {
        // Count line number
        int lineCount = startLine;
        for (size_t i = 0; i < pos; ++i) {
            if (source[i] == '\n') lineCount++;
        }

        // Check if this is an assignment
        size_t afterVar = pos + varStr.length();
        bool isAssignment = false;
        for (size_t i = afterVar; i < source.length(); ++i) {
            if (source[i] == ' ' || source[i] == '\t') continue;
            if (source[i] == '=') {
                isAssignment = true;
            }
            break;
        }

        if (isAssignment && lineCount != line) {
            auto& link = chain.chain[chainIdx++];
            strncpy(link.file, file, sizeof(link.file) - 1);
            link.line = lineCount;
            snprintf(link.operation, sizeof(link.operation),
                     "Variable '%s' assigned at line %d", variable, lineCount);
        }

        if (pos == 0) break;
        pos = source.rfind(varStr, pos - 1);
    }

    chain.chainLength = chainIdx;
    return chain;
}

// =============================================================================
// Subsystem Injection
// =============================================================================

void AutonomousDebugger::setRootCauseAnalyzer(RCAAnalyzerFn fn, void* ud) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rcaAnalyzer = fn;
    m_rcaUD = ud;
}

void AutonomousDebugger::setSourceReader(SourceReaderFn fn, void* ud) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sourceReader = fn;
    m_sourceUD = ud;
}

void AutonomousDebugger::setBisectTester(BisectTestFn fn, void* ud) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_bisectTester = fn;
    m_bisectTestUD = ud;
}

void AutonomousDebugger::setBreakpointEngine(SetHWBreakpointFn fn, void* ud) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_bpEngine = fn;
    m_bpUD = ud;
}

void AutonomousDebugger::setCrashCallback(CrashDetectedCallback cb, void* ud) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_crashCb = cb;
    m_crashCbUD = ud;
}

void AutonomousDebugger::setRootCauseCallback(RootCauseFoundCallback cb, void* ud) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_rootCauseCb = cb;
    m_rootCauseCbUD = ud;
}

void AutonomousDebugger::setWatchpointCallback(WatchpointHitCallback cb, void* ud) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_watchpointCb = cb;
    m_watchpointCbUD = ud;
}

void AutonomousDebugger::setBisectProgressCallback(BisectProgressCallback cb, void* ud) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_bisectCb = cb;
    m_bisectCbUD = ud;
}

// =============================================================================
// ID Generation
// =============================================================================

uint64_t AutonomousDebugger::nextWatchId() {
    return m_nextWatch.fetch_add(1);
}

uint64_t AutonomousDebugger::nextBisectId() {
    return m_nextBisect.fetch_add(1);
}

uint64_t AutonomousDebugger::nextCrashId() {
    return m_nextCrash.fetch_add(1);
}

// =============================================================================
// Stats & JSON
// =============================================================================

AutonomousDebugger::Stats AutonomousDebugger::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

std::string AutonomousDebugger::statsToJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{"
        "\"totalCrashesAnalyzed\":%llu,"
        "\"rootCausesFound\":%llu,"
        "\"watchpointsSet\":%llu,"
        "\"watchpointsTriggered\":%llu,"
        "\"bisectsCompleted\":%llu,"
        "\"autoFixesGenerated\":%llu,"
        "\"avgAnalysisTimeMs\":%.2f"
        "}",
        (unsigned long long)m_stats.totalCrashesAnalyzed,
        (unsigned long long)m_stats.rootCausesFound,
        (unsigned long long)m_stats.watchpointsSet,
        (unsigned long long)m_stats.watchpointsTriggered,
        (unsigned long long)m_stats.bisectsCompleted,
        (unsigned long long)m_stats.autoFixesGenerated,
        m_stats.avgAnalysisTimeMs);
    return std::string(buf);
}

std::string AutonomousDebugger::crashReportToJson(const CrashReport& crash) const {
    char buf[2048];
    snprintf(buf, sizeof(buf),
        "{"
        "\"crashId\":%llu,"
        "\"classification\":\"%s\","
        "\"faultAddress\":\"0x%llX\","
        "\"instructionPointer\":\"0x%llX\","
        "\"frameCount\":%d,"
        "\"exceptionMessage\":\"%s\""
        "}",
        (unsigned long long)crash.crashId,
        bugClassificationStr(crash.classification),
        (unsigned long long)crash.faultAddress,
        (unsigned long long)crash.instructionPointer,
        crash.frameCount,
        crash.exceptionMessage);
    return std::string(buf);
}

std::string AutonomousDebugger::rootCauseToJson(const RootCauseAnalysis& rca) const {
    char buf[4096];
    snprintf(buf, sizeof(buf),
        "{"
        "\"analysisId\":%llu,"
        "\"classification\":\"%s\","
        "\"rootCauseFile\":\"%s\","
        "\"rootCauseLine\":%d,"
        "\"rootCauseFunction\":\"%s\","
        "\"confidence\":%.3f,"
        "\"autoFixable\":%s,"
        "\"propagationSteps\":%d"
        "}",
        (unsigned long long)rca.analysisId,
        bugClassificationStr(rca.classification),
        rca.rootCauseFile,
        rca.rootCauseLine,
        rca.rootCauseFunction,
        rca.confidence,
        rca.autoFixable ? "true" : "false",
        rca.propagationLen);
    return std::string(buf);
}

} // namespace Debugging
} // namespace RawrXD
