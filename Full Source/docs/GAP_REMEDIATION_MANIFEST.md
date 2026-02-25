# Gap Remediation Manifest — Functional Workflows

**Created**: 2026-02-16  
**Purpose**: Reverse-engineer the "bad news" gaps into functional workflows and features.  
**Branch**: cursor/final-kb-audit-ab47

---

## 1. Gap → Workflow Mapping

| Gap | Existing Infrastructure | Remediation | Status |
|-----|-------------------------|-------------|--------|
| **Inline Edit** | `AIIDEIntegration::ApplyInlineEdit`, `EditOperation`, `ReplaceSelection` | C++ wired; needs model backend. Use Ollama or External API. | C++ complete; backend via Ollama/API |
| **Real-time Streaming** | `ChatSystem::startStreamingResponse`, `makeHTTPStreamingRequest`, Ollama `makeStreamingPostRequest` | C++ streaming exists; PowerShell bridge exposes streaming. | Use `scripts/external_api_bridge.ps1 -Streaming` |
| **External APIs** | Ollama only | PowerShell `external_api_bridge.ps1` calls OpenAI/Anthropic/Claude | ✅ Implemented |
| **LSP Integration** | `LSPHotpatchBridge`, `handleLsp*`, `!lsp start`, `!lsp goto` | Full LSP client/server; CLI: `!lsp start` then `!lsp goto`. | ✅ Exists; document workflow |
| **Multi-Agent Parallel** | `AgenticAgentCoordinator` single-threaded | PowerShell parallel: `Start-Job` for multiple agents. | ✅ `multi_agent_parallel.ps1` |
| **Hardcoded Paths** | `D:\lazy init ide` in validation/RE scripts | Use `$env:LAZY_INIT_IDE_ROOT` or `Split-Path $PSScriptRoot -Parent` | ✅ Applied |
| **VS Code Ext** | Not compatible | Out of scope; architectural blocker. | N/A |

---

## 2. Functional Workflows

### 2.1 Full Assistant Stack (One Command)
```powershell
.\scripts\UNIFIED_ASSISTANT_LAUNCHER.ps1 -Mode enhanced
```
- Digests codebase if KB missing
- Starts enhanced chatbot (digested KB + manual fallback)
- Optional: `-EnableExternalAPI` for OpenAI/Claude

### 2.2 Voice + KB Stack
```powershell
.\scripts\UNIFIED_ASSISTANT_LAUNCHER.ps1 -Mode voice -EnableVoice
```

### 2.3 External Model API (OpenAI/Anthropic/Claude)
```powershell
.\scripts\external_api_bridge.ps1 -Provider openai -Prompt "Explain this code"
.\scripts\external_api_bridge.ps1 -Provider anthropic -Streaming -Prompt "Refactor this"
```

### 2.4 LSP Workflow (CLI)
```
!lsp start
!lsp goto        # at symbol
!lsp refs
!lsp hover
!lsp status
```

### 2.5 Multi-Agent Parallel (PowerShell)
```powershell
.\scripts\multi_agent_parallel.ps1 -Tasks @("Analyze src/", "Benchmark model", "Run tests") -MaxParallel 3
```

---

## 3. Files Created/Modified

| File | Purpose |
|------|---------|
| `scripts/GAP_REMEDIATION_WORKFLOW.ps1` | Master orchestration for gap remediation |
| `scripts/external_api_bridge.ps1` | OpenAI/Anthropic/Claude REST bridge |
| `scripts/UNIFIED_ASSISTANT_LAUNCHER.ps1` | Single entry for digest + chatbot/voice |
| `scripts/multi_agent_parallel.ps1` | Parallel agent execution via Start-Job |
| `VALIDATE_REVERSE_ENGINEERING.ps1` | Path fix: use env or PSScriptRoot |
| `REVERSE_ENGINEERING_MASTER.ps1` | Path fix: use env or PSScriptRoot |
| `docs/GAP_REMEDIATION_MANIFEST.md` | This manifest |

---

## 4. Usage Quick Reference

| Need | Command |
|------|---------|
| Chat with codebase knowledge | `.\scripts\ide_chatbot_enhanced.ps1 -Mode interactive` |
| Voice assistant | `.\scripts\voice_assistant.ps1 -Mode voice` |
| Digest codebase first | `.\scripts\source_digester.ps1 -Operation digest` |
| OpenAI/Claude from terminal | `.\scripts\external_api_bridge.ps1 -Provider openai -Prompt "..."` |
| Parallel agent tasks | `.\scripts\multi_agent_parallel.ps1 -Tasks @("task1","task2")` |
| Validate system | `.\VALIDATE_REVERSE_ENGINEERING.ps1` |
| Full stack launch | `.\scripts\UNIFIED_ASSISTANT_LAUNCHER.ps1 -Mode enhanced` |

---

*End of Gap Remediation Manifest*
