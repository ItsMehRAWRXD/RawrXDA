#include "RawrXD_Renderer_D2D.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace RawrXD {

// ToD2D is defined in header inline
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

Font::Font(const Font& other) 
    : family(other.family), size(other.size), bold(other.bold), italic(other.italic) {
    format = other.format;
    if (format) format->AddRef();
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
    : family(std::move(other.family)), size(other.size), bold(other.bold), italic(other.italic), format(other.format) {
    other.format = nullptr;
}

Font& Font::operator=(Font&& other) noexcept {
    if (this != &other) {
        if (format) format->Release();
        
        family = std::move(other.family);
        size = other.size;
        bold = other.bold;
        italic = other.italic;
        format = other.format;
        
        other.format = nullptr;
    }
    return *this;
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
#ifdef _DEBUG
    D2D1_FACTORY_OPTIONS options;
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, &factory);
#else
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory);
#endif
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
    if (target) target->Clear(ToD2D(color));
}

void Renderer2D::drawRect(const Rect& rect, const Color& color, float strokeWidth) {
    if (target && solidBrush) {
        solidBrush->SetColor(ToD2D(color));
        D2D1_RECT_F r = D2D1::RectF((float)rect.x, (float)rect.y, (float)rect.x + rect.width, (float)rect.y + rect.height);
        target->DrawRectangle(r, solidBrush, strokeWidth);
    }
}

void Renderer2D::fillRect(const Rect& rect, const Color& color) {
    if (target && solidBrush) {
        solidBrush->SetColor(ToD2D(color));
        D2D1_RECT_F r = D2D1::RectF((float)rect.x, (float)rect.y, (float)rect.x + rect.width, (float)rect.y + rect.height);
        target->FillRectangle(r, solidBrush);
    }
}

void Renderer2D::drawLine(const Point& p1, const Point& p2, const Color& color, float strokeWidth) {
    if (target && solidBrush) {
        solidBrush->SetColor(ToD2D(color));
        target->DrawLine(D2D1::Point2F((float)p1.x, (float)p1.y), D2D1::Point2F((float)p2.x, (float)p2.y), solidBrush, strokeWidth);
    }
}

void Renderer2D::drawText(const Point& p, const String& text, const Font& font, const Color& color) {
    if (target && solidBrush && writeFactory) {
        // Const cast trick because font is const reference but we need to update it
        const_cast<Font&>(font).updateFormat(writeFactory);
        
        if (font.getFormat()) {
            solidBrush->SetColor(ToD2D(color));
            D2D1_RECT_F layoutRect = D2D1::RectF((float)p.x, (float)p.y, 10000.0f, 10000.0f); // Massive rect
            target->DrawText(text.c_str(), text.length(), font.getFormat(), layoutRect, solidBrush);
        }
    }
}

void Renderer2D::drawStyledText(const Point& p, const String& text, const Font& font, const std::vector<TextRun>& runs, const Color& defaultColor) {
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
        target->CreateSolidColorBrush(ToD2D(run.color), &runBrush);
        if (runBrush) {
            // SetDrawingEffect works with D2D1RenderTarget::DrawTextLayout
            layout->SetDrawingEffect(runBrush, range);
            runBrush->Release(); // Layout holds a reference
        }
    }

    // Default brush for unstyled text
    solidBrush->SetColor(ToD2D(defaultColor));

    target->DrawTextLayout(
        D2D1::Point2F((float)p.x, (float)p.y),
        layout,
        solidBrush,
        D2D1_DRAW_TEXT_OPTIONS_NONE
    );

    layout->Release();
}

void Renderer2D::drawEllipse(const Point& center, float radiusX, float radiusY, const Color& color, float strokeWidth) {
    if (target && solidBrush) {
        solidBrush->SetColor(ToD2D(color));
        D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F((float)center.x, (float)center.y), radiusX, radiusY);
        target->DrawEllipse(ellipse, solidBrush, strokeWidth);
    }
}

void Renderer2D::fillEllipse(const Point& center, float radiusX, float radiusY, const Color& color) {
    if (target && solidBrush) {
        solidBrush->SetColor(ToD2D(color));
        D2D1_ELLIPSE ellipse = D2D1::Ellipse(D2D1::Point2F((float)center.x, (float)center.y), radiusX, radiusY);
        target->FillEllipse(ellipse, solidBrush);
    }
}

void Renderer2D::drawRoundedRect(const Rect& rect, float radiusX, float radiusY, const Color& color, float strokeWidth) {
    if (target && solidBrush) {
        solidBrush->SetColor(ToD2D(color));
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
            D2D1::RectF((float)rect.x, (float)rect.y, (float)rect.x + rect.width, (float)rect.y + rect.height),
            radiusX, 
            radiusY
        );
        target->DrawRoundedRectangle(rr, solidBrush, strokeWidth);
    }
}

void Renderer2D::fillRoundedRect(const Rect& rect, float radiusX, float radiusY, const Color& color) {
    if (target && solidBrush) {
        solidBrush->SetColor(ToD2D(color));
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
            D2D1::RectF((float)rect.x, (float)rect.y, (float)rect.x + rect.width, (float)rect.y + rect.height),
            radiusX, 
            radiusY
        );
        target->FillRoundedRectangle(rr, solidBrush);
    }
}

void Renderer2D::setTransform(const D2D1::Matrix3x2F& transform) {
    if (target) {
        target->SetTransform(transform);
    }
}

void Renderer2D::resetTransform() {
    if (target) {
        target->SetTransform(D2D1::Matrix3x2F::Identity());
    }
}

void Renderer2D::rotate(float angle, const Point& center) {
    if (target) {
        D2D1::Matrix3x2F current;
        target->GetTransform(&current);
        target->SetTransform(current * D2D1::Matrix3x2F::Rotation(angle, D2D1::Point2F((float)center.x, (float)center.y)));
    }
}

void Renderer2D::scale(float x, float y, const Point& center) {
    if (target) {
        D2D1::Matrix3x2F current;
        target->GetTransform(&current);
        target->SetTransform(current * D2D1::Matrix3x2F::Scale(x, y, D2D1::Point2F((float)center.x, (float)center.y)));
    }
}

void Renderer2D::translate(float x, float y) {
    if (target) {
        D2D1::Matrix3x2F current;
        target->GetTransform(&current);
        target->SetTransform(current * D2D1::Matrix3x2F::Translation(x, y));
    }
}

void Renderer2D::pushClip(const Rect& rect) {
    if (target) {
        D2D1_RECT_F r = D2D1::RectF((float)rect.x, (float)rect.y, (float)rect.x + rect.width, (float)rect.y + rect.height);
        target->PushAxisAlignedClip(r, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    }
}

void Renderer2D::popClip() {
    if (target) {
        target->PopAxisAlignedClip();
    }
}

} // namespace RawrXD
