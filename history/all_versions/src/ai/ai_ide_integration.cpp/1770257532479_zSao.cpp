#include "ai_ide_integration.h"
#include "../ide_constants.h"
#include <sstream>
#include <fstream>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

namespace {
struct PromptState {
    HWND hwnd = nullptr;
    HWND edit = nullptr;
    bool done = false;
    bool accepted = false;
    std::wstring text;
};

LRESULT CALLBACK PromptWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<PromptState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            auto* create = reinterpret_cast<LPCREATESTRUCT>(lParam);
            state = reinterpret_cast<PromptState*>(create->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            state->hwnd = hwnd;

            CreateWindowW(L"STATIC", L"Edit instruction:", WS_CHILD | WS_VISIBLE,
                          10, 10, 360, 20, hwnd, nullptr, nullptr, nullptr);
            state->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                          10, 35, 360, 24, hwnd, nullptr, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                          210, 70, 75, 24, hwnd, reinterpret_cast<HMENU>(1), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
                          295, 70, 75, 24, hwnd, reinterpret_cast<HMENU>(2), nullptr, nullptr);
            return 0;
        }
        case WM_COMMAND: {
            if (!state) break;
            if (LOWORD(wParam) == 1) {
                wchar_t buffer[512] = {0};
                GetWindowTextW(state->edit, buffer, 512);
                state->text = buffer;
                state->accepted = true;
                state->done = true;
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == 2) {
                state->accepted = false;
                state->done = true;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            if (state) {
                state->accepted = false;
                state->done = true;
            }
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool PromptForInstruction(HWND owner, std::wstring& outText) {
    const wchar_t* cls = L"RawrXDEditPromptWindow";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = PromptWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = cls;
    RegisterClassW(&wc);

    PromptState state;
    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, cls, L"Edit Instruction",
                                WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                                400, 140, owner, nullptr, wc.hInstance, &state);
    if (!hwnd) return false;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (!state.done && GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (state.accepted) {
        outText = state.text;
        return true;
    }
    return false;
}
} // namespace

namespace RawrXD {
namespace AI {

AIIDEIntegration::AIIDEIntegration(HWND main_window, HWND editor_control)
    : m_main_window(main_window)
    , m_editor_control(editor_control)
    , m_chat_panel(nullptr)
    , m_chat_input(nullptr)
    , m_chat_history(nullptr)
    , m_edit_prompt_dialog(nullptr)
    , m_agent_panel(nullptr)
    , m_suggestion_overlay(nullptr)
    , m_inline_completion_enabled(true)
    , m_suggestion_visible(false)
    , m_inline_completion_timer(0)
{
    m_ai_engine = std::make_unique<AIAssistantEngine>();
}

AIIDEIntegration::~AIIDEIntegration() {
    Shutdown();
}

bool AIIDEIntegration::Initialize(const ModelConfig& config) {
    LoadSettings();

    ModelConfig final_config = config;
    if (!m_settings.preferred_model.empty()) {
        final_config.model_name = m_settings.preferred_model;
        final_config.provider = m_settings.preferred_provider;
    }

    if (!m_ai_engine->Initialize(final_config)) {
        MessageBoxA(m_main_window, "Failed to initialize AI engine", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // Set error callback
    m_ai_engine->SetErrorCallback([this](const std::string& error) {
        OnError(error);
    });

    CreateChatPanel();
    CreateInlineSuggestionOverlay();

    return true;
}

void AIIDEIntegration::Shutdown() {
    if (m_inline_completion_timer) {
        KillTimer(m_main_window, m_inline_completion_timer);
        m_inline_completion_timer = 0;
    }

    if (m_chat_panel) {
        DestroyWindow(m_chat_panel);
        m_chat_panel = nullptr;
    }

    if (m_suggestion_overlay) {
        DestroyWindow(m_suggestion_overlay);
        m_suggestion_overlay = nullptr;
    }

    if (m_ai_engine) {
        m_ai_engine->Shutdown();
    }

    SaveSettings();
}

// ============================================================================
// Inline Completion
// ============================================================================

void AIIDEIntegration::EnableInlineCompletion(bool enable) {
    m_inline_completion_enabled = enable;

    if (enable && !m_inline_completion_timer) {
        m_inline_completion_timer = SetTimer(m_main_window, 1001, 
                                            m_settings.inline_trigger_delay_ms, nullptr);
    } else if (!enable && m_inline_completion_timer) {
        KillTimer(m_main_window, m_inline_completion_timer);
        m_inline_completion_timer = 0;
    }
}

void AIIDEIntegration::TriggerInlineCompletion() {
    if (!m_inline_completion_enabled || !m_ai_engine->IsInitialized()) {
        return;
    }

    CodeContext context = GetCurrentCodeContext();
    
    m_ai_engine->RequestInlineCompletion(context, [this](const AISuggestion& suggestion) {
        OnInlineSuggestion(suggestion);
    });
}

void AIIDEIntegration::OnInlineSuggestion(const AISuggestion& suggestion) {
    m_current_suggestion = suggestion;
    
    if (!suggestion.suggestion_text.empty()) {
        ShowInlineSuggestion(suggestion);
    }
}

void AIIDEIntegration::AcceptCurrentSuggestion() {
    if (m_suggestion_visible && !m_current_suggestion.suggestion_text.empty()) {
        InsertTextAtCursor(m_current_suggestion.suggestion_text);
        m_ai_engine->AcceptSuggestion(m_current_suggestion);
        HideInlineSuggestion();
    }
}

void AIIDEIntegration::RejectCurrentSuggestion() {
    if (m_suggestion_visible) {
        m_ai_engine->RejectSuggestion(m_current_suggestion);
        HideInlineSuggestion();
    }
}

void AIIDEIntegration::ShowNextAlternative() {
    if (m_current_suggestion.alternatives.empty()) {
        return;
    }
    m_alternative_index = (m_alternative_index + 1) % m_current_suggestion.alternatives.size();
    m_current_suggestion.suggestion_text = m_current_suggestion.alternatives[m_alternative_index];
    ShowInlineSuggestion(m_current_suggestion);
}

// ============================================================================
// Chat Panel
// ============================================================================

void AIIDEIntegration::OpenChatPanel() {
    if (m_chat_panel) {
        ShowWindow(m_chat_panel, SW_SHOW);
        SetFocus(m_chat_input);
    } else {
        CreateChatPanel();
    }

    if (m_current_chat_session.empty()) {
        m_current_chat_session = m_ai_engine->StartChatSession();
    }
}

void AIIDEIntegration::CloseChatPanel() {
    if (m_chat_panel) {
        ShowWindow(m_chat_panel, SW_HIDE);
    }
}

void AIIDEIntegration::SendChatMessage(const std::wstring& message) {
    if (!m_ai_engine->IsInitialized() || message.empty()) {
        return;
    }

    // Convert to UTF-8
    int len = WideCharToMultiByte(CP_UTF8, 0, message.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8_message(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, message.c_str(), -1, &utf8_message[0], len, nullptr, nullptr);

    // Display user message immediately
    ChatMessage user_msg;
    user_msg.role = ChatMessage::Role::User;
    user_msg.content = utf8_message;
    AppendToChatHistory(user_msg);

    // Get code context
    CodeContext context = GetCurrentCodeContext();

    // Send to AI engine
    m_ai_engine->SendChatMessage(m_current_chat_session, utf8_message, context,
        [this](const ChatMessage& response) {
            OnChatResponse(response);
        });

    // Clear input
    SetWindowTextW(m_chat_input, L"");
}

void AIIDEIntegration::OnChatResponse(const ChatMessage& message) {
    AppendToChatHistory(message);
}

void AIIDEIntegration::InsertCodeFromChat(const std::string& code) {
    InsertTextAtCursor(code);
}

// ============================================================================
// Inline Edit (Cursor Cmd+K style)
// ============================================================================

void AIIDEIntegration::ShowEditPrompt() {
    std::string selected = GetSelectedText();
    if (selected.empty()) {
        MessageBoxA(m_main_window, "Please select code to edit", "No Selection", MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::wstring instruction;
    if (PromptForInstruction(m_main_window, instruction) && !instruction.empty()) {
        ApplyInlineEdit(instruction);
    }
}

void AIIDEIntegration::ApplyInlineEdit(const std::wstring& instruction) {
    int len = WideCharToMultiByte(CP_UTF8, 0, instruction.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8_instruction(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, instruction.c_str(), -1, &utf8_instruction[0], len, nullptr, nullptr);

    CodeContext context = GetCurrentCodeContext();
    
    m_edit_pending = true;
    m_ai_engine->RequestEdit(utf8_instruction, context, [this](const EditOperation& edit) {
        OnEditComplete(edit);
    });
}

void AIIDEIntegration::OnEditComplete(const EditOperation& edit) {
    if (!m_edit_pending) {
        return;
    }
    m_edit_pending = false;
    if (!edit.new_text.empty()) {
        ReplaceSelection(edit.new_text);
        m_ai_engine->ApplyEdit(edit);
    }
}

void AIIDEIntegration::CancelInlineEdit() {
    m_edit_pending = false;
}

// ============================================================================
// Quick Actions
// ============================================================================

void AIIDEIntegration::ExplainSelectedCode() {
    std::string code = GetSelectedText();
    if (code.empty()) {
        MessageBoxA(m_main_window, "Please select code to explain", "No Selection", MB_OK);
        return;
    }

    std::string language = GetCurrentLanguage();
    std::string explanation = m_ai_engine->ExplainCode(code, language);

    // Show in chat panel
    OpenChatPanel();
    ChatMessage msg;
    msg.role = ChatMessage::Role::Assistant;
    msg.content = explanation;
    AppendToChatHistory(msg);
}

void AIIDEIntegration::RefactorSelectedCode() {
    std::string code = GetSelectedText();
    if (code.empty()) return;

    std::string language = GetCurrentLanguage();
    auto suggestions = m_ai_engine->SuggestRefactorings(code, language);

    // Show suggestions in chat
    OpenChatPanel();
    std::stringstream ss;
    ss << "Refactoring suggestions:\n\n";
    for (size_t i = 0; i < suggestions.size(); ++i) {
        ss << (i + 1) << ". " << suggestions[i] << "\n";
    }

    ChatMessage msg;
    msg.role = ChatMessage::Role::Assistant;
    msg.content = ss.str();
    AppendToChatHistory(msg);
}

void AIIDEIntegration::GenerateTestsForCode() {
    std::string code = GetSelectedText();
    if (code.empty()) return;

    std::string language = GetCurrentLanguage();
    std::string tests = m_ai_engine->GenerateTests(code, language);

    InsertTextAtCursor("\n\n// Generated Tests:\n" + tests);
}

void AIIDEIntegration::GenerateDocsForCode() {
    std::string code = GetSelectedText();
    if (code.empty()) return;

    std::string language = GetCurrentLanguage();
    std::string docs = m_ai_engine->GenerateDocumentation(code, language);

    ReplaceSelection(docs);
}

void AIIDEIntegration::FindBugsInCode() {
    std::string code = GetSelectedText();
    if (code.empty()) {
        code = GetCurrentFileContent();
    }

    std::string language = GetCurrentLanguage();
    auto bugs = m_ai_engine->FindBugs(code, language);

    OpenChatPanel();
    std::stringstream ss;
    ss << "Potential issues found:\n\n";
    for (size_t i = 0; i < bugs.size(); ++i) {
        ss << (i + 1) << ". " << bugs[i] << "\n";
    }

    ChatMessage msg;
    msg.role = ChatMessage::Role::Assistant;
    msg.content = ss.str();
    AppendToChatHistory(msg);
}

void AIIDEIntegration::OptimizeSelectedCode() {
    std::string code = GetSelectedText();
    if (code.empty()) return;

    std::string language = GetCurrentLanguage();
    std::string optimized = m_ai_engine->OptimizeCode(code, language);

    ReplaceSelection(optimized);
}

// ============================================================================
// Agent Mode
// ============================================================================

void AIIDEIntegration::StartAgentMode() {
    ShowAgentPanel();
}

void AIIDEIntegration::ShowAgentPanel() {
    if (!m_agent_panel) {
        CreateAgentPanel();
    }
    ShowWindow(m_agent_panel, SW_SHOW);
}

void AIIDEIntegration::CreateAgentTask(const std::wstring& task_description) {
    int len = WideCharToMultiByte(CP_UTF8, 0, task_description.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8_task(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, task_description.c_str(), -1, &utf8_task[0], len, nullptr, nullptr);

    CodeContext context = GetCurrentCodeContext();
    std::string task_id = m_ai_engine->CreateAgentTask(utf8_task, context);

    m_ai_engine->StartAgent(task_id, [this](const AgentTask& task) {
        OnAgentUpdate(task);
    });
}

void AIIDEIntegration::OnAgentUpdate(const AgentTask& task) {
    UpdateAgentProgress(task);
}

// ============================================================================
// UI Creation
// ============================================================================

void AIIDEIntegration::CreateChatPanel() {
    // TODO: Create dockable chat panel with history and input
    // For now, create a simple child window
}

void AIIDEIntegration::CreateInlineSuggestionOverlay() {
    // TODO: Create transparent overlay window for inline suggestions
}

void AIIDEIntegration::CreateEditPromptDialog() {
    // TODO: Create modal dialog for edit instructions
}

void AIIDEIntegration::CreateAgentPanel() {
    // TODO: Create agent task panel with progress tracking
}

void AIIDEIntegration::CreateModelSelector() {
    // TODO: Create model selection dropdown/dialog
}

// ============================================================================
// Editor Integration
// ============================================================================

CodeContext AIIDEIntegration::GetCurrentCodeContext() {
    CodeContext context;
    context.file_content = GetCurrentFileContent();
    context.selected_text = GetSelectedText();
    context.language = GetCurrentLanguage();
    
    // Get cursor position
    DWORD sel_start, sel_end;
    SendMessage(m_editor_control, EM_GETSEL, (WPARAM)&sel_start, (LPARAM)&sel_end);
    
    // Convert character position to line/column
    context.cursor_line = (int)SendMessage(m_editor_control, EM_LINEFROMCHAR, sel_start, 0);
    int line_start = (int)SendMessage(m_editor_control, EM_LINEINDEX, context.cursor_line, 0);
    context.cursor_column = (int)(sel_start - line_start);

    return context;
}

std::string AIIDEIntegration::GetSelectedText() {
    CHARRANGE range;
    SendMessage(m_editor_control, EM_EXGETSEL, 0, (LPARAM)&range);
    
    if (range.cpMin == range.cpMax) {
        return "";
    }

    int len = range.cpMax - range.cpMin + 1;
    std::vector<char> buffer(len);
    
    TEXTRANGEA tr;
    tr.chrg = range;
    tr.lpstrText = buffer.data();
    SendMessage(m_editor_control, EM_GETTEXTRANGE, 0, (LPARAM)&tr);

    return std::string(buffer.data());
}

std::string AIIDEIntegration::GetCurrentFileContent() {
    int len = GetWindowTextLengthA(m_editor_control);
    std::vector<char> buffer(len + 1);
    GetWindowTextA(m_editor_control, buffer.data(), len + 1);
    return std::string(buffer.data());
}

std::string AIIDEIntegration::GetCurrentLanguage() {
    // TODO: Detect language from file extension
    return "cpp";
}

void AIIDEIntegration::InsertTextAtCursor(const std::string& text) {
    SendMessageA(m_editor_control, EM_REPLACESEL, TRUE, (LPARAM)text.c_str());
}

void AIIDEIntegration::ReplaceSelection(const std::string& new_text) {
    SendMessageA(m_editor_control, EM_REPLACESEL, TRUE, (LPARAM)new_text.c_str());
}

void AIIDEIntegration::ShowInlineSuggestion(const AISuggestion& suggestion) {
    // TODO: Display suggestion overlay at cursor position
    m_suggestion_visible = true;
}

void AIIDEIntegration::HideInlineSuggestion() {
    m_suggestion_visible = false;
}

void AIIDEIntegration::AppendToChatHistory(const ChatMessage& message) {
    // TODO: Append formatted message to chat history control
}

void AIIDEIntegration::UpdateChatUI() {
    // TODO: Refresh chat UI
}

void AIIDEIntegration::UpdateAgentProgress(const AgentTask& task) {
    // TODO: Update agent panel with task progress
}

void AIIDEIntegration::OnError(const std::string& error) {
    MessageBoxA(m_main_window, error.c_str(), "AI Error", MB_OK | MB_ICONERROR);
}

// ============================================================================
// Settings
// ============================================================================

void AIIDEIntegration::LoadSettings() {
    // TODO: Load from config file
    m_settings.auto_inline_completion = true;
    m_settings.inline_trigger_delay_ms = 500;
    m_settings.show_confidence_scores = false;
    m_settings.multi_line_suggestions = true;
    m_settings.preferred_model = "";
    m_settings.preferred_provider = ModelProvider::Local_GGUF;
}

void AIIDEIntegration::SaveSettings() {
    // TODO: Save to config file
}

void AIIDEIntegration::ShowSettingsDialog() {
    // TODO: Show settings dialog
}

// ============================================================================
// Window Procedures
// ============================================================================

LRESULT CALLBACK AIIDEIntegration::ChatPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK AIIDEIntegration::EditPromptProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK AIIDEIntegration::AgentPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace AI
} // namespace RawrXD
