// Planning Agent - AI planning and task management
#include "planning_agent.h"
#include <QTimer>
#include <QDateTime>
#include <QUuid>
#include <QDebug>
#include <QRandomGenerator>

PlanningAgent::PlanningAgent(QObject* parent) 
    : QObject(parent), currentTaskIndex_(-1) {
    taskProcessor_ = new QTimer(this);
    taskProcessor_->setInterval(2000); // Process tasks every 2 seconds
    connect(taskProcessor_, &QTimer::timeout, this, &PlanningAgent::processNextTask);
}

void PlanningAgent::initialize() {
    qDebug() << "Planning Agent initialized";
}

void PlanningAgent::createPlan(const QString& goal) {
    qDebug() << "Creating plan for goal:" << goal;
    
    // Generate a plan based on the goal
    QString plan = generatePlan(goal);
    emit planCreated(plan);
    
    // Start processing tasks
    currentTaskIndex_ = -1;
    taskProcessor_->start();
}

void PlanningAgent::executePlan() {
    if (!tasks_.isEmpty()) {
        currentTaskIndex_ = -1;
        taskProcessor_->start();
    }
}

void PlanningAgent::addTask(const Task& task) {
    tasks_.append(task);
}

QList<Task> PlanningAgent::getTasks() const {
    return tasks_;
}

void PlanningAgent::processNextTask() {
    if (tasks_.isEmpty()) {
        taskProcessor_->stop();
        emit planCompleted();
        return;
    }
    
    // Move to next task
    currentTaskIndex_++;
    if (currentTaskIndex_ >= tasks_.size()) {
        taskProcessor_->stop();
        emit planCompleted();
        return;
    }
    
    Task& task = tasks_[currentTaskIndex_];
    updateTaskStatus(task.id, "in-progress");
    
    // Simulate task execution with a delay
    QTimer::singleShot(3000, this, [this, taskId = task.id]() {
        // Randomly determine if task succeeds or fails
        bool success = QRandomGenerator::global()->bounded(100) < 90; // 90% success rate
        
        if (success) {
            updateTaskStatus(taskId, "completed");
        } else {
            updateTaskStatus(taskId, "failed");
            taskProcessor_->stop();
            emit planFailed("Task " + taskId + " failed");
        }
    });
}

QString PlanningAgent::generatePlan(const QString& goal) {
    tasks_.clear();
    
    // Generate tasks based on the goal
    if (goal.contains("code", Qt::CaseInsensitive)) {
        Task task1{QUuid::createUuid().toString(), "Analyze requirements", "pending", 1, "CodeAnalyzer"};
        Task task2{QUuid::createUuid().toString(), "Design solution", "pending", 2, "Architect"};
        Task task3{QUuid::createUuid().toString(), "Implement code", "pending", 3, "Developer"};
        Task task4{QUuid::createUuid().toString(), "Test implementation", "pending", 4, "Tester"};
        Task task5{QUuid::createUuid().toString(), "Document code", "pending", 5, "Documenter"};
        
        tasks_ << task1 << task2 << task3 << task4 << task5;
        return "Code development plan: Analyze → Design → Implement → Test → Document";
    } else if (goal.contains("debug", Qt::CaseInsensitive)) {
        Task task1{QUuid::createUuid().toString(), "Reproduce issue", "pending", 1, "Debugger"};
        Task task2{QUuid::createUuid().toString(), "Identify root cause", "pending", 2, "Analyzer"};
        Task task3{QUuid::createUuid().toString(), "Fix bug", "pending", 3, "Developer"};
        Task task4{QUuid::createUuid().toString(), "Verify fix", "pending", 4, "Tester"};
        
        tasks_ << task1 << task2 << task3 << task4;
        return "Debugging plan: Reproduce → Identify → Fix → Verify";
    } else {
        Task task1{QUuid::createUuid().toString(), "Research topic", "pending", 1, "Researcher"};
        Task task2{QUuid::createUuid().toString(), "Gather information", "pending", 2, "Collector"};
        Task task3{QUuid::createUuid().toString(), "Analyze findings", "pending", 3, "Analyzer"};
        Task task4{QUuid::createUuid().toString(), "Create report", "pending", 4, "Writer"};
        
        tasks_ << task1 << task2 << task3 << task4;
        return "General plan: Research → Gather → Analyze → Report";
    }
}

void PlanningAgent::updateTaskStatus(const QString& taskId, const QString& status) {
    for (int i = 0; i < tasks_.size(); ++i) {
        if (tasks_[i].id == taskId) {
            tasks_[i].status = status;
            emit taskStatusChanged(taskId, status);
            break;
        }
    }
}