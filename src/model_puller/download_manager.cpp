// ============================================================================
// download_manager.cpp — WinHTTP Download Engine Implementation
// ============================================================================
// Production WinHTTP download core with resume, SHA256, progress reporting.
// Zero external deps beyond Windows SDK + bcrypt for SHA256.
// ============================================================================

#include "model_puller/download_manager.h"

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <cstring>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace RawrXD {

// ============================================================================
// ParsedUrl
// ============================================================================
bool ParsedUrl::Parse(const std::string& url, ParsedUrl& out) {
    out = {};
    std::string work = url;

    // Scheme
    if (work.rfind("https://", 0) == 0) {
        out.https = true;
        work = work.substr(8);
        out.port = 443;
    } else if (work.rfind("http://", 0) == 0) {
        out.https = false;
        work = work.substr(7);
        out.port = 80;
    } else {
        return false; // unsupported scheme
    }

    // Split host and path
    auto slashPos = work.find('/');
    std::string hostPart = (slashPos != std::string::npos) ? work.substr(0, slashPos) : work;
    out.path = (slashPos != std::string::npos) ? work.substr(slashPos) : "/";

    // Check for port
    auto colonPos = hostPart.find(':');
    if (colonPos != std::string::npos) {
        out.host = hostPart.substr(0, colonPos);
        std::string portStr = hostPart.substr(colonPos + 1);
        unsigned long p = 0;
        try { p = std::stoul(portStr); } catch (...) { return false; }
        if (p == 0 || p > 65535) return false;
        out.port = static_cast<uint16_t>(p);
    } else {
        out.host = hostPart;
    }

    return !out.host.empty();
}

// ============================================================================
// Helpers — wide string conversion
// ============================================================================
static std::wstring ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring ws(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), ws.data(), len);
    return ws;
}

// ============================================================================
// DownloadManager implementation
// ============================================================================
DownloadManager::DownloadManager() = default;

DownloadManager::~DownloadManager() {
    Cancel();
    if (m_asyncThread && m_asyncThread->joinable()) {
        m_asyncThread->join();
    }
}

// ---------------------------------------------------------------------------
// DoHttpGet — low-level WinHTTP GET with Range support
// ---------------------------------------------------------------------------
bool DownloadManager::DoHttpGet(const ParsedUrl& url,
                                const std::string& authHeader,
                                const std::string& rangeHeader,
                                std::function<bool(const char* buf, size_t len)> onData,
                                uint64_t& contentLengthOut,
                                int& httpStatusOut) {
    contentLengthOut = 0;
    httpStatusOut = 0;

    HINTERNET hSession = WinHttpOpen(
        ToWide(m_userAgent).c_str(),
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    // Set timeouts (ms)
    int tms = m_timeoutSeconds * 1000;
    WinHttpSetTimeouts(hSession, tms, tms, tms, tms);

    HINTERNET hConnect = WinHttpConnect(
        hSession,
        ToWide(url.host).c_str(),
        url.port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = url.https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"GET",
        ToWide(url.path).c_str(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Build extra headers
    std::wstring extraHeaders;
    if (!authHeader.empty()) {
        extraHeaders += ToWide(authHeader) + L"\r\n";
    }
    if (!rangeHeader.empty()) {
        extraHeaders += ToWide(rangeHeader) + L"\r\n";
    }

    BOOL sent = WinHttpSendRequest(
        hRequest,
        extraHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : extraHeaders.c_str(),
        extraHeaders.empty() ? 0 : static_cast<DWORD>(-1),
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    BOOL recvd = WinHttpReceiveResponse(hRequest, nullptr);
    if (!recvd) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Read HTTP status
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    httpStatusOut = static_cast<int>(statusCode);

    // Follow redirects: WinHTTP handles them by default, but if we see 3xx
    // that slipped through, treat as failure
    if (httpStatusOut >= 400) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Read Content-Length if available
    wchar_t clBuf[64] = {};
    DWORD clBufSize = sizeof(clBuf);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH,
            WINHTTP_HEADER_NAME_BY_INDEX, clBuf, &clBufSize, WINHTTP_NO_HEADER_INDEX)) {
        try { contentLengthOut = std::stoull(std::wstring(clBuf)); } catch (...) {}
    }

    // Stream data
    bool success = true;
    DWORD dwSize = 0;
    do {
        if (m_cancelled.load()) { success = false; break; }

        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) { success = false; break; }
        if (dwSize == 0) break;

        // Cap single read to 256 KB
        DWORD toRead = (std::min)(dwSize, static_cast<DWORD>(262144));
        std::vector<char> buf(toRead);
        DWORD downloaded = 0;
        if (!WinHttpReadData(hRequest, buf.data(), toRead, &downloaded)) {
            success = false;
            break;
        }
        if (downloaded == 0) break;

        if (onData && !onData(buf.data(), static_cast<size_t>(downloaded))) {
            success = false;
            break;
        }
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return success;
}

// ---------------------------------------------------------------------------
// Download (blocking) with resume
// ---------------------------------------------------------------------------
bool DownloadManager::Download(const std::string& url,
                               const std::string& destPath,
                               ProgressCallback onProgress,
                               bool resume,
                               const std::string& authHeader) {
    m_cancelled = false;
    m_status = DownloadStatus::Connecting;

    ParsedUrl parsed;
    if (!ParsedUrl::Parse(url, parsed)) {
        m_status = DownloadStatus::Failed;
        return false;
    }

    // Ensure destination directory exists
    std::filesystem::path destDir = std::filesystem::path(destPath).parent_path();
    if (!destDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(destDir, ec);
    }

    // Build auth header
    std::string authHdr;
    if (!authHeader.empty()) {
        authHdr = "Authorization: " + authHeader;
    }

    // Retry loop: up to 3 attempts with exponential backoff (2s, 4s, 8s)
    static constexpr int kMaxRetries = 3;
    static constexpr int kBaseBackoffMs = 2000;

    for (int attempt = 0; attempt <= kMaxRetries; ++attempt) {
        if (m_cancelled.load()) {
            m_status = DownloadStatus::Cancelled;
            return false;
        }

        if (attempt > 0) {
            int backoffMs = kBaseBackoffMs * (1 << (attempt - 1)); // 2s, 4s, 8s
            std::cout << "[retry] Attempt " << (attempt + 1) << "/" << (kMaxRetries + 1)
                      << " after " << backoffMs << "ms backoff\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));

            if (m_cancelled.load()) {
                m_status = DownloadStatus::Cancelled;
                return false;
            }
        }

        // Check existing partial file for resume
        uint64_t existingBytes = 0;
        if (resume && std::filesystem::exists(destPath)) {
            std::error_code ec;
            existingBytes = std::filesystem::file_size(destPath, ec);
            if (ec) existingBytes = 0;
        }

        // Build Range header
        std::string rangeHeader;
        if (existingBytes > 0) {
            rangeHeader = "Range: bytes=" + std::to_string(existingBytes) + "-";
        }

        // Open file for writing (append if resuming)
        std::ofstream ofs;
        if (existingBytes > 0) {
            ofs.open(destPath, std::ios::binary | std::ios::app);
        } else {
            ofs.open(destPath, std::ios::binary | std::ios::trunc);
        }
        if (!ofs.is_open()) {
            m_status = DownloadStatus::Failed;
            return false; // Permission denied — no point retrying
        }

        m_status = DownloadStatus::Downloading;

        uint64_t totalWritten = existingBytes;
        uint64_t contentLength = 0;
        int httpStatus = 0;
        auto startTime = std::chrono::steady_clock::now();
        bool writeError = false;

        bool ok = DoHttpGet(parsed, authHdr, rangeHeader,
            [&](const char* buf, size_t len) -> bool {
                ofs.write(buf, static_cast<std::streamsize>(len));
                if (!ofs.good()) { writeError = true; return false; }

                totalWritten += len;

                if (onProgress) {
                    auto now = std::chrono::steady_clock::now();
                    double elapsed = std::chrono::duration<double>(now - startTime).count();
                    uint64_t dlBytes = totalWritten - existingBytes;

                    DownloadProgress p;
                    p.bytesDownloaded = totalWritten;
                    p.totalBytes = contentLength > 0 ? (contentLength + existingBytes) : 0;
                    p.speedBytesPerSec = elapsed > 0.1 ? static_cast<double>(dlBytes) / elapsed : 0.0;
                    p.progressPercent = (p.totalBytes > 0)
                        ? (static_cast<double>(totalWritten) / static_cast<double>(p.totalBytes)) * 100.0
                        : 0.0;
                    p.etaSeconds = (p.speedBytesPerSec > 1.0 && p.totalBytes > totalWritten)
                        ? static_cast<int>(static_cast<double>(p.totalBytes - totalWritten) / p.speedBytesPerSec)
                        : -1;
                    p.currentFile = std::filesystem::path(destPath).filename().string();
                    p.statusText = (attempt > 0) ? "Downloading (retry)" : "Downloading";
                    onProgress(p);
                }
                return true;
            },
            contentLength, httpStatus);

        ofs.close();

        // Disk write error — no retry (disk full, perms, etc.)
        if (writeError) {
            m_status = DownloadStatus::Failed;
            return false;
        }

        if (ok && !m_cancelled.load()) {
            m_status = DownloadStatus::Complete;
            return true;
        }

        if (m_cancelled.load()) {
            m_status = DownloadStatus::Cancelled;
            return false;
        }

        // HTTP 4xx client errors — don't retry (except 429 Too Many Requests)
        if (httpStatus >= 400 && httpStatus < 500 && httpStatus != 429) {
            m_status = DownloadStatus::Failed;
            return false;
        }

        // Network/server error — retry with resume on next iteration
        std::cout << "[retry] Download failed (HTTP " << httpStatus << "), will retry...\n";
    }

    m_status = DownloadStatus::Failed;
    return false;
}

// ---------------------------------------------------------------------------
// DownloadAsync
// ---------------------------------------------------------------------------
void DownloadManager::DownloadAsync(const std::string& url,
                                    const std::string& destPath,
                                    ProgressCallback onProgress,
                                    CompletionCallback onComplete,
                                    bool resume,
                                    const std::string& authHeader) {
    // Join any prior thread
    if (m_asyncThread && m_asyncThread->joinable()) {
        m_asyncThread->join();
    }

    m_asyncThread = std::make_unique<std::thread>(
        [this, url, destPath, onProgress, onComplete, resume, authHeader]() {
            bool ok = Download(url, destPath, onProgress, resume, authHeader);
            if (onComplete) {
                std::string err;
                if (!ok) {
                    if (m_cancelled.load()) err = "Download cancelled";
                    else err = "Download failed (HTTP error or network issue)";
                }
                onComplete(ok, err);
            }
        });
}

// ---------------------------------------------------------------------------
// FetchJSON — GET a URL, return body as string
// ---------------------------------------------------------------------------
bool DownloadManager::FetchJSON(const std::string& url,
                                std::string& responseOut,
                                const std::string& authHeader) {
    m_cancelled = false;
    ParsedUrl parsed;
    if (!ParsedUrl::Parse(url, parsed)) return false;

    std::string authHdr;
    if (!authHeader.empty()) {
        authHdr = "Authorization: " + authHeader;
    }

    responseOut.clear();
    uint64_t contentLength = 0;
    int httpStatus = 0;

    bool ok = DoHttpGet(parsed, authHdr, "",
        [&](const char* buf, size_t len) -> bool {
            // Cap response body at 64 MB to prevent OOM from malicious server
            if (responseOut.size() + len > 67108864) return false;
            responseOut.append(buf, len);
            return true;
        },
        contentLength, httpStatus);

    return ok && (httpStatus >= 200 && httpStatus < 300);
}

// ---------------------------------------------------------------------------
// GetContentLength — HEAD-like via GET with zero-byte range
// ---------------------------------------------------------------------------
bool DownloadManager::GetContentLength(const std::string& url,
                                       uint64_t& sizeOut,
                                       const std::string& authHeader) {
    // Trick: send Range: bytes=0-0, read Content-Range for total
    m_cancelled = false;
    ParsedUrl parsed;
    if (!ParsedUrl::Parse(url, parsed)) return false;

    std::string authHdr;
    if (!authHeader.empty()) {
        authHdr = "Authorization: " + authHeader;
    }

    // We'll just do a regular request and check content-length
    // More accurate: parse Content-Range from 206 response
    uint64_t contentLength = 0;
    int httpStatus = 0;
    bool gotFirst = false;

    DoHttpGet(parsed, authHdr, "Range: bytes=0-0",
        [&](const char*, size_t) -> bool {
            gotFirst = true;
            return false; // stop after first chunk
        },
        contentLength, httpStatus);

    // For a 206 with Range: bytes=0-0 the content-length is 1
    // But we need the total. Let's fall back to a plain HEAD-style
    // by doing a full request and reading Content-Length
    if (httpStatus == 206) {
        // Content-Length for range 0-0 = 1, not useful
        // Retry without range to get full Content-Length
        contentLength = 0;
        DoHttpGet(parsed, authHdr, "",
            [&](const char*, size_t) -> bool {
                return false; // stop immediately
            },
            contentLength, httpStatus);
    }

    sizeOut = contentLength;
    return contentLength > 0;
}

// ---------------------------------------------------------------------------
// SHA256 — bcrypt-based
// ---------------------------------------------------------------------------
std::string DownloadManager::ComputeSHA256(const std::string& filePath) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return "";

    DWORD hashLen = 0, cbData = 0;
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&hashLen, sizeof(hashLen), &cbData, 0);

    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs.is_open()) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    }

    char buf[65536];
    while (ifs.read(buf, sizeof(buf)) || ifs.gcount() > 0) {
        auto n = static_cast<ULONG>(ifs.gcount());
        BCryptHashData(hHash, reinterpret_cast<PUCHAR>(buf), n, 0);
        if (ifs.gcount() < static_cast<std::streamsize>(sizeof(buf))) break;
    }
    ifs.close();

    std::vector<BYTE> hash(hashLen);
    BCryptFinishHash(hHash, hash.data(), hashLen, 0);

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    // Hex encode
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (BYTE b : hash) {
        oss << std::setw(2) << static_cast<unsigned>(b);
    }
    return oss.str();
}

bool DownloadManager::VerifySHA256(const std::string& filePath, const std::string& expectedHash) {
    if (expectedHash.empty()) return true; // no hash to verify
    std::string actual = ComputeSHA256(filePath);
    if (actual.empty()) return false;

    // Case-insensitive compare
    std::string expLower = expectedHash;
    std::transform(expLower.begin(), expLower.end(), expLower.begin(), ::tolower);
    return actual == expLower;
}

void DownloadManager::Cancel() {
    m_cancelled = true;
}

// ============================================================================
// DownloadQueue — concurrent download management (pimpl impl)
// ============================================================================

} // namespace RawrXD — close for condition_variable include
#include <condition_variable>
namespace RawrXD { // reopen

struct DownloadQueue::Impl {
    int                                      maxConcurrent;
    std::atomic<bool>                        shutdown{false};
    mutable std::mutex                       queueMutex;
    std::condition_variable                  queueCV;
    std::vector<QueuedDownload>              pending;
    std::atomic<size_t>                      activeCount{0};
    size_t                                   nextSlotId = 1;
    std::vector<std::unique_ptr<std::thread>> workers;

    explicit Impl(int maxConc) : maxConcurrent(maxConc > 0 ? maxConc : 1) {}

    void WorkerLoop() {
        while (!shutdown.load()) {
            QueuedDownload job;
            {
                std::unique_lock<std::mutex> lk(queueMutex);
                queueCV.wait(lk, [this] { return shutdown.load() || !pending.empty(); });
                if (shutdown.load()) return;
                if (pending.empty()) continue;

                job = std::move(pending.front());
                pending.erase(pending.begin());
            }

            ++activeCount;

            DownloadManager dm;
            bool ok = dm.Download(job.url, job.destPath, job.onProgress, true, job.authHeader);

            --activeCount;

            if (job.onComplete) {
                std::string err;
                if (!ok) err = "Download failed: " + job.url;
                job.onComplete(ok, err);
            }
        }
    }
};

DownloadQueue::DownloadQueue(int maxConcurrent)
    : m_impl(std::make_unique<Impl>(maxConcurrent)) {
    for (int i = 0; i < m_impl->maxConcurrent; ++i) {
        m_impl->workers.push_back(std::make_unique<std::thread>(&Impl::WorkerLoop, m_impl.get()));
    }
}

DownloadQueue::~DownloadQueue() {
    CancelAll();
    m_impl->shutdown = true;
    m_impl->queueCV.notify_all();
    for (auto& w : m_impl->workers) {
        if (w && w->joinable()) w->join();
    }
}

size_t DownloadQueue::Enqueue(const QueuedDownload& item) {
    std::lock_guard<std::mutex> lk(m_impl->queueMutex);
    size_t id = m_impl->nextSlotId++;

    // Insert sorted by priority (higher priority first)
    auto it = m_impl->pending.begin();
    while (it != m_impl->pending.end() && it->priority >= item.priority) ++it;
    m_impl->pending.insert(it, item);

    m_impl->queueCV.notify_one();
    return id;
}

void DownloadQueue::Cancel(size_t slotId) {
    std::lock_guard<std::mutex> lk(m_impl->queueMutex);
    if (slotId > 0 && slotId <= m_impl->pending.size()) {
        m_impl->pending.erase(m_impl->pending.begin() + static_cast<ptrdiff_t>(slotId - 1));
    }
}

void DownloadQueue::CancelAll() {
    std::lock_guard<std::mutex> lk(m_impl->queueMutex);
    m_impl->pending.clear();
}

size_t DownloadQueue::PendingCount() const {
    std::lock_guard<std::mutex> lk(m_impl->queueMutex);
    return m_impl->pending.size();
}

size_t DownloadQueue::ActiveCount() const {
    return m_impl->activeCount.load();
}

} // namespace RawrXD
