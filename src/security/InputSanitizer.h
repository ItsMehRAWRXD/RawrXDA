#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace RawrXD::Security {

struct SanitizationResult {
    std::string sanitized;
    bool wasModified{false};
    std::vector<std::string> issues;
};

class InputSanitizer {
public:
    static InputSanitizer& instance();

    SanitizationResult sanitizePrompt(const std::string& input);
    SanitizationResult sanitizePath(const std::string& input);
    SanitizationResult sanitizeModelName(const std::string& input);

    void addCustomRule(const std::string& name, std::function<std::string(const std::string&)> rule);
    void enableStrictMode(bool enable) { strictMode_ = enable; }

private:
    InputSanitizer();

    std::string removeNullBytes(const std::string& input);
    std::string escapeShellMetacharacters(const std::string& input);
    std::string validatePathTraversal(const std::string& input);
    std::string sanitizeModelIdentifier(const std::string& input);

    std::vector<std::function<std::string(const std::string&)>> customRules_;
    bool strictMode_{false};
};

class SecureDefaults {
public:
    static SecureDefaults& instance();

    std::string getSecureTempPath() const;
    std::string getSecureConfigPath() const;
    std::string getSecureModelPath() const;

    void setSecureEnvironment();
    bool validateFilePermissions(const std::string& path) const;

private:
    SecureDefaults() = default;
};

} // namespace RawrXD::Security