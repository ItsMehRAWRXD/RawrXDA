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
    titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #4EC9B0;");
    mainLayout->addWidget(titleLabel);

    // Content preview
    QLabel* previewLabel = new QLabel("Content Preview:");
    previewLabel->setStyleSheet("font-size: 11px; color: #858585;");
    mainLayout->addWidget(previewLabel);

    m_previewText = new QTextEdit();
    m_previewText->setPlainText(m_content);
    m_previewText->setReadOnly(true);
    m_previewText->setStyleSheet(
        "QTextEdit { background-color: #252526; color: #d4d4d4; border: 1px solid #3e3e42; }"
    );
    mainLayout->addWidget(m_previewText);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_keepButton = new QPushButton("✓ Keep");
    m_keepButton->setStyleSheet(
        "QPushButton { background-color: #0e639c; color: white; border-radius: 3px; padding: 6px 16px; }"
        "QPushButton:hover { background-color: #1177bb; }"
    );
    buttonLayout->addWidget(m_keepButton);

    m_undoButton = new QPushButton("↶ Undo");
    m_undoButton->setStyleSheet(
        "QPushButton { background-color: #6a4c93; color: white; border-radius: 3px; padding: 6px 16px; }"
        "QPushButton:hover { background-color: #7d5ba6; }"
    );
    buttonLayout->addWidget(m_undoButton);

    mainLayout->addLayout(buttonLayout);

    // Connect buttons
    connect(m_keepButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_undoButton, &QPushButton::clicked, this, &QDialog::reject);
}

void AgenticActionDialog::applyDarkTheme()
{
    // Theme already applied in setupUI
}

// ============ AgenticFileOperations Implementation ============

AgenticFileOperations::AgenticFileOperations(QObject* parent)
    : QObject(parent)
{
    qDebug() << "[AgenticFileOperations] Initialized with max history size:" << MAX_HISTORY;
}

void AgenticFileOperations::createFileWithApproval(const QString& filePath, const QString& content)
{
    qDebug() << "[AgenticFileOperations] Creating file:" << filePath;

    AgenticActionDialog dialog(filePath, AgenticActionDialog::CREATE_FILE, content);
    
    if (dialog.exec() == QDialog::Accepted) {
        if (QFile::exists(filePath)) {
            qWarning() << "[AgenticFileOperations] File already exists:" << filePath;
            QMessageBox::warning(nullptr, "Error", QString("File already exists: %1").arg(filePath));
            return;
        }

        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
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
            qDebug() << "[AgenticFileOperations] File created successfully:" << filePath;
        } else {
            QString errorMsg = QString("Failed to create file: %1 - %2").arg(filePath, file.errorString());
            qCritical() << "[AgenticFileOperations]" << errorMsg;
            QMessageBox::critical(nullptr, "Error", errorMsg);
        }
    } else {
        qDebug() << "[AgenticFileOperations] File creation cancelled:" << filePath;
        emit operationUndone(filePath);
    }
}

void AgenticFileOperations::modifyFileWithApproval(const QString& filePath, const QString& oldContent, const QString& newContent)
{
    qDebug() << "[AgenticFileOperations] Modifying file:" << filePath;

    if (!QFile::exists(filePath)) {
        qWarning() << "[AgenticFileOperations] File does not exist:" << filePath;
        QMessageBox::warning(nullptr, "Error", QString("File does not exist: %1").arg(filePath));
        return;
    }

    AgenticActionDialog dialog(filePath, AgenticActionDialog::MODIFY_FILE, newContent);
    
    if (dialog.exec() == QDialog::Accepted) {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
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
            qDebug() << "[AgenticFileOperations] File modified successfully:" << filePath;
        } else {
            QString errorMsg = QString("Failed to modify file: %1 - %2").arg(filePath, file.errorString());
            qCritical() << "[AgenticFileOperations]" << errorMsg;
            QMessageBox::critical(nullptr, "Error", errorMsg);
        }
    } else {
        qDebug() << "[AgenticFileOperations] File modification cancelled:" << filePath;
        emit operationUndone(filePath);
    }
}

void AgenticFileOperations::deleteFileWithApproval(const QString& filePath)
{
    qDebug() << "[AgenticFileOperations] Deleting file:" << filePath;

    if (!QFile::exists(filePath)) {
        qWarning() << "[AgenticFileOperations] File does not exist:" << filePath;
        QMessageBox::warning(nullptr, "Error", QString("File does not exist: %1").arg(filePath));
        return;
    }

    // Read content for undo
    QString content;
    QFile readFile(filePath);
    if (readFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&readFile);
        content = stream.readAll();
        readFile.close();
        qDebug() << "[AgenticFileOperations] Backup content size:" << content.size() << "bytes";
    } else {
        qWarning() << "[AgenticFileOperations] Could not read file for backup:" << filePath << "Error:" << readFile.errorString();
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
            QString errorMsg = QString("Failed to delete file: %1").arg(filePath);
            qCritical() << "[AgenticFileOperations]" << errorMsg;
            QMessageBox::warning(nullptr, "Error", errorMsg);
        }
    } else {
        qDebug() << "[AgenticFileOperations] File deletion cancelled:" << filePath;
        emit operationUndone(filePath);
    }
}

void AgenticFileOperations::undoLastAction()
{
    if (m_actionHistory.isEmpty()) {
        qWarning() << "[AgenticFileOperations] No actions to undo";
        QMessageBox::information(nullptr, "Info", "No actions to undo");
        return;
    }

    FileAction action = m_actionHistory.takeLast();
    qDebug() << "[AgenticFileOperations] Undoing action:" << action.filePath << "Type:" << action.actionType;

    switch (action.actionType) {
        case AgenticActionDialog::CREATE_FILE:
            if (QFile::remove(action.filePath)) {
                qDebug() << "[AgenticFileOperations] Undone file creation:" << action.filePath;
            } else {
                QString errorMsg = QString("Failed to undo file creation: %1").arg(action.filePath);
                qCritical() << "[AgenticFileOperations]" << errorMsg;
            }
            break;

        case AgenticActionDialog::MODIFY_FILE: {
            QFile file(action.filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                stream << action.oldContent;
                file.close();
                qDebug() << "[AgenticFileOperations] Undone file modification:" << action.filePath;
            } else {
                QString errorMsg = QString("Failed to undo file modification: %1 - %2").arg(action.filePath, file.errorString());
                qCritical() << "[AgenticFileOperations]" << errorMsg;
            }
            break;
        }

        case AgenticActionDialog::DELETE_FILE: {
            QFile file(action.filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                stream << action.content;
                file.close();
                qDebug() << "[AgenticFileOperations] Undone file deletion:" << action.filePath;
            } else {
                QString errorMsg = QString("Failed to undo file deletion: %1 - %2").arg(action.filePath, file.errorString());
                qCritical() << "[AgenticFileOperations]" << errorMsg;
            }
            break;
        }
    }

    emit operationUndone(action.filePath);
}
