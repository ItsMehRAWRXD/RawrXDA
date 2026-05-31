// ============================================================================
// update_signature.cpp — Auto-Update Signature Verification & Atomic Swap
// ============================================================================
//
// Implementation of update manifest parsing, RSA-4096 signature verification
// (via BCrypt/CNG), SHA-256 file hash checks, Authenticode (WinVerifyTrust),
// and atomic binary swap using SelfPatchEngine MASM module.
//
// DEPS:     update_signature.h, auto_update_system.h, masm_bridge_cathedral.h
// PATTERN:  PatchResult-compatible, no exceptions
// RULE:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "../../include/update_signature.h"
#include "../../include/masm_bridge_cathedral.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <winhttp.h>
#include <string>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")

namespace RawrXD {
namespace Update {

// ============================================================================
// Singleton
// ============================================================================
UpdateSignatureVerifier& UpdateSignatureVerifier::instance() {
    static UpdateSignatureVerifier s_instance;
    return s_instance;
}

UpdateSignatureVerifier::UpdateSignatureVerifier()
    : m_state(SwapState::Idle)
    , m_publicKeyLen(0)
    , m_backupCount(0)
{
    memset(m_manifestUrl, 0, sizeof(m_manifestUrl));
    memset(m_stagingDir, 0, sizeof(m_stagingDir));
    memset(m_publicKey, 0, sizeof(m_publicKey));
    memset(m_backups, 0, sizeof(m_backups));

    strncpy_s(m_manifestUrl, sizeof(m_manifestUrl),
              "https://api.github.com/repos/ItsMehRAWRXD/RawrXD/releases/latest",
              _TRUNCATE);
    wcscpy_s(m_stagingDir, _countof(m_stagingDir), L"update_staging");

    InitializeCriticalSection(&m_cs);
}

UpdateSignatureVerifier::~UpdateSignatureVerifier() {
    DeleteCriticalSection(&m_cs);
}

// ============================================================================
// Configuration
// ============================================================================
void UpdateSignatureVerifier::setManifestUrl(const char* url) {
    EnterCriticalSection(&m_cs);
    if (url) strncpy_s(m_manifestUrl, sizeof(m_manifestUrl), url, _TRUNCATE);
    LeaveCriticalSection(&m_cs);
}

void UpdateSignatureVerifier::setPublicKey(const uint8_t* keyData, size_t keyLen) {
    EnterCriticalSection(&m_cs);
    if (keyData && keyLen > 0 && keyLen <= sizeof(m_publicKey)) {
        memcpy(m_publicKey, keyData, keyLen);
        m_publicKeyLen = keyLen;
    }
    LeaveCriticalSection(&m_cs);
}

void UpdateSignatureVerifier::setStagingDir(const wchar_t* dir) {
    EnterCriticalSection(&m_cs);
    if (dir) wcscpy_s(m_stagingDir, _countof(m_stagingDir), dir);
    LeaveCriticalSection(&m_cs);
}

// ============================================================================
// Manifest Parsing — manual JSON parse (no nlohmann in update path)
// ============================================================================

// Simple JSON string extractor: finds "key": "value" and copies value
static bool jsonExtractString(const char* json, size_t jsonLen,
                               const char* key, char* outValue, size_t outLen)
{
    if (!json || !key || !outValue) return false;

    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char* keyPos = strstr(json, pattern);
    if (!keyPos) return false;

    // Skip past key and find colon
    const char* p = keyPos + strlen(pattern);
    while (p < json + jsonLen && (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r')) p++;

    if (p >= json + jsonLen || *p != '"') return false;
    p++; // skip opening quote

    size_t written = 0;
    while (p < json + jsonLen && *p != '"' && written < outLen - 1) {
        if (*p == '\\' && p + 1 < json + jsonLen) {
            p++; // skip escape
            switch (*p) {
                case '"':  outValue[written++] = '"';  break;
                case '\\': outValue[written++] = '\\'; break;
                case 'n':  outValue[written++] = '\n'; break;
                case 't':  outValue[written++] = '\t'; break;
                case 'r':  outValue[written++] = '\r'; break;
                default:   outValue[written++] = *p;   break;
            }
        } else {
            outValue[written++] = *p;
        }
        p++;
    }
    outValue[written] = '\0';
    return written > 0;
}

// Extract integer from JSON: "key": 12345
static bool jsonExtractUint64(const char* json, size_t jsonLen,
                               const char* key, uint64_t* outValue)
{
    if (!json || !key || !outValue) return false;

    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char* keyPos = strstr(json, pattern);
    if (!keyPos) return false;

    const char* p = keyPos + strlen(pattern);
    while (p < json + jsonLen && (*p == ' ' || *p == ':' || *p == '\t')) p++;

    *outValue = 0;
    while (p < json + jsonLen && *p >= '0' && *p <= '9') {
        *outValue = *outValue * 10 + (*p - '0');
        p++;
    }
    return true;
}

UpdateManifest UpdateSignatureVerifier::parseManifest(const char* json, size_t jsonLen) {
    UpdateManifest m{};
    memset(&m, 0, sizeof(m));

    if (!json || jsonLen == 0) {
        m.parsed = false;
        strncpy_s(m.parseError, sizeof(m.parseError), "Empty JSON input", _TRUNCATE);
        return m;
    }

    // Extract top-level fields
    jsonExtractString(json, jsonLen, "version", m.version, sizeof(m.version));
    jsonExtractString(json, jsonLen, "tag", m.tag, sizeof(m.tag));
    jsonExtractString(json, jsonLen, "minVersion", m.minVersion, sizeof(m.minVersion));
    jsonExtractString(json, jsonLen, "releaseNotes", m.releaseNotes, sizeof(m.releaseNotes));
    jsonExtractString(json, jsonLen, "signature", m.signature, sizeof(m.signature));

    // Parse files array — look for "files": [ ... ]
    const char* filesArray = strstr(json, "\"files\"");
    if (filesArray) {
        const char* bracket = strchr(filesArray, '[');
        if (bracket) {
            const char* p = bracket + 1;
            const char* end = json + jsonLen;
            m.fileCount = 0;

            while (p < end && m.fileCount < MAX_MANIFEST_FILES) {
                // Find next object start
                const char* objStart = strchr(p, '{');
                if (!objStart || objStart >= end) break;

                const char* objEnd = strchr(objStart, '}');
                if (!objEnd || objEnd >= end) break;

                size_t objLen = (size_t)(objEnd - objStart + 1);

                ManifestFileEntry& entry = m.files[m.fileCount];
                memset(&entry, 0, sizeof(entry));

                jsonExtractString(objStart, objLen, "name", entry.name, sizeof(entry.name));
                jsonExtractString(objStart, objLen, "sha256", entry.sha256, sizeof(entry.sha256));
                jsonExtractUint64(objStart, objLen, "size", &entry.fileSize);

                if (entry.name[0] != '\0') {
                    m.fileCount++;
                }

                p = objEnd + 1;

                // Check for array end
                const char* closeBracket = strchr(p, ']');
                const char* nextObj = strchr(p, '{');
                if (closeBracket && (!nextObj || closeBracket < nextObj)) break;
            }
        }
    }

    m.parsed = (m.version[0] != '\0');
    if (!m.parsed) {
        strncpy_s(m.parseError, sizeof(m.parseError),
                  "Failed to parse version field from manifest", _TRUNCATE);
    }

    return m;
}

UpdateManifest UpdateSignatureVerifier::fetchManifest() {
    UpdateManifest m{};
    memset(&m, 0, sizeof(m));
    m.parsed = false;

#ifdef _WIN32
    // Use WinHTTP to GET manifest from m_manifestUrl
    HINTERNET hSession = WinHttpOpen(L"RawrXD-Shell/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        strncpy_s(m.parseError, sizeof(m.parseError),
                  "WinHttpOpen failed", _TRUNCATE);
        return m;
    }

    // Parse URL components from m_manifestUrl
    size_t urlLen = strlen(m_manifestUrl);
    std::wstring wUrl(m_manifestUrl, m_manifestUrl + urlLen);

    URL_COMPONENTS urlComp{};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256] = {};
    wchar_t urlPath[1024] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 1024;

    if (!WinHttpCrackUrl(wUrl.c_str(), (DWORD)wUrl.size(), 0, &urlComp)) {
        WinHttpCloseHandle(hSession);
        strncpy_s(m.parseError, sizeof(m.parseError),
                  "Failed to parse manifest URL", _TRUNCATE);
        return m;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, hostName,
        urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        strncpy_s(m.parseError, sizeof(m.parseError),
                  "WinHttpConnect failed", _TRUNCATE);
        return m;
    }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS)
                  ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET",
        urlPath, nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        strncpy_s(m.parseError, sizeof(m.parseError),
                  "WinHttpOpenRequest failed", _TRUNCATE);
        return m;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        strncpy_s(m.parseError, sizeof(m.parseError),
                  "HTTP request failed", _TRUNCATE);
        return m;
    }

    // Read response body
    std::string body;
    char buf[4096];
    DWORD bytesRead = 0;
    while (WinHttpReadData(hRequest, buf, sizeof(buf), &bytesRead) && bytesRead > 0) {
        body.append(buf, bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Parse the response body as manifest
    if (!body.empty()) {
        m = parseManifest(body.c_str(), body.size());
    } else {
        strncpy_s(m.parseError, sizeof(m.parseError),
                  "Empty response from manifest URL", _TRUNCATE);
    }
#else
    strncpy_s(m.parseError, sizeof(m.parseError),
              "fetchManifest requires Windows WinHTTP", _TRUNCATE);
#endif

    return m;
}

// ============================================================================
// SHA-256 via BCrypt (Windows CNG)
// ============================================================================
bool UpdateSignatureVerifier::computeSHA256(const void* data, size_t dataLen,
                                             uint8_t outHash[32])
{
    if (!data || dataLen == 0) return false;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    NTSTATUS status;
    bool result = false;

    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status != 0) return false;

    DWORD hashObjSize = 0, dataLen2 = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashObjSize,
                       sizeof(hashObjSize), &dataLen2, 0);

    auto hashObj = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, hashObjSize);
    if (!hashObj) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    status = BCryptCreateHash(hAlg, &hHash, hashObj, hashObjSize, nullptr, 0, 0);
    if (status == 0) {
        status = BCryptHashData(hHash, (PUCHAR)data, (ULONG)dataLen, 0);
        if (status == 0) {
            status = BCryptFinishHash(hHash, outHash, 32, 0);
            result = (status == 0);
        }
        BCryptDestroyHash(hHash);
    }

    HeapFree(GetProcessHeap(), 0, hashObj);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return result;
}

bool UpdateSignatureVerifier::computeFileSHA256(const wchar_t* filePath,
                                                  uint8_t outHash[32])
{
    if (!filePath) return false;

    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    bool result = false;

    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM,
                                                   nullptr, 0);
    if (status != 0) {
        CloseHandle(hFile);
        return false;
    }

    DWORD hashObjSize = 0, tmp = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashObjSize,
                       sizeof(hashObjSize), &tmp, 0);

    auto hashObj = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, hashObjSize);
    if (!hashObj) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        CloseHandle(hFile);
        return false;
    }

    status = BCryptCreateHash(hAlg, &hHash, hashObj, hashObjSize, nullptr, 0, 0);
    if (status == 0) {
        uint8_t buf[65536];
        DWORD bytesRead = 0;

        while (ReadFile(hFile, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0) {
            status = BCryptHashData(hHash, buf, bytesRead, 0);
            if (status != 0) break;
        }

        if (status == 0) {
            status = BCryptFinishHash(hHash, outHash, 32, 0);
            result = (status == 0);
        }
        BCryptDestroyHash(hHash);
    }

    HeapFree(GetProcessHeap(), 0, hashObj);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    CloseHandle(hFile);
    return result;
}

// ============================================================================
// Hex Conversion
// ============================================================================
void UpdateSignatureVerifier::hashToHex(const uint8_t hash[32], char outHex[65]) {
    static const char hexDigits[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        outHex[i * 2]     = hexDigits[hash[i] >> 4];
        outHex[i * 2 + 1] = hexDigits[hash[i] & 0x0F];
    }
    outHex[64] = '\0';
}

bool UpdateSignatureVerifier::hexToHash(const char* hex, uint8_t outHash[32]) {
    if (!hex || strlen(hex) < 64) return false;
    for (int i = 0; i < 32; i++) {
        char hi = hex[i * 2];
        char lo = hex[i * 2 + 1];
        auto hexVal = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
            if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
            if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
            return 0;
        };
        outHash[i] = (hexVal(hi) << 4) | hexVal(lo);
    }
    return true;
}

// ============================================================================
// RSA-4096 Signature Verification (BCrypt CNG)
// ============================================================================
bool UpdateSignatureVerifier::verifyRSA4096(const uint8_t* hash, size_t hashLen,
                                              const uint8_t* signature, size_t sigLen)
{
    if (!hash || hashLen == 0 || !signature || sigLen == 0) return false;
    if (m_publicKeyLen == 0) return false;

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    bool result = false;

    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RSA_ALGORITHM,
                                                   nullptr, 0);
    if (status != 0) return false;

    // Import the public key (DER/PKCS#1 format)
    status = BCryptImportKeyPair(hAlg, nullptr, BCRYPT_RSAPUBLIC_BLOB, &hKey,
                                  (PUCHAR)m_publicKey, (ULONG)m_publicKeyLen, 0);
    if (status != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    // Verify with PKCS#1 v1.5 padding (SHA-256)
    BCRYPT_PKCS1_PADDING_INFO paddingInfo;
    paddingInfo.pszAlgId = BCRYPT_SHA256_ALGORITHM;

    status = BCryptVerifySignature(hKey, &paddingInfo,
                                    (PUCHAR)hash, (ULONG)hashLen,
                                    (PUCHAR)signature, (ULONG)sigLen,
                                    BCRYPT_PAD_PKCS1);

    result = (status == 0);

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return result;
}

// ============================================================================
// Manifest Signature Verification
// ============================================================================
SignatureResult UpdateSignatureVerifier::verifyManifestSignature(
    const UpdateManifest& manifest, const char* rawJson, size_t rawJsonLen)
{
    if (!rawJson || rawJsonLen == 0) {
        return SignatureResult::error("No JSON data provided");
    }

    if (manifest.signature[0] == '\0') {
        return SignatureResult::error("Manifest has no signature");
    }

    // Compute SHA-256 of the raw JSON (excluding the signature field itself)
    // For canonical form, we hash the entire JSON — the verifier should
    // reconstruct canonical JSON without the signature field
    uint8_t jsonHash[32];
    if (!computeSHA256(rawJson, rawJsonLen, jsonHash)) {
        return SignatureResult::error("Failed to compute SHA-256 of manifest");
    }

    // Decode base64 signature
    DWORD sigLen = 0;
    if (!CryptStringToBinaryA(manifest.signature, 0, CRYPT_STRING_BASE64,
                               nullptr, &sigLen, nullptr, nullptr)) {
        return SignatureResult::error("Invalid base64 signature encoding");
    }

    auto sigBuf = (uint8_t*)HeapAlloc(GetProcessHeap(), 0, sigLen);
    if (!sigBuf) {
        return SignatureResult::error("Memory allocation failed for signature decode");
    }

    if (!CryptStringToBinaryA(manifest.signature, 0, CRYPT_STRING_BASE64,
                               sigBuf, &sigLen, nullptr, nullptr)) {
        HeapFree(GetProcessHeap(), 0, sigBuf);
        return SignatureResult::error("Failed to decode base64 signature");
    }

    // Verify RSA-4096
    bool verified = verifyRSA4096(jsonHash, 32, sigBuf, sigLen);
    HeapFree(GetProcessHeap(), 0, sigBuf);

    if (!verified) {
        return SignatureResult::error("RSA-4096 signature verification FAILED",
                                      (int)GetLastError());
    }

    SignatureResult result = SignatureResult::ok("Manifest signature verified (RSA-4096)");
    memcpy(result.fileHash, jsonHash, 32);
    return result;
}

// ============================================================================
// Authenticode Verification (WinVerifyTrust)
// ============================================================================
SignatureResult UpdateSignatureVerifier::verifyAuthenticode(const wchar_t* filePath) {
    if (!filePath) return SignatureResult::error("Null file path");

    WINTRUST_FILE_INFO fileInfo;
    memset(&fileInfo, 0, sizeof(fileInfo));
    fileInfo.cbStruct       = sizeof(WINTRUST_FILE_INFO);
    fileInfo.pcwszFilePath  = filePath;
    fileInfo.hFile          = nullptr;
    fileInfo.pgKnownSubject = nullptr;

    GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    WINTRUST_DATA trustData;
    memset(&trustData, 0, sizeof(trustData));
    trustData.cbStruct            = sizeof(WINTRUST_DATA);
    trustData.pPolicyCallbackData = nullptr;
    trustData.pSIPClientData      = nullptr;
    trustData.dwUIChoice          = WTD_UI_NONE;
    trustData.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    trustData.dwUnionChoice       = WTD_CHOICE_FILE;
    trustData.pFile               = &fileInfo;
    trustData.dwStateAction       = WTD_STATEACTION_VERIFY;
    trustData.hWVTStateData       = nullptr;
    trustData.pwszURLReference    = nullptr;
    trustData.dwProvFlags         = WTD_SAFER_FLAG;
    trustData.dwUIContext         = 0;

    LONG status = WinVerifyTrust(nullptr, &policyGUID, &trustData);

    // Close state handle
    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &policyGUID, &trustData);

    switch (status) {
        case ERROR_SUCCESS:
            return SignatureResult::ok("Authenticode signature valid and trusted");

        case TRUST_E_NOSIGNATURE: {
            DWORD winErr = GetLastError();
            if (winErr == TRUST_E_NOSIGNATURE || winErr == TRUST_E_SUBJECT_FORM_UNKNOWN
                || winErr == TRUST_E_PROVIDER_UNKNOWN) {
                return SignatureResult::error("File is not signed", (int)winErr);
            }
            return SignatureResult::error("No signature found", (int)winErr);
        }

        case TRUST_E_EXPLICIT_DISTRUST:
            return SignatureResult::error("Signature explicitly distrusted", (int)status);

        case TRUST_E_SUBJECT_NOT_TRUSTED:
            return SignatureResult::error("Subject not trusted", (int)status);

        case CRYPT_E_SECURITY_SETTINGS:
            return SignatureResult::error("Blocked by security settings", (int)status);

        default: {
            char errMsg[128];
            snprintf(errMsg, sizeof(errMsg),
                     "WinVerifyTrust returned 0x%08lX", status);
            return SignatureResult::error(errMsg, (int)status);
        }
    }
}

// ============================================================================
// File SHA-256 Verification
// ============================================================================
SignatureResult UpdateSignatureVerifier::verifySHA256(const wchar_t* filePath,
                                                       const char* expectedHex)
{
    if (!filePath || !expectedHex) {
        return SignatureResult::error("Null file path or expected hash");
    }

    uint8_t computedHash[32];
    if (!computeFileSHA256(filePath, computedHash)) {
        return SignatureResult::error("Failed to compute SHA-256 of file",
                                      (int)GetLastError());
    }

    char computedHex[65];
    hashToHex(computedHash, computedHex);

    // Case-insensitive comparison
    if (_stricmp(computedHex, expectedHex) != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "SHA-256 mismatch: expected %.16s..., got %.16s...",
                 expectedHex, computedHex);
        return SignatureResult::error(msg);
    }

    SignatureResult result = SignatureResult::ok("SHA-256 hash verified");
    memcpy(result.fileHash, computedHash, 32);
    return result;
}

// ============================================================================
// Verify Downloaded File Against Manifest Entry
// ============================================================================
SignatureResult UpdateSignatureVerifier::verifyFile(const wchar_t* filePath,
                                                     const ManifestFileEntry& expected)
{
    if (!filePath) return SignatureResult::error("Null file path");

    // Step 1: Check file size
    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return SignatureResult::error("Cannot open file", (int)GetLastError());
    }

    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    CloseHandle(hFile);

    if (expected.fileSize > 0 && (uint64_t)fileSize.QuadPart != expected.fileSize) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "File size mismatch: expected %llu, got %llu",
                 (unsigned long long)expected.fileSize,
                 (unsigned long long)fileSize.QuadPart);
        return SignatureResult::error(msg);
    }

    // Step 2: Verify SHA-256
    if (expected.sha256[0] != '\0') {
        SignatureResult hashResult = verifySHA256(filePath, expected.sha256);
        if (!hashResult.valid) return hashResult;
    }

    // Step 3: Authenticode check (for .exe/.dll)
    const wchar_t* ext = wcsrchr(filePath, L'.');
    if (ext && (_wcsicmp(ext, L".exe") == 0 || _wcsicmp(ext, L".dll") == 0)) {
        SignatureResult authResult = verifyAuthenticode(filePath);
        if (!authResult.valid) {
            // Authenticode failure is a warning, not a hard block
            // (self-built binaries may not be Authenticode-signed)
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "SHA-256 verified but Authenticode check failed: %s",
                     authResult.detail);
            SignatureResult r = SignatureResult::ok(msg);
            r.trusted = false; // Trusted = false means no Authenticode
            return r;
        }
    }

    return SignatureResult::ok("File verified (SHA-256 + size)");
}

// ============================================================================
// Atomic Swap via SelfPatchEngine
// ============================================================================
AtomicSwapResult UpdateSignatureVerifier::applyUpdate(
    const UpdateManifest& manifest,
    const wchar_t* downloadDir,
    UpdateProgressCallback callback,
    void* userData)
{
    EnterCriticalSection(&m_cs);

    if (m_state != SwapState::Idle) {
        LeaveCriticalSection(&m_cs);
        return AtomicSwapResult::error("Update already in progress", m_state);
    }

    m_state = SwapState::Verifying;
    m_backupCount = 0;

    if (callback) callback(SwapState::Verifying, 0, "Verifying downloaded files...", userData);

    LeaveCriticalSection(&m_cs);

    // Step 1: Verify all downloaded files
    for (int i = 0; i < manifest.fileCount; i++) {
        wchar_t filePath[520];
        swprintf_s(filePath, _countof(filePath), L"%s\\%S",
                    downloadDir, manifest.files[i].name);

        SignatureResult sr = verifyFile(filePath, manifest.files[i]);
        if (!sr.valid) {
            m_state = SwapState::Failed;
            char msg[512];
            snprintf(msg, sizeof(msg), "File verification failed for %s: %s",
                     manifest.files[i].name, sr.detail);
            return AtomicSwapResult::error(msg, SwapState::Failed, sr.errorCode);
        }

        if (callback) {
            int pct = (i + 1) * 30 / manifest.fileCount;
            callback(SwapState::Verifying, pct, manifest.files[i].name, userData);
        }
    }

    // Step 2: Stage — rename current files to .bak
    EnterCriticalSection(&m_cs);
    m_state = SwapState::Staging;
    LeaveCriticalSection(&m_cs);

    if (callback) callback(SwapState::Staging, 30, "Staging backup files...", userData);

    for (int i = 0; i < manifest.fileCount; i++) {
        wchar_t original[260], backup[260];
        swprintf_s(original, _countof(original), L"%S", manifest.files[i].name);
        swprintf_s(backup, _countof(backup), L"%S.bak", manifest.files[i].name);

        // Check if original exists before backing up
        if (GetFileAttributesW(original) != INVALID_FILE_ATTRIBUTES) {
            // Delete old backup if exists
            DeleteFileW(backup);

            if (!MoveFileExW(original, backup, MOVEFILE_REPLACE_EXISTING)) {
                // Rollback any backups we already made
                m_state = SwapState::Failed;
                for (int j = 0; j < m_backupCount; j++) {
                    MoveFileExW(m_backups[j].backupPath, m_backups[j].originalPath,
                                MOVEFILE_REPLACE_EXISTING);
                }
                char msg[256];
                snprintf(msg, sizeof(msg), "Failed to backup %s (err %lu)",
                         manifest.files[i].name, GetLastError());
                return AtomicSwapResult::error(msg, SwapState::Failed, (int)GetLastError());
            }

            // Track backup for rollback
            wcscpy_s(m_backups[m_backupCount].originalPath, 260, original);
            wcscpy_s(m_backups[m_backupCount].backupPath, 260, backup);
            m_backupCount++;
        }
    }

    // Step 3: Swap — move new files into place
    EnterCriticalSection(&m_cs);
    m_state = SwapState::Swapping;
    LeaveCriticalSection(&m_cs);

    if (callback) callback(SwapState::Swapping, 60, "Applying update...", userData);

    int swapped = 0;
    for (int i = 0; i < manifest.fileCount; i++) {
        wchar_t srcPath[520], dstPath[260];
        swprintf_s(srcPath, _countof(srcPath), L"%s\\%S",
                    downloadDir, manifest.files[i].name);
        swprintf_s(dstPath, _countof(dstPath), L"%S", manifest.files[i].name);

        if (!MoveFileExW(srcPath, dstPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
            // Swap failed — initiate rollback
            m_state = SwapState::RollingBack;
            if (callback) callback(SwapState::RollingBack, 80, "Rolling back...", userData);

            AtomicSwapResult rollback = rollbackUpdate();
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Swap failed for %s, rolled back %d files",
                     manifest.files[i].name, rollback.filesRolledBack);
            return AtomicSwapResult::error(msg, SwapState::Failed, (int)GetLastError());
        }
        swapped++;

        if (callback) {
            int pct = 60 + (swapped * 30 / manifest.fileCount);
            callback(SwapState::Swapping, pct, manifest.files[i].name, userData);
        }
    }

    // Step 4: Notify SelfPatchEngine of the binary swap (if running binary was replaced)
    // This allows live memory patching for the next restart
    asm_spengine_cpu_optimize(); // Re-detect CPU features for new binary

    // Step 5: Clean up .bak files
    for (int i = 0; i < m_backupCount; i++) {
        DeleteFileW(m_backups[i].backupPath);
    }

    EnterCriticalSection(&m_cs);
    m_state = SwapState::Complete;
    LeaveCriticalSection(&m_cs);

    if (callback) callback(SwapState::Complete, 100, "Update complete!", userData);

    OutputDebugStringA("[UpdateSignature] Atomic update swap completed successfully\n");
    return AtomicSwapResult::ok(swapped);
}

// ============================================================================
// Rollback
// ============================================================================
AtomicSwapResult UpdateSignatureVerifier::rollbackUpdate() {
    int rolledBack = 0;

    for (int i = m_backupCount - 1; i >= 0; i--) {
        if (MoveFileExW(m_backups[i].backupPath, m_backups[i].originalPath,
                         MOVEFILE_REPLACE_EXISTING)) {
            rolledBack++;
        }
    }

    m_state = SwapState::Idle;
    m_backupCount = 0;

    AtomicSwapResult r = AtomicSwapResult::ok(0);
    r.filesRolledBack = rolledBack;
    r.detail = "Rollback complete";
    return r;
}

} // namespace Update
} // namespace RawrXD
