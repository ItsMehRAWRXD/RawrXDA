// UEC-X Microkernel - Core Header
// Universal Extension Core eXtended - Microkernel Layer
// Copyright (c) 2026 RawrXD Project

#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>

// Platform detection
#ifdef _WIN32
    #define UEC_PLATFORM_WINDOWS
    #include <windows.h>
#else
    #define UEC_PLATFORM_UNIX
#endif

// Version information
#define UEC_VERSION_MAJOR 1
#define UEC_VERSION_MINOR 0
#define UEC_VERSION_PATCH 0
#define UEC_VERSION_STRING "1.0.0"

// Export macros
#ifdef UEC_BUILDING_CORE
    #ifdef _WIN32
        #define UEC_API __declspec(dllexport)
    #else
        #define UEC_API __attribute__((visibility("default")))
    #endif
#else
    #ifdef _WIN32
        #define UEC_API __declspec(dllimport)
    #else
        #define UEC_API
    #endif
#endif

// Namespace
namespace uec {

// =============================================================================
// Forward Declarations
// =============================================================================
class Microkernel;
class CommandRegistry;
class EventBus;
class Scheduler;
class ExtensionHost;
class SecuritySandbox;
class KVStore;
class HotpatchEngine;
class IPCChannel;
class IDEPort;

// =============================================================================
// Core Types
// =============================================================================

using CommandId = uint32_t;
using ExtensionId = uint32_t;
using EventType = uint32_t;
using CapabilityMask = uint64_t;
using Timestamp = std::chrono::steady_clock::time_point;

// Error codes (must match MASM64 definitions)
enum class ErrorCode : int32_t {
    Success = 0,
    InvalidParameter = 1,
    NoMemory = 2,
    NotFound = 3,
    Busy = 4,
    PermissionDenied = 5,
    Timeout = 6,
    Cancelled = 7,
    AlreadyExists = 8,
    NotInitialized = 9,
    InternalError = 10
};

// Command types
enum class CommandType : uint32_t {
    Register = 1,
    Unregister = 2,
    Execute = 3,
    EmitEvent = 4,
    QueryState = 5,
    Hotpatch = 6
};

// Extension states
enum class ExtensionState : uint32_t {
    Unloaded = 0,
    Loading = 1,
    Loaded = 2,
    Activating = 3,
    Active = 4,
    Deactivating = 5,
    Error = 6,
    Unloading = 7
};

// Capability flags
enum class Capability : uint64_t {
    None = 0,
    FileRead = 1ULL << 0,
    FileWrite = 1ULL << 1,
    FileDelete = 1ULL << 2,
    Network = 1ULL << 3,
    ProcessSpawn = 1ULL << 4,
    ProcessKill = 1ULL << 5,
    MemoryMap = 1ULL << 6,
    MemoryAllocate = 1ULL << 7,
    IPCSend = 1ULL << 8,
    IPCReceive = 1ULL << 9,
    Hotpatch = 1ULL << 10,
    Debug = 1ULL << 11,
    Telemetry = 1ULL << 12,
    AIInference = 1ULL << 13,
    All = 0xFFFFFFFFFFFFFFFFULL
};

// IPC transport types
enum class IPCTransport : uint32_t {
    NamedPipe = 0,
    SharedMemory = 1,
    Socket = 2,
    Loopback = 3
};

// =============================================================================
// Result Type
// =============================================================================

template<typename T>
class Result {
public:
    Result(T value) : m_value(std::move(value)), m_error(ErrorCode::Success) {}
    Result(ErrorCode error) : m_error(error) {}
    
    bool IsSuccess() const { return m_error == ErrorCode::Success; }
    bool IsError() const { return m_error != ErrorCode::Success; }
    
    T& Value() { return m_value; }
    const T& Value() const { return m_value; }
    ErrorCode Error() const { return m_error; }
    
    T ValueOr(T defaultValue) const {
        return IsSuccess() ? m_value : defaultValue;
    }
    
private:
    T m_value;
    ErrorCode m_error;
};

// Void specialization
template<>
class Result<void> {
public:
    Result() : m_error(ErrorCode::Success) {}
    Result(ErrorCode error) : m_error(error) {}
    
    bool IsSuccess() const { return m_error == ErrorCode::Success; }
    bool IsError() const { return m_error != ErrorCode::Success; }
    ErrorCode Error() const { return m_error; }
    
private:
    ErrorCode m_error;
};

// =============================================================================
// Configuration Structures
// =============================================================================

struct MicrokernelConfig {
    uint32_t maxExtensions = 256;
    uint32_t maxCommands = 4096;
    uint32_t ringBufferSize = 65536;
    uint32_t workerThreads = 4;
    uint32_t ipcTimeoutMs = 5000;
    bool enableSandbox = true;
    bool enableHotpatch = true;
    bool enableTelemetry = true;
    std::string ipcEndpoint = "\\\\.\\pipe\\UEC-X-Microkernel";
    IPCTransport ipcTransport = IPCTransport::NamedPipe;
};

struct ExtensionConfig {
    std::string name;
    std::string version;
    std::string entryPoint;
    CapabilityMask capabilities = 0;
    uint32_t maxMemoryMB = 512;
    uint32_t maxThreads = 4;
    uint32_t timeoutMs = 30000;
    bool persistent = false;
};

// =============================================================================
// Event System
// =============================================================================

struct Event {
    EventType type;
    ExtensionId source;
    Timestamp timestamp;
    std::vector<uint8_t> payload;
    
    Event() : type(0), source(0) {}
    Event(EventType t, ExtensionId src, std::vector<uint8_t> data)
        : type(t), source(src), timestamp(std::chrono::steady_clock::now()),
          payload(std::move(data)) {}
};

using EventHandler = std::function<void(const Event&)>;
using EventFilter = std::function<bool(const Event&)>;

// =============================================================================
// Command System
// =============================================================================

struct CommandContext {
    CommandId id;
    ExtensionId caller;
    Timestamp timestamp;
    std::vector<uint8_t> parameters;
    CapabilityMask requiredCapabilities;
};

using CommandHandler = std::function<Result<std::vector<uint8_t>>(const CommandContext&)>;

// =============================================================================
// Statistics
// =============================================================================

struct DispatchStats {
    std::atomic<uint64_t> dispatchCount{0};
    std::atomic<uint64_t> dispatchErrors{0};
    std::atomic<uint64_t> ipcBytesSent{0};
    std::atomic<uint64_t> ipcBytesReceived{0};
    std::atomic<uint64_t> eventsEmitted{0};
    std::atomic<uint64_t> commandsRegistered{0};
    std::atomic<uint64_t> extensionsLoaded{0};
};

// =============================================================================
// Version Info
// =============================================================================

struct VersionInfo {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    std::string build;
    std::string commit;
    
    std::string ToString() const {
        return std::to_string(major) + "." + 
               std::to_string(minor) + "." + 
               std::to_string(patch);
    }
};

// =============================================================================
// Utility Functions
// =============================================================================

UEC_API VersionInfo GetVersion();
UEC_API const char* GetErrorString(ErrorCode code);
UEC_API CapabilityMask ParseCapabilities(const std::vector<std::string>& caps);

} // namespace uec
