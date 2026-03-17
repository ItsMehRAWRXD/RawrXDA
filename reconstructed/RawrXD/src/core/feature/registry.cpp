// ============================================================================
// feature_registry.cpp — Phase 31: Feature Registry Implementation
// ============================================================================
//
// PURPOSE:
//   Singleton FeatureRegistry implementation. Provides:
//     1. Thread-safe feature registration and query
//     2. MASM-accelerated stub detection (with C++ fallback)
//     3. Runtime component test execution
//     4. Production Readiness Report generation
//     5. Completion percentage calculation
//
// STUB PATTERNS:
//   Defined here as byte arrays. The MASM kernel (IsStubFunction) or
//   the C++ fallback scans function prologues for these patterns.
//
// PATTERN:   No exceptions. bool/status returns.
// THREADING: Internal std::mutex for all mutable state.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "../../include/feature_registry.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <iomanip>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// ============================================================================
// STUB PATTERN BYTE ARRAYS
// ============================================================================
namespace StubPatterns {

    // Pattern: bare ret (C3)
    const uint8_t PAT_BARE_RET[] = { 0xC3 };
    const size_t  PAT_BARE_RET_LEN = sizeof(PAT_BARE_RET);

    // Pattern: xor eax, eax ; ret (33 C0 C3)
    const uint8_t PAT_XOR_EAX_RET[] = { 0x33, 0xC0, 0xC3 };
    const size_t  PAT_XOR_EAX_RET_LEN = sizeof(PAT_XOR_EAX_RET);

    // Pattern: mov eax, 0 ; ret (B8 00 00 00 00 C3)
    const uint8_t PAT_MOV_EAX_0_RET[] = { 0xB8, 0x00, 0x00, 0x00, 0x00, 0xC3 };
    const size_t  PAT_MOV_EAX_0_RET_LEN = sizeof(PAT_MOV_EAX_0_RET);

    // Pattern: mov rax, 0 ; ret (48 C7 C0 00 00 00 00 C3)
    const uint8_t PAT_MOV_RAX_0_RET[] = { 0x48, 0xC7, 0xC0, 0x00, 0x00, 0x00, 0x00, 0xC3 };
    const size_t  PAT_MOV_RAX_0_RET_LEN = sizeof(PAT_MOV_RAX_0_RET);

    // Pattern: push rbp ; mov rbp, rsp ; xor eax, eax ; pop rbp ; ret
    // (55 48 89 E5 33 C0 5D C3)
    const uint8_t PAT_FRAME_XOR_RET[] = { 0x55, 0x48, 0x89, 0xE5, 0x33, 0xC0, 0x5D, 0xC3 };
    const size_t  PAT_FRAME_XOR_RET_LEN = sizeof(PAT_FRAME_XOR_RET);

    // Collected pattern table
    const StubPattern ALL_PATTERNS[] = {
        { "bare_ret",       PAT_BARE_RET,       PAT_BARE_RET_LEN       },
        { "xor_eax_ret",    PAT_XOR_EAX_RET,    PAT_XOR_EAX_RET_LEN    },
        { "mov_eax_0_ret",  PAT_MOV_EAX_0_RET,  PAT_MOV_EAX_0_RET_LEN  },
        { "mov_rax_0_ret",  PAT_MOV_RAX_0_RET,  PAT_MOV_RAX_0_RET_LEN  },
        { "frame_xor_ret",  PAT_FRAME_XOR_RET,  PAT_FRAME_XOR_RET_LEN  },
    };
    const size_t ALL_PATTERNS_COUNT = sizeof(ALL_PATTERNS) / sizeof(ALL_PATTERNS[0]);
}

// ============================================================================
// C++ FALLBACK — IsStubFunction (for MinGW / non-MASM builds)
// ============================================================================
#ifndef RAWR_HAS_MASM

extern "C" int IsStubFunction(void* funcPtr, size_t maxBytesToScan) {
    if (!funcPtr || maxBytesToScan == 0) return 0;

    // Use SEH-safe memory read via VirtualQuery
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(funcPtr, &mbi, sizeof(mbi)) == 0) {
        return 0;  // Cannot query memory — assume not a stub
    }

    // Check memory is readable + executable
    DWORD protect = mbi.Protect;
    bool readable = (protect & PAGE_EXECUTE_READ) ||
                    (protect & PAGE_EXECUTE_READWRITE) ||
                    (protect & PAGE_READONLY) ||
                    (protect & PAGE_READWRITE) ||
                    (protect & PAGE_EXECUTE);
    if (!readable) {
        return 0;
    }

    const uint8_t* code = reinterpret_cast<const uint8_t*>(funcPtr);

    // Clamp scan length to region bounds
    uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    uintptr_t codeAddr = reinterpret_cast<uintptr_t>(funcPtr);
    size_t maxSafe = static_cast<size_t>(regionEnd - codeAddr);
    if (maxBytesToScan > maxSafe) {
        maxBytesToScan = maxSafe;
    }

    // Check each known stub pattern
    for (size_t p = 0; p < StubPatterns::ALL_PATTERNS_COUNT; ++p) {
        const StubPattern& pat = StubPatterns::ALL_PATTERNS[p];
        if (pat.length <= maxBytesToScan) {
            if (memcmp(code, pat.bytes, pat.length) == 0) {
                return 1;  // Stub detected
            }
        }
    }

    // Additional heuristic: check if first N bytes are all 0xCC (int3 padding)
    // which indicates an unimplemented function filled with debug breaks
    if (maxBytesToScan >= 4) {
        bool allInt3 = true;
        size_t checkLen = (maxBytesToScan < 16) ? maxBytesToScan : 16;
        for (size_t i = 0; i < checkLen; ++i) {
            if (code[i] != 0xCC) {
                allInt3 = false;
                break;
            }
        }
        if (allInt3) {
            return 1;  // All int3 — stub/padding
        }
    }

    // Additional heuristic: nop slide (0x90 x N followed by ret)
    if (maxBytesToScan >= 2) {
        size_t nopCount = 0;
        for (size_t i = 0; i < maxBytesToScan - 1; ++i) {
            if (code[i] == 0x90) {
                nopCount++;
            } else if (code[i] == 0xC3 && nopCount > 0) {
                return 1;  // NOP slide to ret — likely stub
            } else {
                break;
            }
        }
    }

    return 0;  // Not a stub
}

#endif  // !RAWR_HAS_MASM

// ============================================================================
// FEATURE REGISTRY SINGLETON
// ============================================================================

FeatureRegistry& FeatureRegistry::instance() {
    static FeatureRegistry s_instance;
    return s_instance;
}

// ============================================================================
// REGISTRATION
// ============================================================================

void FeatureRegistry::registerFeature(const FeatureEntry& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check for duplicate by name — update if exists
    for (auto& existing : m_features) {
        if (existing.name && entry.name && strcmp(existing.name, entry.name) == 0) {
            existing = entry;
            return;
        }
    }

    m_features.push_back(entry);
}

bool FeatureRegistry::updateStatus(const char* featureName, ImplStatus newStatus) {
    if (!featureName) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& f : m_features) {
        if (f.name && strcmp(f.name, featureName) == 0) {
            f.status = newStatus;
            return true;
        }
    }
    return false;
}

bool FeatureRegistry::registerComponentTest(const char* featureName, ComponentTestFn testFn) {
    if (!featureName || !testFn) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_componentTests[featureName] = testFn;
    return true;
}

// ============================================================================
// QUERY API
// ============================================================================

std::vector<FeatureEntry> FeatureRegistry::getAllFeatures() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_features;
}

std::vector<FeatureEntry> FeatureRegistry::getByCategory(FeatureCategory cat) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<FeatureEntry> result;
    for (const auto& f : m_features) {
        if (f.category == cat) {
            result.push_back(f);
        }
    }
    return result;
}

std::vector<FeatureEntry> FeatureRegistry::getByStatus(ImplStatus status) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<FeatureEntry> result;
    for (const auto& f : m_features) {
        if (f.status == status) {
            result.push_back(f);
        }
    }
    return result;
}

size_t FeatureRegistry::getFeatureCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_features.size();
}

size_t FeatureRegistry::getCountByStatus(ImplStatus status) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& f : m_features) {
        if (f.status == status) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// STUB DETECTION — Invokes MASM kernel or C++ fallback
// ============================================================================

void FeatureRegistry::detectStubs() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& f : m_features) {
        if (f.funcPtr != nullptr) {
            // Scan up to 32 bytes of the function prologue
            int result = IsStubFunction(f.funcPtr, 32);
            f.stubDetected = (result != 0);

            // If stub is detected, adjust completion estimate
            if (f.stubDetected && f.status == ImplStatus::Complete) {
                f.status = ImplStatus::Stub;
            }
        } else {
            // No function pointer — cannot scan, mark as untested
            f.stubDetected = false;
        }
    }
}

// ============================================================================
// RUNTIME COMPONENT TESTS
// ============================================================================

void FeatureRegistry::runComponentTests() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& f : m_features) {
        if (!f.name) continue;

        auto it = m_componentTests.find(f.name);
        if (it != m_componentTests.end() && it->second) {
            const char* detail = nullptr;
            bool passed = it->second(&detail);
            f.runtimeTested = passed;

            if (passed) {
                // Test passed — if status was Untested, promote to Complete
                if (f.status == ImplStatus::Untested) {
                    f.status = ImplStatus::Complete;
                }
            } else {
                // Test failed — log detail
                if (detail) {
                    char buf[256];
                    snprintf(buf, sizeof(buf),
                             "[AUDIT] Component test FAILED for '%s': %s",
                             f.name, detail);
                    OutputDebugStringA(buf);
                }
            }
        }
    }
}

// ============================================================================
// COMPLETION PERCENTAGE
// ============================================================================

float FeatureRegistry::getCompletionPercentage() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_features.empty()) return 0.0f;

    float total = 0.0f;
    for (const auto& f : m_features) {
        switch (f.status) {
            case ImplStatus::Complete:   total += 1.0f;  break;
            case ImplStatus::Partial:    total += 0.5f;  break;
            case ImplStatus::Untested:   total += 0.8f;  break;
            case ImplStatus::Stub:       total += 0.0f;  break;
            case ImplStatus::Broken:     total += 0.0f;  break;
            case ImplStatus::Deprecated: total += 0.0f;  break;
            default:                     total += 0.0f;  break;
        }
    }

    return total / static_cast<float>(m_features.size());
}

// ============================================================================
// REPORT GENERATION — Production Readiness Report
// ============================================================================

std::string FeatureRegistry::generateReport() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ostringstream oss;

    // Header
    oss << "╔══════════════════════════════════════════════════════════════════╗\n";
    oss << "║         RawrXD-Shell — Production Readiness Report              ║\n";
    oss << "╚══════════════════════════════════════════════════════════════════╝\n\n";

    // Timestamp
    time_t now = time(nullptr);
    char timeBuf[64];
    struct tm tmBuf;
    localtime_s(&tmBuf, &now);
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tmBuf);
    oss << "Generated: " << timeBuf << "\n\n";

    // Summary counts
    size_t total = m_features.size();
    size_t complete = 0, partial = 0, stub = 0, untested = 0, broken = 0, deprecated = 0;
    size_t menuWired = 0, stubDetected = 0, runtimePassed = 0;

    for (const auto& f : m_features) {
        switch (f.status) {
            case ImplStatus::Complete:   complete++;   break;
            case ImplStatus::Partial:    partial++;    break;
            case ImplStatus::Stub:       stub++;       break;
            case ImplStatus::Untested:   untested++;   break;
            case ImplStatus::Broken:     broken++;     break;
            case ImplStatus::Deprecated: deprecated++; break;
            default: break;
        }
        if (f.menuWired) menuWired++;
        if (f.stubDetected) stubDetected++;
        if (f.runtimeTested) runtimePassed++;
    }

    oss << "── Summary ─────────────────────────────────────────────────────\n";
    oss << "  Total Features:     " << total << "\n";
    oss << "  Complete:           " << complete << "\n";
    oss << "  Partial:            " << partial << "\n";
    oss << "  Stub:               " << stub << "\n";
    oss << "  Untested:           " << untested << "\n";
    oss << "  Broken:             " << broken << "\n";
    oss << "  Deprecated:         " << deprecated << "\n";
    oss << "  Menu Wired:         " << menuWired << " / " << total << "\n";
    oss << "  Stubs Detected:     " << stubDetected << "\n";
    oss << "  Runtime Tests OK:   " << runtimePassed << "\n";

    // Completion bar
    float pct = total > 0 ? (static_cast<float>(complete) / static_cast<float>(total)) : 0.0f;
    int barWidth = 40;
    int filled = static_cast<int>(pct * barWidth);
    oss << "\n  Completion: [";
    for (int i = 0; i < barWidth; ++i) {
        oss << (i < filled ? "█" : "░");
    }
    oss << "] " << std::fixed << std::setprecision(1) << (pct * 100.0f) << "%\n\n";

    // Per-category breakdown
    oss << "── By Category ──────────────────────────────────────────────────\n";
    for (int c = 0; c < static_cast<int>(FeatureCategory::COUNT); ++c) {
        FeatureCategory cat = static_cast<FeatureCategory>(c);
        int catTotal = 0, catComplete = 0, catStub = 0;
        for (const auto& f : m_features) {
            if (f.category == cat) {
                catTotal++;
                if (f.status == ImplStatus::Complete) catComplete++;
                if (f.status == ImplStatus::Stub || f.stubDetected) catStub++;
            }
        }
        if (catTotal > 0) {
            oss << "  " << std::left << std::setw(14) << featureCategoryToString(cat)
                << "  " << catComplete << "/" << catTotal << " complete";
            if (catStub > 0) {
                oss << "  (" << catStub << " stubs)";
            }
            oss << "\n";
        }
    }
    oss << "\n";

    // Detailed feature list
    oss << "── Feature Details ──────────────────────────────────────────────\n";
    oss << std::left
        << std::setw(32) << "Feature"
        << std::setw(14) << "Category"
        << std::setw(12) << "Status"
        << std::setw(10) << "Phase"
        << std::setw(7)  << "Menu"
        << std::setw(7)  << "Stub"
        << std::setw(7)  << "Test"
        << "\n";
    oss << std::string(89, '-') << "\n";

    for (const auto& f : m_features) {
        oss << std::left
            << std::setw(32) << (f.name ? f.name : "(null)")
            << std::setw(14) << featureCategoryToString(f.category)
            << std::setw(12) << implStatusToString(f.status)
            << std::setw(10) << (f.phase ? f.phase : "—")
            << std::setw(7)  << (f.menuWired ? "YES" : "NO")
            << std::setw(7)  << (f.stubDetected ? "STUB" : "OK")
            << std::setw(7)  << (f.runtimeTested ? "PASS" : "—")
            << "\n";
    }
    oss << "\n";

    // Warnings section
    oss << "── Warnings ─────────────────────────────────────────────────────\n";
    bool hasWarnings = false;
    for (const auto& f : m_features) {
        if (f.stubDetected) {
            oss << "  [STUB] " << (f.name ? f.name : "(null)")
                << " — detected as stub implementation\n";
            hasWarnings = true;
        }
        if (f.status == ImplStatus::Broken) {
            oss << "  [BROKEN] " << (f.name ? f.name : "(null)")
                << " — marked as broken\n";
            hasWarnings = true;
        }
        if (f.menuWired && f.commandId != 0 && f.stubDetected) {
            oss << "  [WIRE+STUB] " << (f.name ? f.name : "(null)")
                << " — wired to menu (IDM=" << f.commandId
                << ") but implementation is a stub\n";
            hasWarnings = true;
        }
    }
    if (!hasWarnings) {
        oss << "  (none)\n";
    }
    oss << "\n";

    // Footer
    oss << "╔══════════════════════════════════════════════════════════════════╗\n";
    oss << "║                      End of Report                              ║\n";
    oss << "╚══════════════════════════════════════════════════════════════════╝\n";

    return oss.str();
}

// ============================================================================
// MENU WIRE VERIFICATION — Delegates to menu_auditor.cpp
// ============================================================================
// Forward declaration from menu_auditor.cpp
namespace RawrXD { namespace Audit {
    bool verifyCommandInMenu(HMENU hMenu, int commandId);
} }

void FeatureRegistry::verifyMenuWiring(void* hMenu) {
    if (!hMenu) return;

    HMENU menu = reinterpret_cast<HMENU>(hMenu);

    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& f : m_features) {
        if (f.commandId != 0) {
            bool found = RawrXD::Audit::verifyCommandInMenu(menu, f.commandId);
            f.menuWired = found;
        }
    }
}
