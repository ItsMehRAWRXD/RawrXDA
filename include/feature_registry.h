// ============================================================================
// feature_registry.h — Phase 31: IDE Self-Audit & Verification System
// ============================================================================
//
// PURPOSE:
//   Central feature registration, stub detection, and production readiness
//   audit infrastructure. Every IDE feature self-registers via
//   RAW_REGISTER_FEATURE() macro. The AuditDashboard queries this registry
//   to build a comprehensive compliance report.
//
// COMPONENTS:
//   FeatureCategory  — Enum classifying feature domains
//   ImplStatus       — Implementation status (Stub, Partial, Complete, etc.)
//   FeatureEntry     — Per-feature metadata record
//   FeatureRegistry  — Singleton registry + audit engine
//   StubPattern      — Known byte patterns for stub detection (MASM kernel)
//
// ASM KERNEL:
//   IsStubFunction() — MASM64 fast-path stub detector (RawrXD_StubDetector.asm)
//   C++ fallback provided for MinGW builds (#ifndef RAWR_HAS_MASM)
//
// PATTERN:   No exceptions. Returns bool/status codes.
// THREADING: Registry uses std::mutex for thread-safe registration.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>
#include <functional>
#include <cstring>

// Forward-declare Windows handle types to avoid pulling in windows.h
// (The actual HWND/HMENU are pointer-sized opaque handles)
#ifndef _WINDEF_
struct HWND__;
struct HMENU__;
typedef HWND__* HWND;
typedef HMENU__* HMENU;
#endif

// ============================================================================
// FEATURE CATEGORY — Domain classification for audit grouping
// ============================================================================
enum class FeatureCategory {
    Core,               // Core IDE functionality (editor, file ops, session)
    AI,                 // AI/LLM integration (backend, router, agent)
    Editor,             // Editor engines (RichEdit, WebView2, MonacoCore)
    Debugger,           // Debugger and reverse engineering
    Network,            // Network, LSP, server infrastructure
    Build,              // Build system, compilation, linking
    GPU,                // GPU acceleration (AMD, Intel, ARM64, Cerebras)
    Security,           // Sandbox, quantum transport, licensing
    Hotpatch,           // Three-layer hotpatch system
    UI,                 // UI components (themes, transparency, panels)
    PDB,                // PDB symbol server and debug info
    Extension,          // VS Code Extension API compatibility
    Swarm,              // Distributed swarm orchestration
    Compression,        // Compression, quantization, codec
    COUNT               // Sentinel — total category count
};

// ============================================================================
// IMPLEMENTATION STATUS — Per-feature completeness level
// ============================================================================
enum class ImplStatus {
    Stub,               // Function exists but body is empty / returns default
    Partial,            // Some logic implemented, not feature-complete
    Complete,           // Fully implemented and functional
    Untested,           // Implemented but never validated at runtime
    Broken,             // Known broken / compile error / crash
    Deprecated,         // Scheduled for removal
    COUNT               // Sentinel
};

// ============================================================================
// STUB PATTERN — Known byte sequences for automatic stub detection
// ============================================================================
struct StubPattern {
    const char*  name;          // Human-readable pattern name
    const uint8_t* bytes;       // Byte pattern to match
    size_t       length;        // Pattern length in bytes
};

// Well-known stub patterns (defined in feature_registry.cpp)
namespace StubPatterns {
    // Pattern: ret (C3)
    extern const uint8_t PAT_BARE_RET[];
    extern const size_t  PAT_BARE_RET_LEN;

    // Pattern: xor eax, eax ; ret (33 C0 C3)
    extern const uint8_t PAT_XOR_EAX_RET[];
    extern const size_t  PAT_XOR_EAX_RET_LEN;

    // Pattern: mov eax, 0 ; ret (B8 00 00 00 00 C3)
    extern const uint8_t PAT_MOV_EAX_0_RET[];
    extern const size_t  PAT_MOV_EAX_0_RET_LEN;

    // Pattern: mov rax, 0 ; ret (48 C7 C0 00 00 00 00 C3)
    extern const uint8_t PAT_MOV_RAX_0_RET[];
    extern const size_t  PAT_MOV_RAX_0_RET_LEN;

    // Pattern: push rbp ; mov rbp, rsp ; xor eax, eax ; pop rbp ; ret
    // (55 48 89 E5 33 C0 5D C3)
    extern const uint8_t PAT_FRAME_XOR_RET[];
    extern const size_t  PAT_FRAME_XOR_RET_LEN;

    // All patterns collected for scanning
    extern const StubPattern ALL_PATTERNS[];
    extern const size_t ALL_PATTERNS_COUNT;
}

// ============================================================================
// FEATURE ENTRY — Per-feature registration record
// ============================================================================
struct FeatureEntry {
    const char*      name;           // Feature name (e.g., "PDB Symbol Server")
    const char*      file;           // Source file (__FILE__)
    int              line;           // Registration line (__LINE__)
    FeatureCategory  category;       // Domain classification
    ImplStatus       status;         // Current implementation status
    const char*      phase;          // Phase identifier (e.g., "Phase 29")
    const char*      description;    // Brief description
    void*            funcPtr;        // Pointer to implementation entry point
    bool             menuWired;      // true if connected to a menu command
    int              commandId;      // IDM_* value (0 if none)
    bool             stubDetected;   // true if MASM kernel flagged as stub
    bool             runtimeTested;  // true if runtime component test passed
    float            completionPct;  // 0.0–1.0 estimated completeness
};

// ============================================================================
// ASM KERNEL EXTERN — IsStubFunction (MASM64 fast-path)
// ============================================================================
// Checks if a function pointer points to a known stub pattern.
// Returns: 1 if stub detected, 0 if real implementation.
// Args:    funcPtr (RCX), maxBytesToScan (RDX)
//
// On MinGW builds, a C++ fallback is provided in feature_registry.cpp.
// ============================================================================
#ifdef __cplusplus
extern "C" {
#endif

#ifdef RAWR_HAS_MASM
    // MASM64 kernel — assembled from src/asm/RawrXD_StubDetector.asm
    int __cdecl IsStubFunction(void* funcPtr, size_t maxBytesToScan);
#else
    // C++ fallback for non-MSVC (MinGW) builds
    int IsStubFunction(void* funcPtr, size_t maxBytesToScan);
#endif

#ifdef __cplusplus
}
#endif

// ============================================================================
// COMPONENT TEST CALLBACK — Signature for runtime verification functions
// ============================================================================
typedef bool (*ComponentTestFn)(const char** outDetail);

// ============================================================================
// FEATURE REGISTRY — Singleton audit engine
// ============================================================================
class FeatureRegistry {
public:
    // Singleton access
    static FeatureRegistry& instance();

    // ---- Registration API ----

    // Register a new feature entry. Thread-safe.
    void registerFeature(const FeatureEntry& entry);

    // Update status of a previously registered feature by name.
    bool updateStatus(const char* featureName, ImplStatus newStatus);

    // Associate a runtime component test with a feature.
    bool registerComponentTest(const char* featureName, ComponentTestFn testFn);

    // ---- Query API ----

    // Get all registered features (snapshot copy).
    std::vector<FeatureEntry> getAllFeatures() const;

    // Get features filtered by category.
    std::vector<FeatureEntry> getByCategory(FeatureCategory cat) const;

    // Get features filtered by status.
    std::vector<FeatureEntry> getByStatus(ImplStatus status) const;

    // Get total feature count.
    size_t getFeatureCount() const;

    // Get count by status.
    size_t getCountByStatus(ImplStatus status) const;

    // ---- Audit Engine ----

    // Run MASM stub detection on all registered features with funcPtr != nullptr.
    // Updates stubDetected field in each entry.
    void detectStubs();

    // Run all registered component tests.
    // Updates runtimeTested field in each entry.
    void runComponentTests();

    // Calculate overall completion percentage (0.0–1.0).
    float getCompletionPercentage() const;

    // Generate a full text report (Production Readiness Report).
    std::string generateReport() const;

    // ---- Menu Wire Verification (delegated to menu_auditor.cpp) ----

    // Verify that all features with commandId != 0 are wired into the menu.
    // Requires HMENU from the IDE's main menu bar.
    void verifyMenuWiring(void* hMenu);

private:
    FeatureRegistry() = default;
    ~FeatureRegistry() = default;
    FeatureRegistry(const FeatureRegistry&) = delete;
    FeatureRegistry& operator=(const FeatureRegistry&) = delete;

    mutable std::mutex m_mutex;
    std::vector<FeatureEntry> m_features;
    std::map<std::string, ComponentTestFn> m_componentTests;
};

// ============================================================================
// AUTO-DISCOVERY ENGINE — Runtime feature scanner (Phase 31 upgrade)
// ============================================================================
// Eliminates manual RAW_REGISTER_FEATURE() calls by auto-scanning:
//   1. The master IDM_* command table (compiled into the binary)
//   2. The HMENU hierarchy at runtime (discovers unlisted items)
//   3. Stub detection results (MASM kernel / C++ fallback)
//   4. Menu wiring verification
//
// Usage:
//   AutoDiscoveryEngine::instance().discoverAll(hwndMain);
//   // Registry is now fully populated — no manual calls needed
// ============================================================================
class AutoDiscoveryEngine {
public:
    // Singleton access
    static AutoDiscoveryEngine& instance();

    // Run full discovery pipeline: master table + menu scan + stubs + classify
    // Call once during initAuditSystem() with the IDE's main window handle.
    void discoverAll(HWND hwndMain);

    // Query: has discovery been run?
    bool isDiscoveryComplete() const;

    // Get master command table size (for diagnostics)
    static size_t getMasterTableSize();

private:
    AutoDiscoveryEngine() = default;
    ~AutoDiscoveryEngine() = default;
    AutoDiscoveryEngine(const AutoDiscoveryEngine&) = delete;
    AutoDiscoveryEngine& operator=(const AutoDiscoveryEngine&) = delete;

    // Walk HMENU tree and register items not in the master table
    size_t discoverFromMenu(HMENU hMenu);

    // Classify a command ID into a FeatureCategory by ID range
    static FeatureCategory classifyByCommandId(int id);

    // Auto-set status based on discovered state (menu, stubs, funcPtr)
    void autoClassify();

    bool m_discoveryComplete = false;
};

// ============================================================================
// RAW_REGISTER_FEATURE — Manual registration macro (OPTIONAL supplement)
// ============================================================================
// NOTE: With AutoDiscoveryEngine, most features are auto-registered.
//       Use this macro ONLY for features that need a custom funcPtr or
//       special status that auto-discovery cannot infer.
//
// Usage:
//   RAW_REGISTER_FEATURE("GPU Auto-Tuner", FeatureCategory::GPU,
//                         ImplStatus::Complete, "Phase 23",
//                         "CUDA/Vulkan kernel auto-tuning engine",
//                         (void*)&cmdGPUAutoTune, true, IDM_GPU_AUTOTUNE);
// ============================================================================
#define RAW_REGISTER_FEATURE(name, category, status, phase, description, \
                              funcPtr, menuWired, commandId)              \
    namespace {                                                           \
        struct _FeatureReg_##__LINE__ {                                   \
            _FeatureReg_##__LINE__() {                                    \
                FeatureEntry e{};                                         \
                e.name = name;                                            \
                e.file = __FILE__;                                        \
                e.line = __LINE__;                                        \
                e.category = category;                                    \
                e.status = status;                                        \
                e.phase = phase;                                          \
                e.description = description;                              \
                e.funcPtr = funcPtr;                                      \
                e.menuWired = menuWired;                                  \
                e.commandId = commandId;                                  \
                e.stubDetected = false;                                   \
                e.runtimeTested = false;                                  \
                e.completionPct = 0.0f;                                   \
                FeatureRegistry::instance().registerFeature(e);           \
            }                                                             \
        } _featureRegInstance_##__LINE__;                                  \
    }

// ============================================================================
// CONVENIENCE — Status-to-string conversion
// ============================================================================
inline const char* implStatusToString(ImplStatus s) {
    switch (s) {
        case ImplStatus::Stub:       return "Stub";
        case ImplStatus::Partial:    return "Partial";
        case ImplStatus::Complete:   return "Complete";
        case ImplStatus::Untested:   return "Untested";
        case ImplStatus::Broken:     return "Broken";
        case ImplStatus::Deprecated: return "Deprecated";
        default:                     return "Unknown";
    }
}

inline const char* featureCategoryToString(FeatureCategory c) {
    switch (c) {
        case FeatureCategory::Core:        return "Core";
        case FeatureCategory::AI:          return "AI";
        case FeatureCategory::Editor:      return "Editor";
        case FeatureCategory::Debugger:    return "Debugger";
        case FeatureCategory::Network:     return "Network";
        case FeatureCategory::Build:       return "Build";
        case FeatureCategory::GPU:         return "GPU";
        case FeatureCategory::Security:    return "Security";
        case FeatureCategory::Hotpatch:    return "Hotpatch";
        case FeatureCategory::UI:          return "UI";
        case FeatureCategory::PDB:         return "PDB";
        case FeatureCategory::Extension:   return "Extension";
        case FeatureCategory::Swarm:       return "Swarm";
        case FeatureCategory::Compression: return "Compression";
        default:                           return "Unknown";
    }
}
