// ============================================================================
// cli_feature_bridge.h — CLI ↔ SharedFeatureRegistry Adapter
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// Provides route_command_unified() that cli_shell.cpp calls instead of
// its massive if/else chain. Falls back to legacy routing for commands
// not yet registered in the shared dispatch.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifndef RAWRXD_CLI_FEATURE_BRIDGE_H
#define RAWRXD_CLI_FEATURE_BRIDGE_H

#include "../core/shared_feature_dispatch.h"
#include "../core/unified_command_dispatch.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>

// ============================================================================
// CLI OUTPUT CALLBACK
// ============================================================================

static void cli_stdout_output(const char* text, void* /*userData*/) {
    if (text) fputs(text, stdout);
}

// ============================================================================
// EXTRACT ARGS — Get everything after the command keyword
// ============================================================================

static std::string extractArgs(const char* input, const char* prefix) {
    if (!input || !prefix) return {};
    const char* p = input;
    
    // Skip the command prefix
    size_t prefixLen = strlen(prefix);
    if (strncmp(p, prefix, prefixLen) == 0) {
        p += prefixLen;
    }
    
    // Skip whitespace
    while (*p == ' ' || *p == '\t') ++p;
    
    return std::string(p);
}

// ============================================================================
// UNIFIED CLI ROUTER
// ============================================================================
// Returns true if the command was handled by the shared dispatch.
// Returns false if the command should fall back to legacy routing.
// ============================================================================

static bool route_command_unified(const char* input, void* cliStatePtr) {
    if (!input || !input[0]) return false;
    
    // ═══════════════════════════════════════════════════════════════════
    // PRIMARY PATH: Direct dispatch from g_commandRegistry[] (SSOT)
    // No hash map lookup, no manual registration required.
    // If a command is in COMMAND_TABLE, it gets dispatched here.
    // ═══════════════════════════════════════════════════════════════════
    
    // Build the command context
    CommandContext ctx{};
    ctx.rawInput = input;
    ctx.idePtr = nullptr;
    ctx.cliStatePtr = cliStatePtr;
    ctx.commandId = 0;
    ctx.isGui = false;
    ctx.isHeadless = false;
    ctx.outputFn = cli_stdout_output;
    ctx.outputUserData = nullptr;

    // ── SSOT dispatch: try g_commandRegistry[] first ──────────────────────
    {
        auto ssotResult = RawrXD::Dispatch::dispatchByCli(input, ctx);
        if (ssotResult.status == RawrXD::Dispatch::DispatchStatus::OK ||
            ssotResult.status == RawrXD::Dispatch::DispatchStatus::HANDLER_ERROR) {
            return true;  // Handled by compile-time registry
        }
    }

    // ── FALLBACK: SharedFeatureRegistry for dynamically-registered commands ─
    auto& registry = SharedFeatureRegistry::instance();
    
    // ── Try exact CLI command match ────────────────────────────────────────
    // Extract the command word (first token)
    std::string line(input);
    
    // Trim leading/trailing whitespace
    auto start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return false;
    auto end = line.find_last_not_of(" \t\r\n");
    line = line.substr(start, end - start + 1);
    
    // Find the command part (first word or multi-word like "!voice record")
    std::string cmd;
    std::string args;
    
    // Try matching on full multi-word commands first
    // e.g., "!voice record hello" → try "!voice record", then "!voice"
    for (size_t i = line.size(); i > 0; ) {
        // Find the last space within our candidate command
        auto spacePos = line.rfind(' ', i - 1);
        std::string candidate;
        
        if (spacePos == std::string::npos || spacePos == 0) {
            // Try the first word only
            auto firstSpace = line.find(' ');
            if (firstSpace == std::string::npos) {
                candidate = line;
                args = "";
            } else {
                candidate = line.substr(0, firstSpace);
                args = line.substr(firstSpace + 1);
                // Trim args
                auto aStart = args.find_first_not_of(" \t");
                if (aStart != std::string::npos) args = args.substr(aStart);
                else args = "";
            }
            
            ctx.args = args.c_str();
            auto result = registry.dispatchByCli(candidate.c_str(), ctx);
            if (result.success) return true;
            
            // Also try with the full multi-word
            if (firstSpace != std::string::npos) {
                // Try "!voice record" as a unit
                auto secondSpace = line.find(' ', firstSpace + 1);
                if (secondSpace != std::string::npos) {
                    candidate = line.substr(0, secondSpace);
                    args = line.substr(secondSpace + 1);
                    auto aStart2 = args.find_first_not_of(" \t");
                    if (aStart2 != std::string::npos) args = args.substr(aStart2);
                    else args = "";
                    ctx.args = args.c_str();
                    result = registry.dispatchByCli(candidate.c_str(), ctx);
                    if (result.success) return true;
                }
            }
            
            break; // No more candidates
        }
        
        i = spacePos;
    }
    
    // ── Try /-prefix to !-prefix translation ───────────────────────────────
    if (line[0] == '/') {
        std::string bangLine = "!" + line.substr(1);
        
        auto firstSpace = bangLine.find(' ');
        std::string bangCmd;
        if (firstSpace == std::string::npos) {
            bangCmd = bangLine;
            args = "";
        } else {
            bangCmd = bangLine.substr(0, firstSpace);
            args = bangLine.substr(firstSpace + 1);
            auto aStart = args.find_first_not_of(" \t");
            if (aStart != std::string::npos) args = args.substr(aStart);
            else args = "";
        }
        
        ctx.args = args.c_str();
        auto result = registry.dispatchByCli(bangCmd.c_str(), ctx);
        if (result.success) return true;
    }
    
    // ── Not handled by shared dispatch → fall back to legacy ───────────────
    return false;
}

// ============================================================================
// CLI HELP TEXT — Generated from shared registry
// ============================================================================

static void printUnifiedHelp() {
    auto& registry = SharedFeatureRegistry::instance();
    auto cliFeatures = registry.getCliFeatures();
    
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║         RawrXD CLI — Unified Feature Dispatch v1.0         ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("  %zu features available (CLI + GUI unified)\n\n", cliFeatures.size());
    
    FeatureGroup lastGroup = static_cast<FeatureGroup>(0xFFFF);
    
    for (const auto* f : cliFeatures) {
        if (f->group != lastGroup) {
            lastGroup = f->group;
            printf("\n  ── %s ──\n", f->name); // Use first feature name as section header
        }
        
        if (f->cliCommand && f->cliCommand[0]) {
            printf("    %-24s %s", f->cliCommand, f->description);
            if (f->shortcut && f->shortcut[0]) {
                printf("  [%s]", f->shortcut);
            }
            printf("\n");
        }
    }
    
    printf("\n  Type !manifest_json or !manifest_md for full manifest.\n");
    printf("  Type !self_test to validate all features.\n\n");
}

// ============================================================================
// CLI FEATURE COUNT for status bar
// ============================================================================

static size_t getCliFeatureCount() {
    return SharedFeatureRegistry::instance().getCliFeatures().size();
}

static size_t getGuiFeatureCount() {
    return SharedFeatureRegistry::instance().getGuiFeatures().size();
}

static size_t getTotalFeatureCount() {
    return SharedFeatureRegistry::instance().totalRegistered();
}

#endif // RAWRXD_CLI_FEATURE_BRIDGE_H
