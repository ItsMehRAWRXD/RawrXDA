#pragma once
#include <QString>
#include <QJsonArray>

class Planner {
public:
    // Convert natural language wish into structured task list
    QJsonArray plan(const QString& humanWish);

private:
    QJsonArray planQuantKernel(const QString& wish);
    QJsonArray planRelease(const QString& wish);
    QJsonArray planWebProject(const QString& wish);
    QJsonArray planSelfReplication(const QString& wish);
    QJsonArray planGeneric(const QString& wish);
};
