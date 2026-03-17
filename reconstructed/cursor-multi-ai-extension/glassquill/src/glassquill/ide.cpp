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
#include <GL/gl.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "dwmapi.lib")

struct V2 { float x, y; };
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

// Core window state
static HINSTANCE gApp = nullptr;
static HWND      gWnd = nullptr;
static HDC       gDC  = nullptr;
static HGLRC     gRC  = nullptr;
static int       gW   = 1200;
static int       gH   = 800;
static bool      gRunning = true;

// IDE Tab System
enum TabType {
    TAB_EDITOR = 0,
    TAB_SETTINGS = 1,
    TAB_FILES = 2,
    TAB_DEBUG = 3,
    TAB_COUNT = 4
};

static TabType   gCurrentTab = TAB_EDITOR;
static RECT      gTabArea = { 10, 10, 1190, 50 };
static RECT      gContentArea = { 10, 60, 1190, 790 };

// Transparency & RGBA Settings
static BYTE      gMainOpacity = 210;
static float     gMainR = 0.08f;
static float     gMainG = 0.09f;
static float     gMainB = 0.10f;
static float     gMainA = 0.55f;
static bool      gUseOpacityMode = true;  // Toggle between opacity and RGBA

// UI Controls
static RECT      gOpacitySlider = { 50, 100, 370, 124 };
static RECT      gRSlider = { 50, 140, 370, 164 };
static RECT      gGSlider = { 50, 180, 370, 194 };
static RECT      gBSlider = { 50, 220, 370, 244 };
static RECT      gASlider = { 50, 260, 370, 284 };
static RECT      gModeToggle = { 400, 120, 500, 150 };
static bool      gSliderDrag[5] = {false}; // opacity, r, g, b, a
static int       gActiveDrag = -1;

// Text Editor State
static RECT      gEditorRect = { 20, 100, 1170, 770 };
static bool      gEditorFocus = false;
static std::wstring gBuffer;
static size_t    gCaret = 0;
static DWORD     gStartTick = 0;
static int       gScrollY = 0;

// File Management
static std::vector<std::wstring> gOpenFiles;
static int       gCurrentFile = 0;
static std::wstring gCurrentPath = L"";

// Font Atlas
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

static bool InitGL(HWND hWnd) {
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = 32;
    pfd.cAlphaBits   = 8;
    pfd.cDepthBits   = 0;
    pfd.iLayerType   = PFD_MAIN_PLANE;

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
    {0,0,0,0,0,0,0,0},
    {0x04,0x04,0x04,0x04,0x04,0,0x04,0},
    {0x0A,0x0A,0x0A,0,0,0,0,0},
    {0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0,0},
    {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04,0},
    {0x19,0x19,0x02,0x04,0x08,0x13,0x13,0},
    {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D,0},
    {0x06,0x04,0x08,0,0,0,0,0},
    {0x06,0x08,0x10,0x10,0x10,0x08,0x06,0},
    {0x06,0x02,0x01,0x01,0x01,0x02,0x06,0},
    {0x00,0x04,0x15,0x0E,0x15,0x04,0x00,0},
    {0x00,0x04,0x04,0x1F,0x04,0x04,0x00,0},
    {0,0,0,0,0x06,0x04,0x08,0},
    {0,0,0,0x1F,0,0,0,0},
    {0,0,0,0,0,0x06,0x06,0},
    {0x01,0x02,0x04,0x08,0x10,0,0,0},
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E,0},
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E,0},
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F,0},
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E,0},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02,0},
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E,0},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E,0},
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08,0},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E,0},
    {0x0E,0x11,0x11,0x0F,0x01,0x11,0x0E,0},
    {0,0x06,0x06,0,0x06,0x06,0,0},
    {0,0x06,0x06,0,0x06,0x04,0x08,0},
    {0x02,0x04,0x08,0x10,0x08,0x04,0x02,0},
    {0,0,0x1F,0,0x1F,0,0,0},
    {0x08,0x04,0x02,0x01,0x02,0x04,0x08,0},
    {0x0E,0x11,0x01,0x02,0x04,0,0x04,0},
    {0x0E,0x11,0x15,0x15,0x1D,0x10,0x0E,0},
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11,0},
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E,0},
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E,0},
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E,0},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F,0},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10,0},
    {0x0F,0x10,0x10,0x17,0x11,0x11,0x0F,0},
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11,0},
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E,0},
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C,0},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11,0},
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F,0},
    {0x11,0x1B,0x15,0x11,0x11,0x11,0x11,0},
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11,0},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E,0},
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10,0},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D,0},
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11,0},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E,0},
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04,0},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E,0},
    {0x11,0x11,0x11,0x0A,0x0A,0x04,0x04,0},
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11,0},
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11,0},
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04,0},
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F,0},
    {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E,0},
    {0x10,0x08,0x04,0x02,0x01,0,0,0},
    {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E,0},
    {0x04,0x0A,0x11,0,0,0,0,0},
    {0,0,0,0,0,0,0x1F,0},
    {0x08,0x04,0,0,0,0,0,0},
    {0,0,0x0E,0x01,0x0F,0x11,0x0F,0},
    {0x10,0x10,0x1E,0x11,0x11,0x11,0x1E,0},
    {0,0,0x0E,0x10,0x10,0x10,0x0E,0},
    {0x01,0x01,0x0F,0x11,0x11,0x11,0x0F,0},
    {0,0,0x0E,0x11,0x1F,0x10,0x0E,0},
    {0x06,0x08,0x1E,0x08,0x08,0x08,0x08,0},
    {0,0,0x0F,0x11,0x11,0x0F,0x01,0x0E},
    {0x10,0x10,0x1E,0x11,0x11,0x11,0x11,0},
    {0x04,0,0x0C,0x04,0x04,0x04,0x0E,0},
    {0x02,0,0x06,0x02,0x02,0x12,0x0C,0},
    {0x10,0x10,0x12,0x14,0x18,0x14,0x12,0},
    {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E,0},
    {0,0,0x1A,0x15,0x15,0x11,0x11,0},
    {0,0,0x1E,0x11,0x11,0x11,0x11,0},
    {0,0,0x0E,0x11,0x11,0x11,0x0E,0},
    {0,0,0x1E,0x11,0x11,0x1E,0x10,0x10},
    {0,0,0x0F,0x11,0x11,0x0F,0x01,0x01},
    {0,0,0x16,0x19,0x10,0x10,0x10,0},
    {0,0,0x0F,0x10,0x0E,0x01,0x1E,0},
    {0x08,0x08,0x1E,0x08,0x08,0x08,0x06,0},
    {0,0,0x11,0x11,0x11,0x11,0x0F,0},
    {0,0,0x11,0x11,0x0A,0x0A,0x04,0},
    {0,0,0x11,0x11,0x15,0x1B,0x11,0},
    {0,0,0x11,0x0A,0x04,0x0A,0x11,0},
    {0,0,0x11,0x11,0x0F,0x01,0x0E,0},
    {0,0,0x1F,0x02,0x04,0x08,0x1F,0},
    {0x06,0x08,0x08,0x10,0x08,0x08,0x06,0},
    {0x04,0x04,0x04,0x04,0x04,0x04,0x04,0},
    {0x0C,0x02,0x02,0x01,0x02,0x02,0x0C,0},
    {0x00,0x09,0x16,0,0,0,0,0},
    {0,0,0,0,0,0,0,0}
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
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x0, y0);
    glVertex2f(x1, y0);
    glVertex2f(x1, y1);
    glVertex2f(x0, y1);
    glEnd();
}

static void DrawCircle(float cx, float cy, float radius, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
    for (int i = 0; i <= 48; ++i) {
        float t = 6.2831853f * (float)i / 48.0f;
        glVertex2f(cx + radius * cosf(t), cy + radius * sinf(t));
    }
    glEnd();
}

static void DrawGlyph(int code, float x, float y, float scale, float r, float g, float b, float a) {
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

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, gFontTex);
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(u0, v0); glVertex2f(x,     y);
    glTexCoord2f(u1, v0); glVertex2f(x + w, y);
    glTexCoord2f(u1, v1); glVertex2f(x + w, y + h);
    glTexCoord2f(u0, v1); glVertex2f(x,     y + h);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

static void DrawText(const std::wstring& s, float x, float y, float scale, float r, float g, float b, float a, int wrapW = -1) {
    float advance = (gGlyphW + 1) * scale;
    float lineH = (gGlyphH + 2) * scale;
    float cx = x, cy = y;
    for (wchar_t wc : s) {
        if (wc == L'\n') {
            cx = x;
            cy += lineH;
            continue;
        }
        if (wrapW > 0 && cx + advance > x + wrapW) {
            cx = x;
            cy += lineH;
        }
        DrawGlyph((int)wc, cx, cy, scale, r, g, b, a);
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

static void DrawSlider(const RECT& rect, float value, const wchar_t* label, float r, float g, float b) {
    // Background track
    DrawRect((float)rect.left, (float)rect.top, (float)rect.right, (float)rect.bottom,
             0.15f, 0.17f, 0.19f, 0.7f);
    
    // Value fill
    float fillWidth = (rect.right - rect.left) * value;
    DrawRect((float)rect.left, (float)rect.top, (float)rect.left + fillWidth, (float)rect.bottom,
             r, g, b, 0.8f);
    
    // Knob
    float knobX = rect.left + (rect.right - rect.left) * value;
    float knobY = (rect.top + rect.bottom) * 0.5f;
    DrawCircle(knobX, knobY, 8.0f, 0.9f, 0.95f, 1.0f, 0.95f);
    DrawCircle(knobX, knobY, 5.0f, 0.2f, 0.25f, 0.3f, 0.95f);
    
    // Label
    DrawText(label, (float)rect.left - 40, (float)rect.top - 2, 2.0f, 0.85f, 0.9f, 0.95f, 1.0f);
    
    // Value text
    wchar_t valueText[32];
    swprintf_s(valueText, L"%.3f", value);
    DrawText(valueText, (float)rect.right + 10, (float)rect.top - 2, 2.0f, r, g, b, 1.0f);
}

static void DrawTab(const wchar_t* label, int index, float x, float y, float w, float h) {
    bool active = (gCurrentTab == index);
    float r = active ? 0.3f : 0.15f;
    float g = active ? 0.35f : 0.17f;
    float b = active ? 0.4f : 0.19f;
    float a = active ? 0.9f : 0.7f;
    
    DrawRect(x, y, x + w, y + h, r, g, b, a);
    
    // Border
    if (active) {
        DrawRect(x, y + h - 2, x + w, y + h, 0.5f, 0.7f, 0.9f, 0.8f);
    }
    
    DrawText(label, x + 8, y + 8, 2.5f, 0.9f, 0.95f, 1.0f, 1.0f);
}

static void DrawToggle(const RECT& rect, bool state, const wchar_t* label) {
    float r = state ? 0.2f : 0.15f;
    float g = state ? 0.6f : 0.17f;
    float b = state ? 0.3f : 0.19f;
    
    DrawRect((float)rect.left, (float)rect.top, (float)rect.right, (float)rect.bottom, r, g, b, 0.8f);
    
    // Checkmark or X
    if (state) {
        DrawText(L"✓", (float)rect.left + 5, (float)rect.top + 2, 2.0f, 0.9f, 1.0f, 0.9f, 1.0f);
    } else {
        DrawText(L"✗", (float)rect.left + 5, (float)rect.top + 2, 2.0f, 0.7f, 0.3f, 0.3f, 1.0f);
    }
    
    DrawText(label, (float)rect.right + 10, (float)rect.top + 2, 2.5f, 0.85f, 0.9f, 0.95f, 1.0f);
}

static void RenderEditor() {
    // Main editor background
    float bgR = gUseOpacityMode ? gMainR : gMainR;
    float bgG = gUseOpacityMode ? gMainG : gMainG;
    float bgB = gUseOpacityMode ? gMainB : gMainB;
    float bgA = gUseOpacityMode ? gMainA : gMainA;
    
    DrawRect((float)gEditorRect.left, (float)gEditorRect.top, (float)gEditorRect.right, (float)gEditorRect.bottom,
             bgR, bgG, bgB, bgA);
    
    // Line numbers background
    DrawRect((float)gEditorRect.left, (float)gEditorRect.top, (float)gEditorRect.left + 50, (float)gEditorRect.bottom,
             bgR + 0.05f, bgG + 0.05f, bgB + 0.05f, bgA + 0.1f);
    
    // Content text
    float textY = (float)gEditorRect.top + 10 - gScrollY;
    
    // Line numbers
    for (int line = 0; line < 50; ++line) {
        wchar_t lineNum[8];
        swprintf_s(lineNum, L"%d", line + 1);
        DrawText(lineNum, (float)gEditorRect.left + 5, textY + line * 24, 2.0f, 0.5f, 0.6f, 0.7f, 0.8f);
    }
    
    // Main content
    DrawText(gBuffer, (float)gEditorRect.left + 60, textY, 2.5f, 0.95f, 0.97f, 1.0f, 1.0f, gEditorRect.right - gEditorRect.left - 80);
    
    // Caret
    DWORD ticks = GetTickCount() - gStartTick;
    bool caretVis = ((ticks / 500) % 2) == 0 && gEditorFocus;
    if (caretVis) {
        float scale = 2.5f;
        float advance = (gGlyphW + 1) * scale;
        float lineH = (gGlyphH + 2) * scale;
        float x0 = (float)gEditorRect.left + 60;
        float y0 = textY;
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
}

static void RenderSettings() {
    DrawText(L"GlassQuill IDE - Transparency & RGBA Settings", 50, 80, 3.0f, 0.85f, 0.9f, 0.95f, 1.0f);
    
    // Mode toggle
    DrawToggle(gModeToggle, gUseOpacityMode, L"Opacity Mode (vs RGBA)");
    
    if (gUseOpacityMode) {
        // Opacity slider only
        DrawSlider(gOpacitySlider, gMainOpacity / 255.0f, L"Opacity", 0.6f, 0.7f, 0.9f);
    } else {
        // Full RGBA sliders
        DrawSlider(gRSlider, gMainR, L"Red", 0.8f, 0.2f, 0.2f);
        DrawSlider(gGSlider, gMainG, L"Green", 0.2f, 0.8f, 0.2f);
        DrawSlider(gBSlider, gMainB, L"Blue", 0.2f, 0.2f, 0.8f);
        DrawSlider(gASlider, gMainA, L"Alpha", 0.6f, 0.6f, 0.6f);
    }
    
    // Instructions
    DrawText(L"Press F2 for topmost toggle\nPress F3 for click-through\nPress Ctrl+S to save\nPress Ctrl+O to open file",
             50, 320, 2.5f, 0.7f, 0.8f, 0.9f, 0.9f);
    
    // Current values display
    wchar_t info[512];
    swprintf_s(info, L"Current Settings:\nOpacity: %d/255\nRGBA: (%.3f, %.3f, %.3f, %.3f)\nMode: %s\nFile: %s",
               gMainOpacity, gMainR, gMainG, gMainB, gMainA,
               gUseOpacityMode ? L"Opacity" : L"RGBA",
               gCurrentPath.empty() ? L"<untitled>" : gCurrentPath.c_str());
    DrawText(info, 550, 120, 2.0f, 0.8f, 0.85f, 0.9f, 0.95f);
}

static void RenderFiles() {
    DrawText(L"File Browser", 50, 80, 3.0f, 0.85f, 0.9f, 0.95f, 1.0f);
    
    // File list (mock for now)
    const wchar_t* files[] = {
        L"glassquill.cpp", L"shader.vert", L"shader.frag", L"build.bat", L"README.md"
    };
    
    for (int i = 0; i < 5; ++i) {
        bool selected = (i == gCurrentFile);
        if (selected) {
            DrawRect(50, 120 + i * 30, 400, 145 + i * 30, 0.2f, 0.3f, 0.5f, 0.6f);
        }
        DrawText(files[i], 60, 125 + i * 30, 2.5f, selected ? 1.0f : 0.8f, selected ? 1.0f : 0.85f, selected ? 1.0f : 0.9f, 1.0f);
    }
    
    DrawText(L"Recent Files:\n- glassquill_old.cpp\n- settings.json\n- project.txt",
             450, 120, 2.0f, 0.6f, 0.7f, 0.8f, 0.9f);
}

static void RenderDebug() {
    DrawText(L"Debug Console", 50, 80, 3.0f, 0.85f, 0.9f, 0.95f, 1.0f);
    
    // Debug info
    wchar_t debug[1024];
    swprintf_s(debug, L"Window Size: %d x %d\nBuffer Length: %zu\nCaret Position: %zu\nCurrent Tab: %d\nEditor Focus: %s\nScroll Y: %d\nFrame Time: %dms",
               gW, gH, gBuffer.size(), gCaret, (int)gCurrentTab, gEditorFocus ? L"Yes" : L"No", gScrollY, GetTickCount() - gStartTick);
    
    DrawText(debug, 50, 120, 2.0f, 0.7f, 0.8f, 0.9f, 0.95f);
    
    // Performance meters
    DrawText(L"Performance:", 50, 300, 2.5f, 0.8f, 0.9f, 1.0f, 1.0f);
    DrawRect(50, 330, 350, 350, 0.1f, 0.5f, 0.1f, 0.8f);  // GPU usage bar
    DrawRect(50, 360, 280, 380, 0.5f, 0.3f, 0.1f, 0.8f);  // Memory usage bar
    
    DrawText(L"GPU: 75%", 360, 332, 2.0f, 0.7f, 0.8f, 0.9f, 1.0f);
    DrawText(L"Memory: 60%", 360, 362, 2.0f, 0.7f, 0.8f, 0.9f, 1.0f);
}

static void Render() {
    Ortho2D(gW, gH);
    
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Tab bar
    DrawRect((float)gTabArea.left, (float)gTabArea.top, (float)gTabArea.right, (float)gTabArea.bottom,
             0.12f, 0.14f, 0.16f, 0.8f);
    
    float tabW = 150.0f;
    DrawTab(L"Editor", TAB_EDITOR, 20, 15, tabW, 30);
    DrawTab(L"Settings", TAB_SETTINGS, 180, 15, tabW, 30);
    DrawTab(L"Files", TAB_FILES, 340, 15, tabW, 30);
    DrawTab(L"Debug", TAB_DEBUG, 500, 15, tabW, 30);
    
    // Content area background
    DrawRect((float)gContentArea.left, (float)gContentArea.top, (float)gContentArea.right, (float)gContentArea.bottom,
             gMainR * 0.5f, gMainG * 0.5f, gMainB * 0.5f, gMainA * 0.5f);
    
    // Render current tab content
    switch (gCurrentTab) {
        case TAB_EDITOR: RenderEditor(); break;
        case TAB_SETTINGS: RenderSettings(); break;
        case TAB_FILES: RenderFiles(); break;
        case TAB_DEBUG: RenderDebug(); break;
    }
    
    SwapBuffers(gDC);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE: {
        gW = LOWORD(lParam);
        gH = HIWORD(lParam);
        
        // Adjust layout for new size
        gTabArea = { 10, 10, gW - 10, 50 };
        gContentArea = { 10, 60, gW - 10, gH - 10 };
        gEditorRect = { 20, 100, gW - 30, gH - 20 };
        
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lParam);
        int my = GET_Y_LPARAM(lParam);
        
        // Tab clicking
        if (PtInRectI(gTabArea, mx, my)) {
            int tabIndex = (mx - 20) / 150;
            if (tabIndex >= 0 && tabIndex < TAB_COUNT) {
                gCurrentTab = (TabType)tabIndex;
            }
        }
        
        // Settings tab controls
        if (gCurrentTab == TAB_SETTINGS) {
            if (PtInRectI(gModeToggle, mx, my)) {
                gUseOpacityMode = !gUseOpacityMode;
            }
            
            // Slider controls
            RECT sliders[] = { gOpacitySlider, gRSlider, gGSlider, gBSlider, gASlider };
            for (int i = 0; i < 5; ++i) {
                if (PtInRectI(sliders[i], mx, my)) {
                    gActiveDrag = i;
                    gSliderDrag[i] = true;
                    SetCapture(hWnd);
                    
                    float t = (mx - sliders[i].left) / (float)(sliders[i].right - sliders[i].left);
                    t = clampf(t, 0.0f, 1.0f);
                    
                    switch (i) {
                        case 0: gMainOpacity = (BYTE)(t * 255); UpdateWindowAlpha(); break;
                        case 1: gMainR = t; break;
                        case 2: gMainG = t; break;
                        case 3: gMainB = t; break;
                        case 4: gMainA = t; break;
                    }
                    break;
                }
            }
        }
        
        // Editor area
        if (gCurrentTab == TAB_EDITOR && PtInRectI(gEditorRect, mx, my)) {
            gEditorFocus = true;
            gCaret = gBuffer.size();
        } else if (gCurrentTab == TAB_EDITOR) {
            gEditorFocus = false;
        }
        
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (gActiveDrag >= 0) {
            int mx = GET_X_LPARAM(lParam);
            RECT sliders[] = { gOpacitySlider, gRSlider, gGSlider, gBSlider, gASlider };
            
            float t = (mx - sliders[gActiveDrag].left) / (float)(sliders[gActiveDrag].right - sliders[gActiveDrag].left);
            t = clampf(t, 0.0f, 1.0f);
            
            switch (gActiveDrag) {
                case 0: gMainOpacity = (BYTE)(t * 255); UpdateWindowAlpha(); break;
                case 1: gMainR = t; break;
                case 2: gMainG = t; break;
                case 3: gMainB = t; break;
                case 4: gMainA = t; break;
            }
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (gActiveDrag >= 0) {
            gSliderDrag[gActiveDrag] = false;
            gActiveDrag = -1;
            ReleaseCapture();
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        if (wParam == VK_F1) {
            gCurrentTab = TAB_EDITOR;
            return 0;
        }
        if (wParam == VK_F2) {
            gCurrentTab = TAB_SETTINGS;
            return 0;
        }
        if (wParam == VK_F9) {
            gCurrentTab = TAB_FILES;
            return 0;
        }
        if (wParam == VK_F12) {
            gCurrentTab = TAB_DEBUG;
            return 0;
        }
        
        // Editor controls
        if (gCurrentTab == TAB_EDITOR && gEditorFocus) {
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
        }
        return 0;
    }
    case WM_CHAR: {
        if (gCurrentTab == TAB_EDITOR && gEditorFocus) {
            wchar_t ch = (wchar_t)wParam;
            if (ch >= 32 && ch < 127) {
                gBuffer.insert(gBuffer.begin() + gCaret, ch);
                ++gCaret;
            } else if (ch == L'\r') {
                gBuffer.insert(gBuffer.begin() + gCaret, L'\n');
                ++gCaret;
            }
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (gCurrentTab == TAB_EDITOR) {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            gScrollY -= delta / 4;
            if (gScrollY < 0) gScrollY = 0;
        }
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
        MessageBoxW(nullptr, L"GlassQuill IDE must run from D:\\", L"D: only", MB_OK | MB_ICONWARNING);
        return 1;
    }
    
    gApp = hInst;
    gStartTick = GetTickCount();
    
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = gApp;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"GlassQuillIDE";
    RegisterClassExW(&wc);
    
    DWORD ex = WS_EX_LAYERED;
    DWORD st = WS_OVERLAPPEDWINDOW;
    RECT r = { 0,0,gW,gH };
    AdjustWindowRectEx(&r, st, FALSE, ex);
    
    gWnd = CreateWindowExW(ex, wc.lpszClassName, L"GlassQuill IDE", st,
                           100, 100, r.right - r.left, r.bottom - r.top,
                           NULL, NULL, gApp, NULL);
    if (!gWnd) return 2;
    
    MARGINS m = { -1 };
    DwmExtendFrameIntoClientArea(gWnd, &m);
    
    if (!InitGL(gWnd)) return 3;
    
    UpdateWindowAlpha();
    ShowWindow(gWnd, SW_SHOW);
    UpdateWindow(gWnd);
    
    BuildAtlas();
    
    // Initialize with some sample text
    gBuffer = L"// GlassQuill IDE - Your transparent code editor\n// Press F1=Editor, F2=Settings, F9=Files, F12=Debug\n\nint main() {\n    return 0;\n}";
    gCaret = gBuffer.size();
    
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