// ============================================================================
// Error Handler - Centralized Error Management and Reporting
// Thread-safe error handling with detailed error information
// ============================================================================

#pragma once

#include "../pe_writer.h"
#include <string>
#include <vector>
#include <functional>
#include <mutex>

namespace pewriter {

// ============================================================================
// ERROR INFORMATION
// ============================================================================

struct ErrorInfo {
    PEErrorCode code;
    std::string message;
    std::string file;
    int line;
    std::string function;
    std::vector<std::string> details;
};

// ============================================================================
// ERROR HANDLER CLASS
// ============================================================================

class ErrorHandler {
public:
    ErrorHandler();
    ~ErrorHandler() = default;

    // Error setting
    void setError(PEErrorCode code, const std::string& message,
                  const std::string& file = "", int line = 0,
                  const std::string& function = "");
    void addErrorDetail(const std::string& detail);
    void clear();

    // Error querying
    PEErrorCode getLastError() const;
    std::string getErrorMessage() const;
    const ErrorInfo& getLastErrorInfo() const;
    std::vector<ErrorInfo> getAllErrors() const;

    // Error callbacks
    using ErrorCallback = std::function<void(PEErrorCode, const std::string&)>;
    void setErrorCallback(ErrorCallback callback);

    // Error formatting
    std::string formatError(const ErrorInfo& error) const;
    std::string getErrorSummary() const;

    // Thread safety
    void lock();
    void unlock();

private:
    // Error storage
    std::vector<ErrorInfo> errors_;
    ErrorCallback errorCallback_;
    mutable std::mutex mutex_;

    // Error message mapping
    std::string getErrorMessageString(PEErrorCode code) const;
};

} // namespace pewriter