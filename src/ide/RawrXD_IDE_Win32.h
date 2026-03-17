/*=============================================================================
 * RawrXD_IDE_Win32.h
 * Complete Win32 GUI IDE Shell - Header
 *
 * ZERO external dependencies: Win32 API + Common Controls only.
 * Target: Windows 7+ / MSVC cl.exe or MinGW g++
 *
 * (C) RawrXD Project
 *===========================================================================*/
#pragma once

#ifndef RAWRXD_IDE_WIN32_H
#define RAWRXD_IDE_WIN32_H

/* ── Lean Win32 ─────────────────────────────────────────────────────────── */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <richedit.h>
#include <strsafe.h>
#include <stdint.h>

/* ── Version ────────────────────────────────────────────────────────────── */
#define RAWRXD_IDE_VERSION_MAJOR    1
#define RAWRXD_IDE_VERSION_MINOR    0
#define RAWRXD_IDE_VERSION_PATCH    0
#define RAWRXD_IDE_VERSION_STRING   L"1.0.0"

/* ── Window Class ───────────────────────────────────────────────────────── */
#define RAWRXD_IDE_CLASS            L"RawrXD_IDE_Win32_Class"
#define RAWRXD_IDE_TITLE            L"RawrXD IDE"
#define RAWRXD_IDE_DEFAULT_WIDTH    1400
#define RAWRXD_IDE_DEFAULT_HEIGHT   900

/* ── IPC ────────────────────────────────────────────────────────────────── */
#define RAWRXD_PIPE_NAME            L"\\\\.\\pipe\\RawrXD_WidgetIntelligence"
#define RAWRXD_PIPE_BUFFER_SIZE     4096

/* ── Control IDs ────────────────────────────────────────────────────────── */
#define IDC_FILE_TREE               1001
#define IDC_CODE_EDITOR             1002
#define IDC_OUTPUT_PANEL            1003
#define IDC_WIDGET_PANEL            1004
#define IDC_STATUS_BAR              1005
#define IDC_TAB_CONTROL             1006
#define IDC_HSPLITTER               1007
#define IDC_VSPLITTER               1008

/* ── Menu IDs ───────────────────────────────────────────────────────────── */
/* File */
#define IDM_FILE_NEW                2001
#define IDM_FILE_OPEN               2002
#define IDM_FILE_SAVE               2003
#define IDM_FILE_SAVEAS             2004
#define IDM_FILE_CLOSE              2005
#define IDM_FILE_EXIT               2006

/* Edit */
#define IDM_EDIT_UNDO               2101
#define IDM_EDIT_REDO               2102
#define IDM_EDIT_CUT                2103
#define IDM_EDIT_COPY               2104
#define IDM_EDIT_PASTE              2105
#define IDM_EDIT_SELECTALL          2106
#define IDM_EDIT_FIND               2107
#define IDM_EDIT_REPLACE            2108
#define IDM_EDIT_GOTO               2109

/* Build */
#define IDM_BUILD_BUILD             2201
#define IDM_BUILD_REBUILD           2202
#define IDM_BUILD_RUN               2203
#define IDM_BUILD_CLEAN             2204
#define IDM_BUILD_STOP              2205

/* Tools */
#define IDM_TOOLS_PE_INSPECTOR      2301
#define IDM_TOOLS_INSTR_ENCODER     2302
#define IDM_TOOLS_EXT_MANAGER       2303
#define IDM_TOOLS_OPTIONS           2304

/* View */
#define IDM_VIEW_FILEBROWSER        2401
#define IDM_VIEW_OUTPUT             2402
#define IDM_VIEW_WIDGET             2403
#define IDM_VIEW_FULLSCREEN         2404
#define IDM_VIEW_DARK_THEME         2405
#define IDM_VIEW_LIGHT_THEME        2406

/* Help */
#define IDM_HELP_ABOUT              2501
#define IDM_HELP_DOCS               2502

/* ── Accelerator IDs ────────────────────────────────────────────────────── */
#define IDA_MAIN_ACCEL              3001

/* ── Timer IDs ──────────────────────────────────────────────────────────── */
#define IDT_STATUS_UPDATE           4001
#define IDT_IPC_POLL                4002
#define IDT_AUTOSAVE                4003

/* ── Status bar parts ───────────────────────────────────────────────────── */
#define SB_PART_FILE                0
#define SB_PART_LINECOL             1
#define SB_PART_ENCODING            2
#define SB_PART_BUILD               3
#define SB_PART_IPC                 4
#define SB_NUM_PARTS                5

/* ── Layout constants ───────────────────────────────────────────────────── */
#define FILETREE_MIN_WIDTH          180
#define FILETREE_DEFAULT_WIDTH      250
#define OUTPUT_MIN_HEIGHT           80
#define OUTPUT_DEFAULT_HEIGHT       200
#define WIDGET_MIN_WIDTH            200
#define WIDGET_DEFAULT_WIDTH        280
#define SPLITTER_WIDTH              4
#define STATUS_HEIGHT               24

/* ── Color theme ────────────────────────────────────────────────────────── */
typedef struct RawrXD_Theme {
    COLORREF bgWindow;
    COLORREF bgEditor;
    COLORREF bgOutput;
    COLORREF bgTree;
    COLORREF bgWidget;
    COLORREF bgStatus;
    COLORREF fgText;
    COLORREF fgComment;
    COLORREF fgKeyword;
    COLORREF fgString;
    COLORREF fgNumber;
    COLORREF fgOperator;
    COLORREF fgPreprocessor;
    COLORREF fgLineNumber;
    COLORREF bgSelection;
    COLORREF fgSelection;
    COLORREF borderColor;
    COLORREF splitterColor;
    COLORREF menuBg;
    COLORREF menuFg;
} RawrXD_Theme;

/* ── Build state ────────────────────────────────────────────────────────── */
typedef enum RawrXD_BuildState {
    BUILD_IDLE      = 0,
    BUILD_RUNNING   = 1,
    BUILD_SUCCESS   = 2,
    BUILD_FAILED    = 3
} RawrXD_BuildState;

/* ── IPC connection state ───────────────────────────────────────────────── */
typedef enum RawrXD_IPCState {
    IPC_DISCONNECTED = 0,
    IPC_CONNECTING   = 1,
    IPC_CONNECTED    = 2,
    IPC_ERROR        = 3
} RawrXD_IPCState;

/* ── Find/Replace dialog state ──────────────────────────────────────────── */
typedef struct RawrXD_FindState {
    WCHAR searchText[512];
    WCHAR replaceText[512];
    BOOL  matchCase;
    BOOL  wholeWord;
    BOOL  searchUp;
    BOOL  useRegex;
    HWND  hFindDlg;
} RawrXD_FindState;

/* ── Main IDE state ─────────────────────────────────────────────────────── */
typedef struct RawrXD_IDE {
    /* Win32 handles */
    HINSTANCE       hInstance;
    HWND            hWndMain;
    HWND            hWndFileTree;
    HWND            hWndEditor;
    HWND            hWndOutput;
    HWND            hWndWidget;
    HWND            hWndStatusBar;
    HWND            hWndTabCtrl;
    HMENU           hMenuBar;
    HACCEL          hAccelTable;

    /* Fonts */
    HFONT           hFontCode;
    HFONT           hFontUI;

    /* Brushes */
    HBRUSH          hBrushBg;
    HBRUSH          hBrushEditor;
    HBRUSH          hBrushOutput;
    HBRUSH          hBrushTree;
    HBRUSH          hBrushWidget;

    /* Rich edit module */
    HMODULE         hRichEditLib;

    /* Layout */
    int             fileTreeWidth;
    int             outputHeight;
    int             widgetWidth;
    BOOL            showFileTree;
    BOOL            showOutput;
    BOOL            showWidget;
    BOOL            isFullscreen;
    RECT            restoreRect;

    /* File state */
    WCHAR           currentFilePath[MAX_PATH];
    BOOL            isModified;
    BOOL            isUntitled;
    DWORD           fileEncoding; /* 0=ANSI, 1=UTF8, 2=UTF16LE */

    /* Theme */
    RawrXD_Theme    theme;
    BOOL            isDarkTheme;

    /* Build */
    RawrXD_BuildState buildState;
    HANDLE          hBuildProcess;
    HANDLE          hBuildThread;

    /* IPC */
    RawrXD_IPCState ipcState;
    HANDLE          hPipe;
    HANDLE          hIPCThread;
    volatile BOOL   ipcRunning;

    /* Find / Replace */
    RawrXD_FindState findState;

    /* DPI */
    UINT            dpi;
    float           dpiScale;

    /* RichEdit DLL path (for logging) */
    WCHAR           richEditDll[MAX_PATH];

} RawrXD_IDE;

/* ── Function prototypes ────────────────────────────────────────────────── */

#ifdef __cplusplus
extern "C" {
#endif

/* Initialization / shutdown */
BOOL    RawrXD_IDE_Init(RawrXD_IDE* ide, HINSTANCE hInst);
int     RawrXD_IDE_Run(RawrXD_IDE* ide);
void    RawrXD_IDE_Shutdown(RawrXD_IDE* ide);

/* Window creation */
BOOL    RawrXD_IDE_RegisterClass(RawrXD_IDE* ide);
BOOL    RawrXD_IDE_CreateMainWindow(RawrXD_IDE* ide);
void    RawrXD_IDE_CreateControls(RawrXD_IDE* ide);
HMENU   RawrXD_IDE_CreateMenuBar(RawrXD_IDE* ide);
HACCEL  RawrXD_IDE_CreateAccelerators(RawrXD_IDE* ide);
void    RawrXD_IDE_CreateStatusBar(RawrXD_IDE* ide);

/* Window procedure */
LRESULT CALLBACK RawrXD_IDE_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* Message handlers */
LRESULT RawrXD_IDE_OnCreate(RawrXD_IDE* ide, HWND hWnd, LPCREATESTRUCT lpcs);
void    RawrXD_IDE_OnSize(RawrXD_IDE* ide, int cx, int cy);
void    RawrXD_IDE_OnPaint(RawrXD_IDE* ide, HWND hWnd);
void    RawrXD_IDE_OnCommand(RawrXD_IDE* ide, WORD cmdId, WORD notifyCode, HWND hCtrl);
LRESULT RawrXD_IDE_OnNotify(RawrXD_IDE* ide, NMHDR* pnmh);
void    RawrXD_IDE_OnClose(RawrXD_IDE* ide);
void    RawrXD_IDE_OnDestroy(RawrXD_IDE* ide);
void    RawrXD_IDE_OnTimer(RawrXD_IDE* ide, UINT_PTR timerId);
LRESULT RawrXD_IDE_OnCtlColorEdit(RawrXD_IDE* ide, HDC hdc, HWND hCtrl);
LRESULT RawrXD_IDE_OnCtlColorStatic(RawrXD_IDE* ide, HDC hdc, HWND hCtrl);

/* Layout */
void    RawrXD_IDE_LayoutPanes(RawrXD_IDE* ide);
int     RawrXD_IDE_DPIScale(RawrXD_IDE* ide, int value);

/* File operations */
BOOL    RawrXD_IDE_FileNew(RawrXD_IDE* ide);
BOOL    RawrXD_IDE_FileOpen(RawrXD_IDE* ide);
BOOL    RawrXD_IDE_FileSave(RawrXD_IDE* ide);
BOOL    RawrXD_IDE_FileSaveAs(RawrXD_IDE* ide);
BOOL    RawrXD_IDE_FileClose(RawrXD_IDE* ide);
BOOL    RawrXD_IDE_LoadFile(RawrXD_IDE* ide, const WCHAR* path);
BOOL    RawrXD_IDE_SaveFile(RawrXD_IDE* ide, const WCHAR* path);
BOOL    RawrXD_IDE_PromptSaveChanges(RawrXD_IDE* ide);

/* Edit operations */
void    RawrXD_IDE_EditUndo(RawrXD_IDE* ide);
void    RawrXD_IDE_EditRedo(RawrXD_IDE* ide);
void    RawrXD_IDE_EditCut(RawrXD_IDE* ide);
void    RawrXD_IDE_EditCopy(RawrXD_IDE* ide);
void    RawrXD_IDE_EditPaste(RawrXD_IDE* ide);
void    RawrXD_IDE_EditSelectAll(RawrXD_IDE* ide);
void    RawrXD_IDE_EditFind(RawrXD_IDE* ide);
void    RawrXD_IDE_EditReplace(RawrXD_IDE* ide);
void    RawrXD_IDE_EditGotoLine(RawrXD_IDE* ide);

/* Build operations */
void    RawrXD_IDE_BuildProject(RawrXD_IDE* ide);
void    RawrXD_IDE_RebuildProject(RawrXD_IDE* ide);
void    RawrXD_IDE_RunProject(RawrXD_IDE* ide);
void    RawrXD_IDE_CleanProject(RawrXD_IDE* ide);
void    RawrXD_IDE_StopBuild(RawrXD_IDE* ide);
DWORD WINAPI RawrXD_IDE_BuildThread(LPVOID param);

/* Tools */
void    RawrXD_IDE_LaunchPEInspector(RawrXD_IDE* ide);
void    RawrXD_IDE_LaunchInstrEncoder(RawrXD_IDE* ide);
void    RawrXD_IDE_LaunchExtManager(RawrXD_IDE* ide);

/* IPC */
BOOL    RawrXD_IDE_IPCConnect(RawrXD_IDE* ide);
void    RawrXD_IDE_IPCDisconnect(RawrXD_IDE* ide);
BOOL    RawrXD_IDE_IPCSend(RawrXD_IDE* ide, const WCHAR* message);
DWORD WINAPI RawrXD_IDE_IPCThread(LPVOID param);

/* Theme */
void    RawrXD_IDE_SetDarkTheme(RawrXD_IDE* ide);
void    RawrXD_IDE_SetLightTheme(RawrXD_IDE* ide);
void    RawrXD_IDE_ApplyTheme(RawrXD_IDE* ide);
void    RawrXD_IDE_CreateThemeBrushes(RawrXD_IDE* ide);
void    RawrXD_IDE_DestroyThemeBrushes(RawrXD_IDE* ide);

/* Status bar */
void    RawrXD_IDE_UpdateStatusBar(RawrXD_IDE* ide);
void    RawrXD_IDE_UpdateLineCol(RawrXD_IDE* ide);
void    RawrXD_IDE_SetBuildStatus(RawrXD_IDE* ide, const WCHAR* text);

/* File browser tree */
void    RawrXD_IDE_PopulateTree(RawrXD_IDE* ide, const WCHAR* rootPath);
void    RawrXD_IDE_PopulateTreeItem(RawrXD_IDE* ide, HTREEITEM hParent, const WCHAR* path);
void    RawrXD_IDE_OnTreeSelChanged(RawrXD_IDE* ide, NMTREEVIEWW* pnmtv);
void    RawrXD_IDE_OnTreeDblClick(RawrXD_IDE* ide);

/* Output panel */
void    RawrXD_IDE_OutputAppend(RawrXD_IDE* ide, const WCHAR* text);
void    RawrXD_IDE_OutputClear(RawrXD_IDE* ide);

/* Widget panel */
void    RawrXD_IDE_WidgetAppend(RawrXD_IDE* ide, const WCHAR* text);
void    RawrXD_IDE_WidgetClear(RawrXD_IDE* ide);

/* DPI */
void    RawrXD_IDE_InitDPI(RawrXD_IDE* ide);

/* Utility */
void    RawrXD_IDE_UpdateTitle(RawrXD_IDE* ide);
void    RawrXD_IDE_ShowAbout(RawrXD_IDE* ide);
BOOL    RawrXD_IDE_IsSourceFile(const WCHAR* path);

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_IDE_WIN32_H */
