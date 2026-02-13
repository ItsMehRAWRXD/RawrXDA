// gzip_masm_store.cpp — Minimal gzip using a single DEFLATE stored block (BTYPE=00)
// Stage 0: No compression (baseline plumbing); pure C++ so we can bench immediately.
// Exports a C ABI to allocate a gzip buffer: gzip_masm_alloc(src,len,&out_len)

#include <cstdint>
#include <cstdlib>
#include <vector>
#include <chrono>

extern "C" {
    void* gzip_masm_alloc(const void* src, size_t len, size_t* out_len);
}

static uint32_t crc32_table[256];
static bool crc32_init_done = false;

static void crc32_init() {
    if (crc32_init_done) return;
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_init_done = true;
}

static uint32_t crc32_compute(const uint8_t* data, size_t len) {
    crc32_init();
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        c = crc32_table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

// Build a single stored block DEFLATE stream: [0x01][LEN(2)][NLEN(2)][payload]
static std::vector<uint8_t> deflate_store_block(const uint8_t* src, size_t len) {
    // 1 byte header + 4 bytes LEN/NLEN + payload
    std::vector<uint8_t> out;
    out.reserve(1 + 4 + len);
    // First byte: BFINAL=1 (bit0), BTYPE=00 (bits1-2). LSB-first emission => byte 0x01 works.
    out.push_back(0x01);
    // Split into 64KB chunks because stored block LEN is 16-bit
    size_t offset = 0;
    while (offset < len) {
        size_t chunk = len - offset;
        if (chunk > 65535) chunk = 65535;
        // For the first chunk, we already emitted the 0x01. For subsequent chunks, BFINAL=0 => 0x00
        if (offset > 0) out.push_back((offset + chunk < len) ? 0x00 : 0x01);
        uint16_t L = static_cast<uint16_t>(chunk);
        uint16_t NL = static_cast<uint16_t>(~L);
        out.push_back(static_cast<uint8_t>(L & 0xFF));
        out.push_back(static_cast<uint8_t>((L >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(NL & 0xFF));
        out.push_back(static_cast<uint8_t>((NL >> 8) & 0xFF));
        out.insert(out.end(), src + offset, src + offset + chunk);
        offset += chunk;
    }
    return out;
}

void* gzip_masm_alloc(const void* vsrc, size_t len, size_t* out_len) {
    const uint8_t* src = static_cast<const uint8_t*>(vsrc);
    // Build DEFLATE stream (stored block)
    std::vector<uint8_t> deflate = deflate_store_block(src, len);
    // Compute CRC32 of uncompressed data
    uint32_t crc = crc32_compute(src, len);
    // Gzip header (10 bytes): ID1 ID2 CM FLG MTIME[4] XFL OS
    // We'll set FLG=0, MTIME=0, XFL=0, OS=3 (Unix)
    const size_t headerSize = 10;
    const size_t footerSize = 8; // CRC32 + ISIZE
    size_t total = headerSize + deflate.size() + footerSize;
    uint8_t* out = static_cast<uint8_t*>(std::malloc(total));
    if (!out) return nullptr;

    uint8_t* p = out;
    *p++ = 0x1F; // ID1
    *p++ = 0x8B; // ID2
    *p++ = 0x08; // CM=8 (DEFLATE)
    *p++ = 0x00; // FLG
    *p++ = 0x00; // MTIME
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00; // XFL
    *p++ = 0x03; // OS=Unix

    // Deflate payload
    std::memcpy(p, deflate.data(), deflate.size());
    p += deflate.size();

    // Footer: CRC32 (LE) and ISIZE (LE, modulo 2^32)
    *p++ = static_cast<uint8_t>(crc & 0xFF);
    *p++ = static_cast<uint8_t>((crc >> 8) & 0xFF);
    *p++ = static_cast<uint8_t>((crc >> 16) & 0xFF);
    *p++ = static_cast<uint8_t>((crc >> 24) & 0xFF);
    uint32_t isize = static_cast<uint32_t>(len & 0xFFFFFFFFu);
    *p++ = static_cast<uint8_t>(isize & 0xFF);
    *p++ = static_cast<uint8_t>((isize >> 8) & 0xFF);
    *p++ = static_cast<uint8_t>((isize >> 16) & 0xFF);
    *p++ = static_cast<uint8_t>((isize >> 24) & 0xFF);

    if (out_len) *out_len = total;
    return out;
}
