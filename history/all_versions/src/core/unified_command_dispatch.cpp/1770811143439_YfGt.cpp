// ============================================================================
// unified_command_dispatch.cpp — Implementation of Unified Dispatch System
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
//
// This file provides:
//   1. Auto-registration bridge: COMMAND_TABLE → SharedFeatureRegistry
//   2. Win32 WM_COMMAND entry point replacement
//   3. CLI terminal entry point replacement
//   4. Extern "C" bridges for MASM hot-paths
//   5. Startup integrity verification
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "unified_command_dispatch.hpp"
#include "shared_feature_dispatch.h"
#include "command_registry.hpp"
#include <cstdio>
#include <cstring>

// ============================================================================
// AUTO-REGISTER FROM COMMAND_TABLE → SharedFeatureRegistry
// ============================================================================
// This bridge eliminates feature_registration.cpp entirely.
// SharedFeatureRegistry becomes a thin query layer over g_commandRegistry[].
// The runtime hash maps in SharedFeatureRegistry are populated from the
// compile-time table — zero manual maintenance.

namespace {

// Map CmdExposure → gui/cli booleans for FeatureDescriptor
inline void exposureToBools(CmdExposure exp, bool& gui, bool& cli) {
    switch (exp) {
        case CmdExposure::GUI_ONLY:  gui = true;  cli = false; break;
        case CmdExposure::CLI_ONLY:  gui = false; cli = true;  break;
        case CmdExposure::BOTH:      gui = true;  cli = true;  break;
        case CmdExposure::INTERNAL:  gui = false; cli = false; break;
    }
}

// Map category string → FeatureGroup enum (best-effort)
inline FeatureGroup categoryToGroup(const char* cat) {
    if (!cat) return FeatureGroup::Tools;
    if (std::strcmp(cat, "File") == 0)         return FeatureGroup::FileOps;
    if (std::strcmp(cat, "Edit") == 0)         return FeatureGroup::Editing;
    if (std::strcmp(cat, "View") == 0)         return FeatureGroup::View;
    if (std::strcmp(cat, "Terminal") == 0)      return FeatureGroup::Terminal;
    if (std::strcmp(cat, "Agent") == 0)         return FeatureGroup::Agent;
    if (std::strcmp(cat, "SubAgent") == 0)      return FeatureGroup::SubAgent;
    if (std::strcmp(cat, "Autonomy") == 0)      return FeatureGroup::Autonomy;
    if (std::strcmp(cat, "AIMode") == 0)        return FeatureGroup::AIMode;
    if (std::strcmp(cat, "AIContext") == 0)     return FeatureGroup::AIMode;
    if (std::strcmp(cat, "ReverseEng") == 0)    return FeatureGroup::ReverseEng;
    if (std::strcmp(cat, "Backend") == 0)       return FeatureGroup::LLMRouter;
    if (std::strcmp(cat, "Router") == 0)        return FeatureGroup::LLMRouter;
    if (std::strcmp(cat, "LSP") == 0)           return FeatureGroup::LSP;
    if (std::strcmp(cat, "LSPServer") == 0)     return FeatureGroup::LSP;
    if (std::strcmp(cat, "ASM") == 0)           return FeatureGroup::Decompiler;
    if (std::strcmp(cat, "Hybrid") == 0)        return FeatureGroup::LSP;
    if (std::strcmp(cat, "MultiResp") == 0)     return FeatureGroup::Agent;
    if (std::strcmp(cat, "Governor") == 0)      return FeatureGroup::Agent;
    if (std::strcmp(cat, "Safety") == 0)        return FeatureGroup::Security;
    if (std::strcmp(cat, "Replay") == 0)        return FeatureGroup::Session;
    if (std::strcmp(cat, "Confidence") == 0)    return FeatureGroup::Agent;
    if (std::strcmp(cat, "Swarm") == 0)         return FeatureGroup::Swarm;
    if (std::strcmp(cat, "Debug") == 0)         return FeatureGroup::Debug;
    if (std::strcmp(cat, "Plugin") == 0)        return FeatureGroup::Modules;
    if (std::strcmp(cat, "Hotpatch") == 0)      return FeatureGroup::Hotpatch;
    if (std::strcmp(cat, "Monaco") == 0)        return FeatureGroup::View;
    if (std::strcmp(cat, "Editor") == 0)        return FeatureGroup::Editing;
    if (std::strcmp(cat, "PDB") == 0)           return FeatureGroup::Debug;
    if (std::strcmp(cat, "Audit") == 0)         return FeatureGroup::Tools;
    if (std::strcmp(cat, "Gauntlet") == 0)      return FeatureGroup::Tools;
    if (std::strcmp(cat, "Voice") == 0)         return FeatureGroup::Voice;
    if (std::strcmp(cat, "QW") == 0)            return FeatureGroup::Tools;
    if (std::strcmp(cat, "Telemetry") == 0)     return FeatureGroup::Performance;
    if (std::strcmp(cat, "Theme") == 0)         return FeatureGroup::Themes;
    if (std::strcmp(cat, "Transparency") == 0)  return FeatureGroup::View;
    if (std::strcmp(cat, "Git") == 0)           return FeatureGroup::Git;
    if (std::strcmp(cat, "Help") == 0)          return FeatureGroup::Help;
    if (std::strcmp(cat, "CLI") == 0)           return FeatureGroup::Terminal;
    return FeatureGroup::Tools;
}

// ── Static initializer: runs before main() ──
// Populates SharedFeatureRegistry from g_commandRegistry[]
struct AutoRegistrar {
    AutoRegistrar() {
        auto& registry = SharedFeatureRegistry::instance();
        
        for (size_t i = 0; i < g_commandRegistrySize; ++i) {
            const auto& cmd = g_commandRegistry[i];
            
            FeatureDescriptor fd{};
            fd.id          = cmd.canonicalName;    // "file.new" etc.
            fd.name        = cmd.symbol;           // "FILE_NEW" etc.
            fd.description = cmd.canonicalName;    // Use canonical as description
            fd.group       = categoryToGroup(cmd.category);
            fd.commandId   = cmd.id;
            fd.cliCommand  = cmd.cliAlias;
            fd.shortcut    = "";                   // Shortcuts from accelerator table, not registry
            fd.handler     = cmd.handler;
            
            bool gui = false, cli = false;
            exposureToBools(cmd.exposure, gui, cli);
            fd.guiSupported = gui;
            fd.cliSupported = cli;
            fd.asmHotPath   = (cmd.flags & CMD_ASM_HOTPATH) != 0;
            
            registry.registerFeature(fd);
        }
        
        // Startup integrity check
        auto audit = RawrXD::Dispatch::auditRegistry();
        if (!audit.isClean) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "[WARN] Command registry integrity: %zu null handlers, "
                "%zu duplicate IDs, %zu duplicate aliases",
                audit.nullHandlers, audit.duplicateIds, audit.duplicateAliases);
            // Log to stderr in debug builds
#ifdef _DEBUG
            std::fprintf(stderr, "%s\n", buf);
#endif
        }
    }
};

static AutoRegistrar s_autoRegistrar;

} // anonymous namespace


// ============================================================================
// EXTERN "C" BRIDGE — For MASM hot-paths and cross-module dispatch
// ============================================================================

extern "C" {

int rawrxd_dispatch_feature(const char* featureId, const char* args, void* idePtr) {
    CommandContext ctx{};
    ctx.rawInput = featureId;
    ctx.args     = args;
    ctx.idePtr   = idePtr;
    ctx.isGui    = (idePtr != nullptr);
    ctx.isHeadless = false;
    
    auto result = RawrXD::Dispatch::dispatchByCanonical(featureId, ctx);
    return result.status == RawrXD::Dispatch::DispatchStatus::OK ? 1 : 0;
}

int rawrxd_dispatch_command(uint32_t commandId, void* idePtr) {
    CommandContext ctx{};
    ctx.commandId = commandId;
    ctx.idePtr    = idePtr;
    ctx.isGui     = true;
    ctx.isHeadless = false;
    
    auto result = RawrXD::Dispatch::dispatchByGuiId(commandId, ctx);
    return result.status == RawrXD::Dispatch::DispatchStatus::OK ? 1 : 0;
}

int rawrxd_dispatch_cli(const char* cliCommand, const char* args, void* cliStatePtr) {
    CommandContext ctx{};
    ctx.rawInput    = cliCommand;
    ctx.args        = args;
    ctx.cliStatePtr = cliStatePtr;
    ctx.isGui       = false;
    ctx.isHeadless  = false;
    
    auto result = RawrXD::Dispatch::dispatchByCli(cliCommand, ctx);
    return result.status == RawrXD::Dispatch::DispatchStatus::OK ? 1 : 0;
}

int rawrxd_get_feature_count(void) {
    return static_cast<int>(g_commandRegistrySize);
}

int rawrxd_audit_registry(void) {
    auto audit = RawrXD::Dispatch::auditRegistry();
    return audit.isClean ? 1 : 0;
}

} // extern "C"
