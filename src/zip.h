// Minimal ZIP archive reader/writer for VSIX and plugin packages
// Uses Win32 APIs only — no external dependencies
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace RawrXD {

struct ZipEntry {
    std::string name;
    uint64_t compressedSize = 0;
    uint64_t uncompressedSize = 0;
    uint32_t crc32 = 0;
    uint16_t compressionMethod = 0; // 0=stored, 8=deflate
    uint64_t localHeaderOffset = 0;
};

class ZipReader {
public:
    bool open(const std::string& path) {
#ifdef _WIN32
        m_hFile = ::CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                 nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (m_hFile == INVALID_HANDLE_VALUE) return false;

        LARGE_INTEGER fileSize;
        if (!::GetFileSizeEx(m_hFile, &fileSize)) { close(); return false; }
        m_fileSize = static_cast<uint64_t>(fileSize.QuadPart);

        return parseEndOfCentralDir();
#else
        return false;
#endif
    }

    void close() {
#ifdef _WIN32
        if (m_hFile != INVALID_HANDLE_VALUE) {
            ::CloseHandle(m_hFile);
            m_hFile = INVALID_HANDLE_VALUE;
        }
#endif
        m_entries.clear();
    }

    const std::vector<ZipEntry>& entries() const { return m_entries; }

    // Extract a stored (uncompressed) entry to a buffer
    std::vector<uint8_t> extractStored(const ZipEntry& entry) {
        if (entry.compressionMethod != 0) return {}; // Only stored entries
        std::vector<uint8_t> data(static_cast<size_t>(entry.uncompressedSize));
#ifdef _WIN32
        // Seek past local file header to get to data
        LARGE_INTEGER pos;
        pos.QuadPart = static_cast<LONGLONG>(entry.localHeaderOffset);
        ::SetFilePointerEx(m_hFile, pos, nullptr, FILE_BEGIN);

        // Read local header to find data offset
        uint8_t localHeader[30];
        DWORD read = 0;
        ::ReadFile(m_hFile, localHeader, 30, &read, nullptr);
        if (read < 30) return {};

        uint16_t nameLen = localHeader[26] | (localHeader[27] << 8);
        uint16_t extraLen = localHeader[28] | (localHeader[29] << 8);

        // Skip name + extra
        pos.QuadPart = static_cast<LONGLONG>(entry.localHeaderOffset) + 30 + nameLen + extraLen;
        ::SetFilePointerEx(m_hFile, pos, nullptr, FILE_BEGIN);

        ::ReadFile(m_hFile, data.data(), static_cast<DWORD>(data.size()), &read, nullptr);
#endif
        return data;
    }

    bool hasEntry(const std::string& name) const {
        for (const auto& e : m_entries) {
            if (e.name == name) return true;
        }
        return false;
    }

    ~ZipReader() { close(); }

private:
#ifdef _WIN32
    HANDLE m_hFile = INVALID_HANDLE_VALUE;
#endif
    uint64_t m_fileSize = 0;
    std::vector<ZipEntry> m_entries;

    bool parseEndOfCentralDir() {
#ifdef _WIN32
        // Search for End of Central Directory signature (0x06054b50)
        // It's within the last 65557 bytes
        uint64_t searchStart = (m_fileSize > 65557) ? m_fileSize - 65557 : 0;
        uint64_t searchLen = m_fileSize - searchStart;
        std::vector<uint8_t> buf(static_cast<size_t>(searchLen));

        LARGE_INTEGER pos;
        pos.QuadPart = static_cast<LONGLONG>(searchStart);
        ::SetFilePointerEx(m_hFile, pos, nullptr, FILE_BEGIN);
        DWORD read = 0;
        ::ReadFile(m_hFile, buf.data(), static_cast<DWORD>(buf.size()), &read, nullptr);

        // Find signature backwards
        for (int64_t i = static_cast<int64_t>(read) - 22; i >= 0; --i) {
            if (buf[i] == 0x50 && buf[i+1] == 0x4b && buf[i+2] == 0x05 && buf[i+3] == 0x06) {
                uint16_t numEntries = buf[i+10] | (buf[i+11] << 8);
                uint32_t cdOffset = buf[i+16] | (buf[i+17] << 8) |
                                    (buf[i+18] << 16) | (buf[i+19] << 24);
                return parseCentralDir(cdOffset, numEntries);
            }
        }
#endif
        return false;
    }

    bool parseCentralDir(uint32_t offset, uint16_t count) {
#ifdef _WIN32
        LARGE_INTEGER pos;
        pos.QuadPart = offset;
        ::SetFilePointerEx(m_hFile, pos, nullptr, FILE_BEGIN);

        for (uint16_t i = 0; i < count; ++i) {
            uint8_t hdr[46];
            DWORD read = 0;
            ::ReadFile(m_hFile, hdr, 46, &read, nullptr);
            if (read < 46) return false;
            if (hdr[0] != 0x50 || hdr[1] != 0x4b || hdr[2] != 0x01 || hdr[3] != 0x02)
                return false;

            ZipEntry entry;
            entry.compressionMethod = hdr[10] | (hdr[11] << 8);
            entry.crc32 = hdr[16] | (hdr[17] << 8) | (hdr[18] << 16) | (hdr[19] << 24);
            entry.compressedSize = hdr[20] | (hdr[21] << 8) | (hdr[22] << 16) | (hdr[23] << 24);
            entry.uncompressedSize = hdr[24] | (hdr[25] << 8) | (hdr[26] << 16) | (hdr[27] << 24);
            uint16_t nameLen = hdr[28] | (hdr[29] << 8);
            uint16_t extraLen = hdr[30] | (hdr[31] << 8);
            uint16_t commentLen = hdr[32] | (hdr[33] << 8);
            entry.localHeaderOffset = hdr[42] | (hdr[43] << 8) | (hdr[44] << 16) | (hdr[45] << 24);

            std::string name(nameLen, '\0');
            ::ReadFile(m_hFile, name.data(), nameLen, &read, nullptr);
            entry.name = name;

            // Skip extra + comment
            pos.QuadPart = 0;
            LARGE_INTEGER skip;
            skip.QuadPart = extraLen + commentLen;
            ::SetFilePointerEx(m_hFile, skip, nullptr, FILE_CURRENT);

            m_entries.push_back(std::move(entry));
        }
#endif
        return true;
    }
};

} // namespace RawrXD
