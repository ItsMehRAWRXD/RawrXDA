// Agentic File Operations Implementation
#include "agentic_file_operations.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QMessageBox>
#include <QDebug>
#include <QTextEdit>

// ============ AgenticActionDialog Implementation ============

AgenticActionDialog::AgenticActionDialog(const QString& filePath, ActionType type,
                                         const QString& content, QWidget* parent)
    : QDialog(parent)
    , m_filePath(filePath)
    , m_content(content)
    , m_actionType(type)
{
    setWindowTitle(QString("Confirm Agentic Action - %1").arg(
        type == CREATE_FILE ? "Create File" :
        type == MODIFY_FILE ? "Modify File" : "Delete File"));
    setMinimumSize(700, 500);
    setStyleSheet("background-color: #1e1e1e; color: #d4d4d4;");

    setupUI();
    applyDarkTheme();
}

void AgenticActionDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Title
    QString actionText;
    switch (m_actionType) {
        case CREATE_FILE:
            actionText = QString("✨ Create New File: %1").arg(m_filePath);
            break;
        case MODIFY_FILE:
            actionText = QString("✏️ Modify File: %1").arg(m_filePath);
            break;
        case DELETE_FILE:
            actionText = QString("🗑️ Delete File: %1").arg(m_filePath);
            break;
    }

    QLabel* titleLabel = new QLabel(actionText);
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #4ec9b0; margin: 10px 0;");
    mainLayout->addWidget(titleLabel);

    // Preview
    QLabel* previewLabel = new QLabel("Preview:");
    previewLabel->setStyleSheet("font-weight: bold; color: #ce9178;");
    mainLayout->addWidget(previewLabel);

    m_previewText = new QTextEdit();
    m_previewText->setPlainText(m_content);
    m_previewText->setReadOnly(true);
    m_previewText->setStyleSheet(
        "QTextEdit {"
        "  background-color: #252526;"
        "  color: #d4d4d4;"
        "  border: 1px solid #3e3e42;"
        "  border-radius: 3px;"
        "  padding: 5px;"
        "  font-family: 'Courier New', monospace;"
        "  font-size: 10px;"
        "}");
    mainLayout->addWidget(m_previewText);

    // Info message
    QLabel* infoLabel = new QLabel(
        "This file was created autonomously by the AI agent. Review the preview and confirm to keep it, or click Undo to discard.");
    infoLabel->setStyleSheet("color: #888888; font-size: 10px; margin-top: 10px;");
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_undoButton = new QPushButton("❌ Undo");
    m_undoButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #d16969;"
        "  color: white;"
        "  border: 1px solid #e81e3e;"
        "  border-radius: 3px;"
        "  padding: 8px 16px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #e81e3e; }"
        "QPushButton:pressed { background-color: #c01a35; }");
    buttonLayout->addWidget(m_undoButton);

    m_keepButton = new QPushButton("✅ Keep");
    m_keepButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #6bb66f;"
        "  color: white;"
        "  border: 1px solid #7ec97f;"
        "  border-radius: 3px;"
        "  padding: 8px 16px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #7ec97f; }"
        "QPushButton:pressed { background-color: #5da959; }");
    buttonLayout->addWidget(m_keepButton);

    mainLayout->addLayout(buttonLayout);

    // Connect buttons
    connect(m_keepButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_undoButton, &QPushButton::clicked, this, &QDialog::reject);
}

void AgenticActionDialog::applyDarkTheme()
{
    setStyleSheet(
        "AgenticActionDialog {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "}"
        "QLabel {"
        "  color: #d4d4d4;"
        "}");
}

// ============ AgenticFileOperations Implementation ============

AgenticFileOperations::AgenticFileOperations(QObject* parent)
    : QObject(parent)
{
    qDebug() << "[AgenticFileOperations] Initialized with Keep/Undo support";
}

void AgenticFileOperations::createFileWithApproval(const QString& filePath, const QString& content)
{
    qDebug() << "[AgenticFileOperations] Creating file with approval:" << filePath;

    AgenticActionDialog dialog(filePath, AgenticActionDialog::CREATE_FILE, content);

    if (dialog.exec() == QDialog::Accepted) {
        // Create directories if needed
        QFileInfo fileInfo(filePath);
        QDir dir(fileInfo.dir());
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                QMessageBox::warning(nullptr, "Error", "Failed to create directory: " + dir.path());
                return;
            }
        }

        // Write file
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(nullptr, "Error", "Failed to create file: " + filePath);
            return;
        }

        QTextStream stream(&file);
        stream << content;
        file.close();

        // Record in history
        FileAction action;
        action.filePath = filePath;
        action.actionType = AgenticActionDialog::CREATE_FILE;
        action.content = content;
        action.timestamp = QDateTime::currentDateTime();
        m_actionHistory.append(action);

        if (m_actionHistory.size() > MAX_HISTORY) {
            m_actionHistory.removeFirst();
        }

        emit fileCreated(filePath);
        qDebug() << "[AgenticFileOperations] File created and kept:" << filePath;
    } else {
        qDebug() << "[AgenticFileOperations] File creation undone:" << filePath;
        emit operationUndone(filePath);
    }
}

void AgenticFileOperations::modifyFileWithApproval(const QString& filePath, const QString& oldContent, const QString& newContent)
{
    qDebug() << "[AgenticFileOperations] Modifying file with approval:" << filePath;

    AgenticActionDialog dialog(filePath, AgenticActionDialog::MODIFY_FILE, newContent);

    if (dialog.exec() == QDialog::Accepted) {
        // Write modified content
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(nullptr, "Error", "Failed to modify file: " + filePath);
            return;
        }

        QTextStream stream(&file);
        stream << newContent;
        file.close();

        // Record in history
        FileAction action;
        action.filePath = filePath;
        action.actionType = AgenticActionDialog::MODIFY_FILE;
        action.content = newContent;
        action.oldContent = oldContent;
        action.timestamp = QDateTime::currentDateTime();
        m_actionHistory.append(action);

        if (m_actionHistory.size() > MAX_HISTORY) {
            m_actionHistory.removeFirst();
        }

        emit fileModified(filePath);
        qDebug() << "[AgenticFileOperations] File modified and kept:" << filePath;
    } else {
        qDebug() << "[AgenticFileOperations] File modification undone:" << filePath;
        emit operationUndone(filePath);
    }
}

void AgenticFileOperations::deleteFileWithApproval(const QString& filePath)
{
    qDebug() << "[AgenticFileOperations] Deleting file with approval:" << filePath;

    QFile file(filePath);
    QString content;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        content = stream.readAll();
        file.close();
    }

    AgenticActionDialog dialog(filePath, AgenticActionDialog::DELETE_FILE, content);

    if (dialog.exec() == QDialog::Accepted) {
        if (QFile::remove(filePath)) {
            // Record in history
            FileAction action;
            action.filePath = filePath;
            action.actionType = AgenticActionDialog::DELETE_FILE;
            action.content = content;  // Save for undo
            action.timestamp = QDateTime::currentDateTime();
            m_actionHistory.append(action);

            if (m_actionHistory.size() > MAX_HISTORY) {
                m_actionHistory.removeFirst();
            }

            emit fileDeleted(filePath);
            qDebug() << "[AgenticFileOperations] File deleted:" << filePath;
        } else {
            QMessageBox::warning(nullptr, "Error", "Failed to delete file: " + filePath);
        }
    } else {
        qDebug() << "[AgenticFileOperations] File deletion undone:" << filePath;
        emit operationUndone(filePath);
    }
}

void AgenticFileOperations::undoLastAction()
{
    if (m_actionHistory.isEmpty()) {
        qWarning() << "[AgenticFileOperations] No actions to undo";
        return;
    }

    FileAction action = m_actionHistory.takeLast();

    switch (action.actionType) {
        case AgenticActionDialog::CREATE_FILE:
            QFile::remove(action.filePath);
            qDebug() << "[AgenticFileOperations] Undone file creation:" << action.filePath;
            break;

        case AgenticActionDialog::MODIFY_FILE: {
            QFile file(action.filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                stream << action.oldContent;
                file.close();
            }
            qDebug() << "[AgenticFileOperations] Undone file modification:" << action.filePath;
            break;
        }

        case AgenticActionDialog::DELETE_FILE: {
            QFile file(action.filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                stream << action.content;
                file.close();
            }
            qDebug() << "[AgenticFileOperations] Undone file deletion:" << action.filePath;
            break;
        }
    }

    emit operationUndone(action.filePath);
}
