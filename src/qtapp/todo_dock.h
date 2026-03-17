#pragma once

#include <QWidget>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;
class TodoManager;
struct TodoItem;

class TodoDock : public QWidget {
    Q_OBJECT
public:
    explicit TodoDock(TodoManager* todoManager, QWidget* parent = nullptr);
    void initialize();
    
public slots:
    void refreshTodos();
    
signals:
    void openFileRequested(const QString& filePath, const QString& todoId);
    
private slots:
    void onTodoAdded(const TodoItem& todo);
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
    
    QTreeWidget* treeWidget_;
    TodoManager* todoManager_;
};