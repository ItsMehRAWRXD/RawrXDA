#include "native_layout.h"
#include <windows.h>
#include <iostream>

NativeWidget::NativeWidget(NativeWidget* parent)
    : m_parent(parent), m_hwnd(nullptr)
{
}

NativeWidget::~NativeWidget()
{
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

void NativeWidget::setVisible(bool visible)
{
    if (m_hwnd) {
        ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
    }
}

void NativeWidget::setGeometry(int x, int y, int width, int height)
{
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, x, y, width, height, SWP_NOZORDER);
    }
}

NativeHBoxLayout::NativeHBoxLayout(NativeWidget* parent)
    : m_parent(parent), m_spacing(0)
{
    m_margins[0] = m_margins[1] = m_margins[2] = m_margins[3] = 0;
}

void NativeHBoxLayout::addWidget(NativeWidget* widget, int stretch)
{
    // Basic layout implementation - would need proper Windows layout management
    std::cout << "HBoxLayout: Added widget with stretch " << stretch << std::endl;
}

void NativeHBoxLayout::setMargins(int left, int top, int right, int bottom)
{
    m_margins[0] = left;
    m_margins[1] = top;
    m_margins[2] = right;
    m_margins[3] = bottom;
}

void NativeHBoxLayout::setSpacing(int spacing)
{
    m_spacing = spacing;
}

void NativeHBoxLayout::setLayout(NativeWidget* widget)
{
    std::cout << "HBoxLayout: Set layout on widget" << std::endl;
}

NativeVBoxLayout::NativeVBoxLayout(NativeWidget* parent)
    : m_parent(parent), m_spacing(0)
{
    m_margins[0] = m_margins[1] = m_margins[2] = m_margins[3] = 0;
}

void NativeVBoxLayout::addWidget(NativeWidget* widget, int stretch)
{
    std::cout << "VBoxLayout: Added widget with stretch " << stretch << std::endl;
}

void NativeVBoxLayout::setMargins(int left, int top, int right, int bottom)
{
    m_margins[0] = left;
    m_margins[1] = top;
    m_margins[2] = right;
    m_margins[3] = bottom;
}

void NativeVBoxLayout::setSpacing(int spacing)
{
    m_spacing = spacing;
}

void NativeVBoxLayout::setLayout(NativeWidget* widget)
{
    std::cout << "VBoxLayout: Set layout on widget" << std::endl;
}