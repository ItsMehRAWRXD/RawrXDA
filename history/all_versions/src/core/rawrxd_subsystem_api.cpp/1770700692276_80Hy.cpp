/**
 * @file rawrxd_subsystem_api.cpp
 * @brief Implementation of the agent-callable subsystem registry
 *
 * Wraps CLI mode procedures into the SubsystemRegistry dispatch table.
 * Each mode handler calls the corresponding ASM procedure (linked via
 * extern "C") or the C++ equivalent, captures latency, and returns
 * a structured SubsystemResult.
 *
 * NO exceptions. NO std::function. NO Qt.
 * Contract: CLI_CONTRACT_v1.0.md
 */

#include "rawrxd_subsystem_api.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

// ============================================================
// ASM Procedure Linkage
// ============================================================
// These are the actual mode procedures from RawrXD_IDE_unified.asm.
// When linked as a unified binary, they're available directly.
// When used as a library, they're resolved at link time.

extern "C" {
    void CompileMode(void);
    void EncryptMode(void);
    void InjectMode(void);
    void UACBypassMode(void);
    void PersistenceMode(void);
    void SideloadMode(void);
    void AVScanMode(void);
    void EntropyMode(void);
    void StubGenMode(void);
    void TraceEngineMode(void);
    void AgenticMode(void);
    void BasicBlockCovMode(void);
}

// ============================================================
// Timing Helper
// ============================================================

static uint64_t GetTimestampMs() {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

// ============================================================
// Switch Name Table
// ============================================================

static const char* g_switchNames[] = {
    nullptr,        // index 0 unused
    "-compile",     // 1
    "-encrypt",     // 2
    "-inject",      // 3
    "-uac",         // 4
    "-persist",     // 5
    "-sideload",    // 6
    "-avscan",      // 7
    "-entropy",     // 8
    "-stubgen",     // 9
    "-trace",       // 10
    "-agent",       // 11
    "-bbcov"        // 12
};

// ============================================================
// SubsystemRegistry Constructor
// ============================================================

SubsystemRegistry::SubsystemRegistry()
    : m_eventCallback(nullptr)
    , m_eventUserData(nullptr) {

    // Zero-initialize all entries
    memset(m_modes, 0, sizeof(m_modes));

    // Register all mode handlers
    struct Registration {
        SubsystemId id;
        const char* sw;
        ModeHandler handler;
    };

    Registration regs[] = {
        { SubsystemId::Compile,   "-compile",  &SubsystemRegistry::handleCompile },
        { SubsystemId::Encrypt,   "-encrypt",  &SubsystemRegistry::handleEncrypt },
        { SubsystemId::Inject,    "-inject",   &SubsystemRegistry::handleInject },
        { SubsystemId::UACBypass, "-uac",      &SubsystemRegistry::handleUACBypass },
        { SubsystemId::Persist,   "-persist",  &SubsystemRegistry::handlePersist },
        { SubsystemId::Sideload,  "-sideload", &SubsystemRegistry::handleSideload },
        { SubsystemId::AVScan,    "-avscan",   &SubsystemRegistry::handleAVScan },
        { SubsystemId::Entropy,   "-entropy",  &SubsystemRegistry::handleEntropy },
        { SubsystemId::StubGen,   "-stubgen",  &SubsystemRegistry::handleStubGen },
        { SubsystemId::Trace,     "-trace",    &SubsystemRegistry::handleTrace },
        { SubsystemId::Agent,     "-agent",    &SubsystemRegistry::handleAgent },
        { SubsystemId::BBCov,     "-bbcov",    &SubsystemRegistry::handleBBCov },
    };

    for (const auto& r : regs) {
        int idx = static_cast<int>(r.id) - 1;
        if (idx >= 0 && idx < static_cast<int>(SubsystemId::_Count)) {
            m_modes[idx].id = r.id;
            m_modes[idx].switchName = r.sw;
            m_modes[idx].handler = r.handler;
            m_modes[idx].stats = {};
        }
    }
}

// ============================================================
// Core API Implementation
// ============================================================

SubsystemResult SubsystemRegistry::invoke(const SubsystemParams& params) {
    int idx = static_cast<int>(params.id) - 1;
    if (idx < 0 || idx >= static_cast<int>(SubsystemId::_Count)) {
        return SubsystemResult::error("Invalid subsystem ID", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    ModeEntry& entry = m_modes[idx];
    if (!entry.handler) {
        return SubsystemResult::error("Subsystem handler not registered", -2);
    }

    emitEvent(SubsystemEventType::ModeStarted, params.id, entry.switchName);

    uint64_t startMs = GetTimestampMs();
    SubsystemResult result = entry.handler(params);
    uint64_t endMs = GetTimestampMs();

    result.latencyMs = endMs - startMs;

    // Update statistics
    entry.stats.invocationCount++;
    entry.stats.totalLatencyMs += result.latencyMs;
    entry.stats.lastLatencyMs = result.latencyMs;
    if (!result.success) {
        entry.stats.failureCount++;
    }

    emitEvent(
        result.success ? SubsystemEventType::ModeCompleted : SubsystemEventType::ModeFailed,
        params.id,
        result.detail
    );

    if (result.artifactPath) {
        emitEvent(SubsystemEventType::ArtifactGenerated, params.id, result.artifactPath);
    }

    return result;
}

SubsystemResult SubsystemRegistry::invokeBySwitch(const char* switchStr) {
    if (!switchStr) {
        return SubsystemResult::error("Null switch string", -1);
    }

    // Skip leading '-' or '/' if present
    const char* name = switchStr;
    if (*name == '-' || *name == '/') {
        name++;
    }

    // Handle 'c' alias for compile
    if (name[0] == 'c' && name[1] == '\0') {
        SubsystemParams p{};
        p.id = SubsystemId::Compile;
        return invoke(p);
    }

    // Match against known switch names (compare without prefix)
    for (int i = 0; i < static_cast<int>(SubsystemId::_Count); i++) {
        if (m_modes[i].switchName) {
            // Compare 'name' against switchName+1 (skip the '-')
            const char* candidate = m_modes[i].switchName + 1;
            if (strcmp(name, candidate) == 0) {
                SubsystemParams p{};
                p.id = m_modes[i].id;
                return invoke(p);
            }
        }
    }

    return SubsystemResult::error("Unknown switch", -3);
}

bool SubsystemRegistry::isAvailable(SubsystemId id) const {
    int idx = static_cast<int>(id) - 1;
    if (idx < 0 || idx >= static_cast<int>(SubsystemId::_Count)) {
        return false;
    }
    return m_modes[idx].handler != nullptr;
}

const char* SubsystemRegistry::getSwitchName(SubsystemId id) const {
    int idx = static_cast<int>(id) - 1;
    if (idx < 0 || idx >= static_cast<int>(SubsystemId::_Count)) {
        return nullptr;
    }
    return m_modes[idx].switchName;
}

SubsystemRegistry::ModeStats SubsystemRegistry::getStats(SubsystemId id) const {
    int idx = static_cast<int>(id) - 1;
    if (idx < 0 || idx >= static_cast<int>(SubsystemId::_Count)) {
        ModeStats empty{};
        return empty;
    }
    return m_modes[idx].stats;
}

void SubsystemRegistry::emitEvent(SubsystemEventType type, SubsystemId mode, const char* detail) {
    if (m_eventCallback) {
        SubsystemEvent evt;
        evt.type = type;
        evt.mode = mode;
        evt.timestamp = GetTimestampMs();
        evt.detail = detail;
        m_eventCallback(&evt, m_eventUserData);
    }
}

// ============================================================
// Mode Handler Implementations
// ============================================================
// Each handler wraps the ASM procedure call in a structured result.
// The ASM procedures use WriteConsoleA for output and return via ret.
// We capture timing externally in invoke().

SubsystemResult SubsystemRegistry::handleCompile(const SubsystemParams& /*params*/) {
    CompileMode();
    return SubsystemResult::ok("Compile: Inline trace engine generation complete", 0, "trace_map.json");
}

SubsystemResult SubsystemRegistry::handleEncrypt(const SubsystemParams& /*params*/) {
    EncryptMode();
    return SubsystemResult::ok("Encrypt: Camellia-256 operation complete", 0, "encrypted.bin");
}

SubsystemResult SubsystemRegistry::handleInject(const SubsystemParams& params) {
    if (params.inject.pid == 0 && !params.inject.processName) {
        return SubsystemResult::ok("Inject: No target specified (usage: set pid or processName)");
    }
    // Note: The ASM InjectMode reads CLI_ArgBuffer directly.
    // For agent invocation, the caller must set up the arg buffer
    // or use the pid/pname parameters through a shim.
    InjectMode();
    return SubsystemResult::ok("Inject: Process injection executed");
}

SubsystemResult SubsystemRegistry::handleUACBypass(const SubsystemParams& /*params*/) {
    UACBypassMode();
    return SubsystemResult::ok("UAC: Bypass sequence executed");
}

SubsystemResult SubsystemRegistry::handlePersist(const SubsystemParams& /*params*/) {
    PersistenceMode();
    return SubsystemResult::ok("Persist: Registry persistence installed");
}

SubsystemResult SubsystemRegistry::handleSideload(const SubsystemParams& /*params*/) {
    SideloadMode();
    return SubsystemResult::ok("Sideload: DLL search-order demonstration complete");
}

SubsystemResult SubsystemRegistry::handleAVScan(const SubsystemParams& /*params*/) {
    AVScanMode();
    return SubsystemResult::ok("AVScan: PE analysis complete (self-scan)");
}

SubsystemResult SubsystemRegistry::handleEntropy(const SubsystemParams& /*params*/) {
    EntropyMode();
    return SubsystemResult::ok("Entropy: Shannon entropy analysis complete (self-scan)");
}

SubsystemResult SubsystemRegistry::handleStubGen(const SubsystemParams& params) {
    if (!params.stubgen.inputFile) {
        return SubsystemResult::ok("StubGen: No input file specified (usage: set inputFile)");
    }
    StubGenMode();
    return SubsystemResult::ok("StubGen: Self-decrypting stub generated", 0, "stub_output.bin");
}

SubsystemResult SubsystemRegistry::handleTrace(const SubsystemParams& params) {
    TraceEngineMode();
    if (params.trace.pid == 0) {
        return SubsystemResult::ok("Trace: Map-only mode (no debug attach)", 0, "trace_map.json");
    }
    return SubsystemResult::ok("Trace: Debug attach + trace map complete", 0, "trace_map.json");
}

SubsystemResult SubsystemRegistry::handleAgent(const SubsystemParams& /*params*/) {
    AgenticMode();
    return SubsystemResult::ok("Agent: Autonomous control loop executed");
}

SubsystemResult SubsystemRegistry::handleBBCov(const SubsystemParams& /*params*/) {
    BasicBlockCovMode();
    return SubsystemResult::ok("BBCov: Basic block coverage analysis complete", 0, "bbcov_report.json");
}
