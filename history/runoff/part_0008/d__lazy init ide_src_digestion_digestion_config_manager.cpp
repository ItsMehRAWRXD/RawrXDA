#include "digestion_config_manager.h"
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>

DigestionModuleConfig DigestionConfigManager::loadFromFile(const QString &path, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QString("Failed to open config: %1").arg(file.errorString());
        return DigestionModuleConfig();
    }

    const QByteArray data = file.readAll();
    const QString text = QString::fromUtf8(data);

    if (path.endsWith(".yaml", Qt::CaseInsensitive) || path.endsWith(".yml", Qt::CaseInsensitive)) {
        return loadFromYaml(text, error);
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error) *error = QString("JSON parse error: %1").arg(parseError.errorString());
        return DigestionModuleConfig();
    }
    return loadFromJson(doc.object(), error);
}

DigestionModuleConfig DigestionConfigManager::loadFromJson(const QJsonObject &json, QString *error) {
    DigestionModuleConfig moduleConfig;

    const QJsonObject digestion = json.value("digestion").toObject();
    const QJsonObject database = json.value("database").toObject();

    moduleConfig.engineConfig.chunkSize = digestion.value("chunk_size").toInt(moduleConfig.engineConfig.chunkSize);
    moduleConfig.engineConfig.threadCount = digestion.value("threads").toInt(moduleConfig.engineConfig.threadCount);
    moduleConfig.engineConfig.maxTasksPerFile = digestion.value("max_tasks_per_file").toInt(moduleConfig.engineConfig.maxTasksPerFile);
    moduleConfig.engineConfig.maxFiles = digestion.value("max_files").toInt(moduleConfig.engineConfig.maxFiles);
    moduleConfig.engineConfig.applyExtensions = digestion.value("apply_fixes").toBool(moduleConfig.engineConfig.applyExtensions);
    moduleConfig.engineConfig.createBackups = digestion.value("create_backups").toBool(moduleConfig.engineConfig.createBackups);
    moduleConfig.engineConfig.incremental = digestion.value("incremental").toBool(moduleConfig.engineConfig.incremental);
    moduleConfig.engineConfig.useGitMode = digestion.value("git_mode").toBool(moduleConfig.engineConfig.useGitMode);
    moduleConfig.engineConfig.backupDir = digestion.value("backup_dir").toString(moduleConfig.engineConfig.backupDir);

    const QJsonValue flagsValue = digestion.value("flags");
    if (flagsValue.isArray()) {
        for (const QJsonValue &flag : flagsValue.toArray()) {
            moduleConfig.flags.append(flag.toString());
        }
    } else if (flagsValue.isString()) {
        moduleConfig.flags = flagsValue.toString().split(',', Qt::SkipEmptyParts);
    }

    moduleConfig.databasePath = database.value("path").toString("digestion_results.db");
    moduleConfig.schemaPath = database.value("schema").toString();
    moduleConfig.enableDatabase = database.value("enabled").toBool(true);
    moduleConfig.outputPath = digestion.value("output_path").toString();

    if (moduleConfig.engineConfig.chunkSize <= 0) {
        moduleConfig.engineConfig.chunkSize = 50;
    }

    if (moduleConfig.engineConfig.threadCount < 0) {
        moduleConfig.engineConfig.threadCount = 0;
    }

    if (moduleConfig.databasePath.isEmpty() && moduleConfig.enableDatabase) {
        if (error) *error = "Database path is empty";
        moduleConfig.enableDatabase = false;
    }

    return moduleConfig;
}

DigestionModuleConfig DigestionConfigManager::loadFromYaml(const QString &yamlText, QString *error) {
    QJsonObject json = parseYamlToJson(yamlText, error);
    return loadFromJson(json, error);
}

QJsonObject DigestionConfigManager::parseYamlToJson(const QString &yamlText, QString *error) {
    QJsonObject root;
    QString currentSection;
    QString currentListKey;
    QJsonArray currentList;

    const QStringList lines = yamlText.split(QRegularExpression("\r?\n"));
    auto flushList = [&]() {
        if (!currentListKey.isEmpty()) {
            QJsonObject sectionObj = root.value(currentSection).toObject();
            sectionObj.insert(currentListKey, currentList);
            root.insert(currentSection, sectionObj);
            currentListKey.clear();
            currentList = QJsonArray();
        }
    };

    for (const QString &rawLine : lines) {
        QString line = rawLine;
        const int commentIndex = line.indexOf('#');
        if (commentIndex >= 0) line = line.left(commentIndex);
        line = line.trimmed();
        if (line.isEmpty()) continue;

        if (line.startsWith("- ")) {
            if (currentListKey.isEmpty()) {
                if (error) *error = "YAML list item without list key";
                continue;
            }
            currentList.append(parseScalar(line.mid(2).trimmed()));
            continue;
        }

        const int colonIndex = line.indexOf(':');
        if (colonIndex < 0) {
            if (error) *error = QString("Invalid YAML line: %1").arg(line);
            continue;
        }

        const QString key = line.left(colonIndex).trimmed();
        QString value = line.mid(colonIndex + 1).trimmed();

        if (value.isEmpty()) {
            flushList();
            currentSection = key;
            if (!root.contains(currentSection)) root.insert(currentSection, QJsonObject());
            continue;
        }

        if (value.startsWith('[') && value.endsWith(']')) {
            flushList();
            QJsonArray list;
            const QStringList parts = parseInlineList(value);
            for (const QString &part : parts) list.append(parseScalar(part));
            assignValue(root, currentSection, key, list);
            continue;
        }

        if (value == "|") {
            if (error) *error = "Multiline YAML values are not supported";
            continue;
        }

        if (value == "-") {
            currentListKey = key;
            currentList = QJsonArray();
            continue;
        }

        flushList();
        assignValue(root, currentSection, key, parseScalar(value));
    }

    flushList();
    return root;
}

QJsonValue DigestionConfigManager::parseScalar(const QString &value) {
    const QString trimmed = value.trimmed();
    if (trimmed.compare("true", Qt::CaseInsensitive) == 0) return true;
    if (trimmed.compare("false", Qt::CaseInsensitive) == 0) return false;

    bool ok = false;
    const int intValue = trimmed.toInt(&ok);
    if (ok) return intValue;

    const double doubleValue = trimmed.toDouble(&ok);
    if (ok) return doubleValue;

    QString cleaned = trimmed;
    if ((cleaned.startsWith('"') && cleaned.endsWith('"')) || (cleaned.startsWith('\\'') && cleaned.endsWith('\\''))) {
        cleaned = cleaned.mid(1, cleaned.size() - 2);
    }
    return cleaned;
}

void DigestionConfigManager::assignValue(QJsonObject &root, const QString &section, const QString &key, const QJsonValue &value) {
    if (section.isEmpty()) {
        root.insert(key, value);
        return;
    }
    QJsonObject sectionObj = root.value(section).toObject();
    sectionObj.insert(key, value);
    root.insert(section, sectionObj);
}

QStringList DigestionConfigManager::parseInlineList(const QString &value) {
    QString inner = value.mid(1, value.length() - 2).trimmed();
    QStringList parts;
    for (const QString &part : inner.split(',', Qt::SkipEmptyParts)) {
        parts.append(part.trimmed());
    }
    return parts;
}
