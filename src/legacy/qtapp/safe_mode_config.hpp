#pragma once
#include <optional>

namespace SafeMode {

enum class FeatureFlag {
    ProjectExplorer,
    BuildSystem,
    VersionControl,
    Debugger,
    TestRunner,
    Database,
    Docker,
    Cloud,
    AgentSystem,
    Planning,
    AIChat
};

class Config {
public:
    static Config& instance() {
        static Config cfg;
        return cfg;
    }

    bool isFeatureEnabled(FeatureFlag flag) const {
        std::mutexLocker locker(&mutex_);
        if (!enabled_) {
            return true;
        }
        return !disabled_.contains(flag);
    }

    bool isSafeModeEnabled() const {
        std::mutexLocker locker(&mutex_);
        return enabled_;
    }

    void setSafeModeEnabled(bool enabled) {
        std::mutexLocker locker(&mutex_);
        enabled_ = enabled;
    }

    void reload() {
        std::mutexLocker locker(&mutex_);
        loadFromDisk();
    }

private:
    Config() : enabled_(false) { loadFromDisk(); }

    static std::string flagToString(FeatureFlag flag) {
        switch (flag) {
            case FeatureFlag::ProjectExplorer: return "ProjectExplorer";
            case FeatureFlag::BuildSystem: return "BuildSystem";
            case FeatureFlag::VersionControl: return "VersionControl";
            case FeatureFlag::Debugger: return "Debugger";
            case FeatureFlag::TestRunner: return "TestRunner";
            case FeatureFlag::Database: return "Database";
            case FeatureFlag::Docker: return "Docker";
            case FeatureFlag::Cloud: return "Cloud";
            case FeatureFlag::AgentSystem: return "AgentSystem";
            case FeatureFlag::Planning: return "Planning";
            case FeatureFlag::AIChat: return "AIChat";
        }
        return "Unknown";
    }

    static std::optional<FeatureFlag> flagFromString(const std::string& name) {
        const std::string key = name.trimmed();
        if (key.compare("ProjectExplorer", CaseInsensitive) == 0) return FeatureFlag::ProjectExplorer;
        if (key.compare("BuildSystem", CaseInsensitive) == 0) return FeatureFlag::BuildSystem;
        if (key.compare("VersionControl", CaseInsensitive) == 0) return FeatureFlag::VersionControl;
        if (key.compare("Debugger", CaseInsensitive) == 0) return FeatureFlag::Debugger;
        if (key.compare("TestRunner", CaseInsensitive) == 0) return FeatureFlag::TestRunner;
        if (key.compare("Database", CaseInsensitive) == 0) return FeatureFlag::Database;
        if (key.compare("Docker", CaseInsensitive) == 0) return FeatureFlag::Docker;
        if (key.compare("Cloud", CaseInsensitive) == 0) return FeatureFlag::Cloud;
        if (key.compare("AgentSystem", CaseInsensitive) == 0) return FeatureFlag::AgentSystem;
        if (key.compare("Planning", CaseInsensitive) == 0) return FeatureFlag::Planning;
        if (key.compare("AIChat", CaseInsensitive) == 0) return FeatureFlag::AIChat;
        return std::nullopt;
    }

    void loadFromDisk() {
        enabled_ = false;
        disabled_.clear();

        const std::string envEnabled = qEnvironmentVariable("RAWRXD_SAFE_MODE").trimmed();
        if (envEnabled.compare("1") == 0 || envEnabled.compare("true", CaseInsensitive) == 0) {
            enabled_ = true;
        }

        const std::string configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        std::string configPath = configDir.empty()
            ? QCoreApplication::applicationDirPath() + "/safe_mode.json"
            : configDir + "/safe_mode.json";

        // File operation removed;
        if (file.open(std::iostream::ReadOnly | std::iostream::Text)) {
            const auto data = file.readAll();
            const auto doc = nlohmann::json::fromJson(data);
            if (doc.isObject()) {
                const auto obj = doc.object();
                if (obj.contains("enabled")) {
                    enabled_ = obj.value("enabled").toBool(enabled_);
                }
                if (obj.contains("disabled_features") && obj.value("disabled_features").isArray()) {
                    const auto array = obj.value("disabled_features").toArray();
                    for (const auto& item : array) {
                        const auto maybeFlag = flagFromString(item.toString());
                        if (maybeFlag) disabled_.insert(*maybeFlag);
                    }
                }
            }
        }

        const std::string envDisabled = qEnvironmentVariable("RAWRXD_SAFE_MODE_FEATURES");
        if (!envDisabled.empty()) {
            const auto features = envDisabled.split(',', SkipEmptyParts);
            for (const auto& feature : features) {
                const auto maybeFlag = flagFromString(feature);
                if (maybeFlag) disabled_.insert(*maybeFlag);
            }
        }
    }

    mutable std::mutex mutex_;
    bool enabled_;
    std::set<FeatureFlag> disabled_;
};

} // namespace SafeMode

