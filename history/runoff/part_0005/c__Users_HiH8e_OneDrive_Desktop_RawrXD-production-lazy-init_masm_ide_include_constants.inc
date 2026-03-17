; ============================================================================
; RawrXD Agentic IDE - Constants
; ============================================================================

; Menu IDs
IDM_FILE_NEW            equ 100
IDM_FILE_OPEN           equ 101
IDM_FILE_SAVE           equ 102
IDM_FILE_SAVEAS         equ 103
IDM_FILE_CLOSE          equ 104
IDM_FILE_EXIT           equ 109
IDM_FILE_COMPRESS_INFO  equ 110

IDM_EDIT_UNDO           equ 200
IDM_EDIT_REDO           equ 201
IDM_EDIT_CUT            equ 202
IDM_EDIT_COPY           equ 203
IDM_EDIT_PASTE          equ 204
IDM_EDIT_FIND           equ 205
IDM_EDIT_REPLACE        equ 206

IDM_AGENTIC_WISH        equ 300
IDM_AGENTIC_LOOP        equ 301
IDM_AGENTIC_STOP        equ 302
IDM_AGENTIC_PAUSE       equ 303
IDM_AGENTIC_RESUME      equ 304

IDM_TOOLS_REGISTRY      equ 400
IDM_TOOLS_SETTINGS      equ 401
IDM_TOOLS_MODELS        equ 402
IDM_TOOLS_LOAD_GGUF     equ 403
IDM_TOOLS_COMPRESS      equ 404

IDM_VIEW_FILETREE       equ 500
IDM_VIEW_TERMINAL       equ 501
IDM_VIEW_CHAT           equ 502
IDM_VIEW_ORCHESTRA      equ 503
IDM_VIEW_FLOATING       equ 504
IDM_VIEW_MAXMODE        equ 505
IDM_VIEW_REFRESH_TREE   equ 506

IDM_HELP_ABOUT          equ 600

; Control IDs
IDC_FILETREE            equ 1000
IDC_TABCONTROL          equ 1001
IDC_EDITOR              equ 1002
IDC_TERMINAL            equ 1003
IDC_CHAT                equ 1004
IDC_ORCHESTRA           equ 1005
IDC_PROGRESS            equ 1006
IDC_STATUSBAR           equ 1007
IDC_TOOLBAR             equ 1008

; Buffer sizes
MAX_PATH_SIZE           equ 260
MAX_BUFFER_SIZE         equ 65536
MAX_LINE_LENGTH         equ 2048
MAX_TOOL_COUNT          equ 100
MAX_ITERATION_COUNT     equ 20

; Toolbar button IDs
TB_NEW                  equ 2000
TB_OPEN                 equ 2001
TB_SAVE                 equ 2002
TB_WISH                 equ 2003
TB_LOOP                 equ 2004
TB_STOP                 equ 2005
TB_SETTINGS             equ 2006
TB_MAXMODE              equ 2007

; Timer IDs
TIMER_AUTOSAVE          equ 3000
TIMER_STATUS_UPDATE     equ 3001
TIMER_AGENTIC_POLL      equ 3002

; String constants
szSegoeUI               db "Segoe UI", 0
szConsolas              db "Consolas", 0
szCourierNew            db "Courier New", 0
szToolbarClass          db "ToolbarWindow32", 0
szStatusClass           db "msctls_statusbar32", 0
szTabClass              db "SysTabControl32", 0
szTreeClass             db "SysTreeView32", 0
szEditClass             db "EDIT", 0
szRichEditClass         db "RICHEDIT50W", 0
szProgressClass         db "msctls_progress32", 0

; File filters
szAllFilesFilter        db "All Files (*.*)", 0, "*.*", 0, 0
szAsmFilter             db "Assembly Files (*.asm)", 0, "*.asm", 0,
                           "Include Files (*.inc)", 0, "*.inc", 0,
                           "All Files (*.*)", 0, "*.*", 0, 0
szCppFilter             db "C++ Files (*.cpp;*.h)", 0, "*.cpp;*.h", 0,
                           "All Files (*.*)", 0, "*.*", 0, 0
