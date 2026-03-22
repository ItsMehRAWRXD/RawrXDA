# Localhost communication map — IDE as client (reverse-engineered contract)

**Last updated:** 2026-03-21  

**Goal:** Make explicit **who listens** and **who connects** on loopback so the IDE is understood as **communicating with** local services—not **being** the Ollama/GGUF host by default. Optional **bridge/listen** modes are called out separately.

---

## 1. Design posture

| Posture | Meaning |
|--------|---------|
| **Client-first** | IDE components **outbound** `connect()` / `axios` / `WinHttp` to configured `127.0.0.1` / `localhost` services. No model runtime is required *inside* the IDE process for that path. |
| **Optional bridge (listen)** | A deliberate **server socket** inside the IDE that **re-exposes** an already-loaded engine to other tools (browser, extensions). **Off unless enabled.** |
| **Not “hosting Ollama”** | Neither Electron nor Win32 **ships** the Ollama binary. **Port 11434** is owned by **Ollama** (or a compatible server) when that backend is selected. |

**Security:** Loopback is **not** authentication. Any process on the machine can talk to `127.0.0.1` listeners—treat bridge mode as **operator-consented exposure**.

---

## 2. Port / role matrix (common defaults)

| Port (typical) | **Listener (hosts)** | **IDE consumer (client)** | Notes |
|----------------|------------------------|---------------------------|--------|
| **11434** | **Ollama** (or compatible HTTP API) | Electron **`AIProviderManager`** → `POST /api/generate` (`bigdaddyg-ide/src/agentic/providers.js`); Win32 **`Win32IDE_AgentOllamaClient`**, **`Win32IDE_BackendSwitcher`**, probes in **`main_win32.cpp`** | Use **`127.0.0.1`** in config to avoid Node **`::1`** / IPv4-only listener mismatch. Config: `config/providers.json` (Electron), backend settings (Win32). |
| **11435** | **Win32 IDE optional** — `Win32IDE_LocalServer.cpp` (“Embedded GGUF HTTP Server”) | *Other* clients (browser, curl, dual-agent) connect **in** | **Bridge mode:** exposes loaded native model via Ollama/OpenAI-shaped HTTP **only when started**. Default chosen to **avoid colliding with 11434**. |
| **3000** | **webpack-dev-server** (Create React App) in **dev** | Electron **`BrowserWindow.loadURL`** | UI assets only; **not** inference. Production: `file://` + `build/`. |
| **6667 / 6697** | **IRCd** (ngircd, InspIRCd, ZNC plaintext/TLS front) | Electron **`IdeIrcBridge`** (`electron/irc_bridge.js`) | IDE is **IRC client**; does not replace the IRC server. |
| **Varies** | LSP, debug adapters, custom tool servers | IDE or editor panels | Follow per-feature docs; same rule: **document listener vs client**. |

---

## 3. Electron shell (`bigdaddyg-ide`) — traffic summary

```
┌─────────────────┐     outbound      ┌──────────────────┐
│  Electron main  │ ────────────────► │ 127.0.0.1:11434  │  Ollama (external)
│  (providers.js) │    HTTP POST      │  (Ollama owns)   │
└─────────────────┘                   └──────────────────┘

┌─────────────────┐     outbound      ┌──────────────────┐
│  Electron main  │ ────────────────► │ IRC server       │  optional remote ctrl
│  (irc_bridge)   │    TCP/TLS       │ (you run)        │
└─────────────────┘                   └──────────────────┘

┌─────────────────┐     load UI       ┌──────────────────┐
│  BrowserWindow  │ ────────────────► │ 127.0.0.1:3000   │  CRA dev only
│                 │    HTTP GET       │ (webpack hosts)  │
└─────────────────┘                   └──────────────────┘
```

- The Electron app **does not** bind **11434** for inference. It **only** calls out.
- **Reference:** `docs/AUTONOMOUS_AGENT_ELECTRON.md`, `docs/IRC_MIRC_IDE_BRIDGE.md`.

---

## 4. Win32 IDE — traffic summary

| Mode | Client | Server (if any) |
|------|--------|------------------|
| **Backend = Ollama** | Win32 → `http://127.0.0.1:11434` (or configured) | Ollama process |
| **Backend = LocalGGUF** | In-process **BGzipXD** stack (`GGUFRunner`, etc.) — **no HTTP required** for core chat | N/A (unless you enable bridge) |
| **Local server enabled** | External tools → Win32 **listen** on configured port (default **11435**) | `Win32IDE_LocalServer.cpp` forwards to loaded model |

**Reference:** `src/win32app/Win32IDE_LocalServer.cpp` (header comments), `docs/INFERENCE_PATH_MATRIX.md`, `docs/BGZIPXD_WASM.md`.

---

## 5. Configuration knobs (quick index)

| Surface | Where to set |
|---------|----------------|
| Electron Ollama base URL | `bigdaddyg-ide/config/providers.json` → `*.url` |
| Electron dev UI origin | `RAWRXD_DEV_SERVER_HOST`, `PORT` (see `electron/main.js`) |
| IRC host/port/TLS | `userData/irc-bridge.json` + Settings → IRC |
| Win32 Ollama endpoint | Backend switcher / settings (see `Win32IDE_BackendSwitcher.cpp`, `Win32IDE_AgentOllamaClient.cpp`) |
| Win32 local HTTP bridge | Local server toggle + port (see `Win32IDE_LocalServer.cpp`, commands referencing `toggleLocalServer`) |

---

## 6. Cross-stack contract line

Electron **AI** path = **HTTP client to user-operated localhost service**.  
Win32 **primary** GGUF path = **in-process integrated runner**; **localhost HTTP** is an **optional outward bridge**, not the definition of “local AI.”

See also: **`docs/CROSS_STACK_AUTONOMOUS_CONTRACT.md`**, **`docs/INFERENCE_FACADE_CONTRACT.md`**.

---

## 7. Audit checklist (when adding a feature)

1. Does it **`listen()`** on a port? Document **default port**, **toggle**, and **who may connect**.  
2. Does it only **`connect()`** out? Document **target URL** and **config key**.  
3. Error copy: distinguish **“nothing listening”** vs **“wrong host / IPv6”** vs **“forbidden path”**.  
4. Update this table in the **same PR** if you add a new loopback dependency.
