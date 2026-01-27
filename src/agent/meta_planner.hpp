#pragma once
#include <QString>
#include <QJsonArray>
#include <QJsonObject>

class MetaPlanner {
public:
    // natural language  JSON task list
    QJsonArray plan(const QString& humanWish);

    // decompose high-level goal into sub-tasks (simple wrapper for now)
    QJsonArray decomposeGoal(const QString& goal) { return plan(goal); }

private:
    QJsonArray quantPlan(const QString& wish);
    QJsonArray kernelPlan(const QString& wish);
    QJsonArray releasePlan(const QString& wish);
    QJsonArray fixPlan(const QString& wish);
    QJsonArray perfPlan(const QString& wish);
    QJsonArray testPlan(const QString& wish);
    QJsonArray genericPlan(const QString& wish);

    QJsonObject task(const QString& type,
                     const QString& target,
                     const QJsonObject& params = {});
};
