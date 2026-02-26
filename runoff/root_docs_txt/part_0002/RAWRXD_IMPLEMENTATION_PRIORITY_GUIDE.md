# RawrXD IDE - Implementation Priority Guide
## Quick Wins & Critical Gaps (Next 12 Weeks)

**Generated**: December 31, 2025  
**Scope**: Phase 1 (Quick Wins) + Phase 2 (Core Features)  
**Projected Parity**: 45% → 70% with Cursor

---

## 🎯 Why These Are Priority

```
Impact vs Effort Matrix:

HIGH IMPACT
    │
    │    ★ Stream UI (2-3w) → Immediate UX win
    │    ★ Inline Edit (3-4w) → Core workflow
    │    ★ External APIs (4-6w) → Feature parity
    │
    │                    ★ LSP (6-8w) → IDE polish
    │                    ★ Context (2-3w) → Better reasoning
    │
    │
LOW IMPACT └─────────────────────────────────────► HIGH EFFORT
           EASY              MEDIUM              HARD
```

---

## 📍 Phase 1: Quick Wins (Weeks 1-3) 🚀

### Task 1.1: Real-Time Streaming UI Integration
**Time**: 2-3 weeks | **Difficulty**: LOW | **Impact**: HIGH

#### Problem
```
Current Flow:
1. User: "Write a function"
2. Backend generates full response (takes 5 seconds)
3. Chat panel receives complete text at once
4. Looks like UI is hanging for 5 seconds
```

#### Solution
```
New Flow:
1. User: "Write a function"
2. Backend streams tokens: "def" → "find" → "(" → ...
3. Chat panel updates in real-time
4. Looks responsive, tokens appear immediately
```

#### Implementation Steps

**Step 1.1.1**: Wire Streaming Engine Signals to Chat UI (1 week)

**File**: `src/backend/streaming_engine.cpp`
```cpp
// EXISTS BUT NOT CONNECTED:
signals:
    void tokenGenerated(const QString& token);      // Define signal
    void generationStarted(const QString& prompt);
    void generationCompleted(const QString& fullText);
    void generationError(const QString& error);
```

**File**: `src/gui/chat_interface.cpp`
```cpp
// ADD CONNECTION:
void ChatInterface::initialize(StreamingEngine* engine) {
    // Connect streaming signals to UI updates
    connect(engine, &StreamingEngine::tokenGenerated,
            this, &ChatInterface::appendTokenToCurrentMessage);
    
    connect(engine, &StreamingEngine::generationStarted,
            this, &ChatInterface::showGeneratingIndicator);
    
    connect(engine, &StreamingEngine::generationCompleted,
            this, &ChatInterface::finalizeMessage);
}

// Add this method to ChatInterface class:
void ChatInterface::appendTokenToCurrentMessage(const QString& token) {
    // Get last message in chat
    auto lastMsg = m_messages.last();
    lastMsg.text += token;  // Append token
    
    // Update UI
    m_chatDisplay->appendText(token);  // Animate token arrival
    m_chatDisplay->repaint();           // Refresh immediately
}

void ChatInterface::showGeneratingIndicator() {
    // Show "typing..." indicator
    m_statusLabel->setText("Generating response...");
    m_progressBar->setValue(0);
    m_progressBar->show();
}

void ChatInterface::finalizeMessage() {
    // Remove typing indicator
    m_statusLabel->clear();
    m_progressBar->hide();
    
    // Mark message as complete
    m_messages.last().isComplete = true;
}
```

**Step 1.1.2**: Update InferenceEngine to Emit Tokens (1 week)

**File**: `src/backend/model_interface.cpp`
```cpp
class InferenceEngine : public QObject {
    // ADD SIGNALS:
    signals:
        void tokenGenerated(const QString& token);
        void generationProgress(int tokensGenerated, int totalTokens);
    
    // MODIFY generate() method:
    void InferenceEngine::generate(const QString& prompt, int maxTokens) {
        auto inputTokens = tokenize(prompt);
        QString generatedText;
        
        for (int i = 0; i < maxTokens; i++) {
            // Generate next token
            auto nextToken = generateNextToken(inputTokens);
            if (nextToken == EOS_TOKEN) break;
            
            // Detokenize and emit
            QString text = detokenize({nextToken});
            generatedText += text;
            
            // EMIT TOKEN (This is the key change):
            emit tokenGenerated(text);  // Send each token immediately
            emit generationProgress(i + 1, maxTokens);
            
            // Add to input for next iteration
            inputTokens.push_back(nextToken);
            
            // Let UI update
            QApplication::processEvents();
        }
        
        // Emit final result
        emit generationCompleted(generatedText);
    }
};
```

**Step 1.1.3**: Add Animated Text Display (1 week)

**File**: `src/gui/animated_chat_display.cpp` (NEW FILE)
```cpp
#pragma once
#include <QPlainTextEdit>
#include <QTimer>

class AnimatedChatDisplay : public QPlainTextEdit {
public:
    void appendTokenWithAnimation(const QString& token) {
        // Calculate position for animation
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::End);
        
        // Insert token with visual feedback
        cursor.insertText(token);
        
        // Update cursor position
        setTextCursor(cursor);
        ensureCursorVisible();
        
        // Visual effect: highlight token briefly
        QTextCharFormat fmt;
        fmt.setBackground(QColor(200, 200, 255, 100));  // Slight highlight
        
        // Fade highlight after 100ms
        QTimer::singleShot(100, this, [this]() {
            setCurrentCharFormat(QTextCharFormat());  // Reset format
        });
    }
    
    void enableTypewriterMode(bool enabled) {
        m_typewriterMode = enabled;
    }

private:
    bool m_typewriterMode = true;
};
```

**Integration Points**:
```
User Input
    ↓
Chat Panel receives prompt
    ↓
IDEAgentBridge::executeWish()
    ↓
InferenceEngine::generate(prompt)
    ├─ Emit tokenGenerated()           ← Step 1.1.2
    │   └─ ChatInterface::appendTokenToCurrentMessage()  ← Step 1.1.1
    │       └─ AnimatedChatDisplay::appendTokenWithAnimation()  ← Step 1.1.3
    │
    └─ Emit generationCompleted()
        └─ ChatInterface::finalizeMessage()
```

**Testing Checklist**:
- [ ] Start generation, verify tokens appear one-by-one
- [ ] Measure latency between token generation and display (<50ms ideal)
- [ ] Verify typing indicator shows/hides correctly
- [ ] Test with different model sizes (small, medium, large)
- [ ] Verify UI remains responsive during generation

---

### Task 1.2: Chat History Persistence
**Time**: 2-3 weeks | **Difficulty**: LOW | **Impact**: MEDIUM

#### Implementation Steps

**Step 1.2.1**: Create SQLite Backend (1 week)

**File**: `src/backend/chat_history_database.hpp` (NEW)
```cpp
#pragma once
#include <QString>
#include <QList>
#include <sqlite3.h>

struct ChatMessage {
    int id;
    QString timestamp;
    QString role;      // "user" or "assistant"
    QString content;
    QString model;     // Which model generated this
    int tokens;        // Token count
};

class ChatHistoryDatabase {
public:
    ChatHistoryDatabase(const QString& dbPath = "chat_history.db");
    ~ChatHistoryDatabase();
    
    // Core operations
    bool open();
    bool close();
    bool createSchema();  // Create tables if needed
    
    // Save/Load
    bool saveMessage(const ChatMessage& msg);
    bool loadSession(const QString& sessionId, QList<ChatMessage>& messages);
    QStringList listSessions();  // Get all session IDs
    
    // Export
    bool exportToCsv(const QString& sessionId, const QString& filepath);
    bool exportToJson(const QString& sessionId, const QString& filepath);
    bool exportToMarkdown(const QString& sessionId, const QString& filepath);
    
    // Search
    QList<ChatMessage> searchMessages(const QString& query);
    QList<ChatMessage> getMessagesByDateRange(const QString& startDate, 
                                               const QString& endDate);
    
private:
    sqlite3* m_db = nullptr;
    QString m_dbPath;
};
```

**Step 1.2.2**: Integrate with ChatWorkspace (1 week)

**File**: `src/backend/chat_workspace.cpp` (MODIFY)
```cpp
class ChatWorkspace : public QObject {
    Q_OBJECT

public:
    ChatWorkspace(QObject* parent = nullptr);
    
    void initialize(const QString& dbPath) {
        m_database = new ChatHistoryDatabase(dbPath);
        m_database->open();
        m_database->createSchema();
    }
    
    // Existing method - enhance:
    void addMessage(const QString& role, const QString& content) {
        ChatMessage msg;
        msg.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
        msg.role = role;
        msg.content = content;
        msg.model = m_currentModel;
        msg.tokens = estimateTokenCount(content);
        
        // Add to memory
        m_messages.append(msg);
        emit messageAdded(msg);
        
        // SAVE TO DATABASE:
        m_database->saveMessage(msg);  // ← Key addition
    }
    
    // New methods for persistence:
    void saveCurrentSession(const QString& sessionId) {
        for (const auto& msg : m_messages) {
            m_database->saveMessage(msg);
        }
        m_currentSessionId = sessionId;
    }
    
    void loadSession(const QString& sessionId) {
        m_messages.clear();
        m_database->loadSession(sessionId, m_messages);
        emit sessionLoaded(sessionId, m_messages);
    }
    
    QStringList getSessions() const {
        return m_database->listSessions();
    }
    
    void exportSession(const QString& sessionId, 
                      const QString& format,  // "csv", "json", "md"
                      const QString& filepath) {
        if (format == "csv") {
            m_database->exportToCsv(sessionId, filepath);
        } else if (format == "json") {
            m_database->exportToJson(sessionId, filepath);
        } else if (format == "md") {
            m_database->exportToMarkdown(sessionId, filepath);
        }
    }

signals:
    void sessionLoaded(const QString& sessionId, 
                      const QList<ChatMessage>& messages);
    void sessionSaved(const QString& sessionId);

private:
    ChatHistoryDatabase* m_database = nullptr;
    QString m_currentSessionId;
};
```

**Step 1.2.3**: Add UI for Session Management (1 week)

**File**: `src/gui/chat_history_panel.cpp` (NEW)
```cpp
class ChatHistoryPanel : public QDockWidget {
    Q_OBJECT

public:
    ChatHistoryPanel(QWidget* parent = nullptr);
    
    void setWorkspace(ChatWorkspace* workspace) {
        m_workspace = workspace;
        connect(m_workspace, &ChatWorkspace::sessionLoaded,
                this, &ChatHistoryPanel::onSessionLoaded);
        refreshSessionList();
    }

private slots:
    void onSessionLoaded(const QString& sessionId, 
                        const QList<ChatMessage>& messages) {
        m_messageList->clear();
        for (const auto& msg : messages) {
            m_messageList->addItem(
                QString("%1: %2...").arg(msg.role, msg.content.left(50))
            );
        }
    }
    
    void onLoadSessionClicked() {
        QString sessionId = m_sessionCombo->currentText();
        if (!sessionId.isEmpty()) {
            m_workspace->loadSession(sessionId);
        }
    }
    
    void onExportSessionClicked() {
        QString sessionId = m_sessionCombo->currentText();
        QString format = m_formatCombo->currentText().toLower();  // csv, json, md
        QString filepath = QFileDialog::getSaveFileName(
            this, "Export Chat", "", "All Files (*)");
        
        if (!filepath.isEmpty()) {
            m_workspace->exportSession(sessionId, format, filepath);
            QMessageBox::information(this, "Success", "Chat exported!");
        }
    }
    
    void onDeleteSessionClicked() {
        QString sessionId = m_sessionCombo->currentText();
        if (QMessageBox::question(this, "Confirm", 
            "Delete session?") == QMessageBox::Yes) {
            m_workspace->deleteSession(sessionId);
            refreshSessionList();
        }
    }
    
    void refreshSessionList() {
        m_sessionCombo->clear();
        for (const auto& session : m_workspace->getSessions()) {
            m_sessionCombo->addItem(session);
        }
    }

private:
    ChatWorkspace* m_workspace = nullptr;
    QComboBox* m_sessionCombo;
    QListWidget* m_messageList;
    QComboBox* m_formatCombo;  // csv, json, markdown
    QPushButton* m_loadBtn;
    QPushButton* m_exportBtn;
    QPushButton* m_deleteBtn;
};
```

**Testing Checklist**:
- [ ] Save chat message to database
- [ ] Load chat session from database
- [ ] Export to CSV (open in Excel, verify content)
- [ ] Export to JSON (validate JSON syntax)
- [ ] Export to Markdown (render in markdown viewer)
- [ ] Search chat history
- [ ] Delete old sessions
- [ ] Handle corrupted database gracefully

---

### Task 1.3: External Model API Support (Phase 1 Prep)
**Time**: 4-6 weeks | **Difficulty**: MEDIUM | **Impact**: HIGH

#### Overview
Add ability to use cloud models (OpenAI, Anthropic) in addition to local GGUF.

**Implementation will be in Phase 2 (weeks 4-6), but here's the skeleton**:

**File**: `src/backend/model_router.hpp` (SCAFFOLD)
```cpp
class ModelRouter {
public:
    enum Backend {
        LocalGGUF,      // ✅ Exists
        OpenAI,         // ❌ To implement
        Anthropic,      // ❌ To implement
        Google          // ❌ Future
    };
    
    struct ModelConfig {
        Backend backend;
        QString modelName;
        QString apiKey;
        QString endpoint;
        int maxTokens;
        float temperature;
    };
    
    // Route to appropriate backend
    QString invoke(const ModelConfig& config, 
                   const QString& prompt) {
        switch (config.backend) {
            case LocalGGUF:
                return invokeLocalGGUF(config, prompt);
            case OpenAI:
                return invokeOpenAI(config, prompt);  // Not yet implemented
            case Anthropic:
                return invokeAnthropic(config, prompt);  // Not yet implemented
        }
    }
    
private:
    // ✅ Already exists
    QString invokeLocalGGUF(const ModelConfig& config, 
                            const QString& prompt);
    
    // ❌ To implement in Phase 2
    QString invokeOpenAI(const ModelConfig& config, 
                         const QString& prompt);
    QString invokeAnthropic(const ModelConfig& config, 
                            const QString& prompt);
};
```

---

## 📍 Phase 2: Core Features (Weeks 4-9) 🔧

### Task 2.1: External Model API Support (Completion)
**Time**: 4-6 weeks | **Difficulty**: MEDIUM | **Impact**: HIGH

**Files to Create**:
- `src/backend/openai_client.cpp/h`
- `src/backend/anthropic_client.cpp/h`
- `src/gui/api_key_manager_dialog.cpp/h`
- `src/gui/model_selector_widget.cpp/h`

**Implementation Skeleton**:
```cpp
// src/backend/openai_client.hpp
class OpenAIClient {
public:
    struct Request {
        QString model;      // "gpt-4o", "gpt-4-turbo"
        QString prompt;
        int maxTokens;
        float temperature;
        float topP;
    };
    
    QString invoke(const Request& req) {
        // 1. Build JSON request
        // 2. Send to OpenAI API
        // 3. Parse response
        // 4. Return text or error
    }
    
    void setAPIKey(const QString& key) {
        m_apiKey = key;  // Store securely
    }
    
    float estimateCost(const QString& model, int promptTokens, 
                      int completionTokens);
};

// src/backend/anthropic_client.hpp
class AnthropicClient {
    // Same pattern as OpenAI
    QString invoke(const Request& req);
    void setAPIKey(const QString& key);
};
```

### Task 2.2: Inline Edit Mode (Cmd+K Pattern)
**Time**: 3-4 weeks | **Difficulty**: MEDIUM | **Impact**: HIGH

#### What It Does
```
User selects code:
│ if (x > 0) {
│     return 1;
│ } ← user highlights this

User types Cmd+K + "simplify to ternary"

Ghost text appears:
│ if (x > 0) {
│     return 1;
│ } ← ghost: "return x > 0 ? 1 : 0;"

User presses Tab to accept
Ghost becomes real code

Result:
│ return x > 0 ? 1 : 0;
```

**Implementation Files**:
- `src/gui/inline_edit_widget.cpp/h` (NEW)
- `src/gui/ghost_text_renderer.cpp/h` (ENHANCE)
- `src/backend/agentic_text_edit.cpp` (COMPLETE)

**Skeleton**:
```cpp
class InlineEditWidget : public QWidget {
public:
    void setEditorCursor(QTextCursor cursor) {
        m_selectedText = cursor.selectedText();
        m_selectionStart = cursor.selectionStart();
        m_selectionEnd = cursor.selectionEnd();
    }
    
    void showSuggestion(const QString& suggestion) {
        // Render ghost text
        m_ghostText = suggestion;
        renderGhostText();
    }
    
    void acceptSuggestion() {
        // Replace selected text with suggestion
        QTextCursor cursor;
        cursor.setPosition(m_selectionStart);
        cursor.setPosition(m_selectionEnd, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(m_ghostText);
    }
    
    void rejectSuggestion() {
        // Dismiss ghost text
        m_ghostText.clear();
        update();
    }
    
signals:
    void suggestionRequested(const QString& selectedText, 
                           const QString& instruction);

private:
    void renderGhostText() {
        // Draw ghost text in faded color
        QPainter painter(this);
        painter.setPen(QColor(100, 100, 100, 100));  // Faded gray
        painter.drawText(m_renderPosition, m_ghostText);
    }
    
    QString m_selectedText;
    QString m_ghostText;
    int m_selectionStart, m_selectionEnd;
};
```

### Task 2.3: Real LSP Integration
**Time**: 6-8 weeks | **Difficulty**: MEDIUM-HIGH | **Impact**: MEDIUM

**This is more complex - would need**:
- LSP client implementation
- Language server launcher
- Diagnostics panel
- Jump-to-def handler
- Hover tooltip widget

**Simplified skeleton**:
```cpp
class LSPClient : public QObject {
public:
    void initialize(const QString& serverPath);
    
    // Request definitions
    QList<Location> gotoDefinition(const QString& file, int line, int col);
    QString getHoverInfo(const QString& file, int line, int col);
    QList<Diagnostic> getDiagnostics(const QString& file);

private:
    // JSON-RPC communication with language server
    void sendRequest(const QString& method, const QJsonObject& params);
    void handleResponse(const QJsonObject& response);
};
```

---

## 📊 Success Metrics

### After Phase 1 (3 weeks)
- ✅ Streaming UI working - tokens appear in real-time
- ✅ Chat history saved to database
- ✅ Can export conversations to CSV/JSON/Markdown
- 📈 Projected parity improvement: 45% → 55%

### After Phase 2 (9 weeks)
- ✅ Can use GPT-4o/Claude in addition to local GGUF
- ✅ Inline edit mode working (Cmd+K pattern)
- ✅ Basic LSP integration (jump-to-def, hover)
- ✅ Chat context includes recent files
- 📈 Projected parity improvement: 55% → 70%

---

## 🎯 Weekly Timeline

```
WEEK 1-2: Streaming UI
├─ Week 1: Wire signals, update InferenceEngine
└─ Week 2: Build animated display, UI integration

WEEK 2-3: Chat History
├─ Week 2: Create database schema, basic operations
└─ Week 3: UI for loading/exporting, search

WEEK 4-6: External Model APIs
├─ Week 4: OpenAI client implementation
├─ Week 5: Anthropic client, model router
└─ Week 6: API key management UI, testing

WEEK 7-9: Core Features
├─ Week 7-8: Inline edit mode (3-4w compressed)
├─ Week 8-9: LSP client basics
└─ Week 9: Integration testing, polish
```

---

## 💻 Testing Checklist

### Streaming UI
- [ ] Token appears within 50ms of generation
- [ ] UI responsive while generating (doesn't freeze)
- [ ] Typing indicator shows/hides correctly
- [ ] Works with different model sizes

### Chat History
- [ ] Saves to database on every message
- [ ] Loads complete sessions from database
- [ ] Exports valid CSV (opens in Excel)
- [ ] Exports valid JSON (syntax check)
- [ ] Search finds messages correctly

### External APIs
- [ ] OpenAI API key stored securely
- [ ] Can switch between local/OpenAI models
- [ ] Cost estimation shows correct values
- [ ] Fallback to local if API fails
- [ ] Error messages clear and helpful

### Inline Edit
- [ ] Ghost text renders in faded color
- [ ] Accepts on Tab key
- [ ] Rejects on Escape key
- [ ] Works with multi-line suggestions

---

## 📚 Reference Documentation

**For detailed implementation**:
- Full audit report: `d:\RAWRXD_AUDIT_REPORT_FINAL.md`
- Quick reference: `d:\RAWRXD_AUDIT_QUICK_REFERENCE.md`
- Existing code locations: See appendices in main report

---

**Ready to implement?** Start with **Task 1.1** (Streaming UI) for immediate UX improvement!

---

**Generated**: December 31, 2025  
**Total Estimated Time**: 9 weeks (3 weeks Phase 1 + 6 weeks Phase 2)
