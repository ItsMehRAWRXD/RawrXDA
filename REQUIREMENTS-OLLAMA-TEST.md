# Requirements for Ollama Full Test

This document describes what you need to run the full Ollama model list and smoke test, and how the generated list is used in RawrXD.

## What you need

- **PowerShell 5.1+** — Run with `pwsh` or Windows PowerShell.
- **Ollama installed and running** — Default base URL: `http://localhost:11434`. Start with the Ollama app or `ollama serve`.
- **At least one model pulled** — e.g. `ollama pull llama3.2`.

## How to run the script

From the repo root (e.g. `d:\rawrxd`):

```powershell
.\Test-Ollama-Models-Full.ps1
```

- **Default run** — Checks connectivity (GET /api/tags), builds the full model list with original names and details, writes `OllamaAvailableModels.json` and `OllamaAvailableModels.md`, and runs a short smoke test (POST /api/generate) with the first model.

```powershell
.\Test-Ollama-Models-Full.ps1 -SmokeEachModel
```

- **Smoke every model** — Same as above, plus a short generate for each model in the list.

```powershell
.\Test-Ollama-Models-Full.ps1 -SkipGenerate
```

- **List only** — No generate step; only builds and writes the model list.

```powershell
.\Test-Ollama-Models-Full.ps1 -OllamaHost "http://127.0.0.1:11434"
```

- **Custom host** — Use a different Ollama base URL (or set `$env:OLLAMA_HOST`).

## Exit codes

- **0** — All steps passed (connectivity, at least one model, list written, and smoke test if not skipped).
- **1** — Ollama unreachable (e.g. connection refused), no models in /api/tags, or a step failed.

## Names in the generated list

The names in `OllamaAvailableModels.json` and `OllamaAvailableModels.md` are the **original model names** from Ollama’s `/api/tags` (e.g. `llama3.2`, `mistral:7b`). Use these exact names for:

- **RawrXD model selection** (e.g. in the IDE or agentic config).
- **`!model_ollama`** and backend selection when using Ollama.
- **POST /api/chat** and **POST /api/agent/wish** with the optional `model` field to route that request to Ollama.

The complete_server also exposes **GET /v1/models** and **GET /api/models**, which merge local GGUF model IDs with live Ollama models (when Ollama is reachable) and well-known fallback IDs. Cursor Settings > Models can use that endpoint with the RawrXD server base URL.

## Optional manual checks

- List models from CLI: `ollama list`
- List models from API: `Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -Method GET`

If the script fails with “GET /api/tags failed”, start Ollama (e.g. run the app or `ollama serve`), ensure at least one model is pulled (e.g. `ollama pull llama3.2`), then run the script again.
