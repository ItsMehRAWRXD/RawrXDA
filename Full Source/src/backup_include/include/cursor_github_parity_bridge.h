// ============================================================================
// cursor_github_parity_bridge.h — Feature modules bridge (command ID ranges)
// ============================================================================
//
// Defines feature-module command ID ranges and verification. Implementations
// are pure C++20 or x64 MASM with maximum dependency removal.
//
// See: docs/CURSOR_GITHUB_PARITY_SPEC.md for command manifest.
//
// Local mode (no API key): Use LocalParity_* (include/local_parity_kernel.h).
// Full feature set on-device — no API key, no cloud.
//
// Pattern: No exceptions; optional runtime verification; header-only config.
// ============================================================================

#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
namespace RawrXD
{
namespace Parity
{

// ----------------------------------------------------------------------------
// Feature modules: 8 modules, 49 command IDs (11500–11574)
// ----------------------------------------------------------------------------
constexpr int CURSOR_PARITY_FIRST_ID = 11500;
constexpr int CURSOR_PARITY_LAST_ID = 11574;
constexpr int CURSOR_PARITY_MODULE_COUNT = 8;

enum class CursorModule : uint8_t
{
    TelemetryExport = 0,  // 8 commands
    AgenticComposer,      // 6 commands
    ContextMention,       // 4 commands
    VisionEncoder,        // 4 commands
    RefactoringEngine,    // 9 commands
    LanguageRegistry,     // 4 commands
    SemanticIndex,        // 9 commands
    ResourceGenerator,    // 5 commands
};

// Returns command count for module (for verification).
int cursorModuleCommandCount(CursorModule m);

// Returns true if commandId is in feature-module range and assigned.
bool isFeaturesCommand(int commandId);
/** @deprecated Use isFeaturesCommand. */
inline bool isCursorParityCommand(int commandId)
{
    return isFeaturesCommand(commandId);
}

// ----------------------------------------------------------------------------
// GitHub parity: read (update manifest) + write (create release)
// ----------------------------------------------------------------------------
constexpr const char* GITHUB_RELEASES_LATEST_URL = "https://api.github.com/repos/ItsMehRAWRXD/RawrXD/releases/latest";

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

// Optional: call from IDE init to verify all feature modules are wired.
// Returns 0 if all expected modules are present; otherwise first missing module index (1-based).
int verifyFeaturesWiring(void* win32IDEContext);
/** @deprecated Use verifyFeaturesWiring. */
inline int verifyCursorParityWiring(void* ctx)
{
    return verifyFeaturesWiring(ctx);
}

}  // namespace Parity
}  // namespace RawrXD
#endif
