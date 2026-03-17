; win64.inc - Windows 64-bit API declarations
; Minimal version for CODEX REVERSE ENGINE ULTIMATE

; Standard calling conventions
OPTION WIN64:3
OPTION CASEMAP:NONE

; Basic Windows types
NULL                    EQU     0
TRUE                    EQU     1
FALSE                   EQU     0
INVALID_HANDLE_VALUE    EQU     -1

; Standard handles
STD_INPUT_HANDLE        EQU     -10
STD_OUTPUT_HANDLE       EQU     -11
STD_ERROR_HANDLE        EQU     -12

; File access modes
GENERIC_READ            EQU     80000000h
GENERIC_WRITE           EQU     40000000h
GENERIC_EXECUTE         EQU     20000000h
GENERIC_ALL             EQU     10000000h

; File share modes
FILE_SHARE_READ         EQU     1
FILE_SHARE_WRITE        EQU     2
FILE_SHARE_DELETE       EQU     4

; File creation modes
CREATE_NEW              EQU     1
CREATE_ALWAYS           EQU     2
OPEN_EXISTING           EQU     3
OPEN_ALWAYS             EQU     4
TRUNCATE_EXISTING       EQU     5

; File attributes
FILE_ATTRIBUTE_READONLY         EQU     1
FILE_ATTRIBUTE_HIDDEN           EQU     2
FILE_ATTRIBUTE_SYSTEM           EQU     4
FILE_ATTRIBUTE_DIRECTORY        EQU     10h
FILE_ATTRIBUTE_ARCHIVE          EQU     20h
FILE_ATTRIBUTE_NORMAL           EQU     80h
FILE_ATTRIBUTE_TEMPORARY        EQU     100h

; Seek methods
FILE_BEGIN              EQU     0
FILE_CURRENT            EQU     1
FILE_END                EQU     2

; Page protection
PAGE_NOACCESS           EQU     1
PAGE_READONLY           EQU     2
PAGE_READWRITE          EQU     4
PAGE_WRITECOPY          EQU     8
PAGE_EXECUTE            EQU     10h
PAGE_EXECUTE_READ       EQU     20h
PAGE_EXECUTE_READWRITE  EQU     40h

; Memory allocation
MEM_COMMIT              EQU     1000h
MEM_RESERVE             EQU     2000h
MEM_DECOMMIT            EQU     4000h
MEM_RELEASE             EQU     8000h
MEM_FREE                EQU     10000h
MEM_PRIVATE             EQU     20000h
MEM_MAPPED              EQU     40000h
MEM_RESET               EQU     80000h
MEM_TOP_DOWN            EQU     100000h

; Process access rights
PROCESS_TERMINATE               EQU     1
PROCESS_CREATE_THREAD           EQU     2
PROCESS_VM_OPERATION            EQU     8
PROCESS_VM_READ                 EQU     10h
PROCESS_VM_WRITE                EQU     20h
PROCESS_DUP_HANDLE              EQU     40h
PROCESS_CREATE_PROCESS          EQU     80h
PROCESS_SET_QUOTA               EQU     100h
PROCESS_SET_INFORMATION         EQU     200h
PROCESS_QUERY_INFORMATION       EQU     400h
PROCESS_SUSPEND_RESUME          EQU     800h

; Console modes
ENABLE_PROCESSED_INPUT          EQU     1
ENABLE_LINE_INPUT               EQU     2
ENABLE_ECHO_INPUT               EQU     4
ENABLE_WINDOW_INPUT             EQU     8
ENABLE_MOUSE_INPUT              EQU     10h
ENABLE_INSERT_MODE              EQU     20h
ENABLE_QUICK_EDIT_MODE          EQU     40h
ENABLE_EXTENDED_FLAGS           EQU     80h

ENABLE_PROCESSED_OUTPUT         EQU     1
ENABLE_WRAP_AT_EOL_OUTPUT       EQU     2

; Window styles
WS_OVERLAPPED           EQU     0
WS_POPUP                EQU     80000000h
WS_CHILD                EQU     40000000h
WS_MINIMIZE             EQU     20000000h
WS_VISIBLE              EQU     10000000h
WS_DISABLED             EQU     8000000h
WS_CLIPSIBLINGS         EQU     4000000h
WS_CLIPCHILDREN         EQU     2000000h
WS_MAXIMIZE             EQU     1000000h
WS_CAPTION              EQU     C00000h
WS_BORDER               EQU     800000h
WS_DLGFRAME             EQU     400000h
WS_VSCROLL              EQU     200000h
WS_HSCROLL              EQU     100000h
WS_SYSMENU              EQU     80000h
WS_THICKFRAME           EQU     40000h
WS_GROUP                EQU     20000h
WS_TABSTOP              EQU     10000h

WS_MINIMIZEBOX          EQU     20000h
WS_MAXIMIZEBOX          EQU     10000h

WS_TILED                EQU     WS_OVERLAPPED
WS_ICONIC               EQU     WS_MINIMIZE
WS_SIZEBOX              EQU     WS_THICKFRAME
WS_TILEDWINDOW          EQU     WS_OVERLAPPEDWINDOW

WS_OVERLAPPEDWINDOW     EQU     WS_OVERLAPPED OR WS_CAPTION OR WS_SYSMENU OR WS_THICKFRAME OR WS_MINIMIZEBOX OR WS_MAXIMIZEBOX
WS_POPUPWINDOW          EQU     WS_POPUP OR WS_BORDER OR WS_SYSMENU
WS_CHILDWINDOW          EQU     WS_CHILD

; Common window styles
WS_EX_DLGMODALFRAME     EQU     1
WS_EX_NOPARENTNOTIFY    EQU     4
WS_EX_TOPMOST           EQU     8
WS_EX_ACCEPTFILES       EQU     10h
WS_EX_TRANSPARENT       EQU     20h
WS_EX_MDICHILD          EQU     40h
WS_EX_TOOLWINDOW        EQU     80h
WS_EX_WINDOWEDGE        EQU     100h
WS_EX_CLIENTEDGE        EQU     200h
WS_EX_CONTEXTHELP       EQU     400h

WS_EX_RIGHT             EQU     1000h
WS_EX_LEFT              EQU     0
WS_EX_RTLREADING        EQU     2000h
WS_EX_LTRREADING        EQU     0
WS_EX_LEFTSCROLLBAR     EQU     4000h
WS_EX_RIGHTSCROLLBAR    EQU     0

WS_EX_CONTROLPARENT     EQU     10000h
WS_EX_STATICEDGE        EQU     20000h
WS_EX_APPWINDOW         EQU     40000h

WS_EX_OVERLAPPEDWINDOW  EQU     WS_EX_WINDOWEDGE OR WS_EX_CLIENTEDGE
WS_EX_PALETTEWINDOW     EQU     WS_EX_WINDOWEDGE OR WS_EX_TOOLWINDOW OR WS_EX_TOPMOST

; ShowWindow commands
SW_HIDE                 EQU     0
SW_SHOWNORMAL           EQU     1
SW_NORMAL               EQU     1
SW_SHOWMINIMIZED        EQU     2
SW_SHOWMAXIMIZED        EQU     3
SW_MAXIMIZE             EQU     3
SW_SHOWNOACTIVATE       EQU     4
SW_SHOW                 EQU     5
SW_MINIMIZE             EQU     6
SW_SHOWMINNOACTIVE      EQU     7
SW_SHOWNA               EQU     8
SW_RESTORE              EQU     9
SW_SHOWDEFAULT          EQU     10
SW_FORCEMINIMIZE        EQU     11

; Virtual keys
VK_LBUTTON              EQU     1
VK_RBUTTON              EQU     2
VK_CANCEL               EQU     3
VK_MBUTTON              EQU     4
VK_XBUTTON1             EQU     5
VK_XBUTTON2             EQU     6
VK_BACK                 EQU     8
VK_TAB                  EQU     9
VK_CLEAR                EQU     12
VK_RETURN               EQU     13
VK_SHIFT                EQU     10h
VK_CONTROL              EQU     11h
VK_MENU                 EQU     12h
VK_PAUSE                EQU     13h
VK_CAPITAL              EQU     14h
VK_ESCAPE               EQU     1Bh
VK_SPACE                EQU     20h
VK_PRIOR                EQU     21h
VK_NEXT                 EQU     22h
VK_END                  EQU     23h
VK_HOME                 EQU     24h
VK_LEFT                 EQU     25h
VK_UP                   EQU     26h
VK_RIGHT                EQU     27h
VK_DOWN                 EQU     28h
VK_SELECT               EQU     29h
VK_PRINT                EQU     2Ah
VK_EXECUTE              EQU     2Bh
VK_SNAPSHOT             EQU     2Ch
VK_INSERT               EQU     2Dh
VK_DELETE               EQU     2Eh
VK_HELP                 EQU     2Fh

; System parameters
SPI_GETBEEP                     EQU     1
SPI_SETBEEP                     EQU     2
SPI_GETMOUSE                    EQU     3
SPI_SETMOUSE                    EQU     4
SPI_GETBORDER                   EQU     5
SPI_SETBORDER                   EQU     6
SPI_GETKEYBOARDSPEED            EQU     10
SPI_SETKEYBOARDSPEED            EQU     11
SPI_LANGDRIVER                  EQU     12
SPI_ICONHORIZONTALSPACING       EQU     13
SPI_GETSCREENSAVETIMEOUT        EQU     14
SPI_SETSCREENSAVETIMEOUT        EQU     15
SPI_GETSCREENSAVEACTIVE         EQU     16
SPI_SETSCREENSAVEACTIVE         EQU     17
SPI_GETGRIDGRANULARITY          EQU     18
SPI_SETGRIDGRANULARITY          EQU     19
SPI_SETDESKWALLPAPER            EQU     20
SPI_SETDESKPATTERN              EQU     21
SPI_GETKEYBOARDDELAY            EQU     22
SPI_SETKEYBOARDDELAY            EQU     23
SPI_ICONVERTICALSPACING         EQU     24

; GetSystemMetrics
SM_CXSCREEN             EQU     0
SM_CYSCREEN             EQU     1
SM_CXVSCROLL            EQU     2
SM_CYHSCROLL            EQU     3
SM_CYCAPTION            EQU     4
SM_CXBORDER             EQU     5
SM_CYBORDER             EQU     6
SM_CXDLGFRAME           EQU     7
SM_CYDLGFRAME           EQU     8
SM_CYVTHUMB             EQU     9
SM_CXHTHUMB             EQU     10
SM_CXICON               EQU     11
SM_CYICON               EQU     12
SM_CXCURSOR             EQU     13
SM_CYCURSOR             EQU     14
SM_CYMENU               EQU     15
SM_CXFULLSCREEN         EQU     16
SM_CYFULLSCREEN         EQU     17
SM_CYKANJIWINDOW        EQU     18
SM_MOUSEPRESENT         EQU     19
SM_CYVSCROLL            EQU     20
SM_CXHSCROLL            EQU     21
SM_DEBUG                EQU     22
SM_SWAPBUTTON           EQU     23

; Colors
COLOR_SCROLLBAR         EQU     0
COLOR_BACKGROUND        EQU     1
COLOR_ACTIVECAPTION     EQU     2
COLOR_INACTIVECAPTION   EQU     3
COLOR_MENU              EQU     4
COLOR_WINDOW            EQU     5
COLOR_WINDOWFRAME       EQU     6
COLOR_MENUTEXT          EQU     7
COLOR_WINDOWTEXT        EQU     8
COLOR_CAPTIONTEXT       EQU     9
COLOR_ACTIVEBORDER      EQU     10
COLOR_INACTIVEBORDER    EQU     11
COLOR_APPWORKSPACE      EQU     12
COLOR_HIGHLIGHT         EQU     13
COLOR_HIGHLIGHTTEXT     EQU     14
COLOR_BTNFACE           EQU     15
COLOR_BTNSHADOW         EQU     16
COLOR_GRAYTEXT          EQU     17
COLOR_BTNTEXT           EQU     18
COLOR_INACTIVECAPTIONTEXT EQU   19
COLOR_BTNHIGHLIGHT      EQU     20

; GetWindowLong offsets
GWL_WNDPROC             EQU     -4
GWL_HINSTANCE           EQU     -6
GWL_HWNDPARENT          EQU     -8
GWL_STYLE               EQU     -16
GWL_EXSTYLE             EQU     -20
GWL_USERDATA            EQU     -21
GWL_ID                  EQU     -12

; GetClassLong offsets
GCL_MENUNAME            EQU     -8
GCL_HBRBACKGROUND       EQU     -10
GCL_HCURSOR             EQU     -12
GCL_HICON               EQU     -14
GCL_HMODULE             EQU     -16
GCL_CBWNDEXTRA          EQU     -18
GCL_CBCLSEXTRA          EQU     -20
GCL_WNDPROC             EQU     -24
GCL_STYLE               EQU     -26
GCL_HICONSM             EQU     -34
GCLP_MENUNAME           EQU     -8
GCLP_HBRBACKGROUND      EQU     -10
GCLP_HCURSOR            EQU     -12
GCLP_HICON              EQU     -14
GCLP_HMODULE            EQU     -16
GCLP_WNDPROC            EQU     -24
GCLP_HICONSM            EQU     -34

; Message types
WM_NULL                 EQU     0
WM_CREATE               EQU     1
WM_DESTROY              EQU     2
WM_MOVE                 EQU     3
WM_SIZE                 EQU     5
WM_ACTIVATE             EQU     6
WM_SETFOCUS             EQU     7
WM_KILLFOCUS            EQU     8
WM_ENABLE               EQU     10
WM_SETREDRAW            EQU     11
WM_SETTEXT              EQU     12
WM_GETTEXT              EQU     13
WM_GETTEXTLENGTH        EQU     14
WM_PAINT                EQU     15
WM_CLOSE                EQU     16
WM_QUERYENDSESSION      EQU     17
WM_QUIT                 EQU     18
WM_QUERYOPEN            EQU     19
WM_ERASEBKGND           EQU     20
WM_SYSCOLORCHANGE       EQU     21
WM_ENDSESSION           EQU     22
WM_SHOWWINDOW           EQU     24
WM_WININICHANGE         EQU     26
WM_DEVMODECHANGE        EQU     27
WM_ACTIVATEAPP          EQU     28
WM_FONTCHANGE           EQU     29
WM_TIMECHANGE           EQU     30
WM_CANCELMODE           EQU     31
WM_SETCURSOR            EQU     32
WM_MOUSEACTIVATE        EQU     33
WM_CHILDACTIVATE        EQU     34
WM_QUEUESYNC            EQU     35
WM_GETMINMAXINFO        EQU     36
WM_PAINTICON            EQU     38
WM_ICONERASEBKGND       EQU     39
WM_NEXTDLGCTL           EQU     40
WM_SPOOLERSTATUS        EQU     42
WM_DRAWITEM             EQU     43
WM_MEASUREITEM          EQU     44
WM_DELETEITEM           EQU     45
WM_VKEYTOITEM           EQU     46
WM_CHARTOITEM           EQU     47
WM_SETFONT              EQU     48
WM_GETFONT              EQU     49
WM_SETHOTKEY            EQU     50
WM_GETHOTKEY            EQU     51
WM_QUERYDRAGICON        EQU     55
WM_COMPAREITEM          EQU     57
WM_GETOBJECT            EQU     61
WM_COMPACTING           EQU     65
WM_COMMNOTIFY           EQU     68
WM_WINDOWPOSCHANGING    EQU     70
WM_WINDOWPOSCHANGED     EQU     71
WM_POWER                EQU     72
WM_COPYDATA             EQU     74
WM_CANCELJOURNAL        EQU     75
WM_NOTIFY               EQU     78
WM_INPUTLANGCHANGEREQUEST EQU   80
WM_INPUTLANGCHANGE      EQU     81
WM_TCARD                EQU     82
WM_HELP                 EQU     83
WM_USERCHANGED          EQU     84
WM_NOTIFYFORMAT         EQU     85
WM_CONTEXTMENU          EQU     123
WM_STYLECHANGING        EQU     124
WM_STYLECHANGED         EQU     125
WM_DISPLAYCHANGE        EQU     126
WM_GETICON              EQU     127
WM_SETICON              EQU     128
WM_NCCREATE             EQU     129
WM_NCDESTROY            EQU     130
WM_NCCALCSIZE           EQU     131
WM_NCHITTEST            EQU     132
WM_NCPAINT              EQU     133
WM_NCACTIVATE           EQU     134
WM_GETDLGCODE           EQU     135
WM_SYNCPAINT            EQU     136

WM_NCMOUSEMOVE          EQU     160
WM_NCLBUTTONDOWN        EQU     161
WM_NCLBUTTONUP          EQU     162
WM_NCLBUTTONDBLCLK      EQU     163
WM_NCRBUTTONDOWN        EQU     164
WM_NCRBUTTONUP          EQU     165
WM_NCRBUTTONDBLCLK      EQU     166
WM_NCMBUTTONDOWN        EQU     167
WM_NCMBUTTONUP          EQU     168
WM_NCMBUTTONDBLCLK      EQU     169

WM_KEYFIRST             EQU     256
WM_KEYDOWN              EQU     256
WM_KEYUP                EQU     257
WM_CHAR                 EQU     258
WM_DEADCHAR             EQU     259
WM_SYSKEYDOWN           EQU     260
WM_SYSKEYUP             EQU     261
WM_SYSCHAR              EQU     262
WM_SYSDEADCHAR          EQU     263
WM_UNICHAR              EQU     265
WM_KEYLAST              EQU     265

WM_IME_STARTCOMPOSITION EQU     269
WM_IME_ENDCOMPOSITION   EQU     270
WM_IME_COMPOSITION      EQU     271
WM_IME_KEYLAST          EQU     271

WM_INITDIALOG           EQU     272
WM_COMMAND              EQU     273
WM_SYSCOMMAND           EQU     274
WM_TIMER                EQU     275
WM_HSCROLL              EQU     276
WM_VSCROLL              EQU     277
WM_INITMENU             EQU     278
WM_INITMENUPOPUP        EQU     279
WM_GESTURE              EQU     281
WM_GESTURENOTIFY        EQU     282

WM_SYSTIMER             EQU     280

WM_MOUSEFIRST           EQU     512
WM_MOUSEMOVE            EQU     512
WM_LBUTTONDOWN          EQU     513
WM_LBUTTONUP            EQU     514
WM_LBUTTONDBLCLK        EQU     515
WM_RBUTTONDOWN          EQU     516
WM_RBUTTONUP            EQU     517
WM_RBUTTONDBLCLK        EQU     518
WM_MBUTTONDOWN          EQU     519
WM_MBUTTONUP            EQU     520
WM_MBUTTONDBLCLK        EQU     521
WM_MOUSEWHEEL           EQU     522
WM_XBUTTONDOWN          EQU     523
WM_XBUTTONUP            EQU     524
WM_XBUTTONDBLCLK        EQU     525
WM_MOUSEHWHEEL          EQU     526
WM_MOUSELAST            EQU     526

WM_PARENTNOTIFY         EQU     528
WM_ENTERMENULOOP        EQU     529
WM_EXITMENULOOP         EQU     530
WM_NEXTMENU             EQU     531
WM_SIZING               EQU     532
WM_CAPTURECHANGED       EQU     533
WM_MOVING               EQU     534
WM_POWERBROADCAST       EQU     536
WM_DEVICECHANGE         EQU     537

WM_MDICREATE            EQU     544
WM_MDIDESTROY           EQU     545
WM_MDIACTIVATE          EQU     546
WM_MDIRESTORE           EQU     547
WM_MDINEXT              EQU     548
WM_MDIMAXIMIZE          EQU     549
WM_MDITILE              EQU     550
WM_MDICASCADE           EQU     551
WM_MDIICONARRANGE       EQU     552
WM_MDIGETACTIVE         EQU     553
WM_MDISETMENU           EQU     560
WM_ENTERSIZEMOVE        EQU     561
WM_EXITSIZEMOVE         EQU     562
WM_DROPFILES            EQU     563
WM_MDIREFRESHMENU       EQU     564

WM_POINTERDEVICECHANGE  EQU     568
WM_POINTERDEVICEINRANGE EQU     569
WM_POINTERDEVICEOUTOFRANGE EQU  570
WM_TOUCH                EQU     576
WM_NCPOINTERUPDATE      EQU     577
WM_NCPOINTERDOWN        EQU     578
WM_NCPOINTERUP          EQU     579
WM_POINTERUPDATE        EQU     581
WM_POINTERDOWN          EQU     582
WM_POINTERUP            EQU     583
WM_POINTERENTER         EQU     585
WM_POINTERLEAVE         EQU     586
WM_POINTERACTIVATE      EQU     587
WM_POINTERCAPTURECHANGED EQU    588
WM_TOUCHHITTESTING      EQU     589
WM_POINTERWHEEL         EQU     590
WM_POINTERHWHEEL        EQU     591

WM_IME_SETCONTEXT       EQU     641
WM_IME_NOTIFY           EQU     642
WM_IME_CONTROL          EQU     643
WM_IME_COMPOSITIONFULL  EQU     644
WM_IME_SELECT           EQU     645
WM_IME_CHAR             EQU     646

WM_IME_REQUEST          EQU     648
WM_IME_KEYDOWN          EQU     656
WM_IME_KEYUP            EQU     657

WM_MOUSEHOVER           EQU     673
WM_MOUSELEAVE           EQU     675
WM_NCMOUSEHOVER         EQU     672
WM_NCMOUSELEAVE         EQU     674

WM_WTSSESSION_CHANGE    EQU     689
WM_TABLET_FIRST         EQU     704
WM_TABLET_LAST          EQU     735

WM_DPICHANGED           EQU     736
WM_DPICHANGED_BEFOREPARENT EQU   738
WM_DPICHANGED_AFTERPARENT EQU    739

WM_CUT                  EQU     768
WM_COPY                 EQU     769
WM_PASTE                EQU     770
WM_CLEAR                EQU     771
WM_UNDO                 EQU     772
WM_RENDERFORMAT         EQU     773
WM_RENDERALLFORMATS     EQU     774
WM_DESTROYCLIPBOARD     EQU     775
WM_DRAWCLIPBOARD        EQU     776
WM_PAINTCLIPBOARD       EQU     777
WM_VSCROLLCLIPBOARD     EQU     778
WM_SIZECLIPBOARD        EQU     779
WM_ASKCBFORMATNAME      EQU     780
WM_CHANGECBCHAIN        EQU     781
WM_HSCROLLCLIPBOARD     EQU     782
WM_QUERYNEWPALETTE      EQU     783
WM_PALETTEISCHANGING    EQU     784
WM_PALETTECHANGED       EQU     785
WM_HOTKEY               EQU     786

WM_PRINT                EQU     791
WM_PRINTCLIENT          EQU     792

WM_APPCOMMAND           EQU     793

WM_THEMECHANGED         EQU     794

WM_CLIPBOARDUPDATE      EQU     797

WM_DWMCOMPOSITIONCHANGED EQU    798
WM_DWMNCRENDERINGCHANGED EQU    799
WM_DWMCOLORIZATIONCOLORCHANGED EQU 800
WM_DWMWINDOWMAXIMIZEDCHANGE EQU   801
WM_DWMSENDICONICTHUMBNAIL EQU    803
WM_DWMSENDICONICLIVEPREVIEWBITMAP EQU 806

WM_GETTITLEBARINFOEX    EQU     831

WM_HANDHELDFIRST        EQU     856
WM_HANDHELDLAST         EQU     863

WM_AFXFIRST             EQU     864
WM_AFXLAST              EQU     895

WM_PENWINFIRST          EQU     896
WM_PENWINLAST           EQU     911

WM_USER                 EQU     1024
WM_APP                  EQU     32768

; Dialog codes
DLGC_WANTARROWS         EQU     1
DLGC_WANTTAB            EQU     2
DLGC_WANTALLKEYS        EQU     4
DLGC_WANTMESSAGE        EQU     4
DLGC_HASSETSEL          EQU     8
DLGC_DEFPUSHBUTTON      EQU     16
DLGC_UNDEFPUSHBUTTON    EQU     32
DLGC_RADIOBUTTON        EQU     64
DLGC_WANTCHARS          EQU     128
DLGC_STATIC             EQU     256
DLGC_BUTTON             EQU     8192

; Get/SetWindowWord/Long offsets for dialog boxes
DWL_MSGRESULT           EQU     0
DWL_DLGPROC             EQU     4
DWL_USER                EQU     8

; Clipboard formats
CF_TEXT                 EQU     1
CF_BITMAP               EQU     2
CF_METAFILEPICT         EQU     3
CF_SYLK                 EQU     4
CF_DIF                  EQU     5
CF_TIFF                 EQU     6
CF_OEMTEXT              EQU     7
CF_DIB                  EQU     8
CF_PALETTE              EQU     9
CF_PENDATA              EQU     10
CF_RIFF                 EQU     11
CF_WAVE                 EQU     12
CF_UNICODETEXT          EQU     13
CF_ENHMETAFILE          EQU     14
CF_HDROP                EQU     15
CF_LOCALE               EQU     16
CF_DIBV5                EQU     17
CF_MAX                  EQU     18

; Clipboard manager messages
WM_CLIPBOARDUPDATE      EQU     797

; Standard cursor IDs
IDC_ARROW               EQU     32512
IDC_IBEAM               EQU     32513
IDC_WAIT                EQU     32514
IDC_CROSS               EQU     32515
IDC_UPARROW             EQU     32516
IDC_SIZE                EQU     32640
IDC_ICON                EQU     32641
IDC_SIZENWSE            EQU     32642
IDC_SIZENESW            EQU     32643
IDC_SIZEWE              EQU     32644
IDC_SIZENS              EQU     32645
IDC_SIZEALL             EQU     32646
IDC_NO                  EQU     32648
IDC_HAND                EQU     32649
IDC_APPSTARTING         EQU     32650
IDC_HELP                EQU     32651

; Standard icon IDs
IDI_APPLICATION         EQU     32512
IDI_HAND                EQU     32513
IDI_QUESTION            EQU     32514
IDI_EXCLAMATION         EQU     32515
IDI_ASTERISK            EQU     32516
IDI_WINLOGO             EQU     32517
IDI_WARNING             EQU     32515
IDI_ERROR               EQU     32513
IDI_INFORMATION         EQU     32516

; Dialog box command IDs
IDOK                    EQU     1
IDCANCEL                EQU     2
IDABORT                 EQU     3
IDRETRY                 EQU     4
IDIGNORE                EQU     5
IDYES                   EQU     6
IDNO                    EQU     7
IDCLOSE                 EQU     8
IDHELP                  EQU     9
IDTRYAGAIN              EQU     10
IDCONTINUE              EQU     11

; MessageBox flags
MB_OK                   EQU     0
MB_OKCANCEL             EQU     1
MB_ABORTRETRYIGNORE     EQU     2
MB_YESNOCANCEL          EQU     3
MB_YESNO                EQU     4
MB_RETRYCANCEL          EQU     5
MB_CANCELTRYCONTINUE    EQU     6

MB_ICONHAND             EQU     10h
MB_ICONQUESTION         EQU     20h
MB_ICONEXCLAMATION      EQU     30h
MB_ICONASTERISK         EQU     40h
MB_USERICON             EQU     80h
MB_ICONWARNING          EQU     MB_ICONEXCLAMATION
MB_ICONERROR            EQU     MB_ICONHAND
MB_ICONINFORMATION      EQU     MB_ICONASTERISK
MB_ICONSTOP             EQU     MB_ICONHAND

MB_DEFBUTTON1           EQU     0
MB_DEFBUTTON2           EQU     100h
MB_DEFBUTTON3           EQU     200h
MB_DEFBUTTON4           EQU     300h

MB_APPLMODAL            EQU     0
MB_SYSTEMMODAL          EQU     1000h
MB_TASKMODAL            EQU     2000h

MB_HELP                 EQU     4000h
MB_NOFOCUS              EQU     8000h
MB_SETFOREGROUND        EQU     10000h
MB_DEFAULT_DESKTOP_ONLY EQU     20000h
MB_TOPMOST              EQU     40000h
MB_RIGHT                EQU     80000h
MB_RTLREADING           EQU     100000h

; Dialog box styles
DS_ABSALIGN             EQU     1
DS_SYSMODAL             EQU     2
DS_LOCALEDIT            EQU     20h
DS_SETFONT              EQU     40h
DS_MODALFRAME           EQU     80h
DS_NOIDLEMSG            EQU     100h
DS_SETFOREGROUND        EQU     200h
DS_3DLOOK               EQU     4
DS_FIXEDSYS             EQU     8
DS_NOFAILCREATE         EQU     10h
DS_CONTROL              EQU     400h
DS_CENTER               EQU     800h
DS_CENTERMOUSE          EQU     1000h
DS_CONTEXTHELP          EQU     2000h

DS_SHELLFONT            EQU     DS_SETFONT OR DS_FIXEDSYS

; Dialog messages
DM_GETDEFID             EQU     WM_USER
DM_SETDEFID             EQU     WM_USER+1
DM_REPOSITION           EQU     WM_USER+2

; Listbox messages
LB_ADDSTRING            EQU     384
LB_INSERTSTRING         EQU     385
LB_DELETESTRING         EQU     386
LB_SELITEMRANGEEX       EQU     387
LB_RESETCONTENT         EQU     388
LB_SETSEL               EQU     389
LB_SETCURSEL            EQU     390
LB_GETSEL               EQU     391
LB_GETCURSEL            EQU     392
LB_GETTEXT              EQU     393
LB_GETTEXTLEN           EQU     394
LB_GETCOUNT             EQU     395
LB_SELECTSTRING         EQU     396
LB_DIR                  EQU     397
LB_GETTOPINDEX          EQU     398
LB_FINDSTRING           EQU     399
LB_GETSELCOUNT          EQU     400
LB_GETSELITEMS          EQU     401
LB_SETTABSTOPS          EQU     402
LB_GETHORIZONTALEXTENT  EQU     403
LB_SETHORIZONTALEXTENT  EQU     404
LB_SETCOLUMNWIDTH       EQU     405
LB_ADDFILE              EQU     406
LB_SETTOPINDEX          EQU     407
LB_GETITEMRECT          EQU     408
LB_GETITEMDATA          EQU     409
LB_SETITEMDATA          EQU     410
LB_SELITEMRANGE         EQU     411
LB_SETANCHORINDEX       EQU     412
LB_GETANCHORINDEX       EQU     413
LB_SETCARETINDEX        EQU     414
LB_GETCARETINDEX        EQU     415
LB_SETITEMHEIGHT        EQU     416
LB_GETITEMHEIGHT        EQU     417
LB_FINDSTRINGEXACT      EQU     418
LB_SETLOCALE            EQU     421
LB_GETLOCALE            EQU     422
LB_SETCOUNT             EQU     423
LB_INITSTORAGE          EQU     424
LB_ITEMFROMPOINT        EQU     425
LB_MULTIPLEADDSTRING    EQU     433

; Listbox notification codes
LBN_ERRSPACE            EQU     -2
LBN_SELCHANGE           EQU     1
LBN_DBLCLK              EQU     2
LBN_SELCANCEL           EQU     3
LBN_SETFOCUS            EQU     4
LBN_KILLFOCUS           EQU     5

; Listbox styles
LBS_NOTIFY              EQU     1
LBS_SORT                EQU     2
LBS_NOREDRAW            EQU     4
LBS_MULTIPLESEL         EQU     8
LBS_OWNERDRAWFIXED      EQU     10h
LBS_OWNERDRAWVARIABLE   EQU     20h
LBS_HASSTRINGS          EQU     40h
LBS_USETABSTOPS         EQU     80h
LBS_NOINTEGRALHEIGHT    EQU     100h
LBS_MULTICOLUMN         EQU     200h
LBS_WANTKEYBOARDINPUT   EQU     400h
LBS_EXTENDEDSEL         EQU     800h
LBS_DISABLENOSCROLL     EQU     1000h
LBS_NODATA              EQU     2000h
LBS_NOSEL               EQU     4000h
LBS_COMBOBOX            EQU     8000h
LBS_STANDARD            EQU     LBS_NOTIFY OR LBS_SORT OR WS_VSCROLL OR WS_BORDER

; Combo box messages
CB_GETEDITSEL           EQU     320
CB_LIMITTEXT            EQU     321
CB_SETEDITSEL           EQU     322
CB_ADDSTRING            EQU     323
CB_DELETESTRING         EQU     324
CB_DIR                  EQU     325
CB_GETCOUNT             EQU     326
CB_GETCURSEL            EQU     327
CB_GETLBTEXT            EQU     328
CB_GETLBTEXTLEN         EQU     329
CB_INSERTSTRING         EQU     330
CB_RESETCONTENT         EQU     331
CB_FINDSTRING           EQU     332
CB_SELECTSTRING         EQU     333
CB_SETCURSEL            EQU     334
CB_SHOWDROPDOWN         EQU     335
CB_GETITEMDATA          EQU     336
CB_SETITEMDATA          EQU     337
CB_GETDROPPEDCONTROLRECT EQU    338
CB_SETITEMHEIGHT        EQU     339
CB_GETITEMHEIGHT        EQU     340
CB_SETEXTENDEDUI        EQU     341
CB_GETEXTENDEDUI        EQU     342
CB_GETDROPPEDSTATE      EQU     343
CB_FINDSTRINGEXACT      EQU     344
CB_SETLOCALE            EQU     345
CB_GETLOCALE            EQU     346
CB_GETTOPINDEX          EQU     347
CB_SETTOPINDEX          EQU     348
CB_GETHORIZONTALEXTENT  EQU     349
CB_SETHORIZONTALEXTENT  EQU     350
CB_GETDROPPEDWIDTH      EQU     351
CB_SETDROPPEDWIDTH      EQU     352
CB_INITSTORAGE          EQU     353
CB_MULTIPLEADDSTRING    EQU     355
CB_GETCOMBOBOXINFO      EQU     356

; Combo box notification codes
CBN_ERRSPACE            EQU     -1
CBN_SELCHANGE           EQU     1
CBN_DBLCLK              EQU     2
CBN_SETFOCUS            EQU     3
CBN_KILLFOCUS           EQU     4
CBN_EDITCHANGE          EQU     5
CBN_EDITUPDATE          EQU     6
CBN_DROPDOWN            EQU     7
CBN_CLOSEUP             EQU     8
CBN_SELENDOK            EQU     9
CBN_SELENDCANCEL        EQU     10

; Combo box styles
CBS_SIMPLE              EQU     1
CBS_DROPDOWN            EQU     2
CBS_DROPDOWNLIST        EQU     3
CBS_OWNERDRAWFIXED      EQU     10h
CBS_OWNERDRAWVARIABLE   EQU     20h
CBS_AUTOHSCROLL         EQU     40h
CBS_OEMCONVERT          EQU     80h
CBS_SORT                EQU     100h
CBS_HASSTRINGS          EQU     200h
CBS_NOINTEGRALHEIGHT    EQU     400h
CBS_DISABLENOSCROLL     EQU     800h
CBS_UPPERCASE           EQU     2000h
CBS_LOWERCASE           EQU     4000h

; Scroll bar messages
SBM_SETPOS              EQU     224
SBM_GETPOS              EQU     225
SBM_SETRANGE            EQU     226
SBM_SETRANGEREDRAW      EQU     230
SBM_GETRANGE            EQU     227
SBM_ENABLE_ARROWS       EQU     228
SBM_SETSCROLLINFO       EQU     233
SBM_GETSCROLLINFO       EQU     234
SBM_GETSCROLLBARINFO    EQU     235

; Scroll bar styles
SBS_HORZ                EQU     0
SBS_VERT                EQU     1
SBS_TOPALIGN            EQU     2
SBS_LEFTALIGN           EQU     2
SBS_BOTTOMALIGN         EQU     4
SBS_RIGHTALIGN          EQU     4
SBS_SIZEBOXTOPLEFTALIGN EQU     2
SBS_SIZEBOXBOTTOMRIGHTALIGN EQU 4
SBS_SIZEBOX             EQU     8
SBS_SIZEGRIP            EQU     10h

; Scroll bar notification codes
SB_LINEUP               EQU     0
SB_LINELEFT             EQU     0
SB_LINEDOWN             EQU     1
SB_LINERIGHT            EQU     1
SB_PAGEUP               EQU     2
SB_PAGELEFT             EQU     2
SB_PAGEDOWN             EQU     3
SB_PAGERIGHT            EQU     3
SB_THUMBPOSITION        EQU     4
SB_THUMBTRACK           EQU     5
SB_TOP                  EQU     6
SB_LEFT                 EQU     6
SB_BOTTOM               EQU     7
SB_RIGHT                EQU     7
SB_ENDSCROLL            EQU     8

; Edit control messages
EM_GETSEL               EQU     176
EM_SETSEL               EQU     183
EM_GETRECT              EQU     178
EM_SETRECT              EQU     179
EM_SETRECTNP            EQU     180
EM_SCROLL               EQU     182
EM_LINESCROLL           EQU     182
EM_SCROLLCARET          EQU     183
EM_GETMODIFY            EQU     184
EM_SETMODIFY            EQU     185
EM_GETLINECOUNT         EQU     186
EM_LINEINDEX            EQU     187
EM_SETHANDLE            EQU     188
EM_GETHANDLE            EQU     189
EM_GETTHUMB             EQU     190
EM_LINELENGTH           EQU     193
EM_REPLACESEL           EQU     194
EM_GETLINE              EQU     196
EM_LIMITTEXT            EQU     197
EM_CANUNDO              EQU     198
EM_UNDO                 EQU     199
EM_FMTLINES             EQU     200
EM_LINEFROMCHAR         EQU     201
EM_SETTABSTOPS          EQU     203
EM_SETPASSWORDCHAR      EQU     204
EM_EMPTYUNDOBUFFER      EQU     205
EM_GETFIRSTVISIBLELINE  EQU     206
EM_SETREADONLY          EQU     207
EM_SETWORDBREAKPROC     EQU     208
EM_GETWORDBREAKPROC     EQU     209
EM_GETPASSWORDCHAR      EQU     210
EM_SETMARGINS           EQU     211
EM_GETMARGINS           EQU     212
EM_SETLIMITTEXT         EQU     197
EM_GETLIMITTEXT         EQU     213
EM_POSFROMCHAR          EQU     214
EM_CHARFROMPOS          EQU     215
EM_SETIMESTATUS         EQU     216
EM_GETIMESTATUS         EQU     217

; Edit control notification codes
EN_SETFOCUS             EQU     256
EN_KILLFOCUS            EQU     512
EN_CHANGE               EQU     768
EN_UPDATE               EQU     1024
EN_ERRSPACE             EQU     1280
EN_MAXTEXT              EQU     1281
EN_HSCROLL              EQU     1537
EN_VSCROLL              EQU     1538

; Edit control styles
ES_LEFT                 EQU     0
ES_CENTER               EQU     1
ES_RIGHT                EQU     2
ES_MULTILINE            EQU     4
ES_UPPERCASE            EQU     8
ES_LOWERCASE            EQU     10h
ES_PASSWORD             EQU     20h
ES_AUTOVSCROLL          EQU     40h
ES_AUTOHSCROLL          EQU     80h
ES_NOHIDESEL            EQU     100h
ES_OEMCONVERT           EQU     400h
ES_READONLY             EQU     800h
ES_WANTRETURN           EQU     1000h
ES_NUMBER               EQU     2000h

; Button messages
BM_GETCHECK             EQU     240
BM_SETCHECK             EQU     241
BM_GETSTATE             EQU     242
BM_SETSTATE             EQU     243
BM_SETSTYLE             EQU     244
BM_CLICK                EQU     245
BM_GETIMAGE             EQU     246
BM_SETIMAGE             EQU     247
BM_SETDONTCLICK         EQU     248

; Button notification codes
BN_CLICKED              EQU     0
BN_PAINT                EQU     1
BN_HILITE               EQU     2
BN_UNHILITE             EQU     3
BN_DISABLE              EQU     4
BN_DOUBLECLICKED        EQU     5
BN_PUSHED               EQU     BN_HILITE
BN_UNPUSHED             EQU     BN_UNHILITE
BN_DBLCLK               EQU     BN_DOUBLECLICKED
BN_SETFOCUS             EQU     6
BN_KILLFOCUS            EQU     7

; Button styles
BS_PUSHBUTTON           EQU     0
BS_DEFPUSHBUTTON        EQU     1
BS_CHECKBOX             EQU     2
BS_AUTOCHECKBOX         EQU     3
BS_RADIOBUTTON          EQU     4
BS_3STATE               EQU     5
BS_AUTO3STATE           EQU     6
BS_GROUPBOX             EQU     7
BS_USERBUTTON           EQU     8
BS_AUTORADIOBUTTON      EQU     9
BS_PUSHBOX              EQU     10
BS_OWNERDRAW            EQU     11
BS_TYPEMASK             EQU     15
BS_LEFTTEXT             EQU     20h
BS_TEXT                 EQU     0
BS_ICON                 EQU     40h
BS_BITMAP               EQU     80h
BS_LEFT                 EQU     100h
BS_RIGHT                EQU     200h
BS_CENTER               EQU     300h
BS_TOP                  EQU     400h
BS_BOTTOM               EQU     800h
BS_VCENTER              EQU     C00h
BS_PUSHLIKE             EQU     1000h
BS_MULTILINE            EQU     2000h
BS_NOTIFY               EQU     4000h
BS_FLAT                 EQU     8000h
BS_RIGHTBUTTON          EQU     20h

; Static control messages
STM_SETICON             EQU     368
STM_GETICON             EQU     369
STM_SETIMAGE            EQU     370
STM_GETIMAGE            EQU     371
STM_MSGMAX              EQU     372

; Static control notification codes
STN_CLICKED             EQU     0
STN_DBLCLK              EQU     1
STN_ENABLE              EQU     2
STN_DISABLE             EQU     3
STN_ERASEBKGND          EQU     4

; Static control styles
SS_LEFT                 EQU     0
SS_CENTER               EQU     1
SS_RIGHT                EQU     2
SS_ICON                 EQU     3
SS_BLACKRECT            EQU     4
SS_GRAYRECT             EQU     5
SS_WHITERECT            EQU     6
SS_BLACKFRAME           EQU     7
SS_GRAYFRAME            EQU     8
SS_WHITEFRAME           EQU     9
SS_USERITEM             EQU     10
SS_SIMPLE               EQU     11
SS_LEFTNOWORDWRAP       EQU     12
SS_OWNERDRAW            EQU     13
SS_BITMAP               EQU     14
SS_ENHMETAFILE          EQU     15
SS_ETCHEDHORZ           EQU     16
SS_ETCHEDVERT           EQU     17
SS_ETCHEDFRAME          EQU     18
SS_TYPEMASK             EQU     31
SS_REALSIZECONTROL      EQU     40
SS_NOPREFIX             EQU     80h
SS_NOTIFY               EQU     100h
SS_CENTERIMAGE          EQU     200h
SS_RIGHTJUST            EQU     400h
SS_REALSIZEIMAGE        EQU     800h
SS_SUNKEN               EQU     1000h
SS_EDITCONTROL          EQU     2000h
SS_ENDELLIPSIS          EQU     4000h
SS_PATHELLIPSIS         EQU     8000h
SS_WORDELLIPSIS         EQU     C000h
SS_ELLIPSISMASK         EQU     C000h

; Progress bar messages
PBM_SETRANGE            EQU     1025
PBM_SETPOS              EQU     1026
PBM_DELTAPOS            EQU     1027
PBM_SETSTEP             EQU     1028
PBM_STEPIT              EQU     1029
PBM_SETRANGE32          EQU     1030
PBM_GETRANGE            EQU     1031
PBM_GETPOS              EQU     1032
PBM_SETBARCOLOR         EQU     1033
PBM_SETMARQUEE          EQU     1034
PBM_GETSTEP             EQU     1037
PBM_GETBKCOLOR          EQU     1038
PBM_SETBKCOLOR          EQU     1039
PBM_SETSTATE            EQU     1040
PBM_GETSTATE            EQU     1041

; Progress bar states
PBST_NORMAL             EQU     1
PBST_ERROR              EQU     2
PBST_PAUSED             EQU     3

; Progress bar styles
PBS_SMOOTH              EQU     1
PBS_VERTICAL            EQU     4
PBS_MARQUEE             EQU     8
PBS_SMOOTHREVERSE       EQU     10h
PBS_SMOOTHREVERSE       EQU     10h

; Trackbar messages
TBM_GETPOS              EQU     1024
TBM_GETRANGEMIN         EQU     1025
TBM_GETRANGEMAX         EQU     1026
TBM_GETTIC              EQU     1027
TBM_SETTIC              EQU     1028
TBM_SETPOS              EQU     1029
TBM_SETRANGE            EQU     1030
TBM_SETRANGEMIN         EQU     1031
TBM_SETRANGEMAX         EQU     1032
TBM_CLEARTICS           EQU     1033
TBM_SETSEL              EQU     1034
TBM_SETSELSTART         EQU     1035
TBM_SETSELEND           EQU     1036
TBM_GETPTICS            EQU     1038
TBM_GETTICPOS           EQU     1039
TBM_GETNUMTICS          EQU     1040
TBM_GETSELSTART         EQU     1041
TBM_GETSELEND           EQU     1042
TBM_CLEARSEL            EQU     1043
TBM_SETTICFREQ          EQU     1044
TBM_SETPAGESIZE         EQU     1045
TBM_GETPAGESIZE         EQU     1046
TBM_SETLINESIZE         EQU     1047
TBM_GETLINESIZE         EQU     1048
TBM_GETTHUMBRECT        EQU     1049
TBM_GETCHANNELRECT      EQU     1050
TBM_SETTHUMBLENGTH      EQU     1051
TBM_GETTHUMBLENGTH      EQU     1052
TBM_SETTOOLTIPS         EQU     1053
TBM_GETTOOLTIPS         EQU     1054
TBM_SETTIPSIDE          EQU     1055
TBM_SETBUDDY            EQU     1056
TBM_GETBUDDY            EQU     1057
TBM_SETUNICODEFORMAT    EQU     8197
TBM_GETUNICODEFORMAT    EQU     8198

; Trackbar styles
TBS_AUTOTICKS           EQU     1
TBS_VERT                  EQU     2
TBS_HORZ                  EQU     0
TBS_TOP                   EQU     4
TBS_BOTTOM                EQU     0
TBS_LEFT                  EQU     4
TBS_RIGHT                 EQU     0
TBS_BOTH                  EQU     8
TBS_NOTICKS               EQU     10h
TBS_ENABLESELRANGE        EQU     20h
TBS_FIXEDLENGTH           EQU     40h
TBS_NOTHUMB               EQU     80h
TBS_TOOLTIPS              EQU     100h
TBS_REVERSED              EQU     200h
TBS_DOWNISLEFT            EQU     400h

; Trackbar notification codes
TB_LINEUP               EQU     0
TB_LINEDOWN             EQU     1
TB_PAGEUP               EQU     2
TB_PAGEDOWN             EQU     3
TB_THUMBPOSITION        EQU     4
TB_THUMBTRACK           EQU     5
TB_TOP                  EQU     6
TB_BOTTOM               EQU     7
TB_ENDTRACK             EQU     8

; TreeView messages
TVM_INSERTITEM          EQU     4352
TVM_DELETEITEM          EQU     4353
TVM_EXPAND              EQU     4354
TVM_GETITEMRECT         EQU     4356
TVM_GETCOUNT            EQU     4357
TVM_GETINDENT           EQU     4358
TVM_SETINDENT           EQU     4359
TVM_GETIMAGELIST        EQU     4360
TVM_SETIMAGELIST        EQU     4361
TVM_GETNEXTITEM         EQU     4362
TVM_SELECTITEM          EQU     4363
TVM_GETITEM             EQU     4364
TVM_SETITEM             EQU     4365
TVM_EDITLABEL           EQU     4366
TVM_GETEDITCONTROL      EQU     4367
TVM_GETVISIBLECOUNT     EQU     4368
TVM_HITTEST             EQU     4369
TVM_CREATEDRAGIMAGE     EQU     4370
TVM_SORTCHILDREN        EQU     4371
TVM_ENSUREVISIBLE       EQU     4372
TVM_SORTCHILDRENCB      EQU     4373
TVM_ENDEDITLABELNOW     EQU     4374
TVM_GETISEARCHSTRING    EQU     4375
TVM_SETTOOLTIPS         EQU     4376
TVM_GETTOOLTIPS         EQU     4377
TVM_SETINSERTMARK       EQU     4378
TVM_SETUNICODEFORMAT    EQU     8197
TVM_GETUNICODEFORMAT    EQU     8198
TVM_SETITEMHEIGHT       EQU     4379
TVM_GETITEMHEIGHT       EQU     4380
TVM_SETBKCOLOR          EQU     4381
TVM_SETTEXTCOLOR        EQU     4382
TVM_GETBKCOLOR          EQU     4383
TVM_GETTEXTCOLOR        EQU     4384
TVM_SETSCROLLTIME       EQU     4385
TVM_GETSCROLLTIME       EQU     4386
TVM_SETBORDER           EQU     4387
TVM_SETINSERTMARKCOLOR  EQU     4388
TVM_GETINSERTMARKCOLOR  EQU     4389
TVM_GETITEMSTATE        EQU     4391
TVM_SETLINECOLOR        EQU     4392
TVM_GETLINECOLOR        EQU     4393
TVM_MAPACCIDTOHTREEITEM EQU     4394
TVM_MAPHTREEITEMTOACCID EQU     4395

; TreeView styles
TVS_HASBUTTONS          EQU     1
TVS_HASLINES            EQU     2
TVS_LINESATROOT         EQU     4
TVS_EDITLABELS          EQU     8
TVS_DISABLEDRAGDROP     EQU     10h
TVS_SHOWSELALWAYS       EQU     20h
TVS_RTLREADING          EQU     40h
TVS_NOTOOLTIPS          EQU     80h
TVS_CHECKBOXES          EQU     100h
TVS_TRACKSELECT         EQU     200h
TVS_SINGLEEXPAND        EQU     400h
TVS_INFOTIP             EQU     800h
TVS_FULLROWSELECT       EQU     1000h
TVS_NOSCROLL            EQU     2000h
TVS_NONEVENHEIGHT       EQU     4000h
TVS_NOHSCROLL           EQU     8000h

TVS_EX_MULTISELECT      EQU     2
TVS_EX_DOUBLEBUFFER     EQU     4
TVS_EX_NOINDENTSTATE    EQU     8
TVS_EX_RICHTOOLTIP      EQU     10h
TVS_EX_AUTOHSCROLL      EQU     20h
TVS_EX_FADEINOUTEXPANDOS EQU    40h
TVS_EX_PARTIALCHECKBOXES EQU    80h
TVS_EX_EXCLUSIONCHECKBOXES EQU   100h
TVS_EX_DIMMEDCHECKBOXES EQU     200h
TVS_EX_DRAWIMAGEASYNC   EQU     400h

; ListView messages
LVM_FIRST               EQU     4096
LVM_GETBKCOLOR          EQU     4096
LVM_SETBKCOLOR          EQU     4097
LVM_GETIMAGELIST        EQU     4098
LVM_SETIMAGELIST        EQU     4099
LVM_GETITEMCOUNT        EQU     4100
LVM_GETITEM             EQU     4101
LVM_SETITEM             EQU     4102
LVM_INSERTITEM          EQU     4103
LVM_DELETEITEM          EQU     4104
LVM_DELETEALLITEMS      EQU     4105
LVM_GETCALLBACKMASK     EQU     4106
LVM_SETCALLBACKMASK     EQU     4107
LVM_GETNEXTITEM         EQU     4108
LVM_FINDITEM            EQU     4109
LVM_GETITEMRECT         EQU     4110
LVM_SETITEMPOSITION     EQU     4111
LVM_GETITEMPOSITION     EQU     4112
LVM_GETSTRINGWIDTH      EQU     4113
LVM_HITTEST             EQU     4114
LVM_ENSUREVISIBLE       EQU     4115
LVM_SCROLL              EQU     4116
LVM_REDRAWITEMS         EQU     4117
LVM_ARRANGE             EQU     4118
LVM_EDITLABEL           EQU     4119
LVM_GETEDITCONTROL      EQU     4120
LVM_GETCOLUMN           EQU     4121
LVM_SETCOLUMN           EQU     4122
LVM_INSERTCOLUMN        EQU     4123
LVM_DELETECOLUMN        EQU     4124
LVM_GETCOLUMNWIDTH      EQU     4125
LVM_SETCOLUMNWIDTH      EQU     4126
LVM_GETHEADER           EQU     4127
LVM_CREATEDRAGIMAGE     EQU     4129
LVM_GETVIEWRECT         EQU     4130
LVM_GETTEXTCOLOR        EQU     4131
LVM_SETTEXTCOLOR        EQU     4132
LVM_GETTEXTBKCOLOR      EQU     4133
LVM_SETTEXTBKCOLOR      EQU     4134
LVM_GETTOPINDEX         EQU     4135
LVM_GETCOUNTPERPAGE     EQU     4136
LVM_GETORIGIN           EQU     4137
LVM_UPDATE              EQU     4138
LVM_SETITEMSTATE        EQU     4139
LVM_GETITEMSTATE        EQU     4140
LVM_GETITEMTEXT         EQU     4141
LVM_SETITEMTEXT         EQU     4142
LVM_SETITEMCOUNT        EQU     4143
LVM_SORTITEMS           EQU     4144
LVM_SETITEMPOSITION32   EQU     4145
LVM_GETSELECTEDCOUNT    EQU     4146
LVM_GETITEMSPACING      EQU     4147
LVM_GETISEARCHSTRING    EQU     4148
LVM_SETICONSPACING      EQU     4149
LVM_SETEXTENDEDLISTVIEWSTYLE EQU 4150
LVM_GETEXTENDEDLISTVIEWSTYLE EQU 4151
LVM_GETSUBITEMRECT      EQU     4152
LVM_SUBITEMHITTEST      EQU     4153
LVM_SETCOLUMNORDERARRAY EQU     4154
LVM_GETCOLUMNORDERARRAY EQU     4155
LVM_SETHOTITEM          EQU     4156
LVM_GETHOTITEM          EQU     4157
LVM_SETHOTCURSOR        EQU     4158
LVM_GETHOTCURSOR        EQU     4159
LVM_APPROXIMATEVIEWRECT EQU     4160
LVM_SETWORKAREAS        EQU     4161
LVM_GETWORKAREAS        EQU     4162
LVM_GETNUMBEROFWORKAREAS EQU    4163
LVM_GETSELECTIONMARK    EQU     4164
LVM_SETSELECTIONMARK    EQU     4165
LVM_SETHOVERTIME        EQU     4166
LVM_GETHOVERTIME        EQU     4167
LVM_SETTOOLTIPS         EQU     4168
LVM_GETTOOLTIPS         EQU     4169
LVM_SORTITEMSEX         EQU     4170

LVM_SETBKIMAGE          EQU     4165
LVM_GETBKIMAGE          EQU     4166
LVM_SETSELECTEDCOLUMN   EQU     4172
LVM_SETTILEWIDTH        EQU     4173
LVM_SETVIEW             EQU     4238
LVM_GETVIEW             EQU     4239
LVM_INSERTGROUP         EQU     4241
LVM_SETGROUPINFO        EQU     4243
LVM_GETGROUPINFO        EQU     4245
LVM_REMOVEGROUP         EQU     4246
LVM_MOVEGROUP           EQU     4247
LVM_MOVEITEMTOGROUP     EQU     4250
LVM_SETGROUPMETRICS     EQU     4251
LVM_GETGROUPMETRICS     EQU     4252
LVM_ENABLEGROUPVIEW     EQU     4253
LVM_SORTGROUPS          EQU     4254
LVM_INSERTGROUPSORTED   EQU     4255
LVM_REMOVEALLGROUPS     EQU     4256
LVM_HASGROUP            EQU     4257
LVM_SETTILEVIEWINFO     EQU     4258
LVM_GETTILEVIEWINFO     EQU     4259
LVM_SETTILEINFO         EQU     4260
LVM_GETTILEINFO         EQU     4261
LVM_SETINSERTMARK       EQU     4262
LVM_GETINSERTMARK       EQU     4263
LVM_INSERTMARKHITTEST   EQU     4264
LVM_GETINSERTMARKRECT   EQU     4265
LVM_SETINSERTMARKCOLOR  EQU     4266
LVM_GETINSERTMARKCOLOR  EQU     4267
LVM_SETINFOTIP          EQU     4269
LVM_GETSELECTEDCOLUMN   EQU     4270
LVM_ISGROUPVIEWENABLED  EQU     4271
LVM_GETOUTLINECOLOR     EQU     4272
LVM_SETOUTLINECOLOR     EQU     4273
LVM_CANCELEDITLABEL     EQU     4275
LVM_MAPINDEXTOID        EQU     4276
LVM_MAPIDTOINDEX        EQU     4277
LVM_ISITEMVISIBLE       EQU     4278
LVM_GETEMPTYTEXT        EQU     4300
LVM_GETFOOTERRECT       EQU     4301
LVM_GETFOOTERINFO       EQU     4302
LVM_GETFOOTERITEMRECT   EQU     4303
LVM_GETFOOTERITEM       EQU     4304
LVM_GETITEMINDEXRECT    EQU     4305
LVM_SETITEMINDEXSTATE   EQU     4306
LVM_GETNEXTITEMINDEX     EQU     4307

; ListView styles
LVS_ICON                EQU     0
LVS_REPORT              EQU     1
LVS_SMALLICON           EQU     2
LVS_LIST                EQU     3
LVS_TYPEMASK            EQU     3
LVS_SINGLESEL           EQU     4
LVS_SHOWSELALWAYS       EQU     8
LVS_SORTASCENDING       EQU     10h
LVS_SORTDESCENDING      EQU     20h
LVS_SHAREIMAGELISTS     EQU     40h
LVS_NOLABELWRAP         EQU     80h
LVS_AUTOARRANGE         EQU     100h
LVS_EDITLABELS          EQU     200h
LVS_OWNERDATA           EQU     1000h
LVS_NOSCROLL            EQU     2000h
LVS_TYPESTYLEMASK       EQU     FC00h
LVS_ALIGNTOP            EQU     0
LVS_ALIGNLEFT           EQU     800h
LVS_ALIGNMASK           EQU     C00h
LVS_OWNERDRAWFIXED      EQU     400h
LVS_NOCOLUMNHEADER      EQU     4000h
LVS_NOSORTHEADER        EQU     8000h

LVS_EX_GRIDLINES        EQU     1
LVS_EX_SUBITEMIMAGES    EQU     2
LVS_EX_CHECKBOXES       EQU     4
LVS_EX_TRACKSELECT      EQU     8
LVS_EX_HEADERDRAGDROP   EQU     10h
LVS_EX_FULLROWSELECT    EQU     20h
LVS_EX_ONECLICKACTIVATE EQU     40h
LVS_EX_TWOCLICKACTIVATE EQU     80h
LVS_EX_FLATSB           EQU     100h
LVS_EX_REGIONAL         EQU     200h
LVS_EX_INFOTIP          EQU     400h
LVS_EX_UNDERLINEHOT     EQU     800h
LVS_EX_UNDERLINECOLD    EQU     1000h
LVS_EX_MULTIWORKAREAS   EQU     2000h
LVS_EX_LABELTIP         EQU     4000h
LVS_EX_BORDERSELECT     EQU     8000h
LVS_EX_DOUBLEBUFFER     EQU     10000h
LVS_EX_HIDELABELS       EQU     20000h
LVS_EX_SINGLEROW        EQU     40000h
LVS_EX_SNAPTOGRID       EQU     80000h
LVS_EX_SIMPLESELECT     EQU     100000h
LVS_EX_JUSTIFYCOLUMNS   EQU     200000h
LVS_EX_TRANSPARENTBKGND EQU     400000h
LVS_EX_TRANSPARENTSHADOWTEXT EQU 800000h
LVS_EX_AUTOAUTOARRANGE  EQU     1000000h
LVS_EX_HEADERINALLVIEWS EQU     2000000h
LVS_EX_AUTOCHECKSELECT  EQU     4000000h
LVS_EX_AUTOSIZECOLUMNS  EQU     8000000h
LVS_EX_COLUMNSNAPPOINTS EQU     10000000h
LVS_EX_COLUMNOVERFLOW   EQU     20000000h

; ListView group messages
LVM_ENABLEGROUPVIEW     EQU     4253
LVM_SORTGROUPS          EQU     4254
LVM_INSERTGROUPSORTED   EQU     4255
LVM_REMOVEALLGROUPS     EQU     4256
LVM_HASGROUP            EQU     4257

; ListView group styles
LVGS_NORMAL             EQU     0
LVGS_COLLAPSED          EQU     1
LVGS_HIDDEN             EQU     2
LVGS_NOHEADER           EQU     4
LVGS_COLLAPSIBLE        EQU     8
LVGS_FOCUSED            EQU     10h
LVGS_SELECTED           EQU     20h
LVGS_SUBSETED           EQU     40h
LVGS_SUBSETLINKFOCUSED  EQU     80h

LVGS_SUBSET             EQU     100h
LVGS_SUBSETLINK         EQU     200h

; ListView group alignment
LVGA_HEADER_LEFT        EQU     1
LVGA_HEADER_CENTER      EQU     2
LVGA_HEADER_RIGHT       EQU     4
LVGA_FOOTER_LEFT        EQU     8
LVGA_FOOTER_CENTER      EQU     10h
LVGA_FOOTER_RIGHT       EQU     20h

; ListView tile view messages
LVM_SETTILEVIEWINFO     EQU     4258
LVM_GETTILEVIEWINFO     EQU     4259
LVM_SETTILEINFO         EQU     4260
LVM_GETTILEINFO         EQU     4261

; ListView tile view flags
LVTVIF_AUTOSIZE         EQU     0
LVTVIF_FIXEDWIDTH       EQU     1
LVTVIF_FIXEDHEIGHT      EQU     2
LVTVIF_FIXEDSIZE        EQU     3

LVTVIM_TILESIZE         EQU     1
LVTVIM_COLUMNS          EQU     2
LVTVIM_LABELMARGIN      EQU     4

; ListView insert mark messages
LVM_SETINSERTMARK       EQU     4262
LVM_GETINSERTMARK       EQU     4263
LVM_INSERTMARKHITTEST   EQU     4264
LVM_GETINSERTMARKRECT   EQU     4265
LVM_SETINSERTMARKCOLOR  EQU     4266
LVM_GETINSERTMARKCOLOR  EQU     4267

; ListView footer messages
LVM_SETFOOTERINFO       EQU     4307
LVM_GETFOOTERINFO       EQU     4308
LVM_GETFOOTERITEM       EQU     4309
LVM_SETFOOTERITEM       EQU     4310

; ListView footer flags
LVFF_ITEMCOUNT          EQU     1

; ListView footer item flags
LVFIF_TEXT              EQU     1
LVFIF_STATE             EQU     2

LVFIS_FOCUSED           EQU     1
LVFIS_SELECTED          EQU     2

; ListView item index rect messages
LVM_GETITEMINDEXRECT    EQU     4305
LVM_SETITEMINDEXSTATE   EQU     4306
LVM_GETNEXTITEMINDEX     EQU     4307

; ListView item index rect flags
LVIR_BOUNDS             EQU     0
LVIR_ICON               EQU     1
LVIR_LABEL              EQU     2
LVIR_SELECTBOUNDS       EQU     3

; ListView background image messages
LVM_SETBKIMAGE          EQU     4165
LVM_GETBKIMAGE          EQU     4166

; ListView background image flags
LVBKIF_SOURCE_NONE      EQU     0
LVBKIF_SOURCE_HBITMAP   EQU     1
LVBKIF_SOURCE_URL       EQU     2
LVBKIF_SOURCE_MASK      EQU     3
LVBKIF_STYLE_NORMAL     EQU     0
LVBKIF_STYLE_TILE       EQU     10h
LVBKIF_STYLE_MASK       EQU     10h
LVBKIF_FLAG_TILEOFFSET  EQU     100h
LVBKIF_TYPE_WATERMARK   EQU     10000000h
LVBKIF_FLAG_ALPHABLEND  EQU     20000000h

; ListView extended tile view messages
LVM_SETVIEW             EQU     4238
LVM_GETVIEW             EQU     4239

; ListView view flags
LV_VIEW_ICON            EQU     0
LV_VIEW_DETAILS         EQU     1
LV_VIEW_SMALLICON       EQU     2
LV_VIEW_LIST            EQU     3
LV_VIEW_TILE            EQU     4
LV_VIEW_MAX             EQU     4

; ListView grouping messages
LVM_INSERTGROUP         EQU     4241
LVM_SETGROUPINFO        EQU     4243
LVM_GETGROUPINFO        EQU     4245
LVM_REMOVEGROUP         EQU     4246
LVM_MOVEGROUP           EQU     4247
LVM_MOVEITEMTOGROUP     EQU     4250

; ListView group flags
LVGF_NONE               EQU     0
LVGF_HEADER             EQU     1
LVGF_FOOTER             EQU     2
LVGF_STATE              EQU     4
LVGF_ALIGN              EQU     8
LVGF_GROUPID            EQU     10h
LVGF_SUBTITLE           EQU     100h
LVGF_TASK               EQU     200h
LVGF_DESCRIPTIONTOP     EQU     400h
LVGF_DESCRIPTIONBOTTOM  EQU     800h
LVGF_TITLEIMAGE         EQU     1000h
LVGF_EXTENDEDIMAGE      EQU     2000h
LVGF_ITEMS              EQU     4000h
LVGF_SUBSET             EQU     8000h
LVGF_SUBSETITEMS        EQU     10000h

; ListView group states
LVGS_NORMAL             EQU     0
LVGS_COLLAPSED          EQU     1
LVGS_HIDDEN             EQU     2
LVGS_NOHEADER           EQU     4
LVGS_COLLAPSIBLE        EQU     8
LVGS_FOCUSED            EQU     10h
LVGS_SELECTED           EQU     20h
LVGS_SUBSETED           EQU     40h
LVGS_SUBSETLINKFOCUSED  EQU     80h

; ListView group alignment
LVGA_HEADER_LEFT        EQU     1
LVGA_HEADER_CENTER      EQU     2
LVGA_HEADER_RIGHT       EQU     4
LVGA_FOOTER_LEFT        EQU     8
LVGA_FOOTER_CENTER      EQU     10h
LVGA_FOOTER_RIGHT       EQU     20h

; ListView tile info messages
LVM_SETTILEVIEWINFO     EQU     4258
LVM_GETTILEVIEWINFO     EQU     4259
LVM_SETTILEINFO         EQU     4260
LVM_GETTILEINFO         EQU     4261

; ListView tile info flags
TILEVIEWINFO_MASK       EQU     1
TILEVIEWINFO_MASK       EQU     2
TILEVIEWINFO_MASK       EQU     4

TILEVIEWINFO_MASK       EQU     1
TILEVIEWINFO_MASK       EQU     2
TILEVIEWINFO_MASK       EQU     4

; ListView insert mark messages
LVM_SETINSERTMARK       EQU     4262
LVM_GETINSERTMARK       EQU     4263
LVM_INSERTMARKHITTEST   EQU     4264
LVM_GETINSERTMARKRECT   EQU     4265
LVM_SETINSERTMARKCOLOR  EQU     4266
LVM_GETINSERTMARKCOLOR  EQU     4267

; ListView insert mark flags
LVIM_AFTER              EQU     1

; ListView color messages
LVM_SETBKCOLOR          EQU     4097
LVM_GETBKCOLOR          EQU     4096
LVM_SETTEXTCOLOR        EQU     4132
LVM_GETTEXTCOLOR        EQU     4131
LVM_SETTEXTBKCOLOR      EQU     4134
LVM_GETTEXTBKCOLOR      EQU     4133

; ListView Unicode format messages
LVM_SETUNICODEFORMAT    EQU     8197
LVM_GETUNICODEFORMAT    EQU     8198

; ListView item spacing messages
LVM_GETITEMSPACING      EQU     4147
LVM_SETICONSPACING      EQU     4149

; ListView work area messages
LVM_SETWORKAREAS        EQU     4161
LVM_GETWORKAREAS        EQU     4162
LVM_GETNUMBEROFWORKAREAS EQU    4163

; ListView tool tip messages
LVM_SETTOOLTIPS         EQU     4168
LVM_GETTOOLTIPS         EQU     4169
LVM_SETTOOLTIPS         EQU     4168
LVM_GETTOOLTIPS         EQU     4169

; ListView hot item messages
LVM_SETHOTITEM          EQU     4156
LVM_GETHOTITEM          EQU     4157
LVM_SETHOTCURSOR        EQU     4158
LVM_GETHOTCURSOR        EQU     4159

; ListView selection mark messages
LVM_GETSELECTIONMARK    EQU     4164
LVM_SETSELECTIONMARK    EQU     4165

; ListView check box messages
LVM_SETITEMSTATE        EQU     4139
LVM_GETITEMSTATE        EQU     4140

; ListView column order array messages
LVM_SETCOLUMNORDERARRAY EQU     4154
LVM_GETCOLUMNORDERARRAY EQU     4155

; ListView header drag drop messages
LVM_SETEXTENDEDLISTVIEWSTYLE EQU 4150
LVM_GETEXTENDEDLISTVIEWSTYLE EQU 4151

; ListView sub item image messages
LVM_SETSUBITEM          EQU     4152
LVM_GETSUBITEMRECT      EQU     4152
LVM_SUBITEMHITTEST      EQU     4153

; ListView approximate view rect messages
LVM_APPROXIMATEVIEWRECT EQU     4160

; ListView empty text messages
LVM_GETEMPTYTEXT        EQU     4300

; ListView footer messages
LVM_GETFOOTERRECT       EQU     4301
LVM_GETFOOTERINFO       EQU     4302
LVM_GETFOOTERITEMRECT   EQU     4303
LVM_GETFOOTERITEM       EQU     4304

; ListView footer item flags
LVFIF_TEXT              EQU     1
LVFIF_STATE             EQU     2

LVFIS_FOCUSED           EQU     1
LVFIS_SELECTED          EQU     2

; ListView item index rect messages
LVM_GETITEMINDEXRECT    EQU     4305
LVM_SETITEMINDEXSTATE   EQU     4306
LVM_GETNEXTITEMINDEX     EQU     4307

; ListView item index rect flags
LVIR_BOUNDS             EQU     0
LVIR_ICON               EQU     1
LVIR_LABEL              EQU     2
LVIR_SELECTBOUNDS       EQU     3

; ListView background image messages
LVM_SETBKIMAGE          EQU     4165
LVM_GETBKIMAGE          EQU     4166

; ListView background image flags
LVBKIF_SOURCE_NONE      EQU     0
LVBKIF_SOURCE_HBITMAP   EQU     1
LVBKIF_SOURCE_URL       EQU     2
LVBKIF_SOURCE_MASK      EQU     3
LVBKIF_STYLE_NORMAL     EQU     0
LVBKIF_STYLE_TILE       EQU     10h
LVBKIF_STYLE_MASK       EQU     10h
LVBKIF_FLAG_TILEOFFSET  EQU     100h
LVBKIF_TYPE_WATERMARK   EQU     10000000h
LVBKIF_FLAG_ALPHABLEND  EQU     20000000h

; ListView extended tile view messages
LVM_SETVIEW             EQU     4238
LVM_GETVIEW             EQU     4239

; ListView view flags
LV_VIEW_ICON            EQU     0
LV_VIEW_DETAILS         EQU     1
LV_VIEW_SMALLICON       EQU     2
LV_VIEW_LIST            EQU     3
LV_VIEW_TILE            EQU     4
LV_VIEW_MAX             EQU     4

; ListView grouping messages
LVM_INSERTGROUP         EQU     4241
LVM_SETGROUPINFO        EQU     4243
LVM_GETGROUPINFO        EQU     4245
LVM_REMOVEGROUP         EQU     4246
LVM_MOVEGROUP           EQU     4247
LVM_MOVEITEMTOGROUP     EQU     4250

; ListView group flags
LVGF_NONE               EQU     0
LVGF_HEADER             EQU     1
LVGF_FOOTER             EQU     2
LVGF_STATE              EQU     4
LVGF_ALIGN              EQU     8
LVGF_GROUPID            EQU     10h
LVGF_SUBTITLE           EQU     100h
LVGF_TASK               EQU     200h
LVGF_DESCRIPTIONTOP     EQU     400h
LVGF_DESCRIPTIONBOTTOM  EQU     800h
LVGF_TITLEIMAGE         EQU     1000h
LVGF_EXTENDEDIMAGE      EQU     2000h
LVGF_ITEMS              EQU     4000h
LVGF_SUBSET             EQU     8000h
LVGF_SUBSETITEMS        EQU     10000h

; ListView group states
LVGS_NORMAL             EQU     0
LVGS_COLLAPSED          EQU     1
LVGS_HIDDEN             EQU     2
LVGS_NOHEADER           EQU     4
LVGS_COLLAPSIBLE        EQU     8
LVGS_FOCUSED            EQU     10h
LVGS_SELECTED           EQU     20h
LVGS_SUBSETED           EQU     40h
LVGS_SUBSETLINKFOCUSED  EQU     80h

; ListView group alignment
LVGA_HEADER_LEFT        EQU     1
LVGA_HEADER_CENTER      EQU     2
LVGA_HEADER_RIGHT       EQU     4
LVGA_FOOTER_LEFT        EQU     8
LVGA_FOOTER_CENTER      EQU     10h
LVGA_FOOTER_RIGHT       EQU     20h

; ListView tile info messages
LVM_SETTILEVIEWINFO     EQU     4258
LVM_GETTILEVIEWINFO     EQU     4259
LVM_SETTILEINFO         EQU     4260
LVM_GETTILEINFO         EQU     4261

; ListView tile info flags
TILEVIEWINFO_MASK       EQU     1
TILEVIEWINFO_MASK       EQU     2
TILEVIEWINFO_MASK       EQU     4

TILEVIEWINFO_MASK       EQU     1
TILEVIEWINFO_MASK       EQU     2
TILEVIEWINFO_MASK       EQU     4

; ListView insert mark messages
LVM_SETINSERTMARK       EQU     4262
LVM_GETINSERTMARK       EQU     4263
LVM_INSERTMARKHITTEST   EQU     4264
LVM_GETINSERTMARKRECT   EQU     4265
LVM_SETINSERTMARKCOLOR  EQU     4266
LVM_GETINSERTMARKCOLOR  EQU     4267

; ListView insert mark flags
LVIM_AFTER              EQU     1

; ListView color messages
LVM_SETBKCOLOR          EQU     4097
LVM_GETBKCOLOR          EQU     4096
LVM_SETTEXTCOLOR        EQU     4132
LVM_GETTEXTCOLOR        EQU     4131
LVM_SETTEXTBKCOLOR      EQU     4134
LVM_GETTEXTBKCOLOR      EQU     4133

; ListView Unicode format messages
LVM_SETUNICODEFORMAT    EQU     8197
LVM_GETUNICODEFORMAT    EQU     8198

; ListView item spacing messages
LVM_GETITEMSPACING      EQU     4147
LVM_SETICONSPACING      EQU     4149

; ListView work area messages
LVM_SETWORKAREAS        EQU     4161
LVM_GETWORKAREAS        EQU     4162
LVM_GETNUMBEROFWORKAREAS EQU    4163

; ListView tool tip messages
LVM_SETTOOLTIPS         EQU     4168
LVM_GETTOOLTIPS         EQU     4169
LVM_SETTOOLTIPS         EQU     4168
LVM_GETTOOLTIPS         EQU     4169

; ListView hot item messages
LVM_SETHOTITEM          EQU     4156
LVM_GETHOTITEM          EQU     4157
LVM_SETHOTCURSOR        EQU     4158
LVM_GETHOTCURSOR        EQU     4159

; ListView selection mark messages
LVM_GETSELECTIONMARK    EQU     4164
LVM_SETSELECTIONMARK    EQU     4165

; ListView check box messages
LVM_SETITEMSTATE        EQU     4139
LVM_GETITEMSTATE        EQU     4140

; ListView column order array messages
LVM_SETCOLUMNORDERARRAY EQU     4154
LVM_GETCOLUMNORDERARRAY EQU     4155

; ListView header drag drop messages
LVM_SETEXTENDEDLISTVIEWSTYLE EQU 4150
LVM_GETEXTENDEDLISTVIEWSTYLE EQU 4151

; ListView sub item image messages
LVM_SETSUBITEM          EQU     4152
LVM_GETSUBITEMRECT      EQU     4152
LVM_SUBITEMHITTEST      EQU     4153

; ListView approximate view rect messages
LVM_APPROXIMATEVIEWRECT EQU     4160

; ListView empty text messages
LVM_GETEMPTYTEXT        EQU     4300

; ListView footer messages
LVM_GETFOOTERRECT       EQU     4301
LVM_GETFOOTERINFO       EQU     4302
LVM_GETFOOTERITEMRECT   EQU     4303
LVM_GETFOOTERITEM       EQU     4304

; ListView footer item flags
LVFIF_TEXT              EQU     1
LVFIF_STATE             EQU     2

LVFIS_FOCUSED           EQU     1
LVFIS_SELECTED          EQU     2

; ListView item index rect messages
LVM_GETITEMINDEXRECT    EQU     4305
LVM_SETITEMINDEXSTATE   EQU     4306
LVM_GETNEXTITEMINDEX     EQU     4307

; ListView item index rect flags
LVIR_BOUNDS             EQU     0
LVIR_ICON               EQU     1
LVIR_LABEL              EQU     2
LVIR_SELECTBOUNDS       EQU     3

; ListView background image messages
LVM_SETBKIMAGE          EQU     4165
LVM_GETBKIMAGE          EQU     4166

; ListView background image flags
LVBKIF_SOURCE_NONE      EQU     0
LVBKIF_SOURCE_HBITMAP   EQU     1
LVBKIF_SOURCE_URL       EQU     2
LVBKIF_SOURCE_MASK      EQU     3
LVBKIF_STYLE_NORMAL     EQU     0
LVBKIF_STYLE_TILE       EQU     10h
LVBKIF_STYLE_MASK       EQU     10h
LVBKIF_FLAG_TILEOFFSET  EQU     100h
LVBKIF_TYPE_WATERMARK   EQU     10000000h
LVBKIF_FLAG_ALPHABLEND  EQU     20000000h

; ListView extended tile view messages
LVM_SETVIEW             EQU     4238
LVM_GETVIEW             EQU     4239

; ListView view flags
LV_VIEW_ICON            EQU     0
LV_VIEW_DETAILS         EQU     1
LV_VIEW_SMALLICON       EQU     2
LV_VIEW_LIST            EQU     3
LV_VIEW_TILE            EQU     4
LV_VIEW_MAX             EQU     4

; ListView grouping messages
LVM_INSERTGROUP         EQU     4241
LVM_SETGROUPINFO        EQU     4243
LVM_GETGROUPINFO        EQU     4245
LVM_REMOVEGROUP         EQU     4246
LVM_MOVEGROUP           EQU     4247
LVM_MOVEITEMTOGROUP     EQU     4250

; ListView group flags
LVGF_NONE               EQU     0
LVGF_HEADER             EQU     1
LVGF_FOOTER             EQU     2
LVGF_STATE              EQU     4
LVGF_ALIGN              EQU     8
LVGF_GROUPID            EQU     10h
LVGF_SUBTITLE           EQU     100h
LVGF_TASK               EQU     200h
LVGF_DESCRIPTIONTOP     EQU     400h
LVGF_DESCRIPTIONBOTTOM  EQU     800h
LVGF_TITLEIMAGE         EQU     1000h
LVGF_EXTENDEDIMAGE      EQU     2000h
LVGF_ITEMS              EQU     4000h
LVGF_SUBSET             EQU     8000h
LVGF_SUBSETITEMS        EQU     10000h

; ListView group states
LVGS_NORMAL             EQU     0
LVGS_COLLAPSED          EQU     1
LVGS_HIDDEN             EQU     2
LVGS_NOHEADER           EQU     4
LVGS_COLLAPSIBLE        EQU     8
LVGS_FOCUSED            EQU     10h
LVGS_SELECTED           EQU     20h
LVGS_SUBSETED           EQU     40h
LVGS_SUBSETLINKFOCUSED  EQU     80h

; ListView group alignment
LVGA_HEADER_LEFT        EQU     1
LVGA_HEADER_CENTER      EQU     2
LVGA_HEADER_RIGHT       EQU     4
LVGA_FOOTER_LEFT        EQU     8
LVGA_FOOTER_CENTER      EQU     10h
LVGA_FOOTER_RIGHT       EQU     20h

; ListView tile info messages
LVM_SETTILEVIEWINFO     EQU     4258
LVM_GETTILEVIEWINFO     EQU     4259
LVM_SETTILEINFO         EQU     4260
LVM_GETTILEINFO         EQU     4261

; ListView tile info flags
TILEVIEWINFO_MASK       EQU     1
TILEVIEWINFO_MASK       EQU     2
TILEVIEWINFO_MASK       EQU     4

TILEVIEWINFO_MASK       EQU     1
TILEVIEWINFO_MASK       EQU     2
TILEVIEWINFO_MASK       EQU     4

; ListView insert mark messages
LVM_SETINSERTMARK       EQU     4262
LVM_GETINSERTMARK       EQU     4263
LVM_INSERTMARKHITTEST   EQU     4264
LVM_GETINSERTMARKRECT   EQU     4265
LVM_SETINSERTMARKCOLOR  EQU     4266
LVM_GETINSERTMARKCOLOR  EQU     4267

; ListView insert mark flags
LVIM_AFTER              EQU     1

; ListView color messages
LVM_SETBKCOLOR          EQU     4097
LVM_GETBKCOLOR          EQU     4096
LVM_SETTEXTCOLOR        EQU     4132
LVM_GETTEXTCOLOR        EQU     4131
LVM_SETTEXTBKCOLOR      EQU     4134
LVM_GETTEXTBKCOLOR      EQU     4133

; ListView Unicode format messages
LVM_SETUNICODEFORMAT    EQU     8197
LVM_GETUNICODEFORMAT    EQU     8198

; ListView item spacing messages
LVM_GETITEMSPACING      EQU     4147
LVM_SETICONSPACING      EQU     4149

; ListView work area messages
LVM_SETWORKAREAS        EQU     4161
LVM_GETWORKAREAS        EQU     4162
LVM_GETNUMBEROFWORKAREAS EQU    4163

; ListView tool tip messages
LVM_SETTOOLTIPS         EQU     4168
LVM_GETTOOLTIPS         EQU     4169
LVM_SETTOOLTIPS         EQU     4168
LVM_GETTOOLTIPS         EQU     4169

; ListView hot item messages
LVM_SETHOTITEM          EQU     4156
LVM_GETHOTITEM          EQU     4157
LVM_SETHOTCURSOR        EQU     4158
LVM_GETHOTCURSOR        EQU     4159

; ListView selection mark messages
LVM_GETSELECTIONMARK    EQU     4164
LVM_SETSELECTIONMARK    EQU     4165

; ListView check box messages
LVM_SETITEMSTATE        EQU     4139
LVM_GETITEMSTATE        EQU     4140

; ListView column order array messages
LVM_SETCOLUMNORDERARRAY EQU     4154
LVM_GETCOLUMNORDERARRAY EQU     4155

; ListView header drag drop messages
LVM_SETEXTENDEDLISTVIEWSTYLE EQU 4150
LVM_GETEXTENDEDLISTVIEWSTYLE EQU 4151

; ListView sub item image messages
LVM_SETSUBITEM          EQU     4152
LVM_GETSUBITEMRECT      EQU     4152
LVM_SUBITEMHITTEST      EQU     4153

; ListView approximate view rect messages
LVM_APPROXIMATEVIEWRECT EQU     4160

; ListView empty text messages
LVM_GETEMPTYTEXT        EQU     4300

; ListView footer messages
LVM_GETFOOTERRECT       EQU     4301
LVM_GETFOOTERINFO       EQU     4302
LVM_GETFOOTERITEMRECT   EQU     4303
LVM_GETFOOTERITEM       EQU     4304

; ListView footer item flags
LVFIF_TEXT              EQU     1
LVFIF_STATE             EQU     2

LVFIS_FOCUSED           EQU     1
LVFIS_SELECTED          EQU     2

; ListView item index rect messages
LVM_GETITEMINDEXRECT    EQU     4305
LVM_SETITEMINDEXSTATE   EQU     4306
LVM_GETNEXTITEMINDEX     EQU     4307

; ListView item index rect flags
LVIR_BOUNDS             EQU     0
LVIR_ICON               EQU     1
LVIR_LABEL              EQU     2
LVIR_SELECTBOUNDS       EQU     3

; ListView background image messages
LVM_SETBKIMAGE          EQU     4165
LVM_GETBKIMAGE          EQU     4166

; ListView background image flags
LVBKIF_SOURCE_NONE      EQU     0
LVBKIF_SOURCE_HBITMAP   EQU     1
LVBKIF_SOURCE_URL       EQU     2
LVBKIF_SOURCE_MASK      EQU     3
LVBKIF_STYLE_NORMAL     EQU     0
LVBKIF_STYLE_TILE       EQU     10h
LVBKIF_STYLE_MASK       EQU     10h
LVBKIF_FLAG_TILEOFFSET  EQU     100h
LVBKIF_TYPE_WATERMARK   EQU     10000000h
LVBKIF_FLAG_ALPHABLEND  EQU     20000000h

; ListView extended tile view messages
LVM_SETVIEW             EQU     4238
LVM_GETVIEW             EQU     4239

; ListView view flags
LV_VIEW_ICON            EQU     0
LV_VIEW_DETAILS         EQU     1
LV_VIEW_SMALLICON       EQU     2
LV_VIEW_LIST            EQU     3
LV_VIEW_TILE            EQU     4
LV_VIEW_MAX             EQU     4

; ListView grouping messages
LVM_INSERTGROUP         EQU     4241
LVM_SETGROUPINFO        EQU     4243
LVM_GETGROUPINFO        EQU     4245
LVM_REMOVEGROUP         EQU     4246
LVM_MOVEGROUP           EQU     4247
LVM_MOVEITEMTOGROUP     EQU     4250

; ListView group flags
LVGF_NONE               EQU     0
LVGF_HEADER             EQU     1
LVGF_FOOTER             EQU     2
LVGF_STATE              EQU     4
LVGF_ALIGN              EQU     8
LVGF_GROUPID            EQU     10h
LVGF_SUBTITLE           EQU     100h
LVGF_TASK               EQU     200h
LVGF_DESCRIPTIONTOP     EQU     400h
LVGF_DESCRIPTIONBOTTOM  EQU     800h
LVGF_TITLEIMAGE         EQU     1000h
LVGF_EXTENDEDIMAGE      EQU     2000h
LVGF_ITEMS              EQU     4000h
LVGF_SUBSET             EQU     8000h
LVGF_SUBSETITEMS        EQU     10000h

; ListView group states
LVGS_NORMAL             EQU     0
LVGS_COLLAPSED          EQU     1
LVGS_HIDDEN             EQU     2
LVGS_NOHEADER           EQU     4
LVGS_COLLAPSIBLE        EQU     8
LVGS_FOCUSED            EQU     10h
LVGS_SELECTED           EQU     20h
LVGS_SUBSETED           EQU     40h
LVGS_SUBSETLINKFOCUSED  EQU     80h

; ListView group alignment
LVGA_HEADER_LEFT        EQU     1
LVGA_HEADER_CENTER      EQU     2
LVGA_HEADER_RIGHT       EQU     4
LVGA_FOOTER_LEFT        EQU     8
LVGA_FOOTER_CENTER      EQU     10h
LVGA_FOOTER_RIGHT       EQU     20h

; ListView tile info messages
LVM_SETTILEVIEWINFO     EQU     4258
LVM_GETTILEVIEWINFO     EQU     4259
LVM_SETTILEINFO         EQU     4260
LVM_GETTILEINFO         EQU     4261

; ListView tile info flags
TILEVIEWINFO_MASK       EQU     1
TILEVIEWINFO_MASK       EQU     2
TILEVIEWINFO_MASK       EQU     4

TILEVIEWINFO_MASK       EQU     1
TILEVIEWINFO_MASK       EQU     2
TILEVIEWINFO_MASK       EQU     4

; ListView insert mark messages
LVM_SETINSERTMARK       EQU     4262
LVM_GETINSERTMARK       EQU     4263
LVM_INSERTMARKHITTEST   EQU     4264
LVM_GETINSERTMARKRECT   EQU     4265
LVM_SETINSERTMARKCOLOR  EQU     4266
LVM_GETINSERTMARKCOLOR  EQU     4267

; ListView insert mark flags
LVIM_AFTER              EQU     1

END