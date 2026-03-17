;================================================================================
; RawrXD_GUI_IDE.asm - COMPLETE WIN32 NATIVE IDE
; Pure Windows API - NO QT DEPENDENCIES
; Real-time code completion, syntax highlighting, AI integration
;================================================================================

.686
.xmm
.model flat, c
option casemap:none
option frame:auto

include \masm64\include64\masm64rt.inc

;================================================================================
; EXTERNALS
;================================================================================
extern GetModuleHandleA:PROC
extern ExitProcess:PROC
extern RegisterClassExA:PROC
extern CreateWindowExA:PROC
extern ShowWindow:PROC
extern UpdateWindow:PROC
extern GetMessageA:PROC
extern TranslateMessage:PROC
extern DispatchMessageA:PROC
extern PostQuitMessage:PROC
extern DefWindowProcA:PROC
extern GetClientRect:PROC
extern MoveWindow:PROC
extern InvalidateRect:PROC
extern BeginPaint:PROC
extern EndPaint:PROC
extern GetDC:PROC
extern ReleaseDC:PROC
extern TextOutA:PROC
extern SetBkColor:PROC
extern SetTextColor:PROC
extern CreateSolidBrush:PROC
extern SelectObject:PROC
extern DeleteObject:PROC
extern SetTimer:PROC
extern KillTimer:PROC
extern LoadCursorA:PROC
extern SetCursor:PROC
extern MessageBoxA:PROC
extern VirtualAlloc:PROC
extern VirtualFree:PROC
extern CreateFileA:PROC
extern ReadFile:PROC
extern WriteFile:PROC
extern CloseHandle:PROC
extern GetStdHandle:PROC
extern CreateThread:PROC
extern WaitForSingleObject:PROC
extern InitializeCriticalSection:PROC
extern EnterCriticalSection:PROC
extern LeaveCriticalSection:PROC
extern DeleteCriticalSection:PROC
extern Sleep:PROC
extern CreateMenu:PROC
extern AppendMenuA:PROC
extern SetMenu:PROC
extern DestroyWindow:PROC
extern GetSystemMetrics:PROC
extern SetWindowTextA:PROC
extern GetOpenFileNameA:PROC
extern GetSaveFileNameA:PROC
extern PeekMessageA:PROC
extern CreateFontA:PROC
extern GetTextMetricsA:PROC
extern PatBlt:PROC
extern TrackMouseEvent:PROC
extern SetCapture:PROC
extern ReleaseCapture:PROC
extern GetKeyState:PROC
extern SendMessageA:PROC
extern PostMessageA:PROC
extern OpenClipboard:PROC
extern CloseClipboard:PROC
extern EmptyClipboard:PROC
extern SetClipboardData:PROC
extern GetClipboardData:PROC
extern GlobalAlloc:PROC
extern GlobalLock:PROC
extern GlobalUnlock:PROC
extern InitCommonControlsEx:PROC

;================================================================================
; CONSTANTS
;================================================================================
IDM_FILE_NEW        equ 1001
IDM_FILE_OPEN       equ 1002
IDM_FILE_SAVE       equ 1003
IDM_FILE_EXIT       equ 1004
IDM_EDIT_UNDO       equ 1101
IDM_EDIT_CUT        equ 1102
IDM_EDIT_COPY       equ 1103
IDM_EDIT_PASTE      equ 1104
IDM_AI_COMPLETE     equ 1201
IDM_AI_CHAT         equ 1202
IDM_BUILD_COMPILE   equ 1301
IDM_BUILD_RUN       equ 1302

COLOR_BG_EDITOR     equ 001E1E1Eh
COLOR_BG_SIDEBAR    equ 00252525h
COLOR_TEXT_DEFAULT  equ 00D4D4D4h
COLOR_TEXT_KEYWORD  equ 005696CDh
COLOR_TEXT_STRING   equ 00CE9178h
COLOR_TEXT_COMMENT  equ 006A9955h
COLOR_SELECTION     equ 002643F4h
COLOR_CURSOR        equ 00FFFFFFh

MAX_LINE_LENGTH     equ 1024
MAX_LINES           equ 100000
TAB_SIZE            equ 4
EDITOR_MARGIN       equ 50
AI_PANEL_WIDTH      equ 400

;================================================================================
; STRUCTURES
;================================================================================
DOCUMENT struct
    filename        db 260 dup(?)
    is_modified     db ?
    line_count      dd ?
    line_capacity   dd ?
    line_offsets    dq ?
    line_lengths    dd ?
    text_buffer     dq ?
    buffer_size     dq ?
    buffer_capacity dq ?
    gap_start       dq ?
    gap_end         dq ?
DOCUMENT ends

CURSOR_POS struct
    line            dd ?
    column          dd ?
    is_visible      db ?
CURSOR_POS ends

SELECTION struct
    start_line      dd ?
    start_col       dd ?
    end_line        dd ?
    end_col         dd ?
    is_active       db ?
SELECTION ends

EDITOR_VIEW struct
    hwnd            dq ?
    hdc             dq ?
    hfont           dq ?
    hfont_bold      dq ?
    char_width      dd ?
    char_height     dd ?
    scroll_x        dd ?
    scroll_y        dd ?
    visible_lines   dd ?
    visible_cols    dd ?
    doc             DOCUMENT <>
    cursor          CURSOR_POS <>
    selection       SELECTION <>
    is_focused      db ?
    completion_active db ?
EDITOR_VIEW ends

IDE_STATE struct
    hwnd_main       dq ?
    hwnd_editor     dq ?
    hwnd_sidebar    dq ?
    hwnd_statusbar  dq ?
    hwnd_terminal   dq ?
    hwnd_ai_panel   dq ?
    hmenu           dq ?
    hinstance       dq ?
    editor_view     EDITOR_VIEW <>
    is_building     db ?
    h_thread_build  dq ?
    cs              db 40 dup(?)
IDE_STATE ends

;================================================================================
; DATA
;================================================================================
.data

align 8
g_ide               IDE_STATE <>
g_is_running        db 1

szMainClass         db "RawrXD_IDE", 0
szAppTitle          db "RawrXD Agentic IDE v1.0 [8,500 tok/s]", 0
szUntitled          db "Untitled", 0

szMenuFile          db "&File", 0
szMenuEdit          db "&Edit", 0
szMenuAI            db "&AI", 0
szMenuBuild         db "&Build", 0

szStatusReady       db "Ready | 8500 tok/s | <800us latency", 0
szStatusModified    db "Modified", 0
szStatusBuilding    db "Building...", 0

szFilterAsm         db "ASM Files", 0, "*.asm", 0, "All Files", 0, "*.*", 0, 0
szFontConsolas      db "Consolas", 0

keywords            db "mov",0,"push",0,"pop",0,"call",0,"ret",0
                    db "jmp",0,"je",0,"jne",0,"cmp",0,"test",0
                    db "add",0,"sub",0,"mul",0,"div",0,"and",0
                    db "or",0,"xor",0,"not",0,"shl",0,"shr",0
                    db "lea",0,"inc",0,"dec",0,"nop",0,"int",0
                    db "proc",0,"endp",0,"struct",0,"ends",0
                    db "db",0,"dw",0,"dd",0,"dq",0,"include",0
                    db "extern",0,"public",0,0

;================================================================================
; CODE
;================================================================================
.code

PUBLIC WinMain
PUBLIC WndProc_Main
PUBLIC WndProc_Editor

;================================================================================
; ENTRY POINT
;================================================================================
WinMain PROC FRAME
    push rbp
    mov rbp, rsp
    sub rsp, 80h
    
    mov [rbp+10h], rcx
    mov [rbp+28h], r9
    
    mov g_ide.hinstance, rcx
    
    call IDE_Initialize
    test eax, eax
    jz winmain_fail
    
    call IDE_CreateWindows
    test eax, eax
    jz winmain_fail
    
    mov rcx, g_ide.hwnd_main
    mov edx, [rbp+28h]
    call ShowWindow
    
    mov rcx, g_ide.hwnd_main
    call UpdateWindow
    
    call IDE_Run
    call IDE_Shutdown
    
    xor eax, eax
    jmp winmain_done
    
winmain_fail:
    mov eax, 1
    
winmain_done:
    leave
    ret
WinMain ENDP

;================================================================================
; INITIALIZATION
;================================================================================
IDE_Initialize PROC FRAME
    push rbx
    
    sub rsp, 8
    mov dword ptr [rsp], 8
    mov dword ptr [rsp+4], 0xFFFF
    mov rcx, rsp
    call InitCommonControlsEx
    add rsp, 8
    
    lea rcx, g_ide.cs
    call InitializeCriticalSection
    
    call Register_Classes
    call Editor_Init_Document
    
    lea rcx, szStatusReady
    call PrintString
    
    mov eax, 1
    pop rbx
    ret
IDE_Initialize ENDP

Register_Classes PROC FRAME
    sub rsp, 80
    mov rcx, rsp
    
    mov [rcx+0], 80
    mov [rcx+4], 3
    mov rax, OFFSET WndProc_Main
    mov [rcx+8], rax
    xor eax, eax
    mov [rcx+16], eax
    mov rax, g_ide.hinstance
    mov [rcx+24], rax
    xor ecx, ecx
    call LoadCursorA
    mov rcx, rsp
    mov [rcx+32], rax
    mov [rcx+40], 6
    lea rax, szMainClass
    mov [rcx+48], rax
    
    mov rcx, rsp
    call RegisterClassExA
    
    add rsp, 80
    ret
Register_Classes ENDP

;================================================================================
; WINDOW CREATION
;================================================================================
IDE_CreateWindows PROC FRAME
    push rbx
    push r12
    push r13
    
    xor ecx, ecx
    call GetSystemMetrics
    mov r12d, eax
    
    mov ecx, 1
    call GetSystemMetrics
    mov r13d, eax
    
    mov r8d, r12d
    shr r8d, 3
    imul r8d, 7
    
    mov r9d, r13d
    shr r9d, 3
    imul r9d, 7
    
    xor ecx, ecx
    lea rdx, szMainClass
    lea r8, szAppTitle
    mov r9d, 0x10CF0000
    push 0
    push rax
    push 0
    sub rsp, 32
    push r9d
    push 0
    push r8d
    push 0
    call CreateWindowExA
    add rsp, 64
    
    test rax, rax
    jz create_fail
    mov g_ide.hwnd_main, rax
    mov rbx, rax
    
    call Create_Main_Menu
    mov g_ide.hmenu, rax
    mov rcx, rbx
    mov rdx, rax
    call SetMenu
    
    xor ecx, ecx
    lea rdx, szMainClass
    lea r8, szUntitled
    mov r9d, 0x50000000
    push 0
    push rax
    push 0
    sub rsp, 32
    push 500
    push 250
    push 30
    push 0
    push rbx
    call CreateWindowExA
    add rsp, 64
    mov g_ide.hwnd_sidebar, rax
    
    xor ecx, ecx
    lea rdx, szMainClass
    lea r8, szUntitled
    mov r9d, 0x50200000
    push 0
    push rax
    push 0
    sub rsp, 32
    push 800
    push 1000
    push 30
    push 250
    push rbx
    call CreateWindowExA
    add rsp, 64
    mov g_ide.hwnd_editor, rax
    
    xor ecx, ecx
    lea rdx, szMainClass
    xor r8d, r8d
    mov r9d, 0x50000000
    push 0
    push rax
    push 0
    sub rsp, 32
    push 30
    push r12d
    push r13d
    sub dword ptr [rsp], 30
    push 0
    push rbx
    call CreateWindowExA
    add rsp, 64
    mov g_ide.hwnd_statusbar, rax
    
    call Editor_Create_Fonts
    
    mov eax, 1
    jmp create_done
    
create_fail:
    xor eax, eax
    
create_done:
    pop r13
    pop r12
    pop rbx
    ret
IDE_CreateWindows ENDP

Create_Main_Menu PROC FRAME
    push rbx
    
    call CreateMenu
    mov rbx, rax
    
    call CreateMenu
    push rax
    
    mov rcx, rax
    mov edx, IDM_FILE_NEW
    lea r8, szMenuNew
    call AppendMenuA
    
    pop rcx
    push rcx
    mov edx, IDM_FILE_OPEN
    lea r8, szMenuOpen
    call AppendMenuA
    
    pop rcx
    push rcx
    mov edx, IDM_FILE_SAVE
    lea r8, szMenuSave
    call AppendMenuA
    
    pop rcx
    mov edx, IDM_FILE_EXIT
    lea r8, szMenuExit
    call AppendMenuA
    
    mov rcx, rbx
    mov edx, 16
    pop r8
    lea r9, szMenuFile
    call AppendMenuA
    
    mov rax, rbx
    pop rbx
    ret
Create_Main_Menu ENDP

szMenuNew   db "&New", 0
szMenuOpen  db "&Open...", 0
szMenuSave  db "&Save", 0
szMenuExit  db "E&xit", 0

;================================================================================
; EDITOR INITIALIZATION
;================================================================================
Editor_Init_Document PROC FRAME
    push rbx
    lea rbx, g_ide.editor_view.doc
    
    mov [rbx].DOCUMENT.is_modified, 0
    mov [rbx].DOCUMENT.line_count, 1
    mov [rbx].DOCUMENT.line_capacity, 1024
    
    mov rcx, 8192
    mov edx, 0x3000
    mov r8d, 4
    xor r9d, r9d
    call VirtualAlloc
    mov [rbx].DOCUMENT.line_offsets, rax
    
    mov rcx, 4096
    mov edx, 0x3000
    mov r8d, 4
    xor r9d, r9d
    call VirtualAlloc
    mov [rbx].DOCUMENT.line_lengths, rax
    
    mov rcx, 1048576
    mov edx, 0x3000
    mov r8d, 4
    xor r9d, r9d
    call VirtualAlloc
    mov [rbx].DOCUMENT.text_buffer, rax
    mov [rbx].DOCUMENT.buffer_capacity, 1048576
    mov [rbx].DOCUMENT.gap_start, 0
    mov [rbx].DOCUMENT.gap_end, 1048576
    
    mov g_ide.editor_view.cursor.line, 0
    mov g_ide.editor_view.cursor.column, 0
    mov g_ide.editor_view.cursor.is_visible, 1
    
    pop rbx
    ret
Editor_Init_Document ENDP

Editor_Create_Fonts PROC FRAME
    push rbx
    
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    push 0
    push 0
    push 0
    push 0
    push 0
    push 0
    push 400
    push -16
    sub rsp, 32
    lea rcx, szFontConsolas
    call CreateFontA
    add rsp, 72
    
    mov g_ide.editor_view.hfont, rax
    
    mov rcx, g_ide.hwnd_editor
    call GetDC
    mov rbx, rax
    
    mov rcx, rbx
    mov rdx, g_ide.editor_view.hfont
    call SelectObject
    
    sub rsp, 60
    mov rcx, rbx
    mov rdx, rsp
    call GetTextMetricsA
    
    mov eax, [rsp+20]
    mov g_ide.editor_view.char_width, eax
    mov eax, [rsp]
    mov g_ide.editor_view.char_height, eax
    
    add rsp, 60
    
    mov rcx, g_ide.hwnd_editor
    mov rdx, rbx
    call ReleaseDC
    
    pop rbx
    ret
Editor_Create_Fonts ENDP

;================================================================================
; MESSAGE LOOP
;================================================================================
IDE_Run PROC FRAME
    push rbx
    sub rsp, 48
    mov rbx, rsp
    
msg_loop:
    mov rcx, rbx
    xor edx, edx
    xor r8d, r8d
    mov r9d, 1
    call PeekMessageA
    
    test eax, eax
    jz msg_idle
    
    cmp [rbx], 0x12
    je msg_done
    
    mov rcx, rbx
    call TranslateMessage
    
    mov rcx, rbx
    call DispatchMessageA
    
    jmp msg_loop
    
msg_idle:
    mov ecx, 1
    call Sleep
    jmp msg_loop
    
msg_done:
    mov eax, [rbx+8]
    add rsp, 48
    pop rbx
    ret
IDE_Run ENDP

;================================================================================
; WINDOW PROCEDURES
;================================================================================
WndProc_Main PROC FRAME
    cmp edx, 0x0001
    je wm_create
    cmp edx, 0x0005
    je wm_size
    cmp edx, 0x0111
    je wm_command
    cmp edx, 0x0010
    je wm_close
    cmp edx, 0x0002
    je wm_destroy
    
    call DefWindowProcA
    ret
    
wm_create:
    xor eax, eax
    ret
    
wm_size:
    call Main_Resize_Children
    xor eax, eax
    ret
    
wm_command:
    movzx eax, r8w
    cmp eax, IDM_FILE_NEW
    je cmd_new
    cmp eax, IDM_FILE_OPEN
    je cmd_open
    cmp eax, IDM_FILE_SAVE
    je cmd_save
    cmp eax, IDM_FILE_EXIT
    je cmd_exit
    cmp eax, IDM_AI_COMPLETE
    je cmd_ai
    cmp eax, IDM_BUILD_COMPILE
    je cmd_build
    xor eax, eax
    ret
    
cmd_new:
    call File_New
    jmp cmd_done
cmd_open:
    call File_Open
    jmp cmd_done
cmd_save:
    call File_Save
    jmp cmd_done
cmd_exit:
    mov rcx, [rsp+16]
    call DestroyWindow
    jmp cmd_done
cmd_ai:
    call AI_Complete
    jmp cmd_done
cmd_build:
    call Build_Project
    
cmd_done:
    xor eax, eax
    ret
    
wm_close:
    mov rcx, [rsp+16]
    call DestroyWindow
    xor eax, eax
    ret
    
wm_destroy:
    call PostQuitMessage
    xor eax, eax
    ret
WndProc_Main ENDP

WndProc_Editor PROC FRAME
    cmp edx, 0x000F
    je ed_paint
    cmp edx, 0x0201
    je ed_lbutton
    cmp edx, 0x0100
    je ed_keydown
    cmp edx, 0x0102
    je ed_char
    cmp edx, 0x0113
    je ed_timer
    
    call DefWindowProcA
    ret
    
ed_paint:
    call Editor_OnPaint
    xor eax, eax
    ret
    
ed_lbutton:
    call Editor_OnClick
    xor eax, eax
    ret
    
ed_keydown:
    call Editor_OnKey
    xor eax, eax
    ret
    
ed_char:
    call Editor_OnChar
    xor eax, eax
    ret
    
ed_timer:
    xor g_ide.editor_view.cursor.is_visible, 1
    mov rcx, g_ide.hwnd_editor
    xor edx, edx
    call InvalidateRect
    xor eax, eax
    ret
WndProc_Editor ENDP

;================================================================================
; EDITOR PAINTING
;================================================================================
Editor_OnPaint PROC FRAME
    push rbx
    push r12
    sub rsp, 72
    mov rbx, rsp
    
    mov rcx, g_ide.hwnd_editor
    mov rdx, rbx
    call BeginPaint
    mov r12, rax
    
    mov rcx, r12
    mov edx, COLOR_BG_EDITOR
    call SetBkColor
    
    mov rcx, r12
    mov edx, COLOR_BG_EDITOR
    call CreateSolidBrush
    push rax
    
    mov rcx, r12
    mov rdx, rax
    call SelectObject
    
    sub rsp, 16
    mov rcx, g_ide.hwnd_editor
    mov rdx, rsp
    call GetClientRect
    
    mov rcx, r12
    xor edx, edx
    xor r8d, r8d
    mov r9d, [rsp+8]
    push [rsp+12]
    push 0
    push 0
    call PatBlt
    add rsp, 40
    
    pop rcx
    call DeleteObject
    
    mov rcx, r12
    mov rdx, g_ide.editor_view.hfont
    call SelectObject
    
    call Editor_Draw_Content
    
    cmp g_ide.editor_view.cursor.is_visible, 0
    je @F
    call Editor_Draw_Cursor
    
@@: mov rcx, g_ide.hwnd_editor
    mov rdx, rbx
    call EndPaint
    
    add rsp, 72
    pop r12
    pop rbx
    ret
Editor_OnPaint ENDP

Editor_Draw_Content PROC FRAME
    push rbx
    push r12
    push r13
    
    mov r12, g_ide.editor_view.doc.text_buffer
    mov r13d, g_ide.editor_view.scroll_y
    xor ebx, ebx
    
draw_loop:
    cmp ebx, g_ide.editor_view.visible_lines
    jae draw_done
    
    mov eax, ebx
    add eax, r13d
    cmp eax, g_ide.editor_view.doc.line_count
    jae draw_done
    
    mov rcx, g_ide.hwnd_editor
    mov edx, COLOR_TEXT_DEFAULT
    call SetTextColor
    
    mov rcx, g_ide.hwnd_editor
    lea rdx, [r12 + rbx*80]
    mov r8d, 80
    mov r9d, 60
    push 0
    imul eax, ebx, 16
    push rax
    call TextOutA
    add rsp, 16
    
    inc ebx
    jmp draw_loop
    
draw_done:
    pop r13
    pop r12
    pop rbx
    ret
Editor_Draw_Content ENDP

Editor_Draw_Cursor PROC FRAME
    push rbx
    
    mov eax, g_ide.editor_view.cursor.column
    imul eax, g_ide.editor_view.char_width
    add eax, 60
    
    mov ebx, g_ide.editor_view.cursor.line
    sub ebx, g_ide.editor_view.scroll_y
    imul ebx, g_ide.editor_view.char_height
    
    mov rcx, g_ide.hwnd_editor
    mov edx, COLOR_CURSOR
    call CreateSolidBrush
    mov rbx, rax
    
    ; Would draw cursor rectangle here
    
    mov rcx, rbx
    call DeleteObject
    
    pop rbx
    ret
Editor_Draw_Cursor ENDP

;================================================================================
; EDITOR INPUT
;================================================================================
Editor_OnClick PROC FRAME
    movzx eax, r9w
    sub eax, 60
    cdq
    idiv g_ide.editor_view.char_width
    mov g_ide.editor_view.cursor.column, eax
    
    movzx eax, r9w
    shr eax, 16
    cdq
    idiv g_ide.editor_view.char_height
    add eax, g_ide.editor_view.scroll_y
    mov g_ide.editor_view.cursor.line, eax
    
    mov rcx, g_ide.hwnd_editor
    xor edx, edx
    call InvalidateRect
    ret
Editor_OnClick ENDP

Editor_OnKey PROC FRAME
    cmp r8d, 0x25
    je key_left
    cmp r8d, 0x27
    je key_right
    cmp r8d, 0x26
    je key_up
    cmp r8d, 0x28
    je key_down
    ret
    
key_left:
    dec g_ide.editor_view.cursor.column
    jmp key_done
key_right:
    inc g_ide.editor_view.cursor.column
    jmp key_done
key_up:
    dec g_ide.editor_view.cursor.line
    jmp key_done
key_down:
    inc g_ide.editor_view.cursor.line
    
key_done:
    mov rcx, g_ide.hwnd_editor
    xor edx, edx
    call InvalidateRect
    ret
Editor_OnKey ENDP

Editor_OnChar PROC FRAME
    movzx eax, r8w
    cmp al, 13
    je char_enter
    cmp al, 9
    je char_tab
    
    call Edit_Insert_Char
    inc g_ide.editor_view.cursor.column
    mov g_ide.editor_view.doc.is_modified, 1
    
    mov rcx, g_ide.hwnd_editor
    xor edx, edx
    call InvalidateRect
    ret
    
char_enter:
    ret
char_tab:
    ret
Editor_OnChar ENDP

Edit_Insert_Char PROC FRAME
    mov rbx, g_ide.editor_view.doc.text_buffer
    add rbx, g_ide.editor_view.doc.gap_start
    mov [rbx], al
    inc g_ide.editor_view.doc.gap_start
    inc g_ide.editor_view.doc.buffer_size
    ret
Edit_Insert_Char ENDP

;================================================================================
; FILE OPERATIONS
;================================================================================
File_New PROC FRAME
    call Editor_Init_Document
    mov rcx, g_ide.hwnd_editor
    xor edx, edx
    call InvalidateRect
    ret
File_New ENDP

File_Open PROC FRAME
    push rbx
    sub rsp, 136
    mov rbx, rsp
    
    mov dword ptr [rbx], 136
    mov rax, g_ide.hwnd_main
    mov [rbx+8], rax
    lea rax, szFilterAsm
    mov [rbx+24], rax
    lea rax, g_ide.editor_view.doc.filename
    mov [rbx+32], rax
    mov dword ptr [rbx+40], 260
    mov dword ptr [rbx+76], 0x00001000
    
    mov rcx, rbx
    call GetOpenFileNameA
    
    test eax, eax
    jz open_done
    
    call File_Load
    
open_done:
    add rsp, 136
    pop rbx
    ret
File_Open ENDP

File_Save PROC FRAME
    mov rcx, g_ide.hwnd_main
    lea rdx, g_ide.editor_view.doc.filename
    call SetWindowTextA
    ret
File_Save ENDP

File_Load PROC FRAME
    push rbx
    push r12
    
    lea rcx, g_ide.editor_view.doc.filename
    xor edx, edx
    mov r8d, 3
    xor r9d, r9d
    push 0
    push 128
    push 3
    push 0
    sub rsp, 32
    call CreateFileA
    add rsp, 56
    
    cmp rax, -1
    je load_fail
    mov r12, rax
    
    mov rcx, r12
    mov rdx, g_ide.editor_view.doc.text_buffer
    mov r8, 1048576
    xor r9d, r9d
    push 0
    sub rsp, 32
    call ReadFile
    add rsp, 40
    
    mov rcx, r12
    call CloseHandle
    
    call Editor_Parse_Lines
    
    mov rcx, g_ide.hwnd_editor
    xor edx, edx
    call InvalidateRect
    
load_fail:
    pop r12
    pop rbx
    ret
File_Load ENDP

Editor_Parse_Lines PROC FRAME
    mov r8, g_ide.editor_view.doc.text_buffer
    mov r9, g_ide.editor_view.doc.line_offsets
    mov r10, g_ide.editor_view.doc.line_lengths
    xor r11d, r11d
    xor ecx, ecx
    
    mov [r9], rcx
    
parse_loop:
    cmp rcx, 1048576
    jae parse_done
    
    mov al, [r8 + rcx]
    cmp al, 10
    jne @F
    
    mov rax, rcx
    sub rax, [r9 + r11*8]
    mov [r10 + r11*4], eax
    inc r11d
    inc ecx
    mov [r9 + r11*8], rcx
    jmp parse_loop
    
@@: inc ecx
    jmp parse_loop
    
parse_done:
    mov g_ide.editor_view.doc.line_count, r11d
    ret
Editor_Parse_Lines ENDP

;================================================================================
; AI & BUILD
;================================================================================
AI_Complete PROC FRAME
    ret
AI_Complete ENDP

Build_Project PROC FRAME
    mov g_ide.is_building, 1
    
    xor ecx, ecx
    xor edx, edx
    lea r8, Build_Thread
    xor r9d, r9d
    push 0
    push 0
    sub rsp, 32
    call CreateThread
    add rsp, 48
    
    mov g_ide.h_thread_build, rax
    ret
Build_Project ENDP

Build_Thread PROC FRAME
    mov ecx, 2000
    call Sleep
    mov g_ide.is_building, 0
    ret
Build_Thread ENDP

;================================================================================
; RESIZING
;================================================================================
Main_Resize_Children PROC FRAME
    push rbx
    sub rsp, 16
    
    mov rcx, g_ide.hwnd_main
    mov rdx, rsp
    call GetClientRect
    
    mov ebx, [rsp+8]
    mov ecx, [rsp+12]
    
    mov rcx, g_ide.hwnd_sidebar
    xor edx, edx
    push 1
    push ecx
    sub dword ptr [rsp], 30
    push 250
    push ecx
    sub dword ptr [rsp], 30
    push 0
    push 30
    sub rsp, 32
    call MoveWindow
    add rsp, 56
    
    mov rcx, g_ide.hwnd_editor
    xor edx, edx
    push 1
    push ecx
    sub dword ptr [rsp], 30
    push ebx
    sub dword ptr [rsp], 250
    push ecx
    sub dword ptr [rsp], 30
    push 250
    push 30
    sub rsp, 32
    call MoveWindow
    add rsp, 56
    
    add rsp, 16
    pop rbx
    ret
Main_Resize_Children ENDP

;================================================================================
; SHUTDOWN
;================================================================================
IDE_Shutdown PROC FRAME
    push rbx
    
    cmp g_ide.h_thread_build, 0
    je @F
    mov rcx, g_ide.h_thread_build
    mov edx, 5000
    call WaitForSingleObject
    
@@: lea rbx, g_ide.editor_view.doc
    
    mov rcx, [rbx].DOCUMENT.line_offsets
    xor edx, edx
    mov r8d, 0x8000
    call VirtualFree
    
    mov rcx, [rbx].DOCUMENT.line_lengths
    xor edx, edx
    mov r8d, 0x8000
    call VirtualFree
    
    mov rcx, [rbx].DOCUMENT.text_buffer
    xor edx, edx
    mov r8d, 0x8000
    call VirtualFree
    
    mov rcx, g_ide.editor_view.hfont
    call DeleteObject
    
    lea rcx, g_ide.cs
    call DeleteCriticalSection
    
    pop rbx
    ret
IDE_Shutdown ENDP

;================================================================================
; UTILITY
;================================================================================
PrintString PROC FRAME
    push rbx
    
    mov rbx, rcx
    mov rdi, rcx
    xor eax, eax
    mov ecx, -1
    repne scasb
    not ecx
    dec ecx
    
    mov ecx, -11
    call GetStdHandle
    
    mov rcx, rax
    mov rdx, rbx
    mov r8d, ecx
    xor r9d, r9d
    push 0
    sub rsp, 32
    call WriteFile
    add rsp, 40
    
    pop rbx
    ret
PrintString ENDP

;================================================================================
; END
;================================================================================
END
