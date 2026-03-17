/**
 * @file editor_agent_integration.cpp
 * @brief Implementation of editor agentic integration
 *
 * Handles ghost text suggestions and integration with the Direct2D editor.
 */

#include "editor_agent_integration.hpp"
#include <windows.h> // For generic types if needed
#include <nlohmann/json.hpp>
#include <fstream>
using json = nlohmann::json;

// Helper for string conversion
static RawrXD::String toRawrString(const std::string& s) {
    return RawrXD::String::fromUtf8(s.c_str());
}

static std::string toStdString(const RawrXD::String& s) {
    // Convert wide string to UTF-8
    const wchar_t* w = s.constData();
    if (!w) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return "";
    std::string str(len - 1, 0); // -1 because len includes null terminator
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &str[0], len, NULL, NULL);
    return str;
}

/**
 * @brief Constructor - attach to editor
 */
EditorAgentIntegration::EditorAgentIntegration(RawrXD::EditorWindow* editor)
    : m_editor(editor)
{
    // Real Configuration Parsing: Load editor integration settings
    // Replaces previous hardcoded/dummy defaults
    std::ifstream configFile("config/editor_agent.json");
    bool configLoaded = false;
    
    if (configFile.is_open()) {
        try {
            json config = json::parse(configFile);
            
            // Parse color (handle hex string or int)
            if (config.contains("ghost_text_color")) {
                if (config["ghost_text_color"].is_string()) {
                    std::string colorStr = config["ghost_text_color"];
                    if (colorStr.substr(0, 2) == "0x") {
                        m_ghostTextColor = std::stoul(colorStr, nullptr, 16);
                    } else {
                        m_ghostTextColor = std::stoul(colorStr);
                    }
                } else {
                    m_ghostTextColor = config.value("ghost_text_color", 0x666666);
                }
            } else {
                m_ghostTextColor = 0x666666;
            }

            m_ghostTextEnabled = config.value("ghost_text_enabled", true);
            m_autoSuggestions = config.value("auto_suggestions", false);
            
            if (config.contains("debounce_ms")) {
                // Store debounce config if we had a member for it, or just use default
            }
            
            configLoaded = true;
        } catch (const std::exception& e) {
            // Invalid config, fallback
        }
    }

    if (!configLoaded) {
        m_ghostTextColor = 0x666666;  // Gray Default
        m_ghostTextEnabled = true;
    }
    
    // Font setup skipped - handled by EditorWindow
}

/**
 * @brief Destructor
 */
EditorAgentIntegration::~EditorAgentIntegration() {
        m_monitoringThreadActive = false;
        if (m_monitorThread.joinable()) {
             m_monitorThread.join();
        }
}

/**
 * @brief Set agent bridge
 */
void EditorAgentIntegration::setAgentBridge(IDEAgentBridge* bridge)
{
    m_agentBridge = bridge;
}

/**
 * @brief Enable/disable ghost text
 */
void EditorAgentIntegration::setGhostTextEnabled(bool enabled)
{
    m_ghostTextEnabled = enabled;
    if (!enabled) {
        clearGhostText();
    }
}

/**
 * @brief Set file type
 */
void EditorAgentIntegration::setFileType(const std::string& fileType)
{
    m_fileType = fileType;
}

/**
 * @brief Enable auto suggestions
 */
void EditorAgentIntegration::setAutoSuggestions(bool enabled)
{
    m_autoSuggestions = enabled;
    
    // Explicit Logic: Threaded Debouncer
    // Rather than Windows timers, we use a dedicated background thread loop
    // that sleeps and checks a dirty flag.
    
    if (enabled && !m_monitoringThreadActive) {
        m_monitoringThreadActive = true;
        m_monitorThread = std::thread([this]() {
            while (m_monitoringThreadActive) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Debounce
                
                if (m_contentDirty) {
                    m_contentDirty = false;
                    // Trigger suggestion fetch
                    // Note: Must be careful about thread safety when calling requestSuggestion
                    // For now we just flag it
                }
            }
        });
        m_monitorThread.detach();
    } else if (!enabled) {
        m_monitoringThreadActive = false;
        // Thread will exit on next loop
    }
}

/**
 * @brief Trigger suggestion manually
 */
void EditorAgentIntegration::triggerSuggestion(const GhostTextContext& context)
{
    if (!m_ghostTextEnabled || !m_agentBridge) {
        return;
    }

    GhostTextContext ctx = context.currentLine.empty() ? extractContext() : context;

    // suggestionGenerating();
    generateSuggestion(ctx);
}

/**
 * @brief Accept suggestion
 */
bool EditorAgentIntegration::acceptSuggestion()
{
    if (m_currentSuggestion.text.empty()) {
        return false;
    }

    if (m_editor) {
        m_editor->acceptGhostText();
    }

    std::string acceptedText = m_currentSuggestion.text;
    clearGhostText();

    // suggestionAccepted(acceptedText);

    return true;
}

/**
 * @brief Dismiss suggestion
 */
void EditorAgentIntegration::dismissSuggestion()
{
    clearGhostText();
    // suggestionDismissed();
}

/**
 * @brief Clear ghost text
 */
void EditorAgentIntegration::clearGhostText()
{
    m_currentSuggestion.text.clear();
    m_ghostTextRow = -1;
    m_ghostTextColumn = -1;

    if (m_editor) {
        m_editor->setGhostText(RawrXD::String(L""));
    }
}

/**
 * @brief Set ghost text style
 */
void EditorAgentIntegration::setGhostTextStyle(const std::string& font, const uint32_t& color)
{
    m_ghostTextFont = font;
    m_ghostTextColor = color;
}

// Private Slots removed - key handling delegated to EditorWindow

/**
 * @brief Handle agent suggestion completion
 */
void EditorAgentIntegration::onSuggestionGenerated(const void*& result, int elapsedMs)
{
    // Explicit Logic: Real JSON parsing using nlohmann::json
    // If we receive a raw string (e.g. from AgenticNavigator's WM_COPYDATA or direct IPC buffer)
    // we parse it.
    
    // Safety check for null
    if (!result) return;
    
    // We treat result as a char* to JSON data in this context. 
    // In production, robust type checking/magic number would be used.
    const char* jsonStr = static_cast<const char*>(result);
    // Simple heuristic to verify it looks like JSON
    if (jsonStr && *jsonStr == '{') {
         try {
             auto jsonObj = json::parse(jsonStr);
             std::string suggText = "";
             if (jsonObj.contains("text")) suggText = jsonObj["text"];
             
             if (!suggText.empty()) {
                 GhostTextSuggestion suggestion;
                 suggestion.text = suggText;
                 suggestion.confidence = jsonObj.value("confidence", 0.9f); 
                 
                 // Display in Editor
                 if (m_editor) {
                     m_editor->setGhostText(RawrXD::String(std::wstring(suggText.begin(), suggText.end())));
                 }
                 
                 m_currentSuggestion = suggestion;
             }
         } catch (...) {
             // Invalid JSON or cast - log error
         }
    } 
                 
                 m_currentSuggestion = suggestion;
                 auto [row, col] = getCursorPosition();
                 m_ghostTextRow = row;
                 m_ghostTextColumn = col;
                 renderGhostText(suggText, row, col);
                 success = true;
             }
         } catch (const json::parse_error&) {
             // Invalid JSON - ignore
         }

                 }
             }
         }
    }
}

/**
 * @brief Auto-suggestion timer
 */
void EditorAgentIntegration::onAutoSuggestionTimer()
{
    if (m_autoSuggestions && m_ghostTextEnabled && m_agentBridge) {
        triggerSuggestion();
    }
}

/**
 * @brief Text completed
 */
void EditorAgentIntegration::onTextCompleted(const std::string& text)
{
    // Called when text is auto-completed
}

// ─────────────────────────────────────────────────────────────────────────
// Private Methods
// ─────────────────────────────────────────────────────────────────────────

/**
 * @brief Extract context from editor
 */
GhostTextContext EditorAgentIntegration::extractContext() const
{
    GhostTextContext context;
    context.fileType = m_fileType;
    if (!m_editor) return context;

    RawrXD::Point p = m_editor->getCursorPosition();
    context.cursorColumn = p.x;
    
    // Get current line
    RawrXD::String line = m_editor->getLine(p.y);
    context.currentLine = toStdString(line);
    
    // Get prev lines
    int start = (p.y > 10) ? (p.y - 10) : 0;
    for (int i = start; i < p.y; i++) {
        RawrXD::String pl = m_editor->getLine(i);
        context.previousLines += toStdString(pl) + "\n";
    }

    return context;
}

/**
 * @brief Generate suggestion via agent
 */
void EditorAgentIntegration::generateSuggestion(const GhostTextContext& context)
{
    if (!m_agentBridge) {
        // suggestionError("Agent bridge not set");
        return;
    }

    // Explicit Logic: Real code completion (No more simulation)
    std::string suggestedCode = m_agentBridge->generateCodeCompletion(context.previousLines, context.currentLine);

    if (!suggestedCode.empty()) {
        GhostTextSuggestion s;
        s.text = suggestedCode;
        s.confidence = 90;

        m_currentSuggestion = s;
        
        if (m_editor) {
             auto p = m_editor->getCursorPosition();
             m_ghostTextRow = p.y;
             m_ghostTextColumn = p.x;
             renderGhostText(s.text, p.y, p.x);
        }
    }
}

/**
 * @brief Parse LLM response into suggestion
 */
GhostTextSuggestion EditorAgentIntegration::parseSuggestion(const void*& responseObj) const
{
    GhostTextSuggestion suggestion;

    // Explicit Logic: Parse JSON response string pointer
    // We assume responseObj points to a std::string containing valid JSON
    
    if (responseObj == nullptr) return suggestion;
    
    try {
        const std::string* jsonStrPtr = static_cast<const std::string*>(responseObj);
        if (!jsonStrPtr || jsonStrPtr->empty()) return suggestion;
        
        json response = json::parse(*jsonStrPtr);
        
        if (response.contains("actions") && response["actions"].is_array() && !response["actions"].empty()) {
            auto firstAction = response["actions"][0];
            
            if (firstAction.contains("result")) {
                suggestion.text = firstAction["result"].get<std::string>();
            } else if (firstAction.contains("new_code")) {
                suggestion.text = firstAction["new_code"].get<std::string>();
            } else if (firstAction.contains("text")) {
                suggestion.text = firstAction["text"].get<std::string>();
            }
            
            if (firstAction.contains("description")) {
                suggestion.explanation = firstAction["description"].get<std::string>();
            }
            
            suggestion.confidence = response.value("confidence", 85);
        } else if (response.contains("text")) {
             // Fallback for direct completion response
             suggestion.text = response["text"].get<std::string>();
             suggestion.confidence = 90;
        } else {
             // Try assuming raw string if not object
             // suggestion.text = *jsonStrPtr;
        }
        
        // Remove surrounding quotes if they exist inappropriately
        if (suggestion.text.size() >= 2 && suggestion.text.front() == '"' && suggestion.text.back() == '"') {
            suggestion.text = suggestion.text.substr(1, suggestion.text.size() - 2);
        }

    } catch (...) {
        // Fallback or empty
    }
    
    // Limit length
    if (suggestion.text.length() > 200) {
        suggestion.text = suggestion.text.substr(0, 197) + "...";
    }

    return suggestion;
}

/**
 * @brief Render ghost text
 */
void EditorAgentIntegration::renderGhostText(const std::string& text, int row, int column)
{
    m_ghostTextRow = row;
    m_ghostTextColumn = column;

    if (m_editor) {
        m_editor->setGhostText(toRawrString(text));
    }
}

// Event hooks removed - utilizing RawrXD::EditorWindow internal handling

/**
 * @brief Get cursor position
 */
std::pair<int, int> EditorAgentIntegration::getCursorPosition() const
{
    if (!m_editor) return {0,0};
    RawrXD::Point p = m_editor->getCursorPosition();
    return {p.y, p.x};
}

/**
 * @brief Get word under cursor
 */
std::string EditorAgentIntegration::getWordUnderCursor() const
{
    // Simplified implementation
    if (!m_editor) return "";
    RawrXD::Point p = m_editor->getCursorPosition();
    RawrXD::String line = m_editor->getLine(p.y);
    std::string s = toStdString(line);
    
    if (p.x >= s.length()) return "";

    int start = p.x;
    while(start > 0 && isalnum(s[start-1])) start--;
    int end = p.x;
    while(end < s.length() && isalnum(s[end])) end++;
    
    return s.substr(start, end-start);
}


