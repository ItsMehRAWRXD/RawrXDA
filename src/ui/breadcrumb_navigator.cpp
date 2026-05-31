#include "ui/breadcrumb_navigator.h"
#include <algorithm>

namespace RawrXD::UI {

BreadcrumbNavigator::BreadcrumbNavigator() = default;
BreadcrumbNavigator::~BreadcrumbNavigator() {
    shutdown();
}

bool BreadcrumbNavigator::initialize(HWND parent) {
    m_parent = parent;

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "RawrXDBreadcrumbNavigator";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassEx(&wc);

    m_hwnd = CreateWindowEx(0, "RawrXDBreadcrumbNavigator", "Breadcrumb",
                            WS_CHILD | WS_VISIBLE,
                            0, 0, 800, 24,
                            parent, nullptr, GetModuleHandle(nullptr), this);

    return m_hwnd != nullptr;
}

void BreadcrumbNavigator::shutdown() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

HWND BreadcrumbNavigator::getHandle() const {
    return m_hwnd;
}

void BreadcrumbNavigator::resize(int width, int height) {
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
    }
}

void BreadcrumbNavigator::setFilePath(const std::string& path) {
    m_pathItems = parsePath(path);
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void BreadcrumbNavigator::clearFilePath() {
    m_pathItems.clear();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

std::string BreadcrumbNavigator::getFilePath() const {
    if (m_pathItems.empty()) return "";

    std::string path;
    for (const auto& item : m_pathItems) {
        path += item.path;
        if (&item != &m_pathItems.back()) {
            path += "/";
        }
    }
    return path;
}

void BreadcrumbNavigator::setSymbolPath(const std::vector<BreadcrumbItem>& symbols) {
    m_symbolItems = symbols;
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void BreadcrumbNavigator::addSymbol(const BreadcrumbItem& symbol) {
    m_symbolItems.push_back(symbol);
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void BreadcrumbNavigator::clearSymbols() {
    m_symbolItems.clear();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void BreadcrumbNavigator::setMode(BreadcrumbMode mode) {
    m_mode = mode;
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

BreadcrumbMode BreadcrumbNavigator::getMode() const {
    return m_mode;
}

void BreadcrumbNavigator::navigateTo(const std::string& path) {
    // Add to history
    if (m_historyIndex < m_history.size()) {
        m_history.resize(m_historyIndex);
    }
    m_history.push_back(path);
    m_historyIndex++;

    setFilePath(path);

    if (m_navigateCallback) {
        m_navigateCallback(path);
    }
}

void BreadcrumbNavigator::navigateUp() {
    if (m_pathItems.size() > 1) {
        m_pathItems.pop_back();
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void BreadcrumbNavigator::navigateBack() {
    if (canGoBack()) {
        m_historyIndex--;
        setFilePath(m_history[m_historyIndex - 1]);
    }
}

void BreadcrumbNavigator::navigateForward() {
    if (canGoForward()) {
        m_historyIndex++;
        setFilePath(m_history[m_historyIndex - 1]);
    }
}

bool BreadcrumbNavigator::canGoBack() const {
    return m_historyIndex > 1;
}

bool BreadcrumbNavigator::canGoForward() const {
    return m_historyIndex < m_history.size();
}

void BreadcrumbNavigator::showDropdown(const BreadcrumbItem& item, int x, int y) {
    // Show dropdown with children
}

void BreadcrumbNavigator::hideDropdown() {
    if (m_dropdown) {
        DestroyWindow(m_dropdown);
        m_dropdown = nullptr;
    }
}

bool BreadcrumbNavigator::isDropdownVisible() const {
    return m_dropdown != nullptr;
}

void BreadcrumbNavigator::registerSymbolProvider(
    std::function<std::vector<BreadcrumbItem>(const std::string& path)> provider) {
    m_symbolProviders.push_back(provider);
}

void BreadcrumbNavigator::updateSymbols() {
    m_symbolItems.clear();

    std::string path = getFilePath();
    for (const auto& provider : m_symbolProviders) {
        auto symbols = provider(path);
        m_symbolItems.insert(m_symbolItems.end(), symbols.begin(), symbols.end());
    }

    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void BreadcrumbNavigator::setShowIcons(bool show) {
    m_showIcons = show;
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void BreadcrumbNavigator::setShowRoot(bool show) {
    m_showRoot = show;
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void BreadcrumbNavigator::setSeparator(const std::string& separator) {
    m_separator = separator;
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void BreadcrumbNavigator::onPathNavigate(NavigationCallback callback) {
    m_navigateCallback = callback;
}

void BreadcrumbNavigator::onSymbolSelect(SymbolSelectCallback callback) {
    m_symbolCallback = callback;
}

void BreadcrumbNavigator::layout() {
    // Layout breadcrumb items
}

void BreadcrumbNavigator::draw(HDC hdc) {
    RECT rect;
    GetClientRect(m_hwnd, &rect);

    // Draw background
    FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));

    // Draw items based on mode
    int x = 5;
    switch (m_mode) {
        case BreadcrumbMode::FilePath:
            for (const auto& item : m_pathItems) {
                RECT itemRect = {x, 0, x + 100, rect.bottom};
                drawItem(hdc, item, itemRect);
                x += 100;

                if (&item != &m_pathItems.back()) {
                    RECT sepRect = {x, 0, x + 20, rect.bottom};
                    drawSeparator(hdc, sepRect);
                    x += 20;
                }
            }
            break;

        case BreadcrumbMode::SymbolPath:
            for (const auto& item : m_symbolItems) {
                RECT itemRect = {x, 0, x + 100, rect.bottom};
                drawItem(hdc, item, itemRect);
                x += 100;

                if (&item != &m_symbolItems.back()) {
                    RECT sepRect = {x, 0, x + 20, rect.bottom};
                    drawSeparator(hdc, sepRect);
                    x += 20;
                }
            }
            break;

        case BreadcrumbMode::Mixed:
            // Draw both file path and symbol path
            break;
    }
}

void BreadcrumbNavigator::drawItem(HDC hdc, const BreadcrumbItem& item, RECT& rect) {
    // Draw item text
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, item.name.c_str(), -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void BreadcrumbNavigator::drawSeparator(HDC hdc, RECT& rect) {
    // Draw separator
    SetTextColor(hdc, RGB(128, 128, 128));
    SetBkMode(hdc, TRANSPARENT);
    DrawText(hdc, m_separator.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

std::vector<BreadcrumbItem> BreadcrumbNavigator::parsePath(const std::string& path) {
    std::vector<BreadcrumbItem> items;
    std::string current;

    for (char c : path) {
        if (c == '/' || c == '\\') {
            if (!current.empty()) {
                BreadcrumbItem item;
                item.name = current;
                item.path = current;
                items.push_back(item);
                current.clear();
            }
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        BreadcrumbItem item;
        item.name = current;
        item.path = current;
        items.push_back(item);
    }

    return items;
}

BreadcrumbItem BreadcrumbNavigator::hitTest(int x, int y) {
    // Find item at position
    return BreadcrumbItem{};
}

LRESULT BreadcrumbNavigator::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);
            draw(hdc);
            EndPaint(m_hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            // Handle click
            return 0;
        }
    }

    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK BreadcrumbNavigator::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BreadcrumbNavigator* navigator = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        navigator = reinterpret_cast<BreadcrumbNavigator*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(navigator));
    } else {
        navigator = reinterpret_cast<BreadcrumbNavigator*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (navigator) {
        return navigator->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Global instance
BreadcrumbNavigator& getBreadcrumbNavigator() {
    static BreadcrumbNavigator navigator;
    return navigator;
}

} // namespace RawrXD::UI
