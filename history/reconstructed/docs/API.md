# RawrXD IDE — HTTP API Reference

> **Version 7.4.0** | Default port: `11434`

## Overview

The RawrXD IDE LocalServer exposes a REST API compatible with Ollama and OpenAI client libraries. When the IDE is running (GUI or headless mode), the server listens on `http://localhost:11434`.

All endpoints accept and return JSON unless noted otherwise. CORS headers are included for browser-based clients.

## Authentication

None required for localhost connections. The `config/security.json` file controls rate limiting.

---

## Inference Endpoints

### POST `/api/generate`

Ollama-compatible text generation.

**Request:**
```json
{
    "model": "llama3",
    "prompt": "Explain buffer overflow vulnerabilities",
    "stream": false,
    "options": {
        "temperature": 0.7,
        "top_p": 0.9,
        "num_predict": 512
    }
}
```

**Response:**
```json
{
    "model": "llama3",
    "response": "A buffer overflow occurs when...",
    "done": true,
    "total_duration": 1234567890,
    "eval_count": 128
}
```

### POST `/v1/chat/completions`

OpenAI-compatible chat completions.

**Request:**
```json
{
    "model": "llama3",
    "messages": [
        {"role": "system", "content": "You are a helpful assistant."},
        {"role": "user", "content": "What is a PDB file?"}
    ],
    "temperature": 0.7,
    "stream": false
}
```

**Response:**
```json
{
    "id": "chatcmpl-rawrxd",
    "object": "chat.completion",
    "model": "llama3",
    "choices": [{
        "index": 0,
        "message": {
            "role": "assistant",
            "content": "A PDB (Program Database) file..."
        },
        "finish_reason": "stop"
    }],
    "usage": {
        "prompt_tokens": 25,
        "completion_tokens": 128,
        "total_tokens": 153
    }
}
```

### POST `/ask`

Simple single-prompt inference.

**Request:**
```json
{
    "prompt": "What is RawrXD?",
    "model": "llama3"
}
```

**Response:**
```json
{
    "response": "RawrXD is a native Win32 IDE...",
    "model": "llama3",
    "success": true
}
```

---

## Chain-of-Thought (CoT) Endpoints

### GET `/api/cot/status`

Returns the current CoT engine state.

**Response:**
```json
{
    "running": false,
    "stepCount": 3,
    "totalExecutions": 5,
    "totalSteps": 15,
    "avgLatencyMs": 1234.5
}
```

### GET `/api/cot/presets`

Lists all available presets.

**Response:**
```json
{
    "presets": [
        {"name": "review", "label": "Code Review", "stepCount": 3},
        {"name": "audit", "label": "Security Audit", "stepCount": 3},
        {"name": "think", "label": "Deep Thinking", "stepCount": 4},
        {"name": "research", "label": "Research", "stepCount": 4},
        {"name": "debate", "label": "Debate", "stepCount": 3},
        {"name": "custom", "label": "Custom", "stepCount": 1}
    ]
}
```

### GET `/api/cot/steps`

Returns the current step chain configuration.

**Response:**
```json
{
    "steps": [
        {"index": 0, "role": "reviewer", "model": "default", "instruction": ""},
        {"index": 1, "role": "critic", "model": "default", "instruction": ""},
        {"index": 2, "role": "synthesizer", "model": "default", "instruction": ""}
    ]
}
```

### GET `/api/cot/roles`

Lists all 12 available roles.

**Response:**
```json
{
    "roles": [
        {"id": "reviewer", "name": "reviewer", "label": "Reviewer", "icon": "🔍", "instruction": "Review the following..."},
        {"id": "auditor", "name": "auditor", "label": "Auditor", "icon": "📋", "instruction": "Audit the following..."},
        {"id": "thinker", "name": "thinker", "label": "Deep Thinker", "icon": "🧠", "instruction": "Think deeply..."},
        {"id": "researcher", "name": "researcher", "label": "Researcher", "icon": "🔬", "instruction": "Research..."},
        {"id": "debater_for", "name": "debater_for", "label": "Debater (For)", "icon": "👍", "instruction": "Argue in favor..."},
        {"id": "debater_against", "name": "debater_against", "label": "Debater (Against)", "icon": "👎", "instruction": "Argue against..."},
        {"id": "critic", "name": "critic", "label": "Critic", "icon": "⚡", "instruction": "Critically analyze..."},
        {"id": "synthesizer", "name": "synthesizer", "label": "Synthesizer", "icon": "🔗", "instruction": "Synthesize..."},
        {"id": "brainstorm", "name": "brainstorm", "label": "Brainstormer", "icon": "💡", "instruction": "Brainstorm..."},
        {"id": "verifier", "name": "verifier", "label": "Verifier", "icon": "✅", "instruction": "Verify..."},
        {"id": "refiner", "name": "refiner", "label": "Refiner", "icon": "✨", "instruction": "Refine..."},
        {"id": "summarizer", "name": "summarizer", "label": "Summarizer", "icon": "📝", "instruction": "Summarize..."}
    ]
}
```

### POST `/api/cot/preset`

Apply a named preset to the current step chain.

**Request:**
```json
{
    "preset": "review"
}
```

**Response:**
```json
{
    "success": true,
    "preset": "review",
    "stepCount": 3
}
```

### POST `/api/cot/steps`

Set a custom step chain.

**Request:**
```json
{
    "steps": [
        {"role": "brainstorm", "model": "llama3"},
        {"role": "critic", "model": "llama3"},
        {"role": "refiner", "model": "llama3"},
        {"role": "summarizer", "model": "llama3"}
    ]
}
```

**Response:**
```json
{
    "success": true,
    "stepCount": 4
}
```

### POST `/api/cot/execute`

Execute the current chain against a query. This is a blocking call that returns when all steps complete.

**Request:**
```json
{
    "query": "Analyze the security implications of this function: void handle_input(char* buf) { strcpy(global_buf, buf); }"
}
```

**Response:**
```json
{
    "success": true,
    "totalLatencyMs": 4567.8,
    "stepCount": 3,
    "steps": [
        {
            "index": 0,
            "role": "reviewer",
            "model": "llama3",
            "success": true,
            "latencyMs": 1500.2,
            "tokenCount": 256,
            "output": "This function has a critical buffer overflow..."
        },
        {
            "index": 1,
            "role": "critic",
            "model": "llama3",
            "success": true,
            "latencyMs": 1800.1,
            "tokenCount": 312,
            "output": "The reviewer correctly identified the strcpy issue..."
        },
        {
            "index": 2,
            "role": "synthesizer",
            "model": "llama3",
            "success": true,
            "latencyMs": 1267.5,
            "tokenCount": 198,
            "output": "SUMMARY: Critical buffer overflow vulnerability found..."
        }
    ],
    "finalOutput": "SUMMARY: Critical buffer overflow vulnerability found..."
}
```

### POST `/api/cot/cancel`

Cancel a running CoT chain.

**Response:**
```json
{
    "success": true,
    "message": "Chain cancelled"
}
```

---

## Voice Chat Endpoints

### POST `/voice/start`

Start voice capture.

### POST `/voice/stop`

Stop voice capture and process STT.

### GET `/voice/status`

Get voice engine status (recording, VAD level, device info).

### POST `/voice/tts`

Text-to-speech synthesis.

**Request:**
```json
{
    "text": "Hello, this is a test"
}
```

---

## System Endpoints

### GET `/api/status`

System health and feature status.

**Response:**
```json
{
    "version": "7.4.0",
    "uptime": 3600,
    "serverRunning": true,
    "modelLoaded": true,
    "features": {
        "inference": true,
        "cot": true,
        "voice": true,
        "pdb": true,
        "hotpatch": true
    }
}
```

### GET `/api/models`

List available models (Ollama-compatible).

### GET `/api/audit`

Run the self-audit and return results.

### GET `/api/features`

List all registered features with their implementation status.

---

## Multi-Response Endpoints

### POST `/api/multi-response`

Generate multiple responses for comparison.

**Request:**
```json
{
    "prompt": "Explain memory safety",
    "count": 3,
    "temperature": [0.3, 0.7, 1.0]
}
```

---

## Error Responses

All endpoints return errors in a consistent format:

```json
{
    "error": true,
    "message": "Description of the error",
    "code": 400
}
```

| Code | Meaning |
|------|---------|
| 400 | Bad request (missing/invalid parameters) |
| 404 | Endpoint not found |
| 429 | Rate limited |
| 500 | Internal server error |
| 503 | Model not loaded / inference unavailable |

---

## Client Examples

### cURL
```bash
# Simple inference
curl -X POST http://localhost:11434/api/generate \
  -H "Content-Type: application/json" \
  -d '{"model":"llama3","prompt":"Hello","stream":false}'

# CoT review
curl -X POST http://localhost:11434/api/cot/preset \
  -d '{"preset":"review"}'
curl -X POST http://localhost:11434/api/cot/execute \
  -d '{"query":"Review this code for security issues"}'
```

### Python
```python
import requests

# OpenAI-compatible
response = requests.post("http://localhost:11434/v1/chat/completions", json={
    "model": "llama3",
    "messages": [{"role": "user", "content": "Hello"}]
})
print(response.json()["choices"][0]["message"]["content"])
```

### JavaScript
```javascript
// Ollama-compatible
const res = await fetch("http://localhost:11434/api/generate", {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify({model: "llama3", prompt: "Hello", stream: false})
});
const data = await res.json();
console.log(data.response);
```
