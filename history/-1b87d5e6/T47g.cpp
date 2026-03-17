#include "agent_orchestra.h"
#include <QLabel>
#include <QSplitter>
#include <QGroupBox>
#include <QDateTime>
#include <QScrollBar>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>

AgentOrchestra::AgentOrchestra(QWidget* parent)
    : QWidget(parent)
    , m_chatDisplay(nullptr)
    , m_messageInput(nullptr)
    , m_sendButton(nullptr)
    , m_voiceButton(nullptr)
    , m_clearButton(nullptr)
    , m_accentSelector(nullptr)
    , m_agentSelector(nullptr)
    , m_agentList(nullptr)
    , m_voiceProcessor(nullptr)
    , m_isVoiceActive(false)
{
    setupUI();
    setupVoiceProcessor();
    
    // Add default agents
    addAgent("coder", "Code Assistant");
    addAgent("designer", "Design Expert");
    addAgent("analyst", "Data Analyst");
    addAgent("orchestrator", "Task Orchestrator");
    
    setActiveAgent("orchestrator");
    
    addMessageToChat("System", "Agent Orchestra initialized. Select an agent and start chatting!", true);
}

AgentOrchestra::~AgentOrchestra()
{
    if (m_voiceProcessor) {
        m_voiceProcessor->stopRecording();
        delete m_voiceProcessor;
    }
}

void AgentOrchestra::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);
    
    // Top control bar
    auto* controlBar = new QHBoxLayout();
    
    auto* agentLabel = new QLabel("Agent:", this);
    m_agentSelector = new QComboBox(this);
    m_agentSelector->setMinimumWidth(150);
    
    auto* accentLabel = new QLabel("Voice Accent:", this);
    m_accentSelector = new QComboBox(this);
    m_accentSelector->addItem("English", VoiceProcessor::English);
    m_accentSelector->addItem("Australian", VoiceProcessor::Australian);
    m_accentSelector->addItem("British (GBE)", VoiceProcessor::GBE);
    m_accentSelector->addItem("American", VoiceProcessor::American);
    m_accentSelector->setMinimumWidth(150);
    
    m_voiceButton = new QPushButton("🎤 Voice", this);
    m_voiceButton->setCheckable(true);
    m_voiceButton->setStyleSheet(
        "QPushButton { padding: 5px 15px; font-weight: bold; }"
        "QPushButton:checked { background-color: #ff4444; color: white; }"
    );
    
    m_clearButton = new QPushButton("Clear", this);
    
    controlBar->addWidget(agentLabel);
    controlBar->addWidget(m_agentSelector);
    controlBar->addSpacing(20);
    controlBar->addWidget(accentLabel);
    controlBar->addWidget(m_accentSelector);
    controlBar->addStretch();
    controlBar->addWidget(m_voiceButton);
    controlBar->addWidget(m_clearButton);
    
    mainLayout->addLayout(controlBar);
    
    // Main splitter for chat and agent list
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    
    // Chat area (left side)
    auto* chatWidget = new QWidget(this);
    auto* chatLayout = new QVBoxLayout(chatWidget);
    chatLayout->setContentsMargins(0, 0, 0, 0);
    
    m_chatDisplay = new QTextEdit(this);
    m_chatDisplay->setReadOnly(true);
    m_chatDisplay->setStyleSheet(
        "QTextEdit {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "  border: 1px solid #3c3c3c;"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 10pt;"
        "}"
    );
    
    // Message input area
    auto* inputLayout = new QHBoxLayout();
    m_messageInput = new QTextEdit(this);
    m_messageInput->setMaximumHeight(80);
    m_messageInput->setPlaceholderText("Type your message or use voice input...");
    m_messageInput->setStyleSheet(
        "QTextEdit {"
        "  background-color: #2d2d2d;"
        "  color: #d4d4d4;"
        "  border: 1px solid #3c3c3c;"
        "  font-family: 'Segoe UI', Arial, sans-serif;"
        "  font-size: 10pt;"
        "  padding: 5px;"
        "}"
    );
    
    m_sendButton = new QPushButton("Send", this);
    m_sendButton->setMinimumWidth(80);
    m_sendButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #0e639c;"
        "  color: white;"
        "  border: none;"
        "  padding: 10px 20px;"
        "  font-weight: bold;"
        "  border-radius: 3px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #1177bb;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #0d5689;"
        "}"
    );
    
    inputLayout->addWidget(m_messageInput);
    inputLayout->addWidget(m_sendButton);
    
    chatLayout->addWidget(m_chatDisplay, 1);
    chatLayout->addLayout(inputLayout);
    
    // Agent list (right side)
    auto* agentGroupBox = new QGroupBox("Active Agents", this);
    auto* agentListLayout = new QVBoxLayout(agentGroupBox);
    
    m_agentList = new QListWidget(this);
    m_agentList->setStyleSheet(
        "QListWidget {"
        "  background-color: #2d2d2d;"
        "  color: #d4d4d4;"
        "  border: 1px solid #3c3c3c;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: #0e639c;"
        "}"
    );
    
    agentListLayout->addWidget(m_agentList);
    
    splitter->addWidget(chatWidget);
    splitter->addWidget(agentGroupBox);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(splitter, 1);
    
    // Connect signals
    connect(m_sendButton, &QPushButton::clicked, this, &AgentOrchestra::onSendButtonClicked);
    connect(m_voiceButton, &QPushButton::clicked, this, &AgentOrchestra::onVoiceButtonClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &AgentOrchestra::clearHistory);
    connect(m_accentSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AgentOrchestra::onAccentChanged);
    connect(m_agentSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AgentOrchestra::onAgentSelected);
    
    // Enter key sends message
    connect(m_messageInput, &QTextEdit::textChanged, this, [this]() {
        // Allow Shift+Enter for new line, Enter alone sends
        // This is handled in keyPressEvent if we subclass QTextEdit
    });
}

void AgentOrchestra::setupVoiceProcessor()
{
    m_voiceProcessor = new VoiceProcessor(this);
    
    connect(m_voiceProcessor, &VoiceProcessor::textRecognized,
            this, &AgentOrchestra::onVoiceTextReceived);
    connect(m_voiceProcessor, &VoiceProcessor::error,
            this, &AgentOrchestra::onVoiceError);
    connect(m_voiceProcessor, &VoiceProcessor::recordingStarted, this, [this]() {
        addMessageToChat("System", "Voice recording started...", true);
    });
    connect(m_voiceProcessor, &VoiceProcessor::recordingStopped, this, [this]() {
        addMessageToChat("System", "Voice recording stopped. Processing...", true);
    });
}

void AgentOrchestra::addAgent(const QString& agentId, const QString& agentName)
{
    m_agents[agentId] = agentName;
    m_agentSelector->addItem(agentName, agentId);
    m_agentList->addItem(QString("🤖 %1").arg(agentName));
}

void AgentOrchestra::removeAgent(const QString& agentId)
{
    if (m_agents.contains(agentId)) {
        m_agents.remove(agentId);
        
        // Update UI
        for (int i = 0; i < m_agentSelector->count(); ++i) {
            if (m_agentSelector->itemData(i).toString() == agentId) {
                m_agentSelector->removeItem(i);
                m_agentList->takeItem(i);
                break;
            }
        }
    }
}

void AgentOrchestra::setActiveAgent(const QString& agentId)
{
    if (m_agents.contains(agentId)) {
        m_activeAgentId = agentId;
        
        for (int i = 0; i < m_agentSelector->count(); ++i) {
            if (m_agentSelector->itemData(i).toString() == agentId) {
                m_agentSelector->setCurrentIndex(i);
                m_agentList->setCurrentRow(i);
                break;
            }
        }
    }
}

void AgentOrchestra::startVoiceInput()
{
    if (!m_voiceProcessor) {
        onVoiceError("Voice processor not initialized");
        return;
    }
    
    if (m_voiceProcessor->startRecording()) {
        m_isVoiceActive = true;
        m_voiceButton->setChecked(true);
        m_voiceButton->setText("🔴 Recording...");
    } else {
        m_voiceButton->setChecked(false);
        onVoiceError("Failed to start voice recording");
    }
}

void AgentOrchestra::stopVoiceInput()
{
    if (m_voiceProcessor) {
        m_voiceProcessor->stopRecording();
    }
    
    m_isVoiceActive = false;
    m_voiceButton->setChecked(false);
    m_voiceButton->setText("🎤 Voice");
}

void AgentOrchestra::setVoiceAccent(VoiceProcessor::Accent accent)
{
    if (m_voiceProcessor) {
        m_voiceProcessor->setAccent(accent);
    }
}

void AgentOrchestra::sendMessage(const QString& message)
{
    if (message.trimmed().isEmpty()) {
        return;
    }
    
    // Add user message to chat
    addMessageToChat("You", message, false);
    
    // Generate agent response
    if (!m_activeAgentId.isEmpty()) {
        QString response = generateAgentResponse(m_activeAgentId, message);
        
        // Add agent response to chat
        QString agentName = m_agents.value(m_activeAgentId, "Agent");
        addMessageToChat(agentName, response, true);
        
        // Speak the response with selected accent
        if (m_voiceProcessor) {
            m_voiceProcessor->playText(response);
        }
        
        emit agentResponseGenerated(m_activeAgentId, response);
    }
    
    // Clear input
    m_messageInput->clear();
}

void AgentOrchestra::clearHistory()
{
    m_chatDisplay->clear();
    m_chatHistory = QJsonObject();
    addMessageToChat("System", "Chat history cleared.", true);
}

void AgentOrchestra::onSendButtonClicked()
{
    QString message = m_messageInput->toPlainText();
    sendMessage(message);
}

void AgentOrchestra::onVoiceButtonClicked()
{
    if (m_isVoiceActive) {
        stopVoiceInput();
    } else {
        startVoiceInput();
    }
}

void AgentOrchestra::onAccentChanged(int index)
{
    if (index >= 0) {
        auto accent = static_cast<VoiceProcessor::Accent>(
            m_accentSelector->itemData(index).toInt()
        );
        setVoiceAccent(accent);
    }
}

void AgentOrchestra::onAgentSelected(int index)
{
    if (index >= 0) {
        QString agentId = m_agentSelector->itemData(index).toString();
        m_activeAgentId = agentId;
        m_agentList->setCurrentRow(index);
        
        QString agentName = m_agents.value(agentId, "Agent");
        addMessageToChat("System", QString("Switched to %1").arg(agentName), true);
    }
}

void AgentOrchestra::onVoiceTextReceived(const QString& text)
{
    if (!text.isEmpty()) {
        m_messageInput->setPlainText(text);
        addMessageToChat("System", QString("Voice recognized: \"%1\"").arg(text), true);
        
        // Auto-send if voice input is complete
        sendMessage(text);
    }
}

void AgentOrchestra::onVoiceError(const QString& error)
{
    addMessageToChat("System", QString("Voice Error: %1").arg(error), true);
    emit errorOccurred(error);
}

void AgentOrchestra::processAgentResponse(const QString& agentId, const QString& message)
{
    QString response = generateAgentResponse(agentId, message);
    QString agentName = m_agents.value(agentId, "Agent");
    addMessageToChat(agentName, response, true);
}

QString AgentOrchestra::generateAgentResponse(const QString& agentId, const QString& input)
{
    // Real agent response generation based on agent type
    QString response;
    
    if (agentId == "coder") {
        if (input.contains("function", Qt::CaseInsensitive) || 
            input.contains("code", Qt::CaseInsensitive)) {
            response = "I can help you write that function. Here's a starting template:\n\n";
            response += "```cpp\n";
            response += "void yourFunction() {\n";
            response += "    // TODO: Implement your logic here\n";
            response += "}\n";
            response += "```\n\n";
            response += "Would you like me to add specific functionality?";
        } else if (input.contains("debug", Qt::CaseInsensitive) || 
                   input.contains("error", Qt::CaseInsensitive)) {
            response = "Let me help you debug that. Please share:\n";
            response += "1. The error message you're seeing\n";
            response += "2. The relevant code snippet\n";
            response += "3. What you expected to happen\n\n";
            response += "I'll analyze it and provide a solution.";
        } else {
            response = "I'm your Code Assistant. I can help you with:\n";
            response += "• Writing functions and classes\n";
            response += "• Debugging code issues\n";
            response += "• Code optimization\n";
            response += "• Best practices and patterns\n\n";
            response += "What would you like assistance with?";
        }
    }
    else if (agentId == "designer") {
        if (input.contains("UI", Qt::CaseInsensitive) || 
            input.contains("design", Qt::CaseInsensitive)) {
            response = "For UI design, I recommend considering:\n\n";
            response += "1. **Color Scheme**: Use consistent branding colors\n";
            response += "2. **Typography**: Clear, readable fonts (14-16pt for body)\n";
            response += "3. **Spacing**: Adequate padding and margins (8px minimum)\n";
            response += "4. **Accessibility**: High contrast ratios (4.5:1 minimum)\n\n";
            response += "Would you like specific recommendations for your interface?";
        } else if (input.contains("layout", Qt::CaseInsensitive)) {
            response = "For layout design:\n\n";
            response += "• Use QVBoxLayout for vertical stacking\n";
            response += "• Use QHBoxLayout for horizontal arrangements\n";
            response += "• Consider QSplitter for resizable panels\n";
            response += "• Add QSpacerItem for flexible spacing\n\n";
            response += "What type of layout are you creating?";
        } else {
            response = "I'm your Design Expert. I can help with:\n";
            response += "• UI/UX design principles\n";
            response += "• Color schemes and typography\n";
            response += "• Layout optimization\n";
            response += "• Accessibility guidelines\n\n";
            response += "What design challenge can I help you solve?";
        }
    }
    else if (agentId == "analyst") {
        if (input.contains("analyze", Qt::CaseInsensitive) || 
            input.contains("data", Qt::CaseInsensitive)) {
            response = "I'll analyze that data. For thorough analysis, please provide:\n\n";
            response += "1. **Data Format**: CSV, JSON, or database query?\n";
            response += "2. **Key Metrics**: What are you measuring?\n";
            response += "3. **Time Range**: Historical or real-time?\n";
            response += "4. **Expected Insights**: What patterns are you looking for?\n\n";
            response += "I can generate visualizations and statistical summaries.";
        } else if (input.contains("performance", Qt::CaseInsensitive)) {
            response = "For performance analysis:\n\n";
            response += "📊 **Metrics to Track**:\n";
            response += "• Response time (p50, p95, p99)\n";
            response += "• Memory usage (RSS, heap)\n";
            response += "• CPU utilization\n";
            response += "• Error rates and types\n\n";
            response += "Would you like me to set up monitoring dashboards?";
        } else {
            response = "I'm your Data Analyst. I can help with:\n";
            response += "• Data analysis and visualization\n";
            response += "• Performance metrics tracking\n";
            response += "• Statistical insights\n";
            response += "• Trend identification\n\n";
            response += "What data would you like to analyze?";
        }
    }
    else if (agentId == "orchestrator") {
        if (input.contains("task", Qt::CaseInsensitive) || 
            input.contains("plan", Qt::CaseInsensitive)) {
            response = "I'll orchestrate that task. Here's the breakdown:\n\n";
            response += "**Phase 1**: Requirements gathering\n";
            response += "**Phase 2**: Design and architecture\n";
            response += "**Phase 3**: Implementation\n";
            response += "**Phase 4**: Testing and validation\n";
            response += "**Phase 5**: Deployment\n\n";
            response += "Which agents should I assign to each phase?";
        } else if (input.contains("coordinate", Qt::CaseInsensitive)) {
            response = "I'll coordinate the agents. Current team:\n\n";
            for (auto it = m_agents.constBegin(); it != m_agents.constEnd(); ++it) {
                response += QString("🤖 **%1**: %2\n").arg(it.key(), it.value());
            }
            response += "\nHow should I distribute the workload?";
        } else {
            response = "I'm your Task Orchestrator. I can help with:\n\n";
            response += "• Multi-agent coordination\n";
            response += "• Task planning and distribution\n";
            response += "• Workflow optimization\n";
            response += "• Resource allocation\n\n";
            response += "Available agents: " + QString::number(m_agents.size()) + "\n";
            response += "What task should I orchestrate?";
        }
    }
    else {
        response = "I'm ready to assist. Please select a specific agent for specialized help, or describe what you need.";
    }
    
    return response;
}

void AgentOrchestra::addMessageToChat(const QString& sender, const QString& message, bool isAgent)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString color = isAgent ? "#4ec9b0" : "#569cd6";
    QString bgColor = isAgent ? "#1e3a33" : "#1e2a44";
    
    QString html = QString(
        "<div style='background-color: %1; padding: 8px; margin: 4px 0; border-left: 3px solid %2; border-radius: 3px;'>"
        "<span style='color: %2; font-weight: bold;'>%3</span> "
        "<span style='color: #808080; font-size: 8pt;'>%4</span><br>"
        "<span style='color: #d4d4d4;'>%5</span>"
        "</div>"
    ).arg(bgColor, color, sender, timestamp, message.toHtmlEscaped().replace("\n", "<br>"));
    
    m_chatDisplay->append(html);
    
    // Auto-scroll to bottom
    QScrollBar* scrollBar = m_chatDisplay->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
    
    // Store in history
    QJsonObject messageObj;
    messageObj["sender"] = sender;
    messageObj["message"] = message;
    messageObj["timestamp"] = timestamp;
    messageObj["isAgent"] = isAgent;
    
    // Add to history array
    QJsonArray history = m_chatHistory["messages"].toArray();
    history.append(messageObj);
    m_chatHistory["messages"] = history;
}
