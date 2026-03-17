#pragma once
// ============================================================================
// Result.h — Unified error handling type for RawrXD (Finding #9 fix)
// 
// Provides a single consistent error handling pattern across the codebase.
// All public APIs should return Result<T> instead of mixing exceptions,
// bool returns, and raw error codes.
//
// Usage:
//   Result<Model> loadModel(const std::string& path) {
//       if (!fileExists(path))
//           return std::unexpected(Error{ErrorCode::FileNotFound, "Not found: " + path});
//       return Model{...};
//   }
//
//   auto result = loadModel("model.gguf");
//   if (!result) {
//       LOG_ERROR(result.error().message);
//       return;
//   }
//   auto& model = result.value();
// ============================================================================

#include <expected>
#include <string>
#include <cstdint>

namespace RawrXD {

enum class ErrorCode : uint32_t {
    // General
    Success = 0,
    Unknown = 1,
    InvalidArgument = 2,
    NotInitialized = 3,
    AlreadyInitialized = 4,

    // File/IO
    FileNotFound = 100,
    FileReadError = 101,
    FileWriteError = 102,
    InvalidFormat = 103,
    CorruptData = 104,

    // Model/Inference
    ModelLoadFailed = 200,
    ModelNotLoaded = 201,
    InferenceError = 202,
    TokenizationError = 203,
    ContextOverflow = 204,
    UnsupportedQuantType = 205,

    // Memory
    AllocationFailed = 300,
    OutOfMemory = 301,
    BufferOverflow = 302,

    // Network
    ConnectionFailed = 400,
    Timeout = 401,
    HttpError = 402,

    // Security
    PermissionDenied = 500,
    PatchFailed = 501,
    IntegrityCheckFailed = 502,

    // Build/Compile 
    CompilationFailed = 600,
    LinkError = 601,
    AssemblyError = 602,
};

struct Error {
    ErrorCode code = ErrorCode::Unknown;
    std::string message;

    Error() = default;
    Error(ErrorCode c, std::string msg) : code(c), message(std::move(msg)) {}

    // Convenience: implicit from string
    explicit Error(std::string msg) : code(ErrorCode::Unknown), message(std::move(msg)) {}

    bool operator==(const Error& other) const { return code == other.code; }
};

// The unified result type — use this everywhere instead of bool/exceptions
template<typename T>
using Result = std::expected<T, Error>;

// For void-returning operations
using VoidResult = std::expected<void, Error>;

// Convenience factory functions
inline Error makeError(ErrorCode code, const std::string& msg) {
    return Error{code, msg};
}

template<typename T>
Result<T> makeSuccess(T&& value) {
    return Result<T>{std::forward<T>(value)};
}

inline VoidResult makeVoidSuccess() {
    return VoidResult{};
}

} // namespace RawrXD
