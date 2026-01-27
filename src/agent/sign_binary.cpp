#include "sign_binary.hpp"
#include <QProcess>
#include <QProcessEnvironment>
#include <QDebug>

bool signBinary(const QString& exePath) {
    QString signtool = QProcessEnvironment::systemEnvironment().value("SIGNTOOL_PATH", "signtool.exe");
    QString cert = qEnvironmentVariable("CERT_PATH");
    QString pass = qEnvironmentVariable("CERT_PASS");
    if (cert.isEmpty() || pass.isEmpty()) {
        qWarning() << "Sign: CERT_PATH or CERT_PASS not set – skipping";
        return true;
    }
    QStringList args = {
        "sign", "/f", cert, "/p", pass,
        "/fd", "sha256", "/tr", "http://timestamp.digicert.com", "/td", "sha256",
        exePath
    };
    QProcess proc;
    proc.start(signtool, args);
    if (!proc.waitForFinished(60000)) {
        qWarning() << "Sign: signtool timed out";
        return false;
    }
    if (proc.exitCode() != 0) {
        qWarning() << "Sign: failed" << proc.readAllStandardError();
        return false;
    }
    qInfo() << "Sign: SUCCESS" << exePath;
    return true;
}
