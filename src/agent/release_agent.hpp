#pragma once
#include <string>
#include <functional>

class ReleaseAgent {
public:
    ReleaseAgent();
    bool bumpVersion(const std::string& part);
    bool tagAndUpload();
    bool tweet(const std::string& text);
    bool signBinary(const std::string& exePath);
    bool uploadToCDN(const std::string& localFile, const std::string& blobName);
    bool createGitHubRelease(const std::string& tag, const std::string& changelog);
    bool updateUpdateManifest(const std::string& tag, const std::string& sha256);
    bool tweetRelease(const std::string& text);
    std::string version() const { return m_version; }
    void setChangelog(const std::string& changelog) { m_changelog = changelog; }

    // ── GOLD_SIGN: EV certificate signing for production release ─────
    /// Sign binary with EV certificate (hardware-bound key).
    /// Falls back to standard signBinary() if no EV cert is configured.
    bool goldSignBinary(const std::string& exePath);

    /// Batch GOLD_SIGN all binaries in a build directory.
    /// Writes GOLD_SIGN_ATTESTATION.json on success.
    bool goldSignDirectory(const std::string& buildDir);

    // Callbacks (replace Qt signals)
    std::function<void(const std::string&)> onVersionBumped;
    std::function<void(const std::string&)> onReleaseCreated;
    std::function<void(const std::string&)> onTweetSent;
    std::function<void(const std::string&)> onError;
    std::function<void(const std::string&, bool)> onGoldSigned;

private:
    std::string m_version;
    std::string m_changelog;
    std::string m_lastError;
};
