// ============================================================================
// cursor_github_parity_bridge.cpp — Feature modules bridge (command ranges)
// ============================================================================
//
// Implements feature-module manifest and optional runtime verification.
// C++20 or x64 MASM with max dependency removal.
// See: docs/CURSOR_GITHUB_PARITY_SPEC.md
//
// ============================================================================

#include "../../include/cursor_github_parity_bridge.h"

#include <array>

namespace RawrXD
{
namespace Parity
{

static const int s_moduleCommandCounts[] = {
    8,  // TelemetryExport
    6,  // AgenticComposer
    4,  // ContextMention
    5,  // VisionEncoder
    8,  // RefactoringEngine
    4,  // LanguageRegistry
    8,  // SemanticIndex
    5,  // ResourceGenerator
};

static constexpr std::array<int, 48> kCursorParityCommandIds = {
    11500, 11501, 11502, 11503, 11504, 11505, 11506, 11507,
    11510, 11511, 11512, 11513, 11514, 11515,
    11520, 11521, 11522, 11523,
    11530, 11531, 11532, 11533, 11534,
    11540, 11541, 11542, 11543, 11544, 11545, 11546, 11547,
    11550, 11551, 11552, 11553,
    11560, 11561, 11562, 11563, 11564, 11565, 11566, 11567,
    11570, 11571, 11572, 11573, 11574,
};

int cursorModuleCommandCount(CursorModule m)
{
    auto idx = static_cast<size_t>(m);
    if (idx >= sizeof(s_moduleCommandCounts) / sizeof(s_moduleCommandCounts[0]))
        return 0;
    return s_moduleCommandCounts[idx];
}

bool isCursorParityCommand(int commandId)
{
    if (commandId < CURSOR_PARITY_FIRST_ID || commandId > CURSOR_PARITY_LAST_ID)
        return false;
    for (const int id : kCursorParityCommandIds) {
        if (id == commandId)
            return true;
    }
    return false;
}

int verifyCursorParityWiring(void* win32IDEContext)
{
    if (win32IDEContext == nullptr)
        return 1;

    constexpr int expectedCommands = static_cast<int>(kCursorParityCommandIds.size());
    int total = 0;
    for (int i = 0; i < CURSOR_PARITY_MODULE_COUNT; ++i)
        total += cursorModuleCommandCount(static_cast<CursorModule>(i));
    if (total != expectedCommands)
        return 2;  // module count mismatch

    // Check that all listed parity IDs stay inside the declared public range.
    for (const int id : kCursorParityCommandIds) {
        if (id < CURSOR_PARITY_FIRST_ID || id > CURSOR_PARITY_LAST_ID)
            return 3;
    }

    // Ensure ID list has no duplicates (small fixed size, O(n^2) is fine).
    for (size_t i = 0; i < kCursorParityCommandIds.size(); ++i) {
        for (size_t j = i + 1; j < kCursorParityCommandIds.size(); ++j) {
            if (kCursorParityCommandIds[i] == kCursorParityCommandIds[j])
                return 4;
        }
    }

    return 0;
}

}  // namespace Parity
}  // namespace RawrXD
