; ============================================================================
; RawrXD Win32 API Minimal Include (SDK-Compatible)
; Replacement for MASM32 includes to work with Visual Studio MASM
; ============================================================================

; ============================================================================
; BASIC TYPES / HANDLE TYPEDEFS
; ============================================================================
HANDLE      TYPEDEF DWORD
HINSTANCE   TYPEDEF DWORD
HMODULE     TYPEDEF DWORD
HWND        TYPEDEF DWORD
HICON       TYPEDEF DWORD
HCURSOR     TYPEDEF DWORD
HBRUSH      TYPEDEF DWORD
HFONT       TYPEDEF DWORD
HMENU       TYPEDEF DWORD
HDC         TYPEDEF DWORD
HGDIOBJ     TYPEDEF DWORD

; ============================================================================
; CONSTANTS
; ============================================================================

; Boolean
FALSE   EQU 0
TRUE    EQU 1
NULL    EQU 0

; Standard handles
STD_INPUT_HANDLE    EQU -10
STD_OUTPUT_HANDLE   EQU -11
STD_ERROR_HANDLE    EQU -12

; Invalid values
INVALID_HANDLE_VALUE EQU -1

; Window Messages
WM_NULL             EQU 0000h
WM_CREATE           EQU 0001h
WM_DESTROY          EQU 0002h
WM_MOVE             EQU 0003h
WM_SIZE             EQU 0005h
WM_ACTIVATE         EQU 0006h
WM_SETFOCUS         EQU 0007h
WM_KILLFOCUS        EQU 0008h
WM_ENABLE           EQU 000Ah
WM_SETTEXT          EQU 000Ch
WM_GETTEXT          EQU 000Dh
WM_GETTEXTLENGTH    EQU 000Eh
WM_PAINT            EQU 000Fh
WM_CLOSE            EQU 0010h
WM_QUIT             EQU 0012h
WM_ERASEBKGND       EQU 0014h
WM_SHOWWINDOW       EQU 0018h
WM_ACTIVATEAPP      EQU 001Ch
WM_SETFONT          EQU 0030h
WM_GETFONT          EQU 0031h
WM_NOTIFY           EQU 004Eh
WM_CONTEXTMENU      EQU 007Bh
WM_STYLECHANGED     EQU 007Dh
WM_NCCREATE         EQU 0081h
WM_NCDESTROY        EQU 0082h
WM_NCCALCSIZE       EQU 0083h
WM_NCHITTEST        EQU 0084h
WM_NCPAINT          EQU 0085h
WM_NCACTIVATE       EQU 0086h
WM_GETDLGCODE       EQU 0087h
WM_SYNCPAINT        EQU 0088h
WM_NCMOUSEMOVE      EQU 00A0h
WM_NCLBUTTONDOWN    EQU 00A1h
WM_NCLBUTTONUP      EQU 00A2h
WM_KEYDOWN          EQU 0100h
WM_KEYUP            EQU 0101h
WM_CHAR             EQU 0102h
WM_COMMAND          EQU 0111h
WM_SYSCOMMAND       EQU 0112h
WM_TIMER            EQU 0113h
WM_HSCROLL          EQU 0114h
WM_VSCROLL          EQU 0115h
WM_INITMENU         EQU 0116h
WM_INITMENUPOPUP    EQU 0117h
WM_MENUSELECT       EQU 011Fh
WM_MENUCHAR         EQU 0120h
WM_ENTERIDLE        EQU 0121h
WM_CTLCOLORMSGBOX   EQU 0132h
WM_CTLCOLOREDIT     EQU 0133h
WM_CTLCOLORLISTBOX  EQU 0134h
WM_CTLCOLORBTN      EQU 0135h
WM_CTLCOLORDLG      EQU 0136h
WM_CTLCOLORSCROLLBAR EQU 0137h
WM_CTLCOLORSTATIC   EQU 0138h
WM_MOUSEMOVE        EQU 0200h
WM_LBUTTONDOWN      EQU 0201h
WM_LBUTTONUP        EQU 0202h
WM_LBUTTONDBLCLK    EQU 0203h
WM_RBUTTONDOWN      EQU 0204h
WM_RBUTTONUP        EQU 0205h
WM_RBUTTONDBLCLK    EQU 0206h
WM_MBUTTONDOWN      EQU 0207h
WM_MBUTTONUP        EQU 0208h
WM_MBUTTONDBLCLK    EQU 0209h
WM_MOUSEWHEEL       EQU 020Ah
WM_USER             EQU 0400h

; Window Styles
WS_OVERLAPPED       EQU 00000000h
WS_POPUP            EQU 80000000h
WS_CHILD            EQU 40000000h
WS_MINIMIZE         EQU 20000000h
WS_VISIBLE          EQU 10000000h
WS_DISABLED         EQU 08000000h
WS_CLIPSIBLINGS     EQU 04000000h
WS_CLIPCHILDREN     EQU 02000000h
WS_MAXIMIZE         EQU 01000000h
WS_CAPTION          EQU 00C00000h
WS_BORDER           EQU 00800000h
WS_DLGFRAME         EQU 00400000h
WS_VSCROLL          EQU 00200000h
WS_HSCROLL          EQU 00100000h
WS_SYSMENU          EQU 00080000h
WS_THICKFRAME       EQU 00040000h
WS_GROUP            EQU 00020000h
WS_TABSTOP          EQU 00010000h
WS_MINIMIZEBOX      EQU 00020000h
WS_MAXIMIZEBOX      EQU 00010000h
WS_OVERLAPPEDWINDOW EQU (WS_OVERLAPPED OR WS_CAPTION OR WS_SYSMENU OR WS_THICKFRAME OR WS_MINIMIZEBOX OR WS_MAXIMIZEBOX)

; Extended Window Styles
WS_EX_DLGMODALFRAME     EQU 00000001h
WS_EX_NOPARENTNOTIFY    EQU 00000004h
WS_EX_TOPMOST           EQU 00000008h
WS_EX_ACCEPTFILES       EQU 00000010h
WS_EX_TRANSPARENT       EQU 00000020h
WS_EX_MDICHILD          EQU 00000040h
WS_EX_TOOLWINDOW        EQU 00000080h
WS_EX_WINDOWEDGE        EQU 00000100h
WS_EX_CLIENTEDGE        EQU 00000200h
WS_EX_CONTEXTHELP       EQU 00000400h
WS_EX_RIGHT             EQU 00001000h
WS_EX_LEFT              EQU 00000000h
WS_EX_RTLREADING        EQU 00002000h
WS_EX_LTRREADING        EQU 00000000h
WS_EX_LEFTSCROLLBAR     EQU 00004000h
WS_EX_RIGHTSCROLLBAR    EQU 00000000h
WS_EX_CONTROLPARENT     EQU 00010000h
WS_EX_STATICEDGE        EQU 00020000h
WS_EX_APPWINDOW         EQU 00040000h
WS_EX_OVERLAPPEDWINDOW  EQU (WS_EX_WINDOWEDGE OR WS_EX_CLIENTEDGE)
WS_EX_PALETTEWINDOW     EQU (WS_EX_WINDOWEDGE OR WS_EX_TOOLWINDOW OR WS_EX_TOPMOST)

; Class Styles
CS_VREDRAW          EQU 0001h
CS_HREDRAW          EQU 0002h
CS_DBLCLKS          EQU 0008h
CS_OWNDC            EQU 0020h
CS_CLASSDC          EQU 0040h
CS_PARENTDC         EQU 0080h
CS_NOCLOSE          EQU 0200h
CS_SAVEBITS         EQU 0800h
CS_BYTEALIGNCLIENT  EQU 1000h
CS_BYTEALIGNWINDOW  EQU 2000h
CS_GLOBALCLASS      EQU 4000h

; ShowWindow Commands
SW_HIDE             EQU 0
SW_SHOWNORMAL       EQU 1
SW_NORMAL           EQU 1
SW_SHOWMINIMIZED    EQU 2
SW_SHOWMAXIMIZED    EQU 3
SW_MAXIMIZE         EQU 3
SW_SHOWNOACTIVATE   EQU 4
SW_SHOW             EQU 5
SW_MINIMIZE         EQU 6
SW_SHOWMINNOACTIVE  EQU 7
SW_SHOWNA           EQU 8
SW_RESTORE          EQU 9
SW_SHOWDEFAULT      EQU 10
SW_FORCEMINIMIZE    EQU 11

; Virtual Key Codes
VK_RETURN           EQU 0Dh
VK_ESCAPE           EQU 1Bh
VK_SPACE            EQU 20h
VK_TAB              EQU 09h

; Common Error Codes
ERROR_SUCCESS               EQU 0
ERROR_INVALID_FUNCTION      EQU 1
ERROR_FILE_NOT_FOUND        EQU 2
ERROR_PATH_NOT_FOUND        EQU 3
ERROR_ACCESS_DENIED         EQU 5
ERROR_INVALID_HANDLE        EQU 6
ERROR_NOT_ENOUGH_MEMORY     EQU 8
ERROR_INVALID_DATA          EQU 13
ERROR_WRITE_PROTECT         EQU 19
ERROR_OPEN_FAILED           EQU 110
ERROR_DISK_FULL             EQU 112
ERROR_INVALID_PARAMETER     EQU 87
ERROR_OUTOFMEMORY           EQU 14
ERROR_FILE_EXISTS           EQU 80
ERROR_ALREADY_EXISTS        EQU 183

; WinHTTP Constants
WINHTTP_ACCESS_TYPE_DEFAULT_PROXY  EQU 0
WINHTTP_ACCESS_TYPE_NAMED_PROXY    EQU 3
WINHTTP_ACCESS_TYPE_DIRECT         EQU 1
WINHTTP_FLAG_SECURE                EQU 00800000h

; MessageBox Flags
MB_OK               EQU 00000000h
MB_OKCANCEL         EQU 00000001h
MB_ABORTRETRYIGNORE EQU 00000002h
MB_YESNOCANCEL      EQU 00000003h
MB_YESNO            EQU 00000004h
MB_RETRYCANCEL      EQU 00000005h
MB_ICONHAND         EQU 00000010h
MB_ICONQUESTION     EQU 00000020h
MB_ICONEXCLAMATION  EQU 00000030h
MB_ICONASTERISK     EQU 00000040h
MB_ICONWARNING      EQU MB_ICONEXCLAMATION
MB_ICONERROR        EQU MB_ICONHAND
MB_ICONINFORMATION  EQU MB_ICONASTERISK

; File Access Modes
GENERIC_READ        EQU 80000000h
GENERIC_WRITE       EQU 40000000h
GENERIC_EXECUTE     EQU 20000000h
GENERIC_ALL         EQU 10000000h

; File Share Modes
FILE_SHARE_READ     EQU 00000001h
FILE_SHARE_WRITE    EQU 00000002h
FILE_SHARE_DELETE   EQU 00000004h

; File Creation Disposition
CREATE_NEW          EQU 1
CREATE_ALWAYS       EQU 2
OPEN_EXISTING       EQU 3
OPEN_ALWAYS         EQU 4
TRUNCATE_EXISTING   EQU 5

; File Attributes
FILE_ATTRIBUTE_READONLY             EQU 00000001h
FILE_ATTRIBUTE_HIDDEN               EQU 00000002h
FILE_ATTRIBUTE_SYSTEM               EQU 00000004h
FILE_ATTRIBUTE_DIRECTORY            EQU 00000010h
FILE_ATTRIBUTE_ARCHIVE              EQU 00000020h
FILE_ATTRIBUTE_DEVICE               EQU 00000040h
FILE_ATTRIBUTE_NORMAL               EQU 00000080h
FILE_ATTRIBUTE_TEMPORARY            EQU 00000100h
FILE_ATTRIBUTE_SPARSE_FILE          EQU 00000200h
FILE_ATTRIBUTE_REPARSE_POINT        EQU 00000400h
FILE_ATTRIBUTE_COMPRESSED           EQU 00000800h
FILE_ATTRIBUTE_OFFLINE              EQU 00001000h
FILE_ATTRIBUTE_NOT_CONTENT_INDEXED  EQU 00002000h
FILE_ATTRIBUTE_ENCRYPTED            EQU 00004000h

; Memory Allocation Flags
GMEM_FIXED          EQU 0000h
GMEM_MOVEABLE       EQU 0002h
GMEM_ZEROINIT       EQU 0040h
GMEM_SHARE          EQU 2000h
GHND                EQU (GMEM_MOVEABLE OR GMEM_ZEROINIT)
GPTR                EQU (GMEM_FIXED OR GMEM_ZEROINIT)

; Memory Mapping Constants
PAGE_READONLY       EQU 02h
PAGE_READWRITE      EQU 04h
FILE_MAP_READ       EQU 0004h
FILE_MAP_WRITE      EQU 0002h
FILE_MAP_COPY       EQU 0001h
INVALID_FILE_SIZE   EQU 0FFFFFFFFh

; Common Control Classes
ICC_LISTVIEW_CLASSES    EQU 00000001h
ICC_TREEVIEW_CLASSES    EQU 00000002h
ICC_BAR_CLASSES         EQU 00000004h
ICC_TAB_CLASSES         EQU 00000008h
ICC_UPDOWN_CLASS        EQU 00000010h
ICC_PROGRESS_CLASS      EQU 00000020h
ICC_HOTKEY_CLASS        EQU 00000040h
ICC_ANIMATE_CLASS       EQU 00000080h
ICC_WIN95_CLASSES       EQU 000000FFh
ICC_DATE_CLASSES        EQU 00000100h
ICC_USEREX_CLASSES      EQU 00000200h
ICC_COOL_CLASSES        EQU 00000400h
ICC_STANDARD_CLASSES    EQU 00004000h

; TreeView Styles
TVS_HASLINES            EQU 0002h
TVS_HASBUTTONS          EQU 0001h
TVS_LINESATROOT         EQU 0004h
TVS_EDITLABELS          EQU 0008h
TVS_SHOWSELALWAYS       EQU 0020h
TVS_CHECKBOXES          EQU 0100h
TVS_TRACKSELECT         EQU 0200h
TVS_FULLROWSELECT       EQU 1000h

; TreeView Messages
TV_FIRST                EQU 1100h
TVM_INSERTITEMA         EQU (TV_FIRST + 0)
TVM_DELETEITEM          EQU (TV_FIRST + 1)
TVM_EXPAND              EQU (TV_FIRST + 2)
TVM_GETITEMRECT         EQU (TV_FIRST + 4)
TVM_GETCOUNT            EQU (TV_FIRST + 5)
TVM_GETITEMA            EQU (TV_FIRST + 12)
TVM_SETITEMA            EQU (TV_FIRST + 13)
TVM_GETNEXTITEM         EQU (TV_FIRST + 10)
TVM_SELECTITEM          EQU (TV_FIRST + 11)
TVM_SETIMAGELIST        EQU (TV_FIRST + 9)
TVM_SETEXTENDEDSTYLE    EQU (TV_FIRST + 44)
TVM_INSERTITEM          EQU TVM_INSERTITEMA
TVM_GETITEM             EQU TVM_GETITEMA
TVM_SETITEM             EQU TVM_SETITEMA

; TreeView Item Flags
TVIF_TEXT               EQU 0001h
TVIF_IMAGE              EQU 0002h
TVIF_PARAM              EQU 0004h
TVIF_STATE              EQU 0008h
TVIF_HANDLE             EQU 0010h
TVIF_SELECTEDIMAGE      EQU 0020h
TVIF_CHILDREN           EQU 0040h

; TreeView Item States
TVIS_SELECTED           EQU 0002h
TVIS_CUT                EQU 0004h
TVIS_DROPHILITED        EQU 0008h
TVIS_BOLD               EQU 0010h
TVIS_EXPANDED           EQU 0020h
TVIS_EXPANDEDONCE       EQU 0040h

; TreeView GetNextItem Flags
TVGN_ROOT               EQU 0000h
TVGN_NEXT               EQU 0001h
TVGN_PREVIOUS           EQU 0002h
TVGN_PARENT             EQU 0003h
TVGN_CHILD              EQU 0004h
TVGN_FIRSTVISIBLE       EQU 0005h
TVGN_NEXTVISIBLE        EQU 0006h
TVGN_PREVIOUSVISIBLE    EQU 0007h
TVGN_DROPHILITE         EQU 0008h
TVGN_CARET              EQU 0009h

; TreeView Insert After Flags
TVI_ROOT                EQU 0FFFF0000h
TVI_FIRST               EQU 0FFFF0001h
TVI_LAST                EQU 0FFFF0002h
TVI_SORT                EQU 0FFFF0003h

; TreeView Notifications
TVN_FIRST               EQU -400
TVN_SELCHANGINGA        EQU (TVN_FIRST-1)
TVN_SELCHANGEDA         EQU (TVN_FIRST-2)
TVN_GETDISPINFOA        EQU (TVN_FIRST-3)
TVN_SETDISPINFOA        EQU (TVN_FIRST-4)
TVN_ITEMEXPANDINGA      EQU (TVN_FIRST-5)
TVN_ITEMEXPANDEDA       EQU (TVN_FIRST-6)
TVN_BEGINDRAG           EQU (TVN_FIRST-7)
TVN_BEGINRDRAG          EQU (TVN_FIRST-8)
TVN_DELETEITEMA         EQU (TVN_FIRST-9)
TVN_BEGINLABELEDITA     EQU (TVN_FIRST-10)
TVN_ENDLABELEDITA       EQU (TVN_FIRST-11)
TVN_SELCHANGING         EQU TVN_SELCHANGINGA
TVN_SELCHANGED          EQU TVN_SELCHANGEDA
TVN_ITEMEXPANDING       EQU TVN_ITEMEXPANDINGA
TVN_ITEMEXPANDED        EQU TVN_ITEMEXPANDEDA

; TreeView Expand Flags
TVE_COLLAPSE            EQU 0001h
TVE_EXPAND              EQU 0002h
TVE_TOGGLE              EQU 0003h

; Tab Control Styles
TCS_TABS                EQU 0000h
TCS_BUTTONS             EQU 0100h
TCS_FORCEICONLEFT       EQU 0010h
TCS_FORCELABELLEFT      EQU 0020h
TCS_HOTTRACK            EQU 0040h
TCS_VERTICAL            EQU 0080h
TCS_MULTILINE           EQU 0200h
TCS_FIXEDWIDTH          EQU 0400h
TCS_RAGGEDRIGHT         EQU 0800h
TCS_FOCUSONBUTTONDOWN   EQU 1000h
TCS_OWNERDRAWFIXED      EQU 2000h
TCS_TOOLTIPS            EQU 4000h
TCS_FOCUSNEVER          EQU 8000h

; Tab Control Messages
TCM_FIRST               EQU 1300h
TCM_GETIMAGELIST        EQU (TCM_FIRST + 2)
TCM_SETIMAGELIST        EQU (TCM_FIRST + 3)
TCM_GETITEMCOUNT        EQU (TCM_FIRST + 4)
TCM_GETITEMA            EQU (TCM_FIRST + 5)
TCM_SETITEMA            EQU (TCM_FIRST + 6)
TCM_INSERTITEMA         EQU (TCM_FIRST + 7)
TCM_DELETEITEM          EQU (TCM_FIRST + 8)
TCM_DELETEALLITEMS      EQU (TCM_FIRST + 9)
TCM_GETITEMRECT         EQU (TCM_FIRST + 10)
TCM_GETCURSEL           EQU (TCM_FIRST + 11)
TCM_SETCURSEL           EQU (TCM_FIRST + 12)
TCM_GETITEM             EQU TCM_GETITEMA
TCM_SETITEM             EQU TCM_SETITEMA
TCM_INSERTITEM          EQU TCM_INSERTITEMA

; Tab Control Item Flags
TCIF_TEXT               EQU 0001h
TCIF_IMAGE              EQU 0002h
TCIF_RTLREADING         EQU 0004h
TCIF_PARAM              EQU 0008h
TCIF_STATE              EQU 0010h

; Tab Control Notifications
TCN_FIRST               EQU -550
TCN_KEYDOWN             EQU (TCN_FIRST - 0)
TCN_SELCHANGE           EQU (TCN_FIRST - 1)
TCN_SELCHANGING         EQU (TCN_FIRST - 2)

; Edit Control Styles
ES_LEFT                 EQU 0000h
ES_CENTER               EQU 0001h
ES_RIGHT                EQU 0002h
ES_MULTILINE            EQU 0004h
ES_UPPERCASE            EQU 0008h
ES_LOWERCASE            EQU 0010h
ES_PASSWORD             EQU 0020h
ES_AUTOVSCROLL          EQU 0040h
ES_AUTOHSCROLL          EQU 0080h
ES_NOHIDESEL            EQU 0100h
ES_OEMCONVERT           EQU 0400h
ES_READONLY             EQU 0800h
ES_WANTRETURN           EQU 1000h
ES_NUMBER               EQU 2000h

; Edit Control Messages
EM_GETSEL               EQU 00B0h
EM_SETSEL               EQU 00B1h
EM_GETRECT              EQU 00B2h
EM_SETRECT              EQU 00B3h
EM_SCROLL               EQU 00B5h
EM_LINESCROLL           EQU 00B6h
EM_SCROLLCARET          EQU 00B7h
EM_GETMODIFY            EQU 00B8h
EM_SETMODIFY            EQU 00B9h
EM_GETLINECOUNT         EQU 00BAh
EM_LINEINDEX            EQU 00BBh
EM_SETHANDLE            EQU 00BCh
EM_GETHANDLE            EQU 00BDh
EM_GETTHUMB             EQU 00BEh
EM_LINELENGTH           EQU 00C1h
EM_REPLACESEL           EQU 00C2h
EM_GETLINE              EQU 00C4h
EM_LIMITTEXT            EQU 00C5h
EM_CANUNDO              EQU 00C6h
EM_UNDO                 EQU 00C7h
EM_FMTLINES             EQU 00C8h
EM_LINEFROMCHAR         EQU 00C9h
EM_SETTABSTOPS          EQU 00CBh
EM_SETPASSWORDCHAR      EQU 00CCh
EM_EMPTYUNDOBUFFER      EQU 00CDh
EM_GETFIRSTVISIBLELINE  EQU 00CEh
EM_SETREADONLY          EQU 00CFh
EM_SETBKGNDCOLOR        EQU 1043h

; Progress Bar Styles
PBS_SMOOTH              EQU 01h
PBS_VERTICAL            EQU 04h

; Progress Bar Messages
PBM_SETRANGE            EQU (WM_USER+1)
PBM_SETPOS              EQU (WM_USER+2)
PBM_DELTAPOS            EQU (WM_USER+3)
PBM_SETSTEP             EQU (WM_USER+4)
PBM_STEPIT              EQU (WM_USER+5)
PBM_SETRANGE32          EQU (WM_USER+6)

; Status Bar Styles
SBARS_SIZEGRIP          EQU 0100h
SBT_OWNERDRAW           EQU 1000h
SBT_NOBORDERS           EQU 0100h
SBT_POPOUT              EQU 0200h
SBT_RTLREADING          EQU 0400h

; Status Bar Messages
SB_SETTEXT              EQU (WM_USER+1)
SB_GETTEXT              EQU (WM_USER+2)
SB_GETTEXTLENGTH        EQU (WM_USER+3)
SB_SETPARTS             EQU (WM_USER+4)
SB_GETPARTS             EQU (WM_USER+6)
SB_GETBORDERS           EQU (WM_USER+7)
SB_SETMINHEIGHT         EQU (WM_USER+8)
SB_SIMPLE               EQU (WM_USER+9)

; Toolbar Styles
TBSTYLE_BUTTON          EQU 0000h
TBSTYLE_SEP             EQU 0001h
TBSTYLE_CHECK           EQU 0002h
TBSTYLE_GROUP           EQU 0004h
TBSTYLE_CHECKGROUP      EQU (TBSTYLE_GROUP OR TBSTYLE_CHECK)
TBSTYLE_DROPDOWN        EQU 0008h
TBSTYLE_AUTOSIZE        EQU 0010h
TBSTYLE_NOPREFIX        EQU 0020h
TBSTYLE_TOOLTIPS        EQU 0100h
TBSTYLE_WRAPABLE        EQU 0200h
TBSTYLE_ALTDRAG         EQU 0400h
TBSTYLE_FLAT            EQU 0800h
TBSTYLE_LIST            EQU 1000h
TBSTYLE_CUSTOMERASE     EQU 2000h
TBSTYLE_TRANSPARENT     EQU 8000h

; SetWindowPos Flags (SWP_)
SWP_NOSIZE              EQU 0001h
SWP_NOMOVE              EQU 0002h
SWP_NOZORDER            EQU 0004h
SWP_NOREDRAW            EQU 0008h
SWP_NOACTIVATE          EQU 0010h
SWP_FRAMECHANGED        EQU 0020h
SWP_SHOWWINDOW          EQU 0040h
SWP_HIDEWINDOW          EQU 0080h
SWP_NOCOPYBITS          EQU 0100h
SWP_NOOWNERZORDER       EQU 0200h
SWP_NOSENDCHANGING      EQU 0400h
SWP_DRAWFRAME           EQU SWP_FRAMECHANGED
SWP_NOREPOSITION        EQU SWP_NOOWNERZORDER

; Static Control Styles (SS_)
SS_LEFT                 EQU 0000h
SS_CENTER               EQU 0001h
SS_RIGHT                EQU 0002h
SS_ICON                 EQU 0003h
SS_BLACKRECT            EQU 0004h
SS_GRAYRECT             EQU 0005h
SS_WHITERECT            EQU 0006h
SS_BLACKFRAME           EQU 0007h
SS_GRAYFRAME            EQU 0008h
SS_WHITEFRAME           EQU 0009h
SS_USERITEM             EQU 000Ah
SS_SIMPLE               EQU 000Bh
SS_LEFTNOWORDWRAP       EQU 000Ch
SS_OWNERDRAW            EQU 000Dh
SS_BITMAP               EQU 000Eh
SS_ENHMETAFILE          EQU 000Fh
SS_ETCHEDHORZ           EQU 0010h
SS_ETCHEDVERT           EQU 0011h
SS_ETCHEDFRAME          EQU 0012h
SS_TYPEMASK             EQU 000Fh
SS_REALSIZECONTROL      EQU 0040h
SS_NOPREFIX             EQU 0080h
SS_NOTIFY               EQU 0100h
SS_CENTERIMAGE          EQU 0200h
SS_RIGHTJUST            EQU 0400h
SS_REALSIZEIMAGE        EQU 0800h
SS_SUNKEN               EQU 1000h
SS_EDITCONTROL          EQU 2000h
SS_ENDELLIPSIS          EQU 4000h
SS_PATHELLIPSIS         EQU 8000h
SS_WORDELLIPSIS         EQU 0C000h
SS_ELLIPSISMASK         EQU 0C000h

; Toolbar Messages
TB_ENABLEBUTTON         EQU (WM_USER + 1)
TB_CHECKBUTTON          EQU (WM_USER + 2)
TB_PRESSBUTTON          EQU (WM_USER + 3)
TB_HIDEBUTTON           EQU (WM_USER + 4)
TB_INDETERMINATE        EQU (WM_USER + 5)
TB_ISBUTTONENABLED      EQU (WM_USER + 9)
TB_ISBUTTONCHECKED      EQU (WM_USER + 10)
TB_ISBUTTONPRESSED      EQU (WM_USER + 11)
TB_ISBUTTONHIDDEN       EQU (WM_USER + 12)
TB_SETSTATE             EQU (WM_USER + 17)
TB_GETSTATE             EQU (WM_USER + 18)
TB_ADDBITMAP            EQU (WM_USER + 19)
TB_ADDBUTTONS           EQU (WM_USER + 20)
TB_INSERTBUTTON         EQU (WM_USER + 21)
TB_DELETEBUTTON         EQU (WM_USER + 22)
TB_GETBUTTON            EQU (WM_USER + 23)
TB_BUTTONCOUNT          EQU (WM_USER + 24)
TB_COMMANDTOINDEX       EQU (WM_USER + 25)
TB_AUTOSIZE             EQU (WM_USER + 33)

; ComboBox Messages
CB_ADDSTRING            EQU 0143h
CB_DELETESTRING         EQU 0144h
CB_DIR                  EQU 0145h
CB_GETCOUNT             EQU 0146h
CB_GETCURSEL            EQU 0147h
CB_GETLBTEXT            EQU 0148h
CB_GETLBTEXTLEN         EQU 0149h
CB_INSERTSTRING         EQU 014Ah
CB_RESETCONTENT         EQU 014Bh
CB_FINDSTRING           EQU 014Ch
CB_SELECTSTRING         EQU 014Dh
CB_SETCURSEL            EQU 014Eh

; OpenFileName Flags
OFN_READONLY            EQU 00000001h
OFN_OVERWRITEPROMPT     EQU 00000002h
OFN_HIDEREADONLY        EQU 00000004h
OFN_NOCHANGEDIR         EQU 00000008h
OFN_SHOWHELP            EQU 00000010h
OFN_ENABLEHOOK          EQU 00000020h
OFN_NOVALIDATE          EQU 00000100h
OFN_ALLOWMULTISELECT    EQU 00000200h
OFN_EXTENSIONDIFFERENT  EQU 00000400h
OFN_PATHMUSTEXIST       EQU 00000800h
OFN_FILEMUSTEXIST       EQU 00001000h
OFN_CREATEPROMPT        EQU 00002000h
OFN_SHAREAWARE          EQU 00004000h
OFN_NOREADONLYRETURN    EQU 00008000h
OFN_NOTESTFILECREATE    EQU 00010000h
OFN_NONETWORKBUTTON     EQU 00020000h

; Font Constants
FW_DONTCARE             EQU 0
FW_THIN                 EQU 100
FW_EXTRALIGHT           EQU 200
FW_LIGHT                EQU 300
FW_NORMAL               EQU 400
FW_MEDIUM               EQU 500
FW_SEMIBOLD             EQU 600
FW_BOLD                 EQU 700
FW_EXTRABOLD            EQU 800
FW_HEAVY                EQU 900

DEFAULT_CHARSET         EQU 1
ANSI_CHARSET            EQU 0
SYMBOL_CHARSET          EQU 2
OUT_DEFAULT_PRECIS      EQU 0
OUT_TT_PRECIS           EQU 4
CLIP_DEFAULT_PRECIS     EQU 0

; Stock Object Constants (for GetStockObject)
WHITE_BRUSH             EQU 0
LTGRAY_BRUSH            EQU 1
GRAY_BRUSH              EQU 2
DKGRAY_BRUSH            EQU 3
BLACK_BRUSH             EQU 4
NULL_BRUSH              EQU 5
WHITE_PEN               EQU 6
BLACK_PEN               EQU 7
NULL_PEN                EQU 8
OEM_FIXED_FONT          EQU 10
ANSI_FIXED_FONT         EQU 11
ANSI_VAR_FONT           EQU 12
SYSTEM_FONT             EQU 13
DEVICE_DEFAULT_FONT     EQU 14
DEFAULT_PALETTE         EQU 15
DEFAULT_GUI_FONT        EQU 17

; Cursor Constants
IDC_ARROW               EQU 32512
IDC_IBEAM               EQU 32513
IDC_WAIT                EQU 32514
IDC_CROSS               EQU 32515
IDC_UPARROW             EQU 32516
IDC_SIZENWSE            EQU 32642
IDC_SIZENESW            EQU 32643
IDC_SIZEWE              EQU 32644
IDC_SIZENS              EQU 32645
IDC_SIZEALL             EQU 32646
IDC_NO                  EQU 32648
IDC_HAND                EQU 32649
IDC_APPSTARTING         EQU 32650
IDC_HELP                EQU 32651
DEFAULT_QUALITY         EQU 0
CLEARTYPE_QUALITY       EQU 5
DEFAULT_PITCH           EQU 0
FIXED_PITCH             EQU 1
VARIABLE_PITCH          EQU 2
FF_DONTCARE             EQU 00h
FF_ROMAN                EQU 10h
FF_SWISS                EQU 20h
FF_MODERN               EQU 30h
FF_SCRIPT               EQU 40h
FF_DECORATIVE           EQU 50h

; GDI Constants
TRANSPARENT             EQU 1
OPAQUE                  EQU 2
BLACKNESS               EQU 00000042h
NOTSRCERASE             EQU 001100A6h
NOTSRCCOPY              EQU 00330008h
SRCERASE                EQU 00440328h
DSTINVERT               EQU 00550009h
PATINVERT               EQU 005A0049h
SRCINVERT               EQU 00660046h
SRCAND                  EQU 008800C6h
MERGEPAINT              EQU 00BB0226h
MERGECOPY               EQU 00C000CAh
SRCCOPY                 EQU 00CC0020h
SRCPAINT                EQU 00EE0086h
PATCOPY                 EQU 00F00021h
PATPAINT                EQU 00FB0A09h
WHITENESS               EQU 00FF0062h

; Color constants
CLR_INVALID             EQU 0FFFFFFFFh

; COM Constants
COINIT_APARTMENTTHREADED EQU 2
COINIT_MULTITHREADED     EQU 0
COINIT_DISABLE_OLE1DDE   EQU 4
COINIT_SPEED_OVER_MEMORY EQU 8

; Path constants
MAX_PATH                EQU 260

; Notification Messages
NM_FIRST                EQU 0
NM_CLICK                EQU (NM_FIRST-2)
NM_DBLCLK               EQU (NM_FIRST-3)
NM_RETURN               EQU (NM_FIRST-4)
NM_RCLICK               EQU (NM_FIRST-5)
NM_RDBLCLK              EQU (NM_FIRST-6)
NM_SETFOCUS             EQU (NM_FIRST-7)
NM_KILLFOCUS            EQU (NM_FIRST-8)
NM_CUSTOMDRAW           EQU (NM_FIRST-12)
NM_HOVER                EQU (NM_FIRST-13)
NM_NCHITTEST            EQU (NM_FIRST-14)
NM_KEYDOWN              EQU (NM_FIRST-15)
NM_RELEASEDCAPTURE      EQU (NM_FIRST-16)
NM_CHAR                 EQU (NM_FIRST-18)
NM_TOOLTIPSCREATED      EQU (NM_FIRST-19)

; Menu Flags
MF_ENABLED              EQU 00000000h
MF_GRAYED               EQU 00000001h
MF_DISABLED             EQU 00000002h

; Track Popup Menu Flags
TPM_LEFTBUTTON          EQU 0000h
TPM_RIGHTBUTTON         EQU 0002h
TPM_LEFTALIGN           EQU 0000h
TPM_CENTERALIGN         EQU 0004h
TPM_RIGHTALIGN          EQU 0008h
TPM_RETURNCMD           EQU 0100h

; Icon and cursor constants
IDI_APPLICATION         EQU 32512
IDC_ARROW               EQU 32512
CW_USEDEFAULT           EQU 080000000h

; Hotkey constants
MOD_ALT                 EQU 0001h
MOD_CONTROL             EQU 0002h
MOD_SHIFT               EQU 0004h
MOD_WIN                 EQU 0008h

; WM_COMMAND codes
PM_REMOVE               EQU 0001h
ILC_COLOR               EQU 0000h
ILC_COLOR4              EQU 0004h
ILC_COLOR8              EQU 0008h
ILC_COLOR16             EQU 0010h
ILC_COLOR24             EQU 0018h
ILC_COLOR32             EQU 0020h
ILC_COLORDDB            EQU 00FEh
ILC_MIRROR              EQU 2000h
ILC_PERITEMMIRROR       EQU 8000h

; Common Control Styles
CCS_TOP                 EQU 00000001h
CCS_NOMOVEY             EQU 00000002h
CCS_BOTTOM              EQU 00000003h
CCS_NORESIZE            EQU 00000004h
CCS_NOPARENTALIGN       EQU 00000008h
CCS_ADJUSTABLE          EQU 00000020h
CCS_NODIVIDER           EQU 00000040h
CCS_VERT                EQU 00000080h
CCS_LEFT                EQU (CCS_VERT OR CCS_TOP)
CCS_RIGHT               EQU (CCS_VERT OR CCS_BOTTOM)
CCS_NOMOVEX             EQU (CCS_VERT OR CCS_NOMOVEY)

; ============================================================================
; STRUCTURES (using DWORD for all handle/pointer types)
; ============================================================================

POINT STRUCT
    x   DWORD ?
    y   DWORD ?
POINT ENDS

RECT STRUCT
    left    DWORD ?
    top     DWORD ?
    right   DWORD ?
    bottom  DWORD ?
RECT ENDS

MSG STRUCT
    hwnd        DWORD ?
    message     DWORD ?
    wParam      DWORD ?
    lParam      DWORD ?
    time        DWORD ?
    pt          POINT <>
MSG ENDS

WNDCLASSEX STRUCT
    cbSize          DWORD ?
    style           DWORD ?
    lpfnWndProc     DWORD ?
    cbClsExtra      DWORD ?
    cbWndExtra      DWORD ?
    hInstance       DWORD ?
    hIcon           DWORD ?
    hCursor         DWORD ?
    hbrBackground   DWORD ?
    lpszMenuName    DWORD ?
    lpszClassName   DWORD ?
    hIconSm         DWORD ?
WNDCLASSEX ENDS

PAINTSTRUCT STRUCT
    hdc             DWORD ?
    fErase          DWORD ?
    rcPaint         RECT <>
    fRestore        DWORD ?
    fIncUpdate      DWORD ?
    rgbReserved     BYTE 32 DUP(?)
PAINTSTRUCT ENDS

FILETIME STRUCT
    dwLowDateTime   DWORD ?
    dwHighDateTime  DWORD ?
FILETIME ENDS

WIN32_FIND_DATA STRUCT
    dwFileAttributes    DWORD ?
    ftCreationTime      FILETIME <>
    ftLastAccessTime    FILETIME <>
    ftLastWriteTime     FILETIME <>
    nFileSizeHigh       DWORD ?
    nFileSizeLow        DWORD ?
    dwReserved0         DWORD ?
    dwReserved1         DWORD ?
    cFileName           BYTE MAX_PATH DUP(?)
    cAlternateFileName  BYTE 14 DUP(?)
WIN32_FIND_DATA ENDS

NMHDR STRUCT
    hwndFrom    DWORD ?
    idFrom      DWORD ?
    code        DWORD ?
NMHDR ENDS

TVITEM STRUCT
    imask           DWORD ?
    hItem           DWORD ?
    state           DWORD ?
    stateMask       DWORD ?
    pszText         DWORD ?
    cchTextMax      DWORD ?
    iImage          DWORD ?
    iSelectedImage  DWORD ?
    cChildren       DWORD ?
    lParam          DWORD ?
TVITEM ENDS

TVINSERTSTRUCT STRUCT
    hParent         DWORD ?
    hInsertAfter    DWORD ?
    item            TVITEM <>
TVINSERTSTRUCT ENDS

NMTREEVIEW STRUCT
    hdr         NMHDR <>
    action      DWORD ?
    itemOld     TVITEM <>
    itemNew     TVITEM <>
    ptDrag      POINT <>
NMTREEVIEW ENDS

TC_ITEM STRUCT
    imask        DWORD ?
    lpReserved1 DWORD ?
    lpReserved2 DWORD ?
    pszText     DWORD ?
    cchTextMax  DWORD ?
    iImage      DWORD ?
    lParam      DWORD ?
TC_ITEM ENDS

INITCOMMONCONTROLSEX STRUCT
    dwSize  DWORD ?
    dwICC   DWORD ?
INITCOMMONCONTROLSEX ENDS

OPENFILENAME STRUCT
    lStructSize         DWORD ?
    hwndOwner           DWORD ?
    hInstance           DWORD ?
    lpstrFilter         DWORD ?
    lpstrCustomFilter   DWORD ?
    nMaxCustFilter      DWORD ?
    nFilterIndex        DWORD ?
    lpstrFile           DWORD ?
    nMaxFile            DWORD ?
    lpstrFileTitle      DWORD ?
    nMaxFileTitle       DWORD ?
    lpstrInitialDir     DWORD ?
    lpstrTitle          DWORD ?
    Flags               DWORD ?
    nFileOffset         WORD ?
    nFileExtension      WORD ?
    lpstrDefExt         DWORD ?
    lCustData           DWORD ?
    lpfnHook            DWORD ?
    lpTemplateName      DWORD ?
OPENFILENAME ENDS

; Process Creation Structures
STARTUPINFO STRUCT
    cb              DWORD ?
    lpReserved      DWORD ?
    lpDesktop       DWORD ?
    lpTitle         DWORD ?
    dwX             DWORD ?
    dwY             DWORD ?
    dwXSize         DWORD ?
    dwYSize         DWORD ?
    dwXCountChars   DWORD ?
    dwYCountChars   DWORD ?
    dwFillAttribute DWORD ?
    dwFlags         DWORD ?
    wShowWindow     WORD ?
    cbReserved2     WORD ?
    lpReserved2     DWORD ?
    hStdInput       DWORD ?
    hStdOutput      DWORD ?
    hStdError       DWORD ?
STARTUPINFO ENDS

PROCESS_INFORMATION STRUCT
    hProcess    DWORD ?
    hThread     DWORD ?
    dwProcessId DWORD ?
    dwThreadId  DWORD ?
PROCESS_INFORMATION ENDS

; Debugging Structures
EXCEPTION_RECORD STRUCT
    ExceptionCode        DWORD ?
    ExceptionFlags       DWORD ?
    ExceptionRecord      DWORD ?      ; pointer
    ExceptionAddress     DWORD ?
    NumberParameters     DWORD ?
    ExceptionInformation DWORD 15 DUP(?)
EXCEPTION_RECORD ENDS

EXCEPTION_DEBUG_INFO STRUCT
    ExceptionRecord EXCEPTION_RECORD <>
    dwFirstChance   DWORD ?
EXCEPTION_DEBUG_INFO ENDS

CREATE_THREAD_DEBUG_INFO STRUCT
    hThread           DWORD ?
    lpThreadLocalBase DWORD ?
    lpStartAddress    DWORD ?
CREATE_THREAD_DEBUG_INFO ENDS

CREATE_PROCESS_DEBUG_INFO STRUCT
    hFile                 DWORD ?
    hProcess              DWORD ?
    hThread               DWORD ?
    lpBaseOfImage         DWORD ?
    dwDebugInfoFileOffset DWORD ?
    nDebugInfoSize        DWORD ?
    lpThreadLocalBase     DWORD ?
    lpStartAddress        DWORD ?
    lpImageName           DWORD ?
    fUnicode              DWORD ?
CREATE_PROCESS_DEBUG_INFO ENDS

EXIT_THREAD_DEBUG_INFO STRUCT
    dwExitCode DWORD ?
EXIT_THREAD_DEBUG_INFO ENDS

EXIT_PROCESS_DEBUG_INFO STRUCT
    dwExitCode DWORD ?
EXIT_PROCESS_DEBUG_INFO ENDS

DEBUG_EVENT STRUCT
    dwDebugEventCode DWORD ?
    dwProcessId      DWORD ?
    dwThreadId       DWORD ?
    u                BYTE 160 DUP(?)  ; large enough for union payloads
DEBUG_EVENT ENDS

; x86 CONTEXT (minimal layout) – field names prefixed to avoid reserved-word clash
CONTEXT STRUCT
    ContextFlags       DWORD ?
    ctx_Dr0            DWORD ?
    ctx_Dr1            DWORD ?
    ctx_Dr2            DWORD ?
    ctx_Dr3            DWORD ?
    ctx_Dr6            DWORD ?
    ctx_Dr7            DWORD ?
    FloatSave          BYTE 112 DUP(?)
    ctx_SegGs          DWORD ?
    ctx_SegFs          DWORD ?
    ctx_SegEs          DWORD ?
    ctx_SegDs          DWORD ?
    ctx_Edi            DWORD ?
    ctx_Esi            DWORD ?
    ctx_Ebx            DWORD ?
    ctx_Edx            DWORD ?
    ctx_Ecx            DWORD ?
    ctx_Eax            DWORD ?
    ctx_Ebp            DWORD ?
    ctx_Eip            DWORD ?
    ctx_SegCs          DWORD ?
    ctx_EFlags         DWORD ?
    ctx_Esp            DWORD ?
    ctx_SegSs          DWORD ?
    ExtendedRegisters  BYTE 512 DUP(?)
CONTEXT ENDS

; Performance API structures
PROCESS_MEMORY_COUNTERS STRUCT
    cb                          DWORD ?
    PageFaultCount              DWORD ?
    PeakWorkingSetSize          DWORD ?
    WorkingSetSize              DWORD ?
    QuotaPeakPagedPoolUsage     DWORD ?
    QuotaPagedPoolUsage         DWORD ?
    QuotaPeakNonPagedPoolUsage  DWORD ?
    QuotaNonPagedPoolUsage      DWORD ?
    PagefileUsage               DWORD ?
    PeakPagefileUsage           DWORD ?
PROCESS_MEMORY_COUNTERS ENDS

; ============================================================================
; FUNCTION PROTOTYPES (EXTERN)
; ============================================================================

; Kernel32
EXTERN GetModuleHandleA:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteConsoleA:PROC
EXTERN ExitProcess:PROC
EXTERN GetTickCount:PROC
EXTERN QueryPerformanceCounter:PROC
EXTERN QueryPerformanceFrequency:PROC
EXTERN GetLastError:PROC
EXTERN SetLastError:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetFileSize:PROC
EXTERN FindFirstFileA:PROC
EXTERN FindNextFileA:PROC
EXTERN FindClose:PROC
EXTERN GlobalAlloc:PROC
EXTERN GlobalFree:PROC
EXTERN GlobalLock:PROC
EXTERN GlobalUnlock:PROC
EXTERN GetCurrentDirectory:PROC
EXTERN SetCurrentDirectory:PROC
EXTERN CreateThread:PROC
EXTERN WaitForSingleObject:PROC
EXTERN WaitForMultipleObjects:PROC
EXTERN CreateEvent:PROC
EXTERN SetEvent:PROC
EXTERN ResetEvent:PROC
EXTERN Sleep:PROC
EXTERN GetCurrentProcess:PROC
EXTERN GetCurrentThread:PROC
EXTERN CreateSemaphore:PROC
EXTERN ReleaseSemaphore:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN InterlockedCompareExchange:PROC
EXTERN InterlockedIncrement:PROC
EXTERN InterlockedDecrement:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC
EXTERN lstrcmpA:PROC
EXTERN lstrlenA:PROC
EXTERN RtlZeroMemory:PROC
EXTERN RtlMoveMemory:PROC
EXTERN MulDiv:PROC
DebugActiveProcess PROTO :DWORD
DebugActiveProcessStop PROTO :DWORD
WaitForDebugEvent PROTO :DWORD, :DWORD
ContinueDebugEvent PROTO :DWORD, :DWORD, :DWORD
CreateProcessA PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
ReadProcessMemory PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
WriteProcessMemory PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
VirtualProtectEx PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
FlushInstructionCache PROTO :DWORD, :DWORD, :DWORD
GetThreadContext PROTO :DWORD, :DWORD
SetThreadContext PROTO :DWORD, :DWORD
SuspendThread PROTO :DWORD
ResumeThread PROTO :DWORD

; Aliases for non-A suffix
GetModuleHandle EQU <GetModuleHandleA>
CreateFile EQU <CreateFileA>
FindFirstFile EQU <FindFirstFileA>
FindNextFile EQU <FindNextFileA>
lstrcpy EQU <lstrcpyA>
lstrcat EQU <lstrcatA>
lstrcmp EQU <lstrcmpA>
lstrlen EQU <lstrlenA>

; User32
InitCommonControls PROTO
CoInitializeEx PROTO :DWORD, :DWORD
CoUninitialize PROTO
RegisterClassExA PROTO :DWORD
CreateWindowExA PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
ShowWindow PROTO :DWORD, :DWORD
UpdateWindow PROTO :DWORD
GetMessageA PROTO :DWORD, :DWORD, :DWORD, :DWORD
TranslateMessage PROTO :DWORD
DispatchMessageA PROTO :DWORD
DefWindowProcA PROTO :DWORD, :DWORD, :DWORD, :DWORD
PostQuitMessage PROTO :DWORD
DestroyWindow PROTO :DWORD
SendMessageA PROTO :DWORD, :DWORD, :DWORD, :DWORD
PostMessageA PROTO :DWORD, :DWORD, :DWORD, :DWORD
GetClientRect PROTO :DWORD, :DWORD
MoveWindow PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
SetWindowTextA PROTO :DWORD, :DWORD
GetWindowTextA PROTO :DWORD, :DWORD, :DWORD
GetWindowTextLengthA PROTO :DWORD
MessageBoxA PROTO :DWORD, :DWORD, :DWORD, :DWORD
LoadIconA PROTO :DWORD, :DWORD
LoadCursorA PROTO :DWORD, :DWORD
BeginPaint PROTO :DWORD, :DWORD
EndPaint PROTO :DWORD, :DWORD
InvalidateRect PROTO :DWORD, :DWORD, :DWORD
SetTimer PROTO :DWORD, :DWORD, :DWORD, :DWORD
KillTimer PROTO :DWORD, :DWORD
GetMessagePos PROTO
EnableMenuItem PROTO :DWORD, :DWORD, :DWORD
TrackPopupMenu PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
SetMenu PROTO :DWORD, :DWORD
RegisterHotKey PROTO :DWORD, :DWORD, :DWORD, :DWORD
UnregisterHotKey PROTO :DWORD, :DWORD
PeekMessageA PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
CreateMenuA PROTO
AppendMenuA PROTO :DWORD, :DWORD, :DWORD, :DWORD
CreatePopupMenu PROTO
GetMenuItemCountA PROTO :DWORD
GetMenuItemIDA PROTO :DWORD, :DWORD, :DWORD

; Aliases
PeekMessage EQU <PeekMessageA>
CreateMenu EQU <CreateMenuA>
AppendMenu EQU <AppendMenuA>
GetMenuItemCount EQU <GetMenuItemCountA>
GetMenuItemID EQU <GetMenuItemIDA>
RegisterClassEx EQU <RegisterClassExA>
CreateWindowEx EQU <CreateWindowExA>
GetMessage EQU <GetMessageA>
DispatchMessage EQU <DispatchMessageA>
DefWindowProc EQU <DefWindowProcA>
SendMessage EQU <SendMessageA>
PostMessage EQU <PostMessageA>
SetWindowText EQU <SetWindowTextA>
GetWindowText EQU <GetWindowTextA>
GetWindowTextLength EQU <GetWindowTextLengthA>
MessageBox EQU <MessageBoxA>
LoadIcon EQU <LoadIconA>
LoadCursor EQU <LoadCursorA>
CreateFont EQU <CreateFontA>

; GDI32
CreateSolidBrush PROTO :DWORD
DeleteObject PROTO :DWORD
SelectObject PROTO :DWORD, :DWORD
SetTextColor PROTO :DWORD, :DWORD
SetBkColor PROTO :DWORD, :DWORD
SetBkMode PROTO :DWORD, :DWORD
TextOutA PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
CreateFontA PROTO :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
GetStockObject PROTO :DWORD
; ComCtl32
EXTERN InitCommonControlsEx:PROC

; ComDlg32
EXTERN GetOpenFileNameA:PROC
EXTERN GetSaveFileNameA:PROC

; Shell32

; OLE32

; Shlwapi
EXTERN PathMatchSpecA:PROC

; Psapi
EXTERN GetProcessMemoryInfo:PROC
EXTERN EmptyWorkingSet:PROC

; WinHttp (stubs - add if needed)

; RichEd20 (stubs - add if needed)

; ============================================================================
; HELPER MACROS
; ============================================================================

; Note: invoke is built into MASM, no need to redefine

; ============================================================================
; END OF INCLUDE
; ============================================================================
