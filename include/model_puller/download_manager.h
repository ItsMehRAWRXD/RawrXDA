#pragma once
// ============================================================================
// download_manager.h — WinHTTP Download Engine with Resume + SHA256
// ============================================================================
// Core HTTP download layer for the RawrXD Model Puller.
//   - WinHTTP-based (no external dependencies)
//   - HTTP Range resume support
//   - Concurrent chunk downloads
//   - SHA256 integrity verification
//   - Atomic progress callbacks
//   - TLS (HTTPS) native
// ============================================================================

#ifndef RAWRXD_DOWNLOAD_MANAGER_H
#define RAWRXD_DOWNLOAD_MANAGER_H

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <memory>
#include <cstdint>

namespace RawrXD {

// -------------------------------------------------------
// Progress / status types
// -------------------------------------------------------
struct DownloadProgress {
    uint64_t bytesDownloaded  = 0;
    uint64_t totalBytes       = 0;
    double   speedBytesPerSec = 0.0;
    double   progressPercent  = 0.0;
    int      etaSeconds       = -1;
    std::string currentFile;
    std::string statusText;
};

enum class DownloadStatus {
    Idle,
    Connecting,
    Downloading,
    Paused,
    Verifying,
    Complete,
    Failed,
    Cancelled
};

using ProgressCallback   = std::function<void(const DownloadProgress&)>;
using CompletionCallback = std::function<void(bool success, const std::string& error)>;

// -------------------------------------------------------
// Parsed URL helper
// -------------------------------------------------------
struct ParsedUrl {
    bool     https = false;
    std::string host;
    uint16_t port  = 443;
    std::string path;

    static bool Parse(const std::string& url, ParsedUrl& out);
};

// -------------------------------------------------------
// Download Manager — core HTTP download engine
// -------------------------------------------------------
class DownloadManager {
public:
    DownloadManager();
    ~DownloadManager();

    // Non-copyable
    DownloadManager(const DownloadManager&) = delete;
    DownloadManager& operator=(const DownloadManager&) = delete;

    // ---- Single file download (blocking) ----
    bool Download(const std::string& url,
                  const std::string& destPath,
                  ProgressCallback onProgress = nullptr,
                  bool resume = true,
                  const std::string& authHeader = "");

    // ---- Single file download (async) ----
    void DownloadAsync(const std::string& url,
                       const std::string& destPath,
                       ProgressCallback onProgress,
                       CompletionCallback onComplete,
                       bool resume = true,
                       const std::string& authHeader = "");

    // ---- Fetch JSON string from URL (blocking) ----
    bool FetchJSON(const std::string& url,
                   std::string& responseOut,
                   const std::string& authHeader = "");

    // ---- HEAD request to get content-length ----
    bool GetContentLength(const std::string& url,
                          uint64_t& sizeOut,
                          const std::string& authHeader = "");

    // ---- SHA256 file verification ----
    static bool VerifySHA256(const std::string& filePath, const std::string& expectedHash);
    static std::string ComputeSHA256(const std::string& filePath);

    // ---- Control ----
    void Cancel();
    bool IsCancelled() const { return m_cancelled.load(); }
    DownloadStatus GetStatus() const { return m_status.load(); }

    // ---- Settings ----
    void SetTimeout(int seconds) { m_timeoutSeconds = seconds; }
    void SetUserAgent(const std::string& ua) { m_userAgent = ua; }

private:
    // Internal WinHTTP helpers
    bool DoHttpGet(const ParsedUrl& url,
                   const std::string& authHeader,
                   const std::string& rangeHeader,
                   std::function<bool(const char* buf, size_t len)> onData,
                   uint64_t& contentLengthOut,
                   int& httpStatusOut);

    std::atomic<bool>           m_cancelled{false};
    std::atomic<DownloadStatus> m_status{DownloadStatus::Idle};
    std::unique_ptr<std::thread> m_asyncThread;
    std::mutex                   m_mutex;
    int                          m_timeoutSeconds = 600;
    std::string                  m_userAgent = "RawrXD-ModelPuller/1.0";
};

// -------------------------------------------------------
// Download Queue — concurrent download management
// -------------------------------------------------------
struct QueuedDownload {
    std::string url;
    std::string destPath;
    std::string authHeader;
    ProgressCallback   onProgress;
    CompletionCallback onComplete;
    int priority = 0; // higher = sooner
};

class DownloadQueue {
public:
    explicit DownloadQueue(int maxConcurrent = 2);
    ~DownloadQueue();

    // Non-copyable
    DownloadQueue(const DownloadQueue&) = delete;
    DownloadQueue& operator=(const DownloadQueue&) = delete;

    // Enqueue a download. Returns a queue slot ID.
    size_t Enqueue(const QueuedDownload& item);

    // Cancel a specific queued download
    void Cancel(size_t slotId);

    // Cancel all pending + active
    void CancelAll();

    // Get current queue depth
    size_t PendingCount() const;
    size_t ActiveCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace RawrXD

#endif // RAWRXD_DOWNLOAD_MANAGER_H
