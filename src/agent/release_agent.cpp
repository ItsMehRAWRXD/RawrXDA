/**
 * @file release_agent.cpp
 * @brief Automated release pipeline: bump, tag, build, sign, upload, tweet
 * Architecture: C++20, no Qt, no exceptions
 */
#include "release_agent.hpp"
#include "gold_signer.hpp"
#include "json_types.hpp"
#include "self_test_gate.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")
#else
#include <unistd.h>
#endif

namespace {

std::string getEnv(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

std::string readFileContents(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool writeFileContents(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << content;
    return out.good();
}

std::vector<uint8_t> readBinaryFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return {};
    auto sz = in.tellg();
    in.seekg(0);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    in.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}

std::string sha256Hex(const std::vector<uint8_t>& data) {
#ifdef _WIN32
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return {};
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return {};
    }
    CryptHashData(hHash, data.data(), static_cast<DWORD>(data.size()), 0);
    BYTE hash[32];
    DWORD hashLen = 32;
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    char hex[65];
    for (int i = 0; i < 32; ++i)
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    hex[64] = '\0';
    return std::string(hex);
#else
    // POSIX: Built-in SHA-256 (no OpenSSL dependency)
    // Using the same implementation as process_utils.hpp
    static const uint32_t K[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    auto rotr = [](uint32_t x, int n) -> uint32_t { return (x >> n) | (x << (32 - n)); };
    uint32_t h0=0x6a09e667,h1=0xbb67ae85,h2=0x3c6ef372,h3=0xa54ff53a;
    uint32_t h4=0x510e527f,h5=0x9b05688c,h6=0x1f83d9ab,h7=0x5be0cd19;
    const uint8_t* msg = data.data();
    size_t len = data.size();
    uint64_t bitLen = (uint64_t)len * 8;
    size_t paddedLen = ((len + 9 + 63) / 64) * 64;
    std::vector<uint8_t> padded(paddedLen, 0);
    memcpy(padded.data(), msg, len);
    padded[len] = 0x80;
    for (int i = 0; i < 8; ++i) padded[paddedLen - 1 - i] = (uint8_t)(bitLen >> (i * 8));
    for (size_t block = 0; block < paddedLen; block += 64) {
        uint32_t W[64];
        for (int i = 0; i < 16; ++i)
            W[i] = ((uint32_t)padded[block+4*i]<<24)|((uint32_t)padded[block+4*i+1]<<16)|
                   ((uint32_t)padded[block+4*i+2]<<8)|((uint32_t)padded[block+4*i+3]);
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(W[i-15],7)^rotr(W[i-15],18)^(W[i-15]>>3);
            uint32_t s1 = rotr(W[i-2],17)^rotr(W[i-2],19)^(W[i-2]>>10);
            W[i] = W[i-16]+s0+W[i-7]+s1;
        }
        uint32_t a=h0,b=h1,c=h2,d=h3,e=h4,f=h5,g=h6,hh=h7;
        for (int i = 0; i < 64; ++i) {
            uint32_t S1=rotr(e,6)^rotr(e,11)^rotr(e,25);
            uint32_t ch=(e&f)^(~e&g);
            uint32_t t1=hh+S1+ch+K[i]+W[i];
            uint32_t S0=rotr(a,2)^rotr(a,13)^rotr(a,22);
            uint32_t mj=(a&b)^(a&c)^(b&c);
            uint32_t t2=S0+mj;
            hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        h0+=a;h1+=b;h2+=c;h3+=d;h4+=e;h5+=f;h6+=g;h7+=hh;
    }
    char hex[65];
    snprintf(hex,sizeof(hex),"%08x%08x%08x%08x%08x%08x%08x%08x",h0,h1,h2,h3,h4,h5,h6,h7);
    return std::string(hex);
#endif
}

struct ProcResult { int exitCode; std::string stdOut; std::string stdErr; };

ProcResult runCommand(const std::string& cmd) {
    ProcResult r{};
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE hOutRead = nullptr, hOutWrite = nullptr;
    HANDLE hErrRead = nullptr, hErrWrite = nullptr;
    CreatePipe(&hOutRead, &hOutWrite, &sa, 0);
    CreatePipe(&hErrRead, &hErrWrite, &sa, 0);
    SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hErrRead, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hOutWrite; si.hStdError = hErrWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    std::string cmdCopy = cmd;
    if (CreateProcessA(nullptr, cmdCopy.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hOutWrite); CloseHandle(hErrWrite);
        WaitForSingleObject(pi.hProcess, 120000);
        DWORD ec = 0; GetExitCodeProcess(pi.hProcess, &ec);
        r.exitCode = static_cast<int>(ec);
        char buf[4096]; DWORD nr;
        while (ReadFile(hOutRead, buf, sizeof(buf), &nr, nullptr) && nr > 0) r.stdOut.append(buf, nr);
        while (ReadFile(hErrRead, buf, sizeof(buf), &nr, nullptr) && nr > 0) r.stdErr.append(buf, nr);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    } else { r.exitCode = -1; r.stdErr = "CreateProcess failed"; }
    CloseHandle(hOutRead); CloseHandle(hErrRead);
#else
    r.exitCode = system(cmd.c_str());
#endif
    return r;
}

void notifyError(const std::function<void(const std::string&)>& cb, const std::string& msg) {
    fprintf(stderr, "[ERROR] %s\n", msg.c_str());
    if (cb) cb(msg);
}

} // anon

ReleaseAgent::ReleaseAgent()
    : m_version("v1.0.0"), m_changelog("Automated release") {}

bool ReleaseAgent::bumpVersion(const std::string& part) {
    std::string txt = readFileContents("CMakeLists.txt");
    if (txt.empty()) { notifyError(onError, "Failed to open CMakeLists.txt"); return false; }
    std::regex re(R"(project\(RawrXD-ModelLoader VERSION (\d+)\.(\d+)\.(\d+)\))");
    std::smatch m;
    if (!std::regex_search(txt, m, re) || m.size() < 4) {
        notifyError(onError, "Failed to find version in CMakeLists.txt"); return false;
    }
    int major = std::stoi(m[1].str()), minor = std::stoi(m[2].str()), patch = std::stoi(m[3].str());
    if (part == "major") { ++major; minor = 0; patch = 0; }
    else if (part == "minor") { ++minor; patch = 0; }
    else { ++patch; }
    char verBuf[128];
    snprintf(verBuf, sizeof(verBuf), "project(RawrXD-ModelLoader VERSION %d.%d.%d)", major, minor, patch);
    std::string newTxt = std::regex_replace(txt, re, std::string(verBuf));
    if (!writeFileContents("CMakeLists.txt", newTxt)) { notifyError(onError, "Failed to write CMakeLists.txt"); return false; }
    char vBuf[64]; snprintf(vBuf, sizeof(vBuf), "v%d.%d.%d", major, minor, patch);
    m_version = vBuf;
    fprintf(stderr, "[INFO] Version bumped to %s\n", m_version.c_str());
    if (onVersionBumped) onVersionBumped(m_version);
    return true;
}

bool ReleaseAgent::tagAndUpload() {
    bool devMode = (getEnv("RAWRXD_DEV_RELEASE") == "1");
    bool inGitRepo = false;
    if (!devMode) {
        auto probe = runCommand("git rev-parse --is-inside-work-tree");
        if (probe.exitCode == 0) {
            auto out = probe.stdOut;
            while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
            inGitRepo = (out == "true");
        }
    }
    if (inGitRepo) {
        fprintf(stderr, "[INFO] Creating git tag %s\n", m_version.c_str());
        auto r = runCommand("git tag -a " + m_version + " -m \"Auto-release " + m_version + "\"");
        if (r.exitCode != 0) fprintf(stderr, "[WARN] Git tag failed (may already exist): %s\n", r.stdErr.c_str());
    } else {
        fprintf(stderr, "[WARN] Not a git repository; skipping tag step\n");
    }
    fprintf(stderr, "[INFO] Building release binary (RawrXD-Shell target)...\n");
    auto buildResult = runCommand("cmake --build build --config Release --target RawrXD-Shell");
    if (buildResult.exitCode != 0) { notifyError(onError, "Build failed: " + buildResult.stdErr); return false; }
    fprintf(stderr, "[INFO] Build successful\n");
    fprintf(stderr, "[INFO] Running self-test gate...\n");
    if (!runSelfTestGate()) { m_lastError = "Self-test gate failed"; notifyError(onError, m_lastError); return false; }
    fprintf(stderr, "[INFO] Self-test gate PASSED\n");
    if (devMode) { fprintf(stderr, "[INFO] Dev release mode: skipping signing and uploads.\n"); return true; }
    std::string binPath = (std::filesystem::current_path() / "build" / "bin" / "Release" / "RawrXD-Shell.exe").string();
    if (!std::filesystem::exists(binPath)) { notifyError(onError, "Binary not found: " + binPath); return false; }
    // GOLD_SIGN: prefer EV certificate for production releases
    if (!goldSignBinary(binPath)) { notifyError(onError, "Binary signing failed"); return false; }
    auto raw = readBinaryFile(binPath);
    std::string sha256 = sha256Hex(raw);
    std::string blobName = "RawrXD-Shell-" + m_version + ".exe";
    if (!uploadToCDN(binPath, blobName)) return false;
    if (!createGitHubRelease(m_version, m_changelog)) return false;
    if (!updateUpdateManifest(m_version, sha256)) return false;
    if (!tweetRelease(m_changelog)) return false;
    return true;
}

bool ReleaseAgent::tweet(const std::string& text) {
    std::string bearerToken = getEnv("TWITTER_BEARER");
    if (bearerToken.empty()) { fprintf(stderr, "[WARN] TWITTER_BEARER not set, skipping tweet\n"); return true; }
    fprintf(stderr, "[INFO] Would tweet: %s\n", text.c_str());
    if (onTweetSent) onTweetSent(text);
    return true;
}

bool ReleaseAgent::signBinary(const std::string& exePath) {
    std::string certPath = getEnv("CERT_PATH");
    std::string certPass = getEnv("CERT_PASS");
    if (certPath.empty()) { fprintf(stderr, "[WARN] CERT_PATH not set, skipping code signing\n"); return true; }
    std::string signtool = getEnv("SIGNTOOL");
    if (signtool.empty()) signtool = "signtool.exe";
    std::string cmd = signtool + " sign /f \"" + certPath + "\" /p \"" + certPass +
                      "\" /tr http://timestamp.digicert.com /td sha256 /fd sha256 \"" + exePath + "\"";
    auto r = runCommand(cmd);
    if (r.exitCode != 0) { m_lastError = "signtool failed: " + r.stdErr; notifyError(onError, m_lastError); return false; }
    fprintf(stderr, "[INFO] Signed %s\n", exePath.c_str());
    return true;
}

bool ReleaseAgent::uploadToCDN(const std::string& localFile, const std::string& blobName) {
    std::string account = getEnv("AZURE_STORAGE_ACCOUNT");
    std::string key = getEnv("AZURE_STORAGE_KEY");
    if (account.empty() || key.empty()) { m_lastError = "Azure credentials not set"; notifyError(onError, m_lastError); return false; }
    auto data = readBinaryFile(localFile);
    if (data.empty()) { m_lastError = "Cannot open " + localFile; notifyError(onError, m_lastError); return false; }
    std::string url = "https://" + account + ".blob.core.windows.net/updates/" + blobName;
    fprintf(stderr, "[INFO] Would upload %zu bytes to %s\n", data.size(), url.c_str());
    return true;
}

bool ReleaseAgent::createGitHubRelease(const std::string& tag, const std::string& changelog) {
    std::string token = getEnv("GITHUB_TOKEN");
    if (token.empty()) { m_lastError = "GITHUB_TOKEN not set"; notifyError(onError, m_lastError); return false; }
    JsonObject body{{"tag_name", tag}, {"name", tag}, {"body", changelog}, {"draft", false}, {"prerelease", false}};
    fprintf(stderr, "[INFO] Would create GitHub release %s (payload %zu bytes)\n", tag.c_str(), JsonDoc::toJson(body).size());
    if (onReleaseCreated) onReleaseCreated(tag);
    return true;
}

bool ReleaseAgent::updateUpdateManifest(const std::string& tag, const std::string& sha256) {
    JsonObject manifest{{"version", tag}, {"sha256", sha256},
                        {"url", "https://rawrxd.blob.core.windows.net/updates/RawrXD-Shell-" + tag + ".exe"},
                        {"changelog", m_changelog}};
    std::string manifestPath = (std::filesystem::current_path() / "update_manifest.json").string();
    if (!writeFileContents(manifestPath, JsonDoc::toJson(manifest))) {
        m_lastError = "Cannot write manifest"; notifyError(onError, m_lastError); return false;
    }
    return uploadToCDN(manifestPath, "update_manifest.json");
}

bool ReleaseAgent::tweetRelease(const std::string& text) {
    std::string bearer = getEnv("TWITTER_BEARER");
    if (bearer.empty()) { m_lastError = "TWITTER_BEARER not set"; notifyError(onError, m_lastError); return false; }
    fprintf(stderr, "[INFO] Would tweet release: %s\n", text.c_str());
    if (onTweetSent) onTweetSent(text);
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// GOLD_SIGN: EV Certificate Signing for Production Release
// ═══════════════════════════════════════════════════════════════════════════

bool ReleaseAgent::goldSignBinary(const std::string& exePath) {
    using namespace RawrXD;

    // Build config from environment variables
    GoldSignConfig cfg;

    std::string thumbprint = getEnv("GOLD_SIGN_THUMBPRINT");
    std::string subject    = getEnv("GOLD_SIGN_SUBJECT");
    std::string csp        = getEnv("GOLD_SIGN_CSP");
    std::string kc         = getEnv("GOLD_SIGN_KC");
    std::string azEndpoint = getEnv("GOLD_SIGN_AZURE_ENDPOINT");

    // Determine mode
    if (!azEndpoint.empty()) {
        cfg.mode = GoldSignMode::AzureTrustedSigning;
        cfg.azureEndpoint = azEndpoint;
        cfg.azureTenantId = getEnv("GOLD_SIGN_AZURE_TENANT");
        cfg.azureProfile  = getEnv("GOLD_SIGN_AZURE_PROFILE");
    } else if (!csp.empty() && !kc.empty()) {
        cfg.mode = GoldSignMode::HardwareToken;
        cfg.cspName      = csp;
        cfg.keyContainer = kc;
        cfg.tokenPin     = getEnv("GOLD_SIGN_TOKEN_PIN");
        cfg.thumbprint   = thumbprint;
    } else if (!thumbprint.empty()) {
        cfg.mode = GoldSignMode::Thumbprint;
        cfg.thumbprint = thumbprint;
    } else if (!subject.empty()) {
        cfg.mode = GoldSignMode::SubjectName;
        cfg.subjectName = subject;
    } else {
        cfg.mode = GoldSignMode::AutoDetect;
    }

    // Optional overrides
    std::string ts = getEnv("GOLD_SIGN_TIMESTAMP");
    if (!ts.empty()) cfg.timestampUrl = ts;

    std::string store = getEnv("GOLD_SIGN_STORE");
    if (!store.empty()) cfg.certStore = store;

    std::string digest = getEnv("GOLD_SIGN_DIGEST");
    if (!digest.empty()) cfg.digestAlgorithm = digest;

    std::string crossCert = getEnv("GOLD_SIGN_CROSS_CERT");
    if (!crossCert.empty()) cfg.crossCertPath = crossCert;

    std::string signtool = getEnv("SIGNTOOL");
    if (!signtool.empty()) cfg.signtoolPath = signtool;

    cfg.dualSign    = (getEnv("GOLD_SIGN_DUAL") == "1");
    cfg.verifyAfter = true;
    cfg.skipSigned  = true;

    // Create signer
    GoldSigner signer(cfg);

    // Wire callbacks
    signer.onFileSigned = [this](const std::string& file, bool ok) {
        fprintf(stderr, "[ReleaseAgent] GOLD_SIGN %s: %s\n",
                ok ? "OK" : "FAIL", file.c_str());
        if (onGoldSigned) onGoldSigned(file, ok);
    };
    signer.onError = [this](const std::string& msg) {
        notifyError(onError, "GOLD_SIGN: " + msg);
    };

    // Sign
    GoldSignResult result = signer.signFile(exePath);

    if (!result.signed_ok) {
        m_lastError = "GOLD_SIGN failed: " + result.errorDetail;
        notifyError(onError, m_lastError);

        // Fall back to standard signing
        fprintf(stderr, "[ReleaseAgent] Falling back to standard signBinary()...\n");
        return signBinary(exePath);
    }

    fprintf(stderr, "[ReleaseAgent] GOLD_SIGN SUCCESS | SHA256: %s | File: %s\n",
            result.sha256.c_str(), exePath.c_str());
    return true;
}

bool ReleaseAgent::goldSignDirectory(const std::string& buildDir) {
    using namespace RawrXD;

    GoldSignConfig cfg;

    // Same env-var resolution as goldSignBinary
    std::string thumbprint = getEnv("GOLD_SIGN_THUMBPRINT");
    std::string azEndpoint = getEnv("GOLD_SIGN_AZURE_ENDPOINT");

    if (!azEndpoint.empty()) {
        cfg.mode = GoldSignMode::AzureTrustedSigning;
        cfg.azureEndpoint = azEndpoint;
        cfg.azureTenantId = getEnv("GOLD_SIGN_AZURE_TENANT");
        cfg.azureProfile  = getEnv("GOLD_SIGN_AZURE_PROFILE");
    } else if (!thumbprint.empty()) {
        cfg.mode = GoldSignMode::Thumbprint;
        cfg.thumbprint = thumbprint;
    } else {
        cfg.mode = GoldSignMode::AutoDetect;
    }

    std::string ts = getEnv("GOLD_SIGN_TIMESTAMP");
    if (!ts.empty()) cfg.timestampUrl = ts;
    cfg.dualSign    = (getEnv("GOLD_SIGN_DUAL") == "1");
    cfg.verifyAfter = true;
    cfg.skipSigned  = true;

    GoldSigner signer(cfg);

    signer.onFileSigned = [this](const std::string& file, bool ok) {
        if (onGoldSigned) onGoldSigned(file, ok);
    };
    signer.onError = [this](const std::string& msg) {
        notifyError(onError, "GOLD_SIGN: " + msg);
    };

    auto results = signer.signDirectory(buildDir);

    // Write attestation
    signer.writeAttestation(buildDir, results);

    // Check for any failures
    int failed = 0;
    for (const auto& r : results) {
        if (!r.signed_ok) failed++;
    }

    if (failed > 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "GOLD_SIGN: %d of %zu files failed signing",
                 failed, results.size());
        m_lastError = buf;
        notifyError(onError, m_lastError);
        return false;
    }

    fprintf(stderr, "[ReleaseAgent] GOLD_SIGN directory complete: %zu files signed\n",
            results.size());
    return true;
}
