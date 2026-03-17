# RawrXD Port Configuration & API Architecture

## Overview

The RawrXD CLI now uses intelligent port allocation with three strategies:

### 1. **Random Port Allocation (Default)**
- **Range**: 15000-25000
- **Attempts**: Up to 50 random ports before falling back to sequential scan
- **Use Case**: CLI startup with maximum flexibility and minimal conflicts
- **Benefit**: Each instance gets a unique port automatically, enabling multi-instance deployments

### 2. **Sequential Port Scanning**
- **Starting Port**: 11434 (Ollama-compatible default)
- **Range**: 11434-11533 (100 ports)
- **Attempts**: Sequential scan from starting port
- **Use Case**: Fallback if random allocation exhausts attempts
- **Benefit**: Predictable port numbering in controlled environments

### 3. **Specific Port Binding**
- **Usage**: `api_server.Start(port_number)`
- **Validation**: Checks port availability before binding
- **Fallback**: Automatically scans for alternatives if specified port is in use
- **Benefit**: Fine-grained control when needed

## Why We Have an HTTP API

### 1. **External Tool Integration**
While RawrXD uses its own custom GGUF loader and doesn't require external model servers, the HTTP API enables:

- **GitHub Copilot Integration**: VS Code extensions can query the CLI for code analysis and generation
- **Amazon Q Integration**: AWS IDE tools can access RawrXD's inference capabilities
- **LM Studio Compatibility**: Third-party UIs can interface with the loader
- **REST Client Libraries**: Python, JavaScript, PowerShell scripts can automate workflows

### 2. **Multi-Instance Deployment**
- Run multiple CLI instances on different ports simultaneously
- Each instance processes independent workloads
- No port conflicts with intelligent random allocation
- Enables load distribution across CPU/GPU resources

### 3. **Headless/Containerized Execution**
- CLI can run without UI components
- Perfect for Docker/Kubernetes deployments
- Docker containers can expose different ports
- Orchestration systems can discover services via `/api/v1/info`

### 4. **Enterprise/Cloud Integration**
- RESTful APIs integrate with CI/CD pipelines (GitHub Actions, GitLab CI, etc.)
- Microservices architectures can consume the API
- Monitoring systems can query `/health` endpoint
- Load balancers can route requests across multiple instances

### 5. **Cross-Language Support**
- API clients in Python, JavaScript, PowerShell, Go, Rust, etc.
- WebSocket support for streaming (future enhancement)
- Standard HTTP protocol - no custom binary protocols
- JSON request/response format for easy parsing

## API Endpoints Available

### Health & Discovery
```bash
GET /health                    # Server health check with metrics
GET /api/v1/info              # Service discovery and configuration
GET /api/v1/docs              # Full API documentation
```

### AI Inference
```bash
POST /v1/chat/completions     # OpenAI-compatible chat endpoint
POST /api/generate            # Text generation from prompt
GET /api/tags                 # List loaded models
```

### Code Analysis
```bash
POST /api/v1/analyze          # Project analysis (file counts, size breakdown)
POST /api/v1/search           # Code search across project
```

### Command Execution
```bash
POST /api/v1/execute          # Execute CLI commands (load, analyze, search, chat, etc.)
GET /api/v1/status            # Track command status and progress
```

## Configuration Examples

### Use Random Port (Default - Recommended for Most Users)
```cpp
// In main.cpp - automatically finds random available port
uint16_t api_port = APIServer::FindRandomAvailablePort(15000, 25000, 50);
api_server.Start(api_port);
std::cout << "API on port: " << api_port << "\n";  // Shows assigned port
```

### Use Sequential Scanning (Backward Compatible)
```cpp
// Falls back to 11434 if available, then tries 11435, 11436, etc.
uint16_t api_port = APIServer::FindAvailablePort(11434, 100);
api_server.Start(api_port);
```

### Use Specific Port with Fallback
```cpp
// Try port 8080, falls back to random if unavailable
uint16_t api_port = 8080;
if (!APIServer::IsPortAvailable(api_port)) {
    api_port = APIServer::FindRandomAvailablePort(15000, 25000, 50);
}
api_server.Start(api_port);
```

## Port Discovery for Clients

Clients connecting to RawrXD should:

1. **Check stdout output** - CLI prints the assigned port on startup:
   ```
   ✓ API server initialized on port 17843
   ```

2. **Query `/api/v1/info` endpoint** - Get full API configuration:
   ```json
   {
     "name": "RawrXD Agentic IDE",
     "base_url": "http://localhost:17843",
     "api_endpoints": {...}
   }
   ```

3. **Use Environment Variables** (for containerized setups):
   ```bash
   # CLI automatically reads if set
   export RAWRXD_API_PORT=12345
   ```

## Multi-Instance Deployment Example

### Running Multiple CLI Instances
```powershell
# Terminal 1 - Instance 1 (random port)
./RawrXD-CLI.exe
# Output: ✓ API server initialized on port 17234

# Terminal 2 - Instance 2 (random port)  
./RawrXD-CLI.exe
# Output: ✓ API server initialized on port 19456

# Terminal 3 - Instance 3 (random port)
./RawrXD-CLI.exe
# Output: ✓ API server initialized on port 21789
```

All three instances run simultaneously with zero port conflicts!

### Load Balancing Pattern
```python
# Python example: distribute requests across multiple instances
import requests
import random

INSTANCES = [
    "http://localhost:17234",
    "http://localhost:19456", 
    "http://localhost:21789"
]

def analyze_project(project_path):
    instance = random.choice(INSTANCES)
    response = requests.post(
        f"{instance}/api/v1/analyze",
        json={"path": project_path}
    )
    return response.json()
```

## Why Not Use 11434 by Default?

1. **Ollama Compatibility** - 11434 is the default Ollama port, often already in use
2. **Port Exhaustion** - Scanning 11434-11533 (100 ports) is inefficient for many simultaneous instances
3. **Random Allocation Benefits**:
   - Instant port availability (no scanning overhead)
   - Distributed instances use less predictable ports (better for security)
   - Avoids "thundering herd" effect when multiple instances start simultaneously
   - Reduces likelihood of port conflicts with other services

## Security Considerations

### Port Selection Security
- Random ports reduce port scanning attack surface
- Dynamic allocation prevents hardcoded port assumptions
- Range 15000-25000 avoids privileged ports (<1024) on Unix systems

### API Security (Currently)
- Running on localhost only (0.0.0.0 configurable)
- No authentication (suitable for local development)
- Firewall can restrict external access

### Future Enhancements
- TLS/HTTPS support for remote connections
- JWT token authentication for external tools
- Rate limiting per client IP
- CORS configuration for browser-based clients

## Environment Variables

Future support for configuration:
```bash
# Set custom port range
RAWRXD_PORT_MIN=10000
RAWRXD_PORT_MAX=30000

# Force specific port
RAWRXD_API_PORT=9000

# Disable API (CLI only mode)
RAWRXD_API_DISABLED=true

# Enable remote access (not recommended)
RAWRXD_API_BIND=0.0.0.0
```

## Troubleshooting

### "No available ports found"
```
Error: No available ports found in range 15000-25000
```
**Solution**: 
- Check for runaway processes: `netstat -ano | findstr "LISTEN"`
- Increase port range: `FindRandomAvailablePort(10000, 65000, 100)`
- Kill unnecessary processes

### "Cannot bind to port X"
```
Error: Bind failed on port 17234
```
**Solution**:
- Port is in use by another application
- Random allocation should handle this automatically
- Restart CLI to try different port

### "Port persists between runs"
```
Port 11434 still shows "in use" after CLI exits
```
**Solution**:
- TIME_WAIT state on server socket (OS level)
- Random allocation avoids this issue
- Wait 30-60 seconds or use SO_REUSEADDR (implemented)

## Summary Table

| Scenario | Strategy | Range | Best For |
|----------|----------|-------|----------|
| Single CLI instance | Random | 15000-25000 | Development, most users |
| Multi-instance setup | Random | 15000-25000 | Load distribution, testing |
| Docker containers | Random | 15000-25000 | Container orchestration |
| Legacy compatibility | Sequential | 11434-11533 | Existing Ollama setups |
| Specific requirement | Direct bind | Custom | Advanced configurations |

## API Client Libraries

### JavaScript/Node.js
```javascript
const instance = "http://localhost:17234"; // Get from CLI startup
const response = await fetch(`${instance}/api/v1/execute`, {
    method: "POST",
    body: JSON.stringify({ command: "help" })
});
```

### Python
```python
import requests
import subprocess
import re

# Get port from CLI startup message
cli_output = subprocess.run(["./RawrXD-CLI.exe"], capture_output=True, text=True)
port_match = re.search(r"port (\d+)", cli_output.stdout)
port = port_match.group(1)

# Use API
response = requests.post(f"http://localhost:{port}/api/v1/execute", 
    json={"command": "analyze"})
```

### PowerShell
```powershell
# Parse port from CLI startup
$cliOutput = & ".\RawrXD-CLI.exe"
$port = [regex]::Match($cliOutput, "port (\d+)").Groups[1].Value

# Use API
$response = Invoke-RestMethod -Uri "http://localhost:$port/api/v1/execute" `
    -Method Post -Body (@{ command = "help" } | ConvertTo-Json)
```

---

**Version**: 1.0  
**Last Updated**: January 15, 2026  
**Status**: Production Ready
