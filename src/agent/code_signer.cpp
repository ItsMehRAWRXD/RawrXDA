#include "code_signer.hpp"
#include <QProcess>
#include <QDebug>
#include <QFileInfo>
#include <QStandardPaths>
#include <QElapsedTimer>

CodeSigner* CodeSigner::s_instance = nullptr;

CodeSigner* CodeSigner::instance() {
    if (!s_instance) {
        s_instance = new CodeSigner();
    }
    return s_instance;
}

CodeSigner::CodeSigner(QObject* parent)
    : QObject(parent)
{
}

CodeSigner::~CodeSigner() {
}

bool CodeSigner::signWindowsExecutable(const QString& exePath, 
                                       const QString& certPath,
                                       const QString& certPassword) {
#ifdef Q_OS_WIN
    QElapsedTimer timer;
    timer.start();
    
    if (!QFileInfo::exists(exePath)) {
        qWarning() << "[CodeSigner] Executable not found:" << exePath;
        return false;
    }
    
    // PRODUCTION-READY: Get certificate password from environment
    QString password = certPassword.isEmpty() 
        ? qEnvironmentVariable("CODE_SIGN_PASSWORD") 
        : certPassword;
    
    QStringList args;
    args << "sign";
    args << "/fd" << "SHA256";  // Use SHA256 digest algorithm
    args << "/tr" << "http://timestamp.digicert.com";  // Timestamp server
    args << "/td" << "SHA256";  // Timestamp digest algorithm
    
    if (!certPath.isEmpty()) {
        args << "/f" << certPath;
        if (!password.isEmpty()) {
            args << "/p" << password;
        }
    } else {
        // Use certificate from store (auto-select best)
        args << "/a";
    }
    
    args << exePath;
    
    qInfo().noquote() << QString("[CodeSigner] SIGN_START | File: %1").arg(exePath);
    
    bool success = executeCommand("signtool.exe", args);
    
    qint64 latency = timer.elapsed();
    
    if (success) {
        qInfo().noquote() << QString("[CodeSigner] SIGN_SUCCESS | File: %1 | Latency: %2ms")
            .arg(exePath)
            .arg(latency);
    } else {
        qWarning().noquote() << QString("[CodeSigner] SIGN_FAILED | File: %1 | Latency: %2ms")
            .arg(exePath)
            .arg(latency);
    }
    
    emit signatureCompleted(exePath, success);
    return success;
#else
    Q_UNUSED(exePath);
    Q_UNUSED(certPath);
    Q_UNUSED(certPassword);
    qWarning() << "[CodeSigner] Windows code signing not supported on this platform";
    return false;
#endif
}

bool CodeSigner::signMacOSBundle(const QString& bundlePath, const QString& identity) {
#ifdef Q_OS_MACOS
    QElapsedTimer timer;
    timer.start();
    
    if (!QFileInfo::exists(bundlePath)) {
        qWarning() << "[CodeSigner] Bundle not found:" << bundlePath;
        return false;
    }
    
    // PRODUCTION-READY: Get identity from environment if not provided
    QString signingIdentity = identity.isEmpty() 
        ? qEnvironmentVariable("CODESIGN_IDENTITY", "Developer ID Application") 
        : identity;
    
    QStringList args;
    args << "--force";
    args << "--sign" << signingIdentity;
    args << "--options" << "runtime";  // Hardened runtime for notarization
    args << "--timestamp";  // Include timestamp
    args << "--deep";  // Sign nested code
    args << bundlePath;
    
    qInfo().noquote() << QString("[CodeSigner] SIGN_START | Bundle: %1 | Identity: %2")
        .arg(bundlePath)
        .arg(signingIdentity);
    
    bool success = executeCommand("codesign", args);
    
    qint64 latency = timer.elapsed();
    
    if (success) {
        qInfo().noquote() << QString("[CodeSigner] SIGN_SUCCESS | Bundle: %1 | Latency: %2ms")
            .arg(bundlePath)
            .arg(latency);
    } else {
        qWarning().noquote() << QString("[CodeSigner] SIGN_FAILED | Bundle: %1 | Latency: %2ms")
            .arg(bundlePath)
            .arg(latency);
    }
    
    emit signatureCompleted(bundlePath, success);
    return success;
#else
    Q_UNUSED(bundlePath);
    Q_UNUSED(identity);
    qWarning() << "[CodeSigner] macOS code signing not supported on this platform";
    return false;
#endif
}

bool CodeSigner::verifySignature(const QString& exePath) {
    QElapsedTimer timer;
    timer.start();
    
#ifdef Q_OS_WIN
    QStringList args;
    args << "verify" << "/pa" << exePath;
    
    qInfo().noquote() << QString("[CodeSigner] VERIFY_START | File: %1").arg(exePath);
    bool success = executeCommand("signtool.exe", args);
#elif defined(Q_OS_MACOS)
    QStringList args;
    args << "--verify" << "--deep" << "--strict" << exePath;
    
    qInfo().noquote() << QString("[CodeSigner] VERIFY_START | File: %1").arg(exePath);
    bool success = executeCommand("codesign", args);
#else
    qWarning() << "[CodeSigner] Signature verification not supported on this platform";
    bool success = false;
#endif
    
    qint64 latency = timer.elapsed();
    
    if (success) {
        qInfo().noquote() << QString("[CodeSigner] VERIFY_SUCCESS | File: %1 | Latency: %2ms")
            .arg(exePath)
            .arg(latency);
    } else {
        qWarning().noquote() << QString("[CodeSigner] VERIFY_FAILED | File: %1 | Latency: %2ms")
            .arg(exePath)
            .arg(latency);
    }
    
    return success;
}

bool CodeSigner::notarizeMacOSApp(const QString& bundlePath, 
                                  const QString& appleId,
                                  const QString& password) {
#ifdef Q_OS_MACOS
    QElapsedTimer timer;
    timer.start();
    
    // PRODUCTION-READY: Get notarization credentials from environment
    QString notarizePassword = password.isEmpty() 
        ? qEnvironmentVariable("NOTARIZE_PASSWORD") 
        : password;
    
    if (appleId.isEmpty() || notarizePassword.isEmpty()) {
        qWarning() << "[CodeSigner] Notarization requires Apple ID and password";
        return false;
    }
    
    // Create temporary ZIP for notarization
    QString zipPath = bundlePath + ".zip";
    QStringList zipArgs;
    zipArgs << "-r" << zipPath << bundlePath;
    
    if (!executeCommand("zip", zipArgs)) {
        qWarning() << "[CodeSigner] Failed to create ZIP for notarization";
        return false;
    }
    
    qInfo().noquote() << QString("[CodeSigner] NOTARIZE_START | Bundle: %1").arg(bundlePath);
    
    // Submit for notarization (requires Xcode 13+)
    QStringList args;
    args << "notarytool" << "submit" << zipPath;
    args << "--apple-id" << appleId;
    args << "--password" << notarizePassword;
    args << "--wait";  // Wait for completion
    
    bool success = executeCommand("xcrun", args);
    
    qint64 latency = timer.elapsed();
    
    if (success) {
        qInfo().noquote() << QString("[CodeSigner] NOTARIZE_SUCCESS | Bundle: %1 | Latency: %2ms")
            .arg(bundlePath)
            .arg(latency);
        
        // Staple the notarization ticket
        QStringList stapleArgs;
        stapleArgs << "stapler" << "staple" << bundlePath;
        executeCommand("xcrun", stapleArgs);
    } else {
        qWarning().noquote() << QString("[CodeSigner] NOTARIZE_FAILED | Bundle: %1 | Latency: %2ms")
            .arg(bundlePath)
            .arg(latency);
    }
    
    // Cleanup temporary ZIP
    QFile::remove(zipPath);
    
    emit notarizationCompleted(bundlePath, success);
    return success;
#else
    Q_UNUSED(bundlePath);
    Q_UNUSED(appleId);
    Q_UNUSED(password);
    qWarning() << "[CodeSigner] macOS notarization not supported on this platform";
    return false;
#endif
}

bool CodeSigner::executeCommand(const QString& command, const QStringList& args) {
    QProcess process;
    process.setProgram(command);
    process.setArguments(args);
    
    // PRODUCTION-READY: Resource guard - ensure process cleanup
    process.start();
    
    if (!process.waitForStarted(5000)) {
        qWarning().noquote() << QString("[CodeSigner] Failed to start command: %1").arg(command);
        return false;
    }
    
    if (!process.waitForFinished(300000)) {  // 5 minute timeout
        qWarning().noquote() << QString("[CodeSigner] Command timeout: %1").arg(command);
        process.kill();
        return false;
    }
    
    int exitCode = process.exitCode();
    
    if (exitCode != 0) {
        QString errorOutput = QString::fromUtf8(process.readAllStandardError());
        qWarning().noquote() << QString("[CodeSigner] Command failed | Exit code: %1 | Error: %2")
            .arg(exitCode)
            .arg(errorOutput);
        return false;
    }
    
    return true;
}
