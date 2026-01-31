#include "sign_binary.hpp"


bool signBinary(const std::string& exePath) {
    std::string signtool = QProcessEnvironment::systemEnvironment().value("SIGNTOOL_PATH", "signtool.exe");
    std::string cert = qEnvironmentVariable("CERT_PATH");
    std::string pass = qEnvironmentVariable("CERT_PASS");
    if (cert.isEmpty() || pass.isEmpty()) {
        return true;
    }
    std::vector<std::string> args = {
        "sign", "/f", cert, "/p", pass,
        "/fd", "sha256", "/tr", "http://timestamp.digicert.com", "/td", "sha256",
        exePath
    };
    QProcess proc;
    proc.start(signtool, args);
    if (!proc.waitForFinished(60000)) {
        return false;
    }
    if (proc.exitCode() != 0) {
        return false;
    }
    return true;
}

