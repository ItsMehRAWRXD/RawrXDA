#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include "digestion_metrics.h"

class DigestionDatabase : public QObject {
    Q_OBJECT
public:
    explicit DigestionDatabase(QObject *parent = nullptr);

    bool open(const QString &path, QString *error = nullptr);
    bool ensureSchema(const QString &schemaPath, QString *error = nullptr);
    bool insertRun(const QString &rootDir, const DigestionMetrics &metrics, QJsonObject report, int *runId, QString *error = nullptr);
    bool insertFileResult(int runId, const QJsonObject &fileObj, QString *error = nullptr);
    bool insertTask(int fileId, const QJsonObject &taskObj, QString *error = nullptr);

    QVector<QJsonObject> fetchRecentRuns(int limit = 10, QString *error = nullptr) const;
    bool deleteRun(int runId, QString *error = nullptr);

    static QString defaultSchema();

private:
    bool executeBatch(const QStringList &statements, QString *error);

    QSqlDatabase m_db;
};
