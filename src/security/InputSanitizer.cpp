#include "InputSanitizer.h"
#include <algorithm>
#include <regex>
#include <filesystem>
#include <iostream>

namespace RawrXD::Security {

InputSanitizer& InputSanitizer::instance() {
    static InputSanitizer instance;
    return instance;
}

InputSanitizer::InputSanitizer() {
    // Initialize with default sanitization rules
}

SanitizationResult InputSanitizer::sanitizePrompt(const std::string& input) {
    SanitizationResult result;
    result.sanitized = input;

    // Remove null bytes
    result.sanitized = removeNullBytes(result.sanitized);
    if (result.sanitized != input) {
        result.wasModified = true;
        result.issues.push_back("Removed null bytes");
    }

    // Check for potentially dangerous patterns
    if (input.find('\0') != std::string::npos) {
        result.issues.push_back("Input contained null bytes");
    }

    // Length limits
    if (input.length() > 10000) {
        result.issues.push_back("Input exceeds maximum length");
        if (strictMode_) {
            result.sanitized = input.substr(0, 10000);
            result.wasModified = true;
        }
    }

    return result;
}

SanitizationResult InputSanitizer::sanitizePath(const std::string& input) {
    SanitizationResult result;
    result.sanitized = input;

    // Remove null bytes
    result.sanitized = removeNullBytes(result.sanitized);

    // Validate path traversal attempts
    result.sanitized = validatePathTraversal(result.sanitized);
    if (result.sanitized != input) {
        result.wasModified = true;
        result.issues.push_back("Removed path traversal sequences");
    }

    // Check for absolute paths if not allowed
    if (strictMode_ && std::filesystem::path(result.sanitized).is_absolute()) {
        result.issues.push_back("Absolute paths not allowed in strict mode");
    }

    return result;
}

SanitizationResult InputSanitizer::sanitizeModelName(const std::string& input) {
    SanitizationResult result;
    result.sanitized = input;

    // Remove null bytes
    result.sanitized = removeNullBytes(result.sanitized);

    // Sanitize model identifier
    result.sanitized = sanitizeModelIdentifier(result.sanitized);
    if (result.sanitized != input) {
        result.wasModified = true;
        result.issues.push_back("Sanitized model identifier");
    }

    return result;
}

void InputSanitizer::addCustomRule(const std::string& name,
                                  std::function<std::string(const std::string&)> rule) {
    customRules_.push_back(rule);
}

std::string InputSanitizer::removeNullBytes(const std::string& input) {
    std::string result;
    for (char c : input) {
        if (c != '\0') {
            result += c;
        }
    }
    return result;
}

std::string InputSanitizer::escapeShellMetacharacters(const std::string& input) {
    std::string result = input;
    // Basic shell metacharacter escaping
    std::regex metachars(R"([`$\\"])");
    result = std::regex_replace(result, metachars, R"(\$&)");
    return result;
}

std::string InputSanitizer::validatePathTraversal(const std::string& input) {
    std::string result = input;

    // Remove path traversal sequences
    std::regex traversal(R"(\.\./|\.\.\\)");
    result = std::regex_replace(result, traversal, "");

    // Remove absolute path indicators
    if (result.find(":\\") != std::string::npos || result.find(":/") != std::string::npos) {
        // In strict mode, this might be an issue, but we'll just clean it
        result = std::filesystem::path(result).filename().string();
    }

    return result;
}

std::string InputSanitizer::sanitizeModelIdentifier(const std::string& input) {
    std::string result = input;

    // Allow only alphanumeric, dots, hyphens, and underscores
    std::regex invalidChars(R"([^a-zA-Z0-9._-])");
    result = std::regex_replace(result, invalidChars, "_");

    // Limit length
    if (result.length() > 100) {
        result = result.substr(0, 100);
    }

    return result;
}

SecureDefaults& SecureDefaults::instance() {
    static SecureDefaults instance;
    return instance;
}

std::string SecureDefaults::getSecureTempPath() const {
    // Use a secure temporary directory
    return std::filesystem::temp_directory_path().string() + "/rawrxd_secure";
}

std::string SecureDefaults::getSecureConfigPath() const {
    // Use user config directory
    return std::filesystem::path(std::getenv("APPDATA") ? std::getenv("APPDATA") : "").string() +
           "/RawrXD";
}

std::string SecureDefaults::getSecureModelPath() const {
    return getSecureConfigPath() + "/models";
}

void SecureDefaults::setSecureEnvironment() {
    // Set secure environment variables
#ifdef _WIN32
    _putenv_s("TMP", getSecureTempPath().c_str());
    _putenv_s("TEMP", getSecureTempPath().c_str());
#endif
}

bool SecureDefaults::validateFilePermissions(const std::string& path) const {
    try {
        std::filesystem::path p(path);
        if (!std::filesystem::exists(p)) {
            return false;
        }

        // Basic permission check - file should not be world-writable
        auto perms = std::filesystem::status(p).permissions();
        return (perms & std::filesystem::perms::others_write) == std::filesystem::perms::none;
    } catch (...) {
        return false;
    }
}

} // namespace RawrXD::Security