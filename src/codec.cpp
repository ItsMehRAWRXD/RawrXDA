#include "../include/codec.h"

#include <zlib.h>
#include <vector>
#include <cstdlib>
#include <cstring>

namespace codec {

std::vector<uint8_t> deflate(const std::vector<uint8_t>& input, bool* success) {
    if (success) {
        *success = false;
    }

    if (input.isEmpty()) {
        if (success) {
            *success = true;
        }
        return std::vector<uint8_t>();
    }

    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.constData()));
    stream.avail_in = static_cast<uInt>(input.size());

    int init_result = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    if (init_result != Z_OK) {
        return std::vector<uint8_t>();
    }

    uLong bound = compressBound(static_cast<uLong>(input.size()));
    std::vector<unsigned char> output(bound);

    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());

    int deflate_result = deflate(&stream, Z_FINISH);
    if (deflate_result != Z_STREAM_END) {
        deflateEnd(&stream);
        return std::vector<uint8_t>();
    }

    size_t out_len = stream.total_out;
    deflateEnd(&stream);

    if (success) {
        *success = true;
    }

    return std::vector<uint8_t>(reinterpret_cast<const char*>(output.data()), static_cast<int>(out_len));
}

std::vector<uint8_t> inflate(const std::vector<uint8_t>& input, bool* success) {
    if (success) {
        *success = false;
    }

    if (input.isEmpty()) {
        if (success) {
            *success = true;
        }
        return std::vector<uint8_t>();
    }

    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.constData()));
    stream.avail_in = static_cast<uInt>(input.size());

    int init_result = inflateInit2(&stream, -MAX_WBITS);
    if (init_result != Z_OK) {
        return std::vector<uint8_t>();
    }

    size_t initial_size = std::max<size_t>(256, static_cast<size_t>(input.size()) * 4);
    std::vector<unsigned char> output(initial_size);

    int inflate_result = Z_OK;
    while (inflate_result == Z_OK) {
        if (stream.total_out >= output.size()) {
            output.resize(output.size() * 2);
        }

        stream.next_out = output.data() + stream.total_out;
        stream.avail_out = static_cast<uInt>(output.size() - stream.total_out);

        inflate_result = inflate(&stream, Z_NO_FLUSH);
    }

    if (inflate_result != Z_STREAM_END) {
        inflateEnd(&stream);
        return std::vector<uint8_t>();
    }

    size_t out_len = stream.total_out;
    inflateEnd(&stream);

    if (success) {
        *success = true;
    }

    return std::vector<uint8_t>(reinterpret_cast<const char*>(output.data()), static_cast<int>(out_len));
}

}  // namespace codec

extern "C" {
void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len) {
    if (out_len) {
        *out_len = 0;
    }
    if (!src || len == 0) {
        return nullptr;
    }

    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<void*>(src));
    stream.avail_in = static_cast<uInt>(len);

    int init_result = deflateInit2(&stream, Z_BEST_SPEED, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
    if (init_result != Z_OK) {
        return nullptr;
    }

    uLong bound = compressBound(static_cast<uLong>(len)) + 18;
    std::vector<unsigned char> output(bound);

    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());

    int deflate_result = deflate(&stream, Z_FINISH);
    if (deflate_result != Z_STREAM_END) {
        deflateEnd(&stream);
        return nullptr;
    }

    size_t produced = stream.total_out;
    deflateEnd(&stream);

    void* buffer = std::malloc(produced);
    if (!buffer) {
        return nullptr;
    }

    std::memcpy(buffer, output.data(), produced);
    if (out_len) {
        *out_len = produced;
    }
    return buffer;
}

void* deflate_brutal_neon(const void* src, size_t len, size_t* out_len) {
    return deflate_brutal_masm(src, len, out_len);
}
}

