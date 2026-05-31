# UEC-X Protocol Specification
## Version 1.0.0

---

## 1. Overview

UEC-X uses JSON-RPC 2.0 as its primary protocol for communication between IDE adapters and the microkernel. This specification defines the message format, available methods, and error handling.

---

## 2. Message Format

### 2.1 Request

```json
{
  "jsonrpc": "2.0",
  "id": "request-id-123",
  "method": "uec/command/execute",
  "params": {
    "commandId": 42,
    "parameters": "base64-encoded-data"
  }
}
```

### 2.2 Response

```json
{
  "jsonrpc": "2.0",
  "id": "request-id-123",
  "result": {
    "success": true,
    "data": "base64-encoded-result"
  }
}
```

### 2.3 Error Response

```json
{
  "jsonrpc": "2.0",
  "id": "request-id-123",
  "error": {
    "code": -32602,
    "message": "Invalid parameters",
    "data": {
      "details": "Command ID not found"
    }
  }
}
```

### 2.4 Notification (No Response)

```json
{
  "jsonrpc": "2.0",
  "method": "uec/event/emit",
  "params": {
    "eventType": "extension.loaded",
    "payload": {}
  }
}
```

---

## 3. Error Codes

| Code | Message | Description |
|------|---------|-------------|
| -32700 | Parse error | Invalid JSON received |
| -32600 | Invalid Request | JSON is not a valid Request object |
| -32601 | Method not found | Method does not exist |
| -32602 | Invalid params | Invalid method parameters |
| -32603 | Internal error | Internal JSON-RPC error |
| -32000 | Server error | Reserved for implementation-defined server-errors |
| -32001 | Extension not found | Requested extension does not exist |
| -32002 | Command not found | Requested command does not exist |
| -32003 | Capability denied | Extension lacks required capability |
| -32004 | Timeout | Operation timed out |
| -32005 | Sandbox violation | Security policy violation |

---

## 4. Methods

### 4.1 Lifecycle Methods

#### `uec/initialize`

Initialize the UEC-X connection.

**Parameters:**
```json
{
  "clientInfo": {
    "name": "vscode-adapter",
    "version": "1.0.0"
  },
  "capabilities": ["commands", "events", "kv-store"]
}
```

**Result:**
```json
{
  "serverInfo": {
    "name": "UEC-X Microkernel",
    "version": "1.0.0"
  },
  "capabilities": ["commands", "events", "extensions", "kv-store"]
}
```

#### `uec/shutdown`

Gracefully shutdown the connection.

**Parameters:** `{}`

**Result:** `{}`

#### `uec/ping`

Health check.

**Parameters:** `{}`

**Result:**
```json
{
  "timestamp": 1234567890,
  "status": "healthy"
}
```

---

### 4.2 Command Methods

#### `uec/command/register`

Register a new command.

**Parameters:**
```json
{
  "commandId": 100,
  "name": "myExtension.myCommand",
  "requiredCapabilities": ["file:read"]
}
```

**Result:**
```json
{
  "success": true
}
```

#### `uec/command/execute`

Execute a registered command.

**Parameters:**
```json
{
  "commandId": 100,
  "parameters": "base64-encoded-params"
}
```

**Result:**
```json
{
  "success": true,
  "result": "base64-encoded-result"
}
```

#### `uec/command/unregister`

Unregister a command.

**Parameters:**
```json
{
  "commandId": 100
}
```

**Result:**
```json
{
  "success": true
}
```

---

### 4.3 Event Methods

#### `uec/event/subscribe`

Subscribe to events.

**Parameters:**
```json
{
  "eventTypes": ["extension.loaded", "extension.unloaded"],
  "filter": {
    "extensionId": 5
  }
}
```

**Result:**
```json
{
  "subscriptionId": "sub-12345"
}
```

#### `uec/event/unsubscribe`

Unsubscribe from events.

**Parameters:**
```json
{
  "subscriptionId": "sub-12345"
}
```

**Result:**
```json
{
  "success": true
}
```

#### `uec/event/emit` (Notification)

Emit an event.

**Parameters:**
```json
{
  "eventType": "custom.event",
  "source": 5,
  "payload": "base64-encoded-payload"
}
```

---

### 4.4 Extension Methods

#### `uec/extension/load`

Load an extension.

**Parameters:**
```json
{
  "path": "/path/to/extension.uex",
  "config": {
    "name": "My Extension",
    "version": "1.0.0",
    "capabilities": ["file:read", "network"],
    "maxMemoryMB": 512,
    "maxThreads": 4
  }
}
```

**Result:**
```json
{
  "extensionId": 5,
  "status": "loaded"
}
```

#### `uec/extension/unload`

Unload an extension.

**Parameters:**
```json
{
  "extensionId": 5
}
```

**Result:**
```json
{
  "success": true
}
```

#### `uec/extension/activate`

Activate an extension.

**Parameters:**
```json
{
  "extensionId": 5
}
```

**Result:**
```json
{
  "success": true
}
```

#### `uec/extension/deactivate`

Deactivate an extension.

**Parameters:**
```json
{
  "extensionId": 5
}
```

**Result:**
```json
{
  "success": true
}
```

#### `uec/extension/status`

Get extension status.

**Parameters:**
```json
{
  "extensionId": 5
}
```

**Result:**
```json
{
  "extensionId": 5,
  "name": "My Extension",
  "state": "active",
  "memoryUsage": 104857600,
  "threadCount": 2
}
```

---

### 4.5 KV Store Methods

#### `uec/kv/set`

Set a key-value pair.

**Parameters:**
```json
{
  "key": "myKey",
  "value": "myValue",
  "ttl": 3600
}
```

**Result:**
```json
{
  "success": true
}
```

#### `uec/kv/get`

Get a value by key.

**Parameters:**
```json
{
  "key": "myKey"
}
```

**Result:**
```json
{
  "value": "myValue",
  "type": "string"
}
```

#### `uec/kv/delete`

Delete a key.

**Parameters:**
```json
{
  "key": "myKey"
}
```

**Result:**
```json
{
  "success": true
}
```

#### `uec/kv/keys`

List keys matching a pattern.

**Parameters:**
```json
{
  "pattern": "my*"
}
```

**Result:**
```json
{
  "keys": ["myKey", "myOtherKey"]
}
```

---

### 4.6 System Methods

#### `uec/system/stats`

Get system statistics.

**Parameters:** `{}`

**Result:**
```json
{
  "dispatchCount": 12345,
  "dispatchErrors": 12,
  "ipcBytesSent": 104857600,
  "ipcBytesReceived": 52428800,
  "eventsEmitted": 5678,
  "commandsRegistered": 256,
  "extensionsLoaded": 10
}
```

#### `uec/system/health`

Get health status.

**Parameters:** `{}`

**Result:**
```json
{
  "healthy": true,
  "status": "All systems operational",
  "subsystems": {
    "CommandRegistry": true,
    "EventBus": true,
    "Scheduler": true,
    "ExtensionHost": true,
    "SecuritySandbox": true,
    "KVStore": true
  }
}
```

---

## 5. Transport

### 5.1 Named Pipe (Windows)

Default pipe name: `\\.\pipe\UEC-X-Microkernel`

Message framing: Length-prefixed JSON
```
[4 bytes: message length][N bytes: JSON message]
```

### 5.2 Unix Domain Socket (Unix/Linux)

Default socket path: `/tmp/uec-x-microkernel.sock`

Same framing as named pipes.

### 5.3 TCP Socket

Default port: 9942

Same framing as named pipes.

---

## 6. Event Types

### 6.1 System Events

| Event Type | Description | Payload |
|------------|-------------|---------|
| `system.startup` | Microkernel started | `{ "version": "1.0.0" }` |
| `system.shutdown` | Microkernel shutting down | `{}` |
| `system.error` | System error | `{ "code": 123, "message": "..." }` |

### 6.2 Extension Events

| Event Type | Description | Payload |
|------------|-------------|---------|
| `extension.loaded` | Extension loaded | `{ "extensionId": 5, "name": "..." }` |
| `extension.unloaded` | Extension unloaded | `{ "extensionId": 5 }` |
| `extension.activated` | Extension activated | `{ "extensionId": 5 }` |
| `extension.deactivated` | Extension deactivated | `{ "extensionId": 5 }` |
| `extension.error` | Extension error | `{ "extensionId": 5, "error": "..." }` |

### 6.3 Command Events

| Event Type | Description | Payload |
|------------|-------------|---------|
| `command.registered` | Command registered | `{ "commandId": 100, "name": "..." }` |
| `command.unregistered` | Command unregistered | `{ "commandId": 100 }` |
| `command.executed` | Command executed | `{ "commandId": 100, "duration": 123 }` |
| `command.failed` | Command failed | `{ "commandId": 100, "error": "..." }` |

---

## 7. Version Compatibility

| Protocol Version | Microkernel Version | Status |
|------------------|---------------------|--------|
| 2.0 | 1.0.x | Current |

---

## 8. Security

All connections must be authenticated. See UEC-X Security Specification for details on:
- Authentication mechanisms
- Capability-based access control
- Message encryption
- Audit logging
