#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QTimer>

struct Task {
    QString id;
    QString description;
    QString status; // "pending", "in-progress", "completed", "failed"
    int priority;
    QString assignedAgent;
};

class PlanningAgent : public QObject {
    Q_OBJECT
public:
    explicit PlanningAgent(QObject* parent = nullptr);
    virtual ~PlanningAgent() = default;
    
    void initialize();
    void createPlan(const QString& goal);
    void executePlan();
    void addTask(const Task& task);
    QList<Task> getTasks() const;
    
signals:
    void planCreated(const QString& plan);
    void taskStatusChanged(const QString& taskId, const QString& status);
    void planCompleted();
    void planFailed(const QString& error);
    
private slots:
    void processNextTask();
    
private:
    QList<Task> tasks_;
    QTimer* taskProcessor_;
    int currentTaskIndex_;
    
    QString generatePlan(const QString& goal);
    void updateTaskStatus(const QString& taskId, const QString& status);
};