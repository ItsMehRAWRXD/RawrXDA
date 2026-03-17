#include "native_layout.h"
#include <windows.h>
#include <algorithm>
#include <iostream>
#include <numeric>

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
    if (!widget) return;
    if (stretch < 0) stretch = 0;
    m_items.push_back({widget, stretch});
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

void NativeHBoxLayout::applyLayout(HWND host, int width, int height)
{
    if (!host || m_items.empty()) return;

    const int left = m_margins[0];
    const int top = m_margins[1];
    const int right = m_margins[2];
    const int bottom = m_margins[3];

    int availableW = std::max(0, width - left - right - static_cast<int>((m_items.size() - 1) * m_spacing));
    int availableH = std::max(0, height - top - bottom);

    int totalStretch = 0;
    for (const auto& item : m_items) totalStretch += (item.stretch > 0 ? item.stretch : 0);

    int fixedCount = 0;
    for (const auto& item : m_items) if (item.stretch == 0) fixedCount++;

    int fixedWidth = fixedCount > 0 ? availableW / std::max(1, static_cast<int>(m_items.size())) : 0;
    int x = left;

    for (const auto& item : m_items) {
        int w = (item.stretch > 0 && totalStretch > 0)
            ? (availableW * item.stretch) / totalStretch
            : fixedWidth;
        if (item.widget && item.widget->getHandle()) {
            item.widget->setGeometry(x, top, std::max(0, w), availableH);
        }
        x += w + m_spacing;
    }
}

void NativeHBoxLayout::setLayout(NativeWidget* widget)
{
    HWND host = widget ? widget->getHandle() : (m_parent ? m_parent->getHandle() : nullptr);
    if (!host) {
        std::cout << "HBoxLayout: no host handle; layout skipped" << std::endl;
        return;
    }

    RECT rc{};
    GetClientRect(host, &rc);
    applyLayout(host, rc.right - rc.left, rc.bottom - rc.top);
}

NativeVBoxLayout::NativeVBoxLayout(NativeWidget* parent)
    : m_parent(parent), m_spacing(0)
{
    m_margins[0] = m_margins[1] = m_margins[2] = m_margins[3] = 0;
}

void NativeVBoxLayout::addWidget(NativeWidget* widget, int stretch)
{
    if (!widget) return;
    if (stretch < 0) stretch = 0;
    m_items.push_back({widget, stretch});
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

void NativeVBoxLayout::applyLayout(HWND host, int width, int height)
{
    if (!host || m_items.empty()) return;

    const int left = m_margins[0];
    const int top = m_margins[1];
    const int right = m_margins[2];
    const int bottom = m_margins[3];

    int availableH = std::max(0, height - top - bottom - static_cast<int>((m_items.size() - 1) * m_spacing));
    int availableW = std::max(0, width - left - right);

    int totalStretch = 0;
    for (const auto& item : m_items) totalStretch += (item.stretch > 0 ? item.stretch : 0);

    int fixedCount = 0;
    for (const auto& item : m_items) if (item.stretch == 0) fixedCount++;

    int fixedHeight = fixedCount > 0 ? availableH / std::max(1, static_cast<int>(m_items.size())) : 0;
    int y = top;

    for (const auto& item : m_items) {
        int h = (item.stretch > 0 && totalStretch > 0)
            ? (availableH * item.stretch) / totalStretch
            : fixedHeight;
        if (item.widget && item.widget->getHandle()) {
            item.widget->setGeometry(left, y, availableW, std::max(0, h));
        }
        y += h + m_spacing;
    }
}

void NativeVBoxLayout::setLayout(NativeWidget* widget)
{
    HWND host = widget ? widget->getHandle() : (m_parent ? m_parent->getHandle() : nullptr);
    if (!host) {
        std::cout << "VBoxLayout: no host handle; layout skipped" << std::endl;
        return;
    }

    RECT rc{};
    GetClientRect(host, &rc);
    applyLayout(host, rc.right - rc.left, rc.bottom - rc.top);
}