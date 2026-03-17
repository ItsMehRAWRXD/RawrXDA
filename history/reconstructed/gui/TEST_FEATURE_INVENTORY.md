# RawrXD IDE // AGENTIC INTERFACE — Complete Testable Feature Inventory

**Source:** `ide_chatbot.html` (24,590 lines)  
**Generated:** 2026-02-11  
**Purpose:** Exhaustive catalog of every testable feature, with implementation details.

---

## TABLE OF CONTENTS

1. [Application Bootstrap & Initialization](#1-application-bootstrap--initialization)
2. [Backend Connection & Health](#2-backend-connection--health)
3. [Chat / Message System](#3-chat--message-system)
4. [Model Selection & Management](#4-model-selection--management)
5. [MASM Model Bridge](#5-masm-model-bridge)
6. [Streaming Engine (RawrXDStream)](#6-streaming-engine-rawrxdstream)
7. [Terminal Emulation (Dual-Mode)](#7-terminal-emulation-dual-mode)
8. [File Attachment & Drag-Drop](#8-file-attachment--drag-drop)
9. [Code Block Rendering & Syntax Highlighting](#9-code-block-rendering--syntax-highlighting)
10. [Debug Panel](#10-debug-panel)
11. [Endpoints Panel](#11-endpoints-panel)
12. [Failure Timeline & Agentic Recovery](#12-failure-timeline--agentic-recovery)
13. [Agent Dashboard & Hotpatching](#13-agent-dashboard--hotpatching)
14. [Performance Panel](#14-performance-panel)
15. [Security Panel](#15-security-panel)
16. [Backend Switcher](#16-backend-switcher)
17. [LLM Router Panel](#17-llm-router-panel)
18. [Swarm Dashboard](#18-swarm-dashboard)
19. [Multi-Response Panel](#19-multi-response-panel)
20. [ASM Debug Panel](#20-asm-debug-panel)
21. [Safety Monitor Panel](#21-safety-monitor-panel)
22. [Policies Panel](#22-policies-panel)
23. [Hotpatch Detail Panel](#23-hotpatch-detail-panel)
24. [CoT Detail Panel (Chain-of-Thought Inspector)](#24-cot-detail-panel)
25. [Tools Panel (Tool Executor)](#25-tools-panel)
26. [Engine Explorer Panel](#26-engine-explorer-panel)
27. [Metrics Panel (Server Metrics)](#27-metrics-panel)
28. [Subagent & Chain Panel](#28-subagent--chain-panel)
29. [Confidence Evaluator Panel](#29-confidence-evaluator-panel)
30. [Governor Panel (Task Governor)](#30-governor-panel)
31. [LSP Panel](#31-lsp-panel)
32. [Hybrid Completion Panel](#32-hybrid-completion-panel)
33. [Replay Panel](#33-replay-panel)
34. [Phase Status Panel](#34-phase-status-panel)
35. [VSIX Extension Manager (Extensions Panel)](#35-vsix-extension-manager)
36. [VSIX Extension Manager (Ext Panel - Alternate)](#36-vsix-extension-panel-alternate)
37. [WebView2 Browser](#37-webview2-browser)
38. [Agentic File Editor](#38-agentic-file-editor)
39. [Chain-of-Thought (CoT) Multi-Model System](#39-chain-of-thought-cot-multi-model)
40. [DualAgent (Architect+Coder Pipeline)](#40-dualagent)
41. [RawrVoice (STT/TTS)](#41-rawrvoice-stttts)
42. [Ghost IDE Session (RDP-style Embed)](#42-ghost-ide-session)
43. [Standalone Titlebar & Window Chrome](#43-standalone-titlebar--window-chrome)
44. [Keyboard Shortcuts](#44-keyboard-shortcuts)
45. [Mobile/Responsive Layout](#45-mobileresponsive-layout)
46. [localStorage Persistence](#46-localstorage-persistence)
47. [Engine Registry & Swap](#47-engine-registry--swap)
48. [Reverse Engineering Panel](#48-reverse-engineering-panel)
49. [Conversation Memory & Context](#49-conversation-memory--context)
50. [Tensor Bunny Hop](#50-tensor-bunny-hop)
51. [Safe Decode Profile](#51-safe-decode-profile)
52. [Offline Mode](#52-offline-mode)
53. [CDN Fallbacks (DOMPurify, hljs)](#53-cdn-fallbacks)
54. [CRT Scanline Effect](#54-crt-scanline-effect)
55. [Codex Operations](#55-codex-operations)

---

## 1. Application Bootstrap & Initialization

| Aspect | Detail |
|---|---|
| **Functions** | `DOMContentLoaded` handler (~L10152), `connectBackend()` (L11375), `addWelcomeMessage()` (L10280), `startIdePoll()` (L9646) |
| **DOM Elements** | `body`, `.ide-container`, `#standaloneTitlebar` |
| **Backend** | Probes `localhost:11435/status`, `/api/status`, `/health` |
| **localStorage** | Checks availability (L10139-10148), restores session |
| **Standalone?** | Yes — detects `file://` protocol, activates standalone titlebar |

**Testable behaviors:**
- Page loads without errors (file:// and http://)
- DOMPurify shim activates if CDN blocked
- hljs shim activates if CDN blocked
- Standalone titlebar appears for file:// mode
- Defensive check detects missing critical functions (L24563-24585)
- Welcome message rendered with feature list

---

## 2. Backend Connection & Health

| Aspect | Detail |
|---|---|
| **Functions** | `connectBackend()` (L11375), `checkServerStatus()` (L13196), `refreshBackendBeacon()` (L10000), `getActiveUrl()` (L11325), `syncTitlebarStatus()` (L10053) |
| **DOM Elements** | `.status-pill` (online/offline/connecting), `#statusText`, `#statusDot`, `#tbDot`, `#tbStatusText` |
| **API Endpoints** | `GET /status`, `GET /api/status`, `GET /health`, `GET /models`, `GET /api/tags` |
| **Ports Probed** | 11435 (Win32 IDE/tool_server), 11434 (Ollama direct), 8080, 3000, 5000 |
| **Backend?** | Required for full testing; partial standalone via beacon fallback |

**Testable behaviors:**
- Status pill transitions: connecting → online / offline
- DirectMode detection (Ollama at 11434 vs serve.py at 11435)
- Server type detection: `rawrxd-win32ide`, `RawrXD-serve.py`, `ollama-direct`
- `/api/cli` beacon probing (`State.backend.hasCliEndpoint`)
- Auto-reconnect timer (10s interval) on disconnect
- `updateBackendInfo()` (L11543) populates right sidebar info
- `updateModeBadge()` (L11585) shows online/offline badge

---

## 3. Chat / Message System

| Aspect | Detail |
|---|---|
| **Functions** | `sendMessage()` (L12294), `sendToBackend()` (~L22977 via CoT hook), `addMessage()` (L13036), `formatMessage()` (L13089), `showTyping()` (L13172), `hideTyping()` (L13188), `clearChat()` (L18241), `handleInput()` (L12170), `autoResizeInput()` (L12178), `stopGeneration()` (L12548) |
| **DOM Elements** | `#chatInput` (textarea), `#sendBtn`, `#stopBtn`, `#chatMessages`, `.message`, `.message-avatar`, `.message-content`, `.typing-indicator` |
| **API Endpoints** | `POST /v1/chat/completions` (primary), `POST /api/generate` (fallback streaming), `GET /ask` (legacy) |
| **Backend?** | Online mode needs backend; offline mode has `getOfflineResponse()` |

**Testable behaviors:**
- Send message via Enter key or Send button
- Input auto-resize on typing
- Input validation (XSS, length, rate limiting)
- File context injection into message payload
- Streaming token display with rate indicator (`showStreamRate()`, L12592)
- Stop generation via button or Ctrl+C
- Message formatting: Markdown → HTML, code blocks, math
- Typing indicator animation
- Offline fallback responses
- DOMPurify sanitization of assistant HTML output

---

## 4. Model Selection & Management

| Aspect | Detail |
|---|---|
| **Functions** | `refreshModels()` (L11704), `populateModelSelect()` (L11660), `populateModelsFallback()` (L11699), `changeModel()` (L11713) |
| **DOM Elements** | `#modelSelect` (select dropdown), `.model-select` |
| **API Endpoints** | `GET /models` (proxy), `GET /api/tags` (Ollama native) |
| **Backend?** | Yes for live models; fallback list available offline |

**Testable behaviors:**
- Dropdown populated from `/models` or `/api/tags`
- Fallback model list when both fail
- Model change updates `State.model.current`
- Safe Decode auto-clamp triggers for large models (≥27B)
- `estimateModelSizeB()` (L10321) size estimation from model name

---

## 5. MASM Model Bridge

| Aspect | Detail |
|---|---|
| **Functions** | `renderBridgeProfiles()` (L11815), `renderBridgeProfilesFallback()` (L11871), `selectBridgeProfile()` (L11914), `bridgeQuantChanged()` (L12078), `bridgeLoadModel()` (~L11928), `bridgeUnloadModel()` (~L11999), `fetchBridgeCapabilities()` (~L12033), `updateBridgeStatusBar()` (L12084), `mergeBridgeIntoModelSelect()` (L12128) |
| **DOM Elements** | `#modelBridgeBody`, `#bridgeQuantSelect`, `.bridge-model-card`, `#bridgeStatusRow` |
| **API Endpoints** | `GET /api/model/profiles`, `POST /api/model/load`, `POST /api/model/unload`, `GET /api/engine/capabilities` |
| **State** | `BridgeState` (L11745) |
| **Backend?** | Yes — requires Win32 IDE with MASM bridge |

**Testable behaviors:**
- Bridge profile list rendering (tier groups: small/medium/large/ultra/800b)
- Quantization selector (Q2_K through Q8_0)
- Load/unload model via bridge
- Engine capability query (CPU features, RAM)
- Status chips update (CPU, RAM, active model)

---

## 6. Streaming Engine (RawrXDStream)

| Aspect | Detail |
|---|---|
| **Class** | `RawrXDStream` (L12353) |
| **Methods** | `chatGenerate()` (async generator), `chatCompletions()`, `models()`, `ask()`, `replayAgent()` |
| **API Endpoints** | `POST /api/generate` (NDJSON streaming), `POST /v1/chat/completions` (SSE streaming), `GET /models`, `POST /ask`, `POST /api/agents/replay` |
| **Backend?** | Yes |

**Testable behaviors:**
- NDJSON cross-chunk parsing buffer
- SSE `data:` line parsing with `[DONE]` handling
- AbortController integration (stream cancel)
- Token counting and rate calculation
- Fallback chain: `/v1/chat/completions` → `/api/generate` (stream) → `/api/generate` (non-stream) → `/ask`

---

## 7. Terminal Emulation (Dual-Mode)

| Aspect | Detail |
|---|---|
| **Functions** | `switchTerminalMode()` (L13534), `handleTerminal()` (L15234), `executeCommand()` (L15240), `addTerminalLine()` (L18139), `navigateHistory()` (L18148), `clearTerminal()` (L18160), `toggleTerminal()` (L18164), `toggleTerminalFull()` (L18178), `renderCliOutput()` (L13580), `getCliUrl()` (L13566) |
| **DOM Elements** | `#terminalPanel`, `#terminalInput`, `#terminalOutput`, `#termPrompt`, `#termTabLocal`, `#termTabCli`, `#termModeIndicator`, `#termToggleBtn` |
| **API Endpoints** | `POST /api/cli` (remote CLI mode) |
| **Backend?** | Local mode: standalone. CLI mode: requires `/api/cli` endpoint |

**Local commands (client-side, no backend):**
- `help`, `status`, `clear`, `models` / `list models`, `tags`
- `debug`, `endpoints`, `theme`, `export`
- `/ask <question>` — proxied to `/api/generate`
- `/plan`, `/analyze`, `/explain`, `/review`, `/refactor`
- `!engine list`, `!engine swap <name>`, `!engine status`
- `!ext`, `!backend`, `!router`, `!swarm`, `!safety`, `!cot`, `!confidence`, `!governor`, `!lsp`, `!hybrid`, `!re`
- File reading: auto-detects file paths in messages
- Command history (up/down arrows)
- Tab switching (Local ↔ CLI)

**CLI commands (remote via /api/cli):**
- Forward all commands to backend
- Fallback to client-side if `/api/cli` unavailable

---

## 8. File Attachment & Drag-Drop

| Aspect | Detail |
|---|---|
| **Functions** | `attachFile()` (L13374), `setupDragDrop()` (L13249), `handleFiles()` (L13256), `renderFile()` (L13310), `removeFile()` (L13343), `updateFileBadge()` (L13351), `waitForPendingFiles()` (L13293), `getFileIcon()` (L13325), `formatBytes()` (L13336), `detectFilePaths()` (L12186) |
| **DOM Elements** | `#dropZone`, `#fileInput`, `#fileList`, `#fileBadge` |
| **Backend?** | Standalone (FileReader API). File path auto-attach uses `/api/read-file` |

**Testable behaviors:**
- Click to attach files via file picker
- Drag-and-drop file attachment
- File list rendering with icon, name, size
- File content read via FileReader
- File path auto-detection in message text
- Local file reading via `/api/read-file` endpoint
- File removal from attachment list

---

## 9. Code Block Rendering & Syntax Highlighting

| Aspect | Detail |
|---|---|
| **Functions** | `formatMessage()` (L13089), `copyCode()` (L18222), `insertSnippet()` (L18230) |
| **DOM Elements** | `.code-block`, `.code-header`, `.code-lang`, `.code-content`, `.code-btn` (Copy) |
| **Dependencies** | highlight.js (CDN or shim), DOMPurify |
| **Backend?** | Standalone |

**Testable behaviors:**
- Markdown fenced code blocks (```lang) rendered with syntax highlighting
- Copy button copies code to clipboard
- Insert snippet buttons (code, bash) in toolbar
- hljs fallback if CDN unavailable
- DOMPurify sanitization of rendered HTML

---

## 10. Debug Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showDebug()` (L13379), `closeDebug()` (L13383), `exportDebug()` (L13482), `logDebug()` (L13515), `persistDebugSettings()` (L6478), `runDiagnostics()` (~L6416), `testAllEndpoints()` (~L6417) |
| **DOM Elements** | `#debugPanel`, `#debugLog`, `#dbgBackendUrl`, debug checkboxes |
| **localStorage** | `rawrxd_debug_settings` — persists checkbox state |
| **API Endpoints** | `GET /health`, `GET /status`, `GET /models`, `GET /api/tags` (diagnostics) |
| **Backend?** | Panel opens standalone; diagnostics/test endpoints need backend |

**Testable behaviors:**
- Panel open/close
- Debug log entries with timestamps and levels
- Backend URL override input
- Show Timing toggle  
- Debug settings persistence via localStorage
- Run Diagnostics: probes health, status, models
- Test All Endpoints: tests all known API endpoints
- Export debug data as JSON

---

## 11. Endpoints Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showEndpoints()` (L13217) |
| **DOM Elements** | Overlay panel |
| **API Endpoints** | `GET /status` (probes backend) |
| **Backend?** | Panel content is static; probe is backend-dependent |

**Testable behaviors:**
- Lists all 100+ supported API endpoints with descriptions
- Organized by server type (tool_server, complete_server, Ollama)

---

## 12. Failure Timeline & Agentic Recovery

| Aspect | Detail |
|---|---|
| **Functions** | `showFailures()` (L18252), `closeFailures()` (L18257), `renderFailureTimeline()` (L18284), `showFailureDetail()` (L18333), `renderFailureStats()` (L18359), `exportFailures()` (L18399), `replayFailure()` (dynamic) |
| **DOM Elements** | `#failuresPanel`, `#failuresBody`, `#failureDetailPanel`, failure action buttons (retry/skip/escalate) |
| **API Endpoints** | `GET /api/failures?limit=200`, `POST /api/agents/replay` |
| **Backend?** | Yes for live data; can render cached `State.failureData` |

**Testable behaviors:**
- Failure timeline table with sortable columns
- Failure detail card (type, outcome, strategy, evidence, prompt, attempt, session)
- Retry / Skip / Escalate action buttons
- Failure statistics aggregation
- Export failures as JSON
- Replay failed agents via `/api/agents/replay`

---

## 13. Agent Dashboard & Hotpatching

| Aspect | Detail |
|---|---|
| **Functions** | `showAgentDashboard()` (L19172), `closeAgentDashboard()` (L19179), `startAgentPoll()` (L19184), `stopAgentPoll()` (L19200), `toggleAgentPoll()` (L19212), `renderAgentStatus()` (L19229), `toggleHotpatchLayer()` (L19314), `toggleAllHotpatchLayers()` (L19339), `applyHotpatch()` (L19367), `revertHotpatch()` (L19392), `renderAgentTimeline()` (L19429), `exportAgentData()` (L19517) |
| **DOM Elements** | `#agentPanel`, `#agentBody`, hotpatch layer checkboxes (memory/byte/server), `#agentTimeline` |
| **API Endpoints** | `GET /api/agents/status`, `GET /api/agents/history?limit=50`, `POST /api/hotpatch/toggle`, `POST /api/hotpatch/apply`, `POST /api/hotpatch/revert`, `POST /api/agents/replay` |
| **Backend?** | Yes |

**Testable behaviors:**
- Agent status rendering (failure detector, puppeteer, hotpatch layers)
- Auto-poll toggle (5s interval)
- Three-layer hotpatch controls: Memory / Byte / Server
- Toggle individual layers on/off
- Apply / Revert hotpatch per layer
- Toggle All hotpatch layers
- Event timeline rendering
- Export agent data as JSON

---

## 14. Performance Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showPerfPanel()` (L18416), `closePerfPanel()` (L18421), `renderPerfPanel()` (L18425), `drawSparkline()` (L18476), `renderBenchmarkTable()` (L18541), `renderRequestHistory()` (L18576), `renderPercentiles()` (L18611), `percentile()` (L18624), `renderEndpointBreakdown()` (L18636), `renderStructuredLog()` (L18665), `exportPerfData()` (L18703), `recordMetric()` (L9195), `logStructured()` (L9245), `updatePerfSidebar()` (L9259) |
| **DOM Elements** | `#perfPanel`, sparkline canvases, benchmark table, request history table |
| **State** | `State.perf` — history ring buffer (100), structured log ring buffer (500) |
| **Backend?** | Standalone (client-side metrics collection) |

**Testable behaviors:**
- Sparkline graphs (latency, tokens/s)
- Benchmark table per model (requests, tokens, avg TPS, best TPS)
- Request history log (last 100)
- Percentile calculations (p50, p90, p99)
- Endpoint breakdown statistics
- Structured log viewer with levels
- Export performance data as JSON
- Sidebar performance stats update

---

## 15. Security Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showSecurityPanel()` (L19686), `closeSecurityPanel()` (L19693), `renderSecurityPanel()` (L19698), `validateInput()` (L19550), `checkRateLimit()` (L19590), `resetRateLimit()` (L19609), `isUrlAllowed()` (L19617), `validateBackendUrl()` (L19624), `auditCSP()` (L19638), `secLog()` (L19536), `renderSecurityLog()` (L20598), `exportSecurityAudit()` (L20620) |
| **DOM Elements** | `#securityPanel`, CSP audit display, rate limit display, security log, URL allowlist |
| **State** | `State.security` — rateLimit, inputGuard, urlAllowlist, eventLog |
| **Backend?** | Standalone — all client-side |

**Testable behaviors:**
- Input sanitization (XSS pattern detection)
- Input length limit enforcement (50,000 chars)
- Rate limiting (30 req/min)
- Rate limit reset
- URL allowlist validation (localhost only by default)
- Backend URL validation
- CSP audit (parses Content-Security-Policy meta tag)
- Security event log with levels
- Export security audit as JSON

---

## 16. Backend Switcher

| Aspect | Detail |
|---|---|
| **Functions** | `showBackendSwitcher()` (L19749), `closeBackendSwitcher()` (L19753), `_renderBackendCard()` (L19758), `fetchBackends()` (~L19831), `switchBackend()` (~L19881) |
| **DOM Elements** | `#backendSwitcherPanel`, backend cards |
| **API Endpoints** | `GET /api/backends`, `GET /api/backend/active`, `POST /api/backend/switch` |
| **Backend?** | Yes for remote; beacon-mode fallback probes ports directly |

**Testable behaviors:**
- Backend list from `/api/backends` or beacon probe
- Backend card rendering (name, URL, status)
- Switch active backend
- Beacon mode when `/api/backends` unavailable
- Direct mode switch (Ollama/IDE)

---

## 17. LLM Router Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showRouterPanel()` (L19945), `closeRouterPanel()` (L19949), `fetchRouterStatus()` (~L19956) |
| **DOM Elements** | `#routerPanel` |
| **API Endpoints** | `GET /api/router/status`, `GET /api/router/decision`, `GET /api/router/capabilities`, `GET /api/router/heatmap`, `GET /api/router/pins`, `POST /api/router/pin` |
| **Backend?** | Yes |

**Testable behaviors:**
- Router status display
- Last routing decision
- Backend capabilities matrix
- Routing heatmap
- Pinned routes display
- Pin a route to a specific backend

---

## 18. Swarm Dashboard

| Aspect | Detail |
|---|---|
| **Functions** | `showSwarmPanel()` (L20014), `closeSwarmPanel()` (L20018), `fetchSwarmStatus()` (~L20024), `swarmAction()` (~L20080) |
| **DOM Elements** | `#swarmPanel` |
| **API Endpoints** | `GET /api/swarm/status`, `GET /api/swarm/nodes`, `GET /api/swarm/tasks`, `GET /api/swarm/events`, `POST /api/swarm/start`, `POST /api/swarm/stop`, `POST /api/swarm/cache/clear` |
| **Backend?** | Yes |

**Testable behaviors:**
- Swarm status display
- Node list
- Task list
- Event log
- Start/Stop/Clear cache actions

---

## 19. Multi-Response Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showMultiResponsePanel()` (L20098), `closeMultiResponsePanel()` (L20102), `fetchMultiResponseStatus()` (~L20108) |
| **DOM Elements** | `#multiResponsePanel` |
| **API Endpoints** | `GET /api/multi-response/status`, `GET /api/multi-response/templates`, `GET /api/multi-response/stats` |
| **Backend?** | Yes |

**Testable behaviors:**
- S/G/C/X multi-response status
- Template list
- Preference statistics

---

## 20. ASM Debug Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showAsmDebugPanel()` (L20148), `closeAsmDebugPanel()` (L20152), `fetchAsmDebugStatus()` (~L20159), `asmDebugAction()` (~L20235) |
| **DOM Elements** | `#asmDebugPanel` |
| **API Endpoints** | `GET /api/debug/status`, `GET /api/debug/breakpoints`, `GET /api/debug/registers`, `GET /api/debug/stack`, `GET /api/debug/threads`, `GET /api/debug/events`, `POST /api/debug/<action>` |
| **Backend?** | Yes |

**Testable behaviors:**
- Debug status display
- Breakpoint list
- Register state
- Stack frames
- Thread list
- Debug events
- Go / Launch / Attach actions

---

## 21. Safety Monitor Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showSafetyPanel()` (L20252), `closeSafetyPanel()` (L20256), `fetchSafetyStatus()` (~L20528), `safetyCheck()` (~L20565), `safetyRollback()` (~L20582) |
| **DOM Elements** | `#safetyPanel` |
| **API Endpoints** | `GET /api/safety/status`, `GET /api/safety/violations`, `POST /api/safety/check`, `POST /api/safety/rollback` |
| **Backend?** | Yes |

**Testable behaviors:**
- Safety status display
- Violation list
- Check last output for safety
- Rollback last output

---

## 22. Policies Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showPoliciesPanel()` (L20261), `closePoliciesPanel()` (L20265), `refreshPoliciesPanel()` (~dynamic), `importPoliciesPrompt()` (L20324), `applyPolicyById()` (~dynamic), `fetchPolicySuggestions()`, `exportPolicies()`, `fetchPolicyHeuristics()`, `fetchPolicyStats()` |
| **DOM Elements** | `#policiesPanel` |
| **API Endpoints** | `GET /api/policies`, `GET /api/policies/suggestions`, `POST /api/policies/apply`, `POST /api/policies/reject`, `GET /api/policies/export`, `POST /api/policies/import`, `GET /api/policies/heuristics`, `GET /api/policies/stats` |
| **Backend?** | Yes |

**Testable behaviors:**
- Active policies list
- Policy suggestions with Apply button
- Export/Import policies
- Heuristic rules display
- Policy statistics

---

## 23. Hotpatch Detail Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showHotpatchDetailPanel()` (L20354), `closeHotpatchDetailPanel()` (L20358), `refreshHotpatchDetailPanel()` (~dynamic), `hotpatchAction()` (~dynamic) |
| **DOM Elements** | `#hotpatchDetailPanel`, `#hpMemoryStatus`, `#hpByteStatus`, `#hpServerStatus` |
| **API Endpoints** | `GET /api/hotpatch/status`, `POST /api/hotpatch/apply`, `POST /api/hotpatch/revert`, `POST /api/hotpatch/toggle` |
| **Backend?** | Yes |

**Testable behaviors:**
- Three-layer status display (Memory / Byte / Server)
- Toggle each layer
- Apply All / Revert All

---

## 24. CoT Detail Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showCotDetailPanel()` (L20385), `closeCotDetailPanel()` (L20389), `refreshCotDetailPanel()` (~dynamic), `fetchExplainStats()` (~dynamic) |
| **DOM Elements** | `#cotDetailPanel`, `#cotExplainTotal`, `#cotExplainAvgSteps` |
| **API Endpoints** | `GET /api/agents/explain`, `GET /api/agents/explain/stats` |
| **Backend?** | Yes |

**Testable behaviors:**
- Total explanations count
- Average steps per explanation
- Explanation stats

---

## 25. Tools Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showToolsPanel()` (L20421), `closeToolsPanel()` (L20424), `executeToolFromPanel()` (~L7973) |
| **DOM Elements** | `#toolsPanel`, tool name input, args input |
| **API Endpoints** | `POST /api/tool` with `{ tool, args }` |
| **Backend?** | Yes |

**Testable behaviors:**
- Tool name and arguments input
- Execute tool
- Result display

---

## 26. Engine Explorer Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showEnginePanel()` (L20444), `closeEnginePanel()` (L20448), `renderEngineMap()` (L20451), `filterEngineMap()` (L11094) |
| **DOM Elements** | `#enginePanel`, engine subsystem cards |
| **State** | `EngineRegistry` (~L9003) — 30+ engines |
| **Backend?** | Standalone (renders from client-side EngineRegistry) |

**Testable behaviors:**
- All 30+ engines listed with name, module, status, description
- Filter by keyword
- Module grouping (core, agentic, asm, ext, gpu, re, loader, ai, compiler, phase10)
- Active engine indicator
- Clickable endpoints for each subsystem

---

## 27. Metrics Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showMetricsPanel()` (L20455), `closeMetricsPanel()` (L20459), `refreshMetricsPanel()` (~dynamic) |
| **DOM Elements** | `#metricsPanel`, `#metricsUptime`, `#metricsRequests`, `#metricsMemory`, `#metricsHealth` |
| **API Endpoints** | `GET /metrics`, `GET /health`, `GET /api/status` |
| **Backend?** | Yes |

**Testable behaviors:**
- Uptime display
- Request count
- Memory usage
- Health status

---

## 28. Subagent & Chain Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showSubagentPanel()` (L20493), `closeSubagentPanel()` (L20496), `executeSubagentFromPanel()` (~L8091) |
| **DOM Elements** | `#subagentPanel`, task input, context input |
| **API Endpoints** | `POST /api/subagent`, `POST /api/chain` |
| **Backend?** | Yes |

**Testable behaviors:**
- Subagent task submission
- Chain pipeline execution
- Result display

---

## 29. Confidence Evaluator Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showConfidencePanel()` (L20658), `closeConfidencePanel()` (L20662), `evaluateConfidence()` (~L20707) |
| **DOM Elements** | `#confidencePanel` (overlay) |
| **API Endpoints** | `GET /api/confidence/status`, `GET /api/confidence/history`, `POST /api/confidence/evaluate` |
| **Backend?** | Yes |

**Testable behaviors:**
- Confidence status display
- Evaluation history
- Evaluate confidence on text

---

## 30. Governor Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showGovernorPanel()` (L20729), `closeGovernorPanel()` (L20733), `governorSubmitTask()` (~L20776), `governorRefresh()` (~dynamic) |
| **DOM Elements** | `#governorPanel` (overlay) |
| **API Endpoints** | `GET /api/governor/status`, `GET /api/governor/result`, `POST /api/governor/submit`, `POST /api/governor/kill` |
| **Backend?** | Yes |

**Testable behaviors:**
- Task submission
- Task status refresh
- Result retrieval
- Kill running task

---

## 31. LSP Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showLspPanel()` (L20794), `closeLspPanel()` (L20798), `lspRefreshDiagnostics()` (~L20834) |
| **DOM Elements** | `#lspPanel` (overlay) |
| **API Endpoints** | `GET /api/lsp/status`, `GET /api/lsp/diagnostics` |
| **Backend?** | Yes |

**Testable behaviors:**
- LSP integration status
- Diagnostics list refresh

---

## 32. Hybrid Completion Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showHybridPanel()` (L20867), `closeHybridPanel()` (L20871), `hybridComplete()` (~L20895), `hybridAnalyze()` (~L20914) |
| **DOM Elements** | `#hybridPanel` (overlay) |
| **API Endpoints** | `GET /api/hybrid/status`, `POST /api/hybrid/complete`, `POST /api/hybrid/analyze`, `POST /api/hybrid/rename`, `POST /api/hybrid/symbol-usage` |
| **Backend?** | Yes |

**Testable behaviors:**
- Hybrid completion status
- Code completion request
- Code analysis request

---

## 33. Replay Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showReplayPanel()` (L20931), `closeReplayPanel()` (L20935), `replayRefresh()` (~L20941) |
| **DOM Elements** | `#replayPanel` (overlay) |
| **API Endpoints** | `GET /api/replay/status`, `GET /api/replay/sessions`, `GET /api/replay/records` |
| **Backend?** | Yes |

**Testable behaviors:**
- Replay session status
- Session list
- Record retrieval

---

## 34. Phase Status Panel

| Aspect | Detail |
|---|---|
| **Functions** | `showPhaseStatusPanel()` (L20977), `closePhaseStatusPanel()` (L20981), `phaseStatusRefresh()` (~L20991) |
| **DOM Elements** | `#phaseStatusPanel` (overlay) |
| **API Endpoints** | `GET /api/phase10/status`, `GET /api/phase11/status`, `GET /api/phase12/status` |
| **Backend?** | Yes |

**Testable behaviors:**
- Phase 10 (Speculative Decoding) status
- Phase 11 (Flash-Attention v2) status
- Phase 12 (Extreme Compression) status

---

## 35. VSIX Extension Manager (Extensions Panel)

| Aspect | Detail |
|---|---|
| **Functions** | `showExtensionsPanel()` (L21033), `closeExtensionsPanel()` (L21038), `switchExtTab()` (L21042), `refreshExtensionsList()` (~dynamic), `searchExtMarketplace()` (~L22008), `handleVsixFile()` (L21206), `filterExtensions()` (L21310), `updateExtStatusBar()` (L21321), `installVsixFromUrl()` (~dynamic), `restartExtensionHost()`, `killExtensionHost()`, `showExtHostLogs()` |
| **DOM Elements** | `#extensionsPanel`, tabs: installed/marketplace/load/running, `#vsixFileInput`, `#vsixDropZone` |
| **API Endpoints** | `GET /api/extensions`, `POST /api/extensions/install`, `POST /api/extensions/uninstall`, `POST /api/extensions/enable`, `POST /api/extensions/disable`, `POST /api/extensions/marketplace/search`, `POST /api/extensions/load-vsix`, `GET /api/extensions/host/status`, `POST /api/extensions/host/restart`, `POST /api/extensions/host/kill`, `GET /api/extensions/host/logs` |
| **State** | `State.extensions`, `ExtensionState` (L9097) |
| **Backend?** | Yes for marketplace/host; local state management standalone |

**Testable behaviors:**
- Tab switching (installed/marketplace/load/running)
- Extension search (marketplace API)
- VSIX file install (drag-drop or file picker or URL)
- Extension enable/disable/uninstall
- Extension host restart/kill
- Extension host logs
- Filter extensions by keyword
- Extension status bar (count, host status)

---

## 36. VSIX Extension Panel (Alternate / Phase 39)

| Aspect | Detail |
|---|---|
| **Functions** | `toggleExtPanel()` (L21819), `showExtPanel()` (L21830), `refreshExtPanel()` (L21835), `switchExtTab()` (L21862), `extRenderList()` (L21893), `extRenderTypes()` (L21942), `extRenderHistory()` (L21969), `filterExtList()` (L21995), `searchMarketplace()` (L22008), `searchOpenVSX()` (L22062), `installFromMarketplaceResult()` (L22113), `installFromOpenVSX()` (L22135), `installVsixFromPath()` (L22140), `installVsixFromFile()` (L22169), `installFromMarketplaceId()` (L22213), `installNativeExt()` (L22223), `installPsm1Ext()` (L22251), `extPanelEnable()` (L22277), `extPanelDisable()` (L22285), `extPanelUninstall()` (L22293), `extPsAction()` (L22302), `extPsCreate()` (L22329) |
| **Tabs** | installed, builtin, marketplace, vsix, types, powershell, history |
| **API Endpoints** | Same as #35 + `POST /api/extensions/load-native`, `POST /api/extensions/import-psm1`, `POST /api/extensions/scan` |
| **External API** | `https://open-vsx.org/api/-/search` (Open VSX registry) |
| **Backend?** | Yes for remote operations; types/history tabs standalone |

**Testable behaviors:**
- All 7 tabs render correctly
- Marketplace search (VS Code marketplace + Open VSX)
- Install from: marketplace result, Open VSX, VSIX path, VSIX file upload, marketplace ID, native DLL, PSM1
- Enable/Disable/Uninstall per extension
- PowerShell extension commands (list, menu, craft, scan, create)
- Extension type descriptions
- Extension history timeline

---

## 37. WebView2 Browser

| Aspect | Detail |
|---|---|
| **Functions** | `showBrowserPanel()` (L21374), `closeBrowserPanel()` (L21388), `browserNavigate()` (L21472), `browserNavigateFromHome()` (L21491), `browserNavigateTo()` (L21501), `browserBack()` (L21580), `browserForward()` (L21607), `browserRefreshPage()` (L21619), `browserGoHome()` (L21630), `browserNewTab()` (L21641), `browserSwitchTab()` (L21653), `browserCloseTab()` (L21666), `browserAddBookmark()` (L21697), `browserShowBookmarks()` (L21727), `browserRemoveBookmark()` (L21747), `browserDetach()` (L21755), `browserToggleDevtools()` (L21767), `browserExtractContent()` (L21777), `browserSendToModel()` (L21795), `_browserUpdateNavButtons()` (L21572), `_browserUpdateLock()` (L21422), `_browserUpdateStatusBar()` (L21437), `_browserRenderTabs()` (L21457), `_browserRenderBookmarks()` (L21717) |
| **DOM Elements** | `#browserOverlay`, `#browserTabBar`, `#browserViewport`, `#browserUrlBar`, `#browserBtnBack`, `#browserBtnFwd`, `#browserBtnRefresh`, `#browserBtnHome`, `#browserLock`, `#browserHomePage`, `#browserStatusBar` |
| **API Endpoints** | `POST /api/browse` (proxy), `POST /api/browse/extract`, `POST /api/browse/screenshot` |
| **State** | `State.browser` — tabs, bookmarks, history |
| **Backend?** | Proxy mode needs backend; direct iframe mode standalone |

**Testable behaviors:**
- Tab management (new, switch, close)
- URL navigation with address bar
- Back/Forward navigation history
- Refresh page
- Home button (built-in home page with quicklinks)
- Bookmarks (add, remove, render, navigate)
- Lock icon (HTTP/HTTPS indicator)
- Default bookmarks bar (HuggingFace, Ollama, TheBloke, etc.)
- Home page quicklinks
- Search box on home page
- Detach to new window
- DevTools toggle
- Extract page content
- Send page content to model for analysis
- Status bar updates

---

## 38. Agentic File Editor

| Aspect | Detail |
|---|---|
| **Functions** | `showFileEditorPanel()` (L23025), `closeFileEditorPanel()` (L23034), `feApiUrl()` (L23040), `feApiPost()` (L23044), `feRefreshTree()` (L23064), `feRenderTree()` (L23090), `feToggleDir()` (L23121), `feFileIcon()` (L23181), `feFormatSize()` (L23196), `feTreeContextMenu()` (L23202), `feOpenFile()` (L23215), `feActivateTab()` (L23247), `feUpdateTabBar()` (L23277), `feCloseTab()` (L23298), `feSaveFile()` (L23318), `feRevertFile()` (L23337), `feUndo()` (L23354), `feRedo()` (L23367), `feOnEditorInput()` (L23381), `feOnEditorScroll()` (L23400), `feOnEditorKeydown()` (L23406), `feUpdateLineNumbers()` (L23461), `feUpdateStatusBar()` (L23474), `feFindReplace()` (L23511), `feDoFind()` (L23526), `feDoReplace()` (L23578), `feDoReplaceAll()` (L23598), `feGoToLine()` (L23638), `feToggleWordWrap()` (L23656), `feNewFile()` (L23664), `feNewFileInDir()` (L23683), `feNewFolderInDir()` (L23695), `feDeletePath()` (L23707), `feRenamePath()` (L23726), `feRenameActive()` (L23746), `feDeleteActive()` (L23751), `feDuplicateFile()` (L23756), `feShowDiff()` (L23771), `feShowSearch()` (L23817), `feDoGlobalSearch()` (L23827), `feOpenFileAtLine()` (L23876), `feShowHistory()` (L23893), `feAddHistory()` (L23900), `feRenderHistory()` (L23905) |
| **DOM Elements** | `#fileEditorPanel`, `#feTreeRoot`, `#feTabBar`, `#feEditor` (textarea), `#feLineNumbers`, `#feStatusBar`, `#feFindBar`, `#feFindInput`, `#feReplaceInput`, `#feSearchDrawer`, `#feHistoryDrawer`, `#feDiffDrawer`, `#feRootPathInput` |
| **API Endpoints** | `POST /api/list-dir` (expand tree), `POST /api/read-file` (open), `POST /api/write-file` (save/new), `POST /api/delete-file`, `POST /api/rename-file`, `POST /api/mkdir`, `POST /api/search-files` (global search) |
| **Backend?** | Yes — full file system access via backend API |

**Testable behaviors:**
- File tree rendering with directory expand/collapse
- File icons by extension
- File size formatting
- Open file in editor tab
- Multi-tab support (activate, close, update bar)
- Save file (Ctrl+S)
- Revert to last saved
- Undo/Redo
- Editor input handling (Tab indent, auto-indent)
- Scroll sync with line numbers
- Line number gutter
- Status bar (line, col, encoding, file size)
- Find and Replace (Ctrl+F, Ctrl+H)
- Find next / Replace / Replace All
- Go to Line (Ctrl+G)
- Word wrap toggle
- New file / New file in directory / New folder
- Delete file/folder
- Rename file
- Duplicate file
- Diff viewer (side-by-side)
- Global search across files
- Open file at specific line
- Operation history log
- Root path input

---

## 39. Chain-of-Thought (CoT) Multi-Model

| Aspect | Detail |
|---|---|
| **Object** | `CoT` (~L22487) |
| **Methods** | `toggle()`, `setStepCount()`, `applyPreset()`, `renderChain()`, `executeChain()`, `cancel()`, `persist()`, `load()`, `init()` |
| **DOM Elements** | `#cotSwitch`, `#cotPresets`, `#cotStepVal`, `#cotChainList`, `.cot-preset-btn`, `.cot-step-count` |
| **Presets** | review, audit, think, research, debate, custom |
| **API Endpoints** | `POST /api/cot/execute` (server-side), falls back to client-side sequential model calls via `/v1/chat/completions` or `/api/generate` |
| **localStorage** | `rawrxd_cot` — persists enabled state, steps, preset |
| **Backend?** | Server-side CoT needs backend; client-side fallback runs step-by-step |

**Testable behaviors:**
- CoT toggle on/off
- Preset selection (review, audit, think, research, debate, custom)
- Step count adjustment (1-10)
- Chain rendering (step cards with role, model, instructions)
- Step optional/skip toggle
- Role selector per step (COT_ROLES)
- Model selector per step
- Instruction editing per step
- Execute chain (server-side first, fallback to client-side)
- Chain result accordion display
- Cancel mid-execution
- Persist/Load from localStorage
- CoT overrides `sendToBackend` when enabled

---

## 40. DualAgent (Architect + Coder Pipeline)

| Aspect | Detail |
|---|---|
| **Object** | `DualAgent` (L11099) |
| **Methods** | `init()`, `shutdown()`, `status()`, `execute()`, `toggle()` |
| **DOM Elements** | `#dualAgentBtn`, `#dualAgentLabel` |
| **API Endpoints** | `POST /api/agent/dual/init`, `POST /api/agent/dual/shutdown`, `GET /api/agent/dual/status`, `POST /api/agent/dual/handoff`, `POST /v1/chat/completions` |
| **Backend?** | Yes |

**Testable behaviors:**
- Toggle DualAgent mode
- Init Architect + Coder agents
- Architect generates multi-step plan
- Each step handed off to Coder model
- Pipeline execution with progress display
- Abort pipeline
- Shutdown agents
- DualAgent overrides `sendToBackend` when enabled

---

## 41. RawrVoice (STT/TTS)

| Aspect | Detail |
|---|---|
| **Object** | `RawrVoice` (L24021) — IIFE module |
| **Functions** | `init()` (L24053), `toggleRecording()` (L24168), `startRecording()` (L24180), `stopRecording()` (L24193), `insertTranscript()` (L24203), `showInterim()` (L24216), `clearInterim()` (L24231), `toggleTTS()` (L24237), `speak()` (L24246), `chunkText()` (L24288), `stopSpeaking()` (L24303), `updateUI()` (L24308), `showSettings()` (L24340), `closeSettings()` (L24427), `setLang()` (L24436), `setVoice()` (L24437), `setRate()` (L24438), `setPitch()` (L24439), `setVolume()` (L24440), `toggleContinuous()` (L24441), `toggleAutoSend()` (L24447), `testTTS()` (L24454), `testSTT()` (L24462), `savePrefs()` (L24470), `loadPrefs()` (L24485), `logVoice()` (L24502), `hookTTS()` (L24508) |
| **DOM Elements** | `#voiceBtn`, `#voiceToolbarBtn`, `#voiceSettingsBtn`, voice settings overlay (dynamic), `#voiceLangSel`, `#voiceTTSVoiceSel`, `#voiceContinuousToggle`, `#voiceAutoSendToggle`, `#voiceTTSToggle` |
| **Browser APIs** | `webkitSpeechRecognition` / `SpeechRecognition` (STT), `speechSynthesis` (TTS) |
| **localStorage** | `rawrxd_voice_prefs` — lang, voice, rate, pitch, volume, continuous, autoSend, ttsEnabled |
| **Backend?** | Standalone (browser-native) |

**Testable behaviors:**
- Toggle voice recording (Ctrl+Shift+V)
- Speech-to-text recognition  
- Interim results display
- Final transcript insertion into chat input
- Auto-send on silence
- Continuous listening mode
- TTS toggle (Ctrl+Shift+U)
- Speak assistant messages aloud
- Text chunking for long TTS
- Stop speaking
- Voice settings panel (language, voice, rate, pitch, volume)
- Test TTS button
- Test STT button
- Preferences persist to localStorage
- Voice UI button state updates
- Hook into addMessage for auto-read

---

## 42. Ghost IDE Session (RDP-style Embed)

| Aspect | Detail |
|---|---|
| **Functions** | `ghostIntoIDE()` (L9782), `ghostCloseIDE()` (L9924), `ghostRefreshIDE()` (L9955), `ghostDetachIDE()` (L9963), `ghostMinimizeIDE()` (L9981), `_ghostScanResultsHtml()` (L9714), `_ghostStopAutoRetry()` (L9738), `_ghostStartAutoRetry()` (L9747) |
| **DOM Elements** | `#ghostOverlay`, `#ghostIframeWrap`, `#ghostDot`, `#ghostTitleText`, `#ghostLoading`, `#ghostProbeUrl` |
| **State** | `State.ghost` — active, minimized, ideUrl, probeTime, iframeRef, detachedWindow, sessionId, waitingForIde, autoRetryTimer, scanPorts |
| **Backend?** | Needs Win32 IDE running on a detected port |

**Testable behaviors:**
- Probe multiple ports for IDE (11435, 8080, 3000, 5000, 11434)
- Loading spinner during probe
- iframe injection when IDE found
- Refresh/Detach/Minimize/Close ghost session
- Auto-retry polling when IDE is starting
- Port scan results display
- Ghost toolbar
- Detach to popup window

---

## 43. Standalone Titlebar & Window Chrome

| Aspect | Detail |
|---|---|
| **Functions** | `toggleStandaloneFullscreen()` (L9633), `syncTitlebarStatus()` (L10053), `launchWin32IDE()` (L9622) |
| **DOM Elements** | `#standaloneTitlebar`, `#tbDot`, `#tbStatusText`, `#tbMode`, `#tbIdeBridge` |
| **CSS Classes** | `.standalone-titlebar`, `.standalone-titlebar.active`, `body.standalone-mode`, `body.standalone-fullscreen` |
| **Backend?** | Standalone — activates for file:// protocol |

**Testable behaviors:**
- Titlebar visible only in standalone/file:// mode
- Backend status dot (green/red)
- Status text updates
- Mode badge (STANDALONE/SERVED)
- IDE bridge button visibility when IDE detected
- Fullscreen toggle (F11)
- Reload button
- Drag area for window positioning
- Content pushed down by titlebar height (36px)

---

## 44. Keyboard Shortcuts

| Shortcut | Action | Function | Scope |
|---|---|---|---|
| **Enter** | Send message | `handleInput()` | Chat input |
| **Shift+Enter** | Newline in input | `handleInput()` | Chat input |
| **Ctrl+C** / **Escape** | Stop generation | `stopGeneration()` | Global (L12564) |
| **F11** | Fullscreen toggle | `toggleStandaloneFullscreen()` | Global (L12572) |
| **Ctrl+S** | Save file editor tab | `feSaveFile()` | Global (L23928) |
| **Ctrl+`** | Toggle terminal | `toggleTerminalFull()` | Global (L23972) |
| **Ctrl+B** | Toggle right sidebar | `toggleRightbar()` | Global (L23977) |
| **Ctrl+Shift+B** | Toggle browser | `showBrowserPanel()` / `closeBrowserPanel()` | Global (L23982) |
| **Ctrl+Shift+A** | Toggle security panel | `showSecurityPanel()` | Global (L23990) |
| **Ctrl+Shift+V** | Toggle voice recording | `RawrVoice.toggleRecording()` | Global (L24151) |
| **Ctrl+Shift+U** | Toggle TTS | `RawrVoice.toggleTTS()` | Global (L24156) |
| **Ctrl+F** | Find (file editor) | `feFindReplace()` | File editor (L23438) |
| **Ctrl+G** | Go to line | `feGoToLine()` | File editor (L23440) |
| **Ctrl+H** | Find & Replace | `feFindReplace()` | File editor (L23442) |
| **Ctrl+Z** | Undo | `feUndo()` | File editor (L23434) |
| **Ctrl+Shift+Z** / **Ctrl+Y** | Redo | `feRedo()` | File editor (L23436) |
| **Tab** | Indent | via `feOnEditorKeydown()` | File editor (L23406) |
| **Up/Down** | Command history | `navigateHistory()` | Terminal input |
| **Enter** | Execute command | `handleTerminal()` → `executeCommand()` | Terminal input |

---

## 45. Mobile/Responsive Layout

| Aspect | Detail |
|---|---|
| **Functions** | `toggleSidebarMobile()` (L23937), `closeMobileSidebars()` (L23952), `resize` handler (L24001) |
| **DOM Elements** | `#sidebarToggle`, `#rightbarToggle`, `#sidebarOverlay`, `.sidebar`, `.rightbar` |
| **Breakpoint** | ≤768px |
| **Backend?** | Standalone |

**Testable behaviors:**
- Sidebar hamburger toggle visible at narrow viewport
- Rightbar toggle visible at narrow viewport
- Overlay click closes sidebars
- Escape key closes sidebars
- Layout adapts (single column on mobile)
- Auto-close sidebars on window resize past breakpoint

---

## 46. localStorage Persistence

| Key | Data | Read at | Written at |
|---|---|---|---|
| `rawrxd_conversation` | messages array + system prompt | Boot (L10174) | After every message (L9398) |
| `rawrxd_gen_settings` | Generation parameters | Boot | On persist |
| `rawrxd_debug_settings` | Debug checkbox states | DOMContentLoaded (L6459) | On change (L6480) |
| `rawrxd_cot` | CoT enabled, steps, preset | `CoT.init()` (L22963) | `CoT.persist()` (L22933) |
| `rawrxd_voice_prefs` | Voice settings (lang, voice, rate, pitch, volume, toggles) | `loadPrefs()` (L24485) | `savePrefs()` (L24470) |

**Testable behaviors:**
- All 5 keys persist and restore correctly
- Graceful degradation when localStorage unavailable (file:// in some browsers)
- `_localStorageAvailable` guard (L10140)

---

## 47. Engine Registry & Swap

| Aspect | Detail |
|---|---|
| **Object** | `EngineRegistry` (~L9003) |
| **Methods** | `getActive()`, `swap()`, `listByModule()` |
| **Engines** | 30+ engines across modules: core, agentic, asm, ext, gpu, re, loader, ai, compiler, phase10 |
| **Terminal** | `!engine list`, `!engine swap <name>`, `!engine status` |
| **Backend?** | Standalone (client-side state) |

**Testable behaviors:**
- List all engines grouped by module
- Swap active engine
- Status display
- Engine Explorer panel renders from this registry

---

## 48. Reverse Engineering Panel

| Aspect | Detail |
|---|---|
| **Functions** | `toggleREPanel()` (L18730), `showREPanel()` (L18741), `switchRETab()` (L18749), `refreshREPanel()` (L18763), `reActivateModule()` (L18841), `rePEAnalyze()` (L18862), `reDisassemble()` (L18899), `reLoadASM()` (L18931), `reGGUFInspect()` (L18959), `reDeobfuscate()` (L19005), `reMemScan()` (L19045), `reResolveSymbols()` (L19072), `reOmegaScan()` (L19110) |
| **DOM Elements** | RE panel, tabs: overview/pe/disasm/gguf/deobf/memory/symbols/omega |
| **State** | `REState` — activeTab, modules[], history[] |
| **API Endpoints** | Via `POST /api/cli` (CLI commands: `re pe`, `re disasm`, `re gguf`, etc.) |
| **Backend?** | Yes for actual analysis; UI renders standalone |

**Testable behaviors:**
- 8 tabs render correctly (overview, pe, disasm, gguf, deobf, memory, symbols, omega)
- PE header analysis
- Disassembly view with ASM file loading (memory_patch.asm, byte_search.asm, request_patch.asm)
- GGUF model inspection
- Deobfuscation
- Memory scanning
- Symbol resolution
- Omega full scan
- Module activation
- Operation history log

---

## 49. Conversation Memory & Context

| Aspect | Detail |
|---|---|
| **Object** | `Conversation` (L9280) |
| **Methods** | `addMessage()`, `getContextForAPI()`, `getLegacyPrompt()`, `persist()`, `restore()`, `updateUI()` |
| **Max Messages** | 40 (20 exchanges) |
| **Backend?** | Standalone (client-side) |

**Testable behaviors:**
- Message accumulation with trimming at 40
- System prompt prepending
- File context injection (right before last user message)
- File truncation when exceeding context window
- OpenAI-compatible messages array building
- Legacy prompt string building for /ask
- Persistence to localStorage
- Restoration on page load
- Dismiss restored banner

---

## 50. Tensor Bunny Hop

| Aspect | Detail |
|---|---|
| **Functions** | `updateTensorHopUI()` (L10315), `buildTensorHopOptions()` (L10372) |
| **DOM Elements** | `#tensorHopBody`, tensor hop strategy selector, skip ratio slider |
| **State** | `State.gen.tensorHop` — enabled, strategy (auto/even/front/back/custom), skipRatio, keepFirst, keepLast, customSkip |
| **Backend?** | Options are sent to backend in API requests; UI is standalone |

**Testable behaviors:**
- Toggle tensor hop on/off
- Strategy selector change
- Skip ratio adjustment
- Custom skip layer indices
- Options serialized into API request payload

---

## 51. Safe Decode Profile

| Aspect | Detail |
|---|---|
| **Functions** | `isSafeDecodeActive()` (L10338), `getEffectiveGenParams()` (L10346), `updateSafeDecodeStatus()` (L10453), `firstTokenProbe()` (~L10420), `estimateModelSizeB()` (L10321) |
| **DOM Elements** | `#safeDecodeBody`, `#safeDecodeStatus` |
| **State** | `State.gen.safeDecodeProfile` — enabled, autoClamp, thresholdB (27), safeContext, safeMaxTokens, etc. |
| **API Endpoints** | `POST /api/generate` (first-token probe) |
| **Backend?** | Probe requires backend; clamping is client-side |

**Testable behaviors:**
- Auto-detect model size from name
- Clamp parameters for models ≥27B
- First-token probe before real request
- Status display (probe-ok, probe-failed, normal mode)
- Parameter overrides (context→3072, maxTokens→128, temperature→0.3)

---

## 52. Offline Mode

| Aspect | Detail |
|---|---|
| **Functions** | `getOfflineResponse()` (L13000), `markOffline()` (L13025) |
| **Backend?** | No |

**Testable behaviors:**
- Automatic offline detection
- Pre-programmed offline responses
- Mode badge shows "OFFLINE"
- All client-side features remain functional

---

## 53. CDN Fallbacks

| Aspect | Detail |
|---|---|
| **DOMPurify** | Inline tag-stripping shim (~L42-107) |
| **highlight.js** | No-op shim (~L109-115) |
| **Google Fonts** | Non-blocking preload, falls back to system fonts |
| **hljs CSS** | Inline fallback stylesheet (L119-182) |
| **Backend?** | Standalone |

**Testable behaviors:**
- DOMPurify shim sanitizes HTML without CDN
- hljs shim returns code unchanged
- hljs fallback CSS provides dark theme code styling
- Fallback CSS removed when CDN loads successfully (~L187-196)
- Fonts fall back to system monospace/sans-serif

---

## 54. CRT Scanline Effect

| Aspect | Detail |
|---|---|
| **CSS** | `body::before` with repeating-linear-gradient (L396-407) |
| **Backend?** | Standalone |
| **Testable** | Visual — scanline overlay visible on page |

---

## 55. Codex Operations

| Aspect | Detail |
|---|---|
| **Object** | `CodexOps` (~L9071) |
| **Properties** | `targets[]` (6 build targets), `buildCmd()`, `masmCmd()` |
| **Backend?** | Standalone (data only) |

**Testable behaviors:**
- Build target list: RawrXD-Win32IDE, RawrEngine, rawrxd-monaco-gen, self_test_gate, quant_utils, tool_server
- Build command generation
- MASM command generation

---

## COMPLETE API ENDPOINT CATALOG

### EngineAPI Methods (L10478+)

| Category | Endpoints |
|---|---|
| **Chat/Generate** | `POST /v1/chat/completions`, `POST /api/generate`, `POST /api/chat`, `POST /ask` |
| **Models** | `GET /api/tags`, `GET /models`, `GET /v1/models`, `GET /api/model/profiles`, `POST /api/model/load`, `POST /api/model/unload`, `GET /api/engine/capabilities` |
| **Status/Health** | `GET /status`, `GET /api/status`, `GET /health`, `GET /metrics` |
| **File System** | `POST /api/read-file`, `POST /api/write-file`, `POST /api/delete-file`, `POST /api/rename-file`, `POST /api/copy-file`, `POST /api/move-file`, `POST /api/mkdir`, `POST /api/stat-file`, `POST /api/list-directory`, `POST /api/search-files` |
| **CLI** | `POST /api/cli` |
| **Agents** | `GET /api/agents`, `GET /api/agents/status`, `GET /api/agents/history`, `POST /api/agents/replay`, `POST /api/agent/dual/init`, `POST /api/agent/dual/shutdown`, `GET /api/agent/dual/status`, `POST /api/agent/dual/handoff` |
| **Subagent/Chain** | `POST /api/subagent`, `POST /api/chain`, `POST /api/swarm` |
| **Policies** | `GET /api/policies`, `GET /api/policies/suggestions`, `POST /api/policies/apply`, `POST /api/policies/reject`, `GET /api/policies/export`, `POST /api/policies/import`, `GET /api/policies/heuristics`, `GET /api/policies/stats` |
| **CoT/Explain** | `GET /api/agents/explain`, `GET /api/agents/explain/stats`, `POST /api/cot/execute`, `GET /api/cot/status`, `GET /api/cot/presets`, `GET /api/cot/steps`, `GET /api/cot/roles` |
| **Backends** | `GET /api/backends`, `GET /api/backends/status`, `POST /api/backends/use`, `GET /api/backend/active`, `POST /api/backend/switch` |
| **Hotpatch** | `GET /api/hotpatch/status`, `POST /api/hotpatch/apply`, `POST /api/hotpatch/revert`, `POST /api/hotpatch/toggle` |
| **Tools** | `POST /api/tool` |
| **Failures** | `GET /api/failures` |
| **Extensions** | `GET /api/extensions`, `GET /api/extensions/<id>`, `POST /api/extensions/install`, `POST /api/extensions/uninstall`, `POST /api/extensions/enable`, `POST /api/extensions/disable`, `POST /api/extensions/activate`, `POST /api/extensions/deactivate`, `POST /api/extensions/marketplace/search`, `GET /api/extensions/marketplace/<id>`, `POST /api/extensions/load-vsix`, `GET /api/extensions/host/status`, `POST /api/extensions/host/restart`, `POST /api/extensions/host/kill`, `GET /api/extensions/host/logs`, `GET /api/extensions/<id>/contributes`, `POST /api/extensions/scan`, `GET /api/extensions/export`, `POST /api/extensions/import`, `POST /api/extensions/load-native`, `POST /api/extensions/import-psm1` |
| **Browse** | `POST /api/browse`, `POST /api/browse/extract`, `POST /api/browse/screenshot` |
| **Router** | `GET /api/router/status`, `GET /api/router/decision`, `GET /api/router/capabilities`, `GET /api/router/heatmap`, `GET /api/router/pins`, `POST /api/router/pin` |
| **Swarm** | `GET /api/swarm/status`, `GET /api/swarm/nodes`, `GET /api/swarm/tasks`, `GET /api/swarm/events`, `POST /api/swarm/start`, `POST /api/swarm/stop`, `POST /api/swarm/cache/clear` |
| **Multi-Response** | `GET /api/multi-response/status`, `GET /api/multi-response/templates`, `GET /api/multi-response/stats` |
| **ASM Debug** | `GET /api/debug/status`, `GET /api/debug/breakpoints`, `GET /api/debug/registers`, `GET /api/debug/stack`, `GET /api/debug/threads`, `GET /api/debug/events`, `POST /api/debug/<action>` |
| **Safety** | `GET /api/safety/status`, `GET /api/safety/violations`, `POST /api/safety/check`, `POST /api/safety/rollback` |
| **Confidence** | `GET /api/confidence/status`, `GET /api/confidence/history`, `POST /api/confidence/evaluate` |
| **Governor** | `GET /api/governor/status`, `GET /api/governor/result`, `POST /api/governor/submit`, `POST /api/governor/kill` |
| **LSP** | `GET /api/lsp/status`, `GET /api/lsp/diagnostics` |
| **Hybrid** | `GET /api/hybrid/status`, `POST /api/hybrid/complete`, `POST /api/hybrid/analyze`, `POST /api/hybrid/rename`, `POST /api/hybrid/symbol-usage` |
| **Replay** | `GET /api/replay/status`, `GET /api/replay/sessions`, `GET /api/replay/records` |
| **Phase** | `GET /api/phase10/status`, `GET /api/phase11/status`, `GET /api/phase12/status` |

---

## STANDALONE vs BACKEND-REQUIRED SUMMARY

### Fully Standalone (No Backend Needed)
1. Page load & bootstrap
2. CDN fallback shims (DOMPurify, hljs)
3. Standalone titlebar
4. CRT scanline effect
5. CSS layout & theming
6. Code block rendering & copy
7. Performance metrics (client-side recording)
8. Security panel (input validation, rate limiting, CSP audit)
9. Engine Registry & Explorer
10. CoT UI (chain configuration, presets)
11. RawrVoice (STT/TTS via browser APIs)
12. Mobile responsive layout
13. localStorage persistence (all 5 keys)
14. Keyboard shortcuts
15. Offline mode responses
16. Terminal (local mode — 30+ client-side commands)
17. Tensor Hop / Safe Decode UI toggles
18. File attachment (via FileReader)
19. Conversation memory management
20. Defensive function check

### Requires Live Backend
1. Chat message sending (all API strategies)
2. Model list fetching
3. MASM Model Bridge operations
4. Streaming inference
5. Terminal CLI mode (/api/cli)
6. All 30+ overlay panels that fetch from /api/* endpoints
7. File editor (all file system operations)
8. Ghost IDE session
9. DualAgent pipeline
10. Extension marketplace/install/host management
11. Browser proxy mode
12. Backend switching
13. Hotpatch apply/revert/toggle
14. Agent dashboard polling
15. Failure timeline fetching

---

## CRITICAL FUNCTIONS (from defensive check at L24563)

These are verified at page load — if ANY is missing, the page was truncated:

```
switchTerminalMode, handleTerminal, showExtensionsPanel,
checkServerStatus, showDebug, showEndpoints, showFailures,
showAgentDashboard, showPerfPanel, showSecurityPanel,
showToolsPanel, showEnginePanel, showMetricsPanel
```

Plus critical object: `RawrVoice`

---

**Total Testable Features: 55 categories, 300+ individual functions, 150+ API endpoints, 100+ DOM element IDs**
