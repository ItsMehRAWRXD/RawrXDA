// agent_self_repair.cpp — Agent Self-Repair Implementation
//
// The agent uses its own MASM64 kernel to scan for, detect, and fix
// bugs in its running binary. No restart, no external tools, no deps.
//
// Architecture: C++20 bridge → MASM64 self-patch kernel
// Error model: PatchResult::ok() / PatchResult::error() — no exceptions
// Threading: std::mutex + std::lock_guard — no recursive locks
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "agent_self_repair.hpp"
#include <cstring>
#include <cstdio>

// ---------------------------------------------------------------------------
// Built-in bug signatures — common x64 patterns the agent knows are bugs
// ---------------------------------------------------------------------------

// Bug #1: Invalid RET after CALL (missing epilogue — crashes on return)
//   Pattern: CALL rel32 followed immediately by 00 00 (null bytes = bug)
static const uint8_t kBugSig_NullAfterCall[]  = { 0xE8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t kBugFix_NullAfterCall[]  = { 0xE8, 0x00, 0x00, 0x00, 0x00, 0x90, 0x90 }; // NOP pad

// Bug #2: UD2 (undefined instruction) left in release code
//   Pattern: 0F 0B = UD2 (intentional crash, should not be in release)
static const uint8_t kBugSig_UD2[]  = { 0x0F, 0x0B };
static const uint8_t kBugFix_UD2[]  = { 0x90, 0x90 }; // NOP out

// Bug #3: INT 3 breakpoint left in release (debugger artifact)
//   Pattern: CC CC CC CC (4+ consecutive INT3 = stale debug break)
static const uint8_t kBugSig_Int3Sled[]  = { 0xCC, 0xCC, 0xCC, 0xCC };
static const uint8_t kBugFix_Int3Sled[]  = { 0x90, 0x90, 0x90, 0x90 };

// Bug #4: JMP to self (infinite loop bug)
//   Pattern: EB FE = JMP $-0 (jumps to itself forever)
static const uint8_t kBugSig_JmpSelf[]  = { 0xEB, 0xFE };
static const uint8_t kBugFix_JmpSelf[]  = { 0x90, 0xC3 }; // NOP + RET

// Bug #5: MOV to null pointer (access violation)
//   Pattern: 48 89 04 25 00 00 00 00 = MOV [0], RAX
static const uint8_t kBugSig_MovNull[]  = { 0x48, 0x89, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t kBugFix_MovNull[]  = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0xC3 }; // NOP + RET

// Bug #6: Double FREE pattern — CALL to HeapFree with zero-check missing
//   Pattern: 48 85 C9 = TEST RCX,RCX followed by wrong jump
//   (Detection only, no auto-fix — requires manual review)
static const uint8_t kBugSig_DoubleFreeEntry[]  = { 0x48, 0x85, 0xC9, 0x75, 0x00, 0xFF };
// No fix — manual review needed

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
    if (rc != 0) {
        return PatchResult::error("ASM self-patch init failed", rc);
    }
#endif

    if (!locateTextSection()) {
        return PatchResult::error("Failed to locate .text section");
    }

    m_initialized = true;
    loadBuiltinSignatures();

    return PatchResult::ok("Agent self-repair initialized");
}

bool AgentSelfRepair::isInitialized() const {
    return m_initialized;
}

// ---------------------------------------------------------------------------
// PE .text section location
// ---------------------------------------------------------------------------
bool AgentSelfRepair::locateTextSection() {
    // Get base address of the running executable
    HMODULE hModule = GetModuleHandleA(nullptr);
    if (!hModule) return false;

    auto* dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(hModule);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;

    auto* ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS64*>(
        reinterpret_cast<uintptr_t>(hModule) + dosHeader->e_lfanew
    );
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return false;

    // Walk section headers to find .text
    WORD sectionCount = ntHeaders->FileHeader.NumberOfSections;
    auto* section = IMAGE_FIRST_SECTION(ntHeaders);

    for (WORD i = 0; i < sectionCount; ++i) {
        if (std::memcmp(section[i].Name, ".text", 5) == 0) {
            m_textBase = reinterpret_cast<uintptr_t>(hModule) + section[i].VirtualAddress;
            m_textSize = section[i].Misc.VirtualSize;
            return true;
        }
    }

    // Fallback: use the first executable section
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
// Bug Signature Registry
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
    // Bug #1: Null bytes after CALL
    BugSignature sig1{};
    sig1.name       = "NullAfterCall";
    sig1.pattern    = kBugSig_NullAfterCall;
    sig1.patternLen = sizeof(kBugSig_NullAfterCall);
    sig1.fix        = kBugFix_NullAfterCall;
    sig1.fixLen     = sizeof(kBugFix_NullAfterCall);
    sig1.autoFix    = false;    // Dangerous — needs review
    sig1.severity   = 2;        // Error
    m_signatures.push_back(sig1);

    // Bug #2: UD2 in release
    BugSignature sig2{};
    sig2.name       = "UD2InRelease";
    sig2.pattern    = kBugSig_UD2;
    sig2.patternLen = sizeof(kBugSig_UD2);
    sig2.fix        = kBugFix_UD2;
    sig2.fixLen     = sizeof(kBugFix_UD2);
    sig2.autoFix    = true;     // Safe to NOP out
    sig2.severity   = 3;        // Critical
    m_signatures.push_back(sig2);

    // Bug #3: INT3 sled in release
    BugSignature sig3{};
    sig3.name       = "Int3Sled";
    sig3.pattern    = kBugSig_Int3Sled;
    sig3.patternLen = sizeof(kBugSig_Int3Sled);
    sig3.fix        = kBugFix_Int3Sled;
    sig3.fixLen     = sizeof(kBugFix_Int3Sled);
    sig3.autoFix    = true;
    sig3.severity   = 1;        // Warning
    m_signatures.push_back(sig3);

    // Bug #4: JMP-to-self infinite loop
    BugSignature sig4{};
    sig4.name       = "JmpToSelf";
    sig4.pattern    = kBugSig_JmpSelf;
    sig4.patternLen = sizeof(kBugSig_JmpSelf);
    sig4.fix        = kBugFix_JmpSelf;
    sig4.fixLen     = sizeof(kBugFix_JmpSelf);
    sig4.autoFix    = true;     // Replace infinite loop with return
    sig4.severity   = 3;        // Critical
    m_signatures.push_back(sig4);

    // Bug #5: MOV to null pointer
    BugSignature sig5{};
    sig5.name       = "MovToNull";
    sig5.pattern    = kBugSig_MovNull;
    sig5.patternLen = sizeof(kBugSig_MovNull);
    sig5.fix        = kBugFix_MovNull;
    sig5.fixLen     = sizeof(kBugFix_MovNull);
    sig5.autoFix    = true;     // NOP out the write + RET
    sig5.severity   = 3;        // Critical
    m_signatures.push_back(sig5);

    // Bug #6: Double-free pattern (detection only)
    BugSignature sig6{};
    sig6.name       = "DoubleFreeEntry";
    sig6.pattern    = kBugSig_DoubleFreeEntry;
    sig6.patternLen = sizeof(kBugSig_DoubleFreeEntry);
    sig6.fix        = nullptr;
    sig6.fixLen     = 0;
    sig6.autoFix    = false;    // Manual review required
    sig6.severity   = 2;        // Error
    m_signatures.push_back(sig6);
}

// ---------------------------------------------------------------------------
// Scanning
// ---------------------------------------------------------------------------
size_t AgentSelfRepair::scanSelf() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized || m_textBase == 0 || m_textSize == 0) {
        return 0;
    }

    return scanRegion(reinterpret_cast<const void*>(m_textBase), m_textSize);
}

size_t AgentSelfRepair::scanRegion(const void* base, size_t length) {
    // Note: caller must hold m_mutex if calling from scanSelf
    if (!base || length == 0) return 0;

    size_t totalBugs = 0;

    for (const auto& sig : m_signatures) {
#ifdef RAWR_HAS_MASM
        // Use the MASM64 kernel for pattern scanning
        void* result = asm_selfpatch_scan_text(base, length, sig.pattern, sig.patternLen);
        if (!result) continue;

        // Cast to our scan result mirror
        auto* sr = reinterpret_cast<SelfPatchScanResult*>(result);

        if (sr->statusCode == 1 /* SCAN_PATTERN_FOUND */ && sr->matchCount > 0) {
            BugReport report = BugReport::found(&sig, sr->firstMatch, sr->matchCount);

            // Get timestamp
            report.timestamp = GetTickCount64();

            m_reports.push_back(report);
            notifyBugDetected(report);
            totalBugs += sr->matchCount;
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
    // C++ fallback
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
// Self-Repair
// ---------------------------------------------------------------------------
PatchResult AgentSelfRepair::autoRepairAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        return PatchResult::error("Not initialized");
    }

    int fixed = 0;
    int failed = 0;

    for (auto& report : m_reports) {
        if (report.fixed) continue;                     // Already fixed
        if (!report.signature) continue;
        if (!report.signature->autoFix) continue;       // Manual review
        if (!report.signature->fix) continue;            // No fix available
        if (report.signature->fixLen == 0) continue;

        uint64_t patchId = 0;
        PatchResult r = applyFix(report.address,
                                  report.signature->fix,
                                  report.signature->fixLen,
                                  &patchId);
        if (r.success) {
            report.fixed = true;
            report.patchId = patchId;
            ++fixed;
        } else {
            ++failed;
        }
    }

    char msg[128];
    _snprintf_s(msg, sizeof(msg), _TRUNCATE,
                "Auto-repair: %d fixed, %d failed", fixed, failed);

    if (failed > 0) {
        return PatchResult::error(msg, failed);
    }
    return PatchResult::ok(fixed > 0 ? "Bugs auto-repaired" : "No bugs to repair");
}

PatchResult AgentSelfRepair::applyFix(uintptr_t address, const uint8_t* fix,
                                       size_t fixLen, uint64_t* outPatchId) {
    if (!m_initialized) {
        return PatchResult::error("Not initialized");
    }
    if (!fix || fixLen == 0 || fixLen > 64) {
        return PatchResult::error("Invalid fix parameters");
    }

#ifdef RAWR_HAS_MASM
    int rc = asm_selfpatch_apply(reinterpret_cast<void*>(address),
                                  fix, fixLen);
    if (rc != 0) {
        return PatchResult::error("ASM self-patch apply failed", rc);
    }

    // The patch_id is returned in RDX by the ASM function.
    // In MSVC x64, we can't directly access RDX from C.
    // Use the stats to infer the ID (patches_applied count).
    if (outPatchId) {
        auto* stats = reinterpret_cast<SelfPatchStats*>(asm_selfpatch_get_stats());
        if (stats) {
            *outPatchId = stats->patchesApplied; // Monotonically increasing
        }
    }

    return PatchResult::ok("Patch applied via MASM64 kernel");
#else
    // C++ fallback via Layer 1 memory patch
    PatchResult r = apply_memory_patch(reinterpret_cast<void*>(address), fixLen, fix);
    if (outPatchId) *outPatchId = 0;
    return r;
#endif
}

PatchResult AgentSelfRepair::installTrampoline(void* buggyFunction,
                                                void* fixedFunction,
                                                uint64_t* outPatchId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        return PatchResult::error("Not initialized");
    }
    if (!buggyFunction || !fixedFunction) {
        return PatchResult::error("Invalid function pointers");
    }

#ifdef RAWR_HAS_MASM
    int rc = asm_selfpatch_install_tramp(buggyFunction, fixedFunction);
    if (rc != 0) {
        return PatchResult::error("Trampoline install failed", rc);
    }

    if (outPatchId) {
        auto* stats = reinterpret_cast<SelfPatchStats*>(asm_selfpatch_get_stats());
        if (stats) {
            *outPatchId = stats->patchesApplied;
        }
    }

    return PatchResult::ok("Trampoline installed — function redirected");
#else
    // C++ fallback: build trampoline in-memory
    uint8_t trampoline[14];
    trampoline[0] = 0xFF;
    trampoline[1] = 0x25;
    *reinterpret_cast<uint32_t*>(&trampoline[2]) = 0; // RIP+0
    *reinterpret_cast<uint64_t*>(&trampoline[6]) = reinterpret_cast<uint64_t>(fixedFunction);

    PatchResult r = apply_memory_patch(buggyFunction, 14, trampoline);
    if (outPatchId) *outPatchId = 0;
    return r;
#endif
}

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
    // C++ fallback: InterlockedCompareExchangePointer
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
PatchResult AgentSelfRepair::verifyPatch(uint64_t patchId) {
#ifdef RAWR_HAS_MASM
    int rc = asm_selfpatch_verify_crc(patchId);
    if (rc != 0) {
        return PatchResult::error("CRC32 verification failed — patch corrupted", rc);
    }
    return PatchResult::ok("Patch integrity verified");
#else
    (void)patchId;
    return PatchResult::ok("No verification available without MASM");
#endif
}

PatchResult AgentSelfRepair::verifyAllPatches() {
    std::lock_guard<std::mutex> lock(m_mutex);

    int verified = 0;
    int failed   = 0;

    for (uint64_t id : m_activePatchIds) {
        PatchResult r = verifyPatch(id);
        if (r.success) ++verified;
        else ++failed;
    }

    if (failed > 0) {
        return PatchResult::error("Some patches failed CRC verification", failed);
    }
    return PatchResult::ok("All patches verified");
}

// ---------------------------------------------------------------------------
// Rollback
// ---------------------------------------------------------------------------
PatchResult AgentSelfRepair::rollbackPatch(uint64_t patchId) {
    std::lock_guard<std::mutex> lock(m_mutex);

#ifdef RAWR_HAS_MASM
    int rc = asm_selfpatch_rollback(patchId);
    if (rc != 0) {
        return PatchResult::error("Rollback failed", rc);
    }

    // Remove from active list
    for (auto it = m_activePatchIds.begin(); it != m_activePatchIds.end(); ++it) {
        if (*it == patchId) {
            m_activePatchIds.erase(it);
            break;
        }
    }

    // Update reports
    for (auto& report : m_reports) {
        if (report.patchId == patchId) {
            report.fixed = false;
            report.patchId = 0;
        }
    }

    return PatchResult::ok("Patch rolled back successfully");
#else
    (void)patchId;
    return PatchResult::error("Rollback requires MASM kernel");
#endif
}

PatchResult AgentSelfRepair::rollbackAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    int rolledBack = 0;
    int failed = 0;

    // Copy list since rollbackPatch modifies it
    auto ids = m_activePatchIds;
    for (uint64_t id : ids) {
#ifdef RAWR_HAS_MASM
        int rc = asm_selfpatch_rollback(id);
        if (rc == 0) ++rolledBack;
        else ++failed;
#endif
    }

    m_activePatchIds.clear();

    for (auto& report : m_reports) {
        report.fixed = false;
        report.patchId = 0;
    }

    if (failed > 0) {
        return PatchResult::error("Some rollbacks failed", failed);
    }
    return PatchResult::ok("All patches rolled back");
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

const std::vector<BugReport>& AgentSelfRepair::getReports() const {
    return m_reports;
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

    append("=== Agent Self-Repair Diagnostics ===\n");
    append("Initialized: %s\n", m_initialized ? "YES" : "NO");
    append(".text base: 0x%llX  size: %llu bytes\n",
           static_cast<unsigned long long>(m_textBase),
           static_cast<unsigned long long>(m_textSize));
    append("Signatures: %zu\n", m_signatures.size());
    append("Reports: %zu\n", m_reports.size());
    append("Active patches: %zu\n\n", m_activePatchIds.size());

    // Stats
    SelfPatchStats stats = getStats();
    append("--- Statistics ---\n");
    append("Total scans:       %llu\n", stats.totalScans);
    append("Patterns found:    %llu\n", stats.patternsFound);
    append("Patches applied:   %llu\n", stats.patchesApplied);
    append("Patches rolled:    %llu\n", stats.patchesRolledBack);
    append("Patches failed:    %llu\n", stats.patchesFailed);
    append("Trampolines:       %llu\n", stats.trampolinesSet);
    append("CRC pass:          %llu\n", stats.crcChecksPassed);
    append("CRC fail:          %llu\n", stats.crcChecksFailed);
    append("CAS ops:           %llu\n", stats.casOperations);
    append("NOP sleds found:   %llu\n", stats.nopSledsFound);
    append("Bytes scanned:     %llu\n\n", stats.bytesScanned);

    // Bug reports
    append("--- Bug Reports ---\n");
    for (size_t i = 0; i < m_reports.size(); ++i) {
        const auto& r = m_reports[i];
        append("[%zu] %-20s addr=0x%llX count=%u fixed=%s",
               i,
               r.signature ? r.signature->name : "?",
               static_cast<unsigned long long>(r.address),
               r.matchCount,
               r.fixed ? "YES" : "NO");
        if (r.fixed) {
            append(" patchId=%llu", static_cast<unsigned long long>(r.patchId));
        }
        append("\n");
    }

    return written;
}
