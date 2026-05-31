#include <windows.h>
#include <commctrl.h>

#include "../agentic/slash_command_parser.hpp"
#include "../core/scoped_instructions_provider.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int IDC_CHAT_PREVIEW_EXECUTE = 7211;
constexpr int IDC_CHAT_PREVIEW_CANCEL = 7212;

std::wstring Utf8ToWide(const std::string& utf8)
{
    if (utf8.empty())
    {
        return {};
    }

    int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0)
    {
        len = MultiByteToWideChar(CP_ACP, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
        if (len <= 0)
        {
            return {};
        }
        std::wstring out(static_cast<size_t>(len), L'\0');
        if (MultiByteToWideChar(CP_ACP, 0, utf8.c_str(), static_cast<int>(utf8.size()), out.data(), len) <= 0)
        {
            return {};
        }
        return out;
    }

    std::wstring out(static_cast<size_t>(len), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), static_cast<int>(utf8.size()), out.data(), len) <= 0)
    {
        return {};
    }
    return out;
}

std::string WideToUtf8(const std::wstring& wide)
{
    if (wide.empty())
    {
        return {};
    }

    const int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0)
    {
        return {};
    }

    std::string out(static_cast<size_t>(len), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), out.data(), len, nullptr, nullptr) <= 0)
    {
        return {};
    }
    return out;
}

std::vector<std::string> ExtractFileRefs(const std::vector<std::string>& args)
{
    std::vector<std::string> refs;
    for (const auto& arg : args)
    {
        if (arg.rfind("#file:", 0) == 0 && arg.size() > 6)
        {
            refs.push_back(arg.substr(6));
        }
        else if (!arg.empty() && arg[0] != '-' && arg.find('=') == std::string::npos)
        {
            // Accept positional file-ish args for slash commands like /edit file1 file2.
            if (arg.find('.') != std::string::npos || arg.find('/') != std::string::npos || arg.find('\\') != std::string::npos)
            {
                refs.push_back(arg);
            }
        }
    }
    return refs;
}

class CommandPreviewPanel
{
public:
    void Create(HWND hwndParent, HINSTANCE hInstance)
    {
        parent_ = hwndParent;
        hinstance_ = hInstance;

        if (!parent_ || !IsWindow(parent_))
        {
            return;
        }

        if (panel_ && IsWindow(panel_))
        {
            return;
        }

        RECT rc{};
        GetClientRect(parent_, &rc);
        const int h = 156;
        const int y = static_cast<int>(std::max(0L, rc.bottom - 230));

        panel_ = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"STATIC",
            L"",
            WS_CHILD,
            5,
            y,
            static_cast<int>(std::max(320L, rc.right - 10)),
            h,
            parent_,
            nullptr,
            hinstance_,
            nullptr);

        if (!panel_)
        {
            return;
        }

        header_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 8, 8, rc.right - 30, 20, panel_, nullptr, hinstance_, nullptr);
        details_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 8, 30, rc.right - 30, 86, panel_, nullptr, hinstance_, nullptr);
        execute_btn_ = CreateWindowExW(0,
                           L"BUTTON",
                           L"Execute (Enter)",
                           WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                           static_cast<int>(std::max(8L, rc.right - 248)),
                           120,
                           112,
                           26,
                           panel_,
                           reinterpret_cast<HMENU>(IDC_CHAT_PREVIEW_EXECUTE),
                           hinstance_,
                           nullptr);
        cancel_btn_ = CreateWindowExW(0,
                          L"BUTTON",
                          L"Cancel (Esc)",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          static_cast<int>(std::max(126L, rc.right - 130)),
                          120,
                          112,
                          26,
                          panel_,
                          reinterpret_cast<HMENU>(IDC_CHAT_PREVIEW_CANCEL),
                          hinstance_,
                          nullptr);

        HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        if (header_) SendMessageW(header_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        if (details_) SendMessageW(details_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        if (execute_btn_) SendMessageW(execute_btn_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        if (cancel_btn_) SendMessageW(cancel_btn_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);

        ShowWindow(panel_, SW_HIDE);
    }

    void Destroy()
    {
        if (panel_ && IsWindow(panel_))
        {
            DestroyWindow(panel_);
        }
        panel_ = nullptr;
        header_ = nullptr;
        details_ = nullptr;
        execute_btn_ = nullptr;
        cancel_btn_ = nullptr;
    }

    void Hide()
    {
        if (panel_ && IsWindow(panel_))
        {
            ShowWindow(panel_, SW_HIDE);
        }
        last_route_.clear();
    }

    void Update(const wchar_t* input)
    {
        const std::wstring ws = input ? input : L"";
        const std::string utf8 = WideToUtf8(ws);

        if (utf8.empty() || utf8.front() != '/')
        {
            Hide();
            return;
        }

        const auto parsed = RawrXD::Agentic::SlashCommandParser::Parse(utf8);
        if (!parsed.valid)
        {
            RenderInvalid(parsed.error);
            return;
        }

        RenderValid(parsed, utf8);
    }

    bool Validate(const wchar_t* input, std::wstring& errorOut)
    {
        errorOut.clear();
        const std::wstring ws = input ? input : L"";
        const std::string utf8 = WideToUtf8(ws);
        if (utf8.empty() || utf8.front() != '/')
        {
            return true;
        }

        const auto parsed = RawrXD::Agentic::SlashCommandParser::Parse(utf8);
        if (!parsed.valid)
        {
            errorOut = L"Invalid slash command. Use /help.";
            return false;
        }

        const bool modifiesFiles = (parsed.command == "edit" || parsed.command == "write" || parsed.command == "refactor");
        const bool hasDryRun = utf8.find("--dry-run") != std::string::npos;
        const bool hasForce = utf8.find("--force") != std::string::npos;
        if (modifiesFiles && !hasDryRun && !hasForce)
        {
            errorOut = L"Command may modify files. Add --dry-run or --force.";
            return false;
        }

        return true;
    }

    std::wstring CurrentRouteLine() const
    {
        return last_route_;
    }

    bool HandleCommand(int controlId)
    {
        if (controlId == IDC_CHAT_PREVIEW_CANCEL)
        {
            Hide();
            return true;
        }
        if (controlId == IDC_CHAT_PREVIEW_EXECUTE)
        {
            return true;
        }
        return false;
    }

private:
    HWND parent_ = nullptr;
    HINSTANCE hinstance_ = nullptr;
    HWND panel_ = nullptr;
    HWND header_ = nullptr;
    HWND details_ = nullptr;
    HWND execute_btn_ = nullptr;
    HWND cancel_btn_ = nullptr;
    std::wstring last_route_;

    static std::string RouteForParsed(const RawrXD::Agentic::ParsedCommand& parsed)
    {
        const auto toolCall = parsed.ToToolCall();
        return toolCall.value("tool", std::string("agent.execute"));
    }

    void RenderInvalid(const std::string& error)
    {
        if (!panel_ || !header_ || !details_)
        {
            return;
        }

        SetWindowTextW(header_, L"Slash Command: invalid");
        const std::wstring werror = Utf8ToWide(error);
        SetWindowTextW(details_, (L"Error: " + werror).c_str());
        last_route_ = L"[Slash] invalid";
        ShowWindow(panel_, SW_SHOW);
    }

    void RenderValid(const RawrXD::Agentic::ParsedCommand& parsed, const std::string& rawInput)
    {
        if (!panel_ || !header_ || !details_)
        {
            return;
        }

        const std::string route = RouteForParsed(parsed);
        std::wstring header = L"/" + Utf8ToWide(parsed.command) + L" -> " + Utf8ToWide(route);
        SetWindowTextW(header_, header.c_str());
        last_route_ = L"Routing: " + Utf8ToWide(route);

        std::wostringstream details;

        const std::vector<std::string> fileRefs = ExtractFileRefs(parsed.args);
        if (!fileRefs.empty())
        {
            details << L"Files:";
            for (const auto& f : fileRefs)
            {
                details << L" " << Utf8ToWide(f);
            }
            details << L"\n";
        }

        const bool modifiesFiles = (parsed.command == "edit" || parsed.command == "write" || parsed.command == "refactor");
        const bool hasDryRun = rawInput.find("--dry-run") != std::string::npos;
        const bool hasForce = rawInput.find("--force") != std::string::npos;
        if (modifiesFiles && !hasDryRun && !hasForce)
        {
            details << L"Warning: add --dry-run or --force before execution.\n";
        }

        try
        {
            const std::string cwd = std::filesystem::current_path().string();
            auto& provider = RawrXD::Core::ScopedInstructionsProvider::instance();
            provider.setProjectRoot(cwd);
            auto resolved = provider.resolveForTargets(fileRefs, 800);
            if (!resolved.empty())
            {
                details << L"Scoped: " << Utf8ToWide(RawrXD::Core::ScopedInstructionsProvider::formatTelemetry(resolved)) << L"\n";
            }
        }
        catch (...)
        {
            // Non-fatal diagnostics panel. Ignore lookup exceptions.
        }

        details << L"Tokens(est): " << (rawInput.size() * 2 + 300);

        SetWindowTextW(details_, details.str().c_str());
        ShowWindow(panel_, SW_SHOW);
    }
};

CommandPreviewPanel g_previewPanel;

}  // namespace

extern "C" {

void CommandPreview_Create(HWND hwndParent, HINSTANCE hInstance)
{
    g_previewPanel.Create(hwndParent, hInstance);
}

void CommandPreview_Destroy()
{
    g_previewPanel.Destroy();
}

void CommandPreview_Update(const wchar_t* input)
{
    g_previewPanel.Update(input);
}

void CommandPreview_Hide()
{
    g_previewPanel.Hide();
}

bool CommandPreview_Validate(const wchar_t* input, wchar_t* errorBuffer, int errorBufferChars)
{
    std::wstring error;
    const bool ok = g_previewPanel.Validate(input, error);
    if (errorBuffer && errorBufferChars > 0)
    {
        errorBuffer[0] = L'\0';
        if (!error.empty())
        {
            wcsncpy_s(errorBuffer, static_cast<size_t>(errorBufferChars), error.c_str(), _TRUNCATE);
        }
    }
    return ok;
}

void CommandPreview_GetRouteLine(wchar_t* routeBuffer, int routeBufferChars)
{
    if (!routeBuffer || routeBufferChars <= 0)
    {
        return;
    }

    const std::wstring route = g_previewPanel.CurrentRouteLine();
    wcsncpy_s(routeBuffer, static_cast<size_t>(routeBufferChars), route.c_str(), _TRUNCATE);
}

bool CommandPreview_HandleCommand(int controlId)
{
    return g_previewPanel.HandleCommand(controlId);
}

}
