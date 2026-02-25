// ============================================================================
// RawrXD_SocketResponder.h — IOCP Zero-Copy HTTP Server C++ Bridge
// ============================================================================
//
// MASM64 high-performance HTTP responder for RawrXD IDE
// Serves /health, /status, /models with sub-0.5ms latency via IOCP
// Pre-serialized responses, zero heap allocation in hot path
//
// Binary: RawrXD_SocketResponder.asm → linked into RawrXD-Win32IDE
// Dependencies: ws2_32.lib
//
// Pattern:  RAX=0 success, non-zero failure
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>

// ============================================================================
// Server Statistics (matches ASM layout)
// ============================================================================
struct RawrXD_ServerStats {
    uint64_t hitsHealth;
    uint64_t hitsStatus;
    uint64_t hitsModels;
    uint64_t hits404;
    uint64_t hitsCORS;
    uint64_t totalConnections;
    uint64_t activeConnections;
    uint64_t uptimeSeconds;
};

// ============================================================================
// C-Linkage Declarations for MASM64 Exported Procedures
// ============================================================================
#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// RawrXD_StartSocketServer
//   Initialize Winsock, create IOCP, bind to port, spawn worker/accept threads.
//   port: TCP port number (e.g. 11435)
//   Returns: 0 on success, -1 on failure
// ---------------------------------------------------------------------------
int64_t RawrXD_StartSocketServer(uint32_t port);

// ---------------------------------------------------------------------------
// RawrXD_StopSocketServer
//   Signal all threads to exit, close IOCP, cleanup Winsock.
//   Returns: 0 on success
// ---------------------------------------------------------------------------
int64_t RawrXD_StopSocketServer(void);

// ---------------------------------------------------------------------------
// RawrXD_IsServerRunning
//   Returns: 1 if server is running, 0 if stopped
// ---------------------------------------------------------------------------
int64_t RawrXD_IsServerRunning(void);

// ---------------------------------------------------------------------------
// RawrXD_UpdateHealthCache
//   Write new pre-serialized /health response body.
//   data: JSON string (null-terminated)
//   len:  byte length excluding null
//   Returns: 0 on success
// ---------------------------------------------------------------------------
int64_t RawrXD_UpdateHealthCache(const char* data, uint32_t len);

// ---------------------------------------------------------------------------
// RawrXD_UpdateStatusCache
//   Write new pre-serialized /status response body.
//   data: JSON string (null-terminated)
//   len:  byte length excluding null
//   Returns: 0 on success
// ---------------------------------------------------------------------------
int64_t RawrXD_UpdateStatusCache(const char* data, uint32_t len);

// ---------------------------------------------------------------------------
// RawrXD_UpdateModelsCache
//   Write new pre-serialized /models response body.
//   data: JSON string (null-terminated)
//   len:  byte length excluding null (max 128KB)
//   Returns: 0 on success
// ---------------------------------------------------------------------------
int64_t RawrXD_UpdateModelsCache(const char* data, uint32_t len);

// ---------------------------------------------------------------------------
// RawrXD_GetServerStats
//   Fill a stats structure with current counters.
//   stats: pointer to RawrXD_ServerStats
//   Returns: 0 on success
// ---------------------------------------------------------------------------
int64_t RawrXD_GetServerStats(RawrXD_ServerStats* stats);

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ RAII Wrapper
// ============================================================================
#ifdef __cplusplus

class RawrXDSocketServer {
public:
    explicit RawrXDSocketServer(uint32_t port) : m_running(false) {
        if (RawrXD_StartSocketServer(port) == 0) {
            m_running = true;
        }
    }

    ~RawrXDSocketServer() {
        if (m_running) {
            RawrXD_StopSocketServer();
        }
    }

    // Non-copyable
    RawrXDSocketServer(const RawrXDSocketServer&) = delete;
    RawrXDSocketServer& operator=(const RawrXDSocketServer&) = delete;

    bool isRunning() const { return RawrXD_IsServerRunning() != 0; }

    void updateHealth(const char* json, uint32_t len) {
        RawrXD_UpdateHealthCache(json, len);
    }

    void updateStatus(const char* json, uint32_t len) {
        RawrXD_UpdateStatusCache(json, len);
    }

    void updateModels(const char* json, uint32_t len) {
        RawrXD_UpdateModelsCache(json, len);
    }

    RawrXD_ServerStats getStats() const {
        RawrXD_ServerStats s{};
        RawrXD_GetServerStats(&s);
        return s;
    }

private:
    bool m_running;
};

#endif // __cplusplus
