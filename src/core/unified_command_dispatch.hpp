// ============================================================================
// unified_command_dispatch.hpp — Zero-Drift Unified Command Dispatcher
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
//
// This file provides the ONLY dispatch path for all commands:
//   - GUI (WM_COMMAND) → dispatchByGuiId()
//   - CLI (terminal)   → dispatchByCli()
//   - Palette          → dispatchByCanonical()
//   - Programmatic     → dispatchByGuiId() or dispatchByCanonical()
//
// All dispatch reads from g_commandRegistry[] (COMMAND_TABLE in
// command_registry.hpp). There is NO legacy fallback. There is
// NO second registry. There is NO manual wiring.
//
// If a command is not in COMMAND_TABLE → it does not exist.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifndef RAWRXD_UNIFIED_COMMAND_DISPATCH_HPP
#define RAWRXD_UNIFIED_COMMAND_DISPATCH_HPP

#include "command_registry.hpp"
#include <cstring>
#include <cstdio>

namespace RawrXD::Dispatch {

// ============================================================================
// DISPATCH RESULT — What happened when we tried to run a command
// ============================================================================

enum class DispatchStatus : uint8_t {
    OK              = 0,   // Handler ran successfully
    HANDLER_ERROR   = 1,   // Handler ran but returned error
    NOT_FOUND       = 2,   // No command with this ID/alias/name
    WRONG_EXPOSURE  = 3,   // Command exists but not exposed to this source
    PRECOND_FAIL    = 4,   // Precondition flags not met (no file, no model, etc.)
    NULL_HANDLER    = 5    // Registered but handler is null (link error state)
};

struct DispatchResult {
    DispatchStatus  status;
    CommandResult   cmdResult;     // From the handler if it ran
    const char*     detail;        // Human-readable explanation
    const CmdDescriptor* matched;  // Which descriptor matched (nullptr if NOT_FOUND)

    static DispatchResult ok(CommandResult cr, const CmdDescriptor* d) {
        return { DispatchStatus::OK, cr, "OK", d };
    }
    static DispatchResult notFound(const char* what) {
        return { DispatchStatus::NOT_FOUND, CommandResult::error("Not found"), what, nullptr };
    }
    static DispatchResult wrongExposure(const CmdDescriptor* d) {
        return { DispatchStatus::WRONG_EXPOSURE, 
                 CommandResult::error("Wrong exposure"), 
                 "Command not exposed to this source", d };
    }
    static DispatchResult nullHandler(const CmdDescriptor* d) {
        return { DispatchStatus::NULL_HANDLER, 
                 CommandResult::error("Null handler"), 
                 "Handler not linked", d };
    }
    static DispatchResult handlerError(CommandResult cr, const CmdDescriptor* d) {
        return { DispatchStatus::HANDLER_ERROR, cr, cr.detail, d };
    }
};

// ============================================================================
// LOOKUP FUNCTIONS — Find descriptors in g_commandRegistry[]
// ============================================================================

// O(n) scan by Win32 command ID. For 300 commands this is <1μs.
// A perfect hash or sorted array could make it O(1) but adds complexity
// for zero measurable benefit at this scale.
inline const CmdDescriptor* lookupById(uint32_t commandId) {
    if (commandId == 0) return nullptr;  // CLI-only commands have ID=0
    for (size_t i = 0; i < g_commandRegistrySize; ++i) {
        if (g_commandRegistry[i].id == commandId) {
            return &g_commandRegistry[i];
        }
    }
    return nullptr;
}

// O(n) scan by canonical name (e.g. "file.new", "lsp.gotoDef")
inline const CmdDescriptor* lookupByCanonical(const char* name) {
    if (!name) return nullptr;
    for (size_t i = 0; i < g_commandRegistrySize; ++i) {
        if (std::strcmp(g_commandRegistry[i].canonicalName, name) == 0) {
            return &g_commandRegistry[i];
        }
    }
    return nullptr;
}

// O(n) scan by CLI alias (e.g. "!save", "!lsp start")
// Matches the FULL alias string. Input should include the '!' prefix.
inline const CmdDescriptor* lookupByCli(const char* cliInput) {
    if (!cliInput || cliInput[0] != '!') return nullptr;
    for (size_t i = 0; i < g_commandRegistrySize; ++i) {
        const char* alias = g_commandRegistry[i].cliAlias;
        if (!alias) continue;
        // Compare input against alias (alias includes '!' prefix)
        if (std::strcmp(cliInput, alias) == 0) {
            return &g_commandRegistry[i];
        }
    }
    return nullptr;
}

// Prefix match for CLI — finds command where input starts with alias
// Handles "!lsp start --verbose" matching "!lsp start"
inline const CmdDescriptor* lookupByCliPrefix(const char* cliInput) {
    if (!cliInput || cliInput[0] != '!') return nullptr;
    
    const CmdDescriptor* bestMatch = nullptr;
    size_t bestLen = 0;
    
    for (size_t i = 0; i < g_commandRegistrySize; ++i) {
        const char* alias = g_commandRegistry[i].cliAlias;
        if (!alias) continue;
        size_t aliasLen = std::strlen(alias);
        
        // Input must start with alias, and next char must be \0 or space
        if (aliasLen > bestLen &&
            std::strncmp(cliInput, alias, aliasLen) == 0 &&
            (cliInput[aliasLen] == '\0' || cliInput[aliasLen] == ' '))
        {
            bestMatch = &g_commandRegistry[i];
            bestLen = aliasLen;
        }
    }
    return bestMatch;
}

// ============================================================================
// DISPATCH BY GUI COMMAND ID
// ============================================================================
// Called from Win32IDE WM_COMMAND handler. Replaces legacy routeCommand().

inline DispatchResult dispatchByGuiId(uint32_t commandId, CommandContext& ctx) {
    const CmdDescriptor* desc = lookupById(commandId);
    if (!desc) {
        return DispatchResult::notFound("Unknown GUI command ID");
    }
    
    // Verify exposure
    if (desc->exposure != CmdExposure::GUI_ONLY && 
        desc->exposure != CmdExposure::BOTH) {
        return DispatchResult::wrongExposure(desc);
    }
    
    // Verify handler exists
    if (!desc->handler) {
        return DispatchResult::nullHandler(desc);
    }
    
    // Set context
    ctx.commandId = commandId;
    ctx.isGui = true;
    
    // Execute
    CommandResult cr = desc->handler(ctx);
    if (cr.success) {
        return DispatchResult::ok(cr, desc);
    }
    return DispatchResult::handlerError(cr, desc);
}

// ============================================================================
// DISPATCH BY CLI ALIAS
// ============================================================================
// Called from terminal handler. Replaces dispatchByCli in SharedFeatureRegistry.
// Input format: "!command args..." — e.g. "!lsp start", "!save", "!router pin"

inline DispatchResult dispatchByCli(const char* cliInput, CommandContext& ctx) {
    if (!cliInput || cliInput[0] != '!') {
        return DispatchResult::notFound("Not a CLI command (missing !)");
    }
    
    // Find matching command (longest-prefix match)
    const CmdDescriptor* desc = lookupByCliPrefix(cliInput);
    if (!desc) {
        return DispatchResult::notFound("Unknown CLI command");
    }
    
    // Verify exposure
    if (desc->exposure != CmdExposure::CLI_ONLY &&
        desc->exposure != CmdExposure::BOTH) {
        return DispatchResult::wrongExposure(desc);
    }
    
    // Verify handler exists
    if (!desc->handler) {
        return DispatchResult::nullHandler(desc);
    }
    
    // Extract args (everything after the alias)
    size_t aliasLen = std::strlen(desc->cliAlias);
    const char* args = cliInput + aliasLen;
    while (*args == ' ') ++args;  // Skip whitespace
    
    ctx.commandId = desc->id;
    ctx.args = args;
    ctx.rawInput = cliInput;
    ctx.isGui = false;
    
    // Execute
    CommandResult cr = desc->handler(ctx);
    if (cr.success) {
        return DispatchResult::ok(cr, desc);
    }
    return DispatchResult::handlerError(cr, desc);
}

// ============================================================================
// DISPATCH BY CANONICAL NAME
// ============================================================================
// Called from command palette, programmatic dispatch, replay harness.
// Name format: "file.new", "lsp.gotoDef", "router.pin"

inline DispatchResult dispatchByCanonical(const char* name, CommandContext& ctx) {
    const CmdDescriptor* desc = lookupByCanonical(name);
    if (!desc) {
        return DispatchResult::notFound("Unknown canonical command name");
    }
    
    if (!desc->handler) {
        return DispatchResult::nullHandler(desc);
    }
    
    ctx.commandId = desc->id;
    
    CommandResult cr = desc->handler(ctx);
    if (cr.success) {
        return DispatchResult::ok(cr, desc);
    }
    return DispatchResult::handlerError(cr, desc);
}

// ============================================================================
// UNIFIED ENTRY POINT — Auto-detects input type
// ============================================================================
// Accepts:
//   "!command"     → CLI dispatch
//   "name.dotted"  → Canonical dispatch
//   numeric string → GUI ID dispatch

inline DispatchResult dispatchUnified(const char* input, CommandContext& ctx) {
    if (!input || input[0] == '\0') {
        return DispatchResult::notFound("Empty input");
    }
    
    // CLI command (starts with !)
    if (input[0] == '!') {
        return dispatchByCli(input, ctx);
    }
    
    // Numeric (GUI command ID)
    bool allDigit = true;
    for (const char* p = input; *p; ++p) {
        if (*p < '0' || *p > '9') { allDigit = false; break; }
    }
    if (allDigit) {
        uint32_t id = static_cast<uint32_t>(std::atoi(input));
        return dispatchByGuiId(id, ctx);
    }
    
    // Canonical name (dotted notation)
    return dispatchByCanonical(input, ctx);
}

// ============================================================================
// ITERATION / INTROSPECTION — For help, telemetry, audit
// ============================================================================

// Get all commands in a category
inline size_t getCommandsByCategory(const char* category, 
                                     const CmdDescriptor** out, 
                                     size_t maxOut) {
    size_t count = 0;
    for (size_t i = 0; i < g_commandRegistrySize && count < maxOut; ++i) {
        if (std::strcmp(g_commandRegistry[i].category, category) == 0) {
            out[count++] = &g_commandRegistry[i];
        }
    }
    return count;
}

// Get all CLI-accessible commands
inline size_t getCliCommands(const CmdDescriptor** out, size_t maxOut) {
    size_t count = 0;
    for (size_t i = 0; i < g_commandRegistrySize && count < maxOut; ++i) {
        if (g_commandRegistry[i].exposure == CmdExposure::CLI_ONLY ||
            g_commandRegistry[i].exposure == CmdExposure::BOTH) {
            out[count++] = &g_commandRegistry[i];
        }
    }
    return count;
}

// Get all GUI-accessible commands
inline size_t getGuiCommands(const CmdDescriptor** out, size_t maxOut) {
    size_t count = 0;
    for (size_t i = 0; i < g_commandRegistrySize && count < maxOut; ++i) {
        if (g_commandRegistry[i].id != 0 &&
            (g_commandRegistry[i].exposure == CmdExposure::GUI_ONLY ||
             g_commandRegistry[i].exposure == CmdExposure::BOTH)) {
            out[count++] = &g_commandRegistry[i];
        }
    }
    return count;
}

// Get unique category list
inline size_t getCategories(const char** out, size_t maxOut) {
    size_t count = 0;
    for (size_t i = 0; i < g_commandRegistrySize && count < maxOut; ++i) {
        const char* cat = g_commandRegistry[i].category;
        bool seen = false;
        for (size_t j = 0; j < count; ++j) {
            if (std::strcmp(out[j], cat) == 0) { seen = true; break; }
        }
        if (!seen) {
            out[count++] = cat;
        }
    }
    return count;
}

// ============================================================================
// AUTO-GENERATED HELP — Replaces manual !help implementation
// ============================================================================

inline void printHelp(const CommandContext& ctx, const char* filterCategory = nullptr) {
    char buf[512];
    
    if (!filterCategory) {
        ctx.outputLine("╔══════════════════════════════════════════════════════════╗");
        ctx.outputLine("║        RawrXD IDE — Unified Command Reference           ║");
        ctx.outputLine("╚══════════════════════════════════════════════════════════╝");
        std::snprintf(buf, sizeof(buf), "  Total commands: %zu (GUI: %zu, CLI: %zu)",
                      g_commandRegistrySize, g_guiCommandCount, g_cliCommandCount);
        ctx.outputLine(buf);
        ctx.outputLine("");
    }
    
    // Collect categories
    const char* categories[64] = {};
    size_t catCount = getCategories(categories, 64);
    
    for (size_t c = 0; c < catCount; ++c) {
        if (filterCategory && std::strcmp(categories[c], filterCategory) != 0) continue;
        
        std::snprintf(buf, sizeof(buf), "── %s ──", categories[c]);
        ctx.outputLine(buf);
        
        for (size_t i = 0; i < g_commandRegistrySize; ++i) {
            const auto& cmd = g_commandRegistry[i];
            if (std::strcmp(cmd.category, categories[c]) != 0) continue;
            
            // Only show CLI-accessible commands in help
            if (cmd.exposure == CmdExposure::GUI_ONLY) continue;
            
            std::snprintf(buf, sizeof(buf), "  %-24s  %s", 
                          cmd.cliAlias ? cmd.cliAlias : "(no alias)",
                          cmd.canonicalName);
            ctx.outputLine(buf);
        }
        ctx.outputLine("");
    }
}

// ============================================================================
// TELEMETRY EXPORT — Auto-generated from table
// ============================================================================

inline void exportRegistryAsJSON(char* out, size_t maxLen) {
    size_t pos = 0;
    pos += std::snprintf(out + pos, maxLen - pos, "{\n  \"commands\": [\n");
    
    for (size_t i = 0; i < g_commandRegistrySize; ++i) {
        const auto& cmd = g_commandRegistry[i];
        const char* expStr = "UNKNOWN";
        switch (cmd.exposure) {
            case CmdExposure::GUI_ONLY: expStr = "GUI"; break;
            case CmdExposure::CLI_ONLY: expStr = "CLI"; break;
            case CmdExposure::BOTH:     expStr = "BOTH"; break;
            case CmdExposure::INTERNAL: expStr = "INTERNAL"; break;
        }
        
        pos += std::snprintf(out + pos, maxLen - pos,
            "    {\"id\":%u,\"symbol\":\"%s\",\"name\":\"%s\","
            "\"cli\":\"%s\",\"exposure\":\"%s\",\"category\":\"%s\","
            "\"flags\":%u,\"hasHandler\":%s}%s\n",
            cmd.id, cmd.symbol, cmd.canonicalName,
            cmd.cliAlias ? cmd.cliAlias : "",
            expStr, cmd.category, cmd.flags,
            cmd.handler ? "true" : "false",
            (i + 1 < g_commandRegistrySize) ? "," : "");
        
        if (pos >= maxLen - 256) break;  // Safety margin
    }
    
    pos += std::snprintf(out + pos, maxLen - pos,
        "  ],\n  \"total\": %zu,\n  \"guiCount\": %zu,\n  \"cliCount\": %zu\n}\n",
        g_commandRegistrySize, g_guiCommandCount, g_cliCommandCount);
}

// ============================================================================
// SELF-AUDIT — Detects integrity issues at startup
// ============================================================================

struct AuditResult {
    size_t totalCommands;
    size_t guiCommands;
    size_t cliCommands;
    size_t nullHandlers;
    size_t duplicateIds;
    size_t duplicateAliases;
    bool   isClean;
};

inline AuditResult auditRegistry() {
    AuditResult result = {};
    result.totalCommands = g_commandRegistrySize;
    
    for (size_t i = 0; i < g_commandRegistrySize; ++i) {
        const auto& cmd = g_commandRegistry[i];
        
        // Count by exposure
        if (cmd.id != 0) result.guiCommands++;
        if (cmd.exposure == CmdExposure::CLI_ONLY || cmd.exposure == CmdExposure::BOTH) {
            result.cliCommands++;
        }
        
        // Check for null handlers
        if (!cmd.handler) result.nullHandlers++;
        
        // Check for duplicate IDs (skip CLI-only which all have 0)
        if (cmd.id != 0) {
            for (size_t j = i + 1; j < g_commandRegistrySize; ++j) {
                if (g_commandRegistry[j].id == cmd.id) {
                    result.duplicateIds++;
                }
            }
        }
        
        // Check for duplicate CLI aliases
        if (cmd.cliAlias) {
            for (size_t j = i + 1; j < g_commandRegistrySize; ++j) {
                if (g_commandRegistry[j].cliAlias &&
                    std::strcmp(cmd.cliAlias, g_commandRegistry[j].cliAlias) == 0) {
                    result.duplicateAliases++;
                }
            }
        }
    }
    
    result.isClean = (result.nullHandlers == 0 && 
                      result.duplicateIds == 0 && 
                      result.duplicateAliases == 0);
    return result;
}

} // namespace RawrXD::Dispatch

#endif // RAWRXD_UNIFIED_COMMAND_DISPATCH_HPP
