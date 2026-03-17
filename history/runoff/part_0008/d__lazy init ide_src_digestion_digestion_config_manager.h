#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include "digestion_reverse_engineering.h"

struct DigestionModuleConfig {
    DigestionConfig engineConfig;
    QString databasePath;
    QString schemaPath;
    QString outputPath;
    QStringList flags;
    bool enableDatabase = true;
    bool enableMetrics = true;
};

class DigestionConfigManager {
public:
    static DigestionModuleConfig loadFromFile(const QString &path, QString *error = nullptr);
    static DigestionModuleConfig loadFromJson(const QJsonObject &json, QString *error = nullptr);
    static DigestionModuleConfig loadFromYaml(const QString &yamlText, QString *error = nullptr);

    static QJsonObject parseYamlToJson(const QString &yamlText, QString *error = nullptr);

private:
    static QJsonValue parseScalar(const QString &value);
    static void assignValue(QJsonObject &root, const QString &section, const QString &key, const QJsonValue &value);
    static QStringList parseInlineList(const QString &value);
};
