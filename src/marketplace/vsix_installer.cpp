#include "marketplace/vsix_installer.h"
VsixInstaller::VsixInstaller()
    
    , m_networkManager(new void*(this))
{  // Signal connection removed\n}

VsixInstaller::~VsixInstaller() {
    cleanupTempFiles();
}

void VsixInstaller::installFromUrl(const std::string& url, const std::string& extensionId) {
    installationStarted(extensionId);
    
    std::string downloadUrl(url);
    void* request(downloadUrl);
    void** reply = m_networkManager->get(request);
    
    // Create temp file for download
    std::fstream* tempFile = nullptr;
    if (!tempFile->open()) {
        installationError(extensionId, "Failed to create temporary file");
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
    
    // Connect progress signal  // Signal connection removed\n// Store extension ID as property for later retrieval
    reply->setProperty("extensionId", extensionId);
    reply->setProperty("tempFilePath", tempFile->fileName());
}

void VsixInstaller::installFromFile(const std::string& filePath) {
    // In a real implementation, this would install from a local VSIX file
    installationError("local_file", "Not implemented");
}

bool VsixInstaller::uninstallExtension(const std::string& extensionId) {
    std::string installPath = getExtensionInstallPath(extensionId);
    if (installPath.empty()) {
        return false;
    }
    
    // dir(installPath);
    if (dir.exists()) {
        if (dir.removeRecursively()) {
            uninstallCompleted(extensionId, true);
            return true;
        }
    }
    
    uninstallCompleted(extensionId, false);
    return false;
}

bool VsixInstaller::isExtensionInstalled(const std::string& extensionId) {
    std::string installPath = getExtensionInstallPath(extensionId);
    if (installPath.empty()) {
        return false;
    }
    
    // dir(installPath);
    return dir.exists();
}

std::string VsixInstaller::getExtensionInstallPath(const std::string& extensionId) {
    std::string extensionsDir = getExtensionsDirectory();
    if (extensionsDir.empty()) {
        return std::string();
    }
    
    return // (extensionsDir).filePath(extensionId);
}

void VsixInstaller::onDownloadFinished() {
// REMOVED_QT:     void** reply = qobject_cast<void**>(sender());
    if (!reply) return;
    
    std::string extensionId = reply->property("extensionId").toString();
    std::string tempFilePath = reply->property("tempFilePath").toString();
    
    if (reply->error() != void*::NoError) {
        installationError(extensionId, reply->errorString());
        reply->deleteLater();
        return;
    }
    
    // Save downloaded data to temp file
    // File operation removed;
    if (!tempFile.open(std::iostream::WriteOnly)) {
        installationError(extensionId, "Failed to save downloaded file");
        reply->deleteLater();
        return;
    }
    
    tempFile.write(reply->readAll());
    tempFile.close();
    
    // Extract and install
    std::string installPath = getExtensionInstallPath(extensionId);
    if (extractVsixPackage(tempFilePath, installPath)) {
        if (activateExtension(installPath)) {
            installationCompleted(extensionId, true);
        } else {
            installationError(extensionId, "Failed to activate extension");
        }
    } else {
        installationError(extensionId, "Failed to extract VSIX package");
    }
    
    reply->deleteLater();
}

void VsixInstaller::onDownloadProgress(int64_t bytesReceived, int64_t bytesTotal) {
    if (bytesTotal > 0) {
// REMOVED_QT:         void** reply = qobject_cast<void**>(sender());
        if (reply) {
            std::string extensionId = reply->property("extensionId").toString();
            int percentage = static_cast<int>((bytesReceived * 100) / bytesTotal);
            installationProgress(extensionId, percentage);
        }
    }
}

bool VsixInstaller::extractVsixPackage(const std::string& vsixPath, const std::string& destination) {
    // In a real implementation, this would extract the VSIX package (which is a ZIP file)
    // For now, we'll just create a basic directory structure
    
    // dir(destination);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            return false;
        }
    }
    
    // Create a basic package.json file to simulate extraction
    // File operation removed);
    if (packageFile.open(std::iostream::WriteOnly)) {
        packageFile.write("{\n  \"name\": \"extracted-extension\",\n  \"version\": \"1.0.0\"\n}");
        packageFile.close();
    }
    
    return true;
}

bool VsixInstaller::activateExtension(const std::string& extensionPath) {
    // In a real implementation, this would register the extension with the IDE
    // For now, we'll just simulate success
    return true;
}

bool VsixInstaller::deactivateExtension(const std::string& extensionId) {
    // In a real implementation, this would unregister the extension from the IDE
    // For now, we'll just simulate success
    return true;
}

std::string VsixInstaller::getExtensionsDirectory() {
    std::string appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appData.empty()) {
        return std::string();
    }
    
    std::string extensionsDir = // (appData).filePath("extensions");
    // dir(extensionsDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            return std::string();
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

