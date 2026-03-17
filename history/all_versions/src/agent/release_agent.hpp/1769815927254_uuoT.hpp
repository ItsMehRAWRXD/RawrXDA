#pragma once

#include <string>
#include <functional>
#include <vector>

class ReleaseAgent {
public:
    explicit ReleaseAgent();
    virtual ~ReleaseAgent() = default;

    // Bump version in CMakeLists.txt (major/minor/patch)
    bool bumpVersion(const std::string& part);

    // Git tag and upload to GitHub
    bool tagAndUpload();

    // Tweet announcement
    bool tweet(const std::string& text);

    // Extended autonomous release helpers
    bool signBinary(const std::string& exePath);
    bool uploadToCDN(const std::string& localFile, const std::string& blobName);
    bool createGitHubRelease(const std::string& tag, const std::string& changelog);
    bool updateUpdateManifest(const std::string& tag, const std::string& sha256);
    bool tweetRelease(const std::string& text);

    // Get current version string
    std::string version() const { return m_version; }

    // Set changelog for release notes
    void setChangelog(const std::string& changelog) { m_changelog = changelog; }

    // Callbacks
    std::function<void(const std::string&)> onVersionBumped;
    std::function<void(const std::string&)> onReleaseCreated;
    std::function<void(const std::string&)> onTweetSent;
    std::function<void(const std::string&)> onError;

private:
    // Helper for running external processes
    struct ProcessResult {
        int exitCode = -1;
        std::string stdOut;
        std::string stdErr;
    };
    ProcessResult runProcess(const std::string& command, const std::vector<std::string>& args);

    // Network helper placeholder
    std::string performHttpRequest(const std::string& url, 
                                   const std::string& method, 
                                   const std::string& body, 
                                   const std::vector<std::pair<std::string, std::string>>& headers);

    std::string m_version;
    std::string m_changelog;
    std::string m_lastError;
};
