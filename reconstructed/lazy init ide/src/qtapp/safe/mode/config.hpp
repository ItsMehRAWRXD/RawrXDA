#pragma once

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
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
        QMutexLocker locker(&mutex_);
        if (!enabled_) {
            return true;
        }
        return !disabled_.contains(flag);
    }

    bool isSafeModeEnabled() const {
        QMutexLocker locker(&mutex_);
        return enabled_;
    }

    void setSafeModeEnabled(bool enabled) {
        QMutexLocker locker(&mutex_);
        enabled_ = enabled;
    }

    void reload() {
        QMutexLocker locker(&mutex_);
        loadFromDisk();
    }

private:
    Config() : enabled_(false) { loadFromDisk(); }

    static QString flagToString(FeatureFlag flag) {
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

    static std::optional<FeatureFlag> flagFromString(const QString& name) {
        const QString key = name.trimmed();
        if (key.compare("ProjectExplorer", Qt::CaseInsensitive) == 0) return FeatureFlag::ProjectExplorer;
        if (key.compare("BuildSystem", Qt::CaseInsensitive) == 0) return FeatureFlag::BuildSystem;
        if (key.compare("VersionControl", Qt::CaseInsensitive) == 0) return FeatureFlag::VersionControl;
        if (key.compare("Debugger", Qt::CaseInsensitive) == 0) return FeatureFlag::Debugger;
        if (key.compare("TestRunner", Qt::CaseInsensitive) == 0) return FeatureFlag::TestRunner;
        if (key.compare("Database", Qt::CaseInsensitive) == 0) return FeatureFlag::Database;
        if (key.compare("Docker", Qt::CaseInsensitive) == 0) return FeatureFlag::Docker;
        if (key.compare("Cloud", Qt::CaseInsensitive) == 0) return FeatureFlag::Cloud;
        if (key.compare("AgentSystem", Qt::CaseInsensitive) == 0) return FeatureFlag::AgentSystem;
        if (key.compare("Planning", Qt::CaseInsensitive) == 0) return FeatureFlag::Planning;
        if (key.compare("AIChat", Qt::CaseInsensitive) == 0) return FeatureFlag::AIChat;
        return std::nullopt;
    }

    void loadFromDisk() {
        enabled_ = false;
        disabled_.clear();

        const QString envEnabled = qEnvironmentVariable("RAWRXD_SAFE_MODE").trimmed();
        if (envEnabled.compare("1") == 0 || envEnabled.compare("true", Qt::CaseInsensitive) == 0) {
            enabled_ = true;
        }

        const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QString configPath = configDir.isEmpty()
            ? QCoreApplication::applicationDirPath() + "/safe_mode.json"
            : configDir + "/safe_mode.json";

        QFile file(configPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const auto data = file.readAll();
            const auto doc = QJsonDocument::fromJson(data);
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

        const QString envDisabled = qEnvironmentVariable("RAWRXD_SAFE_MODE_FEATURES");
        if (!envDisabled.isEmpty()) {
            const auto features = envDisabled.split(',', Qt::SkipEmptyParts);
            for (const auto& feature : features) {
                const auto maybeFlag = flagFromString(feature);
                if (maybeFlag) disabled_.insert(*maybeFlag);
            }
        }
    }

    mutable QMutex mutex_;
    bool enabled_;
    QSet<FeatureFlag> disabled_;
};

} // namespace SafeMode