#include "FeatureFlags.hpp"
#include <fstream>
#include <sstream>

namespace RawrXD::Agentic::Wiring {

FeatureFlags& FeatureFlags::instance() {
    static FeatureFlags inst;
    return inst;
}

void FeatureFlags::set(const std::string& name, bool value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    bool oldValue = m_boolFlags[name].load();
    m_boolFlags[name].store(value);
    notifyCallbacks(name, oldValue, value);
}

bool FeatureFlags::get(const std::string& name, bool defaultValue) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_boolFlags.find(name);
    return (it != m_boolFlags.end()) ? it->second.load() : defaultValue;
}

std::string FeatureFlags::getString(const std::string& name, const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_stringFlags.find(name);
    return (it != m_stringFlags.end()) ? it->second : defaultValue;
}

void FeatureFlags::setString(const std::string& name, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stringFlags[name] = value;
}

int FeatureFlags::getInt(const std::string& name, int defaultValue) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_intFlags.find(name);
    return (it != m_intFlags.end()) ? it->second.load() : defaultValue;
}

void FeatureFlags::setInt(const std::string& name, int value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_intFlags[name].store(value);
}

float FeatureFlags::getFloat(const std::string& name, float defaultValue) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_floatFlags.find(name);
    return (it != m_floatFlags.end()) ? it->second.load() : defaultValue;
}

void FeatureFlags::setFloat(const std::string& name, float value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_floatFlags[name].store(value);
}

void FeatureFlags::onFlagChanged(const std::string& name, FlagCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks[name].push_back(callback);
}

bool FeatureFlags::loadFromFile(const std::string& filePath) {
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());

    std::lock_guard<std::mutex> lock(m_mutex);

    // Parse simple JSON format: {"key": value, ...}
    // Supports bool (true/false), int, float, and string values
    size_t pos = 0;
    while ((pos = content.find('"', pos)) != std::string::npos) {
        size_t keyStart = pos + 1;
        size_t keyEnd = content.find('"', keyStart);
        if (keyEnd == std::string::npos) break;

        std::string key = content.substr(keyStart, keyEnd - keyStart);
        pos = content.find(':', keyEnd);
        if (pos == std::string::npos) break;
        pos++;

        // Skip whitespace
        while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t')) pos++;

        if (pos >= content.size()) break;

        if (content[pos] == '"') {
            // String value
            size_t valStart = pos + 1;
            size_t valEnd = content.find('"', valStart);
            if (valEnd == std::string::npos) break;
            m_stringFlags[key] = content.substr(valStart, valEnd - valStart);
            pos = valEnd + 1;
        } else if (content.substr(pos, 4) == "true") {
            m_boolFlags[key].store(true);
            pos += 4;
        } else if (content.substr(pos, 5) == "false") {
            m_boolFlags[key].store(false);
            pos += 5;
        } else {
            // Numeric value — extract until delimiter
            size_t numStart = pos;
            while (pos < content.size() && content[pos] != ',' &&
                   content[pos] != '}' && content[pos] != '\n') {
                pos++;
            }
            std::string numStr = content.substr(numStart, pos - numStart);
            // Trim whitespace
            while (!numStr.empty() && (numStr.back() == ' ' || numStr.back() == '\r')) {
                numStr.pop_back();
            }
            if (numStr.find('.') != std::string::npos) {
                m_floatFlags[key].store(std::stof(numStr));
            } else {
                m_intFlags[key].store(std::stoi(numStr));
            }
        }
    }

    return true;
}

bool FeatureFlags::saveToFile(const std::string& filePath) const {
    std::ofstream ofs(filePath, std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }

    ofs << toJson();
    return ofs.good();
}

std::string FeatureFlags::toJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream json;
    json << "{\n";

    bool first = true;

    // Serialize bool flags
    for (const auto& [name, value] : m_boolFlags) {
        if (!first) json << ",\n";
        first = false;
        json << "  \"" << name << "\": " << (value.load() ? "true" : "false");
    }

    // Serialize int flags
    for (const auto& [name, value] : m_intFlags) {
        if (!first) json << ",\n";
        first = false;
        json << "  \"" << name << "\": " << value.load();
    }

    // Serialize float flags
    for (const auto& [name, value] : m_floatFlags) {
        if (!first) json << ",\n";
        first = false;
        json << "  \"" << name << "\": " << value.load();
    }

    // Serialize string flags
    for (const auto& [name, value] : m_stringFlags) {
        if (!first) json << ",\n";
        first = false;
        json << "  \"" << name << "\": \"" << value << "\"";
    }

    json << "\n}\n";
    return json.str();
}

void FeatureFlags::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_boolFlags.clear();
    m_intFlags.clear();
    m_floatFlags.clear();
    m_stringFlags.clear();
    m_callbacks.clear();
}

std::vector<std::string> FeatureFlags::listFlags() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> flags;
    for (const auto& [name, _] : m_boolFlags) {
        flags.push_back(name);
    }
    return flags;
}

void FeatureFlags::notifyCallbacks(const std::string& name, bool oldValue, bool newValue) {
    auto it = m_callbacks.find(name);
    if (it != m_callbacks.end()) {
        for (const auto& callback : it->second) {
            callback(name, oldValue, newValue);
        }
    }
}

} // namespace RawrXD::Agentic::Wiring
