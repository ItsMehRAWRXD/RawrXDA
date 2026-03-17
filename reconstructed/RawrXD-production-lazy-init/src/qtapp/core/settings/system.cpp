#include "settings_system.h"

#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <stdexcept>

/**
 * @file settings_system.cpp
 * @brief Implementation of centralized settings management
 */

SettingsSystem& SettingsSystem::instance() {
    static SettingsSystem inst;
    return inst;
}

SettingsSystem::SettingsSystem()
    : QObject(nullptr)
{
    try {
        initializeDefaults();
        loadSettings();
        qDebug() << "[SettingsSystem] Initialized with" << m_settings.size() << "settings";
    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error during initialization:" << e.what();
    }
}

SettingsSystem::~SettingsSystem() {
    try {
        saveSettings();
        qDebug() << "[SettingsSystem] Shutdown";
    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error during shutdown:" << e.what();
    }
}

void SettingsSystem::registerSetting(const Setting& setting) {
    try {
        if (setting.key.isEmpty()) {
            qWarning() << "[SettingsSystem] Cannot register setting with empty key";
            return;
        }

        Setting s = setting;
        if (s.currentValue.isNull()) {
            s.currentValue = s.defaultValue;
        }

        m_settings[setting.key] = s;
        qDebug() << "[SettingsSystem] Registered setting:" << setting.key;

    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error registering setting:" << e.what();
    }
}

QVariant SettingsSystem::getSetting(const QString& key, const QVariant& defaultValue) const {
    try {
        if (m_settings.contains(key)) {
            return m_settings[key].currentValue;
        }
        return defaultValue;
    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error getting setting:" << e.what();
        return defaultValue;
    }
}

bool SettingsSystem::setSetting(const QString& key, const QVariant& value) {
    try {
        if (!m_settings.contains(key)) {
            qWarning() << "[SettingsSystem] Unknown setting:" << key;
            return false;
        }

        // Validate value
        if (!validateValue(key, value)) {
            qWarning() << "[SettingsSystem] Invalid value for setting:" << key;
            return false;
        }

        Setting& setting = m_settings[key];
        QVariant oldValue = setting.currentValue;

        // Coerce value to correct type
        setting.currentValue = validateAndCoerce(key, value);

        qDebug() << "[SettingsSystem] Setting changed:" << key << "=" << setting.currentValue;

        // Emit change signal
        emit settingChanged(key, setting.currentValue, oldValue);

        // Notify watchers
        if (m_watchers.contains(key)) {
            for (const SettingWatcher& watcher : m_watchers[key]) {
                if (watcher.receiver && watcher.member) {
                    QMetaObject::invokeMethod(watcher.receiver, watcher.member);
                }
            }
        }

        return true;

    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error setting value:" << e.what();
        return false;
    }
}

QMap<QString, QVariant> SettingsSystem::getSettingsInCategory(Category category) const {
    QMap<QString, QVariant> result;

    try {
        for (auto it = m_settings.begin(); it != m_settings.end(); ++it) {
            if (it.value().category == category) {
                result[it.key()] = it.value().currentValue;
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error getting category settings:" << e.what();
    }

    return result;
}

QMap<QString, QVariant> SettingsSystem::getAllSettings() const {
    QMap<QString, QVariant> result;

    try {
        for (auto it = m_settings.begin(); it != m_settings.end(); ++it) {
            result[it.key()] = it.value().currentValue;
        }
    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error getting all settings:" << e.what();
    }

    return result;
}

bool SettingsSystem::hasSetting(const QString& key) const {
    return m_settings.contains(key);
}

SettingsSystem::Setting SettingsSystem::getSettingDefinition(const QString& key) const {
    if (m_settings.contains(key)) {
        return m_settings[key];
    }
    return Setting();
}

QStringList SettingsSystem::getRegisteredSettings() const {
    return m_settings.keys();
}

void SettingsSystem::saveSettings() {
    try {
        QSettings settings(SETTINGS_ORG, SETTINGS_APP);

        for (auto it = m_settings.begin(); it != m_settings.end(); ++it) {
            settings.setValue(it.key(), it.value().currentValue);
        }

        settings.setValue(VERSION_KEY, m_currentVersion);
        settings.sync();

        qDebug() << "[SettingsSystem] Settings saved";
        emit settingsSaved();

    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error saving settings:" << e.what();
    }
}

void SettingsSystem::loadSettings() {
    try {
        QSettings settings(SETTINGS_ORG, SETTINGS_APP);

        int loadedVersion = settings.value(VERSION_KEY, 0).toInt();

        // Migrate if needed
        if (loadedVersion < m_currentVersion) {
            qDebug() << "[SettingsSystem] Migrating from version" << loadedVersion 
                     << "to" << m_currentVersion;
            migrateSettings(loadedVersion);
        }

        // Load each setting
        for (auto it = m_settings.begin(); it != m_settings.end(); ++it) {
            if (settings.contains(it.key())) {
                QVariant value = settings.value(it.key());
                it.value().currentValue = validateAndCoerce(it.key(), value);
            } else {
                it.value().currentValue = it.value().defaultValue;
            }
        }

        qDebug() << "[SettingsSystem] Settings loaded from persistent storage";
        emit settingsLoaded();

    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error loading settings:" << e.what();
    }
}

void SettingsSystem::resetToDefaults() {
    try {
        for (auto it = m_settings.begin(); it != m_settings.end(); ++it) {
            it.value().currentValue = it.value().defaultValue;
        }

        saveSettings();
        qDebug() << "[SettingsSystem] All settings reset to defaults";
        emit settingsReset();

    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error resetting settings:" << e.what();
    }
}

void SettingsSystem::resetCategoryToDefaults(Category category) {
    try {
        for (auto it = m_settings.begin(); it != m_settings.end(); ++it) {
            if (it.value().category == category) {
                it.value().currentValue = it.value().defaultValue;
            }
        }

        saveSettings();
        qDebug() << "[SettingsSystem] Category reset to defaults";
        emit settingsReset();

    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error resetting category:" << e.what();
    }
}

bool SettingsSystem::resetSetting(const QString& key) {
    try {
        if (!m_settings.contains(key)) {
            return false;
        }

        m_settings[key].currentValue = m_settings[key].defaultValue;
        saveSettings();
        return true;

    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error resetting setting:" << e.what();
        return false;
    }
}

bool SettingsSystem::importSettings(const QJsonObject& json) {
    try {
        for (auto it = json.begin(); it != json.end(); ++it) {
            if (m_settings.contains(it.key())) {
                setSetting(it.key(), it.value().toVariant());
            }
        }

        saveSettings();
        qDebug() << "[SettingsSystem] Settings imported from JSON";
        return true;

    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error importing settings:" << e.what();
        return false;
    }
}

QJsonObject SettingsSystem::exportSettings() const {
    QJsonObject obj;

    try {
        for (auto it = m_settings.begin(); it != m_settings.end(); ++it) {
            obj[it.key()] = QJsonValue::fromVariant(it.value().currentValue);
        }
    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error exporting settings:" << e.what();
    }

    return obj;
}

QJsonObject SettingsSystem::exportCategory(Category category) const {
    QJsonObject obj;

    try {
        for (auto it = m_settings.begin(); it != m_settings.end(); ++it) {
            if (it.value().category == category) {
                obj[it.key()] = QJsonValue::fromVariant(it.value().currentValue);
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error exporting category:" << e.what();
    }

    return obj;
}

bool SettingsSystem::validateValue(const QString& key, const QVariant& value) const {
    try {
        if (!m_settings.contains(key)) {
            return false;
        }

        const Setting& setting = m_settings[key];

        // Type check
        if (setting.valueType == "int") {
            bool ok;
            value.toInt(&ok);
            if (!ok) return false;

            if (setting.minValue.isValid() && value.toInt() < setting.minValue.toInt())
                return false;
            if (setting.maxValue.isValid() && value.toInt() > setting.maxValue.toInt())
                return false;
        } else if (setting.valueType == "double") {
            bool ok;
            value.toDouble(&ok);
            if (!ok) return false;

            if (setting.minValue.isValid() && value.toDouble() < setting.minValue.toDouble())
                return false;
            if (setting.maxValue.isValid() && value.toDouble() > setting.maxValue.toDouble())
                return false;
        } else if (setting.valueType == "bool") {
            // Any type can be converted to bool
        } else if (setting.valueType == "string") {
            // Any type can be converted to string
        } else if (!setting.enumValues.isEmpty()) {
            // Enum validation
            if (!setting.enumValues.contains(value.toString())) {
                return false;
            }
        }

        return true;

    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error validating value:" << e.what();
        return false;
    }
}

QString SettingsSystem::getValueType(const QString& key) const {
    if (m_settings.contains(key)) {
        return m_settings[key].valueType;
    }
    return "string";
}

QStringList SettingsSystem::getEnumValues(const QString& key) const {
    if (m_settings.contains(key)) {
        return m_settings[key].enumValues;
    }
    return QStringList();
}

void SettingsSystem::watchSetting(const QString& key, QObject* receiver, const char* member) {
    try {
        if (!receiver || !member) {
            return;
        }

        SettingWatcher watcher;
        watcher.receiver = receiver;
        watcher.member = member;

        m_watchers[key].append(watcher);
        qDebug() << "[SettingsSystem] Watching setting:" << key;

    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error watching setting:" << e.what();
    }
}

void SettingsSystem::unwatchSetting(const QString& key, QObject* receiver) {
    try {
        if (m_watchers.contains(key)) {
            auto& watchers = m_watchers[key];
            watchers.erase(
                std::remove_if(watchers.begin(), watchers.end(),
                    [receiver](const SettingWatcher& w) { return w.receiver == receiver; }),
                watchers.end()
            );
        }
    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error unwatching setting:" << e.what();
    }
}

QStringList SettingsSystem::getSettingsGroup(const QString& groupName) const {
    if (m_settingsGroups.contains(groupName)) {
        return m_settingsGroups[groupName];
    }
    return QStringList();
}

void SettingsSystem::registerSettingsGroup(const QString& groupName, const QStringList& settingKeys) {
    try {
        m_settingsGroups[groupName] = settingKeys;
        qDebug() << "[SettingsSystem] Registered settings group:" << groupName;
    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error registering group:" << e.what();
    }
}

bool SettingsSystem::migrateSettings(int fromVersion) {
    try {
        if (fromVersion == 0) {
            return migrateFromV0();
        }
        return true;
    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Migration error:" << e.what();
        return false;
    }
}

int SettingsSystem::getSettingsVersion() const {
    return m_currentVersion;
}

QMap<QString, QVariant> SettingsSystem::getEditorSettings() const {
    return getSettingsInCategory(Category::Editor);
}

QMap<QString, QVariant> SettingsSystem::getBuildSettings() const {
    return getSettingsInCategory(Category::Build);
}

QMap<QString, QVariant> SettingsSystem::getAppearanceSettings() const {
    return getSettingsInCategory(Category::Appearance);
}

void SettingsSystem::initializeDefaults() {
    try {
        // Editor settings
        registerSetting({
            "Editor/FontFamily", "Font Family", "Default font family",
            "Consolas", {}, Category::Editor, "string", {}, {}, {}
        });

        registerSetting({
            "Editor/FontSize", "Font Size", "Editor font size in points",
            12, {}, Category::Editor, "int", 6, 32, {}
        });

        registerSetting({
            "Editor/TabWidth", "Tab Width", "Spaces per tab",
            4, {}, Category::Editor, "int", 1, 8, {}
        });

        registerSetting({
            "Editor/UseSpaces", "Use Spaces", "Use spaces instead of tabs",
            true, {}, Category::Editor, "bool", {}, {}, {}
        });

        registerSetting({
            "Editor/ShowLineNumbers", "Show Line Numbers", "Display line numbers",
            true, {}, Category::Editor, "bool", {}, {}, {}
        });

        registerSetting({
            "Editor/ShowWhitespace", "Show Whitespace", "Visualize whitespace characters",
            false, {}, Category::Editor, "bool", {}, {}, {}
        });

        // Appearance settings
        registerSetting({
            "Appearance/Theme", "Theme", "Application theme",
            "Dark", {}, Category::Appearance, "string", {}, {},
            QStringList() << "Light" << "Dark" << "HighContrast"
        });

        registerSetting({
            "Appearance/SyntaxTheme", "Syntax Theme", "Code syntax highlighting theme",
            "OneDark", {}, Category::Appearance, "string", {}, {},
            QStringList() << "OneDark" << "OneLight" << "Monokai" << "Solarized"
        });

        // Build settings
        registerSetting({
            "Build/CMakePath", "CMake Path", "Path to CMake executable",
            "cmake", {}, Category::Build, "string", {}, {}, {}
        });

        registerSetting({
            "Build/CompileFlags", "Compile Flags", "Additional compiler flags",
            "", {}, Category::Build, "string", {}, {}, {}
        });

        registerSetting({
            "Build/BuildType", "Build Type", "Release or Debug",
            "Debug", {}, Category::Build, "string", {}, {},
            QStringList() << "Debug" << "Release" << "MinSizeRel" << "RelWithDebInfo"
        });

        // Performance settings
        registerSetting({
            "Performance/MaxMemoryMB", "Max Memory (MB)", "Maximum memory usage",
            2048, {}, Category::Performance, "int", 512, 16384, {}
        });

        registerSetting({
            "Performance/NumThreads", "Thread Count", "Number of worker threads",
            4, {}, Category::Performance, "int", 1, 32, {}
        });

        qDebug() << "[SettingsSystem] Initialized default settings";

    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error initializing defaults:" << e.what();
    }
}

bool SettingsSystem::migrateFromV0() {
    try {
        // Migration logic from version 0 to 1
        // This would handle any format changes or renamed settings
        qDebug() << "[SettingsSystem] Migrated from version 0";
        emit settingsMigrated(0, m_currentVersion);
        return true;
    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Migration error:" << e.what();
        return false;
    }
}

QVariant SettingsSystem::validateAndCoerce(const QString& key, const QVariant& value) const {
    try {
        if (!m_settings.contains(key)) {
            return value;
        }

        const Setting& setting = m_settings[key];

        if (setting.valueType == "int") {
            return value.toInt();
        } else if (setting.valueType == "double") {
            return value.toDouble();
        } else if (setting.valueType == "bool") {
            return value.toBool();
        } else if (setting.valueType == "string" || setting.valueType == "color" || setting.valueType == "font") {
            return value.toString();
        }

        return value;

    } catch (const std::exception& e) {
        qWarning() << "[SettingsSystem] Error coercing value:" << e.what();
        return value;
    }
}
