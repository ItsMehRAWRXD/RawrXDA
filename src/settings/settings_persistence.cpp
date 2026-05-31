#include "settings/settings_persistence.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iomanip>
#include <chrono>

namespace RawrXD::Settings {

using json = nlohmann::json;

SettingsPersistence::SettingsPersistence() = default;
SettingsPersistence::~SettingsPersistence() {
    if (m_syncEnabled) {
        save();
    }
}

bool SettingsPersistence::initialize(const std::string& configPath) {
    m_configPath = configPath;
    return load();
}

void SettingsPersistence::registerSchema(const SettingSchema& schema) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_schemas[schema.key] = schema;

    // Set default if not exists
    if (m_values.find(schema.key) == m_values.end()) {
        m_values[schema.key] = schema.defaultValue;
    }
}

void SettingsPersistence::registerSchemas(const std::vector<SettingSchema>& schemas) {
    for (const auto& schema : schemas) {
        registerSchema(schema);
    }
}

bool SettingsPersistence::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_values.find(key) != m_values.end();
}

void SettingsPersistence::reset(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_schemas.find(key);
    if (it != m_schemas.end()) {
        SettingChange change;
        change.key = key;
        change.oldValue = m_values[key];
        change.newValue = it->second.defaultValue;
        change.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

        m_values[key] = it->second.defaultValue;
        notifyChange(change);
    }
}

void SettingsPersistence::resetAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [key, schema] : m_schemas) {
        SettingChange change;
        change.key = key;
        change.oldValue = m_values[key];
        change.newValue = schema.defaultValue;
        change.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

        m_values[key] = schema.defaultValue;
        notifyChange(change);
    }
}

// Type-safe getters
bool SettingsPersistence::getBool(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_values.find(key);
    if (it != m_values.end() && it->second.type() == typeid(bool)) {
        return std::any_cast<bool>(it->second);
    }
    return false;
}

int64_t SettingsPersistence::getInt(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        if (it->second.type() == typeid(int64_t)) {
            return std::any_cast<int64_t>(it->second);
        }
        if (it->second.type() == typeid(int)) {
            return std::any_cast<int>(it->second);
        }
    }
    return 0;
}

double SettingsPersistence::getFloat(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_values.find(key);
    if (it != m_values.end()) {
        if (it->second.type() == typeid(double)) {
            return std::any_cast<double>(it->second);
        }
        if (it->second.type() == typeid(float)) {
            return std::any_cast<float>(it->second);
        }
    }
    return 0.0;
}

std::string SettingsPersistence::getString(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_values.find(key);
    if (it != m_values.end() && it->second.type() == typeid(std::string)) {
        return std::any_cast<std::string>(it->second);
    }
    return "";
}

// Type-safe setters
void SettingsPersistence::setBool(const std::string& key, bool value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    SettingChange change;
    change.key = key;
    change.oldValue = m_values[key];
    change.newValue = value;
    change.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

    m_values[key] = value;
    notifyChange(change);
}

void SettingsPersistence::setInt(const std::string& key, int64_t value) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Validate against schema
    auto schemaIt = m_schemas.find(key);
    if (schemaIt != m_schemas.end() && schemaIt->second.minValue.has_value()) {
        int64_t min = std::any_cast<int64_t>(schemaIt->second.minValue.value());
        if (value < min) value = min;
    }
    if (schemaIt != m_schemas.end() && schemaIt->second.maxValue.has_value()) {
        int64_t max = std::any_cast<int64_t>(schemaIt->second.maxValue.value());
        if (value > max) value = max;
    }

    SettingChange change;
    change.key = key;
    change.oldValue = m_values[key];
    change.newValue = value;
    change.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

    m_values[key] = value;
    notifyChange(change);
}

void SettingsPersistence::setFloat(const std::string& key, double value) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto schemaIt = m_schemas.find(key);
    if (schemaIt != m_schemas.end() && schemaIt->second.minValue.has_value()) {
        double min = std::any_cast<double>(schemaIt->second.minValue.value());
        if (value < min) value = min;
    }
    if (schemaIt != m_schemas.end() && schemaIt->second.maxValue.has_value()) {
        double max = std::any_cast<double>(schemaIt->second.maxValue.value());
        if (value > max) value = max;
    }

    SettingChange change;
    change.key = key;
    change.oldValue = m_values[key];
    change.newValue = value;
    change.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

    m_values[key] = value;
    notifyChange(change);
}

void SettingsPersistence::setString(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    SettingChange change;
    change.key = key;
    change.oldValue = m_values[key];
    change.newValue = value;
    change.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

    m_values[key] = value;
    notifyChange(change);
}

// Persistence
bool SettingsPersistence::load() {
    std::ifstream file(m_configPath);
    if (!file.is_open()) {
        return false;
    }

    try {
        json j;
        file >> j;

        std::lock_guard<std::mutex> lock(m_mutex);

        if (j.contains("version")) {
            m_version = j["version"];
        }

        if (j.contains("settings")) {
            for (auto& [key, value] : j["settings"].items()) {
                if (value.is_boolean()) {
                    m_values[key] = value.get<bool>();
                } else if (value.is_number_integer()) {
                    m_values[key] = value.get<int64_t>();
                } else if (value.is_number_float()) {
                    m_values[key] = value.get<double>();
                } else if (value.is_string()) {
                    m_values[key] = value.get<std::string>();
                }
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool SettingsPersistence::save() {
    std::lock_guard<std::mutex> lock(m_mutex);

    json j;
    j["version"] = m_version;
    j["settings"] = json::object();

    for (const auto& [key, value] : m_values) {
        if (value.type() == typeid(bool)) {
            j["settings"][key] = std::any_cast<bool>(value);
        } else if (value.type() == typeid(int64_t)) {
            j["settings"][key] = std::any_cast<int64_t>(value);
        } else if (value.type() == typeid(double)) {
            j["settings"][key] = std::any_cast<double>(value);
        } else if (value.type() == typeid(std::string)) {
            j["settings"][key] = std::any_cast<std::string>(value);
        }
    }

    std::ofstream file(m_configPath);
    if (!file.is_open()) {
        return false;
    }

    file << std::setw(4) << j << std::endl;
    return true;
}

void SettingsPersistence::onChanged(const std::string& key, ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callbacks[key].push_back(callback);
}

void SettingsPersistence::onAnyChanged(ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_globalCallbacks.push_back(callback);
}

void SettingsPersistence::notifyChange(const SettingChange& change) {
    m_history.push_back(change);
    if (m_history.size() > 1000) {
        m_history.erase(m_history.begin());
    }

    auto it = m_callbacks.find(change.key);
    if (it != m_callbacks.end()) {
        for (const auto& cb : it->second) {
            cb(change);
        }
    }

    for (const auto& cb : m_globalCallbacks) {
        cb(change);
    }

    if (m_syncEnabled) {
        save();
    }
}

std::vector<SettingChange> SettingsPersistence::getChangeHistory(size_t limit) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t start = m_history.size() > limit ? m_history.size() - limit : 0;
    return std::vector<SettingChange>(m_history.begin() + start, m_history.end());
}

void SettingsPersistence::clearHistory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.clear();
}

std::vector<std::string> SettingsPersistence::getCategories() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::set<std::string> categories;
    for (const auto& [key, schema] : m_schemas) {
        categories.insert(schema.category);
    }
    return std::vector<std::string>(categories.begin(), categories.end());
}

std::vector<SettingSchema> SettingsPersistence::getSettingsInCategory(const std::string& category) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<SettingSchema> result;
    for (const auto& [key, schema] : m_schemas) {
        if (schema.category == category) {
            result.push_back(schema);
        }
    }
    return result;
}

void SettingsPersistence::setSyncEnabled(bool enabled) {
    m_syncEnabled = enabled;
}

bool SettingsPersistence::isSyncEnabled() const {
    return m_syncEnabled;
}

void SettingsPersistence::syncNow() {
    save();
}

// Global instance
SettingsPersistence& getSettingsPersistence() {
    static SettingsPersistence instance;
    return instance;
}

std::string settingTypeToString(SettingType type) {
    switch (type) {
        case SettingType::Boolean: return "boolean";
        case SettingType::Integer: return "integer";
        case SettingType::Float: return "float";
        case SettingType::String: return "string";
        case SettingType::Array: return "array";
        case SettingType::Object: return "object";
        case SettingType::Color: return "color";
        case SettingType::Font: return "font";
        case SettingType::Keybinding: return "keybinding";
        default: return "unknown";
    }
}

} // namespace RawrXD::Settings
