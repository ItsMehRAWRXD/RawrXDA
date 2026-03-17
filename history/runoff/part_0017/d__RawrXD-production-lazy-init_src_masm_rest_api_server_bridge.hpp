/*
 * RESTAPIServer Bridge - C++ interface to MASM REST server
 * 
 * This header provides extern "C" declarations to call the pure MASM
 * REST API server implementation. All functions are thread-safe.
 *
 * Usage:
 *   #include "rest_api_server_bridge.hpp"
 *   
 *   // Get singleton
 *   void* server = RESTAPIServer_GetInstance();
 *   
 *   // Start on port 8080
 *   RESTAPIServer_Start(8080, nullptr);
 *
 * Copyright (c) 2025 RawrXD Project
 */

#ifndef REST_API_SERVER_BRIDGE_HPP
#define REST_API_SERVER_BRIDGE_HPP

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get RESTAPIServer singleton instance
 * Thread-safe via Windows InitOnceExecuteOnce
 * @return Pointer to server struct
 */
void* __cdecl RESTAPIServer_GetInstance(void);

/**
 * @brief Start the REST API server
 * @param port Port number (0 = default 8080)
 * @param callback Optional callback when started
 * @return 1 on success, 0 on failure
 */
int __cdecl RESTAPIServer_Start(uint16_t port, void (*callback)(int));

/**
 * @brief Stop the REST API server
 */
void __cdecl RESTAPIServer_Stop(void);

/**
 * @brief Check if server is running
 * @return 1 if running, 0 if not
 */
int __cdecl RESTAPIServer_IsRunning(void);

/**
 * @brief Get server port
 * @return Port number
 */
uint16_t __cdecl RESTAPIServer_GetPort(void);

/**
 * @brief Register a route handler
 * @param method HTTP method ("GET", "POST", etc.)
 * @param path Route path ("/api/something")
 * @param handler Handler function
 * @return 1 on success, 0 on failure
 */
int __cdecl RESTAPIServer_RegisterRoute(
    const char* method,
    const char* path,
    void (*handler)(void* request, void* response)
);

#ifdef __cplusplus
}

// C++ convenience namespace
namespace rawrxd {
namespace masm {

/**
 * @brief C++ wrapper for MASM REST API Server
 */
class MasmRESTServer {
public:
    static void* getInstance() {
        return RESTAPIServer_GetInstance();
    }
    
    static bool start(uint16_t port = 0) {
        return RESTAPIServer_Start(port, nullptr) != 0;
    }
    
    static void stop() {
        RESTAPIServer_Stop();
    }
    
    static bool isRunning() {
        return RESTAPIServer_IsRunning() != 0;
    }
    
    static uint16_t getPort() {
        return RESTAPIServer_GetPort();
    }
    
    // Deleted to prevent misuse
    MasmRESTServer() = delete;
    MasmRESTServer(const MasmRESTServer&) = delete;
    MasmRESTServer& operator=(const MasmRESTServer&) = delete;
};

} // namespace masm
} // namespace rawrxd

#endif // __cplusplus

#endif // REST_API_SERVER_BRIDGE_HPP
