#pragma once

#include <string>
#include <expected>
#include <vector>
#include <optional>

namespace RawrXD::Registry {

// Error types
enum class Error {
    Success = 0,
    InvalidInput,
    DatabaseError,
    ProcessFailed,
    ExtensionNotFound,
    InstallationFailed,
    NetworkError,
    UnknownError
};

// Extension types
enum class ExtensionType {
    PowerShell,
    VSCode,
    QtPlugin,
    Unknown
};

// Extension metadata
struct ExtensionInfo {
    std::string id;
    std::string name;
    std::string version;
    ExtensionType type;
    std::string description;
    std::string author;
    std::string localPath;
    bool isInstalled;
    std::string lastUpdated;
    
    ExtensionInfo() : type(ExtensionType::Unknown), isInstalled(false) {}
};

// Installation result
struct InstallationResult {
    bool success;
    std::string message;
    std::string extensionId;
    
    InstallationResult() : success(false) {}
    InstallationResult(bool s, std::string msg, std::string id)
        : success(s), message(msg), extensionId(id) {}
};

// Process output
struct ProcessResult {
    int exitCode;
    std::string stdout;
    std::string stderr;
    bool success;
    
    ProcessResult() : exitCode(-1), success(false) {}
    ProcessResult(int code, std::string out, std::string err)
        : exitCode(code), stdout(out), stderr(err), success(code == 0) {}
};

// HTTP response types
enum class HttpStatus {
    OK = 200,
    Created = 201,
    Accepted = 202,
    BadRequest = 400,
    NotFound = 404,
    InternalError = 500
};

// Forward declarations
class RegistryManager;
class ExtensionInstaller;
class DatabaseManager;
class HttpServer;
class ProcessBridge;

} // namespace RawrXD::Registry
