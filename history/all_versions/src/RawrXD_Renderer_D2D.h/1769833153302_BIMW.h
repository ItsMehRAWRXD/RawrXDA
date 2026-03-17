#pragma once
#include "RawrXD_Win32_Foundation.h"
#include <d2d1.h>
#include <dwrite.h>

namespace RawrXD {

struct Color {
    float r, g, b, a;
    Color() : r(0), g(0), b(0), a(1.0f) {} // Default constructor
    Color(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}
    Color(int r_, int g_, int b_, int a_ = 255) 
        : r(r_/255.0f), g(g_/255.0f), b(b_/255.0f), a(a_/255.0f) {}
    
    D2D1_COLOR_F toD2D() const { return D2D1::ColorF(r, g, b, a); }
    
    static const Color Black;
    static const Color White;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Transparent;
};

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
    void drawRect(const Rect& rect, const Color& color, float strokeWidth = 1.0f);
    void fillRect(const Rect& rect, const Color& color);
    void drawLine(const Point& p1, const Point& p2, const Color& color, float strokeWidth = 1.0f);
    void drawText(const Point& p, const String& text, const Font& font, const Color& color);
    void drawStyledText(const Point& p, const String& text, const Font& font, const std::vector<TextRun>& runs);
    
    ID2D1HwndRenderTarget* getTarget() { return target; }
    IDWriteFactory* getWriteFactory() { return writeFactory; }
    
private:
    void createResources();
    void discardResources();
};

} // namespace RawrXD
