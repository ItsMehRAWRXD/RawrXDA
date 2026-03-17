# RawrXD API Server - Advanced Features Guide

## Table of Contents
1. [Environment Variables](#environment-variables)
2. [TLS/HTTPS Support](#tlshttps-support)
3. [JWT Authentication](#jwt-authentication)
4. [WebSocket Streaming](#websocket-streaming)
5. [Service Registry](#service-registry)
6. [Rate Limiting](#rate-limiting)
7. [Metrics Endpoint](#metrics-endpoint)

---

## Environment Variables

Configure the API server using environment variables:

### Port Configuration

```powershell
# Set custom port range for random allocation
$env:RAWRXD_PORT_MIN = "15000"
$env:RAWRXD_PORT_MAX = "25000"

# Set fixed port (overrides random allocation)
$env:RAWRXD_PORT = "11434"

# Disable API server completely
$env:RAWRXD_API_DISABLED = "true"
```

### TLS/HTTPS Configuration

```powershell
# Enable TLS/HTTPS
$env:RAWRXD_TLS_ENABLED = "true"

# Certificate paths
$env:RAWRXD_TLS_CERT = "C:\certs\server.crt"
$env:RAWRXD_TLS_KEY = "C:\certs\server.key"
$env:RAWRXD_TLS_CA = "C:\certs\ca.crt"  # Optional
```

### JWT Authentication

```powershell
# Enable JWT authentication
$env:RAWRXD_JWT_ENABLED = "true"

# Secret key for signing tokens (CHANGE THIS IN PRODUCTION!)
$env:RAWRXD_JWT_SECRET = "your-super-secret-key-min-32-chars"

# Token expiration (hours)
$env:RAWRXD_JWT_EXPIRATION_HOURS = "24"
```

### WebSocket Configuration

```powershell
# Enable WebSocket streaming
$env:RAWRXD_WEBSOCKET_ENABLED = "true"

# Max concurrent WebSocket connections
$env:RAWRXD_WEBSOCKET_MAX_CONNECTIONS = "100"
```

### Service Registry

```powershell
# Enable service discovery
$env:RAWRXD_SERVICE_REGISTRY_ENABLED = "true"

# Service name
$env:RAWRXD_SERVICE_NAME = "RawrXD-API"

# External registry URL (Consul, etcd, etc.)
$env:RAWRXD_SERVICE_REGISTRY_URL = "http://consul.local:8500"
```

### Rate Limiting

```powershell
# Enable rate limiting
$env:RAWRXD_RATE_LIMIT_ENABLED = "true"

# Requests per minute per client
$env:RAWRXD_RATE_LIMIT_RPM = "60"
```

### Metrics

```powershell
# Enable Prometheus metrics endpoint
$env:RAWRXD_METRICS_ENABLED = "true"
```

---

## TLS/HTTPS Support

### Generating Self-Signed Certificates

For development/testing:

```powershell
# Generate private key
openssl genrsa -out server.key 2048

# Generate certificate signing request
openssl req -new -key server.key -out server.csr -subj "/CN=localhost"

# Generate self-signed certificate (valid for 365 days)
openssl x509 -req -days 365 -in server.csr -signkey server.key -out server.crt

# Set environment variables
$env:RAWRXD_TLS_ENABLED = "true"
$env:RAWRXD_TLS_CERT = "server.crt"
$env:RAWRXD_TLS_KEY = "server.key"
```

### Using Let's Encrypt (Production)

```bash
# Install certbot
sudo apt-get install certbot

# Generate certificate
sudo certbot certonly --standalone -d api.yourdomain.com

# Certificates will be in /etc/letsencrypt/live/api.yourdomain.com/
$env:RAWRXD_TLS_CERT = "/etc/letsencrypt/live/api.yourdomain.com/fullchain.pem"
$env:RAWRXD_TLS_KEY = "/etc/letsencrypt/live/api.yourdomain.com/privkey.pem"
```

### Client Example (HTTPS)

```python
import requests

# With TLS enabled
response = requests.post(
    "https://localhost:11434/v1/chat/completions",
    json={"messages": [{"role": "user", "content": "Hello!"}]},
    verify=False  # For self-signed certs in dev
)
```

---

## JWT Authentication

### Generating a Token

```python
import jwt
import datetime

# Secret key (must match RAWRXD_JWT_SECRET)
secret = "your-super-secret-key-min-32-chars"

# Create token
payload = {
    "sub": "user123",
    "iat": datetime.datetime.utcnow(),
    "exp": datetime.datetime.utcnow() + datetime.timedelta(hours=24),
    "scopes": ["inference", "model:read", "model:write"]
}

token = jwt.encode(payload, secret, algorithm="HS256")
print(f"Token: {token}")
```

### Using Token in Requests

```python
import requests

headers = {
    "Authorization": f"Bearer {token}"
}

response = requests.post(
    "http://localhost:11434/v1/chat/completions",
    headers=headers,
    json={"messages": [{"role": "user", "content": "Hello!"}]}
)
```

### Token Scopes

| Scope | Description |
|-------|-------------|
| `*` | All permissions (admin) |
| `inference` | Generate completions |
| `model:read` | List and view models |
| `model:write` | Upload and manage models |
| `admin` | Full administrative access |

### Revoking Tokens

Tokens can be revoked via the API:

```bash
curl -X POST http://localhost:11434/api/v1/auth/revoke \
  -H "Authorization: Bearer $TOKEN"
```

---

## WebSocket Streaming

### Connecting to WebSocket

WebSocket server runs on `port + 1` (e.g., if HTTP is on 11434, WS is on 11435).

```javascript
// JavaScript WebSocket client
const ws = new WebSocket('ws://localhost:11435');

ws.onopen = () => {
    console.log('Connected to RawrXD WebSocket');
    
    // Send inference request
    ws.send(JSON.stringify({
        type: 'inference',
        prompt: 'Write a poem about AI',
        stream: true
    }));
};

ws.onmessage = (event) => {
    const chunk = event.data;
    console.log('Received chunk:', chunk);
};

ws.onerror = (error) => {
    console.error('WebSocket error:', error);
};

ws.onclose = () => {
    console.log('WebSocket closed');
};
```

### Python WebSocket Client

```python
import websockets
import asyncio
import json

async def stream_inference():
    uri = "ws://localhost:11435"
    
    async with websockets.connect(uri) as websocket:
        # Send request
        await websocket.send(json.dumps({
            "type": "inference",
            "prompt": "Write a poem about AI",
            "stream": True
        }))
        
        # Receive streaming response
        async for message in websocket:
            print(f"Chunk: {message}")
            
            if message == "[DONE]":
                break

asyncio.run(stream_inference())
```

### PowerShell WebSocket Client

```powershell
# Using ClientWebSocket
Add-Type -AssemblyName System.Net.WebSockets
Add-Type -AssemblyName System.Net.Http

$ws = New-Object System.Net.WebSockets.ClientWebSocket
$uri = New-Object Uri("ws://localhost:11435")

$ct = [System.Threading.CancellationToken]::None
$ws.ConnectAsync($uri, $ct).Wait()

# Send message
$message = @{
    type = "inference"
    prompt = "Hello, World!"
    stream = $true
} | ConvertTo-Json

$bytes = [System.Text.Encoding]::UTF8.GetBytes($message)
$segment = [System.ArraySegment[byte]]::new($bytes)
$ws.SendAsync($segment, [System.Net.WebSockets.WebSocketMessageType]::Text, $true, $ct).Wait()

# Receive messages
$buffer = New-Object byte[] 4096
while ($ws.State -eq 'Open') {
    $segment = [System.ArraySegment[byte]]::new($buffer)
    $result = $ws.ReceiveAsync($segment, $ct).Result
    
    $text = [System.Text.Encoding]::UTF8.GetString($buffer, 0, $result.Count)
    Write-Host "Received: $text"
}

$ws.Dispose()
```

---

## Service Registry

### Manual Service Registration

```bash
# Register service
curl -X POST http://localhost:11434/api/v1/registry/register \
  -H "Content-Type: application/json" \
  -d '{
    "service_id": "rawrxd-api-001",
    "service_name": "RawrXD-API",
    "host": "localhost",
    "port": 11434,
    "protocol": "http",
    "tags": ["inference", "ml", "local"],
    "metadata": {
      "version": "1.0.0",
      "region": "us-west"
    }
  }'
```

### Service Discovery

```bash
# Discover all instances of a service
curl http://localhost:11434/api/v1/registry/discover/RawrXD-API

# Response:
# [
#   {
#     "service_id": "rawrxd-api-001",
#     "service_name": "RawrXD-API",
#     "host": "localhost",
#     "port": 11434,
#     "protocol": "http",
#     "status": "HEALTHY",
#     "last_heartbeat": "2026-01-15T10:30:00Z"
#   }
# ]
```

### Health Checks

Automatic health checks via `/health` endpoint:

```bash
curl http://localhost:11434/health

# Response:
# {
#   "status": "healthy",
#   "uptime_seconds": 3600,
#   "version": "1.0.0",
#   "service_id": "rawrxd-api-001"
# }
```

### Integration with Consul

```powershell
# Set Consul URL
$env:RAWRXD_SERVICE_REGISTRY_URL = "http://consul.local:8500"
$env:RAWRXD_SERVICE_REGISTRY_ENABLED = "true"

# RawrXD will automatically register with Consul on startup
```

---

## Rate Limiting

### Per-Client Rate Limiting

Rate limiting is enforced per client IP address:

```bash
# First request: Success
curl http://localhost:11434/api/generate

# After 60 requests/minute: Rate limited
curl http://localhost:11434/api/generate
# Response: HTTP 429 Too Many Requests
# {
#   "error": "Rate limit exceeded. Try again in 30 seconds."
# }
```

### Rate Limit Headers

```
X-RateLimit-Limit: 60
X-RateLimit-Remaining: 45
X-RateLimit-Reset: 1642244400
```

### Bypass Rate Limiting (JWT Scope)

Tokens with `admin` scope bypass rate limiting:

```python
headers = {
    "Authorization": f"Bearer {admin_token}"
}

# No rate limiting applied
for i in range(1000):
    response = requests.post(url, headers=headers, json=data)
```

---

## Metrics Endpoint

### Prometheus Metrics

```bash
curl http://localhost:11434/metrics

# Response:
# # HELP rawrxd_requests_total Total number of requests
# # TYPE rawrxd_requests_total counter
# rawrxd_requests_total{endpoint="/api/generate",status="success"} 1234
# rawrxd_requests_total{endpoint="/api/generate",status="error"} 12
#
# # HELP rawrxd_request_duration_seconds Request duration in seconds
# # TYPE rawrxd_request_duration_seconds histogram
# rawrxd_request_duration_seconds_bucket{endpoint="/api/generate",le="0.1"} 850
# rawrxd_request_duration_seconds_bucket{endpoint="/api/generate",le="1.0"} 1200
#
# # HELP rawrxd_active_connections Current number of active connections
# # TYPE rawrxd_active_connections gauge
# rawrxd_active_connections 12
```

### Grafana Dashboard

Import the included Grafana dashboard (`grafana-dashboard.json`):

1. Open Grafana
2. Go to Dashboards → Import
3. Upload `grafana-dashboard.json`
4. Configure Prometheus data source

Visualizations include:
- Request rate (req/s)
- Error rate (%)
- Response time (p50, p95, p99)
- Active connections
- WebSocket connections
- JWT validation success rate

---

## Complete Example: All Features Enabled

```powershell
# Configure all features
$env:RAWRXD_PORT_MIN = "15000"
$env:RAWRXD_PORT_MAX = "25000"
$env:RAWRXD_TLS_ENABLED = "true"
$env:RAWRXD_TLS_CERT = "server.crt"
$env:RAWRXD_TLS_KEY = "server.key"
$env:RAWRXD_JWT_ENABLED = "true"
$env:RAWRXD_JWT_SECRET = "super-secret-key-min-32-characters"
$env:RAWRXD_WEBSOCKET_ENABLED = "true"
$env:RAWRXD_SERVICE_REGISTRY_ENABLED = "true"
$env:RAWRXD_RATE_LIMIT_ENABLED = "true"
$env:RAWRXD_METRICS_ENABLED = "true"

# Start RawrXD
.\RawrXD-CLI.exe

# Output:
# [2026-01-15 10:00:00] [APIServer] [INFO] JWT authentication enabled
# [2026-01-15 10:00:00] [APIServer] [INFO] TLS/HTTPS enabled
# [2026-01-15 10:00:01] [APIServer] [INFO] Port 17234 is available
# [2026-01-15 10:00:01] [ServiceRegistry] [INFO] Service registered: rawrxd-api-17234
# [2026-01-15 10:00:01] [WebSocket] [INFO] WebSocket server started on port 17235
# [2026-01-15 10:00:01] [APIServer] [INFO] API Server ready on https://localhost:17234
```

---

## Security Best Practices

1. **Always use TLS in production**
   - Never send JWT tokens over unencrypted connections
   - Use valid certificates (Let's Encrypt, commercial CA)

2. **Rotate JWT secrets regularly**
   - Change `RAWRXD_JWT_SECRET` every 90 days
   - Revoke old tokens after rotation

3. **Enable rate limiting**
   - Prevents abuse and DoS attacks
   - Tune limits based on expected load

4. **Monitor metrics**
   - Set up alerts for error rate spikes
   - Track authentication failures

5. **Secure service registry**
   - Use authentication for external registries
   - Encrypt registry traffic (HTTPS)

6. **Firewall configuration**
   - Only expose necessary ports
   - Use VPN for remote access to management endpoints

---

## Troubleshooting

### TLS Handshake Failures

```
Error: SSL handshake failed
```

**Solution**: Check certificate validity and paths
```powershell
# Verify certificate
openssl x509 -in server.crt -text -noout

# Check file permissions
icacls server.key
```

### JWT Validation Errors

```
Error: Invalid token signature
```

**Solution**: Ensure JWT_SECRET matches on client and server
```powershell
# Regenerate token with correct secret
$env:RAWRXD_JWT_SECRET = "correct-secret-key"
```

### WebSocket Connection Refused

```
Error: WebSocket connection refused on port 11435
```

**Solution**: Check if WebSocket is enabled and port is available
```powershell
$env:RAWRXD_WEBSOCKET_ENABLED = "true"

# Check port availability
Test-NetConnection -Port 11435 -ComputerName localhost
```

### Service Registry Not Responding

```
Error: Failed to register with external registry
```

**Solution**: Verify registry URL and network connectivity
```powershell
# Test Consul connectivity
curl http://consul.local:8500/v1/status/leader

# Check DNS resolution
Resolve-DnsName consul.local
```

---

## Performance Tuning

### WebSocket Connections

```powershell
# Increase max connections for high-traffic scenarios
$env:RAWRXD_WEBSOCKET_MAX_CONNECTIONS = "500"
```

### Rate Limiting

```powershell
# Adjust for high-volume authenticated users
$env:RAWRXD_RATE_LIMIT_RPM = "300"
```

### Service Registry Heartbeat

```powershell
# Reduce network traffic (increase interval)
# Default: 60 seconds
# For stable environments: 300 seconds (5 min)
```

---

## Next Steps

- [PORT_CONFIGURATION.md](PORT_CONFIGURATION.md) - Basic port configuration
- [PORT_RANDOMIZATION_QUICK_GUIDE.md](PORT_RANDOMIZATION_QUICK_GUIDE.md) - Quick reference
- [WHY_HTTP_API.md](WHY_HTTP_API.md) - Architecture rationale
- [test-advanced-features.ps1](test-advanced-features.ps1) - Test suite

**Implementation Date**: January 15, 2026  
**Status**: Production Ready ✅
