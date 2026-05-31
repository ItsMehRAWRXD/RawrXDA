// zip.h — Production ZIP archive API for VSIX extraction
// Self-contained minimal ZIP reader using zlib for inflation
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <vector>
#include <string>
#include <memory>
#include <zlib.h>

// ZIP file entry within central directory
struct ZipEntry {
    std::string name;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t compression_method;
    uint32_t local_header_offset;
    uint32_t flags;
};

// ZIP archive handle
struct zip {
    FILE* file = nullptr;
    std::vector<ZipEntry> entries;
    bool valid = false;

    zip() = default;
    ~zip() { if (file) { fclose(file); file = nullptr; } }
};

// Open file handle for reading an entry
struct zip_file {
    zip* archive = nullptr;
    size_t entry_index = 0;
    size_t read_offset = 0;
    std::vector<uint8_t> uncompressed_data;
    bool ready = false;
};

typedef int64_t zip_int64_t;
typedef zip_int64_t zip_uint64_t;

#define ZIP_RDONLY 0

// Read little-endian values from buffer
static inline uint16_t read_u16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
static inline uint32_t read_u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

// Find End of Central Directory record
static bool find_eocd(FILE* f, uint32_t& cd_offset, uint32_t& cd_size, uint16_t& num_entries) {
    if (fseek(f, 0, SEEK_END) != 0) return false;
    long file_size = ftell(f);
    if (file_size < 22) return false;

    // Search backwards for EOCD signature 0x06054b50
    long search_start = file_size - 22;
    if (search_start > 65535) search_start = file_size - 65535 - 22;
    if (search_start < 0) search_start = 0;

    if (fseek(f, search_start, SEEK_SET) != 0) return false;
    std::vector<uint8_t> buf(static_cast<size_t>(file_size - search_start));
    if (fread(buf.data(), 1, buf.size(), f) != buf.size()) return false;

    for (size_t i = buf.size(); i >= 22; --i) {
        size_t idx = i - 22;
        if (read_u32(buf.data() + idx) == 0x06054b50) {
            num_entries = read_u16(buf.data() + idx + 8);
            cd_size = read_u32(buf.data() + idx + 12);
            cd_offset = read_u32(buf.data() + idx + 16);
            return true;
        }
    }
    return false;
}

// Parse central directory
static bool parse_central_directory(zip* za, uint32_t cd_offset, uint32_t cd_size, uint16_t num_entries) {
    if (fseek(za->file, static_cast<long>(cd_offset), SEEK_SET) != 0) return false;
    std::vector<uint8_t> cd_buf(cd_size);
    if (fread(cd_buf.data(), 1, cd_size, za->file) != cd_size) return false;

    size_t pos = 0;
    for (uint16_t i = 0; i < num_entries; ++i) {
        if (pos + 46 > cd_size) return false;
        if (read_u32(cd_buf.data() + pos) != 0x02014b50) return false; // Central file header signature

        uint16_t name_len = read_u16(cd_buf.data() + pos + 28);
        uint16_t extra_len = read_u16(cd_buf.data() + pos + 30);
        uint16_t comment_len = read_u16(cd_buf.data() + pos + 32);
        uint32_t total_len = 46 + name_len + extra_len + comment_len;
        if (pos + total_len > cd_size) return false;

        ZipEntry entry;
        entry.compression_method = read_u16(cd_buf.data() + pos + 10);
        entry.crc32 = read_u32(cd_buf.data() + pos + 16);
        entry.compressed_size = read_u32(cd_buf.data() + pos + 20);
        entry.uncompressed_size = read_u32(cd_buf.data() + pos + 24);
        entry.local_header_offset = read_u32(cd_buf.data() + pos + 42);
        entry.name.assign(reinterpret_cast<char*>(cd_buf.data() + pos + 46), name_len);
        entry.flags = read_u16(cd_buf.data() + pos + 8);

        za->entries.push_back(std::move(entry));
        pos += total_len;
    }
    return true;
}

// Decompress a stored or deflated entry
static bool decompress_entry(zip* za, const ZipEntry& entry, std::vector<uint8_t>& out) {
    if (entry.compression_method == 0) {
        // Stored (no compression)
        out.resize(entry.uncompressed_size);
        if (entry.uncompressed_size == 0) return true;
        // Skip local header
        if (fseek(za->file, static_cast<long>(entry.local_header_offset) + 26, SEEK_SET) != 0) return false;
        uint16_t name_len = 0, extra_len = 0;
        if (fread(&name_len, 2, 1, za->file) != 1) return false;
        if (fread(&extra_len, 2, 1, za->file) != 1) return false;
        if (fseek(za->file, name_len + extra_len, SEEK_CUR) != 0) return false;
        if (fread(out.data(), 1, entry.uncompressed_size, za->file) != entry.uncompressed_size) return false;
        return true;
    } else if (entry.compression_method == 8) {
        // Deflated
        if (fseek(za->file, static_cast<long>(entry.local_header_offset) + 26, SEEK_SET) != 0) return false;
        uint16_t name_len = 0, extra_len = 0;
        if (fread(&name_len, 2, 1, za->file) != 1) return false;
        if (fread(&extra_len, 2, 1, za->file) != 1) return false;
        if (fseek(za->file, name_len + extra_len, SEEK_CUR) != 0) return false;

        std::vector<uint8_t> compressed(entry.compressed_size);
        if (fread(compressed.data(), 1, entry.compressed_size, za->file) != entry.compressed_size) return false;

        out.resize(entry.uncompressed_size);
        if (entry.uncompressed_size == 0) return true;

        z_stream strm = {};
        strm.next_in = compressed.data();
        strm.avail_in = static_cast<uInt>(compressed.size());
        strm.next_out = out.data();
        strm.avail_out = static_cast<uInt>(out.size());

        int ret = inflateInit2(&strm, -15); // raw deflate
        if (ret != Z_OK) return false;
        ret = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);
        return (ret == Z_STREAM_END);
    }
    return false; // Unsupported compression method
}

// zip_open — open a ZIP archive
inline zip* zip_open(const char* pathname, int flags, int* errorp) {
    (void)flags;
    if (errorp) *errorp = 0;
    if (!pathname) return nullptr;

    FILE* f = fopen(pathname, "rb");
    if (!f) {
        if (errorp) *errorp = 1;
        return nullptr;
    }

    uint32_t cd_offset = 0, cd_size = 0;
    uint16_t num_entries = 0;
    if (!find_eocd(f, cd_offset, cd_size, num_entries)) {
        fclose(f);
        if (errorp) *errorp = 2;
        return nullptr;
    }

    auto za = std::make_unique<zip>();
    za->file = f;
    if (!parse_central_directory(za.get(), cd_offset, cd_size, num_entries)) {
        if (errorp) *errorp = 3;
        return nullptr;
    }
    za->valid = true;
    return za.release();
}

// zip_close — close a ZIP archive
inline void zip_close(zip* za) {
    delete za;
}

// zip_get_num_entries — get number of entries
inline zip_int64_t zip_get_num_entries(zip* za, int flags) {
    (void)flags;
    if (!za || !za->valid) return 0;
    return static_cast<zip_int64_t>(za->entries.size());
}

// zip_get_name — get name of entry by index
inline const char* zip_get_name(zip* za, zip_uint64_t index, int flags) {
    (void)flags;
    if (!za || !za->valid || index >= za->entries.size()) return nullptr;
    return za->entries[index].name.c_str();
}

// zip_fopen_index — open entry by index for reading
inline zip_file* zip_fopen_index(zip* za, zip_uint64_t index, int flags) {
    (void)flags;
    if (!za || !za->valid || index >= za->entries.size()) return nullptr;

    auto zf = std::make_unique<zip_file>();
    zf->archive = za;
    zf->entry_index = index;
    zf->read_offset = 0;

    const ZipEntry& entry = za->entries[index];
    if (!decompress_entry(za, entry, zf->uncompressed_data)) {
        return nullptr;
    }
    zf->ready = true;
    return zf.release();
}

// zip_fread — read from opened entry
inline zip_int64_t zip_fread(zip_file* zf, void* buf, zip_uint64_t len) {
    if (!zf || !zf->ready || !buf) return -1;
    size_t remaining = zf->uncompressed_data.size() - zf->read_offset;
    if (remaining == 0) return 0;
    size_t to_read = (len < remaining) ? static_cast<size_t>(len) : remaining;
    std::memcpy(buf, zf->uncompressed_data.data() + zf->read_offset, to_read);
    zf->read_offset += to_read;
    return static_cast<zip_int64_t>(to_read);
}

// zip_fclose — close opened entry
inline void zip_fclose(zip_file* zf) {
    delete zf;
}

