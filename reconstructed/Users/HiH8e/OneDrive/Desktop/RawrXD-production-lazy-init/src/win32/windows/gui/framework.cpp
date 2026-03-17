#include "windows_gui_framework.h"
#include <windows.h>
#include <commctrl.h>
#include <iostream>

// Windows message handler
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    NativeWindow* window = reinterpret_cast<NativeWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    
    switch (uMsg) {
        case WM_CLOSE:
            if (window && window->m_onClose) {
                window->m_onClose();
            }
            return 0;
        case WM_SIZE:
            if (window && window->m_onResize) {
                window->m_onResize(LOWORD(lParam), HIWORD(lParam));
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_COMMAND:
            // Handle button clicks and other commands
            break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

NativeWindow::NativeWindow() : m_hwnd(nullptr) {}

NativeWindow::~NativeWindow() {
    close();
}

bool NativeWindow::create(const std::string& title, int width, int height) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "NativeWindow";
    
    RegisterClass(&wc);
    
    m_hwnd = CreateWindowEx(
        0,
        "NativeWindow",
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    if (m_hwnd) {
        SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        return true;
    }
    return false;
}

void NativeWindow::show() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
    }
}

void NativeWindow::hide() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void NativeWindow::close() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void NativeWindow::setOnClose(std::function<void()> callback) {
    m_onClose = callback;
}

void NativeWindow::setOnResize(std::function<void(int, int)> callback) {
    m_onResize = callback;
}

// NativeWidget implementation
NativeWidget::NativeWidget(NativeWidget* parent)
    : m_parent(parent), m_hwnd(nullptr)
{
}

NativeWidget::~NativeWidget() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

void NativeWidget::setVisible(bool visible) {
    if (m_hwnd) {
        ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
    }
}

void NativeWidget::setGeometry(int x, int y, int width, int height) {
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, x, y, width, height, SWP_NOZORDER);
    }
}

void NativeWidget::setText(const std::string& text) {
    if (m_hwnd) {
        SetWindowText(m_hwnd, text.c_str());
    }
}

// NativeButton implementation
NativeButton::NativeButton(NativeWidget* parent)
    : NativeWidget(parent)
{
    if (parent && parent->getHandle()) {
        m_hwnd = CreateWindow(
            "BUTTON", "Button",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            0, 0, 100, 30,
            parent->getHandle(), nullptr, GetModuleHandle(nullptr), nullptr
        );
    }
}

void NativeButton::setOnClick(std::function<void()> callback) {
    m_onClick = callback;
}

// NativeTextEditor implementation
NativeTextEditor::NativeTextEditor(NativeWidget* parent)
    : NativeWidget(parent)
{
    if (parent && parent->getHandle()) {
        m_hwnd = CreateWindow(
            "EDIT", "",
            WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
            0, 0, 400, 300,
            parent->getHandle(), nullptr, GetModuleHandle(nullptr), nullptr
        );
    }
}

std::string NativeTextEditor::getText() const {
    if (m_hwnd) {
        int length = GetWindowTextLength(m_hwnd);
        std::string text(length, '\0');
        GetWindowText(m_hwnd, &text[0], length + 1);
        return text;
    }
    return "";
}

void NativeTextEditor::setBackgroundColor(int r, int g, int b) {
    std::cout << "TextEditor: Set background color to RGB(" << r << "," << g << "," << b << ")" << std::endl;
}

void NativeTextEditor::setTextColor(int r, int g, int b) {
    std::cout << "TextEditor: Set text color to RGB(" << r << "," << g << "," << b << ")" << std::endl;
}

void NativeTextEditor::setFont(const std::string& family, int size) {
    std::cout << "TextEditor: Set font to " << family << " size " << size << std::endl;
}

// NativeComboBox implementation
NativeComboBox::NativeComboBox(NativeWidget* parent)
    : NativeWidget(parent)
{
    if (parent && parent->getHandle()) {
        m_hwnd = CreateWindow(
            "COMBOBOX", "",
            CBS_DROPDOWN | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
            0, 0, 200, 30,
            parent->getHandle(), nullptr, GetModuleHandle(nullptr), nullptr
        );
    }
}

void NativeComboBox::addItem(const std::string& text) {
    if (m_hwnd) {
        SendMessage(m_hwnd, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }
}

void NativeComboBox::clear() {
    if (m_hwnd) {
        SendMessage(m_hwnd, CB_RESETCONTENT, 0, 0);
    }
}

int NativeComboBox::getSelectedIndex() const {
    if (m_hwnd) {
        return SendMessage(m_hwnd, CB_GETCURSEL, 0, 0);
    }
    return -1;
}

void NativeComboBox::setOnChange(std::function<void(int)> callback) {
    m_onChange = callback;
}

// NativeSlider implementation
NativeSlider::NativeSlider(NativeWidget* parent)
    : NativeWidget(parent)
{
    if (parent && parent->getHandle()) {
        m_hwnd = CreateWindow(
            TRACKBAR_CLASS, "",
            WS_VISIBLE | WS_CHILD | TBS_AUTOTICKS,
            0, 0, 200, 30,
            parent->getHandle(), nullptr, GetModuleHandle(nullptr), nullptr
        );
    }
}

void NativeSlider::setRange(int min, int max) {
    if (m_hwnd) {
        SendMessage(m_hwnd, TBM_SETRANGE, TRUE, MAKELONG(min, max));
    }
}

void NativeSlider::setValue(int value) {
    if (m_hwnd) {
        SendMessage(m_hwnd, TBM_SETPOS, TRUE, value);
    }
}

int NativeSlider::getValue() const {
    if (m_hwnd) {
        return SendMessage(m_hwnd, TBM_GETPOS, 0, 0);
    }
    return 0;
}

void NativeSlider::setOnValueChanged(std::function<void(int)> callback) {
    m_onValueChanged = callback;
}

// NativeLabel implementation
NativeLabel::NativeLabel(NativeWidget* parent)
    : NativeWidget(parent)
{
    if (parent && parent->getHandle()) {
        m_hwnd = CreateWindow(
            "STATIC", "Label",
            WS_VISIBLE | WS_CHILD,
            0, 0, 100, 20,
            parent->getHandle(), nullptr, GetModuleHandle(nullptr), nullptr
        );
    }
}

// NativeTabWidget implementation
NativeTabWidget::NativeTabWidget(NativeWidget* parent)
    : NativeWidget(parent)
{
    if (parent && parent->getHandle()) {
        m_hwnd = CreateWindow(
            WC_TABCONTROL, "",
            WS_VISIBLE | WS_CHILD | TCS_FIXEDWIDTH,
            0, 0, 400, 300,
            parent->getHandle(), nullptr, GetModuleHandle(nullptr), nullptr
        );
    }
}

void NativeTabWidget::addTab(const std::string& title, NativeWidget* widget) {
    if (m_hwnd) {
        TCITEM tie = {};
        tie.mask = TCIF_TEXT;
        tie.pszText = const_cast<char*>(title.c_str());
        SendMessage(m_hwnd, TCM_INSERTITEM, 0, reinterpret_cast<LPARAM>(&tie));
    }
}

void NativeTabWidget::removeTab(int index) {
    if (m_hwnd) {
        SendMessage(m_hwnd, TCM_DELETEITEM, index, 0);
    }
}

int NativeTabWidget::getCurrentIndex() const {
    if (m_hwnd) {
        return SendMessage(m_hwnd, TCM_GETCURSEL, 0, 0);
    }
    return -1;
}

void NativeTabWidget::setOnTabChanged(std::function<void(int)> callback) {
    m_onTabChanged = callback;
}

// Layout implementations
NativeHBoxLayout::NativeHBoxLayout(NativeWidget* parent) : m_parent(parent) {}

void NativeHBoxLayout::addWidget(NativeWidget* widget, int stretch) {
    std::cout << "HBoxLayout: Added widget with stretch " << stretch << std::endl;
}

void NativeHBoxLayout::setMargins(int left, int top, int right, int bottom) {
    std::cout << "HBoxLayout: Set margins L:" << left << " T:" << top << " R:" << right << " B:" << bottom << std::endl;
}

void NativeHBoxLayout::setSpacing(int spacing) {
    std::cout << "HBoxLayout: Set spacing " << spacing << std::endl;
}

NativeVBoxLayout::NativeVBoxLayout(NativeWidget* parent) : m_parent(parent) {}

void NativeVBoxLayout::addWidget(NativeWidget* widget, int stretch) {
    std::cout << "VBoxLayout: Added widget with stretch " << stretch << std::endl;
}

void NativeVBoxLayout::setMargins(int left, int top, int right, int bottom) {
    std::cout << "VBoxLayout: Set margins L:" << left << " T:" << top << " R:" << right << " B:" << bottom << std::endl;
}

void NativeVBoxLayout::setSpacing(int spacing) {
    std::cout << "VBoxLayout: Set spacing " << spacing << std::endl;
}