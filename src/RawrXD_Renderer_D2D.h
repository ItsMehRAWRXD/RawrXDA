#pragma once
#include "RawrXD_Win32_Foundation.h"
#include "RawrXD_ColorSpace.h"
#include <d2d1.h>
#include <dwrite.h>

namespace RawrXD {

// Extension for Foundation Color to D2D with Adobe RGBa support
inline D2D1_COLOR_F ToD2D(const Color& c) { 
    // Convert to Adobe RGBa for professional color accuracy
    using namespace ColorSpace;
    AdobeRGBa color(c.r/255.0f, c.g/255.0f, c.b/255.0f, c.a/255.0f);
    auto d2d = color.ToD2D();
    return D2D1::ColorF(d2d.r, d2d.g, d2d.b, d2d.a);
}

// Direct Adobe RGBa to D2D conversion
inline D2D1_COLOR_F ToD2D(const ColorSpace::AdobeRGBa& c) {
    auto d2d = c.ToD2D();
    return D2D1::ColorF(d2d.r, d2d.g, d2d.b, d2d.a);
}

class Font {
    IDWriteTextFormat* format = nullptr;
    String family;
    float size;
    bool bold = false;
    bool italic = false;
    
public:
    Font() = default;
    Font(const String& family, float size);
    ~Font();
    
    // Copy/Move ...
    
    void setBold(bool b);
    void setItalic(bool i);
    IDWriteTextFormat* getFormat() const { return format; }
    
    // Recreate format if changed
    void updateFormat(IDWriteFactory* factory);
};

struct TextRun {
    int start;
    int length;
    Color color;
    bool bold = false;
    bool italic = false;
};

class Renderer2D {
    ID2D1Factory* factory = nullptr;
    ID2D1HwndRenderTarget* target = nullptr;
    IDWriteFactory* writeFactory = nullptr;
    ID2D1SolidColorBrush* solidBrush = nullptr;
    HWND hwnd = nullptr;
    
public:
    Renderer2D();
    ~Renderer2D();
    
    bool initialize(HWND h);
    void beginPaint();
    void endPaint();
    void resize(int w, int h);
    
    void clear(const Color& color);
    void clear(const ColorSpace::AdobeRGBa& color);
    void drawRect(const Rect& rect, const Color& color, float strokeWidth = 1.0f);
    void drawRect(const Rect& rect, const ColorSpace::AdobeRGBa& color, float strokeWidth = 1.0f);
    void fillRect(const Rect& rect, const Color& color);
    void fillRect(const Rect& rect, const ColorSpace::AdobeRGBa& color);
    void drawLine(const Point& p1, const Point& p2, const Color& color, float strokeWidth = 1.0f);
    void drawLine(const Point& p1, const Point& p2, const ColorSpace::AdobeRGBa& color, float strokeWidth = 1.0f);
    void drawText(const Point& p, const String& text, const Font& font, const Color& color);
    void drawText(const Point& p, const String& text, const Font& font, const ColorSpace::AdobeRGBa& color);
    void drawStyledText(const Point& p, const String& text, const Font& font, const std::vector<TextRun>& runs);
    
    // VSU Effect Rendering
    void drawAcrylicBackground(const Rect& rect, const ColorSpace::AdobeRGBa& baseTint, const ColorSpace::AdobeRGBa& luminosity);
    void drawMicaBackground(const Rect& rect, const ColorSpace::AdobeRGBa& tint);
    void drawElevationShadow(const Rect& rect, const ColorSpace::AdobeRGBa& shadowColor, float elevation);
    void drawRoundedRect(const Rect& rect, float radius, const ColorSpace::AdobeRGBa& color, float strokeWidth = 1.0f);
    void fillRoundedRect(const Rect& rect, float radius, const ColorSpace::AdobeRGBa& color);
    
    ID2D1HwndRenderTarget* getTarget() { return target; }
    IDWriteFactory* getWriteFactory() { return writeFactory; }
    
private:
    void createResources();
    void discardResources();
};

} // namespace RawrXD
