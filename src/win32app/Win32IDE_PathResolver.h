#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include <shlobj.h>
#include <initguid.h>
#include <knownfolders.h>

namespace RawrXDPathResolver {

inline std::string WideToUTF8(const std::wstring& wideStr) {
    if (wideStr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

inline std::string GetKnownFolderPath(REFKNOWNFOLDERID folderId) {
    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(folderId, 0, nullptr, &path))) {
        std::wstring widePath(path);
        CoTaskMemFree(path);
        return WideToUTF8(widePath);
    }
    return "";
}

inline std::string GetUserDocumentsPath() {
    return GetKnownFolderPath(FOLDERID_Documents);
}

inline std::string GetUserDesktopPath() {
    return GetKnownFolderPath(FOLDERID_Desktop);
}

inline std::string GetAppDataPath() {
    return GetKnownFolderPath(FOLDERID_RoamingAppData);
}

inline std::string GetLocalAppDataPath() {
    return GetKnownFolderPath(FOLDERID_LocalAppData);
}

inline std::string GetTempPath() {
    wchar_t tempPath[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tempPath)) {
        return WideToUTF8(tempPath);
    }
    return "C:\\Temp";
}

inline std::vector<std::string> GetDefaultModelPaths() {
    return {
        "F:\\OllamaModels",
        "C:\\Users\\Public\\Models", 
        "D:\\Models",
        "E:\\Models",
        GetUserDocumentsPath() + "\\Models",
        GetAppDataPath() + "\\RawrXD\\Models"
    };
}

inline std::string GetExtensionsPath() {
    return GetAppDataPath() + "\\RawrXD\\Extensions";
}

inline std::string GetGlobalStoragePath() {
    return GetAppDataPath() + "\\RawrXD";
}

} // namespace RawrXDPathResolver
