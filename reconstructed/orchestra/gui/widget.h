#ifndef ORCHESTRA_GUI_WIDGET_H
#define ORCHESTRA_GUI_WIDGET_H

/**
 * @file orchestra_gui_widget.h
 * @brief Qt widgets for orchestra task management in the GUI
 */

#include "orchestra_integration.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QSpinBox>
#include <QLabel>
#include <QTextEdit>
#include <QProgressBar>
#include <QTimer>
#include <QString>
#include <memory>

/**
 * @class OrchestraTaskPanel
 * @brief GUI panel for submitting tasks to the orchestra
 */
class OrchestraTaskPanel : public QWidget {
    Q_OBJECT

public:
    explicit OrchestraTaskPanel(QWidget* parent = nullptr) : QWidget(parent) {
        setupUI();
        connect(&timer_, &QTimer::timeout, this, &OrchestraTaskPanel::updateProgress);
    }

    virtual ~OrchestraTaskPanel() = default;

private slots:
    void submitTask() {
        QString taskId = taskIdInput_->text().trimmed();
        QString goal = goalInput_->text().trimmed();
        QString file = fileInput_->text().trimmed();

        if (taskId.isEmpty() || goal.isEmpty()) {
            statusLabel_->setText("✗ Task ID and Goal are required");
            statusLabel_->setStyleSheet("color: red;");
            return;
        }

        OrchestraManager::getInstance().submitTask(
            taskId.toStdString(),
            goal.toStdString(),
            file.toStdString()
        );

        statusLabel_->setText("✓ Task submitted: " + taskId);
        statusLabel_->setStyleSheet("color: green;");

        taskIdInput_->clear();
        goalInput_->clear();
        fileInput_->clear();

        updateTaskList();
    }

    void executeAllTasks() {
        int numAgents = agentsSpinBox_->value();
        executeButton_->setEnabled(false);
        statusLabel_->setText("Executing tasks...");
        statusLabel_->setStyleSheet("color: blue;");

        progressBar_->setValue(0);
        progressBar_->setVisible(true);
        timer_.start(100);

        // Execute in background thread
        std::thread([this, numAgents]() {
            auto metrics = OrchestraManager::getInstance().executeAllTasks(numAgents);
            
            // Update results
            updateResults(metrics);
            
            executeButton_->setEnabled(true);
            timer_.stop();
            progressBar_->setValue(100);
            statusLabel_->setText("✓ Execution complete");
            statusLabel_->setStyleSheet("color: green;");
        }).detach();
    }

    void clearAllTasks() {
        OrchestraManager::getInstance().clearAllTasks();
        statusLabel_->setText("✓ All tasks cleared");
        statusLabel_->setStyleSheet("color: green;");
        updateTaskList();
        updateResultsTable();
    }

    void updateProgress() {
        double progress = OrchestraManager::getInstance().getProgressPercentage();
        progressBar_->setValue(static_cast<int>(progress));
    }

private:
    void setupUI() {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);

        // Title
        QLabel* titleLabel = new QLabel("Orchestra Task Management");
        QFont titleFont = titleLabel->font();
        titleFont.setPointSize(14);
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        mainLayout->addWidget(titleLabel);

        // Task submission section
        QGroupBox* submitGroup = new QGroupBox("Submit New Task");
        QVBoxLayout* submitLayout = new QVBoxLayout(submitGroup);

        // Task ID input
        QHBoxLayout* idLayout = new QHBoxLayout();
        idLayout->addWidget(new QLabel("Task ID:"));
        taskIdInput_ = new QLineEdit();
        taskIdInput_->setPlaceholderText("e.g., task-001");
        idLayout->addWidget(taskIdInput_);
        submitLayout->addLayout(idLayout);

        // Goal input
        QHBoxLayout* goalLayout = new QHBoxLayout();
        goalLayout->addWidget(new QLabel("Goal:"));
        goalInput_ = new QLineEdit();
        goalInput_->setPlaceholderText("e.g., Analyze security vulnerabilities");
        goalLayout->addWidget(goalInput_);
        submitLayout->addLayout(goalLayout);

        // File input
        QHBoxLayout* fileLayout = new QHBoxLayout();
        fileLayout->addWidget(new QLabel("File:"));
        fileInput_ = new QLineEdit();
        fileInput_->setPlaceholderText("e.g., main.cpp (optional)");
        fileLayout->addWidget(fileInput_);
        submitLayout->addLayout(fileLayout);

        // Submit button
        QPushButton* submitBtn = new QPushButton("Submit Task");
        connect(submitBtn, &QPushButton::clicked, this, &OrchestraTaskPanel::submitTask);
        submitLayout->addWidget(submitBtn);

        mainLayout->addWidget(submitGroup);

        // Execution section
        QGroupBox* execGroup = new QGroupBox("Execute Tasks");
        QVBoxLayout* execLayout = new QVBoxLayout(execGroup);

        // Agents selection
        QHBoxLayout* agentsLayout = new QHBoxLayout();
        agentsLayout->addWidget(new QLabel("Number of Agents:"));
        agentsSpinBox_ = new QSpinBox();
        agentsSpinBox_->setMinimum(1);
        agentsSpinBox_->setMaximum(16);
        agentsSpinBox_->setValue(4);
        agentsLayout->addWidget(agentsSpinBox_);
        agentsLayout->addStretch();
        execLayout->addLayout(agentsLayout);

        // Execute button
        executeButton_ = new QPushButton("Execute All Tasks");
        executeButton_->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 8px; }");
        connect(executeButton_, &QPushButton::clicked, this, &OrchestraTaskPanel::executeAllTasks);
        execLayout->addWidget(executeButton_);

        // Progress bar
        progressBar_ = new QProgressBar();
        progressBar_->setVisible(false);
        execLayout->addWidget(progressBar_);

        mainLayout->addWidget(execGroup);

        // Tasks list
        QGroupBox* tasksGroup = new QGroupBox("Pending Tasks");
        QVBoxLayout* tasksLayout = new QVBoxLayout(tasksGroup);
        tasksList_ = new QTableWidget();
        tasksList_->setColumnCount(3);
        tasksList_->setHorizontalHeaderLabels({"Task ID", "Goal", "File"});
        tasksList_->horizontalHeader()->setStretchLastSection(true);
        tasksLayout->addWidget(tasksList_);
        mainLayout->addWidget(tasksGroup);

        // Results section
        QGroupBox* resultsGroup = new QGroupBox("Task Results");
        QVBoxLayout* resultsLayout = new QVBoxLayout(resultsGroup);
        resultsTable_ = new QTableWidget();
        resultsTable_->setColumnCount(5);
        resultsTable_->setHorizontalHeaderLabels({"Task ID", "Status", "Exit Code", "Time (ms)", "Output"});
        resultsTable_->horizontalHeader()->setStretchLastSection(true);
        resultsLayout->addWidget(resultsTable_);
        mainLayout->addWidget(resultsGroup);

        // Status and control buttons
        QHBoxLayout* statusLayout = new QHBoxLayout();
        statusLabel_ = new QLabel("Ready");
        statusLabel_->setStyleSheet("color: blue;");
        statusLayout->addWidget(statusLabel_);
        statusLayout->addStretch();

        QPushButton* clearBtn = new QPushButton("Clear All");
        connect(clearBtn, &QPushButton::clicked, this, &OrchestraTaskPanel::clearAllTasks);
        statusLayout->addWidget(clearBtn);

        mainLayout->addLayout(statusLayout);

        setLayout(mainLayout);
    }

    void updateTaskList() {
        size_t count = OrchestraManager::getInstance().getPendingTaskCount();
        tasksList_->setRowCount(static_cast<int>(count));
        // Note: In a full implementation, this would show actual task details
    }

    void updateResultsTable() {
        auto results = OrchestraManager::getInstance().getAllResults();
        resultsTable_->setRowCount(static_cast<int>(results.size()));

        int row = 0;
        for (const auto& [taskId, result] : results) {
            QTableWidgetItem* idItem = new QTableWidgetItem(QString::fromStdString(taskId));
            QTableWidgetItem* statusItem = new QTableWidgetItem(
                result.success ? "SUCCESS" : "FAILED"
            );
            QTableWidgetItem* codeItem = new QTableWidgetItem(
                QString::number(result.exit_code)
            );
            QTableWidgetItem* timeItem = new QTableWidgetItem(
                QString::number(result.execution_time.count())
            );
            QTableWidgetItem* outputItem = new QTableWidgetItem(
                QString::fromStdString(result.output).left(50) + "..."
            );

            resultsTable_->setItem(row, 0, idItem);
            resultsTable_->setItem(row, 1, statusItem);
            resultsTable_->setItem(row, 2, codeItem);
            resultsTable_->setItem(row, 3, timeItem);
            resultsTable_->setItem(row, 4, outputItem);

            row++;
        }
    }

    void updateResults(const ExecutionMetrics& metrics) {
        updateResultsTable();
    }

    QLineEdit* taskIdInput_;
    QLineEdit* goalInput_;
    QLineEdit* fileInput_;
    QPushButton* executeButton_;
    QSpinBox* agentsSpinBox_;
    QProgressBar* progressBar_;
    QLabel* statusLabel_;
    QTableWidget* tasksList_;
    QTableWidget* resultsTable_;
    QTimer timer_;
};

#endif // ORCHESTRA_GUI_WIDGET_H
