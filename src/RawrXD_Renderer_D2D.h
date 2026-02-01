#pragma once
#include "RawrXD_Win32_Foundation.h"
#include <d2d1.h>
#include <dwrite.h>

namespace RawrXD {

// Extension for Foundation Color to D2D
inline D2D1_COLOR_F ToD2D(const Color& c) { 
    return D2D1::ColorF(c.r/255.0f, c.g/255.0f, c.b/255.0f, c.a/255.0f); 
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
    
    // Copy constructors
    Font(const Font& other);
    Font& operator=(const Font& other);
    
    // Move constructors
    Font(Font&& other) noexcept;
    Font& operator=(Font&& other) noexcept;
    
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
    void drawRect(const Rect& rect, const Color& color, float strokeWidth = 1.0f);
    void fillRect(const Rect& rect, const Color& color);
    void drawLine(const Point& p1, const Point& p2, const Color& color, float strokeWidth = 1.0f);
    void drawText(const Point& p, const String& text, const Font& font, const Color& color);
    void drawStyledText(const Point& p, const String& text, const Font& font, const std::vector<TextRun>& runs, const Color& defaultColor = Color::Black);
    
    // Measurement
    SizeF measureText(const String& text, const Font& font);

    // Additional shapes
    void drawEllipse(const Point& center, float radiusX, float radiusY, const Color& color, float strokeWidth = 1.0f);
    void fillEllipse(const Point& center, float radiusX, float radiusY, const Color& color);
    void drawRoundedRect(const Rect& rect, float radiusX, float radiusY, const Color& color, float strokeWidth = 1.0f);
    void fillRoundedRect(const Rect& rect, float radiusX, float radiusY, const Color& color);

    // Transforms
    void setTransform(const D2D1::Matrix3x2F& transform);
    void resetTransform();
    void rotate(float angle, const Point& center);
    void scale(float x, float y, const Point& center);
    void translate(float x, float y);

    // Clipping
    void pushClip(const Rect& rect);
    void popClip();
    
    ID2D1HwndRenderTarget* getTarget() { return target; }
    IDWriteFactory* getWriteFactory() { return writeFactory; }
    
private:
    void createResources();
    void discardResources();
};

} // namespace RawrXD
