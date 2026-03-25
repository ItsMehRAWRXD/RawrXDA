#include "agentic_executor.h"

#include <fstream>

std::string AgenticExecutor::readFile(const std::string& path)
{
    auto safe = safePath(path);
    if (!isPathSafe(safe)) {
        errorOccurred("Path traversal blocked: " + path);
        return "";
    }

    std::ifstream file(safe, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    std::string out;
    file.seekg(0, std::ios::end);
    out.resize(static_cast<size_t>(file.tellg()));
    file.seekg(0, std::ios::beg);
    file.read(out.data(), static_cast<std::streamsize>(out.size()));
    return out;
}

std::vector<std::string> AgenticExecutor::listDirectory(const std::string& path)
{
    auto safe = safePath(path);
    if (!isPathSafe(safe)) {
        errorOccurred("Path traversal blocked: " + path);
        return {};
    }

    std::vector<std::string> entries;
    try {
        if (std::filesystem::exists(safe) && std::filesystem::is_directory(safe)) {
            for (const auto& entry : std::filesystem::directory_iterator(safe)) {
                entries.push_back(entry.path().filename().string());
            }
        }
    } catch (...) {
        return {};
    }

    return entries;
}
