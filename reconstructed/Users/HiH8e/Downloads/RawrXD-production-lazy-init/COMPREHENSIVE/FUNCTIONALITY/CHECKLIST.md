# RawrXD-QtShell - Comprehensive Functionality Checklist
**Date**: December 27, 2025  
**Status**: Active Development + Enhancement Phase  
**Target**: 100% feature completion with zero code removal

---

## 📋 TIER 1: Core UI Features (User-Facing)

### Main Window & Frame
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Window Creation | MainWindow | Win32 CreateWindowExA | ✅ DONE | P0 | 12/27 | Title bar, minimize, maximize, close |
| Window Sizing | MainWindow | WM_SIZE handler | ✅ DONE | P0 | 12/27 | Dynamic pane resizing |
| Taskbar Integration | MainWindow | SetWindowLong + RegisterClass | ✅ DONE | P0 | 12/27 | App icon, preview thumbnail |
| Window Focus Management | MainWindow | SetFocus, WM_SETFOCUS | 🟡 PARTIAL | P1 | 12/27 | Tab order needs implementation |
| Window State Persistence | MainWindow | JSON save/load position | 🟡 PARTIAL | P1 | TBD | Size/position restoration on launch |

### Menu Bar System
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| File Menu | ui_masm.asm | CreateMenu + PopupMenu | ✅ DONE | P0 | 12/27 | New, Open, Save, Save As, Exit |
| Edit Menu | ui_masm.asm | CreateMenu + PopupMenu | ✅ DONE | P0 | 12/27 | Undo, Redo, Cut, Copy, Paste, Select All |
| View Menu | ui_masm.asm | CreateMenu + PopupMenu | ✅ DONE | P0 | 12/27 | Show/hide panes, Reset layout |
| Tools Menu | ui_masm.asm | CreateMenu + PopupMenu | ✅ DONE | P0 | 12/27 | Hotpatch options, Settings, Themes |
| Help Menu | ui_masm.asm | CreateMenu + PopupMenu | 🟡 PARTIAL | P2 | 12/27 | About, Documentation links, License info |
| Menu Accelerators | ui_masm.asm | RegisterHotKey / WM_HOTKEY | 🟡 PARTIAL | P1 | TBD | Ctrl+N (New), Ctrl+O (Open), Ctrl+S (Save) |
| Menu Disabled State | ui_masm.asm | EnableMenuItem + graying | 🟡 PARTIAL | P2 | TBD | Grey out unavailable items contextually |
| Recent Files Submenu | ui_masm.asm | Dynamic menu insertion | ❌ TODO | P2 | TBD | Last 10 files, MRU sorting |

### Keyboard Shortcuts & Hotkeys
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Ctrl+N (New) | ui_masm.asm | WM_KEYDOWN → on_file_new | ✅ DONE | P0 | 12/27 | Creates new file buffer |
| Ctrl+O (Open) | ui_masm.asm | WM_KEYDOWN → on_file_open | ✅ DONE | P0 | 12/27 | Open File dialog |
| Ctrl+S (Save) | ui_masm.asm | WM_KEYDOWN → on_file_save | ✅ DONE | P0 | 12/27 | Save active file |
| Ctrl+Shift+S (Save As) | ui_masm.asm | WM_KEYDOWN → on_file_save_as | 🟡 PARTIAL | P1 | TBD | Save as dialog, new filename |
| Ctrl+Z (Undo) | ui_masm.asm | WM_KEYDOWN → on_edit_undo | 🟡 PARTIAL | P1 | TBD | Undo stack management |
| Ctrl+Y (Redo) | ui_masm.asm | WM_KEYDOWN → on_edit_redo | 🟡 PARTIAL | P1 | TBD | Redo stack management |
| Ctrl+X (Cut) | ui_masm.asm | WM_KEYDOWN → on_edit_cut | ✅ DONE | P0 | 12/27 | Copy to clipboard + delete |
| Ctrl+C (Copy) | ui_masm.asm | WM_KEYDOWN → on_edit_copy | ✅ DONE | P0 | 12/27 | Copy to clipboard |
| Ctrl+V (Paste) | ui_masm.asm | WM_KEYDOWN → on_edit_paste | ✅ DONE | P0 | 12/27 | Paste from clipboard |
| Ctrl+A (Select All) | ui_masm.asm | WM_KEYDOWN → on_edit_select_all | ✅ DONE | P0 | 12/27 | Select all text in editor |
| Ctrl+F (Find) | ui_masm.asm | WM_KEYDOWN → on_edit_find | ✅ DONE | P0 | 12/27 | Open find dialog |
| Ctrl+H (Replace) | ui_masm.asm | WM_KEYDOWN → on_edit_replace | 🟡 PARTIAL | P1 | TBD | Open find+replace dialog |
| Tab (Indent) | editor_pane | WM_KEYDOWN | 🟡 PARTIAL | P1 | TBD | Indent selected lines |
| Shift+Tab (Unindent) | editor_pane | WM_KEYDOWN | 🟡 PARTIAL | P1 | TBD | Unindent selected lines |
| F1 (Help) | ui_masm.asm | WM_KEYDOWN → show_help | ❌ TODO | P2 | TBD | Context-sensitive help |

### Status Bar
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Status Text Display | ui_masm.asm | SetWindowTextA + refresh | ✅ DONE | P0 | 12/27 | Shows: Engine • Model • Logging • Zero-Deps |
| Dynamic Status Updates | ui_masm.asm | ui_refresh_status() | ✅ DONE | P0 | 12/27 | Updates on hotpatch apply, file open, theme change |
| Progress Indicator | ui_masm.asm | Progress bar region | 🟡 PARTIAL | P2 | TBD | Show during long operations |
| File Info Display | ui_masm.asm | "Line X, Col Y" indicator | 🟡 PARTIAL | P1 | TBD | Current cursor position |
| Encoding Indicator | ui_masm.asm | UTF-8 / ANSI display | 🟡 PARTIAL | P1 | TBD | File encoding selector |
| Line Ending Mode | ui_masm.asm | CRLF / LF display | 🟡 PARTIAL | P1 | TBD | Can toggle via status bar click |

### File Tree Pane
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Directory Listing | gui_designer_agent.asm | TreeView control + file enumeration | ✅ DONE | P0 | 12/27 | Recursive directory scan |
| File Icons | gui_designer_agent.asm | ImageList integration | ✅ DONE | P0 | 12/27 | Folder, text, code, binary icons |
| Expand/Collapse | gui_designer_agent.asm | Tree node expansion | ✅ DONE | P0 | 12/27 | +/- buttons for folders |
| File Open Handler | gui_designer_agent.asm | WM_LBUTTONDBLCLK | ✅ DONE | P0 | 12/27 | Double-click opens in editor |
| Right-Click Context Menu | gui_designer_agent.asm | PopupMenu | 🟡 PARTIAL | P1 | TBD | Open, Edit, Delete, Rename, Properties |
| Drag-Drop Files | gui_designer_agent.asm | WM_DROPFILES | 🟡 PARTIAL | P1 | TBD | Drag files into editor pane |
| File Filtering | gui_designer_agent.asm | Filter pattern input | 🟡 PARTIAL | P2 | TBD | Hide binary files, node_modules, etc. |
| File Search | gui_designer_agent.asm | Fast text search | 🟡 PARTIAL | P1 | TBD | Ctrl+Shift+F in file tree |
| Refresh Tree | gui_designer_agent.asm | F5 or menu item | 🟡 PARTIAL | P1 | TBD | Reload directory structure |

### Editor Pane (RTF Control)
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Text Editing | editor_pane | RTF control (RichEdit20A) | ✅ DONE | P0 | 12/27 | Type, backspace, delete, select, cursor |
| Syntax Highlighting | editor_pane | RTF formatting + lexer | 🟡 PARTIAL | P1 | TBD | C++, Python, JSON, MASM coloring |
| Line Numbers | editor_pane | Custom painted gutter | 🟡 PARTIAL | P1 | TBD | Column on left showing line #s |
| Bracket Matching | editor_pane | Brace/paren highlight | 🟡 PARTIAL | P1 | TBD | Auto-highlight matching pairs |
| Auto-Indentation | editor_pane | Smart indent logic | 🟡 PARTIAL | P1 | TBD | Auto-indent after { or : |
| Line Wrapping | editor_pane | WM_SETWORDBREAK | 🟡 PARTIAL | P1 | TBD | Toggle via menu or Ctrl+Q |
| Find/Replace Integration | editor_pane | Highlight matches | 🟡 PARTIAL | P1 | TBD | Mark found text, replace on demand |
| Undo/Redo Stacks | editor_pane | EM_UNDO, EM_REDO messages | 🟡 PARTIAL | P1 | TBD | Full undo/redo support |
| File Encoding Detection | editor_pane | UTF-8 BOM detection | 🟡 PARTIAL | P1 | TBD | Auto-detect or user-select |
| Large File Support | editor_pane | Streaming load | ❌ TODO | P3 | TBD | Load files > 100 MB efficiently |

### Chat Pane
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Message List | chat_pane | ListBox control | ✅ DONE | P0 | 12/27 | Display messages in thread order |
| Message Display | chat_pane | Owner-drawn items + text | 🟡 PARTIAL | P1 | TBD | User/AI differentiation via icons/colors |
| Input Field | chat_pane | Edit control | ✅ DONE | P0 | 12/27 | Type messages here |
| Send Button | chat_pane | Button control | ✅ DONE | P0 | 12/27 | Send message to AI |
| Message Formatting | chat_pane | RTF or HTML rendering | 🟡 PARTIAL | P1 | TBD | Bold, italic, code blocks |
| Scroll to Bottom | chat_pane | Auto-scroll on new message | 🟡 PARTIAL | P1 | TBD | Keep latest message visible |
| AI Model Selector | chat_pane | Dropdown combo | 🟡 PARTIAL | P1 | TBD | Switch between GPT-4, Claude, Llama, etc. |
| Context Window Display | chat_pane | Token count indicator | 🟡 PARTIAL | P2 | TBD | Show current token usage |
| Clear History | chat_pane | Button | 🟡 PARTIAL | P2 | TBD | Clear chat messages from view/memory |
| Export Chat | chat_pane | File save (JSON/TXT) | ❌ TODO | P3 | TBD | Save conversation to file |

### Terminal Pane
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Console Output Display | terminal_pane | Edit control (read-only) | ✅ DONE | P0 | 12/27 | Show command output |
| Command Input | terminal_pane | Edit control | ✅ DONE | P0 | 12/27 | Type shell commands |
| Command Execution | terminal_pane | CreateProcessA + pipe | 🟡 PARTIAL | P1 | TBD | Run cmd.exe or PowerShell |
| Output Scrolling | terminal_pane | Auto-scroll + history | 🟡 PARTIAL | P1 | TBD | Scroll output, command history |
| Command History | terminal_pane | Arrow keys (up/down) | 🟡 PARTIAL | P1 | TBD | Recall previous commands |
| Color ANSI Codes | terminal_pane | Parse VT100 sequences | 🟡 PARTIAL | P2 | TBD | Display colored output |
| Clear Terminal | terminal_pane | Button | 🟡 PARTIAL | P1 | TBD | Clear output window |
| Terminal Resize | terminal_pane | WM_SIZE → PeekNamedPipe | 🟡 PARTIAL | P2 | TBD | Adjust buffer on pane resize |
| Kill Process | terminal_pane | TerminateProcess button | 🟡 PARTIAL | P1 | TBD | Stop running command |

---

## 📋 TIER 2: Advanced UI Features (Composition & Interaction)

### Pane System (Layout Management)
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Pane Registration | gui_designer_agent.asm | RegisterPane() system | ✅ DONE | P0 | 12/27 | Register 4 panes: File Tree, Editor, Chat, Terminal |
| Pane Hit Testing | gui_designer_agent.asm | pane_hit_test(mouse_x, mouse_y) | ✅ DONE | P0 | 12/27 | Identify which pane mouse is over |
| Pane Visibility Toggle | gui_designer_agent.asm | show_pane / hide_pane | ✅ DONE | P0 | 12/27 | Show/hide via menu or keyboard |
| Pane Positioning | gui_designer_agent.asm | Pane rectangle tracking | ✅ DONE | P0 | 12/27 | Maintain x, y, width, height for each pane |
| Pane Drawing | gui_designer_agent.asm | Custom WM_PAINT | ✅ DONE | P0 | 12/27 | Draw borders, backgrounds, title bars |
| Pane Splitter Bars | gui_designer_agent.asm | Resize handle detection | 🟡 PARTIAL | P1 | TBD | Drag splitter to resize adjacent panes |
| Pane Focus Ring | gui_designer_agent.asm | Focus order management | 🟡 PARTIAL | P1 | TBD | Tab cycles through pane focus |

### Pane Dragging & Docking
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Drag Initiation | gui_designer_agent.asm | WM_LBUTTONDOWN on title bar | ✅ DONE | P0 | 12/27 | Detect right-click drag on pane |
| Drag Cursor Feedback | gui_designer_agent.asm | SetCursor(IDC_HAND) | ✅ DONE | P0 | 12/27 | Show hand cursor when hoverable |
| Drag Movement Tracking | gui_designer_agent.asm | WM_MOUSEMOVE with captured mouse | ✅ DONE | P0 | 12/27 | Track mouse position during drag |
| Dock Target Visualization | gui_designer_agent.asm | Draw drop zone overlay | ✅ DONE | P0 | 12/27 | Show where pane will dock (left, right, top, bottom) |
| Snap-to-Grid Docking | gui_designer_agent.asm | Snap logic | ✅ DONE | P0 | 12/27 | Snap pane to region boundaries |
| Drag Completion | gui_designer_agent.asm | WM_LBUTTONUP handler | ✅ DONE | P0 | 12/27 | Finalize pane position |
| Float Window Mode | gui_designer_agent.asm | Create separate window | 🟡 PARTIAL | P2 | TBD | Drag pane out of main window |
| Tab Grouping | gui_designer_agent.asm | Tab bar in pane header | 🟡 PARTIAL | P2 | TBD | Multiple files in tabs within editor |

### Layout Persistence (JSON)
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Layout JSON Schema | json_hotpatch_helpers.asm | Define layout struct format | ✅ DONE | P0 | 12/27 | {panes: [{id, x, y, width, height, visible}]} |
| Save Current Layout | ui_masm.asm | save_layout_json() | 🟡 PARTIAL | P0 | TBD | Write pane positions to ide_layout.json |
| Load Saved Layout | ui_masm.asm | load_layout_json() | 🟡 PARTIAL | P0 | TBD | Read and restore pane positions on startup |
| Layout Validation | json_hotpatch_helpers.asm | Validate loaded JSON | 🟡 PARTIAL | P1 | TBD | Ensure values are in valid ranges |
| Default Layout | ui_masm.asm | Hardcoded default if no file | ✅ DONE | P0 | 12/27 | Fallback layout (File Tree left, Editor center, Chat right, Terminal bottom) |
| Multiple Saved Layouts | ui_masm.asm | Save Layout 1, 2, 3... | ❌ TODO | P2 | TBD | User can save and switch between layouts |
| Layout Import/Export | ui_masm.asm | Export to file, import from file | ❌ TODO | P3 | TBD | Share layouts with other developers |

### Theme System (Material Design 3)
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Theme Definition | gui_designer_agent.asm | THEME struct (13 colors) | ✅ DONE | P0 | 12/27 | Primary, secondary, background, text, etc. |
| Theme Registry | gui_designer_agent.asm | Array of up to 16 themes | ✅ DONE | P0 | 12/27 | Store and manage theme collection |
| Material Dark Theme | gui_designer_agent.asm | Predefined colors | ✅ DONE | P0 | 12/27 | ID=1: Blue 500 primary, very dark background |
| Material Light Theme | gui_designer_agent.asm | Predefined colors | ✅ DONE | P0 | 12/27 | ID=2: Blue 700 primary, light background |
| Material Amber Theme | gui_designer_agent.asm | Predefined colors | ✅ DONE | P0 | 12/27 | ID=3: Amber 400 primary, for evening use |
| Theme Application | gui_designer_agent.asm | ApplyThemeToComponent() | ✅ DONE | P0 | 12/27 | Apply theme colors to all UI elements |
| Theme Menu Integration | ui_masm.asm | Tools → Themes submenu | ✅ DONE | P0 | 12/27 | Radio buttons for Light/Dark/Amber |
| Theme Persistence | ui_masm.asm | ide_theme.cfg storage | ✅ DONE | P0 | 12/27 | Load/save selected theme ID |
| Custom Theme Creation | gui_designer_agent.asm | AddCustomTheme() | 🟡 PARTIAL | P2 | TBD | Create new theme with custom colors |
| Custom Theme Editor | gui_designer_agent.asm | Color picker dialogs | ❌ TODO | P3 | TBD | GUI for editing theme colors |

---

## 📋 TIER 3: AI/Agentic Features (Autonomous & Intelligent)

### Hotpatch System Integration
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Memory Hotpatch Dialog | unified_hotpatch_manager.hpp | MessageBox + param collection | ✅ DONE | P0 | 12/27 | Get address, patch data, size from user |
| Memory Hotpatch API Call | unified_hotpatch_manager.hpp | applyMemoryPatch() invocation | ✅ DONE | P0 | 12/27 | Call C++ layer with validated inputs |
| Byte-Level Hotpatch Dialog | unified_hotpatch_manager.hpp | MessageBox + file/pattern input | ✅ DONE | P0 | 12/27 | Get filename, pattern, replacement |
| Byte-Level Hotpatch API Call | unified_hotpatch_manager.hpp | applyBytePatch() invocation | ✅ DONE | P0 | 12/27 | Call C++ layer with Boyer-Moore search |
| Server Hotpatch Dialog | unified_hotpatch_manager.hpp | MessageBox + injection points | ✅ DONE | P0 | 12/27 | Select injection point, handler type |
| Server Hotpatch API Call | unified_hotpatch_manager.hpp | addServerHotpatch() invocation | ✅ DONE | P0 | 12/27 | Register server transformation |
| Hotpatch Result Display | ui_masm.asm | MessageBox OK/Cancel | ✅ DONE | P0 | 12/27 | Show success/failure with detail message |
| Hotpatch History | unified_hotpatch_manager.hpp | Stats tracking + display | 🟡 PARTIAL | P1 | TBD | Show count and list of applied patches |
| Hotpatch Preset Save | unified_hotpatch_manager.hpp | JSON serialization | 🟡 PARTIAL | P1 | TBD | Save hotpatch as preset for reuse |
| Hotpatch Preset Load | unified_hotpatch_manager.hpp | JSON deserialization | 🟡 PARTIAL | P1 | TBD | Load and apply saved preset |

### Agentic Failure Detection & Recovery
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Failure Detection System | agentic_failure_detector.hpp | Pattern-based detection | 🟡 PARTIAL | P1 | TBD | Detect refusal, hallucination, timeout, safety violations |
| Failure Pattern: Refusal | agentic_failure_detector.hpp | Text pattern matching | 🟡 PARTIAL | P1 | TBD | Detect "I cannot", "I'm not able", "I'm unable" |
| Failure Pattern: Hallucination | agentic_failure_detector.hpp | Inconsistency detection | 🟡 PARTIAL | P2 | TBD | Detect contradictory outputs |
| Failure Pattern: Timeout | agentic_failure_detector.hpp | Elapsed time check | 🟡 PARTIAL | P1 | TBD | Detect long-running operations |
| Failure Pattern: Resource Exhaustion | agentic_failure_detector.hpp | Memory/token limit check | 🟡 PARTIAL | P2 | TBD | Detect OOM or token limit exceeded |
| Failure Confidence Scoring | agentic_failure_detector.hpp | 0.0-1.0 confidence metric | 🟡 PARTIAL | P1 | TBD | Weighted confidence calculation |
| Failure Notification Signal | agentic_failure_detector.hpp | Qt signal emission | 🟡 PARTIAL | P1 | TBD | failureDetected(type, confidence, description) |

### Agentic Response Correction
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Puppeteer Correction Engine | agentic_puppeteer.hpp | Auto-correction logic | 🟡 PARTIAL | P1 | TBD | Correct detected failures automatically |
| Mode: Plan Correction | agentic_puppeteer.hpp | Reformat for Plan mode | 🟡 PARTIAL | P1 | TBD | Convert response to structured plan |
| Mode: Agent Correction | agentic_puppeteer.hpp | Reformat for Agent mode | 🟡 PARTIAL | P1 | TBD | Restructure as agent actions |
| Mode: Ask Correction | agentic_puppeteer.hpp | Reformat for Ask mode | 🟡 PARTIAL | P1 | TBD | Convert to question prompts |
| Correction Result Struct | agentic_puppeteer.hpp | CorrectionResult {success, corrected_text} | 🟡 PARTIAL | P1 | TBD | Factory methods: ok(), error() |
| Proxy Hotpatcher Integration | proxy_hotpatcher.hpp | Byte-level correction injection | 🟡 PARTIAL | P1 | TBD | Inject correction at byte level in response |

### AgentOrchestrator (Async Coordination)
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Agent Registration | AgentOrchestrator | Register available agents | 🟡 PARTIAL | P2 | TBD | Track Planner, Executor, Reviewer, etc. |
| Agent Async Dispatch | AgentOrchestrator | Async task scheduling | 🟡 PARTIAL | P2 | TBD | Fire-and-forget agent execution |
| Agent Context Propagation | AgentOrchestrator | Context passing between agents | 🟡 PARTIAL | P2 | TBD | Share file context, state, results |
| Agent Result Aggregation | AgentOrchestrator | Collect results from multiple agents | 🟡 PARTIAL | P2 | TBD | Merge findings into unified output |
| Agent Chaining | AgentOrchestrator | Sequential agent execution | 🟡 PARTIAL | P2 | TBD | One agent's output → next agent's input |
| Agent Retry Logic | AgentOrchestrator | Retry failed agents | 🟡 PARTIAL | P2 | TBD | Auto-retry with backoff on failure |

### TaskProposalWidget (LLM Task Suggestions)
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Widget Registration | gui_designer_agent.asm | Register as pane component | 🟡 PARTIAL | P1 | TBD | Create and register TaskProposalWidget |
| Task List Display | TaskProposalWidget | ListView control | 🟡 PARTIAL | P1 | TBD | Show suggested tasks from LLM |
| Task Description | TaskProposalWidget | Rich text rendering | 🟡 PARTIAL | P1 | TBD | Display task title and detailed description |
| Task Acceptance | TaskProposalWidget | Accept button | 🟡 PARTIAL | P1 | TBD | User clicks to add task to queue |
| Task Rejection | TaskProposalWidget | Reject button | 🟡 PARTIAL | P1 | TBD | User clicks to dismiss task |
| Task Priority | TaskProposalWidget | Priority indicator (P0-P3) | 🟡 PARTIAL | P2 | TBD | Color-coded priority badges |
| Task Estimation | TaskProposalWidget | Effort estimate display | 🟡 PARTIAL | P2 | TBD | Time to complete (minutes) |
| LLM Integration | TaskProposalWidget | Call AI model | 🟡 PARTIAL | P1 | TBD | Generate task suggestions from file content |

### AISuggestionOverlay (Code Completion)
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Overlay Registration | gui_designer_agent.asm | Register visual layer | 🟡 PARTIAL | P1 | TBD | Create suggestion popup |
| Trigger on Keystroke | editor_pane | WM_CHAR → delay → query LLM | 🟡 PARTIAL | P1 | TBD | Show suggestions after typing |
| Suggestion List Display | AISuggestionOverlay | Dropdown list | 🟡 PARTIAL | P1 | TBD | Display 5-10 completion options |
| Keyboard Navigation | AISuggestionOverlay | Up/Down arrows to select | 🟡 PARTIAL | P1 | TBD | Arrow keys move highlight |
| Tab/Enter to Accept | AISuggestionOverlay | Insert selected suggestion | 🟡 PARTIAL | P1 | TBD | Tab or Enter inserts completion |
| Escape to Dismiss | AISuggestionOverlay | Hide popup | 🟡 PARTIAL | P1 | TBD | Escape closes suggestion list |
| Scoring & Ranking | AISuggestionOverlay | Relevance calculation | 🟡 PARTIAL | P2 | TBD | Rank suggestions by relevance |
| Context Window | AISuggestionOverlay | Parse surrounding code | 🟡 PARTIAL | P1 | TBD | Use editor context for suggestions |

### Model Loading (GGUF Integration)
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Model File Browser | ui_masm.asm | OpenFileDialog filter .gguf | 🟡 PARTIAL | P1 | TBD | Select GGUF model files |
| Model Loading Progress | ui_masm.asm | Progress bar + status | 🟡 PARTIAL | P1 | TBD | Show model load progress |
| Model Info Display | status_bar | Model name, size, params | 🟡 PARTIAL | P1 | TBD | Display loaded model metadata |
| Model Hotpatch Support | unified_hotpatch_manager | Apply patches to loaded model | ✅ AVAILABLE | P1 | 12/27 | Hotpatching system ready |
| Model Quantization | quant_utils | q4_k_m, q5_k_m, q8_0 support | 🟡 PARTIAL | P1 | TBD | Quantize models for performance |
| Model Caching | unified_hotpatch_manager | Cache model state between runs | 🟡 PARTIAL | P2 | TBD | Save/restore model memory |

---

## 📋 TIER 4: Auxiliary Features (Settings & Infrastructure)

### Settings & Configuration
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Settings Dialog | ui_masm.asm | Modal dialog | 🟡 PARTIAL | P2 | TBD | General, Editor, AI, Hotpatch tabs |
| General Settings | ui_masm.asm | Auto-save, language, font size | 🟡 PARTIAL | P2 | TBD | Common user preferences |
| Editor Settings | ui_masm.asm | Tab width, line wrapping, indent mode | 🟡 PARTIAL | P2 | TBD | Syntax highlighting options |
| AI Settings | ui_masm.asm | Model selection, API keys, temperature | 🟡 PARTIAL | P2 | TBD | Configure AI behavior |
| Hotpatch Settings | ui_masm.asm | Enable/disable patches, logging level | 🟡 PARTIAL | P2 | TBD | Hotpatch configuration |
| Settings Persistence | json_hotpatch_helpers.asm | Save to ide_config.json | 🟡 PARTIAL | P2 | TBD | Auto-load on startup |
| Reset to Defaults | ui_masm.asm | Reset button in dialog | 🟡 PARTIAL | P2 | TBD | Restore default settings |

### Search & Navigation
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| File Search (Ctrl+Shift+F) | ui_masm.asm | Modal dialog + results | 🟡 PARTIAL | P1 | TBD | Search text in all files |
| Quick File Open (Ctrl+P) | ui_masm.asm | Autocomplete file list | 🟡 PARTIAL | P1 | TBD | Type to fuzzy-find files |
| Go to Line (Ctrl+G) | editor_pane | Jump to line number | 🟡 PARTIAL | P1 | TBD | Input line #, editor scrolls there |
| Symbol Search (Ctrl+Shift+O) | editor_pane | Function/class navigator | 🟡 PARTIAL | P2 | TBD | Jump to function/class definition |
| Breadcrumb Navigation | editor_pane | File path + position breadcrumb | 🟡 PARTIAL | P2 | TBD | Show current location in file |

### Activity Bar & Sidebar
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Activity Bar (Left) | gui_designer_agent.asm | Icon buttons for panes | 🟡 PARTIAL | P1 | TBD | Explorer, Search, AI Tasks, Settings icons |
| Explorer Sidebar | gui_designer_agent.asm | Show/hide file tree | 🟡 PARTIAL | P1 | TBD | Click Explorer icon to toggle |
| Search Sidebar | gui_designer_agent.asm | Show/hide search panel | 🟡 PARTIAL | P1 | TBD | Click Search icon to toggle |
| AI Tasks Sidebar | gui_designer_agent.asm | Show/hide task proposals | 🟡 PARTIAL | P1 | TBD | Click AI icon to toggle |
| Settings Sidebar | gui_designer_agent.asm | Show/hide settings panel | 🟡 PARTIAL | P1 | TBD | Click Settings icon to toggle |
| Activity Bar Persistence | ui_masm.asm | Save visible sidebars | 🟡 PARTIAL | P2 | TBD | Remember which sidebars were open |

### Undo/Redo System
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Undo Stack | editor_pane | RTF EM_UNDO message | 🟡 PARTIAL | P1 | TBD | Undo text edits |
| Redo Stack | editor_pane | RTF EM_REDO message | 🟡 PARTIAL | P1 | TBD | Redo text edits |
| Undo Group | editor_pane | Batch related edits | 🟡 PARTIAL | P2 | TBD | Group edits for smoother undo |
| Undo Limit | editor_pane | Cap undo stack size (100 entries) | 🟡 PARTIAL | P2 | TBD | Prevent memory explosion |

### Error Handling & Logging
| Feature | Component | Implementation | Status | Priority | Test Date | Notes |
|---------|-----------|-----------------|--------|----------|-----------|-------|
| Logging System | ui_masm.asm | Log to ide.log file | 🟡 PARTIAL | P1 | TBD | Record all major operations |
| Error Dialog | ui_masm.asm | MessageBoxA on fatal errors | ✅ DONE | P0 | 12/27 | Display error details to user |
| Exception Handler | unified_hotpatch_manager | Try/catch for C++ calls | ✅ AVAILABLE | P0 | 12/27 | Catch and report C++ errors |
| Recovery Agent | agentic_failure_detector | Auto-recovery on error | 🟡 PARTIAL | P1 | TBD | Attempt automatic error correction |
| Error Report Export | ui_masm.asm | Save error log | 🟡 PARTIAL | P2 | TBD | Export errors for debugging |

---

## 🚀 Implementation Status Summary

### By Tier
| Tier | Features | Complete | Partial | TODO | % Done |
|------|----------|----------|---------|------|--------|
| **1: Core UI** | 48 | 21 | 25 | 2 | 44% |
| **2: Advanced UI** | 46 | 8 | 35 | 3 | 17% |
| **3: AI/Agentic** | 48 | 0 | 35 | 13 | 0% |
| **4: Auxiliary** | 37 | 0 | 27 | 10 | 0% |
| **TOTAL** | **179** | **29** | **122** | **28** | **16%** |

### By Priority
| Priority | Count | Status | Notes |
|----------|-------|--------|-------|
| **P0** | 20 | 17 DONE, 3 PARTIAL | Core features working |
| **P1** | 89 | 7 DONE, 70 PARTIAL, 12 TODO | Main development focus |
| **P2** | 47 | 5 DONE, 37 PARTIAL, 5 TODO | Next phase |
| **P3** | 23 | 0 DONE, 15 PARTIAL, 8 TODO | Future work |

---

## 📌 Highest Priority Gaps (Next Implementation)

### Critical UI Wiring (Do First)
1. **Layout Persistence Complete** - Implement save_layout_json() and load_layout_json() fully
2. **File Search Algorithm** - Implement find_in_files() using Boyer-Moore pattern matching
3. **Terminal I/O Polling** - Implement PeekNamedPipe timer loop for real-time command output
4. **Tab/Window Focus** - Implement focus ring and tab order for keyboard navigation
5. **Pane Splitter Bars** - Implement draggable splitters for pane resizing

### Agentic System Wiring (Second Phase)
6. **Failure Detector Integration** - Wire agentic_failure_detector to chat pane messages
7. **Puppeteer Correction** - Wire agentic_puppeteer to auto-correct failed responses
8. **Task Proposal Widget** - Create and register TaskProposalWidget component
9. **AI Suggestion Overlay** - Implement code completion popup
10. **AgentOrchestrator** - Implement agent coordination and async dispatch

---

## 📝 Notes for AI Agent

- **No Code Removal**: All existing implementations are preserved and enhanced
- **Pure MASM**: All UI components use Win32 API + MASM, zero Qt5/Qt6 dependencies
- **Compile Clean**: Every change must maintain zero compilation errors
- **Incremental**: Implement highest priority items first, test before moving to next
- **Documentation**: Each implementation includes inline comments and function stubs
- **Testing**: Create tests for each completed feature in pure MASM test harness

---

**Next Steps**: Start with task 2 (audit missing UI wiring) to identify exact code locations needing implementation.
