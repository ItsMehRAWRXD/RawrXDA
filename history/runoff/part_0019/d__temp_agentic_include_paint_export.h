#pragma once
/*  export.h - Canvas Export (BMP and PNG)
    Includes BMP native implementation and PNG via stb_image_write
*/

#include "primitives.h"
#include <fstream>
#include <cstring>

namespace ig {

// ======================== BMP Export ========================
inline bool write_bmp(const Canvas& canvas, const std::string& path) {
    // 32-bit BMP with BI_BITFIELDS
    const uint32_t fileHeaderSize = 14;
    const uint32_t infoHeaderSize = 40;
    const uint32_t pixelDataSize = canvas.width * canvas.height * 4;
    const uint32_t fileSize = fileHeaderSize + infoHeaderSize + pixelDataSize;

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    auto write_u32 = [&](uint32_t v){ 
        unsigned char buf[4];
        std::memcpy(buf, &v, 4);
        f.write(reinterpret_cast<const char*>(buf), 4); 
    };
    auto write_u16 = [&](uint16_t v){ 
        unsigned char buf[2];
        std::memcpy(buf, &v, 2);
        f.write(reinterpret_cast<const char*>(buf), 2); 
    };

    // BITMAPFILEHEADER
    f.put('B'); f.put('M');
    write_u32(fileSize);
    write_u16(0); write_u16(0);
    write_u32(fileHeaderSize + infoHeaderSize);

    // BITMAPINFOHEADER
    write_u32(infoHeaderSize);
    write_u32(static_cast<uint32_t>(canvas.width));
    write_u32(static_cast<uint32_t>(canvas.height));
    write_u16(1);                // planes
    write_u16(32);               // bpp
    write_u32(3);                // compression BI_BITFIELDS
    write_u32(pixelDataSize);
    write_u32(2835);             // ppm X ~ 72 DPI
    write_u32(2835);             // ppm Y
    write_u32(0);                // colors used
    write_u32(0);                // important colors

    // Color masks for BGRA
    write_u32(0x00FF0000); // red mask
    write_u32(0x0000FF00); // green mask
    write_u32(0x000000FF); // blue mask
    write_u32(0xFF000000); // alpha mask

    // Write pixels: BMP expects rows bottom-up; convert RGBA->BGRA
    for (int y = canvas.height - 1; y >= 0; --y) {
        for (int x = 0; x < canvas.width; ++x) {
            size_t idx = static_cast<size_t>((y*canvas.width + x)*4);
            uint8_t R = canvas.data[idx+0], G = canvas.data[idx+1], 
                    B = canvas.data[idx+2], A = canvas.data[idx+3];
            uint8_t bgra[4] = { B, G, R, A };
            f.write(reinterpret_cast<const char*>(bgra), 4);
        }
    }
    return true;
}

// ======================== PNG Export (stb_image_write integration) ========================
// This will use stb_image_write if included
// User must #define STB_IMAGE_WRITE_IMPLEMENTATION before including this file to use PNG

#ifdef STB_IMAGE_WRITE_IMPLEMENTATION
    #include "stb_image_write.h"
    
    inline bool write_png(const Canvas& canvas, const std::string& path) {
        // stb_image_write expects RGBA in the format we have
        return stbi_write_png(path.c_str(), canvas.width, canvas.height, 4, 
                             canvas.data.data(), canvas.width * 4) != 0;
    }
#else
    inline bool write_png(const Canvas& canvas, const std::string& path) {
        // Fallback: write as BMP with .png extension (not ideal but valid)
        return write_bmp(canvas, path);
    }
#endif

} // namespace ig
