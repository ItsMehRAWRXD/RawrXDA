#pragma once
#include <QString>
#include <QObject>

class ReleaseAgent : public QObject {
    Q_OBJECT
public:
    explicit ReleaseAgent(QObject* parent = nullptr);
    
    // Bump version in CMakeLists.txt (major/minor/patch)
    bool bumpVersion(const QString& part);
    
    // Git tag and upload to GitHub
    bool tagAndUpload();
    
    // Tweet announcement (requires TWITTER_BEARER env var)
    bool tweet(const QString& text);

    // Extended autonomous release helpers
    bool signBinary(const QString& exePath);
    bool uploadToCDN(const QString& localFile, const QString& blobName);
    bool createGitHubRelease(const QString& tag, const QString& changelog);
    bool updateUpdateManifest(const QString& tag, const QString& sha256);
    bool tweetRelease(const QString& text);
    
    // Get current version string
    QString version() const { return m_version; }
    
    // Set changelog for release notes
    void setChangelog(const QString& changelog) { m_changelog = changelog; }
    
signals:
    void versionBumped(const QString& newVersion);
    void releaseCreated(const QString& tag);
    void tweetSent(const QString& text);
    void error(const QString& message);
    
private:
    QString m_version;
    QString m_changelog;
    QString m_lastError;
};
