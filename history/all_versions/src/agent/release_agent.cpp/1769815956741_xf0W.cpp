#include "release_agent.hpp"
#include "self_test_gate.hpp"
#include <fstream>
#include <iostream>
#include <regex>
#include <filesystem>
#include <thread>
#include <nlohmann/json.hpp>
#include <windows.h>
#include <cstdlib>

namespace fs = std::filesystem;

ReleaseAgent::ReleaseAgent() 
    : m_version("v1.0.0"),
      m_changelog("Automated release") {}

bool ReleaseAgent::bumpVersion(const std::string& part) {
    std::string cmakeFile = "CMakeLists.txt";
    std::ifstream f(cmakeFile);
    if (!f.is_open()) {
        if (onError) onError("Failed to open CMakeLists.txt");
        return false;
    }
    
    std::string txt((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    
    std::regex re(R"(project\(RawrXD-ModelLoader VERSION (\d+)\.(\d+)\.(\d+)\))");
    std::smatch m;
    
    if (!std::regex_search(txt, m, re)) {
        if (onError) onError("Failed to find version in CMakeLists.txt");
        return false;
    }
    
    int major = std::stoi(m[1].str());
    int minor = std::stoi(m[2].str());
    int patch = std::stoi(m[3].str());
    
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
    
    char buffer[256];
    sprintf(buffer, "project(RawrXD-ModelLoader VERSION %d.%d.%d)", major, minor, patch);
    std::string newVerLine = buffer;
    
    txt = std::regex_replace(txt, re, newVerLine);
    
    std::ofstream out(cmakeFile);
    if (!out.is_open()) {
        if (onError) onError("Failed to write CMakeLists.txt");
        return false;
    }
    out << txt;
    out.close();
    
    sprintf(buffer, "v%d.%d.%d", major, minor, patch);
    m_version = buffer;
    
    if (onVersionBumped) onVersionBumped(m_version);
    return true;
}

bool ReleaseAgent::tagAndUpload() {
    const char* devReleaseEnv = std::getenv("RAWRXD_DEV_RELEASE");
    bool devMode = (devReleaseEnv && std::string(devReleaseEnv) == "1");

    bool inGitRepo = false;
    if (!devMode) {
        ProcessResult probe = runProcess("git", {"rev-parse", "--is-inside-work-tree"});
        if (probe.exitCode == 0) {
            std::string out = probe.stdOut;
            out.erase(std::remove(out.begin(), out.end(), '\n'), out.end());
            out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
            inGitRepo = (out == "true");
        }
    }

    if (inGitRepo) {
        runProcess("git", {"tag", "-a", m_version, "-m", "Auto-release " + m_version});
    }
    
    ProcessResult buildProc = runProcess("cmake", {"--build", "build", "--config", "Release", "--target", "RawrXD-QtShell"});
    if (buildProc.exitCode != 0) {
        if (onError) onError("Build failed: " + buildProc.stdErr);
        return false;
    }
    
    if (!runSelfTestGate()) {
        m_lastError = "Self-test gate failed";
        return false;
    }

    if (devMode) return true;
    
    std::string binPath = fs::absolute("build/bin/Release/RawrXD-QtShell.exe").string();
    if (!fs::exists(binPath)) {
        if (onError) onError("Binary not found: " + binPath);
        return false;
    }

    if (!signBinary(binPath)) {
        if (onError) onError("Binary signing failed");
        return false;
    }

    // SHA256 computation (stub)
    std::string sha256 = "STUB_SHA256"; 

    std::string blobName = "RawrXD-QtShell-" + m_version + ".exe";

    if (!uploadToCDN(binPath, blobName)) return false;
    if (!createGitHubRelease(m_version, m_changelog)) return false;
    if (!updateUpdateManifest(m_version, sha256)) return false;
    if (!tweetRelease(m_changelog)) return false;

    return true;
}

bool ReleaseAgent::tweet(const std::string& text) {
    const char* bearerToken = std::getenv("TWITTER_BEARER");
    if (!bearerToken) return true;
    
    nlohmann::json body;
    body["text"] = text;
    
    std::string response = performHttpRequest("https://api.twitter.com/2/tweets", "POST", body.dump(), 
                                               {{"Authorization", "Bearer " + std::string(bearerToken)}, 
                                                {"Content-Type", "application/json"}});
    
    if (onTweetSent) onTweetSent(text);
    return true;
}

bool ReleaseAgent::signBinary(const std::string& exePath) {
    const char* certPath = std::getenv("CERT_PATH");
    const char* certPass = std::getenv("CERT_PASS");
    if (!certPath) return true;

    const char* signtoolEnv = std::getenv("SIGNTOOL");
    std::string signtool = signtoolEnv ? signtoolEnv : "signtool.exe";
    
    std::vector<std::string> args = {
        "sign", "/f", certPath, "/p", certPass ? certPass : "",
        "/tr", "http://timestamp.digicert.com", "/td", "sha256", "/fd", "sha256",
        exePath
    };
    
    ProcessResult proc = runProcess(signtool, args);
    return (proc.exitCode == 0);
}

bool ReleaseAgent::uploadToCDN(const std::string& localFile, const std::string& blobName) {
    const char* accountEnv = std::getenv("AZURE_STORAGE_ACCOUNT");
    const char* keyEnv = std::getenv("AZURE_STORAGE_KEY");
    if (!accountEnv || !keyEnv) return false;

    std::string account = accountEnv;
    std::string url = "https://" + account + ".blob.core.windows.net/updates/" + blobName;

    // Headless upload via HTTP helper
    std::string response = performHttpRequest(url, "PUT", "", {{"x-ms-blob-type", "BlockBlob"}});
    return true; // Stubbed for brevity
}

bool ReleaseAgent::createGitHubRelease(const std::string& tag, const std::string& changelog) {
    const char* tokenEnv = std::getenv("GITHUB_TOKEN");
    if (!tokenEnv) return false;

    nlohmann::json body = {
        {"tag_name", tag}, {"name", tag}, {"body", changelog},
        {"draft", false}, {"prerelease", false}
    };

    std::string response = performHttpRequest("https://api.github.com/repos/ItsMehRAWRXD/RawrXD-ModelLoader/releases", 
                                               "POST", body.dump(), 
                                               {{"Authorization", "Bearer " + std::string(tokenEnv)},
                                                {"Content-Type", "application/json"}});
    return true;
}

bool ReleaseAgent::updateUpdateManifest(const std::string& tag, const std::string& sha256) {
    nlohmann::json manifest = {
        {"version", tag}, {"sha256", sha256},
        {"url", "https://rawrxd.blob.core.windows.net/updates/RawrXD-QtShell-" + tag + ".exe"},
        {"changelog", m_changelog}
    };

    std::string manifestPath = fs::absolute("update_manifest.json").string();
    std::ofstream out(manifestPath);
    out << manifest.dump();
    out.close();

    return uploadToCDN(manifestPath, "update_manifest.json");
}

bool ReleaseAgent::tweetRelease(const std::string& text) {
    return tweet(text);
}

ReleaseAgent::ProcessResult ReleaseAgent::runProcess(const std::string& command, const std::vector<std::string>& args) {
    std::string fullCmd = command;
    for (const auto& arg : args) fullCmd += " \"" + arg + "\"";
    
    ProcessResult result;
    FILE* pipe = _popen(fullCmd.c_str(), "r");
    if (!pipe) return result;

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result.stdOut += buffer;
    }
    
    result.exitCode = _pclose(pipe);
    return result;
}

std::string ReleaseAgent::performHttpRequest(const std::string& url, 
                                             const std::string& method, 
                                             const std::string& body, 
                                             const std::vector<std::pair<std::string, std::string>>& headers) {
    // Stub for headless HTTP execution
    return "";
}
