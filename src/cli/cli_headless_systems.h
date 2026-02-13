// ============================================================================
// cli_headless_systems.h — Phase 20: Headless CLI Surface Completion
// ============================================================================
//
// Wires every Phase 10 singleton + Phase 7/8/9 engine into the CLI shell.
// These are the systems that were already built and integrated into the Win32IDE
// but had zero surface in the headless REPL — leaving the autonomy pipeline
// running without its safety net, confidence gate, replay journal, execution
// governor, multi-response engine, history/explainability, policy engine,
// streaming engine registry, and tool registry.
//
// This file declares the CLI command handlers. The implementations live in
// cli_headless_systems.cpp.
//
// Singleton Dependencies (all platform-independent, already compiled):
//   - AgentSafetyContract    (Phase 10B)  agent_safety_contract.h
//   - ConfidenceGate         (Phase 10D)  confidence_gate.h
//   - ReplayJournal          (Phase 10C)  deterministic_replay.h
//   - ExecutionGovernor      (Phase 10A)  execution_governor.h
//   - MultiResponseEngine    (Phase 9C)   multi_response_engine.h
//   - AgentHistoryRecorder   (Phase 5)    agent_history.h
//   - ExplainabilityEngine   (Phase 8A)   agent_explainability.h
//   - PolicyEngine           (Phase 7)    agent_policy.h
//   - ToolRegistry                        tool_registry.h
//
// Pattern:  Structured results (PatchResult-style), no exceptions
// Threading: All singletons are internally thread-safe
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#ifndef RAWRXD_CLI_HEADLESS_SYSTEMS_H
#define RAWRXD_CLI_HEADLESS_SYSTEMS_H

#include <string>

// Forward declarations — the implementations include the real headers
class AgenticEngine;
class SubAgentManager;
class AgentHistoryRecorder;
class PolicyEngine;
class ExplainabilityEngine;
class MultiResponseEngine;

// ============================================================================
// INITIALIZATION — call from main() after engine setup
// ============================================================================

// Initialize all headless subsystems. Pass engine pointers for systems
// that need model inference (multi-response, governor command execution).
// Returns true if all subsystems initialized successfully.
bool cli_headless_init(AgenticEngine* engine, SubAgentManager* subAgentMgr);

// Shutdown all headless subsystems cleanly.
void cli_headless_shutdown();

// Get the shared AgentHistoryRecorder (for use by other CLI modules)
AgentHistoryRecorder* cli_get_history_recorder();

// Get the shared PolicyEngine (for use by other CLI modules)
PolicyEngine* cli_get_policy_engine();

// Get the shared ExplainabilityEngine (for use by other CLI modules)
ExplainabilityEngine* cli_get_explainability_engine();

// Get the shared MultiResponseEngine (for use by other CLI modules)
MultiResponseEngine* cli_get_multi_response_engine();

// ============================================================================
// SAFETY CONTRACT COMMANDS (Phase 10B)
// ============================================================================

// !safety status         — Show budget, risk tiers, violations, stats
// !safety reset          — Reset intent budget to defaults
// !safety rollback       — Rollback last action
// !safety rollback_all   — Rollback all recorded actions
// !safety violations     — Show violation log
// !safety block <class>  — Block an action class
// !safety unblock <class>— Unblock an action class
// !safety risk <tier>    — Set maximum allowed risk tier
// !safety budget         — Show remaining intent budget
void cmd_safety(const std::string& args);

// ============================================================================
// CONFIDENCE GATE COMMANDS (Phase 10D)
// ============================================================================

// !confidence status     — Show gate stats, thresholds, trend
// !confidence policy <p> — Set gate policy (strict|normal|relaxed|disabled)
// !confidence threshold <execute> <escalate> <abort> — Set thresholds
// !confidence history    — Show recent confidence evaluations
// !confidence trend      — Show confidence trend analysis
// !confidence selfabort  — Show self-abort status
// !confidence reset      — Reset gate state
void cmd_confidence(const std::string& args);

// ============================================================================
// REPLAY JOURNAL COMMANDS (Phase 10C)
// ============================================================================

// !replay status         — Show journal stats
// !replay last [n]       — Show last N recorded actions (default 10)
// !replay session        — Show current session snapshot
// !replay sessions       — List all sessions
// !replay checkpoint [l] — Insert checkpoint marker with optional label
// !replay export <file>  — Export current session to file
// !replay filter <type>  — Show actions of a specific type
// !replay start          — Start recording
// !replay pause          — Pause recording
// !replay stop           — Stop recording
void cmd_replay(const std::string& args);

// ============================================================================
// EXECUTION GOVERNOR COMMANDS (Phase 10A)
// ============================================================================

// !governor status       — Show governor stats, active tasks
// !governor run <cmd>    — Execute a command through the governor with timeout
// !governor run <cmd> --timeout <ms> — Custom timeout
// !governor tasks        — List all active governed tasks
// !governor kill <id>    — Kill a specific task
// !governor kill_all     — Kill all running tasks
// !governor wait <id>    — Block until task completes
void cmd_governor(const std::string& args);

// ============================================================================
// MULTI-RESPONSE ENGINE COMMANDS (Phase 9C)
// ============================================================================

// !multi <prompt>        — Generate responses from all enabled templates
// !multi compare         — Show side-by-side comparison of last responses
// !multi prefer <index>  — Record preference for response index (0-3)
// !multi templates       — Show all templates with status
// !multi toggle <id>     — Toggle a template on/off
// !multi recommend       — Show recommended template based on history
// !multi stats           — Show generation statistics
void cmd_multi_response(const std::string& args);

// ============================================================================
// AGENT HISTORY COMMANDS (Phase 5)
// ============================================================================

// !history show [n]      — Show last N events (default 20)
// !history session       — Show current session timeline
// !history agent <id>    — Show timeline for specific agent
// !history type <type>   — Show events of a specific type
// !history stats         — Show history statistics
// !history flush         — Flush events to disk
// !history clear         — Clear in-memory events
// !history export <file> — Export events to JSON file
void cmd_history(const std::string& args);

// ============================================================================
// EXPLAINABILITY COMMANDS (Phase 8A)
// ============================================================================

// !explain agent <id>    — Trace causal chain for an agent
// !explain chain <id>    — Trace causal chain for a chain execution
// !explain swarm <id>    — Trace causal chain for a swarm execution
// !explain failures      — Explain all failures in current session
// !explain policies      — Explain all policy firings in current session
// !explain session       — Full session-level explanation
// !explain snapshot <f>  — Export full explainability snapshot to file
void cmd_explain(const std::string& args);

// ============================================================================
// POLICY ENGINE COMMANDS (Phase 7)
// ============================================================================

// !policy list           — List all policies
// !policy show <id>      — Show policy details
// !policy enable <id>    — Enable a policy
// !policy disable <id>   — Disable a policy
// !policy remove <id>    — Remove a policy
// !policy heuristics     — Show computed heuristics
// !policy suggest        — Generate policy suggestions
// !policy accept <id>    — Accept a suggestion
// !policy reject <id>    — Reject a suggestion
// !policy pending        — Show pending suggestions
// !policy export <file>  — Export policies to JSON file
// !policy import <file>  — Import policies from JSON file
// !policy stats          — Show policy engine statistics
void cmd_policy(const std::string& args);

// ============================================================================
// TOOL REGISTRY COMMANDS
// ============================================================================

// !tools                 — List all registered tools
void cmd_tools(const std::string& args);

// ============================================================================
// MODEL HOTPATCHER COMMANDS (Phase 20 AVX-512 requantization pipeline)
// ============================================================================

// !model_load <path>     — Analyze a GGUF model file (sets up hotpatcher)
// !model_plan            — Compute optimal quantization plan for loaded model
// !model_surgery         — Apply the computed quant plan (streaming requantization)
// !pressure_auto         — Enable automatic VRAM pressure response
// !model_status          — Show model hotpatcher status and VRAM budget
// !model_layers          — List all loaded model layers with quant types
// !model_evict <idx>     — Evict a specific layer from VRAM
// !model_reload <idx>    — Reload an evicted layer
void cmd_model(const std::string& args);

// ============================================================================
// SWARM ORCHESTRATOR COMMANDS (Phase 21 distributed inference)
// ============================================================================

// !swarm_join [ip]       — Join an existing swarm or become coordinator
// !swarm_status          — Show swarm topology, nodes, shards
// !swarm_distribute <model> <layers> — Distribute model layers across swarm
// !swarm_rebalance       — Rebalance shards based on VRAM pressure
// !swarm_nodes           — List all swarm nodes
// !swarm_shards          — List all layer shards
// !swarm_leave           — Leave the current swarm
// !swarm_stats           — Show swarm networking statistics
void cmd_swarm_orchestrator(const std::string& args);

// ============================================================================
// CONVENIENCE: Check an action through Safety + Confidence before executing
// ============================================================================

// Unified safety+confidence gate check. Used by other CLI commands before
// performing mutating operations. Returns true if the action is allowed.
// Records the check in the replay journal.
bool cli_gate_check(int actionClass, int riskTier,
                    float confidence, const std::string& description);

// Record an action in the replay journal (convenience wrapper).
void cli_record_action(int actionType, const std::string& category,
                       const std::string& action, const std::string& input,
                       const std::string& output, int exitCode = 0,
                       float confidence = 1.0f, double durationMs = 0.0);

#endif // RAWRXD_CLI_HEADLESS_SYSTEMS_H
