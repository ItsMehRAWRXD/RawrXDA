// win32_file_dialog.h — Native Win32 Open/Save dialogs (no Qt)
#pragma once

#include <string>
#include <windows.h>
#include <commdlg.h>

namespace RawrXD {

inline std::string getOpenFileName(void* hwndOwner, const char* title, const char* filter) {
    (void)title;
    char path[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner ? (HWND)hwndOwner : nullptr;
    ofn.lpstrFilter = filter ? filter : "All Files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) return path;
    return {};
}

inline std::string getSaveFileName(void* hwndOwner, const char* title, const char* filter, const char* defaultExt) {
    (void)title;
    char path[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner ? (HWND)hwndOwner : nullptr;
    ofn.lpstrFilter = filter ? filter : "All Files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = defaultExt ? defaultExt : "";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (GetSaveFileNameA(&ofn)) return path;
    return {};
}

} // namespace RawrXD
