# CURSOR REVERSE ENGINEERING - QUICK REFERENCE

## 🎯 CRITICAL FINDINGS (ONE-PAGE)

### API Endpoint
```
https://api2.cursor.sh
```

### Local Proxy
```javascript
http://127.0.0.1:${dynamic_port}
ANTHROPIC_API_KEY via createProxyAuthHeaderValue()
```

### Claude Agent SDK
```
Version: 0.2.4
Location: cursor-agent/dist/claude-agent-sdk/
Entry: sdk-ts
```

### Key Extensions
```
cursor-agent          - Core AI (3.5MB)
cursor-agent-exec     - Command execution
cursor-mcp            - Model Context Protocol (3.4MB)
cursor-retrieval      - Code search
cursor-file-service   - File indexing
```

### Extracted Code Statistics
```
Files:      10 JavaScript files
Size:       22.59 MB
Functions:  302
Classes:    133
Exports:    19
```

### File Locations
```
Source:     D:\Cursor_Source_Complete\
Critical:   D:\Cursor_Critical_Source\
Analysis:   D:\Cursor_Analysis_Results\
Reports:    D:\CURSOR_*.md
```

### Environment Variables
```
CLAUDE_AGENT_SDK_VERSION="0.2.4"
CLAUDE_CODE_ENTRYPOINT="sdk-ts"
CLAUDE_CODE_ENABLE_SDK_FILE_CHECKPOINTING="true"
ANTHROPIC_API_KEY=(injected)
ANTHROPIC_BASE_URL=http://127.0.0.1:${port}
```

### Request Flow
```
User → Cursor → Proxy (127.0.0.1) → api2.cursor.sh → Anthropic
```

### Key Technologies
```
- TypeScript → JavaScript (compiled)
- WebAssembly (tree-sitter, resvg)
- Model Context Protocol (MCP)
- VS Code Extension API
- Electron
- Node.js
```

### Tools Built
```
1. OMEGA-POLYGLOT v4.0 PRO     (D:\lazy init ide\omega_pro.exe)
2. cursor_dump.ps1             (Full extraction)
3. cursor_js_analyzer.ps1      (Code analysis)
```

### Next Steps
```
1. De-minify main.js (3.5MB)
2. Extract createProxyAuthHeaderValue() logic
3. Test api2.cursor.sh API
4. Build working proxy clone
5. Implement MCP protocol
```

### Error Codes
```
API_KEY_NOT_SUPPORTED = 24
API_KEY_RATE_LIMIT = 34
BAD_API_KEY = 1
BAD_USER_API_KEY = 42
```

---
**Status:** ✅ FULLY EXTRACTED & ANALYZED  
**Date:** January 24, 2026  
**Success:** 100%
