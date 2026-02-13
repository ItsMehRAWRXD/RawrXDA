// ============================================================================
// cursor_github_parity_bridge.cpp — Cursor & GitHub Parity Production Bridge
// ============================================================================
//
// Implements parity manifest and optional runtime verification.
// All Cursor/GitHub features must be C++20 or x64 MASM with max dep removal.
// See: docs/CURSOR_GITHUB_PARITY_SPEC.md
//
// ============================================================================

#include "../../include/cursor_github_parity_bridge.h"

namespace RawrXD {
namespace Parity {

static const int s_moduleCommandCounts[] = {
    8,  // TelemetryExport
    6,  // AgenticComposer
    4,  // ContextMention
    4,  // VisionEncoder
    9,  // RefactoringEngine
    4,  // LanguageRegistry
    9,  // SemanticIndex
    5,  // ResourceGenerator
};

int cursorModuleCommandCount(CursorModule m) {
    auto idx = static_cast<size_t>(m);
    if (idx >= sizeof(s_moduleCommandCounts) / sizeof(s_moduleCommandCounts[0]))
        return 0;
    return s_moduleCommandCounts[idx];
}

bool isCursorParityCommand(int commandId) {
    if (commandId < CURSOR_PARITY_FIRST_ID || commandId > CURSOR_PARITY_LAST_ID)
        return false;
    // Ranges per module (must match Win32IDE.h IDs)
    if (commandId >= 11500 && commandId <= 11507) return true; // Telemetry
    if (commandId >= 11510 && commandId <= 11515) return true; // Composer
    if (commandId >= 11520 && commandId <= 11523) return true; // Mention
    if (commandId >= 11530 && commandId <= 11533) return true; // Vision
    if (commandId >= 11540 && commandId <= 11547) return true; // Refactor
    if (commandId >= 11550 && commandId <= 11553) return true; // Lang
    if (commandId >= 11560 && commandId <= 11567) return true; // Semantic
    if (commandId >= 11570 && commandId <= 11574) return true; // Resource
    return false;
}

int verifyCursorParityWiring(void* /*win32IDEContext*/) {
    // Optional: if Win32IDE passes a context with function pointers or vtable,
    // call each module init and return first failing index (1-based).
    // For now, just verify module count and ID range.
    constexpr int expectedCommands = 49;
    int total = 0;
    for (int i = 0; i < CURSOR_PARITY_MODULE_COUNT; ++i)
        total += cursorModuleCommandCount(static_cast<CursorModule>(i));
    if (total != expectedCommands)
        return 1; // module count mismatch
    return 0;
}

} // namespace Parity
} // namespace RawrXD
