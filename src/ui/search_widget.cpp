#include "ui/search_widget.h"
#include <regex>

namespace RawrXD::UI {

SearchWidget::SearchWidget() = default;
SearchWidget::~SearchWidget() {
    shutdown();
}

bool SearchWidget::initialize(HWND parent) {
    m_parent = parent;

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "RawrXDSearchWidget";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassEx(&wc);

    m_hwnd = CreateWindowEx(0, "RawrXDSearchWidget", "Search",
                            WS_CHILD | WS_CLIPCHILDREN,
                            0, 0, 400, 60,
                            parent, nullptr, GetModuleHandle(nullptr), this);

    if (m_hwnd) {
        createControls();
    }

    return m_hwnd != nullptr;
}

void SearchWidget::shutdown() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

HWND SearchWidget::getHandle() const {
    return m_hwnd;
}

void SearchWidget::show() {
    m_visible = true;
    ShowWindow(m_hwnd, SW_SHOW);
    SetFocus(m_searchBox);
}

void SearchWidget::hide() {
    m_visible = false;
    ShowWindow(m_hwnd, SW_HIDE);
}

void SearchWidget::toggle() {
    if (m_visible) {
        hide();
    } else {
        show();
    }
}

bool SearchWidget::isVisible() const {
    return m_visible;
}

void SearchWidget::resize(int width, int height) {
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
    }
}

void SearchWidget::setMode(SearchMode mode) {
    m_mode = mode;
    updateVisibility();
}

SearchMode SearchWidget::getMode() const {
    return m_mode;
}

void SearchWidget::setScope(SearchScope scope) {
    m_scope = scope;
}

SearchScope SearchWidget::getScope() const {
    return m_scope;
}

void SearchWidget::setSearchText(const std::string& text) {
    if (m_searchBox) {
        SetWindowTextA(m_searchBox, text.c_str());
    }
}

std::string SearchWidget::getSearchText() const {
    if (!m_searchBox) return "";

    char buffer[256];
    GetWindowTextA(m_searchBox, buffer, sizeof(buffer));
    return buffer;
}

void SearchWidget::setReplaceText(const std::string& text) {
    if (m_replaceBox) {
        SetWindowTextA(m_replaceBox, text.c_str());
    }
}

std::string SearchWidget::getReplaceText() const {
    if (!m_replaceBox) return "";

    char buffer[256];
    GetWindowTextA(m_replaceBox, buffer, sizeof(buffer));
    return buffer;
}

void SearchWidget::setOptions(const SearchOptions& options) {
    m_options = options;
}

SearchOptions SearchWidget::getOptions() const {
    return m_options;
}

void SearchWidget::toggleCaseSensitive() {
    m_options.caseSensitive = !m_options.caseSensitive;
}

void SearchWidget::toggleWholeWord() {
    m_options.wholeWord = !m_options.wholeWord;
}

void SearchWidget::toggleRegex() {
    m_options.regex = !m_options.regex;
}

void SearchWidget::togglePreserveCase() {
    m_options.preserveCase = !m_options.preserveCase;
}

void SearchWidget::findNext() {
    if (m_searchCallback) {
        m_searchCallback(getSearchText(), m_options);
    }
}

void SearchWidget::findPrevious() {
    if (m_searchCallback) {
        m_searchCallback(getSearchText(), m_options);
    }
}

void SearchWidget::findAll() {
    if (m_searchCallback) {
        m_searchCallback(getSearchText(), m_options);
    }
}

void SearchWidget::replace() {
    if (m_replaceCallback) {
        m_replaceCallback(getSearchText(), getReplaceText());
    }
}

void SearchWidget::replaceAll() {
    if (m_replaceCallback) {
        m_replaceCallback(getSearchText(), getReplaceText());
    }
}

void SearchWidget::replaceNext() {
    replace();
}

std::vector<SearchResult> SearchWidget::getResults() const {
    return m_results;
}

int SearchWidget::getResultCount() const {
    return static_cast<int>(m_results.size());
}

int SearchWidget::getCurrentResultIndex() const {
    return m_currentResultIndex;
}

void SearchWidget::clearResults() {
    m_results.clear();
    m_currentResultIndex = -1;
}

void SearchWidget::addToHistory(const std::string& text) {
    if (text.empty()) return;

    // Remove if already exists
    m_searchHistory.erase(std::remove(m_searchHistory.begin(), m_searchHistory.end(), text), m_searchHistory.end());
    m_searchHistory.insert(m_searchHistory.begin(), text);

    // Keep only last 20
    if (m_searchHistory.size() > 20) {
        m_searchHistory.resize(20);
    }
}

std::vector<std::string> SearchWidget::getSearchHistory() const {
    return m_searchHistory;
}

std::vector<std::string> SearchWidget::getReplaceHistory() const {
    return m_replaceHistory;
}

void SearchWidget::clearHistory() {
    m_searchHistory.clear();
    m_replaceHistory.clear();
}

void SearchWidget::setSelection(const std::string& text) {
    // Set selected text
}

std::string SearchWidget::getSelectedText() const {
    return "";
}

void SearchWidget::onSearch(SearchCallback callback) {
    m_searchCallback = callback;
}

void SearchWidget::onReplace(ReplaceCallback callback) {
    m_replaceCallback = callback;
}

void SearchWidget::onResultSelected(ResultCallback callback) {
    m_resultCallback = callback;
}

void SearchWidget::setShowOptions(bool show) {
    if (m_optionsPanel) {
        ShowWindow(m_optionsPanel, show ? SW_SHOW : SW_HIDE);
    }
}

void SearchWidget::setShowHistory(bool show) {
    // Show/hide history dropdown
}

void SearchWidget::setAutoSearch(bool enabled) {
    m_autoSearch = enabled;
}

void SearchWidget::setSearchDelay(int milliseconds) {
    m_searchDelay = milliseconds;
}

void SearchWidget::createControls() {
    // Create search box
    m_searchBox = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
                                  WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                  5, 5, 200, 20,
                                  m_hwnd, (HMENU)1, GetModuleHandle(nullptr), nullptr);

    // Create replace box
    m_replaceBox = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
                                   WS_CHILD | ES_AUTOHSCROLL,
                                   5, 30, 200, 20,
                                   m_hwnd, (HMENU)2, GetModuleHandle(nullptr), nullptr);

    // Create options panel
    m_optionsPanel = CreateWindowEx(0, "BUTTON", "Options",
                                    WS_CHILD | BS_GROUPBOX,
                                    210, 5, 150, 50,
                                    m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
}

void SearchWidget::layout() {
    // Layout controls
}

void SearchWidget::updateVisibility() {
    ShowWindow(m_replaceBox, (m_mode == SearchMode::Replace || m_mode == SearchMode::ReplaceInFiles) ? SW_SHOW : SW_HIDE);
}

void SearchWidget::performSearch() {
    if (m_searchCallback) {
        m_searchCallback(getSearchText(), m_options);
    }
}

void SearchWidget::performReplace() {
    if (m_replaceCallback) {
        m_replaceCallback(getSearchText(), getReplaceText());
    }
}

std::vector<SearchMatch> SearchWidget::findMatches(const std::string& text, const std::string& query) {
    std::vector<SearchMatch> matches;

    if (m_options.regex) {
        try {
            std::regex pattern(query, m_options.caseSensitive ? std::regex::normal : std::regex::icase);
            std::sregex_iterator it(text.begin(), text.end(), pattern);
            std::sregex_iterator end;

            for (; it != end; ++it) {
                SearchMatch match;
                match.start = static_cast<int>(it->position());
                match.end = match.start + static_cast<int>(it->length());
                match.text = it->str();
                matches.push_back(match);
            }
        } catch (...) {
            // Invalid regex
        }
    } else {
        size_t pos = 0;
        while ((pos = text.find(query, pos)) != std::string::npos) {
            SearchMatch match;
            match.start = static_cast<int>(pos);
            match.end = match.start + static_cast<int>(query.length());
            match.text = query;
            matches.push_back(match);
            pos += query.length();
        }
    }

    return matches;
}

LRESULT SearchWidget::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND: {
            if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == 1) {
                if (m_autoSearch) {
                    performSearch();
                }
            }
            return 0;
        }

        case WM_SIZE: {
            layout();
            return 0;
        }
    }

    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SearchWidget::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SearchWidget* widget = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        widget = reinterpret_cast<SearchWidget*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(widget));
    } else {
        widget = reinterpret_cast<SearchWidget*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (widget) {
        return widget->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Global instance
SearchWidget& getSearchWidget() {
    static SearchWidget widget;
    return widget;
}

} // namespace RawrXD::UI
