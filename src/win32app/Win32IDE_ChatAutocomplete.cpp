#include <windows.h>
#include <commctrl.h>

#include "../agentic/slash_command_parser.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::wstring Utf8ToWide(const std::string& utf8)
{
    if (utf8.empty())
    {
        return {};
    }

    const int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0)
    {
        return {};
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

std::string ToLowerCopy(std::string v)
{
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return v;
}

class ChatAutocomplete
{
public:
    void Attach(HWND hwndInput, HINSTANCE hInstance)
    {
        hwnd_input_ = hwndInput;
        hinstance_ = hInstance;
    }

    void Detach()
    {
        Hide();
        hwnd_input_ = nullptr;
    }

    void Hide()
    {
        if (hwnd_popup_ && IsWindow(hwnd_popup_))
        {
            ShowWindow(hwnd_popup_, SW_HIDE);
        }
        filtered_.clear();
        selected_ = 0;
    }

    bool IsVisible() const
    {
        return hwnd_popup_ && IsWindow(hwnd_popup_) && IsWindowVisible(hwnd_popup_);
    }

    bool HandleKeyDown(WPARAM key, bool ctrlDown)
    {
        if (!hwnd_input_ || !IsWindow(hwnd_input_))
        {
            return false;
        }

        if (ctrlDown && key == VK_SPACE)
        {
            BuildAndShow();
            return true;
        }

        if (!IsVisible())
        {
            return false;
        }

        if (filtered_.empty())
        {
            if (key == VK_ESCAPE)
            {
                Hide();
                return true;
            }
            return false;
        }

        if (key == VK_UP)
        {
            selected_ = (selected_ - 1 + static_cast<int>(filtered_.size())) % static_cast<int>(filtered_.size());
            RefreshSelectionOnly();
            return true;
        }
        if (key == VK_DOWN)
        {
            selected_ = (selected_ + 1) % static_cast<int>(filtered_.size());
            RefreshSelectionOnly();
            return true;
        }
        if (key == VK_ESCAPE)
        {
            Hide();
            return true;
        }
        if (key == VK_TAB || key == VK_RETURN)
        {
            ApplySelection();
            Hide();
            return true;
        }

        return false;
    }

    void OnInputChanged(const wchar_t* text)
    {
        if (!hwnd_input_ || !IsWindow(hwnd_input_))
        {
            return;
        }

        const std::wstring ws = text ? text : L"";
        const std::wstring token = ExtractLastToken(ws);
        if (token.empty() || token[0] != L'/')
        {
            Hide();
            return;
        }

        current_filter_ = token;
        BuildAndShow();
    }

private:
    HWND hwnd_input_ = nullptr;
    HINSTANCE hinstance_ = nullptr;
    HWND hwnd_popup_ = nullptr;
    HWND hwnd_list_ = nullptr;
    HWND hwnd_preview_ = nullptr;

    std::wstring current_filter_;
    std::vector<std::string> filtered_;
    int selected_ = 0;

    static std::wstring ExtractLastToken(const std::wstring& text)
    {
        const size_t lastSpace = text.find_last_of(L" \t\r\n");
        if (lastSpace == std::wstring::npos)
        {
            return text;
        }
        return text.substr(lastSpace + 1);
    }

    static std::string DescriptionFor(const std::string& command)
    {
        static const std::unordered_map<std::string, std::string> kDescriptions = {
            {"edit", "Multi-file edit plan"},
            {"terminal", "Run shell command"},
            {"search", "Search workspace"},
            {"read", "Read file"},
            {"write", "Write file"},
            {"refactor", "Refactor operation"},
            {"git", "Git operation"},
            {"help", "Show command help"},
        };
        const auto it = kDescriptions.find(command);
        return it != kDescriptions.end() ? it->second : std::string("Command");
    }

    static std::string RouteFor(const std::string& command)
    {
        if (command == "edit") return "propose_multifile_edits";
        if (command == "terminal") return "run_terminal";
        if (command == "search") return "semantic_search";
        if (command == "read") return "read_file";
        if (command == "write") return "write_file";
        if (command == "refactor") return "refactor_code";
        if (command == "git") return "git_operation";
        if (command == "help") return "help";
        return "agent.execute";
    }

    void EnsurePopup()
    {
        if (hwnd_popup_ && IsWindow(hwnd_popup_))
        {
            return;
        }

        if (!hwnd_input_ || !IsWindow(hwnd_input_))
        {
            return;
        }

        hwnd_popup_ = CreateWindowExW(
            WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            L"STATIC",
            L"",
            WS_POPUP | WS_BORDER,
            0,
            0,
            480,
            230,
            hwnd_input_,
            nullptr,
            hinstance_,
            nullptr);

        if (!hwnd_popup_)
        {
            return;
        }

        hwnd_list_ = CreateWindowExW(
            0,
            L"LISTBOX",
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
            8,
            8,
            464,
            150,
            hwnd_popup_,
            nullptr,
            hinstance_,
            nullptr);

        hwnd_preview_ = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE,
            8,
            165,
            464,
            56,
            hwnd_popup_,
            nullptr,
            hinstance_,
            nullptr);

        HFONT hFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        if (hwnd_list_)
        {
            SendMessageW(hwnd_list_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        }
        if (hwnd_preview_)
        {
            SendMessageW(hwnd_preview_, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        }
    }

    void BuildAndShow()
    {
        EnsurePopup();
        if (!hwnd_popup_ || !hwnd_list_ || !hwnd_input_)
        {
            return;
        }

        std::string filter = WideToUtf8(current_filter_);
        if (!filter.empty() && filter[0] == '/')
        {
            filter.erase(filter.begin());
        }
        filter = ToLowerCopy(filter);

        filtered_.clear();
        const auto commands = RawrXD::Agentic::SlashCommandParser::AvailableCommands();
        for (const auto& cmd : commands)
        {
            const std::string lowered = ToLowerCopy(cmd);
            if (filter.empty() || lowered.rfind(filter, 0) == 0)
            {
                filtered_.push_back(cmd);
            }
        }

        if (filtered_.empty())
        {
            Hide();
            return;
        }

        SendMessageW(hwnd_list_, LB_RESETCONTENT, 0, 0);
        for (const auto& cmd : filtered_)
        {
            const std::wstring row = Utf8ToWide(std::string("/") + cmd + " - " + DescriptionFor(cmd));
            SendMessageW(hwnd_list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.c_str()));
        }

        if (selected_ >= static_cast<int>(filtered_.size()))
        {
            selected_ = 0;
        }

        RefreshSelectionOnly();

        RECT inputRc{};
        GetWindowRect(hwnd_input_, &inputRc);
        const int width = static_cast<int>(std::max(420L, inputRc.right - inputRc.left));
        const int height = 230;
        const int x = inputRc.left;
        const int y = inputRc.top - height - 6;
        SetWindowPos(hwnd_popup_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    void RefreshSelectionOnly()
    {
        if (!hwnd_list_ || filtered_.empty())
        {
            return;
        }

        SendMessageW(hwnd_list_, LB_SETCURSEL, static_cast<WPARAM>(selected_), 0);
        if (hwnd_preview_)
        {
            const std::string selected = filtered_[static_cast<size_t>(selected_)];
            const std::string preview = "Route: /" + selected + " -> " + RouteFor(selected);
            const std::wstring wpreview = Utf8ToWide(preview);
            SetWindowTextW(hwnd_preview_, wpreview.c_str());
        }
    }

    void ApplySelection()
    {
        if (!hwnd_input_ || !IsWindow(hwnd_input_) || filtered_.empty())
        {
            return;
        }

        const int len = GetWindowTextLengthW(hwnd_input_);
        std::wstring text;
        text.resize(static_cast<size_t>(std::max(0, static_cast<int>(len))));
        if (len > 0)
        {
            GetWindowTextW(hwnd_input_, text.data(), len + 1);
        }

        size_t start = text.find_last_of(L" \t\r\n");
        if (start == std::wstring::npos)
        {
            start = 0;
        }
        else
        {
            start += 1;
        }

        const std::wstring completion = Utf8ToWide(std::string("/") + filtered_[static_cast<size_t>(selected_)] + " ");
        const std::wstring merged = text.substr(0, start) + completion;

        SetWindowTextW(hwnd_input_, merged.c_str());
        SendMessageW(hwnd_input_, EM_SETSEL, static_cast<WPARAM>(merged.size()), static_cast<LPARAM>(merged.size()));
    }
};

ChatAutocomplete g_chatAutocomplete;

}  // namespace

extern "C" {

void ChatAutocomplete_Attach(HWND hwndChatInput, HINSTANCE hInstance)
{
    g_chatAutocomplete.Attach(hwndChatInput, hInstance);
}

void ChatAutocomplete_Detach()
{
    g_chatAutocomplete.Detach();
}

void ChatAutocomplete_Hide()
{
    g_chatAutocomplete.Hide();
}

bool ChatAutocomplete_HandleKeyDown(WPARAM key, bool ctrlDown)
{
    return g_chatAutocomplete.HandleKeyDown(key, ctrlDown);
}

void ChatAutocomplete_OnInputChanged(const wchar_t* text)
{
    g_chatAutocomplete.OnInputChanged(text);
}

bool ChatAutocomplete_IsVisible()
{
    return g_chatAutocomplete.IsVisible();
}

}
