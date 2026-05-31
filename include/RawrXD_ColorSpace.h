// ============================================================================
// RawrXD_ColorSpace.h — Professional Color Management with Adobe RGBa Support
// ============================================================================
//
// Phase 28.5: Professional Color Space Management
//
// This header provides:
//   1. Adobe RGB (1998) color space support for professional color accuracy
//   2. sRGB/Adobe RGB conversion with proper gamma handling
//   3. VSU (Visual Studio UI) effect color palettes
//   4. Alpha-premultiplied color blending
//   5. HDR color support preparation
//
// Adobe RGB (1998) provides a wider gamut than sRGB, particularly in cyan-green
// tones, making it ideal for professional IDE color schemes.
//
// Dependencies: None (header-only)
// Pattern: PatchResult-compatible, constexpr where possible
// ============================================================================

#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace RawrXD {
namespace ColorSpace {

// ============================================================================
// Color Space Constants
// ============================================================================

// Adobe RGB (1998) primaries (relative to D65)
// Reference: Adobe RGB (1998) Color Image Encoding
namespace AdobeRGB {
    constexpr float RedX   = 0.6400f;  constexpr float RedY   = 0.3300f;
    constexpr float GreenX = 0.2100f;  constexpr float GreenY = 0.7100f;
    constexpr float BlueX  = 0.1500f;  constexpr float BlueY  = 0.0600f;
    constexpr float WhiteX = 0.3127f;  constexpr float WhiteY = 0.3290f;  // D65
    
    // Gamma curve (2.2 with linear segment near zero)
    constexpr float Gamma = 2.2f;
    constexpr float LinearThreshold = 0.0f;  // Adobe RGB uses pure power curve
}

// sRGB primaries (IEC 61966-2-1)
namespace sRGB {
    constexpr float RedX   = 0.6400f;  constexpr float RedY   = 0.3300f;
    constexpr float GreenX = 0.3000f;  constexpr float GreenY = 0.6000f;
    constexpr float BlueX  = 0.1500f;  constexpr float BlueY  = 0.0600f;
    constexpr float WhiteX = 0.3127f;  constexpr float WhiteY = 0.3290f;  // D65
    
    // Gamma curve (approximately 2.2 with linear segment)
    inline float ToLinear(float c) {
        if (c <= 0.04045f) return c / 12.92f;
        return std::pow((c + 0.055f) / 1.055f, 2.4f);
    }
    
    inline float FromLinear(float c) {
        if (c <= 0.0031308f) return 12.92f * c;
        return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    }
}

// ============================================================================
// Adobe RGB Gamma Functions
// ============================================================================

inline float AdobeRGB_ToLinear(float c) {
    // Adobe RGB uses pure gamma 2.2
    return std::pow(c, AdobeRGB::Gamma);
}

inline float AdobeRGB_FromLinear(float c) {
    return std::pow(c, 1.0f / AdobeRGB::Gamma);
}

// ============================================================================
// Color Structure with Adobe RGBa Support
// ============================================================================

struct AdobeRGBa {
    float r, g, b, a;  // 0.0 - 1.0 range
    
    // Constructors
    constexpr AdobeRGBa() : r(0.0f), g(0.0f), b(0.0f), a(1.0f) {}
    constexpr AdobeRGBa(float red, float green, float blue, float alpha = 1.0f)
        : r(red), g(green), b(blue), a(alpha) {}
    
    // From COLORREF/DWORD (Windows RGB macro)
    AdobeRGBa(uint32_t colorref)
        : r(AdobeRGB_FromLinear(((colorref) & 0xFF) / 255.0f))
        , g(AdobeRGB_FromLinear(((colorref >> 8) & 0xFF) / 255.0f))
        , b(AdobeRGB_FromLinear(((colorref >> 16) & 0xFF) / 255.0f))
        , a(1.0f) {}
    
    // From 8-bit components
    static AdobeRGBa FromUint8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return AdobeRGBa(
            AdobeRGB_FromLinear(r / 255.0f),
            AdobeRGB_FromLinear(g / 255.0f),
            AdobeRGB_FromLinear(b / 255.0f),
            a / 255.0f
        );
    }
    
    // From BGRA (Windows/Direct2D native format)
    static AdobeRGBa FromBGRA(uint32_t bgra) {
        return FromUint8(
            static_cast<uint8_t>((bgra >> 16) & 0xFF),  // R
            static_cast<uint8_t>((bgra >>  8) & 0xFF),  // G
            static_cast<uint8_t>((bgra >>  0) & 0xFF),  // B
            static_cast<uint8_t>((bgra >> 24) & 0xFF)   // A
        );
    }
    
    // From ARGB (common web format)
    static AdobeRGBa FromARGB(uint32_t argb) {
        return FromUint8(
            static_cast<uint8_t>((argb >> 16) & 0xFF),  // R
            static_cast<uint8_t>((argb >>  8) & 0xFF),  // G
            static_cast<uint8_t>((argb >>  0) & 0xFF),  // B
            static_cast<uint8_t>((argb >> 24) & 0xFF)   // A
        );
    }
    
    // To BGRA for Direct2D
    uint32_t ToBGRA() const {
        auto clamp = [](float v) -> uint8_t {
            v = std::max(0.0f, std::min(1.0f, v));
            return static_cast<uint8_t>(v * 255.0f + 0.5f);
        };
        
        float lr = AdobeRGB_ToLinear(r);
        float lg = AdobeRGB_ToLinear(g);
        float lb = AdobeRGB_ToLinear(b);
        
        return (clamp(a) << 24) | (clamp(lr) << 16) | (clamp(lg) << 8) | clamp(lb);
    }
    
    // To D2D1_COLOR_F format (for Direct2D)
    // Note: Direct2D expects colors in scRGB (linear) space
    struct D2DColor {
        float r, g, b, a;
    };
    
    D2DColor ToD2D() const {
        // Convert Adobe RGB to linear scRGB for Direct2D
        return {
            AdobeRGB_ToLinear(r),
            AdobeRGB_ToLinear(g),
            AdobeRGB_ToLinear(b),
            a
        };
    }
    
    // To sRGB (for compatibility with standard displays)
    struct sRGBa {
        float r, g, b, a;
    };
    
    sRGBa TosRGB() const {
        // Adobe RGB -> Linear -> sRGB
        float lr = AdobeRGB_ToLinear(r);
        float lg = AdobeRGB_ToLinear(g);
        float lb = AdobeRGB_ToLinear(b);
        
        return {
            sRGB::FromLinear(lr),
            sRGB::FromLinear(lg),
            sRGB::FromLinear(lb),
            a
        };
    }
    
    // Clamping
    AdobeRGBa Clamped() const {
        auto clamp01 = [](float v) { return std::max(0.0f, std::min(1.0f, v)); };
        return AdobeRGBa(clamp01(r), clamp01(g), clamp01(b), clamp01(a));
    }
    
    // Premultiply alpha (for proper blending)
    AdobeRGBa Premultiplied() const {
        return AdobeRGBa(r * a, g * a, b * a, a);
    }
    
    // Unpremultiply alpha
    AdobeRGBa Unpremultiplied() const {
        if (a <= 0.0f) return AdobeRGBa(0, 0, 0, 0);
        if (a >= 1.0f) return *this;
        return AdobeRGBa(r / a, g / a, b / a, a);
    }
};

// ============================================================================
// VSU (Visual Studio UI) Effect Colors
// ============================================================================

namespace VSU {
    // Acrylic material effect colors (in Adobe RGB space)
    // These provide the subtle, modern look of VS Code/Visual Studio
    namespace Acrylic {
        // Base tints for acrylic backgrounds
        constexpr AdobeRGBa DarkBase(0.12f, 0.12f, 0.14f, 0.85f);
        constexpr AdobeRGBa LightBase(0.96f, 0.96f, 0.98f, 0.90f);
        
        // Luminosity overlay (adds depth)
        constexpr AdobeRGBa DarkLuminosity(0.08f, 0.08f, 0.10f, 0.60f);
        constexpr AdobeRGBa LightLuminosity(0.98f, 0.98f, 1.00f, 0.65f);
        
        // Tint colors for different UI elements
        constexpr AdobeRGBa SidebarTint(0.10f, 0.12f, 0.15f, 0.95f);
        constexpr AdobeRGBa PanelTint(0.11f, 0.11f, 0.13f, 0.92f);
        constexpr AdobeRGBa EditorTint(0.13f, 0.13f, 0.15f, 1.00f);
        
        // Noise texture overlay intensity
        constexpr float NoiseIntensity = 0.03f;
    }
    
    // Mica material effect (Windows 11 style)
    namespace Mica {
        // Mica uses the desktop wallpaper with a subtle tint
        constexpr AdobeRGBa DarkTint(0.11f, 0.11f, 0.13f, 0.96f);
        constexpr AdobeRGBa LightTint(0.95f, 0.95f, 0.97f, 0.97f);
        
        // Alt variants (for inactive/focused states)
        constexpr AdobeRGBa DarkTintAlt(0.13f, 0.13f, 0.15f, 0.94f);
        constexpr AdobeRGBa LightTintAlt(0.93f, 0.93f, 0.95f, 0.95f);
    }
    
    // Elevation shadows (for depth perception)
    namespace Shadows {
        // Shadow colors (premultiplied alpha)
        constexpr AdobeRGBa Shadow01(0.00f, 0.00f, 0.00f, 0.04f);  // 1px elevation
        constexpr AdobeRGBa Shadow02(0.00f, 0.00f, 0.00f, 0.08f);  // 2px elevation
        constexpr AdobeRGBa Shadow04(0.00f, 0.00f, 0.00f, 0.12f);  // 4px elevation
        constexpr AdobeRGBa Shadow08(0.00f, 0.00f, 0.00f, 0.18f);  // 8px elevation
        constexpr AdobeRGBa Shadow16(0.00f, 0.00f, 0.00f, 0.24f);  // 16px elevation
        constexpr AdobeRGBa Shadow32(0.00f, 0.00f, 0.00f, 0.32f);  // 32px elevation
    }
    
    // Accent colors (VS Code blue, etc.)
    namespace Accents {
        // Primary accent (VS Code blue in Adobe RGB)
        constexpr AdobeRGBa Blue(0.34f, 0.60f, 0.84f, 1.00f);
        constexpr AdobeRGBa BlueLight(0.45f, 0.70f, 0.92f, 1.00f);
        constexpr AdobeRGBa BlueDark(0.24f, 0.48f, 0.72f, 1.00f);
        
        // Semantic colors
        constexpr AdobeRGBa Success(0.20f, 0.72f, 0.36f, 1.00f);
        constexpr AdobeRGBa Warning(0.96f, 0.70f, 0.20f, 1.00f);
        constexpr AdobeRGBa Error(0.92f, 0.28f, 0.28f, 1.00f);
        constexpr AdobeRGBa Info(0.20f, 0.60f, 0.92f, 1.00f);
    }
}

// ============================================================================
// Blending Operations (Alpha Compositing)
// ============================================================================

// Standard alpha compositing (src-over)
inline AdobeRGBa BlendSrcOver(const AdobeRGBa& src, const AdobeRGBa& dst) {
    float outA = src.a + dst.a * (1.0f - src.a);
    if (outA < 0.001f) return AdobeRGBa(0, 0, 0, 0);
    
    float invSrcA = 1.0f - src.a;
    float r = (src.r * src.a + dst.r * dst.a * invSrcA) / outA;
    float g = (src.g * src.a + dst.g * dst.a * invSrcA) / outA;
    float b = (src.b * src.a + dst.b * dst.a * invSrcA) / outA;
    
    return AdobeRGBa(r, g, b, outA);
}

// Multiply blend (for shadows)
inline AdobeRGBa BlendMultiply(const AdobeRGBa& src, const AdobeRGBa& dst) {
    return AdobeRGBa(
        src.r * dst.r,
        src.g * dst.g,
        src.b * dst.b,
        src.a + dst.a * (1.0f - src.a)
    );
}

// Screen blend (for highlights)
inline AdobeRGBa BlendScreen(const AdobeRGBa& src, const AdobeRGBa& dst) {
    return AdobeRGBa(
        1.0f - (1.0f - src.r) * (1.0f - dst.r),
        1.0f - (1.0f - src.g) * (1.0f - dst.g),
        1.0f - (1.0f - src.b) * (1.0f - dst.b),
        src.a + dst.a * (1.0f - src.a)
    );
}

// Overlay blend (for contrast enhancement)
inline AdobeRGBa BlendOverlay(const AdobeRGBa& src, const AdobeRGBa& dst) {
    auto overlay = [](float s, float d) -> float {
        if (d < 0.5f) return 2.0f * s * d;
        return 1.0f - 2.0f * (1.0f - s) * (1.0f - d);
    };
    return AdobeRGBa(
        overlay(src.r, dst.r),
        overlay(src.g, dst.g),
        overlay(src.b, dst.b),
        src.a + dst.a * (1.0f - src.a)
    );
}

// Linear interpolation between two colors
inline AdobeRGBa Lerp(const AdobeRGBa& a, const AdobeRGBa& b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    return AdobeRGBa(
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t
    );
}

// ============================================================================
// Theme Color Palettes (Adobe RGBa)
// ============================================================================

namespace Themes {
    // Dark theme with Adobe RGB color accuracy
    namespace Dark {
        // Backgrounds
        constexpr AdobeRGBa BgPrimary(0.10f, 0.10f, 0.12f, 1.00f);      // Main background
        constexpr AdobeRGBa BgSecondary(0.08f, 0.08f, 0.10f, 1.00f);    // Sidebar
        constexpr AdobeRGBa BgTertiary(0.12f, 0.12f, 0.14f, 1.00f);     // Active elements
        constexpr AdobeRGBa BgCurrentLine(0.15f, 0.15f, 0.17f, 1.00f); // Current line
        constexpr AdobeRGBa BgSelection(0.15f, 0.35f, 0.47f, 0.60f);    // Selection
        
        // Foregrounds
        constexpr AdobeRGBa FgPrimary(0.90f, 0.90f, 0.92f, 1.00f);       // Primary text
        constexpr AdobeRGBa FgSecondary(0.65f, 0.65f, 0.67f, 1.00f);     // Secondary text
        constexpr AdobeRGBa FgDisabled(0.45f, 0.45f, 0.47f, 1.00f);     // Disabled text
        
        // Syntax highlighting (Adobe RGB optimized)
        constexpr AdobeRGBa Keyword(0.33f, 0.58f, 0.82f, 1.00f);        // Blue
        constexpr AdobeRGBa String(0.81f, 0.57f, 0.47f, 1.00f);          // Orange
        constexpr AdobeRGBa Comment(0.42f, 0.60f, 0.33f, 1.00f);         // Green
        constexpr AdobeRGBa Number(0.71f, 0.81f, 0.66f, 1.00f);          // Light green
        constexpr AdobeRGBa Function(0.61f, 0.81f, 0.99f, 1.00f);        // Light blue
        constexpr AdobeRGBa Type(0.20f, 0.72f, 0.36f, 1.00f);            // Green
        constexpr AdobeRGBa Variable(0.90f, 0.90f, 0.92f, 1.00f);        // White
        constexpr AdobeRGBa Operator(0.90f, 0.90f, 0.92f, 1.00f);       // White
        
        // UI elements
        constexpr AdobeRGBa Border(0.20f, 0.20f, 0.22f, 1.00f);
        constexpr AdobeRGBa BorderActive(0.33f, 0.58f, 0.82f, 1.00f);
        constexpr AdobeRGBa Cursor(0.95f, 0.95f, 0.97f, 1.00f);
        constexpr AdobeRGBa LineNumber(0.52f, 0.52f, 0.54f, 1.00f);
        
        // Ghost text (AI suggestions)
        constexpr AdobeRGBa GhostText(0.50f, 0.50f, 0.50f, 0.50f);
    }
    
    // Light theme with Adobe RGB color accuracy
    namespace Light {
        // Backgrounds
        constexpr AdobeRGBa BgPrimary(0.98f, 0.98f, 1.00f, 1.00f);       // Main background
        constexpr AdobeRGBa BgSecondary(0.95f, 0.95f, 0.97f, 1.00f);    // Sidebar
        constexpr AdobeRGBa BgTertiary(1.00f, 1.00f, 1.00f, 1.00f);      // Active elements
        constexpr AdobeRGBa BgCurrentLine(0.93f, 0.95f, 0.97f, 1.00f);  // Current line
        constexpr AdobeRGBa BgSelection(0.67f, 0.85f, 0.95f, 0.60f);     // Selection
        
        // Foregrounds
        constexpr AdobeRGBa FgPrimary(0.10f, 0.10f, 0.12f, 1.00f);       // Primary text
        constexpr AdobeRGBa FgSecondary(0.35f, 0.35f, 0.37f, 1.00f);    // Secondary text
        constexpr AdobeRGBa FgDisabled(0.55f, 0.55f, 0.57f, 1.00f);      // Disabled text
        
        // Syntax highlighting
        constexpr AdobeRGBa Keyword(0.00f, 0.00f, 0.80f, 1.00f);        // Blue
        constexpr AdobeRGBa String(0.80f, 0.20f, 0.00f, 1.00f);          // Red/Orange
        constexpr AdobeRGBa Comment(0.20f, 0.50f, 0.20f, 1.00f);        // Green
        constexpr AdobeRGBa Number(0.00f, 0.50f, 0.00f, 1.00f);           // Green
        constexpr AdobeRGBa Function(0.00f, 0.00f, 0.60f, 1.00f);         // Dark blue
        constexpr AdobeRGBa Type(0.20f, 0.20f, 0.60f, 1.00f);             // Purple
        constexpr AdobeRGBa Variable(0.10f, 0.10f, 0.12f, 1.00f);         // Black
        constexpr AdobeRGBa Operator(0.10f, 0.10f, 0.12f, 1.00f);        // Black
        
        // UI elements
        constexpr AdobeRGBa Border(0.80f, 0.80f, 0.82f, 1.00f);
        constexpr AdobeRGBa BorderActive(0.00f, 0.40f, 0.80f, 1.00f);
        constexpr AdobeRGBa Cursor(0.00f, 0.00f, 0.00f, 1.00f);
        constexpr AdobeRGBa LineNumber(0.48f, 0.48f, 0.50f, 1.00f);
        
        // Ghost text
        constexpr AdobeRGBa GhostText(0.50f, 0.50f, 0.50f, 0.50f);
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

// Calculate luminance (perceived brightness)
inline float Luminance(const AdobeRGBa& c) {
    // Adobe RGB luminance coefficients
    return 0.2973f * c.r + 0.6274f * c.g + 0.0753f * c.b;
}

// Determine if color is dark (for contrast calculations)
inline bool IsDark(const AdobeRGBa& c) {
    return Luminance(c) < 0.5f;
}

// Calculate contrast ratio (WCAG 2.0)
inline float ContrastRatio(const AdobeRGBa& c1, const AdobeRGBa& c2) {
    auto lum = [](const AdobeRGBa& c) -> float {
        float l = Luminance(c);
        return (l <= 0.03928f) ? (l / 12.92f) : std::pow((l + 0.055f) / 1.055f, 2.4f);
    };
    
    float l1 = lum(c1) + 0.05f;
    float l2 = lum(c2) + 0.05f;
    return (l1 > l2) ? (l1 / l2) : (l2 / l1);
}

// Ensure minimum contrast (WCAG AA: 4.5:1 for normal text)
inline AdobeRGBa EnsureContrast(const AdobeRGBa& fg, const AdobeRGBa& bg, float minRatio = 4.5f) {
    if (ContrastRatio(fg, bg) >= minRatio) return fg;
    
    // Adjust foreground color to meet contrast
    AdobeRGBa adjusted = fg;
    bool bgIsDark = IsDark(bg);
    
    for (int i = 0; i < 20 && ContrastRatio(adjusted, bg) < minRatio; ++i) {
        if (bgIsDark) {
            // Lighten foreground
            adjusted = Lerp(adjusted, AdobeRGBa(1, 1, 1, 1), 0.1f);
        } else {
            // Darken foreground
            adjusted = Lerp(adjusted, AdobeRGBa(0, 0, 0, 1), 0.1f);
        }
    }
    
    return adjusted;
}

} // namespace ColorSpace
} // namespace RawrXD
