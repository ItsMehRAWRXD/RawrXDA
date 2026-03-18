#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>

namespace RawrXD::Core {

struct ValidationRule {
    std::string name;
    std::function<bool(const std::string&)> validator;
    std::string errorMessage;
    bool required{true};
};

struct ValidationResult {
    bool valid{true};
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

class ConfigurationValidator {
public:
    static ConfigurationValidator& instance();

    void addRule(const std::string& section, const ValidationRule& rule);
    ValidationResult validateSection(const std::string& section,
                                   const std::unordered_map<std::string, std::string>& config) const;
    ValidationResult validateAll(const std::unordered_map<std::string,
                                std::unordered_map<std::string, std::string>>& allConfig) const;

    void setStrictMode(bool strict) { strictMode_ = strict; }
    bool isStrictMode() const { return strictMode_; }

private:
    ConfigurationValidator() = default;

    std::unordered_map<std::string, std::vector<ValidationRule>> rules_;
    bool strictMode_{false};

    // Built-in validators
    static bool validatePort(const std::string& value);
    static bool validatePath(const std::string& value);
    static bool validateBoolean(const std::string& value);
    static bool validatePositiveInteger(const std::string& value);
    static bool validateMemorySize(const std::string& value);
};

} // namespace RawrXD::Core