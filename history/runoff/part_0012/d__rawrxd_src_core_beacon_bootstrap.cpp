// ============================================================================
// beacon_bootstrap.cpp — Circular Beacon System Bootstrap & Panel Wiring
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions, no external deps
//
// PURPOSE:
//   Called once at IDE startup (from main_win32.cpp init sequence).
//   Registers every core subsystem and every GUI panel as beacons.
//   Installs verb routes so any beacon can invoke operations cross-system.
//   After bootstrap, the full circular ring is live — every panel can reach
//   every subsystem and every other panel via BeaconHub.
//
// CONNECTIONS ESTABLISHED:
//   Subsystem → Subsystem  (Forward, Reverse, Bilateral)
//   Panel     → Subsystem  (Forward — panel invokes core)
//   Subsystem → Panel      (Reverse — core pushes to UI)
//   Panel     → Panel      (Circular — cross-panel coordination)
//
// RULE: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "../../include/circular_beacon_system.h"
#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD {

// ============================================================================
// Default beacon handler — handles common verbs every beacon understands
// ============================================================================
static BeaconResponse defaultBeaconHandler(const BeaconMessage& msg) {
    BeaconResponse resp{};
    resp.handled = false;
    resp.statusCode = 0;

    if (!msg.verb) return resp;

    // "ping" — every beacon responds to prove liveness
    if (strcmp(msg.verb, "ping") == 0) {
        resp.handled = true;
        resp.result = "pong";
        return resp;
    }

    // "beacon.info" — return beacon kind as string
    if (strcmp(msg.verb, "beacon.info") == 0) {
        resp.handled = true;
        resp.result = beaconKindToString(msg.targetKind);
        return resp;
    }

    // "beacon.stats" — handled by hub, not individual beacons
    // Unknown verb — unhandled (let caller try next beacon)
    return resp;
}

// ============================================================================
// Verb Route Table — maps verbs to their authoritative handler beacons
// ============================================================================
struct VerbRoute {
    const char* verb;
    BeaconKind  handler;
};

static const VerbRoute g_verbRoutes[] = {
    // ── Agentic Engine verbs ──
    { "agent.execute",          BeaconKind::AgenticEngine },
    { "agent.plan",             BeaconKind::AgenticEngine },
    { "agent.loop.start",       BeaconKind::AgenticEngine },
    { "agent.loop.stop",        BeaconKind::AgenticEngine },
    { "agent.tool.invoke",      BeaconKind::AgenticEngine },
    { "agent.memory.query",     BeaconKind::AgenticEngine },
    { "agent.memory.store",     BeaconKind::AgenticEngine },
    { "agent.status",           BeaconKind::AgenticEngine },

    // ── Hotpatch Manager verbs ──
    { "hotpatch.apply",         BeaconKind::HotpatchManager },
    { "hotpatch.rollback",      BeaconKind::HotpatchManager },
    { "hotpatch.list",          BeaconKind::HotpatchManager },
    { "hotpatch.memory.patch",  BeaconKind::HotpatchManager },
    { "hotpatch.byte.patch",    BeaconKind::HotpatchManager },
    { "hotpatch.server.patch",  BeaconKind::HotpatchManager },
    { "hotpatch.status",        BeaconKind::HotpatchManager },
    { "hotpatch.proxy.apply",   BeaconKind::ProxyHotpatcher },

    // ── Plan Executor verbs ──
    { "plan.create",            BeaconKind::PlanExecutor },
    { "plan.execute",           BeaconKind::PlanExecutor },
    { "plan.approve",           BeaconKind::PlanExecutor },
    { "plan.reject",            BeaconKind::PlanExecutor },
    { "plan.pause",             BeaconKind::PlanExecutor },
    { "plan.status",            BeaconKind::PlanExecutor },

    // ── Autonomy Manager verbs ──
    { "autonomy.enable",        BeaconKind::AutonomyManager },
    { "autonomy.disable",       BeaconKind::AutonomyManager },
    { "autonomy.status",        BeaconKind::AutonomyManager },
    { "autonomy.daemon.start",  BeaconKind::AutonomyManager },
    { "autonomy.daemon.stop",   BeaconKind::AutonomyManager },

    // ── LLM Router verbs ──
    { "llm.route",              BeaconKind::LLMRouter },
    { "llm.generate",           BeaconKind::LLMRouter },
    { "llm.classify",           BeaconKind::LLMRouter },
    { "llm.status",             BeaconKind::LLMRouter },
    { "backend.switch",         BeaconKind::BackendSwitcher },
    { "backend.list",           BeaconKind::BackendSwitcher },
    { "backend.health",         BeaconKind::BackendSwitcher },

    // ── Encryption verbs ──
    { "encrypt.aes256",         BeaconKind::EncryptionEngine },
    { "encrypt.camellia256",    BeaconKind::EncryptionEngine },
    { "decrypt.aes256",         BeaconKind::EncryptionEngine },
    { "decrypt.camellia256",    BeaconKind::EncryptionEngine },
    { "encrypt.status",         BeaconKind::EncryptionEngine },

    // ── IDE Core verbs ──
    { "ide.file.open",          BeaconKind::IDECore },
    { "ide.file.save",          BeaconKind::IDECore },
    { "ide.file.close",         BeaconKind::IDECore },
    { "ide.editor.getContent",  BeaconKind::IDECore },
    { "ide.editor.setContent",  BeaconKind::IDECore },
    { "ide.terminal.execute",   BeaconKind::IDECore },
    { "ide.status",             BeaconKind::IDECore },

    // ── Inference Engine verbs ──
    { "inference.generate",     BeaconKind::InferenceEngine },
    { "inference.load",         BeaconKind::InferenceEngine },
    { "inference.unload",       BeaconKind::InferenceEngine },
    { "inference.status",       BeaconKind::InferenceEngine },

    // ── Swarm verbs ──
    { "swarm.dispatch",         BeaconKind::SwarmCoordinator },
    { "swarm.status",           BeaconKind::SwarmCoordinator },
    { "swarm.workers.list",     BeaconKind::SwarmCoordinator },

    // ── Debug verbs ──
    { "debug.start",            BeaconKind::DebugEngine },
    { "debug.stop",             BeaconKind::DebugEngine },
    { "debug.breakpoint.set",   BeaconKind::DebugEngine },
    { "debug.step",             BeaconKind::DebugEngine },
    { "debug.continue",         BeaconKind::DebugEngine },

    // ── LSP verbs ──
    { "lsp.hover",              BeaconKind::LSPServer },
    { "lsp.completion",         BeaconKind::LSPServer },
    { "lsp.definition",         BeaconKind::LSPServer },
    { "lsp.references",         BeaconKind::LSPServer },
    { "lsp.diagnostics",        BeaconKind::LSPServer },

    // ── MCP verbs ──
    { "mcp.tool.list",          BeaconKind::MCPServer },
    { "mcp.tool.invoke",        BeaconKind::MCPServer },
    { "mcp.resource.list",      BeaconKind::MCPServer },

    // ── Voice verbs ──
    { "voice.start",            BeaconKind::VoiceEngine },
    { "voice.stop",             BeaconKind::VoiceEngine },
    { "voice.tts",              BeaconKind::VoiceEngine },

    // ── Git verbs ──
    { "git.commit",             BeaconKind::GitIntegration },
    { "git.push",               BeaconKind::GitIntegration },
    { "git.diff",               BeaconKind::GitIntegration },
    { "git.status",             BeaconKind::GitIntegration },

    // ── Static Analysis verbs ──
    { "analysis.scan",          BeaconKind::StaticAnalysis },
    { "analysis.lint",          BeaconKind::StaticAnalysis },

    // ── Semantic verbs ──
    { "semantic.search",        BeaconKind::SemanticIntel },
    { "semantic.index",         BeaconKind::SemanticIntel },

    // ── Telemetry verbs ──
    { "telemetry.report",       BeaconKind::TelemetryCore },
    { "telemetry.export",       BeaconKind::TelemetryCore },

    // ── Session verbs ──
    { "session.save",           BeaconKind::SessionManager },
    { "session.restore",        BeaconKind::SessionManager },

    // ── Self repair ──
    { "repair.self",            BeaconKind::SelfRepairAgent },
    { "repair.diagnose",        BeaconKind::SelfRepairAgent },

    // ── Tool Registry ──
    { "tools.list",             BeaconKind::ToolRegistry },
    { "tools.invoke",           BeaconKind::ToolRegistry },

    // ── Feature Registry ──
    { "features.list",          BeaconKind::FeatureRegistry },
    { "features.audit",         BeaconKind::FeatureRegistry },

    // ── Flight Recorder ──
    { "flight.record",          BeaconKind::FlightRecorder },
    { "flight.replay",          BeaconKind::FlightRecorder },

    // ── Crash containment ──
    { "crash.report",           BeaconKind::CrashContainment },

    // ── Vulkan ──
    { "vulkan.dispatch",        BeaconKind::VulkanCompute },
    { "vulkan.status",          BeaconKind::VulkanCompute },

    // ── Knowledge Graph ──
    { "kg.query",               BeaconKind::KnowledgeGraph },
    { "kg.store",               BeaconKind::KnowledgeGraph },

    // Sentinel
    { nullptr, BeaconKind::Unknown }
};

// ============================================================================
// bootstrapBeaconSystem — Called once at IDE startup
// ============================================================================
// Parameters:
//   ide — pointer to Win32IDE instance (registered as IDECore beacon)
//
// After this call, every panel can reach every subsystem and vice versa.
// Each panel's init* method should then call its PanelBeaconBridge::init().
// ============================================================================
void bootstrapBeaconSystem(void* ide) {
    auto& hub = BeaconHub::instance();
    
    OutputDebugStringA("[BeaconBootstrap] === Circular Beacon System — Bootstrap Start ===");

    // ── 1. Register IDE Core as the anchor beacon ──
    hub.registerBeacon(BeaconKind::IDECore, "Win32IDE", ide, defaultBeaconHandler);

    // ── 2. Install all verb routes ──
    for (int i = 0; g_verbRoutes[i].verb != nullptr; ++i) {
        hub.registerVerb(g_verbRoutes[i].verb, g_verbRoutes[i].handler);
    }

    // ── 3. Pre-register all subsystem slots (instances wired later by each subsystem) ──
    // These are registered with null instances; each subsystem calls registerBeacon()
    // with its real instance during its own init. This ensures the ring order is stable.
    struct SlotDef { BeaconKind kind; const char* name; };
    static const SlotDef subsystemSlots[] = {
        { BeaconKind::AgenticEngine,     "AgenticEngine"     },
        { BeaconKind::HotpatchManager,   "HotpatchManager"   },
        { BeaconKind::PlanExecutor,      "PlanExecutor"      },
        { BeaconKind::AutonomyManager,   "AutonomyManager"   },
        { BeaconKind::LLMRouter,         "LLMRouter"         },
        { BeaconKind::EncryptionEngine,  "EncryptionEngine"  },
        { BeaconKind::InferenceEngine,   "InferenceEngine"   },
        { BeaconKind::FeatureRegistry,   "FeatureRegistry"   },
        { BeaconKind::CommandDispatch,   "CommandDispatch"    },
        { BeaconKind::ToolRegistry,      "ToolRegistry"      },
        { BeaconKind::SubAgentManager,   "SubAgentManager"   },
        { BeaconKind::SwarmCoordinator,  "SwarmCoordinator"  },
        { BeaconKind::KnowledgeGraph,    "KnowledgeGraph"    },
        { BeaconKind::DebugEngine,       "DebugEngine"       },
        { BeaconKind::LSPServer,         "LSPServer"         },
        { BeaconKind::VulkanCompute,     "VulkanCompute"     },
        { BeaconKind::FlightRecorder,    "FlightRecorder"    },
        { BeaconKind::CrashContainment,  "CrashContainment"  },
        { BeaconKind::TelemetryCore,     "TelemetryCore"     },
        { BeaconKind::MCPServer,         "MCPServer"         },
        { BeaconKind::SessionManager,    "SessionManager"    },
        { BeaconKind::BackendSwitcher,   "BackendSwitcher"   },
        { BeaconKind::ProxyHotpatcher,   "ProxyHotpatcher"   },
        { BeaconKind::SelfRepairAgent,   "SelfRepairAgent"   },
        { BeaconKind::VoiceEngine,       "VoiceEngine"       },
        { BeaconKind::StaticAnalysis,    "StaticAnalysis"     },
        { BeaconKind::SemanticIntel,     "SemanticIntel"     },
        { BeaconKind::GitIntegration,    "GitIntegration"    },
    };
    for (auto& slot : subsystemSlots) {
        if (!hub.isRegistered(slot.kind)) {
            hub.registerBeacon(slot.kind, slot.name, nullptr, defaultBeaconHandler);
        }
    }

    // ── 4. Pre-register all panel slots (instances wired by each panel's init) ──
    static const SlotDef panelSlots[] = {
        { BeaconKind::PanelChat,           "PanelChat"           },
        { BeaconKind::PanelAgent,          "PanelAgent"          },
        { BeaconKind::PanelHotpatch,       "PanelHotpatch"       },
        { BeaconKind::PanelHotpatchCtrl,   "PanelHotpatchCtrl"   },
        { BeaconKind::PanelSwarm,          "PanelSwarm"          },
        { BeaconKind::PanelDualAgent,      "PanelDualAgent"      },
        { BeaconKind::PanelCrucible,       "PanelCrucible"       },
        { BeaconKind::PanelTranscendence,  "PanelTranscendence"  },
        { BeaconKind::PanelPipeline,       "PanelPipeline"       },
        { BeaconKind::PanelSemantic,       "PanelSemantic"       },
        { BeaconKind::PanelStaticAnalysis, "PanelStaticAnalysis" },
        { BeaconKind::PanelTelemetry,      "PanelTelemetry"      },
        { BeaconKind::PanelVoiceChat,      "PanelVoiceChat"      },
        { BeaconKind::PanelNetwork,        "PanelNetwork"        },
        { BeaconKind::PanelPowerShell,     "PanelPowerShell"     },
        { BeaconKind::PanelGit,            "PanelGit"            },
        { BeaconKind::PanelFailureIntel,   "PanelFailureIntel"   },
        { BeaconKind::PanelAgentHistory,   "PanelAgentHistory"   },
        { BeaconKind::PanelDebugger,       "PanelDebugger"       },
        { BeaconKind::PanelDecompiler,     "PanelDecompiler"     },
        { BeaconKind::PanelAudit,          "PanelAudit"          },
        { BeaconKind::PanelGauntlet,       "PanelGauntlet"       },
        { BeaconKind::PanelMarketplace,    "PanelMarketplace"    },
        { BeaconKind::PanelTestExplorer,   "PanelTestExplorer"   },
        { BeaconKind::PanelGameEngine,     "PanelGameEngine"     },
        { BeaconKind::PanelCopilotGap,     "PanelCopilotGap"     },
        { BeaconKind::PanelProvableAgent,  "PanelProvableAgent"  },
        { BeaconKind::PanelAIReverseEng,   "PanelAIReverseEng"   },
        { BeaconKind::PanelAirgapped,      "PanelAirgapped"      },
        { BeaconKind::PanelNativeDebug,    "PanelNativeDebug"    },
        { BeaconKind::PanelShortcutEditor, "PanelShortcutEditor" },
        { BeaconKind::PanelColorPicker,    "PanelColorPicker"    },
        { BeaconKind::PanelPlanDialog,     "PanelPlanDialog"     },
        { BeaconKind::PanelOutline,        "PanelOutline"        },
        { BeaconKind::PanelReference,      "PanelReference"      },
        { BeaconKind::PanelBreadcrumbs,    "PanelBreadcrumbs"    },
        { BeaconKind::PanelMCP,            "PanelMCP"            },
        { BeaconKind::PanelInstructions,   "PanelInstructions"   },
        { BeaconKind::PanelCursorParity,   "PanelCursorParity"   },
        { BeaconKind::PanelFlagship,       "PanelFlagship"       },
    };
    for (auto& slot : panelSlots) {
        if (!hub.isRegistered(slot.kind)) {
            hub.registerBeacon(slot.kind, slot.name, nullptr, defaultBeaconHandler);
        }
    }

    // ── 5. Pre-register language/domain beacon slots ──
    static const SlotDef langSlots[] = {
        { BeaconKind::BeaconJava,        "Java"              },
        { BeaconKind::BeaconPython,      "Python"            },
        { BeaconKind::BeaconMASM,        "MASM"              },
        { BeaconKind::BeaconRust,        "Rust"              },
        { BeaconKind::BeaconTypeScript,  "TypeScript"        },
        { BeaconKind::BeaconCSharp,      "CSharp"            },
        { BeaconKind::BeaconGo,          "Go"                },
        { BeaconKind::BeaconEncryption,  "Encryption"        },
        { BeaconKind::BeaconCompression, "Compression"       },
        { BeaconKind::BeaconReverseEng,  "ReverseEngineering"},
        { BeaconKind::BeaconSecurity,    "Security"          },
    };
    for (auto& slot : langSlots) {
        if (!hub.isRegistered(slot.kind)) {
            hub.registerBeacon(slot.kind, slot.name, nullptr, defaultBeaconHandler);
        }
    }

    // ── 6. Verify ring integrity ──
    auto stats = hub.getStats();
    char buf[512];
    snprintf(buf, sizeof(buf),
             "[BeaconBootstrap] Ring complete: %zu beacons (%zu active), "
             "%d verb routes installed",
             stats.totalBeacons, stats.activeBeacons,
             static_cast<int>(sizeof(g_verbRoutes) / sizeof(g_verbRoutes[0]) - 1));
    OutputDebugStringA(buf);

    // ── 7. Broadcast "system.ready" to all beacons ──
    hub.broadcast(BeaconKind::IDECore, "system.ready");

    OutputDebugStringA("[BeaconBootstrap] === Circular Beacon System — Bootstrap Complete ===");
}

// ============================================================================
// shutdownBeaconSystem — Called at IDE shutdown (optional cleanup)
// ============================================================================
void shutdownBeaconSystem() {
    auto& hub = BeaconHub::instance();
    
    // Broadcast shutdown notification
    hub.broadcast(BeaconKind::IDECore, "system.shutdown");
    
    // Log final stats
    auto stats = hub.getStats();
    char buf[256];
    snprintf(buf, sizeof(buf),
             "[BeaconShutdown] Total messages: sent=%llu recv=%llu broadcasts=%llu",
             stats.totalMessagesSent, stats.totalMessagesReceived, stats.totalBroadcasts);
    OutputDebugStringA(buf);
}

// ============================================================================
// Extern "C" entry points for bootstrap (MASM / WinMain can call these)
// ============================================================================
extern "C" void rawrxd_beacon_bootstrap(void* ide) {
    bootstrapBeaconSystem(ide);
}

extern "C" void rawrxd_beacon_shutdown() {
    shutdownBeaconSystem();
}

} // namespace RawrXD
