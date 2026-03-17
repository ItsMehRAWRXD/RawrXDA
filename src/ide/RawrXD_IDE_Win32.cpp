/*=============================================================================
 * RawrXD_IDE_Win32.cpp
 * Complete Win32 GUI IDE Shell — monolithic, zero-dependency implementation
 *
 * Panels : File Tree (TreeView) | Code Editor (RichEdit) | Output | Widget
 * IPC    : Named pipe client to \\.\pipe\RawrXD_WidgetIntelligence
 * Build  : Invokes ml64.exe / link.exe via CreateProcess
 * Theme  : Dark / Light with custom WM_CTLCOLOR* handling
 *
 * Compile: cl /W4 /O2 /DUNICODE /D_UNICODE RawrXD_IDE_Win32.cpp
 *               /link user32.lib gdi32.lib comctl32.lib comdlg32.lib
 *                     shell32.lib shlwapi.lib advapi32.lib ole32.lib
 *
 * (C) RawrXD Project — ZERO external dependencies
 *===========================================================================*/

#include "RawrXD_IDE_Win32.h"
#include "ide_completion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Globals ────────────────────────────────────────────────────────────── */
static RawrXD_IDE g_IDE;

/* SetProcessDpiAwareness / GetDpiForWindow loaded at runtime for Win7 compat */
typedef HRESULT (WINAPI *PFN_SetProcessDpiAwareness)(int);
typedef UINT    (WINAPI *PFN_GetDpiForWindow)(HWND);
static PFN_GetDpiForWindow pfnGetDpiForWindow = NULL;

/* ── Forward declarations for helpers ───────────────────────────────────── */
static void     IDE_ReadBuildOutput(RawrXD_IDE* ide, HANDLE hRead);
static HTREEITEM IDE_TreeAddItem(HWND hTree, HTREEITEM hParent, const WCHAR* text, BOOL isFolder);
static void     IDE_AutoDetectEncoding(const BYTE* data, DWORD size, DWORD* outEncoding);
static int      IDE_ScaleForDPI(RawrXD_IDE* ide, int val);
static void     IDE_SetRichEditFont(HWND hEdit, const WCHAR* faceName, int pointSize, COLORREF color);

/*===========================================================================
 * THEME DEFINITIONS
 *=========================================================================*/
static const RawrXD_Theme g_DarkTheme = {
    /* bgWindow */      RGB(30,  30,  30),
    /* bgEditor */      RGB(28,  28,  28),
    /* bgOutput */      RGB(25,  25,  25),
    /* bgTree */        RGB(33,  33,  33),
    /* bgWidget */      RGB(35,  35,  35),
    /* bgStatus */      RGB(0,   122, 204),
    /* fgText */        RGB(212, 212, 212),
    /* fgComment */     RGB(106, 153, 85),
    /* fgKeyword */     RGB(86,  156, 214),
    /* fgString */      RGB(206, 145, 120),
    /* fgNumber */      RGB(181, 206, 168),
    /* fgOperator */    RGB(180, 180, 180),
    /* fgPreprocessor */RGB(155, 155, 255),
    /* fgLineNumber */  RGB(133, 133, 133),
    /* bgSelection */   RGB(38,  79,  120),
    /* fgSelection */   RGB(255, 255, 255),
    /* borderColor */   RGB(60,  60,  60),
    /* splitterColor */ RGB(60,  60,  60),
    /* menuBg */        RGB(45,  45,  45),
    /* menuFg */        RGB(212, 212, 212),
};

static const RawrXD_Theme g_LightTheme = {
    /* bgWindow */      RGB(243, 243, 243),
    /* bgEditor */      RGB(255, 255, 255),
    /* bgOutput */      RGB(245, 245, 245),
    /* bgTree */        RGB(248, 248, 248),
    /* bgWidget */      RGB(248, 248, 248),
    /* bgStatus */      RGB(0,   122, 204),
    /* fgText */        RGB(0,   0,   0),
    /* fgComment */     RGB(0,   128, 0),
    /* fgKeyword */     RGB(0,   0,   255),
    /* fgString */      RGB(163, 21,  21),
    /* fgNumber */      RGB(9,   134, 88),
    /* fgOperator */    RGB(0,   0,   0),
    /* fgPreprocessor */RGB(128, 0,   128),
    /* fgLineNumber */  RGB(150, 150, 150),
    /* bgSelection */   RGB(173, 214, 255),
    /* fgSelection */   RGB(0,   0,   0),
    /* borderColor */   RGB(200, 200, 200),
    /* splitterColor */ RGB(200, 200, 200),
    /* menuBg */        RGB(243, 243, 243),
    /* menuFg */        RGB(0,   0,   0),
};

/*===========================================================================
 * DPI HELPERS
 *=========================================================================*/
void RawrXD_IDE_InitDPI(RawrXD_IDE* ide) {
    /* Try to set per-monitor DPI awareness via shcore.dll (Win8.1+) */
    HMODULE hShcore = LoadLibraryW(L"shcore.dll");
    if (hShcore) {
        PFN_SetProcessDpiAwareness pfn =
            (PFN_SetProcessDpiAwareness)GetProcAddress(hShcore, "SetProcessDpiAwareness");
        if (pfn) pfn(2); /* PROCESS_PER_MONITOR_DPI_AWARE */
        FreeLibrary(hShcore);
    } else {
        /* Win7 fallback */
        SetProcessDPIAware();
    }

    /* Cache GetDpiForWindow for later */
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        pfnGetDpiForWindow =
            (PFN_GetDpiForWindow)GetProcAddress(hUser32, "GetDpiForWindow");
    }

    ide->dpi      = 96;
    ide->dpiScale = 1.0f;
}

static int IDE_ScaleForDPI(RawrXD_IDE* ide, int val) {
    return (int)(val * ide->dpiScale);
}

int RawrXD_IDE_DPIScale(RawrXD_IDE* ide, int value) {
    return IDE_ScaleForDPI(ide, value);
}

static void IDE_UpdateDPI(RawrXD_IDE* ide) {
    if (pfnGetDpiForWindow && ide->hWndMain) {
        ide->dpi = pfnGetDpiForWindow(ide->hWndMain);
    } else {
        HDC hdc = GetDC(NULL);
        ide->dpi = (UINT)GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(NULL, hdc);
    }
    if (ide->dpi == 0) ide->dpi = 96;
    ide->dpiScale = (float)ide->dpi / 96.0f;
}

/*===========================================================================
 * THEME
 *=========================================================================*/
void RawrXD_IDE_SetDarkTheme(RawrXD_IDE* ide) {
    ide->theme       = g_DarkTheme;
    ide->isDarkTheme = TRUE;
}

void RawrXD_IDE_SetLightTheme(RawrXD_IDE* ide) {
    ide->theme       = g_LightTheme;
    ide->isDarkTheme = FALSE;
}

void RawrXD_IDE_CreateThemeBrushes(RawrXD_IDE* ide) {
    RawrXD_IDE_DestroyThemeBrushes(ide);
    ide->hBrushBg     = CreateSolidBrush(ide->theme.bgWindow);
    ide->hBrushEditor = CreateSolidBrush(ide->theme.bgEditor);
    ide->hBrushOutput = CreateSolidBrush(ide->theme.bgOutput);
    ide->hBrushTree   = CreateSolidBrush(ide->theme.bgTree);
    ide->hBrushWidget = CreateSolidBrush(ide->theme.bgWidget);
}

void RawrXD_IDE_DestroyThemeBrushes(RawrXD_IDE* ide) {
    if (ide->hBrushBg)     { DeleteObject(ide->hBrushBg);     ide->hBrushBg     = NULL; }
    if (ide->hBrushEditor) { DeleteObject(ide->hBrushEditor); ide->hBrushEditor = NULL; }
    if (ide->hBrushOutput) { DeleteObject(ide->hBrushOutput); ide->hBrushOutput = NULL; }
    if (ide->hBrushTree)   { DeleteObject(ide->hBrushTree);   ide->hBrushTree   = NULL; }
    if (ide->hBrushWidget) { DeleteObject(ide->hBrushWidget); ide->hBrushWidget = NULL; }
}

void RawrXD_IDE_ApplyTheme(RawrXD_IDE* ide) {
    RawrXD_IDE_CreateThemeBrushes(ide);

    /* Apply to editor */
    if (ide->hWndEditor) {
        SendMessage(ide->hWndEditor, EM_SETBKGNDCOLOR, 0, (LPARAM)ide->theme.bgEditor);
        CHARFORMAT2W cf;
        ZeroMemory(&cf, sizeof(cf));
        cf.cbSize      = sizeof(cf);
        cf.dwMask      = CFM_COLOR;
        cf.crTextColor = ide->theme.fgText;
        SendMessage(ide->hWndEditor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    }

    /* Apply to tree */
    if (ide->hWndFileTree) {
        TreeView_SetBkColor(ide->hWndFileTree, ide->theme.bgTree);
        TreeView_SetTextColor(ide->hWndFileTree, ide->theme.fgText);
    }

    /* Apply dark-mode to window frame (Win10 1809+ undocumented) */
    if (ide->isDarkTheme) {
        typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
        HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
        if (hDwm) {
            PFN_DwmSetWindowAttribute pfnDwm =
                (PFN_DwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");
            if (pfnDwm) {
                BOOL dark = TRUE;
                pfnDwm(ide->hWndMain, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
            }
            FreeLibrary(hDwm);
        }
    }

    /* Status bar color */
    if (ide->hWndStatusBar) {
        SendMessage(ide->hWndStatusBar, SB_SETBKCOLOR, 0, (LPARAM)ide->theme.bgStatus);
    }

    /* Force full repaint */
    if (ide->hWndMain)
        RedrawWindow(ide->hWndMain, NULL, NULL,
                     RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

/*===========================================================================
 * FONT CREATION
 *=========================================================================*/
static HFONT IDE_CreateFont(const WCHAR* faceName, int pointSize, BOOL bold, RawrXD_IDE* ide) {
    int height = -MulDiv(pointSize, (int)ide->dpi, 72);
    return CreateFontW(
        height, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN,
        faceName
    );
}

static void IDE_SetRichEditFont(HWND hEdit, const WCHAR* faceName, int pointSize, COLORREF color) {
    CHARFORMAT2W cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize      = sizeof(cf);
    cf.dwMask      = CFM_FACE | CFM_SIZE | CFM_COLOR;
    cf.yHeight     = pointSize * 20; /* twips */
    cf.crTextColor = color;
    StringCchCopyW(cf.szFaceName, LF_FACESIZE, faceName);
    SendMessage(hEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
}

/*===========================================================================
 * INITIALIZATION
 *=========================================================================*/
BOOL RawrXD_IDE_Init(RawrXD_IDE* ide, HINSTANCE hInst) {
    ZeroMemory(ide, sizeof(RawrXD_IDE));

    ide->hInstance      = hInst;
    ide->fileTreeWidth  = FILETREE_DEFAULT_WIDTH;
    ide->outputHeight   = OUTPUT_DEFAULT_HEIGHT;
    ide->widgetWidth    = WIDGET_DEFAULT_WIDTH;
    ide->showFileTree   = TRUE;
    ide->showOutput     = TRUE;
    ide->showWidget     = TRUE;
    ide->isUntitled     = TRUE;
    ide->fileEncoding   = 1; /* UTF-8 default */
    ide->buildState     = BUILD_IDLE;
    ide->ipcState       = IPC_DISCONNECTED;

    /* DPI */
    RawrXD_IDE_InitDPI(ide);

    /* Default dark theme */
    RawrXD_IDE_SetDarkTheme(ide);

    /* Common controls v6 */
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES
               | ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES | ICC_COOL_CLASSES;
    InitCommonControlsEx(&icc);

    /* Load RichEdit 4.1 (msftedit.dll) or fall back to 2.0 */
    ide->hRichEditLib = LoadLibraryW(L"msftedit.dll");
    if (ide->hRichEditLib) {
        StringCchCopyW(ide->richEditDll, MAX_PATH, L"msftedit.dll (RICHEDIT50W)");
    } else {
        ide->hRichEditLib = LoadLibraryW(L"riched20.dll");
        if (ide->hRichEditLib)
            StringCchCopyW(ide->richEditDll, MAX_PATH, L"riched20.dll (RICHEDIT20W)");
    }

    /* Register window class */
    if (!RawrXD_IDE_RegisterClass(ide))
        return FALSE;

    /* Create main window */
    if (!RawrXD_IDE_CreateMainWindow(ide))
        return FALSE;

    /* Update DPI with actual window */
    IDE_UpdateDPI(ide);

    /* Fonts */
    ide->hFontCode = IDE_CreateFont(L"Consolas", 11, FALSE, ide);
    ide->hFontUI   = IDE_CreateFont(L"Segoe UI", 9,  FALSE, ide);

    /* Create child controls */
    RawrXD_IDE_CreateControls(ide);

    /* Accelerators */
    ide->hAccelTable = RawrXD_IDE_CreateAccelerators(ide);

    /* Apply initial theme */
    RawrXD_IDE_ApplyTheme(ide);

    /* Timers */
    SetTimer(ide->hWndMain, IDT_STATUS_UPDATE, 500,  NULL);
    SetTimer(ide->hWndMain, IDT_IPC_POLL,      2000, NULL);
    SetTimer(ide->hWndMain, IDT_AUTOSAVE,      60000, NULL);

    /* Show */
    ShowWindow(ide->hWndMain, SW_SHOWDEFAULT);
    UpdateWindow(ide->hWndMain);

    /* Try initial IPC connection */
    RawrXD_IDE_IPCConnect(ide);

    /* Populate file tree with project root */
    RawrXD_IDE_PopulateTree(ide, L"D:\\rawrxd\\src");

    return TRUE;
}

/*===========================================================================
 * REGISTER WINDOW CLASS
 *=========================================================================*/
BOOL RawrXD_IDE_RegisterClass(RawrXD_IDE* ide) {
    WNDCLASSEXW wcx;
    ZeroMemory(&wcx, sizeof(wcx));
    wcx.cbSize        = sizeof(WNDCLASSEXW);
    wcx.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcx.lpfnWndProc   = RawrXD_IDE_WndProc;
    wcx.cbClsExtra    = 0;
    wcx.cbWndExtra    = sizeof(void*);
    wcx.hInstance     = ide->hInstance;
    wcx.hIcon         = LoadIconW(NULL, IDI_APPLICATION);
    wcx.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wcx.hbrBackground = NULL; /* we paint ourselves */
    wcx.lpszMenuName  = NULL;
    wcx.lpszClassName = RAWRXD_IDE_CLASS;
    wcx.hIconSm       = LoadIconW(NULL, IDI_APPLICATION);

    return RegisterClassExW(&wcx) != 0;
}

/*===========================================================================
 * CREATE MAIN WINDOW
 *=========================================================================*/
BOOL RawrXD_IDE_CreateMainWindow(RawrXD_IDE* ide) {
    ide->hWndMain = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_CONTROLPARENT,
        RAWRXD_IDE_CLASS,
        RAWRXD_IDE_TITLE,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CW_USEDEFAULT, CW_USEDEFAULT,
        RAWRXD_IDE_DEFAULT_WIDTH, RAWRXD_IDE_DEFAULT_HEIGHT,
        NULL,      /* parent */
        NULL,      /* menu - created separately  */
        ide->hInstance,
        ide        /* pass IDE struct to WM_NCCREATE */
    );

    return ide->hWndMain != NULL;
}

/*===========================================================================
 * CREATE CHILD CONTROLS
 *=========================================================================*/
void RawrXD_IDE_CreateControls(RawrXD_IDE* ide) {
    HWND hWnd = ide->hWndMain;
    HINSTANCE hInst = ide->hInstance;

    /* ── File Tree (TreeView) ─────────────────────────────────────────── */
    ide->hWndFileTree = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_TREEVIEWW,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL |
        TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT |
        TVS_SHOWSELALWAYS | TVS_EDITLABELS,
        0, 0, 0, 0,
        hWnd,
        (HMENU)(UINT_PTR)IDC_FILE_TREE,
        hInst,
        NULL
    );
    if (ide->hWndFileTree && ide->hFontUI)
        SendMessage(ide->hWndFileTree, WM_SETFONT, (WPARAM)ide->hFontUI, TRUE);

    /* ── Code Editor (RichEdit) ───────────────────────────────────────── */
    const WCHAR* richEditClass = ide->hRichEditLib ?
        (GetProcAddress(ide->hRichEditLib, "ITextDocument") ? MSFTEDIT_CLASS : RICHEDIT_CLASSW) :
        L"EDIT";

    /* Determine class name based on which DLL loaded */
    const WCHAR* editorClass = MSFTEDIT_CLASS;
    if (!ide->hRichEditLib) {
        editorClass = L"EDIT";   /* absolute fallback */
    } else if (wcsstr(ide->richEditDll, L"riched20") != NULL) {
        editorClass = RICHEDIT_CLASSW;
    }

    ide->hWndEditor = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        editorClass,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
        ES_WANTRETURN | ES_NOHIDESEL,
        0, 0, 0, 0,
        hWnd,
        (HMENU)(UINT_PTR)IDC_CODE_EDITOR,
        hInst,
        NULL
    );
    if (ide->hWndEditor) {
        if (ide->hFontCode)
            SendMessage(ide->hWndEditor, WM_SETFONT, (WPARAM)ide->hFontCode, TRUE);
        /* Allow unlimited text */
        SendMessage(ide->hWndEditor, EM_EXLIMITTEXT, 0, (LPARAM)0x7FFFFFFF);
        /* Set tab stops to 4 characters */
        int tabStop = 16; /* 4 chars × 4 dialog units */
        SendMessage(ide->hWndEditor, EM_SETTABSTOPS, 1, (LPARAM)&tabStop);
        /* Enable EN_CHANGE notifications */
        SendMessage(ide->hWndEditor, EM_SETEVENTMASK, 0,
                    ENM_CHANGE | ENM_SELCHANGE | ENM_SCROLL | ENM_UPDATE);
        /* Font via CHARFORMAT */
        IDE_SetRichEditFont(ide->hWndEditor, L"Consolas", 11, ide->theme.fgText);
    }

    /* ── Output Panel ─────────────────────────────────────────────────── */
    ide->hWndOutput = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        editorClass,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_NOHIDESEL,
        0, 0, 0, 0,
        hWnd,
        (HMENU)(UINT_PTR)IDC_OUTPUT_PANEL,
        hInst,
        NULL
    );
    if (ide->hWndOutput) {
        if (ide->hFontCode)
            SendMessage(ide->hWndOutput, WM_SETFONT, (WPARAM)ide->hFontCode, TRUE);
        SendMessage(ide->hWndOutput, EM_EXLIMITTEXT, 0, (LPARAM)0x7FFFFFFF);
        IDE_SetRichEditFont(ide->hWndOutput, L"Consolas", 10, ide->theme.fgText);
    }

    /* ── Widget Intelligence Panel ────────────────────────────────────── */
    ide->hWndWidget = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        editorClass,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | ES_NOHIDESEL,
        0, 0, 0, 0,
        hWnd,
        (HMENU)(UINT_PTR)IDC_WIDGET_PANEL,
        hInst,
        NULL
    );
    if (ide->hWndWidget) {
        if (ide->hFontUI)
            SendMessage(ide->hWndWidget, WM_SETFONT, (WPARAM)ide->hFontUI, TRUE);
        SendMessage(ide->hWndWidget, EM_EXLIMITTEXT, 0, (LPARAM)0x7FFFFFFF);
        IDE_SetRichEditFont(ide->hWndWidget, L"Segoe UI", 10, ide->theme.fgText);
    }

    /* ── Status Bar ───────────────────────────────────────────────────── */
    RawrXD_IDE_CreateStatusBar(ide);

    /* ── Menu Bar ─────────────────────────────────────────────────────── */
    ide->hMenuBar = RawrXD_IDE_CreateMenuBar(ide);
    SetMenu(hWnd, ide->hMenuBar);

    /* Initial title */
    RawrXD_IDE_UpdateTitle(ide);
}

/*===========================================================================
 * MENU BAR (created programmatically — no .rc dependency at runtime)
 *=========================================================================*/
HMENU RawrXD_IDE_CreateMenuBar(RawrXD_IDE* ide) {
    (void)ide;
    HMENU hBar   = CreateMenu();
    HMENU hFile  = CreatePopupMenu();
    HMENU hEdit  = CreatePopupMenu();
    HMENU hView  = CreatePopupMenu();
    HMENU hBuild = CreatePopupMenu();
    HMENU hTools = CreatePopupMenu();
    HMENU hHelp  = CreatePopupMenu();

    /* File */
    AppendMenuW(hFile, MF_STRING, IDM_FILE_NEW,    L"&New\tCtrl+N");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_OPEN,   L"&Open...\tCtrl+O");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_SAVE,   L"&Save\tCtrl+S");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_SAVEAS, L"Save &As...\tCtrl+Shift+S");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_CLOSE,  L"&Close\tCtrl+W");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_EXIT,   L"E&xit\tAlt+F4");

    /* Edit */
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_UNDO,      L"&Undo\tCtrl+Z");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_REDO,       L"&Redo\tCtrl+Y");
    AppendMenuW(hEdit, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_CUT,        L"Cu&t\tCtrl+X");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_COPY,       L"&Copy\tCtrl+C");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_PASTE,      L"&Paste\tCtrl+V");
    AppendMenuW(hEdit, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_SELECTALL,  L"Select &All\tCtrl+A");
    AppendMenuW(hEdit, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_FIND,       L"&Find...\tCtrl+F");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_REPLACE,    L"&Replace...\tCtrl+H");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_GOTO,       L"&Go to Line...\tCtrl+G");

    /* View */
    AppendMenuW(hView, MF_STRING | MF_CHECKED,  IDM_VIEW_FILEBROWSER, L"File &Browser\tCtrl+E");
    AppendMenuW(hView, MF_STRING | MF_CHECKED,  IDM_VIEW_OUTPUT,      L"&Output Panel\tCtrl+`");
    AppendMenuW(hView, MF_STRING | MF_CHECKED,  IDM_VIEW_WIDGET,      L"&Widget Panel");
    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING, IDM_VIEW_FULLSCREEN, L"&Fullscreen\tF11");
    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING | MF_CHECKED, IDM_VIEW_DARK_THEME,  L"&Dark Theme");
    AppendMenuW(hView, MF_STRING,              IDM_VIEW_LIGHT_THEME, L"&Light Theme");

    /* Build */
    AppendMenuW(hBuild, MF_STRING, IDM_BUILD_BUILD,   L"&Build PE\tF7");
    AppendMenuW(hBuild, MF_STRING, IDM_BUILD_REBUILD, L"&Rebuild\tCtrl+Shift+B");
    AppendMenuW(hBuild, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hBuild, MF_STRING,  IDM_BUILD_RUN,    L"&Run\tF5");
    AppendMenuW(hBuild, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hBuild, MF_STRING, IDM_BUILD_CLEAN,   L"&Clean");
    AppendMenuW(hBuild, MF_STRING | MF_GRAYED, IDM_BUILD_STOP, L"&Stop Build");

    /* Tools */
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_PE_INSPECTOR,  L"PE &Inspector");
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_INSTR_ENCODER, L"Instruction &Encoder");
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_EXT_MANAGER,   L"Extension &Manager");
    AppendMenuW(hTools, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_OPTIONS,       L"&Options...");

    /* Help */
    AppendMenuW(hHelp, MF_STRING, IDM_HELP_DOCS,  L"&Documentation");
    AppendMenuW(hHelp, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hHelp, MF_STRING, IDM_HELP_ABOUT, L"&About RawrXD IDE");

    /* Assemble bar */
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hFile,  L"&File");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hEdit,  L"&Edit");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hView,  L"&View");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hBuild, L"&Build");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hTools, L"&Tools");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hHelp,  L"&Help");

    return hBar;
}

/*===========================================================================
 * ACCELERATOR TABLE (created programmatically)
 *=========================================================================*/
HACCEL RawrXD_IDE_CreateAccelerators(RawrXD_IDE* ide) {
    (void)ide;
    ACCEL accelTable[] = {
        { FCONTROL | FVIRTKEY,            'N',      IDM_FILE_NEW      },
        { FCONTROL | FVIRTKEY,            'O',      IDM_FILE_OPEN     },
        { FCONTROL | FVIRTKEY,            'S',      IDM_FILE_SAVE     },
        { FCONTROL | FSHIFT | FVIRTKEY,   'S',      IDM_FILE_SAVEAS   },
        { FCONTROL | FVIRTKEY,            'W',      IDM_FILE_CLOSE    },
        { FCONTROL | FVIRTKEY,            'Z',      IDM_EDIT_UNDO     },
        { FCONTROL | FVIRTKEY,            'Y',      IDM_EDIT_REDO     },
        { FCONTROL | FVIRTKEY,            'X',      IDM_EDIT_CUT      },
        { FCONTROL | FVIRTKEY,            'C',      IDM_EDIT_COPY     },
        { FCONTROL | FVIRTKEY,            'V',      IDM_EDIT_PASTE    },
        { FCONTROL | FVIRTKEY,            'A',      IDM_EDIT_SELECTALL},
        { FCONTROL | FVIRTKEY,            'F',      IDM_EDIT_FIND     },
        { FCONTROL | FVIRTKEY,            'H',      IDM_EDIT_REPLACE  },
        { FCONTROL | FVIRTKEY,            'G',      IDM_EDIT_GOTO     },
        { FVIRTKEY,                       VK_F7,    IDM_BUILD_BUILD   },
        { FCONTROL | FVIRTKEY,            'B',      IDM_BUILD_BUILD   },
        { FCONTROL | FSHIFT | FVIRTKEY,   'B',      IDM_BUILD_REBUILD },
        { FVIRTKEY,                       VK_F5,    IDM_BUILD_RUN     },
        { FCONTROL | FVIRTKEY,            'E',      IDM_VIEW_FILEBROWSER },
        { FCONTROL | FVIRTKEY,            VK_OEM_3, IDM_VIEW_OUTPUT   },
        { FVIRTKEY,                       VK_F11,   IDM_VIEW_FULLSCREEN },
    };
    int count = sizeof(accelTable) / sizeof(accelTable[0]);
    return CreateAcceleratorTableW(accelTable, count);
}

/*===========================================================================
 * STATUS BAR
 *=========================================================================*/
void RawrXD_IDE_CreateStatusBar(RawrXD_IDE* ide) {
    ide->hWndStatusBar = CreateWindowExW(
        0,
        STATUSCLASSNAMEW,
        NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP | CCS_BOTTOM,
        0, 0, 0, 0,
        ide->hWndMain,
        (HMENU)(UINT_PTR)IDC_STATUS_BAR,
        ide->hInstance,
        NULL
    );
    if (ide->hWndStatusBar && ide->hFontUI) {
        SendMessage(ide->hWndStatusBar, WM_SETFONT, (WPARAM)ide->hFontUI, TRUE);
    }

    /* Set initial parts */
    int widths[SB_NUM_PARTS] = { 400, 520, 600, 740, -1 };
    SendMessage(ide->hWndStatusBar, SB_SETPARTS, SB_NUM_PARTS, (LPARAM)widths);
    SendMessage(ide->hWndStatusBar, SB_SETTEXTW, SB_PART_FILE,     (LPARAM)L" Untitled");
    SendMessage(ide->hWndStatusBar, SB_SETTEXTW, SB_PART_LINECOL,  (LPARAM)L" Ln 1, Col 1");
    SendMessage(ide->hWndStatusBar, SB_SETTEXTW, SB_PART_ENCODING, (LPARAM)L" UTF-8");
    SendMessage(ide->hWndStatusBar, SB_SETTEXTW, SB_PART_BUILD,    (LPARAM)L" Ready");
    SendMessage(ide->hWndStatusBar, SB_SETTEXTW, SB_PART_IPC,      (LPARAM)L" IPC: ---");
}

void RawrXD_IDE_UpdateStatusBar(RawrXD_IDE* ide) {
    RawrXD_IDE_UpdateLineCol(ide);

    /* File path */
    WCHAR buf[MAX_PATH + 4];
    if (ide->isUntitled)
        StringCchCopyW(buf, MAX_PATH + 4, L" Untitled");
    else
        StringCchPrintfW(buf, MAX_PATH + 4, L" %s%s", ide->currentFilePath, ide->isModified ? L" *" : L"");
    SendMessage(ide->hWndStatusBar, SB_SETTEXTW, SB_PART_FILE, (LPARAM)buf);

    /* Encoding */
    const WCHAR* enc = L" UTF-8";
    if (ide->fileEncoding == 0) enc = L" ANSI";
    else if (ide->fileEncoding == 2) enc = L" UTF-16 LE";
    SendMessage(ide->hWndStatusBar, SB_SETTEXTW, SB_PART_ENCODING, (LPARAM)enc);

    /* Build state */
    const WCHAR* bstate = L" Ready";
    switch (ide->buildState) {
        case BUILD_RUNNING: bstate = L" Building..."; break;
        case BUILD_SUCCESS: bstate = L" Build OK";    break;
        case BUILD_FAILED:  bstate = L" Build FAILED";break;
        default: break;
    }
    SendMessage(ide->hWndStatusBar, SB_SETTEXTW, SB_PART_BUILD, (LPARAM)bstate);

    /* IPC state */
    const WCHAR* ipcStr = L" IPC: ---";
    switch (ide->ipcState) {
        case IPC_CONNECTING:  ipcStr = L" IPC: connecting"; break;
        case IPC_CONNECTED:   ipcStr = L" IPC: connected";  break;
        case IPC_ERROR:       ipcStr = L" IPC: error";      break;
        default: break;
    }
    SendMessage(ide->hWndStatusBar, SB_SETTEXTW, SB_PART_IPC, (LPARAM)ipcStr);
}

void RawrXD_IDE_UpdateLineCol(RawrXD_IDE* ide) {
    if (!ide->hWndEditor) return;

    /* Get caret position */
    DWORD start = 0;
    SendMessage(ide->hWndEditor, EM_GETSEL, (WPARAM)&start, 0);
    int line = (int)SendMessage(ide->hWndEditor, EM_LINEFROMCHAR, (WPARAM)start, 0);
    int lineStart = (int)SendMessage(ide->hWndEditor, EM_LINEINDEX, (WPARAM)line, 0);
    int col = (int)(start - lineStart);

    WCHAR buf[64];
    StringCchPrintfW(buf, 64, L" Ln %d, Col %d", line + 1, col + 1);
    SendMessage(ide->hWndStatusBar, SB_SETTEXTW, SB_PART_LINECOL, (LPARAM)buf);
}

void RawrXD_IDE_SetBuildStatus(RawrXD_IDE* ide, const WCHAR* text) {
    SendMessage(ide->hWndStatusBar, SB_SETTEXTW, SB_PART_BUILD, (LPARAM)text);
}

/*===========================================================================
 * WINDOW PROCEDURE
 *=========================================================================*/
LRESULT CALLBACK RawrXD_IDE_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RawrXD_IDE* ide = NULL;

    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* pcs = (CREATESTRUCTW*)lParam;
        ide = (RawrXD_IDE*)pcs->lpCreateParams;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)ide);
        ide->hWndMain = hWnd;
    } else {
        ide = (RawrXD_IDE*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    }

    if (!ide) return DefWindowProcW(hWnd, msg, wParam, lParam);

    switch (msg) {
    case WM_CREATE:
        return RawrXD_IDE_OnCreate(ide, hWnd, (LPCREATESTRUCT)lParam);

    case WM_SIZE:
        RawrXD_IDE_OnSize(ide, LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_PAINT:
        RawrXD_IDE_OnPaint(ide, hWnd);
        return 0;

    case WM_COMMAND:
        RawrXD_IDE_OnCommand(ide, LOWORD(wParam), HIWORD(wParam), (HWND)lParam);
        return 0;

    case WM_NOTIFY:
        return RawrXD_IDE_OnNotify(ide, (NMHDR*)lParam);

    case WM_TIMER:
        RawrXD_IDE_OnTimer(ide, (UINT_PTR)wParam);
        return 0;

    case WM_APP + 100:
        /* Build thread completed — re-enable menu items on UI thread */
        EnableMenuItem(ide->hMenuBar, IDM_BUILD_BUILD, MF_ENABLED);
        EnableMenuItem(ide->hMenuBar, IDM_BUILD_STOP,  MF_GRAYED);
        return 0;

    case WM_KEYDOWN:
    {
        /* Ctrl+Space — trigger Ollama completion */
        if (wParam == VK_SPACE && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
            if (ide->hWndEditor && IDECompletion::IsCompletionEngineReady()) {
                /* Get editor cursor position */
                DWORD charPos = 0;
                SendMessage(ide->hWndEditor, EM_GETSEL, (WPARAM)&charPos, 0);
                
                /* Get line number and extract line text */
                int line = (int)SendMessage(ide->hWndEditor, EM_LINEFROMCHAR, (WPARAM)charPos, 0);
                int lineStart = (int)SendMessage(ide->hWndEditor, EM_LINEINDEX, (WPARAM)line, 0);
                int col = (int)(charPos - lineStart);
                
                /* Get entire editor text */
                int textLength = GetWindowTextLengthW(ide->hWndEditor);
                if (textLength > 0 && textLength < 32000) {
                    WCHAR* editorText = new WCHAR[textLength + 1];
                    GetWindowTextW(ide->hWndEditor, editorText, textLength + 1);
                    
                    /* Extract current line */
                    WCHAR* lineStart = editorText;
                    WCHAR* lineEnd = editorText;
                    for (int i = 0; i < line && *lineEnd; lineEnd++) {
                        if (*lineEnd == L'\n') i++;
                    }
                    while (lineEnd > editorText && lineEnd[-1] != L'\n') lineEnd--;
                    lineStart = lineEnd;
                    while (*lineEnd && *lineEnd != L'\n' && *lineEnd != L'\r') lineEnd++;
                    
                    WCHAR currentLine[512] = {0};
                    int lineLen = (int)(lineEnd - lineStart);
                    if (lineLen > 0 && lineLen < 512) {
                        wcsncpy_s(currentLine, 512, lineStart, lineLen);
                    }
                    
                    delete[] editorText;
                    
                    /* Get cursor screen position */
                    POINT cursorPos = {0, 0};
                    SendMessage(ide->hWndEditor, EM_POSFROMCHAR, (WPARAM)&cursorPos, (LPARAM)charPos);
                    
                    RECT editorRect;
                    GetWindowRect(ide->hWndEditor, &editorRect);
                    int screenX = editorRect.left + cursorPos.x;
                    int screenY = editorRect.top + cursorPos.y + 20;
                    
                    /* Request completion */
                    IDECompletion::PopupContext ctx;
                    ctx.hParentWnd = ide->hWndEditor;
                    ctx.x = screenX;
                    ctx.y = screenY;
                    ctx.current_line = currentLine;
                    ctx.model = L"codellama:7b";
                    ctx.on_select = nullptr;
                    
                    IDECompletion::RequestCompletion(ctx);
                }
            }
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        RawrXD_IDE_OnClose(ide);
        return 0;

    case WM_DESTROY:
        RawrXD_IDE_OnDestroy(ide);
        return 0;

    case WM_ERASEBKGND:
        return 1; /* we handle painting */

    case WM_CTLCOLOREDIT:
        return RawrXD_IDE_OnCtlColorEdit(ide, (HDC)wParam, (HWND)lParam);

    case WM_CTLCOLORSTATIC:
        return RawrXD_IDE_OnCtlColorStatic(ide, (HDC)wParam, (HWND)lParam);

    case WM_DPICHANGED:
    {
        ide->dpi      = HIWORD(wParam);
        ide->dpiScale = (float)ide->dpi / 96.0f;
        RECT* prc = (RECT*)lParam;
        SetWindowPos(hWnd, NULL,
                     prc->left, prc->top,
                     prc->right - prc->left, prc->bottom - prc->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        /* Rebuild fonts at new DPI */
        if (ide->hFontCode) DeleteObject(ide->hFontCode);
        if (ide->hFontUI)   DeleteObject(ide->hFontUI);
        ide->hFontCode = IDE_CreateFont(L"Consolas", 11, FALSE, ide);
        ide->hFontUI   = IDE_CreateFont(L"Segoe UI", 9, FALSE, ide);
        if (ide->hWndEditor)   SendMessage(ide->hWndEditor,   WM_SETFONT, (WPARAM)ide->hFontCode, TRUE);
        if (ide->hWndOutput)   SendMessage(ide->hWndOutput,   WM_SETFONT, (WPARAM)ide->hFontCode, TRUE);
        if (ide->hWndFileTree) SendMessage(ide->hWndFileTree,  WM_SETFONT, (WPARAM)ide->hFontUI, TRUE);
        if (ide->hWndWidget)   SendMessage(ide->hWndWidget,    WM_SETFONT, (WPARAM)ide->hFontUI, TRUE);
        return 0;
    }

    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = 640;
        mmi->ptMinTrackSize.y = 480;
        return 0;
    }

    case WM_DROPFILES:
    {
        HDROP hDrop = (HDROP)wParam;
        WCHAR path[MAX_PATH];
        if (DragQueryFileW(hDrop, 0, path, MAX_PATH))
            RawrXD_IDE_LoadFile(ide, path);
        DragFinish(hDrop);
        return 0;
    }

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

/*===========================================================================
 * WM_CREATE
 *=========================================================================*/
LRESULT RawrXD_IDE_OnCreate(RawrXD_IDE* ide, HWND hWnd, LPCREATESTRUCT lpcs) {
    (void)lpcs;
    /* Enable drag-drop */
    DragAcceptFiles(hWnd, TRUE);
    return 0;
}

/*===========================================================================
 * WM_SIZE  — lay out all panes
 *=========================================================================*/
void RawrXD_IDE_OnSize(RawrXD_IDE* ide, int cx, int cy) {
    if (cx <= 0 || cy <= 0) return;
    RawrXD_IDE_LayoutPanes(ide);
    SendMessage(ide->hWndStatusBar, WM_SIZE, 0, 0);
}

void RawrXD_IDE_LayoutPanes(RawrXD_IDE* ide) {
    RECT rc;
    GetClientRect(ide->hWndMain, &rc);

    int cx = rc.right  - rc.left;
    int cy = rc.bottom - rc.top;

    /* Status bar occupies the bottom */
    int statusH = STATUS_HEIGHT;
    if (ide->hWndStatusBar) {
        RECT sbrc;
        GetWindowRect(ide->hWndStatusBar, &sbrc);
        statusH = sbrc.bottom - sbrc.top;
    }
    cy -= statusH;

    int leftW   = ide->showFileTree ? ide->fileTreeWidth : 0;
    int rightW  = ide->showWidget   ? ide->widgetWidth   : 0;
    int bottomH = ide->showOutput   ? ide->outputHeight  : 0;
    int splW    = SPLITTER_WIDTH;

    /* Clamp */
    if (leftW  > cx / 3) leftW  = cx / 3;
    if (rightW > cx / 3) rightW = cx / 3;
    if (bottomH > cy / 2) bottomH = cy / 2;

    int centerX = leftW + (leftW > 0 ? splW : 0);
    int centerW = cx - centerX - (rightW > 0 ? rightW + splW : 0);
    if (centerW < 100) centerW = 100;

    int editorH = cy - bottomH - (bottomH > 0 ? splW : 0);
    if (editorH < 50) editorH = 50;

    HDWP hdwp = BeginDeferWindowPos(5);

    /* File tree */
    if (ide->hWndFileTree) {
        if (ide->showFileTree) {
            hdwp = DeferWindowPos(hdwp, ide->hWndFileTree, NULL,
                                  0, 0, leftW, cy,
                                  SWP_NOZORDER | SWP_SHOWWINDOW);
        } else {
            hdwp = DeferWindowPos(hdwp, ide->hWndFileTree, NULL,
                                  0, 0, 0, 0,
                                  SWP_NOZORDER | SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE);
        }
    }

    /* Code editor */
    if (ide->hWndEditor) {
        hdwp = DeferWindowPos(hdwp, ide->hWndEditor, NULL,
                              centerX, 0, centerW, editorH,
                              SWP_NOZORDER | SWP_SHOWWINDOW);
    }

    /* Output panel */
    if (ide->hWndOutput) {
        if (ide->showOutput) {
            hdwp = DeferWindowPos(hdwp, ide->hWndOutput, NULL,
                                  centerX, editorH + splW, centerW, bottomH,
                                  SWP_NOZORDER | SWP_SHOWWINDOW);
        } else {
            hdwp = DeferWindowPos(hdwp, ide->hWndOutput, NULL,
                                  0, 0, 0, 0,
                                  SWP_NOZORDER | SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE);
        }
    }

    /* Widget panel */
    if (ide->hWndWidget) {
        if (ide->showWidget) {
            int widgetX = cx - rightW;
            hdwp = DeferWindowPos(hdwp, ide->hWndWidget, NULL,
                                  widgetX, 0, rightW, cy,
                                  SWP_NOZORDER | SWP_SHOWWINDOW);
        } else {
            hdwp = DeferWindowPos(hdwp, ide->hWndWidget, NULL,
                                  0, 0, 0, 0,
                                  SWP_NOZORDER | SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOSIZE);
        }
    }

    EndDeferWindowPos(hdwp);
}

/*===========================================================================
 * WM_PAINT — paint splitter bars and background
 *=========================================================================*/
void RawrXD_IDE_OnPaint(RawrXD_IDE* ide, HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    RECT rc;
    GetClientRect(hWnd, &rc);

    /* Fill entire background */
    HBRUSH hBrush = CreateSolidBrush(ide->theme.bgWindow);
    FillRect(hdc, &ps.rcPaint, hBrush);
    DeleteObject(hBrush);

    /* Draw splitter zones */
    HBRUSH hSplBrush = CreateSolidBrush(ide->theme.splitterColor);

    /* Vertical splitter after file tree */
    if (ide->showFileTree) {
        RECT splRect = { ide->fileTreeWidth, 0,
                         ide->fileTreeWidth + SPLITTER_WIDTH, rc.bottom };
        FillRect(hdc, &splRect, hSplBrush);
    }

    /* Vertical splitter before widget panel */
    if (ide->showWidget) {
        int wx = rc.right - ide->widgetWidth - SPLITTER_WIDTH;
        RECT splRect = { wx, 0, wx + SPLITTER_WIDTH, rc.bottom };
        FillRect(hdc, &splRect, hSplBrush);
    }

    /* Horizontal splitter above output */
    if (ide->showOutput) {
        RECT sbrc;
        GetWindowRect(ide->hWndStatusBar, &sbrc);
        int statusH = sbrc.bottom - sbrc.top;
        int cy      = rc.bottom - statusH;
        int editorH = cy - ide->outputHeight - SPLITTER_WIDTH;
        int centerX = ide->showFileTree ? ide->fileTreeWidth + SPLITTER_WIDTH : 0;
        int centerW = rc.right - centerX - (ide->showWidget ? ide->widgetWidth + SPLITTER_WIDTH : 0);
        RECT splRect = { centerX, editorH, centerX + centerW, editorH + SPLITTER_WIDTH };
        FillRect(hdc, &splRect, hSplBrush);
    }

    DeleteObject(hSplBrush);
    EndPaint(hWnd, &ps);
}

/*===========================================================================
 * WM_COMMAND
 *=========================================================================*/
void RawrXD_IDE_OnCommand(RawrXD_IDE* ide, WORD cmdId, WORD notifyCode, HWND hCtrl) {
    /* Editor change notifications */
    if (hCtrl == ide->hWndEditor && notifyCode == EN_CHANGE) {
        ide->isModified = TRUE;
        RawrXD_IDE_UpdateTitle(ide);
        return;
    }

    switch (cmdId) {
    /* ── File ─────────────────────────────────────────────────────────── */
    case IDM_FILE_NEW:      RawrXD_IDE_FileNew(ide);    break;
    case IDM_FILE_OPEN:     RawrXD_IDE_FileOpen(ide);   break;
    case IDM_FILE_SAVE:     RawrXD_IDE_FileSave(ide);   break;
    case IDM_FILE_SAVEAS:   RawrXD_IDE_FileSaveAs(ide); break;
    case IDM_FILE_CLOSE:    RawrXD_IDE_FileClose(ide);  break;
    case IDM_FILE_EXIT:
        PostMessage(ide->hWndMain, WM_CLOSE, 0, 0);
        break;

    /* ── Edit ─────────────────────────────────────────────────────────── */
    case IDM_EDIT_UNDO:      RawrXD_IDE_EditUndo(ide);      break;
    case IDM_EDIT_REDO:      RawrXD_IDE_EditRedo(ide);      break;
    case IDM_EDIT_CUT:       RawrXD_IDE_EditCut(ide);       break;
    case IDM_EDIT_COPY:      RawrXD_IDE_EditCopy(ide);      break;
    case IDM_EDIT_PASTE:     RawrXD_IDE_EditPaste(ide);     break;
    case IDM_EDIT_SELECTALL: RawrXD_IDE_EditSelectAll(ide); break;
    case IDM_EDIT_FIND:      RawrXD_IDE_EditFind(ide);      break;
    case IDM_EDIT_REPLACE:   RawrXD_IDE_EditReplace(ide);   break;
    case IDM_EDIT_GOTO:      RawrXD_IDE_EditGotoLine(ide);  break;

    /* ── View ─────────────────────────────────────────────────────────── */
    case IDM_VIEW_FILEBROWSER:
        ide->showFileTree = !ide->showFileTree;
        CheckMenuItem(ide->hMenuBar, IDM_VIEW_FILEBROWSER,
                      ide->showFileTree ? MF_CHECKED : MF_UNCHECKED);
        RawrXD_IDE_LayoutPanes(ide);
        InvalidateRect(ide->hWndMain, NULL, TRUE);
        break;

    case IDM_VIEW_OUTPUT:
        ide->showOutput = !ide->showOutput;
        CheckMenuItem(ide->hMenuBar, IDM_VIEW_OUTPUT,
                      ide->showOutput ? MF_CHECKED : MF_UNCHECKED);
        RawrXD_IDE_LayoutPanes(ide);
        InvalidateRect(ide->hWndMain, NULL, TRUE);
        break;

    case IDM_VIEW_WIDGET:
        ide->showWidget = !ide->showWidget;
        CheckMenuItem(ide->hMenuBar, IDM_VIEW_WIDGET,
                      ide->showWidget ? MF_CHECKED : MF_UNCHECKED);
        RawrXD_IDE_LayoutPanes(ide);
        InvalidateRect(ide->hWndMain, NULL, TRUE);
        break;

    case IDM_VIEW_FULLSCREEN:
    {
        if (!ide->isFullscreen) {
            GetWindowRect(ide->hWndMain, &ide->restoreRect);
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfoW(MonitorFromWindow(ide->hWndMain, MONITOR_DEFAULTTONEAREST), &mi);
            SetWindowLongPtrW(ide->hWndMain, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowPos(ide->hWndMain, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_FRAMECHANGED);
            ide->isFullscreen = TRUE;
        } else {
            SetWindowLongPtrW(ide->hWndMain, GWL_STYLE,
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
            SetWindowPos(ide->hWndMain, NULL,
                         ide->restoreRect.left, ide->restoreRect.top,
                         ide->restoreRect.right - ide->restoreRect.left,
                         ide->restoreRect.bottom - ide->restoreRect.top,
                         SWP_FRAMECHANGED | SWP_NOZORDER);
            ide->isFullscreen = FALSE;
        }
        break;
    }

    case IDM_VIEW_DARK_THEME:
        RawrXD_IDE_SetDarkTheme(ide);
        CheckMenuItem(ide->hMenuBar, IDM_VIEW_DARK_THEME,  MF_CHECKED);
        CheckMenuItem(ide->hMenuBar, IDM_VIEW_LIGHT_THEME, MF_UNCHECKED);
        RawrXD_IDE_ApplyTheme(ide);
        break;

    case IDM_VIEW_LIGHT_THEME:
        RawrXD_IDE_SetLightTheme(ide);
        CheckMenuItem(ide->hMenuBar, IDM_VIEW_DARK_THEME,  MF_UNCHECKED);
        CheckMenuItem(ide->hMenuBar, IDM_VIEW_LIGHT_THEME, MF_CHECKED);
        RawrXD_IDE_ApplyTheme(ide);
        break;

    /* ── Build ────────────────────────────────────────────────────────── */
    case IDM_BUILD_BUILD:   RawrXD_IDE_BuildProject(ide);   break;
    case IDM_BUILD_REBUILD: RawrXD_IDE_RebuildProject(ide); break;
    case IDM_BUILD_RUN:     RawrXD_IDE_RunProject(ide);     break;
    case IDM_BUILD_CLEAN:   RawrXD_IDE_CleanProject(ide);   break;
    case IDM_BUILD_STOP:    RawrXD_IDE_StopBuild(ide);      break;

    /* ── Tools ────────────────────────────────────────────────────────── */
    case IDM_TOOLS_PE_INSPECTOR:  RawrXD_IDE_LaunchPEInspector(ide);  break;
    case IDM_TOOLS_INSTR_ENCODER: RawrXD_IDE_LaunchInstrEncoder(ide); break;
    case IDM_TOOLS_EXT_MANAGER:   RawrXD_IDE_LaunchExtManager(ide);   break;
    case IDM_TOOLS_OPTIONS:
        MessageBoxW(ide->hWndMain, L"Options dialog - TODO", L"Options", MB_OK);
        break;

    /* ── Help ─────────────────────────────────────────────────────────── */
    case IDM_HELP_ABOUT: RawrXD_IDE_ShowAbout(ide); break;
    case IDM_HELP_DOCS:
        ShellExecuteW(NULL, L"open", L"https://github.com/RawrXD-Project", NULL, NULL, SW_SHOWNORMAL);
        break;

    default:
        break;
    }
}

/*===========================================================================
 * WM_NOTIFY
 *=========================================================================*/
LRESULT RawrXD_IDE_OnNotify(RawrXD_IDE* ide, NMHDR* pnmh) {
    if (!pnmh) return 0;

    switch (pnmh->code) {
    case TVN_SELCHANGEDW:
        if (pnmh->hwndFrom == ide->hWndFileTree)
            RawrXD_IDE_OnTreeSelChanged(ide, (NMTREEVIEWW*)pnmh);
        break;

    case NM_DBLCLK:
        if (pnmh->hwndFrom == ide->hWndFileTree)
            RawrXD_IDE_OnTreeDblClick(ide);
        break;

    case EN_SELCHANGE:
        if (pnmh->hwndFrom == ide->hWndEditor)
            RawrXD_IDE_UpdateLineCol(ide);
        break;

    default:
        break;
    }
    return 0;
}

/*===========================================================================
 * WM_CLOSE / WM_DESTROY
 *=========================================================================*/
void RawrXD_IDE_OnClose(RawrXD_IDE* ide) {
    if (ide->isModified) {
        if (!RawrXD_IDE_PromptSaveChanges(ide))
            return; /* user cancelled */
    }
    DestroyWindow(ide->hWndMain);
}

void RawrXD_IDE_OnDestroy(RawrXD_IDE* ide) {
    /* Kill timers */
    KillTimer(ide->hWndMain, IDT_STATUS_UPDATE);
    KillTimer(ide->hWndMain, IDT_IPC_POLL);
    KillTimer(ide->hWndMain, IDT_AUTOSAVE);

    /* Disconnect IPC */
    RawrXD_IDE_IPCDisconnect(ide);

    /* Stop active build */
    RawrXD_IDE_StopBuild(ide);

    PostQuitMessage(0);
}

/*===========================================================================
 * WM_TIMER
 *=========================================================================*/
void RawrXD_IDE_OnTimer(RawrXD_IDE* ide, UINT_PTR timerId) {
    switch (timerId) {
    case IDT_STATUS_UPDATE:
        RawrXD_IDE_UpdateStatusBar(ide);
        break;

    case IDT_IPC_POLL:
        /* Reconnect if disconnected */
        if (ide->ipcState == IPC_DISCONNECTED || ide->ipcState == IPC_ERROR) {
            RawrXD_IDE_IPCConnect(ide);
        }
        break;

    case IDT_AUTOSAVE:
        if (ide->isModified && !ide->isUntitled) {
            RawrXD_IDE_SaveFile(ide, ide->currentFilePath);
        }
        break;

    default:
        break;
    }
}

/*===========================================================================
 * WM_CTLCOLOR* — theme colors for child controls
 *=========================================================================*/
LRESULT RawrXD_IDE_OnCtlColorEdit(RawrXD_IDE* ide, HDC hdc, HWND hCtrl) {
    SetTextColor(hdc, ide->theme.fgText);

    if (hCtrl == ide->hWndEditor) {
        SetBkColor(hdc, ide->theme.bgEditor);
        return (LRESULT)ide->hBrushEditor;
    }
    if (hCtrl == ide->hWndOutput) {
        SetBkColor(hdc, ide->theme.bgOutput);
        return (LRESULT)ide->hBrushOutput;
    }
    if (hCtrl == ide->hWndWidget) {
        SetBkColor(hdc, ide->theme.bgWidget);
        return (LRESULT)ide->hBrushWidget;
    }

    SetBkColor(hdc, ide->theme.bgWindow);
    return (LRESULT)ide->hBrushBg;
}

LRESULT RawrXD_IDE_OnCtlColorStatic(RawrXD_IDE* ide, HDC hdc, HWND hCtrl) {
    /* Read-only edit controls send WM_CTLCOLORSTATIC */
    return RawrXD_IDE_OnCtlColorEdit(ide, hdc, hCtrl);
}

/*===========================================================================
 * FILE OPERATIONS
 *=========================================================================*/

/* Common file filter for Open/Save dialogs */
static const WCHAR g_FileFilter[] =
    L"Assembly Files (*.asm)\0*.asm\0"
    L"C/C++ Files (*.c;*.cpp;*.h;*.hpp)\0*.c;*.cpp;*.h;*.hpp\0"
    L"All Files (*.*)\0*.*\0\0";

BOOL RawrXD_IDE_FileNew(RawrXD_IDE* ide) {
    if (ide->isModified) {
        if (!RawrXD_IDE_PromptSaveChanges(ide))
            return FALSE;
    }
    SetWindowTextW(ide->hWndEditor, L"");
    ide->currentFilePath[0] = L'\0';
    ide->isModified = FALSE;
    ide->isUntitled = TRUE;
    ide->fileEncoding = 1; /* UTF-8 */
    RawrXD_IDE_UpdateTitle(ide);
    return TRUE;
}

BOOL RawrXD_IDE_FileOpen(RawrXD_IDE* ide) {
    if (ide->isModified) {
        if (!RawrXD_IDE_PromptSaveChanges(ide))
            return FALSE;
    }

    WCHAR filePath[MAX_PATH] = {0};
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = ide->hWndMain;
    ofn.lpstrFilter = g_FileFilter;
    ofn.lpstrFile   = filePath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrTitle  = L"Open File";

    if (!GetOpenFileNameW(&ofn))
        return FALSE;

    return RawrXD_IDE_LoadFile(ide, filePath);
}

BOOL RawrXD_IDE_FileSave(RawrXD_IDE* ide) {
    if (ide->isUntitled)
        return RawrXD_IDE_FileSaveAs(ide);
    return RawrXD_IDE_SaveFile(ide, ide->currentFilePath);
}

BOOL RawrXD_IDE_FileSaveAs(RawrXD_IDE* ide) {
    WCHAR filePath[MAX_PATH] = {0};
    if (!ide->isUntitled)
        StringCchCopyW(filePath, MAX_PATH, ide->currentFilePath);

    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = ide->hWndMain;
    ofn.lpstrFilter  = g_FileFilter;
    ofn.lpstrFile    = filePath;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_OVERWRITEPROMPT | OFN_EXPLORER;
    ofn.lpstrDefExt  = L"asm";
    ofn.lpstrTitle   = L"Save As";

    if (!GetSaveFileNameW(&ofn))
        return FALSE;

    if (RawrXD_IDE_SaveFile(ide, filePath)) {
        StringCchCopyW(ide->currentFilePath, MAX_PATH, filePath);
        ide->isUntitled = FALSE;
        RawrXD_IDE_UpdateTitle(ide);
        return TRUE;
    }
    return FALSE;
}

BOOL RawrXD_IDE_FileClose(RawrXD_IDE* ide) {
    return RawrXD_IDE_FileNew(ide);
}

BOOL RawrXD_IDE_LoadFile(RawrXD_IDE* ide, const WCHAR* path) {
    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        WCHAR msg[MAX_PATH + 64];
        StringCchPrintfW(msg, MAX_PATH + 64, L"Cannot open file:\n%s", path);
        MessageBoxW(ide->hWndMain, msg, L"Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE || fileSize > 64 * 1024 * 1024) {
        CloseHandle(hFile);
        MessageBoxW(ide->hWndMain, L"File too large (max 64 MB).", L"Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    BYTE* rawData = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, fileSize + 4);
    if (!rawData) {
        CloseHandle(hFile);
        return FALSE;
    }

    DWORD bytesRead = 0;
    ReadFile(hFile, rawData, fileSize, &bytesRead, NULL);
    CloseHandle(hFile);

    /* Detect encoding */
    DWORD enc = 1; /* default UTF-8 */
    IDE_AutoDetectEncoding(rawData, bytesRead, &enc);
    ide->fileEncoding = enc;

    /* Convert to wide string */
    WCHAR* wideText = NULL;
    int wideLen = 0;

    if (enc == 2 && bytesRead >= 2) {
        /* UTF-16 LE with BOM */
        int skip = (rawData[0] == 0xFF && rawData[1] == 0xFE) ? 2 : 0;
        wideLen  = (int)((bytesRead - skip) / sizeof(WCHAR));
        wideText = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                     (wideLen + 1) * sizeof(WCHAR));
        if (wideText)
            memcpy(wideText, rawData + skip, wideLen * sizeof(WCHAR));
    } else {
        /* UTF-8 or ANSI */
        UINT cp = (enc == 1) ? CP_UTF8 : CP_ACP;
        int skip = 0;
        if (enc == 1 && bytesRead >= 3 &&
            rawData[0] == 0xEF && rawData[1] == 0xBB && rawData[2] == 0xBF)
            skip = 3; /* skip UTF-8 BOM */

        wideLen = MultiByteToWideChar(cp, 0, (char*)(rawData + skip),
                                      (int)(bytesRead - skip), NULL, 0);
        wideText = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                     (wideLen + 1) * sizeof(WCHAR));
        if (wideText)
            MultiByteToWideChar(cp, 0, (char*)(rawData + skip),
                                (int)(bytesRead - skip), wideText, wideLen);
    }

    HeapFree(GetProcessHeap(), 0, rawData);

    if (wideText) {
        /* Set text into editor. Use streaming for large files. */
        SetWindowTextW(ide->hWndEditor, wideText);
        HeapFree(GetProcessHeap(), 0, wideText);
    }

    StringCchCopyW(ide->currentFilePath, MAX_PATH, path);
    ide->isModified = FALSE;
    ide->isUntitled = FALSE;
    RawrXD_IDE_UpdateTitle(ide);

    /* Output log */
    WCHAR logMsg[MAX_PATH + 32];
    StringCchPrintfW(logMsg, MAX_PATH + 32, L"Opened: %s\r\n", path);
    RawrXD_IDE_OutputAppend(ide, logMsg);

    /* Scroll to top */
    SendMessage(ide->hWndEditor, EM_SETSEL, 0, 0);
    SendMessage(ide->hWndEditor, EM_SCROLLCARET, 0, 0);

    return TRUE;
}

BOOL RawrXD_IDE_SaveFile(RawrXD_IDE* ide, const WCHAR* path) {
    /* Get text length */
    GETTEXTLENGTHEX gtl;
    gtl.flags    = GTL_DEFAULT | GTL_NUMCHARS;
    gtl.codepage = 1200; /* Unicode */
    int textLen = (int)SendMessage(ide->hWndEditor, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
    if (textLen <= 0) textLen = GetWindowTextLengthW(ide->hWndEditor);

    WCHAR* wideText = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                        (textLen + 2) * sizeof(WCHAR));
    if (!wideText) return FALSE;

    /* Try EM_GETTEXTEX first (RichEdit), fallback GetWindowTextW */
    GETTEXTEX gte;
    gte.cb            = (DWORD)((textLen + 1) * sizeof(WCHAR));
    gte.flags         = GT_DEFAULT;
    gte.codepage      = 1200;
    gte.lpDefaultChar  = NULL;
    gte.lpUsedDefChar  = NULL;
    LRESULT got = SendMessage(ide->hWndEditor, EM_GETTEXTEX, (WPARAM)&gte, (LPARAM)wideText);
    if (got <= 0) {
        GetWindowTextW(ide->hWndEditor, wideText, textLen + 1);
    }

    /* Convert to UTF-8 */
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wideText, -1, NULL, 0, NULL, NULL);
    char* utf8 = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, utf8Len + 4);
    if (!utf8) {
        HeapFree(GetProcessHeap(), 0, wideText);
        return FALSE;
    }
    WideCharToMultiByte(CP_UTF8, 0, wideText, -1, utf8, utf8Len, NULL, NULL);
    HeapFree(GetProcessHeap(), 0, wideText);

    /* Write file */
    HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        HeapFree(GetProcessHeap(), 0, utf8);
        MessageBoxW(ide->hWndMain, L"Cannot write file.", L"Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    /* Write UTF-8 BOM + data */
    DWORD written;
    BYTE bom[3] = { 0xEF, 0xBB, 0xBF };
    WriteFile(hFile, bom, 3, &written, NULL);
    WriteFile(hFile, utf8, (DWORD)(utf8Len - 1), &written, NULL); /* -1 to skip null term */
    CloseHandle(hFile);
    HeapFree(GetProcessHeap(), 0, utf8);

    ide->isModified = FALSE;
    ide->fileEncoding = 1; /* UTF-8 */
    RawrXD_IDE_UpdateTitle(ide);

    WCHAR logMsg[MAX_PATH + 32];
    StringCchPrintfW(logMsg, MAX_PATH + 32, L"Saved: %s\r\n", path);
    RawrXD_IDE_OutputAppend(ide, logMsg);

    return TRUE;
}

BOOL RawrXD_IDE_PromptSaveChanges(RawrXD_IDE* ide) {
    int result = MessageBoxW(ide->hWndMain,
        L"Current file has unsaved changes.\nDo you want to save?",
        L"Save Changes",
        MB_YESNOCANCEL | MB_ICONQUESTION);

    if (result == IDCANCEL)
        return FALSE;
    if (result == IDYES)
        return RawrXD_IDE_FileSave(ide);
    return TRUE; /* IDNO — discard */
}

static void IDE_AutoDetectEncoding(const BYTE* data, DWORD size, DWORD* outEncoding) {
    if (size >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        *outEncoding = 2; /* UTF-16 LE */
        return;
    }
    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        *outEncoding = 1; /* UTF-8 BOM */
        return;
    }
    /* Heuristic: check for high bytes that suggest UTF-8 multi-byte */
    BOOL hasHighBytes = FALSE;
    for (DWORD i = 0; i < size && i < 8192; i++) {
        if (data[i] & 0x80) { hasHighBytes = TRUE; break; }
    }
    if (hasHighBytes) {
        /* Try MultiByteToWideChar with MB_ERR_INVALID_CHARS */
        int r = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                    (const char*)data, (int)size, NULL, 0);
        *outEncoding = (r > 0) ? 1 : 0; /* UTF-8 if valid, else ANSI */
    } else {
        *outEncoding = 1; /* ASCII is valid UTF-8 */
    }
}

/*===========================================================================
 * EDIT OPERATIONS
 *=========================================================================*/
void RawrXD_IDE_EditUndo(RawrXD_IDE* ide) {
    SendMessage(ide->hWndEditor, EM_UNDO, 0, 0);
}

void RawrXD_IDE_EditRedo(RawrXD_IDE* ide) {
    SendMessage(ide->hWndEditor, EM_REDO, 0, 0);
}

void RawrXD_IDE_EditCut(RawrXD_IDE* ide) {
    SendMessage(ide->hWndEditor, WM_CUT, 0, 0);
}

void RawrXD_IDE_EditCopy(RawrXD_IDE* ide) {
    SendMessage(ide->hWndEditor, WM_COPY, 0, 0);
}

void RawrXD_IDE_EditPaste(RawrXD_IDE* ide) {
    SendMessage(ide->hWndEditor, WM_PASTE, 0, 0);
}

void RawrXD_IDE_EditSelectAll(RawrXD_IDE* ide) {
    SendMessage(ide->hWndEditor, EM_SETSEL, 0, -1);
}

void RawrXD_IDE_EditFind(RawrXD_IDE* ide) {
    /* Simple find dialog using FindText common dialog */
    static FINDREPLACEW fr;
    ZeroMemory(&fr, sizeof(fr));
    fr.lStructSize = sizeof(fr);
    fr.hwndOwner   = ide->hWndMain;
    fr.lpstrFindWhat = ide->findState.searchText;
    fr.wFindWhatLen  = (WORD)(sizeof(ide->findState.searchText) / sizeof(WCHAR));
    fr.Flags = FR_DOWN;

    ide->findState.hFindDlg = FindTextW(&fr);
}

void RawrXD_IDE_EditReplace(RawrXD_IDE* ide) {
    static FINDREPLACEW fr;
    ZeroMemory(&fr, sizeof(fr));
    fr.lStructSize    = sizeof(fr);
    fr.hwndOwner      = ide->hWndMain;
    fr.lpstrFindWhat  = ide->findState.searchText;
    fr.wFindWhatLen   = (WORD)(sizeof(ide->findState.searchText) / sizeof(WCHAR));
    fr.lpstrReplaceWith = ide->findState.replaceText;
    fr.wReplaceWithLen  = (WORD)(sizeof(ide->findState.replaceText) / sizeof(WCHAR));
    fr.Flags = FR_DOWN;

    ide->findState.hFindDlg = ReplaceTextW(&fr);
}

/* GoToLine dialog callback */
static INT_PTR CALLBACK GoToLineDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, lParam);
        int totalLines = (int)lParam;
        WCHAR label[64];
        StringCchPrintfW(label, 64, L"Line number (1-%d):", totalLines);
        SetDlgItemTextW(hDlg, 101, label);
        SetDlgItemTextW(hDlg, 102, L"1");
        SendDlgItemMessageW(hDlg, 102, EM_SETSEL, 0, -1);
        SetFocus(GetDlgItem(hDlg, 102));
        return FALSE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            WCHAR buf[32];
            GetDlgItemTextW(hDlg, 102, buf, 32);
            int line = _wtoi(buf);
            if (line < 1) line = 1;
            EndDialog(hDlg, (INT_PTR)line);
            return TRUE;
        } else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, 0);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hDlg, 0);
        return TRUE;
    }
    return FALSE;
}

/* Build GoToLine dialog template in memory (no .rc dependency) */
static INT_PTR ShowGoToLineDialog(HWND hParent, int totalLines) {
    /* DLGTEMPLATE + 3 controls: static label, edit box, OK button */
    _Alignas(4) BYTE dlgBuf[1024];
    ZeroMemory(dlgBuf, sizeof(dlgBuf));
    BYTE* p = dlgBuf;

    /* DLGTEMPLATE */
    DLGTEMPLATE* pDlg = (DLGTEMPLATE*)p;
    pDlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    pDlg->cdit  = 3; /* 3 controls */
    pDlg->cx    = 180;
    pDlg->cy    = 60;
    p += sizeof(DLGTEMPLATE);

    /* Menu (none) */
    *(WORD*)p = 0; p += sizeof(WORD);
    /* Class (default) */
    *(WORD*)p = 0; p += sizeof(WORD);
    /* Title: "Go To Line" */
    const WCHAR title[] = L"Go To Line";
    memcpy(p, title, sizeof(title));
    p += sizeof(title);

    /* Align to DWORD for first control */
    p = (BYTE*)((ULONG_PTR)(p + 3) & ~3);

    /* Control 1: Static label (ID 101) */
    DLGITEMTEMPLATE* pItem = (DLGITEMTEMPLATE*)p;
    pItem->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
    pItem->x = 7; pItem->y = 7; pItem->cx = 166; pItem->cy = 10;
    pItem->id = 101;
    p += sizeof(DLGITEMTEMPLATE);
    *(WORD*)p = 0xFFFF; p += sizeof(WORD);
    *(WORD*)p = 0x0082; p += sizeof(WORD); /* Static class */
    *(WORD*)p = 0; p += sizeof(WORD); /* Empty text (set in WM_INITDIALOG) */
    *(WORD*)p = 0; p += sizeof(WORD); /* No creation data */

    p = (BYTE*)((ULONG_PTR)(p + 3) & ~3);

    /* Control 2: Edit box (ID 102) */
    pItem = (DLGITEMTEMPLATE*)p;
    pItem->style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_NUMBER;
    pItem->x = 7; pItem->y = 20; pItem->cx = 166; pItem->cy = 14;
    pItem->id = 102;
    p += sizeof(DLGITEMTEMPLATE);
    *(WORD*)p = 0xFFFF; p += sizeof(WORD);
    *(WORD*)p = 0x0081; p += sizeof(WORD); /* Edit class */
    *(WORD*)p = 0; p += sizeof(WORD);
    *(WORD*)p = 0; p += sizeof(WORD);

    p = (BYTE*)((ULONG_PTR)(p + 3) & ~3);

    /* Control 3: OK button (IDOK) */
    pItem = (DLGITEMTEMPLATE*)p;
    pItem->style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
    pItem->x = 64; pItem->y = 40; pItem->cx = 50; pItem->cy = 14;
    pItem->id = IDOK;
    p += sizeof(DLGITEMTEMPLATE);
    *(WORD*)p = 0xFFFF; p += sizeof(WORD);
    *(WORD*)p = 0x0080; p += sizeof(WORD); /* Button class */
    const WCHAR okText[] = L"Go";
    memcpy(p, okText, sizeof(okText));
    p += sizeof(okText);
    *(WORD*)p = 0; p += sizeof(WORD);

    return DialogBoxIndirectParamW(
        GetModuleHandleW(NULL),
        (DLGTEMPLATE*)dlgBuf,
        hParent,
        GoToLineDlgProc,
        (LPARAM)totalLines
    );
}

void RawrXD_IDE_EditGotoLine(RawrXD_IDE* ide) {
    int totalLines = (int)SendMessage(ide->hWndEditor, EM_GETLINECOUNT, 0, 0);

    INT_PTR result = ShowGoToLineDialog(ide->hWndMain, totalLines);
    if (result <= 0) return; /* Cancelled */

    int targetLine = (int)result;
    if (targetLine > totalLines) targetLine = totalLines;

    int charIndex = (int)SendMessage(ide->hWndEditor, EM_LINEINDEX, (WPARAM)(targetLine - 1), 0);
    if (charIndex >= 0) {
        SendMessage(ide->hWndEditor, EM_SETSEL, (WPARAM)charIndex, (LPARAM)charIndex);
        SendMessage(ide->hWndEditor, EM_SCROLLCARET, 0, 0);
    }
}

/*===========================================================================
 * BUILD OPERATIONS
 *=========================================================================*/
typedef struct BuildThreadData {
    RawrXD_IDE* ide;
    WCHAR       cmdLine[2048];
    BOOL        isClean;
} BuildThreadData;

static BuildThreadData g_BuildData;

void RawrXD_IDE_BuildProject(RawrXD_IDE* ide) {
    if (ide->buildState == BUILD_RUNNING) {
        RawrXD_IDE_OutputAppend(ide, L"Build already in progress.\r\n");
        return;
    }

    /* Save current file first */
    if (ide->isModified && !ide->isUntitled)
        RawrXD_IDE_FileSave(ide);

    /* Determine source file */
    const WCHAR* srcFile = ide->currentFilePath;
    if (ide->isUntitled || srcFile[0] == L'\0') {
        RawrXD_IDE_OutputAppend(ide, L"No file to build. Save first.\r\n");
        return;
    }

    /* Build command: use ml64 for .asm, cl for .cpp */
    WCHAR ext[16] = {0};
    const WCHAR* dot = wcsrchr(srcFile, L'.');
    if (dot) StringCchCopyW(ext, 16, dot);

    RawrXD_IDE_OutputClear(ide);
    RawrXD_IDE_OutputAppend(ide, L"=== BUILD STARTED ===\r\n");

    g_BuildData.ide     = ide;
    g_BuildData.isClean = FALSE;

    if (_wcsicmp(ext, L".asm") == 0) {
        /* Extract dir and base name */
        WCHAR dir[MAX_PATH], fname[MAX_PATH];
        StringCchCopyW(dir, MAX_PATH, srcFile);
        WCHAR* lastSlash = wcsrchr(dir, L'\\');
        if (lastSlash) {
            *lastSlash = L'\0';
            StringCchCopyW(fname, MAX_PATH, lastSlash + 1);
        } else {
            StringCchCopyW(fname, MAX_PATH, srcFile);
            dir[0] = L'.'; dir[1] = L'\0';
        }

        /* Remove extension from fname */
        WCHAR* fDot = wcsrchr(fname, L'.');
        if (fDot) *fDot = L'\0';

        StringCchPrintfW(g_BuildData.cmdLine, 2048,
            L"cmd /c \"cd /d \"%s\" && ml64.exe /nologo /c /Fo\"%s.obj\" \"%s\" "
            L"&& link.exe /nologo /subsystem:console /entry:main \"%s.obj\" "
            L"kernel32.lib user32.lib /out:\"%s.exe\"\"",
            dir, fname, srcFile, fname, fname);
    } else {
        StringCchPrintfW(g_BuildData.cmdLine, 2048,
            L"cmd /c \"cl.exe /nologo /W4 /O2 /Fe:\"%s\" \"%s\" "
            L"user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib shlwapi.lib\"",
            srcFile, srcFile);
    }

    ide->buildState = BUILD_RUNNING;
    ide->hBuildThread = CreateThread(NULL, 0, RawrXD_IDE_BuildThread, &g_BuildData, 0, NULL);

    /* Grey out build, enable stop */
    EnableMenuItem(ide->hMenuBar, IDM_BUILD_BUILD, MF_GRAYED);
    EnableMenuItem(ide->hMenuBar, IDM_BUILD_STOP,  MF_ENABLED);
}

void RawrXD_IDE_RebuildProject(RawrXD_IDE* ide) {
    RawrXD_IDE_CleanProject(ide);
    RawrXD_IDE_BuildProject(ide);
}

void RawrXD_IDE_RunProject(RawrXD_IDE* ide) {
    if (ide->isUntitled) {
        RawrXD_IDE_OutputAppend(ide, L"No file loaded.\r\n");
        return;
    }

    /* Derive exe name from source */
    WCHAR exePath[MAX_PATH];
    StringCchCopyW(exePath, MAX_PATH, ide->currentFilePath);
    WCHAR* dot = wcsrchr(exePath, L'.');
    if (dot) {
        *dot = L'\0';
        StringCchCatW(exePath, MAX_PATH, L".exe");
    }

    if (GetFileAttributesW(exePath) == INVALID_FILE_ATTRIBUTES) {
        RawrXD_IDE_OutputAppend(ide, L"Executable not found. Build first.\r\n");
        return;
    }

    RawrXD_IDE_OutputAppend(ide, L"=== RUN ===\r\n");

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    if (CreateProcessW(exePath, NULL, NULL, NULL, FALSE,
                       CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        WCHAR msg[MAX_PATH + 32];
        StringCchPrintfW(msg, MAX_PATH + 32, L"Started: %s (PID %lu)\r\n", exePath, pi.dwProcessId);
        RawrXD_IDE_OutputAppend(ide, msg);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else {
        RawrXD_IDE_OutputAppend(ide, L"Failed to launch executable.\r\n");
    }
}

void RawrXD_IDE_CleanProject(RawrXD_IDE* ide) {
    if (ide->isUntitled) return;

    WCHAR basePath[MAX_PATH];
    StringCchCopyW(basePath, MAX_PATH, ide->currentFilePath);
    WCHAR* dot = wcsrchr(basePath, L'.');
    if (!dot) return;
    *dot = L'\0';

    WCHAR objPath[MAX_PATH], exePath[MAX_PATH];
    StringCchPrintfW(objPath, MAX_PATH, L"%s.obj", basePath);
    StringCchPrintfW(exePath, MAX_PATH, L"%s.exe", basePath);

    DeleteFileW(objPath);
    DeleteFileW(exePath);

    RawrXD_IDE_OutputAppend(ide, L"Clean complete.\r\n");
}

void RawrXD_IDE_StopBuild(RawrXD_IDE* ide) {
    /* Atomically swap handle to NULL to prevent double-close race */
    HANDLE hProc = (HANDLE)InterlockedExchangePointer((PVOID*)&ide->hBuildProcess, NULL);
    if (hProc) {
        TerminateProcess(hProc, 1);
        CloseHandle(hProc);
    }
    if (ide->hBuildThread) {
        /* Use MsgWaitForMultipleObjects to avoid deadlock with SendMessage from build thread */
        DWORD result;
        do {
            result = MsgWaitForMultipleObjects(1, &ide->hBuildThread, FALSE, 1000, QS_ALLINPUT);
            if (result == WAIT_OBJECT_0 + 1) {
                /* Process messages to unblock any pending SendMessage */
                MSG msg;
                while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        } while (result == WAIT_OBJECT_0 + 1);
        CloseHandle(ide->hBuildThread);
        ide->hBuildThread = NULL;
    }
    ide->buildState = BUILD_IDLE;
    RawrXD_IDE_OutputAppend(ide, L"Build stopped.\r\n");
    EnableMenuItem(ide->hMenuBar, IDM_BUILD_BUILD, MF_ENABLED);
    EnableMenuItem(ide->hMenuBar, IDM_BUILD_STOP,  MF_GRAYED);
}

DWORD WINAPI RawrXD_IDE_BuildThread(LPVOID param) {
    BuildThreadData* data = (BuildThreadData*)param;
    RawrXD_IDE* ide = data->ide;

    SECURITY_ATTRIBUTES sa;
    sa.nLength              = sizeof(sa);
    sa.bInheritHandle       = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE hReadPipe = NULL, hWritePipe = NULL;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        ide->buildState = BUILD_FAILED;
        return 1;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput  = hWritePipe;
    si.hStdError   = hWritePipe;
    si.wShowWindow = SW_HIDE;

    BOOL ok = CreateProcessW(NULL, data->cmdLine, NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(hWritePipe);

    if (!ok) {
        CloseHandle(hReadPipe);
        ide->buildState = BUILD_FAILED;
        PostMessage(ide->hWndMain, WM_TIMER, IDT_STATUS_UPDATE, 0); /* force update */
        return 1;
    }

    ide->hBuildProcess = pi.hProcess;

    /* Read build output */
    IDE_ReadBuildOutput(ide, hReadPipe);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(hReadPipe);

    /* Atomically clear handle to prevent race with StopBuild */
    InterlockedExchangePointer((PVOID*)&ide->hBuildProcess, NULL);

    if (exitCode == 0) {
        ide->buildState = BUILD_SUCCESS;
        /* Post output on UI thread */
        RawrXD_IDE_OutputAppend(ide, L"\r\n=== BUILD SUCCEEDED ===\r\n");
    } else {
        ide->buildState = BUILD_FAILED;
        RawrXD_IDE_OutputAppend(ide, L"\r\n=== BUILD FAILED ===\r\n");
    }

    /* Signal UI thread to re-enable menu items (no cross-thread menu calls) */
    PostMessage(ide->hWndMain, WM_APP + 100, 0, 0);

    return 0;
}

static void IDE_ReadBuildOutput(RawrXD_IDE* ide, HANDLE hRead) {
    char buf[512];
    DWORD bytesRead;
    while (ReadFile(hRead, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        /* Convert to wide */
        WCHAR wBuf[512];
        MultiByteToWideChar(CP_ACP, 0, buf, -1, wBuf, 512);
        RawrXD_IDE_OutputAppend(ide, wBuf);
    }
}

/*===========================================================================
 * TOOL LAUNCHERS
 *=========================================================================*/
void RawrXD_IDE_LaunchPEInspector(RawrXD_IDE* ide) {
    /* Check if dumpbin_final.asm's EXE exists, or fall back to dumpbin.exe */
    WCHAR toolPath[MAX_PATH] = L"D:\\rawrxd\\src\\dumpbin_final.exe";
    if (GetFileAttributesW(toolPath) == INVALID_FILE_ATTRIBUTES) {
        StringCchCopyW(toolPath, MAX_PATH, L"dumpbin.exe");
    }

    if (!ide->isUntitled && ide->currentFilePath[0]) {
        WCHAR cmd[MAX_PATH * 2 + 32];
        StringCchPrintfW(cmd, MAX_PATH * 2 + 32, L"\"%s\" /headers \"%s\"", toolPath, ide->currentFilePath);
        ShellExecuteW(NULL, L"open", L"cmd.exe", cmd, NULL, SW_SHOWNORMAL);
    } else {
        RawrXD_IDE_OutputAppend(ide, L"PE Inspector: No file loaded.\r\n");
    }
}

void RawrXD_IDE_LaunchInstrEncoder(RawrXD_IDE* ide) {
    /* Launch the instruction encoder tool */
    WCHAR toolPath[MAX_PATH] = L"D:\\rawrxd\\src\\asm\\RawrXD_InstrEncoder.exe";
    if (GetFileAttributesW(toolPath) != INVALID_FILE_ATTRIBUTES) {
        ShellExecuteW(NULL, L"open", toolPath, NULL, L"D:\\rawrxd\\src", SW_SHOWNORMAL);
    } else {
        RawrXD_IDE_OutputAppend(ide, L"Instruction Encoder not found.\r\n");
    }
}

void RawrXD_IDE_LaunchExtManager(RawrXD_IDE* ide) {
    /* Launch extension manager (PowerShell script) */
    WCHAR cmd[MAX_PATH * 2];
    StringCchPrintfW(cmd, MAX_PATH * 2,
        L"/c powershell.exe -ExecutionPolicy Bypass -File \"D:\\rawrxd\\src\\RawrXD-CLI.ps1\" extensions list");
    ShellExecuteW(NULL, L"open", L"cmd.exe", cmd, L"D:\\rawrxd", SW_SHOWNORMAL);
}

/*===========================================================================
 * IPC — Named Pipe Client to \\.\pipe\RawrXD_WidgetIntelligence
 *=========================================================================*/
BOOL RawrXD_IDE_IPCConnect(RawrXD_IDE* ide) {
    if (ide->ipcState == IPC_CONNECTED)
        return TRUE;

    ide->ipcState = IPC_CONNECTING;

    /* Try to connect to the pipe */
    ide->hPipe = CreateFileW(
        RAWRXD_PIPE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,               /* Synchronous I/O — no OVERLAPPED */
        NULL
    );

    if (ide->hPipe == INVALID_HANDLE_VALUE) {
        ide->hPipe    = NULL;
        ide->ipcState = IPC_DISCONNECTED;
        return FALSE;
    }

    /* Set pipe mode to message mode */
    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(ide->hPipe, &mode, NULL, NULL);

    ide->ipcState  = IPC_CONNECTED;
    ide->ipcRunning = TRUE;

    /* Start reader thread */
    ide->hIPCThread = CreateThread(NULL, 0, RawrXD_IDE_IPCThread, ide, 0, NULL);

    RawrXD_IDE_WidgetAppend(ide, L"[IPC] Connected to WidgetIntelligence\r\n");

    return TRUE;
}

void RawrXD_IDE_IPCDisconnect(RawrXD_IDE* ide) {
    ide->ipcRunning = FALSE;

    if (ide->hPipe) {
        /* Cancel pending I/O */
        CancelIo(ide->hPipe);
        CloseHandle(ide->hPipe);
        ide->hPipe = NULL;
    }

    if (ide->hIPCThread) {
        WaitForSingleObject(ide->hIPCThread, 2000);
        CloseHandle(ide->hIPCThread);
        ide->hIPCThread = NULL;
    }

    ide->ipcState = IPC_DISCONNECTED;
}

BOOL RawrXD_IDE_IPCSend(RawrXD_IDE* ide, const WCHAR* message) {
    if (ide->ipcState != IPC_CONNECTED || !ide->hPipe)
        return FALSE;

    /* Convert to UTF-8 for the pipe protocol */
    char utf8Buf[RAWRXD_PIPE_BUFFER_SIZE];
    int len = WideCharToMultiByte(CP_UTF8, 0, message, -1, utf8Buf,
                                  RAWRXD_PIPE_BUFFER_SIZE - 1, NULL, NULL);
    if (len <= 0) return FALSE;

    DWORD written;
    BOOL ok = WriteFile(ide->hPipe, utf8Buf, (DWORD)(len - 1), &written, NULL);
    if (!ok) {
        ide->ipcState = IPC_ERROR;
        return FALSE;
    }
    return TRUE;
}

DWORD WINAPI RawrXD_IDE_IPCThread(LPVOID param) {
    RawrXD_IDE* ide = (RawrXD_IDE*)param;
    char buf[RAWRXD_PIPE_BUFFER_SIZE];

    while (ide->ipcRunning && ide->hPipe) {
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(ide->hPipe, buf, sizeof(buf) - 1, &bytesRead, NULL);

        if (!ok || bytesRead == 0) {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
                ide->ipcState = IPC_DISCONNECTED;
                RawrXD_IDE_WidgetAppend(ide, L"[IPC] Disconnected.\r\n");
                break;
            }
            Sleep(100);
            continue;
        }

        buf[bytesRead] = '\0';

        /* Convert UTF-8 response to wide */
        WCHAR wBuf[RAWRXD_PIPE_BUFFER_SIZE];
        MultiByteToWideChar(CP_UTF8, 0, buf, -1, wBuf, RAWRXD_PIPE_BUFFER_SIZE);

        /* Display in widget panel */
        RawrXD_IDE_WidgetAppend(ide, wBuf);
        RawrXD_IDE_WidgetAppend(ide, L"\r\n");
    }

    return 0;
}

/*===========================================================================
 * FILE TREE
 *=========================================================================*/
static HTREEITEM IDE_TreeAddItem(HWND hTree, HTREEITEM hParent, const WCHAR* text, BOOL isFolder) {
    TVINSERTSTRUCTW tvis;
    ZeroMemory(&tvis, sizeof(tvis));
    tvis.hParent      = hParent;
    tvis.hInsertAfter = TVI_SORT;
    tvis.item.mask    = TVIF_TEXT | TVIF_CHILDREN;
    tvis.item.pszText = (LPWSTR)text;
    tvis.item.cChildren = isFolder ? 1 : 0;
    return TreeView_InsertItem(hTree, &tvis);
}

void RawrXD_IDE_PopulateTree(RawrXD_IDE* ide, const WCHAR* rootPath) {
    if (!ide->hWndFileTree) return;
    TreeView_DeleteAllItems(ide->hWndFileTree);

    /* Root item */
    const WCHAR* rootName = wcsrchr(rootPath, L'\\');
    rootName = rootName ? rootName + 1 : rootPath;

    HTREEITEM hRoot = IDE_TreeAddItem(ide->hWndFileTree, TVI_ROOT,
                                       rootName, TRUE);

    /* Expand first level */
    RawrXD_IDE_PopulateTreeItem(ide, hRoot, rootPath);
    TreeView_Expand(ide->hWndFileTree, hRoot, TVE_EXPAND);
}

void RawrXD_IDE_PopulateTreeItem(RawrXD_IDE* ide, HTREEITEM hParent, const WCHAR* path) {
    WCHAR searchPath[MAX_PATH + 4];
    StringCchPrintfW(searchPath, MAX_PATH + 4, L"%s\\*", path);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    int itemCount = 0;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        /* Skip hidden/system files */
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))
            continue;

        BOOL isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        /* For files, only show source-related extensions */
        if (!isDir && !RawrXD_IDE_IsSourceFile(fd.cFileName))
            continue;

        IDE_TreeAddItem(ide->hWndFileTree, hParent, fd.cFileName, isDir);
        itemCount++;

        if (itemCount > 500) break; /* safety limit */
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

BOOL RawrXD_IDE_IsSourceFile(const WCHAR* path) {
    const WCHAR* dot = wcsrchr(path, L'.');
    if (!dot) return FALSE;

    const WCHAR* srcExts[] = {
        L".asm", L".inc", L".c", L".cpp", L".h", L".hpp",
        L".def", L".rc",  L".bat", L".ps1", L".py",
        L".md",  L".txt", L".json", L".xml", L".toml",
        L".cs",  L".rs",  L".mak",  L".cmake",
        NULL
    };

    for (int i = 0; srcExts[i]; i++) {
        if (_wcsicmp(dot, srcExts[i]) == 0) return TRUE;
    }
    return FALSE;
}

void RawrXD_IDE_OnTreeSelChanged(RawrXD_IDE* ide, NMTREEVIEWW* pnmtv) {
    (void)pnmtv;
    /* Update status bar with selected item */
    HTREEITEM hSel = TreeView_GetSelection(ide->hWndFileTree);
    if (!hSel) return;

    WCHAR itemText[MAX_PATH];
    TVITEMW tvi;
    ZeroMemory(&tvi, sizeof(tvi));
    tvi.hItem      = hSel;
    tvi.mask       = TVIF_TEXT;
    tvi.pszText    = itemText;
    tvi.cchTextMax = MAX_PATH;
    TreeView_GetItem(ide->hWndFileTree, &tvi);

    /* Show in output */
    WCHAR msg[MAX_PATH + 32];
    StringCchPrintfW(msg, MAX_PATH + 32, L"Selected: %s\r\n", itemText);
    /* Don't spam the output — just update status bar silently */
}

void RawrXD_IDE_OnTreeDblClick(RawrXD_IDE* ide) {
    /* Build full path from tree hierarchy */
    HTREEITEM hSel = TreeView_GetSelection(ide->hWndFileTree);
    if (!hSel) return;

    /* Check if it's a folder (has children) */
    TVITEMW tvi;
    WCHAR itemText[MAX_PATH];
    ZeroMemory(&tvi, sizeof(tvi));
    tvi.hItem      = hSel;
    tvi.mask       = TVIF_TEXT | TVIF_CHILDREN;
    tvi.pszText    = itemText;
    tvi.cchTextMax = MAX_PATH;
    TreeView_GetItem(ide->hWndFileTree, &tvi);

    if (tvi.cChildren > 0) {
        /* Expand/populate folder on demand */
        TreeView_Expand(ide->hWndFileTree, hSel, TVE_TOGGLE);
        return;
    }

    /* Build path by walking up the tree */
    WCHAR segments[16][MAX_PATH];
    int depth = 0;
    HTREEITEM hItem = hSel;
    while (hItem && depth < 16) {
        TVITEMW tv2;
        ZeroMemory(&tv2, sizeof(tv2));
        tv2.hItem      = hItem;
        tv2.mask       = TVIF_TEXT;
        tv2.pszText    = segments[depth];
        tv2.cchTextMax = MAX_PATH;
        TreeView_GetItem(ide->hWndFileTree, &tv2);
        depth++;
        hItem = TreeView_GetParent(ide->hWndFileTree, hItem);
    }

    /* Reconstruct path: root is "src" → D:\rawrxd\src\... */
    WCHAR fullPath[MAX_PATH * 2] = L"D:\\rawrxd\\";
    for (int i = depth - 1; i >= 0; i--) {
        StringCchCatW(fullPath, MAX_PATH * 2, segments[i]);
        if (i > 0) StringCchCatW(fullPath, MAX_PATH * 2, L"\\");
    }

    /* Open the file */
    if (GetFileAttributesW(fullPath) != INVALID_FILE_ATTRIBUTES) {
        RawrXD_IDE_LoadFile(ide, fullPath);
    }
}

/*===========================================================================
 * OUTPUT / WIDGET PANELS
 *=========================================================================*/
void RawrXD_IDE_OutputAppend(RawrXD_IDE* ide, const WCHAR* text) {
    if (!ide->hWndOutput) return;

    /* Move caret to end and append */
    int len = GetWindowTextLengthW(ide->hWndOutput);
    SendMessage(ide->hWndOutput, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessage(ide->hWndOutput, EM_REPLACESEL, FALSE, (LPARAM)text);

    /* Auto-scroll to bottom */
    SendMessage(ide->hWndOutput, WM_VSCROLL, SB_BOTTOM, 0);
}

void RawrXD_IDE_OutputClear(RawrXD_IDE* ide) {
    if (ide->hWndOutput)
        SetWindowTextW(ide->hWndOutput, L"");
}

void RawrXD_IDE_WidgetAppend(RawrXD_IDE* ide, const WCHAR* text) {
    if (!ide->hWndWidget) return;

    int len = GetWindowTextLengthW(ide->hWndWidget);
    SendMessage(ide->hWndWidget, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessage(ide->hWndWidget, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessage(ide->hWndWidget, WM_VSCROLL, SB_BOTTOM, 0);
}

void RawrXD_IDE_WidgetClear(RawrXD_IDE* ide) {
    if (ide->hWndWidget)
        SetWindowTextW(ide->hWndWidget, L"");
}

/*===========================================================================
 * TITLE BAR
 *=========================================================================*/
void RawrXD_IDE_UpdateTitle(RawrXD_IDE* ide) {
    WCHAR title[MAX_PATH + 64];
    if (ide->isUntitled) {
        StringCchPrintfW(title, MAX_PATH + 64, L"%sUntitled - %s",
                         ide->isModified ? L"* " : L"", RAWRXD_IDE_TITLE);
    } else {
        const WCHAR* fileName = wcsrchr(ide->currentFilePath, L'\\');
        fileName = fileName ? fileName + 1 : ide->currentFilePath;
        StringCchPrintfW(title, MAX_PATH + 64, L"%s%s - %s",
                         ide->isModified ? L"* " : L"", fileName, RAWRXD_IDE_TITLE);
    }
    SetWindowTextW(ide->hWndMain, title);
}

/*===========================================================================
 * ABOUT DIALOG
 *=========================================================================*/
void RawrXD_IDE_ShowAbout(RawrXD_IDE* ide) {
    WCHAR msg[512];
    StringCchPrintfW(msg, 512,
        L"RawrXD IDE v%s\n\n"
        L"Win32 GUI IDE Shell for the RawrXD Project\n\n"
        L"Components:\n"
        L"  \x2022 PE Generator (BareMetal_PE_Writer)\n"
        L"  \x2022 Instruction Encoder\n"
        L"  \x2022 Widget Intelligence (IPC)\n"
        L"  \x2022 Extension System\n\n"
        L"RichEdit: %s\n"
        L"DPI: %u (%.0f%%)\n\n"
        L"ZERO external dependencies.\n"
        L"Built with Win32 API + Common Controls.",
        RAWRXD_IDE_VERSION_STRING,
        ide->richEditDll,
        ide->dpi,
        ide->dpiScale * 100.0f);

    MessageBoxW(ide->hWndMain, msg, L"About RawrXD IDE",
                MB_OK | MB_ICONINFORMATION);
}

/*===========================================================================
 * SHUTDOWN
 *=========================================================================*/
void RawrXD_IDE_Shutdown(RawrXD_IDE* ide) {
    /* Disconnect IPC */
    RawrXD_IDE_IPCDisconnect(ide);

    /* Stop build */
    RawrXD_IDE_StopBuild(ide);

    /* Destroy accelerator table */
    if (ide->hAccelTable) {
        DestroyAcceleratorTable(ide->hAccelTable);
        ide->hAccelTable = NULL;
    }

    /* Destroy fonts */
    if (ide->hFontCode) { DeleteObject(ide->hFontCode); ide->hFontCode = NULL; }
    if (ide->hFontUI)   { DeleteObject(ide->hFontUI);   ide->hFontUI   = NULL; }

    /* Destroy brushes */
    RawrXD_IDE_DestroyThemeBrushes(ide);

    /* Free RichEdit library */
    if (ide->hRichEditLib) {
        FreeLibrary(ide->hRichEditLib);
        ide->hRichEditLib = NULL;
    }

    /* Unregister class */
    UnregisterClassW(RAWRXD_IDE_CLASS, ide->hInstance);
}

/*===========================================================================
 * MESSAGE LOOP
 *=========================================================================*/
int RawrXD_IDE_Run(RawrXD_IDE* ide) {
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        /* Find/Replace dialog messages */
        if (ide->findState.hFindDlg && IsDialogMessageW(ide->findState.hFindDlg, &msg))
            continue;

        /* Accelerators */
        if (ide->hAccelTable &&
            TranslateAcceleratorW(ide->hWndMain, ide->hAccelTable, &msg))
            continue;

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

/*===========================================================================
 * ENTRY POINT — WinMain
 *=========================================================================*/
#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(linker, "/subsystem:windows")
#endif

static int IDE_Main(HINSTANCE hInstance) {
    /* Initialise COM (for potential D2D1 / drag-drop) */
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    if (!RawrXD_IDE_Init(&g_IDE, hInstance)) {
        MessageBoxW(NULL, L"Failed to initialise RawrXD IDE.", L"Fatal Error",
                    MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    /* Initialize Ollama completion engine */
    #include "ide_completion.h"
    IDECompletion::InitializeCompletionEngine("codellama:7b");
    
    /* Check Ollama status and show status bar message */
    if (IDECompletion::IsCompletionEngineReady()) {
        MessageBoxW(NULL, 
            L"✓ Ollama completion engine initialized successfully!\n\n"
            L"Press Ctrl+Space in the editor to get AI code suggestions.\n\n"
            L"Make sure Ollama is running: ollama serve",
            L"RawrXD IDE - Completion Ready", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(NULL, 
            L"⚠ Ollama not detected at localhost:11434\n\n"
            L"To enable AI completions:\n"
            L"1. Install Ollama from https://ollama.ai\n"
            L"2. Run: ollama serve\n"
            L"3. Run: ollama pull codellama:7b\n"
            L"4. Restart RawrXD IDE\n\n"
            L"Editor will work without Ollama, but completions won't be available.",
            L"RawrXD IDE - Ollama Not Available", MB_OK | MB_ICONWARNING);
    }

    int exitCode = RawrXD_IDE_Run(&g_IDE);

    RawrXD_IDE_Shutdown(&g_IDE);
    CoUninitialize();

    return exitCode;
}

/* MSVC uses wWinMain; MinGW uses WinMain.  Provide both. */
#ifdef _MSC_VER
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return IDE_Main(hInstance);
}
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return IDE_Main(hInstance);
}
#endif
