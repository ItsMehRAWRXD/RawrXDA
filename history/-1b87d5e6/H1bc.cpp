// Native/CLI implementation, no Qt
#include "agent_orchestra.h"
#include "agent_orchestra.h"
#include "native_ui.h"
#include <algorithm>

static std::string to_std_string(const std::string &s) { return s; }

AgentOrchestra::AgentOrchestra()
    : m_voiceProcessor(nullptr)
    , m_isVoiceActive(false)
{
    setupVoiceProcessor();

    // Add default agents
    addAgent("coder", "Code Assistant");
    addAgent("designer", "Design Expert");
    addAgent("analyst", "Data Analyst");
    addAgent("orchestrator", "Task Orchestrator");

    setActiveAgent("orchestrator");

    addMessageToChat("System", "Agent Orchestra initialized. Select an agent and start chatting!", true);

    setupUI();
}

AgentOrchestra::~AgentOrchestra()
{
    if (m_voiceProcessor) {
        m_voiceProcessor->stopRecording();
        delete m_voiceProcessor;
        m_voiceProcessor = nullptr;
    }
}

void AgentOrchestra::setupUI()
{
    // Create native UI controls via cross-platform helpers
    // Host-specific wiring (Windows/Gtk/Cocoa) will be implemented in native_ui.

    // Agent selector
    auto* agentCombo = Native_CreateCombo();
    Native_SetComboMinWidth(agentCombo, 150);
    Native_SetComboCallback(agentCombo, [this](int index, const std::string &id) {
        onAgentSelected(index);
    });

    auto* accentCombo = Native_CreateCombo();
    Native_AddComboItem(accentCombo, "English", static_cast<int>(VoiceProcessor::English));
    Native_AddComboItem(accentCombo, "Australian", static_cast<int>(VoiceProcessor::Australian));
    Native_AddComboItem(accentCombo, "British (GBE)", static_cast<int>(VoiceProcessor::GBE));
    Native_AddComboItem(accentCombo, "American", static_cast<int>(VoiceProcessor::American));
    Native_SetComboMinWidth(accentCombo, 150);
    Native_SetComboCallback(accentCombo, [this](int idx, const std::string&) { onAccentChanged(idx); });

    // Voice/Send/Clear buttons
    auto* voiceBtn = Native_CreateButton("\xF0\x9F\x8E\xA4 Voice");
    Native_SetButtonToggle(voiceBtn, true);
    Native_SetButtonCallback(voiceBtn, [this]() { onVoiceButtonClicked(); });

    auto* clearBtn = Native_CreateButton("Clear");
    Native_SetButtonCallback(clearBtn, [this]() { clearHistory(); });

    auto* sendBtn = Native_CreateButton("Send");
    Native_SetButtonCallback(sendBtn, [this]() { onSendButtonClicked(); });

    // Chat display and input are hosted by native editor components
    // We only connect logical callbacks here: input->send
}

void AgentOrchestra::setupVoiceProcessor()
{
    m_voiceProcessor = new VoiceProcessor();
    m_voiceProcessor->setVolume(m_outputVolumePercent);

    // Wire native callbacks
    m_voiceProcessor->setTextRecognizedCallback([this](const std::string &text) {
        onVoiceTextReceived(text);
    });
    m_voiceProcessor->setErrorCallback([this](const std::string &err) {
        onVoiceError(err);
    });
    m_voiceProcessor->setRecordingStateCallback([this](bool recording) {
        if (recording) {
            addMessageToChat("System", "Voice recording started...", true);
        } else {
            addMessageToChat("System", "Voice recording stopped. Processing...", true);
        }
    });
}

void AgentOrchestra::addAgent(const std::string& agentId, const std::string& agentName)
{
    m_agents[agentId] = agentName;
    Native_AddComboItemByValue(agentId, agentName);
    Native_AddAgentListItem((std::string("🤖 ") + agentName));
}

void AgentOrchestra::removeAgent(const std::string& agentId)
{
    auto it = m_agents.find(agentId);
    if (it != m_agents.end()) {
        m_agents.erase(it);
        Native_RemoveComboItemByValue(agentId);
        Native_RemoveAgentListItemByValue(agentId);
    }
}

void AgentOrchestra::setActiveAgent(const std::string& agentId)
{
    if (m_agents.find(agentId) != m_agents.end()) {
        m_activeAgentId = agentId;
        Native_SelectComboItemByValue(agentId);
        Native_SelectAgentListRowByValue(agentId);
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
        Native_SetButtonChecked("voice", true);
        Native_SetButtonText("voice", "\xF0\x9F\x94\xB4 Recording...");
    } else {
        Native_SetButtonChecked("voice", false);
        onVoiceError("Failed to start voice recording");
    }
}

void AgentOrchestra::stopVoiceInput()
{
    if (m_voiceProcessor) {
        m_voiceProcessor->stopRecording();
    }

    m_isVoiceActive = false;
    Native_SetButtonChecked("voice", false);
    Native_SetButtonText("voice", "\xF0\x9F\x8E\xA4 Voice");
}

void AgentOrchestra::setVoiceAccent(VoiceProcessor::Accent accent)
{
    if (m_voiceProcessor) {
        m_voiceProcessor->setAccent(accent);
    }
}

void AgentOrchestra::sendMessage(const std::string& message)
{
    std::string trimmed = message;
    // trim
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), trimmed.end());
    if (trimmed.empty()) return;

    addMessageToChat("You", trimmed, false);

    if (!m_activeAgentId.empty()) {
        std::string response = generateAgentResponse(m_activeAgentId, trimmed);
        std::string agentName = m_agents.count(m_activeAgentId) ? m_agents[m_activeAgentId] : "Agent";
        addMessageToChat(agentName, response, true);

        if (m_voiceProcessor) {
            m_voiceProcessor->speakText(response);
        }

        if (m_messageCallback) m_messageCallback(m_activeAgentId, response);
    }

    Native_ClearInputText();
}

void AgentOrchestra::clearHistory()
{
    Native_ClearChat();
    m_chatHistory.clear();
    addMessageToChat("System", "Chat history cleared.", true);
}

void AgentOrchestra::onSendButtonClicked()
{
    std::string message = Native_GetInputText();
    sendMessage(message);
}

void AgentOrchestra::onVoiceButtonClicked()
{
    if (m_isVoiceActive) stopVoiceInput(); else startVoiceInput();
}

void AgentOrchestra::onAccentChanged(int index)
{
    if (index >= 0) {
        setVoiceAccent(static_cast<VoiceProcessor::Accent>(index));
    }
}

void AgentOrchestra::onAgentSelected(int index)
{
    std::string agentId = Native_GetComboValueAt(index);
    if (!agentId.empty()) {
        m_activeAgentId = agentId;
        Native_SelectAgentListRowByIndex(index);
        std::string agentName = m_agents.count(agentId) ? m_agents[agentId] : "Agent";
        addMessageToChat("System", std::string("Switched to ") + agentName, true);
    }
}

void AgentOrchestra::onVoiceTextReceived(const std::string& text)
{
    if (!text.empty()) {
        Native_SetInputText(text);
        addMessageToChat("System", std::string("Voice recognized: \"") + text + "\"", true);
        sendMessage(text);
    }
}

void AgentOrchestra::onVoiceError(const std::string& error)
{
    addMessageToChat("System", std::string("Voice Error: ") + error, true);
    if (m_errorCallback) m_errorCallback(error);
}

void AgentOrchestra::processAgentResponse(const std::string& agentId, const std::string& message)
{
    std::string response = generateAgentResponse(agentId, message);
    std::string agentName = m_agents.count(agentId) ? m_agents[agentId] : "Agent";
    addMessageToChat(agentName, response, true);
}

std::string to_lower(const std::string &s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c){ return std::tolower(c); });
    return out;
}

std::string AgentOrchestra::generateAgentResponse(const std::string& agentId, const std::string& input)
{
    std::string response;
    std::string lower = to_lower(input);

    if (agentId == "coder") {
        if (lower.find("function") != std::string::npos || lower.find("code") != std::string::npos) {
            response = "I can help you write that function. Here's a starting template:\n\n";
            response += "```cpp\nvoid yourFunction() {\n    // TODO: Implement your logic here\n}\n```\n\n";
            response += "Would you like me to add specific functionality?";
        } else if (lower.find("debug") != std::string::npos || lower.find("error") != std::string::npos) {
            response = "Let me help you debug that. Please share:\n1. The error message you're seeing\n2. The relevant code snippet\n3. What you expected to happen\n\nI'll analyze it and provide a solution.";
        } else {
            response = "I'm your Code Assistant. I can help you with:\n• Writing functions and classes\n• Debugging code issues\n• Code optimization\n• Best practices and patterns\n\nWhat would you like assistance with?";
        }
    }
    else if (agentId == "designer") {
        if (lower.find("ui") != std::string::npos || lower.find("design") != std::string::npos) {
            response = "For UI design, I recommend considering:\n1. Color Scheme: Use consistent branding colors\n2. Typography: Clear, readable fonts (14-16pt for body)\n3. Spacing: Adequate padding and margins (8px minimum)\n4. Accessibility: High contrast ratios (4.5:1 minimum)\n\nWould you like specific recommendations for your interface?";
        } else if (lower.find("layout") != std::string::npos) {
            response = "For layout design:\n• Use vertical stacking for sections\n• Use horizontal groupings for related controls\n• Use resizable panels for large views\n\nWhat type of layout are you creating?";
        } else {
            response = "I'm your Design Expert. I can help with UI/UX, color schemes, typography, and accessibility.";
        }
    }
    else if (agentId == "analyst") {
        if (lower.find("analyze") != std::string::npos || lower.find("data") != std::string::npos) {
            response = "I'll analyze that data. For thorough analysis, please provide: CSV/JSON, key metrics, time range, and expected insights.";
        } else if (lower.find("performance") != std::string::npos) {
            response = "For performance analysis: track response time (p50/p95/p99), memory, CPU, error rates. I can help set up dashboards.";
        } else {
            response = "I'm your Data Analyst. I can help with data analysis, visualization, and metrics.";
        }
    }
    else if (agentId == "orchestrator") {
        if (lower.find("task") != std::string::npos || lower.find("plan") != std::string::npos) {
            response = "I'll orchestrate that task. Phase 1: Requirements; Phase 2: Design; Phase 3: Implementation; Phase 4: Testing; Phase 5: Deployment. Which agents should I assign?";
        } else if (lower.find("coordinate") != std::string::npos) {
            response = "I'll coordinate the agents. Current team:\n";
            for (auto &it : m_agents) {
                response += "🤖 " + it.first + ": " + it.second + "\n";
            }
            response += "\nHow should I distribute the workload?";
        } else {
            response = "I'm your Task Orchestrator. Available agents: " + std::to_string(m_agents.size()) + "\nWhat task should I orchestrate?";
        }
    }
    else {
        response = "I'm ready to assist. Please select a specific agent for specialized help, or describe what you need.";
    }

    return response;
}

void AgentOrchestra::addMessageToChat(const std::string& sender, const std::string& message, bool isAgent)
{
    // Build timestamp
    time_t t = time(nullptr);
    tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    std::string timestamp(buf);

    // Append to native chat view
    Native_AppendChatMessage(sender, message, timestamp, isAgent);
    Native_ScrollChatToBottom();

    // Store in history
    ChatMessage entry{sender, message, timestamp, isAgent};
    m_chatHistory.push_back(entry);
}
