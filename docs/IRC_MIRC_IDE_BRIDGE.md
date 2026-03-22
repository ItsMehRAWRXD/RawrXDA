# mIRC ↔ IDE bridge (BigDaddyG Electron shell)

Drive the **Electron IDE** (`bigdaddyg-ide/`) from **mIRC** (or any IRC client): the IDE runs a small **IRC bot** that joins a channel, idles, and accepts **commands** from **your nickname only**, gated by a **shared secret**. Replies use **NOTICE** so the public channel stays quiet.

> **Security:** This is **remote control** of the machine running the IDE. Use a **private IRC server** or **VPN**, **invite-only channel**, and a **long random `sharedSecret`**. Never expose weak secrets on public networks.

---

## 1. Threat model (read first)

| Risk | Mitigation |
|------|------------|
| Anyone on IRC sends `!ide` | Wrong secret → **no reply** (no oracle). |
| Secret brute force | Long random secret; **min gap** between cmds (`commandCooldownMs`, default **900ms**) **plus** rolling cap (**15 cmds / 60s**). |
| Machine compromise | Same as RDP/SSH: network placement + secret strength. |
| NOTICE injection | Outbound chunks sanitized (`\r` `\n` `\0` stripped, length capped). |
| Oversized / malicious lines | Inbound lines **> 2048** chars dropped; buffer capped. |

---

## 2. Architecture

```
[mIRC — your nick]  ←→  [IRC server]  ←→  [Bot nick = IDE bridge]
                              ↑
                    NOTICE (and PRIVMSG) from controller only
```

- **Controller nick** = exact IRC nickname you use in mIRC (`controllerNick`). Only that nick can run commands (case-insensitive).
- **Bot nick** = separate identity for the IDE (`botNick`).
- Commands may be sent in **channel PRIVMSG** or **direct NOTICE/PRIVMSG** to the bot (same parser).

---

## 3. Quick start (~5 minutes)

1. Run a local IRCd (**InspIRCd** / **ngircd**) on `127.0.0.1:6667` or use **ZNC**.
2. In the IDE: **Settings → IRC (mIRC)** — set host, port, channel, **bot nick**, **your mIRC nick**, **shared secret (8+ chars)**, **Save**.
3. Click **Start bridge** (or enable **Auto-start** and restart the app).
4. Connect mIRC, join the same channel, match **controllerNick**.
5. In channel: `!ide <secret> ping` → you should get a **NOTICE** like `pong ok v=3 nick=…`.

---

## 4. Command format

```
!ide <sharedSecret> <subcommand> [arguments...]
```

Examples:

```text
!ide MyLongRandomSecret ping
!ide MyLongRandomSecret workspace
!ide MyLongRandomSecret tasks
!ide MyLongRandomSecret status task_123
!ide MyLongRandomSecret agent Summarize src/App.js
!ide MyLongRandomSecret approve task_123 yes
!ide MyLongRandomSecret rollback task_123
```

---

## 5. Subcommand reference (bridge v3)

| Subcommand | Action |
|------------|--------|
| `help` | Lists commands (NOTICE, multi-line). |
| `ping` | Pong + bridge nick (alive check). |
| `stats` | JSON diagnostics: host, TLS, reconnect attempt, `commandsHandled`, etc. |
| `workspace` | Current project folder or “none”. |
| `tree` | Shallow workspace file listing (same indexer as agent planning; truncated). |
| `search <query>` | Bounded **`searchWorkspace`** (min 2 chars); JSON hits, truncated. |
| `tasks` | JSON snapshot of agent tasks (truncated ~3800 chars). |
| `status <taskId>` | JSON task status (truncated). |
| `agent <goal...>` | **`agent:start`** with default orchestrator policy (`{}`). |
| `approve <taskId> yes\|no` | **`agent:approve`** (plan / step / mutation batch gates). |
| `cancel <taskId>` | **`agent:cancel`**. |
| `rollback <taskId>` | **`agent:rollback`** — LIFO snapshot restore (**`docs/AUTONOMOUS_AGENT_ELECTRON.md`**). |

---

## 6. Agent policy from IRC

IRC **`agent`** uses **`startTask(goal, {})`** — persisted **Copilot / Cursor** toggles in the renderer do **not** apply to IRC-started tasks unless you extend `main.js` to merge `IdeFeaturesContext`-like defaults from disk. For product deployments, prefer **explicit policy** in code or a small `irc-agent-policy.json` (future).

---

## 7. Configuration file

**Path:** `%APPDATA%\BigDaddyG IDE\irc-bridge.json` (exact folder = Electron **`app.getPath('userData')`**).

```json
{
  "host": "127.0.0.1",
  "port": 6667,
  "serverPassword": "",
  "channel": "##rawrxd-ide",
  "botNick": "RawrIDE_Bot",
  "controllerNick": "YourMircNick",
  "sharedSecret": "change-me-16chars-min",
  "autoStart": false
}
```

- **`sharedSecret`:** minimum **8** characters unless **`RAWRXD_IRC_ALLOW_WEAK_SECRET=1`** (lab only).
- **`autoStart`:** if `true`, main process starts the bridge after **`app.whenReady`** (logs failures to IRC log ring).

---

## 8. Environment variables

| Variable | Effect |
|----------|--------|
| `RAWRXD_IRC_ALLOW_WEAK_SECRET=1` | Allow `sharedSecret` shorter than 8 (not for production). |

---

## 9. Transport limits & keepalive

- **TCP/TLS connect timeout:** 20s (failure → reconnect schedule or error on cold start).
- **PING/PONG:** Parsed via **`irc_protocol.parsePing`** (RFC-style lines).
- **NOTICE chunk size:** **`MAX_IRC_TRAILING`** (420) with **`sanitizeForIrcTrailing`** in **`electron/irc_protocol.js`**.
- **Rate limit:** **`commandCooldownMs`** (default **900ms**) between commands **plus** max **15** cmds / rolling **60s** → NOTICE `rate limited: wait or slow down`.
- **Auto-reconnect:** optional (`reconnect`); backoff from **`reconnectBaseDelayMs`**; **`maxReconnectAttempts`** then halts (query **`stats`** over IRC).

---

## 10. TLS / SSL

Native **`tls.connect`** when **`useTls`** is true or **`port === 6697`**. **`tlsInsecure`** maps to **`rejectUnauthorized: false`** (lab only).

For strict production TLS, use a proper IRCd cert and keep **`tlsInsecure: false`**.

---

## 11. Troubleshooting

| Symptom | Check |
|---------|--------|
| **`running=false`** and **empty log** | **Start** reads **`irc-bridge.json` on disk**, not unsaved form fields — click **Save IRC config** first, then **Start** (or **Refresh status**). After the fix in main/settings, **lastLaunchError=** in the status box explains validation failures (missing nick, short secret, connection refused). |
| **Start bridge** fails immediately | `controllerNick`, 8+ char secret, host reachable, firewall. |
| Bot never joins | Server `001` handling; some nets need **`PASS`** / **`CAP`** (not implemented — use server that accepts minimal client). |
| **Nick in use** | Bridge auto-appends **`_1`**, **`_2`**, … to `botNick` on numeric **433**; or change `botNick` in settings. |
| Commands ignored | `controllerNick` must match **your** nick exactly (case-insensitive); secret must match. |
| Settings don’t persist | **Save IRC config**; path under `userData`. |
| No **Start** button effect | Preload must expose **`getIrcConfig`** … **`stopIrcBridge`** (shipped in repo `electron/preload.js`). |

---

## 12. Logs & status

- Main process: IRC lines also **`console.log`**.
- Renderer: **Settings → IRC → Refresh status** shows `running=` + last lines of the in-memory ring (**40** lines max).
- IPC: **`irc:status`** → `{ running, diagnostics, recentLog }` (`diagnostics` from **`IdeIrcBridge.getDiagnostics()`** when connected).

---

## 13. mIRC snippets

```text
; Optional: alias
/alias idebridge msg $chan !ide YOURSECRET $1-
```

Use **`/msg #channel`** or type in channel as normal.

---

## 14. Implementation map (repo)

| Piece | Path |
|-------|------|
| IRC protocol helpers (tests, chunking, parse) | `bigdaddyg-ide/electron/irc_protocol.js` |
| IRC client (TCP/TLS, reconnect, commands) | `bigdaddyg-ide/electron/irc_bridge.js` |
| IPC, config, autoStart, `before-quit`, `buildIrcCommandContext` | `bigdaddyg-ide/electron/main.js` |
| Preload API | `bigdaddyg-ide/electron/preload.js` |
| UI | `bigdaddyg-ide/src/components/SettingsPanel.js` — **IRC (mIRC)** tab |
| Protocol tests | `npm run test:irc` — `electron/irc_protocol.test.js`; **`npm run test:irc:all`** adds `scripts/irc-bridge-selfcheck.js` |
| Wiring notes | `bigdaddyg-ide/electron/WIRING_IRC_BRIDGE.md` |

**`PATCH_PRELOAD_IRC.txt`** is legacy; preload in tree already includes IRC handlers.

---

## 15. RawrXD native Win32 IDE

The **C++ Win32 IDE** does not embed this IRC client. Options:

- Run **Electron** for IRC-driven control, or  
- Add a **localhost HTTP/WebSocket** control plane in Win32 and a **separate** Node script that speaks IRC and proxies to that API (authenticate everything).

---

## Related

- **`docs/AUTONOMOUS_AGENT_ELECTRON.md`** — agent IPC, planning orchestrator, rollback.
