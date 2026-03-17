// ============================================================================
// Win32IDE_CircularBeaconIntegration.cpp
// ============================================================================
// Circular Beacon Interconnect — ALL 40+ panels wired to ALL subsystems.
// C++20, Win32 only, zero external deps (no OpenSSL, no Vulkan, no JNI).
//
// HOW IT WORKS:
//   1. Win32IDE::initBeaconSystem() is called during IDE startup.
//   2. Bootstrap registers 29 core subsystems, 40 panel slots, 11 language beacons,
//      and ~100 verb routes on the BeaconHub singleton.
//   3. Each panel gets a BeaconHandler lambda that routes incoming verbs to the
//      correct subsystem via BeaconHub::send().
//   4. Every panel can reach every subsystem — no vertical silos.
//
// RULE: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "Win32IDE.h"
#include "../../include/circular_beacon_system.h"
#include "CircularBeaconManager.h"

using namespace RawrXD;

// ============================================================================
// Generic panel handler — routes verbs to the correct subsystem
// ============================================================================
// Each panel handler follows the same pattern:
//   - Accept a BeaconMessage
//   - Examine the verb prefix
//   - Forward to the target subsystem via hub.send()
//   - Log via OutputDebugStringA (Win32 native, no deps)
//   - Return a BeaconResponse
// ============================================================================

namespace {

// ── Utility: log a beacon message (truncated payload for safety) ──
inline void logBeacon(const char* panel, const BeaconMessage& msg) {
    char buf[512];
    const char* verb = msg.verb ? msg.verb : "(null)";
    wsprintfA(buf, "[Beacon:%s] verb=%s src=0x%04X\n",
              panel, verb, static_cast<uint32_t>(msg.sourceKind));
    OutputDebugStringA(buf);
}

// ── Verb prefix dispatcher ──
// Routes a verb to the proper subsystem based on its prefix.
// Returns true if handled, false if unrecognized.
inline BeaconResponse dispatchByPrefix(BeaconKind selfKind, const BeaconMessage& msg) {
    auto& hub = BeaconHub::instance();
    const char* v = msg.verb ? msg.verb : "";

    // agent.*  → AgenticEngine
    if (strncmp(v, "agent.", 6) == 0) {
        return hub.send(selfKind, BeaconKind::AgenticEngine, v, msg.payload, msg.payloadLen);
    }
    // hotpatch.* → HotpatchManager
    if (strncmp(v, "hotpatch.", 9) == 0) {
        return hub.send(selfKind, BeaconKind::HotpatchManager, v, msg.payload, msg.payloadLen);
    }
    // plan.* → PlanExecutor
    if (strncmp(v, "plan.", 5) == 0) {
        return hub.send(selfKind, BeaconKind::PlanExecutor, v, msg.payload, msg.payloadLen);
    }
    // autonomy.* → AutonomyManager
    if (strncmp(v, "autonomy.", 9) == 0) {
        return hub.send(selfKind, BeaconKind::AutonomyManager, v, msg.payload, msg.payloadLen);
    }
    // llm.* → LLMRouter
    if (strncmp(v, "llm.", 4) == 0) {
        return hub.send(selfKind, BeaconKind::LLMRouter, v, msg.payload, msg.payloadLen);
    }
    // encrypt.* / decrypt.* → EncryptionEngine
    if (strncmp(v, "encrypt.", 8) == 0 || strncmp(v, "decrypt.", 8) == 0) {
        return hub.send(selfKind, BeaconKind::EncryptionEngine, v, msg.payload, msg.payloadLen);
    }
    // gpu.* → VulkanCompute
    if (strncmp(v, "gpu.", 4) == 0) {
        return hub.send(selfKind, BeaconKind::VulkanCompute, v, msg.payload, msg.payloadLen);
    }
    // debug.* → DebugEngine
    if (strncmp(v, "debug.", 6) == 0) {
        return hub.send(selfKind, BeaconKind::DebugEngine, v, msg.payload, msg.payloadLen);
    }
    // lsp.* → LSPServer
    if (strncmp(v, "lsp.", 4) == 0) {
        return hub.send(selfKind, BeaconKind::LSPServer, v, msg.payload, msg.payloadLen);
    }
    // swarm.* → SwarmCoordinator
    if (strncmp(v, "swarm.", 6) == 0) {
        return hub.send(selfKind, BeaconKind::SwarmCoordinator, v, msg.payload, msg.payloadLen);
    }
    // voice.* → VoiceEngine
    if (strncmp(v, "voice.", 6) == 0) {
        return hub.send(selfKind, BeaconKind::VoiceEngine, v, msg.payload, msg.payloadLen);
    }
    // backend.* → BackendSwitcher
    if (strncmp(v, "backend.", 8) == 0) {
        return hub.send(selfKind, BeaconKind::BackendSwitcher, v, msg.payload, msg.payloadLen);
    }
    // mcp.* → MCPServer
    if (strncmp(v, "mcp.", 4) == 0) {
        return hub.send(selfKind, BeaconKind::MCPServer, v, msg.payload, msg.payloadLen);
    }
    // session.* → SessionManager
    if (strncmp(v, "session.", 8) == 0) {
        return hub.send(selfKind, BeaconKind::SessionManager, v, msg.payload, msg.payloadLen);
    }
    // telemetry.* → TelemetryCore
    if (strncmp(v, "telemetry.", 10) == 0) {
        return hub.send(selfKind, BeaconKind::TelemetryCore, v, msg.payload, msg.payloadLen);
    }
    // flight.* → FlightRecorder
    if (strncmp(v, "flight.", 7) == 0) {
        return hub.send(selfKind, BeaconKind::FlightRecorder, v, msg.payload, msg.payloadLen);
    }
    // crash.* → CrashContainment
    if (strncmp(v, "crash.", 6) == 0) {
        return hub.send(selfKind, BeaconKind::CrashContainment, v, msg.payload, msg.payloadLen);
    }
    // repair.* → SelfRepairAgent
    if (strncmp(v, "repair.", 7) == 0) {
        return hub.send(selfKind, BeaconKind::SelfRepairAgent, v, msg.payload, msg.payloadLen);
    }
    // tool.* → ToolRegistry
    if (strncmp(v, "tool.", 5) == 0) {
        return hub.send(selfKind, BeaconKind::ToolRegistry, v, msg.payload, msg.payloadLen);
    }
    // knowledge.* → KnowledgeGraph
    if (strncmp(v, "knowledge.", 10) == 0) {
        return hub.send(selfKind, BeaconKind::KnowledgeGraph, v, msg.payload, msg.payloadLen);
    }
    // inference.* → InferenceEngine
    if (strncmp(v, "inference.", 10) == 0) {
        return hub.send(selfKind, BeaconKind::InferenceEngine, v, msg.payload, msg.payloadLen);
    }
    // feature.* → FeatureRegistry
    if (strncmp(v, "feature.", 8) == 0) {
        return hub.send(selfKind, BeaconKind::FeatureRegistry, v, msg.payload, msg.payloadLen);
    }
    // dispatch.* → CommandDispatch
    if (strncmp(v, "dispatch.", 9) == 0) {
        return hub.send(selfKind, BeaconKind::CommandDispatch, v, msg.payload, msg.payloadLen);
    }
    // git.* → GitIntegration
    if (strncmp(v, "git.", 4) == 0) {
        return hub.send(selfKind, BeaconKind::GitIntegration, v, msg.payload, msg.payloadLen);
    }
    // static.* → StaticAnalysis
    if (strncmp(v, "static.", 7) == 0) {
        return hub.send(selfKind, BeaconKind::StaticAnalysis, v, msg.payload, msg.payloadLen);
    }
    // semantic.* → SemanticIntel
    if (strncmp(v, "semantic.", 9) == 0) {
        return hub.send(selfKind, BeaconKind::SemanticIntel, v, msg.payload, msg.payloadLen);
    }
    // subagent.* → SubAgentManager
    if (strncmp(v, "subagent.", 9) == 0) {
        return hub.send(selfKind, BeaconKind::SubAgentManager, v, msg.payload, msg.payloadLen);
    }
    // proxy.* → ProxyHotpatcher
    if (strncmp(v, "proxy.", 6) == 0) {
        return hub.send(selfKind, BeaconKind::ProxyHotpatcher, v, msg.payload, msg.payloadLen);
    }

    // beacon.* → IDECore (system-level)
    if (strncmp(v, "beacon.", 7) == 0) {
        return hub.send(selfKind, BeaconKind::IDECore, v, msg.payload, msg.payloadLen);
    }

    // Language beacons
    if (strncmp(v, "java.", 5) == 0) {
        return hub.send(selfKind, BeaconKind::BeaconJava, v, msg.payload, msg.payloadLen);
    }
    if (strncmp(v, "python.", 7) == 0) {
        return hub.send(selfKind, BeaconKind::BeaconPython, v, msg.payload, msg.payloadLen);
    }
    if (strncmp(v, "masm.", 5) == 0) {
        return hub.send(selfKind, BeaconKind::BeaconMASM, v, msg.payload, msg.payloadLen);
    }
    if (strncmp(v, "rust.", 5) == 0) {
        return hub.send(selfKind, BeaconKind::BeaconRust, v, msg.payload, msg.payloadLen);
    }
    if (strncmp(v, "typescript.", 11) == 0) {
        return hub.send(selfKind, BeaconKind::BeaconTypeScript, v, msg.payload, msg.payloadLen);
    }
    if (strncmp(v, "csharp.", 7) == 0) {
        return hub.send(selfKind, BeaconKind::BeaconCSharp, v, msg.payload, msg.payloadLen);
    }
    if (strncmp(v, "go.", 3) == 0) {
        return hub.send(selfKind, BeaconKind::BeaconGo, v, msg.payload, msg.payloadLen);
    }

    // Unknown verb — not handled
    return { false, -1, "unrecognized_verb", selfKind };
}

// ── Factory: create a handler for any panel ──
inline BeaconHandler makePanelHandler(BeaconKind panelKind, const char* panelName) {
    return [panelKind, panelName](const BeaconMessage& msg) -> BeaconResponse {
        logBeacon(panelName, msg);
        return dispatchByPrefix(panelKind, msg);
    };
}

} // anonymous namespace

// ============================================================================
// Win32IDE::initBeaconSystem()
// ============================================================================
// Called once from the IDE init sequence. Registers ALL panels and subsystems.
// ============================================================================
void Win32IDE::initBeaconSystem() {
    auto& hub = BeaconHub::instance();

    OutputDebugStringA("[BeaconInit] === Initializing Circular Beacon System ===\n");

    // ─── 1. Register core subsystems (stub handlers — real implementations
    //        override these when they come online) ───
    auto stubHandler = [](const BeaconMessage& msg) -> BeaconResponse {
        char buf[256];
        wsprintfA(buf, "[BeaconStub] verb=%s target=0x%04X\n",
                  msg.verb ? msg.verb : "(null)",
                  static_cast<uint32_t>(msg.targetKind));
        OutputDebugStringA(buf);
        return { true, 0, "stub_ok", msg.targetKind };
    };

    // Register all 29 core subsystem slots
    struct SubsystemSlot { BeaconKind kind; const char* name; void* ptr; };
    SubsystemSlot subsystems[] = {
        { BeaconKind::AgenticEngine,     "AgenticEngine",     nullptr },
        { BeaconKind::HotpatchManager,   "HotpatchManager",   nullptr },
        { BeaconKind::PlanExecutor,      "PlanExecutor",      nullptr },
        { BeaconKind::AutonomyManager,   "AutonomyManager",   nullptr },
        { BeaconKind::LLMRouter,         "LLMRouter",         nullptr },
        { BeaconKind::EncryptionEngine,  "EncryptionEngine",  nullptr },
        { BeaconKind::IDECore,           "IDECore",           this    },
        { BeaconKind::InferenceEngine,   "InferenceEngine",   nullptr },
        { BeaconKind::FeatureRegistry,   "FeatureRegistry",   nullptr },
        { BeaconKind::CommandDispatch,   "CommandDispatch",   nullptr },
        { BeaconKind::ToolRegistry,      "ToolRegistry",      nullptr },
        { BeaconKind::SubAgentManager,   "SubAgentManager",   nullptr },
        { BeaconKind::SwarmCoordinator,  "SwarmCoordinator",  nullptr },
        { BeaconKind::KnowledgeGraph,    "KnowledgeGraph",    nullptr },
        { BeaconKind::DebugEngine,       "DebugEngine",       nullptr },
        { BeaconKind::LSPServer,         "LSPServer",         nullptr },
        { BeaconKind::VulkanCompute,     "VulkanCompute",     nullptr },
        { BeaconKind::FlightRecorder,    "FlightRecorder",    nullptr },
        { BeaconKind::CrashContainment,  "CrashContainment",  nullptr },
        { BeaconKind::TelemetryCore,     "TelemetryCore",     nullptr },
        { BeaconKind::MCPServer,         "MCPServer",         nullptr },
        { BeaconKind::SessionManager,    "SessionManager",    nullptr },
        { BeaconKind::BackendSwitcher,   "BackendSwitcher",   nullptr },
        { BeaconKind::ProxyHotpatcher,   "ProxyHotpatcher",   nullptr },
        { BeaconKind::SelfRepairAgent,   "SelfRepairAgent",   nullptr },
        { BeaconKind::VoiceEngine,       "VoiceEngine",       nullptr },
        { BeaconKind::StaticAnalysis,    "StaticAnalysis",    nullptr },
        { BeaconKind::SemanticIntel,     "SemanticIntel",     nullptr },
        { BeaconKind::GitIntegration,    "GitIntegration",    nullptr },
    };
    for (auto& s : subsystems) {
        hub.registerBeacon(s.kind, s.name, s.ptr, stubHandler);
    }

    // ─── 2. Register ALL 40 GUI panel beacons ───
    struct PanelSlot { BeaconKind kind; const char* name; };
    PanelSlot panels[] = {
        { BeaconKind::PanelChat,           "Chat"            },
        { BeaconKind::PanelAgent,          "Agent"           },
        { BeaconKind::PanelHotpatch,       "Hotpatch"        },
        { BeaconKind::PanelHotpatchCtrl,   "HotpatchCtrl"    },
        { BeaconKind::PanelSwarm,          "Swarm"           },
        { BeaconKind::PanelDualAgent,      "DualAgent"       },
        { BeaconKind::PanelCrucible,       "Crucible"        },
        { BeaconKind::PanelTranscendence,  "Transcendence"   },
        { BeaconKind::PanelPipeline,       "Pipeline"        },
        { BeaconKind::PanelSemantic,       "Semantic"        },
        { BeaconKind::PanelStaticAnalysis, "StaticAnalysis"  },
        { BeaconKind::PanelTelemetry,      "Telemetry"       },
        { BeaconKind::PanelVoiceChat,      "VoiceChat"       },
        { BeaconKind::PanelNetwork,        "Network"         },
        { BeaconKind::PanelPowerShell,     "PowerShell"      },
        { BeaconKind::PanelGit,            "Git"             },
        { BeaconKind::PanelFailureIntel,   "FailureIntel"    },
        { BeaconKind::PanelAgentHistory,   "AgentHistory"    },
        { BeaconKind::PanelDebugger,       "Debugger"        },
        { BeaconKind::PanelDecompiler,     "Decompiler"      },
        { BeaconKind::PanelAudit,          "Audit"           },
        { BeaconKind::PanelGauntlet,       "Gauntlet"        },
        { BeaconKind::PanelMarketplace,    "Marketplace"     },
        { BeaconKind::PanelTestExplorer,   "TestExplorer"    },
        { BeaconKind::PanelGameEngine,     "GameEngine"      },
        { BeaconKind::PanelCopilotGap,     "CopilotGap"      },
        { BeaconKind::PanelProvableAgent,  "ProvableAgent"   },
        { BeaconKind::PanelAIReverseEng,   "AIReverseEng"    },
        { BeaconKind::PanelAirgapped,      "Airgapped"       },
        { BeaconKind::PanelNativeDebug,    "NativeDebug"     },
        { BeaconKind::PanelShortcutEditor, "ShortcutEditor"  },
        { BeaconKind::PanelColorPicker,    "ColorPicker"     },
        { BeaconKind::PanelPlanDialog,     "PlanDialog"      },
        { BeaconKind::PanelOutline,        "Outline"         },
        { BeaconKind::PanelReference,      "Reference"       },
        { BeaconKind::PanelBreadcrumbs,    "Breadcrumbs"     },
        { BeaconKind::PanelMCP,            "MCP"             },
        { BeaconKind::PanelInstructions,   "Instructions"    },
        { BeaconKind::PanelCursorParity,   "CursorParity"    },
        { BeaconKind::PanelFlagship,       "Flagship"        },
    };
    for (auto& p : panels) {
        hub.registerBeacon(p.kind, p.name, this, makePanelHandler(p.kind, p.name));
    }

    // ─── 3. Register 11 language / domain beacons ───
    struct LangSlot { BeaconKind kind; const char* name; };
    LangSlot langs[] = {
        { BeaconKind::BeaconJava,       "Java"          },
        { BeaconKind::BeaconPython,     "Python"        },
        { BeaconKind::BeaconMASM,       "MASM"          },
        { BeaconKind::BeaconRust,       "Rust"          },
        { BeaconKind::BeaconTypeScript, "TypeScript"    },
        { BeaconKind::BeaconCSharp,     "CSharp"        },
        { BeaconKind::BeaconGo,         "Go"            },
        { BeaconKind::BeaconEncryption, "Encryption"    },
        { BeaconKind::BeaconCompression,"Compression"   },
        { BeaconKind::BeaconReverseEng, "ReverseEng"    },
        { BeaconKind::BeaconSecurity,   "Security"      },
    };
    for (auto& l : langs) {
        hub.registerBeacon(l.kind, l.name, nullptr, stubHandler);
    }

    // ─── 4. Install verb routing table ───
    // Maps verb prefixes to their handler beacons so that
    // hub.resolveVerb("agent.execute") → AgenticEngine
    struct VerbRoute { const char* verb; BeaconKind target; };
    VerbRoute routes[] = {
        // Agent verbs
        { "agent.execute",         BeaconKind::AgenticEngine    },
        { "agent.plan",            BeaconKind::AgenticEngine    },
        { "agent.reason",          BeaconKind::AgenticEngine    },
        { "agent.loop",            BeaconKind::AgenticEngine    },
        { "agent.merge",           BeaconKind::AgenticEngine    },
        { "agent.rollback",        BeaconKind::AgenticEngine    },
        { "agent.checkpoint",      BeaconKind::AgenticEngine    },
        { "agent.request",         BeaconKind::AgenticEngine    },
        { "agent.autonomous_scan", BeaconKind::AgenticEngine    },

        // Hotpatch verbs
        { "hotpatch.apply",        BeaconKind::HotpatchManager  },
        { "hotpatch.revert",       BeaconKind::HotpatchManager  },
        { "hotpatch.list",         BeaconKind::HotpatchManager  },
        { "hotpatch.reload",       BeaconKind::HotpatchManager  },
        { "hotpatch.swap_kernel",  BeaconKind::HotpatchManager  },
        { "hotpatch.probe",        BeaconKind::HotpatchManager  },
        { "hotpatch.byte_patch",   BeaconKind::ProxyHotpatcher  },

        // Plan executor
        { "plan.submit",           BeaconKind::PlanExecutor     },
        { "plan.status",           BeaconKind::PlanExecutor     },
        { "plan.cancel",           BeaconKind::PlanExecutor     },
        { "plan.approve",          BeaconKind::PlanExecutor     },

        // Autonomy
        { "autonomy.start",        BeaconKind::AutonomyManager  },
        { "autonomy.pause",        BeaconKind::AutonomyManager  },
        { "autonomy.status",       BeaconKind::AutonomyManager  },

        // LLM router
        { "llm.route",            BeaconKind::LLMRouter        },
        { "llm.infer",            BeaconKind::LLMRouter        },
        { "llm.complete",         BeaconKind::LLMRouter        },
        { "llm.chat",             BeaconKind::LLMRouter        },
        { "llm.switch_backend",   BeaconKind::BackendSwitcher  },
        { "llm.classify",         BeaconKind::LLMRouter        },

        // Encryption
        { "encrypt.aes256",        BeaconKind::EncryptionEngine },
        { "encrypt.camellia",      BeaconKind::EncryptionEngine },
        { "encrypt.stream",        BeaconKind::EncryptionEngine },
        { "decrypt.aes256",        BeaconKind::EncryptionEngine },
        { "decrypt.camellia",      BeaconKind::EncryptionEngine },

        // GPU
        { "gpu.tune",             BeaconKind::VulkanCompute    },
        { "gpu.benchmark",        BeaconKind::VulkanCompute    },
        { "gpu.allocate",         BeaconKind::VulkanCompute    },
        { "gpu.offload",          BeaconKind::VulkanCompute    },

        // Debug
        { "debug.breakpoint",     BeaconKind::DebugEngine      },
        { "debug.step",           BeaconKind::DebugEngine      },
        { "debug.continue",       BeaconKind::DebugEngine      },
        { "debug.inspect",        BeaconKind::DebugEngine      },
        { "debug.attach",         BeaconKind::DebugEngine      },

        // LSP
        { "lsp.completion",       BeaconKind::LSPServer        },
        { "lsp.hover",            BeaconKind::LSPServer        },
        { "lsp.definition",       BeaconKind::LSPServer        },
        { "lsp.diagnostic",       BeaconKind::LSPServer        },
        { "lsp.format",           BeaconKind::LSPServer        },

        // Swarm
        { "swarm.spawn",          BeaconKind::SwarmCoordinator },
        { "swarm.distribute",     BeaconKind::SwarmCoordinator },
        { "swarm.status",         BeaconKind::SwarmCoordinator },

        // Voice
        { "voice.recognize",      BeaconKind::VoiceEngine      },
        { "voice.speak",          BeaconKind::VoiceEngine      },

        // MCP
        { "mcp.list_tools",       BeaconKind::MCPServer        },
        { "mcp.call_tool",        BeaconKind::MCPServer        },
        { "mcp.list_prompts",     BeaconKind::MCPServer        },

        // Session
        { "session.save",         BeaconKind::SessionManager   },
        { "session.restore",      BeaconKind::SessionManager   },
        { "session.list",         BeaconKind::SessionManager   },

        // Telemetry
        { "telemetry.record",     BeaconKind::TelemetryCore    },
        { "telemetry.export",     BeaconKind::TelemetryCore    },

        // Flight recorder
        { "flight.snapshot",      BeaconKind::FlightRecorder   },
        { "flight.replay",        BeaconKind::FlightRecorder   },

        // Crash containment
        { "crash.report",         BeaconKind::CrashContainment },
        { "crash.recover",        BeaconKind::CrashContainment },

        // Self repair
        { "repair.diagnose",      BeaconKind::SelfRepairAgent  },
        { "repair.apply",         BeaconKind::SelfRepairAgent  },

        // Tools
        { "tool.register",        BeaconKind::ToolRegistry     },
        { "tool.invoke",          BeaconKind::ToolRegistry     },
        { "tool.list",            BeaconKind::ToolRegistry     },

        // Knowledge
        { "knowledge.query",      BeaconKind::KnowledgeGraph   },
        { "knowledge.index",      BeaconKind::KnowledgeGraph   },

        // Inference
        { "inference.generate",   BeaconKind::InferenceEngine  },
        { "inference.tokenize",   BeaconKind::InferenceEngine  },
        { "inference.embed",      BeaconKind::InferenceEngine  },

        // Features
        { "feature.check",        BeaconKind::FeatureRegistry  },
        { "feature.toggle",       BeaconKind::FeatureRegistry  },
        { "feature.audit",        BeaconKind::FeatureRegistry  },

        // Command dispatch
        { "dispatch.execute",     BeaconKind::CommandDispatch  },
        { "dispatch.register",    BeaconKind::CommandDispatch  },

        // Git
        { "git.status",           BeaconKind::GitIntegration   },
        { "git.commit",           BeaconKind::GitIntegration   },
        { "git.diff",             BeaconKind::GitIntegration   },

        // Static analysis
        { "static.analyze",       BeaconKind::StaticAnalysis   },
        { "static.lint",          BeaconKind::StaticAnalysis   },

        // Semantic intel
        { "semantic.search",      BeaconKind::SemanticIntel    },
        { "semantic.index",       BeaconKind::SemanticIntel    },

        // Sub-agent
        { "subagent.spawn",       BeaconKind::SubAgentManager  },
        { "subagent.status",      BeaconKind::SubAgentManager  },

        // Proxy hotpatcher
        { "proxy.detour",         BeaconKind::ProxyHotpatcher  },
        { "proxy.restore",        BeaconKind::ProxyHotpatcher  },

        // Backend switcher
        { "backend.switch",       BeaconKind::BackendSwitcher  },
        { "backend.list",         BeaconKind::BackendSwitcher  },
        { "backend.status",       BeaconKind::BackendSwitcher  },

        // System-level
        { "beacon.refresh",       BeaconKind::IDECore          },
        { "beacon.update",        BeaconKind::IDECore          },
        { "beacon.shutdown",      BeaconKind::IDECore          },
        { "beacon.circular_broadcast", BeaconKind::IDECore     },
        { "system.shutdown",      BeaconKind::IDECore          },
    };
    for (auto& r : routes) {
        hub.registerVerb(r.verb, r.target);
    }

    // ─── 5. Final stats ───
    auto stats = hub.getStats();
    char finalBuf[256];
    wsprintfA(finalBuf,
              "[BeaconInit] Complete: %zu beacons, %zu active\n",
              stats.totalBeacons, stats.activeBeacons);
    OutputDebugStringA(finalBuf);
    OutputDebugStringA("[BeaconInit] === Circular Beacon System ONLINE ===\n");
}

// ============================================================================
// Win32IDE::shutdownBeaconSystem()
// ============================================================================
void Win32IDE::shutdownBeaconSystem() {
    auto& hub = BeaconHub::instance();

    // Broadcast shutdown signal to all beacons
    hub.broadcast(BeaconKind::IDECore, "system.shutdown");

    // Log final stats before teardown
    auto stats = hub.getStats();
    char buf[256];
    wsprintfA(buf, "[BeaconShutdown] Final: %llu sent, %llu recv, %llu broadcasts\n",
              stats.totalMessagesSent, stats.totalMessagesReceived, stats.totalBroadcasts);
    OutputDebugStringA(buf);

    // Unregister all panel beacons (panels range 0x1001..0x1028)
    auto active = hub.getActiveBeacons();
    for (auto k : active) {
        uint32_t raw = static_cast<uint32_t>(k);
        if (raw >= 0x1001 && raw <= 0x1028) {
            hub.unregisterBeacon(k);
        }
    }

    // Unregister language beacons (0x2001..0x200B)
    for (auto k : active) {
        uint32_t raw = static_cast<uint32_t>(k);
        if (raw >= 0x2001 && raw <= 0x200B) {
            hub.unregisterBeacon(k);
        }
    }

    // Unregister core subsystems
    for (auto k : active) {
        uint32_t raw = static_cast<uint32_t>(k);
        if (raw >= 0x0001 && raw <= 0x001D) {
            hub.unregisterBeacon(k);
        }
    }

    // Shutdown the manager if we have one
    if (m_circularBeaconManager) {
        m_circularBeaconManager->shutdown();
    }

    OutputDebugStringA("[BeaconShutdown] === Circular Beacon System OFFLINE ===\n");
}

// ============================================================================
// Win32IDE::beaconSend() — Send from any panel to a specific target
// ============================================================================
bool Win32IDE::beaconSend(uint32_t targetKind, const char* verb,
                           const char* payload, size_t payloadLen) {
    auto& hub = BeaconHub::instance();
    auto resp = hub.send(BeaconKind::IDECore,
                         static_cast<BeaconKind>(targetKind),
                         verb, payload, payloadLen);
    return resp.handled;
}

// ============================================================================
// Win32IDE::beaconBroadcast() — Broadcast from IDECore to all beacons
// ============================================================================
int Win32IDE::beaconBroadcast(const char* verb, const char* payload, size_t payloadLen) {
    auto& hub = BeaconHub::instance();
    auto responses = hub.broadcast(BeaconKind::IDECore, verb, payload, payloadLen);
    int handled = 0;
    for (auto& r : responses) {
        if (r.handled) handled++;
    }
    return handled;
}
