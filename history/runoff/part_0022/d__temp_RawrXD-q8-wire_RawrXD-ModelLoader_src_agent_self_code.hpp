#pragma once
#include <string>
#include <vector>

namespace RawrXD {

class SelfCode {
public:
    // High-level helpers
    bool editSource(const std::string& filePath,
                    const std::string& oldSnippet,
                    const std::string& newSnippet);

    bool addInclude(const std::string& hppFile,
                    const std::string& includeLine);

    // Legacy: No longer needed in Sovereign build
    bool regenerateMOC(const std::string& /*header*/) { return true; }

    bool rebuildTarget(const std::string& target,
                       const std::string& config = "Release");

    std::string lastError() const { return m_lastError; }

private:
    std::string m_lastError;

    // Low-level helpers
    bool replaceInFile(const std::string& path,
                       const std::string& oldText,
                       const std::string& newText);
    bool insertAfterIncludeGuard(const std::string& hpp,
                                 const std::string& includeLine);
    bool runProcess(const std::string& program,
                    const std::vector<std::string>& args);
};

} // namespace RawrXD
