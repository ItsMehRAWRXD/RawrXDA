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

#include <nlohmann/json.hpp>

// ...existing code...


bool FeatureFlags::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;
    
    try {
        nlohmann::json j;
        file >> j;
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Parse simple key-value pairs
        if (j.contains("bools")) {
             for (auto& element : j["bools"].items()) {
                 m_boolFlags[element.key()].store(element.value().get<bool>());
             }
        }
        if (j.contains("strings")) {
             for (auto& element : j["strings"].items()) {
                 m_stringFlags[element.key()] = element.value().get<std::string>();
             }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool FeatureFlags::saveToFile(const std::string& filePath) const {
    try {
        nlohmann::json j;
        j["bools"] = nlohmann::json::object();
        j["strings"] = nlohmann::json::object();

        {
             std::lock_guard<std::mutex> lock(m_mutex);
             for (const auto& pair : m_boolFlags) {
                 j["bools"][pair.first] = pair.second.load();
             }
             for (const auto& pair : m_stringFlags) {
                 j["strings"][pair.first] = pair.second;
             }
        }
        
        std::ofstream file(filePath);
        if (!file.is_open()) return false;
        
        file << j.dump(4);
        return true;
    } catch (...) {
        return false;
    }
}

std::string FeatureFlags::toJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream json;
    json << "{\n";
    json << "  \"boolFlags\": {\n";
    bool first = true;
    for (const auto& [key, val] : m_boolFlags) {
        if (!first) json << ",\n";
        json << "    \"" << key << "\": " << (val ? "true" : "false");
        first = false;
    }
    json << "\n  },\n";
    
    json << "  \"intFlags\": {\n";
    first = true;
    for (const auto& [key, val] : m_intFlags) {
        if (!first) json << ",\n";
        json << "    \"" << key << "\": " << val;
        first = false;
    }
    json << "\n  },\n";
    
    json << "  \"floatFlags\": {\n";
    first = true;
    for (const auto& [key, val] : m_floatFlags) {
        if (!first) json << ",\n";
        json << "    \"" << key << "\": " << val;
        first = false;
    }
    json << "\n  },\n";
    
    json << "  \"stringFlags\": {\n";
    first = true;
    for (const auto& [key, val] : m_stringFlags) {
        if (!first) json << ",\n";
        json << "    \"" << key << "\": \"" << val << "\"";
        first = false;
    }
    json << "\n  }\n";
    
    json << "}\n";
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
