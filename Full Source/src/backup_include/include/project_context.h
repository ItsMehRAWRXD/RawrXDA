/**
 * @file project_context.h
 * @brief Simple project context for working directory, file list, and metadata (C++20, no Qt)
 * Used by src/project_context.cpp. For full agentic project analysis see include/agentic/project_context.h
 */
#ifndef RAWRXD_PROJECT_CONTEXT_H
#define RAWRXD_PROJECT_CONTEXT_H

#include <string>
#include <vector>
#include <unordered_map>

namespace RawrXD {

class ProjectContext {
public:
    std::string workingDirectory;
    std::vector<std::string> files;
    std::unordered_map<std::string, std::string> fileMetadata;
    long long scanTimestamp = 0;

    ProjectContext();
    void addFileMetadata(const std::string& path, const std::string& description);
    std::string serializeToJson() const;
    bool loadFromDirectory(const std::string& path);
};

} // namespace RawrXD

#endif
