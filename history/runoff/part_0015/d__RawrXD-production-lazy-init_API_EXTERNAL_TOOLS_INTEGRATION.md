# RawrXD CLI - External Tools Integration Guide

## Overview

The RawrXD CLI is now accessible to external tools like **GitHub Copilot**, **Amazon Q**, **LM Studio**, and other AI development assistants through a comprehensive REST API.

## Quick Start

### 1. Start the RawrXD CLI

```powershell
# From PowerShell
.\build\bin-msvc\Release\RawrXD-CLI.exe

# Output:
# RawrXD CLI Instance [PID: 1234] [Port: 11434]
# RawrXD CLI v1.0.0 - Agentic IDE Command Line Interface
# Type 'help' for available commands
```

The API server automatically starts on port `11434 + (PID % 100)`.

### 2. Test Basic Connectivity

```bash
# Health check
curl http://localhost:11434/health

# Response:
# {
#   "status": "ok",
#   "version": "1.0.0",
#   "port": 11434,
#   "uptime_seconds": 10,
#   "model_loaded": true,
#   "api_version": "v1"
# }
```

### 3. Get API Documentation

```bash
# Retrieve full API documentation
curl http://localhost:11434/api/v1/docs

# Retrieve API info
curl http://localhost:11434/api/v1/info
```

## Supported External Tools

### GitHub Copilot

GitHub Copilot can use RawrXD as a custom model provider for enhanced code completion and analysis.

#### Configuration

1. Configure in VS Code settings:
   ```json
   {
     "github.copilot.advanced": {
       "backendUrl": "http://localhost:11434",
       "debug": true
     }
   }
   ```

2. Use OpenAI-compatible chat completions endpoint:
   ```bash
   curl -X POST http://localhost:11434/v1/chat/completions \
     -H "Content-Type: application/json" \
     -d '{
       "model": "bigdaddyg",
       "messages": [{"role": "user", "content": "Explain this code"}],
       "temperature": 0.7
     }'
   ```

### Amazon Q

Amazon Q integration for AWS IDE development.

#### Configuration

1. In Amazon Q settings, configure custom backend:
   ```
   Endpoint: http://localhost:11434/v1/chat/completions
   Model: bigdaddyg
   API Version: OpenAI-compatible (v1)
   ```

2. Send requests:
   ```bash
   curl -X POST http://localhost:11434/api/v1/analyze \
     -H "Content-Type: application/json" \
     -d '{
       "path": "C:/your/project",
       "format": "json",
       "include_metrics": true
     }'
   ```

### LM Studio

LM Studio can use RawrXD as an alternative model server.

#### Configuration

1. Add custom model server:
   - URL: `http://localhost:11434`
   - API Standard: OpenAI-compatible
   - Model: `bigdaddyg`

2. Test connectivity:
   ```bash
   curl http://localhost:11434/api/tags
   ```

## API Endpoints Reference

### Health & Discovery

#### GET /health
Check server status

**Response:**
```json
{
  "status": "ok",
  "version": "1.0.0",
  "port": 11434,
  "uptime_seconds": 120,
  "model_loaded": true,
  "api_version": "v1"
}
```

### Remote Command Execution

#### POST /api/v1/execute
Execute CLI commands remotely (FULL REMOTE CONTROL)

**Request:**
```json
{
  "command": "help"
}
```

**Response:**
```json
{
  "command": "help",
  "status": "success",
  "result": "Available commands:\n  help - Show this help\n  status - Show server status\n  ...",
  "timestamp": "2025-01-15T12:00:00Z"
}
```

**Available Commands:**
- `help` - Show available commands
- `status` / `serverinfo` - Show server and model status
- `models` - List available models
- `load <model>` - Load a model (e.g., `load 1` or `load bigdaddyg`)
- `analyze` - Analyze current project
- `search <query>` - Search project files
- `chat <message>` - Send message to AI
- `generate <prompt>` - Generate text completion
- `version` - Show version info

#### GET /api/v1/status
Track command execution status

**Query Parameters:**
- `id` - Command ID (optional)

**Response:**
```json
{
  "command_id": "cmd_12345",
  "status": "completed",
  "progress": 100,
  "output": "Command execution complete",
  "timestamps": {
    "created": "2025-01-15T12:00:00Z",
    "completed": "2025-01-15T12:00:05Z"
  }
}
```

#### GET /api/v1/info
Get API information and available endpoints

**Response:**
```json
{
  "name": "RawrXD Agentic IDE",
  "description": "AI-powered IDE with GPU acceleration",
  "version": "1.0.0",
  "base_url": "http://localhost:11434",
  "supported_tools": ["GitHub Copilot", "Amazon Q", "LM Studio", "Ollama Compatible"],
  "api_endpoints": {
    "health": "GET /health",
    "info": "GET /api/v1/info",
    "docs": "GET /api/v1/docs",
    "chat": "POST /v1/chat/completions",
    "generate": "POST /api/generate",
    "tags": "GET /api/tags",
    "execute": "POST /api/v1/execute",
    "status": "GET /api/v1/status",
    "analysis": "POST /api/v1/analyze",
    "search": "POST /api/v1/search"
  }
}
```

#### GET /api/v1/docs
Get comprehensive API documentation

**Response:**
```json
{
  "api_version": "1.0.0",
  "title": "RawrXD Agentic IDE API",
  "endpoints": [
    {
      "path": "/api/v1/execute",
      "method": "POST",
      "description": "Execute CLI commands",
      "request": {"command": "help"},
      "response": {"command": "help", "status": "success", "result": "..."}
    }
  ]
}
```

### Chat & Inference

#### GET /api/v1/info
Get API information and available endpoints

**Response:**
```json
{
  "name": "RawrXD Agentic IDE",
  "description": "AI-powered IDE with GPU acceleration and advanced analysis",
  "version": "1.0.0",
  "base_url": "http://localhost:11434",
  "supported_tools": [
    "GitHub Copilot",
    "Amazon Q",
    "LM Studio",
    "Ollama Compatible"
  ],
  "api_endpoints": {
    "health": "GET /health",
    "info": "GET /api/v1/info",
    "docs": "GET /api/v1/docs",
    "chat": "POST /v1/chat/completions",
    "generate": "POST /api/generate",
    "tags": "GET /api/tags",
    "analysis": "POST /api/v1/analyze",
    "search": "POST /api/v1/search"
  }
}
```

#### POST /v1/chat/completions
OpenAI-compatible chat completion endpoint

**Request:**
```json
{
  "model": "bigdaddyg",
  "messages": [
    {"role": "system", "content": "You are a helpful assistant"},
    {"role": "user", "content": "How do I optimize this code?"}
  ],
  "temperature": 0.7,
  "max_tokens": 2000
}
```

**Response:**
```json
{
  "id": "chatcmpl-123456",
  "object": "chat.completion",
  "created": 1705348200,
  "model": "bigdaddyg",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "Here are some optimization strategies..."
      },
      "finish_reason": "stop"
    }
  ],
  "usage": {
    "prompt_tokens": 25,
    "completion_tokens": 150,
    "total_tokens": 175
  }
}
```

#### POST /api/generate
Text generation endpoint

**Request:**
```json
{
  "prompt": "def fibonacci",
  "model": "bigdaddyg",
  "stream": false
}
```

**Response:**
```json
{
  "response": "(n):\n    if n <= 1:\n        return n\n    return fibonacci(n-1) + fibonacci(n-2)",
  "done": true,
  "created_at": 1705348200
}
```

### Project Analysis

#### POST /api/v1/analyze
Analyze project structure and generate insights

**Request:**
```json
{
  "path": "C:/project",
  "format": "json",
  "include_metrics": true,
  "exclude_artifacts": true
}
```

**Response:**
```json
{
  "path": "C:/project",
  "status": "success",
  "statistics": {
    "total_files": 46663,
    "total_size_mb": 7800,
    "file_types": {
      "cpp": 1200,
      "h": 800,
      "json": 250,
      "python": 400,
      "other": 43913
    }
  },
  "timestamp": "1705348200"
}
```

#### POST /api/v1/search
Search project files and code

**Request:**
```json
{
  "query": "function authenticate",
  "path": "C:/project",
  "case_sensitive": false,
  "file_types": ["cpp", "h", "py"]
}
```

**Response:**
```json
{
  "query": "function authenticate",
  "status": "success",
  "results": [
    {
      "file": "src/auth/auth.cpp",
      "line": 45,
      "content": "bool authenticate(const std::string& user, const std::string& pass)"
    },
    {
      "file": "include/auth.h",
      "line": 12,
      "content": "bool authenticate(const std::string& user, const std::string& pass);"
    }
  ],
  "result_count": 2
}
```

#### GET /api/tags
List available models

**Response:**
```json
{
  "models": [
    {
      "name": "bigdaddyg",
      "modified_at": "2025-01-15T10:30:00Z",
      "size": 36200000000
    }
  ]
}
```

## Integration Examples

### Python Client

```python
import requests
import json

class RawrXDClient:
    def __init__(self, base_url="http://localhost:11434"):
        self.base_url = base_url
    
    def health_check(self):
        response = requests.get(f"{self.base_url}/health")
        return response.json()
    
    def chat_completion(self, messages, model="bigdaddyg", temperature=0.7):
        payload = {
            "model": model,
            "messages": messages,
            "temperature": temperature
        }
        response = requests.post(
            f"{self.base_url}/v1/chat/completions",
            json=payload
        )
        return response.json()
    
    def analyze_project(self, path, format="json"):
        payload = {
            "path": path,
            "format": format,
            "include_metrics": True
        }
        response = requests.post(
            f"{self.base_url}/api/v1/analyze",
            json=payload
        )
        return response.json()
    
    def search_code(self, query, path="."):
        payload = {
            "query": query,
            "path": path,
            "case_sensitive": False
        }
        response = requests.post(
            f"{self.base_url}/api/v1/search",
            json=payload
        )
        return response.json()

# Usage
client = RawrXDClient()

# Check health
status = client.health_check()
print(f"Server status: {status['status']}")

# Chat with model
messages = [
    {"role": "user", "content": "Write a Python function for sorting"}
]
response = client.chat_completion(messages)
print(response['choices'][0]['message']['content'])

# Analyze project
analysis = client.analyze_project("C:/my/project")
print(f"Total files: {analysis['statistics']['total_files']}")

# Search code
results = client.search_code("class Database", "C:/my/project")
print(f"Found {results['result_count']} matches")
```

### JavaScript/Node.js Client

```javascript
const axios = require('axios');

class RawrXDClient {
  constructor(baseUrl = 'http://localhost:11434') {
    this.baseUrl = baseUrl;
  }

  async healthCheck() {
    const response = await axios.get(`${this.baseUrl}/health`);
    return response.data;
  }

  async chatCompletion(messages, model = 'bigdaddyg', temperature = 0.7) {
    const response = await axios.post(`${this.baseUrl}/v1/chat/completions`, {
      model,
      messages,
      temperature
    });
    return response.data;
  }

  async analyzeProject(path, format = 'json') {
    const response = await axios.post(`${this.baseUrl}/api/v1/analyze`, {
      path,
      format,
      include_metrics: true
    });
    return response.data;
  }

  async searchCode(query, path = '.') {
    const response = await axios.post(`${this.baseUrl}/api/v1/search`, {
      query,
      path,
      case_sensitive: false
    });
    return response.data;
  }
}

// Usage
const client = new RawrXDClient();

(async () => {
  // Check health
  const status = await client.healthCheck();
  console.log(`Server: ${status.status}`);

  // Chat completion
  const messages = [
    { role: 'user', content: 'Explain closure in JavaScript' }
  ];
  const response = await client.chatCompletion(messages);
  console.log(response.choices[0].message.content);

  // Analyze project
  const analysis = await client.analyzeProject('C:/project');
  console.log(`Files: ${analysis.statistics.total_files}`);
})();
```

### PowerShell Integration

```powershell
# RawrXD CLI Client for PowerShell

function Get-RawrXDHealth {
    param([string]$BaseUrl = "http://localhost:11434")
    
    try {
        $response = Invoke-RestMethod -Uri "$BaseUrl/health" -Method Get
        return $response
    }
    catch {
        Write-Error "Failed to connect to RawrXD CLI: $_"
    }
}

function Invoke-RawrXDChat {
    param(
        [string]$Message,
        [string]$Model = "bigdaddyg",
        [string]$BaseUrl = "http://localhost:11434"
    )
    
    $payload = @{
        model = $Model
        messages = @(
            @{
                role = "user"
                content = $Message
            }
        )
        temperature = 0.7
    } | ConvertTo-Json
    
    try {
        $response = Invoke-RestMethod `
            -Uri "$BaseUrl/v1/chat/completions" `
            -Method Post `
            -ContentType "application/json" `
            -Body $payload
        
        return $response.choices[0].message.content
    }
    catch {
        Write-Error "Chat completion failed: $_"
    }
}

function Invoke-RawrXDAnalysis {
    param(
        [string]$Path = ".",
        [string]$BaseUrl = "http://localhost:11434"
    )
    
    $payload = @{
        path = $Path
        format = "json"
        include_metrics = $true
    } | ConvertTo-Json
    
    try {
        $response = Invoke-RestMethod `
            -Uri "$BaseUrl/api/v1/analyze" `
            -Method Post `
            -ContentType "application/json" `
            -Body $payload
        
        return $response
    }
    catch {
        Write-Error "Analysis failed: $_"
    }
}

# Usage
$health = Get-RawrXDHealth
Write-Host "Server Status: $($health.status)"

$response = Invoke-RawrXDChat "How do I read a file in PowerShell?"
Write-Host "AI Response: $response"

$analysis = Invoke-RawrXDAnalysis "C:\project"
Write-Host "Files analyzed: $($analysis.statistics.total_files)"
```

## Authentication & Security

### Current Implementation

- **Rate Limiting**: 60 requests per minute per client IP
- **Request Size Limit**: 10 MB maximum
- **CORS**: Enabled for cross-origin requests (`Access-Control-Allow-Origin: *`)

### Recommended Security Measures

For production deployment:

1. **Add API Key Authentication**:
   ```bash
   curl -H "Authorization: Bearer YOUR_API_KEY" \
        http://localhost:11434/v1/chat/completions
   ```

2. **Enable HTTPS**:
   - Configure SSL/TLS certificate
   - Use HTTPS endpoints

3. **Restrict IP Access**:
   ```powershell
   # Windows Firewall rule
   netsh advfirewall firewall add rule `
     name="RawrXD CLI API" `
     dir=in action=allow protocol=tcp `
     localport=11434 `
     remoteip=192.168.1.0/24
   ```

## Troubleshooting

### Port Already in Use

If port 11434 is already in use:

```powershell
# Find process using port 11434
Get-NetTCPConnection -LocalPort 11434

# Kill the process (if needed)
Stop-Process -Id <PID> -Force

# Or start CLI on different port (automatic via PID offset)
```

### Connection Refused

```powershell
# Test connectivity
Test-NetConnection -ComputerName localhost -Port 11434

# Check firewall
Get-NetFirewallRule | Where-Object { $_.Name -match "11434" }
```

### Slow Responses

```bash
# Check active connections
curl http://localhost:11434/api/v1/info | jq '.endpoints'

# Monitor metrics (from CLI)
serverinfo  # CLI command to show current metrics
```

## Performance Considerations

- **Concurrent Requests**: Supports 64+ concurrent connections
- **Response Time**: 
  - Health check: < 10ms
  - Chat completion: 500-2000ms (depends on model)
  - Project analysis: 5-30s (depends on project size)
- **Memory Usage**: ~500MB base + model size

## Next Steps

1. **Load BigDaddyG Model**:
   ```
   In CLI: load 1  (if numbered discovery is available)
   Or: load d:\path\to\BigDaddyG-F32-FROM-Q4.gguf
   ```

2. **Test with External Tools**:
   - Configure GitHub Copilot
   - Set up Amazon Q
   - Add LM Studio integration

3. **Monitor Performance**:
   ```bash
   curl http://localhost:11434/health -s | jq '.uptime_seconds'
   ```

## Support

For issues or questions:
- Repository: https://github.com/ItsMehRAWRXD/RawrXD
- Issues: https://github.com/ItsMehRAWRXD/RawrXD/issues
- Documentation: See API_EXTERNAL_TOOLS_INTEGRATION.md
