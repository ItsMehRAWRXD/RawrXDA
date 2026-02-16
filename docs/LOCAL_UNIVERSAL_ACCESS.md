# RawrXD Universal Access (Local-Only)

This guide runs the full dual-surface stack **locally** (no hosted infrastructure):

- RawrEngine API on `127.0.0.1:23959`
- Web UI on `127.0.0.1:8088`
- Optional Wine IDE wrappers for Linux/macOS

---

## 1) One-command local launch

### Linux / macOS

```bash
./wrapper/launch-local.sh --open-browser
```

### Windows (PowerShell)

```powershell
.\wrapper\launch-local.ps1 --open-browser
```

This starts:

- `Ship/RawrEngine.py` (if present)
- local static web server for `web_interface/`

The launcher emits a URL like:

`http://127.0.0.1:8088/?api=http://127.0.0.1:23959`

The `?api=...` query parameter is automatically consumed by `web_interface/index.html`.

---

## 2) Local Docker deployment (host loopback only)

```bash
docker compose -f docker-compose.local.yml up --build
```

Endpoints:

- Web: `http://127.0.0.1:8088`
- API: `http://127.0.0.1:23959/status`

All ports are bound to `127.0.0.1` only.

---

## 3) Local auth and CORS controls

`Ship/RawrEngine.py` supports environment variables:

- `RAWRXD_REQUIRE_AUTH=1`
- `RAWRXD_API_KEYS=rawrxd_standard_key1,rawrxd_standard_key2`
- `RAWRXD_CORS_ORIGINS=http://localhost,http://127.0.0.1`
- `RAWRXD_MODELS=rawrxd-local,rawrxd-plan,rawrxd-full`
- `RAWRXD_DEFAULT_MODEL=rawrxd-local`

Example:

```bash
export RAWRXD_REQUIRE_AUTH=1
export RAWRXD_API_KEYS="rawrxd_standard_demo123"
./wrapper/launch-local.sh
```

---

## 4) API compatibility provided

The local server includes these endpoints used by the web client:

- `GET /status`
- `GET /v1/models`
- `GET /api/tools`
- `POST /api/chat` (SSE streaming + non-stream)
- `POST /api/agent/wish`
- `POST /api/agentic/config`
- `POST /api/generate` (legacy compatibility)

---

## 5) Troubleshooting

- **Web says disconnected**
  - Check `http://127.0.0.1:23959/status`
  - Confirm no process is already using port `23959`

- **Wrong API URL remembered in browser**
  - Launch with `?api=http://127.0.0.1:23959` (auto-applied)
  - Or open settings in the web UI and set API URL manually

- **Need backend only**
  - `./wrapper/launch-local.sh --backend-only`
  - `.\wrapper\launch-local.ps1 --backend-only`

