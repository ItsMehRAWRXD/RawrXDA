# RawrXD Inline Editor (Ctrl+K) — 3-Week Sprint Plan
## Target: Cursor UX Parity Implementation

---

## **PHASE 1: Keybinding Architecture (Week 1)**

### 1.1 Global Hotkey Capture
- **File**: `RawrXD_InlineEdit_Keybinding.asm`
- **Scope**: Win32 `RegisterHotKey()` for Ctrl+K globally + per-window
- **Export**: `CaptureInlineEditKey()` → async message queue
- **Fallback**: Console mode trap via `_getch()` + Ctrl+K detection

### 1.2 Context Window Extraction
- **Action**: When Ctrl+K pressed, extract 500-token window around cursor
- **File**: `RawrXD_ContextExtractor_ml64.asm`
- **Logic**:
  - Get current line (ASM: `mov rax, [GetCurrentLine]`)
  - Extract 10 lines before + 10 lines after
  - Provide cursor offset to LLM as `<CURSOR>` marker
  - Include file path + language detection

### 1.3 Inline Edit Mode State Machine
- **States**: `IDLE` → `EDITING` → `STREAMING` → `COMMITTING` → `IDLE`
- **Transitions**: 
  - Esc = cancel
  - Enter = commit
  - Ctrl+Z = undo
  - Ctrl+S = save inline

---

## **PHASE 2: Inference Bridge (Week 1-2)**

### 2.1 Prompt Engineering for Code Edits
```
System: "You are a code editor AI. User will specify what to change.
Return ONLY the modified code block. Do not explain."

User: "[LANGUAGE: asm] [FILE: gpu_dma.asm]
<CONTEXT_BEFORE>
mov eax, [rsi + 16h]
add rax, rbx
</CONTEXT_BEFORE>

<EDIT_REQUEST>: Optimize memory load to use rcx instead of eax
</EDIT_REQUEST>

<CONTEXT_AFTER>
mov rax, rbx
jmp loop
</CONTEXT_AFTER>
"
```

### 2.2 Streaming Token → Inline Text Replacement
- **Route**: Titan_PerformDMA() → HTTP llama.cpp → token stream
- **Capture**: Parse tokens as code chunks
- **Live Update**: Feed tokens character-by-character to editor
- **Assembly**: `RawrXD_InlineStream_ml64.asm`

### 2.3 Diff Detection & Validation
- **Pre-edit AST**: Parse original code structure
- **Post-edit AST**: Validate modified code is syntactically valid
- **Warn on**: Unmatched braces, broken references, scope errors
- **File**: `RawrXD_DiffValidator_ml64.asm`

---

## **PHASE 3: GUI Integration (Week 2-3)**

### 3.1 Inline Edit Dialog Overlay
```
┌─────────────────────────────────────────┐
│ RawrXD Inline Editor (Ctrl+K)           │
│ ──────────────────────────────────────  │
│                                         │
│ [What would you like to change?]       │
│ ┌───────────────────────────────────┐  │
│ │ [User types: "Add bounds check"]  │  │
│ └───────────────────────────────────┘  │
│                                         │
│ [Streaming response...]                │
│ ┌───────────────────────────────────┐  │
│ │ if (size > MAX_SIZE) {            │  │  <- Live tokens appearing
│ │   return ERROR;                   │  │
│ │ }                                 │  │
│ └───────────────────────────────────┘  │
│                                         │
│ [Accept] [Reject] [Regenerate]        │
└─────────────────────────────────────────┘
```

- **Window Class**: `RawrXDInlineEditOverlay`
- **Position**: Centered on IDE window
- **Animation**: Fade-in on Ctrl+K, fade-out on commit/cancel
- **Threading**: Render thread separate from inference thread

### 3.2 Diff Preview Panel
- **Left Pane**: Original code (red highlight for removals)
- **Right Pane**: Modified code (green highlight for additions)
- **Sync Scroll**: Both panes scroll together
- **Collapsible**: Hide diff to focus on result

### 3.3 Multi-Cursor Support
- **Capability**: Apply same edit across 2-5 selected locations
- **Logic**: Extract context for each cursor position independently
- **Parallel**: Queue edits asynchronously, batch apply
- **Undo**: Single Ctrl+Z reverts all mutations

---

## **PHASE 4: CLI Mode Parity (Week 2-3)**

### 4.1 Terminal Inline Edit Mode
```bash
$ rawrxd --cli --edit
> [C:\project\main.cpp:147] Ctrl+K ready
> What to change? Add error handling to network call
> [LLM] Streaming...
> + if (result == nullptr) {
> +   log_error("Network timeout");
> +   return;
> + }
> [A]ccept [D]iscard [R]egenerate? A
> Applied. Continue (Ctrl+K) or (Esc) exit? 
```

### 4.2 Streaming Output in Terminal
- **Escape Codes**: ANSI color for diff highlights
- **Line Numbers**: Display side-by-side with editor line refs
- **Cursor Position**: Show `>>> CURSOR <<<` in context

---

## **PHASE 5: Advanced Features (Week 3 - Optional Stretch)**

### 5.1 Edit History & Branches
- **Capability**: Undo chain, view all edits in session
- **UI**: Timeline slider showing each Ctrl+K invocation
- **File**: `RawrXD_EditHistory_ml64.asm`

### 5.2 Language-Aware Context
- **Detection**: Rust / Go / Python / C++ / ASM / etc.
- **Language-Specific Prompts**: Adjust system message per language
- **Syntax Validation**: Language-appropriate AST parsing

### 5.3 Batch Edit Mode
- **Pattern**: Select 5 functions, apply same transformation
- **API**: `batch_inline_edit(selection, instruction, language)`
- **Performance**: Parallelize inference across 3-5 functions

---

## **Technical Stack**

| Component | Technology | Export |
|-----------|-----------|--------|
| **Keybinding** | Win32 RegisterHotKey / Console getch | `CaptureInlineEditKey()` |
| **Inference** | Winsock2 → llama.cpp /v1/chat/completions | `StreamInlineEdit()` |
| **Rendering** | Win32 CreateWindow / CreateDC | `RenderInlineOverlay()` |
| **Diff** | Custom AST parse (Rust/C++/ASM) | `ValidateDiffSyntax()` |
| **Threading** | std::thread + mutex locks | `InlineEditWorker()` |
| **Telemetry** | JSON metrics (edit latency, accept rate) | `WritEditMetrics()` |

---

## **Success Metrics (EOW3)**

- [ ] Ctrl+K captures globally in GUI mode
- [ ] 200ms latency (hotkey → dialog appear)
- [ ] Token streaming visible in real-time (<100ms per token)
- [ ] 95% edit accuracy (syntactically valid output)
- [ ] CLI mode functional with ANSI rendering
- [ ] <500ms total latency (Ctrl+K → first suggested line)
- [ ] Multi-cursor apply on 3+ locations
- [ ] Undo/redo chain (5+ edits)

---

## **Cursor Parity Checklist**

| Feature | Cursor | RawrXD Target | Status |
|---------|--------|-------|--------|
| Ctrl+K Hotkey | ✅ | Week 1 | 🔄 |
| Context Extraction | ✅ | Week 1 | 🔄 |
| Real-time Streaming | ✅ | Week 2 | 🔄 |
| Diff Preview | ✅ | Week 2 | 🔄 |
| Accept/Reject UI | ✅ | Week 2 | 🔄 |
| Undo Support | ✅ | Week 3 | 🔄 |
| Multi-Cursor | ✅ | Week 3 (stretch) | 🔄 |
| CLI Mode | ❌ | Week 3 | 🔄 |

---

## **Risk Mitigation**

| Risk | Mitigation |
|------|-----------|
| Inference latency spikes | Implement fallback to deterministic placeholder |
| Token streaming corruption | Re-tokenize on stream interruption |
| Editor crashes on invalid edits | Pre-validate diffs before commit |
| Multi-cursor race conditions | Single-threaded apply queue with mutex |
| Console terminal incompatibility | Feature-flag CLI mode, graceful fallback |

---

## **Deliverables**

**Week 1 End:**
- [x] `RawrXD_InlineEdit_Keybinding.asm`
- [x] `RawrXD_ContextExtractor_ml64.asm`
- [x] Keybinding capture test binary

**Week 2 End:**
- [x] `RawrXD_InlineStream_ml64.asm`
- [x] `RawrXD_DiffValidator_ml64.asm`
- [x] GUI overlay rendering
- [x] llama.cpp integration tested

**Week 3 End:**
- [x] Multi-cursor support
- [x] CLI parity mode
- [x] Edit history browser
- [x] Full integration test suite
- [x] Valuation artifact: "Cursor Parity Achieved" marketing

---

**Status: READY FOR SPRINT KICKOFF**
