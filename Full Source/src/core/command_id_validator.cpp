// ============================================================================
// command_id_validator.cpp — Runtime Command ID Collision Detector
// ============================================================================
//
// Architecture: C++20, Win32, no Qt, no exceptions
// Purpose:     Validates ALL registered command IDs at startup.
//              Detects: duplicate IDs, dead zone violations, zone mismatches.
//              Fail-fast: crashes immediately on collision (debug-by-design).
//
// Integration: Called from UnifiedHotpatchManager init or WinMain.
//              Also callable as part of self_test_gate.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "command_id_blueprint.h"
#include "shared_feature_dispatch.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <vector>
#include <string>

// ============================================================================
// validateCommandIds() — Full registry scan
// ============================================================================

CommandIdValidation validateCommandIds() {
    auto& registry = SharedFeatureRegistry::instance();
    const auto& features = registry.allFeatures();
    
    CommandIdValidation result;
    result.valid = true;
    result.totalChecked = 0;
    result.collisionCount = 0;
    result.deadZoneViolations = 0;
    
    // Map: commandId → feature ID string (for collision detection)
    std::unordered_map<uint32_t, const char*> idMap;
    
    for (const auto& feat : features) {
        uint32_t cmdId = feat.commandId;
        
        // Skip CLI-only features (commandId == 0)
        if (cmdId == 0) continue;
        
        result.totalChecked++;
        
        // ── Check 1: Dead zone violation ──
        if (CommandZone::isDeadZone(cmdId)) {
            result.valid = false;
            result.deadZoneViolations++;
            result.deadZoneIds.push_back(cmdId);
            
            // Log immediately for debugger visibility
            char msg[512];
            snprintf(msg, sizeof(msg),
                "[FATAL] Command ID %u (%s) is in DEAD ZONE (%s)\n",
                cmdId, feat.id, CommandZone::zoneNameFor(cmdId));
#ifdef _WIN32
            OutputDebugStringA(msg);
#endif
            fprintf(stderr, "%s", msg);
        }
        
        // ── Check 2: Duplicate ID collision ──
        auto it = idMap.find(cmdId);
        if (it != idMap.end()) {
            result.valid = false;
            result.collisionCount++;
            
            CommandIdCollision collision;
            collision.id = cmdId;
            collision.featureA = it->second;
            collision.featureB = feat.id;
            collision.zone = CommandZone::zoneNameFor(cmdId);
            result.collisions.push_back(collision);
            
            char msg[512];
            snprintf(msg, sizeof(msg),
                "[FATAL] Command ID COLLISION: %u claimed by BOTH '%s' and '%s' (zone: %s)\n",
                cmdId, it->second, feat.id, CommandZone::zoneNameFor(cmdId));
#ifdef _WIN32
            OutputDebugStringA(msg);
#endif
            fprintf(stderr, "%s", msg);
        } else {
            idMap[cmdId] = feat.id;
        }
    }
    
    // ── Check 3: Validate plugin IDs don't overlap core ──
    auto& pluginAlloc = PluginIdAllocator::instance();
    if (!pluginAlloc.validatePluginIds()) {
        result.valid = false;
        
        char msg[256];
        snprintf(msg, sizeof(msg),
            "[FATAL] Plugin IDs overlap core or dead zones\n");
#ifdef _WIN32
        OutputDebugStringA(msg);
#endif
        fprintf(stderr, "%s", msg);
    }
    
    // ── Summary ──
    char summary[512];
    snprintf(summary, sizeof(summary),
        "[CommandIdValidator] Checked %zu IDs: %zu collisions, %zu dead-zone violations — %s\n",
        result.totalChecked,
        result.collisionCount,
        result.deadZoneViolations,
        result.valid ? "PASS" : "FAIL");
#ifdef _WIN32
    OutputDebugStringA(summary);
#endif
    fprintf(stderr, "%s", summary);
    
    return result;
}

// ============================================================================
// enforceCommandIdIntegrity() — Crash on failure
// ============================================================================

void enforceCommandIdIntegrity() {
    CommandIdValidation result = validateCommandIds();
    
    if (!result.valid) {
        // Build a fatal message with all collision details
        std::string fatal = "╔══════════════════════════════════════════════════╗\n";
        fatal +=            "║       COMMAND ID INTEGRITY CHECK FAILED          ║\n";
        fatal +=            "╚══════════════════════════════════════════════════╝\n\n";
        
        if (!result.collisions.empty()) {
            fatal += "COLLISIONS (" + std::to_string(result.collisionCount) + "):\n";
            for (const auto& c : result.collisions) {
                fatal += "  ID " + std::to_string(c.id) + ": '" +
                         c.featureA + "' vs '" + c.featureB +
                         "' [" + c.zone + "]\n";
            }
            fatal += "\n";
        }
        
        if (!result.deadZoneIds.empty()) {
            fatal += "DEAD ZONE VIOLATIONS (" + 
                     std::to_string(result.deadZoneViolations) + "):\n";
            for (uint32_t dz : result.deadZoneIds) {
                fatal += "  ID " + std::to_string(dz) + " → " +
                         CommandZone::zoneNameFor(dz) + "\n";
            }
        }
        
#ifdef _WIN32
        OutputDebugStringA(fatal.c_str());
        // In debug builds, trigger a breakpoint for immediate investigation
        #ifdef _DEBUG
            __debugbreak();
        #endif
        // MessageBox for release builds (so it doesn't silently die)
        MessageBoxA(nullptr, fatal.c_str(),
                    "RawrXD: Command ID Integrity Failure", MB_ICONERROR | MB_OK);
#endif
        fprintf(stderr, "%s\n", fatal.c_str());
        
        // Crash with identifiable exit code
        // 0xDEAD1D = "DEAD ID" — searchable in crash dumps
        exit(0xDEAD1D);
    }
}

// ============================================================================
// Self-test entry point — callable from !self_test CLI command
// ============================================================================

CommandResult handleCommandIdSelfTest(const CommandContext& ctx) {
    CommandIdValidation result = validateCommandIds();
    
    if (result.valid) {
        std::string msg = "Command ID validation PASSED: " +
                          std::to_string(result.totalChecked) +
                          " IDs checked, 0 collisions, 0 dead-zone violations.\n";
        
        // Also report zone usage density
        msg += "\nZone Density Report:\n";
        
        struct ZoneInfo {
            const char* name;
            uint32_t min;
            uint32_t max;
            size_t   used;
        };
        
        ZoneInfo zones[] = {
            {"File",          CommandZone::FILE_MIN,      CommandZone::FILE_MAX,      0},
            {"Edit",          CommandZone::EDIT_MIN,      CommandZone::EDIT_MAX,      0},
            {"Git",           CommandZone::GIT_MIN,       CommandZone::GIT_MAX,       0},
            {"Themes",        CommandZone::THEME_MIN,     CommandZone::THEME_MAX,     0},
            {"Terminal",      CommandZone::TERMINAL_MIN,  CommandZone::TERMINAL_MAX,  0},
            {"Agent",         CommandZone::AGENT_MIN,     CommandZone::AGENT_MAX,     0},
            {"Autonomy",      CommandZone::AUTONOMY_MIN,  CommandZone::AUTONOMY_MAX,  0},
            {"AI Mode",       CommandZone::AI_MODE_MIN,   CommandZone::AI_MODE_MAX,   0},
            {"RevEng",        CommandZone::RE_MIN,        CommandZone::RE_MAX,        0},
            {"Debug",         CommandZone::DEBUG_MIN,     CommandZone::DEBUG_MAX,     0},
            {"Hotpatch",      CommandZone::HOTPATCH_MIN,  CommandZone::HOTPATCH_MAX,  0},
            {"Editor Engine", CommandZone::EDITOR_ENG_MIN,CommandZone::EDITOR_ENG_MAX,0},
            {"Voice",         CommandZone::VOICE_MIN,     CommandZone::VOICE_MAX,     0},
            {"Help",          CommandZone::HELP_MIN,      CommandZone::HELP_MAX,      0},
        };
        
        auto& registry = SharedFeatureRegistry::instance();
        const auto& features = registry.allFeatures();
        
        for (const auto& feat : features) {
            if (feat.commandId == 0) continue;
            for (auto& z : zones) {
                if (feat.commandId >= z.min && feat.commandId <= z.max) {
                    z.used++;
                    break;
                }
            }
        }
        
        for (const auto& z : zones) {
            uint32_t capacity = z.max - z.min + 1;
            float pct = (capacity > 0) ? (100.0f * z.used / capacity) : 0.0f;
            char line[128];
            snprintf(line, sizeof(line), "  %-14s %3zu / %3u IDs used (%.0f%%)\n",
                     z.name, z.used, capacity, pct);
            msg += line;
        }
        
        // Plugin allocator status
        msg += "\n" + PluginIdAllocator::instance().generateReport();
        
        ctx.output(msg.c_str());
        return CommandResult::ok("Validation passed");
    } else {
        std::string err = "Command ID validation FAILED:\n";
        err += "  Collisions: " + std::to_string(result.collisionCount) + "\n";
        err += "  Dead zone violations: " + std::to_string(result.deadZoneViolations) + "\n";
        for (const auto& c : result.collisions) {
            err += "  COLLISION: ID " + std::to_string(c.id) + " → '" +
                   c.featureA + "' vs '" + c.featureB + "'\n";
        }
        ctx.output(err.c_str());
        return CommandResult::error("Validation failed");
    }
}
