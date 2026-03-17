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
    qDebug() << "[AgenticFileOperations] File size:" << content.size() << "bytes";
    qDebug() << "[AgenticFileOperations] File path length:" << filePath.length();

    AgenticActionDialog dialog(filePath, AgenticActionDialog::CREATE_FILE, content);
    
    // Set error handler for the dialog
    if (m_errorHandler) {
        dialog.setErrorHandler(m_errorHandler);
    }

    if (dialog.exec() == QDialog::Accepted) {
        // Log timing for performance monitoring
        qint64 startTime = QDateTime::currentMSecsSinceEpoch();
        
        // Increment create counter
        m_createCounter->increment();
        
        // Create directories if needed with resource guard
        QFileInfo fileInfo(filePath);
        QDir dir(fileInfo.dir());
        std::unique_ptr<DirectoryResourceGuard> dirGuard;
        
        if (!dir.exists()) {
            qDebug() << "[AgenticFileOperations] Creating directory path for file:" << dir.path();
            if (!dir.mkpath(".")) {
                QString errorMsg = QString("Failed to create directory path: %1").arg(dir.path());
                qCritical() << "[AgenticFileOperations]" << errorMsg;
                m_errorCounter->increment();
                
                // Record error with error handler
                if (m_errorHandler) {
                    m_errorHandler->recordError(
                        AgenticErrorHandler::ErrorType::ResourceError,
                        errorMsg,
                        "AgenticFileOperations",
                        "",
                        QJsonObject{{"filePath", filePath}, {"operation", "create_directory"}});
                }
                
                QMessageBox::warning(nullptr, "Error", errorMsg);
                return;
            }
        }

        // Write file with resource guard
        std::unique_ptr<FileResourceGuard> fileGuard;
        QFile file(filePath);
        fileGuard = std::make_unique<FileResourceGuard>(filePath, &file, false);
        
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QString errorMsg = QString("Failed to create file: %1 - %2").arg(filePath, file.errorString());
            qCritical() << "[AgenticFileOperations]" << errorMsg;
            m_errorCounter->increment();
            
            // Record error with error handler
            if (m_errorHandler) {
                m_errorHandler->recordError(
                    AgenticErrorHandler::ErrorType::ResourceError,
                    errorMsg,
                    "AgenticFileOperations",
                    "",
                    QJsonObject{{"filePath", filePath}, {"operation", "create_file"}});
            }
            
            QMessageBox::warning(nullptr, "Error", errorMsg);
            return;
        }

        QTextStream stream(&file);
        stream << content;
        // File will be automatically closed by resource guard
        
        qint64 endTime = QDateTime::currentMSecsSinceEpoch();
        qint64 duration = endTime - startTime;

        // Record in history
        FileAction action;
        action.filePath = filePath;
        action.actionType = AgenticActionDialog::CREATE_FILE;
        action.content = content;
        action.timestamp = QDateTime::currentDateTime();
        m_actionHistory.append(action);

        if (m_actionHistory.size() > m_maxHistorySize) {
            m_actionHistory.removeFirst();
        }

        // Update metrics
        m_keepCounter->increment();
        m_undoStackGauge->set(m_actionHistory.size());
        m_operationDurationHistogram->record(duration / 1000.0); // Convert to seconds

        emit fileCreated(filePath);
        qDebug() << "[AgenticFileOperations] File created and kept:" << filePath << "Duration:" << duration << "ms";
    } else {
        qDebug() << "[AgenticFileOperations] File creation undone:" << filePath;
        m_undoCounter->increment();
        emit operationUndone(filePath);
    }
}

void AgenticFileOperations::modifyFileWithApproval(const QString& filePath, const QString& oldContent, const QString& newContent)
{
    qDebug() << "[AgenticFileOperations] Modifying file with approval:" << filePath;
    qDebug() << "[AgenticFileOperations] Old content size:" << oldContent.size() << "bytes";
    qDebug() << "[AgenticFileOperations] New content size:" << newContent.size() << "bytes";
    qDebug() << "[AgenticFileOperations] File path length:" << filePath.length();

    AgenticActionDialog dialog(filePath, AgenticActionDialog::MODIFY_FILE, newContent);
    
    // Set error handler for the dialog
    if (m_errorHandler) {
        dialog.setErrorHandler(m_errorHandler);
    }

    if (dialog.exec() == QDialog::Accepted) {
        // Log timing for performance monitoring
        qint64 startTime = QDateTime::currentMSecsSinceEpoch();
        
        // Increment modify counter
        m_modifyCounter->increment();
        
        // Write modified content with resource guard
        std::unique_ptr<FileResourceGuard> fileGuard;
        QFile file(filePath);
        fileGuard = std::make_unique<FileResourceGuard>(filePath, &file, false);
        
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QString errorMsg = QString("Failed to modify file: %1 - %2").arg(filePath, file.errorString());
            qCritical() << "[AgenticFileOperations]" << errorMsg;
            m_errorCounter->increment();
            
            // Record error with error handler
            if (m_errorHandler) {
                m_errorHandler->recordError(
                    AgenticErrorHandler::ErrorType::ResourceError,
                    errorMsg,
                    "AgenticFileOperations",
                    "",
                    QJsonObject{{"filePath", filePath}, {"operation", "modify_file"}});
            }
            
            QMessageBox::warning(nullptr, "Error", errorMsg);
            return;
        }

        QTextStream stream(&file);
        stream << newContent;
        // File will be automatically closed by resource guard
        
        qint64 endTime = QDateTime::currentMSecsSinceEpoch();
        qint64 duration = endTime - startTime;

        // Record in history
        FileAction action;
        action.filePath = filePath;
        action.actionType = AgenticActionDialog::MODIFY_FILE;
        action.content = newContent;
        action.oldContent = oldContent;
        action.timestamp = QDateTime::currentDateTime();
        m_actionHistory.append(action);

        if (m_actionHistory.size() > m_maxHistorySize) {
            m_actionHistory.removeFirst();
        }

        // Update metrics
        m_keepCounter->increment();
        m_undoStackGauge->set(m_actionHistory.size());
        m_operationDurationHistogram->record(duration / 1000.0); // Convert to seconds

        emit fileModified(filePath);
        qDebug() << "[AgenticFileOperations] File modified and kept:" << filePath << "Duration:" << duration << "ms";
    } else {
        qDebug() << "[AgenticFileOperations] File modification undone:" << filePath;
        m_undoCounter->increment();
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
        qDebug() << "[AgenticFileOperations] Backup content size:" << content.size() << "bytes";
    } else {
        qWarning() << "[AgenticFileOperations] Could not read file for backup:" << filePath << "Error:" << file.errorString();
    }

    AgenticActionDialog dialog(filePath, AgenticActionDialog::DELETE_FILE, content);
    
    // Set error handler for the dialog
    if (m_errorHandler) {
        dialog.setErrorHandler(m_errorHandler);
    }

    if (dialog.exec() == QDialog::Accepted) {
        // Log timing for performance monitoring
        qint64 startTime = QDateTime::currentMSecsSinceEpoch();
        
        // Increment delete counter
        m_deleteCounter->increment();
        
        if (QFile::remove(filePath)) {
            qint64 endTime = QDateTime::currentMSecsSinceEpoch();
            qint64 duration = endTime - startTime;
            
            // Record in history
            FileAction action;
            action.filePath = filePath;
            action.actionType = AgenticActionDialog::DELETE_FILE;
            action.content = content;  // Save for undo
            action.timestamp = QDateTime::currentDateTime();
            m_actionHistory.append(action);

            if (m_actionHistory.size() > m_maxHistorySize) {
                m_actionHistory.removeFirst();
            }

            // Update metrics
            m_keepCounter->increment();
            m_undoStackGauge->set(m_actionHistory.size());
            m_operationDurationHistogram->record(duration / 1000.0); // Convert to seconds

            emit fileDeleted(filePath);
            qDebug() << "[AgenticFileOperations] File deleted:" << filePath << "Duration:" << duration << "ms";
        } else {
            QString errorMsg = QString("Failed to delete file: %1").arg(filePath);
            qCritical() << "[AgenticFileOperations]" << errorMsg;
            m_errorCounter->increment();
            
            // Record error with error handler
            if (m_errorHandler) {
                m_errorHandler->recordError(
                    AgenticErrorHandler::ErrorType::ResourceError,
                    errorMsg,
                    "AgenticFileOperations",
                    "",
                    QJsonObject{{"filePath", filePath}, {"operation", "delete_file"}});
            }
            
            QMessageBox::warning(nullptr, "Error", errorMsg);
        }
    } else {
        qDebug() << "[AgenticFileOperations] File deletion undone:" << filePath;
        m_undoCounter->increment();
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
    qDebug() << "[AgenticFileOperations] Undoing action:" << action.filePath << "Type:" << action.actionType;
    
    // Log timing for performance monitoring
    qint64 startTime = QDateTime::currentMSecsSinceEpoch();

    switch (action.actionType) {
        case AgenticActionDialog::CREATE_FILE:
            if (QFile::remove(action.filePath)) {
                qint64 endTime = QDateTime::currentMSecsSinceEpoch();
                qint64 duration = endTime - startTime;
                qDebug() << "[AgenticFileOperations] Undone file creation:" << action.filePath << "Duration:" << duration << "ms";
                m_operationDurationHistogram->record(duration / 1000.0); // Convert to seconds
            } else {
                QString errorMsg = QString("Failed to undo file creation: %1").arg(action.filePath);
                qCritical() << "[AgenticFileOperations]" << errorMsg;
                m_errorCounter->increment();
                
                // Record error with error handler
                if (m_errorHandler) {
                    m_errorHandler->recordError(
                        AgenticErrorHandler::ErrorType::ResourceError,
                        errorMsg,
                        "AgenticFileOperations",
                        "",
                        QJsonObject{{"filePath", action.filePath}, {"operation", "undo_create"}});
                }
            }
            break;

        case AgenticActionDialog::MODIFY_FILE: {
            std::unique_ptr<FileResourceGuard> fileGuard;
            QFile file(action.filePath);
            fileGuard = std::make_unique<FileResourceGuard>(action.filePath, &file, false);
            
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                stream << action.oldContent;
                // File will be automatically closed by resource guard
                qint64 endTime = QDateTime::currentMSecsSinceEpoch();
                qint64 duration = endTime - startTime;
                qDebug() << "[AgenticFileOperations] Undone file modification:" << action.filePath << "Duration:" << duration << "ms";
                m_operationDurationHistogram->record(duration / 1000.0); // Convert to seconds
            } else {
                QString errorMsg = QString("Failed to undo file modification: %1 - %2").arg(action.filePath, file.errorString());
                qCritical() << "[AgenticFileOperations]" << errorMsg;
                m_errorCounter->increment();
                
                // Record error with error handler
                if (m_errorHandler) {
                    m_errorHandler->recordError(
                        AgenticErrorHandler::ErrorType::ResourceError,
                        errorMsg,
                        "AgenticFileOperations",
                        "",
                        QJsonObject{{"filePath", action.filePath}, {"operation", "undo_modify"}});
                }
            }
            break;
        }

        case AgenticActionDialog::DELETE_FILE: {
            std::unique_ptr<FileResourceGuard> fileGuard;
            QFile file(action.filePath);
            fileGuard = std::make_unique<FileResourceGuard>(action.filePath, &file, false);
            
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                stream << action.content;
                // File will be automatically closed by resource guard
                qint64 endTime = QDateTime::currentMSecsSinceEpoch();
                qint64 duration = endTime - startTime;
                qDebug() << "[AgenticFileOperations] Undone file deletion:" << action.filePath << "Duration:" << duration << "ms";
                m_operationDurationHistogram->record(duration / 1000.0); // Convert to seconds
            } else {
                QString errorMsg = QString("Failed to undo file deletion: %1 - %2").arg(action.filePath, file.errorString());
                qCritical() << "[AgenticFileOperations]" << errorMsg;
                m_errorCounter->increment();
                
                // Record error with error handler
                if (m_errorHandler) {
                    m_errorHandler->recordError(
                        AgenticErrorHandler::ErrorType::ResourceError,
                        errorMsg,
                        "AgenticFileOperations",
                        "",
                        QJsonObject{{"filePath", action.filePath}, {"operation", "undo_delete"}});
                }
            }
            break;
        }
    }

    // Update metrics
    m_undoCounter->increment();
    m_undoStackGauge->set(m_actionHistory.size());

    emit operationUndone(action.filePath);
}
