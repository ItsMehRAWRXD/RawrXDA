/**
 * @file task_manager_widget.cpp
 * @brief Implementation of TaskManagerWidget - Task management
 */

#include "task_manager_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>

TaskManagerWidget::TaskManagerWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    connectSignals();
    setWindowTitle("Task Manager");
}

TaskManagerWidget::~TaskManagerWidget() = default;

void TaskManagerWidget::setupUI()
{
    mMainLayout = new QVBoxLayout(this);
    
    QHBoxLayout* inputLayout = new QHBoxLayout();
    mTaskInput = new QLineEdit(this);
    mTaskInput->setPlaceholderText("Enter a new task...");
    inputLayout->addWidget(mTaskInput);
    
    mAddButton = new QPushButton("Add", this);
    mAddButton->setStyleSheet("background-color: #4CAF50; color: white;");
    inputLayout->addWidget(mAddButton);
    
    mMainLayout->addLayout(inputLayout);
    
    mTaskList = new QListWidget(this);
    mMainLayout->addWidget(mTaskList);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    mCompleteButton = new QPushButton("Complete", this);
    buttonLayout->addWidget(mCompleteButton);
    
    mEditButton = new QPushButton("Edit", this);
    buttonLayout->addWidget(mEditButton);
    
    mRemoveButton = new QPushButton("Remove", this);
    mRemoveButton->setStyleSheet("background-color: #f44336; color: white;");
    buttonLayout->addWidget(mRemoveButton);
    
    buttonLayout->addStretch();
    mMainLayout->addLayout(buttonLayout);
}

void TaskManagerWidget::connectSignals()
{
    connect(mAddButton, &QPushButton::clicked, this, &TaskManagerWidget::onAddTask);
    connect(mRemoveButton, &QPushButton::clicked, this, &TaskManagerWidget::onRemoveTask);
    connect(mCompleteButton, &QPushButton::clicked, this, &TaskManagerWidget::onCompleteTask);
    connect(mEditButton, &QPushButton::clicked, this, &TaskManagerWidget::onEditTask);
}

void TaskManagerWidget::onAddTask()
{
    QString task = mTaskInput->text().trimmed();
    if (!task.isEmpty()) {
        mTaskList->addItem(task);
        mTaskInput->clear();
    }
}

void TaskManagerWidget::onRemoveTask()
{
    int row = mTaskList->currentRow();
    if (row >= 0) {
        delete mTaskList->takeItem(row);
    }
}

void TaskManagerWidget::onCompleteTask()
{
    int row = mTaskList->currentRow();
    if (row >= 0) {
        QListWidgetItem* item = mTaskList->item(row);
        if (item) {
            item->setText("✓ " + item->text());
        }
    }
}

void TaskManagerWidget::onEditTask()
{
    int row = mTaskList->currentRow();
    if (row >= 0) {
        mTaskInput->setText(mTaskList->item(row)->text());
    }
}
