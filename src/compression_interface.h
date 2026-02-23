#pragma once
#include <vector>
#include <cstdint>

class BrutalGzipWrapper {
public:
    BrutalGzipWrapper();
    ~BrutalGzipWrapper();
    
    bool Compress(const std::vector<uint8_t>& raw, std::vector<uint8_t>& compressed);
    bool Decompress(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& raw);

private:
    int thread_count_;
    bool is_initialized_;
};
