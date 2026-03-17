/**
 * @file RawrXD_SovereignIDE.cpp
 * @brief Pure Win32/C++20 IDE Entry Point - Zero Qt Dependencies
 * @integrated Live schematic approval system for layout design
 */

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <iostream>
#include <string>
#include <vector>

#include "agent/RawrXD_AgentKernel.hpp"
#include "agent/μ_live_controller.hpp"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

using namespace RawrXD::Agent;

// Global Agent Kernel
AgentKernel g_kernel;

// Live controller
LiveIDEController g_live_controller;

// Window Procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            InitCommonControls();
            
            // Wait for live schematic approval before initializing IDE components
            g_live_controller.Initialize(hwnd);
            
            // Message loop to process approval window
            MSG msg = {};
            while (g_live_controller.IsWaiting() && GetMessage(&msg, nullptr, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            
            // Now create IDE components with approved layout
            if (!g_live_controller.IsCommitted()) {
                MessageBoxW(hwnd, L"Layout approval cancelled. Exiting.", L"Cancelled", MB_OK);
                PostQuitMessage(0);
                return 0;
            }
            
            // Initialize Agent Kernel
            UI::AgentWindowBridge::initialize(hwnd);
            AgentKernel::Config config;
            config.max_concurrent_tasks = 4;
            g_kernel.initialize(config);
            
            // Create menu bar
            HMENU hMenu = CreateMenu();
            
            HMENU hFile = CreatePopupMenu();
            AppendMenuW(hFile, MF_STRING, 1001, L"&New File\tCtrl+N");
            AppendMenuW(hFile, MF_STRING, 1002, L"&Open File\tCtrl+O");
            AppendMenuW(hFile, MF_STRING, 1003, L"&Save\tCtrl+S");
            AppendMenuW(hFile, MF_STRING, 1004, L"Save &As\tCtrl+Shift+S");
            AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hFile, MF_STRING, 1005, L"E&xit");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFile, L"&File");
            
            HMENU hEdit = CreatePopupMenu();
            AppendMenuW(hEdit, MF_STRING, 2001, L"&Undo\tCtrl+Z");
            AppendMenuW(hEdit, MF_STRING, 2002, L"&Redo\tCtrl+Y");
            AppendMenuW(hEdit, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hEdit, MF_STRING, 2003, L"Cu&t\tCtrl+X");
            AppendMenuW(hEdit, MF_STRING, 2004, L"&Copy\tCtrl+C");
            AppendMenuW(hEdit, MF_STRING, 2005, L"&Paste\tCtrl+V");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hEdit, L"&Edit");
            
            HMENU hView = CreatePopupMenu();
            AppendMenuW(hView, MF_STRING, 3001, L"Toggle &Explorer\tCtrl+B");
            AppendMenuW(hView, MF_STRING, 3002, L"Toggle &Chat\tCtrl+I");
            AppendMenuW(hView, MF_STRING, 3003, L"Toggle &Settings\tCtrl+,");
            AppendMenuW(hView, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hView, MF_STRING, 3004, L"&Full Screen\tF11");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hView, L"&View");
            
            HMENU hTerminal = CreatePopupMenu();
            AppendMenuW(hTerminal, MF_STRING, 4001, L"&New Terminal\tCtrl+`");
            AppendMenuW(hTerminal, MF_STRING, 4002, L"&Run Build\tCtrl+Shift+B");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTerminal, L"&Terminal");
            
            HMENU hHelp = CreatePopupMenu();
            AppendMenuW(hHelp, MF_STRING, 5001, L"&Documentation");
            AppendMenuW(hHelp, MF_STRING, 5002, L"&About");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelp, L"&Help");
            
            SetMenu(hwnd, hMenu);
            
            // Create child windows for approved layout
            CreateWindowExW(
                WS_EX_CLIENTEDGE, WC_TREEVIEWW, nullptr,
                WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
                0, 48, 250, 552, hwnd, (HMENU)0x1000, GetModuleHandleW(nullptr), nullptr
            );
            
            CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE,
                250, 48, 700, 552, hwnd, (HMENU)0x8000, GetModuleHandleW(nullptr), nullptr
            );
            
            CreateWindowExW(
                0, L"STATIC", nullptr,
                WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
                950, 48, 250, 552, hwnd, (HMENU)0x6000, GetModuleHandleW(nullptr), nullptr
            );
            
            CreateWindowExW(
                0, STATUSCLASSNAMEW, nullptr,
                WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                0, 0, 0, 0, hwnd, (HMENU)0x9000, GetModuleHandleW(nullptr), nullptr
            );
            
            CreateWindowExW(
                WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                250, 600, 700, 180, hwnd, (HMENU)0x7000, GetModuleHandleW(nullptr), nullptr
            );
            
            return 0;
        }
        
        case UI::AgentWindowBridge::WM_AGENT_EVENT: {
            return UI::AgentWindowBridge::handle_message(hwnd, uMsg, wParam, lParam);
        }
        
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            
            // File menu
            if (wmId == 1001) {
                // New File
                HWND hEdit = GetDlgItem(hwnd, 0x8000);
                if (hEdit) SetWindowTextW(hEdit, L"");
            }
            else if (wmId == 1003) {
                // Save - Ctrl+S implementation
                HWND hEdit = GetDlgItem(hwnd, 0x8000);
                if (hEdit) {
                    int len = GetWindowTextLengthW(hEdit) + 1;
                    wchar_t* buf = new wchar_t[len];
                    GetWindowTextW(hEdit, buf, len);
                    
                    OPENFILENAMEW ofn = {};
                    wchar_t filename[MAX_PATH] = L"untitled.txt";
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFile = filename;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.lpstrFilter = L"Text Files\0*.txt\0All Files\0*.*\0";
                    ofn.Flags = OFN_OVERWRITEPROMPT;
                    
                    if (GetSaveFileNameW(&ofn)) {
                        HANDLE hFile = CreateFileW(filename, GENERIC_WRITE, 0, nullptr, 
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                        if (hFile != INVALID_HANDLE_VALUE) {
                            DWORD written;
                            WriteFile(hFile, buf, len * sizeof(wchar_t), &written, nullptr);
                            CloseHandle(hFile);
                            
                            wchar_t msg[512];
                            swprintf(msg, 512, L"Saved to %s", filename);
                            MessageBoxW(hwnd, msg, L"Saved", MB_OK);
                        }
                    }
                    delete[] buf;
                }
            }
            else if (wmId == 1005) {
                // Exit
                PostQuitMessage(0);
            }
            
            // View menu
            else if (wmId == 3001) {
                HWND hTree = GetDlgItem(hwnd, 0x1000);
                if (hTree) {
                    bool visible = IsWindowVisible(hTree);
                    ShowWindow(hTree, visible ? SW_HIDE : SW_SHOW);
                }
            }
            else if (wmId == 3003) {
                HWND hSettings = GetDlgItem(hwnd, 0x6000);
                if (hSettings) {
                    bool visible = IsWindowVisible(hSettings);
                    ShowWindow(hSettings, visible ? SW_HIDE : SW_SHOW);
                }
            }
            
            return 0;
        }
        
        case WM_SIZE: {
            int w = (int)(short)LOWORD(lParam);
            int h = (int)(short)HIWORD(lParam);
            
            // Reposition children according to approved layout
            HWND hTree = GetDlgItem(hwnd, 0x1000);
            HWND hEdit = GetDlgItem(hwnd, 0x8000);
            HWND hSettings = GetDlgItem(hwnd, 0x6000);
            HWND hChat = GetDlgItem(hwnd, 0x7000);
            HWND hStatus = GetDlgItem(hwnd, 0x9000);
            
            HDWP hdwp = BeginDeferWindowPos(5);
            
            if (hTree) {
                DeferWindowPos(hdwp, hTree, nullptr, 0, 48, 250, h - 72, SWP_NOZORDER);
            }
            if (hEdit) {
                DeferWindowPos(hdwp, hEdit, nullptr, 250, 48, w - 500, h - 228, SWP_NOZORDER);
            }
            if (hSettings) {
                DeferWindowPos(hdwp, hSettings, nullptr, w - 250, 48, 250, h - 72, SWP_NOZORDER);
            }
            if (hStatus) {
                DeferWindowPos(hdwp, hStatus, nullptr, 0, h - 24, w, 24, SWP_NOZORDER);
            }
            if (hChat) {
                DeferWindowPos(hdwp, hChat, nullptr, 250, h - 180, w - 500, 180, SWP_NOZORDER);
            }
            
            EndDeferWindowPos(hdwp);
            return 0;
        }
        
        case WM_DESTROY: {
            g_kernel.shutdown();
            UI::AgentWindowBridge::shutdown();
            PostQuitMessage(0);
            return 0;
        }
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    // Register the window class.
    const wchar_t CLASS_NAME[] = L"RawrXD Sovereign IDE";
    
    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
    wc.style         = CS_VREDRAW | CS_HREDRAW;

    RegisterClassW(&wc);

    // Create the window.
    HWND hwnd = CreateWindowExW(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"RawrXD Sovereign IDE - Layout Approval System",  // Window text
        WS_OVERLAPPEDWINDOW,            // Window style
        CW_USEDEFAULT, CW_USEDEFAULT, 1400, 900,
        NULL,       // Parent window    
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Run the message loop.
    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
