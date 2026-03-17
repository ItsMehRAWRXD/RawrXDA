#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUuid>

namespace beacon {

/**
 * @brief Enum for the status of a TaskNode in the DAG.
 */
enum class TaskStatus {
    PENDING,
    IN_PROGRESS,
    COMPLETED,
    FAILED,
    SKIPPED
};

/**
 * @brief Represents a single task in the autonomous orchestration DAG.
 */
class TaskNode {
public:
    TaskNode(const QString& description = QString(), const QList<QString>& dependencies = QList<QString>());

    QString id;
    QString description;
    TaskStatus status;
    QList<QString> dependencies;
    QList<QString> outputs;
    QString log;

    QJsonObject toJson() const;
    static TaskNode fromJson(const QJsonObject& json);

    QString statusToString() const;
    static QString statusToString(TaskStatus status);
    static TaskStatus stringToStatus(const QString& statusStr);
};

} // namespace beacon
