# GGUF Server - Auto-Start HTTP API

## Overview
The GGUF Server provides an Ollama-compatible HTTP API for GGUF model inference with automatic server detection and startup.

## Features

### Auto-Start Capability
- **Port Detection**: Checks if a server is already running on the target port (default: 11434)
- **Automatic Binding**: If port is in use, tries alternative ports (11435-11443)
- **Seamless Fallback**: Uses existing server instance if already running
- **Zero Configuration**: Just call `start()` and it handles everything

### Ollama-Compatible Endpoints

#### POST /api/generate
Generate text completion from a prompt
```json
{
  "prompt": "Explain quantum computing",
  "model": "my-model"
}
```

#### POST /v1/chat/completions
OpenAI-compatible chat completions
```json
{
  "model": "gpt-4",
  "messages": [
    {"role": "system", "content": "You are helpful"},
    {"role": "user", "content": "Hello!"}
  ]
}
```

#### GET /api/tags
List loaded models
```json
{
  "models": [
    {
      "name": "model.gguf",
      "modified_at": "2025-12-02T...",
      "size": 0
    }
  ]
}
```

#### GET /health
Server health and statistics
```json
{
  "status": "ok",
  "uptime_seconds": 3600,
  "total_requests": 150,
  "successful_requests": 148,
  "failed_requests": 2,
  "tokens_generated": 15000,
  "model_loaded": true,
  "model_path": "path/to/model.gguf"
}
```

### Additional Features
- **CORS Support**: Full CORS headers for web applications
- **Statistics Tracking**: Request counts, token generation, uptime
- **Health Monitoring**: Periodic health checks every 30 seconds
- **Error Handling**: Graceful error responses with detailed messages
- **Streaming Support**: Foundation for streaming responses (future)

## Usage Example

### C++ Integration
```cpp
#include "gguf_server.hpp"
#include "inference_engine.hpp"

// Create inference engine with a model
InferenceEngine* engine = new InferenceEngine("path/to/model.gguf");

// Create and start server (auto-starts if not running)
GGUFServer* server = new GGUFServer(engine);

if (server->start(11434)) {
    qInfo() << "Server running on port" << server->port();
    
    // Server stats
    auto stats = server->getStats();
    qInfo() << "Total requests:" << stats.totalRequests;
}

// Server runs in background, handles requests automatically
```

### Client Usage (curl)
```bash
# Generate text
curl -X POST http://localhost:11434/api/generate \
  -H "Content-Type: application/json" \
  -d '{"prompt": "Write a poem about coding"}'

# Chat completion
curl -X POST http://localhost:11434/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "gpt-4",
    "messages": [
      {"role": "user", "content": "Hello!"}
    ]
  }'

# List models
curl http://localhost:11434/api/tags

# Health check
curl http://localhost:11434/health
```

### Client Usage (PowerShell)
```powershell
# Generate text
$body = @{
    prompt = "Explain AI in simple terms"
    model = "my-model"
} | ConvertTo-Json

$response = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" `
    -Method POST -Body $body -ContentType "application/json"

Write-Host $response.response

# Check health
$health = Invoke-RestMethod -Uri "http://localhost:11434/health"
Write-Host "Status: $($health.status)"
Write-Host "Uptime: $($health.uptime_seconds) seconds"
Write-Host "Requests: $($health.total_requests)"
```

### Client Usage (JavaScript)
```javascript
// Generate text
async function generateText(prompt) {
    const response = await fetch('http://localhost:11434/api/generate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ prompt: prompt })
    });
    
    const data = await response.json();
    console.log(data.response);
}

// Chat completion
async function chatCompletion(messages) {
    const response = await fetch('http://localhost:11434/v1/chat/completions', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            model: 'gpt-4',
            messages: messages
        })
    });
    
    const data = await response.json();
    return data.choices[0].message.content;
}

// Health check
async function checkHealth() {
    const response = await fetch('http://localhost:11434/health');
    const health = await response.json();
    console.log('Server status:', health.status);
    console.log('Uptime:', health.uptime_seconds, 'seconds');
}
```

## Auto-Start Behavior

### Scenario 1: No Server Running
```cpp
server->start(11434);
// → Binds to port 11434
// → Returns true
// → Server ready for requests
```

### Scenario 2: Server Already Running
```cpp
server->start(11434);
// → Detects existing server on 11434
// → Returns true (uses existing instance)
// → No duplicate server created
```

### Scenario 3: Port Conflict
```cpp
server->start(11434);
// → Port 11434 busy (non-server process)
// → Tries ports 11435, 11436, ...
// → Binds to first available port
// → Returns true
```

### Scenario 4: All Ports Busy
```cpp
server->start(11434);
// → Ports 11434-11443 all busy
// → Returns false
// → Error signal emitted
```

## Implementation Details

### Architecture
```
┌─────────────────┐
│  Client Request │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  GGUFServer     │ ← QTcpServer (listens on port)
│  - Port check   │
│  - HTTP parsing │
│  - Routing      │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ InferenceEngine │ ← Model operations
│  - Tokenization │
│  - Generation   │
│  - Detokenization│
└─────────────────┘
```

### Threading Model
- Main thread handles TCP connections
- Request parsing and routing on main thread
- Inference runs synchronously (can be moved to worker thread)
- Statistics updates are thread-safe (QMutex)

### Security Considerations
- **No authentication** (local-only server recommended)
- **CORS enabled** (allows web application access)
- **Request size limit**: 100MB maximum
- **Timeout**: 2 minute default per request

### Performance
- **Zero-copy request buffering**: Efficient for large payloads
- **Concurrent connections**: Supports multiple simultaneous clients
- **Streaming foundation**: Ready for streaming responses
- **Health monitoring**: Minimal overhead (30s intervals)

## Building

The GGUF server is automatically included when building RawrXD-QtShell:

```bash
# Configure with CMake
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release --target RawrXD-QtShell

# Run
./build/bin/Release/RawrXD-QtShell.exe
```

## Dependencies

- **Qt6::Core**: Basic Qt functionality
- **Qt6::Network**: TCP networking (QTcpServer, QTcpSocket)
- **InferenceEngine**: Model inference backend

All dependencies are automatically linked by CMake.

## Future Enhancements

### Planned Features
- [ ] **Streaming responses**: SSE for token-by-token generation
- [ ] **Batch processing**: Multiple requests in parallel
- [ ] **Model loading**: POST /api/pull to load models dynamically
- [ ] **Authentication**: API key support for security
- [ ] **Rate limiting**: Prevent abuse
- [ ] **Caching**: Response caching for identical prompts
- [ ] **Metrics export**: Prometheus-compatible metrics
- [ ] **WebSocket support**: Bidirectional communication

### Performance Optimizations
- [ ] Worker thread pool for inference
- [ ] Request queuing with priority
- [ ] Connection pooling
- [ ] Zero-copy streaming
- [ ] Compression (gzip/brotli)

## Troubleshooting

### Server won't start
```cpp
if (!server->start(11434)) {
    qWarning() << "Failed to start server";
    // Check: Is port 11434-11443 range available?
    // Check: Firewall blocking Qt application?
}
```

### Inference returns placeholder
```
"tok_1000 tok_1001 tok_1002"
```
This means the transformer is not ready. Check:
- Model file loaded correctly
- GGML library linked
- Weights extracted from GGUF

### Connection refused
```bash
curl: (7) Failed to connect to localhost port 11434
```
- Server not started
- Port number mismatch
- Firewall blocking connections

### Empty response
```json
{"response": ""}
```
- Model not loaded: Call `engine->loadModel("path.gguf")`
- Tokenizer failed: Check vocabulary extraction
- Generation error: Check transformer initialization

## License

Same as RawrXD-ModelLoader project.
