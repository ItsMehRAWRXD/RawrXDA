// Vision Encoder — image file reading + base64 encoding for multimodal AI
// Production implementation: reads image file, produces base64 data URI

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <algorithm>

namespace RawrXD {
namespace Vision {

class VisionEncoder {
public:
    static VisionEncoder& instance() {
        static VisionEncoder inst;
        return inst;
    }

    std::string encodeImage(const std::string& imagePath) {
        // Read raw image bytes
        std::ifstream file(imagePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return "";

        std::streamsize size = file.tellg();
        if (size <= 0 || size > 100 * 1024 * 1024) return ""; // 100MB limit
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> data(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(data.data()), size)) return "";

        // Detect MIME type from magic bytes
        std::string mime = detectMime(data);

        // Base64 encode
        std::string b64 = base64Encode(data.data(), data.size());

        // Return data URI for multimodal model consumption
        return "data:" + mime + ";base64," + b64;
    }

    std::string getImageDimensions(const std::string& imagePath) {
        std::ifstream file(imagePath, std::ios::binary);
        if (!file.is_open()) return "";

        uint8_t header[32];
        file.read(reinterpret_cast<char*>(header), 32);
        size_t read = static_cast<size_t>(file.gcount());

        // PNG: width at offset 16, height at offset 20 (big-endian uint32)
        if (read >= 24 && header[0] == 0x89 && header[1] == 'P') {
            uint32_t w = (header[16] << 24) | (header[17] << 16) | (header[18] << 8) | header[19];
            uint32_t h = (header[20] << 24) | (header[21] << 16) | (header[22] << 8) | header[23];
            return std::to_string(w) + "x" + std::to_string(h);
        }
        // BMP: width at offset 18, height at offset 22 (little-endian int32)
        if (read >= 26 && header[0] == 'B' && header[1] == 'M') {
            int32_t w = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
            int32_t h = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
            if (h < 0) h = -h;
            return std::to_string(w) + "x" + std::to_string(h);
        }
        return "unknown";
    }

private:
    static std::string detectMime(const std::vector<uint8_t>& data) {
        if (data.size() >= 8 && data[0] == 0x89 && data[1] == 'P' &&
            data[2] == 'N' && data[3] == 'G') return "image/png";
        if (data.size() >= 3 && data[0] == 0xFF && data[1] == 0xD8 &&
            data[2] == 0xFF) return "image/jpeg";
        if (data.size() >= 6 && data[0] == 'G' && data[1] == 'I' &&
            data[2] == 'F') return "image/gif";
        if (data.size() >= 4 && data[0] == 'R' && data[1] == 'I' &&
            data[2] == 'F' && data[3] == 'F') return "image/webp";
        if (data.size() >= 2 && data[0] == 'B' && data[1] == 'M') return "image/bmp";
        return "application/octet-stream";
    }

    static std::string base64Encode(const uint8_t* data, size_t len) {
        static const char table[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((len + 2) / 3) * 4);
        for (size_t i = 0; i < len; i += 3) {
            uint32_t n = static_cast<uint32_t>(data[i]) << 16;
            if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
            if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);
            out += table[(n >> 18) & 0x3F];
            out += table[(n >> 12) & 0x3F];
            out += (i + 1 < len) ? table[(n >> 6) & 0x3F] : '=';
            out += (i + 2 < len) ? table[n & 0x3F] : '=';
        }
        return out;
    }
};

} // namespace Vision
} // namespace RawrXD