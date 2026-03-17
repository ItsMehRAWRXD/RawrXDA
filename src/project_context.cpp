#include "../include/project_context.h"
#include <fstream>
#include <sstream>

namespace RawrXD {

ProjectContext::ProjectContext()
    : scanTimestamp(0)
{
}

void ProjectContext::addFileMetadata(const std::string& path, const std::string& description)
{
    fileMetadata[path] = description;
}

std::string ProjectContext::serializeToJson() const
{
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"workingDirectory\": \"" << workingDirectory << "\",\n";
    ss << "  \"files\": [\n";
    
    for (const auto& file : files) {
        ss << "    \"" << file << "\",\n";
    }
    
    ss << "  ],\n";
    ss << "  \"metadataCount\": " << fileMetadata.size() << "\n";
    ss << "}";
    
    return ss.str();
}

bool ProjectContext::loadFromDirectory(const std::string& path)
{
    workingDirectory = path;
    // In a real implementation, we would crawl the directory here
    return true;
}

} // namespace RawrXD
