#include "LayoutManager.h"
#include "Resource.h"
#include <windows.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include <fstream>
#include <map>
#include <algorithm>
#include <sstream>

// This source file is only a stub; the full implementation would manage layout deletion/restoration.

// Terminal globals
HWND gTerminalPane = NULL;
HANDLE gTermInWrite = NULL;
HANDLE gTermOutRead = NULL;
HANDLE gTermThread = NULL;

// Terminal input box for direct typing
HWND gTerminalInput = NULL;

// Embedded terminal launcher with input
void LaunchEmbeddedTerminal(HWND hwnd, const wchar_t* app) {
    if (gTerminalPane) DestroyWindow(gTerminalPane);
    if (gTerminalInput) DestroyWindow(gTerminalInput);
    gTerminalPane = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        0, 400, 800, 170, hwnd, (HMENU)9001, GetModuleHandle(NULL), NULL);
    gTerminalInput = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", NULL,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 570, 800, 30, hwnd, (HMENU)9002, GetModuleHandle(NULL), NULL);

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hInRead, hOutWrite;
    CreatePipe(&hInRead, &gTermInWrite, &sa, 0);
    CreatePipe(&gTermOutRead, &hOutWrite, &sa, 0);
    SetHandleInformation(gTermInWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(gTermOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = hInRead;
    si.hStdOutput = hOutWrite;
    si.hStdError  = hOutWrite;
    CreateProcessW(NULL, (LPWSTR)app, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hInRead);
    CloseHandle(hOutWrite);

    gTermThread = CreateThread(NULL, 0, [](LPVOID param)->DWORD {
        HWND hEdit = (HWND)param;
        char buffer[256];
        DWORD read;
        while (ReadFile(gTermOutRead, buffer, sizeof(buffer)-1, &read, NULL) && read > 0) {
            buffer[read] = 0;
            int wlen = MultiByteToWideChar(CP_UTF8, 0, buffer, read, NULL, 0);
            std::wstring wtext(wlen, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, buffer, read, &wtext[0], wlen);
            int len = GetWindowTextLength(hEdit);
            SendMessage(hEdit, EM_SETSEL, len, len);
            SendMessage(hEdit, EM_REPLACESEL, FALSE, (LPARAM)wtext.c_str());
        }
        return 0;
    }, gTerminalPane, 0, NULL);
}

// Globals for Layout Manager
static HWND gLayoutManager = NULL;
static HWND gLayoutList = NULL;
static HWND gTrashList = NULL;
static HWND gPreviewPanel = NULL;
static HWND gFilterBox = NULL;
static LayoutPreview gPreview;
static std::vector<DeletedLayout> gDeletedLayouts;
static TrashSortMode gTrashSort = SORT_BY_TIME;

// List saved layouts
std::vector<std::wstring> ListSavedLayouts(const std::wstring& solutionName) {
    std::vector<std::wstring> layouts;
    WIN32_FIND_DATAW fd;
    std::wstring search = solutionName + L".depgraph.*.json";
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::wstring fname = fd.cFileName;
            size_t p1 = fname.find(L".depgraph.");
            size_t p2 = fname.rfind(L".json");
            if (p1 != std::wstring::npos && p2 != std::wstring::npos) {
                std::wstring layout = fname.substr(p1+10, p2-(p1+10));
                layouts.push_back(layout);
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    return layouts;
}

// Refresh trash bin list
void RefreshTrashList(HWND hwnd) {
    HWND hTrashList = GetDlgItem(hwnd,2);
    HWND hFilterBox = GetDlgItem(hwnd,3);
    SendMessageW(hTrashList, LB_RESETCONTENT, 0, 0);
    wchar_t filter[128];
    GetWindowTextW(hFilterBox, filter, 128);
    std::wstring filterText = filter;
    std::transform(filterText.begin(), filterText.end(), filterText.begin(), ::towlower);
    std::vector<DeletedLayout> copy = gDeletedLayouts;
    if (gTrashSort == SORT_BY_NAME) {
        std::sort(copy.begin(), copy.end(), [](const DeletedLayout& a, const DeletedLayout& b){
            return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
        });
    } else {
        std::sort(copy.begin(), copy.end(), [](const DeletedLayout& a, const DeletedLayout& b){
            FILETIME fa, fb;
            SystemTimeToFileTime(&a.deletedAt, &fa);
            SystemTimeToFileTime(&b.deletedAt, &fb);
            return CompareFileTime(&fb, &fa) < 0;
        });
    }
    for (auto& d : copy) {
        std::wstring nameLower = d.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::towlower);
        if (filterText.empty() || nameLower.find(filterText) != std::wstring::npos) {
            wchar_t entry[256];
            swprintf(entry, 256, L"%s (deleted %04d-%02d-%02d %02d:%02d:%02d)",
                     d.name.c_str(),
                     d.deletedAt.wYear, d.deletedAt.wMonth, d.deletedAt.wDay,
                     d.deletedAt.wHour, d.deletedAt.wMinute, d.deletedAt.wSecond);
            SendMessageW(hTrashList, LB_ADDSTRING, 0, (LPARAM)entry);
        }
    }
}

// Main Layout Manager window
void ShowLayoutManager(HWND parent) {
    if (gLayoutManager) {
        ShowWindow(gLayoutManager, SW_SHOW);
        SetForegroundWindow(gLayoutManager);
        return;
    }
    WNDCLASS wc = {0};
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        switch (msg) {
        case WM_CREATE: {
            gLayoutList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTBOX, NULL,
                WS_CHILD | WS_VISIBLE | LBS_STANDARD,
                10, 10, 300, 200, hwnd, (HMENU)1, GetModuleHandle(NULL), NULL);
            std::vector<std::wstring> layouts = ListSavedLayouts(L"MySolution");
            for (auto& l : layouts) {
                SendMessageW(gLayoutList, LB_ADDSTRING, 0, (LPARAM)l.c_str());
            }
            gTrashList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTBOX, NULL,
                WS_CHILD | WS_VISIBLE | LBS_EXTENDEDSEL | WS_VSCROLL | LBS_NOTIFY,
                10, 380, 300, 100, hwnd, (HMENU)2, GetModuleHandle(NULL), NULL);
            gFilterBox = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", NULL,
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                10, 350, 300, 20, hwnd, (HMENU)3, GetModuleHandle(NULL), NULL);
            gPreviewPanel = CreateWindowEx(WS_EX_CLIENTEDGE, L"STATIC", NULL,
                WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                10, 220, 400, 150, hwnd, (HMENU)2001, GetModuleHandle(NULL), NULL);
            CreateWindow(L"BUTTON", L"Restore", WS_CHILD|WS_VISIBLE, 320, 380, 80, 25, hwnd, (HMENU)1005, GetModuleHandle(NULL), NULL);
            CreateWindow(L"BUTTON", L"Clear Trash", WS_CHILD|WS_VISIBLE, 320, 410, 80, 25, hwnd, (HMENU)1006, GetModuleHandle(NULL), NULL);
            CreateWindow(L"BUTTON", L"Sort by Name", WS_CHILD|WS_VISIBLE, 320, 440, 100, 25, hwnd, (HMENU)1007, GetModuleHandle(NULL), NULL);
            CreateWindow(L"BUTTON", L"Sort by Time", WS_CHILD|WS_VISIBLE, 320, 470, 100, 25, hwnd, (HMENU)1008, GetModuleHandle(NULL), NULL);
        } break;
        case WM_COMMAND: {
            if (LOWORD(wParam) == 3 && HIWORD(wParam) == EN_CHANGE) {
                RefreshTrashList(hwnd);
            }
            if (LOWORD(wParam) == 1007) { gTrashSort = SORT_BY_NAME; RefreshTrashList(hwnd); }
            if (LOWORD(wParam) == 1008) { gTrashSort = SORT_BY_TIME; RefreshTrashList(hwnd); }
            // Add more handlers for restore, clear trash, etc.
        } break;
        case WM_CLOSE:
            DestroyWindow(hwnd); gLayoutManager = NULL; break;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    };
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"LayoutMgrWnd";
    RegisterClass(&wc);
    gLayoutManager = CreateWindow(L"LayoutMgrWnd", L"Layout Manager",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 300, 300, 450, 600,
        parent, NULL, GetModuleHandle(NULL), NULL);
}
