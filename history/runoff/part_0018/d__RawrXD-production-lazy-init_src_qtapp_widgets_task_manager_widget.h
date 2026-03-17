/**
 * @file task_manager_widget.h
 * @brief Header for TaskManagerWidget - Task management interface
 */

#pragma once

#include <QWidget>

class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QLineEdit;
class QListWidget;
class QCheckBox;

class TaskManagerWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit TaskManagerWidget(QWidget* parent = nullptr);
    ~TaskManagerWidget();
    
public slots:
    void onAddTask();
    void onRemoveTask();
    void onCompleteTask();
    void onEditTask();
    
private:
    void setupUI();
    void connectSignals();
    
    QVBoxLayout* mMainLayout;
    QLineEdit* mTaskInput;
    QPushButton* mAddButton;
    QPushButton* mRemoveButton;
    QPushButton* mCompleteButton;
    QPushButton* mEditButton;
    QListWidget* mTaskList;
};

#endif // TASK_MANAGER_WIDGET_H
