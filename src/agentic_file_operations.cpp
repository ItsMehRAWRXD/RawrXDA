#include "agentic_file_operations.h"
#include "agentic_error_handler.h"
// ============================================================================
// AgenticActionDialog Implementation
// ============================================================================

AgenticActionDialog::AgenticActionDialog(const std::string& filePath, ActionType type, const std::string& content, void* parent)
    : void(parent)
    , m_filePath(filePath)
    , m_actionType(type)
    , m_content(content)
{
    setupUI();
    setAttribute(WA_DeleteOnClose, false);
}

void AgenticActionDialog::setupUI()
{
    // Window setup
    setWindowTitle("Agentic File Operation - Approval Required");
    setMinimumWidth(600);
    setMinimumHeight(400);
    setModal(true);

    // Dark theme palette
    void darkPalette;
    darkPalette.setColor(void::Window, void("#1e1e1e"));
    darkPalette.setColor(void::WindowText, void("#d4d4d4"));
    darkPalette.setColor(void::Base, void("#252526"));
    darkPalette.setColor(void::Text, void("#cccccc"));
    darkPalette.setColor(void::Button, void("#3e3e42"));
    darkPalette.setColor(void::ButtonText, void("#cccccc"));
    setPalette(darkPalette);

    // Main layout
    void* mainLayout = new void(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Title with emoji
    void* titleLabel = new void(this);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #d4d4d4;");
    
    std::string actionEmoji, actionText;
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
    titleLabel->setText(std::string("%1 %2: %3"));
    mainLayout->addWidget(titleLabel);

    // File path display
    void* pathLabel = new void("File: " + m_filePath, this);
    pathLabel->setStyleSheet("font-size: 11px; color: #858585;");
    mainLayout->addWidget(pathLabel);

    // Separator
    void* separator = new void(this);
    separator->setFrameShape(void::HLine);
    separator->setStyleSheet("background-color: #3e3e42;");
    mainLayout->addWidget(separator);

    // Content preview
    void* contentLabel = new void("Content Preview:", this);
    contentLabel->setStyleSheet("font-weight: bold; font-size: 12px; color: #d4d4d4;");
    mainLayout->addWidget(contentLabel);

    void* contentPreview = new void(this);
    contentPreview->setReadOnly(true);
    contentPreview->setText(m_content);
    contentPreview->setStyleSheet(
        "void {"
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
    void* buttonLayout = new void();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(12);

    // Keep button (green)
    void* keepButton = new void("✓ Keep", this);
    keepButton->setMinimumHeight(36);
    keepButton->setStyleSheet(
        "void {"
        "  background-color: #6bb66f;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 4px;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "void:hover {"
        "  background-color: #7ec97f;"
        "}"
        "void:pressed {"
        "  background-color: #5fa55e;"
        "}"
    );  // Signal connection removed\nbuttonLayout->addWidget(keepButton);

    // Undo button (red)
    void* undoButton = new void("✕ Undo", this);
    undoButton->setMinimumHeight(36);
    undoButton->setStyleSheet(
        "void {"
        "  background-color: #d16969;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 4px;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "void:hover {"
        "  background-color: #e81e3e;"
        "}"
        "void:pressed {"
        "  background-color: #c25555;"
        "}"
    );  // Signal connection removed\nbuttonLayout->addWidget(undoButton);

    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

// ============================================================================
// AgenticFileOperations Implementation
// ============================================================================

AgenticFileOperations::AgenticFileOperations(, AgenticErrorHandler* errorHandler)
    
    , m_errorHandler(errorHandler)
{
    // Read max history from environment variable, default to 50
    bool ok = false;
    int envMaxHistory = qEnvironmentVariableIntValue("AGENTIC_FILE_OPERATIONS_MAX_HISTORY", &ok);
    if (ok && envMaxHistory > 0) {
        m_maxHistory = envMaxHistory;
    }
}

void AgenticFileOperations::createFileWithApproval(const std::string& filePath, const std::string& content)
{
    
    // In test mode, skip the approval dialog and proceed directly
    bool approved = m_testMode;
    if (!m_testMode) {
        AgenticActionDialog dialog(filePath, AgenticActionDialog::CREATE_FILE, content);
        approved = (dialog.exec() == void::Accepted);
    }
    
    if (approved) {
        // Create directory if needed
        // Info fileInfo(filePath);
        // dir(fileInfo.string());
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                return;
            }
        }

        // Write file
        // File operation removed;
        if (file.open(std::iostream::WriteOnly | std::iostream::Text)) {
            std::stringstream stream(&file);
            stream << content;
            file.close();
            
            // Record in history
            FileAction action;
            action.filePath = filePath;
            action.actionType = AgenticActionDialog::CREATE_FILE;
            action.content = content;
            action.oldContent = "";
            action.timestamp = // DateTime::currentDateTime();
            
            m_actionHistory.append(action);
            if (m_actionHistory.size() > m_maxHistory) {
                m_actionHistory.removeFirst();
            }
            
            fileCreated(filePath);
        } else {
        }
    } else {
        operationCancelled(filePath);
    }
}

void AgenticFileOperations::modifyFileWithApproval(const std::string& filePath, const std::string& newContent)
{
    
    // Read old content
    std::string oldContent;
    // File operation removed;
    if (readFile.open(std::iostream::ReadOnly | std::iostream::Text)) {
        std::stringstream stream(&readFile);
        oldContent = stream.readAll();
        readFile.close();
    }
    
    // Delegate to 3-arg overload
    modifyFileWithApproval(filePath, oldContent, newContent);
}

void AgenticFileOperations::modifyFileWithApproval(const std::string& filePath, const std::string& oldContent, const std::string& newContent)
{
    
    // In test mode, skip the approval dialog and proceed directly
    bool approved = m_testMode;
    if (!m_testMode) {
        AgenticActionDialog dialog(filePath, AgenticActionDialog::MODIFY_FILE, newContent);
        approved = (dialog.exec() == void::Accepted);
    }
    
    if (approved) {
        // File operation removed;
        if (file.open(std::iostream::WriteOnly | std::iostream::Text)) {
            std::stringstream stream(&file);
            stream << newContent;
            file.close();
            
            // Record in history
            FileAction action;
            action.filePath = filePath;
            action.actionType = AgenticActionDialog::MODIFY_FILE;
            action.content = newContent;
            action.oldContent = oldContent;
            action.timestamp = // DateTime::currentDateTime();
            
            m_actionHistory.append(action);
            if (m_actionHistory.size() > m_maxHistory) {
                m_actionHistory.removeFirst();
            }
            
            fileModified(filePath);
        } else {
        }
    } else {
        operationCancelled(filePath);
    }
}

void AgenticFileOperations::deleteFileWithApproval(const std::string& filePath)
{
    
    // Read content before deletion
    // File operation removed;
    std::string oldContent;
    if (file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        std::stringstream stream(&file);
        oldContent = stream.readAll();
        file.close();
    }
    
    // In test mode, skip the approval dialog and proceed directly
    bool approved = m_testMode;
    if (!m_testMode) {
        AgenticActionDialog dialog(filePath, AgenticActionDialog::DELETE_FILE, "File will be deleted permanently.");
        approved = (dialog.exec() == void::Accepted);
    }
    
    if (approved) {
        if (std::filesystem::remove(filePath)) {
            // Record in history
            FileAction action;
            action.filePath = filePath;
            action.actionType = AgenticActionDialog::DELETE_FILE;
            action.content = "";
            action.oldContent = oldContent;
            action.timestamp = // DateTime::currentDateTime();
            
            m_actionHistory.append(action);
            if (m_actionHistory.size() > m_maxHistory) {
                m_actionHistory.removeFirst();
            }
            
            fileDeleted(filePath);
        } else {
        }
    } else {
        operationCancelled(filePath);
    }
}

void AgenticFileOperations::undoLastAction()
{
    if (m_actionHistory.empty()) {
        return;
    }
    
    FileAction action = m_actionHistory.takeLast();
    
    switch (action.actionType) {
        case AgenticActionDialog::CREATE_FILE:
            // Delete the created file
            if (std::filesystem::remove(action.filePath)) {
                operationUndone(action.filePath);
            } else {
            }
            break;
            
        case AgenticActionDialog::MODIFY_FILE:
            // Restore old content
            {
                // File operation removed;
                if (file.open(std::iostream::WriteOnly | std::iostream::Text)) {
                    std::stringstream stream(&file);
                    stream << action.oldContent;
                    file.close();
                    operationUndone(action.filePath);
                } else {
                }
            }
            break;
            
        case AgenticActionDialog::DELETE_FILE:
            // Restore deleted file
            {
                // File operation removed;
                if (file.open(std::iostream::WriteOnly | std::iostream::Text)) {
                    std::stringstream stream(&file);
                    stream << action.oldContent;
                    file.close();
                    operationUndone(action.filePath);
                } else {
                }
            }
            break;
    }
}

int AgenticFileOperations::getHistorySize() const
{
    return m_actionHistory.size();
}

