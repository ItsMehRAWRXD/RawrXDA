#include "agentic_file_operations.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QTextEdit>
#include <QTextStream>

// ============================================================================
// AgenticActionDialog Implementation
// ============================================================================

AgenticActionDialog::AgenticActionDialog(const QString& filePath, ActionType type, const QString& content, QWidget* parent)
    : QDialog(parent)
    , m_filePath(filePath)
    , m_actionType(type)
    , m_content(content)
{
    setupUI();
    setAttribute(Qt::WA_DeleteOnClose, false);
}

void AgenticActionDialog::setupUI()
{
    // Window setup
    setWindowTitle("Agentic File Operation - Approval Required");
    setMinimumWidth(600);
    setMinimumHeight(400);
    setModal(true);

    // Dark theme palette
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor("#1e1e1e"));
    darkPalette.setColor(QPalette::WindowText, QColor("#d4d4d4"));
    darkPalette.setColor(QPalette::Base, QColor("#252526"));
    darkPalette.setColor(QPalette::Text, QColor("#cccccc"));
    darkPalette.setColor(QPalette::Button, QColor("#3e3e42"));
    darkPalette.setColor(QPalette::ButtonText, QColor("#cccccc"));
    setPalette(darkPalette);

    // Main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Title with emoji
    QLabel* titleLabel = new QLabel(this);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #d4d4d4;");
    
    QString actionEmoji, actionText;
    switch (m_actionType) {
        case CREATE_FILE:
            actionEmoji = "✨";
            actionText = "Create File";
            break;
        case MODIFY_FILE:
            actionEmoji = "✏️";
            actionText = "Modify File";
            break;
        case DELETE_FILE:
            actionEmoji = "🗑️";
            actionText = "Delete File";
            break;
    }
    titleLabel->setText(QString("%1 %2: %3").arg(actionEmoji, actionText, m_filePath));
    mainLayout->addWidget(titleLabel);

    // File path display
    QLabel* pathLabel = new QLabel("File: " + m_filePath, this);
    pathLabel->setStyleSheet("font-size: 11px; color: #858585;");
    mainLayout->addWidget(pathLabel);

    // Separator
    QFrame* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("background-color: #3e3e42;");
    mainLayout->addWidget(separator);

    // Content preview
    QLabel* contentLabel = new QLabel("Content Preview:", this);
    contentLabel->setStyleSheet("font-weight: bold; font-size: 12px; color: #d4d4d4;");
    mainLayout->addWidget(contentLabel);

    QTextEdit* contentPreview = new QTextEdit(this);
    contentPreview->setReadOnly(true);
    contentPreview->setText(m_content);
    contentPreview->setStyleSheet(
        "QTextEdit {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "  border: 1px solid #3e3e42;"
        "  border-radius: 4px;"
        "  padding: 8px;"
        "  font-family: 'Consolas', monospace;"
        "  font-size: 11px;"
        "}"
    );
    mainLayout->addWidget(contentPreview, 1);

    // Button layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(12);

    // Keep button (green)
    QPushButton* keepButton = new QPushButton("✓ Keep", this);
    keepButton->setMinimumHeight(36);
    keepButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #6bb66f;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 4px;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #7ec97f;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #5fa55e;"
        "}"
    );
    connect(keepButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(keepButton);

    // Undo button (red)
    QPushButton* undoButton = new QPushButton("✕ Undo", this);
    undoButton->setMinimumHeight(36);
    undoButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #d16969;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 4px;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #e81e3e;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #c25555;"
        "}"
    );
    connect(undoButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(undoButton);

    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

// ============================================================================
// AgenticFileOperations Implementation
// ============================================================================

AgenticFileOperations::AgenticFileOperations(QObject* parent)
    : QObject(parent)
{
    qDebug() << "AgenticFileOperations initialized with max history:" << MAX_HISTORY;
}

void AgenticFileOperations::createFileWithApproval(const QString& filePath, const QString& content)
{
    qDebug() << "Creating file with approval:" << filePath;
    
    AgenticActionDialog dialog(filePath, AgenticActionDialog::CREATE_FILE, content);
    
    if (dialog.exec() == QDialog::Accepted) {
        // Create directory if needed
        QFileInfo fileInfo(filePath);
        QDir dir(fileInfo.absolutePath());
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                qWarning() << "Failed to create directory:" << fileInfo.absolutePath();
                return;
            }
        }

        // Write file
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
            action.oldContent = "";
            action.timestamp = QDateTime::currentDateTime();
            
            m_actionHistory.append(action);
            if (m_actionHistory.size() > MAX_HISTORY) {
                m_actionHistory.removeFirst();
            }
            
            qDebug() << "File created successfully:" << filePath;
            emit fileCreated(filePath);
        } else {
            qWarning() << "Failed to open file for writing:" << filePath;
        }
    } else {
        qDebug() << "File creation cancelled by user:" << filePath;
        emit operationCancelled(filePath);
    }
}

void AgenticFileOperations::modifyFileWithApproval(const QString& filePath, const QString& newContent)
{
    qDebug() << "Modifying file with approval:" << filePath;
    
    // Read old content
    QFile file(filePath);
    QString oldContent;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        oldContent = stream.readAll();
        file.close();
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
            
            qDebug() << "File modified successfully:" << filePath;
            emit fileModified(filePath);
        } else {
            qWarning() << "Failed to open file for writing:" << filePath;
        }
    } else {
        qDebug() << "File modification cancelled by user:" << filePath;
        emit operationCancelled(filePath);
    }
}

void AgenticFileOperations::deleteFileWithApproval(const QString& filePath)
{
    qDebug() << "Deleting file with approval:" << filePath;
    
    // Read content before deletion
    QFile file(filePath);
    QString oldContent;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        oldContent = stream.readAll();
        file.close();
    }
    
    AgenticActionDialog dialog(filePath, AgenticActionDialog::DELETE_FILE, "File will be deleted permanently.");
    
    if (dialog.exec() == QDialog::Accepted) {
        if (QFile::remove(filePath)) {
            // Record in history
            FileAction action;
            action.filePath = filePath;
            action.actionType = AgenticActionDialog::DELETE_FILE;
            action.content = "";
            action.oldContent = oldContent;
            action.timestamp = QDateTime::currentDateTime();
            
            m_actionHistory.append(action);
            if (m_actionHistory.size() > MAX_HISTORY) {
                m_actionHistory.removeFirst();
            }
            
            qDebug() << "File deleted successfully:" << filePath;
            emit fileDeleted(filePath);
        } else {
            qWarning() << "Failed to delete file:" << filePath;
        }
    } else {
        qDebug() << "File deletion cancelled by user:" << filePath;
        emit operationCancelled(filePath);
    }
}

void AgenticFileOperations::undoLastAction()
{
    if (m_actionHistory.isEmpty()) {
        qDebug() << "No actions to undo";
        return;
    }
    
    FileAction action = m_actionHistory.takeLast();
    qDebug() << "Undoing action on file:" << action.filePath;
    
    switch (action.actionType) {
        case AgenticActionDialog::CREATE_FILE:
            // Delete the created file
            if (QFile::remove(action.filePath)) {
                qDebug() << "Undo: deleted created file" << action.filePath;
                emit operationUndone(action.filePath);
            } else {
                qWarning() << "Undo failed: could not delete file" << action.filePath;
            }
            break;
            
        case AgenticActionDialog::MODIFY_FILE:
            // Restore old content
            {
                QFile file(action.filePath);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream stream(&file);
                    stream << action.oldContent;
                    file.close();
                    qDebug() << "Undo: restored file content" << action.filePath;
                    emit operationUndone(action.filePath);
                } else {
                    qWarning() << "Undo failed: could not restore file" << action.filePath;
                }
            }
            break;
            
        case AgenticActionDialog::DELETE_FILE:
            // Restore deleted file
            {
                QFile file(action.filePath);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream stream(&file);
                    stream << action.oldContent;
                    file.close();
                    qDebug() << "Undo: restored deleted file" << action.filePath;
                    emit operationUndone(action.filePath);
                } else {
                    qWarning() << "Undo failed: could not restore file" << action.filePath;
                }
            }
            break;
    }
}

int AgenticFileOperations::getHistorySize() const
{
    return m_actionHistory.size();
}
