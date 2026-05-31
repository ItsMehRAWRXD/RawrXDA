/**
 * @file code_signer.cpp
 * @brief Code signing via signtool / codesign subprocesses (Qt-free)
 */
#include "code_signer.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <sys/wait.h>
#  include <unistd.h>
#endif

namespace fs = std::filesystem;

CodeSigner* CodeSigner::s_instance = nullptr;

CodeSigner* CodeSigner::instance() {
    if (!s_instance) s_instance = new CodeSigner();
    return s_instance;
}

// ---------------------------------------------------------------------------
bool CodeSigner::executeCommand(const std::string& command,
                                const std::vector<std::string>& args) {
    std::string cmdLine = command;
    for (const auto& a : args) {
        cmdLine += ' ';
        if (a.find(' ') != std::string::npos)
            cmdLine += '"' + a + '"';
        else
            cmdLine += a;
    }

#ifdef _WIN32
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::string cmd = cmdLine;

    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr,
                        FALSE, 0, nullptr, nullptr, &si, &pi)) {
        return false;
    }

    WaitForSingleObject(pi.hProcess, 300000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0) {
        return false;
    }
    return true;
#else
    int rc = std::system(cmdLine.c_str());
    if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) return true;
    return false;
#endif
}

// ---------------------------------------------------------------------------
bool CodeSigner::signWindowsExecutable(const std::string& exePath,
                                       const std::string& certPath,
                                       const std::string& certPassword) {
#ifdef _WIN32
    if (!fs::exists(exePath)) {
        return false;
    }

    std::string password = certPassword;
    if (password.empty()) {
        const char* env = std::getenv("CODE_SIGN_PASSWORD");
        if (env) password = env;
    }

    std::vector<std::string> args = {"sign", "/fd", "SHA256",
        "/tr", "http://timestamp.digicert.com", "/td", "SHA256"};

    if (!certPath.empty()) {
        args.push_back("/f"); args.push_back(certPath);
        if (!password.empty()) {
            args.push_back("/p"); args.push_back(password);
        }
    } else {
        args.push_back("/a");
    }
    args.push_back(exePath);

    bool ok = executeCommand("signtool.exe", args);
    if (onSignatureCompleted) onSignatureCompleted(exePath, ok);
    return ok;
#else
    (void)exePath; (void)certPath; (void)certPassword;
    return false;
#endif
}

// ---------------------------------------------------------------------------
bool CodeSigner::signMacOSBundle(const std::string& bundlePath,
                                 const std::string& identity) {
#ifdef __APPLE__
    if (!fs::exists(bundlePath)) {
        return false;
    }
    std::string sigId = identity.empty()
        ? (std::getenv("CODESIGN_IDENTITY") ? std::getenv("CODESIGN_IDENTITY")
                                            : "Developer ID Application")
        : identity;

    std::vector<std::string> args = {
        "--force", "--sign", sigId, "--options", "runtime",
        "--timestamp", "--deep", bundlePath};

    bool ok = executeCommand("codesign", args);
    if (onSignatureCompleted) onSignatureCompleted(bundlePath, ok);
    return ok;
#else
    (void)bundlePath; (void)identity;
    return false;
#endif
}

// ---------------------------------------------------------------------------
bool CodeSigner::verifySignature(const std::string& exePath) {
#ifdef _WIN32
    bool ok = executeCommand("signtool.exe", {"verify", "/pa", exePath});
#elif defined(__APPLE__)
    bool ok = executeCommand("codesign", {"--verify", "--deep", "--strict", exePath});
#else
    (void)exePath;
    bool ok = false;
#endif
    return ok;
}

// ---------------------------------------------------------------------------
bool CodeSigner::notarizeMacOSApp(const std::string& bundlePath,
                                  const std::string& appleId,
                                  const std::string& password) {
#ifdef __APPLE__
    std::string pwd = password;
    if (pwd.empty()) {
        const char* env = std::getenv("NOTARIZE_PASSWORD");
        if (env) pwd = env;
    }
    if (appleId.empty() || pwd.empty()) {
        return false;
    }

    std::string zipPath = bundlePath + ".zip";
    if (!executeCommand("zip", {"-r", zipPath, bundlePath})) {
        return false;
    }

    bool ok = executeCommand("xcrun",
        {"notarytool", "submit", zipPath,
         "--apple-id", appleId, "--password", pwd, "--wait"});

    if (ok) {
        executeCommand("xcrun", {"stapler", "staple", bundlePath});
    }

    fs::remove(zipPath);
    if (onNotarizationCompleted) onNotarizationCompleted(bundlePath, ok);
    return ok;
#else
    (void)bundlePath; (void)appleId; (void)password;
    return false;
#endif
}
