#include "agentic_executor.h"

#include <windows.h>
#include <fstream>

std::string AgenticExecutor::readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    std::string out;
    file.seekg(0, std::ios::end);
    const auto sz = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    out.resize(sz);
    if (sz > 0) {
        file.read(out.data(), static_cast<std::streamsize>(sz));
    }
    return out;
}

std::vector<std::string> AgenticExecutor::listDirectory(const std::string& path)
{
    std::vector<std::string> entries;

    std::string pattern = path;
    if (!pattern.empty() && pattern.back() != '\\' && pattern.back() != '/') {
        pattern += "\\\\";
    }
    pattern += "*";

    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return entries;
    }

    do {
        const char* n = fd.cFileName;
        if (std::strcmp(n, ".") == 0 || std::strcmp(n, "..") == 0) {
            continue;
        }
        entries.emplace_back(n);
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return entries;
}
