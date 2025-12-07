// Chat Interface - Chat UI component
#include "chat_interface.h"
#include "agentic_engine.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QtConcurrent>

ChatInterface::ChatInterface(QWidget* parent) : QWidget(parent), maxMode_(false) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Header with title
    QLabel* title = new QLabel("Agent Chat", this);
    title->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(title);
    
    // Model selector row
    QHBoxLayout* modelLayout = new QHBoxLayout();
    
    QLabel* modelLabel = new QLabel("Model 1:", this);
    modelLayout->addWidget(modelLabel);
    
    modelSelector_ = new QComboBox(this);
    modelSelector_->setMinimumWidth(200);
    modelSelector_->addItem("No Model Selected");
    //try {
    //    loadAvailableModels(); // Re-enabled scanner
    //} catch (const std::exception &e) {
    //    qWarning() << "[ChatInterface] loadAvailableModels threw exception:" << e.what();
    //} catch (...) {
    //    qWarning() << "[ChatInterface] loadAvailableModels threw unknown exception";
    //}
    connect(modelSelector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChatInterface::onModelChanged);
    modelLayout->addWidget(modelSelector_);
    
    // Second model selector for dual GGUF loading
    QLabel* model2Label = new QLabel("Model 2:", this);
    modelLayout->addWidget(model2Label);
    
    modelSelector2_ = new QComboBox(this);
    modelSelector2_->setMinimumWidth(200);
    modelSelector2_->addItem("No Model Selected");
    //try {
    //    loadAvailableModelsForSecond(); // Re-enabled scanner
    //} catch (const std::exception &e) {
    //    qWarning() << "[ChatInterface] loadAvailableModelsForSecond threw exception:" << e.what();
    //} catch (...) {
    //    qWarning() << "[ChatInterface] loadAvailableModelsForSecond threw unknown exception";
    //}
    connect(modelSelector2_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChatInterface::onModel2Changed);
    modelLayout->addWidget(modelSelector2_);
    
    modelLayout->addStretch();
    
    // Max Mode toggle
    maxModeToggle_ = new QCheckBox("Max Mode", this);
    maxModeToggle_->setToolTip("Enable maximum context and response length");
    connect(maxModeToggle_, &QCheckBox::toggled, this, &ChatInterface::onMaxModeToggled);
    modelLayout->addWidget(maxModeToggle_);
    
    // Refresh models button
    QPushButton* refreshBtn = new QPushButton("🔄", this);
    refreshBtn->setMaximumWidth(30);
    refreshBtn->setToolTip("Refresh model list");
    connect(refreshBtn, &QPushButton::clicked, this, &ChatInterface::refreshModels);
    modelLayout->addWidget(refreshBtn);
    
    layout->addLayout(modelLayout);
    
    // Message history
    message_history_ = new QTextEdit(this);
    message_history_->setReadOnly(true);
    message_history_->setStyleSheet(
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #3c3c3c; }");
    layout->addWidget(message_history_);
    
    // Input area
    QHBoxLayout* inputLayout = new QHBoxLayout();
    message_input_ = new QLineEdit(this);
    message_input_->setPlaceholderText("Type your message here...");
    message_input_->setStyleSheet(
        "QLineEdit { background-color: #252526; color: #d4d4d4; border: 1px solid #3c3c3c; padding: 5px; }");
    connect(message_input_, &QLineEdit::returnPressed, this, &ChatInterface::sendMessage);
    
    QPushButton* sendButton = new QPushButton("Send", this);
    sendButton->setStyleSheet(
        "QPushButton { background-color: #0e639c; color: white; padding: 5px 15px; border: none; }"
        "QPushButton:hover { background-color: #1177bb; }");
    connect(sendButton, &QPushButton::clicked, this, &ChatInterface::sendMessage);
    
    inputLayout->addWidget(message_input_);
    inputLayout->addWidget(sendButton);
    layout->addLayout(inputLayout);
    
    // Status label
    statusLabel_ = new QLabel("Ready", this);
    statusLabel_->setStyleSheet("color: #888888; font-size: 11px;");
    layout->addWidget(statusLabel_);
    
    // Connect messageReceived signal to add response message
    connect(this, &ChatInterface::messageReceived,
            this, [this](const QString& reply){ addMessage("Bot", reply); });
}

void ChatInterface::loadAvailableModels() {
    // === RECURSIVE GGUF SCANNER ===
    // Scans Ollama's nested directory structure (e.g., .ollama/models/blobs/sha256-xxx)
    QStringList searchPaths = {
        QDir::homePath() + "/.ollama/models",
        "D:/OllamaModels",
        QDir::homePath() + "/models",
        "C:/models",
        "./models"
    };
    
    int modelsFound = 0;
    for (const QString& rootPath : searchPaths) {
        QDir dir(rootPath);
        if (!dir.exists()) {
            qDebug() << "[ChatInterface] path does not exist:" << rootPath;
            continue;
        }
        
        qDebug() << "[ChatInterface] scanning" << rootPath;
        
        try {
            // Recursive iterator finds .gguf files in all subdirectories
            // NOTE: Disabled recursive Subdirectories flag to avoid potential access violations.
            // If deeper scanning is required, a safe recursive implementation can be added later.
            QDirIterator it(rootPath, QStringList() << "*.gguf", QDir::Files, QDirIterator::NoIteratorFlags);
            while (it.hasNext()) {
                QString filePath = it.next();
                QFileInfo fileInfo(filePath);
                
                qDebug() << "  found" << filePath;
                
                // Display relative path to help identify the model
                QString relativePath = dir.relativeFilePath(filePath);
                QString displayName = fileInfo.fileName();
                
                // If file is deep in subdirectories, show part of path
                if (relativePath.contains('/')) {
                    displayName = relativePath;
                }
                
                modelSelector_->addItem(displayName, filePath);
                modelsFound++;
            }
        } catch (const std::exception& e) {
            qWarning() << "[ChatInterface] exception scanning" << rootPath << ":" << e.what();
        } catch (...) {
            qWarning() << "[ChatInterface] skipping unreadable path" << rootPath;
        }
    }
    
    if (modelsFound == 0) {
        statusLabel_->setText("No GGUF models found. Run 'ollama pull <model>' or add files to D:/OllamaModels");
        qDebug() << "[ChatInterface] No models found in any scanned path";
    } else {
        statusLabel_->setText(QString("Found %1 GGUF model(s)").arg(modelsFound));
        qDebug() << "[ChatInterface] Total models found:" << modelsFound;
    }
}

void ChatInterface::refreshModels() {
    QString currentModel = modelSelector_->currentData().toString();
    QString currentModel2 = modelSelector2_->currentData().toString();
    
    modelSelector_->clear();
    modelSelector_->addItem("No Model Selected");
    loadAvailableModels();
    
    modelSelector2_->clear();
    modelSelector2_->addItem("No Model Selected");
    loadAvailableModelsForSecond();
    
    // Try to restore previous selections
    int idx = modelSelector_->findData(currentModel);
    if (idx >= 0) {
        modelSelector_->setCurrentIndex(idx);
    }
    
    int idx2 = modelSelector2_->findData(currentModel2);
    if (idx2 >= 0) {
        modelSelector2_->setCurrentIndex(idx2);
    }
    
    statusLabel_->setText("Model list refreshed");
}

void ChatInterface::onModelChanged(int index) {
    if (index > 0) {
        QString modelPath = modelSelector_->currentData().toString();
        QString modelName = modelSelector_->currentText();
        statusLabel_->setText("Selected: " + modelName);
        emit modelSelected(modelPath);
    } else {
        statusLabel_->setText("No model selected");
    }
}

void ChatInterface::onMaxModeToggled(bool enabled) {
    maxMode_ = enabled;
    if (enabled) {
        statusLabel_->setText("Max Mode enabled - Extended context and responses");
    } else {
        statusLabel_->setText("Standard mode");
    }
    emit maxModeChanged(enabled);
}

void ChatInterface::loadAvailableModelsForSecond() {
    // === RECURSIVE GGUF SCANNER (for Model 2) ===
    // Scans Ollama's nested directory structure
    QStringList searchPaths = {
        QDir::homePath() + "/.ollama/models",
        "D:/OllamaModels",
        QDir::homePath() + "/models",
        "C:/models",
        "./models"
    };
    
    for (const QString& rootPath : searchPaths) {
        QDir dir(rootPath);
        if (!dir.exists()) {
            continue;
        }
        
        try {
            // Recursive iterator finds .gguf files in all subdirectories
            QDirIterator it(rootPath, QStringList() << "*.gguf", QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QString filePath = it.next();
                QFileInfo fileInfo(filePath);
                
                // Display relative path to help identify the model
                QString relativePath = dir.relativeFilePath(filePath);
                QString displayName = fileInfo.fileName();
                
                // If file is deep in subdirectories, show part of path
                if (relativePath.contains('/')) {
                    displayName = relativePath;
                }
                
                modelSelector2_->addItem(displayName, filePath);
            }
        } catch (const std::exception& e) {
            qWarning() << "[ChatInterface] exception scanning Model 2 path" << rootPath << ":" << e.what();
        } catch (...) {
            qWarning() << "[ChatInterface] skipping unreadable Model 2 path" << rootPath;
        }
    }
}

void ChatInterface::onModel2Changed(int index) {
    if (index > 0) {
        QString modelPath = modelSelector2_->currentData().toString();
        QString modelName = modelSelector2_->currentText();
        statusLabel_->setText("Model 2 selected: " + modelName);
        emit model2Selected(modelPath);
    } else {
        statusLabel_->setText("No secondary model selected");
    }
}

QString ChatInterface::selectedModel() const {
    return modelSelector_->currentData().toString();
}

bool ChatInterface::isMaxMode() const {
    return maxMode_;
}

void ChatInterface::addMessage(const QString& sender, const QString& message) {
    QString color = (sender == "User") ? "#569cd6" : "#4ec9b0";
    message_history_->append("<span style='color:" + color + ";font-weight:bold;'>" + sender + ":</span> " + message);
}

void ChatInterface::displayResponse(const QString& response) {
    addMessage("Agent", response);
    statusLabel_->setText("Ready");
    m_busy = false;  // Unblock input after response received
}

void ChatInterface::focusInput() {
    message_input_->setFocus();
}

void ChatInterface::sendMessage() {
    if (m_busy) return;  // Ignore while generating
    
    QString message = message_input_->text().trimmed();
    if (!message.isEmpty()) {
        m_busy = true;
        m_lastPrompt = message;
        addMessage("User", message);
        statusLabel_->setText("Processing...");
        
        // Check if this is an agent command
        if (isAgentCommand(message)) {
            executeAgentCommand(message);
            m_busy = false;
        } else {
            // Emit signal to process message (will be handled asynchronously)
            emit messageSent(message);
        }
        
        message_input_->clear();
    }
}

bool ChatInterface::isAgentCommand(const QString& message) const {
    // Commands start with @ or use known keywords
    return message.startsWith("@") || 
           message.startsWith("grep ") ||
           message.startsWith("read ") ||
           message.startsWith("search ") ||
           message.startsWith("ref ");
}

void ChatInterface::executeAgentCommand(const QString& command, const QString& args) {
    Q_UNUSED(args);  // Currently using command parsing instead
    
    if (!m_agenticEngine) {
        addMessage("System", "Agentic Engine not initialized");
        statusLabel_->setText("Agent error: Engine not ready");
        return;
    }
    
    QString response;
    
    // Parse and execute agent commands
    if (command.startsWith("@grep ")) {
        QString pattern = command.mid(6).trimmed();
        response = m_agenticEngine->grepFiles(pattern, ".");
        addMessage("Agent", response);
    } 
    else if (command.startsWith("@read ")) {
        QString filepath = command.mid(6).trimmed();
        response = m_agenticEngine->readFile(filepath);
        addMessage("Agent", response);
    } 
    else if (command.startsWith("@search ")) {
        QString query = command.mid(8).trimmed();
        response = m_agenticEngine->searchFiles(query, ".");
        addMessage("Agent", response);
    } 
    else if (command.startsWith("@ref ")) {
        QString symbol = command.mid(5).trimmed();
        response = m_agenticEngine->referenceSymbol(symbol);
        addMessage("Agent", response);
    }
    else if (command.startsWith("grep ")) {
        // Support without @ prefix
        QString pattern = command.mid(5).trimmed();
        response = m_agenticEngine->grepFiles(pattern, ".");
        addMessage("Agent", response);
    }
    else if (command.startsWith("read ")) {
        QString filepath = command.mid(5).trimmed();
        response = m_agenticEngine->readFile(filepath);
        addMessage("Agent", response);
    }
    else if (command.startsWith("search ")) {
        QString query = command.mid(7).trimmed();
        response = m_agenticEngine->searchFiles(query, ".");
        addMessage("Agent", response);
    }
    else if (command.startsWith("ref ")) {
        QString symbol = command.mid(4).trimmed();
        response = m_agenticEngine->referenceSymbol(symbol);
        addMessage("Agent", response);
    }
    else {
        response = "Unknown agent command. Available commands:\n"
                   "  @grep <pattern> - Search for text pattern in files\n"
                   "  @read <filepath> - Read file contents\n"
                   "  @search <query> - Search for files matching query\n"
                   "  @ref <symbol> - Find symbol references and definitions";
        addMessage("System", response);
    }
    
    statusLabel_->setText("Agent command executed");
}

