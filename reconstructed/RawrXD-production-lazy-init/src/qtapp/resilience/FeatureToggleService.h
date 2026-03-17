#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QMutex>

// FeatureToggleService manages runtime feature flags with metadata and override support.
class FeatureToggleService : public QObject {
    Q_OBJECT
public:
    struct Toggle {
        QString key;
        bool enabled{false};
        QString description;
        QString owner;
        QStringList tags;
    };

    explicit FeatureToggleService(QObject* parent = nullptr);
    ~FeatureToggleService();

    bool define(const Toggle& t);
    bool set(const QString& key, bool enabled);
    bool remove(const QString& key);

    bool isEnabled(const QString& key) const;
    Toggle get(const QString& key) const;
    QList<Toggle> list(const QString& tagFilter = QString()) const;

signals:
    void toggleChanged(const QString& key, bool enabled);

private:
    QMap<QString, Toggle> m_toggles;
    mutable QMutex m_mutex;
};
