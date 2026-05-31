// ═════════════════════════════════════════════════════════════════════════════
// RawrXD_NativeHttpServer.h - C++ Interface to Native HTTP Server
// Kernel-mode HTTP server using http.sys (IIS-shared driver)
// Zero Python dependency, pure x64 MASM implementation
// ═════════════════════════════════════════════════════════════════════════════

#ifndef RAWRXD_NATIVE_HTTP_SERVER_H
#define RAWRXD_NATIVE_HTTP_SERVER_H

#pragma once

#include <cstdint>
#include <string>
#include <memory>

// ═════════════════════════════════════════════════════════════════════════════
// C-LINKAGE EXTERN FUNCTIONS (MASM exports)
// ═════════════════════════════════════════════════════════════════════════════

extern "C"
{
    /// Initialize native HTTP server using http.sys kernel API
    /// @param port Port number to listen on (default 23959)
    /// @return 0 on success, non-zero on error
    uint32_t __stdcall HttpServer_Initialize(uint32_t port);

    /// Shutdown HTTP server and clean up resources
    /// @return 0 on success
    uint32_t __stdcall HttpServer_Shutdown();

    /// Check if server is currently running
    /// @return 1 if running, 0 if stopped
    uint8_t __stdcall HttpServer_IsRunning();

    /// Load inference model for /api/chat and /api/generate endpoints
    /// @param modelPath Path to model file (GGUF format)
    /// @return 0 on success, non-zero on error
    uint32_t __stdcall HttpServer_LoadModel(const char* modelPath);

    /// Get server statistics (requests/responses processed)
    /// @return High 32-bits: request count, Low 32-bits: response count
    uint64_t __stdcall HttpServer_GetStatus();
}

// ═════════════════════════════════════════════════════════════════════════════
// C++ WRAPPER CLASS (RAII Pattern)
// ═════════════════════════════════════════════════════════════════════════════

namespace RawrXD
{
    class NativeHttpServer
    {
    public:
        /// Constructor - initialize with port
        /// Throws std::runtime_error if initialization fails
        explicit NativeHttpServer(uint16_t port = 23959)
            : m_port(port), m_initialized(false)
        {
            uint32_t result = HttpServer_Initialize(m_port);
            if (result != 0)
            {
                std::string message = "Failed to initialize HTTP server (code=";
                message += std::to_string(result);
                message += ")";
                throw std::runtime_error(message);
            }
            m_initialized = true;
        }

        /// Destructor - automatically shutdown server
        ~NativeHttpServer()
        {
            if (m_initialized)
            {
                HttpServer_Shutdown();
                m_initialized = false;
            }
        }

        /// Non-copyable
        NativeHttpServer(const NativeHttpServer&) = delete;
        NativeHttpServer& operator=(const NativeHttpServer&) = delete;

        /// Movable
        NativeHttpServer(NativeHttpServer&& other) noexcept
            : m_port(other.m_port), m_initialized(other.m_initialized)
        {
            other.m_initialized = false;
        }

        NativeHttpServer& operator=(NativeHttpServer&& other) noexcept
        {
            if (this != &other)
            {
                if (m_initialized)
                {
                    HttpServer_Shutdown();
                }
                m_port = other.m_port;
                m_initialized = other.m_initialized;
                other.m_initialized = false;
            }
            return *this;
        }

        /// Load inference model for chat/generate endpoints
        /// @param modelPath Path to model file
        /// @throws std::runtime_error if model loading fails
        void LoadModel(const std::string& modelPath)
        {
            if (!m_initialized)
            {
                throw std::runtime_error("HTTP server not initialized");
            }

            uint32_t result = HttpServer_LoadModel(modelPath.c_str());
            if (result != 0)
            {
                throw std::runtime_error("Failed to load model: " + modelPath);
            }
        }

        /// Check if server is running
        /// @return true if running, false if stopped
        bool IsRunning() const
        {
            return HttpServer_IsRunning() != 0;
        }

        /// Get server statistics
        /// @return pair of (request_count, response_count)
        std::pair<uint32_t, uint32_t> GetStatus() const
        {
            uint64_t status = HttpServer_GetStatus();
            uint32_t requests = static_cast<uint32_t>(status >> 32);
            uint32_t responses = static_cast<uint32_t>(status & 0xFFFFFFFF);
            return { requests, responses };
        }

        /// Get configured port number
        /// @return port number
        uint16_t GetPort() const { return m_port; }

        /// Check if properly initialized
        /// @return true if initialized, false otherwise
        bool IsInitialized() const { return m_initialized; }

    private:
        uint16_t m_port;
        bool m_initialized;
    };

    /// Smart pointer type for HTTP server
    using NativeHttpServerPtr = std::unique_ptr<NativeHttpServer>;

    /// Factory function to create HTTP server
    /// @param port Port to listen on (default 23959)
    /// @return unique_ptr to NativeHttpServer
    inline NativeHttpServerPtr CreateNativeHttpServer(uint16_t port = 23959)
    {
        return std::make_unique<NativeHttpServer>(port);
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// CONSTANTS AND CONFIGURATION
// ═════════════════════════════════════════════════════════════════════════════

/// Default HTTP server port
constexpr uint16_t NATIVE_HTTP_SERVER_DEFAULT_PORT = 23959;

/// Maximum request size (64KB)
constexpr size_t NATIVE_HTTP_SERVER_MAX_REQUEST_SIZE = 65536;

/// Maximum response size (1MB)
constexpr size_t NATIVE_HTTP_SERVER_MAX_RESPONSE_SIZE = 1048576;

/// Maximum worker threads for concurrent requests
constexpr size_t NATIVE_HTTP_SERVER_MAX_WORKERS = 4;

// ═════════════════════════════════════════════════════════════════════════════
// HTTP ENDPOINT PATHS (for reference)
// ═════════════════════════════════════════════════════════════════════════════

/// Health check endpoint
constexpr const char* HTTP_ENDPOINT_HEALTH = "/health";

/// Chat endpoint (POST with JSON message)
constexpr const char* HTTP_ENDPOINT_CHAT = "/api/chat";

/// Generate endpoint (POST with prompt)
constexpr const char* HTTP_ENDPOINT_GENERATE = "/api/generate";

/// Models list endpoint
constexpr const char* HTTP_ENDPOINT_MODELS = "/api/models";

#endif // RAWRXD_NATIVE_HTTP_SERVER_H
