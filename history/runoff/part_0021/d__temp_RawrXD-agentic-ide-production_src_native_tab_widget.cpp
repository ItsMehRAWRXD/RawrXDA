#include "windows_gui_framework.h"
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

NativeTabWidget::NativeTabWidget(NativeWidget* parent) : NativeWidget(parent) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);
    
    if (parent && parent->getHandle()) {
        // Create tab control
        m_hwnd = CreateWindow(
            WC_TABCONTROL, 
            "",
            WS_CHILD | WS_VISIBLE | TCS_TABS,
            0, 0, 400, 300,
            parent->getHandle(),
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
}

NativeTabWidget::~NativeTabWidget() {
    if (m_hwnd) {
        auto* impl = (NativeTabWidgetImpl*)GetWindowLongPtr(m_hwnd, GWLP_USERDATA);
        delete impl;
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
        if (widget && widget->getHandle()) {
            SetParent(widget->getHandle(), contentArea);
            widget->setGeometry(0, 0, tabRect.right - tabRect.left, tabRect.bottom - tabRect.top);
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

// Handle tab selection changes
LRESULT CALLBACK TabWidgetSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_NOTIFY) {
        NMHDR* pnmh = (NMHDR*)lParam;
        if (pnmh->code == TCN_SELCHANGE) {
            auto* impl = (NativeTabWidgetImpl*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
            if (impl) {
                int newIndex = TabCtrl_GetCurSel(hWnd);
                if (newIndex != impl->currentIndex && newIndex >= 0 && newIndex < (int)impl->tabs.size()) {
                    // Hide old content
                    if (impl->currentIndex >= 0 && impl->currentIndex < (int)impl->tabs.size()) {
                        ShowWindow(impl->tabs[impl->currentIndex].content, SW_HIDE);
                    }
                    
                    // Show new content
                    ShowWindow(impl->tabs[newIndex].content, SW_SHOW);
                    impl->currentIndex = newIndex;
                    
                    // Call callback
                    if (impl->onTabChanged) {
                        impl->onTabChanged(newIndex);
                    }
                }
            }
        }
    }
    
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// Initialize tab widget with subclassing for message handling
void NativeTabWidget::initializeTabHandling() {
    if (m_hwnd) {
        SetWindowSubclass(m_hwnd, TabWidgetSubclassProc, 1, 0);
    }
}