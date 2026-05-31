#include "RawrXD_Renderer_D2D.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace RawrXD {

const Color Color::Black(0, 0, 0);
const Color Color::White(255, 255, 255);
const Color Color::Red(255, 0, 0);
const Color Color::Green(0, 255, 0);
const Color Color::Blue(0, 0, 255);
const Color Color::Transparent(0, 0, 0, 0);

// --- Font ---

Font::Font(const String& f, float s) : family(f), size(s) {}

Font::~Font() {
    if (format) format->Release();
}

void Font::setBold(bool b) { bold = b; if (format) { format->Release(); format = nullptr; } }
void Font::setItalic(bool i) { italic = i; if (format) { format->Release(); format = nullptr; } }

void Font::updateFormat(IDWriteFactory* factory) {
    if (format) return;
    if (!factory) return;
    
    factory->CreateTextFormat(
        family.c_str(),
        nullptr,
        bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
        italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        size,
        L"en-us",
        &format
    );
}

// --- Renderer2D ---

Renderer2D::Renderer2D() {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&writeFactory));
}

Renderer2D::~Renderer2D() {
    discardResources();
    if (writeFactory) writeFactory->Release();
    if (factory) factory->Release();
}

bool Renderer2D::initialize(HWND h) {
    hwnd = h;
    return true; // Resources created on demand or here?
}

void Renderer2D::createResources() {
    if (!target && hwnd) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
        
        factory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd, size),
            &target
        );
        
        if (target) {
            target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &solidBrush);
        }
    }
}

void Renderer2D::discardResources() {
    if (solidBrush) { solidBrush->Release(); solidBrush = nullptr; }
    if (target) { target->Release(); target = nullptr; }
}

void Renderer2D::beginPaint() {
    createResources();
    if (target) {
        target->BeginDraw();
        target->SetTransform(D2D1::Matrix3x2F::Identity());
    }
}

void Renderer2D::endPaint() {
    if (target) {
        HRESULT hr = target->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            discardResources();
        }
    }
}

void Renderer2D::resize(int w, int h) {
    if (target) {
        target->Resize(D2D1::SizeU(w, h));
    }
}

void Renderer2D::clear(const Color& color) {
    if (target) target->Clear(color.toD2D());
}

void Renderer2D::clear(const ColorSpace::AdobeRGBa& color) {
    if (target) {
        auto d2d = color.ToD2D();
        target->Clear(D2D1::ColorF(d2d.r, d2d.g, d2d.b, d2d.a));
    }
}

void Renderer2D::drawRect(const Rect& rect, const Color& color, float strokeWidth) {
    if (target && solidBrush) {
        solidBrush->SetColor(color.toD2D());
        D2D1_RECT_F r = D2D1::RectF((float)rect.x, (float)rect.y, (float)rect.x + rect.width, (float)rect.y + rect.height);
        target->DrawRectangle(r, solidBrush, strokeWidth);
    }
}

void Renderer2D::drawRect(const Rect& rect, const ColorSpace::AdobeRGBa& color, float strokeWidth) {
    if (target && solidBrush) {
        auto d2d = color.ToD2D();
        solidBrush->SetColor(D2D1::ColorF(d2d.r, d2d.g, d2d.b, d2d.a));
        D2D1_RECT_F r = D2D1::RectF((float)rect.x, (float)rect.y, (float)rect.x + rect.width, (float)rect.y + rect.height);
        target->DrawRectangle(r, solidBrush, strokeWidth);
    }
}

void Renderer2D::fillRect(const Rect& rect, const Color& color) {
    if (target && solidBrush) {
        solidBrush->SetColor(color.toD2D());
        D2D1_RECT_F r = D2D1::RectF((float)rect.x, (float)rect.y, (float)rect.x + rect.width, (float)rect.y + rect.height);
        target->FillRectangle(r, solidBrush);
    }
}

void Renderer2D::fillRect(const Rect& rect, const ColorSpace::AdobeRGBa& color) {
    if (target && solidBrush) {
        auto d2d = color.ToD2D();
        solidBrush->SetColor(D2D1::ColorF(d2d.r, d2d.g, d2d.b, d2d.a));
        D2D1_RECT_F r = D2D1::RectF((float)rect.x, (float)rect.y, (float)rect.x + rect.width, (float)rect.y + rect.height);
        target->FillRectangle(r, solidBrush);
    }
}

void Renderer2D::drawLine(const Point& p1, const Point& p2, const Color& color, float strokeWidth) {
    if (target && solidBrush) {
        solidBrush->SetColor(color.toD2D());
        target->DrawLine(D2D1::Point2F((float)p1.x, (float)p1.y), D2D1::Point2F((float)p2.x, (float)p2.y), solidBrush, strokeWidth);
    }
}

void Renderer2D::drawLine(const Point& p1, const Point& p2, const ColorSpace::AdobeRGBa& color, float strokeWidth) {
    if (target && solidBrush) {
        auto d2d = color.ToD2D();
        solidBrush->SetColor(D2D1::ColorF(d2d.r, d2d.g, d2d.b, d2d.a));
        target->DrawLine(D2D1::Point2F((float)p1.x, (float)p1.y), D2D1::Point2F((float)p2.x, (float)p2.y), solidBrush, strokeWidth);
    }
}

void Renderer2D::drawText(const Point& p, const String& text, const Font& font, const Color& color) {
    if (target && solidBrush && writeFactory) {
        // Const cast trick because font is const reference but we need to update it
        const_cast<Font&>(font).updateFormat(writeFactory);
        
        if (font.getFormat()) {
            solidBrush->SetColor(color.toD2D());
            D2D1_RECT_F layoutRect = D2D1::RectF((float)p.x, (float)p.y, 10000.0f, 10000.0f); // Massive rect
            target->DrawText(text.c_str(), text.length(), font.getFormat(), layoutRect, solidBrush);
        }
    }
}

void Renderer2D::drawText(const Point& p, const String& text, const Font& font, const ColorSpace::AdobeRGBa& color) {
    if (target && solidBrush && writeFactory) {
        const_cast<Font&>(font).updateFormat(writeFactory);
        
        if (font.getFormat()) {
            auto d2d = color.ToD2D();
            solidBrush->SetColor(D2D1::ColorF(d2d.r, d2d.g, d2d.b, d2d.a));
            D2D1_RECT_F layoutRect = D2D1::RectF((float)p.x, (float)p.y, 10000.0f, 10000.0f);
            target->DrawText(text.c_str(), text.length(), font.getFormat(), layoutRect, solidBrush);
        }
    }
}

void Renderer2D::drawStyledText(const Point& p, const String& text, const Font& font, const std::vector<TextRun>& runs) {
    if (!target || !writeFactory) return;

    // Ensure base font format is ready
    const_cast<Font&>(font).updateFormat(writeFactory);
    IDWriteTextFormat* fmt = font.getFormat();
    if (!fmt) return;

    IDWriteTextLayout* layout = nullptr;
    HRESULT hr = writeFactory->CreateTextLayout(
        text.c_str(), 
        (UINT32)text.length(), 
        fmt, 
        10000.0f, // Max width
        10000.0f, // Max height
        &layout
    );

    if (FAILED(hr) || !layout) return;

    // Apply runs
    for (const auto& run : runs) {
        DWRITE_TEXT_RANGE range = { (UINT32)run.start, (UINT32)run.length };
        
        // Bounds check just in case
        if (range.startPosition + range.length > text.length()) continue;

        if (run.bold) {
            layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range);
        }
        if (run.italic) {
            layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range);
        }

        // Color brush
        ID2D1SolidColorBrush* runBrush = nullptr;
        target->CreateSolidColorBrush(run.color.toD2D(), &runBrush);
        if (runBrush) {
            // SetDrawingEffect works with D2D1RenderTarget::DrawTextLayout
            layout->SetDrawingEffect(runBrush, range);
            runBrush->Release(); // Layout holds a reference
        }
    }

    // Default brush for unstyled text?
    // We pass solidBrush to DrawTextLayout as default
    solidBrush->SetColor(Color::Black.toD2D()); // Or some default

    target->DrawTextLayout(
        D2D1::Point2F((float)p.x, (float)p.y),
        layout,
        solidBrush,
        D2D1_DRAW_TEXT_OPTIONS_NONE
    );

    layout->Release();
}

// ============================================================================
// VSU Effect Rendering Implementation
// ============================================================================

void Renderer2D::drawAcrylicBackground(const Rect& rect, const ColorSpace::AdobeRGBa& baseTint, const ColorSpace::AdobeRGBa& luminosity) {
    if (!target || !solidBrush) return;
    
    // Base layer
    auto baseD2d = baseTint.ToD2D();
    solidBrush->SetColor(D2D1::ColorF(baseD2d.r, baseD2d.g, baseD2d.b, baseD2d.a));
    D2D1_RECT_F r = D2D1::RectF((float)rect.x, (float)rect.y, (float)rect.x + rect.width, (float)rect.y + rect.height);
    target->FillRectangle(r, solidBrush);
    
    // Luminosity layer (simulated with semi-transparent overlay)
    auto lumD2d = luminosity.ToD2D();
    solidBrush->SetColor(D2D1::ColorF(lumD2d.r, lumD2d.g, lumD2d.b, lumD2d.a * 0.6f));  // 60% intensity
    target->FillRectangle(r, solidBrush);
}

void Renderer2D::drawMicaBackground(const Rect& rect, const ColorSpace::AdobeRGBa& tint) {
    if (!target || !solidBrush) return;
    
    auto d2d = tint.ToD2D();
    solidBrush->SetColor(D2D1::ColorF(d2d.r, d2d.g, d2d.b, d2d.a));
    D2D1_RECT_F r = D2D1::RectF((float)rect.x, (float)rect.y, (float)rect.x + rect.width, (float)rect.y + rect.height);
    target->FillRectangle(r, solidBrush);
}

void Renderer2D::drawElevationShadow(const Rect& rect, const ColorSpace::AdobeRGBa& shadowColor, float elevation) {
    if (!target || !solidBrush) return;
    
    // Calculate shadow opacity based on elevation
    float opacity = std::min(0.32f, 0.04f + (elevation / 32.0f) * 0.28f);
    auto d2d = shadowColor.ToD2D();
    
    // Draw shadow with calculated opacity
    solidBrush->SetColor(D2D1::ColorF(d2d.r, d2d.g, d2d.b, opacity));
    D2D1_RECT_F r = D2D1::RectF((float)rect.x, (float)rect.y, (float)rect.x + rect.width, (float)rect.y + rect.height);
    target->FillRectangle(r, solidBrush);
}

void Renderer2D::drawRoundedRect(const Rect& rect, float radius, const ColorSpace::AdobeRGBa& color, float strokeWidth) {
    if (!target || !solidBrush) return;
    
    ID2D1RoundedRectangleGeometry* geometry = nullptr;
    ID2D1Factory* d2dFactory = nullptr;
    target->GetFactory(&d2dFactory);
    
    if (d2dFactory) {
        D2D1_ROUNDED_RECT roundedRect = {
            D2D1::RectF((float)rect.x, (float)rect.y, (float)rect.x + rect.width, (float)rect.y + rect.height),
            radius, radius
        };
        
        d2dFactory->CreateRoundedRectangleGeometry(roundedRect, &geometry);
        if (geometry) {
            auto d2d = color.ToD2D();
            solidBrush->SetColor(D2D1::ColorF(d2d.r, d2d.g, d2d.b, d2d.a));
            target->DrawGeometry(geometry, solidBrush, strokeWidth);
            geometry->Release();
        }
        d2dFactory->Release();
    }
}

void Renderer2D::fillRoundedRect(const Rect& rect, float radius, const ColorSpace::AdobeRGBa& color) {
    if (!target || !solidBrush) return;
    
    ID2D1RoundedRectangleGeometry* geometry = nullptr;
    ID2D1Factory* d2dFactory = nullptr;
    target->GetFactory(&d2dFactory);
    
    if (d2dFactory) {
        D2D1_ROUNDED_RECT roundedRect = {
            D2D1::RectF((float)rect.x, (float)rect.y, (float)rect.x + rect.width, (float)rect.y + rect.height),
            radius, radius
        };
        
        d2dFactory->CreateRoundedRectangleGeometry(roundedRect, &geometry);
        if (geometry) {
            auto d2d = color.ToD2D();
            solidBrush->SetColor(D2D1::ColorF(d2d.r, d2d.g, d2d.b, d2d.a));
            target->FillGeometry(geometry, solidBrush);
            geometry->Release();
        }
        d2dFactory->Release();
    }
}

} // namespace RawrXD
