// ============================================================================
// Configuration Parser Implementation
// Parses JSON and XML configuration files for PE generation
// ============================================================================

#include "config_parser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace pewriter {

// ============================================================================
// ConfigParser Implementation
// ============================================================================

ConfigParser::ConfigParser() {}

bool ConfigParser::parseJSON(const std::string& jsonPath, PEConfig& config) {
    try {
        std::ifstream file(jsonPath);
        if (!file) {
            return false;
        }

        std::stringstream buffer;
        buffer << file.rfile();
        std::string content = buffer.str();

        return parseString(content, config, true);
    } catch (const std::exception&) {
        return false;
    }
}

bool ConfigParser::parseXML(const std::string& xmlPath, PEConfig& config) {
    try {
        std::ifstream file(xmlPath);
        if (!file) {
            return false;
        }

        std::stringstream buffer;
        buffer << file.rfile();
        std::string content = buffer.str();

        return parseString(content, config, false);
    } catch (const std::exception&) {
        return false;
    }
}

bool ConfigParser::parseString(const std::string& content, PEConfig& config, bool isJSON) {
    if (isJSON) {
        size_t pos = 0;
        return parseJSONObject(content, pos, config);
    } else {
        return parseXMLDocument(content, config);
    }
}

bool ConfigParser::validateConfig(const PEConfig& config) const {
    return validateArchitecture(config.architecture) &&
           validateSubsystem(config.subsystem) &&
           validateImageBase(config.imageBase) &&
           validateAlignment(config.sectionAlignment) &&
           validateAlignment(config.fileAlignment);
}

// ============================================================================
// JSON PARSING
// ============================================================================

bool ConfigParser::parseJSONObject(const std::string& json, size_t& pos, PEConfig& config) {
    skipWhitespace(json, pos);
    if (json[pos] != '{') return false;
    pos++;

    while (pos < json.length()) {
        skipWhitespace(json, pos);
        if (json[pos] == '}') {
            pos++;
            break;
        }

        std::string key;
        if (!parseJSONValue(json, pos, key)) return false;

        skipWhitespace(json, pos);
        if (json[pos] != ':') return false;
        pos++;

        // Parse values based on key
        if (key == "architecture") {
            std::string value;
            if (!parseJSONValue(json, pos, value)) return false;
            config.architecture = stringToArchitecture(value);
        } else if (key == "subsystem") {
            std::string value;
            if (!parseJSONValue(json, pos, value)) return false;
            config.subsystem = stringToSubsystem(value);
        } else if (key == "imageBase") {
            std::string value;
            if (!parseJSONValue(json, pos, value)) return false;
            config.imageBase = stringToUint64(value);
        } else if (key == "sectionAlignment") {
            std::string value;
            if (!parseJSONValue(json, pos, value)) return false;
            config.sectionAlignment = stringToUint32(value);
        } else if (key == "fileAlignment") {
            std::string value;
            if (!parseJSONValue(json, pos, value)) return false;
            config.fileAlignment = stringToUint32(value);
        } else if (key == "libraries") {
            if (!parseJSONArray(json, pos, config.libraries)) return false;
        } else if (key == "symbols") {
            if (!parseJSONArray(json, pos, config.symbols)) return false;
        } else if (key == "enableASLR") {
            std::string value;
            if (!parseJSONValue(json, pos, value)) return false;
            config.enableASLR = stringToBool(value);
        } else if (key == "enableDEP") {
            std::string value;
            if (!parseJSONValue(json, pos, value)) return false;
            config.enableDEP = stringToBool(value);
        }
        // Skip unknown keys
        else {
            std::string dummy;
            parseJSONValue(json, pos, dummy);
        }

        skipWhitespace(json, pos);
        if (json[pos] == ',') {
            pos++;
        }
    }

    return true;
}

bool ConfigParser::parseJSONValue(const std::string& json, size_t& pos, std::string& value) {
    skipWhitespace(json, pos);

    if (json[pos] == '"') {
        value = extractString(json, pos);
        return true;
    } else {
        // Simple value (number, true, false, null)
        size_t start = pos;
        while (pos < json.length() && json[pos] != ',' && json[pos] != '}' && json[pos] != ']') {
            pos++;
        }
        value = json.substr(start, pos - start);
        // Trim whitespace
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](int ch) {
            return !std::isspace(ch);
        }));
        value.erase(std::find_if(value.rbegin(), value.rend(), [](int ch) {
            return !std::isspace(ch);
        }).base(), value.end());
        return true;
    }
}

bool ConfigParser::parseJSONArray(const std::string& json, size_t& pos, std::vector<std::string>& array) {
    skipWhitespace(json, pos);
    if (json[pos] != '[') return false;
    pos++;

    array.clear();
    while (pos < json.length()) {
        skipWhitespace(json, pos);
        if (json[pos] == ']') {
            pos++;
            break;
        }

        std::string value;
        if (!parseJSONValue(json, pos, value)) return false;
        array.push_back(value);

        skipWhitespace(json, pos);
        if (json[pos] == ',') {
            pos++;
        }
    }

    return true;
}

void ConfigParser::skipWhitespace(const std::string& str, size_t& pos) const {
    while (pos < str.length() && std::isspace(str[pos])) {
        pos++;
    }
}

std::string ConfigParser::extractString(const std::string& json, size_t& pos) const {
    if (json[pos] != '"') return "";
    pos++; // Skip opening quote

    std::string result;
    while (pos < json.length() && json[pos] != '"') {
        if (json[pos] == '\\') {
            pos++;
            if (pos < json.length()) {
                if (json[pos] == 'n') result += '\n';
                else if (json[pos] == 't') result += '\t';
                else if (json[pos] == '"') result += '"';
                else if (json[pos] == '\\') result += '\\';
                else result += json[pos];
                pos++;
            }
        } else {
            result += json[pos];
            pos++;
        }
    }

    if (pos < json.length() && json[pos] == '"') {
        pos++; // Skip closing quote
    }

    return result;
}

// ============================================================================
// XML PARSING — PROPER TAG-BASED WITH ATTRIBUTE SUPPORT
// ============================================================================

bool ConfigParser::parseXMLDocument(const std::string& xml, PEConfig& config) {
    // Parse architecture, subsystem, and other top-level elements
    config.architecture = stringToArchitecture(
        getXMLElementValue(xml, "architecture"));
    config.subsystem = stringToSubsystem(
        getXMLElementValue(xml, "subsystem"));

    std::string imageBaseStr = getXMLElementValue(xml, "imageBase");
    if (!imageBaseStr.empty()) {
        config.imageBase = stringToUint64(imageBaseStr);
    }

    std::string sectionAlignStr = getXMLElementValue(xml, "sectionAlignment");
    if (!sectionAlignStr.empty()) {
        config.sectionAlignment = stringToUint32(sectionAlignStr);
    }

    std::string fileAlignStr = getXMLElementValue(xml, "fileAlignment");
    if (!fileAlignStr.empty()) {
        config.fileAlignment = stringToUint32(fileAlignStr);
    }

    config.libraries = getXMLElementArray(xml, "library");
    config.symbols = getXMLElementArray(xml, "symbol");

    config.enableASLR = stringToBool(getXMLElementValue(xml, "enableASLR"));
    config.enableDEP = stringToBool(getXMLElementValue(xml, "enableDEP"));

    // Parse attribute-based elements: <library name="..."/> and <symbol name="..."/>
    auto attrLibs = getXMLElementAttributeArray(xml, "library", "name");
    config.libraries.insert(config.libraries.end(), attrLibs.begin(), attrLibs.end());

    auto attrSyms = getXMLElementAttributeArray(xml, "symbol", "name");
    config.symbols.insert(config.symbols.end(), attrSyms.begin(), attrSyms.end());

    // Parse stack/heap sizes if present
    std::string stackReserve = getXMLElementValue(xml, "stackReserve");
    if (!stackReserve.empty()) config.stackReserve = stringToUint32(stackReserve);
    std::string stackCommit = getXMLElementValue(xml, "stackCommit");
    if (!stackCommit.empty()) config.stackCommit = stringToUint32(stackCommit);
    std::string heapReserve = getXMLElementValue(xml, "heapReserve");
    if (!heapReserve.empty()) config.heapReserve = stringToUint32(heapReserve);
    std::string heapCommit = getXMLElementValue(xml, "heapCommit");
    if (!heapCommit.empty()) config.heapCommit = stringToUint32(heapCommit);

    std::string entryPoint = getXMLElementValue(xml, "entryPoint");
    if (!entryPoint.empty()) config.entryPointSymbol = entryPoint;

    config.enableHighEntropyVA = stringToBool(getXMLElementValue(xml, "enableHighEntropyVA"));
    config.enableSEH = stringToBool(getXMLElementValue(xml, "enableSEH"));

    return true;
}

std::string ConfigParser::getXMLElementValue(const std::string& xml, const std::string& element) const {
    // Try self-closing tag with value= attribute first: <element value="..."/>
    {
        size_t pos = 0;
        while (pos < xml.size()) {
            std::string openTag = "<" + element;
            size_t tagStart = xml.find(openTag, pos);
            if (tagStart == std::string::npos) break;

            size_t tagEnd = xml.find('>', tagStart);
            if (tagEnd == std::string::npos) break;

            // Check it's truly the right tag (followed by space or >)
            size_t afterName = tagStart + openTag.size();
            if (afterName < xml.size() && xml[afterName] != ' ' &&
                xml[afterName] != '>' && xml[afterName] != '/' && xml[afterName] != '\t') {
                pos = tagEnd + 1;
                continue;
            }

            std::string tagContent = xml.substr(tagStart, tagEnd - tagStart + 1);

            // Extract value="..." attribute
            size_t valPos = tagContent.find("value=\"");
            if (valPos != std::string::npos) {
                valPos += 7; // skip value="
                size_t valEnd = tagContent.find('"', valPos);
                if (valEnd != std::string::npos) {
                    return tagContent.substr(valPos, valEnd - valPos);
                }
            }
            pos = tagEnd + 1;
        }
    }

    // Fall back to content between <element>...</element>
    std::string startTag = "<" + element + ">";
    std::string endTag = "</" + element + ">";

    size_t startPos = xml.find(startTag);
    if (startPos == std::string::npos) {
        // Also try with attributes: <element ...>content</element>
        size_t tagOpen = xml.find("<" + element + " ");
        if (tagOpen == std::string::npos) tagOpen = xml.find("<" + element + "\t");
        if (tagOpen != std::string::npos) {
            size_t closeAngle = xml.find('>', tagOpen);
            if (closeAngle != std::string::npos && xml[closeAngle - 1] != '/') {
                size_t endPos = xml.find(endTag, closeAngle + 1);
                if (endPos != std::string::npos) {
                    return xml.substr(closeAngle + 1, endPos - closeAngle - 1);
                }
            }
        }
        return "";
    }

    startPos += startTag.length();
    size_t endPos = xml.find(endTag, startPos);
    if (endPos == std::string::npos) return "";

    return xml.substr(startPos, endPos - startPos);
}

std::vector<std::string> ConfigParser::getXMLElementArray(const std::string& xml, const std::string& element) const {
    std::vector<std::string> result;
    std::string startTag = "<" + element + ">";
    std::string endTag = "</" + element + ">";

    size_t pos = 0;
    while ((pos = xml.find(startTag, pos)) != std::string::npos) {
        pos += startTag.length();
        size_t endPos = xml.find(endTag, pos);
        if (endPos != std::string::npos) {
            result.push_back(xml.substr(pos, endPos - pos));
            pos = endPos + endTag.length();
        } else {
            break;
        }
    }

    // Also handle <element attr="...">content</element> form
    pos = 0;
    std::string tagPrefix = "<" + element + " ";
    while ((pos = xml.find(tagPrefix, pos)) != std::string::npos) {
        size_t closeAngle = xml.find('>', pos);
        if (closeAngle == std::string::npos) break;

        if (xml[closeAngle - 1] == '/') {
            // Self-closing, skip (handled by attribute variant)
            pos = closeAngle + 1;
            continue;
        }

        size_t contentStart = closeAngle + 1;
        size_t endPos = xml.find(endTag, contentStart);
        if (endPos != std::string::npos) {
            std::string content = xml.substr(contentStart, endPos - contentStart);
            if (!content.empty()) {
                result.push_back(content);
            }
            pos = endPos + endTag.length();
        } else {
            break;
        }
    }

    return result;
}

std::vector<std::string> ConfigParser::getXMLElementAttributeArray(
        const std::string& xml, const std::string& element, const std::string& attr) const {
    std::vector<std::string> result;
    size_t pos = 0;

    while (pos < xml.size()) {
        // Find <element ...
        std::string tagPrefix = "<" + element;
        size_t tagStart = xml.find(tagPrefix, pos);
        if (tagStart == std::string::npos) break;

        // Verify it's the right tag (next char is space, >, /)
        size_t afterName = tagStart + tagPrefix.size();
        if (afterName < xml.size() && xml[afterName] != ' ' &&
            xml[afterName] != '>' && xml[afterName] != '/' && xml[afterName] != '\t') {
            pos = afterName;
            continue;
        }

        size_t tagEnd = xml.find('>', tagStart);
        if (tagEnd == std::string::npos) break;

        std::string tagContent = xml.substr(tagStart, tagEnd - tagStart + 1);

        // Look for attr="value"
        std::string attrSearch = attr + "=\"";
        size_t attrPos = tagContent.find(attrSearch);
        if (attrPos != std::string::npos) {
            attrPos += attrSearch.size();
            size_t attrEnd = tagContent.find('"', attrPos);
            if (attrEnd != std::string::npos) {
                result.push_back(tagContent.substr(attrPos, attrEnd - attrPos));
            }
        }

        pos = tagEnd + 1;
    }

    return result;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

PEArchitecture ConfigParser::stringToArchitecture(const std::string& str) const {
    if (str == "x64") return PEArchitecture::x64;
    if (str == "x86") return PEArchitecture::x86;
    if (str == "ARM64") return PEArchitecture::ARM64;
    return PEArchitecture::x64; // Default
}

PESubsystem ConfigParser::stringToSubsystem(const std::string& str) const {
    if (str == "WINDOWS_GUI") return PESubsystem::WINDOWS_GUI;
    if (str == "WINDOWS_CUI") return PESubsystem::WINDOWS_CUI;
    return PESubsystem::WINDOWS_CUI; // Default
}

bool ConfigParser::stringToBool(const std::string& str) const {
    return str == "true" || str == "1" || str == "yes";
}

uint64_t ConfigParser::stringToUint64(const std::string& str) const {
    try {
        if (str.empty()) return 0;
        if (str.substr(0, 2) == "0x") {
            return std::stoull(str.substr(2), nullptr, 16);
        }
        return std::stoull(str);
    } catch (...) {
        return 0;
    }
}

uint32_t ConfigParser::stringToUint32(const std::string& str) const {
    try {
        if (str.empty()) return 0;
        if (str.substr(0, 2) == "0x") {
            return static_cast<uint32_t>(std::stoul(str.substr(2), nullptr, 16));
        }
        return static_cast<uint32_t>(std::stoul(str));
    } catch (...) {
        return 0;
    }
}

bool ConfigParser::validateArchitecture(PEArchitecture arch) const {
    return arch == PEArchitecture::x64 || arch == PEArchitecture::x86 || arch == PEArchitecture::ARM64;
}

bool ConfigParser::validateSubsystem(PESubsystem subsystem) const {
    return true; // All enum values are valid
}

bool ConfigParser::validateImageBase(uint64_t base) const {
    return base >= 0x10000 && base <= 0x7FFFFFFFFFFFFFFFULL;
}

bool ConfigParser::validateAlignment(uint32_t alignment) const {
    return alignment >= 0x200 && (alignment & (alignment - 1)) == 0; // Power of 2
}

} // namespace pewriter