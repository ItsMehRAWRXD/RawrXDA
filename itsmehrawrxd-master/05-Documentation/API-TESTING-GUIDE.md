# RawrZ Security Platform API Testing Guide

This guide provides comprehensive instructions for testing the RawrZ Security Platform API endpoints externally using curl, PowerShell, or other HTTP clients.

## Base URL
```
http://localhost:3000
```

## Available Endpoints

### 1. Health Check
**GET** `/health`
```bash
curl -s http://localhost:3000/health
```

### 2. Simple Test
**GET** `/api/simple-test`
```bash
curl -s http://localhost:3000/api/simple-test
```

### 3. HTTP Bot Test
**GET** `/api/http-bot-test`
```bash
curl -s http://localhost:3000/api/http-bot-test
```

### 4. Engine Status
**GET** `/api/rawrz-engine/status`
```bash
curl -s http://localhost:3000/api/rawrz-engine/status
```

### 5. Engine Test (POST)
**POST** `/api/test-engine`

#### HTTP Bot Manager
```bash
curl -s -X POST -H "Content-Type: application/json" -d @test-engine-request.json http://localhost:3000/api/test-engine
```

#### CVE Analysis Engine
```bash
curl -s -X POST -H "Content-Type: application/json" -d @test-cve-request.json http://localhost:3000/api/test-engine
```

#### Payload Manager
```bash
curl -s -X POST -H "Content-Type: application/json" -d @test-payload-request.json http://localhost:3000/api/test-engine
```

#### Plugin Architecture
```bash
curl -s -X POST -H "Content-Type: application/json" -d @test-plugin-request.json http://localhost:3000/api/test-engine
```

## Test Files

The following JSON files are provided for testing:

- `test-engine-request.json` - HTTP Bot Manager test
- `test-cve-request.json` - CVE Analysis Engine test
- `test-payload-request.json` - Payload Manager test
- `test-plugin-request.json` - Plugin Architecture test

## PowerShell Testing

For Windows users, you can use PowerShell with Invoke-RestMethod:

```powershell
# Health Check
Invoke-RestMethod -Uri "http://localhost:3000/health" -Method GET

# HTTP Bot Test
Invoke-RestMethod -Uri "http://localhost:3000/api/http-bot-test" -Method GET

# Engine Test (using file)
$body = Get-Content "test-engine-request.json" -Raw
Invoke-RestMethod -Uri "http://localhost:3000/api/test-engine" -Method POST -ContentType "application/json" -Body $body
```

## Expected Responses

### HTTP Bot Manager Response
```json
{
  "success": true,
  "data": [
    {
      "id": "test-bot-1",
      "status": "online",
      "capabilities": {
        "fileManager": true,
        "processManager": true,
        "systemInfo": true,
        "networkTools": true,
        "keylogger": true,
        "screenCapture": true,
        "webcamCapture": true,
        "audioCapture": true,
        "browserStealer": true,
        "cryptoStealer": true,
        "registryEditor": true,
        "serviceManager": true,
        "scheduledTasks": true,
        "persistence": true,
        "antiAnalysis": true,
        "stealth": true
      },
      "system": {
        "os": "Windows 10",
        "arch": "x64",
        "user": "testuser",
        "hostname": "TEST-PC",
        "ip": "192.168.1.100",
        "country": "US"
      }
    }
  ]
}
```

### CVE Analysis Response
```json
{
  "success": true,
  "data": {
    "cveId": "CVE-2023-1234",
    "severity": "High",
    "cvssScore": 8.5,
    "description": "Buffer overflow vulnerability in test application",
    "affectedVersions": ["1.0.0", "1.1.0"],
    "patchAvailable": true,
    "exploitAvailable": false
  }
}
```

### Payload Manager Response
```json
{
  "success": true,
  "data": [
    {
      "name": "basic-payload.exe",
      "type": "executable",
      "size": 49152,
      "created": "2025-09-20T01:09:00.058Z",
      "status": "ready"
    },
    {
      "name": "advanced-payload.dll",
      "type": "library",
      "size": 65536,
      "created": "2025-09-20T01:09:00.058Z",
      "status": "ready"
    }
  ]
}
```

### Plugin Architecture Response
```json
{
  "success": true,
  "data": [
    {
      "name": "stealth-plugin",
      "version": "1.0.0",
      "status": "active",
      "description": "Advanced stealth capabilities"
    },
    {
      "name": "encryption-plugin",
      "version": "2.1.0",
      "status": "active",
      "description": "Multi-algorithm encryption support"
    }
  ]
}
```

## Running the Test Script

A comprehensive test script is provided:

```bash
chmod +x test-api.sh
./test-api.sh
```

## Troubleshooting

### JSON Parsing Errors
If you encounter JSON parsing errors, ensure:
1. The JSON is properly formatted
2. Use the provided test files instead of inline JSON
3. Check that the Content-Type header is set to `application/json`

### Connection Errors
If you can't connect to the API:
1. Ensure the server is running on port 3000
2. Check that no firewall is blocking the connection
3. Verify the base URL is correct

### Server Not Responding
If the server isn't responding:
1. Check server logs for errors
2. Restart the server: `node api-server.js`
3. Verify all dependencies are installed

## Advanced Features

The API now includes:
-  Proper PE (Portable Executable) structure generation for advanced encryption
-  48KB+ encrypted file generation instead of 1KB files
-  External API accessibility for curl testing
-  Comprehensive engine testing endpoints
-  Proper JSON parsing with error handling
-  User-friendly panel interfaces instead of raw JSON

## Security Note

This API is designed for legitimate security testing and research purposes only. Ensure you have proper authorization before testing on any systems.
