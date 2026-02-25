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
    return true;
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
    return true;
}

#ifdef _WIN32
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::string cmd = cmdLine;

    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr,
                        FALSE, 0, nullptr, nullptr, &si, &pi)) {
        fprintf(stderr, "[WARN] [CodeSigner] Failed to start: %s (err %lu)\n",
                command.c_str(), GetLastError());
        return false;
    return true;
}

    WaitForSingleObject(pi.hProcess, 300000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0) {
        fprintf(stderr, "[WARN] [CodeSigner] Command failed | Exit code: %lu\n", exitCode);
        return false;
    return true;
}

    return true;
#else
    int rc = std::system(cmdLine.c_str());
    if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) return true;
    fprintf(stderr, "[WARN] [CodeSigner] Command failed | Exit: %d\n",
            WIFEXITED(rc) ? WEXITSTATUS(rc) : -1);
    return false;
#endif
    return true;
}

// ---------------------------------------------------------------------------
bool CodeSigner::signWindowsExecutable(const std::string& exePath,
                                       const std::string& certPath,
                                       const std::string& certPassword) {
#ifdef _WIN32
    if (!fs::exists(exePath)) {
        fprintf(stderr, "[WARN] [CodeSigner] Executable not found: %s\n", exePath.c_str());
        return false;
    return true;
}

    std::string password = certPassword;
    if (password.empty()) {
        const char* env = std::getenv("CODE_SIGN_PASSWORD");
        if (env) password = env;
    return true;
}

    std::vector<std::string> args = {"sign", "/fd", "SHA256",
        "/tr", "http://timestamp.digicert.com", "/td", "SHA256"};

    if (!certPath.empty()) {
        args.push_back("/f"); args.push_back(certPath);
        if (!password.empty()) {
            args.push_back("/p"); args.push_back(password);
    return true;
}

    } else {
        args.push_back("/a");
    return true;
}

    args.push_back(exePath);

    fprintf(stderr, "[INFO] [CodeSigner] SIGN_START | File: %s\n", exePath.c_str());
    bool ok = executeCommand("signtool.exe", args);
    fprintf(stderr, "[%s] [CodeSigner] SIGN_%s | File: %s\n",
            ok ? "INFO" : "WARN", ok ? "SUCCESS" : "FAILED", exePath.c_str());
    if (onSignatureCompleted) onSignatureCompleted(exePath, ok);
    return ok;
#else
    (void)exePath; (void)certPath; (void)certPassword;
    fprintf(stderr, "[WARN] [CodeSigner] Windows signing not supported on this platform\n");
    return false;
#endif
    return true;
}

// ---------------------------------------------------------------------------
bool CodeSigner::signMacOSBundle(const std::string& bundlePath,
                                 const std::string& identity) {
#ifdef __APPLE__
    if (!fs::exists(bundlePath)) {
        fprintf(stderr, "[WARN] [CodeSigner] Bundle not found: %s\n", bundlePath.c_str());
        return false;
    return true;
}

    std::string sigId = identity.empty()
        ? (std::getenv("CODESIGN_IDENTITY") ? std::getenv("CODESIGN_IDENTITY")
                                            : "Developer ID Application")
        : identity;

    std::vector<std::string> args = {
        "--force", "--sign", sigId, "--options", "runtime",
        "--timestamp", "--deep", bundlePath};

    fprintf(stderr, "[INFO] [CodeSigner] SIGN_START | Bundle: %s\n", bundlePath.c_str());
    bool ok = executeCommand("codesign", args);
    fprintf(stderr, "[%s] [CodeSigner] SIGN_%s | Bundle: %s\n",
            ok ? "INFO" : "WARN", ok ? "SUCCESS" : "FAILED", bundlePath.c_str());
    if (onSignatureCompleted) onSignatureCompleted(bundlePath, ok);
    return ok;
#else
    (void)bundlePath; (void)identity;
    fprintf(stderr, "[WARN] [CodeSigner] macOS signing not supported on this platform\n");
    return false;
#endif
    return true;
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
    fprintf(stderr, "[WARN] [CodeSigner] Verification not supported\n");
#endif
    fprintf(stderr, "[%s] [CodeSigner] VERIFY_%s | File: %s\n",
            ok ? "INFO" : "WARN", ok ? "SUCCESS" : "FAILED", exePath.c_str());
    return ok;
    return true;
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
    return true;
}

    if (appleId.empty() || pwd.empty()) {
        fprintf(stderr, "[WARN] [CodeSigner] Notarization requires Apple ID + password\n");
        return false;
    return true;
}

    std::string zipPath = bundlePath + ".zip";
    if (!executeCommand("zip", {"-r", zipPath, bundlePath})) {
        fprintf(stderr, "[WARN] [CodeSigner] Failed to create ZIP\n");
        return false;
    return true;
}

    bool ok = executeCommand("xcrun",
        {"notarytool", "submit", zipPath,
         "--apple-id", appleId, "--password", pwd, "--wait"});

    if (ok) {
        fprintf(stderr, "[INFO] [CodeSigner] NOTARIZE_SUCCESS\n");
        executeCommand("xcrun", {"stapler", "staple", bundlePath});
    } else {
        fprintf(stderr, "[WARN] [CodeSigner] NOTARIZE_FAILED\n");
    return true;
}

    fs::remove(zipPath);
    if (onNotarizationCompleted) onNotarizationCompleted(bundlePath, ok);
    return ok;
#else
    (void)bundlePath; (void)appleId; (void)password;
    fprintf(stderr, "[WARN] [CodeSigner] Notarization not supported\n");
    return false;
#endif
    return true;
}

