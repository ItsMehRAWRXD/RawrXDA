# RawrXD CLI - External Tools Quick Reference

## Supported Tools

| Tool | API Standard | Status |
|------|-------------|---------|
| **GitHub Copilot** | OpenAI Compatible | ✅ Ready |
| **Amazon Q** | REST API | ✅ Ready |
| **LM Studio** | OpenAI Compatible | ✅ Ready |
| **Ollama** | Compatible | ✅ Ready |
| **VS Code Extensions** | REST API | ✅ Ready |
| **JetBrains IDEs** | Custom Integration | ✅ Ready |

## Quick Start Commands

### 1. Start RawrXD CLI
```powershell
.\build\bin-msvc\Release\RawrXD-CLI.exe
# Or from anywhere if added to PATH
RawrXD-CLI.exe
```

### 2. Test API Connectivity
```powershell
# Health check
curl http://localhost:11434/health

# View all endpoints
curl http://localhost:11434/api/v1/info

# Get detailed docs
curl http://localhost:11434/api/v1/docs
```

### 3. Load a Model (from CLI)
```
load 1              # Load first numbered model
load bigdaddyg      # Load by name
help                # See all CLI commands
```

## Configuration URLs

### GitHub Copilot (VS Code)
```
Endpoint: http://localhost:11434/v1/chat/completions
Type: OpenAI-compatible
Model: bigdaddyg
```

### Amazon Q (AWS)
```
Endpoint: http://localhost:11434/api/v1/analyze
Type: Custom REST
```

### LM Studio
```
Server: http://localhost:11434
API Version: OpenAI v1
```

### Generic REST Client
```
Base URL: http://localhost:11434
API Docs: http://localhost:11434/api/v1/docs
Health: http://localhost:11434/health
```

## API Endpoints Cheat Sheet

| Method | Endpoint | Purpose |
|--------|----------|---------|
| GET | `/health` | Server status |
| GET | `/api/v1/info` | API information |
| GET | `/api/v1/docs` | Full API documentation |
| POST | `/v1/chat/completions` | Chat (OpenAI-compatible) |
| POST | `/api/generate` | Text generation |
| GET | `/api/tags` | List models |
| POST | `/api/v1/analyze` | Project analysis |
| POST | `/api/v1/search` | Code search |

## Port Assignment

- **Primary Instance**: `11434`
- **Multi-Instance**: `11434 + (PID % 100)`
- **Default for CLI**: Automatic on startup

Example: If CLI has PID 1234, API runs on `11434 + (1234 % 100) = 11458`

## One-Liner Test Examples

### PowerShell
```powershell
# Test health
Invoke-RestMethod http://localhost:11434/health

# Send chat message
$body = @{model="bigdaddyg";messages=@(@{role="user";content="Hello"})} | ConvertTo-Json
Invoke-RestMethod -Uri http://localhost:11434/v1/chat/completions -Method Post -Body $body

# Analyze project
$body = @{path=".";format="json"} | ConvertTo-Json
Invoke-RestMethod -Uri http://localhost:11434/api/v1/analyze -Method Post -Body $body
```

### Bash/cURL
```bash
# Test health
curl http://localhost:11434/health

# Chat completion
curl -X POST http://localhost:11434/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{"model":"bigdaddyg","messages":[{"role":"user","content":"Hello"}]}'

# Analyze project
curl -X POST http://localhost:11434/api/v1/analyze \
  -H "Content-Type: application/json" \
  -d '{"path":".","format":"json"}'
```

### Python
```python
import requests

# Test health
response = requests.get('http://localhost:11434/health')
print(response.json())

# Chat completion
response = requests.post(
    'http://localhost:11434/v1/chat/completions',
    json={"model":"bigdaddyg","messages":[{"role":"user","content":"Hello"}]}
)
print(response.json())
```

## Troubleshooting Quick Fixes

| Issue | Solution |
|-------|----------|
| Connection refused | Start CLI: `RawrXD-CLI.exe` |
| Port in use | Check: `Get-NetTCPConnection -LocalPort 11434` |
| Model not loaded | In CLI: `load 1` |
| Slow responses | Check uptime: `curl http://localhost:11434/health` |
| No results | Verify format: See `/api/v1/docs` |

## Environment Variables

```powershell
# Set custom port (for multi-instance)
$env:RAWRXD_PORT = "11440"

# Enable debug logging
$env:RAWRXD_DEBUG = "1"

# Model paths
$env:RAWRXD_MODEL_PATH = "D:\OllamaModels"
```

## Common Integration Patterns

### Pattern 1: Auto-Discovery
```powershell
# Discover service
curl http://localhost:11434/health | jq '.port'
# Use result for further requests
```

### Pattern 2: Health Monitoring
```powershell
# Monitor service
while($true) {
    $health = curl http://localhost:11434/health
    Write-Host "Status: $($health.status) | Uptime: $($health.uptime_seconds)s"
    Start-Sleep -Seconds 10
}
```

### Pattern 3: Batch Analysis
```powershell
# Analyze multiple projects
@("C:\project1", "C:\project2", "C:\project3") | ForEach-Object {
    Invoke-RestMethod -Uri http://localhost:11434/api/v1/analyze -Method Post `
        -Body (@{path=$_} | ConvertTo-Json)
}
```

## Security Notes

- ✅ Rate limiting: 60 req/min per IP
- ✅ CORS enabled for cross-origin
- ✅ Size limit: 10 MB max request
- ⚠️ No authentication (use firewall)
- ⚠️ HTTP only (use reverse proxy for HTTPS)

## Performance Metrics

- **Health Check**: < 10ms
- **Chat Response**: 500-2000ms
- **Project Analysis**: 5-30s
- **Code Search**: 100-500ms

## File Locations

| Component | Path |
|-----------|------|
| CLI Binary | `build/bin-msvc/Release/RawrXD-CLI.exe` |
| Config | `~/.rawrxd/config.json` |
| Models | `D:/OllamaModels/` |
| API Docs | `API_EXTERNAL_TOOLS_INTEGRATION.md` |

## Support Resources

- **Full Documentation**: `API_EXTERNAL_TOOLS_INTEGRATION.md`
- **Test Script**: `test-external-api.ps1`
- **Repository**: https://github.com/ItsMehRAWRXD/RawrXD
- **Issues**: https://github.com/ItsMehRAWRXD/RawrXD/issues

## Version Info

- **CLI Version**: 1.0.0
- **API Version**: v1
- **OpenAI Compatibility**: ✅ Full
- **Ollama Compatibility**: ✅ Full
