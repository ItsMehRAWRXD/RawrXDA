#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#endif

namespace RawrXD {

/**
 * CommandPalette - A VS Code style command palette for Win32 GUI
 * 
 * Features:
 * - Fuzzy search for commands
 * - Keyboard shortcuts display
 * - Categories and grouping
 * - Recent commands tracking
 * - Integration with AgenticEngine
 */
class CommandPalette {
public:
    struct PaletteCommand {
        std::string id;
        std::string label;
        std::string description;
        std::string category;
        std::string shortcut;
        std::function<void()> handler;
        int commandId; // For Win32 WM_COMMAND
    };
    
    struct PaletteConfig {
        int maxVisibleItems = 10;
        bool showShortcuts = true;
        bool showCategories = true;
        bool trackRecent = true;
        int maxRecentItems = 10;
    };

    CommandPalette() = default;
    
    // Register a command
    void registerCommand(const PaletteCommand& cmd) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_commands[cmd.id] = cmd;
        m_allCommands.push_back(cmd.id);
    }
    
    // Bulk register commands
    void registerCommands(const std::vector<PaletteCommand>& commands) {
        for (const auto& cmd : commands) {
            registerCommand(cmd);
        }
    }
    
    // Search commands with fuzzy matching
    std::vector<PaletteCommand> search(const std::string& query) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (query.empty()) {
            // Return recent commands first, then all commands
            std::vector<PaletteCommand> result;
            for (const auto& id : m_recentCommands) {
                auto it = m_commands.find(id);
                if (it != m_commands.end()) {
                    result.push_back(it->second);
                }
            }
            
            // Add remaining commands up to max visible
            for (const auto& id : m_allCommands) {
                if (result.size() >= static_cast<size_t>(m_config.maxVisibleItems)) break;
                if (std::find(m_recentCommands.begin(), m_recentCommands.end(), id) == m_recentCommands.end()) {
                    auto it = m_commands.find(id);
                    if (it != m_commands.end()) {
                        result.push_back(it->second);
                    }
                }
            }
            return result;
        }
        
        // Fuzzy search
        std::vector<std::pair<int, PaletteCommand>> scored;
        std::string lowerQuery = toLower(query);
        
        for (const auto& [id, cmd] : m_commands) {
            int score = fuzzyScore(lowerQuery, toLower(cmd.label));
            if (score > 0) {
                // Boost score for category match
                if (toLower(cmd.category).find(lowerQuery) != std::string::npos) {
                    score += 50;
                }
                // Boost score for recent commands
                if (std::find(m_recentCommands.begin(), m_recentCommands.end(), id) != m_recentCommands.end()) {
                    score += 25;
                }
                scored.emplace_back(score, cmd);
            }
        }
        
        // Sort by score descending
        std::sort(scored.begin(), scored.end(), 
            [](const auto& a, const auto& b) { return a.first > b.first; });
        
        std::vector<PaletteCommand> result;
        for (const auto& [score, cmd] : scored) {
            if (result.size() >= static_cast<size_t>(m_config.maxVisibleItems)) break;
            result.push_back(cmd);
        }
        
        return result;
    }
    
    // Execute a command by ID
    bool execute(const std::string& id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_commands.find(id);
        if (it == m_commands.end()) return false;
        
        // Track as recent
        if (m_config.trackRecent) {
            m_recentCommands.erase(
                std::remove(m_recentCommands.begin(), m_recentCommands.end(), id),
                m_recentCommands.end());
            m_recentCommands.insert(m_recentCommands.begin(), id);
            if (m_recentCommands.size() > static_cast<size_t>(m_config.maxRecentItems)) {
                m_recentCommands.pop_back();
            }
        }
        
        // Execute the handler
        if (it->second.handler) {
            it->second.handler();
            return true;
        }
        
        return false;
    }
    
    // Get all registered commands
    std::vector<PaletteCommand> getAllCommands() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<PaletteCommand> result;
        for (const auto& [id, cmd] : m_commands) {
            result.push_back(cmd);
        }
        return result;
    }
    
    // Get commands by category
    std::vector<PaletteCommand> getByCategory(const std::string& category) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<PaletteCommand> result;
        for (const auto& [id, cmd] : m_commands) {
            if (cmd.category == category) {
                result.push_back(cmd);
            }
        }
        return result;
    }
    
    // Configure the palette
    void configure(const PaletteConfig& config) {
        m_config = config;
    }

#ifdef _WIN32
    // Win32 specific: Create the palette window
    HWND createWindow(HWND parent, HINSTANCE hInstance) {
        // Register window class
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = PaletteWndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"RawrXDCommandPalette";
        wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassEx(&wc);
        
        // Get parent window rect
        RECT parentRect;
        GetClientRect(parent, &parentRect);
        
        int width = 600;
        int height = 400;
        int x = (parentRect.right - width) / 2;
        int y = 50;
        
        // Create palette window
        m_hwnd = CreateWindowEx(
            WS_EX_TOPMOST,
            L"RawrXDCommandPalette",
            L"Command Palette",
            WS_POPUP | WS_BORDER,
            x, y, width, height,
            parent,
            nullptr,
            hInstance,
            this
        );
        
        if (m_hwnd) {
            // Create search edit control
            m_hwndSearch = CreateWindowEx(
                0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                10, 10, width - 20, 30,
                m_hwnd,
                (HMENU)IDC_PALETTE_SEARCH,
                hInstance,
                nullptr
            );
            
            // Create listbox for results
            m_hwndList = CreateWindowEx(
                0, L"LISTBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED,
                10, 50, width - 20, height - 60,
                m_hwnd,
                (HMENU)IDC_PALETTE_LIST,
                hInstance,
                nullptr
            );
            
            // Set fonts
            HFONT hFont = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            SendMessage(m_hwndSearch, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(m_hwndList, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            // Set item height for listbox
            SendMessage(m_hwndList, LB_SETITEMHEIGHT, 0, 40);
        }
        
        return m_hwnd;
    }
    
    // Show the palette
    void show() {
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_SHOW);
            SetFocus(m_hwndSearch);
            updateList("");
        }
    }
    
    // Hide the palette
    void hide() {
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_HIDE);
            SetWindowText(m_hwndSearch, L"");
        }
    }
    
    // Toggle visibility
    void toggle() {
        if (m_hwnd) {
            if (IsWindowVisible(m_hwnd)) {
                hide();
            } else {
                show();
            }
        }
    }
    
    // Check if visible
    bool isVisible() const {
        return m_hwnd && IsWindowVisible(m_hwnd);
    }
    
private:
    static constexpr int IDC_PALETTE_SEARCH = 1001;
    static constexpr int IDC_PALETTE_LIST = 1002;
    
    HWND m_hwnd = nullptr;
    HWND m_hwndSearch = nullptr;
    HWND m_hwndList = nullptr;
    std::vector<std::string> m_currentResults;
    
    static LRESULT CALLBACK PaletteWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        CommandPalette* self = nullptr;
        
        if (msg == WM_CREATE) {
            CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            self = reinterpret_cast<CommandPalette*>(cs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<CommandPalette*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }
        
        switch (msg) {
            case WM_COMMAND:
                if (self) {
                    if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == IDC_PALETTE_SEARCH) {
                        wchar_t buffer[256];
                        GetWindowText(self->m_hwndSearch, buffer, 256);
                        std::wstring wquery(buffer);
                        std::string query(wquery.begin(), wquery.end());
                        self->updateList(query);
                    }
                    else if (HIWORD(wParam) == LBN_DBLCLK && LOWORD(wParam) == IDC_PALETTE_LIST) {
                        self->executeSelected();
                    }
                }
                break;
                
            case WM_KEYDOWN:
                if (wParam == VK_ESCAPE) {
                    if (self) self->hide();
                    return 0;
                }
                else if (wParam == VK_RETURN) {
                    if (self) {
                        self->executeSelected();
                        return 0;
                    }
                }
                else if (wParam == VK_DOWN) {
                    if (self && self->m_hwndList) {
                        int sel = (int)SendMessage(self->m_hwndList, LB_GETCURSEL, 0, 0);
                        int count = (int)SendMessage(self->m_hwndList, LB_GETCOUNT, 0, 0);
                        if (sel < count - 1) {
                            SendMessage(self->m_hwndList, LB_SETCURSEL, sel + 1, 0);
                        }
                        return 0;
                    }
                }
                else if (wParam == VK_UP) {
                    if (self && self->m_hwndList) {
                        int sel = (int)SendMessage(self->m_hwndList, LB_GETCURSEL, 0, 0);
                        if (sel > 0) {
                            SendMessage(self->m_hwndList, LB_SETCURSEL, sel - 1, 0);
                        }
                        return 0;
                    }
                }
                break;
                
            case WM_DRAWITEM:
                if (self && wParam == IDC_PALETTE_LIST) {
                    return self->drawListItem(reinterpret_cast<DRAWITEMSTRUCT*>(lParam));
                }
                break;
                
            case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                RECT rect;
                GetClientRect(hwnd, &rect);
                FillRect(hdc, &rect, CreateSolidBrush(RGB(30, 30, 30)));
                EndPaint(hwnd, &ps);
                return 0;
            }
                
            case WM_DESTROY:
                return 0;
        }
        
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    void updateList(const std::string& query) {
        if (!m_hwndList) return;
        
        SendMessage(m_hwndList, LB_RESETCONTENT, 0, 0);
        m_currentResults.clear();
        
        auto results = search(query);
        for (const auto& cmd : results) {
            m_currentResults.push_back(cmd.id);
            std::wstring wlabel(cmd.label.begin(), cmd.label.end());
            SendMessage(m_hwndList, LB_ADDSTRING, 0, (LPARAM)wlabel.c_str());
        }
        
        if (!m_currentResults.empty()) {
            SendMessage(m_hwndList, LB_SETCURSEL, 0, 0);
        }
    }
    
    void executeSelected() {
        int sel = (int)SendMessage(m_hwndList, LB_GETCURSEL, 0, 0);
        if (sel >= 0 && sel < static_cast<int>(m_currentResults.size())) {
            hide();
            execute(m_currentResults[sel]);
        }
    }
    
    LRESULT drawListItem(DRAWITEMSTRUCT* dis) {
        if (dis->itemID == (UINT)-1) return 0;
        
        HDC hdc = dis->hDC;
        RECT rect = dis->rcItem;
        
        // Background
        COLORREF bgColor = (dis->itemState & ODS_SELECTED) ? RGB(0, 120, 215) : RGB(45, 45, 45);
        COLORREF textColor = RGB(255, 255, 255);
        COLORREF descColor = RGB(180, 180, 180);
        
        HBRUSH hBrush = CreateSolidBrush(bgColor);
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);
        
        if (dis->itemID < m_currentResults.size()) {
            auto it = m_commands.find(m_currentResults[dis->itemID]);
            if (it != m_commands.end()) {
                const auto& cmd = it->second;
                
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, textColor);
                
                // Draw label
                std::wstring wlabel(cmd.label.begin(), cmd.label.end());
                rect.left += 10;
                rect.top += 5;
                DrawText(hdc, wlabel.c_str(), -1, &rect, DT_LEFT | DT_TOP);
                
                // Draw description
                SetTextColor(hdc, descColor);
                std::wstring wdesc(cmd.description.begin(), cmd.description.end());
                rect.top += 18;
                DrawText(hdc, wdesc.c_str(), -1, &rect, DT_LEFT | DT_TOP);
                
                // Draw shortcut if available
                if (m_config.showShortcuts && !cmd.shortcut.empty()) {
                    SetTextColor(hdc, RGB(100, 180, 100));
                    std::wstring wshortcut(cmd.shortcut.begin(), cmd.shortcut.end());
                    RECT shortcutRect = dis->rcItem;
                    shortcutRect.right -= 10;
                    shortcutRect.top += 10;
                    DrawText(hdc, wshortcut.c_str(), -1, &shortcutRect, DT_RIGHT | DT_TOP);
                }
            }
        }
        
        return TRUE;
    }
#endif

    mutable std::mutex m_mutex;
    PaletteConfig m_config;
    std::unordered_map<std::string, PaletteCommand> m_commands;
    std::vector<std::string> m_allCommands;
    std::vector<std::string> m_recentCommands;
    
    // Fuzzy matching score
    static int fuzzyScore(const std::string& query, const std::string& target) {
        if (query.empty()) return 1;
        if (target.empty()) return 0;
        
        // Exact match
        if (target == query) return 100;
        
        // Contains
        if (target.find(query) != std::string::npos) return 75;
        
        // Prefix match
        if (target.find(query) == 0) return 90;
        
        // Character-by-character fuzzy
        int score = 0;
        size_t qi = 0;
        for (size_t ti = 0; ti < target.size() && qi < query.size(); ++ti) {
            if (target[ti] == query[qi]) {
                score += 10;
                qi++;
            }
        }
        
        return (qi == query.size()) ? score : 0;
    }
    
    static std::string toLower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }
};

// Factory function to register all IDE commands
inline void registerIDECommands(CommandPalette& palette, std::function<void(int)> commandCallback) {
    std::vector<CommandPalette::PaletteCommand> commands = {
        // File operations
        {"file.new", "New File", "Create a new file", "File", "Ctrl+N", [=]{ commandCallback(1001); }, 1001},
        {"file.open", "Open File", "Open an existing file", "File", "Ctrl+O", [=]{ commandCallback(1002); }, 1002},
        {"file.save", "Save File", "Save the current file", "File", "Ctrl+S", [=]{ commandCallback(1003); }, 1003},
        {"file.saveAs", "Save As", "Save file with a new name", "File", "Ctrl+Shift+S", [=]{ commandCallback(1004); }, 1004},
        
        // Edit operations
        {"edit.undo", "Undo", "Undo last action", "Edit", "Ctrl+Z", [=]{ commandCallback(2001); }, 2001},
        {"edit.redo", "Redo", "Redo last undone action", "Edit", "Ctrl+Y", [=]{ commandCallback(2002); }, 2002},
        {"edit.cut", "Cut", "Cut selection", "Edit", "Ctrl+X", [=]{ commandCallback(2003); }, 2003},
        {"edit.copy", "Copy", "Copy selection", "Edit", "Ctrl+C", [=]{ commandCallback(2004); }, 2004},
        {"edit.paste", "Paste", "Paste from clipboard", "Edit", "Ctrl+V", [=]{ commandCallback(2005); }, 2005},
        {"edit.find", "Find", "Find text in file", "Edit", "Ctrl+F", [=]{ commandCallback(2006); }, 2006},
        {"edit.replace", "Replace", "Find and replace text", "Edit", "Ctrl+H", [=]{ commandCallback(2007); }, 2007},
        
        // View operations
        {"view.commandPalette", "Command Palette", "Open command palette", "View", "Ctrl+Shift+P", [=]{ commandCallback(3001); }, 3001},
        {"view.terminal", "Toggle Terminal", "Show/hide terminal", "View", "Ctrl+`", [=]{ commandCallback(3002); }, 3002},
        {"view.sidebar", "Toggle Sidebar", "Show/hide sidebar", "View", "Ctrl+B", [=]{ commandCallback(3003); }, 3003},
        {"view.explorer", "Explorer", "Show file explorer", "View", "Ctrl+Shift+E", [=]{ commandCallback(3004); }, 3004},
        
        // AI operations
        {"ai.chat", "AI Chat", "Open AI chat panel", "AI", "Ctrl+Shift+A", [=]{ commandCallback(4001); }, 4001},
        {"ai.suggest", "AI Suggestions", "Get code suggestions", "AI", "Ctrl+Space", [=]{ commandCallback(4002); }, 4002},
        {"ai.explain", "Explain Code", "AI explains selected code", "AI", "Ctrl+Shift+/", [=]{ commandCallback(4003); }, 4003},
        {"ai.refactor", "AI Refactor", "AI-assisted refactoring", "AI", "", [=]{ commandCallback(4004); }, 4004},
        {"ai.bugReport", "Bug Report", "AI analyzes code for bugs", "AI", "", [=]{ commandCallback(4005); }, 4005},
        
        // Context operations
        {"context.4k", "Context: 4K", "Set 4K token context", "Context", "", [=]{ commandCallback(5001); }, 5001},
        {"context.8k", "Context: 8K", "Set 8K token context", "Context", "", [=]{ commandCallback(5002); }, 5002},
        {"context.32k", "Context: 32K", "Set 32K token context", "Context", "", [=]{ commandCallback(5003); }, 5003},
        {"context.128k", "Context: 128K", "Set 128K token context", "Context", "", [=]{ commandCallback(5004); }, 5004},
        {"context.512k", "Context: 512K", "Set 512K token context", "Context", "", [=]{ commandCallback(5005); }, 5005},
        {"context.1m", "Context: 1M", "Set 1M token context", "Context", "", [=]{ commandCallback(5006); }, 5006},
        
        // Mode toggles
        {"mode.maxMode", "Toggle Max Mode", "Toggle maximum context mode", "Mode", "", [=]{ commandCallback(6001); }, 6001},
        {"mode.deepThinking", "Toggle Deep Thinking", "Toggle chain-of-thought mode", "Mode", "", [=]{ commandCallback(6002); }, 6002},
        {"mode.deepResearch", "Toggle Deep Research", "Toggle file scanning mode", "Mode", "", [=]{ commandCallback(6003); }, 6003},
        {"mode.noRefusal", "Toggle No Refusal", "Toggle safety override", "Mode", "", [=]{ commandCallback(6004); }, 6004},
        
        // Tools
        {"tools.reactGen", "Generate React Project", "Create a new React project", "Tools", "", [=]{ commandCallback(7001); }, 7001},
        {"tools.installVsix", "Install VSIX", "Install VS Code extension", "Tools", "", [=]{ commandCallback(7002); }, 7002},
        {"tools.hotPatch", "Hot Patch", "Apply live code patch", "Tools", "", [=]{ commandCallback(7003); }, 7003},
        
        // Build/Run
        {"build.build", "Build", "Build the project", "Build", "Ctrl+Shift+B", [=]{ commandCallback(8001); }, 8001},
        {"build.run", "Run", "Run the project", "Build", "F5", [=]{ commandCallback(8002); }, 8002},
        {"build.debug", "Debug", "Debug the project", "Build", "Ctrl+F5", [=]{ commandCallback(8003); }, 8003},
    };
    
    palette.registerCommands(commands);
}

} // namespace RawrXD
