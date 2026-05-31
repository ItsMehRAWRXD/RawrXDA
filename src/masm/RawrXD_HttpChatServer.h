/**
 * @file RawrXD_HttpChatServer.h
 * @brief C/C++ interface for the pure x64 MASM HTTP Chat Server implementation
 * 
 * This header provides the public API for integrating the MASM-based HTTP client
 * and Python chat server management into RawrXD applications.
 * 
 * Features:
 * - Process management for Python chat server with race condition prevention
 * - WinINet-based HTTP client with connection pooling
 * - UTF-8/UTF-16 conversion for JSON payloads
 * - Proper resource lifecycle management
 * 
 * @note Link with: RawrXD_HttpChatServer.obj kernel32.lib user32.lib wininet.lib shlwapi.lib
 */

#ifndef RAWRXD_HTTP_CHAT_SERVER_H
#define RAWRXD_HTTP_CHAT_SERVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Constants
// ═══════════════════════════════════════════════════════════════════════════

#define RAWRXD_CHAT_PORT            23959
#define RAWRXD_MAX_MESSAGE_SIZE     65536
#define RAWRXD_MAX_RESPONSE_SIZE    1048576
#define RAWRXD_DEFAULT_TIMEOUT_MS   30000

// HTTP Status codes returned by HttpClient_Post
#define RAWRXD_HTTP_OK              200
#define RAWRXD_HTTP_CREATED         201
#define RAWRXD_HTTP_NO_CONTENT      204

// Error codes (Win32 errors will be > 1000)
#define RAWRXD_ERROR_SUCCESS        0
#define RAWRXD_ERROR_NOT_FOUND      2
#define RAWRXD_ERROR_ACCESS_DENIED  5
#define RAWRXD_ERROR_TIMEOUT        12002
#define RAWRXD_ERROR_CONNECT_FAIL   12029

// ═══════════════════════════════════════════════════════════════════════════
// Server Management API
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Start the Python chat server process
 * 
 * Locates Python in the system PATH, finds chat_server.py in the application
 * directory, and launches the server process. Uses mutex to prevent multiple
 * server instances.
 * 
 * @return true if server started or already running, false on error
 * 
 * @note The server is started with CREATE_NO_WINDOW | DETACHED_PROCESS flags
 * @note Includes 500ms startup delay for server initialization
 */
bool StartChatServer(void);

/**
 * @brief Stop the Python chat server process
 * 
 * Attempts graceful shutdown within the timeout period, then force-terminates
 * if the process doesn't exit cleanly.
 * 
 * @param timeoutMs Milliseconds to wait for graceful shutdown (0 = force immediate)
 * @return true if server stopped, false on error
 */
bool StopChatServer(uint32_t timeoutMs);

/**
 * @brief Check if the chat server process is running
 * 
 * Queries the process exit code to determine if still active.
 * 
 * @return true if server process is running
 */
bool ChatServer_IsRunning(void);

// ═══════════════════════════════════════════════════════════════════════════
// HTTP Client API
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Initialize the WinINet HTTP session
 * 
 * Creates the internet handle and configures timeouts. Called automatically
 * by HttpClient_Connect and HttpClient_Post if not already initialized.
 * 
 * @return true on success
 */
bool HttpClient_Initialize(void);

/**
 * @brief Connect to an HTTP server
 * 
 * Establishes a connection to the specified host and port. The connection
 * is kept open for reuse across multiple requests.
 * 
 * @param hostname Server hostname (wide string, e.g., L"localhost")
 * @param port Server port (e.g., 23959)
 * @return true on success
 */
bool HttpClient_Connect(const wchar_t* hostname, uint16_t port);

/**
 * @brief Send an HTTP POST request
 * 
 * Sends a POST request with the specified data and reads the response.
 * Automatically connects to localhost:23959 if not already connected.
 * 
 * @param urlPath URL path (wide string, e.g., L"/api/chat")
 * @param postData POST body data (UTF-8 encoded)
 * @param postDataLen Length of POST data in bytes
 * @param responseBuffer Buffer to receive response
 * @param responseBufferSize Size of response buffer
 * @return HTTP status code (200 on success), or Win32 error code (> 1000) on failure
 */
uint32_t HttpClient_Post(
    const wchar_t* urlPath,
    const char* postData,
    uint32_t postDataLen,
    char* responseBuffer,
    uint32_t responseBufferSize
);

/**
 * @brief Clean up HTTP client resources
 * 
 * Closes all open handles (request, connection, internet session).
 * Should be called during application shutdown.
 */
void HttpClient_Cleanup(void);

// ═══════════════════════════════════════════════════════════════════════════
// High-Level Chat API
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Send a chat message and receive the response
 * 
 * High-level API that handles JSON encoding, server startup, and HTTP
 * communication. Automatically escapes special characters in the message.
 * 
 * @param message Chat message text (UTF-8 encoded)
 * @param responseBuffer Buffer to receive JSON response
 * @param responseBufferSize Size of response buffer
 * @return true on success (HTTP 200), false on error
 * 
 * @note The server is automatically started if not running
 * @note Response is null-terminated if buffer has space
 * 
 * Example usage:
 * @code
 * char response[4096];
 * if (SendChatMessage("Hello, how are you?", response, sizeof(response))) {
 *     printf("Response: %s\n", response);
 * }
 * @endcode
 */
bool SendChatMessage(
    const char* message,
    char* responseBuffer,
    uint32_t responseBufferSize
);

// ═══════════════════════════════════════════════════════════════════════════
// Shutdown API
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Complete shutdown of the chat server subsystem
 * 
 * Stops the server process, cleans up HTTP resources, and releases the mutex.
 * Call this during application exit or when unloading the module.
 * 
 * @return true on success
 */
bool RawrXD_ChatServer_Shutdown(void);

#ifdef __cplusplus
}

// ═══════════════════════════════════════════════════════════════════════════
// C++ RAII Wrapper (optional)
// ═══════════════════════════════════════════════════════════════════════════

#include <string>
#include <memory>
#include <stdexcept>

namespace RawrXD {

/**
 * @brief RAII wrapper for the chat server subsystem
 * 
 * Automatically initializes on construction and shuts down on destruction.
 */
class ChatServerSession {
public:
    ChatServerSession() {
        if (!StartChatServer()) {
            throw std::runtime_error("Failed to start chat server");
        }
    }
    
    ~ChatServerSession() {
        RawrXD_ChatServer_Shutdown();
    }
    
    // Non-copyable
    ChatServerSession(const ChatServerSession&) = delete;
    ChatServerSession& operator=(const ChatServerSession&) = delete;
    
    // Movable
    ChatServerSession(ChatServerSession&&) = default;
    ChatServerSession& operator=(ChatServerSession&&) = default;
    
    /**
     * @brief Send a message and get the response
     * @param message The message to send
     * @return The server's response
     * @throws std::runtime_error on communication failure
     */
    std::string chat(const std::string& message) {
        std::unique_ptr<char[]> buffer(new char[RAWRXD_MAX_RESPONSE_SIZE]);
        
        if (!SendChatMessage(message.c_str(), buffer.get(), RAWRXD_MAX_RESPONSE_SIZE)) {
            throw std::runtime_error("Chat request failed");
        }
        
        return std::string(buffer.get());
    }
    
    /**
     * @brief Check if the server is running
     */
    bool isRunning() const {
        return ChatServer_IsRunning();
    }
};

} // namespace RawrXD

#endif // __cplusplus

#endif // RAWRXD_HTTP_CHAT_SERVER_H
