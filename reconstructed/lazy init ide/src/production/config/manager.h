#pragma once

#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QVariant>

namespace RawrXD {

class ProductionConfigManager {
public:
    static ProductionConfigManager& instance();

    bool loadConfig(const QString& path = QString());
    QString getEnvironment() const;
    bool isFeatureEnabled(const QString& feature) const;
    QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;

private:
    ProductionConfigManager();

    void applyDefaults();
    void applyFeatureList(const QJsonObject& root);

    QJsonObject config_;
    QString environment_;
    QSet<QString> enabledFeatures_;
};

} // namespace RawrXD
