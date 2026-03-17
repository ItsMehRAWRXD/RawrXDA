#pragma once
#include <string>
#include <vector>

class ReleaseAgent {
public:
    explicit ReleaseAgent();
    
    // Bump version in CMakeLists.txt (major/minor/patch)
    bool bumpVersion(const std::string& part);
    
    // Git tag and upload to GitHub
    bool tagAndUpload();
    
    // Tweet announcement (requires TWITTER_BEARER env var)
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

private:
    std::string m_version = "v1.0.0";
    std::string m_changelog = "Automated release";
    std::string m_lastError;
};
