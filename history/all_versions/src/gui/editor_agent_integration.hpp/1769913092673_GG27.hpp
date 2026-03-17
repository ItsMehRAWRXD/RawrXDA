/**
 * @file editor_agent_integration.hpp
 * @brief Integration of agentic features into the code editor
 *
 * Provides:
 * - Ghost text suggestions (TAB to trigger, ENTER to accept)
 * - Real-time code completions via agent
 * - Context-aware refactoring suggestions
 *
 * @author RawrXD Agent Team
 * @version 1.0.0
 */

#pragma once

#include "ide_agent_bridge.hpp"


#include "RawrXD_EditorWindow.h"
#include <atomic>
#include <thread>

// Forward declaration
namespace RawrXD { class EditorWindow; }

/**
 * @struct GhostTextContext
 * @brief Context for ghost text generation
 */
struct GhostTextContext {
    std::string currentLine;                    ///< Current line being edited
    std::string previousLines;                  ///< Context from previous lines
    int cursorColumn = 0;                   ///< Cursor column in line
    std::string fileType;                       ///< File type (cpp, python, etc)
    int maxSuggestionLength = 200;          ///< Max chars for ghost text
};

/**
 * @struct GhostTextSuggestion
 * @brief Suggested completion text
 */
struct GhostTextSuggestion {
    std::string text;                           ///< Suggested code
    std::string explanation;                    ///< Why this suggestion
    int confidence = 100;                   ///< Confidence 0-100
    bool isComplete = false;                ///< Is this a complete statement?
};

/**
 * @class EditorAgentIntegration
 * @brief Integrates agentic features into the code editor
 *
 * Handles:
 * - TAB key event → trigger ghost text
 * - ENTER key event → accept ghost text
 * - Periodic background suggestions
 * - Ghost text rendering overlay
 *
 * @note Works with RawrXD::EditorWindow
 * @note Non-blocking suggestion generation
 *
 * @example
 * @code
 * EditorAgentIntegration agentEditor(m_editorWindow);
 * agentEditor.setAgentBridge(&agentBridge);
 *
 * // TAB and ENTER now trigger agent suggestions
 * @endcode
 */
class EditorAgentIntegration {

public:
    /**
     * @brief Constructor - attach to code editor
     * @param editor Target code editor widget
     */
    explicit EditorAgentIntegration(RawrXD::EditorWindow* editor);

    /**
     * @brief Destructor
     */
    virtual ~EditorAgentIntegration();

    /**
     * @brief Set the IDEAgentBridge instance
     * @param bridge Agent bridge for generating suggestions
     */
    void setAgentBridge(IDEAgentBridge* bridge);

    /**
     * @brief Enable/disable ghost text feature
     * @param enabled true to enable ghost text
     */
    void setGhostTextEnabled(bool enabled);

    /**
     * @brief Get whether ghost text is enabled
     * @return true if enabled
     */
    bool isGhostTextEnabled() const { return m_ghostTextEnabled; }

    /**
     * @brief Set file type for context (cpp, python, java, etc)
     * @param fileType Language/file type
     */
    void setFileType(const std::string& fileType);

    /**
     * @brief Enable/disable automatic suggestions (periodic)
     * @param enabled If true, generate suggestions while typing
     */
    void setAutoSuggestions(bool enabled);

    /**
     * @brief Get current ghost text suggestion
     * @return Current suggestion if any
     */
    GhostTextSuggestion currentSuggestion() const { return m_currentSuggestion; }

    /**
     * @brief Trigger suggestion generation manually
     * @param context Optional context override
     */
    void triggerSuggestion(const GhostTextContext& context = GhostTextContext());

    /**
     * @brief Accept current ghost text suggestion
     * @return true if suggestion was accepted
     */
    bool acceptSuggestion();

    /**
     * @brief Reject/dismiss current ghost text
     */
    void dismissSuggestion();

    /**
     * @brief Clear any visible ghost text
     */
    void clearGhostText();

    /**
     * @brief Set the visual style for ghost text
     * @param font Font to use for ghost text display
     * @param color Color for ghost text
     */
    void setGhostTextStyle(const std::string& font, const uint32_t& color);

    /**
     * @brief Emitted when suggestion generation starts
     */
    void suggestionGenerating();

    /**
     * @brief Emitted when new suggestion is available
     * @param suggestion The generated suggestion
     */
    void suggestionAvailable(const GhostTextSuggestion& suggestion);

    /**
     * @brief Emitted when user accepts suggestion
     * @param text Accepted text
     */
    void suggestionAccepted(const std::string& text);

    /**
     * @brief Emitted when suggestion is dismissed
     */
    void suggestionDismissed();

    /**
     * @brief Emitted if suggestion generation fails
     * @param error Error message
     */
    void suggestionError(const std::string& error);

private:
    /**
     * @brief Handle editor key press events
     * @param event Key event
     */
    void onEditorKeyPressed(void*  event);

    /**
     * @brief Handle agent suggestion completion
     * @param result Agent's suggested action plan
     * @param elapsedMs Time taken
     */
    void onSuggestionGenerated(const void*& result, int elapsedMs);

    /**
     * @brief Periodic timer for automatic suggestions
     */
    void onAutoSuggestionTimer();

    /**
     * @brief Handle text edit completion
     * @param text Completed text
     */
    void onTextCompleted(const std::string& text);

private:
    /**
     * @brief Extract context from current editor state
     * @return Context suitable for LLM suggestion
     */
    GhostTextContext extractContext() const;

    /**
     * @brief Generate suggestion via agent
     * @param context Editor context
     */
    void generateSuggestion(const GhostTextContext& context);

    /**
     * @brief Parse LLM response into suggestion
     * @param response LLM response
     * @return Parsed suggestion
     */
    GhostTextSuggestion parseSuggestion(const void*& response) const;

    /**
     * @brief Render ghost text overlay in editor
     * @param text Ghost text to display
     * @param row Line to display at
     * @param column Column to display at
     */
    void renderGhostText(const std::string& text, int row, int column);

    /**
     * @brief Install event filter on editor
     */
    void installEventFilter();

    /**
     * @brief Get cursor position in editor
     * @return (row, column) pair
     */
    std::pair<int, int> getCursorPosition() const;

    /**
     * @brief Get text under cursor
     * @return Current word/token
     */
    std::string getWordUnderCursor() const;

    /**
     * @brief Qt event filter override
     */
    bool eventFilter(void* obj, QEvent* event) override;

    // ─────────────────────────────────────────────────────────────────────
    // Member Variables
    // ─────────────────────────────────────────────────────────────────────

    QPlainTextEdit* m_editor = nullptr;     ///< Target editor widget
    IDEAgentBridge* m_agentBridge = nullptr; ///< Agent communication

    bool m_ghostTextEnabled = true;         ///< Ghost text feature enabled
    bool m_autoSuggestions = false;         ///< Auto-generate suggestions
    std::string m_fileType = "cpp";             ///< Current file type

    GhostTextSuggestion m_currentSuggestion; ///< Current ghost text
    int m_ghostTextRow = -1;                ///< Where ghost text is displayed
    int m_ghostTextColumn = -1;

    std::string m_ghostTextFont;                  ///< Font for ghost text display
    uint32_t m_ghostTextColor;                ///< Color for ghost text (usually dim)

    void** m_autoSuggestionTimer = nullptr; ///< Timer for periodic suggestions

    // Threading
    std::atomic<bool> m_monitoringThreadActive{false};
    std::atomic<bool> m_contentDirty{false};
    std::thread m_monitorThread;
};

