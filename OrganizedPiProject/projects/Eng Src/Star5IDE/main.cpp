#include <windows.h>
#include <commctrl.h>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include "Resource.h"
#include "LayoutManager.h"
#include "ProjectSettings.h"
#include "SolutionSettings.h"
#include "ReportsViewer.h"
#include "Config.h"
#include "Agent.h"

#pragma comment(lib,"comctl32.lib")

static std::wstring gOutputDir = L".";

#ifndef SCI_SETLEXER
#define SCI_SETLEXER 4001
#endif
#ifndef SCLEX_CPP
#define SCLEX_CPP 3
#endif
#ifndef SCI_SETKEYWORDS
#define SCI_SETKEYWORDS 4005
#endif
#ifndef SCI_SETPROPERTY
#define SCI_SETPROPERTY 4008
#endif
#ifndef SCI_STYLESETFORE
#define SCI_STYLESETFORE 2051
#endif
#ifndef SCE_C_COMMENT
#define SCE_C_COMMENT 1
#endif
#ifndef SCE_C_NUMBER
#define SCE_C_NUMBER 2
#endif
#ifndef SCE_C_STRING
#define SCE_C_STRING 6
#endif
#ifndef SCE_C_WORD
#define SCE_C_WORD 5
#endif
#ifndef SCI_STYLESETBOLD
#define SCI_STYLESETBOLD 2053
#endif
#ifndef SCI_AUTOCSHOW
#define SCI_AUTOCSHOW 2100
#endif
#ifndef SCI_SETTEXT
#define SCI_SETTEXT 2181
#endif
#ifndef SCI_GETTEXTLength
#define SCI_GETTEXTLength 2182
#endif
#ifndef SCI_GETTEXT
#define SCI_GETTEXT 2182
#endif

void RunCopilotAI(HWND) {}
void DebugProject(HWND) {}
void LoadPlugin(HWND) {}
void RunScript(HWND) {}
void ToggleSyntaxHighlighting(HWND) {}
void ToggleCodeFolding(HWND) {}
void ShowAutoComplete(HWND) {}
void ChangeTheme(HWND) {}
void ToggleDocking(HWND) {}
void ShowAdvancedUI(HWND) {}
void ToggleMultiCursor(HWND) {}
void ToggleBlockCursor(HWND) {}
void ToggleBlinkCursor(HWND) {}

void ShowSaveAsDialog(HWND hwnd) {
    MessageBoxW(hwnd, L"Save As dialog not yet implemented.", L"Save As", MB_OK);
}
void GoToLastLineDialog(HWND hwnd) {
    MessageBoxW(hwnd, L"Go To Last Line dialog not yet implemented.", L"Go To Last Line", MB_OK);
}
void FindNext(HWND hwnd) {
    MessageBoxW(hwnd, L"Find Next not yet implemented.", L"Find Next", MB_OK);
}
void FindPrev(HWND hwnd) {
    MessageBoxW(hwnd, L"Find Previous not yet implemented.", L"Find Previous", MB_OK);
}

static HWND gTree, gTab, gStatusBar;
struct EditorPane {
    HWND hwndEdit = nullptr;
    std::wstring filePath;
    bool dirty = false;
};
static std::vector<EditorPane> gEditors;
static int gCurrentTab = -1;
static HTREEITEM hRoot;

struct ProjectNode {
    std::wstring name;
    std::vector<std::wstring> files;
};
static std::vector<ProjectNode> gProjects = {
    {L"CoreLib",{L"core.cpp",L"core.h"}},
    {L"App",{L"main.cpp",L"app.h",L"Resources.rc"}}
};

enum NodeType { NODE_SOLUTION, NODE_PROJECT, NODE_FILE };
struct NodeData {
    NodeType type = NODE_FILE;
    std::wstring name;
    std::wstring project;
};
static std::map<HTREEITEM,NodeData> gNodeMap;

static void SetStatusLeft(const std::wstring& msg) {
    SendMessage(gStatusBar, SB_SETTEXT, 0, (LPARAM)msg.c_str());
}
static void SetStatusRight(const std::wstring& msg) {
    SendMessage(gStatusBar, SB_SETTEXT, 1, (LPARAM)msg.c_str());
}
static void SetStatusAnnotation(const std::wstring& why, const std::wstring& what, const std::wstring& outcome) {
    std::wstring msg = L"Why: " + why + L" | What: " + what + L" | Outcome: " + outcome;
    SetStatusLeft(msg);
}

static void UpdateTabText(int idx) {
    if(idx<0 || idx>=(int)gEditors.size()) return;
    std::wstring name=gEditors[idx].filePath.substr(gEditors[idx].filePath.find_last_of(L"\\/")+1);
    if(gEditors[idx].dirty) name=L"*"+name;
    TCITEM tie={0}; tie.mask=TCIF_TEXT; tie.pszText=(LPWSTR)name.c_str();
    TabCtrl_SetItem(gTab,idx,&tie);
}

static void LoadFileToEditor(EditorPane& ep,int idx) {
    std::wifstream in(ep.filePath);
    if(!in.is_open()) {
        SetWindowTextW(ep.hwndEdit,(L"Could not open file: "+ep.filePath).c_str());
        return;
    }
    std::wstring content,line;
    while(std::getline(in,line)) content+=line+L"\r\n";
    SetWindowTextW(ep.hwndEdit,content.c_str());
    ep.dirty=false;
    UpdateTabText(idx);
    SetStatusLeft(L"Opened file: " + ep.filePath);
}

static void SaveEditorToFile(EditorPane& ep,int idx) {
    int len=GetWindowTextLengthW(ep.hwndEdit);
    std::wstring buf(len+1,L'\0');
    GetWindowTextW(ep.hwndEdit,&buf[0],len+1);
    std::wofstream out(ep.filePath);
    out<<buf;
    ep.dirty=false;
    UpdateTabText(idx);
    SetStatusLeft(L"Saved file: " + ep.filePath);
    SetStatusAnnotation(L"Persist changes to disk", L"Save file operation", L"File saved and marked clean");
}

static void MarkDirty(int idx) {
    if(idx<0 || idx>=(int)gEditors.size()) return;
    if(!gEditors[idx].dirty) {
        gEditors[idx].dirty=true;
        UpdateTabText(idx);
        SetStatusLeft(L"Unsaved changes: " + gEditors[idx].filePath);
    }
}

static HTREEITEM AddNode(HTREEITEM parent,const std::wstring& text,NodeType type,
                         const std::wstring& name,const std::wstring& proj=L"") {
    TVINSERTSTRUCT tvi={0};
    tvi.hParent=parent; tvi.hInsertAfter=TVI_LAST;
    tvi.item.mask=TVIF_TEXT;
    tvi.item.pszText=(LPWSTR)text.c_str();
    HTREEITEM hItem=TreeView_InsertItem(gTree,&tvi);
    gNodeMap[hItem]={type,name,proj};
    return hItem;
}

static void BuildTree() {
    TreeView_DeleteAllItems(gTree); gNodeMap.clear();
    hRoot=AddNode(NULL,L"MySolution.sln",NODE_SOLUTION,L"MySolution.sln");
    for(auto& proj:gProjects) {
        HTREEITEM hProj=AddNode(hRoot,proj.name,NODE_PROJECT,proj.name);
        for(auto& f:proj.files) AddNode(hProj,f,NODE_FILE,f,proj.name);
    }
    TreeView_Expand(gTree,hRoot,TVE_EXPAND);
}

static void SwitchTab(int idx) {
    if(idx<0 || idx>= (int)gEditors.size()) return;
    for(int i=0;i<(int)gEditors.size();i++) {
        ShowWindow(gEditors[i].hwndEdit, (i==idx)?SW_SHOW:SW_HIDE);
    }
    gCurrentTab=idx;
    if(idx>=0) SetStatusLeft(L"Active file: " + gEditors[idx].filePath);
    else SetStatusLeft(L"Ready");
    SetStatusAnnotation(L"Change active editor", L"Switch tab", L"Tab switched and editor shown");
}

static int FindTabByPath(const std::wstring& path) {
    for(int i=0;i<(int)gEditors.size();i++) {
        if(gEditors[i].filePath==path) return i;
    }
    return -1;
}

static void SetupScintilla(HWND hSci) {
    SendMessage(hSci, SCI_SETLEXER, SCLEX_CPP, 0);
    SendMessage(hSci, SCI_SETKEYWORDS, 0, (LPARAM)"int float double char void if else for while return class struct public private protected static const");
    SendMessage(hSci, SCI_SETPROPERTY, (WPARAM)"fold", (LPARAM)"1");
    SendMessage(hSci, SCI_SETPROPERTY, (WPARAM)"fold.compact", (LPARAM)"1");
    SendMessage(hSci, SCI_STYLESETFORE, SCE_C_COMMENT, RGB(0,128,0));
    SendMessage(hSci, SCI_STYLESETFORE, SCE_C_NUMBER, RGB(0,0,255));
    SendMessage(hSci, SCI_STYLESETFORE, SCE_C_STRING, RGB(128,0,128));
    SendMessage(hSci, SCI_STYLESETFORE, SCE_C_WORD, RGB(0,0,128));
    SendMessage(hSci, SCI_STYLESETBOLD, SCE_C_WORD, 1);
    SendMessage(hSci, SCI_AUTOCSHOW, 3, (LPARAM)"int float double char void if else for while return class struct");
}

static void LoadFileToScintilla(HWND hSci, const std::wstring& filePath) {
    std::wifstream in(filePath);
    if(!in.is_open()) {
        SendMessage(hSci, SCI_SETTEXT, 0, (LPARAM)L"Could not open file.");
        return;
    }
    std::wstring content, line;
    while(std::getline(in, line)) content += line + L"\r\n";
    SendMessage(hSci, SCI_SETTEXT, 0, (LPARAM)content.c_str());
}

static void SaveScintillaToFile(HWND hSci, const std::wstring& filePath) {
    int len = SendMessage(hSci, SCI_GETTEXTLength, 0, 0);
    std::wstring buf(len+1, L'\0');
    SendMessage(hSci, SCI_GETTEXT, len+1, (LPARAM)&buf[0]);
    std::wofstream out(filePath);
    out << buf;
}

static void OpenFileInTab(HWND hwndMain, const std::wstring& path) {
    int existing = FindTabByPath(path);
    if(existing >= 0) {
        TabCtrl_SetCurSel(gTab, existing);
        SwitchTab(existing);
        return;
    }
    TCITEM tie = {0}; tie.mask = TCIF_TEXT;
    std::wstring name = path.substr(path.find_last_of(L"\\/")+1);
    tie.pszText = (LPWSTR)name.c_str();
    int idx = TabCtrl_InsertItem(gTab, (int)gEditors.size(), &tie);
    HWND hSci = CreateWindowEx(0, L"Scintilla", NULL,
        WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_HSCROLL,
        270,40,700,570, hwndMain, NULL, GetModuleHandle(NULL), NULL);
    SetupScintilla(hSci);
    EditorPane ep; ep.hwndEdit = hSci; ep.filePath = path;
    gEditors.push_back(ep);
    LoadFileToScintilla(hSci, path);
    TabCtrl_SetCurSel(gTab, idx);
    SwitchTab(idx);
}

static bool PromptSaveIfDirty(int idx) {
    if(idx<0 || idx>=(int)gEditors.size()) return true;
    if(!gEditors[idx].dirty) return true;
    int res=MessageBoxW(NULL,(L"Save changes to "+gEditors[idx].filePath+L"?").c_str(),
                        L"Unsaved Changes",MB_YESNOCANCEL|MB_ICONQUESTION);
    if(res==IDCANCEL) return false;
    if(res==IDYES) SaveEditorToFile(gEditors[idx],idx);
    return true;
}

static void ShowCopilotOutput(HWND parent, const std::wstring& title, const std::wstring& output) {
    MessageBoxW(parent, output.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
}

static void ShowCompilerSupport(HWND hwnd) {
    std::wstring output =
        L"Supported Compilers:\n\n"
        L"- Visual Studio 2022 (MSVC)\n"
        L"- G++ (MinGW, GCC)\n\n"
        L"You can select your preferred compiler in the settings or project configuration.\n"
        L"MSVC: Full C++14/17/20 support, Windows SDK integration.\n"
        L"G++: Cross-platform, C++14/17/20 support, MinGW/GCC toolchain.\n";
    ShowCopilotOutput(hwnd, L"Compiler Support", output);
}

static void ShowBuildLogSummary(HWND hwnd) {
    std::wstring logFile = gOutputDir + L"\\build.log";
    std::wifstream in(logFile);
    if (!in.is_open()) {
        MessageBoxW(hwnd, L"Could not open build.log.", L"Build Log", MB_OK | MB_ICONWARNING);
        return;
    }
    std::wstring line, errors, warnings;
    int errorCount = 0, warningCount = 0;
    while (std::getline(in, line)) {
        if (line.find(L"error") != std::wstring::npos) { errors += line + L"\n"; errorCount++; }
        if (line.find(L"warning") != std::wstring::npos) { warnings += line + L"\n"; warningCount++; }
    }
    std::wstring summary = L"Build Log Summary:\nErrors: " + std::to_wstring(errorCount) + L"\nWarnings: " + std::to_wstring(warningCount);
    if (errorCount > 0) summary += L"\n\nErrors:\n" + errors;
    if (warningCount > 0) summary += L"\n\nWarnings:\n" + warnings;
    ShowCopilotOutput(hwnd, L"Build Log Summary", summary);
}

enum BuildSystem { BUILD_MSVC, BUILD_MSYS2 };
static BuildSystem gBuildSystem = BUILD_MSVC;

static void ShowBuildSystemDialog(HWND hwnd) {
    int res = MessageBoxW(hwnd,
        L"Select build system:\nYes = Visual Studio (MSBuild)\nNo = MSYS2 (GCC)",
        L"Build System Selection", MB_YESNO | MB_ICONQUESTION);
    gBuildSystem = (res == IDYES) ? BUILD_MSVC : BUILD_MSYS2;
    SetStatusLeft((gBuildSystem == BUILD_MSVC) ? L"Build System: Visual Studio (MSBuild)" : L"Build System: MSYS2 (GCC)");
}

static void BuildSolution(HWND hwnd) {
    std::wstring cmd;
    if (gBuildSystem == BUILD_MSVC) {
        // Try to build with MSBuild (assumes .sln in current dir)
        cmd = L"msbuild MySolution.sln /p:Configuration=Debug > build.log 2>&1";
    } else {
        // Try to build with MSYS2 GCC (assumes main.cpp in current dir)
        cmd = L"C:\\msys64\\usr\\bin\\g++.exe main.cpp -o main.exe > build.log 2>&1";
    }
    _wsystem(cmd.c_str());
    ShowBuildLogSummary(hwnd);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            gTree = CreateWindowEx(0, WC_TREEVIEW, L"Solution Explorer",
                WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
                0, 0, 250, 600, hwnd, (HMENU)1001, GetModuleHandle(NULL), NULL);
            gTab = CreateWindowEx(0, WC_TABCONTROL, NULL,
                WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
                250, 0, 774, 40, hwnd, (HMENU)1002, GetModuleHandle(NULL), NULL);
            gStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL,
                WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                0, 600, 1024, 30, hwnd, (HMENU)1003, GetModuleHandle(NULL), NULL);
            BuildTree();
            SetStatusLeft(L"Ready");
            break;
        }
        case WM_SIZE: {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            MoveWindow(gTree, 0, 0, 250, h - 30, TRUE);
            MoveWindow(gTab, 250, 0, w - 250, 40, TRUE);
            MoveWindow(gStatusBar, 0, h - 30, w, 30, TRUE);
            for (auto& ep : gEditors) {
                MoveWindow(ep.hwndEdit, 250, 40, w - 250, h - 70, TRUE);
            }
            break;
        }
        case WM_NOTIFY: {
            NMHDR* nmhdr = (NMHDR*)lParam;
            if (nmhdr->hwndFrom == gTree && nmhdr->code == NM_DBLCLK) {
                TVITEM tvi = {0};
                tvi.mask = TVIF_HANDLE;
                tvi.hItem = TreeView_GetSelection(gTree);
                if (tvi.hItem && gNodeMap.count(tvi.hItem)) {
                    NodeData nd = gNodeMap[tvi.hItem];
                    if (nd.type == NODE_FILE) {
                        OpenFileInTab(hwnd, nd.name);
                    }
                }
            }
            break;
        }
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_FILE_EXIT:
                    PostQuitMessage(0);
                    break;
                case ID_VIEW_LAYOUTMGR:
                    MessageBoxW(hwnd, L"Layout Manager dialog not yet implemented.", L"Layout Manager", MB_OK);
                    break;
                case 60003:
                    ShowCompilerSupport(hwnd);
                    break;
                case 50020:
                    ShowBuildSystemDialog(hwnd);
                    break;
                case 50021:
                    BuildSolution(hwnd);
                    break;
                default:
                    break;
            }
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"IDEWndClass";
    RegisterClass(&wc);

    HMENU hMenu = CreateMenu();
    HMENU hFileMenu = CreateMenu();
    HMENU hEditMenu = CreateMenu();
    HMENU hViewMenu = CreateMenu();
    HMENU hHelpMenu = CreateMenu();

    AppendMenu(hFileMenu, MF_STRING, ID_FILE_EXIT, L"E&xit");
    AppendMenu(hEditMenu, MF_STRING, 50005, L"Undo");
    AppendMenu(hEditMenu, MF_STRING, 50006, L"Redo");
    AppendMenu(hEditMenu, MF_STRING, 50007, L"Cut");
    AppendMenu(hEditMenu, MF_STRING, 50008, L"Copy");
    AppendMenu(hEditMenu, MF_STRING, 50009, L"Paste");
    AppendMenu(hEditMenu, MF_STRING, 50010, L"Select All");
    AppendMenu(hViewMenu, MF_STRING, ID_VIEW_LAYOUTMGR, L"Layout Manager...");
    AppendMenu(hHelpMenu, MF_STRING, 60003, L"Compiler Support...");
    AppendMenu(hFileMenu, MF_STRING, 50020, L"Select Build System...");
    AppendMenu(hFileMenu, MF_STRING, 50021, L"Build Solution");

    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hEditMenu, L"&Edit");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hViewMenu, L"&View");
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, L"&Help");

    HWND hwnd = CreateWindow(L"IDEWndClass", L"CoPilot IDE", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 700, NULL, hMenu, hInstance, NULL);
    SetMenu(hwnd, hMenu);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
