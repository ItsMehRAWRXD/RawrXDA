#include "release_agent.hpp"
#include "self_test_gate.hpp"
#include <fstream>
#include <iostream>
#include <regex>
#include <filesystem>
#include <thread>
#include <vector>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <cstdlib>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace fs = std::filesystem;

static std::wstring toWide(const std::string& input) {
    if (input.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
    if (sizeNeeded <= 0) return L"";
    std::wstring output(sizeNeeded - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, output.data(), sizeNeeded);
    return output;
}

static std::string bytesToHex(const std::vector<unsigned char>& bytes) {
    static const char* kHex = "0123456789abcdef";
    std::string hex;
    hex.reserve(bytes.size() * 2);
    for (unsigned char b : bytes) {
        hex.push_back(kHex[(b >> 4) & 0xF]);
        hex.push_back(kHex[b & 0xF]);
    }
    return hex;
}

static std::string computeSha256Hex(const std::string& filePath) {
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) {
        return std::string();
    }

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    DWORD hashObjectSize = 0;
    DWORD dataSize = 0;
    DWORD hashLength = 0;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        return std::string();
    }

    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&hashObjectSize), sizeof(DWORD), &dataSize, 0);
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLength), sizeof(DWORD), &dataSize, 0);

    std::vector<unsigned char> hashObject(hashObjectSize);
    std::vector<unsigned char> hash(hashLength);

    if (BCryptCreateHash(hAlg, &hHash, hashObject.data(), hashObjectSize, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::string();
    }

    std::vector<char> buffer(64 * 1024);
    while (in.good()) {
        in.read(buffer.data(), buffer.size());
        std::streamsize readBytes = in.gcount();
        if (readBytes > 0) {
            BCryptHashData(hHash, reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(readBytes), 0);
        }
    }

    if (BCryptFinishHash(hHash, hash.data(), hashLength, 0) != 0) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::string();
    }

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return bytesToHex(hash);
}

static std::string hmacSha256Base64(const std::string& keyBase64, const std::string& message) {
    DWORD keyBinarySize = 0;
    if (!CryptStringToBinaryA(keyBase64.c_str(), 0, CRYPT_STRING_BASE64, nullptr, &keyBinarySize, nullptr, nullptr)) {
        return std::string();
    }

    std::vector<unsigned char> keyBinary(keyBinarySize);
    if (!CryptStringToBinaryA(keyBase64.c_str(), 0, CRYPT_STRING_BASE64, keyBinary.data(), &keyBinarySize, nullptr, nullptr)) {
        return std::string();
    }

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    DWORD hashObjectSize = 0;
    DWORD dataSize = 0;
    DWORD hashLength = 0;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) {
        return std::string();
    }

    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&hashObjectSize), sizeof(DWORD), &dataSize, 0);
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLength), sizeof(DWORD), &dataSize, 0);

    std::vector<unsigned char> hashObject(hashObjectSize);
    std::vector<unsigned char> hash(hashLength);

    if (BCryptCreateHash(hAlg, &hHash, hashObject.data(), hashObjectSize, keyBinary.data(), keyBinarySize, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::string();
    }

    BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<char*>(message.data())), static_cast<ULONG>(message.size()), 0);

    if (BCryptFinishHash(hHash, hash.data(), hashLength, 0) != 0) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::string();
    }

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    DWORD base64Size = 0;
    if (!CryptBinaryToStringA(hash.data(), hashLength, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &base64Size)) {
        return std::string();
    }

    std::string base64(base64Size, '\0');
    if (!CryptBinaryToStringA(hash.data(), hashLength, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, base64.data(), &base64Size)) {
        return std::string();
    }

    if (!base64.empty() && base64.back() == '\0') {
        base64.pop_back();
    }
    return base64;
}

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
    
    ProcessResult buildProc = runProcess("cmake", {"--build", "build", "--config", "Release", "--target", "RawrXD-Agent"});
    if (buildProc.exitCode != 0) {
        if (onError) onError("Build failed: " + buildProc.stdErr);
        return false;
    }
    
    if (!runSelfTestGate()) {
        m_lastError = "Self-test gate failed";
        if (onError) onError(m_lastError);
        return false;
    }

    if (devMode) return true;
    
    std::string binPath = fs::absolute("build/bin/Release/RawrXD-Agent.exe").string();
    if (!fs::exists(binPath)) {
        if (onError) onError("Binary not found: " + binPath);
        return false;
    }

    if (!signBinary(binPath)) {
        if (onError) onError("Binary signing failed");
        return false;
    }

    std::string sha256 = computeSha256Hex(binPath);
    if (sha256.empty()) {
        if (onError) onError("SHA256 computation failed");
        return false;
    }

    std::string blobName = "RawrXD-Agent-" + m_version + ".exe";

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
    
    if (response.empty()) {
        if (onError) onError(m_lastError.empty() ? "Tweet failed" : m_lastError);
        return false;
    }

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

    std::ifstream file(localFile, std::ios::binary);
    if (!file.is_open()) {
        m_lastError = "Cannot open " + localFile;
        if (onError) onError(m_lastError);
        return false;
    }

    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    std::string stringToSign = "PUT\n\n\n" + std::to_string(data.size()) +
                               "\napplication/octet-stream\n\n\n\n\n\n\n\n" +
                               std::string("x-ms-blob-type:BlockBlob\n") +
                               "/" + account + "/updates/" + blobName;

    std::string signature = hmacSha256Base64(keyEnv, stringToSign);
    if (signature.empty()) {
        m_lastError = "Failed to sign CDN request";
        if (onError) onError(m_lastError);
        return false;
    }

    std::vector<std::pair<std::string, std::string>> headers = {
        {"x-ms-blob-type", "BlockBlob"},
        {"Content-Type", "application/octet-stream"},
        {"Authorization", "SharedKey " + account + ":" + signature}
    };

    std::string response = performHttpRequest(url, "PUT", data, headers);
    if (response.empty()) {
        if (onError) onError(m_lastError.empty() ? "CDN upload failed" : m_lastError);
        return false;
    }

    return true;
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
    if (response.empty()) {
        if (onError) onError(m_lastError.empty() ? "GitHub release failed" : m_lastError);
        return false;
    }
    
    if (onReleaseCreated) onReleaseCreated(tag);
    return true;
}

bool ReleaseAgent::updateUpdateManifest(const std::string& tag, const std::string& sha256) {
    nlohmann::json manifest = {
        {"version", tag}, {"sha256", sha256},
        {"url", "https://rawrxd.blob.core.windows.net/updates/RawrXD-Agent-" + tag + ".exe"},
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
    auto quote = [](const std::string& arg) {
        std::string q = "\"" + arg + "\"";
        return q;
    };

    std::string cmdLine = command;
    for (const auto& arg : args) {
        cmdLine += " " + quote(arg);
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE outRead = nullptr;
    HANDLE outWrite = nullptr;
    HANDLE errRead = nullptr;
    HANDLE errWrite = nullptr;

    if (!CreatePipe(&outRead, &outWrite, &sa, 0)) {
        return ProcessResult();
    }
    if (!CreatePipe(&errRead, &errWrite, &sa, 0)) {
        CloseHandle(outRead);
        CloseHandle(outWrite);
        return ProcessResult();
    }

    SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(errRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = outWrite;
    si.hStdError = errWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::vector<char> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back('\0');

    BOOL created = CreateProcessA(nullptr,
                                  cmdBuffer.data(),
                                  nullptr,
                                  nullptr,
                                  TRUE,
                                  0,
                                  nullptr,
                                  nullptr,
                                  &si,
                                  &pi);
    CloseHandle(outWrite);
    CloseHandle(errWrite);

    ProcessResult result;
    if (!created) {
        CloseHandle(outRead);
        CloseHandle(errRead);
        return result;
    }

    auto readPipe = [](HANDLE handle, std::string& output) {
        char buffer[4096];
        DWORD bytesRead = 0;
        while (ReadFile(handle, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            output.append(buffer, buffer + bytesRead);
        }
    };

    std::thread outThread(readPipe, outRead, std::ref(result.stdOut));
    std::thread errThread(readPipe, errRead, std::ref(result.stdErr));

    WaitForSingleObject(pi.hProcess, INFINITE);

    if (outThread.joinable()) outThread.join();
    if (errThread.joinable()) errThread.join();

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    result.exitCode = static_cast<int>(exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(outRead);
    CloseHandle(errRead);

    return result;
}

std::string ReleaseAgent::performHttpRequest(const std::string& url, 
                                             const std::string& method, 
                                             const std::string& body, 
                                             const std::vector<std::pair<std::string, std::string>>& headers) {
    std::wstring urlW = toWide(url);
    if (urlW.empty()) {
        m_lastError = "Invalid URL";
        return std::string();
    }

    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(urlW.c_str(), 0, 0, &components)) {
        m_lastError = "Failed to parse URL";
        return std::string();
    }

    std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.lpszExtraInfo && components.dwExtraInfoLength > 0) {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }

    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS,
                                    0);
    if (!hSession) {
        m_lastError = "WinHTTP session failed";
        return std::string();
    }

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), components.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        m_lastError = "WinHTTP connect failed";
        return std::string();
    }

    std::wstring methodW = toWide(method);
    DWORD flags = (components.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect,
                                            methodW.c_str(),
                                            path.c_str(),
                                            nullptr,
                                            WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_lastError = "WinHTTP request failed";
        return std::string();
    }

    for (const auto& header : headers) {
        std::string headerLine = header.first + ": " + header.second + "\r\n";
        std::wstring headerWide = toWide(headerLine);
        WinHttpAddRequestHeaders(hRequest, headerWide.c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
    }

    DWORD bodySize = static_cast<DWORD>(body.size());
    BOOL sent = WinHttpSendRequest(hRequest,
                                   WINHTTP_NO_ADDITIONAL_HEADERS,
                                   0,
                                   bodySize > 0 ? (LPVOID)body.data() : WINHTTP_NO_REQUEST_DATA,
                                   bodySize,
                                   bodySize,
                                   0);
    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        m_lastError = "WinHTTP send failed";
        return std::string();
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &statusCode,
                        &statusCodeSize,
                        WINHTTP_NO_HEADER_INDEX);

    std::string response;
    DWORD bytesAvailable = 0;
    do {
        bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) {
            break;
        }
        if (bytesAvailable == 0) {
            break;
        }
        std::vector<char> buffer(bytesAvailable + 1, 0);
        DWORD bytesRead = 0;
        if (!WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            break;
        }
        response.append(buffer.data(), bytesRead);
    } while (bytesAvailable > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (statusCode >= 400) {
        m_lastError = "HTTP error: " + std::to_string(statusCode);
        return std::string();
    }

    return response;
}
