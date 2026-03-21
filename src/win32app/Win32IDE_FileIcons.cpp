// ============================================================================
// Win32IDE_FileIcons.cpp — Tier 1 Cosmetic #7: File Icon Theme Support
// ============================================================================
// Loads file extension → icon mappings for the sidebar file tree.
// Supports Seti and Material icon themes with programmatic color-coded
// icons generated at runtime (no external .ico files required).
//
// Pattern:  ImageList + GDI icon generation, no exceptions
// Threading: UI thread only
// ============================================================================

#include "Win32IDE.h"
#include <sstream>
#include <algorithm>

// ============================================================================
// FILE EXTENSION → COLOR/ICON MAPPING (Seti Theme)
// ============================================================================

struct FileIconDef {
    const char* extension;
    COLORREF color;
    const char* glyph;  // 1-2 char abbreviation drawn in icon
};

static const FileIconDef SETI_ICONS[] = {
    // Source code
    {"cpp",    RGB(0, 136, 204),   "C+"},
    {"c",      RGB(85, 85, 255),   "C"},
    {"h",      RGB(160, 100, 200), "H"},
    {"hpp",    RGB(160, 100, 200), "H+"},
    {"cs",     RGB(80, 160, 0),    "C#"},
    {"java",   RGB(204, 102, 51),  "J"},
    {"py",     RGB(55, 118, 171),  "Py"},
    {"js",     RGB(228, 208, 10),  "JS"},
    {"ts",     RGB(0, 122, 204),   "TS"},
    {"jsx",    RGB(97, 218, 251),  "JX"},
    {"tsx",    RGB(0, 122, 204),   "TX"},
    {"go",     RGB(0, 173, 216),   "Go"},
    {"rs",     RGB(222, 165, 132), "Rs"},
    {"rb",     RGB(204, 52, 45),   "Rb"},
    {"php",    RGB(119, 123, 180), "<?"},
    {"swift",  RGB(240, 81, 56),   "Sw"},
    {"kt",     RGB(169, 123, 255), "Kt"},
    {"scala",  RGB(222, 52, 43),   "Sc"},
    {"lua",    RGB(0, 0, 128),     "Lu"},
    {"r",      RGB(25, 108, 189),  "R"},
    {"asm",    RGB(200, 50, 50),   "As"},
    {"s",      RGB(200, 50, 50),   "As"},

    // Web
    {"html",   RGB(227, 76, 38),   "<>"},
    {"htm",    RGB(227, 76, 38),   "<>"},
    {"css",    RGB(86, 61, 124),   "#"},
    {"scss",   RGB(205, 103, 153), "S"},
    {"less",   RGB(29, 54, 93),    "L"},
    {"vue",    RGB(65, 184, 131),  "V"},
    {"svelte", RGB(255, 62, 0),    "Sv"},

    // Data / Config
    {"json",   RGB(203, 203, 65),  "{}"},
    {"xml",    RGB(227, 76, 38),   "XM"},
    {"yaml",   RGB(203, 75, 22),   "YA"},
    {"yml",    RGB(203, 75, 22),   "YA"},
    {"toml",   RGB(156, 68, 30),   "TM"},
    {"ini",    RGB(128, 128, 128), "IN"},
    {"cfg",    RGB(128, 128, 128), "CF"},
    {"env",    RGB(234, 210, 51),  "EN"},

    // Markup & Docs
    {"md",     RGB(83, 141, 213),  "MD"},
    {"txt",    RGB(180, 180, 180), "TX"},
    {"rst",    RGB(80, 80, 200),   "RS"},
    {"tex",    RGB(0, 128, 0),     "TX"},
    {"pdf",    RGB(200, 50, 50),   "PD"},

    // Shell / Scripts
    {"sh",     RGB(137, 224, 81),  "$"},
    {"bash",   RGB(137, 224, 81),  "$"},
    {"zsh",    RGB(137, 224, 81),  "$"},
    {"ps1",    RGB(1, 36, 86),     ">_"},
    {"psm1",   RGB(1, 36, 86),     ">_"},
    {"bat",    RGB(1, 130, 64),    "BA"},
    {"cmd",    RGB(1, 130, 64),    "CM"},

    // Build / Package
    {"cmake",  RGB(6, 152, 239),   "CM"},
    {"makefile", RGB(200, 80, 50), "MK"},
    {"dockerfile", RGB(56, 130, 202), "DK"},
    {"sln",    RGB(128, 0, 128),   "SL"},
    {"vcxproj", RGB(128, 0, 128),  "VC"},
    {"csproj", RGB(80, 160, 0),    "CS"},

    // Database
    {"sql",    RGB(0, 130, 180),   "SQ"},
    {"db",     RGB(100, 100, 100), "DB"},
    {"sqlite", RGB(0, 130, 180),   "SQ"},

    // Images
    {"png",    RGB(140, 200, 60),  "PN"},
    {"jpg",    RGB(140, 200, 60),  "JP"},
    {"jpeg",   RGB(140, 200, 60),  "JP"},
    {"gif",    RGB(140, 200, 60),  "GI"},
    {"svg",    RGB(255, 180, 0),   "SV"},
    {"ico",    RGB(140, 200, 60),  "IC"},
    {"bmp",    RGB(140, 200, 60),  "BM"},

    // AI / ML
    {"gguf",   RGB(255, 100, 0),   "GG"},
    {"onnx",   RGB(0, 150, 255),   "ON"},
    {"pt",     RGB(238, 76, 44),   "PT"},
    {"safetensors", RGB(255, 180, 0), "ST"},

    // Binary
    {"exe",    RGB(100, 100, 100), "EX"},
    {"dll",    RGB(100, 100, 100), "DL"},
    {"obj",    RGB(100, 100, 100), "OB"},
    {"lib",    RGB(100, 100, 100), "LI"},

    // Git
    {"gitignore", RGB(240, 80, 50), "GI"},
    {"gitattributes", RGB(240, 80, 50), "GA"},

    {nullptr, 0, nullptr} // sentinel
};

// ============================================================================
// CREATE FILE TYPE ICON — Generate colored icon with text glyph
// ============================================================================

HICON Win32IDE::createFileTypeIcon(const std::string& ext, COLORREF color)
{
    const int iconSize = 16;

    // Create a device context and bitmap for the icon
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = iconSize;
    bmi.bmiHeader.biHeight = -iconSize; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;

    void* pvBits = nullptr;
    HBITMAP hBmpColor = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
    HBITMAP hBmpMask = CreateBitmap(iconSize, iconSize, 1, 1, nullptr);

    HBITMAP oldBmp = (HBITMAP)SelectObject(hdcMem, hBmpColor);

    // Fill background (rounded rect look)
    RECT rc = { 0, 0, iconSize, iconSize };
    HBRUSH bgBrush = CreateSolidBrush(color);
    FillRect(hdcMem, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Draw dark border
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(
        GetRValue(color) * 6 / 10,
        GetGValue(color) * 6 / 10,
        GetBValue(color) * 6 / 10));
    HPEN oldPen = (HPEN)SelectObject(hdcMem, borderPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdcMem, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdcMem, 0, 0, iconSize, iconSize);
    SelectObject(hdcMem, oldBrush);
    SelectObject(hdcMem, oldPen);
    DeleteObject(borderPen);

    // Draw glyph text
    HFONT hFont = CreateFontA(-9, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    HFONT oldFont = (HFONT)SelectObject(hdcMem, hFont);
    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, RGB(255, 255, 255));

    // Find the glyph for this extension
    const char* glyph = "?";
    std::string extLower = ext;
    std::transform(extLower.begin(), extLower.end(), extLower.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    for (int i = 0; SETI_ICONS[i].extension != nullptr; i++) {
        if (extLower == SETI_ICONS[i].extension) {
            glyph = SETI_ICONS[i].glyph;
            break;
        }
    }

    DrawTextA(hdcMem, glyph, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdcMem, oldFont);
    DeleteObject(hFont);
    SelectObject(hdcMem, oldBmp);

    // Create ICONINFO
    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmColor = hBmpColor;
    ii.hbmMask = hBmpMask;

    HICON hIcon = CreateIconIndirect(&ii);

    // Cleanup
    DeleteObject(hBmpColor);
    DeleteObject(hBmpMask);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    return hIcon;
}

// ============================================================================
// INIT FILE ICON THEME — Build imagelist with all extension icons
// ============================================================================

void Win32IDE::initFileIconTheme()
{
    if (m_hFileIconList) {
        ImageList_Destroy(m_hFileIconList);
        m_hFileIconList = nullptr;
    }
    m_fileIconMap.clear();

    // Create imagelist (16x16 icons)
    m_hFileIconList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 64, 16);
    if (!m_hFileIconList) {
        RAWRXD_LOG_INFO("Win32IDE_FileIcons") << "Failed to create file icon imagelist";
        return;
    }

    // Add default folder icon (index 0)
    {
        HICON hFolder = createFileTypeIcon("folder", RGB(220, 186, 96));
        ImageList_AddIcon(m_hFileIconList, hFolder);
        DestroyIcon(hFolder);
        m_fileIconMap["__folder__"] = 0;
    }

    // Add default file icon (index 1)
    {
        HICON hDefault = createFileTypeIcon("file", RGB(128, 128, 128));
        ImageList_AddIcon(m_hFileIconList, hDefault);
        DestroyIcon(hDefault);
        m_fileIconMap["__default__"] = 1;
    }

    // Add all extension-specific icons
    for (int i = 0; SETI_ICONS[i].extension != nullptr; i++) {
        HICON hIcon = createFileTypeIcon(SETI_ICONS[i].extension, SETI_ICONS[i].color);
        int index = ImageList_AddIcon(m_hFileIconList, hIcon);
        DestroyIcon(hIcon);
        m_fileIconMap[SETI_ICONS[i].extension] = index;
    }

    RAWRXD_LOG_INFO("Win32IDE_FileIcons")
        << "File icon theme initialized (" << m_fileIconMap.size() << " icons)";
}

void Win32IDE::shutdownFileIconTheme()
{
    if (m_hFileIconList) {
        ImageList_Destroy(m_hFileIconList);
        m_hFileIconList = nullptr;
    }
    m_fileIconMap.clear();
}

// ============================================================================
// GET FILE ICON INDEX — Lookup icon for a filename
// ============================================================================

int Win32IDE::getFileIconIndex(const std::string& filename)
{
    // Extract extension
    size_t dotPos = filename.rfind('.');
    if (dotPos == std::string::npos) {
        return m_fileIconMap.count("__default__") ? m_fileIconMap["__default__"] : 1;
    }

    std::string ext = filename.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });

    // Special filenames
    std::string nameLower = filename;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });

    if (nameLower == "makefile" || nameLower == "gnumakefile") {
        return m_fileIconMap.count("makefile") ? m_fileIconMap["makefile"] : 1;
    }
    if (nameLower == "dockerfile") {
        return m_fileIconMap.count("dockerfile") ? m_fileIconMap["dockerfile"] : 1;
    }
    if (nameLower == ".gitignore") {
        return m_fileIconMap.count("gitignore") ? m_fileIconMap["gitignore"] : 1;
    }

    auto it = m_fileIconMap.find(ext);
    if (it != m_fileIconMap.end()) {
        return it->second;
    }

    return m_fileIconMap.count("__default__") ? m_fileIconMap["__default__"] : 1;
}

// ============================================================================
// APPLY FILE ICONS TO TREE — Set imagelist on TreeView control
// ============================================================================

void Win32IDE::applyFileIconsToTree(HWND hwndTree)
{
    if (!hwndTree || !m_hFileIconList) return;

    TreeView_SetImageList(hwndTree, m_hFileIconList, TVSIL_NORMAL);

    RAWRXD_LOG_INFO("Win32IDE_FileIcons") << "File icons applied to tree view";
}
