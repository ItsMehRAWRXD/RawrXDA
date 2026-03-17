#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shlobj.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <tchar.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <algorithm>
#include <math.h>
#include <sstream>
#include <GL/gl.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ws2_32.lib")

struct V2 { float x, y; };
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

// Core system
static HINSTANCE gApp = nullptr;
static HWND      gWnd = nullptr;
static HDC       gDC  = nullptr;
static HGLRC     gRC  = nullptr;
static int       gW   = 1200;
static int       gH   = 800;
static bool      gRunning = true;

// Chameleon effect
static bool      gChameleonEnabled = true;
static float     gChameleonTime = 0.0f;
static BYTE      gMainOpacity = 200;

// UI Layout
static RECT      gOpacitySlider = { 50, 50, 370, 74 };
static RECT      gTabBar = { 20, 90, 1180, 120 };
static RECT      gEditorRect = { 20, 120, 1180, 780 };
static RECT      gChameleonToggle = { 400, 50, 550, 74 };
static RECT      gDriveBrowser = { 600, 50, 800, 74 };
static bool      gSliderDrag = false;
static bool      gEditorFocus = false;
static bool      gShowDriveBrowser = false;

// Tab system
struct EditorTab {
    std::wstring name;
    std::wstring filePath;
    std::wstring buffer;
    size_t caret;
    int scrollY;
    bool modified;
};

static std::vector<EditorTab> gTabs;
static int gCurrentTab = 0;
static int gTabHover = -1;

// Text editor state (now tab-specific)
static std::wstring gBuffer;
static size_t    gCaret = 0;
static int       gScrollY = 0;
static DWORD     gStartTick = 0;

/* ----------  OLLAMA WRAPPER  ---------- */
class OllamaWrapper {
    static constexpr int BUF_SZ = 64 * 1024;
public:
    OllamaWrapper() { WSADATA w{}; WSAStartup(MAKEWORD(2, 2), &w); }
    ~OllamaWrapper() { WSACleanup(); }

    std::string chat(const std::string& model, const std::string& prompt, int timeoutMs = 8000) {
        std::string json = buildJson(model, prompt);
        std::string hdr  = buildHeader(json.size());

        SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == INVALID_SOCKET) return "[socket err]";

        u_long nonBlk = 1;
        ioctlsocket(s, FIONBIO, &nonBlk);

        sockaddr_in addr{ AF_INET, htons(11434) };
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        connect(s, (sockaddr*)&addr, sizeof(addr));
        fd_set wfd{}, efd{};
        FD_SET(s, &wfd); FD_SET(s, &efd);
        timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
        if (select(int(s) + 1, nullptr, &wfd, &efd, &tv) != 1 || FD_ISSET(s, &efd)) {
            closesocket(s);
            return "[connect err]";
        }

        send(s, hdr.c_str(), (int)hdr.size(), 0);
        send(s, json.c_str(), (int)json.size(), 0);

        std::string body;
        char buf[BUF_SZ];
        int rd;
        do {
            rd = recv(s, buf, sizeof(buf), 0);
            if (rd > 0) body.append(buf, rd);
        } while (rd > 0);

        closesocket(s);
        return extractContent(body);
    }

private:
    std::string buildJson(const std::string& m, const std::string& p) const {
        std::ostringstream o;
        o << R"({"model":")" << m << R"(","prompt":")" << escape(p) << R"(","stream":false})";
        return o.str();
    }
    std::string buildHeader(size_t len) const {
        std::ostringstream o;
        o << "POST /api/generate HTTP/1.1\r\n"
          << "Host: 127.0.0.1:11434\r\n"
          << "Content-Type: application/json\r\n"
          << "Content-Length: " << len << "\r\n"
          << "Connection: close\r\n\r\n";
        return o.str();
    }
    std::string escape(const std::string& s) const {
        std::string r;
        for (char c : s)
            switch (c) {
            case '"': r += "\\\""; break;
            case '\\':r += "\\\\"; break;
            case '\n':r += "\\n";  break;
            case '\r':r += "\\r";  break;
            default:  r += c;
            }
        return r;
    }
    std::string extractContent(const std::string& http) const {
        auto pos = http.rfind("\"response\":\"");
        if (pos == std::string::npos) return "[parse err]";
        pos += 12;
        auto end = http.find("\"", pos);
        if (end == std::string::npos) return "[parse err]";
        std::string raw = http.substr(pos, end - pos);
        size_t off = 0;
        while ((off = raw.find("\\n", off)) != std::string::npos)
            raw.replace(off, 2, "\n");
        return raw;
    }
};

static OllamaWrapper gOllama;
static bool gAIThinking = false;

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
        
        DrawGlyph((int)wc, cx, cy, scale, 0.9f, 0.9f, 0.9f, 1.0f, useChameleon);
        cx += advance;
    }
}

static void UpdateWindowAlpha() {
    SetLayeredWindowAttributes(gWnd, 0, gMainOpacity, LWA_ALPHA);
}

static bool PtInRectI(const RECT& r, int x, int y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

static void Render() {
    gChameleonTime = (GetTickCount() - gStartTick) / 1000.0f;
    
    Ortho2D(gW, gH);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    // Background
    DrawRect(0, 0, (float)gW, (float)gH, 0.08f, 0.09f, 0.12f, 0.65f);

    // Opacity slider
    DrawRect((float)gOpacitySlider.left, (float)gOpacitySlider.top, (float)gOpacitySlider.right, (float)gOpacitySlider.bottom,
             0.15f, 0.17f, 0.19f, 0.7f);
    
    float sliderPos = gMainOpacity / 255.0f;
    float knobX = gOpacitySlider.left + (gOpacitySlider.right - gOpacitySlider.left) * sliderPos;
    float knobY = (gOpacitySlider.top + gOpacitySlider.bottom) * 0.5f;
    DrawCircle(knobX, knobY, 8.0f, 0.9f, 0.95f, 1.0f, 0.95f);

    DrawText(L"Opacity", 10, (float)gOpacitySlider.top - 2, 2.0f, false);

    // Chameleon toggle
    DrawRect((float)gChameleonToggle.left, (float)gChameleonToggle.top, (float)gChameleonToggle.right, (float)gChameleonToggle.bottom,
             gChameleonEnabled ? 0.2f : 0.15f, gChameleonEnabled ? 0.6f : 0.17f, gChameleonEnabled ? 0.3f : 0.19f, 0.8f);
    DrawText(gChameleonEnabled ? L"Chameleon ON" : L"Chameleon OFF", 
             (float)gChameleonToggle.left + 5, (float)gChameleonToggle.top + 2, 2.0f, false);

    // Drive browser button
    DrawRect((float)gDriveBrowser.left, (float)gDriveBrowser.top, (float)gDriveBrowser.right, (float)gDriveBrowser.bottom,
             0.15f, 0.17f, 0.19f, 0.8f);
    DrawText(L"Browse Drives", (float)gDriveBrowser.left + 5, (float)gDriveBrowser.top + 2, 2.0f, false);

    // Tab bar
    DrawRect((float)gTabBar.left, (float)gTabBar.top, (float)gTabBar.right, (float)gTabBar.bottom,
             0.1f, 0.12f, 0.14f, 0.9f);
    
    // Draw tabs
    float tabWidth = 150.0f;
    float tabX = (float)gTabBar.left + 5;
    for (int i = 0; i < (int)gTabs.size(); ++i) {
        bool isActive = (i == gCurrentTab);
        bool isHover = (i == gTabHover);
        
        float r = isActive ? 0.25f : (isHover ? 0.2f : 0.15f);
        float g = isActive ? 0.3f : (isHover ? 0.25f : 0.17f);
        float b = isActive ? 0.35f : (isHover ? 0.3f : 0.19f);
        
        DrawRect(tabX, (float)gTabBar.top + 2, tabX + tabWidth, (float)gTabBar.bottom - 2, r, g, b, 0.9f);
        
        std::wstring tabName = gTabs[i].name;
        if (tabName.length() > 15) {
            tabName = tabName.substr(0, 12) + L"...";
        }
        
        DrawText(tabName, tabX + 5, (float)gTabBar.top + 5, 1.8f, false);
        
        // Close button (X)
        DrawText(L"X", tabX + tabWidth - 20, (float)gTabBar.top + 5, 1.8f, false);
        
        tabX += tabWidth + 5;
    }
    
    // New tab button (+)
    DrawRect(tabX, (float)gTabBar.top + 2, tabX + 30, (float)gTabBar.bottom - 2, 0.2f, 0.4f, 0.2f, 0.9f);
    DrawText(L"+", tabX + 10, (float)gTabBar.top + 5, 2.0f, false);

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

    // Main text with chameleon effect
    DrawText(gBuffer, (float)gEditorRect.left + 60, contentY, 2.5f, gChameleonEnabled, 
             gEditorRect.right - gEditorRect.left - 80);

    // AI working indicator
    if (gAIThinking) {
        DrawText(L"AI working...", (float)gW - 140.0f, 40.0f, 2.0f, false);
    }

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
    swprintf_s(status, L"GlassQuill Chameleon | Pos: %zu | Lines: %zu | Chameleon: %s | F4=Toggle ESC=Exit", 
               gCaret, std::count(gBuffer.begin(), gBuffer.end(), L'\n') + 1,
               gChameleonEnabled ? L"ON" : L"OFF");
    DrawText(status, 20, (float)gH - 30, 2.0f, false);

    SwapBuffers(gDC);
}

// Helper function to convert wide string to narrow string  
static std::string narrow(const std::wstring& wide) {
    if (wide.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &result[0], len, nullptr, nullptr);
    return result;
}

// Recursively copy directory contents
static bool recurseCopy(const std::wstring& src, const std::wstring& dst) {
    CreateDirectoryW(dst.c_str(), nullptr);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((src + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring s = src + L"\\" + fd.cFileName;
        std::wstring d = dst + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            recurseCopy(s, d);
        else
            CopyFileW(s.c_str(), d.c_str(), FALSE);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return true;
}

// Install extension from marketplace
static void installExtension(const std::wstring& folder) {
    std::wstring name = folder.substr(folder.find_last_of(L"\\") + 1);
    std::wstring dest = L"D:\\cursor-multi-ai-extension\\extensions\\" + name;
    CreateDirectoryW(L"D:\\cursor-multi-ai-extension\\extensions", nullptr);
    
    if (recurseCopy(folder, dest)) {
        // Try to run install script if present
        std::wstring bat = dest + L"\\install.bat";
        std::wstring py = dest + L"\\install.py";
        
        if (GetFileAttributesW(bat.c_str()) != INVALID_FILE_ATTRIBUTES) {
            WinExec(("cmd /c \"" + narrow(bat) + "\"").c_str(), SW_HIDE);
        } else if (GetFileAttributesW(py.c_str()) != INVALID_FILE_ATTRIBUTES) {
            WinExec(("python \"" + narrow(py) + "\"").c_str(), SW_HIDE);
        }
        
        MessageBoxW(gWnd, (L"Extension '" + name + L"' installed successfully!").c_str(), 
                    L"Extension Installed", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(gWnd, L"Failed to copy extension files", L"Installation Error", MB_OK | MB_ICONERROR);
    }
}

// Tab management functions
static void saveCurrentTab() {
    if (gCurrentTab >= 0 && gCurrentTab < (int)gTabs.size()) {
        gTabs[gCurrentTab].buffer = gBuffer;
        gTabs[gCurrentTab].caret = gCaret;
        gTabs[gCurrentTab].scrollY = gScrollY;
    }
}

static void loadTab(int tabIndex) {
    if (tabIndex >= 0 && tabIndex < (int)gTabs.size()) {
        saveCurrentTab();
        gCurrentTab = tabIndex;
        gBuffer = gTabs[tabIndex].buffer;
        gCaret = gTabs[tabIndex].caret;
        gScrollY = gTabs[tabIndex].scrollY;
        gEditorFocus = true;
    }
}

static void createNewTab(const std::wstring& name = L"New File", const std::wstring& filePath = L"") {
    saveCurrentTab();
    EditorTab newTab;
    newTab.name = name;
    newTab.filePath = filePath;
    newTab.buffer = L"";
    newTab.caret = 0;
    newTab.scrollY = 0;
    newTab.modified = false;
    gTabs.push_back(newTab);
    gCurrentTab = (int)gTabs.size() - 1;
    loadTab(gCurrentTab);
}

static void closeTab(int tabIndex) {
    if (tabIndex >= 0 && tabIndex < (int)gTabs.size()) {
        gTabs.erase(gTabs.begin() + tabIndex);
        if (gTabs.empty()) {
            createNewTab();
        } else {
            if (gCurrentTab >= tabIndex && gCurrentTab > 0) {
                gCurrentTab--;
            }
            if (gCurrentTab >= (int)gTabs.size()) {
                gCurrentTab = (int)gTabs.size() - 1;
            }
            loadTab(gCurrentTab);
        }
    }
}

static void loadFileIntoTab(const std::wstring& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (file.good()) {
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        std::wstring wcontent(content.begin(), content.end());
        
        std::wstring fileName = filePath.substr(filePath.find_last_of(L"\\") + 1);
        createNewTab(fileName, filePath);
        gBuffer = wcontent;
        gTabs[gCurrentTab].buffer = wcontent;
        gCaret = 0;
        gScrollY = 0;
    }
}

// Drive browser functions
static void browseDrives() {
    wchar_t drives[256];
    DWORD len = GetLogicalDriveStringsW(256, drives);
    
    std::wstring driveList = L"Available Drives:\\n";
    wchar_t* drive = drives;
    while (*drive) {
        driveList += drive;
        driveList += L"\\n";
        drive += wcslen(drive) + 1;
    }
    
    createNewTab(L"Drives", L"");
    gBuffer = driveList;
    gTabs[gCurrentTab].buffer = driveList;
    gCaret = 0;
    gScrollY = 0;
}

static void browseFolder() {
    wchar_t folderPath[MAX_PATH] = L"C:\\";
    BROWSEINFOW bi{0};
    bi.lpszTitle = L"Browse folders and files";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.hwndOwner = gWnd;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    
    if (pidl && SHGetPathFromIDListW(pidl, folderPath)) {
        // List folder contents
        std::wstring content = L"Folder: " + std::wstring(folderPath) + L"\\n\\n";
        
        WIN32_FIND_DATAW findData;
        std::wstring searchPath = std::wstring(folderPath) + L"\\*";
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0) {
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        content += L"[DIR]  " + std::wstring(findData.cFileName) + L"\\n";
                    } else {
                        content += L"[FILE] " + std::wstring(findData.cFileName) + L"\\n";
                    }
                }
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
        
        std::wstring tabName = std::wstring(folderPath);
        size_t lastSlash = tabName.find_last_of(L'\\');
        if (lastSlash != std::wstring::npos) {
            tabName = tabName.substr(lastSlash + 1);
        }
        if (tabName.empty()) tabName = folderPath;
        
        createNewTab(tabName, folderPath);
        gBuffer = content;
        gTabs[gCurrentTab].buffer = content;
        gCaret = 0;
        gScrollY = 0;
    }
    
    if (pidl) CoTaskMemFree(pidl);
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
        } else if (PtInRectI(gDriveBrowser, mx, my)) {
            browseFolder();
        } else if (PtInRectI(gTabBar, mx, my)) {
            // Handle tab clicks
            float tabWidth = 150.0f;
            float tabX = (float)gTabBar.left + 5;
            
            for (int i = 0; i < (int)gTabs.size(); ++i) {
                // Check if clicking in this tab
                if (mx >= tabX && mx <= tabX + tabWidth) {
                    // Check if clicking close button (X)
                    if (mx >= tabX + tabWidth - 25) {
                        closeTab(i);
                    } else {
                        loadTab(i);
                    }
                    break;
                }
                tabX += tabWidth + 5;
            }
            
            // Check if clicking new tab button (+)
            if (mx >= tabX && mx <= tabX + 30) {
                createNewTab();
            }
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
        
        // Ctrl+M: Open marketplace browser
        if (wParam == 'M' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            wchar_t mp[MAX_PATH] = L"D:\\cursor-multi-ai-extension\\marketplace";
            BROWSEINFOW bi{0};
            bi.lpszTitle = L"Pick extension folder (inside marketplace)";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            bi.hwndOwner = hWnd;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl && SHGetPathFromIDListW(pidl, mp)) {
                installExtension(mp);
            }
            if (pidl) CoTaskMemFree(pidl);
            return 0;
        }
        
        // Ctrl+T: New tab
        if (wParam == 'T' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            createNewTab();
            return 0;
        }
        
        // Ctrl+W: Close current tab
        if (wParam == 'W' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            closeTab(gCurrentTab);
            return 0;
        }
        
        // Ctrl+D: Browse drives
        if (wParam == 'D' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            browseDrives();
            return 0;
        }
        
        // Ctrl+O: Browse folders
        if (wParam == 'O' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            browseFolder();
            return 0;
        }
        
        // Ctrl+Tab: Next tab
        if (wParam == VK_TAB && (GetKeyState(VK_CONTROL) & 0x8000)) {
            if (!gTabs.empty()) {
                int nextTab = (gCurrentTab + 1) % (int)gTabs.size();
                loadTab(nextTab);
            }
            return 0;
        }
        
        if (!gEditorFocus) return 0;
        
        if (wParam == VK_BACK) {
            if (gCaret > 0 && !gBuffer.empty()) {
                gBuffer.erase(gBuffer.begin() + (gCaret - 1));
                --gCaret;
            }
            return 0;
        }
        
        // Ctrl+Space: AI code completion
        if (wParam == VK_SPACE && (GetKeyState(VK_CONTROL) & 0x8000)) {
            if (gAIThinking) return 0;
            gAIThinking = true;
            InvalidateRect(hWnd, nullptr, FALSE);          // show "thinking"

            std::string utf8;
            for (wchar_t wc : gBuffer) if (wc < 128) utf8 += char(wc);

            std::string prompt = "Complete this code (output only the completion, no explanation):\n" + utf8;
            std::string result = gOllama.chat("qwen2.5-coder:7b", prompt);

            for (char c : result) {
                gBuffer.insert(gBuffer.begin() + gCaret, (wchar_t)c);
                ++gCaret;
            }

            gAIThinking = false;
            InvalidateRect(hWnd, nullptr, FALSE);
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
    
    // Initialize tab system with sample code
    createNewTab(L"Welcome.cpp", L"");
    gBuffer = L"// GlassQuill Multi-Tab IDE with Drive Browser!\n// NEW FEATURES:\n// • Multi-tab editing (Ctrl+T for new, Ctrl+W to close)\n// • Drive browser (Ctrl+D for drives, Ctrl+O for folders)\n// • Marketplace extensions (Ctrl+M)\n// • AI coding assistant (Ctrl+Space)\n// • Chameleon effects (F4)\n\n#include <iostream>\n\nint main() {\n    // Multi-tab IDE with full drive access!\n    std::cout << \"Welcome to enhanced GlassQuill!\" << std::endl;\n    \n    // Try:\n    // Ctrl+T = New tab\n    // Ctrl+D = Browse drives (C:, D:, etc.)\n    // Ctrl+O = Browse folders and files\n    // Click tabs to switch, X to close\n    \n    return 0;\n}";
    gTabs[0].buffer = gBuffer;
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