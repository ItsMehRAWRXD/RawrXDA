#include "release_agent.hpp"
#include "self_test_gate.hpp"
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QEventLoop>
#include <QDebug>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QProcessEnvironment>
#include <QStandardPaths>

ReleaseAgent::ReleaseAgent(QObject* parent) 
    : QObject(parent), 
      m_version("v1.0.0"),
      m_changelog("Automated release") {}

bool ReleaseAgent::bumpVersion(const QString& part) {
    QFile f("CMakeLists.txt");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit error("Failed to open CMakeLists.txt");
        return false;
    }
    
    QString txt = QString::fromUtf8(f.readAll());
    f.close();
    
    // Match version in project() command
    QRegularExpression re(R"(project\(RawrXD-ModelLoader VERSION (\d+)\.(\d+)\.(\d+)\))");
    QRegularExpressionMatch m = re.match(txt);
    
    if (!m.hasMatch()) {
        emit error("Failed to find version in CMakeLists.txt");
        return false;
    }
    
    int major = m.captured(1).toInt();
    int minor = m.captured(2).toInt();
    int patch = m.captured(3).toInt();
    
    // Bump appropriate component
    if (part == "major") {
        major++;
        minor = 0;
        patch = 0;
    } else if (part == "minor") {
        minor++;
        patch = 0;
    } else {
        patch++;
    }
    
    QString newVer = QString("project(RawrXD-ModelLoader VERSION %1.%2.%3)")
                        .arg(major).arg(minor).arg(patch);
    txt.replace(re, newVer);
    
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit error("Failed to write CMakeLists.txt");
        return false;
    }
    f.write(txt.toUtf8());
    f.close();
    
    m_version = QString("v%1.%2.%3").arg(major).arg(minor).arg(patch);
    
    qDebug() << "Version bumped to" << m_version;
    emit versionBumped(m_version);
    return true;
}

bool ReleaseAgent::tagAndUpload() {
    bool devMode = (qEnvironmentVariable("RAWRXD_DEV_RELEASE") == "1");
    // Step 1: Git tag (skip gracefully if not a git repo)
    bool inGitRepo = false;
    if (!devMode) {
        QProcess probe;
        probe.start("git", {"rev-parse", "--is-inside-work-tree"});
        if (probe.waitForFinished(3000) && probe.exitCode() == 0) {
            QByteArray out = probe.readAllStandardOutput().trimmed();
            inGitRepo = (out == "true");
        }
    }
    if (inGitRepo) {
        qDebug() << "Creating git tag" << m_version;
        QProcess tagProc;
        tagProc.start("git", {"tag", "-a", m_version, "-m", "Auto-release " + m_version});
        if (!tagProc.waitForFinished(10000)) {
            emit error("Git tag timeout");
            return false;
        }
        if (tagProc.exitCode() != 0) {
            qWarning() << "Git tag failed (may already exist):" << tagProc.readAllStandardError();
            // Continue anyway - tag might already exist
        }
    } else {
        qWarning() << "Not a git repository; skipping tag step";
    }
    
    // Step 2: Build
    qDebug() << "Building release binary (RawrXD-QtShell target)...";
    QProcess buildProc;
    buildProc.start("cmake", {"--build", "build", "--config", "Release", "--target", "RawrXD-QtShell"});
    if (!buildProc.waitForFinished(120000)) {
        emit error("Build timeout");
        return false;
    }
    
    if (buildProc.exitCode() != 0) {
        emit error(QString("Build failed: %1").arg(QString::fromUtf8(buildProc.readAllStandardError())));
        return false;
    }
    
    qDebug() << "Build successful";

    qInfo() << "Running self-test gate...";
    if (!runSelfTestGate()) {
        m_lastError = "Self-test gate failed";
        return false;
    }
    qInfo() << "Self-test gate PASSED";
    if (devMode) {
        qInfo() << "Dev release mode: skipping signing and uploads.";
        return true;
    }
    
    QString binPath = QDir::current().absoluteFilePath("build/bin/Release/RawrXD-QtShell.exe");
    if (!QFile::exists(binPath)) {
        emit error(QString("Binary not found: %1").arg(binPath));
        return false;
    }

    if (!signBinary(binPath)) {
        emit error("Binary signing failed");
        return false;
    }

    QFile binFile(binPath);
    if (!binFile.open(QIODevice::ReadOnly)) {
        emit error("Cannot hash binary");
        return false;
    }
    QByteArray raw = binFile.readAll();
    binFile.close();
    QString sha256 = QCryptographicHash::hash(raw, QCryptographicHash::Sha256).toHex();

    QString blobName = QString("RawrXD-QtShell-%1.exe").arg(m_version);

    if (!uploadToCDN(binPath, blobName))
        return false;
    if (!createGitHubRelease(m_version, m_changelog))
        return false;
    if (!updateUpdateManifest(m_version, sha256))
        return false;
    if (!tweetRelease(m_changelog))
        return false;

    return true;
}

bool ReleaseAgent::tweet(const QString& text) {
    // Get bearer token from environment
    QString bearerToken = qEnvironmentVariable("TWITTER_BEARER");
    if (bearerToken.isEmpty()) {
        qWarning() << "TWITTER_BEARER not set, skipping tweet";
        return true; // Not an error - just skip
    }
    
    // Prepare request
    QUrl url("https://api.twitter.com/2/tweets");
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", QString("Bearer %1").arg(bearerToken).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonObject body;
    body["text"] = text;
    
    QNetworkAccessManager nam;
    QNetworkReply* reply = nam.post(req, QJsonDocument(body).toJson());
    
    // Wait for completion
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    bool ok = (reply->error() == QNetworkReply::NoError);
    
    if (ok) {
        qDebug() << "Tweet sent:" << text;
        emit tweetSent(text);
    } else {
        qWarning() << "Tweet failed:" << reply->errorString();
        emit error(QString("Tweet failed: %1").arg(reply->errorString()));
    }
    
    reply->deleteLater();
    return ok;
}

// ---------- 1. sign binary ----------
bool ReleaseAgent::signBinary(const QString& exePath) {
    QString certPath = qEnvironmentVariable("CERT_PATH");
    QString certPass = qEnvironmentVariable("CERT_PASS");
    if (certPath.isEmpty()) {
        qWarning() << "CERT_PATH not set, skipping code signing";
        return true; // not fatal in dev builds
    }
    QString signtool = qEnvironmentVariable("SIGNTOOL");
    if (signtool.isEmpty()) signtool = "signtool.exe";
    QStringList args = {
        "sign",
        "/f", certPath,
        "/p", certPass,
        "/tr", "http://timestamp.digicert.com",
        "/td", "sha256",
        "/fd", "sha256",
        exePath
    };
    QProcess proc;
    proc.start(signtool, args);
    if (!proc.waitForFinished(60000)) {
        m_lastError = "signtool timeout";
        emit error(m_lastError);
        return false;
    }
    if (proc.exitCode() != 0) {
        m_lastError = QString("signtool failed: %1").arg(QString::fromUtf8(proc.readAllStandardError()));
        emit error(m_lastError);
        return false;
    }
    qInfo() << "Signed" << exePath;
    return true;
}

// ---------- 2. upload to CDN (Azure Blob) ----------
bool ReleaseAgent::uploadToCDN(const QString& localFile,
                               const QString& blobName) {
    QString account = qEnvironmentVariable("AZURE_STORAGE_ACCOUNT");
    QString key     = qEnvironmentVariable("AZURE_STORAGE_KEY");
    if (account.isEmpty() || key.isEmpty()) {
        m_lastError = "Azure credentials not set";
        emit error(m_lastError);
        return false;
    }

    QString url = QString("https://%1.blob.core.windows.net/updates/%2")
                      .arg(account, blobName);

    QFile f(localFile);
    if (!f.open(QIODevice::ReadOnly)) {
        m_lastError = "Cannot open " + localFile;
        emit error(m_lastError);
        return false;
    }
    QByteArray data = f.readAll();
    f.close();

    QUrl azureUrl(url);
    QNetworkRequest req(azureUrl);
    req.setRawHeader("x-ms-blob-type", "BlockBlob");
    req.setRawHeader("Content-Type", "application/octet-stream");

    // NOTE: This is a simplified SharedKey construction; for production you
    // should use the official Azure SDK or a fully correct canonical string.
    QByteArray stringToSign = QByteArray("PUT\n\n\n") + QByteArray::number(data.size()) +
                              "\napplication/octet-stream\n\n\n\n\n\n\n\n" +
                              "x-ms-blob-type:BlockBlob\n" +
                              "/" + account.toUtf8() + "/updates/" + blobName.toUtf8();

    QByteArray decodedKey = QByteArray::fromBase64(key.toUtf8());
    QByteArray signature = QMessageAuthenticationCode::hash(stringToSign, decodedKey, QCryptographicHash::Sha256).toBase64();
    QByteArray authHeader = "SharedKey " + account.toUtf8() + ":" + signature;
    req.setRawHeader("Authorization", authHeader);

    QNetworkAccessManager nam;
    QNetworkReply* reply = nam.put(req, data);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    bool ok = reply->error() == QNetworkReply::NoError;
    if (!ok) {
        m_lastError = "CDN upload: " + reply->errorString();
        emit error(m_lastError);
    }
    reply->deleteLater();
    return ok;
}

// ---------- 3. GitHub release ----------
bool ReleaseAgent::createGitHubRelease(const QString& tag,
                                       const QString& changelog) {
    QString token = qEnvironmentVariable("GITHUB_TOKEN");
    if (token.isEmpty()) {
        m_lastError = "GITHUB_TOKEN not set";
        emit error(m_lastError);
        return false;
    }

    QJsonObject body{
        {"tag_name", tag},
        {"name", tag},
        {"body", changelog},
        {"draft", false},
        {"prerelease", false}
    };

    QNetworkRequest req(QUrl("https://api.github.com/repos/ItsMehRAWRXD/RawrXD-ModelLoader/releases"));
    req.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkAccessManager nam;
    QNetworkReply* reply = nam.post(req, QJsonDocument(body).toJson());
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    bool ok = reply->error() == QNetworkReply::NoError;
    if (!ok) {
        m_lastError = "GitHub release: " + reply->errorString();
        emit error(m_lastError);
    }
    reply->deleteLater();
    return ok;
}

// ---------- 4. auto-update manifest ----------
bool ReleaseAgent::updateUpdateManifest(const QString& tag,
                                        const QString& sha256) {
    QJsonObject manifest{
        {"version", tag},
        {"sha256", sha256},
        {"url", QString("https://rawrxd.blob.core.windows.net/updates/RawrXD-QtShell-%1.exe").arg(tag)},
        {"changelog", m_changelog}
    };

    QString manifestPath = QDir::current().absoluteFilePath("update_manifest.json");
    QFile f(manifestPath);
    if (!f.open(QIODevice::WriteOnly)) {
        m_lastError = "Cannot write manifest";
        emit error(m_lastError);
        return false;
    }
    f.write(QJsonDocument(manifest).toJson(QJsonDocument::Compact));
    f.close();

    // upload manifest to same CDN
    return uploadToCDN(manifestPath, "update_manifest.json");
}

// ---------- 5. tweet release ----------
bool ReleaseAgent::tweetRelease(const QString& text) {
    QString bearer = qEnvironmentVariable("TWITTER_BEARER");
    if (bearer.isEmpty()) {
        m_lastError = "TWITTER_BEARER not set";
        emit error(m_lastError);
        return false;
    }

    QJsonObject body{{"text", text}};
    QNetworkRequest req(QUrl("https://api.twitter.com/2/tweets"));
    req.setRawHeader("Authorization", QString("Bearer %1").arg(bearer).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkAccessManager nam;
    QNetworkReply* reply = nam.post(req, QJsonDocument(body).toJson());
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    bool ok = reply->error() == QNetworkReply::NoError;
    if (!ok) {
        m_lastError = "Tweet: " + reply->errorString();
        emit error(m_lastError);
    } else {
        emit tweetSent(text);
    }
    reply->deleteLater();
    return ok;
}
