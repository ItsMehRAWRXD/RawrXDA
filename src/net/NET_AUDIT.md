# NET_AUDIT.md

## Folder: `src/net/`

### Summary
This folder contains networking logic for the IDE/CLI, including rate limiting and (potentially) HTTP or socket communication. The code is implemented in C++ and may rely on external libraries for networking and compression. Some routines are stubs or placeholders for future MASM migration.

### Key Files Audited
- `rate_limiter.cpp`: Implements rate limiting logic for network requests. May use C++ standard library and platform-specific networking APIs.
- `NET_FOLDER_AUDIT.md`: Existing audit file (to be updated with MASM migration plan).

### External Dependencies
- C++ standard library (memory, threading, chrono)
- Platform-specific networking APIs (WinSock, POSIX sockets, etc.)
- Compression libraries (planned: zstd, gzip, lz4)

### Stub/Placeholder Routines
- Some networking routines may be stubbed or simplified for unsupported features.
- Compression and protocol handling may be incomplete or placeholder.

### MASM Migration Targets
- All networking routines to be ported to MASM (`masm/net.asm`, `ggml_masm/compression.asm`).
- Remove all external networking and compression library dependencies in favor of MASM64 implementations.
- Refactor rate limiting and protocol handling for MASM64 compatibility.

### Implementation Status
- [x] Created MASM64 networking routines (HttpGet, HttpPost, WebSocket, TcpConnect/Send/Recv) in `net.asm`
- [x] Created C-callable bridge header `net_masm_bridge.h`
- [x] Implemented C++ backend wrappers in `net_backend.cpp` (HttpClient, WebSocketClient, TcpClient)
- [x] Created regression tests in `test_net_ops.cpp`
- [x] Documented all MASM entry points

### Next Steps
- [ ] Implement full HTTP/TCP/WebSocket protocol logic in MASM (currently stubs)
- [ ] Compile and link MASM networking routines with C++ backend
- [ ] Run regression tests to validate MASM networking ops
- [ ] Integrate MASM networking with rate_limiter.cpp
- [ ] Remove all external networking and compression dependencies
