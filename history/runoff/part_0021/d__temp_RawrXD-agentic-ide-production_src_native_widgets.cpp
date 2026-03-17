#include "native_widgets.h"
#include <windows.h>
#include <commctrl.h>
#include <iostream>

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

void NativeButton::setText(const std::string& text)
{
    if (m_hwnd) {
        SetWindowText(m_hwnd, text.c_str());
    }
}

void NativeButton::setOnClick(std::function<void()> callback)
{
    m_onClick = std::move(callback);
}

NativeTextEditor::NativeTextEditor(NativeWidget* parent)
    : NativeWidget(parent)
{
    HWND parentHwnd = nullptr;
    if (parent && parent->getHandle()) {
        parentHwnd = parent->getHandle();
    }
    
    m_hwnd = CreateWindow(
        "EDIT", "",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        0, 0, 400, 300,
        parentHwnd, nullptr, GetModuleHandle(nullptr), nullptr
    );

    if (!m_hwnd) {
        std::cerr << "NativeTextEditor: CreateWindow failed (parent=" << parentHwnd
                  << ") error=" << GetLastError() << std::endl;
    }
}

void NativeTextEditor::setText(const std::string& text)
{
    if (m_hwnd) {
        SetWindowText(m_hwnd, text.c_str());
    }
}

std::string NativeTextEditor::getText() const
{
    if (m_hwnd) {
        int length = GetWindowTextLength(m_hwnd);
        std::string text(length, '\0');
        GetWindowText(m_hwnd, &text[0], length + 1);
        return text;
    }
    return "";
}

void NativeTextEditor::setBackgroundColor(const std::string& color)
{
    std::cout << "TextEditor: Set background color to " << color << std::endl;
}

void NativeTextEditor::setTextColor(const std::string& color)
{
    std::cout << "TextEditor: Set text color to " << color << std::endl;
}

void NativeTextEditor::setFont(const std::string& family, int size)
{
    std::cout << "TextEditor: Set font to " << family << " size " << size << std::endl;
}

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

void NativeComboBox::addItem(const std::string& text)
{
    if (m_hwnd) {
        SendMessage(m_hwnd, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }
}

void NativeComboBox::clear()
{
    if (m_hwnd) {
        SendMessage(m_hwnd, CB_RESETCONTENT, 0, 0);
    }
}

int NativeComboBox::getSelectedIndex() const
{
    if (m_hwnd) {
        return static_cast<int>(SendMessage(m_hwnd, CB_GETCURSEL, 0, 0));
    }
    return -1;
}

void NativeComboBox::setOnChange(std::function<void(int)> callback)
{
    m_onChange = std::move(callback);
}

NativeSlider::NativeSlider(NativeOrientation orient, NativeWidget* parent)
    : NativeWidget(parent)
    , m_orientation(orient)
{
    if (parent && parent->getHandle()) {
        DWORD style = WS_VISIBLE | WS_CHILD | TBS_AUTOTICKS;
        if (m_orientation == NativeOrientation::Vertical) {
            style |= TBS_VERT;
        }

        m_hwnd = CreateWindow(
            TRACKBAR_CLASS, "",
            style,
            0, 0, 200, 30,
            parent->getHandle(), nullptr, GetModuleHandle(nullptr), nullptr
        );
    }
}

void NativeSlider::setRange(int min, int max)
{
    if (m_hwnd) {
        SendMessage(m_hwnd, TBM_SETRANGE, TRUE, MAKELONG(min, max));
    }
}

void NativeSlider::setValue(int value)
{
    if (m_hwnd) {
        SendMessage(m_hwnd, TBM_SETPOS, TRUE, value);
    }
    if (m_onValueChanged) m_onValueChanged(value);
}

int NativeSlider::getValue() const
{
    if (m_hwnd) {
        return static_cast<int>(SendMessage(m_hwnd, TBM_GETPOS, 0, 0));
    }
    return 0;
}

void NativeSlider::setOnValueChanged(std::function<void(int)> callback)
{
    m_onValueChanged = std::move(callback);
}

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

void NativeLabel::setText(const std::string& text)
{
    if (m_hwnd) {
        SetWindowText(m_hwnd, text.c_str());
    }
}