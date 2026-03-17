// ============================================================================
// Error Handler Implementation
// Centralized error management with thread safety
// ============================================================================

#include "error_handler.h"
#include <sstream>
#include <algorithm>

namespace pewriter {

// ============================================================================
// ErrorHandler Implementation
// ============================================================================

ErrorHandler::ErrorHandler() {}

void ErrorHandler::setError(PEErrorCode code, const std::string& message,
                           const std::string& file, int line,
                           const std::string& function) {
    std::lock_guard<std::mutex> lock(mutex_);

    ErrorInfo error;
    error.code = code;
    error.message = message;
    error.file = file;
    error.line = line;
    error.function = function;

    errors_.push_back(error);

    // Call callback if set
    if (errorCallback_) {
        errorCallback_(code, message);
    }
}

void ErrorHandler::addErrorDetail(const std::string& detail) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!errors_.empty()) {
        errors_.back().details.push_back(detail);
    }
}

void ErrorHandler::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    errors_.clear();
}

PEErrorCode ErrorHandler::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return errors_.empty() ? PEErrorCode::SUCCESS : errors_.back().code;
}

std::string ErrorHandler::getErrorMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return errors_.empty() ? "" : errors_.back().message;
}

const ErrorInfo& ErrorHandler::getLastErrorInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    static ErrorInfo emptyError = {PEErrorCode::SUCCESS, "", "", 0, ""};
    return errors_.empty() ? emptyError : errors_.back();
}

std::vector<ErrorInfo> ErrorHandler::getAllErrors() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return errors_;
}

void ErrorHandler::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    errorCallback_ = callback;
}

std::string ErrorHandler::formatError(const ErrorInfo& error) const {
    std::stringstream ss;

    ss << "Error " << static_cast<int>(error.code) << ": " << error.message;

    if (!error.file.empty()) {
        ss << " (in " << error.file;
        if (error.line > 0) {
            ss << ":" << error.line;
        }
        ss << ")";
    }

    if (!error.function.empty()) {
        ss << " in function " << error.function;
    }

    if (!error.details.empty()) {
        ss << "\nDetails:";
        for (const auto& detail : error.details) {
            ss << "\n  - " << detail;
        }
    }

    return ss.str();
}

std::string ErrorHandler::getErrorSummary() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (errors_.empty()) {
        return "No errors";
    }

    std::stringstream ss;
    ss << "Total errors: " << errors_.size() << "\n";

    // Count errors by type
    std::unordered_map<PEErrorCode, int> errorCounts;
    for (const auto& error : errors_) {
        errorCounts[error.code]++;
    }

    for (const auto& pair : errorCounts) {
        ss << "  " << getErrorMessageString(pair.first) << ": " << pair.second << "\n";
    }

    return ss.str();
}

void ErrorHandler::lock() {
    mutex_.lock();
}

void ErrorHandler::unlock() {
    mutex_.unlock();
}

std::string ErrorHandler::getErrorMessageString(PEErrorCode code) const {
    switch (code) {
    case PEErrorCode::SUCCESS:
        return "Success";
    case PEErrorCode::INVALID_PARAMETER:
        return "Invalid parameter";
    case PEErrorCode::OUT_OF_MEMORY:
        return "Out of memory";
    case PEErrorCode::FILE_IO_ERROR:
        return "File I/O error";
    case PEErrorCode::INVALID_PE_STRUCTURE:
        return "Invalid PE structure";
    case PEErrorCode::CODE_GENERATION_ERROR:
        return "Code generation error";
    case PEErrorCode::IMPORT_RESOLUTION_ERROR:
        return "Import resolution error";
    case PEErrorCode::RELOCATION_ERROR:
        return "Relocation error";
    case PEErrorCode::VALIDATION_ERROR:
        return "Validation error";
    case PEErrorCode::CONFIGURATION_ERROR:
        return "Configuration error";
    default:
        return "Unknown error";
    }
}

} // namespace pewriter