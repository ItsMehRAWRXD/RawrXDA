/**
 * @file gold_signer.cpp
 * @brief GOLD_SIGN: EV Certificate Signing Engine (C++20 / Win32)
 *
 * Production EV code signing.  Private keys are hardware-bound (HSM, USB
 * token, or Azure cloud KMS) — never PFX.  Uses signtool.exe with /sha1,
 * /csp, /kc, or AzureSignTool for Azure Trusted Signing.
 */

#include "gold_signer.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <wincrypt.h>
#  pragma comment(lib, "advapi32.lib")
#  pragma comment(lib, "crypt32.lib")
#else
// Stub for non-Windows compilation
#endif

namespace fs = std::filesystem;

namespace RawrXD {

// ═══════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════

GoldSigner::GoldSigner(const GoldSignConfig& config) : m_config(config) {
    m_signtoolPath = resolveSignTool();
}

// ═══════════════════════════════════════════════════════════════════════════
// Mode name
// ═══════════════════════════════════════════════════════════════════════════

const char* GoldSigner::modeName() const {
    switch (m_config.mode) {
    case GoldSignMode::Thumbprint:          return "Thumbprint";
    case GoldSignMode::SubjectName:         return "SubjectName";
    case GoldSignMode::HardwareToken:       return "HardwareToken";
    case GoldSignMode::AzureTrustedSigning: return "AzureTrustedSigning";
    case GoldSignMode::AutoDetect:          return "AutoDetect";
    }
    return "Unknown";
}

// ═══════════════════════════════════════════════════════════════════════════
// Process execution
// ═══════════════════════════════════════════════════════════════════════════

bool GoldSigner::execProcess(const std::string& exe,
                             const std::vector<std::string>& args,
                             int timeoutMs) {
#ifdef _WIN32
    // Build command line
    std::string cmdLine = "\"" + exe + "\"";
    for (const auto& a : args) {
        cmdLine += ' ';
        if (a.find(' ') != std::string::npos || a.find('"') != std::string::npos)
            cmdLine += "\"" + a + "\"";
        else
            cmdLine += a;
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    // Redirect stdout/stderr to pipes for capture
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hReadOut = nullptr, hWriteOut = nullptr;
    CreatePipe(&hReadOut, &hWriteOut, &sa, 0);
    SetHandleInformation(hReadOut, HANDLE_FLAG_INHERIT, 0);
    si.hStdOutput = hWriteOut;
    si.hStdError  = hWriteOut;
    si.dwFlags   |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};

    if (!CreateProcessA(nullptr, const_cast<char*>(cmdLine.c_str()),
                        nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        DWORD err = GetLastError();
        setError("CreateProcess failed for " + exe + " (error " + std::to_string(err) + ")");
        CloseHandle(hReadOut);
        CloseHandle(hWriteOut);
        return false;
    }

    CloseHandle(hWriteOut); // Close write end in parent

    // Read output
    std::string output;
    char buf[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hReadOut, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        output += buf;
    }
    CloseHandle(hReadOut);

    DWORD waitResult = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(timeoutMs));
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        setError("Process timed out: " + exe);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0) {
        setError("signtool exit code " + std::to_string(exitCode) + ": " + output);
        return false;
    }
    return true;
#else
    (void)exe; (void)args; (void)timeoutMs;
    setError("GOLD_SIGN requires Windows");
    return false;
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// Resolve signtool.exe
// ═══════════════════════════════════════════════════════════════════════════

std::string GoldSigner::resolveSignTool() {
    // Explicit config override
    if (!m_config.signtoolPath.empty() && fs::exists(m_config.signtoolPath))
        return m_config.signtoolPath;

    // Environment variable
    const char* envPath = std::getenv("SIGNTOOL_PATH");
    if (envPath && fs::exists(envPath))
        return envPath;

#ifdef _WIN32
    // Search Windows SDK (newest version first)
    const char* sdkRoots[] = {
        "C:\\Program Files (x86)\\Windows Kits\\10\\bin",
        "C:\\Program Files\\Windows Kits\\10\\bin"
    };

    for (const auto& root : sdkRoots) {
        if (!fs::exists(root)) continue;
        std::vector<fs::path> versions;
        for (const auto& entry : fs::directory_iterator(root)) {
            if (entry.is_directory()) versions.push_back(entry.path());
        }
        // Sort descending
        std::sort(versions.begin(), versions.end(), std::greater<>());
        for (const auto& ver : versions) {
            auto tool = ver / "x64" / "signtool.exe";
            if (fs::exists(tool)) return tool.string();
        }
    }

    // VS2022
    const char* vsRoots[] = {
        "C:\\VS2022Enterprise",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools"
    };
    for (const auto& vs : vsRoots) {
        if (!fs::exists(vs)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(vs)) {
            if (entry.path().filename() == "signtool.exe")
                return entry.path().string();
        }
    }
#endif

    return "signtool.exe"; // Hope it's in PATH
}

// ═══════════════════════════════════════════════════════════════════════════
// Detect EV certificate in Windows certificate store
// ═══════════════════════════════════════════════════════════════════════════

std::string GoldSigner::detectEVCertificate() {
#ifdef _WIN32
    // Open certificate store
    HCERTSTORE hStore = CertOpenStore(
        CERT_STORE_PROV_SYSTEM_A, 0, 0,
        CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_READONLY_FLAG,
        m_config.certStore.c_str());

    if (!hStore) {
        // Try machine store
        hStore = CertOpenStore(
            CERT_STORE_PROV_SYSTEM_A, 0, 0,
            CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_READONLY_FLAG,
            m_config.certStore.c_str());
    }

    if (!hStore) {
        setError("Cannot open certificate store: " + m_config.certStore);
        return {};
    }

    // EV Code Signing OID: 2.23.140.1.3
    static const char* kEVCodeSigningOID = "2.23.140.1.3";

    std::string bestThumbprint;
    FILETIME    bestExpiry{};

    PCCERT_CONTEXT pCert = nullptr;
    while ((pCert = CertEnumCertificatesInStore(hStore, pCert)) != nullptr) {
        // Check for code signing EKU
        DWORD cbUsage = 0;
        if (!CertGetEnhancedKeyUsage(pCert, 0, nullptr, &cbUsage) || cbUsage == 0)
            continue;

        std::vector<uint8_t> usageBuf(cbUsage);
        auto* pUsage = reinterpret_cast<CERT_ENHKEY_USAGE*>(usageBuf.data());
        if (!CertGetEnhancedKeyUsage(pCert, 0, pUsage, &cbUsage))
            continue;

        bool isCodeSigning = false;
        for (DWORD i = 0; i < pUsage->cUsageIdentifier; i++) {
            if (std::string(pUsage->rgpszUsageIdentifier[i]) == szOID_PKIX_KP_CODE_SIGNING) {
                isCodeSigning = true;
                break;
            }
        }
        if (!isCodeSigning) continue;

        // Check if expired
        SYSTEMTIME stNow;
        GetSystemTime(&stNow);
        FILETIME ftNow;
        SystemTimeToFileTime(&stNow, &ftNow);
        if (CompareFileTime(&pCert->pCertInfo->NotAfter, &ftNow) < 0)
            continue;

        // Check for EV policy OID in certificate policies extension
        PCERT_EXTENSION pExt = CertFindExtension(
            szOID_CERT_POLICIES, pCert->pCertInfo->cExtension,
            pCert->pCertInfo->rgExtension);

        bool isEV = false;
        if (pExt) {
            DWORD cbDecoded = 0;
            if (CryptDecodeObjectEx(X509_ASN_ENCODING, szOID_CERT_POLICIES,
                                     pExt->Value.pbData, pExt->Value.cbData,
                                     CRYPT_DECODE_ALLOC_FLAG, nullptr,
                                     &cbDecoded, &cbDecoded)) {
                // Simplified: search raw extension data for EV OID string
                std::string extData(reinterpret_cast<const char*>(pExt->Value.pbData),
                                    pExt->Value.cbData);
                if (extData.find("2.23.140.1.3") != std::string::npos)
                    isEV = true;
            }
            // Alternative: check for well-known CA EV OIDs
            // DigiCert EV:    2.16.840.1.114412.3.1
            // Sectigo EV:     1.3.6.1.4.1.6449.1.2.1.5.1
            // GlobalSign EV:  1.3.6.1.4.1.4146.1.1
            std::string rawExt(reinterpret_cast<const char*>(pExt->Value.pbData),
                               pExt->Value.cbData);
            if (rawExt.find("2.16.840.1.114412.3.1") != std::string::npos ||
                rawExt.find("1.3.6.1.4.1.6449.1.2.1.5.1") != std::string::npos ||
                rawExt.find("1.3.6.1.4.1.4146.1.1") != std::string::npos) {
                isEV = true;
            }
        }

        // Extract thumbprint (SHA-1 hash of the cert)
        BYTE thumbHash[20]{};
        DWORD thumbLen = sizeof(thumbHash);
        if (!CertGetCertificateContextProperty(pCert, CERT_HASH_PROP_ID,
                                                thumbHash, &thumbLen))
            continue;

        char thumbStr[41]{};
        for (DWORD i = 0; i < thumbLen; i++)
            snprintf(thumbStr + i * 2, 3, "%02X", thumbHash[i]);

        // Prefer EV certs, but track best non-EV code-signing cert as fallback
        if (isEV || bestThumbprint.empty()) {
            if (isEV || CompareFileTime(&pCert->pCertInfo->NotAfter, &bestExpiry) > 0) {
                bestThumbprint = thumbStr;
                bestExpiry = pCert->pCertInfo->NotAfter;
            }
        }

        if (isEV) {
            fprintf(stderr, "[GOLD_SIGN] Detected EV cert: %s\n", thumbStr);
        }
    }

    CertCloseStore(hStore, 0);

    if (bestThumbprint.empty()) {
        setError("No EV or code-signing certificate found in store");
    }
    return bestThumbprint;
#else
    setError("Certificate store scanning requires Windows");
    return {};
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// Check if already signed
// ═══════════════════════════════════════════════════════════════════════════

bool GoldSigner::isAlreadySigned(const std::string& filePath) {
#ifdef _WIN32
    // Use WinVerifyTrust for Authenticode check
    // Simplified: just call signtool verify
    std::vector<std::string> args = {"verify", "/pa", "/q", filePath};
    // execProcess sets error on failure, but we don't want that here
    std::string savedError = m_lastError;

    std::vector<std::string> verifyArgs = {"verify", "/pa", "/q", filePath};
    bool verified = execProcess(m_signtoolPath, verifyArgs);
    m_lastError = savedError; // Restore

    return verified;
#else
    (void)filePath;
    return false;
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// SHA-256 hash computation
// ═══════════════════════════════════════════════════════════════════════════

std::string GoldSigner::computeSHA256(const std::string& filePath) {
#ifdef _WIN32
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    if (!CryptAcquireContextA(&hProv, nullptr, nullptr,
                               PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        return {};
    }

    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return {};
    }

    std::ifstream in(filePath, std::ios::binary);
    if (!in) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return {};
    }

    char buf[65536];
    while (in.read(buf, sizeof(buf)) || in.gcount() > 0) {
        CryptHashData(hHash, reinterpret_cast<const BYTE*>(buf),
                      static_cast<DWORD>(in.gcount()), 0);
    }

    BYTE hash[32]{};
    DWORD hashLen = sizeof(hash);
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    char hex[65]{};
    for (DWORD i = 0; i < hashLen; i++)
        snprintf(hex + i * 2, 3, "%02x", hash[i]);

    return hex;
#else
    (void)filePath;
    return {};
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// Build signtool arguments
// ═══════════════════════════════════════════════════════════════════════════

std::string GoldSigner::buildSignArgs(const std::string& filePath,
                                       const std::string& algo) {
    std::string args = "sign /fd " + algo;

    switch (m_config.mode) {
    case GoldSignMode::Thumbprint:
        args += " /sha1 " + m_config.thumbprint;
        args += " /s " + m_config.certStore;
        break;

    case GoldSignMode::SubjectName:
        args += " /n \"" + m_config.subjectName + "\"";
        args += " /s " + m_config.certStore;
        break;

    case GoldSignMode::HardwareToken:
        args += " /csp \"" + m_config.cspName + "\"";
        args += " /kc " + m_config.keyContainer;
        if (!m_config.thumbprint.empty())
            args += " /sha1 " + m_config.thumbprint;
        break;

    case GoldSignMode::AutoDetect:
        if (!m_config.thumbprint.empty()) {
            args += " /sha1 " + m_config.thumbprint;
            args += " /s " + m_config.certStore;
        } else {
            args += " /a /s " + m_config.certStore;
        }
        break;

    case GoldSignMode::AzureTrustedSigning:
        // Handled separately via AzureSignTool
        break;
    }

    // Cross-cert for kernel drivers
    if (!m_config.crossCertPath.empty() && fs::exists(m_config.crossCertPath))
        args += " /ac \"" + m_config.crossCertPath + "\"";

    // RFC 3161 timestamp
    args += " /tr " + m_config.timestampUrl;
    args += " /td " + algo;

    // Verbose
    args += " /v";

    // Target file
    args += " \"" + filePath + "\"";

    return args;
}

// ═══════════════════════════════════════════════════════════════════════════
// Sign a single file
// ═══════════════════════════════════════════════════════════════════════════

GoldSignResult GoldSigner::signFile(const std::string& filePath) {
    GoldSignResult result;
    result.filePath = filePath;

    if (!fs::exists(filePath)) {
        result.errorDetail = "File not found: " + filePath;
        setError(result.errorDetail);
        if (onError) onError(result.errorDetail);
        return result;
    }

    result.fileSizeBytes = fs::file_size(filePath);

    // Skip already-signed binaries unless overridden
    if (m_config.skipSigned && isAlreadySigned(filePath)) {
        fprintf(stderr, "[GOLD_SIGN] SKIP (already signed): %s\n", filePath.c_str());
        result.signed_ok = true;
        result.verified_ok = true;
        return result;
    }

    // Auto-detect thumbprint on first use
    if (m_config.mode == GoldSignMode::AutoDetect && m_config.thumbprint.empty()) {
        m_config.thumbprint = detectEVCertificate();
        if (m_config.thumbprint.empty()) {
            result.errorDetail = "No EV certificate detected";
            setError(result.errorDetail);
            if (onError) onError(result.errorDetail);
            return result;
        }
    }

    fprintf(stderr, "[GOLD_SIGN] SIGN_START | Mode: %s | File: %s\n",
            modeName(), filePath.c_str());

    if (m_config.mode == GoldSignMode::AzureTrustedSigning) {
        // Azure Trusted Signing path
        std::vector<std::string> azArgs = {
            "sign",
            "-kvu", m_config.azureEndpoint,
            "-kvt", m_config.azureTenantId,
            "-kvc", m_config.azureProfile,
            "-fd",  m_config.digestAlgorithm,
            "-tr",  m_config.timestampUrl,
            "-td",  m_config.digestAlgorithm,
            "-v",
            filePath
        };

        // Look for AzureSignTool
        std::string azTool = "AzureSignTool.exe";
        if (const char* env = std::getenv("AZURE_SIGN_TOOL_PATH"))
            azTool = env;

        result.signed_ok = execProcess(azTool, azArgs);
    }
    else if (m_config.dualSign) {
        // Dual-sign: SHA1 pass first, then SHA256 appended
        // SHA1 pass via execProcess (safe: no shell interpolation)
        std::string sha1ArgsStr = buildSignArgs(filePath, "SHA1");
        std::vector<std::string> sha1Args;
        // Split the arg string — buildSignArgs returns a space-delimited string
        {
            std::istringstream iss(sha1ArgsStr);
            std::string tok;
            while (iss >> tok) sha1Args.push_back(tok);
        }
        if (!execProcess(m_signtoolPath, sha1Args)) {
            result.errorDetail = "SHA1 signing failed";
            setError(result.errorDetail);
            if (onFileSigned) onFileSigned(filePath, false);
            if (onError) onError(result.errorDetail);
            return result;
        }

        // Append SHA256 signature via execProcess
        std::string sha256ArgsStr = buildSignArgs(filePath, "SHA256");
        std::vector<std::string> sha256Args;
        {
            std::istringstream iss(sha256ArgsStr);
            std::string tok;
            while (iss >> tok) sha256Args.push_back(tok);
        }
        // Insert /as after "sign" token
        auto it = std::find(sha256Args.begin(), sha256Args.end(), "sign");
        if (it != sha256Args.end()) sha256Args.insert(it + 1, "/as");

        result.signed_ok = execProcess(m_signtoolPath, sha256Args);
        if (!result.signed_ok) {
            result.errorDetail = "SHA256 append signing failed";
            setError(result.errorDetail);
        }
    }
    else {
        // Standard single-pass signing
        std::string singleArgsStr = buildSignArgs(filePath, m_config.digestAlgorithm);
        std::vector<std::string> singleArgs;
        {
            std::istringstream iss(singleArgsStr);
            std::string tok;
            while (iss >> tok) singleArgs.push_back(tok);
        }
        result.signed_ok = execProcess(m_signtoolPath, singleArgs);
        if (!result.signed_ok) {
            result.errorDetail = "signtool signing failed";
            setError(result.errorDetail);
        }
    }

    fprintf(stderr, "[GOLD_SIGN] SIGN_%s | File: %s\n",
            result.signed_ok ? "SUCCESS" : "FAILED", filePath.c_str());

    if (onFileSigned) onFileSigned(filePath, result.signed_ok);

    // Post-sign verification
    if (result.signed_ok && m_config.verifyAfter) {
        result.verified_ok = verifyFile(filePath);
    }

    // Compute post-sign hash
    if (result.signed_ok) {
        result.sha256 = computeSHA256(filePath);
    }

    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// Batch-sign a directory
// ═══════════════════════════════════════════════════════════════════════════

std::vector<GoldSignResult> GoldSigner::signDirectory(const std::string& dirPath) {
    std::vector<GoldSignResult> results;

    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        setError("Directory not found: " + dirPath);
        return results;
    }

    fprintf(stderr, "[GOLD_SIGN] Scanning directory: %s\n", dirPath.c_str());

    // Collect signable files
    const std::vector<std::string> extensions = {".exe", ".dll", ".msi", ".msix", ".sys"};

    for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        bool signable = false;
        for (const auto& e : extensions) {
            if (ext == e) { signable = true; break; }
        }
        if (!signable) continue;

        results.push_back(signFile(entry.path().string()));
    }

    // Summary
    int signed_ok = 0, verified_ok = 0, failed = 0;
    for (const auto& r : results) {
        if (r.signed_ok) signed_ok++;
        if (r.verified_ok) verified_ok++;
        if (!r.signed_ok) failed++;
    }

    fprintf(stderr, "[GOLD_SIGN] Directory complete: %d signed, %d verified, %d failed\n",
            signed_ok, verified_ok, failed);

    return results;
}

// ═══════════════════════════════════════════════════════════════════════════
// Verify a signed binary
// ═══════════════════════════════════════════════════════════════════════════

bool GoldSigner::verifyFile(const std::string& filePath) {
#ifdef _WIN32
    std::vector<std::string> args = {"verify", "/pa", "/v", filePath};
    bool ok = execProcess(m_signtoolPath, args);

    fprintf(stderr, "[GOLD_SIGN] VERIFY_%s | File: %s\n",
            ok ? "SUCCESS" : "FAILED", filePath.c_str());

    if (onFileVerified) onFileVerified(filePath, ok);
    return ok;
#else
    (void)filePath;
    setError("Verification requires Windows");
    return false;
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// Write attestation manifest
// ═══════════════════════════════════════════════════════════════════════════

bool GoldSigner::writeAttestation(const std::string& directory,
                                   const std::vector<GoldSignResult>& results) {
    // Build JSON manually (no nlohmann dependency)
    std::ostringstream json;
    json << "{\n";
    json << "  \"schema\": \"gold_sign_attestation_v1\",\n";

    // Timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &time_t);
#else
    gmtime_r(&time_t, &tm_buf);
#endif
    char timeBuf[64]{};
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    json << "  \"timestamp\": \"" << timeBuf << "\",\n";

    // Hostname
#ifdef _WIN32
    char hostname[256]{};
    DWORD hostnameLen = sizeof(hostname);
    GetComputerNameA(hostname, &hostnameLen);
    json << "  \"hostname\": \"" << hostname << "\",\n";
#else
    json << "  \"hostname\": \"unknown\",\n";
#endif

    json << "  \"mode\": \"" << modeName() << "\",\n";
    json << "  \"digest\": \"" << m_config.digestAlgorithm << "\",\n";
    json << "  \"thumbprint\": \"" << m_config.thumbprint << "\",\n";
    json << "  \"timestamp_server\": \"" << m_config.timestampUrl << "\",\n";
    json << "  \"dual_sign\": " << (m_config.dualSign ? "true" : "false") << ",\n";

    // Files array
    json << "  \"files\": [\n";
    for (size_t i = 0; i < results.size(); i++) {
        const auto& r = results[i];
        json << "    {\n";
        json << "      \"path\": \"" << r.filePath << "\",\n";
        json << "      \"sha256\": \"" << r.sha256 << "\",\n";
        json << "      \"size_bytes\": " << r.fileSizeBytes << ",\n";
        json << "      \"signed\": " << (r.signed_ok ? "true" : "false") << ",\n";
        json << "      \"verified\": " << (r.verified_ok ? "true" : "false");
        if (!r.errorDetail.empty())
            json << ",\n      \"error\": \"" << r.errorDetail << "\"";
        json << "\n    }";
        if (i + 1 < results.size()) json << ",";
        json << "\n";
    }
    json << "  ]\n";
    json << "}\n";

    // Write file
    fs::path outPath = fs::path(directory) / "GOLD_SIGN_ATTESTATION.json";
    std::ofstream out(outPath, std::ios::trunc);
    if (!out) {
        setError("Cannot write attestation file: " + outPath.string());
        return false;
    }
    out << json.str();
    out.close();

    fprintf(stderr, "[GOLD_SIGN] Attestation written: %s\n", outPath.string().c_str());
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// Error handling
// ═══════════════════════════════════════════════════════════════════════════

void GoldSigner::setError(const std::string& msg) {
    m_lastError = msg;
    fprintf(stderr, "[GOLD_SIGN] ERROR: %s\n", msg.c_str());
}

} // namespace RawrXD
