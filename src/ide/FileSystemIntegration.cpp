// ═══════════════════════════════════════════════════════════════════════════════
// FileSystemIntegration.cpp
// Live File Watcher + Text Encoding Detection + Dirty Buffer Tracking
// Pure Win32 API, C++20, Zero Dependencies
// ═══════════════════════════════════════════════════════════════════════════════

#include "ide/FileSystemIntegration.hpp"
#include <cstring>
#include <thread>
#include <string>
#include <windows.h>
#include <cstring>
#include <ctime>
#include <vector>
#include <utility>

namespace RawrXD::FileSystem {

// ═════════════════════════════════════════════════════════════════════════════
// TEXT ENCODING DETECTION
// ═════════════════════════════════════════════════════════════════════════════

EncodingInfo DetectEncoding(const uint8_t* data, size_t size) {
    EncodingInfo info{};
    info.encoding   = TextEncoding::UTF8;
    info.lineEnding = LineEnding::UNKNOWN;
    info.bomSize    = 0;
    info.hasBOM     = false;
    info.hasNulls   = false;

    if (!data || size == 0) {
        info.encoding   = TextEncoding::ASCII;
        info.lineEnding = LineEnding::LF;
        return info;
    }

    // ── BOM detection ────────────────────────────────────────────────────
    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        info.encoding = TextEncoding::UTF8_BOM;
        info.hasBOM   = true;
        info.bomSize  = 3;
    } else if (size >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        info.encoding = TextEncoding::UTF16_LE;
        info.hasBOM   = true;
        info.bomSize  = 2;
    } else if (size >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
        info.encoding = TextEncoding::UTF16_BE;
        info.hasBOM   = true;
        info.bomSize  = 2;
    }

    // ── Heuristic scan (first 8KB) ───────────────────────────────────────
    size_t scanLimit = (size < 8192) ? size : 8192;
    bool hasHighBytes     = false;
    bool hasNullBytes     = false;
    bool validUTF8        = true;
    uint32_t crlfCount    = 0;
    uint32_t lfCount      = 0;
    uint32_t crOnlyCount  = 0;

    for (size_t i = info.bomSize; i < scanLimit; ) {
        uint8_t b = data[i];

        // Null byte check (binary indicator, or UTF-16)
        if (b == 0x00) {
            hasNullBytes = true;
            ++i;
            continue;
        }

        // Line ending detection
        if (b == '\r') {
            if (i + 1 < scanLimit && data[i + 1] == '\n') {
                ++crlfCount;
                i += 2;
                continue;
            } else {
                ++crOnlyCount;
                ++i;
                continue;
            }
        }
        if (b == '\n') {
            ++lfCount;
            ++i;
            continue;
        }

        // High-byte / UTF-8 multi-byte validation
        if (b >= 0x80) {
            hasHighBytes = true;
            // Validate UTF-8 sequence
            uint32_t expected = 0;
            if      ((b & 0xE0) == 0xC0) expected = 1;
            else if ((b & 0xF0) == 0xE0) expected = 2;
            else if ((b & 0xF8) == 0xF0) expected = 3;
            else { validUTF8 = false; ++i; continue; }

            for (uint32_t j = 0; j < expected; ++j) {
                if (i + 1 + j >= scanLimit || (data[i + 1 + j] & 0xC0) != 0x80) {
                    validUTF8 = false;
                    break;
                }
            }
            i += 1 + expected;
        } else {
            ++i;
        }
    }

    // ── Encoding decision ────────────────────────────────────────────────
    if (!info.hasBOM) {
        if (hasNullBytes) {
            // Null bytes without BOM — likely UTF-16 without BOM, or binary
            // Check for alternating-null pattern (UTF-16 LE ascii)
            bool utf16Pattern = true;
            for (size_t i = 1; i < scanLimit && i < 64; i += 2) {
                if (data[i] != 0x00) { utf16Pattern = false; break; }
            }
            if (utf16Pattern && size >= 4) {
                info.encoding = TextEncoding::UTF16_LE;
            } else {
                info.hasNulls = true; // Binary file
                info.encoding = TextEncoding::UNKNOWN;
            }
        } else if (!hasHighBytes) {
            info.encoding = TextEncoding::ASCII;
        } else if (validUTF8) {
            info.encoding = TextEncoding::UTF8;
        } else {
            info.encoding = TextEncoding::LATIN1;
        }
    }

    info.hasNulls = hasNullBytes;

    // ── Line ending decision ─────────────────────────────────────────────
    uint32_t totalEndings = crlfCount + lfCount + crOnlyCount;
    if (totalEndings == 0) {
        info.lineEnding = LineEnding::LF; // Default for single-line files
    } else if (crlfCount > 0 && lfCount == 0 && crOnlyCount == 0) {
        info.lineEnding = LineEnding::CRLF;
    } else if (lfCount > 0 && crlfCount == 0 && crOnlyCount == 0) {
        info.lineEnding = LineEnding::LF;
    } else if (crOnlyCount > 0 && crlfCount == 0 && lfCount == 0) {
        info.lineEnding = LineEnding::CR;
    } else {
        info.lineEnding = LineEnding::MIXED;
    }

    return info;
}

std::string DecodeToUTF8(const uint8_t* data, size_t size, const EncodingInfo& info) {
    if (!data || size == 0) return {};

    const uint8_t* start = data + info.bomSize;
    size_t contentSize    = size - info.bomSize;

    switch (info.encoding) {
        case TextEncoding::UTF8:
        case TextEncoding::UTF8_BOM:
        case TextEncoding::ASCII:
            // Already UTF-8 compatible
            return std::string(reinterpret_cast<const char*>(start), contentSize);

        case TextEncoding::UTF16_LE: {
            // Convert UTF-16 LE to UTF-8 using Win32 WideCharToMultiByte
            int wcharCount = static_cast<int>(contentSize / 2);
            if (wcharCount == 0) return {};

            const wchar_t* wstr = reinterpret_cast<const wchar_t*>(start);
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wstr, wcharCount, nullptr, 0, nullptr, nullptr);
            if (utf8Len <= 0) return {};

            std::string result(utf8Len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wstr, wcharCount, result.data(), utf8Len, nullptr, nullptr);
            return result;
        }

        case TextEncoding::UTF16_BE: {
            // Byte-swap to LE, then convert
            std::vector<uint8_t> swapped(contentSize);
            for (size_t i = 0; i + 1 < contentSize; i += 2) {
                swapped[i]     = start[i + 1];
                swapped[i + 1] = start[i];
            }
            int wcharCount = static_cast<int>(contentSize / 2);
            const wchar_t* wstr = reinterpret_cast<const wchar_t*>(swapped.data());
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wstr, wcharCount, nullptr, 0, nullptr, nullptr);
            if (utf8Len <= 0) return {};

            std::string result(utf8Len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wstr, wcharCount, result.data(), utf8Len, nullptr, nullptr);
            return result;
        }

        case TextEncoding::LATIN1: {
            // Latin-1 → UTF-8 (each byte 0x80-0xFF becomes 2-byte UTF-8)
            std::string result;
            result.reserve(contentSize + contentSize / 4);
            for (size_t i = 0; i < contentSize; ++i) {
                uint8_t c = start[i];
                if (c < 0x80) {
                    result.push_back(static_cast<char>(c));
                } else {
                    result.push_back(static_cast<char>(0xC0 | (c >> 6)));
                    result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
                }
            }
            return result;
        }

        default:
            return std::string(reinterpret_cast<const char*>(start), contentSize);
    }
}

std::vector<uint8_t> EncodeFromUTF8(const std::string& utf8, const EncodingInfo& info) {
    std::vector<uint8_t> result;

    // Add BOM if original had one
    if (info.hasBOM) {
        switch (info.encoding) {
            case TextEncoding::UTF8_BOM:
                result.push_back(0xEF);
                result.push_back(0xBB);
                result.push_back(0xBF);
                break;
            case TextEncoding::UTF16_LE:
                result.push_back(0xFF);
                result.push_back(0xFE);
                break;
            case TextEncoding::UTF16_BE:
                result.push_back(0xFE);
                result.push_back(0xFF);
                break;
            default:
                break;
        }
    }

    switch (info.encoding) {
        case TextEncoding::UTF8:
        case TextEncoding::UTF8_BOM:
        case TextEncoding::ASCII:
            result.insert(result.end(), utf8.begin(), utf8.end());
            break;

        case TextEncoding::UTF16_LE: {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
            if (wlen > 0) {
                std::vector<wchar_t> wbuf(wlen);
                MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wbuf.data(), wlen);
                const uint8_t* raw = reinterpret_cast<const uint8_t*>(wbuf.data());
                result.insert(result.end(), raw, raw + wlen * 2);
            }
            break;
        }

        case TextEncoding::UTF16_BE: {
            int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
            if (wlen > 0) {
                std::vector<wchar_t> wbuf(wlen);
                MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), wbuf.data(), wlen);
                // Byte-swap to BE
                const uint8_t* raw = reinterpret_cast<const uint8_t*>(wbuf.data());
                for (int i = 0; i < wlen * 2; i += 2) {
                    result.push_back(raw[i + 1]);
                    result.push_back(raw[i]);
                }
            }
            break;
        }

        case TextEncoding::LATIN1: {
            // UTF-8 → Latin-1 (lossy for chars > 0xFF)
            for (size_t i = 0; i < utf8.size(); ) {
                uint8_t b = static_cast<uint8_t>(utf8[i]);
                if (b < 0x80) {
                    result.push_back(b);
                    ++i;
                } else if ((b & 0xE0) == 0xC0 && i + 1 < utf8.size()) {
                    uint32_t cp = ((b & 0x1F) << 6) | (static_cast<uint8_t>(utf8[i + 1]) & 0x3F);
                    result.push_back(cp <= 0xFF ? static_cast<uint8_t>(cp) : '?');
                    i += 2;
                } else {
                    result.push_back('?'); // Lossy
                    // Skip multi-byte
                    if      ((b & 0xF0) == 0xE0) i += 3;
                    else if ((b & 0xF8) == 0xF0) i += 4;
                    else ++i;
                }
            }
            break;
        }

        default:
            result.insert(result.end(), utf8.begin(), utf8.end());
            break;
    }

    return result;
}

std::string NormalizeLineEndings(const std::string& text, LineEnding target) {
    std::string result;
    result.reserve(text.size());

    const char* targetStr = (target == LineEnding::CRLF) ? "\r\n" :
                            (target == LineEnding::CR)   ? "\r" : "\n";

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                ++i; // Skip the \n in CRLF
            }
            result.append(targetStr);
        } else if (text[i] == '\n') {
            result.append(targetStr);
        } else {
            result.push_back(text[i]);
        }
    }

    return result;
}

const char* EncodingName(TextEncoding enc) {
    switch (enc) {
        case TextEncoding::UTF8:     return "UTF-8";
        case TextEncoding::UTF8_BOM: return "UTF-8 with BOM";
        case TextEncoding::UTF16_LE: return "UTF-16 LE";
        case TextEncoding::UTF16_BE: return "UTF-16 BE";
        case TextEncoding::ASCII:    return "ASCII";
        case TextEncoding::LATIN1:   return "Windows-1252";
        default:                     return "Unknown";
    }
}

const char* LineEndingName(LineEnding le) {
    switch (le) {
        case LineEnding::CRLF:  return "CRLF";
        case LineEnding::LF:    return "LF";
        case LineEnding::CR:    return "CR";
        case LineEnding::MIXED: return "Mixed";
        default:                return "?";
    }
}


// ═════════════════════════════════════════════════════════════════════════════
// DIRTY BUFFER TRACKING
// ═════════════════════════════════════════════════════════════════════════════

bool BufferManager::ReadFileRaw(const std::wstring& path, std::vector<uint8_t>& out_data) {
    HANDLE hFile = CreateFileW(
        path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart > 100 * 1024 * 1024) {
        // Cap at 100 MB for text buffers
        CloseHandle(hFile);
        return false;
    }

    out_data.resize(static_cast<size_t>(fileSize.QuadPart));
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, out_data.data(), static_cast<DWORD>(out_data.size()), &bytesRead, nullptr);
    CloseHandle(hFile);

    if (!ok || bytesRead != out_data.size()) {
        out_data.clear();
        return false;
    }
    return true;
}

bool BufferManager::WriteFileRaw(const std::wstring& path, const uint8_t* data, size_t size) {
    // Write to temp file first, then rename (atomic save)
    std::wstring tmpPath = path + L".rawrxd_tmp";

    HANDLE hFile = CreateFileW(
        tmpPath.c_str(), GENERIC_WRITE, 0,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD bytesWritten = 0;
    BOOL ok = WriteFile(hFile, data, static_cast<DWORD>(size), &bytesWritten, nullptr);
    FlushFileBuffers(hFile);
    CloseHandle(hFile);

    if (!ok || bytesWritten != size) {
        DeleteFileW(tmpPath.c_str());
        return false;
    }

    // Atomic rename: delete old, rename temp → target
    // MoveFileExW with MOVEFILE_REPLACE_EXISTING is atomic on NTFS
    if (!MoveFileExW(tmpPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        // Fallback: try direct rename
        DeleteFileW(path.c_str());
        if (!MoveFileW(tmpPath.c_str(), path.c_str())) {
            DeleteFileW(tmpPath.c_str());
            return false;
        }
    }

    return true;
}

uint64_t BufferManager::GetFileModTime(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
        return 0;
    ULARGE_INTEGER uli;
    uli.LowPart  = fad.ftLastWriteTime.dwLowDateTime;
    uli.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    // Convert 100-ns intervals since 1601 to epoch ms
    return (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
}

bool BufferManager::OpenFile(const std::wstring& path, BufferState& out_state) {
    std::vector<uint8_t> raw;
    if (!ReadFileRaw(path, raw)) return false;

    // Detect encoding
    EncodingInfo enc = DetectEncoding(raw.data(), raw.size());

    // Decode to UTF-8
    std::string utf8Content = DecodeToUTF8(raw.data(), raw.size(), enc);

    // Populate state
    // Convert wstring path to narrow string for filePath member
    int narrowLen = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrowPath(narrowLen - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, narrowPath.data(), narrowLen, nullptr, nullptr);

    out_state.filePath           = narrowPath;
    out_state.content            = utf8Content;
    out_state.diskContent        = utf8Content;
    out_state.encoding           = enc;
    out_state.diskModTimeEpochMs = GetFileModTime(path);
    out_state.lastEditEpochMs    = out_state.diskModTimeEpochMs;
    out_state.editSequence       = 0;
    out_state.savedAtSequence    = 0;
    out_state.isNewFile          = false;
    out_state.externallyModified = false;

    return true;
}

bool BufferManager::SaveFile(BufferState& state) {
    if (state.filePath.empty()) return false;

    // Normalize line endings to match detected style
    std::string toSave = NormalizeLineEndings(state.content, state.encoding.lineEnding);

    // Encode back to original encoding
    std::vector<uint8_t> encoded = EncodeFromUTF8(toSave, state.encoding);

    // Convert narrow path to wide
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, state.filePath.c_str(), -1, nullptr, 0);
    std::wstring widePath(wideLen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, state.filePath.c_str(), -1, widePath.data(), wideLen);

    if (!WriteFileRaw(widePath, encoded.data(), encoded.size()))
        return false;

    // Update state
    state.diskContent        = state.content;
    state.savedAtSequence    = state.editSequence;
    state.diskModTimeEpochMs = GetFileModTime(widePath);
    state.externallyModified = false;
    state.isNewFile          = false;

    return true;
}

bool BufferManager::SaveFileAs(BufferState& state, const std::wstring& newPath) {
    int narrowLen = WideCharToMultiByte(CP_UTF8, 0, newPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrowPath(narrowLen - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, newPath.c_str(), -1, narrowPath.data(), narrowLen, nullptr, nullptr);

    state.filePath = narrowPath;
    return SaveFile(state);
}

void BufferManager::MarkEdited(BufferState& state) {
    ++state.editSequence;
    // Get current time in epoch ms
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart  = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    state.lastEditEpochMs = (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
}

bool BufferManager::CheckExternalModification(BufferState& state) {
    if (state.filePath.empty() || state.isNewFile) return false;

    int wideLen = MultiByteToWideChar(CP_UTF8, 0, state.filePath.c_str(), -1, nullptr, 0);
    std::wstring widePath(wideLen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, state.filePath.c_str(), -1, widePath.data(), wideLen);

    uint64_t currentModTime = GetFileModTime(widePath);
    if (currentModTime != state.diskModTimeEpochMs) {
        state.externallyModified = true;
        return true;
    }
    return false;
}

bool BufferManager::ReloadFromDisk(BufferState& state) {
    if (state.filePath.empty()) return false;

    int wideLen = MultiByteToWideChar(CP_UTF8, 0, state.filePath.c_str(), -1, nullptr, 0);
    std::wstring widePath(wideLen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, state.filePath.c_str(), -1, widePath.data(), wideLen);

    std::vector<uint8_t> raw;
    if (!ReadFileRaw(widePath, raw)) return false;

    EncodingInfo enc = DetectEncoding(raw.data(), raw.size());
    std::string utf8Content = DecodeToUTF8(raw.data(), raw.size(), enc);

    state.content            = utf8Content;
    state.diskContent        = utf8Content;
    state.encoding           = enc;
    state.diskModTimeEpochMs = GetFileModTime(widePath);
    state.editSequence       = 0;
    state.savedAtSequence    = 0;
    state.externallyModified = false;

    return true;
}


// ═════════════════════════════════════════════════════════════════════════════
// LIVE FILE WATCHER (ReadDirectoryChangesW)
// ═════════════════════════════════════════════════════════════════════════════

DirectoryWatcher::DirectoryWatcher() {
    m_wakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
}

DirectoryWatcher::~DirectoryWatcher() {
    ShutdownAll();
    if (m_wakeEvent) {
        CloseHandle(m_wakeEvent);
        m_wakeEvent = nullptr;
    }
}

bool DirectoryWatcher::Watch(const std::wstring& directoryPath, FileChangeCallback callback) {
    // Open directory handle for overlapped I/O
    HANDLE hDir = CreateFileW(
        directoryPath.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (hDir == INVALID_HANDLE_VALUE) return false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        WatchEntry entry;
        entry.dirPath   = directoryPath;
        entry.dirHandle = hDir;
        entry.callback  = callback;
        entry.active    = true;
        memset(&entry.overlapped, 0, sizeof(OVERLAPPED));
        memset(entry.buffer, 0, sizeof(entry.buffer));

        m_watches.push_back(std::move(entry));

        // Issue first async read
        if (!IssueRead(m_watches.back())) {
            CloseHandle(hDir);
            m_watches.pop_back();
            return false;
        }
    }

    // Start watcher thread if not running
    if (!m_thread.joinable()) {
        m_shutdown = false;
        m_thread = std::thread(&DirectoryWatcher::WatcherThread, this);
    } else {
        // Signal thread to pick up new watch
        SetEvent(m_wakeEvent);
    }

    return true;
}

void DirectoryWatcher::Unwatch(const std::wstring& directoryPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_watches.begin(); it != m_watches.end(); ++it) {
        if (it->dirPath == directoryPath) {
            it->active = false;
            CancelIo(it->dirHandle);
            CloseHandle(it->dirHandle);
            it->dirHandle = INVALID_HANDLE_VALUE;
            m_watches.erase(it);
            break;
        }
    }
}

void DirectoryWatcher::ShutdownAll() {
    m_shutdown = true;
    if (m_wakeEvent) SetEvent(m_wakeEvent);

    if (m_thread.joinable()) {
        m_thread.join();
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& w : m_watches) {
        if (w.dirHandle != INVALID_HANDLE_VALUE) {
            CancelIo(w.dirHandle);
            CloseHandle(w.dirHandle);
            w.dirHandle = INVALID_HANDLE_VALUE;
        }
    }
    m_watches.clear();
}

bool DirectoryWatcher::IsWatching() const {
    return !m_watches.empty();
}

std::vector<std::wstring> DirectoryWatcher::GetWatchedPaths() const {
    std::vector<std::wstring> result;
    // Note: not locking here for const correctness; caller should ensure safety
    for (const auto& w : m_watches) {
        if (w.active) result.push_back(w.dirPath);
    }
    return result;
}

bool DirectoryWatcher::IssueRead(WatchEntry& entry) {
    memset(&entry.overlapped, 0, sizeof(OVERLAPPED));

    DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME  |
                   FILE_NOTIFY_CHANGE_DIR_NAME   |
                   FILE_NOTIFY_CHANGE_SIZE        |
                   FILE_NOTIFY_CHANGE_LAST_WRITE  |
                   FILE_NOTIFY_CHANGE_CREATION;

    BOOL ok = ReadDirectoryChangesW(
        entry.dirHandle,
        entry.buffer,
        sizeof(entry.buffer),
        TRUE,       // Watch subtree (recursive)
        filter,
        nullptr,    // Bytes returned (async — not used)
        &entry.overlapped,
        nullptr     // Completion routine (using GetOverlappedResult instead)
    );

    return ok != FALSE;
}

void DirectoryWatcher::ProcessNotifications(WatchEntry& entry, DWORD bytesTransferred) {
    if (bytesTransferred == 0) return;

    uint8_t* ptr = entry.buffer;

    for (;;) {
        FILE_NOTIFY_INFORMATION* fni = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(ptr);

        // Extract filename (length is in bytes, not chars)
        std::wstring fileName(fni->FileName, fni->FileNameLength / sizeof(wchar_t));

        FileChangeEvent event;
        event.type       = static_cast<FileChangeType>(fni->Action);
        event.filePath   = fileName;
        event.watchedDir = entry.dirPath;

        // Call user callback
        if (entry.callback) {
            try {
                entry.callback(event);
            } catch (...) {
                // Swallow exceptions from callbacks
            }
        }

        if (fni->NextEntryOffset == 0) break;
        ptr += fni->NextEntryOffset;
    }
}

void DirectoryWatcher::WatcherThread() {
    while (!m_shutdown) {
        // Build array of handles to wait on
        std::vector<HANDLE> handles;
        handles.push_back(m_wakeEvent); // Index 0 = wake/shutdown signal

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& w : m_watches) {
                if (w.active && w.dirHandle != INVALID_HANDLE_VALUE) {
                    handles.push_back(w.dirHandle);
                }
            }
        }

        if (handles.size() <= 1) {
            // No watches — just wait for wake event or shutdown
            WaitForSingleObject(m_wakeEvent, 500);
            continue;
        }

        // Wait for any handle to signal (or timeout for periodic checks)
        DWORD result = WaitForMultipleObjects(
            static_cast<DWORD>(handles.size()),
            handles.data(),
            FALSE,      // Wait for ANY
            1000        // 1s timeout for periodic external-mod checks
        );

        if (m_shutdown) break;

        if (result == WAIT_OBJECT_0) {
            // Wake event — new watch added or shutdown requested
            continue;
        }

        // Check all watch entries for completed overlapped I/O
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& w : m_watches) {
                if (!w.active || w.dirHandle == INVALID_HANDLE_VALUE) continue;

                DWORD bytesTransferred = 0;
                if (GetOverlappedResult(w.dirHandle, &w.overlapped, &bytesTransferred, FALSE)) {
                    ProcessNotifications(w, bytesTransferred);
                    // Re-issue the read for next batch
                    IssueRead(w);
                }
            }
        }
    }
}

} // namespace RawrXD::FileSystem
