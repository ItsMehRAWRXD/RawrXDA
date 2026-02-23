# Connection & Model Error Troubleshooting

This guide helps resolve "model returned an error" and connection issues across RawrXD and development tools.

---

## RawrXD IDE Chat

### Model returned an error

| Symptom | Cause | Fix |
|---------|-------|-----|
| "Cannot connect to Ollama" | Ollama not running | Run `ollama serve` or start Ollama from system tray. Or use **File > Load GGUF** for local inference. |
| "model not found" (Ollama) | Model not pulled | Run `ollama pull <model>` (e.g. `ollama pull llama3`). Or load a GGUF via **File > Load GGUF**. |
| "Native pipeline not ready" | No GGUF loaded | Use **File > Load GGUF...** to load a local model, then select **RawrXD-Native** in the model dropdown. |
| Claude/GPT "requires API key" | Missing env var | Set `ANTHROPIC_API_KEY` or `OPENAI_API_KEY` in your environment. Or use RawrXD-Native / Ollama for local. |

### Fully connected paths (no mocking)

- **RawrXD-Native (Win32)** + Load GGUF → Uses `NativeInferencePipeline` (local CPU inference)
- **Ollama models** (llama3, mistral, etc.) → Real WinHTTP to `localhost:11434`
- **RawrXD Reasoning (Alpha)** → Uses `LocalReasoningIntegration`
- **Claude / GPT** → Requires API keys; cloud HTTP integration via config

---

## Cursor IDE (when developing RawrXD)

### "The model returned an error. Try disabling MCP servers, or switch models."

This message is from **Cursor's AI chat**, not RawrXD. If you see it while using Cursor:

1. **Disable MCP servers**  
   Cursor Settings → Features → MCP → Turn off problematic servers.
2. **Switch models**  
   Cursor chat model selector → try a different model (e.g. Claude, GPT-4).
3. **Check network**  
   Cloud models need internet; local models need Ollama or similar running.

RawrXD’s MCP server (`Win32IDE_MCP.cpp`) is for the RawrXD IDE only and does not affect Cursor’s AI.

---

## Quick reference

| Path | Connection | Status |
|------|------------|--------|
| RawrXD-Native + GGUF | Native pipeline (CPU) | ✅ Real |
| Ollama (any model) | WinHTTP localhost:11434 | ✅ Real |
| RawrXD Reasoning | LocalReasoningIntegration | ✅ Real |
| Claude (Anthropic) | Requires ANTHROPIC_API_KEY | ⚠️ Config + env |
| GPT (OpenAI) | Requires OPENAI_API_KEY | ⚠️ Config + env |
