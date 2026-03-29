// Win32_DataDiode_Handler.cpp — see Win32_DataDiode_Handler.hpp

#include "Win32_DataDiode_Handler.hpp"
#include "IDELogger.h"

#include <ShlObj.h>
#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

// After all includes: Windows / CRT macros that break std::expected, `.error()`, and `std::unexpected`.
#ifdef unexpected
#undef unexpected
#endif
#ifdef error
#undef error
#endif

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shell32.lib")

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

namespace rawrxd::data_diode
{
namespace
{

namespace fs = std::filesystem;

constexpr std::uint32_t kBundleVersion = 1;
constexpr char kFileMagic[8] = {'R', 'X', 'D', 'S', 'N', 'E', 'K', '1'};
constexpr std::uint32_t kPbkdf2Iterations = 310000;
constexpr std::size_t kMaxFiles = 4096;
constexpr std::uint64_t kMaxFileBytes = 256ull * 1024ull * 1024ull;
constexpr std::uint64_t kMaxTotalPlaintext = 1024ull * 1024ull * 1024ull;

struct SneakerFileRecord
{
    std::string relUtf8;
    std::vector<std::uint8_t> bytes;
};

bool sha256Digest(const std::uint8_t* data, std::size_t len, std::uint8_t out[32])
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
        return false;
    if (!NT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0)))
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }
    if (!NT_SUCCESS(BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<std::uint8_t*>(data)),
                                   static_cast<ULONG>(len), 0)))
        goto fail;
    {
        ULONG olen = 32;
        if (!NT_SUCCESS(BCryptFinishHash(hHash, out, olen, 0)))
            goto fail;
    }
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return true;
fail:
    if (hHash)
        BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return false;
}

std::string hexLower(const std::uint8_t* b, std::size_t n)
{
    static const char* hx = "0123456789abcdef";
    std::string s(n * 2, '0');
    for (std::size_t i = 0; i < n; ++i)
    {
        s[i * 2] = hx[(b[i] >> 4) & 0xF];
        s[i * 2 + 1] = hx[b[i] & 0xF];
    }
    return s;
}

std::string jsonEsc(std::string_view s)
{
    std::string o;
    o.reserve(s.size() + 8);
    for (char c : s)
    {
        if (c == '\\' || c == '"')
            o += '\\';
        o += c;
    }
    return o;
}

bool writeFileAll(const std::wstring& wpath, const void* data, std::size_t len);

bool writeSneakerChainManifest(const std::vector<SneakerFileRecord>& files, const std::wstring& pathW)
{
    std::uint8_t prevLink[32]{};
    std::ostringstream json;
    json << "{\n  \"schema\": \"rawrxd-sneaker-chain-v1\",\n  \"entries\": [\n";
    for (std::size_t i = 0; i < files.size(); ++i)
    {
        std::uint8_t contentHash[32]{};
        if (!sha256Digest(files[i].bytes.data(), files[i].bytes.size(), contentHash))
            return false;
        std::vector<std::uint8_t> linkIn;
        linkIn.reserve(32 + 32 + 8 + files[i].relUtf8.size());
        linkIn.insert(linkIn.end(), prevLink, prevLink + 32);
        linkIn.insert(linkIn.end(), contentHash, contentHash + 32);
        std::uint64_t sz = static_cast<std::uint64_t>(files[i].bytes.size());
        for (int b = 0; b < 8; ++b)
            linkIn.push_back(static_cast<std::uint8_t>((sz >> (8 * b)) & 0xFF));
        linkIn.insert(linkIn.end(), files[i].relUtf8.begin(), files[i].relUtf8.end());
        std::uint8_t entryHash[32]{};
        if (!sha256Digest(linkIn.data(), linkIn.size(), entryHash))
            return false;

        if (i)
            json << ",\n";
        json << "    {\"filename\":\"" << jsonEsc(files[i].relUtf8) << "\",\"size\":" << sz << ",\"content_sha256\":\""
             << hexLower(contentHash, 32) << "\""
             << ",\"prev_link_hash\":\"" << hexLower(prevLink, 32) << "\""
             << ",\"entry_hash\":\"" << hexLower(entryHash, 32) << "\"}";
        memcpy(prevLink, entryHash, sizeof(prevLink));
    }
    json << "\n  ]\n}\n";
    const std::string blob = json.str();
    return writeFileAll(pathW, blob.data(), blob.size());
}

void appendLe32(std::vector<std::uint8_t>& b, std::uint32_t v)
{
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

void appendLe64(std::vector<std::uint8_t>& b, std::uint64_t v)
{
    for (int i = 0; i < 8; ++i)
        b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}

bool readFileAll(const std::wstring& wpath, std::vector<std::uint8_t>& out)
{
    HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER li{};
    if (!GetFileSizeEx(h, &li))
    {
        CloseHandle(h);
        return false;
    }
    if (li.QuadPart < 0 || static_cast<std::uint64_t>(li.QuadPart) > kMaxFileBytes)
    {
        CloseHandle(h);
        return false;
    }
    out.resize(static_cast<std::size_t>(li.QuadPart));
    DWORD got = 0;
    bool ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &got, nullptr) && got == out.size();
    CloseHandle(h);
    return ok;
}

bool writeFileAll(const std::wstring& wpath, const void* data, std::size_t len)
{
    HANDLE h = CreateFileW(wpath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    DWORD written = 0;
    const BYTE* p = static_cast<const BYTE*>(data);
    std::size_t off = 0;
    while (off < len)
    {
        DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(len - off, 1u << 20));
        if (!WriteFile(h, p + off, chunk, &written, nullptr) || written != chunk)
        {
            CloseHandle(h);
            return false;
        }
        off += chunk;
    }
    CloseHandle(h);
    return true;
}

std::wstring utf8ToWide(std::string_view u)
{
    if (u.empty())
        return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, u.data(), static_cast<int>(u.size()), nullptr, 0);
    if (n <= 0)
        return L"";
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, u.data(), static_cast<int>(u.size()), w.data(), n);
    return w;
}

std::string wideToUtf8(const std::wstring& w)
{
    if (w.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return {};
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::expected<std::vector<std::uint8_t>, DiodeError> deriveKey(std::string_view secret, const std::uint8_t salt[16])
{
    BCRYPT_ALG_HANDLE hPrf = nullptr;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hPrf, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!NT_SUCCESS(st))
        return std::unexpected<DiodeError>(DiodeError::CryptoUnavailable);

    std::vector<std::uint8_t> key(32);
    st = BCryptDeriveKeyPBKDF2(hPrf, reinterpret_cast<PUCHAR>(const_cast<char*>(secret.data())),
                               static_cast<ULONG>(secret.size()), const_cast<PUCHAR>(salt), 16, kPbkdf2Iterations,
                               key.data(), static_cast<ULONG>(key.size()), 0);
    BCryptCloseAlgorithmProvider(hPrf, 0);
    if (!NT_SUCCESS(st))
        return std::unexpected<DiodeError>(DiodeError::CryptoOperationFailed);
    return key;
}

std::expected<std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>, DiodeError> aesGcmEncrypt(
    const std::uint8_t* key32, const std::uint8_t nonce[12], const std::vector<std::uint8_t>& plaintext)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!NT_SUCCESS(st))
        return std::unexpected<DiodeError>(DiodeError::CryptoUnavailable);

    st = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!NT_SUCCESS(st))
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::unexpected<DiodeError>(DiodeError::CryptoOperationFailed);
    }

    BCRYPT_KEY_HANDLE hKey = nullptr;
    st = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)key32, 32, 0);
    if (!NT_SUCCESS(st))
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::unexpected<DiodeError>(DiodeError::CryptoOperationFailed);
    }

    std::vector<std::uint8_t> ciphertext(plaintext.size());
    std::vector<std::uint8_t> tag(16);
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth{};
    BCRYPT_INIT_AUTH_MODE_INFO(auth);
    auth.pbNonce = const_cast<PUCHAR>(nonce);
    auth.cbNonce = 12;
    auth.pbTag = tag.data();
    auth.cbTag = 16;

    ULONG outLen = 0;
    st = BCryptEncrypt(hKey, const_cast<PUCHAR>(plaintext.data()), static_cast<ULONG>(plaintext.size()), &auth, nullptr,
                       0, ciphertext.data(), static_cast<ULONG>(ciphertext.size()), &outLen, 0);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!NT_SUCCESS(st) || outLen != ciphertext.size())
        return std::unexpected<DiodeError>(DiodeError::CryptoOperationFailed);

    return std::pair<std::vector<std::uint8_t>, std::vector<std::uint8_t>>{std::move(ciphertext), std::move(tag)};
}

std::expected<std::vector<std::uint8_t>, DiodeError> aesGcmDecrypt(const std::uint8_t* key32,
                                                                   const std::uint8_t nonce[12],
                                                                   const std::uint8_t* tag16,
                                                                   const std::vector<std::uint8_t>& ciphertext)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    NTSTATUS st = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (!NT_SUCCESS(st))
        return std::unexpected<DiodeError>(DiodeError::CryptoUnavailable);

    st = BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!NT_SUCCESS(st))
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::unexpected<DiodeError>(DiodeError::CryptoOperationFailed);
    }

    BCRYPT_KEY_HANDLE hKey = nullptr;
    st = BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, (PUCHAR)key32, 32, 0);
    if (!NT_SUCCESS(st))
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return std::unexpected<DiodeError>(DiodeError::CryptoOperationFailed);
    }

    std::vector<std::uint8_t> plaintext(ciphertext.size());
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth{};
    BCRYPT_INIT_AUTH_MODE_INFO(auth);
    auth.pbNonce = const_cast<PUCHAR>(nonce);
    auth.cbNonce = 12;
    auth.pbTag = const_cast<PUCHAR>(tag16);
    auth.cbTag = 16;

    ULONG outLen = 0;
    st = BCryptDecrypt(hKey, const_cast<PUCHAR>(ciphertext.data()), static_cast<ULONG>(ciphertext.size()), &auth,
                       nullptr, 0, plaintext.data(), static_cast<ULONG>(plaintext.size()), &outLen, 0);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (!NT_SUCCESS(st) || outLen != plaintext.size())
        return std::unexpected<DiodeError>(DiodeError::FormatInvalid);

    return plaintext;
}

std::string sha256HexFile(const std::wstring& wpath)
{
    HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return {};

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::string out;

    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
        goto done;
    if (!NT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0)))
        goto done;

    {
        std::vector<std::uint8_t> buf(1 << 20);
        for (;;)
        {
            DWORD r = 0;
            if (!ReadFile(h, buf.data(), static_cast<DWORD>(buf.size()), &r, nullptr))
                break;
            if (r == 0)
                break;
            if (!NT_SUCCESS(BCryptHashData(hHash, buf.data(), r, 0)))
                goto done;
        }
    }
    {
        std::uint8_t hash[32];
        ULONG hlen = 32;
        if (!NT_SUCCESS(BCryptFinishHash(hHash, hash, hlen, 0)))
            goto done;
        char hex[65];
        for (int i = 0; i < 32; ++i)
            snprintf(hex + i * 2, 3, "%02x", hash[i]);
        hex[64] = 0;
        out = hex;
    }
done:
    if (hHash)
        BCryptDestroyHash(hHash);
    if (hAlg)
        BCryptCloseAlgorithmProvider(hAlg, 0);
    CloseHandle(h);
    return out;
}

bool isSafeRelativeUtf8(std::string_view rel)
{
    if (rel.empty() || rel.size() > 4096)
        return false;
    if (rel.find("..") != std::string_view::npos)
        return false;
    if (rel.front() == '/' || rel.front() == '\\')
        return false;
    return true;
}

}  // namespace

const char* diodeErrorMessage(DiodeError e)
{
    switch (e)
    {
        case DiodeError::InvalidArgument:
            return "invalid argument";
        case DiodeError::IoFailure:
            return "I/O failure";
        case DiodeError::CryptoUnavailable:
            return "CNG/BCrypt unavailable";
        case DiodeError::CryptoOperationFailed:
            return "crypto operation failed";
        case DiodeError::PayloadTooLarge:
            return "payload exceeds sovereign caps";
        case DiodeError::FormatInvalid:
            return "invalid bundle or authentication failure";
        case DiodeError::UnpackPathUnsafe:
            return "unsafe path inside bundle";
    }
    return "unknown";
}

std::string defaultDiodeIngestPathUtf8()
{
    wchar_t base[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, base)))
        return std::string("RawrXD\\Sovereign\\DiodeIngest");
    std::wstring w = std::wstring(base) + L"\\RawrXD\\Sovereign\\DiodeIngest";
    return wideToUtf8(w);
}

std::expected<void, DiodeError> ensureDirectoryUtf8(std::string_view dirUtf8)
{
    if (dirUtf8.empty())
        return std::unexpected<DiodeError>(DiodeError::InvalidArgument);
    std::error_code ec;
    fs::create_directories(fs::path(utf8ToWide(std::string(dirUtf8))), ec);
    if (ec)
        return std::unexpected<DiodeError>(DiodeError::IoFailure);
    return {};
}

std::expected<std::vector<StagingFileSummary>, DiodeError> scanDiodeStagingDirectory(std::string_view dirUtf8)
{
    if (dirUtf8.empty())
        return std::unexpected<DiodeError>(DiodeError::InvalidArgument);
    std::error_code ec;
    fs::path root = fs::path(utf8ToWide(std::string(dirUtf8)));
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec))
        return std::unexpected<DiodeError>(DiodeError::IoFailure);

    std::vector<StagingFileSummary> rows;
    for (const auto& ent : fs::directory_iterator(root, ec))
    {
        if (ec)
            return std::unexpected<DiodeError>(DiodeError::IoFailure);
        if (!ent.is_regular_file())
            continue;
        StagingFileSummary row;
        row.relativePath = wideToUtf8(ent.path().filename().wstring());
        row.sizeBytes = static_cast<std::uint64_t>(fs::file_size(ent.path(), ec));
        if (ec)
            return std::unexpected<DiodeError>(DiodeError::IoFailure);
        row.sha256Hex = sha256HexFile(ent.path().wstring());
        if (row.sha256Hex.empty())
            return std::unexpected<DiodeError>(DiodeError::IoFailure);
        rows.push_back(std::move(row));
    }
    IDELogger::getInstance().info("DataDiode", "staging scan: " + std::to_string(rows.size()) + " files");
    return rows;
}

std::expected<void, DiodeError> packSneakernetBundle(std::string_view sourceDirUtf8, std::string_view outputFileUtf8,
                                                     std::string_view secretUtf8)
{
    if (sourceDirUtf8.empty() || outputFileUtf8.empty() || secretUtf8.empty())
        return std::unexpected<DiodeError>(DiodeError::InvalidArgument);

    fs::path src = fs::path(utf8ToWide(std::string(sourceDirUtf8)));
    std::error_code ec;
    if (!fs::exists(src, ec) || !fs::is_directory(src, ec))
        return std::unexpected<DiodeError>(DiodeError::IoFailure);

    std::vector<SneakerFileRecord> pending;
    std::uint64_t totalPlain = 0;

    for (const auto& ent : fs::recursive_directory_iterator(src, ec))
    {
        if (ec)
            return std::unexpected<DiodeError>(DiodeError::IoFailure);
        if (!ent.is_regular_file())
            continue;
        if (pending.size() >= kMaxFiles)
            return std::unexpected<DiodeError>(DiodeError::PayloadTooLarge);

        auto rel = fs::relative(ent.path(), src, ec);
        if (ec)
            return std::unexpected<DiodeError>(DiodeError::IoFailure);
        std::string relUtf8 = wideToUtf8(rel.wstring());
        std::replace(relUtf8.begin(), relUtf8.end(), '\\', '/');
        if (!isSafeRelativeUtf8(relUtf8))
            return std::unexpected<DiodeError>(DiodeError::InvalidArgument);

        std::vector<std::uint8_t> fileBytes;
        if (!readFileAll(ent.path().wstring(), fileBytes))
            return std::unexpected<DiodeError>(DiodeError::IoFailure);
        std::uint64_t sz = static_cast<std::uint64_t>(fileBytes.size());
        if (sz > kMaxFileBytes)
            return std::unexpected<DiodeError>(DiodeError::PayloadTooLarge);

        std::uint64_t entryCost = 4ull + relUtf8.size() + 8ull + sz;
        if (totalPlain + entryCost > kMaxTotalPlaintext)
            return std::unexpected<DiodeError>(DiodeError::PayloadTooLarge);

        totalPlain += entryCost;
        pending.push_back(SneakerFileRecord{std::move(relUtf8), std::move(fileBytes)});
    }

    constexpr char kPay[4] = {'P', 'L', 'D', '1'};
    std::vector<std::uint8_t> framed;
    framed.reserve(static_cast<std::size_t>(totalPlain + 16));
    framed.insert(framed.end(), kPay, kPay + 4);
    appendLe64(framed, static_cast<std::uint64_t>(pending.size()));
    for (const auto& e : pending)
    {
        if (e.relUtf8.size() > 0xFFFFFFFFull)
            return std::unexpected<DiodeError>(DiodeError::PayloadTooLarge);
        appendLe32(framed, static_cast<std::uint32_t>(e.relUtf8.size()));
        framed.insert(framed.end(), e.relUtf8.begin(), e.relUtf8.end());
        appendLe64(framed, static_cast<std::uint64_t>(e.bytes.size()));
        framed.insert(framed.end(), e.bytes.begin(), e.bytes.end());
    }

    const std::uint64_t fileCount = static_cast<std::uint64_t>(pending.size());

    std::uint8_t salt[16];
    std::uint8_t nonce[12];
    if (!NT_SUCCESS(BCryptGenRandom(nullptr, salt, sizeof(salt), BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        return std::unexpected<DiodeError>(DiodeError::CryptoOperationFailed);
    if (!NT_SUCCESS(BCryptGenRandom(nullptr, nonce, sizeof(nonce), BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
        return std::unexpected<DiodeError>(DiodeError::CryptoOperationFailed);

    auto dk = deriveKey(secretUtf8, salt);
    if (!dk)
        return std::unexpected<DiodeError>(dk.error());

    auto enc = aesGcmEncrypt(dk->data(), nonce, framed);
    if (!enc)
        return std::unexpected<DiodeError>(enc.error());

    std::vector<std::uint8_t> fileOut;
    fileOut.reserve(8 + 4 + 16 + 12 + 4 + enc->first.size() + 16);
    fileOut.insert(fileOut.end(), kFileMagic, kFileMagic + 8);
    appendLe32(fileOut, kBundleVersion);
    fileOut.insert(fileOut.end(), salt, salt + 16);
    fileOut.insert(fileOut.end(), nonce, nonce + 12);
    appendLe32(fileOut, static_cast<std::uint32_t>(enc->first.size()));
    fileOut.insert(fileOut.end(), enc->first.begin(), enc->first.end());
    fileOut.insert(fileOut.end(), enc->second.begin(), enc->second.end());

    std::wstring wOut = utf8ToWide(std::string(outputFileUtf8));
    if (!writeFileAll(wOut, fileOut.data(), fileOut.size()))
        return std::unexpected<DiodeError>(DiodeError::IoFailure);

    const std::wstring manifestPath = wOut + L".sneaker-chain.json";
    if (!writeSneakerChainManifest(pending, manifestPath))
        IDELogger::getInstance().warning("DataDiode", "sneaker-chain manifest write failed (bundle is still valid)");

    IDELogger::getInstance().info("DataDiode", std::string("sneakernet pack: ") + std::to_string(fileCount) +
                                                   " files, " + std::to_string(fileOut.size()) + " bytes");
    return {};
}

std::expected<void, DiodeError> unpackSneakernetBundle(std::string_view bundlePathUtf8, std::string_view destDirUtf8,
                                                       std::string_view secretUtf8)
{
    if (bundlePathUtf8.empty() || destDirUtf8.empty() || secretUtf8.empty())
        return std::unexpected<DiodeError>(DiodeError::InvalidArgument);

    std::vector<std::uint8_t> raw;
    {
        std::wstring w = utf8ToWide(std::string(bundlePathUtf8));
        HANDLE h = CreateFileW(w.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                               nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return std::unexpected<DiodeError>(DiodeError::IoFailure);
        LARGE_INTEGER li{};
        if (!GetFileSizeEx(h, &li))
        {
            CloseHandle(h);
            return std::unexpected<DiodeError>(DiodeError::IoFailure);
        }
        if (li.QuadPart <= 0 || static_cast<std::uint64_t>(li.QuadPart) > (kMaxTotalPlaintext + 64 * 1024 * 1024))
        {
            CloseHandle(h);
            return std::unexpected<DiodeError>(DiodeError::PayloadTooLarge);
        }
        raw.resize(static_cast<std::size_t>(li.QuadPart));
        DWORD got = 0;
        bool ok = ReadFile(h, raw.data(), static_cast<DWORD>(raw.size()), &got, nullptr) && got == raw.size();
        CloseHandle(h);
        if (!ok)
            return std::unexpected<DiodeError>(DiodeError::IoFailure);
    }

    if (raw.size() < 8 + 4 + 16 + 12 + 4 + 16)
        return std::unexpected<DiodeError>(DiodeError::FormatInvalid);
    if (memcmp(raw.data(), kFileMagic, 8) != 0)
        return std::unexpected<DiodeError>(DiodeError::FormatInvalid);

    std::uint32_t ver = static_cast<std::uint32_t>(raw[8]) | (static_cast<std::uint32_t>(raw[9]) << 8) |
                        (static_cast<std::uint32_t>(raw[10]) << 16) | (static_cast<std::uint32_t>(raw[11]) << 24);
    if (ver != kBundleVersion)
        return std::unexpected<DiodeError>(DiodeError::FormatInvalid);

    std::uint8_t salt[16];
    memcpy(salt, raw.data() + 12, 16);
    std::uint8_t nonce[12];
    memcpy(nonce, raw.data() + 28, 12);
    std::uint32_t cLen = static_cast<std::uint32_t>(raw[40]) | (static_cast<std::uint32_t>(raw[41]) << 8) |
                         (static_cast<std::uint32_t>(raw[42]) << 16) | (static_cast<std::uint32_t>(raw[43]) << 24);
    std::size_t expected = 44 + static_cast<std::size_t>(cLen) + 16;
    if (raw.size() != expected)
        return std::unexpected<DiodeError>(DiodeError::FormatInvalid);

    std::vector<std::uint8_t> ciphertext(cLen);
    memcpy(ciphertext.data(), raw.data() + 44, cLen);
    std::uint8_t tag[16];
    memcpy(tag, raw.data() + 44 + cLen, 16);

    auto dk = deriveKey(secretUtf8, salt);
    if (!dk)
        return std::unexpected<DiodeError>(dk.error());

    auto decryptedPayload = aesGcmDecrypt(dk->data(), nonce, tag, ciphertext);
    if (!decryptedPayload)
        return std::unexpected<DiodeError>(decryptedPayload.error());

    const std::uint8_t* p = decryptedPayload->data();
    std::size_t remain = decryptedPayload->size();
    if (remain < 4 + 8)
        return std::unexpected<DiodeError>(DiodeError::FormatInvalid);
    if (memcmp(p, "PLD1", 4) != 0)
        return std::unexpected<DiodeError>(DiodeError::FormatInvalid);
    p += 4;
    remain -= 4;
    std::uint64_t count = 0;
    for (int i = 0; i < 8; ++i)
        count |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    p += 8;
    remain -= 8;
    if (count > kMaxFiles)
        return std::unexpected<DiodeError>(DiodeError::FormatInvalid);

    std::error_code ec;
    fs::path destRoot = fs::path(utf8ToWide(std::string(destDirUtf8)));
    fs::create_directories(destRoot, ec);
    if (ec)
        return std::unexpected<DiodeError>(DiodeError::IoFailure);

    for (std::uint64_t i = 0; i < count; ++i)
    {
        if (remain < 4)
            return std::unexpected<DiodeError>(DiodeError::FormatInvalid);
        std::uint32_t plen = static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
                             (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
        p += 4;
        remain -= 4;
        if (remain < plen + 8)
            return std::unexpected<DiodeError>(DiodeError::FormatInvalid);
        std::string rel(reinterpret_cast<const char*>(p), plen);
        p += plen;
        remain -= plen;
        std::uint64_t fsize = 0;
        for (int b = 0; b < 8; ++b)
            fsize |= static_cast<std::uint64_t>(p[b]) << (8 * b);
        p += 8;
        remain -= 8;
        if (remain < fsize)
            return std::unexpected<DiodeError>(DiodeError::FormatInvalid);
        if (!isSafeRelativeUtf8(rel))
            return std::unexpected<DiodeError>(DiodeError::UnpackPathUnsafe);

        fs::path outPath = destRoot / fs::path(utf8ToWide(rel));
        fs::create_directories(outPath.parent_path(), ec);
        if (ec)
            return std::unexpected<DiodeError>(DiodeError::IoFailure);
        if (!writeFileAll(outPath.wstring(), p, static_cast<std::size_t>(fsize)))
            return std::unexpected<DiodeError>(DiodeError::IoFailure);
        p += static_cast<std::size_t>(fsize);
        remain -= static_cast<std::size_t>(fsize);
    }
    if (remain != 0)
        return std::unexpected<DiodeError>(DiodeError::FormatInvalid);

    IDELogger::getInstance().info("DataDiode", std::string("sneakernet unpack: ") + std::to_string(count) + " files");
    return {};
}

}  // namespace rawrxd::data_diode
