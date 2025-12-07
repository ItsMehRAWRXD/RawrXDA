#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>

struct TodoItem {
    QString id;
    QString description;
    QString filePath;
    int lineNumber;
    QDateTime created;
    QDateTime completed;
    bool isCompleted;
};

class TodoManager : public QObject {
    Q_OBJECT
public:
    explicit TodoManager(QObject* parent = nullptr);
    virtual ~TodoManager() = default;
    
    void addTodo(const QString& description, const QString& filePath = "", int lineNumber = -1);
    void completeTodo(const QString& id);
    void removeTodo(const QString& id);
    QList<TodoItem> getTodos() const;
    QList<TodoItem> getPendingTodos() const;
    QList<TodoItem> getCompletedTodos() const;
    
signals:
    void todoAdded(const TodoItem& todo);
    void todoCompleted(const QString& id);
    void todoRemoved(const QString& id);
    
private:
    QList<TodoItem> todos_;
};