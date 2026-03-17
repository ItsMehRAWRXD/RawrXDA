# RawrXD IDE - Keyboard Shortcuts Quick Reference

## Essential Shortcuts (Always Available)

### File Operations
| Shortcut | Action | Description |
|----------|--------|-------------|
| `Ctrl+N` | New File | Create new editor tab |
| `Ctrl+O` | Open File | Open file dialog (supports multiple files) |
| `Ctrl+S` | Save | Save current file |
| `Ctrl+Shift+S` | Save As | Save with new file name |
| `Ctrl+W` | Close Tab | Close current editor tab |
| `Alt+F4` | Exit | Close IDE |

### Editing
| Shortcut | Action | Description |
|----------|--------|-------------|
| `Ctrl+Z` | Undo | Undo last change |
| `Ctrl+Y` | Redo | Redo undone change |
| `Ctrl+X` | Cut | Cut selection to clipboard |
| `Ctrl+C` | Copy | Copy selection to clipboard |
| `Ctrl+V` | Paste | Paste from clipboard |
| `Ctrl+A` | Select All | Select entire document |

### Text Selection
| Shortcut | Action | Description |
|----------|--------|-------------|
| `Ctrl+L` | Select Line | Select current line |
| `Shift+Alt+Up` | Copy Line Up | Duplicate line above |
| `Shift+Alt+Down` | Copy Line Down | Duplicate line below |
| `Shift+Alt+Right` | Expand Selection | Expand selection region |

### Search & Navigation
| Shortcut | Action | Description |
|----------|--------|-------------|
| `Ctrl+F` | Find | Find text in current file |
| `Ctrl+H` | Replace | Find and replace text |
| `F3` | Find Next | Find next occurrence |
| `Shift+F3` | Find Previous | Find previous occurrence |
| `Ctrl+G` | Go to Line | Jump to line number |
| `Ctrl+P` | Go to File | Quick file opener (fuzzy search) |
| `Ctrl+Shift+O` | Go to Symbol | Jump to function/class |
| `Alt+Left` | Go Back | Navigate backwards |
| `Alt+Right` | Go Forward | Navigate forwards |

### Build & Run
| Shortcut | Action | Description |
|----------|--------|-------------|
| `Ctrl+Shift+B` | Build | Build project |
| `Ctrl+F5` | Run | Run without debugging |
| `F5` | Debug | Start debugging |
| `Shift+F5` | Stop Debug | Stop debugging |
| `F10` | Step Over | Debug: Step over function |
| `F11` | Step Into | Debug: Step into function |
| `Shift+F11` | Step Out | Debug: Step out of function |
| `F9` | Toggle Breakpoint | Add/remove breakpoint |

### Panels & Views
| Shortcut | Action | Description |
|----------|--------|-------------|
| <code>Ctrl+`</code> | Toggle Terminal | Show/hide terminal panel |
| `Ctrl+Shift+M` | Problems Panel | Show compiler/lint errors |
| `Ctrl+Shift+P` | Command Palette | Search all commands |
| `F8` | Next Problem | Jump to next error/warning |
| `Shift+F8` | Previous Problem | Jump to previous error/warning |

### Terminal Operations
| Shortcut | Action | Description |
|----------|--------|-------------|
| <code>Ctrl+`</code> | New Terminal | Create new terminal |
| `Ctrl+Shift+5` | Split Terminal | Split terminal pane |

---

## Context Menu (Right-Click in Editor)

### Standard Actions
- **Undo** (Ctrl+Z)
- **Redo** (Ctrl+Y)
- **Cut** (Ctrl+X)
- **Copy** (Ctrl+C)
- **Paste** (Ctrl+V)
- **Select All** (Ctrl+A)

### AI Assistant Submenu
- **Explain Code** - Get AI explanation of selected code
- **Fix Code** - AI identifies and fixes issues
- **Refactor Code** - AI suggests improvements
- **Generate Tests** - AI creates unit tests
- **Generate Documentation** - AI writes code comments

---

## Command Palette

### Access
- Press `Ctrl+Shift+P` to open
- Type to filter commands
- Use ↑↓ to navigate
- Press Enter to execute

### Categories
- **File**: New, Open, Save, Close operations
- **Edit**: Text editing and formatting
- **View**: Toggle panels and docks
- **AI**: Model loading and AI features
- **Settings**: Configuration and preferences
- **Agentic**: Autonomous systems and monitoring

---

## Tips & Tricks

### Multi-File Editing
1. `Ctrl+O` → Select multiple files with Ctrl+Click
2. Each file opens in a new tab
3. Switch tabs with mouse or Ctrl+Tab

### Quick AI Assistance
1. Select code snippet
2. Right-click → AI Assistant
3. Choose action (Explain/Fix/Refactor/Tests/Docs)
4. AI Chat panel opens automatically

### Search with Wrap-Around
1. `Ctrl+F` to find text
2. Search wraps from bottom to top automatically
3. Status bar shows "Found (wrapped)" when it loops

### Keyboard-Driven Workflow
1. `Ctrl+P` - Quick open file
2. `Ctrl+G` - Jump to line
3. `Ctrl+F` - Find text
4. `F3` / `Shift+F3` - Navigate results
5. `Ctrl+H` - Replace all
6. `Ctrl+S` - Save
7. `Ctrl+W` - Close

### Panel Management
- <code>Ctrl+`</code> - Terminal
- `Ctrl+Shift+M` - Problems
- All panels accessible via View menu
- Panels can be dragged and docked

---

## Advanced Features

### AI Chat Integration
All AI features require:
- Model loaded (GGUF or Ollama)
- AI Chat Panel enabled (View menu)
- Text selected in editor

### Build System
- `Ctrl+Shift+B` triggers build
- Build output in Output panel
- Errors appear in Problems panel
- Click error to jump to line

### Debugging
- `F9` to set breakpoint (red dot in margin)
- `F5` to start debugging
- `F10/F11` to step through code
- Variables in Debug Console panel

---

## Customization

### Keyboard Shortcuts
- File → Settings → Keyboard Shortcuts
- Search for command
- Click pencil icon
- Press new key combination
- Conflicts highlighted automatically

### Theme & Colors
- File → Settings → Color Theme
- Choose from Light/Dark/High Contrast
- Syntax highlighting per file type
- Terminal colors customizable

---

## Platform-Specific Notes

### Windows
- `Alt+F4` closes application
- `Ctrl` key for all shortcuts
- PowerShell default terminal

### Conflicts
- `Ctrl+P` may conflict with browser print dialog
- `F12` may trigger browser DevTools if embedded
- `Alt` shortcuts work in windowed mode only

---

## Quick Reference Card (Print-Friendly)

```
┌─────────────────────────────────────────────────┐
│           RawrXD IDE Shortcuts                  │
├─────────────────────────────────────────────────┤
│ FILE                                            │
│  Ctrl+N      New File                           │
│  Ctrl+O      Open File                          │
│  Ctrl+S      Save                               │
│  Ctrl+W      Close Tab                          │
│                                                 │
│ EDIT                                            │
│  Ctrl+Z      Undo                               │
│  Ctrl+Y      Redo                               │
│  Ctrl+X/C/V  Cut/Copy/Paste                     │
│  Ctrl+F      Find                               │
│  Ctrl+H      Replace                            │
│                                                 │
│ NAVIGATION                                      │
│  Ctrl+G      Go to Line                         │
│  Ctrl+P      Go to File                         │
│  F8          Next Problem                       │
│                                                 │
│ BUILD & RUN                                     │
│  Ctrl+Shift+B  Build                            │
│  Ctrl+F5       Run                              │
│  F5            Debug                            │
│  F9            Toggle Breakpoint                │
│                                                 │
│ PANELS                                          │
│  Ctrl+`        Terminal                         │
│  Ctrl+Shift+M  Problems                         │
│  Ctrl+Shift+P  Command Palette                  │
└─────────────────────────────────────────────────┘
```

---

## Getting Help

- **Command Palette**: `Ctrl+Shift+P` → Type "help"
- **AI Chat**: Ask questions directly in AI panel
- **Context Menu**: Right-click for contextual actions
- **Status Bar**: Shows keyboard hints for actions
- **Menu Bar**: All commands with shortcuts listed

---

**Version**: 1.0.0
**Last Updated**: 2024
**Platform**: Windows (Qt 6.x)
