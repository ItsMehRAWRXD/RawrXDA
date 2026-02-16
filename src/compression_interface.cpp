#include "compression_interface.h"
#include "codec/compression.h"

BrutalGzipWrapper::BrutalGzipWrapper() {
    thread_count_ = 1;
    is_initialized_ = true;
}

BrutalGzipWrapper::~BrutalGzipWrapper() {
    is_initialized_ = false;
}

bool BrutalGzipWrapper::Compress(const std::vector<uint8_t>& raw, std::vector<uint8_t>& compressed) {
    if (!is_initialized_) return false;
    
    bool ok = false;
    compressed = codec::deflate(raw, &ok);
    return ok;
}

bool BrutalGzipWrapper::Decompress(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& raw) {
    if (!is_initialized_) return false;
    
    bool ok = false;
    raw = codec::inflate(compressed, &ok);
    return ok;
}
