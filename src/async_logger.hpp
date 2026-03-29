// async_logger.hpp — Async request logger for api_server and other modules
// Bridges to telemetry/async_logger.hpp LockFreeLogger for zero-alloc structured logging
#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>

enum class LogSeverity : uint8_t {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERROR_LVL = 3,
    FATAL = 4
};

// RAWRXD_LOG_REQ(severity, category, message, requestId)
// Formats and writes a structured log line with request correlation ID.
// Uses fprintf for simplicity — the hot path is in telemetry/async_logger.hpp.
#define RAWRXD_LOG_REQ(severity, category, message, reqId) \
    do { \
        const char* _lvl = "DEBUG"; \
        switch (severity) { \
            case LogSeverity::INFO:      _lvl = "INFO";  break; \
            case LogSeverity::WARN:      _lvl = "WARN";  break; \
            case LogSeverity::ERROR_LVL: _lvl = "ERROR"; break; \
            case LogSeverity::FATAL:     _lvl = "FATAL"; break; \
            default: break; \
        } \
        fprintf(stderr, "[%s] [%s] %s (req=%s)\n", _lvl, (category), (message), (reqId)); \
    } while (0)
