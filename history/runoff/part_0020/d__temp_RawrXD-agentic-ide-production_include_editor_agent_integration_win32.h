/**
 * @file editor_agent_integration_win32.h
 * @brief Win32 native implementation of agentic editor features
 *
 * Provides:
 * - Ghost text suggestions (TAB to trigger, Ctrl+ENTER to accept)
 * - Real-time code completions via agent
 * - Context-aware refactoring suggestions
 * - Right-click context menu with Copilot-style options
 *
 * @author RawrXD Agent Team
 * @version 1.0.0
 */

#pragma once

#ifdef _WIN32
#include <windows.h>
#include <string>
#include <functional>
#include <vector>
#include <memory>

// Forward declarations
class AgenticExecutor;
class AgenticTools;
class CodeSuggestionEngine;

/**
 * @struct GhostTextContext
 * @brief Context for ghost text generation
 */
struct GhostTextContext {
    std::string currentLine;                    ///< Current line being edited
    std::string previousLines;                  ///< Context from previous lines
    int cursorColumn = 0;                       ///< Cursor column in line
    std::string fileType = "cpp";               ///< File type (cpp, python, etc)
    int maxSuggestionLength = 200;              ///< Max chars for ghost text
};

/**
 * @struct GhostTextSuggestion
 * @brief Suggested completion text
 */
struct GhostTextSuggestion {
    std::string text;                           ///< Suggested code
    std::string explanation;                    ///< Why this suggestion
    int confidence = 100;                       ///< Confidence 0-100
    bool isComplete = false;                    ///< Is this a complete statement?
};

/**
 * @class EditorAgentIntegrationWin32
 * @brief Integrates agentic features into Win32 edit controls
 *
 * Handles:
 * - TAB key event → trigger ghost text
 * - Ctrl+ENTER key event → accept ghost text
 * - ESC key event → dismiss ghost text
 * - Periodic background suggestions
 * - Ghost text rendering overlay (using owner-draw or transparent window)
 * - Right-click context menu with AI-powered actions
 *
 * @note Works with standard EDIT and RichEdit controls
 * @note Non-blocking suggestion generation via agentic executor
 *
 * Usage:
 * @code
 * EditorAgentIntegrationWin32 integration(hwndEdit, agenticExecutor);
 * integration.setGhostTextEnabled(true);
 * integration.setFileType("cpp");
 * @endcode
 */
class EditorAgentIntegrationWin32 {
public:
    /**
     * @brief Constructor - attach to Win32 editor control
     * @param hwndEditor Target EDIT or RichEdit control
     * @param agenticExecutor Agentic execution engine for completions
     * @param agenticTools Tool registry for agent actions
     */
    explicit EditorAgentIntegrationWin32(
        HWND hwndEditor,
        AgenticExecutor* agenticExecutor,
        AgenticTools* agenticTools);

    /**
     * @brief Destructor
     */
    ~EditorAgentIntegrationWin32();

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
     * @brief Process keyboard input for ghost text triggers
     * @param message Window message
     * @param wParam WPARAM of message
     * @param lParam LPARAM of message
     * @return true if message was handled (don't process further)
     */
    bool handleKeyDown(UINT message, WPARAM wParam, LPARAM lParam);

    /**
     * @brief Show right-click context menu with AI actions
     * @param x Screen X coordinate
     * @param y Screen Y coordinate
     */
    void showContextMenu(int x, int y);

    /**
     * @brief Handle context menu command
     * @param commandId Menu command ID
     * @return true if handled
     */
    bool handleContextMenuCommand(int commandId);

    /**
     * @brief Update ghost text rendering (call from WM_PAINT or timer)
     */
    void updateGhostTextDisplay();

    /**
     * @brief Set callback for when suggestion is generated
     * @param callback Function to call with suggestion text
     */
    void setSuggestionCallback(std::function<void(const std::string&)> callback);

private:
    /**
     * @brief Extract context from current editor state
     * @return Context suitable for LLM suggestion
     */
    GhostTextContext extractContext() const;

    /**
     * @brief Generate suggestion via agentic executor
     * @param context Editor context
     */
    void generateSuggestion(const GhostTextContext& context);

    /**
     * @brief Render ghost text overlay on editor
     * @param text Ghost text to display
     * @param row Line to display at
     * @param column Column to display at
     */
    void renderGhostText(const std::string& text, int row, int column);

    /**
     * @brief Get cursor position in editor
     * @return (row, column) pair
     */
    std::pair<int, int> getCursorPosition() const;

    /**
     * @brief Get current line text
     * @return Current line string
     */
    std::string getCurrentLine() const;

    /**
     * @brief Get previous N lines for context
     * @param lineCount Number of lines to retrieve
     * @return Previous lines concatenated
     */
    std::string getPreviousLines(int lineCount = 10) const;

    /**
     * @brief Insert text at cursor position
     * @param text Text to insert
     */
    void insertTextAtCursor(const std::string& text);

    /**
     * @brief Handle code completion action
     */
    void actionCodeCompletion();

    /**
     * @brief Handle explain code action
     */
    void actionExplainCode();

    /**
     * @brief Handle refactor code action
     */
    void actionRefactorCode();

    /**
     * @brief Handle generate tests action
     */
    void actionGenerateTests();

    /**
     * @brief Handle find bugs action
     */
    void actionFindBugs();

    // ─────────────────────────────────────────────────────────────────────
    // Member Variables
    // ─────────────────────────────────────────────────────────────────────

    HWND m_hwndEditor;                          ///< Target editor control
    AgenticExecutor* m_agenticExecutor;         ///< Agent execution engine
    AgenticTools* m_agenticTools;               ///< Tool registry
    CodeSuggestionEngine* m_suggestionEngine;   ///< Code suggestion generation

    bool m_ghostTextEnabled = true;             ///< Ghost text feature enabled
    bool m_autoSuggestions = false;             ///< Auto-generate suggestions
    bool m_ghostTextVisible = false;            ///< Ghost text currently visible
    std::string m_fileType = "cpp";             ///< Current file type

    GhostTextSuggestion m_currentSuggestion;    ///< Current ghost text
    int m_ghostTextRow = -1;                    ///< Where ghost text is displayed
    int m_ghostTextColumn = -1;

    HFONT m_ghostTextFont = nullptr;            ///< Font for ghost text display
    COLORREF m_ghostTextColor = RGB(102, 102, 102); ///< Dim gray color

    HWND m_hwndGhostOverlay = nullptr;          ///< Transparent overlay window
    HMENU m_contextMenu = nullptr;              ///< Right-click context menu

    std::function<void(const std::string&)> m_suggestionCallback;

    // Context menu command IDs
    static constexpr int CMD_CODE_COMPLETION = 9001;
    static constexpr int CMD_EXPLAIN_CODE = 9002;
    static constexpr int CMD_REFACTOR_CODE = 9003;
    static constexpr int CMD_GENERATE_TESTS = 9004;
    static constexpr int CMD_FIND_BUGS = 9005;
    static constexpr int CMD_TOGGLE_GHOST_TEXT = 9010;
};

#endif // _WIN32
