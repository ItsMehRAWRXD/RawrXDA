// ============================================================================
// cursor_github_parity_bridge.h — Cursor & GitHub Parity Production Bridge
// ============================================================================
//
// Ensures all Cursor IDE and GitHub parities are included and fully useable
// via production-ready, elegant implementations in pure C++20 or x64 MASM,
// with maximum dependency removal (DSA in-house where reverse engineering allows).
//
// See: docs/CURSOR_GITHUB_PARITY_SPEC.md
//
// Local parity mode (no API key): Use LocalParity_* (see include/local_parity_kernel.h
// and docs/LOCAL_PARITY_NO_API_KEY_SPEC.md). Same Cursor/Copilot-style agentic and
// autonomous behavior, fully on-device — no API key, no cloud.
//
// Pattern: No exceptions; optional runtime verification; header-only config.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
namespace RawrXD {
namespace Parity {

// ----------------------------------------------------------------------------
// Cursor parity: 8 modules, 48 assigned command IDs (11500–11574)
// ----------------------------------------------------------------------------
constexpr int CURSOR_PARITY_FIRST_ID = 11500;
constexpr int CURSOR_PARITY_LAST_ID  = 11574;
constexpr int CURSOR_PARITY_MODULE_COUNT = 8;

enum class CursorModule : uint8_t {
    TelemetryExport = 0,  // 8 commands
    AgenticComposer,      // 6 commands
    ContextMention,       // 4 commands
    VisionEncoder,        // 5 commands
    RefactoringEngine,    // 8 commands
    LanguageRegistry,    // 4 commands
    SemanticIndex,        // 8 commands
    ResourceGenerator,   // 5 commands
};

// Returns command count for module (for verification).
int cursorModuleCommandCount(CursorModule m);

// Returns true if commandId is in Cursor parity range and assigned.
bool isCursorParityCommand(int commandId);

// ----------------------------------------------------------------------------
// GitHub parity: read (update manifest) + write (create release)
// ----------------------------------------------------------------------------
constexpr const char* GITHUB_RELEASES_LATEST_URL =
    "https://api.github.com/repos/ItsMehRAWRXD/RawrXD/releases/latest";

// Production contract: update path uses WinHTTP + manual JSON only (no curl, no nlohmann).
// Release create path: WinHTTP POST + GITHUB_TOKEN; JSON build manual or minimal.

// ----------------------------------------------------------------------------
// Dependency-removal contracts (for static analysis / docs)
// ----------------------------------------------------------------------------
// - HTTP (update/release): WinHTTP only.
// - JSON (update manifest parse): manual parse only (see update_signature.cpp).
// - Hot paths (inference, compression, search): C++20 or x64 MASM.
// - Plugins: Win32 LoadLibrary/GetProcAddress; no Node in parity command path.
// - Local parity mode: use LocalParity_* (include/local_parity_kernel.h); no API key required.

// Optional: call from IDE init to verify all parity modules are wired.
// Returns 0 if all expected modules are present; otherwise first missing module index (1-based).
int verifyCursorParityWiring(void* win32IDEContext);

} // namespace Parity
} // namespace RawrXD
#endif
