#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace RawrXD {

class ProjectContext {
public:
    ProjectContext();

    void addFileMetadata(const std::string& path, const std::string& description);
    std::string serializeToJson() const;
    bool loadFromDirectory(const std::string& path);

    std::string workingDirectory;
    std::vector<std::string> files;
    std::map<std::string, std::string> fileMetadata;
    int64_t scanTimestamp = 0;
};

} // namespace RawrXD
