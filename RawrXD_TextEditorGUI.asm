; ============================================================================
; RawrXD_TextEditorGUI.asm
; Minimal monolithic x64 MASM text editor with IDE frame integration hooks
; ============================================================================

EXTERN GetWindowLongPtrA:PROC
EXTERN SetWindowLongPtrA:PROC
EXTERN DefWindowProcA:PROC
EXTERN GetStockObject:PROC
EXTERN RegisterClassA:PROC
EXTERN CreateWindowExA:PROC
EXTERN BeginPaintA:PROC
EXTERN EndPaintA:PROC
EXTERN FillRect:PROC
EXTERN InvalidateRect:PROC
EXTERN GlobalAlloc:PROC
EXTERN GlobalFree:PROC
EXTERN PostQuitMessage:PROC
EXTERN CreateMenu:PROC
EXTERN AppendMenuA:PROC
EXTERN SetMenu:PROC
EXTERN DrawMenuBar:PROC
EXTERN SetWindowTextA:PROC
EXTERN DestroyWindow:PROC
EXTERN CreateAcceleratorTableA:PROC
EXTERN GetMessageA:PROC
EXTERN TranslateAcceleratorA:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageA:PROC

GWLP_USERDATA       EQU -21

ID_FILE_NEW         EQU 1001
ID_FILE_OPEN        EQU 1002
ID_FILE_SAVE        EQU 1003
ID_FILE_EXIT        EQU 1004
ID_EDIT_UNDO        EQU 2001
ID_EDIT_CUT         EQU 2002
ID_EDIT_COPY        EQU 2003
ID_AI_COMPLETE      EQU 3001

MF_STRING_FLAG      EQU 0
MF_POPUP_FLAG       EQU 10h
FVIRTKEY_FLAG       EQU 01h
FCONTROL_FLAG       EQU 08h

CTX_HWND            EQU 0
CTX_HDC             EQU 8
CTX_HFONT           EQU 16
CTX_CURSOR          EQU 24
CTX_BUFFER          EQU 32
CTX_CHARW           EQU 40
CTX_CHARH           EQU 44
CTX_WIDTH           EQU 48
CTX_HEIGHT          EQU 52
CTX_TOOLBAR         EQU 56
CTX_STATUS          EQU 64
CTX_ACCEL           EQU 72

BUFFER_DATA         EQU 0
BUFFER_CAP          EQU 16
BUFFER_USED         EQU 20

CURSOR_OFFSET       EQU 0
CURSOR_LINE         EQU 8
CURSOR_COL          EQU 16

.DATA
g_CommandHandlers   dq 0
g_AIBackendProc     dq 0
g_AIBackendCtx      dq 0
g_AITokenCount      dd 0
g_AISnapshotBuffer  db 65536 dup (0)

AccelTable:
	db FVIRTKEY_FLAG or FCONTROL_FLAG
	dw 'N'
	dw ID_FILE_NEW
	db FVIRTKEY_FLAG or FCONTROL_FLAG
	dw 'O'
	dw ID_FILE_OPEN
	db FVIRTKEY_FLAG or FCONTROL_FLAG
	dw 'S'
	dw ID_FILE_SAVE
	db FVIRTKEY_FLAG or FCONTROL_FLAG
	dw 'Z'
	dw ID_EDIT_UNDO
	db FVIRTKEY_FLAG or FCONTROL_FLAG
	dw 'X'
	dw ID_EDIT_CUT
	db FVIRTKEY_FLAG or FCONTROL_FLAG
	dw 'C'
	dw ID_EDIT_COPY
	db FVIRTKEY_FLAG or FCONTROL_FLAG
	dw 20h
	dw ID_AI_COMPLETE

szClassName         db "RawrXDTextEditorClass", 0
szStatic            db "STATIC", 0
szToolbar           db "Toolbar", 0
szFileMenu          db "&File", 0
szEditMenu          db "&Edit", 0
szNew               db "&New", 0
szOpen              db "&Open", 0
szSave              db "&Save", 0
szExit              db "E&xit", 0
szUndo              db "&Undo", 0
szCut               db "Cu&t", 0
szCopy              db "&Copy", 0
szAIComplete        db "Co&mplete", 0
szReady             db "Ready", 0
szNeedOpenHandler   db "Register custom Open handler", 0
szNeedSaveHandler   db "Register custom Save handler", 0
szNeedUndoHandler   db "Register custom Undo handler", 0
szNeedCutHandler    db "Register custom Cut handler", 0
szNeedCopyHandler   db "Register custom Copy handler", 0
szAINotConfigured   db "AI backend not configured", 0
szAIUnavailable     db "AI backend returned no tokens", 0
szAIInserted        db "AI completion inserted", 0

.CODE


; ============================================================================
; Window/class bootstrap
; ============================================================================

EditorWindow_RegisterClass PROC
	sub rsp, 120

	lea r10, [rsp + 32]
	mov dword ptr [r10 + 0], 3
	lea rax, [EditorWindow_WNDPROC]
	mov qword ptr [r10 + 8], rax
	mov qword ptr [r10 + 16], 0
	mov qword ptr [r10 + 24], 0
	mov qword ptr [r10 + 32], 0
	mov qword ptr [r10 + 40], 0

	mov ecx, 0
	call GetStockObject
	mov qword ptr [r10 + 48], rax

	mov qword ptr [r10 + 56], 0
	lea rax, [szClassName]
	mov qword ptr [r10 + 64], rax

	mov rcx, r10
	call RegisterClassA

	mov eax, 1
	add rsp, 120
	ret
EditorWindow_RegisterClass ENDP


EditorWindow_Create PROC
	push rbx
	push r12
	sub rsp, 104

	mov rbx, rcx
	mov r12, rdx

	mov qword ptr [rbx + CTX_HWND], 0
	mov qword ptr [rbx + CTX_HDC], 0
	mov qword ptr [rbx + CTX_HFONT], 0
	mov qword ptr [rbx + CTX_CURSOR], 0
	mov qword ptr [rbx + CTX_BUFFER], 0
	mov dword ptr [rbx + CTX_CHARW], 8
	mov dword ptr [rbx + CTX_CHARH], 16
	mov dword ptr [rbx + CTX_WIDTH], 800
	mov dword ptr [rbx + CTX_HEIGHT], 600
	mov qword ptr [rbx + CTX_TOOLBAR], 0
	mov qword ptr [rbx + CTX_STATUS], 0
	mov qword ptr [rbx + CTX_ACCEL], 0

	mov qword ptr [rsp + 32], 100
	mov qword ptr [rsp + 40], 100
	mov qword ptr [rsp + 48], 800
	mov qword ptr [rsp + 56], 600
	mov qword ptr [rsp + 64], 0
	mov qword ptr [rsp + 72], 0
	mov qword ptr [rsp + 80], 0
	mov qword ptr [rsp + 88], rbx

	xor ecx, ecx
	lea rdx, [szClassName]
	mov r8, r12
	mov r9d, 0CF0000h
	call CreateWindowExA

	mov qword ptr [rbx + CTX_HWND], rax

	add rsp, 104
	pop r12
	pop rbx
	ret
EditorWindow_Create ENDP


EditorWindow_WNDPROC PROC
	push rbx
	push r12
	push r13
	push r14
	sub rsp, 40

	mov rbx, rcx
	mov r12d, edx
	mov r13, r8
	mov r14, r9

	cmp r12d, 1
	je WndProc_OnCreate

	mov rcx, rbx
	mov edx, GWLP_USERDATA
	call GetWindowLongPtrA
	mov r10, rax
	test r10, r10
	jz WndProc_Default

	cmp r12d, 15
	je WndProc_OnPaint
	cmp r12d, 256
	je WndProc_OnKeyDown
	cmp r12d, 258
	je WndProc_OnChar
	cmp r12d, 513
	je WndProc_OnMouse
	cmp r12d, 5
	je WndProc_OnSize
	cmp r12d, 273
	je WndProc_OnCommand
	cmp r12d, 2
	je WndProc_OnDestroy
	jmp WndProc_Default

WndProc_OnCreate:
	test r14, r14
	jz WndProc_ReturnZero
	mov r10, [r14]
	test r10, r10
	jz WndProc_ReturnZero
	mov qword ptr [r10 + CTX_HWND], rbx
	mov rcx, rbx
	mov edx, GWLP_USERDATA
	mov r8, r10
	call SetWindowLongPtrA
	mov rcx, r10
	call EditorWindow_OnCreate_Handler
	jmp WndProc_ReturnZero

WndProc_OnPaint:
	mov rcx, r10
	mov rdx, rbx
	call EditorWindow_OnPaint_Handler
	jmp WndProc_ReturnZero

WndProc_OnKeyDown:
	mov rcx, r10
	mov edx, r13d
	call EditorWindow_OnKeyDown_Handler
	jmp WndProc_ReturnZero

WndProc_OnChar:
	mov rcx, r10
	mov edx, r13d
	call EditorWindow_OnChar_Handler
	jmp WndProc_ReturnZero

WndProc_OnMouse:
	mov rcx, r10
	mov edx, r14d
	call EditorWindow_OnMouse_Handler
	jmp WndProc_ReturnZero

WndProc_OnSize:
	mov rcx, r10
	mov edx, r14d
	and edx, 0FFFFh
	mov r8d, r14d
	shr r8d, 16
	call EditorWindow_OnSize_Handler
	jmp WndProc_ReturnZero

WndProc_OnCommand:
	mov rcx, r10
	mov edx, r13d
	and edx, 0FFFFh
	call IDE_HandleCommand
	jmp WndProc_ReturnZero

WndProc_OnDestroy:
	mov rcx, r10
	call EditorWindow_OnDestroy_Handler
	jmp WndProc_ReturnZero

WndProc_Default:
	mov rcx, rbx
	mov edx, r12d
	mov r8, r13
	mov r9, r14
	call DefWindowProcA
	jmp WndProc_Exit

WndProc_ReturnZero:
	xor eax, eax

WndProc_Exit:
	add rsp, 40
	pop r14
	pop r13
	pop r12
	pop rbx
	ret
EditorWindow_WNDPROC ENDP


; ============================================================================
; Core editor state
; ============================================================================

EditorWindow_OnCreate_Handler PROC
	push rbx
	sub rsp, 32

	mov rbx, rcx

	xor ecx, ecx
	mov edx, 32
	call GlobalAlloc
	mov qword ptr [rbx + CTX_BUFFER], rax
	test rax, rax
	jz OnCreate_Done

	xor ecx, ecx
	mov edx, 8192
	call GlobalAlloc
	mov r10, [rbx + CTX_BUFFER]
	mov qword ptr [r10 + BUFFER_DATA], rax
	mov dword ptr [r10 + BUFFER_CAP], 8191
	mov dword ptr [r10 + BUFFER_USED], 0
	test rax, rax
	jz OnCreate_Done
	mov byte ptr [rax], 0

	xor ecx, ecx
	mov edx, 24
	call GlobalAlloc
	mov qword ptr [rbx + CTX_CURSOR], rax
	test rax, rax
	jz OnCreate_Done
	mov qword ptr [rax + CURSOR_OFFSET], 0
	mov qword ptr [rax + CURSOR_LINE], 0
	mov qword ptr [rax + CURSOR_COL], 0

OnCreate_Done:
	add rsp, 32
	pop rbx
	ret
EditorWindow_OnCreate_Handler ENDP


EditorWindow_OnPaint_Handler PROC
	push rbx
	sub rsp, 128

	mov rbx, rcx

	lea rdx, [rsp + 32]
	mov rcx, [rbx + CTX_HWND]
	test rcx, rcx
	jz OnPaint_Done
	call BeginPaintA
	mov qword ptr [rbx + CTX_HDC], rax

	mov dword ptr [rsp + 0], 0
	mov dword ptr [rsp + 4], 0
	mov eax, [rbx + CTX_WIDTH]
	mov dword ptr [rsp + 8], eax
	mov eax, [rbx + CTX_HEIGHT]
	mov dword ptr [rsp + 12], eax

	mov ecx, 0
	call GetStockObject

	mov rcx, [rbx + CTX_HDC]
	lea rdx, [rsp]
	mov r8, rax
	call FillRect

	mov rcx, rbx
	call EditorWindow_RenderDisplay

	mov rcx, [rbx + CTX_HWND]
	lea rdx, [rsp + 32]
	call EndPaintA

OnPaint_Done:
	add rsp, 128
	pop rbx
	ret
EditorWindow_OnPaint_Handler ENDP


EditorWindow_RenderDisplay PROC
	sub rsp, 40
	call RenderLineNumbers_Display
	call RenderTextContent_Display
	call RenderCursor_Display
	add rsp, 40
	ret
EditorWindow_RenderDisplay ENDP


RenderLineNumbers_Display PROC
	ret
RenderLineNumbers_Display ENDP


RenderTextContent_Display PROC
	ret
RenderTextContent_Display ENDP


RenderCursor_Display PROC
	ret
RenderCursor_Display ENDP


EditorWindow_OnKeyDown_Handler PROC
	push rbx
	push r12
	sub rsp, 40

	mov rbx, rcx
	mov r12, [rbx + CTX_CURSOR]
	test r12, r12
	jz KeyDone

	cmp edx, 37
	je KeyLeft
	cmp edx, 39
	je KeyRight
	cmp edx, 38
	je KeyUp
	cmp edx, 40
	je KeyDown
	cmp edx, 36
	je KeyHome
	cmp edx, 35
	je KeyEnd
	cmp edx, 8
	je KeyBackspace
	cmp edx, 46
	je KeyDelete
	jmp KeyDone

KeyLeft:
	mov eax, [r12 + CURSOR_COL]
	test eax, eax
	jz KeyDone
	dec dword ptr [r12 + CURSOR_COL]
	cmp dword ptr [r12 + CURSOR_OFFSET], 0
	jz KeyDone
	dec dword ptr [r12 + CURSOR_OFFSET]
	jmp KeyDone

KeyRight:
	inc dword ptr [r12 + CURSOR_COL]
	inc dword ptr [r12 + CURSOR_OFFSET]
	jmp KeyDone

KeyUp:
	cmp dword ptr [r12 + CURSOR_LINE], 0
	jz KeyDone
	dec dword ptr [r12 + CURSOR_LINE]
	jmp KeyDone

KeyDown:
	inc dword ptr [r12 + CURSOR_LINE]
	jmp KeyDone

KeyHome:
	mov dword ptr [r12 + CURSOR_COL], 0
	mov dword ptr [r12 + CURSOR_OFFSET], 0
	jmp KeyDone

KeyEnd:
	mov rax, [rbx + CTX_BUFFER]
	test rax, rax
	jz KeyDone
	mov edx, [rax + BUFFER_USED]
	mov [r12 + CURSOR_OFFSET], edx
	mov [r12 + CURSOR_COL], edx
	jmp KeyDone

KeyBackspace:
	mov rax, [rbx + CTX_BUFFER]
	test rax, rax
	jz KeyDone
	mov edx, [r12 + CURSOR_OFFSET]
	test edx, edx
	jz KeyDone
	dec edx
	mov rcx, rax
	call TextBuffer_DeleteChar_Impl
	test eax, eax
	jz KeyDone
	dec dword ptr [r12 + CURSOR_OFFSET]
	cmp dword ptr [r12 + CURSOR_COL], 0
	jz KeyDone
	dec dword ptr [r12 + CURSOR_COL]
	jmp KeyDone

KeyDelete:
	mov rax, [rbx + CTX_BUFFER]
	test rax, rax
	jz KeyDone
	mov edx, [r12 + CURSOR_OFFSET]
	mov rcx, rax
	call TextBuffer_DeleteChar_Impl

KeyDone:
	mov rcx, rbx
	call InvalidateContext_Impl
	add rsp, 40
	pop r12
	pop rbx
	ret
EditorWindow_OnKeyDown_Handler ENDP


EditorWindow_OnChar_Handler PROC
	push rbx
	push r12
	sub rsp, 40

	mov rbx, rcx
	mov r12d, edx

	cmp r12d, 20h
	jb CharDone
	cmp r12d, 7Fh
	jae CharDone

	mov r10, [rbx + CTX_CURSOR]
	mov r11, [rbx + CTX_BUFFER]
	test r10, r10
	jz CharDone
	test r11, r11
	jz CharDone

	mov rcx, r11
	mov edx, [r10 + CURSOR_OFFSET]
	mov r8b, r12b
	call TextBuffer_InsertChar_Impl
	test eax, eax
	jz CharDone

	inc dword ptr [r10 + CURSOR_OFFSET]
	inc dword ptr [r10 + CURSOR_COL]

	mov rcx, rbx
	call InvalidateContext_Impl

CharDone:
	add rsp, 40
	pop r12
	pop rbx
	ret
EditorWindow_OnChar_Handler ENDP


EditorWindow_OnMouse_Handler PROC
	push rbx
	sub rsp, 32

	mov rbx, rcx
	mov r10, [rbx + CTX_CURSOR]
	test r10, r10
	jz MouseDone

	mov eax, edx
	movzx r8d, ax
	shr eax, 16
	mov r9d, eax

	mov eax, r8d
	xor edx, edx
	mov ecx, [rbx + CTX_CHARW]
	test ecx, ecx
	jz MouseDone
	div ecx
	mov r8d, eax

	mov eax, r9d
	xor edx, edx
	mov ecx, [rbx + CTX_CHARH]
	test ecx, ecx
	jz MouseDone
	div ecx
	mov r9d, eax

	mov eax, r9d
	imul eax, 80
	add eax, r8d
	mov [r10 + CURSOR_OFFSET], eax
	mov [r10 + CURSOR_LINE], r9d
	mov [r10 + CURSOR_COL], r8d

	mov rcx, rbx
	call InvalidateContext_Impl

MouseDone:
	add rsp, 32
	pop rbx
	ret
EditorWindow_OnMouse_Handler ENDP


EditorWindow_OnSize_Handler PROC
	mov [rcx + CTX_WIDTH], edx
	mov [rcx + CTX_HEIGHT], r8d
	ret
EditorWindow_OnSize_Handler ENDP


EditorWindow_OnDestroy_Handler PROC
	push rbx
	sub rsp, 32

	mov rbx, rcx

	mov rax, [rbx + CTX_CURSOR]
	test rax, rax
	jz Destroy_NoCursor
	mov rcx, rax
	call GlobalFree

Destroy_NoCursor:
	mov rax, [rbx + CTX_BUFFER]
	test rax, rax
	jz Destroy_NoBuffer
	mov rcx, [rax + BUFFER_DATA]
	test rcx, rcx
	jz Destroy_FreeHeader
	call GlobalFree

Destroy_FreeHeader:
	mov rcx, [rbx + CTX_BUFFER]
	call GlobalFree

Destroy_NoBuffer:
	xor ecx, ecx
	call PostQuitMessage

	add rsp, 32
	pop rbx
	ret
EditorWindow_OnDestroy_Handler ENDP


; ============================================================================
; Text buffer helpers
; ============================================================================

TextBuffer_InsertChar_Impl PROC
	sub rsp, 40

	mov r9, [rcx + BUFFER_DATA]
	mov r10d, [rcx + BUFFER_CAP]
	mov r11d, [rcx + BUFFER_USED]

	cmp r11d, r10d
	jge InsertChar_Fail
	cmp edx, r11d
	jg InsertChar_Fail

	mov eax, r11d
InsertChar_Shift:
	cmp eax, edx
	jle InsertChar_Store
	mov r10b, byte ptr [r9 + rax - 1]
	mov byte ptr [r9 + rax], r10b
	dec eax
	jmp InsertChar_Shift

InsertChar_Store:
	mov byte ptr [r9 + rdx], r8b
	inc dword ptr [rcx + BUFFER_USED]
	mov eax, [rcx + BUFFER_USED]
	mov byte ptr [r9 + rax], 0
	mov eax, 1
	jmp InsertChar_Exit

InsertChar_Fail:
	xor eax, eax

InsertChar_Exit:
	add rsp, 40
	ret
TextBuffer_InsertChar_Impl ENDP


TextBuffer_DeleteChar_Impl PROC
	sub rsp, 40

	mov r9, [rcx + BUFFER_DATA]
	mov r10d, [rcx + BUFFER_USED]
	cmp edx, r10d
	jge DeleteChar_Fail

	mov eax, edx
DeleteChar_Shift:
	mov r11d, eax
	inc r11d
	cmp r11d, r10d
	jge DeleteChar_Done
	mov r8b, byte ptr [r9 + r11]
	mov byte ptr [r9 + rax], r8b
	inc eax
	jmp DeleteChar_Shift

DeleteChar_Done:
	dec dword ptr [rcx + BUFFER_USED]
	mov eax, [rcx + BUFFER_USED]
	mov byte ptr [r9 + rax], 0
	mov eax, 1
	jmp DeleteChar_Exit

DeleteChar_Fail:
	xor eax, eax

DeleteChar_Exit:
	add rsp, 40
	ret
TextBuffer_DeleteChar_Impl ENDP


; ============================================================================
; Integration helpers
; ============================================================================

InvalidateContext_Impl PROC
	sub rsp, 40
	mov rcx, [rcx + CTX_HWND]
	test rcx, rcx
	jz Invalidate_Exit
	xor edx, edx
	mov r8d, 1
	call InvalidateRect
Invalidate_Exit:
	add rsp, 40
	ret
InvalidateContext_Impl ENDP


EditorWindow_UpdateStatus PROC
	sub rsp, 40
	mov r8, [rcx + CTX_STATUS]
	test r8, r8
	jz UpdateStatus_Exit
	mov rcx, r8
	call SetWindowTextA
UpdateStatus_Exit:
	add rsp, 40
	ret
EditorWindow_UpdateStatus ENDP


IDE_CreateToolbar PROC
	push rbx
	sub rsp, 96
	mov rbx, rcx

	xor ecx, ecx
	lea rdx, [szStatic]
	lea r8, [szToolbar]
	mov r9d, 50000000h
	mov qword ptr [rsp + 32], 0
	mov qword ptr [rsp + 40], 0
	mov qword ptr [rsp + 48], 800
	mov qword ptr [rsp + 56], 24
	mov rax, [rbx + CTX_HWND]
	mov qword ptr [rsp + 64], rax
	mov qword ptr [rsp + 72], 0
	mov qword ptr [rsp + 80], 0
	mov qword ptr [rsp + 88], 0
	call CreateWindowExA
	mov [rbx + CTX_TOOLBAR], rax

	add rsp, 96
	pop rbx
	ret
IDE_CreateToolbar ENDP


IDE_CreateStatusBar PROC
	push rbx
	sub rsp, 96
	mov rbx, rcx

	xor ecx, ecx
	lea rdx, [szStatic]
	lea r8, [szReady]
	mov r9d, 50000000h
	mov qword ptr [rsp + 32], 0
	mov qword ptr [rsp + 40], 580
	mov qword ptr [rsp + 48], 800
	mov qword ptr [rsp + 56], 20
	mov rax, [rbx + CTX_HWND]
	mov qword ptr [rsp + 64], rax
	mov qword ptr [rsp + 72], 0
	mov qword ptr [rsp + 80], 0
	mov qword ptr [rsp + 88], 0
	call CreateWindowExA
	mov [rbx + CTX_STATUS], rax

	add rsp, 96
	pop rbx
	ret
IDE_CreateStatusBar ENDP


IDE_CreateMenu PROC
	push rbx
	push r12
	push r13
	sub rsp, 32

	mov rbx, rcx

	call CreateMenu
	mov r12, rax
	call CreateMenu
	mov r13, rax

	mov rcx, r13
	mov edx, MF_STRING_FLAG
	mov r8d, ID_FILE_NEW
	lea r9, [szNew]
	call AppendMenuA
	mov rcx, r13
	mov edx, MF_STRING_FLAG
	mov r8d, ID_FILE_OPEN
	lea r9, [szOpen]
	call AppendMenuA
	mov rcx, r13
	mov edx, MF_STRING_FLAG
	mov r8d, ID_FILE_SAVE
	lea r9, [szSave]
	call AppendMenuA
	mov rcx, r13
	mov edx, MF_STRING_FLAG
	mov r8d, ID_FILE_EXIT
	lea r9, [szExit]
	call AppendMenuA

	mov rcx, r12
	mov edx, MF_POPUP_FLAG
	mov r8, r13
	lea r9, [szFileMenu]
	call AppendMenuA

	call CreateMenu
	mov r13, rax
	mov rcx, r13
	mov edx, MF_STRING_FLAG
	mov r8d, ID_EDIT_UNDO
	lea r9, [szUndo]
	call AppendMenuA
	mov rcx, r13
	mov edx, MF_STRING_FLAG
	mov r8d, ID_EDIT_CUT
	lea r9, [szCut]
	call AppendMenuA
	mov rcx, r13
	mov edx, MF_STRING_FLAG
	mov r8d, ID_EDIT_COPY
	lea r9, [szCopy]
	call AppendMenuA
	mov rcx, r13
	mov edx, MF_STRING_FLAG
	mov r8d, ID_AI_COMPLETE
	lea r9, [szAIComplete]
	call AppendMenuA

	mov rcx, r12
	mov edx, MF_POPUP_FLAG
	mov r8, r13
	lea r9, [szEditMenu]
	call AppendMenuA

	mov rcx, [rbx + CTX_HWND]
	mov rdx, r12
	call SetMenu
	mov rcx, [rbx + CTX_HWND]
	call DrawMenuBar

	add rsp, 32
	pop r13
	pop r12
	pop rbx
	ret
IDE_CreateMenu ENDP


IDE_CreateMainWindow PROC
	push rbx
	push r12
	sub rsp, 40

	mov rbx, rcx
	mov r12, rdx

	call EditorWindow_RegisterClass
	mov rcx, r12
	mov rdx, rbx
	call EditorWindow_Create
	test rax, rax
	jz CreateMain_Fail

	mov rcx, r12
	call IDE_CreateToolbar
	mov rcx, r12
	call IDE_CreateMenu
	mov rcx, r12
	call IDE_CreateStatusBar
	mov rcx, r12
	lea rdx, [szReady]
	call EditorWindow_UpdateStatus

	mov rax, [r12 + CTX_HWND]
	jmp CreateMain_Exit

CreateMain_Fail:
	xor eax, eax

CreateMain_Exit:
	add rsp, 40
	pop r12
	pop rbx
	ret
IDE_CreateMainWindow ENDP


EditorWindow_CreateMenuBar PROC
	sub rsp, 40
	call IDE_CreateMenu
	add rsp, 40
	ret
EditorWindow_CreateMenuBar ENDP


EditorWindow_CreateToolbar PROC
	sub rsp, 40
	call IDE_CreateToolbar
	add rsp, 40
	ret
EditorWindow_CreateToolbar ENDP


EditorWindow_CreateStatusBar PROC
	sub rsp, 40
	call IDE_CreateStatusBar
	add rsp, 40
	ret
EditorWindow_CreateStatusBar ENDP


EditorWindow_OpenFile PROC
	xor eax, eax
	ret
EditorWindow_OpenFile ENDP


EditorWindow_SaveFile PROC
	xor eax, eax
	ret
EditorWindow_SaveFile ENDP


IDE_SetCommandHandlers PROC
	mov [g_CommandHandlers], rcx
	mov eax, 1
	ret
IDE_SetCommandHandlers ENDP


IDE_SetAIBackend PROC
	mov [g_AIBackendProc], rcx
	mov [g_AIBackendCtx], rdx
	mov eax, 1
	ret
IDE_SetAIBackend ENDP


AICompletion_GetBufferSnapshot PROC
	sub rsp, 40

	mov r8, [rcx + BUFFER_DATA]
	mov r9d, [rcx + BUFFER_USED]
	xor r10d, r10d

Snapshot_Loop:
	cmp r10d, r9d
	jge Snapshot_Done
	mov al, [r8 + r10]
	mov [rdx + r10], al
	inc r10d
	jmp Snapshot_Loop

Snapshot_Done:
	mov byte ptr [rdx + r10], 0
	mov eax, r10d
	add rsp, 40
	ret
AICompletion_GetBufferSnapshot ENDP


AICompletion_InsertTokens PROC
	push rbx
	push r12
	sub rsp, 40

	mov rbx, rcx
	mov r12, rdx
	mov r10, [rbx + CTX_BUFFER]
	mov r11, [rbx + CTX_CURSOR]
	xor r9d, r9d

	test r10, r10
	jz InsertTokens_Fail
	test r11, r11
	jz InsertTokens_Fail

InsertTokens_Loop:
	cmp r9d, r8d
	jge InsertTokens_Done
	mov rcx, r10
	mov edx, [r11 + CURSOR_OFFSET]
	mov al, [r12 + r9]
	mov r8b, al
	call TextBuffer_InsertChar_Impl
	test eax, eax
	jz InsertTokens_Fail
	inc dword ptr [r11 + CURSOR_OFFSET]
	cmp byte ptr [r12 + r9], 10
	jne InsertTokens_Col
	inc dword ptr [r11 + CURSOR_LINE]
	mov dword ptr [r11 + CURSOR_COL], 0
	jmp InsertTokens_Next

InsertTokens_Col:
	inc dword ptr [r11 + CURSOR_COL]

InsertTokens_Next:
	inc r9d
	jmp InsertTokens_Loop

InsertTokens_Done:
	mov rcx, rbx
	call InvalidateContext_Impl
	mov eax, 1
	jmp InsertTokens_Exit

InsertTokens_Fail:
	xor eax, eax

InsertTokens_Exit:
	add rsp, 40
	pop r12
	pop rbx
	ret
AICompletion_InsertTokens ENDP


IDE_RequestAICompletion PROC
	push rbx
	sub rsp, 40

	mov rbx, rcx
	mov qword ptr [rsp + 32], 0

	mov rax, [g_AIBackendProc]
	test rax, rax
	jz AIRequest_NoBackend

	mov rcx, [rbx + CTX_BUFFER]
	lea rdx, [g_AISnapshotBuffer]
	call AICompletion_GetBufferSnapshot

	mov rcx, [g_AIBackendCtx]
	lea rdx, [g_AISnapshotBuffer]
	mov r8d, eax
	lea r9, [rsp + 32]
	mov r10, [g_AIBackendProc]
	call r10
	test rax, rax
	jz AIRequest_None
	cmp dword ptr [rsp + 32], 0
	jle AIRequest_None

	mov rcx, rbx
	mov rdx, rax
	mov r8d, dword ptr [rsp + 32]
	call AICompletion_InsertTokens

	mov rcx, rbx
	lea rdx, [szAIInserted]
	call EditorWindow_UpdateStatus
	mov eax, 1
	jmp AIRequest_Exit

AIRequest_NoBackend:
	mov rcx, rbx
	lea rdx, [szAINotConfigured]
	call EditorWindow_UpdateStatus
	xor eax, eax
	jmp AIRequest_Exit

AIRequest_None:
	mov rcx, rbx
	lea rdx, [szAIUnavailable]
	call EditorWindow_UpdateStatus
	xor eax, eax

AIRequest_Exit:
	add rsp, 40
	pop rbx
	ret
IDE_RequestAICompletion ENDP


IDE_HandleCommand PROC
	push rbx
	push r12
	sub rsp, 40

	mov rbx, rcx
	mov r12d, edx

	mov rax, [g_CommandHandlers]
	test rax, rax
	jz Handle_Default

	cmp r12d, ID_FILE_NEW
	jne Handle_CheckOpen
	mov rax, [rax + 0]
	jmp Handle_InvokeMaybe

Handle_CheckOpen:
	cmp r12d, ID_FILE_OPEN
	jne Handle_CheckSave
	mov rax, [rax + 8]
	jmp Handle_InvokeMaybe

Handle_CheckSave:
	cmp r12d, ID_FILE_SAVE
	jne Handle_CheckExit
	mov rax, [rax + 16]
	jmp Handle_InvokeMaybe

Handle_CheckExit:
	cmp r12d, ID_FILE_EXIT
	jne Handle_CheckUndo
	mov rax, [rax + 24]
	jmp Handle_InvokeMaybe

Handle_CheckUndo:
	cmp r12d, ID_EDIT_UNDO
	jne Handle_CheckCut
	mov rax, [rax + 32]
	jmp Handle_InvokeMaybe

Handle_CheckCut:
	cmp r12d, ID_EDIT_CUT
	jne Handle_CheckCopy
	mov rax, [rax + 40]
	jmp Handle_InvokeMaybe

Handle_CheckCopy:
	cmp r12d, ID_EDIT_COPY
	jne Handle_CheckAI
	mov rax, [rax + 48]
	jmp Handle_InvokeMaybe

Handle_CheckAI:
	cmp r12d, ID_AI_COMPLETE
	jne Handle_Default
	mov rax, [rax + 56]

Handle_InvokeMaybe:
	test rax, rax
	jz Handle_Default
	mov rcx, rbx
	mov edx, r12d
	call rax
	test eax, eax
	jnz Handle_Exit

Handle_Default:
	cmp r12d, ID_FILE_NEW
	jne Handle_DefaultOpen
	mov rax, [rbx + CTX_BUFFER]
	test rax, rax
	jz Handle_ResetCursor
	mov rcx, [rax + BUFFER_DATA]
	test rcx, rcx
	jz Handle_ResetCursor
	mov byte ptr [rcx], 0
	mov dword ptr [rax + BUFFER_USED], 0

Handle_ResetCursor:
	mov rax, [rbx + CTX_CURSOR]
	test rax, rax
	jz Handle_NewDone
	mov dword ptr [rax + CURSOR_OFFSET], 0
	mov dword ptr [rax + CURSOR_LINE], 0
	mov dword ptr [rax + CURSOR_COL], 0

Handle_NewDone:
	mov rcx, rbx
	lea rdx, [szReady]
	call EditorWindow_UpdateStatus
	mov rcx, rbx
	call InvalidateContext_Impl
	jmp Handle_Exit

Handle_DefaultOpen:
	cmp r12d, ID_FILE_OPEN
	jne Handle_DefaultSave
	mov rcx, rbx
	lea rdx, [szNeedOpenHandler]
	call EditorWindow_UpdateStatus
	jmp Handle_Exit

Handle_DefaultSave:
	cmp r12d, ID_FILE_SAVE
	jne Handle_DefaultExit
	mov rcx, rbx
	lea rdx, [szNeedSaveHandler]
	call EditorWindow_UpdateStatus
	jmp Handle_Exit

Handle_DefaultExit:
	cmp r12d, ID_FILE_EXIT
	jne Handle_DefaultUndo
	mov rcx, [rbx + CTX_HWND]
	call DestroyWindow
	jmp Handle_Exit

Handle_DefaultUndo:
	cmp r12d, ID_EDIT_UNDO
	jne Handle_DefaultCut
	mov rcx, rbx
	lea rdx, [szNeedUndoHandler]
	call EditorWindow_UpdateStatus
	jmp Handle_Exit

Handle_DefaultCut:
	cmp r12d, ID_EDIT_CUT
	jne Handle_DefaultCopy
	mov rcx, rbx
	lea rdx, [szNeedCutHandler]
	call EditorWindow_UpdateStatus
	jmp Handle_Exit

Handle_DefaultCopy:
	cmp r12d, ID_EDIT_COPY
	jne Handle_DefaultAI
	mov rcx, rbx
	lea rdx, [szNeedCopyHandler]
	call EditorWindow_UpdateStatus
	jmp Handle_Exit

Handle_DefaultAI:
	cmp r12d, ID_AI_COMPLETE
	jne Handle_Exit
	mov rcx, rbx
	call IDE_RequestAICompletion

Handle_Exit:
	add rsp, 40
	pop r12
	pop rbx
	ret
IDE_HandleCommand ENDP


IDE_SetupAccelerators PROC
	sub rsp, 40
	lea rcx, [AccelTable]
	mov edx, 7
	call CreateAcceleratorTableA
	add rsp, 40
	ret
IDE_SetupAccelerators ENDP


IDE_MessageLoop PROC
	push rbx
	push r12
	sub rsp, 72

	mov rbx, rcx
	mov r12, rdx

MessageLoop_Next:
	lea rcx, [rsp + 32]
	xor edx, edx
	xor r8d, r8d
	xor r9d, r9d
	call GetMessageA
	test eax, eax
	jz MessageLoop_Exit

	mov rcx, rbx
	mov rdx, r12
	lea r8, [rsp + 32]
	call TranslateAcceleratorA
	test eax, eax
	jnz MessageLoop_Next

	lea rcx, [rsp + 32]
	call TranslateMessage
	lea rcx, [rsp + 32]
	call DispatchMessageA
	jmp MessageLoop_Next

MessageLoop_Exit:
	mov rax, [rsp + 48]
	add rsp, 72
	pop r12
	pop rbx
	ret
IDE_MessageLoop ENDP

END
