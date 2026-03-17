#pragma once

#include <QWidget>
#include <QObject>

class TodoManager;
class QTreeWidget;
class QTreeWidgetItem;

class TodoDock : public QWidget
{
    Q_OBJECT

public:
    explicit TodoDock(TodoManager* todoManager, QWidget* parent = nullptr);
    
    void initialize();
    void refreshTodos();

signals:
    void openFileRequested(const QString& filePath, const QString& todoId);

private slots:
    void onTodoAdded(const class TodoItem& todo);
    void onTodoCompleted(const QString& id);
    void onTodoRemoved(const QString& id);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onAddTodo();
    void onCompleteTodo();
    void onRemoveTodo();
    void onScanCode();

private:
    void setupUI();
    void loadTodos();
    
    TodoManager* todoManager_;
    QTreeWidget* treeWidget_;
};