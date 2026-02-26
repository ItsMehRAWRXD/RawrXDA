# RawrXD Autonomous IDE Agent - TODO Roadmap

## 🚀 PHASE 1: CORE INFRASTRUCTURE (HOUR 1)
- [x] Create `RawrXD_AutonomousCore.hpp` (Task Orchestration Engine)
- [x] Create `RawrXD_TerminalIntegration.hpp` (Real Win32 Pipes)
- [x] Create `RawrXD_MultiFileEditor.hpp` (Diff/Edit Engine)
- [x] Create `RawrXD_MainIntegration.cpp` (The Glue)
- [ ] Implement robust JSON parsing in `AutonomousOrchestrator::ParsePlanFromJSON`
- [ ] Connect `LLMClient` to a real inference backend (Ollama/OpenAI/GGUF)

## 🛠️ PHASE 2: TERMINAL & TOOL IMPROVEMENTS (HOUR 1.5)
- [ ] Enhance recursive directory scanning in `FileSystemTools`
- [ ] Add non-blocking output reading to `TerminalIntegration`
- [ ] Implement `get_diagnostics` tool to scrape MSVC/Clang build logs
- [ ] Create `search_code` tool using specialized regex or AST parsing

## 🎨 PHASE 3: INTERACTIVE AGENT UI (HOUR 2)
- [ ] Design and implement the Win32 Diff Viewer Resource Template
- [ ] Add side-by-side highlighting for added/removed lines
- [ ] Implement "Apply" and "Revert" transaction logic in the Editor
- [ ] Add a notification system for agent status (Planning, Executing, Healing)

## 🔥 PHASE 4: AUTONOMOUS BEHAVIORS (BEYOND)
- [ ] Implement the "Self-Healing" loop:
    - [ ] Detect compilation errors
    - [ ] Extract error line and context
    - [ ] Generate corrective edits
    - [ ] Verify fix by re-running build
- [ ] Implement "Context Awareness":
    - [ ] Automatically include relevant headers and class definitions in prompts
    - [ ] Track recent files and dependencies
- [ ] Performance Optimization:
    - [ ] Parallelize non-conflicting edits
    - [ ] Cache LLM planning steps for similar goals

---

## 📅 CURRENT SPRINT: 2-Hour Autonomous IDE Kickstart
**Goal**: Go from "framework ready" to a functional self-healing agent.

**Status**: 
- Infrastructure: 100% (Skeleton & Integration Files Created)
- Wiring: 25% (Basic glue in place)
- Self-Healing: 0% (Planned)
