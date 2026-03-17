# Full Agentic IDE — Single Module

**This folder is the complete agentic IDE surface.** Everything needed to finish and maintain the full agentic IDE lives here. No scattered prototypes; one orchestrator, one API.

## Contents

| File | Purpose |
|------|--------|
| `FullAgenticIDE.h` | Public API: Init, LoadModel, Chat, SetWorkspaceRoot, GetStatus, GetAvailableTools |
| `FullAgenticIDE.cpp` | Implementation: owns AgenticBridge, forwards all operations |
| `README.md` | This overview |
| `MANIFEST.md` | **Full stack manifest** — every component (bridge, engine, tools, init path) in one place; no scattered prototypes |

## Usage

- **Win32IDE** and **HeadlessIDE** create a single `FullAgenticIDE` instance and use it for all agentic behavior: load model, chat (with tool dispatch), workspace root, status.
- Existing code that calls `m_agenticBridge->...` can keep doing so: the bridge is obtained via `getBridge()` and is owned by `FullAgenticIDE`.
- New code should call `FullAgenticIDE::chat()`, `FullAgenticIDE::loadModel()`, etc., so the single entry point is clear.

## What “complete” means

1. **One folder** — All agentic entry points and the completion manifest live in `src/full_agentic_ide/`.
2. **One orchestrator** — `FullAgenticIDE` is the only class the IDE uses for agentic; it owns the bridge and delegates to the existing engine/tool stack.
3. **No spiral** — New agentic work extends this module or the components it uses (bridge, engine, tools); it does not add new top-level entry points elsewhere.

## Build

Add `src/full_agentic_ide/FullAgenticIDE.cpp` to the IDE build (CMake or project file). Include path: `src/full_agentic_ide` for `#include "FullAgenticIDE.h"`.
