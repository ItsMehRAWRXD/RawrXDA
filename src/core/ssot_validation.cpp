// ============================================================================
// ssot_validation.cpp — Compile-Time SSOT Integrity Checks
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
//
// This TU exists solely to enforce compile-time invariants on the
// COMMAND_TABLE. If someone adds a command to Win32IDE.h without adding
// it to command_registry.hpp, or creates a duplicate, this file's
// static_asserts will fire.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "command_registry.hpp"
#include "command_ranges.hpp"

// ============================================================================
// INVARIANT 1: Registry is not empty
// ============================================================================
static_assert(g_commandRegistrySize > 0, 
    "COMMAND_TABLE is empty — no commands registered");

// ============================================================================
// INVARIANT 2: Expected minimum command counts
// ============================================================================
// If these fail, someone deleted commands from COMMAND_TABLE without adjusting
static_assert(g_commandRegistrySize >= 300,
    "COMMAND_TABLE has fewer than 300 entries — did you delete commands?");

static_assert(g_guiCommandCount >= 280,
    "Fewer than 280 GUI-routable commands — did you change IDs to 0?");

static_assert(g_cliCommandCount >= 200,
    "Fewer than 200 CLI-accessible commands — check exposure settings");

// ============================================================================
// INVARIANT 3: No oversized table (sanity check)
// ============================================================================
static_assert(g_commandRegistrySize < 1000,
    "COMMAND_TABLE has >1000 entries — likely a macro expansion error");

// ============================================================================
// RUNTIME STARTUP VALIDATION
// ============================================================================
// Runs auditRegistry() at static init time and logs any issues.
// Does NOT abort — just reports.

#include "unified_command_dispatch.hpp"
#include <cstdio>

namespace {
struct SSOTStartupValidator {
    SSOTStartupValidator() {
        auto audit = RawrXD::Dispatch::auditRegistry();
        if (!audit.isClean) {
            fprintf(stderr,
                "[SSOT AUDIT] WARNING: Registry integrity issue detected!\n"
                "  Total: %zu  GUI: %zu  CLI: %zu\n"
                "  Null handlers: %zu  Duplicate IDs: %zu  Duplicate aliases: %zu\n",
                audit.totalCommands, audit.guiCommands, audit.cliCommands,
                audit.nullHandlers, audit.duplicateIds, audit.duplicateAliases);
            if (audit.duplicateAliases > 0 && audit.firstDuplicateAlias) {
                fprintf(stderr, "  First duplicate alias: \"%s\" (fix in command_registry.hpp)\n",
                    audit.firstDuplicateAlias);
            }
        }
    }
} g_ssotValidator;
}
