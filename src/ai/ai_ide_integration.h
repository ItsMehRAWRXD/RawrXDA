#pragma once

#include "ai_assistant_engine.h"
#include <windows.h>
#include <richedit.h>
#include <string>
#include <vector>
#include <memory>

namespace RawrXD {
namespace AI {

/**
 * IDE Integration for AI Assistant
 * Connects AI engine to IDE UI components (editor, chat panel, etc.)
 */
class AIIDEIntegration {
public:
    AIIDEIntegration(HWND main_window, HWND editor_control);
    ~AIIDEIntegration();

    bool Initialize(const ModelConfig& config);
    void Shutdown();

    // Inline Completion (triggers automatically as user types)
    void EnableInlineCompletion(bool enable);
    void TriggerInlineCompletion();  // Called on typing pause
    void AcceptCurrentSuggestion();
    void RejectCurrentSuggestion();
    void ShowNextAlternative();

    // Chat Panel Integration
    void OpenChatPanel();
    void CloseChatPanel();
    void SendChatMessage(const std::wstring& message);
    void InsertCodeFromChat(const std::string& code);

    // Inline Edit (Cursor Cmd+K style)
    void ShowEditPrompt();
    void ApplyInlineEdit(const std::wstring& instruction);
    void CancelInlineEdit();

    // Agent Mode
    void StartAgentMode();
    void ShowAgentPanel();
    void CreateAgentTask(const std::wstring& task_description);

    // Quick Actions (context menu commands)
    void ExplainSelectedCode();
    void RefactorSelectedCode();
    void GenerateTestsForCode();
    void GenerateDocsForCode();
    void FindBugsInCode();
    void OptimizeSelectedCode();

    // Model Management UI
    void ShowModelSelector();
    void SwitchToModel(const std::string& model_name);

    // Settings
    void ShowSettingsDialog();
    void SaveSettings();
    void LoadSettings();

private:
    // UI Components
    void CreateChatPanel();
    void CreateInlineSuggestionOverlay();
    void CreateEditPromptDialog();
    void CreateAgentPanel();
    void CreateModelSelector();

    // Editor Integration
    CodeContext GetCurrentCodeContext();
    std::string GetSelectedText();
    std::string GetCurrentFileContent();
    std::string GetCurrentLanguage();
    void InsertTextAtCursor(const std::string& text);
    void ReplaceSelection(const std::string& new_text);
    void ShowInlineSuggestion(const AISuggestion& suggestion);
    void HideInlineSuggestion();

    // Chat UI
    void AppendToChatHistory(const ChatMessage& message);
    void UpdateChatUI();

    // Agent UI
    void UpdateAgentProgress(const AgentTask& task);

    // Callbacks
    void OnInlineSuggestion(const AISuggestion& suggestion);
    void OnChatResponse(const ChatMessage& message);
    void OnEditComplete(const EditOperation& edit);
    void OnAgentUpdate(const AgentTask& task);
    void OnError(const std::string& error);

    // Window Procedures
    static LRESULT CALLBACK ChatPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditPromptProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK AgentPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // State
    HWND m_main_window;
    HWND m_editor_control;
    HWND m_chat_panel;
    HWND m_chat_input;
    HWND m_chat_history;
    HWND m_edit_prompt_dialog;
    HWND m_agent_panel;
    HWND m_suggestion_overlay;

    std::unique_ptr<AIAssistantEngine> m_ai_engine;
    std::string m_current_chat_session;
    AISuggestion m_current_suggestion;
    size_t m_alternative_index = 0;
    bool m_inline_completion_enabled;
    bool m_suggestion_visible;
    bool m_edit_pending = false;

    // Timers
    UINT_PTR m_inline_completion_timer;

    // Settings
    struct Settings {
        bool auto_inline_completion;
        int inline_trigger_delay_ms;
        bool show_confidence_scores;
        bool multi_line_suggestions;
        std::string preferred_model;
        ModelProvider preferred_provider;
        std::string api_key;
        std::string api_endpoint;
    } m_settings;
};

} // namespace AI
} // namespace RawrXD
