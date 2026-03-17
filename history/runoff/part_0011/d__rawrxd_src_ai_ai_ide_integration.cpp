#include "ai_ide_integration.h"
#include <algorithm>
#include <windows.h>
#include <commctrl.h>

#include "../ide_constants.h"
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstdlib>

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

constexpr int ID_CHAT_HISTORY = 5001;
constexpr int ID_CHAT_INPUT = 5002;
constexpr int ID_CHAT_SEND = 5003;
constexpr int ID_AGENT_STATUS = 6001;
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
    const wchar_t* cls = L"RawrXDChatPanel";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = ChatPanelProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = cls;
    RegisterClassW(&wc);

    RECT rc;
    GetClientRect(m_main_window, &rc);
    int width = 400;
    int height = rc.bottom - rc.top;
    int x = (rc.right - rc.left) - width;
    int y = 0;

    m_chat_panel = CreateWindowExW(0, cls, L"AI Chat",
                                   WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                                   x, y, width, height, m_main_window, nullptr,
                                   wc.hInstance, this);

    if (!m_chat_panel) return;

    m_chat_history = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                     WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                                     6, 6, width - 12, height - 50, m_chat_panel,
                                     reinterpret_cast<HMENU>(ID_CHAT_HISTORY), wc.hInstance, nullptr);

    m_chat_input = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                   WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                   6, height - 36, width - 98, 26, m_chat_panel,
                                   reinterpret_cast<HMENU>(ID_CHAT_INPUT), wc.hInstance, nullptr);

    CreateWindowExW(0, L"BUTTON", L"Send",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    width - 86, height - 36, 80, 26, m_chat_panel,
                    reinterpret_cast<HMENU>(ID_CHAT_SEND), wc.hInstance, nullptr);

    HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(m_chat_history, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessage(m_chat_input, WM_SETFONT, (WPARAM)font, TRUE);
}

void AIIDEIntegration::CreateInlineSuggestionOverlay() {
    if (m_suggestion_overlay) return;

    m_suggestion_overlay = CreateWindowExW(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE, L"STATIC", L"",
                                           WS_CHILD, 0, 0, 1, 1,
                                           m_main_window, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (m_suggestion_overlay) {
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessage(m_suggestion_overlay, WM_SETFONT, (WPARAM)font, TRUE);
        ShowWindow(m_suggestion_overlay, SW_HIDE);
    }
}

void AIIDEIntegration::CreateEditPromptDialog() {
    // Dialog is created on demand via PromptForInstruction.
}

void AIIDEIntegration::CreateAgentPanel() {
    if (m_agent_panel) return;

    RECT rc;
    GetClientRect(m_main_window, &rc);
    int width = 300;
    int height = 120;
    int x = 10;
    int y = 10;

    m_agent_panel = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"Agent Status",
                                    WS_CHILD | WS_VISIBLE, x, y, width, height,
                                    m_main_window, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (!m_agent_panel) return;

    CreateWindowExW(0, L"STATIC", L"Idle",
                    WS_CHILD | WS_VISIBLE, 10, 25, width - 20, 80,
                    m_agent_panel, reinterpret_cast<HMENU>(ID_AGENT_STATUS),
                    GetModuleHandleW(nullptr), nullptr);
}

void AIIDEIntegration::CreateModelSelector() {
    // Model selector uses MessageBox-based selection in ShowModelSelector.
}

void AIIDEIntegration::ShowModelSelector() {
    if (!m_ai_engine || !m_ai_engine->IsInitialized()) {
        MessageBoxA(m_main_window, "AI engine not initialized", "Model Selector", MB_OK | MB_ICONWARNING);
        return;
    }

    auto models = m_ai_engine->ListAvailableModels();
    if (models.empty()) {
        MessageBoxA(m_main_window, "No models found", "Model Selector", MB_OK | MB_ICONINFORMATION);
        return;
    }

    size_t index = 0;
    if (!m_settings.preferred_model.empty()) {
        std::string target = m_settings.preferred_model;
        auto it = std::find(models.begin(), models.end(), target);
        if (it != models.end()) {
            index = (static_cast<size_t>(std::distance(models.begin(), it)) + 1) % models.size();
        }
    }

    m_settings.preferred_model = models[index];
    SwitchToModel(m_settings.preferred_model);

    std::string msg = "Selected model:\n" + m_settings.preferred_model;
    MessageBoxA(m_main_window, msg.c_str(), "Model Selector", MB_OK | MB_ICONINFORMATION);
}

void AIIDEIntegration::SwitchToModel(const std::string& model_name) {
    if (!m_ai_engine) return;

    ModelConfig cfg = m_ai_engine->GetCurrentModel();
    cfg.model_name = model_name;
    if (!m_settings.api_key.empty()) cfg.api_key = m_settings.api_key;
    if (!m_settings.api_endpoint.empty()) cfg.api_endpoint = m_settings.api_endpoint;

    if (!m_ai_engine->SwitchModel(cfg)) {
        MessageBoxA(m_main_window, "Failed to switch model", "Model Selector", MB_OK | MB_ICONERROR);
        return;
    }
    m_settings.preferred_model = model_name;
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
    std::string content = GetCurrentFileContent();
    if (content.find("#include") != std::string::npos || content.find("std::") != std::string::npos) return "cpp";
    if (content.find("def ") != std::string::npos || content.find("import ") != std::string::npos) return "python";
    if (content.find("function ") != std::string::npos || content.find("console.") != std::string::npos) return "javascript";
    return "text";
}

void AIIDEIntegration::InsertTextAtCursor(const std::string& text) {
    SendMessageA(m_editor_control, EM_REPLACESEL, TRUE, (LPARAM)text.c_str());
}

void AIIDEIntegration::ReplaceSelection(const std::string& new_text) {
    SendMessageA(m_editor_control, EM_REPLACESEL, TRUE, (LPARAM)new_text.c_str());
}

void AIIDEIntegration::ShowInlineSuggestion(const AISuggestion& suggestion) {
    if (!m_suggestion_overlay) {
        CreateInlineSuggestionOverlay();
    }
    if (!m_suggestion_overlay) return;

    DWORD sel_start = 0, sel_end = 0;
    SendMessage(m_editor_control, EM_GETSEL, (WPARAM)&sel_start, (LPARAM)&sel_end);
    LRESULT pos = SendMessage(m_editor_control, EM_POSFROMCHAR, sel_start, 0);
    int x = LOWORD(pos);
    int y = HIWORD(pos);

    POINT pt = { x, y };
    ClientToScreen(m_editor_control, &pt);
    ScreenToClient(m_main_window, &pt);

    SetWindowTextA(m_suggestion_overlay, suggestion.suggestion_text.c_str());
    MoveWindow(m_suggestion_overlay, pt.x + 10, pt.y + 20, 400, 24, TRUE);
    ShowWindow(m_suggestion_overlay, SW_SHOWNA);
    m_suggestion_visible = true;
}

void AIIDEIntegration::HideInlineSuggestion() {
    if (m_suggestion_overlay) {
        ShowWindow(m_suggestion_overlay, SW_HIDE);
    }
    m_suggestion_visible = false;
}

void AIIDEIntegration::AppendToChatHistory(const ChatMessage& message) {
    if (!m_chat_history) return;

    std::string role = (message.role == ChatMessage::Role::User) ? "User" :
                       (message.role == ChatMessage::Role::System) ? "System" : "Assistant";
    std::string line = "[" + role + "] " + message.content + "\r\n\r\n";

    SendMessageA(m_chat_history, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageA(m_chat_history, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
    UpdateChatUI();
}

void AIIDEIntegration::UpdateChatUI() {
    if (!m_chat_history) return;
    SendMessage(m_chat_history, EM_SCROLLCARET, 0, 0);
}

void AIIDEIntegration::UpdateAgentProgress(const AgentTask& task) {
    if (!m_agent_panel) return;
    HWND status = GetDlgItem(m_agent_panel, ID_AGENT_STATUS);
    if (!status) return;

    std::ostringstream oss;
    oss << "Status: "
        << (task.status == AgentTask::Status::Pending ? "Pending" :
            task.status == AgentTask::Status::Running ? "Running" :
            task.status == AgentTask::Status::Completed ? "Completed" : "Failed")
        << "\nStep: " << task.current_step << "/" << task.steps.size();

    std::string text = oss.str();
    SetWindowTextA(status, text.c_str());
}

void AIIDEIntegration::OnError(const std::string& error) {
    MessageBoxA(m_main_window, error.c_str(), "AI Error", MB_OK | MB_ICONERROR);
}

// ============================================================================
// Settings
// ============================================================================

void AIIDEIntegration::LoadSettings() {
    m_settings.auto_inline_completion = true;
    m_settings.inline_trigger_delay_ms = 500;
    m_settings.show_confidence_scores = false;
    m_settings.multi_line_suggestions = true;
    m_settings.preferred_model = "";
    m_settings.preferred_provider = ModelProvider::Local_GGUF;
    m_settings.api_key = "";
    m_settings.api_endpoint = "";

    const char* appdata = std::getenv("APPDATA");
    std::filesystem::path cfg_dir = appdata ? std::filesystem::path(appdata) / "RawrXD" : std::filesystem::current_path();
    std::filesystem::path cfg_path = cfg_dir / "ai_settings.ini";

    std::ifstream in(cfg_path);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "auto_inline_completion") m_settings.auto_inline_completion = (val == "1");
        else if (key == "inline_trigger_delay_ms") m_settings.inline_trigger_delay_ms = std::stoi(val);
        else if (key == "show_confidence_scores") m_settings.show_confidence_scores = (val == "1");
        else if (key == "multi_line_suggestions") m_settings.multi_line_suggestions = (val == "1");
        else if (key == "preferred_model") m_settings.preferred_model = val;
        else if (key == "preferred_provider") m_settings.preferred_provider = static_cast<ModelProvider>(std::stoi(val));
        else if (key == "api_key") m_settings.api_key = val;
        else if (key == "api_endpoint") m_settings.api_endpoint = val;
    }
}

void AIIDEIntegration::SaveSettings() {
    const char* appdata = std::getenv("APPDATA");
    std::filesystem::path cfg_dir = appdata ? std::filesystem::path(appdata) / "RawrXD" : std::filesystem::current_path();
    std::filesystem::create_directories(cfg_dir);
    std::filesystem::path cfg_path = cfg_dir / "ai_settings.ini";

    std::ofstream out(cfg_path, std::ios::trunc);
    if (!out.is_open()) return;

    out << "auto_inline_completion=" << (m_settings.auto_inline_completion ? "1" : "0") << "\n";
    out << "inline_trigger_delay_ms=" << m_settings.inline_trigger_delay_ms << "\n";
    out << "show_confidence_scores=" << (m_settings.show_confidence_scores ? "1" : "0") << "\n";
    out << "multi_line_suggestions=" << (m_settings.multi_line_suggestions ? "1" : "0") << "\n";
    out << "preferred_model=" << m_settings.preferred_model << "\n";
    out << "preferred_provider=" << static_cast<int>(m_settings.preferred_provider) << "\n";
    out << "api_key=" << m_settings.api_key << "\n";
    out << "api_endpoint=" << m_settings.api_endpoint << "\n";
}

void AIIDEIntegration::ShowSettingsDialog() {
    SaveSettings();

    const char* appdata = std::getenv("APPDATA");
    std::filesystem::path cfg_dir = appdata ? std::filesystem::path(appdata) / "RawrXD" : std::filesystem::current_path();
    std::filesystem::path cfg_path = cfg_dir / "ai_settings.ini";

    std::wstring msg = L"Settings saved to:\n" + cfg_path.wstring();
    MessageBoxW(m_main_window, msg.c_str(), L"AI Settings", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// Window Procedures
// ============================================================================

LRESULT CALLBACK AIIDEIntegration::ChatPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CREATE) {
        auto* create = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return 0;
    }

    auto* self = reinterpret_cast<AIIDEIntegration*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_CHAT_SEND && self) {
                wchar_t buffer[2048] = {0};
                GetWindowTextW(self->m_chat_input, buffer, 2048);
                if (wcslen(buffer) > 0) {
                    self->SendChatMessage(buffer);
                }
                return 0;
            }
            break;
        }
        case WM_SIZE: {
            if (!self) break;
            RECT rc;
            GetClientRect(hwnd, &rc);
            int width = rc.right - rc.left;
            int height = rc.bottom - rc.top;
            int input_height = 28;
            int button_width = 80;

            MoveWindow(self->m_chat_history, 6, 6, width - 12, height - input_height - 18, TRUE);
            MoveWindow(self->m_chat_input, 6, height - input_height - 6, width - button_width - 12, input_height, TRUE);
            MoveWindow(GetDlgItem(hwnd, ID_CHAT_SEND), width - button_width - 6, height - input_height - 6, button_width, input_height, TRUE);
            return 0;
        }
    }
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
