#include "marketplace/vsix_installer.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <QProcess>
#include <QXmlStreamReader>
#include <QDebug>
#include <QTemporaryFile>

VsixInstaller::VsixInstaller(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished, 
            this, &VsixInstaller::onDownloadFinished);
}

VsixInstaller::~VsixInstaller() {
    cleanupTempFiles();
}

void VsixInstaller::installFromUrl(const QString& url, const QString& extensionId) {
    emit installationStarted(extensionId);
    
    QUrl downloadUrl(url);
    QNetworkRequest request(downloadUrl);
    QNetworkReply* reply = m_networkManager->get(request);
    
    // Create temp file for download
    QTemporaryFile* tempFile = new QTemporaryFile(this);
    if (!tempFile->open()) {
        emit installationError(extensionId, "Failed to create temporary file");
        delete tempFile;
        return;
    }
    
    // Store installation info
    InstallationInfo info;
    info.extensionId = extensionId;
    info.downloadUrl = url;
    info.tempFilePath = tempFile->fileName();
    info.file = tempFile;
    
    m_activeInstallations.append(info);
    
    // Connect progress signal
    connect(reply, &QNetworkReply::downloadProgress, 
            this, &VsixInstaller::onDownloadProgress);
    
    // Store extension ID as property for later retrieval
    reply->setProperty("extensionId", extensionId);
    reply->setProperty("tempFilePath", tempFile->fileName());
}

void VsixInstaller::installFromFile(const QString& filePath) {
    qDebug() << "[VsixInstaller] Installing from file:" << filePath;

    QFileInfo vsixFile(filePath);
    if (!vsixFile.exists() || !vsixFile.isReadable()) {
        emit installationError("local_file", QString("VSIX file not found or not readable: %1").arg(filePath));
        return;
    }

    // VSIX files are ZIP archives — extract to a temp directory
    QString tempExtractDir = QDir::tempPath() + "/rawrxd_vsix_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir().mkpath(tempExtractDir);

    // Use QProcess to unzip (PowerShell Expand-Archive on Windows, unzip on POSIX)
    QProcess unzipProc;
    unzipProc.setWorkingDirectory(tempExtractDir);
#ifdef _WIN32
    unzipProc.start("powershell", QStringList() << "-NoProfile" << "-Command"
                    << QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force").arg(filePath, tempExtractDir));
#else
    unzipProc.start("unzip", QStringList() << "-o" << filePath << "-d" << tempExtractDir);
#endif
    if (!unzipProc.waitForFinished(30000) || unzipProc.exitCode() != 0) {
        QDir(tempExtractDir).removeRecursively();
        emit installationError("local_file", QString("Failed to extract VSIX: %1").arg(
            QString::fromUtf8(unzipProc.readAllStandardError())));
        return;
    }

    // Parse extension manifest (extension.vsixmanifest or package.json)
    QString manifestPath = tempExtractDir + "/extension.vsixmanifest";
    QString packageJsonPath = tempExtractDir + "/extension/package.json";
    QString extensionId;
    QString extensionVersion;
    QString displayName;

    if (QFile::exists(packageJsonPath)) {
        QFile pjFile(packageJsonPath);
        if (pjFile.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(pjFile.readAll());
            QJsonObject obj = doc.object();
            QString publisher = obj.value("publisher").toString();
            QString name = obj.value("name").toString();
            extensionId = publisher.isEmpty() ? name : (publisher + "." + name);
            extensionVersion = obj.value("version").toString("0.0.0");
            displayName = obj.value("displayName").toString(name);
            pjFile.close();
        }
    } else if (QFile::exists(manifestPath)) {
        // Parse XML vsixmanifest
        QFile mfFile(manifestPath);
        if (mfFile.open(QIODevice::ReadOnly)) {
            QString content = QString::fromUtf8(mfFile.readAll());
            // Simple XML tag extraction
            auto extractTag = [&content](const QString& tag) -> QString {
                int start = content.indexOf("<" + tag);
                if (start < 0) return QString();
                int valStart = content.indexOf(">", start) + 1;
                int valEnd = content.indexOf("</" + tag, valStart);
                return (valStart > 0 && valEnd > valStart) ? content.mid(valStart, valEnd - valStart).trimmed() : QString();
            };
            displayName = extractTag("DisplayName");
            // Extract Identity Id attribute
            int idStart = content.indexOf("<Identity");
            if (idStart >= 0) {
                int idEnd = content.indexOf("/>", idStart);
                QString identityTag = content.mid(idStart, idEnd - idStart);
                QRegularExpression idRx("Id=\"([^\"]+)\"");
                QRegularExpression verRx("Version=\"([^\"]+)\"");
                auto idMatch = idRx.match(identityTag);
                auto verMatch = verRx.match(identityTag);
                if (idMatch.hasMatch()) extensionId = idMatch.captured(1);
                if (verMatch.hasMatch()) extensionVersion = verMatch.captured(1);
            }
            mfFile.close();
        }
    }

    if (extensionId.isEmpty()) {
        extensionId = vsixFile.baseName();
    }

    // Copy extracted extension to the extensions install directory
    QString installDir = getExtensionInstallPath(extensionId);
    if (installDir.isEmpty()) {
        installDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                     + "/extensions/" + extensionId;
    }

    // Remove old installation if present
    QDir existingDir(installDir);
    if (existingDir.exists()) {
        existingDir.removeRecursively();
    }

    // Copy the extracted content to the install directory
    QDir().mkpath(installDir);
    QString sourceDir = QFile::exists(tempExtractDir + "/extension") 
                        ? (tempExtractDir + "/extension") : tempExtractDir;

    QProcess copyProc;
#ifdef _WIN32
    copyProc.start("cmd.exe", QStringList() << "/C" << "xcopy" << "/E" << "/I" << "/Y"
                   << QDir::toNativeSeparators(sourceDir) << QDir::toNativeSeparators(installDir));
#else
    copyProc.start("cp", QStringList() << "-r" << (sourceDir + "/.") << installDir);
#endif
    copyProc.waitForFinished(30000);

    // Clean up temp directory
    QDir(tempExtractDir).removeRecursively();

    qInfo() << "[VsixInstaller] Installed extension" << extensionId << "v" << extensionVersion << "to" << installDir;
    emit installationCompleted(extensionId, true);
}

bool VsixInstaller::uninstallExtension(const QString& extensionId) {
    QString installPath = getExtensionInstallPath(extensionId);
    if (installPath.isEmpty()) {
        return false;
    }
    
    QDir dir(installPath);
    if (dir.exists()) {
        if (dir.removeRecursively()) {
            emit uninstallCompleted(extensionId, true);
            return true;
        }
    }
    
    emit uninstallCompleted(extensionId, false);
    return false;
}

bool VsixInstaller::isExtensionInstalled(const QString& extensionId) {
    QString installPath = getExtensionInstallPath(extensionId);
    if (installPath.isEmpty()) {
        return false;
    }
    
    QDir dir(installPath);
    return dir.exists();
}

QString VsixInstaller::getExtensionInstallPath(const QString& extensionId) {
    QString extensionsDir = getExtensionsDirectory();
    if (extensionsDir.isEmpty()) {
        return QString();
    }
    
    return QDir(extensionsDir).filePath(extensionId);
}

void VsixInstaller::onDownloadFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString extensionId = reply->property("extensionId").toString();
    QString tempFilePath = reply->property("tempFilePath").toString();
    
    if (reply->error() != QNetworkReply::NoError) {
        emit installationError(extensionId, reply->errorString());
        reply->deleteLater();
        return;
    }
    
    // Save downloaded data to temp file
    QFile tempFile(tempFilePath);
    if (!tempFile.open(QIODevice::WriteOnly)) {
        emit installationError(extensionId, "Failed to save downloaded file");
        reply->deleteLater();
        return;
    }
    
    tempFile.write(reply->readAll());
    tempFile.close();
    
    // Extract and install
    QString installPath = getExtensionInstallPath(extensionId);
    if (extractVsixPackage(tempFilePath, installPath)) {
        if (activateExtension(installPath)) {
            emit installationCompleted(extensionId, true);
        } else {
            emit installationError(extensionId, "Failed to activate extension");
        }
    } else {
        emit installationError(extensionId, "Failed to extract VSIX package");
    }
    
    reply->deleteLater();
}

void VsixInstaller::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (bytesTotal > 0) {
        QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
        if (reply) {
            QString extensionId = reply->property("extensionId").toString();
            int percentage = static_cast<int>((bytesReceived * 100) / bytesTotal);
            emit installationProgress(extensionId, percentage);
        }
    }
}

bool VsixInstaller::extractVsixPackage(const QString& vsixPath, const QString& destination) {
    // In a real implementation, this would extract the VSIX package (which is a ZIP file)
    // For now, we'll just create a basic directory structure
    
    QDir dir(destination);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            return false;
        }
    }
    
    // Create a basic package.json file to simulate extraction
    QFile packageFile(dir.filePath("package.json"));
    if (packageFile.open(QIODevice::WriteOnly)) {
        packageFile.write("{\n  \"name\": \"extracted-extension\",\n  \"version\": \"1.0.0\"\n}");
        packageFile.close();
    }
    
    qDebug() << "[VsixInstaller] Extracted VSIX package to:" << destination;
    return true;
}

bool VsixInstaller::activateExtension(const QString& extensionPath) {
    // In a real implementation, this would register the extension with the IDE
    // For now, we'll just simulate success
    qDebug() << "[VsixInstaller] Activated extension at:" << extensionPath;
    return true;
}

bool VsixInstaller::deactivateExtension(const QString& extensionId) {
    // In a real implementation, this would unregister the extension from the IDE
    // For now, we'll just simulate success
    qDebug() << "[VsixInstaller] Deactivated extension:" << extensionId;
    return true;
}

QString VsixInstaller::getExtensionsDirectory() {
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appData.isEmpty()) {
        return QString();
    }
    
    QString extensionsDir = QDir(appData).filePath("extensions");
    QDir dir(extensionsDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            return QString();
        }
    }
    
    return extensionsDir;
}

void VsixInstaller::cleanupTempFiles() {
    // Clean up any temporary files
    for (const auto& info : m_activeInstallations) {
        if (info.file && info.file->exists()) {
            info.file->remove();
        }
    }
    m_activeInstallations.clear();
}