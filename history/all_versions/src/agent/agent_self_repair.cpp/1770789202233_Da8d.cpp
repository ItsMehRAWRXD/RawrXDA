// agent_self_repair.cpp — Enterprise-Safe Agent Self-Repair Implementation
//
// Tier-3-compliant: NO byte-level mutation. ALL repairs are function-level
// redirections via LiveBinaryPatcher with thread suspension, CRC guards,
// telemetry emission, and deterministic replay checkpoints.
//
// Architecture: C++20 bridge → LiveBinaryPatcher (Layer 5) + MASM64 scan kernel
// Error model: PatchResult::ok() / PatchResult::error() — no exceptions
// Threading: std::mutex + std::lock_guard + SuspendThread barrier
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include "agent_self_repair.hpp"
#include <cstring>
#include <cstdio>

// ---------------------------------------------------------------------------
// Built-in bug signatures — READ-ONLY detection patterns (no fix bytes)
// These are used for scanning and reporting only. The agent never applies
// byte-level fixes. If a signature triggers, the response is to redirect
// the containing function via LiveBinaryPatcher trampoline.
// ---------------------------------------------------------------------------

// UD2 (undefined instruction) left in release code
static const uint8_t kBugSig_UD2[]  = { 0x0F, 0x0B };

// INT 3 sled left in release (debugger artifact)
static const uint8_t kBugSig_Int3Sled[]  = { 0xCC, 0xCC, 0xCC, 0xCC };

// JMP to self (infinite loop bug: EB FE)
static const uint8_t kBugSig_JmpSelf[]  = { 0xEB, 0xFE };

// MOV to null pointer (access violation: MOV [0], RAX)
static const uint8_t kBugSig_MovNull[]  = { 0x48, 0x89, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00 };

// Null bytes after CALL (missing epilogue)
static const uint8_t kBugSig_NullAfterCall[]  = { 0xE8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// Double-free entry pattern
static const uint8_t kBugSig_DoubleFreeEntry[]  = { 0x48, 0x85, 0xC9, 0x75, 0x00, 0xFF };

// CRC32 constant
static const uint32_t kCRC32Poly = 0xEDB88320u;

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
AgentSelfRepair& AgentSelfRepair::instance() {
    static AgentSelfRepair s_instance;
    return s_instance;
}

AgentSelfRepair::AgentSelfRepair()
    : m_initialized(false)
    , m_textBase(0)
    , m_textSize(0)
{
    std::memset(&m_telemetry, 0, sizeof(m_telemetry));
}

AgentSelfRepair::~AgentSelfRepair() = default;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
PatchResult AgentSelfRepair::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        return PatchResult::ok("Already initialized");
    }

#ifdef RAWR_HAS_MASM
    int rc = asm_selfpatch_init();
    if (rc != 0 && rc != 1 /* 1 = already init */) {
        return PatchResult::error("ASM self-patch init failed", rc);
    }
#endif

    // Initialize LiveBinaryPatcher (Layer 5)
    PatchResult lbpInit = LiveBinaryPatcher::instance().initialize();
    if (!lbpInit.success) {
        // Non-fatal: continue without live patching capability
        // The scan-only mode still works
    }

    if (!locateTextSection()) {
        return PatchResult::error("Failed to locate .text section");
    }

    m_initialized = true;
    loadBuiltinSignatures();

    emitTelemetry("AgentSelfRepair: initialized (enterprise-safe mode)");

    return PatchResult::ok("Agent self-repair initialized (function-level only)");
}

bool AgentSelfRepair::isInitialized() const {
    return m_initialized;
}

// ---------------------------------------------------------------------------
// PE .text section location
// ---------------------------------------------------------------------------
bool AgentSelfRepair::locateTextSection() {
    HMODULE hModule = GetModuleHandleA(nullptr);
    if (!hModule) return false;

    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(hModule);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;

    auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS64*>(
        reinterpret_cast<uintptr_t>(hModule) + dosHeader->e_lfanew
    );
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return false;

    WORD sectionCount = ntHeaders->FileHeader.NumberOfSections;
    auto* section = IMAGE_FIRST_SECTION(ntHeaders);

    for (WORD i = 0; i < sectionCount; ++i) {
        if (std::memcmp(section[i].Name, ".text", 5) == 0) {
            m_textBase = reinterpret_cast<uintptr_t>(hModule) + section[i].VirtualAddress;
            m_textSize = section[i].Misc.VirtualSize;
            return true;
        }
    }

    // Fallback: first executable section
    for (WORD i = 0; i < sectionCount; ++i) {
        if (section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) {
            m_textBase = reinterpret_cast<uintptr_t>(hModule) + section[i].VirtualAddress;
            m_textSize = section[i].Misc.VirtualSize;
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// CRC32 computation (software, for function prologue verification)
// ---------------------------------------------------------------------------
uint32_t AgentSelfRepair::computeFunctionCRC(void* entry) const {
    if (!entry) return 0;

    // CRC32 over the first 64 bytes of the function (the prologue)
    auto* bytes = static_cast<const uint8_t*>(entry);
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < 64; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 1) crc = (crc >> 1) ^ kCRC32Poly;
            else         crc >>= 1;
        }
    }

    return ~crc;
}

// ---------------------------------------------------------------------------
// Bug Signature Registry (read-only detection patterns)
// ---------------------------------------------------------------------------
PatchResult AgentSelfRepair::registerBugSignature(const BugSignature& sig) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!sig.name || !sig.pattern || sig.patternLen == 0) {
        return PatchResult::error("Invalid bug signature");
    }

    m_signatures.push_back(sig);
    return PatchResult::ok("Bug signature registered");
}

size_t AgentSelfRepair::getBugSignatureCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_signatures.size();
}

const std::vector<BugSignature>& AgentSelfRepair::getSignatures() const {
    return m_signatures;
}

void AgentSelfRepair::loadBuiltinSignatures() {
    // All signatures are escalateOnly or detection-only.
    // No byte-level fixes. If detected, the containing function
    // must be redirected via RepairableFunction registry.

    auto addSig = [&](const char* name, const uint8_t* pat, size_t len,
                       uint32_t sev, bool escalate) {
        BugSignature sig{};
        sig.name         = name;
        sig.pattern      = pat;
        sig.patternLen   = len;
        sig.severity     = sev;
        sig.escalateOnly = escalate;
        m_signatures.push_back(sig);
    };

    addSig("UD2InRelease",     kBugSig_UD2,             sizeof(kBugSig_UD2),             3, false);
    addSig("Int3Sled",         kBugSig_Int3Sled,        sizeof(kBugSig_Int3Sled),        1, false);
    addSig("JmpToSelf",        kBugSig_JmpSelf,         sizeof(kBugSig_JmpSelf),         3, false);
    addSig("MovToNull",        kBugSig_MovNull,         sizeof(kBugSig_MovNull),         3, false);
    addSig("NullAfterCall",    kBugSig_NullAfterCall,   sizeof(kBugSig_NullAfterCall),   2, true);
    addSig("DoubleFreeEntry",  kBugSig_DoubleFreeEntry, sizeof(kBugSig_DoubleFreeEntry), 2, true);
}

// ---------------------------------------------------------------------------
// RepairableFunction Registry
// ---------------------------------------------------------------------------
PatchResult AgentSelfRepair::registerRepairableFunction(const RepairableFunction& func) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!func.name || !func.entry) {
        return PatchResult::error("Invalid repairable function: null name or entry");
    }
    if (!func.fallbackImpl) {
        return PatchResult::error("Invalid repairable function: no fallback implementation");
    }

    // Register with LiveBinaryPatcher to get a slot ID
    RepairableFunction rf = func;
    uint32_t slotId = 0;
    PatchResult regResult = LiveBinaryPatcher::instance().register_function(
        func.name, reinterpret_cast<uintptr_t>(func.entry), &slotId
    );

    if (!regResult.success) {
        return PatchResult::error("LiveBinaryPatcher registration failed");
    }

    rf.slotId = slotId;

    // Compute expected CRC32 of the function prologue (first 64 bytes)
    if (rf.expectedCRC == 0) {
        rf.expectedCRC = computeFunctionCRC(func.entry);
    }

    rf.isRedirected = false;
    m_repairables.push_back(rf);

    emitTelemetry("AgentSelfRepair: registered repairable function");
    return PatchResult::ok("Repairable function registered");
}

size_t AgentSelfRepair::getRepairableFunctionCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_repairables.size();
}

const std::vector<RepairableFunction>& AgentSelfRepair::getRepairableFunctions() const {
    return m_repairables;
}

// ---------------------------------------------------------------------------
// Scanning (read-only — uses MASM64 kernel for pattern search)
// ---------------------------------------------------------------------------
size_t AgentSelfRepair::scanSelf() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized || m_textBase == 0 || m_textSize == 0) {
        return 0;
    }

    return scanRegion(reinterpret_cast<const void*>(m_textBase), m_textSize);
}

size_t AgentSelfRepair::scanRegion(const void* base, size_t length) {
    if (!base || length == 0) return 0;

    size_t totalBugs = 0;

    for (const auto& sig : m_signatures) {
#ifdef RAWR_HAS_MASM
        // Use the MASM64 kernel for read-only pattern scanning
        void* result = asm_selfpatch_scan_text(base, length, sig.pattern, sig.patternLen);
        if (!result) continue;

        auto* sr = reinterpret_cast<SelfPatchScanResult*>(result);

        if (sr->statusCode == 1 /* SCAN_PATTERN_FOUND */ && sr->matchCount > 0) {
            BugReport report = BugReport::found(&sig, sr->firstMatch, sr->matchCount);
            report.timestamp = GetTickCount64();

            m_reports.push_back(report);
            notifyBugDetected(report);
            totalBugs += sr->matchCount;

            // Emit telemetry for each detection
            incrementTelemetryCounter(&m_telemetry.bugsDetected);
        }
#else
        // C++ fallback — linear scan
        auto* haystack = static_cast<const uint8_t*>(base);
        for (size_t i = 0; i + sig.patternLen <= length; ++i) {
            if (std::memcmp(haystack + i, sig.pattern, sig.patternLen) == 0) {
                uintptr_t addr = reinterpret_cast<uintptr_t>(base) + i;
                BugReport report = BugReport::found(&sig, addr, 1);
                report.timestamp = GetTickCount64();
                m_reports.push_back(report);
                notifyBugDetected(report);
                ++totalBugs;
                incrementTelemetryCounter(&m_telemetry.bugsDetected);
            }
        }
#endif
    }

    // Also scan for NOP sleds
    int nopSleds = scanForNopSleds(base, length);
    totalBugs += static_cast<size_t>(nopSleds > 0 ? nopSleds : 0);

    return totalBugs;
}

int AgentSelfRepair::scanForNopSleds(const void* base, size_t length) {
#ifdef RAWR_HAS_MASM
    return asm_selfpatch_scan_nop_sled(base, length);
#else
    auto* bytes = static_cast<const uint8_t*>(base);
    int sledCount = 0;
    int consecutive = 0;

    for (size_t i = 0; i < length; ++i) {
        if (bytes[i] == 0x90 || bytes[i] == 0xCC) {
            ++consecutive;
        } else {
            if (consecutive >= 8) ++sledCount;
            consecutive = 0;
        }
    }
    if (consecutive >= 8) ++sledCount;
    return sledCount;
#endif
}

// ---------------------------------------------------------------------------
// Thread Suspension Barrier
// Suspends all threads in the process except the calling thread.
// REQUIRED before any trampoline installation to prevent torn instruction reads.
// ---------------------------------------------------------------------------
bool AgentSelfRepair::suspendOtherThreads(std::vector<HANDLE>& outHandles) {
    outHandles.clear();
    DWORD currentTid = GetCurrentThreadId();
    DWORD currentPid = GetCurrentProcessId();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    THREADENTRY32 te;
    te.dwSize = sizeof(te);

    if (Thread32First(snapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == currentPid && te.th32ThreadID != currentTid) {
                HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT,
                                            FALSE, te.th32ThreadID);
                if (hThread) {
                    DWORD suspendCount = SuspendThread(hThread);
                    if (suspendCount != static_cast<DWORD>(-1)) {
                        outHandles.push_back(hThread);
                    } else {
                        CloseHandle(hThread);
                    }
                }
            }
        } while (Thread32Next(snapshot, &te));
    }

    CloseHandle(snapshot);
    incrementTelemetryCounter(&m_telemetry.threadSuspensions);
    return true;
}

void AgentSelfRepair::resumeOtherThreads(const std::vector<HANDLE>& handles) {
    for (HANDLE h : handles) {
        ResumeThread(h);
        CloseHandle(h);
    }
}

// ---------------------------------------------------------------------------
// Stack Safety Check
// Verify that the target address is not on any thread's active stack.
// If it is, patching would corrupt a live return address.
// ---------------------------------------------------------------------------
bool AgentSelfRepair::isAddressOnAnyStack(uintptr_t addr) const {
    DWORD currentPid = GetCurrentProcessId();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    bool onStack = false;
    THREADENTRY32 te;
    te.dwSize = sizeof(te);

    if (Thread32First(snapshot, &te)) {
        do {
            if (te.th32OwnerProcessID != currentPid) continue;

            HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT,
                                        FALSE, te.th32ThreadID);
            if (!hThread) continue;

            // Get thread's stack bounds via NT_TIB
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL;
            if (GetThreadContext(hThread, &ctx)) {
                // RSP is the current stack pointer
                // Stack grows downward: [RSP, StackBase)
                // Conservative: check if addr is within 1MB below stack base
                uintptr_t rsp = ctx.Rsp;
                if (addr >= rsp && addr < rsp + (1024 * 1024)) {
                    onStack = true;
                }
            }

            CloseHandle(hThread);
            if (onStack) break;

        } while (Thread32Next(snapshot, &te));
    }

    CloseHandle(snapshot);
    return onStack;
}

// ---------------------------------------------------------------------------
// Self-Repair: Function-Level Redirection
// ---------------------------------------------------------------------------
PatchResult AgentSelfRepair::verifyAndRepairAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        return PatchResult::error("Not initialized");
    }

    int repaired = 0;
    int failed   = 0;
    int clean    = 0;

    for (auto& func : m_repairables) {
        if (func.isRedirected) continue; // Already redirected

        // Verify CRC of the function's prologue
        uint32_t currentCRC = computeFunctionCRC(func.entry);
        if (currentCRC == func.expectedCRC) {
            ++clean;
            continue; // Function is intact
        }

        // CRC mismatch detected
        if (!func.autoRepair) {
            // Report but don't fix
            ++failed;
            continue;
        }

        // Attempt redirect
        PatchResult r = redirectFunction(func.slotId);
        if (r.success) {
            func.isRedirected = true;
            ++repaired;
        } else {
            ++failed;
        }
    }

    if (failed > 0) {
        return PatchResult::error("Some functions failed CRC check", failed);
    }
    if (repaired > 0) {
        return PatchResult::ok("Functions redirected to fallbacks");
    }
    return PatchResult::ok("All functions intact — no repair needed");
}

PatchResult AgentSelfRepair::redirectFunction(uint32_t slotId) {
    // Note: m_mutex must be held by caller

    if (!m_initialized) {
        return PatchResult::error("Not initialized");
    }

    // Find the repairable function entry
    RepairableFunction* target = nullptr;
    for (auto& func : m_repairables) {
        if (func.slotId == slotId) {
            target = &func;
            break;
        }
    }
    if (!target) {
        return PatchResult::error("Unknown slot ID — function not registered");
    }
    if (target->isRedirected) {
        return PatchResult::ok("Already redirected");
    }

    // Stack safety check: don't patch a function that's currently executing
    if (isAddressOnAnyStack(reinterpret_cast<uintptr_t>(target->entry))) {
        return PatchResult::error("Cannot redirect — function is on active stack");
    }

    // Deterministic replay checkpoint: pre-patch
    emitReplayCheckpoint("pre_redirect", reinterpret_cast<uintptr_t>(target->entry));

    // Suspend all other threads (two-phase patch safety)
    std::vector<HANDLE> suspendedThreads;
    if (!suspendOtherThreads(suspendedThreads)) {
        return PatchResult::error("Failed to suspend threads for safe patching");
    }

    // Install trampoline via LiveBinaryPatcher
    PatchResult result = LiveBinaryPatcher::instance().install_trampoline(slotId);

    // Now swap the target to the fallback implementation
    if (result.success) {
        // The LiveBinaryPatcher trampoline redirects entry → trampoline page.
        // We need to point the trampoline target at our fallback.
        // Use swap_implementation to redirect to fallback code.
        auto* fallback = static_cast<const uint8_t*>(target->fallbackImpl);
        result = LiveBinaryPatcher::instance().swap_implementation(
            slotId, fallback, 0, nullptr, 0
        );
    }

    // Resume all threads
    resumeOtherThreads(suspendedThreads);

    if (result.success) {
        target->isRedirected = true;
        m_activeRedirections.push_back(slotId);

        // Telemetry
        incrementTelemetryCounter(&m_telemetry.patchesApplied);
#ifdef RAWR_HAS_MASM
        UTC_IncrementCounter(&g_Counter_MemPatches);
#endif
        emitTelemetry("AgentSelfRepair: function redirected to fallback");

        // Deterministic replay checkpoint: post-patch
        emitReplayCheckpoint("post_redirect", reinterpret_cast<uintptr_t>(target->entry));

        notifySelfPatch(slotId, true);

        return PatchResult::ok("Function redirected to known-good fallback");
    }

    incrementTelemetryCounter(&m_telemetry.crcVerifyFails);
#ifdef RAWR_HAS_MASM
    UTC_IncrementCounter(&g_Counter_Errors);
#endif
    emitTelemetry("AgentSelfRepair: function redirect FAILED");
    notifySelfPatch(slotId, false);

    return PatchResult::error("LiveBinaryPatcher trampoline install failed");
}

PatchResult AgentSelfRepair::revertFunction(uint32_t slotId) {
    // Note: m_mutex must be held by caller

    RepairableFunction* target = nullptr;
    for (auto& func : m_repairables) {
        if (func.slotId == slotId) {
            target = &func;
            break;
        }
    }
    if (!target || !target->isRedirected) {
        return PatchResult::error("Function not redirected or unknown slot");
    }

    // Suspend threads for safe revert
    std::vector<HANDLE> suspendedThreads;
    suspendOtherThreads(suspendedThreads);

    PatchResult result = LiveBinaryPatcher::instance().revert_trampoline(slotId);

    resumeOtherThreads(suspendedThreads);

    if (result.success) {
        target->isRedirected = false;

        // Remove from active list
        for (auto it = m_activeRedirections.begin(); it != m_activeRedirections.end(); ++it) {
            if (*it == slotId) {
                m_activeRedirections.erase(it);
                break;
            }
        }

        incrementTelemetryCounter(&m_telemetry.patchesRolledBack);
        emitTelemetry("AgentSelfRepair: function revert successful");
        emitReplayCheckpoint("post_revert", reinterpret_cast<uintptr_t>(target->entry));
    }

    return result;
}

// ---------------------------------------------------------------------------
// Atomic CAS for callback/vtable pointer patching
// ---------------------------------------------------------------------------
PatchResult AgentSelfRepair::casPatchPointer(void** slot, void* expected, void* desired) {
    if (!slot) {
        return PatchResult::error("Null slot pointer");
    }

#ifdef RAWR_HAS_MASM
    int rc = asm_selfpatch_cas_patch(slot,
                                      reinterpret_cast<uint64_t>(expected),
                                      reinterpret_cast<uint64_t>(desired));
    if (rc != 0) {
        return PatchResult::error("CAS patch failed — value changed concurrently", rc);
    }
    return PatchResult::ok("Atomic pointer patch applied");
#else
    void* actual = InterlockedCompareExchangePointer(slot, desired, expected);
    if (actual != expected) {
        return PatchResult::error("CAS failed — stale expected value");
    }
    return PatchResult::ok("Pointer patched atomically");
#endif
}

// ---------------------------------------------------------------------------
// Verification
// ---------------------------------------------------------------------------
PatchResult AgentSelfRepair::verifyFunctionCRC(uint32_t slotId) const {
    for (const auto& func : m_repairables) {
        if (func.slotId == slotId) {
            if (func.isRedirected) {
                return PatchResult::ok("Function is redirected — CRC check skipped");
            }
            uint32_t currentCRC = computeFunctionCRC(func.entry);
            if (currentCRC == func.expectedCRC) {
                return PatchResult::ok("CRC match — function is intact");
            }
            return PatchResult::error("CRC mismatch — function may be corrupted");
        }
    }
    return PatchResult::error("Unknown slot ID");
}

PatchResult AgentSelfRepair::verifyAllFunctions() const {
    int verified = 0;
    int mismatched = 0;

    for (const auto& func : m_repairables) {
        if (func.isRedirected) {
            ++verified;
            continue;
        }
        uint32_t currentCRC = computeFunctionCRC(func.entry);
        if (currentCRC == func.expectedCRC) {
            ++verified;
        } else {
            ++mismatched;
        }
    }

    if (mismatched > 0) {
        return PatchResult::error("Some functions failed CRC verification", mismatched);
    }
    return PatchResult::ok("All functions verified");
}

PatchResult AgentSelfRepair::verifyAllPatches() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Delegate to LiveBinaryPatcher's full integrity check
    return LiveBinaryPatcher::instance().verify_integrity();
}

// ---------------------------------------------------------------------------
// Rollback
// ---------------------------------------------------------------------------
PatchResult AgentSelfRepair::rollbackFunction(uint32_t slotId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return revertFunction(slotId);
}

PatchResult AgentSelfRepair::rollbackAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    int reverted = 0;
    int failed   = 0;

    // Copy list since revertFunction modifies it
    auto ids = m_activeRedirections;
    for (uint32_t id : ids) {
        PatchResult r = revertFunction(id);
        if (r.success) ++reverted;
        else ++failed;
    }

    if (failed > 0) {
        return PatchResult::error("Some reverts failed", failed);
    }
    return PatchResult::ok("All functions reverted to original");
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------
SelfPatchStats AgentSelfRepair::getStats() const {
    SelfPatchStats stats{};

#ifdef RAWR_HAS_MASM
    void* raw = asm_selfpatch_get_stats();
    if (raw) {
        std::memcpy(&stats, raw, sizeof(SelfPatchStats));
    }
#endif

    return stats;
}

const SelfPatchTelemetry& AgentSelfRepair::getTelemetry() const {
    return m_telemetry;
}

const std::vector<BugReport>& AgentSelfRepair::getReports() const {
    return m_reports;
}

// ---------------------------------------------------------------------------
// Telemetry Emission
// ---------------------------------------------------------------------------
void AgentSelfRepair::emitTelemetry(const char* event) {
#ifdef RAWR_HAS_MASM
    UTC_LogEvent(event);
#else
    OutputDebugStringA(event);
    OutputDebugStringA("\n");
#endif
}

void AgentSelfRepair::incrementTelemetryCounter(volatile uint64_t* counter) {
    if (!counter) return;
#ifdef RAWR_HAS_MASM
    UTC_IncrementCounter(counter);
#else
    InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(counter));
#endif
}

// ---------------------------------------------------------------------------
// Deterministic Replay Checkpoint
// ---------------------------------------------------------------------------
void AgentSelfRepair::emitReplayCheckpoint(const char* tag, uintptr_t address) {
    incrementTelemetryCounter(&m_telemetry.replayCheckpoints);

    // Log the checkpoint event for the replay engine to capture
    char msg[256];
    _snprintf_s(msg, sizeof(msg), _TRUNCATE,
                "SelfRepair::Checkpoint [%s] addr=0x%llX",
                tag, static_cast<unsigned long long>(address));
    emitTelemetry(msg);
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
void AgentSelfRepair::registerBugCallback(BugDetectedCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (cb) {
        m_bugCallbacks.push_back({cb, userData});
    }
}

void AgentSelfRepair::registerPatchCallback(SelfPatchCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (cb) {
        m_patchCallbacks.push_back({cb, userData});
    }
}

void AgentSelfRepair::notifyBugDetected(const BugReport& report) {
    for (const auto& cb : m_bugCallbacks) {
        cb.fn(&report, cb.userData);
    }
}

void AgentSelfRepair::notifySelfPatch(uint64_t patchId, bool success) {
    for (const auto& cb : m_patchCallbacks) {
        cb.fn(patchId, success, cb.userData);
    }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------
bool AgentSelfRepair::getTextSection(uintptr_t* outBase, size_t* outSize) const {
    if (!m_initialized) return false;
    if (outBase) *outBase = m_textBase;
    if (outSize) *outSize = m_textSize;
    return true;
}

size_t AgentSelfRepair::dumpDiagnostics(char* buffer, size_t bufferSize) const {
    if (!buffer || bufferSize == 0) return 0;

    std::lock_guard<std::mutex> lock(m_mutex);

    size_t written = 0;
    auto append = [&](const char* fmt, ...) {
        if (written >= bufferSize - 1) return;
        va_list args;
        va_start(args, fmt);
        int n = _vsnprintf_s(buffer + written,
                             bufferSize - written,
                             _TRUNCATE, fmt, args);
        va_end(args);
        if (n > 0) written += static_cast<size_t>(n);
    };

    append("=== Agent Self-Repair Diagnostics (Enterprise-Safe) ===\n");
    append("Mode: Function-level redirection ONLY (no byte mutation)\n");
    append("Initialized: %s\n", m_initialized ? "YES" : "NO");
    append(".text base: 0x%llX  size: %llu bytes\n",
           static_cast<unsigned long long>(m_textBase),
           static_cast<unsigned long long>(m_textSize));
    append("Bug signatures: %zu\n", m_signatures.size());
    append("Repairable functions: %zu\n", m_repairables.size());
    append("Active redirections: %zu\n", m_activeRedirections.size());
    append("Bug reports: %zu\n\n", m_reports.size());

    // Telemetry counters
    append("--- Telemetry ---\n");
    append("Bugs detected:      %llu\n", m_telemetry.bugsDetected);
    append("Patches applied:    %llu\n", m_telemetry.patchesApplied);
    append("Patches rolled:     %llu\n", m_telemetry.patchesRolledBack);
    append("CRC verify fails:   %llu\n", m_telemetry.crcVerifyFails);
    append("Thread suspensions: %llu\n", m_telemetry.threadSuspensions);
    append("Replay checkpoints: %llu\n\n", m_telemetry.replayCheckpoints);

    // ASM kernel stats
    SelfPatchStats stats = getStats();
    append("--- MASM64 Scan Kernel ---\n");
    append("Total scans:     %llu\n", stats.totalScans);
    append("Patterns found:  %llu\n", stats.patternsFound);
    append("NOP sleds found: %llu\n", stats.nopSledsFound);
    append("Bytes scanned:   %llu\n\n", stats.bytesScanned);

    // Repairable functions
    append("--- Repairable Functions ---\n");
    for (size_t i = 0; i < m_repairables.size(); ++i) {
        const auto& f = m_repairables[i];
        append("[%zu] %-30s slot=%u crc=0x%08X redirected=%s autoRepair=%s sev=%u\n",
               i, f.name ? f.name : "?",
               f.slotId, f.expectedCRC,
               f.isRedirected ? "YES" : "no",
               f.autoRepair ? "yes" : "no",
               f.severity);
    }

    // Bug reports
    append("\n--- Bug Reports (detection only) ---\n");
    for (size_t i = 0; i < m_reports.size(); ++i) {
        const auto& r = m_reports[i];
        append("[%zu] %-20s addr=0x%llX count=%u escalate=%s redirected=%s\n",
               i,
               r.signature ? r.signature->name : "?",
               static_cast<unsigned long long>(r.address),
               r.matchCount,
               (r.signature && r.signature->escalateOnly) ? "YES" : "no",
               r.redirected ? "YES" : "no");
    }

    return written;
}
