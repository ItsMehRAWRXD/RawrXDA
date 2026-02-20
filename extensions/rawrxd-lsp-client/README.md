# RawrXD LSP Client — VS Code Extension

VS Code client extension for the **RawrXD LSP Server**. Provides full IDE integration for the three-layer hotpatch system and GGUF model operations via 9 custom JSON-RPC methods.

## Features

### Hotpatch Management
| Command | Method | Description |
|---------|--------|-------------|
| `RawrXD: List Active Hotpatches` | `rawrxd/hotpatch/list` | Enumerate patches across Memory/Byte/Server layers |
| `RawrXD: Apply Hotpatch` | `rawrxd/hotpatch/apply` | Apply a patch via UnifiedHotpatchManager |
| `RawrXD: Revert Hotpatch` | `rawrxd/hotpatch/revert` | Revert a previously applied patch |
| `RawrXD: Hotpatch Diagnostics` | `rawrxd/hotpatch/diagnostics` | View conflict/validation diagnostics |

### GGUF Model Operations
| Command | Method | Description |
|---------|--------|-------------|
| `RawrXD: GGUF Model Info` | `rawrxd/gguf/modelInfo` | Query architecture, quant type, parameter count |
| `RawrXD: GGUF Tensor List` | `rawrxd/gguf/tensorList` | Enumerate tensors with searchable quick pick |
| `RawrXD: Validate GGUF File` | `rawrxd/gguf/validate` | Structural validation (magic, version, tensor integrity) |

### Workspace Intelligence
| Command | Method | Description |
|---------|--------|-------------|
| `RawrXD: Workspace Symbols` | `rawrxd/workspace/symbols` | ASM-accelerated symbol lookup (FNV-1a + binary search) |
| `RawrXD: Workspace Stats` | `rawrxd/workspace/stats` | Hotpatch manager statistics dashboard |

### Server Notifications (Reactive)
| Notification | Description |
|-------------|-------------|
| `rawrxd/hotpatch/event` | Hotpatch applied/reverted/failed/conflicted |
| `rawrxd/gguf/loadProgress` | Model loading progress with percentage |
| `rawrxd/diagnostics/refresh` | Server-pushed diagnostic refresh |

## Sidebar Views

The extension adds a **RawrXD** activity bar with three views:
- **Hotpatches** — Live list of active patches with layer/kind indicators
- **GGUF Models** — Loaded model metadata at a glance
- **Statistics** — Real-time hotpatch manager metrics

## Configuration

| Setting | Default | Description |
|---------|---------|-------------|
| `rawrxd.server.path` | (auto-detect) | Path to `RawrXD_IDE_unified.exe` |
| `rawrxd.server.args` | `["--lsp"]` | Server launch arguments |
| `rawrxd.trace.server` | `off` | JSON-RPC trace level (`off`, `messages`, `verbose`) |
| `rawrxd.diagnostics.autoRefresh` | `true` | Auto-refresh diagnostics on hotpatch events |
| `rawrxd.diagnostics.showInline` | `true` | Show hotpatch diagnostics inline in editor |
| `rawrxd.gguf.autoValidate` | `true` | Validate GGUF files on open/change |

## Architecture

```
VS Code Extension (TypeScript)
  │
  ├── LanguageClient (vscode-languageclient)
  │     │
  │     └── stdio transport ──→ RawrXD_IDE_unified.exe --lsp
  │                                    │
  │                                    ├── RawrXD_LSPServer (C++)
  │                                    │     └── LSPHotpatchBridge
  │                                    │           ├── UnifiedHotpatchManager
  │                                    │           ├── GGUFDiagnosticProvider
  │                                    │           └── HotpatchSymbolProvider
  │                                    │                 └── asm_symbol_hash_lookup (MASM64)
  │                                    │
  │                                    └── Three-Layer Hotpatch System
  │                                          ├── Memory (VirtualProtect)
  │                                          ├── Byte (mmap/CreateFileMapping)
  │                                          └── Server (Request/Response transforms)
  │
  ├── HotpatchTreeProvider ──→ Sidebar: Active Patches
  ├── GGUFTreeProvider ──→ Sidebar: Model Info
  ├── StatsTreeProvider ──→ Sidebar: Manager Stats
  └── DiagnosticCollection ──→ Problems Panel
```

## Building

```bash
cd extensions/rawrxd-lsp-client
npm install
npm run compile
```

## Packaging

```bash
npm run package
# Produces rawrxd-lsp-client-1.0.0.vsix
```

## Protocol Reference

Wire format mirrors the C++ structs in `src/lsp/lsp_bridge_protocol.hpp`. All custom methods use `rawrxd/` namespace prefix to avoid LSP standard method collisions.

## Requirements

- **RawrXD LSP Server** (`RawrXD_IDE_unified.exe`) built with `--lsp` support
- VS Code 1.85+
- Node.js 18+ (for development)
