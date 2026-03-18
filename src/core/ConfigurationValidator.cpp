#include "ConfigurationValidator.h"
#include <algorithm>
#include <regex>
#include <filesystem>

namespace RawrXD::Core {

ConfigurationValidator& ConfigurationValidator::instance() {
    static ConfigurationValidator instance;
    return instance;
}

void ConfigurationValidator::addRule(const std::string& section, const ValidationRule& rule) {
    rules_[section].push_back(rule);
}

ValidationResult ConfigurationValidator::validateSection(
    const std::string& section,
    const std::unordered_map<std::string, std::string>& config) const {

    ValidationResult result;
    auto it = rules_.find(section);
    if (it == rules_.end()) {
        result.warnings.push_back("No validation rules defined for section: " + section);
        return result;
    }

    const auto& sectionRules = it->second;
    for (const auto& rule : sectionRules) {
        auto configIt = config.find(rule.name);
        if (configIt == config.end()) {
            if (rule.required) {
                result.valid = false;
                result.errors.push_back("Missing required configuration: " + rule.name);
            }
            continue;
        }

        if (!rule.validator(configIt->second)) {
            result.valid = false;
            result.errors.push_back(rule.errorMessage + " (value: " + configIt->second + ")");
        }
    }

    // Check for unknown configurations in strict mode
    if (strictMode_) {
        std::vector<std::string> knownKeys;
        for (const auto& rule : sectionRules) {
            knownKeys.push_back(rule.name);
        }

        for (const auto& [key, value] : config) {
            if (std::find(knownKeys.begin(), knownKeys.end(), key) == knownKeys.end()) {
                result.warnings.push_back("Unknown configuration key: " + key);
            }
        }
    }

    return result;
}

ValidationResult ConfigurationValidator::validateAll(
    const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& allConfig) const {

    ValidationResult result;
    for (const auto& [section, config] : allConfig) {
        auto sectionResult = validateSection(section, config);
        if (!sectionResult.valid) {
            result.valid = false;
            result.errors.insert(result.errors.end(),
                               sectionResult.errors.begin(),
                               sectionResult.errors.end());
        }
        result.warnings.insert(result.warnings.end(),
                             sectionResult.warnings.begin(),
                             sectionResult.warnings.end());
    }
    return result;
}

bool ConfigurationValidator::validatePort(const std::string& value) {
    try {
        int port = std::stoi(value);
        return port > 0 && port <= 65535;
    } catch (...) {
        return false;
    }
}

bool ConfigurationValidator::validatePath(const std::string& value) {
    return !value.empty() && std::filesystem::path(value).has_root_path();
}

bool ConfigurationValidator::validateBoolean(const std::string& value) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "true" || lower == "false" || lower == "1" || lower == "0";
}

bool ConfigurationValidator::validatePositiveInteger(const std::string& value) {
    try {
        int num = std::stoi(value);
        return num > 0;
    } catch (...) {
        return false;
    }
}

bool ConfigurationValidator::validateMemorySize(const std::string& value) {
    static std::regex pattern(R"(^\d+[kmgt]?$)", std::regex_constants::icase);
    return std::regex_match(value, pattern);
}

} // namespace RawrXD::Core