# Copilot Proxy (streaming)

This service proxies streaming LLM responses (NDJSON or chunked text) and exposes them as Server-Sent Events (SSE) to clients.

Basic usage (dev):

1. Ensure the auth service is running at `http://localhost:3000` and has a saved token (`/token`).
2. Configure `.env` if you want a default upstream (`COPILOT_UPSTREAM_URL`) or leave it blank and provide `upstream` in request body.

Start:

```powershell
cd services\copilot-proxy
npm install
node server.js
```

API:
- `POST /stream` (SSE) - start a streamed completion. Body: `{ upstream?: string, provider?: string, prompt: string, max_tokens?: number }`.

The proxy will fetch token from the local auth service at `http://localhost:3000/token` (dev only) unless an `Authorization` header is provided with a bearer token.
