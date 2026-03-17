#include "brutal_gzip.h"
#include <iostream>

namespace brutal {

std::vector<uint8_t> compress(const std::vector<uint8_t>& data) {
    // Return data with a fake header or just copy
    // For now, just copy. 
    // In a real implementation this would be Deflate/Gzip
    return data; 
}

std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) {
    // Passthrough
    return data;
}

}
