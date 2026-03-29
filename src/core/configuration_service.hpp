#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace RawrXD {

enum class ConfigurationScope {
    User,
    Workspace,
};

// Minimal configuration backend for P0.4:
// - User/workspace JSON documents
// - Typed get/set helpers
// - Change notifications per key
class ConfigurationService {
public:
    using ChangeCallback = std::function<void(const std::string&, ConfigurationScope)>;

    static ConfigurationService& instance() {
        static ConfigurationService svc;
        return svc;
    }

    void setStoragePath(ConfigurationScope scope, const std::string& filePath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        storage(scope).path = filePath;
    }

    std::string getStoragePath(ConfigurationScope scope) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return storageConst(scope).path;
    }

    bool load(ConfigurationScope scope) {
        std::lock_guard<std::mutex> lock(m_mutex);
        ScopeStorage& s = storage(scope);
        s.data = nlohmann::json::object();

        if (s.path.empty()) {
            return false;
        }

        std::ifstream in(s.path, std::ios::binary);
        if (!in) {
            return false;
        }

        try {
            const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (content.empty()) {
                return true;
            }
            s.data = nlohmann::json::parse(content, nullptr, false);
            if (s.data.is_discarded() || !s.data.is_object()) {
                s.data = nlohmann::json::object();
                return false;
            }
            return true;
        } catch (...) {
            s.data = nlohmann::json::object();
            return false;
        }
    }

    bool save(ConfigurationScope scope) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        const ScopeStorage& s = storageConst(scope);
        if (s.path.empty()) {
            return false;
        }

        try {
            std::filesystem::path p(s.path);
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
            (void)ec;

            std::ofstream out(s.path, std::ios::trunc);
            if (!out) {
                return false;
            }
            out << s.data.dump(2);
            return true;
        } catch (...) {
            return false;
        }
    }

    bool has(const std::string& key, ConfigurationScope scope = ConfigurationScope::User) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return storageConst(scope).data.contains(key);
    }

    std::string getString(const std::string& key, const std::string& defaultValue = {},
                          ConfigurationScope scope = ConfigurationScope::User) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        const ScopeStorage& s = storageConst(scope);
        if (!s.data.contains(key)) {
            return defaultValue;
        }
        return s.data.value(key, defaultValue);
    }

    int getInt(const std::string& key, int defaultValue = 0,
               ConfigurationScope scope = ConfigurationScope::User) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        const ScopeStorage& s = storageConst(scope);
        if (!s.data.contains(key)) {
            return defaultValue;
        }
        return s.data.value(key, defaultValue);
    }

    bool getBool(const std::string& key, bool defaultValue = false,
                 ConfigurationScope scope = ConfigurationScope::User) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        const ScopeStorage& s = storageConst(scope);
        if (!s.data.contains(key)) {
            return defaultValue;
        }
        return s.data.value(key, defaultValue);
    }

    double getFloat(const std::string& key, double defaultValue = 0.0,
                    ConfigurationScope scope = ConfigurationScope::User) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        const ScopeStorage& s = storageConst(scope);
        if (!s.data.contains(key)) {
            return defaultValue;
        }
        return s.data.value(key, defaultValue);
    }

    void setString(const std::string& key, const std::string& value,
                   ConfigurationScope scope = ConfigurationScope::User) {
        setValue(key, value, scope);
    }

    void setInt(const std::string& key, int value,
                ConfigurationScope scope = ConfigurationScope::User) {
        setValue(key, value, scope);
    }

    void setBool(const std::string& key, bool value,
                 ConfigurationScope scope = ConfigurationScope::User) {
        setValue(key, value, scope);
    }

    void setFloat(const std::string& key, double value,
                  ConfigurationScope scope = ConfigurationScope::User) {
        setValue(key, value, scope);
    }

    void remove(const std::string& key, ConfigurationScope scope = ConfigurationScope::User) {
        std::vector<ChangeCallback> callbacks;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ScopeStorage& s = storage(scope);
            if (!s.data.contains(key)) {
                return;
            }
            s.data.erase(key);
            callbacks = callbackSnapshotLocked();
        }
        notify(callbacks, key, scope);
    }

    int subscribe(ChangeCallback cb) {
        std::lock_guard<std::mutex> lock(m_mutex);
        const int id = ++m_nextSubscriberId;
        m_subscribers[id] = std::move(cb);
        return id;
    }

    void unsubscribe(int id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_subscribers.erase(id);
    }

private:
    struct ScopeStorage {
        std::string path;
        nlohmann::json data = nlohmann::json::object();
    };

    ConfigurationService() = default;

    ScopeStorage& storage(ConfigurationScope scope) {
        return (scope == ConfigurationScope::Workspace) ? m_workspace : m_user;
    }

    const ScopeStorage& storageConst(ConfigurationScope scope) const {
        return (scope == ConfigurationScope::Workspace) ? m_workspace : m_user;
    }

    template <typename T>
    void setValue(const std::string& key, const T& value, ConfigurationScope scope) {
        std::vector<ChangeCallback> callbacks;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ScopeStorage& s = storage(scope);
            if (!s.data.is_object()) {
                s.data = nlohmann::json::object();
            }

            const nlohmann::json newValue = value;
            const bool hasKey = s.data.contains(key);
            if (!hasKey || s.data[key] != newValue) {
                s.data[key] = newValue;
                changed = true;
                callbacks = callbackSnapshotLocked();
            }
        }
        if (changed) {
            notify(callbacks, key, scope);
        }
    }

    std::vector<ChangeCallback> callbackSnapshotLocked() const {
        std::vector<ChangeCallback> out;
        out.reserve(m_subscribers.size());
        for (const auto& kv : m_subscribers) {
            out.push_back(kv.second);
        }
        return out;
    }

    static void notify(const std::vector<ChangeCallback>& callbacks, const std::string& key,
                       ConfigurationScope scope) {
        for (const auto& cb : callbacks) {
            if (cb) {
                cb(key, scope);
            }
        }
    }

    mutable std::mutex m_mutex;
    ScopeStorage m_user;
    ScopeStorage m_workspace;

    int m_nextSubscriberId = 0;
    std::unordered_map<int, ChangeCallback> m_subscribers;
};

}  // namespace RawrXD
