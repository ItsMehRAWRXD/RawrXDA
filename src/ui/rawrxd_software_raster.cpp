#include "rawrxd/rawrxd_software_raster.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#if defined(__AVX2__)
#include <immintrin.h>
#endif

namespace rawrxd::ui
{

namespace
{

constexpr int kAtlasFirstChar = 32;
constexpr int kAtlasLastChar = 126;
constexpr int kAtlasSlotCount = kAtlasLastChar - kAtlasFirstChar + 1;

#if defined(__AVX2__)

void blitGlyphRowAvx2(std::uint32_t* destRow, const std::uint32_t* srcRow, std::uint32_t width,
                      std::uint32_t packedColor)
{
    const __m256i rgbMask = _mm256_set1_epi32(static_cast<int>(0x00FFFFFF));
    const __m256i packed = _mm256_set1_epi32(static_cast<int>(packedColor));
    const __m256i zero = _mm256_setzero_si256();

    std::uint32_t col = 0;
    for (; col + 8 <= width; col += 8)
    {
        const __m256i src = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcRow + col));
        const __m256i rgb = _mm256_and_si256(src, rgbMask);
        const __m256i isForeground =
            _mm256_xor_si256(_mm256_cmpeq_epi32(rgb, zero), _mm256_set1_epi32(static_cast<int>(0xFFFFFFFF)));
        const __m256i dest = _mm256_loadu_si256(reinterpret_cast<__m256i*>(destRow + col));
        const __m256i out = _mm256_blendv_epi8(dest, packed, isForeground);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(destRow + col), out);
    }

    for (; col < width; ++col)
    {
        if (srcRow[col] & 0x00FFFFFFu)
        {
            destRow[col] = packedColor;
        }
    }
}

void clearSurfaceRowsAvx2(std::uint32_t* bits, std::size_t pixelCount, std::uint32_t packedColor)
{
    const __m256i fill = _mm256_set1_epi32(static_cast<int>(packedColor));
    std::size_t index = 0;
    for (; index + 8 <= pixelCount; index += 8)
    {
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(bits + index), fill);
    }
    for (; index < pixelCount; ++index)
    {
        bits[index] = packedColor;
    }
}

#endif

void blitGlyphSlice(const SoftwareRenderSurface* surface, const SoftwareRasterWorkspace* raster,
                    const GlyphBitmapSlice& slice, std::int32_t targetX, std::int32_t targetY,
                    std::uint32_t packedColor)
{
    if (!surface || !surface->surfaceBits || !slice.pixelDataStart || packedColor == 0)
    {
        return;
    }

    const std::int32_t cellWidth = static_cast<std::int32_t>(raster->cellWidth);
    const bool rowFullyVisible = targetX >= 0 && (targetX + cellWidth) <= static_cast<std::int32_t>(surface->width);

    for (std::uint32_t row = 0; row < raster->cellHeight; ++row)
    {
        const std::int32_t canvasY = targetY + static_cast<std::int32_t>(row);
        if (canvasY < 0 || canvasY >= static_cast<std::int32_t>(surface->height))
        {
            continue;
        }

        std::uint32_t* destRow = surface->surfaceBits + static_cast<std::size_t>(canvasY) * surface->width + targetX;
        const std::uint32_t* srcRow = slice.pixelDataStart + static_cast<std::size_t>(row) * slice.pitchPixels;

#if defined(__AVX2__)
        if (rowFullyVisible)
        {
            blitGlyphRowAvx2(destRow, srcRow, raster->cellWidth, packedColor);
            continue;
        }
#endif

        for (std::uint32_t col = 0; col < raster->cellWidth; ++col)
        {
            const std::int32_t canvasX = targetX + static_cast<std::int32_t>(col);
            if (canvasX < 0 || canvasX >= static_cast<std::int32_t>(surface->width))
            {
                continue;
            }
            if (srcRow[col] & 0x00FFFFFFu)
            {
                destRow[col] = packedColor;
            }
        }
    }
}

std::uint32_t packedColorForByte(std::uint32_t byteIndex, const SyntaxColorRun* runs, std::uint32_t runCount,
                                 COLORREF fallback)
{
    for (std::uint32_t runIndex = 0; runIndex < runCount; ++runIndex)
    {
        const SyntaxColorRun& run = runs[runIndex];
        if (byteIndex >= run.byteOffset && byteIndex < run.byteOffset + run.length)
        {
            return packColorRef(run.color);
        }
    }
    return packColorRef(fallback);
}

}  // namespace

std::uint32_t packColorRef(COLORREF color)
{
    return 0xFF000000u | (static_cast<std::uint32_t>(GetRValue(color)) << 16) |
           (static_cast<std::uint32_t>(GetGValue(color)) << 8) | static_cast<std::uint32_t>(GetBValue(color));
}

bool softwareBlitRasterEnabled()
{
    const char* value = std::getenv("RAWRXD_SOFTWARE_BLIT_RASTER");
    return value != nullptr && value[0] == '1' && value[1] == '\0';
}

bool initializeSoftwareRaster(HDC referenceDc, HFONT fontHandle, SoftwareRasterWorkspace* raster,
                              SoftwareRenderSurface* surface, std::uint32_t surfaceWidth, std::uint32_t surfaceHeight)
{
    if (!referenceDc || !fontHandle || !raster || !surface || surfaceWidth == 0 || surfaceHeight == 0)
    {
        return false;
    }

    SIZE cellSize{};
    const HFONT oldFont = static_cast<HFONT>(SelectObject(referenceDc, fontHandle));
    if (!GetTextExtentPoint32A(referenceDc, "X", 1, &cellSize))
    {
        if (oldFont)
        {
            SelectObject(referenceDc, oldFont);
        }
        return false;
    }
    if (oldFont)
    {
        SelectObject(referenceDc, oldFont);
    }

    raster->cellWidth = static_cast<std::uint16_t>(std::max<LONG>(1, cellSize.cx));
    raster->cellHeight = static_cast<std::uint16_t>(std::max<LONG>(1, cellSize.cy));
    raster->atlasWidthPixels = raster->cellWidth * static_cast<std::uint32_t>(kAtlasSlotCount);
    raster->atlasByteSize =
        static_cast<std::size_t>(raster->atlasWidthPixels) * raster->cellHeight * sizeof(std::uint32_t);

    raster->glyphAtlasBits = static_cast<std::uint32_t*>(
        VirtualAlloc(nullptr, raster->atlasByteSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!raster->glyphAtlasBits)
    {
        return false;
    }
    std::memset(raster->glyphAtlasBits, 0, raster->atlasByteSize);

    BITMAPINFO atlasInfo{};
    atlasInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    atlasInfo.bmiHeader.biWidth = static_cast<LONG>(raster->atlasWidthPixels);
    atlasInfo.bmiHeader.biHeight = -static_cast<LONG>(raster->cellHeight);
    atlasInfo.bmiHeader.biPlanes = 1;
    atlasInfo.bmiHeader.biBitCount = 32;
    atlasInfo.bmiHeader.biCompression = BI_RGB;

    void* gdiAtlasBits = nullptr;
    HBITMAP atlasBitmap = CreateDIBSection(referenceDc, &atlasInfo, DIB_RGB_COLORS, &gdiAtlasBits, nullptr, 0);
    if (!atlasBitmap || !gdiAtlasBits)
    {
        VirtualFree(raster->glyphAtlasBits, 0, MEM_RELEASE);
        raster->glyphAtlasBits = nullptr;
        return false;
    }

    HDC atlasDc = CreateCompatibleDC(referenceDc);
    if (!atlasDc)
    {
        DeleteObject(atlasBitmap);
        VirtualFree(raster->glyphAtlasBits, 0, MEM_RELEASE);
        raster->glyphAtlasBits = nullptr;
        return false;
    }

    SelectObject(atlasDc, atlasBitmap);
    SelectObject(atlasDc, fontHandle);
    PatBlt(atlasDc, 0, 0, raster->atlasWidthPixels, raster->cellHeight, BLACKNESS);
    SetTextColor(atlasDc, RGB(255, 255, 255));
    SetBkColor(atlasDc, RGB(0, 0, 0));
    SetBkMode(atlasDc, OPAQUE);

    char glyph[2] = {' ', '\0'};
    for (int code = kAtlasFirstChar; code <= kAtlasLastChar; ++code)
    {
        glyph[0] = static_cast<char>(code);
        const int slot = code - kAtlasFirstChar;
        const int x = slot * raster->cellWidth;
        TextOutA(atlasDc, x, 0, glyph, 1);
    }

    std::memcpy(raster->glyphAtlasBits, gdiAtlasBits, raster->atlasByteSize);

    for (int code = 0; code < 128; ++code)
    {
        const int slot = std::clamp(code, kAtlasFirstChar, kAtlasLastChar) - kAtlasFirstChar;
        GlyphBitmapSlice& slice = raster->glyphSlices[code];
        slice.width = raster->cellWidth;
        slice.height = raster->cellHeight;
        slice.pitchPixels = raster->atlasWidthPixels;
        slice.pixelDataStart = raster->glyphAtlasBits + static_cast<std::size_t>(slot) * raster->cellWidth;
    }

    DeleteDC(atlasDc);
    DeleteObject(atlasBitmap);

    const std::size_t surfaceBytes = static_cast<std::size_t>(surfaceWidth) * surfaceHeight * sizeof(std::uint32_t);
    surface->surfaceBits =
        static_cast<std::uint32_t*>(VirtualAlloc(nullptr, surfaceBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!surface->surfaceBits)
    {
        VirtualFree(raster->glyphAtlasBits, 0, MEM_RELEASE);
        raster->glyphAtlasBits = nullptr;
        return false;
    }

    surface->width = surfaceWidth;
    surface->height = surfaceHeight;
    surface->bitmapInfo = {};
    surface->bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    surface->bitmapInfo.bmiHeader.biWidth = static_cast<LONG>(surfaceWidth);
    surface->bitmapInfo.bmiHeader.biHeight = -static_cast<LONG>(surfaceHeight);
    surface->bitmapInfo.bmiHeader.biPlanes = 1;
    surface->bitmapInfo.bmiHeader.biBitCount = 32;
    surface->bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* dibBits = nullptr;
    surface->dibBitmap = CreateDIBSection(referenceDc, &surface->bitmapInfo, DIB_RGB_COLORS, &dibBits, nullptr, 0);
    if (!surface->dibBitmap || !dibBits)
    {
        VirtualFree(surface->surfaceBits, 0, MEM_RELEASE);
        VirtualFree(raster->glyphAtlasBits, 0, MEM_RELEASE);
        surface->surfaceBits = nullptr;
        raster->glyphAtlasBits = nullptr;
        return false;
    }

    surface->dibMappedBits = static_cast<std::uint32_t*>(dibBits);
    std::memcpy(surface->dibMappedBits, surface->surfaceBits, surfaceBytes);

    surface->dibDc = CreateCompatibleDC(referenceDc);
    if (!surface->dibDc)
    {
        DeleteObject(surface->dibBitmap);
        VirtualFree(surface->surfaceBits, 0, MEM_RELEASE);
        VirtualFree(raster->glyphAtlasBits, 0, MEM_RELEASE);
        surface->dibBitmap = nullptr;
        surface->surfaceBits = nullptr;
        raster->glyphAtlasBits = nullptr;
        return false;
    }
    SelectObject(surface->dibDc, surface->dibBitmap);

    return true;
}

void shutdownSoftwareRaster(SoftwareRasterWorkspace* raster, SoftwareRenderSurface* surface)
{
    if (surface)
    {
        if (surface->dibDc)
        {
            DeleteDC(surface->dibDc);
            surface->dibDc = nullptr;
        }
        if (surface->dibBitmap)
        {
            DeleteObject(surface->dibBitmap);
            surface->dibBitmap = nullptr;
        }
        if (surface->surfaceBits)
        {
            VirtualFree(surface->surfaceBits, 0, MEM_RELEASE);
            surface->surfaceBits = nullptr;
        }
    }

    if (raster && raster->glyphAtlasBits)
    {
        VirtualFree(raster->glyphAtlasBits, 0, MEM_RELEASE);
        raster->glyphAtlasBits = nullptr;
    }
}

void clearSoftwareSurface(SoftwareRenderSurface* surface, COLORREF background)
{
    if (!surface || !surface->surfaceBits)
    {
        return;
    }
    RECT fullRect{0, 0, static_cast<LONG>(surface->width), static_cast<LONG>(surface->height)};
    clearSoftwareSurfaceRect(surface, background, fullRect);
}

void clearSoftwareSurfaceRect(SoftwareRenderSurface* surface, COLORREF background, const RECT& rect)
{
    if (!surface || !surface->surfaceBits)
    {
        return;
    }

    RECT clipped = rect;
    if (clipped.left < 0)
    {
        clipped.left = 0;
    }
    if (clipped.top < 0)
    {
        clipped.top = 0;
    }
    if (clipped.right > static_cast<LONG>(surface->width))
    {
        clipped.right = static_cast<LONG>(surface->width);
    }
    if (clipped.bottom > static_cast<LONG>(surface->height))
    {
        clipped.bottom = static_cast<LONG>(surface->height);
    }

    const int dirtyWidth = clipped.right - clipped.left;
    const int dirtyHeight = clipped.bottom - clipped.top;
    if (dirtyWidth <= 0 || dirtyHeight <= 0)
    {
        return;
    }

    const std::uint32_t packed = packColorRef(background);
    for (int row = clipped.top; row < clipped.bottom; ++row)
    {
        std::uint32_t* rowBits =
            surface->surfaceBits + static_cast<std::size_t>(row) * surface->width + clipped.left;
#if defined(__AVX2__)
        clearSurfaceRowsAvx2(rowBits, static_cast<std::size_t>(dirtyWidth), packed);
#else
        for (int col = 0; col < dirtyWidth; ++col)
        {
            rowBits[col] = packed;
        }
#endif
    }
}

void blitMonochromeTextSoftware(const SoftwareRenderSurface* surface, const SoftwareRasterWorkspace* raster,
                                const char* lineText, std::uint32_t length, std::int32_t targetX, std::int32_t targetY,
                                COLORREF color)
{
    if (!surface || !raster || !lineText || length == 0)
    {
        return;
    }

    const std::uint32_t packed = packColorRef(color);
    std::int32_t penX = targetX;
    for (std::uint32_t i = 0; i < length; ++i)
    {
        const unsigned char code = static_cast<unsigned char>(lineText[i]);
        const GlyphBitmapSlice& slice = raster->glyphSlices[code > 127 ? '?' : code];
        blitGlyphSlice(surface, raster, slice, penX, targetY, packed);
        penX += raster->cellWidth;
    }
}

void blitSyntaxColoredTextSoftware(const SoftwareRenderSurface* surface, const SoftwareRasterWorkspace* raster,
                                   const char* lineText, std::uint32_t length, const SyntaxColorRun* runs,
                                   std::uint32_t runCount, std::int32_t targetX, std::int32_t targetY)
{
    if (!surface || !raster || !lineText || length == 0)
    {
        return;
    }

    const std::uint32_t fallbackPacked = packColorRef(RGB(212, 212, 212));
    std::uint32_t runIndex = 0;
    std::uint32_t currentPacked = runCount > 0 ? packColorRef(runs[0].color) : fallbackPacked;

    std::int32_t penX = targetX;
    for (std::uint32_t i = 0; i < length; ++i)
    {
        while (runIndex < runCount && i >= runs[runIndex].byteOffset + runs[runIndex].length)
        {
            ++runIndex;
        }
        if (runIndex < runCount && i >= runs[runIndex].byteOffset)
        {
            currentPacked = packColorRef(runs[runIndex].color);
        }
        else
        {
            currentPacked = fallbackPacked;
        }

        const unsigned char code = static_cast<unsigned char>(lineText[i]);
        const GlyphBitmapSlice& slice = raster->glyphSlices[code > 127 ? '?' : code];
        blitGlyphSlice(surface, raster, slice, penX, targetY, currentPacked);
        penX += raster->cellWidth;
    }
}

void renderSoftwareLayoutMonochrome(const SoftwareRenderSurface* surface, const SoftwareRasterWorkspace* raster,
                                    const RenderBufferWorkspace* workspace, const TextLayoutViewport* viewport,
                                    COLORREF textColor)
{
    if (!surface || !raster || !workspace || !viewport)
    {
        return;
    }

    std::int32_t currentY = viewport->renderBounds.top - viewport->scrollOffsetY;
    const std::int32_t baseX = viewport->renderBounds.left - viewport->scrollOffsetX;

    for (std::uint32_t lineIndex = viewport->firstVisibleLine; lineIndex <= viewport->lastVisibleLine; ++lineIndex)
    {
        if (lineIndex >= workspace->poolCount)
        {
            break;
        }

        const TextLineSpan& line = workspace->visibleLinesPool[lineIndex];
        if (line.textStart && line.length > 0)
        {
            blitMonochromeTextSoftware(surface, raster, line.textStart, line.length, baseX, currentY, textColor);
        }
        currentY += raster->cellHeight;
    }
}

void renderSoftwareLayoutColored(const SoftwareRenderSurface* surface, const SoftwareRasterWorkspace* raster,
                                 const RenderBufferWorkspace* workspace, const TextLayoutViewport* viewport,
                                 const SyntaxColorRun* const* runsPerLine, const std::uint32_t* runCountPerLine)
{
    if (!surface || !raster || !workspace || !viewport)
    {
        return;
    }

    std::int32_t currentY = viewport->renderBounds.top - viewport->scrollOffsetY;
    const std::int32_t baseX = viewport->renderBounds.left - viewport->scrollOffsetX;

    for (std::uint32_t lineIndex = viewport->firstVisibleLine; lineIndex <= viewport->lastVisibleLine; ++lineIndex)
    {
        if (lineIndex >= workspace->poolCount)
        {
            break;
        }

        const TextLineSpan& line = workspace->visibleLinesPool[lineIndex];
        if (!line.textStart || line.length == 0)
        {
            currentY += raster->cellHeight;
            continue;
        }

        const SyntaxColorRun* runs = runsPerLine != nullptr ? runsPerLine[lineIndex] : nullptr;
        const std::uint32_t runCount = runCountPerLine != nullptr ? runCountPerLine[lineIndex] : 0u;

        if (runs == nullptr || runCount == 0u)
        {
            blitMonochromeTextSoftware(surface, raster, line.textStart, line.length, baseX, currentY,
                                       RGB(212, 212, 212));
        }
        else
        {
            blitSyntaxColoredTextSoftware(surface, raster, line.textStart, line.length, runs, runCount, baseX,
                                          currentY);
        }
        currentY += raster->cellHeight;
    }
}

void presentSoftwareSurface(HDC destinationDc, const SoftwareRenderSurface* surface)
{
    if (!destinationDc || !surface || !surface->surfaceBits || !surface->dibDc || !surface->dibBitmap ||
        !surface->dibMappedBits)
    {
        return;
    }

    RECT fullRect{0, 0, static_cast<LONG>(surface->width), static_cast<LONG>(surface->height)};
    presentSoftwareSurfaceRect(destinationDc, surface, fullRect);
}

void presentSoftwareSurfaceRect(HDC destinationDc, const SoftwareRenderSurface* surface, const RECT& dirtyRect)
{
    if (!destinationDc || !surface || !surface->surfaceBits || !surface->dibDc || !surface->dibBitmap ||
        !surface->dibMappedBits)
    {
        return;
    }

    RECT clipped = dirtyRect;
    if (clipped.left < 0)
    {
        clipped.left = 0;
    }
    if (clipped.top < 0)
    {
        clipped.top = 0;
    }
    if (clipped.right > static_cast<LONG>(surface->width))
    {
        clipped.right = static_cast<LONG>(surface->width);
    }
    if (clipped.bottom > static_cast<LONG>(surface->height))
    {
        clipped.bottom = static_cast<LONG>(surface->height);
    }

    const int dirtyWidth = clipped.right - clipped.left;
    const int dirtyHeight = clipped.bottom - clipped.top;
    if (dirtyWidth <= 0 || dirtyHeight <= 0)
    {
        return;
    }

    for (int row = clipped.top; row < clipped.bottom; ++row)
    {
        const std::uint32_t* srcRow =
            surface->surfaceBits + static_cast<std::size_t>(row) * surface->width + clipped.left;
        std::uint32_t* destRow = surface->dibMappedBits + static_cast<std::size_t>(row) * surface->width + clipped.left;
        std::memcpy(destRow, srcRow, static_cast<std::size_t>(dirtyWidth) * sizeof(std::uint32_t));
    }

    BitBlt(destinationDc, clipped.left, clipped.top, dirtyWidth, dirtyHeight, surface->dibDc, clipped.left, clipped.top,
           SRCCOPY);
}

}  // namespace rawrxd::ui
