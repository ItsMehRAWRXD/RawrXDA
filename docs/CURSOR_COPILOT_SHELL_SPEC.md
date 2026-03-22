# Cursor / GitHub Copilot–Style Shell — Reverse-Engineering Spec

**Product:** RawrXD `bigdaddyg-ide` (Electron + React)  
**Goal:** Surface parity with **Command Palette**, **Settings**, **Chat**, **Agent**, **Modules** (extension-like toggles), aligned with VS Code / Cursor / Copilot UX patterns.

---

## 1. Behavioral model (what we reverse-engineered)

| Surface | Cursor / VS Code pattern | RawrXD implementation |
|---------|-------------------------|------------------------|
| **Command Palette** | Fuzzy list, Enter to run, Esc dismiss, `Ctrl+Shift+P` | `CommandPalette.js` + `IdeFeaturesContext` |
| **Settings** | Tabbed modal, `Ctrl+,` | `SettingsPanel.js` — General, AI, Copilot/Cursor, **Noise & feedback**, Keyboard |
| **Chat** | Side panel, model-backed, file context | `ChatPanel.js` → `electronAPI.invokeAI` + active file snippet |
| **Agent** | Autonomous multi-step task | Existing `AgentPanel.js` in right dock |
| **Modules** | Enable/disable capabilities (extensions-lite) | `ModulesPanel.js` + `localStorage` via `IdeFeaturesContext` |
| **Toolbar** | Quick access + provider switch | `Toolbar.js` — Palette, Settings, Chat, Agent, Modules, AI provider |

---

## 2. Keyboard map

| Shortcut | Action |
|----------|--------|
| `Ctrl+Shift+P` / `Cmd+Shift+P` | Open command palette |
| `Ctrl+,` / `Cmd+,` | Open settings |
| `Ctrl+L` / `Cmd+L` | Open AI chat dock |
| `Ctrl+Shift+A` | Toggle agent dock |
| `Ctrl+Shift+M` | Toggle modules dock |

Menu: **View** → Command Palette, Settings, AI Chat, Agent Panel, Modules (same accelerators).

---

## 3. Persistence

- **Key:** `rawrxd.ide.shell.v1` in `localStorage`
- **Payload:** `{ settings, modules }`
- **Settings:** temperature, maxTokens, copilotInlineHints, agentAutoApprove, attachActiveFileToChat, chatPersona, **uiSounds**, **noisyToasts**, **noisyConsole**, **noiseIntensity** (`normal` \| `maximum`)
- **Modules:** gitIntegration, testRunner, lspBridge, inlineCompletion, commandPalette, telemetryOptIn

---

## 4. IPC

- **Preload:** `onIdeAction(callback)` — listens for `ide:action` from main menu
- **Main menu:** `webContents.send('ide:action', 'command-palette' | 'settings' | 'chat' | 'agent' | 'modules')`

---

## 5. Win32 IDE (`src/win32app`)

The native Win32 IDE already ships a large **command registry** in `Win32IDE::buildCommandRegistry()` (`Win32IDE_Commands.cpp`) including Agent, View → AI Chat, Settings GUI, etc. This Electron shell is the **parallel** Cursor/Copilot UX for the web-tech IDE track.

---

## 6. Noisy shell (done)

1. **`noisyAudio.js`** — Web Audio presets: palette, dock, chat, success/error, **fanfare** in maximum mode.
2. **`NoisyLayer.js`** — Toast stack + fixed status bar; maximum mode adds gradient bar, marquee text, flashier toasts.
3. **`IdeFeaturesContext`** — `pushToast`, `setStatusLine`, `playUiSound`, `noisyLog`, `celebrate`; dock/palette/settings edge-triggered feedback in `App.js`.
4. **`EditorPanel`** — Monaco `registerInlineCompletionsProvider` when `copilotInlineHints` + `inlineCompletion` module; extra suggestions in **maximum** mode.
5. **`providers.js`** — Ollama `options.temperature` and `num_predict` from IPC `context` (`temperature`, `maxTokens`).
6. **`CommandPalette` / `ChatPanel`** — Sounds + status + toasts (gated by settings).

## 7. Next wiring (optional)

1. **Agent auto-approve:** Pass `settings.agentAutoApprove` into `AgentOrchestrator` when implemented.

---

**Files added/updated (bigdaddyg-ide):**

- `src/contexts/IdeFeaturesContext.js`
- `src/utils/noisyAudio.js`
- `src/components/NoisyLayer.js`
- `src/components/CommandPalette.js`
- `src/components/SettingsPanel.js`
- `src/components/ChatPanel.js`
- `src/components/EditorPanel.js`
- `src/components/ModulesPanel.js`
- `src/components/RightSidebarDock.js`
- `src/agentic/providers.js`
- `src/App.js`, `src/components/Toolbar.js`
- `electron/preload.js`, `electron/menu.js`
