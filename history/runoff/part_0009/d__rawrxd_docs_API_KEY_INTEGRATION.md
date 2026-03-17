# API Key Integration Architecture

## Overview

RawrXD IDE supports backend authentication via API keys for cloud-based AI services and enterprise features. This document describes the integration points and implementation strategy.

## Current State (2026-02-16)

### Existing Infrastructure

1. **Telemetry API Key** (`src/net/net.asm`)
   - Global storage: `g_szTelemetryApiKey` (256-byte buffer)
   - Initialization: `Telemetry_Init proc endpoint:dq, apiKey:dq`
   - HTTP header injection stubs at lines 21407-21640
   - Can be repurposed for general API key management

2. **Configuration System** (`rawrxd.config.json`)
   - JSON-based IDE settings
   - Loading: `ProductionConfigManager::loadConfig()`
   - Current structure: editor, theme, inference, ollama, agent, features
   - **Missing:** Dedicated `api` section for keys

3. **Backend Switcher** (`Win32IDE_BackendSwitcher.cpp`)
   - Probes environment variables:
     - `OPENAI_API_KEY` → OpenAI backend
     - `ANTHROPIC_API_KEY` → Claude backend
     - `GOOGLE_API_KEY` / `GEMINI_API_KEY` → Gemini backend
     - `GITHUB_COPILOT_TOKEN` → GitHub Copilot (VSIX)
     - `AWS_ACCESS_KEY_ID` → Amazon Q (VSIX)
   - Returns "Not configured" when keys missing

4. **User-Provided Key** (from session context)
   - Key: `key_1bbe2f4d33423a095fc03d9f873eb4a161a680df099e82410be7bb19e65c319f`
   - Intent: Integrate into Win32 GUI and CLI for backend auth
   - **Not yet wired** to config, HTTP client, or net.asm

## Integration Strategy

### Phase 1: Configuration Storage (PRIORITY)

**Add to `rawrxd.config.json`:**
```json
{
  "api": {
    "backend_key": "key_1bbe2f4d33423a095fc03d9f873eb4a161a680df099e82410be7bb19e65c319f",
    "endpoint": "https://api.example.com/v1",
    "timeout_ms": 30000,
    "retry_count": 3
  }
}
```

**Implementation:**
- File: `src/core/ProductionConfigManager.cpp`
- Add `ApiConfig` struct with `backend_key`, `endpoint`, `timeout_ms`, `retry_count`
- Parse from JSON during `loadConfig()`
- Expose via `ProductionConfigManager::getApiKey()` getter

### Phase 2: HTTP Client Integration

**Option A: WinHTTP Headers**
- File: `src/net/http_client.cpp` (if exists) or new `src/net/win32_http_client.cpp`
- Inject header: `Authorization: Bearer <key>`
- Call sites: All HTTP requests to backend APIs

**Option B: net.asm Telemetry Path**
- Repurpose `Telemetry_Init` → `ApiAuth_Init`
- Store key in `g_szTelemetryApiKey` → `g_szApiKey`
- Inject header in existing HTTP transmission stubs (lines 21407-21640)

### Phase 3: IDE/CLI Startup Wiring

**IDE Startup** (`Win32IDE.cpp`):
```cpp
void Win32IDE::initializeApiAuth() {
    auto& config = ProductionConfigManager::instance();
    std::string apiKey = config.getApiKey();
    if (!apiKey.empty()) {
        // Option A: Set global for HTTP client
        g_httpApiKey = apiKey;
        
        // Option B: Call net.asm
        extern "C" void ApiAuth_Init(const char* endpoint, const char* key);
        ApiAuth_Init(config.getApiEndpoint().c_str(), apiKey.c_str());
        
        LOG_INFO("API authentication configured");
    }
}
```

**CLI Startup** (`HeadlessIDE` / `main.cpp`):
- Same logic as IDE
- Fallback: Read from environment variable if config missing
- Priority: config.json > ENV > hardcoded default

### Phase 4: Backend Auth Flow

**Inference Request:**
1. User triggers model inference (chat, completion, etc.)
2. `AgenticBridge` prepares HTTP request
3. HTTP client adds `Authorization` header using stored key
4. Request sent to backend endpoint
5. Response validated (401 → "Invalid API key" error dialog)

**Error Handling:**
- **401 Unauthorized:** Show dialog "API key invalid or expired. Update in Settings → API Configuration."
- **403 Forbidden:** "API key lacks permissions for this operation."
- **Network errors:** Fallback to local inference if available

### Phase 5: Settings UI (Optional)

**Add to Settings Dialog:**
- Section: "API Configuration"
- Fields:
  - Backend API Key (password field, masked)
  - Endpoint URL (text field)
  - Test Connection button
- Save to `rawrxd.config.json` on Apply

**Test Connection:**
```cpp
bool testApiConnection() {
    // Send minimal request to /health or /models endpoint
    // Return true if 200 OK, false otherwise
}
```

## Security Considerations

1. **Storage:**
   - **Current:** Plain text in `rawrxd.config.json`
   - **Recommended:** Encrypt key using DPAPI (Windows) or keyring (cross-platform)
   - **Implementation:** `CryptProtectData` / `CryptUnprotectData` on Windows

2. **Transmission:**
   - **Always use HTTPS** for API requests
   - **TLS 1.2+ required**
   - Certificate validation enabled

3. **Logging:**
   - **Never log API keys** in debug output
   - Redact keys: `key_1bbe...e7bb` → `key_***...***bb` (first 8 + last 2 chars)

4. **Memory:**
   - Zero key buffers after use (`SecureZeroMemory` on Windows)
   - Avoid string copies where possible

## Implementation Checklist

- [ ] Add `api` section to `rawrxd.config.json` with user-provided key
- [ ] Parse API config in `ProductionConfigManager::loadConfig()`
- [ ] Add `getApiKey()` / `getApiEndpoint()` getters
- [ ] Implement `initializeApiAuth()` in Win32IDE and HeadlessIDE
- [ ] Wire API key to HTTP client (WinHTTP headers or net.asm)
- [ ] Add Authorization header injection
- [ ] Implement 401/403 error handling with user prompts
- [ ] Add Settings UI for API configuration (optional)
- [ ] Implement key encryption with DPAPI (optional but recommended)
- [ ] Add logging redaction for API keys
- [ ] Write unit tests for API auth flow

## Current Blockers

1. **Codebase Size:** 179K line .asm files cause grep/search timeouts
   - **Workaround:** Use targeted PowerShell `Select-String` commands
   - **Resolution:** Focus on specific entry points (config load, HTTP init)

2. **HTTP Client Location:** Unclear which file handles HTTP requests
   - **Next Step:** Search for `WinHttpSendRequest`, `InternetOpenUrl`, or similar
   - **Fallback:** Create new `win32_http_client.cpp` with WinHTTP wrapper

3. **Config Reload:** IDE may need restart after config changes
   - **Resolution:** Add "Reload Configuration" menu item (File → Reload Config)
   - **Bonus:** Watch `rawrxd.config.json` for changes, auto-reload

## Testing Plan

1. **Unit Tests:**
   - Config parsing with/without API key
   - Header injection (mock HTTP client)
   - Error handling (401/403 responses)

2. **Integration Tests:**
   - End-to-end inference request with valid key
   - Invalid key → error dialog flow
   - Fallback to local inference on auth failure

3. **Manual Tests:**
   - Update `rawrxd.config.json` with test key
   - Launch IDE → verify no errors
   - Trigger inference → check Authorization header in network logs
   - Test invalid key → verify error message

## References

- Telemetry API infrastructure: `src/net/net.asm` lines 21407-21640
- Config manager: `src/core/ProductionConfigManager.cpp`
- Backend switcher: `src/win32app/Win32IDE_BackendSwitcher.cpp`
- HTTP client (TBD): Search for WinHTTP/WinInet usage

## User-Provided Key (DO NOT COMMIT)

```
key_1bbe2f4d33423a095fc03d9f873eb4a161a680df099e82410be7bb19e65c319f
```

**WARNING:** This key is for development only. Never commit API keys to git. Use environment variables or encrypted config for production.
