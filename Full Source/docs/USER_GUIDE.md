# RawrXD IDE — User Guide

> **Version 7.4.0** | Native Win32 AI IDE

## Quick Start

1. **Extract** `RawrXD-IDE-v7.4.0-win64.zip` to any folder
2. **Run** `RawrXD-Win32IDE.exe`
3. The IDE opens with a VS Code-style layout: Activity Bar → Sidebar → Editor → Terminal

No installation required. No admin rights needed. No runtime dependencies beyond Windows 10/11 x64.

## System Requirements

| Requirement | Minimum | Recommended |
|-------------|---------|-------------|
| **OS** | Windows 10 x64 (1903+) | Windows 11 x64 |
| **CPU** | x64 SSE4.2 | AVX2 / AVX-512 |
| **RAM** | 4 GB | 16 GB+ (for large models) |
| **Disk** | 50 MB | 1 GB+ (models directory) |
| **GPU** | None (CPU inference) | AMD ROCm / Intel oneAPI / NVIDIA |

## Layout

```
┌───┬───────────────────────────────────────────┬───────────┐
│ A │  Tab Bar                                   │           │
│ c ├───────────────────────────────────────────┤  Chat     │
│ t │                                            │  Panel    │
│ i │  Editor Area                               │           │
│ v │  (syntax highlighted, line numbers)        │           │
│ i │                                            │           │
│ t ├───────────────────────────────────────────┤           │
│ y │  Terminal / Output Panel                   │           │
│   │                                            │           │
│ B ├───────────────────────────────────────────┴───────────┤
│ a │  Status Bar                                            │
│ r │                                                        │
└───┴────────────────────────────────────────────────────────┘
```

## Core Features

### 1. AI Chat & Inference

The IDE connects to a local Ollama instance (or compatible API) for AI assistance.

**Setup:**
1. Install [Ollama](https://ollama.ai) and pull a model: `ollama pull llama3`
2. Ollama runs on `localhost:11434` by default — the IDE auto-connects

**Usage:**
- Open the Chat Panel from the Activity Bar or press the chat icon
- Type your question and press Enter
- The IDE streams responses in real-time

**Configuration** (`config/settings.json`):
```json
{
    "OllamaHost": "http://localhost:11434",
    "OllamaModel": "llama3"
}
```

### 2. Chain-of-Thought (CoT) Review

Multi-model sequential analysis with cumulative context. Each step sees the original query plus all prior outputs.

**Presets:**
| Preset | Steps | Use Case |
|--------|-------|----------|
| `review` | Reviewer → Critic → Synthesizer | Code review |
| `audit` | Auditor → Verifier → Summarizer | Security audit |
| `think` | Brainstorm → Thinker → Refiner → Synthesizer | Deep analysis |
| `research` | Researcher → Thinker → Verifier → Synthesizer | Research task |
| `debate` | Debater For → Debater Against → Synthesizer | Balanced analysis |

**CLI Usage:**
```
!cot preset review
!cot run "Analyze this buffer overflow in function X"
```

**HTTP API:**
```bash
# Apply a preset
curl -X POST http://localhost:11434/api/cot/preset -d '{"preset":"review"}'

# Execute chain
curl -X POST http://localhost:11434/api/cot/execute -d '{"query":"Analyze this code"}'
```

**HTML Frontend:**
Open `gui/ide_chatbot.html` in the embedded browser for a rich CoT UI with role icons and step-by-step visualization.

### 3. Voice Chat

Native Win32 voice interface with push-to-talk and voice activity detection.

**Shortcuts:**
| Key | Action |
|-----|--------|
| `Ctrl+Shift+V` | Toggle Voice Panel |
| Hold `Space` | Push-to-Talk (when voice panel focused) |

**Features:**
- Quad-buffered `waveIn`/`waveOut` for low-latency capture
- RMS-based Voice Activity Detection (VAD)
- Speech-to-Text via WinHTTP (Whisper API compatible)
- Text-to-Speech for responses
- VU meter with real-time level display

### 4. PDB Symbol Resolution

Native MSF v7.00 parser for binary analysis and reverse engineering.

**Loading a PDB:**
1. File → Open PDB (or `Ctrl+Shift+P` → "Load PDB")
2. Select the `.pdb` file
3. Symbols appear in the Sidebar under "Symbols"

**Features:**
- O(1) GSI hash table lookup
- TPI type stream parsing
- Public symbol enumeration
- GUID + Age extraction for symbol server matching

### 5. Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New File |
| `Ctrl+O` | Open File |
| `Ctrl+S` | Save |
| `Ctrl+Shift+S` | Save As |
| `Ctrl+W` | Close Tab |
| `Ctrl+F` | Find |
| `Ctrl+H` | Find & Replace |
| `Ctrl+G` | Go to Line |
| `Ctrl+B` | Build |
| `F5` | Run |
| `F9` | Debug |
| `` Ctrl+` `` | Toggle Terminal |
| `Ctrl+Shift+P` | Command Palette |
| `Ctrl+Shift+A` | Audit Dashboard |
| `Ctrl+Shift+V` | Voice Chat |
| `Ctrl+Shift+R` | CoT Review |

Shortcuts are customizable via `config/shortcuts.json`.

### 6. Three-Layer Hotpatching

Apply live patches to loaded models without restarting:

- **Memory patches:** Modify tensor values in RAM
- **Byte patches:** Edit GGUF file on disk (pattern search + replace)
- **Server patches:** Intercept and modify inference requests/responses

Access via the Hotpatch Panel in the Activity Bar.

### 7. Audit Dashboard

Built-in system health monitoring:

- `Ctrl+Shift+A` opens the Audit Dashboard
- Shows compilation unit status, feature registry, stub detection
- SLO tracking with P99 latency and error budgets
- Alert system with tray notifications for threshold breaches

## Headless Mode

Run without GUI for CI/CD or server deployments:

```powershell
RawrXD-Win32IDE.exe --headless --port 8080
```

**REPL Commands:**
```
help              Show all commands
load <path>       Load a GGUF model
ask <query>       Send inference request
cot preset review Apply CoT preset
cot run <query>   Execute CoT chain
server start      Start HTTP server
server stop       Stop HTTP server
status            Show system status
quit              Exit
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| "VCRUNTIME140.dll not found" | Install [VC Redistributable 2022](https://aka.ms/vs/17/release/vc_redist.x64.exe) |
| Ollama not connecting | Ensure Ollama is running: `ollama serve` |
| AVX-512 crash | Run on AVX2 fallback (automatic detection) |
| Voice chat no audio | Check Windows Sound settings, ensure microphone permissions |
| Build failed | Re-run with `cmake --build build --config Release` after fixing errors |

## File Structure

```
RawrXD-IDE-v7.4.0-win64/
├── RawrXD-Win32IDE.exe    (3.73 MB — the entire IDE)
├── MANIFEST.json          (release metadata + SHA256 hashes)
├── LICENSE                (MIT)
├── README.md
├── RawrXD.ico
├── config/
│   ├── settings.json      (AI backend, features)
│   ├── shortcuts.json     (keyboard bindings)
│   └── security.json      (rate limits, TLS, input validation)
├── gui/
│   └── ide_chatbot.html   (CoT frontend)
├── models/                (user-populated GGUF files)
├── symbols/               (PDB cache)
├── plugins/               (VSIX extensions)
└── docs/
    ├── ARCHITECTURE.md
    ├── USER_GUIDE.md
    └── API.md
```
