# RawrXD Agentic IDE - Critical Missing Features Quick Reference

## 🚨 Must Fix Before Build Success

### 1. ChatInterface Missing Methods

**File**: `include/chat_interface.h`  
**Add to class declaration**:
```cpp
public:
    void displayResponse(const QString& response);
    void addMessage(const QString& sender, const QString& message);
    void focusInput();
    void setVisible(bool visible);
```

**File**: `src/chat_interface.cpp`  
**Add implementations**:
```cpp
void ChatInterface::displayResponse(const QString& response) {
    message_history_->append("<b>Agent:</b> " + response);
    message_history_->verticalScrollBar()->setValue(
        message_history_->verticalScrollBar()->maximum());
}

void ChatInterface::addMessage(const QString& sender, const QString& message) {
    message_history_->append("<b>" + sender + ":</b> " + message);
    message_history_->verticalScrollBar()->setValue(
        message_history_->verticalScrollBar()->maximum());
}

void ChatInterface::focusInput() {
    message_input_->setFocus();
}

void ChatInterface::setVisible(bool visible) {
    QWidget::setVisible(visible);
    if (visible) focusInput();
}
```

---

### 2. MultiTabEditor Missing Methods

**File**: `include/multi_tab_editor.h`  
**Add to class declaration**:
```cpp
public:
    QString getCurrentText() const;
```

**File**: `src/multi_tab_editor.cpp`  
**Add implementation**:
```cpp
QString MultiTabEditor::getCurrentText() const {
    QTextEdit* currentEditor = qobject_cast<QTextEdit*>(tab_widget_->currentWidget());
    if (currentEditor) {
        return currentEditor->toPlainText();
    }
    return "";
}
```

**Also complete replace() method**:
```cpp
void MultiTabEditor::replace() {
    QTextEdit* currentEditor = qobject_cast<QTextEdit*>(tab_widget_->currentWidget());
    if (currentEditor) {
        QString searchText = QInputDialog::getText(this, "Find", "Enter text to find:");
        if (!searchText.isEmpty()) {
            QString replaceText = QInputDialog::getText(this, "Replace", "Replace with:");
            QString text = currentEditor->toPlainText();
            text.replace(searchText, replaceText);
            currentEditor->setPlainText(text);
        }
    }
}
```

---

### 3. Dock Widget Toggle Fix

**File**: `include/agentic_ide.h`  
**Add member variables**:
```cpp
private:
    // Dock widget pointers for toggle operations
    QDockWidget *m_fileBrowserDock;
    QDockWidget *m_chatDock;
    QDockWidget *m_terminalDock;
```

**File**: `src/agentic_ide.cpp`  
**In setupUI(), store dock pointers**:
```cpp
void AgenticIDE::setupUI() {
    // ...existing code...
    
    // Left panel: File browser
    m_fileBrowser = new FileBrowser(this);
    m_fileBrowserDock = new QDockWidget("Files", this);  // STORE POINTER
    m_fileBrowserDock->setWidget(m_fileBrowser);
    addDockWidget(Qt::LeftDockWidgetArea, m_fileBrowserDock);
    
    // Right panel: Chat interface
    m_chatInterface = new ChatInterface(this);
    m_chatDock = new QDockWidget("Agent Chat", this);  // STORE POINTER
    m_chatDock->setWidget(m_chatInterface);
    addDockWidget(Qt::RightDockWidgetArea, m_chatDock);
    
    // Bottom: Terminal pool
    m_terminalPool = new TerminalPool(3, this);
    m_terminalDock = new QDockWidget("Terminals", this);  // STORE POINTER
    m_terminalDock->setWidget(m_terminalPool);
    addDockWidget(Qt::BottomDockWidgetArea, m_terminalDock);
    
    // ...rest of code...
}
```

**Fix toggle methods**:
```cpp
void AgenticIDE::toggleFileBrowser() {
    if (m_fileBrowserDock) {
        m_fileBrowserDock->setVisible(!m_fileBrowserDock->isVisible());
    }
}

void AgenticIDE::toggleChat() {
    if (m_chatDock) {
        m_chatDock->setVisible(!m_chatDock->isVisible());
    }
}

void AgenticIDE::toggleTerminals() {
    if (m_terminalDock) {
        m_terminalDock->setVisible(!m_terminalDock->isVisible());
    }
}
```

---

### 4. InferenceEngine::HotPatchModel() Implementation

**File**: `src/inference_engine_stub.cpp`  
**Add implementation**:
```cpp
bool InferenceEngine::HotPatchModel(const std::string& model_path)
{
    if (!m_initialized) {
        qWarning() << "Engine not initialized. Initialize first with model.";
        return false;
    }
    
    try {
        // Log hotpatch attempt
        qInfo() << "HotPatching model:" << QString::fromStdString(model_path);
        
        // Option 1: Apply runtime optimizations to existing model
        // Option 2: Reload model from new path
        if (!LoadModelFromGGUF(model_path)) {
            qCritical() << "Failed to hotpatch model from:" << QString::fromStdString(model_path);
            return false;
        }
        
        // Re-upload tensors to GPU if available
        UploadTensorsToGPU();
        
        qInfo() << "Model hotpatched successfully";
        return true;
    } catch (const std::exception& e) {
        qCritical() << "Exception during hotpatch:" << e.what();
        return false;
    }
}
```

---

### 5. Settings Dialog Implementation

**File**: `include/agentic_ide.h`  
**No changes needed** - slot already declared

**File**: `src/agentic_ide.cpp`  
**Replace placeholder**:
```cpp
void AgenticIDE::showSettings()
{
    QDialog settingsDialog(this);
    settingsDialog.setWindowTitle("Settings");
    settingsDialog.setMinimumWidth(400);
    
    QVBoxLayout* layout = new QVBoxLayout(&settingsDialog);
    
    // Model settings
    QGroupBox* modelGroup = new QGroupBox("Model Settings", &settingsDialog);
    QVBoxLayout* modelLayout = new QVBoxLayout(modelGroup);
    
    QLabel* modelLabel = new QLabel("Default Model Path:", &settingsDialog);
    QLineEdit* modelPath = new QLineEdit(&settingsDialog);
    modelPath->setText(m_settings->getValue("modelPath", "").toString());
    modelLayout->addWidget(modelLabel);
    modelLayout->addWidget(modelPath);
    
    // Terminal settings
    QGroupBox* terminalGroup = new QGroupBox("Terminal Settings", &settingsDialog);
    QVBoxLayout* terminalLayout = new QVBoxLayout(terminalGroup);
    
    QLabel* shellLabel = new QLabel("Shell Command:", &settingsDialog);
    QLineEdit* shellPath = new QLineEdit(&settingsDialog);
    shellPath->setText(m_settings->getValue("shellCommand", "cmd.exe").toString());
    terminalLayout->addWidget(shellLabel);
    terminalLayout->addWidget(shellPath);
    
    layout->addWidget(modelGroup);
    layout->addWidget(terminalGroup);
    layout->addStretch();
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton("OK", &settingsDialog);
    QPushButton* cancelBtn = new QPushButton("Cancel", &settingsDialog);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    
    connect(okBtn, &QPushButton::clicked, [&]() {
        m_settings->setValue("modelPath", modelPath->text());
        m_settings->setValue("shellCommand", shellPath->text());
        settingsDialog.accept();
    });
    connect(cancelBtn, &QPushButton::clicked, &settingsDialog, &QDialog::reject);
    
    layout->addLayout(buttonLayout);
    
    settingsDialog.exec();
}
```

---

### 6. File Browser Lazy Loading Fix

**File**: `src/file_browser.cpp`  
**Complete the handleItemExpanded method**:
```cpp
void FileBrowser::handleItemExpanded(QTreeWidgetItem* item) {
    QString filepath = item->data(0, Qt::UserRole).toString();
    QString type = item->data(0, Qt::UserRole + 1).toString();
    
    if (type == "drive" || type == "dir") {
        // Clear placeholder children first
        if (item->childCount() > 0) {
            QTreeWidgetItem* firstChild = item->child(0);
            if (firstChild && firstChild->text(0) == "Loading...") {
                item->removeChild(firstChild);
                delete firstChild;
            }
        }
        
        // Load actual directory contents
        QDir dir(filepath);
        if (!dir.exists()) {
            qWarning() << "Directory does not exist:" << filepath;
            return;
        }
        
        QFileInfoList entries = dir.entryInfoList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
            QDir::Name);
        
        for (const QFileInfo& info : entries) {
            QTreeWidgetItem* childItem = new QTreeWidgetItem();
            childItem->setText(0, info.fileName());
            childItem->setData(0, Qt::UserRole, info.absoluteFilePath());
            childItem->setData(0, Qt::UserRole + 1, info.isDir() ? "dir" : "file");
            childItem->setChildIndicatorPolicy(
                info.isDir() ? QTreeWidgetItem::ShowIndicator : QTreeWidgetItem::DontShowIndicator);
            item->addChild(childItem);
        }
        
        item->setData(0, Qt::UserRole + 1, type); // Keep type
    }
}
```

---

### 7. Settings::setValue() and getValue() Implementation

**File**: `src/settings.cpp`  
**Add implementations**:
```cpp
void Settings::setValue(const QString& key, const QVariant& value) {
    if (settings_) {
        settings_->setValue(key, value);
    }
}

QVariant Settings::getValue(const QString& key, const QVariant& default_value) {
    if (settings_) {
        return settings_->value(key, default_value);
    }
    return default_value;
}
```

**In constructor**:
```cpp
Settings::Settings() {
    settings_ = new QSettings("RawrXD", "AgenticIDE");
}

Settings::~Settings() {
    if (settings_) {
        delete settings_;
        settings_ = nullptr;
    }
}
```

---

## Verification Checklist

- [ ] All ChatInterface methods compiled without errors
- [ ] getCurrentText() method exists and compiles  
- [ ] Dock widget pointers stored and toggle methods fixed
- [ ] HotPatchModel() has full implementation (not stub)
- [ ] Settings dialog shows instead of message box
- [ ] File browser expands directories properly
- [ ] Settings::setValue/getValue work correctly
- [ ] Application compiles: `cmake --build . --target RawrXD-AgenticIDE --config Release -j8`
- [ ] No unresolved symbol errors
- [ ] All menu items clickable and functional

---

## Next Steps After Fixes

1. **Build & Test**: Verify compilation succeeds
2. **Runtime Test**: Check each menu item works
3. **Feature Completion**: Implement Phase 2 items
4. **Production Hardening**: Add error handling, logging, metrics
5. **Deployment**: Docker containerization, resource limits

