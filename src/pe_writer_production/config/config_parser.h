// ============================================================================
// Configuration Parser - Handles JSON/XML Configuration Files
// Supports flexible PE generation configuration
// ============================================================================

#pragma once

#include "../pe_writer.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace pewriter {

// ============================================================================
// CONFIGURATION PARSER CLASS
// ============================================================================

class ConfigParser {
public:
    ConfigParser();
    ~ConfigParser() = default;

    bool parseJSON(const std::string& jsonPath, PEConfig& config);
    bool parseXML(const std::string& xmlPath, PEConfig& config);
    bool parseString(const std::string& content, PEConfig& config, bool isJSON = true);

    // Validation
    bool validateConfig(const PEConfig& config) const;

private:
    // JSON parsing helpers
    bool parseJSONObject(const std::string& json, size_t& pos, PEConfig& config);
    bool parseJSONValue(const std::string& json, size_t& pos, std::string& value);
    bool parseJSONArray(const std::string& json, size_t& pos, std::vector<std::string>& array);
    void skipWhitespace(const std::string& str, size_t& pos) const;
    std::string extractString(const std::string& json, size_t& pos) const;

    // XML parsing helpers
    bool parseXMLDocument(const std::string& xml, PEConfig& config);
    std::string getXMLElementValue(const std::string& xml, const std::string& element) const;
    std::vector<std::string> getXMLElementArray(const std::string& xml, const std::string& element) const;
    std::vector<std::string> getXMLElementAttributeArray(const std::string& xml, const std::string& element, const std::string& attr) const;

    // Utility functions
    PEArchitecture stringToArchitecture(const std::string& str) const;
    PESubsystem stringToSubsystem(const std::string& str) const;
    bool stringToBool(const std::string& str) const;
    uint64_t stringToUint64(const std::string& str) const;
    uint32_t stringToUint32(const std::string& str) const;

    // Validation helpers
    bool validateArchitecture(PEArchitecture arch) const;
    bool validateSubsystem(PESubsystem subsystem) const;
    bool validateImageBase(uint64_t base) const;
    bool validateAlignment(uint32_t alignment) const;
};

} // namespace pewriter