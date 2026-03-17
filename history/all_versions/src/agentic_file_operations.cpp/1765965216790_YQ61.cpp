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
    , m_metrics(std::make_shared<Metrics>())
{
    // Initialize metrics counters
    m_keepCounter = std::make_unique<Metrics::Counter>(m_metrics.get(), "agentic_file_operations_keep_total", 
        {{"component", "agentic_file_operations"}});
    m_undoCounter = std::make_unique<Metrics::Counter>(m_metrics.get(), "agentic_file_operations_undo_total", 
        {{"component", "agentic_file_operations"}});
    m_createCounter = std::make_unique<Metrics::Counter>(m_metrics.get(), "agentic_file_operations_create_total", 
        {{"component", "agentic_file_operations"}});
    m_modifyCounter = std::make_unique<Metrics::Counter>(m_metrics.get(), "agentic_file_operations_modify_total", 
        {{"component", "agentic_file_operations"}});
    m_deleteCounter = std::make_unique<Metrics::Counter>(m_metrics.get(), "agentic_file_operations_delete_total", 
        {{"component", "agentic_file_operations"}});
    m_errorCounter = std::make_unique<Metrics::Counter>(m_metrics.get(), "agentic_file_operations_error_total", 
        {{"component", "agentic_file_operations"}});
    
    // Initialize gauges
    m_undoStackGauge = std::make_unique<Metrics::Gauge>(m_metrics.get(), "agentic_file_operations_undo_stack_size", 
        {{"component", "agentic_file_operations"}});
    
    // Initialize histograms
    m_operationDurationHistogram = std::make_unique<Metrics::Histogram>(m_metrics.get(), "agentic_file_operations_duration_seconds", 
        {{"component", "agentic_file_operations"}});
    
    qDebug() << "[AgenticFileOperations] Initialized with Keep/Undo support and metrics tracking";
}

void AgenticFileOperations::createFileWithApproval(const QString& filePath, const QString& content)
{
    qDebug() << "[AgenticFileOperations] Creating file with approval:" << filePath;
    qDebug() << "[AgenticFileOperations] File size:" << content.size() << "bytes";
    qDebug() << "[AgenticFileOperations] File path length:" << filePath.length();

    AgenticActionDialog dialog(filePath, AgenticActionDialog::CREATE_FILE, content);

    if (dialog.exec() == QDialog::Accepted) {
        // Log timing for performance monitoring
        qint64 startTime = QDateTime::currentMSecsSinceEpoch();
        
        // Increment create counter
        m_createCounter->increment();
        
        // Create directories if needed
        QFileInfo fileInfo(filePath);
        QDir dir(fileInfo.dir());
        if (!dir.exists()) {
            qDebug() << "[AgenticFileOperations] Creating directory path for file:" << dir.path();
            if (!dir.mkpath(".")) {
                qCritical() << "[AgenticFileOperations] Failed to create directory path:" << dir.path();
                m_errorCounter->increment();
                QMessageBox::warning(nullptr, "Error", "Failed to create directory: " + dir.path());
                return;
            }
        }

        // Write file
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qCritical() << "[AgenticFileOperations] Failed to create file:" << filePath << "Error:" << file.errorString();
            m_errorCounter->increment();
            QMessageBox::warning(nullptr, "Error", "Failed to create file: " + filePath);
            return;
        }

        QTextStream stream(&file);
        stream << content;
        file.close();
        
        qint64 endTime = QDateTime::currentMSecsSinceEpoch();
        qint64 duration = endTime - startTime;

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

    if (dialog.exec() == QDialog::Accepted) {
        // Log timing for performance monitoring
        qint64 startTime = QDateTime::currentMSecsSinceEpoch();
        
        // Increment modify counter
        m_modifyCounter->increment();
        
        // Write modified content
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qCritical() << "[AgenticFileOperations] Failed to modify file:" << filePath << "Error:" << file.errorString();
            m_errorCounter->increment();
            QMessageBox::warning(nullptr, "Error", "Failed to modify file: " + filePath);
            return;
        }

        QTextStream stream(&file);
        stream << newContent;
        file.close();
        
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

        if (m_actionHistory.size() > MAX_HISTORY) {
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

            if (m_actionHistory.size() > MAX_HISTORY) {
                m_actionHistory.removeFirst();
            }

            // Update metrics
            m_keepCounter->increment();
            m_undoStackGauge->set(m_actionHistory.size());
            m_operationDurationHistogram->record(duration / 1000.0); // Convert to seconds

            emit fileDeleted(filePath);
            qDebug() << "[AgenticFileOperations] File deleted:" << filePath << "Duration:" << duration << "ms";
        } else {
            qCritical() << "[AgenticFileOperations] Failed to delete file:" << filePath;
            m_errorCounter->increment();
            QMessageBox::warning(nullptr, "Error", "Failed to delete file: " + filePath);
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
                qCritical() << "[AgenticFileOperations] Failed to undo file creation:" << action.filePath;
                m_errorCounter->increment();
            }
            break;

        case AgenticActionDialog::MODIFY_FILE: {
            QFile file(action.filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                stream << action.oldContent;
                file.close();
                qint64 endTime = QDateTime::currentMSecsSinceEpoch();
                qint64 duration = endTime - startTime;
                qDebug() << "[AgenticFileOperations] Undone file modification:" << action.filePath << "Duration:" << duration << "ms";
                m_operationDurationHistogram->record(duration / 1000.0); // Convert to seconds
            } else {
                qCritical() << "[AgenticFileOperations] Failed to undo file modification:" << action.filePath << "Error:" << file.errorString();
                m_errorCounter->increment();
            }
            break;
        }

        case AgenticActionDialog::DELETE_FILE: {
            QFile file(action.filePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                stream << action.content;
                file.close();
                qint64 endTime = QDateTime::currentMSecsSinceEpoch();
                qint64 duration = endTime - startTime;
                qDebug() << "[AgenticFileOperations] Undone file deletion:" << action.filePath << "Duration:" << duration << "ms";
                m_operationDurationHistogram->record(duration / 1000.0); // Convert to seconds
            } else {
                qCritical() << "[AgenticFileOperations] Failed to undo file deletion:" << action.filePath << "Error:" << file.errorString();
                m_errorCounter->increment();
            }
            break;
        }
    }

    // Update metrics
    m_undoCounter->increment();
    m_undoStackGauge->set(m_actionHistory.size());

    emit operationUndone(action.filePath);
}
