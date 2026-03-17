# AI Chat Panel - Developer Quick Reference

## Architecture Overview

```
MainWindow_v5
├── QTabWidget (m_chatTabs) - holds multiple panels
├── AIChatPanelManager (m_chatPanelManager) - lifecycle
├── AIChatPanel* (m_currentChatPanel) - active panel
└── Signals:
    ├── Tab switching → update m_currentChatPanel
    ├── New Chat → create panel, add to manager
    ├── Model ready → enable/disable all panel inputs
    └── Chat message → forward to AgenticEngine
```

## Key Classes

### AIChatPanel
**Location:** `src/qtapp/ai_chat_panel.hpp/cpp`

**Public Methods:**
```cpp
void initialize();                              // Two-phase init
void addUserMessage(const QString& msg);        // Display user message
void addAssistantMessage(const QString& msg);   // Display AI response
void setInputEnabled(bool enabled);             // Gate input by model status
void setCloudConfig(bool enable, QString endpoint, QString apiKey);
void setLocalConfig(bool enable, QString endpoint);
void setRequestTimeout(int ms);
```

**Key Members:**
```cpp
QLineEdit* m_inputField;              // User input
QPushButton* m_sendButton;            // Send button
QScrollArea* m_scrollArea;            // Message display
QNetworkAccessManager* m_netManager;  // HTTP client (created in initialize)
```

**Signals:**
```cpp
void messageSubmitted(const QString& message);  // User sent message
void quickActionTriggered(const QString& action, const QString& context);
```

### AIChatPanelManager
**Location:** `src/qtapp/ai_chat_panel_manager.hpp/cpp`

**Public Methods:**
```cpp
AIChatPanel* createPanel(QWidget* parent);  // Max 100 panels
void destroyPanel(AIChatPanel* panel);      // Safe cleanup
void setCloudConfig(bool enable, QString endpoint, QString apiKey);
void setLocalConfig(bool enable, QString endpoint);
void setRequestTimeout(int ms);             // Broadcast to all panels
int getPanelCount() const;                  // Current count
```

**Signals:**
```cpp
void panelCreated(AIChatPanel* panel);
void panelDestroyed(AIChatPanel* panel);
```

## Usage Patterns

### Creating a New Chat Panel
```cpp
// Manager handles creation and config propagation
AIChatPanel* newPanel = m_chatPanelManager->createPanel(this);
if (newPanel) {
    int idx = m_chatTabs->addTab(newPanel, tr("Chat %1").arg(count));
    m_chatTabs->setCurrentIndex(idx);
    m_currentChatPanel = newPanel;
}
```

### Sending a Message
```cpp
// User sends message → signal → onChatMessageSent
void MainWindow::onChatMessageSent(const QString& message)
{
    if (m_agenticEngine) {
        QString context = m_multiTabEditor->getSelectedText();
        m_agenticEngine->processMessage(message, context);
    }
}
```

### Receiving AI Response
```cpp
// AgenticEngine emits → connected to current panel
connect(m_agenticEngine, &AgenticEngine::responseReady,
        m_currentChatPanel, &AIChatPanel::addAssistantMessage);
```

### Gating on Model Ready
```cpp
// Model loads → emit modelReady(true) → enable all inputs
connect(m_agenticEngine, &AgenticEngine::modelReady, this, [this](bool ready){
    for (int i = 0; i < m_chatTabs->count(); ++i) {
        if (auto* panel = qobject_cast<AIChatPanel*>(m_chatTabs->widget(i))) {
            panel->setInputEnabled(ready);
        }
    }
});
```

## Configuration Flow

```
Settings Dialog (user changes)
    ↓
QSettings (saves: aichat/enableCloud, aichat/cloudEndpoint, etc.)
    ↓
settingsApplied() signal
    ↓
MainWindow loads from QSettings
    ↓
m_chatPanelManager->setCloudConfig(...) / setLocalConfig(...)
    ↓
Broadcasts to all panels via setters
    ↓
Each panel updates: m_cloudEnabled, m_cloudEndpoint, m_apiKey, etc.
```

## Logging

All operations log to `qDebug()` with context:

```cpp
qDebug() << "[MainWindow] Panel" << i << "input enabled - model ready";
qDebug() << "[AIChatPanel] Cloud request to:" << m_cloudEndpoint;
qDebug() << "AIChatPanel request latency ms:" << latencyMs;
```

## Common Issues & Fixes

### Issue: Message goes to wrong panel
**Solution:** Verify `m_currentChatPanel` is updated on tab switch
```cpp
connect(m_chatTabs, QOverload<int>::of(&QTabWidget::currentChanged),
        this, [this](int index){
    if (index >= 0) m_currentChatPanel = qobject_cast<AIChatPanel*>(m_chatTabs->widget(index));
});
```

### Issue: Input field always disabled
**Solution:** Ensure model signals `modelReady(true)` after loading
```cpp
// In your model load completion handler:
m_agenticEngine->modelReady(true);  // Emit signal
```

### Issue: Network timeout errors
**Solution:** Increase timeout in settings (default 30000ms)
```cpp
// Settings → AI Chat tab → Request Timeout
int timeout = settings.value("aichat/requestTimeout", 30000).toInt();
m_chatPanelManager->setRequestTimeout(timeout);
```

## Testing Checklist

- [ ] Create 5 chat panels
- [ ] Send message in panel 1, verify appears
- [ ] Switch to panel 2, send message, verify goes to panel 2
- [ ] Verify panel 1 still shows its message
- [ ] Close panel 2, verify removed from tab
- [ ] Load model, verify all inputs become enabled
- [ ] Test cloud endpoint in Settings
- [ ] Test local endpoint (if available)
- [ ] Verify timeout applied to all panels
- [ ] Verify logs show correct context

## Performance Notes

- **Panel limit:** 100 (enforced by manager)
- **Memory per panel:** ~2-3 MB (QWidget overhead)
- **Network timeout:** 30 seconds default (configurable)
- **Message history:** Unlimited per panel (consider pagination for 1000+ messages)

## Future Enhancements

1. **Persistent chat history** - Save/load conversation from disk
2. **Search across panels** - Full-text search in all messages
3. **Panel themes** - Different color schemes per panel
4. **Conversation export** - Save to Markdown/PDF
5. **Model switching** - Change model mid-conversation
6. **Response streaming** - Show tokens as they arrive (token visualization)

---

**Last Updated:** Dec 15, 2025  
**Status:** Production Ready  
**Maintainer:** RawrXD Development Team
