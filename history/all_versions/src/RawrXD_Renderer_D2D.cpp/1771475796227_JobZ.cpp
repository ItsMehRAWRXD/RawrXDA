#include "RawrXD_Renderer_D2D.h"
#include <stdexcept>
#include <iostream>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace RawrXD {

// ═════════════════════════════════════════════════════════════════════════════
// ColorDrawingEffect — IUnknown wrapper carrying a D2D1_COLOR_F per text range
// Used as the drawing effect passed to IDWriteTextLayout::SetDrawingEffect
// ═════════════════════════════════════════════════════════════════════════════

class ColorDrawingEffect : public IUnknown {
    ULONG m_refCount;
public:
    D2D1_COLOR_F color;

    ColorDrawingEffect(D2D1_COLOR_F c) : m_refCount(1), color(c) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown)) {
            *ppv = static_cast<IUnknown*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = --m_refCount;
        if (r == 0) delete this;
        return r;
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// CustomTextRenderer — IDWriteTextRenderer that respects per-range color effects
// ═════════════════════════════════════════════════════════════════════════════

class CustomTextRenderer : public IDWriteTextRenderer {
    ULONG              m_refCount;
    ID2D1RenderTarget* m_target;
    ID2D1SolidColorBrush* m_brush; // shared brush, color changed per glyph run
    D2D1_COLOR_F       m_defaultColor;

public:
    CustomTextRenderer(ID2D1RenderTarget* target, ID2D1SolidColorBrush* brush, D2D1_COLOR_F defaultColor)
        : m_refCount(1), m_target(target), m_brush(brush), m_defaultColor(defaultColor) {}

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDWritePixelSnapping) || riid == __uuidof(IDWriteTextRenderer)) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = --m_refCount;
        if (r == 0) delete this;
        return r;
    }

    // IDWritePixelSnapping
    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* isDisabled) override {
        *isDisabled = FALSE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* transform) override {
        m_target->GetTransform(reinterpret_cast<D2D1_MATRIX_3X2_F*>(transform));
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* ppd) override {
        *ppd = 1.0f;
        return S_OK;
    }

    // IDWriteTextRenderer
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(
        void* clientDrawingContext,
        FLOAT baselineOriginX, FLOAT baselineOriginY,
        DWRITE_MEASURING_MODE measuringMode,
        const DWRITE_GLYPH_RUN* glyphRun,
        const DWRITE_GLYPH_RUN_DESCRIPTION* glyphRunDescription,
        IUnknown* clientDrawingEffect) override
    {
        // Extract color from the drawing effect if present
        D2D1_COLOR_F color = m_defaultColor;
        if (clientDrawingEffect) {
            ColorDrawingEffect* effect = static_cast<ColorDrawingEffect*>(clientDrawingEffect);
            color = effect->color;
        }

        m_brush->SetColor(color);
        m_target->DrawGlyphRun(
            D2D1::Point2F(baselineOriginX, baselineOriginY),
            glyphRun, m_brush, measuringMode);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawUnderline(
        void*, FLOAT baselineOriginX, FLOAT baselineOriginY,
        const DWRITE_UNDERLINE* underline, IUnknown* clientDrawingEffect) override
    {
        D2D1_COLOR_F color = m_defaultColor;
        if (clientDrawingEffect) {
            color = static_cast<ColorDrawingEffect*>(clientDrawingEffect)->color;
        }
        m_brush->SetColor(color);
        D2D1_RECT_F rect = D2D1::RectF(
            baselineOriginX,
            baselineOriginY + underline->offset,
            baselineOriginX + underline->width,
            baselineOriginY + underline->offset + underline->thickness);
        m_target->FillRectangle(&rect, m_brush);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawStrikethrough(
        void*, FLOAT baselineOriginX, FLOAT baselineOriginY,
        const DWRITE_STRIKETHROUGH* strikethrough, IUnknown* clientDrawingEffect) override
    {
        D2D1_COLOR_F color = m_defaultColor;
        if (clientDrawingEffect) {
            color = static_cast<ColorDrawingEffect*>(clientDrawingEffect)->color;
        }
        m_brush->SetColor(color);
        D2D1_RECT_F rect = D2D1::RectF(
            baselineOriginX,
            baselineOriginY + strikethrough->offset,
            baselineOriginX + strikethrough->width,
            baselineOriginY + strikethrough->offset + strikethrough->thickness);
        m_target->FillRectangle(&rect, m_brush);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DrawInlineObject(
        void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) override
    {
        return E_NOTIMPL;
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// Font Implementation
// ═════════════════════════════════════════════════════════════════════════════

Font::Font(const String& family, float size) 
    : family(family), size(size) {
}

Font::~Font() {
    if (format) format->Release();
}

Font::Font(const Font& other) 
    : family(other.family), size(other.size), bold(other.bold), italic(other.italic) {
    if (other.format) {
        other.format->AddRef();
        format = other.format;
    }
}

Font& Font::operator=(const Font& other) {
    if (this != &other) {
        if (format) format->Release();
        family = other.family;
        size = other.size;
        bold = other.bold;
        italic = other.italic;
        format = other.format;
        if (format) format->AddRef();
    }
    return *this;
}

Font::Font(Font&& other) noexcept 
    : format(other.format), family(std::move(other.family)), size(other.size), bold(other.bold), italic(other.italic) {
    other.format = nullptr;
}

Font& Font::operator=(Font&& other) noexcept {
    if (this != &other) {
        if (format) format->Release();
        format = other.format;
        other.format = nullptr;
        family = std::move(other.family);
        size = other.size;
        bold = other.bold;
        italic = other.italic;
    }
    return *this;
}

void Font::setBold(bool b) {
    if (bold != b) {
        bold = b;
        if (format) {
            format->Release();
            format = nullptr;
        }
    }
}

void Font::setItalic(bool i) {
    if (italic != i) {
        italic = i;
        if (format) {
            format->Release();
            format = nullptr;
        }
    }
}

void Font::updateFormat(IDWriteFactory* factory) {
    if (format) return;
    if (!factory) return;

    HRESULT hr = factory->CreateTextFormat(
        family.c_str(),
        nullptr,
        bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
        italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        size,
        L"en-us", // Locale
        &format
    );
}

// ═════════════════════════════════════════════════════════════════════════════
// Renderer2D Implementation
// ═════════════════════════════════════════════════════════════════════════════

Renderer2D::Renderer2D() {
}

Renderer2D::~Renderer2D() {
    discardResources();
    if (writeFactory) writeFactory->Release();
    if (factory) factory->Release();
}

bool Renderer2D::initialize(HWND h) {
    hwnd = h;
    
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory);
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&writeFactory));
    if (FAILED(hr)) return false;

    return true;
}

void Renderer2D::createResources() {
    if (!target && hwnd) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

        HRESULT hr = factory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd, size),
            &target
        );

        if (SUCCEEDED(hr)) {
            // Create solid brush (we'll change color as needed)
            hr = target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &solidBrush);
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
    if (target) target->Clear(ToD2D(color));
}

void Renderer2D::drawRect(const Rect& rect, const Color& color, float strokeWidth) {
    if (target && solidBrush) {
        solidBrush->SetColor(ToD2D(color));
        D2D1_RECT_F r = D2D1::RectF((float)rect.x, (float)rect.y, (float)(rect.x + rect.width), (float)(rect.y + rect.height));
        target->DrawRectangle(&r, solidBrush, strokeWidth);
    }
}

void Renderer2D::fillRect(const Rect& rect, const Color& color) {
    if (target && solidBrush) {
        solidBrush->SetColor(ToD2D(color));
        D2D1_RECT_F r = D2D1::RectF((float)rect.x, (float)rect.y, (float)(rect.x + rect.width), (float)(rect.y + rect.height));
        target->FillRectangle(&r, solidBrush);
    }
}

void Renderer2D::drawLine(const Point& p1, const Point& p2, const Color& color, float strokeWidth) {
    if (target && solidBrush) {
        solidBrush->SetColor(ToD2D(color));
        target->DrawLine(
            D2D1::Point2F((float)p1.x, (float)p1.y),
            D2D1::Point2F((float)p2.x, (float)p2.y),
            solidBrush, strokeWidth
        );
    }
}

void Renderer2D::drawText(const Point& p, const String& text, const Font& font, const Color& color) {
    if (target && solidBrush && writeFactory) {
        const_cast<Font&>(font).updateFormat(writeFactory); // Ensure format exists
        if (font.getFormat()) {
            solidBrush->SetColor(ToD2D(color));
            D2D1_RECT_F rect = D2D1::RectF((float)p.x, (float)p.y, 10000.0f, 10000.0f); // Large bounds
            target->DrawText(
                text.c_str(),
                text.length(),
                font.getFormat(),
                rect,
                solidBrush
            );
        }
    }
}

void Renderer2D::drawStyledText(const Point& p, const String& text, const Font& font, const std::vector<TextRun>& runs, const Color& defaultColor) {
    if (target && solidBrush && writeFactory) {
        const_cast<Font&>(font).updateFormat(writeFactory);
        if (!font.getFormat()) return;

        IDWriteTextLayout* layout = nullptr;
        HRESULT hr = writeFactory->CreateTextLayout(
            text.c_str(),
            text.length(),
            font.getFormat(),
            10000.0f, // Max width
            10000.0f, // Max height
            &layout
        );

        if (SUCCEEDED(hr)) {
            // Apply ranges
            for (const auto& run : runs) {
                DWRITE_TEXT_RANGE range = { (UINT32)run.start, (UINT32)run.length };
                if (run.bold) layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range);
                if (run.italic) layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range);
                
                // For color, we need a specific drawing effect, which is complex in basic D2D.
                // For simplicity in this non-Qt version, we might skip per-character color in this basic implementation
                // OR use a custom renderer. 
                // However, standard DrawTextLayout with a brush uses one color. 
                // To support multi-color, we'd need to use SetDrawingEffect with a brush, but brushes serve as effects.
                // Let's settle for default color for now or implement full IDWriteTextRenderer later.
            }

            solidBrush->SetColor(ToD2D(defaultColor));
            target->DrawTextLayout(
                D2D1::Point2F((float)p.x, (float)p.y),
                layout,
                solidBrush
            );
            
            layout->Release();
        }
    }
}

SizeF Renderer2D::measureText(const String& text, const Font& font) {
    if (!writeFactory) return {0, 0};
    const_cast<Font&>(font).updateFormat(writeFactory);
    if (!font.getFormat()) return {0, 0};
    
    IDWriteTextLayout* layout = nullptr;
    HRESULT hr = writeFactory->CreateTextLayout(
        text.c_str(),
        text.length(),
        font.getFormat(),
        10000.0f,
        10000.0f,
        &layout
    );
    
    if (SUCCEEDED(hr)) {
        DWRITE_TEXT_METRICS metrics;
        layout->GetMetrics(&metrics);
        layout->Release();
        return { metrics.width, metrics.height }; 
    }
    return {0, 0};
}

void Renderer2D::drawEllipse(const Point& center, float radiusX, float radiusY, const Color& color, float strokeWidth) {
    if (target && solidBrush) {
        solidBrush->SetColor(ToD2D(color));
        target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F((float)center.x, (float)center.y), radiusX, radiusY), solidBrush, strokeWidth);
    }
}

void Renderer2D::fillEllipse(const Point& center, float radiusX, float radiusY, const Color& color) {
    if (target && solidBrush) {
        solidBrush->SetColor(ToD2D(color));
        target->FillEllipse(D2D1::Ellipse(D2D1::Point2F((float)center.x, (float)center.y), radiusX, radiusY), solidBrush);
    }
}

void Renderer2D::drawRoundedRect(const Rect& rect, float radiusX, float radiusY, const Color& color, float strokeWidth) {
    if (target && solidBrush) {
        solidBrush->SetColor(ToD2D(color));
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
            D2D1::RectF((float)rect.x, (float)rect.y, (float)(rect.x + rect.width), (float)(rect.y + rect.height)),
            radiusX, radiusY
        );
        target->DrawRoundedRectangle(&rr, solidBrush, strokeWidth);
    }
}

void Renderer2D::fillRoundedRect(const Rect& rect, float radiusX, float radiusY, const Color& color) {
    if (target && solidBrush) {
        solidBrush->SetColor(ToD2D(color));
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
            D2D1::RectF((float)rect.x, (float)rect.y, (float)(rect.x + rect.width), (float)(rect.y + rect.height)),
            radiusX, radiusY
        );
        target->FillRoundedRectangle(&rr, solidBrush);
    }
}

void Renderer2D::setTransform(const D2D1::Matrix3x2F& transform) {
    if (target) target->SetTransform(transform);
}

void Renderer2D::resetTransform() {
    if (target) target->SetTransform(D2D1::Matrix3x2F::Identity());
}

void Renderer2D::rotate(float angle, const Point& center) {
    if (target) {
        D2D1_MATRIX_3X2_F current;
        target->GetTransform(&current);
        target->SetTransform(current * D2D1::Matrix3x2F::Rotation(angle, D2D1::Point2F((float)center.x, (float)center.y)));
    }
}

void Renderer2D::scale(float x, float y, const Point& center) {
     if (target) {
        D2D1_MATRIX_3X2_F current;
        target->GetTransform(&current);
        target->SetTransform(current * D2D1::Matrix3x2F::Scale(x, y, D2D1::Point2F((float)center.x, (float)center.y)));
    }
}

void Renderer2D::translate(float x, float y) {
     if (target) {
        D2D1_MATRIX_3X2_F current;
        target->GetTransform(&current);
        target->SetTransform(current * D2D1::Matrix3x2F::Translation(x, y));
    }
}

void Renderer2D::pushClip(const Rect& rect) {
    if (target) {
        D2D1_RECT_F r = D2D1::RectF((float)rect.x, (float)rect.y, (float)(rect.x + rect.width), (float)(rect.y + rect.height));
        target->PushAxisAlignedClip(r, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
}

void Renderer2D::popClip() {
    if (target) {
        target->PopAxisAlignedClip();
    }
}

} // namespace RawrXD
