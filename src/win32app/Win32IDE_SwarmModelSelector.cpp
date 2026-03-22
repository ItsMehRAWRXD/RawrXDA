// Win32IDE_SwarmModelSelector.cpp — Local swarm-capable model enumeration
#include "Win32IDE_SwarmModelSelector.h"

#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

namespace RawrXD {
namespace Win32App {

namespace {

std::string wideToUtf8(PCWSTR wstr) {
    if (!wstr) {
        return {};
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) {
        return {};
    }
    std::string out(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int nw = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (nw <= 1) {
        return {};
    }
    std::wstring w(static_cast<size_t>(nw), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, w.data(), nw);
    return w;
}

void appendGgufFromDirectory(const std::wstring& dirW, std::vector<std::string>& out, unsigned maxEntries) {
    if (out.size() >= maxEntries || dirW.empty()) {
        return;
    }
    const std::wstring pattern = dirW + L"\\*.gguf";
    WIN32_FIND_DATAW fd{};
    const HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        const std::wstring full = dirW + L"\\" + fd.cFileName;
        out.push_back(wideToUtf8(full.c_str()));
    } while (out.size() < maxEntries && FindNextFileW(h, &fd) != 0);
    FindClose(h);
}

} // namespace

std::vector<std::string> enumerateSwarmModelCandidates(unsigned maxEntries) {
    std::vector<std::string> out;
    out.reserve(std::min(maxEntries, 512u));

    std::wstring rootW;
    if (const char* env = std::getenv("RAWRXD_SWARM_MODEL_DIR")) {
        rootW = utf8ToWide(env);
    } else {
        wchar_t profile[MAX_PATH]{};
        const DWORD n = GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) {
            return out;
        }
        rootW = std::wstring(profile) + L"\\.ollama\\models";
    }

    appendGgufFromDirectory(rootW, out, maxEntries);
    return out;
}

} // namespace Win32App
} // namespace RawrXD
