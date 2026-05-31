#pragma once

#include "rawrxd/rawrxd_text_engine.h"

#include <cstddef>
#include <cstdint>
#include <windows.h>

namespace rawrxd::ui
{

struct GlyphBitmapSlice
{
    const std::uint32_t* pixelDataStart = nullptr;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint32_t pitchPixels = 0;
};

struct SoftwareRenderSurface
{
    std::uint32_t* surfaceBits = nullptr;
    std::uint32_t* dibMappedBits = nullptr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    BITMAPINFO bitmapInfo{};
    HBITMAP dibBitmap = nullptr;
    HDC dibDc = nullptr;
};

struct SoftwareRasterWorkspace
{
    std::uint32_t* glyphAtlasBits = nullptr;
    GlyphBitmapSlice glyphSlices[128]{};
    std::uint16_t cellWidth = 0;
    std::uint16_t cellHeight = 0;
    std::uint32_t atlasWidthPixels = 0;
    std::size_t atlasByteSize = 0;
};

std::uint32_t packColorRef(COLORREF color);

bool initializeSoftwareRaster(HDC referenceDc, HFONT fontHandle, SoftwareRasterWorkspace* raster,
                              SoftwareRenderSurface* surface, std::uint32_t surfaceWidth, std::uint32_t surfaceHeight);

void shutdownSoftwareRaster(SoftwareRasterWorkspace* raster, SoftwareRenderSurface* surface);

void clearSoftwareSurface(SoftwareRenderSurface* surface, COLORREF background);

void clearSoftwareSurfaceRect(SoftwareRenderSurface* surface, COLORREF background, const RECT& rect);

void blitMonochromeTextSoftware(const SoftwareRenderSurface* surface, const SoftwareRasterWorkspace* raster,
                                const char* lineText, std::uint32_t length, std::int32_t targetX, std::int32_t targetY,
                                COLORREF color);

void blitSyntaxColoredTextSoftware(const SoftwareRenderSurface* surface, const SoftwareRasterWorkspace* raster,
                                   const char* lineText, std::uint32_t length, const SyntaxColorRun* runs,
                                   std::uint32_t runCount, std::int32_t targetX, std::int32_t targetY);

void renderSoftwareLayoutMonochrome(const SoftwareRenderSurface* surface, const SoftwareRasterWorkspace* raster,
                                    const RenderBufferWorkspace* workspace, const TextLayoutViewport* viewport,
                                    COLORREF textColor);

void renderSoftwareLayoutColored(const SoftwareRenderSurface* surface, const SoftwareRasterWorkspace* raster,
                                 const RenderBufferWorkspace* workspace, const TextLayoutViewport* viewport,
                                 const SyntaxColorRun* const* runsPerLine, const std::uint32_t* runCountPerLine);

void presentSoftwareSurface(HDC destinationDc, const SoftwareRenderSurface* surface);

void presentSoftwareSurfaceRect(HDC destinationDc, const SoftwareRenderSurface* surface, const RECT& dirtyRect);

bool softwareBlitRasterEnabled();

}  // namespace rawrxd::ui
