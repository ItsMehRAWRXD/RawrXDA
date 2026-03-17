// win32_file_dialog.h — Native Win32 Open/Save/Directory dialogs (no Qt)
// Use these so file-picker features (training dialog, metrics export, paint, etc.) work without Qt.
#pragma once

#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

namespace RawrXD {

// Optional: convert UTF-8 caption/filter to wide for Unicode APIs
inline std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    int need = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    if (need <= 0) return {};
    std::wstring out(need, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &out[0], need);
    return out;
}

inline std::string wideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int need = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    if (need <= 0) return {};
    std::string out(need, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &out[0], need, nullptr, nullptr);
    return out;
}

// Build wide filter from null-separated pairs (e.g. "Text\0*.txt\0All\0*.*\0")
inline std::wstring filterToWide(const char* filter) {
    if (!filter || !*filter) return std::wstring(L"All Files (*.*)\0*.*\0", 21);
    std::wstring out;
    const char* p = filter;
    while (*p) {
        std::string seg;
        while (*p) seg += *p++;
        out += utf8ToWide(seg);
        out += L'\0';
        ++p; // skip null to next segment
    }
    out += L'\0';
    return out;
}

/** Get path to a single file to open. Returns empty string if cancelled. */
inline std::string getOpenFileName(void* hwndOwner, const char* title, const char* filter,
    const std::string& initialDir = {}) {
    wchar_t path[MAX_PATH] = {};
    std::wstring wfilter = filterToWide(filter);
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner ? (HWND)hwndOwner : nullptr;
    ofn.lpstrFilter = wfilter.c_str();
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!initialDir.empty()) {
        std::wstring wdir = utf8ToWide(initialDir);
        ofn.lpstrInitialDir = wdir.c_str();
    }
    if (GetOpenFileNameW(&ofn)) return wideToUtf8(path);
    return {};
}

/** Get path to a single file to save. Returns empty string if cancelled. */
inline std::string getSaveFileName(void* hwndOwner, const char* title, const char* filter,
    const char* defaultExt, const std::string& initialDir = {}) {
    wchar_t path[MAX_PATH] = {};
    std::wstring wfilter = filterToWide(filter);
    std::wstring wdef = defaultExt ? utf8ToWide(defaultExt) : L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner ? (HWND)hwndOwner : nullptr;
    ofn.lpstrFilter = wfilter.c_str();
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = wdef.empty() ? nullptr : wdef.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!initialDir.empty()) {
        std::wstring wdir = utf8ToWide(initialDir);
        ofn.lpstrInitialDir = wdir.c_str();
    }
    if (GetSaveFileNameW(&ofn)) return wideToUtf8(path);
    return {};
}

/** Get existing directory path. Returns empty string if cancelled. */
inline std::string getExistingDirectory(void* hwndOwner, const char* title, const std::string& initialDir = {}) {
    BROWSEINFOW bi = {};
    bi.hwndOwner = hwndOwner ? (HWND)hwndOwner : nullptr;
    bi.lpszTitle = title ? (LPCWSTR)utf8ToWide(title).c_str() : L"Select Directory";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    wchar_t path[MAX_PATH] = {};
    if (!initialDir.empty()) {
        std::wstring wdir = utf8ToWide(initialDir);
        bi.lParam = (LPARAM)wdir.c_str();
    }
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return {};
    BOOL ok = SHGetPathFromIDListW(pidl, path);
    CoTaskMemFree(pidl);
    if (!ok) return {};
    return wideToUtf8(path);
}

} // namespace RawrXD
