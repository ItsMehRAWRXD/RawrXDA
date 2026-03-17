#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <tchar.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <algorithm>
#include <math.h>
#include <fstream>
#include <sstream>
#include <map>
#include <GL/gl.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "dwmapi.lib")

// OpenGL extensions (basic compatibility)
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif

struct V2 { float x, y; };
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

// Core system
static HINSTANCE gApp = nullptr;
static HWND      gWnd = nullptr;
static HDC       gDC  = nullptr;
static HGLRC     gRC  = nullptr;
static int       gW   = 1400;
static int       gH   = 900;
static bool      gRunning = true;

// File type detection
enum FileType {
    FILE_UNKNOWN = 0,
    FILE_CPP = 1,
    FILE_GLSL_VERT = 2,
    FILE_GLSL_FRAG = 3,
    FILE_HEADER = 4,
    FILE_TEXT = 5
};

// Chameleon effect (software-based since OpenGL extensions are problematic)
static bool      gChameleonEnabled = true;
static float     gChameleonTime = 0.0f;

// Syntax highlighting colors
struct SyntaxColor {
    float r, g, b, a;
};

static const SyntaxColor SYNTAX_COLORS[] = {
    {0.9f, 0.9f, 0.9f, 1.0f},  // Default text
    {0.3f, 0.6f, 1.0f, 1.0f},  // Keywords (blue)
    {0.9f, 0.4f, 0.2f, 1.0f},  // Strings (orange)
    {0.4f, 0.8f, 0.3f, 1.0f},  // Comments (green)
    {1.0f, 0.8f, 0.2f, 1.0f},  // Numbers (yellow)
    {0.8f, 0.3f, 0.9f, 1.0f},  // Preprocessor (purple)
    {0.6f, 0.9f, 0.9f, 1.0f}   // GLSL keywords (cyan)
};

// Transparency controls
static BYTE      gMainOpacity = 200;
static float     gMainR = 0.08f;
static float     gMainG = 0.09f;
static float     gMainB = 0.12f;
static float     gMainA = 0.65f;

// UI Layout
static RECT      gOpacitySlider = { 50, 50, 370, 74 };
static RECT      gEditorRect = { 20, 90, 1380, 880 };
static RECT      gFileTypeArea = { 400, 50, 600, 74 };
static RECT      gChameleonToggle = { 620, 50, 750, 74 };
static bool      gSliderDrag = false;
static bool      gEditorFocus = false;

// Text editor state
static std::wstring gBuffer;
static size_t    gCaret = 0;
static int       gScrollY = 0;
static DWORD     gStartTick = 0;
static FileType  gCurrentFileType = FILE_CPP;
static std::wstring gCurrentFilename = L"untitled.cpp";

// Font system
static GLuint    gFontTex = 0;
static int       gGlyphW = 6;
static int       gGlyphH = 8;
static int       gAtlasCols = 16;
static int       gAtlasRows = 6;
static int       gAtlasW = 0;
static int       gAtlasH = 0;

static bool EnsureDDriveOnly() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (path[0] != L'D' || path[1] != L':') return false;
    return true;
}

static FileType DetectFileType(const std::wstring& filename) {
    std::wstring ext = filename.substr(filename.find_last_of(L".") + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == L"cpp" || ext == L"cc" || ext == L"cxx") return FILE_CPP;
    if (ext == L"vert" || ext == L"vertex") return FILE_GLSL_VERT;
    if (ext == L"frag" || ext == L"fragment") return FILE_GLSL_FRAG;
    if (ext == L"h" || ext == L"hpp" || ext == L"hxx") return FILE_HEADER;
    if (ext == L"txt" || ext == L"md") return FILE_TEXT;
    
    return FILE_UNKNOWN;
}

// HSV to RGB conversion for chameleon effect
static void HSVtoRGB(float h, float s, float v, float& r, float& g, float& b) {
    int i = int(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    switch (i % 6) {
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
    }
}

static bool InitGL(HWND hWnd) {
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 0;
    pfd.iLayerType = PFD_MAIN_PLANE;

    gDC = GetDC(hWnd);
    if (!gDC) return false;

    int pf = ChoosePixelFormat(gDC, &pfd);
    if (!pf) return false;
    if (!SetPixelFormat(gDC, pf, &pfd)) return false;

    gRC = wglCreateContext(gDC);
    if (!gRC) return false;
    if (!wglMakeCurrent(gDC, gRC)) return false;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    return true;
}

static void Ortho2D(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static const uint8_t FONT6x8[96][8] = {
    {0,0,0,0,0,0,0,0},{0x04,0x04,0x04,0x04,0x04,0,0x04,0},{0x0A,0x0A,0x0A,0,0,0,0,0},
    {0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0,0},{0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04,0},
    {0x19,0x19,0x02,0x04,0x08,0x13,0x13,0},{0x0C,0x12,0x14,0x08,0x15,0x12,0x0D,0},
    {0x06,0x04,0x08,0,0,0,0,0},{0x06,0x08,0x10,0x10,0x10,0x08,0x06,0},
    {0x06,0x02,0x01,0x01,0x01,0x02,0x06,0},{0x00,0x04,0x15,0x0E,0x15,0x04,0x00,0},
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00,0},{0,0,0,0,0x06,0x04,0x08,0},
    {0,0,0,0x1F,0,0,0,0},{0,0,0,0,0,0x06,0x06,0},{0x01,0x02,0x04,0x08,0x10,0,0,0},
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E,0},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E,0},
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F,0},{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E,0},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02,0},{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E,0},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E,0},{0x1F,0x01,0x02,0x04,0x08,0x08,0x08,0},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E,0},{0x0E,0x11,0x11,0x0F,0x01,0x11,0x0E,0},
    {0,0x06,0x06,0,0x06,0x06,0,0},{0,0x06,0x06,0,0x06,0x04,0x08,0},
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02,0},{0,0,0x1F,0,0x1F,0,0,0},
    {0x08,0x04,0x02,0x01,0x02,0x04,0x08,0},{0x0E,0x11,0x01,0x02,0x04,0,0x04,0},
    {0x0E,0x11,0x15,0x15,0x1D,0x10,0x0E,0},{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11,0},
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E,0},{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E,0},
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E,0},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F,0},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10,0},{0x0F,0x10,0x10,0x17,0x11,0x11,0x0F,0},
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11,0},{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E,0},
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C,0},{0x11,0x12,0x14,0x18,0x14,0x12,0x11,0},
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F,0},{0x11,0x1B,0x15,0x11,0x11,0x11,0x11,0},
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11,0},{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E,0},
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10,0},{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D,0},
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11,0},{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E,0},
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04,0},{0x11,0x11,0x11,0x11,0x11,0x11,0x0E,0},
    {0x11,0x11,0x11,0x0A,0x0A,0x04,0x04,0},{0x11,0x11,0x11,0x15,0x15,0x1B,0x11,0},
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11,0},{0x11,0x11,0x0A,0x04,0x04,0x04,0x04,0},
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F,0},{0x0E,0x08,0x08,0x08,0x08,0x08,0x0E,0},
    {0x10,0x08,0x04,0x02,0x01,0,0,0},{0x0E,0x02,0x02,0x02,0x02,0x02,0x0E,0},
    {0x04,0x0A,0x11,0,0,0,0,0},{0,0,0,0,0,0,0x1F,0},{0x08,0x04,0,0,0,0,0,0},
    {0,0,0x0E,0x01,0x0F,0x11,0x0F,0},{0x10,0x10,0x1E,0x11,0x11,0x11,0x1E,0},
    {0,0,0x0E,0x10,0x10,0x10,0x0E,0},{0x01,0x01,0x0F,0x11,0x11,0x11,0x0F,0},
    {0,0,0x0E,0x11,0x1F,0x10,0x0E,0},{0x06,0x08,0x1E,0x08,0x08,0x08,0x08,0},
    {0,0,0x0F,0x11,0x11,0x0F,0x01,0x0E},{0x10,0x10,0x1E,0x11,0x11,0x11,0x11,0},
    {0x04,0,0x0C,0x04,0x04,0x04,0x0E,0},{0x02,0,0x06,0x02,0x02,0x12,0x0C,0},
    {0x10,0x10,0x12,0x14,0x18,0x14,0x12,0},{0x0C,0x04,0x04,0x04,0x04,0x04,0x0E,0},
    {0,0,0x1A,0x15,0x15,0x11,0x11,0},{0,0,0x1E,0x11,0x11,0x11,0x11,0},
    {0,0,0x0E,0x11,0x11,0x11,0x0E,0},{0,0,0x1E,0x11,0x11,0x1E,0x10,0x10},
    {0,0,0x0F,0x11,0x11,0x0F,0x01,0x01},{0,0,0x16,0x19,0x10,0x10,0x10,0},
    {0,0,0x0F,0x10,0x0E,0x01,0x1E,0},{0x08,0x08,0x1E,0x08,0x08,0x08,0x06,0},
    {0,0,0x11,0x11,0x11,0x11,0x0F,0},{0,0,0x11,0x11,0x0A,0x0A,0x04,0},
    {0,0,0x11,0x11,0x15,0x1B,0x11,0},{0,0,0x11,0x0A,0x04,0x0A,0x11,0},
    {0,0,0x11,0x11,0x0F,0x01,0x0E,0},{0,0,0x1F,0x02,0x04,0x08,0x1F,0},
    {0x06,0x08,0x08,0x10,0x08,0x08,0x06,0},{0x04,0x04,0x04,0x04,0x04,0x04,0x04,0},
    {0x0C,0x02,0x02,0x01,0x02,0x02,0x0C,0},{0x00,0x09,0x16,0,0,0,0,0},{0,0,0,0,0,0,0,0}
};

static void BuildAtlas() {
    gAtlasW = gAtlasCols * gGlyphW;
    gAtlasH = gAtlasRows * gGlyphH;
    std::vector<uint8_t> alpha(gAtlasW * gAtlasH, 0);

    for (int gi = 0; gi < 96; ++gi) {
        int c = gi % gAtlasCols;
        int r = gi / gAtlasCols;
        int ox = c * gGlyphW;
        int oy = r * gGlyphH;

        for (int y = 0; y < gGlyphH; ++y) {
            uint8_t row = FONT6x8[gi][y];
            for (int x = 0; x < gGlyphW; ++x) {
                bool on = (row >> x) & 1;
                int px = ox + x;
                int py = oy + y;
                alpha[py * gAtlasW + px] = on ? 255 : 0;
            }
        }
    }

    if (!gFontTex) glGenTextures(1, &gFontTex);
    glBindTexture(GL_TEXTURE_2D, gFontTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, gAtlasW, gAtlasH, 0, GL_ALPHA, GL_UNSIGNED_BYTE, alpha.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

static void DrawRect(float x0, float y0, float x1, float y1, float r, float g, float b, float a) {
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x0, y0);
    glVertex2f(x1, y0);
    glVertex2f(x1, y1);
    glVertex2f(x0, y1);
    glEnd();
}

static void DrawCircle(float cx, float cy, float radius, float r, float g, float b, float a) {
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
    for (int i = 0; i <= 48; ++i) {
        float t = 6.2831853f * (float)i / 48.0f;
        glVertex2f(cx + radius * cosf(t), cy + radius * sinf(t));
    }
    glEnd();
}

static void DrawGlyph(int code, float x, float y, float scale, float r, float g, float b, float a, bool useChameleon = false) {
    if (code < 32 || code > 127) code = 32;
    int gi = code - 32;
    int col = gi % gAtlasCols;
    int row = gi / gAtlasCols;

    float u0 = (float)(col * gGlyphW) / (float)gAtlasW;
    float v0 = (float)(row * gGlyphH) / (float)gAtlasH;
    float u1 = (float)((col + 1) * gGlyphW) / (float)gAtlasW;
    float v1 = (float)((row + 1) * gGlyphH) / (float)gAtlasH;

    float w = gGlyphW * scale;
    float h = gGlyphH * scale;

    // Chameleon effect (software implementation)
    float finalR = r, finalG = g, finalB = b;
    if (useChameleon && gChameleonEnabled) {
        float hue = fmodf(gChameleonTime * 0.3f + (x + y) * 0.01f, 1.0f);
        float sat = 0.8f + 0.2f * sinf(gChameleonTime * 2.0f + x * 0.05f);
        float val = 0.9f + 0.1f * sinf(gChameleonTime * 3.0f + y * 0.03f);
        
        HSVtoRGB(hue, sat, val, finalR, finalG, finalB);
        
        // Add glow
        float glow = 1.0f + 0.3f * sinf(gChameleonTime * 4.0f + sqrtf(x*x + y*y) * 0.02f);
        finalR *= glow;
        finalG *= glow;
        finalB *= glow;
        
        finalR = clampf(finalR, 0.0f, 1.0f);
        finalG = clampf(finalG, 0.0f, 1.0f);
        finalB = clampf(finalB, 0.0f, 1.0f);
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gFontTex);
    glColor4f(finalR, finalG, finalB, a);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(u0, v0); glVertex2f(x,     y);
    glTexCoord2f(u1, v0); glVertex2f(x + w, y);
    glTexCoord2f(u1, v1); glVertex2f(x + w, y + h);
    glTexCoord2f(u0, v1); glVertex2f(x,     y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

// Simple syntax highlighting
static int GetSyntaxColorIndex(wchar_t ch, const std::wstring& context, size_t pos) {
    // Keywords for different languages
    static const std::vector<std::wstring> cppKeywords = {
        L"int", L"float", L"double", L"char", L"bool", L"void", L"if", L"else", L"for", L"while", 
        L"return", L"class", L"struct", L"public", L"private", L"protected", L"namespace", L"using"
    };
    
    static const std::vector<std::wstring> glslKeywords = {
        L"vec2", L"vec3", L"vec4", L"mat2", L"mat3", L"mat4", L"uniform", L"varying", L"attribute",
        L"precision", L"vertex", L"fragment", L"texture2D", L"gl_Position", L"gl_FragColor"
    };
    
    // Check for comments
    if (pos > 0 && context[pos-1] == L'/' && ch == L'/') return 3;
    if (pos > 1 && context[pos-2] == L'/' && context[pos-1] == L'/') return 3;
    
    // Check for strings
    bool inString = false;
    for (size_t i = 0; i < pos; ++i) {
        if (context[i] == L'"' && (i == 0 || context[i-1] != L'\\')) {
            inString = !inString;
        }
    }
    if (inString || ch == L'"') return 2;
    
    // Check for numbers
    if (ch >= L'0' && ch <= L'9') return 4;
    
    // Check for preprocessor
    if (ch == L'#' || (pos > 0 && context[pos-1] == L'#')) return 5;
    
    // Check for GLSL keywords if it's a GLSL file
    if (gCurrentFileType == FILE_GLSL_VERT || gCurrentFileType == FILE_GLSL_FRAG) {
        // Simple keyword detection
        for (const auto& keyword : glslKeywords) {
            if (pos >= keyword.length()) {
                bool match = true;
                for (size_t j = 0; j < keyword.length(); ++j) {
                    if (context[pos - keyword.length() + j] != keyword[j]) {
                        match = false;
                        break;
                    }
                }
                if (match) return 6;
            }
        }
    }
    
    return 0; // Default
}

static void DrawText(const std::wstring& s, float x, float y, float scale, bool useChameleon = false, int wrapW = -1) {
    float advance = (gGlyphW + 1) * scale;
    float lineH = (gGlyphH + 2) * scale;
    float cx = x, cy = y;
    
    for (size_t i = 0; i < s.length(); ++i) {
        wchar_t wc = s[i];
        
        if (wc == L'\n') {
            cx = x;
            cy += lineH;
            continue;
        }
        if (wrapW > 0 && cx + advance > x + wrapW) {
            cx = x;
            cy += lineH;
        }
        
        // Get syntax color
        int colorIndex = GetSyntaxColorIndex(wc, s, i);
        SyntaxColor color = SYNTAX_COLORS[colorIndex];
        
        DrawGlyph((int)wc, cx, cy, scale, color.r, color.g, color.b, color.a, useChameleon);
        cx += advance;
    }
}

static void UpdateWindowAlpha() {
    SetLayeredWindowAttributes(gWnd, 0, gMainOpacity, LWA_ALPHA);
}

static float Remap(float v, float a0, float a1, float b0, float b1) {
    float t = (v - a0) / (a1 - a0);
    return b0 + t * (b1 - b0);
}

static bool PtInRectI(const RECT& r, int x, int y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

static void SaveFile(const std::wstring& filename) {
    std::wofstream file(filename);
    if (file.is_open()) {
        file << gBuffer;
        file.close();
        gCurrentFilename = filename;
        gCurrentFileType = DetectFileType(filename);
    }
}

static void LoadFile(const std::wstring& filename) {
    std::wifstream file(filename);
    if (file.is_open()) {
        std::wstringstream buffer;
        buffer << file.rdbuf();
        gBuffer = buffer.str();
        gCaret = gBuffer.size();
        gCurrentFilename = filename;
        gCurrentFileType = DetectFileType(filename);
        file.close();
    }
}

static void Render() {
    gChameleonTime = (GetTickCount() - gStartTick) / 1000.0f;
    
    Ortho2D(gW, gH);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    // Background
    DrawRect(0, 0, (float)gW, (float)gH, gMainR, gMainG, gMainB, gMainA);

    // Opacity slider
    DrawRect((float)gOpacitySlider.left, (float)gOpacitySlider.top, (float)gOpacitySlider.right, (float)gOpacitySlider.bottom,
             0.15f, 0.17f, 0.19f, 0.7f);
    
    float sliderPos = gMainOpacity / 255.0f;
    float knobX = gOpacitySlider.left + (gOpacitySlider.right - gOpacitySlider.left) * sliderPos;
    float knobY = (gOpacitySlider.top + gOpacitySlider.bottom) * 0.5f;
    DrawCircle(knobX, knobY, 8.0f, 0.9f, 0.95f, 1.0f, 0.95f);

    DrawText(L"Opacity", 10, (float)gOpacitySlider.top - 2, 2.0f, false);

    // File type indicator
    const wchar_t* fileTypeNames[] = { L"Unknown", L"C++", L"Vertex Shader", L"Fragment Shader", L"Header", L"Text" };
    wchar_t fileInfo[256];
    swprintf_s(fileInfo, L"%s (%s)", fileTypeNames[gCurrentFileType], gCurrentFilename.c_str());
    DrawText(fileInfo, (float)gFileTypeArea.left, (float)gFileTypeArea.top, 2.0f, false);

    // Chameleon toggle
    DrawRect((float)gChameleonToggle.left, (float)gChameleonToggle.top, (float)gChameleonToggle.right, (float)gChameleonToggle.bottom,
             gChameleonEnabled ? 0.2f : 0.15f, gChameleonEnabled ? 0.6f : 0.17f, gChameleonEnabled ? 0.3f : 0.19f, 0.8f);
    DrawText(gChameleonEnabled ? L"Chameleon ON" : L"Chameleon OFF", 
             (float)gChameleonToggle.left + 5, (float)gChameleonToggle.top + 2, 2.0f, false);

    // Editor area
    DrawRect((float)gEditorRect.left, (float)gEditorRect.top, (float)gEditorRect.right, (float)gEditorRect.bottom,
             0.05f, 0.06f, 0.08f, 0.85f);

    // Line numbers
    DrawRect((float)gEditorRect.left, (float)gEditorRect.top, (float)gEditorRect.left + 50, (float)gEditorRect.bottom,
             0.08f, 0.09f, 0.11f, 0.9f);

    // Content
    float contentY = (float)gEditorRect.top + 10 - gScrollY;
    
    // Line numbers
    for (int line = 0; line < 60; ++line) {
        wchar_t lineNum[8];
        swprintf_s(lineNum, L"%d", line + 1);
        DrawText(lineNum, (float)gEditorRect.left + 5, contentY + line * 24, 2.0f, false);
    }

    // Main text with syntax highlighting and chameleon effect
    DrawText(gBuffer, (float)gEditorRect.left + 60, contentY, 2.5f, gChameleonEnabled, 
             gEditorRect.right - gEditorRect.left - 80);

    // Caret
    DWORD ticks = GetTickCount() - gStartTick;
    bool caretVis = ((ticks / 500) % 2) == 0 && gEditorFocus;
    if (caretVis) {
        float scale = 2.5f;
        float advance = (gGlyphW + 1) * scale;
        float lineH = (gGlyphH + 2) * scale;
        float x0 = (float)gEditorRect.left + 60;
        float y0 = contentY;
        float cx = x0, cy = y0;
        int wrapW = gEditorRect.right - gEditorRect.left - 80;
        size_t idx = 0;
        for (; idx < gCaret && idx < gBuffer.size(); ++idx) {
            wchar_t ch = gBuffer[idx];
            if (ch == L'\n') {
                cx = x0;
                cy += lineH;
            } else {
                if (cx + advance > x0 + wrapW) {
                    cx = x0;
                    cy += lineH;
                }
                cx += advance;
            }
        }
        DrawRect(cx, cy, cx + 2.0f, cy + lineH, 0.9f, 0.95f, 1.0f, 0.9f);
    }

    // Status line
    wchar_t status[512];
    swprintf_s(status, L"Pos: %zu | Lines: %zu | Chameleon: %s | File: %s", 
               gCaret, std::count(gBuffer.begin(), gBuffer.end(), L'\n') + 1,
               gChameleonEnabled ? L"ON" : L"OFF", gCurrentFilename.c_str());
    DrawText(status, 20, (float)gH - 30, 2.0f, false);

    SwapBuffers(gDC);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE: {
        gW = LOWORD(lParam);
        gH = HIWORD(lParam);
        gEditorRect = { 20, 90, gW - 20, gH - 40 };
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lParam);
        int my = GET_Y_LPARAM(lParam);
        
        if (PtInRectI(gOpacitySlider, mx, my)) {
            gSliderDrag = true;
            SetCapture(hWnd);
            float t = (mx - gOpacitySlider.left) / (float)(gOpacitySlider.right - gOpacitySlider.left);
            gMainOpacity = (BYTE)(clampf(t, 0.0f, 1.0f) * 255);
            UpdateWindowAlpha();
        } else if (PtInRectI(gChameleonToggle, mx, my)) {
            gChameleonEnabled = !gChameleonEnabled;
        } else if (PtInRectI(gEditorRect, mx, my)) {
            gEditorFocus = true;
            gCaret = gBuffer.size();
        } else {
            gEditorFocus = false;
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (gSliderDrag) {
            int mx = GET_X_LPARAM(lParam);
            float t = (mx - gOpacitySlider.left) / (float)(gOpacitySlider.right - gOpacitySlider.left);
            gMainOpacity = (BYTE)(clampf(t, 0.0f, 1.0f) * 255);
            UpdateWindowAlpha();
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (gSliderDrag) {
            gSliderDrag = false;
            ReleaseCapture();
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        if (wParam == VK_F4) {
            gChameleonEnabled = !gChameleonEnabled;
            return 0;
        }
        if ((GetKeyState(VK_CONTROL) & 0x8000)) {
            if (wParam == 'S') {
                SaveFile(L"D:\\cursor-multi-ai-extension\\glassquill\\test.cpp");
                return 0;
            }
            if (wParam == 'O') {
                LoadFile(L"D:\\cursor-multi-ai-extension\\glassquill\\src\\glassquill.cpp");
                return 0;
            }
        }
        
        if (!gEditorFocus) return 0;
        
        if (wParam == VK_BACK) {
            if (gCaret > 0 && !gBuffer.empty()) {
                gBuffer.erase(gBuffer.begin() + (gCaret - 1));
                --gCaret;
            }
            return 0;
        }
        if (wParam == VK_LEFT && gCaret > 0) {
            --gCaret;
            return 0;
        }
        if (wParam == VK_RIGHT && gCaret < gBuffer.size()) {
            ++gCaret;
            return 0;
        }
        if (wParam == VK_HOME) {
            gCaret = 0;
            return 0;
        }
        if (wParam == VK_END) {
            gCaret = gBuffer.size();
            return 0;
        }
        return 0;
    }
    case WM_CHAR: {
        if (!gEditorFocus) return 0;
        wchar_t ch = (wchar_t)wParam;
        if (ch >= 32 && ch < 127) {
            gBuffer.insert(gBuffer.begin() + gCaret, ch);
            ++gCaret;
        } else if (ch == L'\r') {
            gBuffer.insert(gBuffer.begin() + gCaret, L'\n');
            ++gCaret;
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        gScrollY -= delta / 4;
        if (gScrollY < 0) gScrollY = 0;
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    if (!EnsureDDriveOnly()) {
        MessageBoxW(nullptr, L"GlassQuill Chameleon must run from D:\\", L"D: only", MB_OK | MB_ICONWARNING);
        return 1;
    }
    
    gApp = hInst;
    gStartTick = GetTickCount();
    
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = gApp;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"GlassQuillChameleon";
    RegisterClassExW(&wc);
    
    DWORD ex = WS_EX_LAYERED;
    DWORD st = WS_OVERLAPPEDWINDOW;
    RECT r = { 0,0,gW,gH };
    AdjustWindowRectEx(&r, st, FALSE, ex);
    
    gWnd = CreateWindowExW(ex, wc.lpszClassName, L"GlassQuill Chameleon IDE", st,
                           50, 50, r.right - r.left, r.bottom - r.top,
                           NULL, NULL, gApp, NULL);
    if (!gWnd) return 2;
    
    MARGINS m = { -1 };
    DwmExtendFrameIntoClientArea(gWnd, &m);
    
    if (!InitGL(gWnd)) return 3;
    
    UpdateWindowAlpha();
    ShowWindow(gWnd, SW_SHOW);
    UpdateWindow(gWnd);
    
    BuildAtlas();
    
    // Initialize with sample code
    gBuffer = L"// GlassQuill Chameleon IDE - Fixed Edition!\n// Press F4 to toggle chameleon effect\n// Ctrl+S to save, Ctrl+O to open\n\n#include <iostream>\n\nint main() {\n    // Watch the colors flow like a chameleon!\n    std::cout << \"Hello, colorful world!\" << std::endl;\n    \n    // Fragment shader keywords:\n    // uniform vec3 resolution;\n    // varying vec2 vTexCoord;\n    \n    return 0;\n}";
    gCaret = gBuffer.size();
    gEditorFocus = true;
    
    MSG msg;
    while (gRunning) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { gRunning = false; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Render();
        Sleep(1);
    }
    
    if (gRC) { wglMakeCurrent(NULL, NULL); wglDeleteContext(gRC); gRC = NULL; }
    if (gDC) { ReleaseDC(gWnd, gDC); gDC = NULL; }
    if (gFontTex) { glDeleteTextures(1, &gFontTex); gFontTex = 0; }
    return 0;
}