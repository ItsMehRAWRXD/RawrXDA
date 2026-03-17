#include "production_agentic_ide.h"
#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <iostream>

#pragma comment(lib, "comctl32.lib")

struct TabInfo {
    std::string title;
    NativeWidget* widget;
    HWND content;
};

class NativeTabWidgetImpl {
public:
    std::vector<TabInfo> tabs;
    int currentIndex = -1;
    std::function<void(int)> onTabChanged;
};

// Implementation for the forward-declared NativeTabWidget class
// Existing constructor that accepts a NativeWidget parent (may be nullptr).
NativeTabWidget::NativeTabWidget(NativeWidget* parent) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);
    
    HWND parentHwnd = nullptr;
    if (parent) {
        parentHwnd = parent->getHandle();
    }
    
    // Create tab control
    m_hwnd = CreateWindow(
        WC_TABCONTROL,
        "",
        WS_CHILD | WS_VISIBLE | TCS_TABS,
        0, 0, 400, 300,
        parentHwnd,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    
    if (m_hwnd) {
        // Create implementation storage
        auto* impl = new NativeTabWidgetImpl();
        SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)impl);
        
        std::cout << "NativeTabWidget created successfully" << std::endl;
    } else {
        std::cout << "Failed to create NativeTabWidget: " << GetLastError() << std::endl;
    }
}

NativeTabWidget::~NativeTabWidget() {
    if (m_hwnd) {
        auto* impl = (NativeTabWidgetImpl*)GetWindowLongPtr(m_hwnd, GWLP_USERDATA);
        delete impl;
        DestroyWindow(m_hwnd);
    }
}

void NativeTabWidget::addTab(const std::string& title, NativeWidget* widget) {
    if (!m_hwnd) return;
    
    auto* impl = (NativeTabWidgetImpl*)GetWindowLongPtr(m_hwnd, GWLP_USERDATA);
    if (!impl) return;
    
    // Add tab to tab control
    TCITEM tie;
    tie.mask = TCIF_TEXT;
    tie.pszText = const_cast<char*>(title.c_str());
    
    int tabIndex = TabCtrl_InsertItem(m_hwnd, impl->tabs.size(), &tie);
    
    if (tabIndex >= 0) {
        // Create content area for this tab
        RECT tabRect;
        GetClientRect(m_hwnd, &tabRect);
        TabCtrl_AdjustRect(m_hwnd, FALSE, &tabRect);
        
        HWND contentArea = CreateWindow(
            "STATIC",
            "",
            WS_CHILD | WS_VISIBLE,
            tabRect.left, tabRect.top,
            tabRect.right - tabRect.left,
            tabRect.bottom - tabRect.top,
            m_hwnd,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr
        );
        
        // Store tab info
        TabInfo tabInfo;
        tabInfo.title = title;
        tabInfo.widget = widget;
        tabInfo.content = contentArea;
        impl->tabs.push_back(tabInfo);
        
        // If this is the first tab, make it active
        if (impl->tabs.size() == 1) {
            TabCtrl_SetCurSel(m_hwnd, 0);
            impl->currentIndex = 0;
            ShowWindow(contentArea, SW_SHOW);
        } else {
            ShowWindow(contentArea, SW_HIDE);
        }
        
        // Position the widget within the content area if provided
        if (widget) {
            SetParent(widget->getHandle(), contentArea);
            // Simple positioning
            SetWindowPos(widget->getHandle(), nullptr, 0, 0,
                        tabRect.right - tabRect.left,
                        tabRect.bottom - tabRect.top,
                        SWP_NOZORDER);
            // Ensure the widget is visible after reparenting
            ShowWindow(widget->getHandle(), SW_SHOW);
        }
        
        std::cout << "Added tab: " << title << " (index " << tabIndex << ")" << std::endl;
    }
}

void NativeTabWidget::removeTab(int index) {
    if (!m_hwnd) return;
    
    auto* impl = (NativeTabWidgetImpl*)GetWindowLongPtr(m_hwnd, GWLP_USERDATA);
    if (!impl || index < 0 || index >= (int)impl->tabs.size()) return;
    
    // Remove from tab control
    TabCtrl_DeleteItem(m_hwnd, index);
    
    // Destroy content area
    if (impl->tabs[index].content) {
        DestroyWindow(impl->tabs[index].content);
    }
    
    // Remove from vector
    impl->tabs.erase(impl->tabs.begin() + index);
    
    // Adjust current index
    if (impl->currentIndex >= index) {
        impl->currentIndex--;
        if (impl->currentIndex < 0 && !impl->tabs.empty()) {
            impl->currentIndex = 0;
        }
    }
    
    std::cout << "Removed tab at index: " << index << std::endl;
}

int NativeTabWidget::getCurrentIndex() const {
    if (!m_hwnd) return -1;
    
    auto* impl = (NativeTabWidgetImpl*)GetWindowLongPtr(m_hwnd, GWLP_USERDATA);
    if (!impl) return -1;
    
    return impl->currentIndex;
}

void NativeTabWidget::setOnTabChanged(std::function<void(int)> callback) {
    if (!m_hwnd) return;
    
    auto* impl = (NativeTabWidgetImpl*)GetWindowLongPtr(m_hwnd, GWLP_USERDATA);
    if (!impl) return;
    
    impl->onTabChanged = callback;
}

void NativeTabWidget::setGeometry(int x, int y, int width, int height) {
    if (!m_hwnd) return;

    SetWindowPos(m_hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);

    auto* impl = (NativeTabWidgetImpl*)GetWindowLongPtr(m_hwnd, GWLP_USERDATA);
    if (!impl) return;

    RECT tabRect;
    GetClientRect(m_hwnd, &tabRect);
    TabCtrl_AdjustRect(m_hwnd, FALSE, &tabRect);

    for (auto& t : impl->tabs) {
        if (t.content) {
            SetWindowPos(
                t.content,
                nullptr,
                tabRect.left,
                tabRect.top,
                tabRect.right - tabRect.left,
                tabRect.bottom - tabRect.top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        if (t.widget && t.widget->getHandle()) {
            SetWindowPos(
                t.widget->getHandle(),
                nullptr,
                0,
                0,
                tabRect.right - tabRect.left,
                tabRect.bottom - tabRect.top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
}

HWND NativeTabWidget::getHandle() const {
    return m_hwnd;
}

void NativeTabWidget::initializeTabHandling() {
    // Tab handling would be set up here for proper message processing
    std::cout << "Tab handling initialized" << std::endl;
}

void NativeTabWidget::handleSelectionChanged() {
    if (!m_hwnd) return;

    auto* impl = (NativeTabWidgetImpl*)GetWindowLongPtr(m_hwnd, GWLP_USERDATA);
    if (!impl) return;

    int sel = TabCtrl_GetCurSel(m_hwnd);
    if (sel < 0 || sel >= (int)impl->tabs.size()) return;

    impl->currentIndex = sel;
    for (int i = 0; i < (int)impl->tabs.size(); ++i) {
        HWND content = impl->tabs[i].content;
        if (!content) continue;
        ShowWindow(content, (i == sel) ? SW_SHOW : SW_HIDE);
    }

    if (impl->onTabChanged) {
        impl->onTabChanged(sel);
    }
}

// New overload that accepts a raw HWND as the parent window.
NativeTabWidget::NativeTabWidget(HWND parentHwnd) : NativeWidget(static_cast<NativeWidget*>(nullptr)) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);

    // Use CreateWindowEx to ensure proper extended style handling.
    m_hwnd = CreateWindowEx(
        0,
        WC_TABCONTROL,
        "",
        WS_CHILD | WS_VISIBLE | TCS_TABS,
        0, 0, 400, 300,
        parentHwnd,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );

    if (m_hwnd) {
        auto* impl = new NativeTabWidgetImpl();
        SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)impl);
        std::cout << "NativeTabWidget created successfully (HWND parent)" << std::endl;
    } else {
        std::cout << "Failed to create NativeTabWidget (HWND parent): " << GetLastError() << std::endl;
    }
}