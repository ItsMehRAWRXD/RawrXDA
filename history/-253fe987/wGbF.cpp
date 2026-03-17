#include "native_layout.h"
#include <windows.h>
#include <algorithm>
#include <iostream>
#include <unordered_map>

namespace {
struct LayoutHook {
    bool vertical = false;
    NativeHBoxLayout* hbox = nullptr;
    NativeVBoxLayout* vbox = nullptr;
    WNDPROC original = nullptr;
};

std::unordered_map<HWND, LayoutHook> g_layoutHooks;

LRESULT CALLBACK LayoutWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto it = g_layoutHooks.find(hwnd);
    if (it != g_layoutHooks.end()) {
        auto& hook = it->second;
        if (msg == WM_SIZE) {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            if (hook.vertical && hook.vbox) {
                hook.vbox->applyLayout(hwnd, width, height);
            } else if (!hook.vertical && hook.hbox) {
                hook.hbox->applyLayout(hwnd, width, height);
            }
        } else if (msg == WM_NCDESTROY) {
            if (hook.original) {
                SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hook.original));
            }
            g_layoutHooks.erase(it);
        }
        if (hook.original) {
            return CallWindowProc(hook.original, hwnd, msg, wParam, lParam);
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void InstallLayoutHook(HWND hwnd, const LayoutHook& hook) {
    if (!hwnd) return;
    auto it = g_layoutHooks.find(hwnd);
    if (it == g_layoutHooks.end()) {
        LayoutHook newHook = hook;
        newHook.original = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hwnd, GWLP_WNDPROC));
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(LayoutWndProc));
        g_layoutHooks.emplace(hwnd, newHook);
    } else {
        it->second = hook;
    }
}
}

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
    if (stretch <= 0) stretch = 1;
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

    int availableW = std::max(0, width - left - right);
    int availableH = std::max(0, height - top - bottom);
    int spacingTotal = m_spacing * static_cast<int>(m_items.size() > 0 ? (m_items.size() - 1) : 0);
    availableW = std::max(0, availableW - spacingTotal);

    int totalStretch = 0;
    for (const auto& item : m_items) totalStretch += item.stretch;
    if (totalStretch <= 0) totalStretch = static_cast<int>(m_items.size());

    int x = left;
    int remainingStretch = totalStretch;
    for (size_t i = 0; i < m_items.size(); ++i) {
        const auto& item = m_items[i];
        HWND child = item.widget ? item.widget->getHandle() : nullptr;
        if (!child) continue;

        int stretch = item.stretch;
        int w = (i == m_items.size() - 1)
            ? std::max(0, availableW - (x - left))
            : (availableW * stretch) / std::max(1, remainingStretch);
        SetWindowPos(child, nullptr, x, top, w, availableH, SWP_NOZORDER | SWP_NOACTIVATE);

        x += w + m_spacing;
        remainingStretch -= stretch;
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

    LayoutHook hook;
    hook.vertical = false;
    hook.hbox = this;
    InstallLayoutHook(host, hook);
}

NativeVBoxLayout::NativeVBoxLayout(NativeWidget* parent)
    : m_parent(parent), m_spacing(0)
{
    m_margins[0] = m_margins[1] = m_margins[2] = m_margins[3] = 0;
}

void NativeVBoxLayout::addWidget(NativeWidget* widget, int stretch)
{
    if (!widget) return;
    if (stretch <= 0) stretch = 1;
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

    int availableH = std::max(0, height - top - bottom);
    int availableW = std::max(0, width - left - right);
    int spacingTotal = m_spacing * static_cast<int>(m_items.size() > 0 ? (m_items.size() - 1) : 0);
    availableH = std::max(0, availableH - spacingTotal);

    int totalStretch = 0;
    for (const auto& item : m_items) totalStretch += item.stretch;
    if (totalStretch <= 0) totalStretch = static_cast<int>(m_items.size());

    int y = top;
    int remainingStretch = totalStretch;
    for (size_t i = 0; i < m_items.size(); ++i) {
        const auto& item = m_items[i];
        HWND child = item.widget ? item.widget->getHandle() : nullptr;
        if (!child) continue;

        int stretch = item.stretch;
        int h = (i == m_items.size() - 1)
            ? std::max(0, availableH - (y - top))
            : (availableH * stretch) / std::max(1, remainingStretch);
        SetWindowPos(child, nullptr, left, y, availableW, h, SWP_NOZORDER | SWP_NOACTIVATE);

        y += h + m_spacing;
        remainingStretch -= stretch;
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

    LayoutHook hook;
    hook.vertical = true;
    hook.vbox = this;
    InstallLayoutHook(host, hook);
}