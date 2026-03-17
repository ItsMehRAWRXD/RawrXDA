#include "native_ide.h"

namespace IDEUtils {

std::wstring GetExecutableDirectory() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);
    return std::wstring(exePath);
}

std::wstring GetFileExtension(const std::wstring& filename) {
    size_t pos = filename.find_last_of(L'.');
    if (pos != std::wstring::npos && pos < filename.length() - 1) {
        return filename.substr(pos);
    }
    return L"";
}

std::wstring GetFileName(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

std::wstring GetDirectoryFromPath(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return path.substr(0, pos);
    }
    return L"";
}

std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
}

std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

bool PathExists(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES;
}

bool IsDirectory(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return (attributes != INVALID_FILE_ATTRIBUTES) && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

std::vector<std::wstring> SplitString(const std::wstring& str, wchar_t delimiter) {
    std::vector<std::wstring> result;
    std::wstringstream ss(str);
    std::wstring item;
    
    while (std::getline(ss, item, delimiter)) {
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    
    return result;
}

std::wstring JoinPath(const std::wstring& path1, const std::wstring& path2) {
    if (path1.empty()) return path2;
    if (path2.empty()) return path1;
    
    wchar_t separator = L'\\';
    if (path1.back() == L'\\' || path1.back() == L'/') {
        return path1 + path2;
    }
    
    return path1 + separator + path2;
}

std::string DetectLanguageFromExtension(const std::wstring& extension) {
    std::wstring ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    
    if (ext == L".cpp" || ext == L".cc" || ext == L".cxx") return "cpp";
    if (ext == L".c") return "c";
    if (ext == L".h" || ext == L".hpp" || ext == L".hxx") return "cpp";
    if (ext == L".asm" || ext == L".s") return "assembly";
    if (ext == L".py") return "python";
    if (ext == L".js") return "javascript";
    if (ext == L".ts") return "typescript";
    if (ext == L".java") return "java";
    if (ext == L".cs") return "csharp";
    if (ext == L".rs") return "rust";
    if (ext == L".go") return "go";
    if (ext == L".txt" || ext == L".md") return "text";
    if (ext == L".json") return "json";
    if (ext == L".xml") return "xml";
    if (ext == L".html" || ext == L".htm") return "html";
    if (ext == L".css") return "css";
    
    return "text";  // Default fallback
}

COLORREF ColorFromHex(uint32_t hex) {
    return RGB((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

} // namespace IDEUtils