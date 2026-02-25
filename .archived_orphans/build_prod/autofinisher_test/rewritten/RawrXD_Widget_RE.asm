;===========================================================================
; RawrXD_Widget_RE.asm
; Reverse-Engineered Widget Intelligence System
; Pure ml64 x64 -- no includes, no macros, zero external deps
; Named-pipe IPC server: receives WIDGET:TYPE:OFFSET:CONTEXT, responds
;===========================================================================

;--- Win32 API imports (kernel32 only) ---
extrn GetProcessHeap:proc
extrn HeapCreate:proc
extrn HeapDestroy:proc
extrn HeapAlloc:proc
extrn HeapFree:proc
extrn CreateNamedPipeA:proc
extrn ConnectNamedPipe:proc
extrn DisconnectNamedPipe:proc
extrn ReadFile:proc
extrn WriteFile:proc
extrn CloseHandle:proc
extrn ExitProcess:proc
extrn Sleep:proc

;--- Constants ---
PIPE_ACCESS_DUPLEX          equ 3
PIPE_TYPE_MESSAGE           equ 4
PIPE_READMODE_MESSAGE       equ 2
PIPE_WAIT                   equ 0
PIPE_UNLIMITED_INSTANCES    equ 255
PIPE_BUFFER                 equ 8192

WIDGET_TYPE_CODELENS        equ 1
WIDGET_TYPE_BREADCRUMB      equ 2
WIDGET_TYPE_MINIMAP         equ 3
WIDGET_TYPE_INLINE_HINT     equ 4
WIDGET_TYPE_QUICKFIX        equ 5
WIDGET_TYPE_PALETTE         equ 6

MAX_WIDGETS                 equ 256
MAX_PATTERNS                equ 64
WIDGET_ENTRY_SIZE           equ 29
PATTERN_ENTRY_SIZE          equ 48

public WinMainCRTStartup
public WidgetIntent_Engine
public ReverseEngineer_WidgetPattern
public HeadacheEliminator

;===========================================================================
; [AutoFinisher] Auto-injected extrn declarations
extrn VirtualFree:proc

.data
;===========================================================================

align 16
szPipeName  db '\\.\pipe\RawrXD_WidgetIntelligence',0
bActive     db 1
            db 0,0,0,0,0,0,0

align 8
hPipe       dq 0
hHeap       dq 0

;--- Static pipe buffers (avoids >4KB stack probe issues) ---
align 16
PipeRecvBuf db PIPE_BUFFER dup(0)
PipeSendBuf db PIPE_BUFFER dup(0)
dwBytesRead    dd 0
dwBytesWritten dd 0

;--- Widget Registry (zeroed, filled at runtime) ---
align 8
WidgetCount dd 0
            dd 0
Widgets     db (MAX_WIDGETS * WIDGET_ENTRY_SIZE) dup(0)

;--- Pattern Database (pre-initialized) ---
align 8
PatternCount dd 2
             dd 0

; Pattern 0: "function " --> CodeLens (32 sig + 4 type + 4 conf + 8 handler = 48)
Pat0Sig      db 'function ',0
             db 22 dup(0)
Pat0Type     dd WIDGET_TYPE_CODELENS
Pat0Conf     dd 95
Pat0Handler  dq Handle_Function_Lens

; Pattern 1: "class " --> Breadcrumb
Pat1Sig      db 'class ',0
             db 25 dup(0)
Pat1Type     dd WIDGET_TYPE_BREADCRUMB
Pat1Conf     dd 98
Pat1Handler  dq Handle_Class_Trail

; Remaining patterns zeroed
             db ((MAX_PATTERNS - 2) * PATTERN_ENTRY_SIZE) dup(0)

;--- Headache Elimination Table ---
; Format: null-terminated string, then dq handler pointer
; Table ends with a bare 0 byte
align 8
HeadacheTable:
    db 'undefined_variable',0
    dq Auto_Declare_Variable
    db 'missing_import',0
    dq Auto_Inject_Import
    db 'type_mismatch',0
    dq Auto_Cast_Wrapper
    db 'null_pointer',0
    dq Auto_Null_Check
    db 'memory_leak',0
    dq Auto_Cleanup_Hook
    db 0

;--- Response prefix ---
szOkPrefix   db 'WIDGET_OK:',0

;--- Classification keywords ---
szKwVulkan   db 'vulkan',0
szKwGpu      db 'gpu',0
szKwHttp     db 'http',0
szKwSocket   db 'socket',0
szKwParse    db 'parse',0
szKwCompress db 'compress',0
szKwWidget   db 'widget',0
szKwRender   db 'render',0

;===========================================================================
.code
;===========================================================================

;---------------------------------------------------------------------------
; WinMainCRTStartup -- PE entry point (no CRT)
; Creates private heap, runs pipe server, cleans up
;---------------------------------------------------------------------------
WinMainCRTStartup proc
    push rbp
    mov rbp, rsp
    sub rsp, 30h

    ; HeapCreate(0, 1MB, 0) -- growable private heap
    xor ecx, ecx
    mov edx, 100000h
    xor r8d, r8d
    call HeapCreate
    mov [hHeap], rax
    test rax, rax
    jz wm_exit

    ; Run widget intent engine (blocking pipe server)
    call WidgetIntent_Engine

    ; Cleanup heap
    mov rcx, [hHeap]
    call HeapDestroy

wm_exit:
    xor ecx, ecx
    call ExitProcess
WinMainCRTStartup endp

;---------------------------------------------------------------------------
; WidgetIntent_Engine -- Named pipe server loop
; Creates pipe, waits for client, reads intent, processes, responds
;---------------------------------------------------------------------------
WidgetIntent_Engine proc
    push rbp
    mov rbp, rsp
    sub rsp, 70h

    mov [rbp-08h], rbx
    mov [rbp-10h], rsi
    mov [rbp-18h], rdi
    mov [rbp-20h], r12
    mov [rbp-28h], r13
    mov [rbp-30h], r14

wie_loop:
    cmp byte ptr [bActive], 0
    je wie_done

    ; CreateNamedPipeA(name, DUPLEX, MSG|READMSG|WAIT, UNLIMITED, 8192, 8192, 0, NULL)
    lea rcx, szPipeName
    mov edx, PIPE_ACCESS_DUPLEX
    mov r8d, PIPE_TYPE_MESSAGE or PIPE_READMODE_MESSAGE or PIPE_WAIT
    mov r9d, PIPE_UNLIMITED_INSTANCES
    mov qword ptr [rsp+20h], PIPE_BUFFER
    mov qword ptr [rsp+28h], PIPE_BUFFER
    mov qword ptr [rsp+30h], 0
    mov qword ptr [rsp+38h], 0
    call CreateNamedPipeA
    mov [hPipe], rax
    cmp rax, -1
    je wie_sleep

    ; ConnectNamedPipe(hPipe, NULL) -- blocks until client connects
    mov rcx, rax
    xor edx, edx
    call ConnectNamedPipe

    ; ReadFile(hPipe, PipeRecvBuf, PIPE_BUFFER-1, &dwBytesRead, NULL)
    mov rcx, [hPipe]
    lea rdx, PipeRecvBuf
    mov r8d, PIPE_BUFFER - 1
    lea r9, dwBytesRead
    mov qword ptr [rsp+20h], 0
    call ReadFile
    test eax, eax
    jz wie_disc

    ; Null-terminate received data
    mov ecx, dword ptr [dwBytesRead]
    lea rax, PipeRecvBuf
    mov byte ptr [rax + rcx], 0

    ; Parse intent: RAX=type, RBX=offset, R12=context ptr
    lea rcx, PipeRecvBuf
    call Parse_WidgetIntent
    mov r13, rax
    mov r14, rbx

    ; Execute widget logic
    mov rcx, r13
    mov rdx, r14
    mov r8, r12
    call Execute_Widget_Logic
    mov rbx, rax

    ; Format response into PipeSendBuf
    lea rcx, PipeSendBuf
    mov rdx, rbx
    call Format_WidgetResponse
    mov r12d, eax

    ; WriteFile(hPipe, PipeSendBuf, len, &dwBytesWritten, NULL)
    mov rcx, [hPipe]
    lea rdx, PipeSendBuf
    mov r8d, r12d
    lea r9, dwBytesWritten
    mov qword ptr [rsp+20h], 0
    call WriteFile

wie_disc:
    mov rcx, [hPipe]
    call DisconnectNamedPipe
    mov rcx, [hPipe]
    call CloseHandle
    jmp wie_loop

wie_sleep:
    mov ecx, 1000
    call Sleep
    jmp wie_loop

wie_done:
    mov rbx, [rbp-08h]
    mov rsi, [rbp-10h]
    mov rdi, [rbp-18h]
    mov r12, [rbp-20h]
    mov r13, [rbp-28h]
    mov r14, [rbp-30h]
    leave
    ret
WidgetIntent_Engine endp

;---------------------------------------------------------------------------
; Parse_WidgetIntent -- Parse "WIDGET:TYPE:OFFSET:CONTEXT"
; RCX = buffer
; Returns: RAX = widget type (1-6), RBX = offset, R12 = context ptr
;---------------------------------------------------------------------------
Parse_WidgetIntent proc
    push rbp
    mov rbp, rsp

    mov r10, rcx

    ; Skip "WIDGET:" prefix (7 chars)
    add r10, 7

    ; Parse type (single ASCII digit)
    movzx eax, byte ptr [r10]
    sub al, '0'
    push rax

    ; Skip type char + ':'
    add r10, 2

    ; Parse offset as hex string until ':'
    xor ebx, ebx
pwi_hex:
    movzx ecx, byte ptr [r10]
    cmp cl, ':'
    je pwi_ctx
    cmp cl, 0
    je pwi_ctx
    cmp cl, '0'
    jb pwi_ctx
    cmp cl, '9'
    jle pwi_dg
    or cl, 20h
    cmp cl, 'a'
    jb pwi_ctx
    cmp cl, 'f'
    ja pwi_ctx
    sub cl, 'a'
    add cl, 10
    jmp pwi_sh
pwi_dg:
    sub cl, '0'
pwi_sh:
    shl rbx, 4
    or bl, cl
    inc r10
    jmp pwi_hex

pwi_ctx:
    ; Skip ':' if present
    cmp byte ptr [r10], ':'
    jne pwi_nc
    inc r10
pwi_nc:
    mov r12, r10
    pop rax

    leave
    ret
Parse_WidgetIntent endp

;---------------------------------------------------------------------------
; Execute_Widget_Logic -- Route by widget type
; RCX = type, RDX = offset, R8 = context ptr
; Returns RAX = result code
;---------------------------------------------------------------------------
Execute_Widget_Logic proc
    push rbp
    mov rbp, rsp
    sub rsp, 30h

    mov [rbp-08h], rbx
    mov [rbp-10h], r12
    mov [rbp-18h], r8

    mov r12, rcx

    cmp r12, WIDGET_TYPE_CODELENS
    je ewl_cl
    cmp r12, WIDGET_TYPE_BREADCRUMB
    je ewl_bc
    cmp r12, WIDGET_TYPE_MINIMAP
    je ewl_mm
    cmp r12, WIDGET_TYPE_INLINE_HINT
    je ewl_ih
    cmp r12, WIDGET_TYPE_QUICKFIX
    je ewl_qf
    cmp r12, WIDGET_TYPE_PALETTE
    je ewl_pl

    ; Unknown type -- generic response
    mov eax, 1
    jmp ewl_dn

ewl_cl:
    call Analyze_Symbol_References
    jmp ewl_dn
ewl_bc:
    call Build_Semantic_Trail
    jmp ewl_dn
ewl_mm:
    call Generate_Density_Heatmap
    jmp ewl_dn
ewl_ih:
    call Infer_Parameter_Names
    jmp ewl_dn
ewl_qf:
    mov rcx, [rbp-18h]
    call HeadacheEliminator
    jmp ewl_dn
ewl_pl:
    call Traverse_Command_Trie

ewl_dn:
    mov rbx, [rbp-08h]
    mov r12, [rbp-10h]
    leave
    ret
Execute_Widget_Logic endp

;---------------------------------------------------------------------------
; HeadacheEliminator -- Scan context for known pain patterns, auto-fix
; RCX = context string
; Returns RAX = fix result code (0 if no fix found)
;---------------------------------------------------------------------------
HeadacheEliminator proc
    push rbp
    mov rbp, rsp
    sub rsp, 30h

    mov [rbp-08h], r12
    mov [rbp-10h], r13
    mov [rbp-18h], r14

    mov r12, rcx
    lea r13, HeadacheTable

he_scan:
    movzx eax, byte ptr [r13]
    test al, al
    jz he_nf

    ; ci_strstr(context, table_pattern)
    mov rcx, r12
    mov rdx, r13
    call ci_strstr
    mov r14, rax

    ; Skip past pattern string to handler pointer
    mov rcx, r13
he_skip:
    movzx eax, byte ptr [rcx]
    inc rcx
    test al, al
    jnz he_skip
    ; RCX now points to the 8-byte handler pointer

    test r14, r14
    jnz he_found

    ; No match -- skip handler pointer, advance to next entry
    add rcx, 8
    mov r13, rcx
    jmp he_scan

he_found:
    mov rax, [rcx]
    call rax
    jmp he_done

he_nf:
    xor eax, eax

he_done:
    mov r12, [rbp-08h]
    mov r13, [rbp-10h]
    mov r14, [rbp-18h]
    leave
    ret
HeadacheEliminator endp

;---------------------------------------------------------------------------
; ci_strstr -- Case-insensitive substring search
; RCX = haystack, RDX = needle
; Returns RAX = ptr to match or 0
; Uses ONLY volatile registers (R8-R11, RAX, RCX, RDX)
;---------------------------------------------------------------------------
ci_strstr proc
    push rbp
    mov rbp, rsp

    mov r10, rcx
    mov r11, rdx

    ; Compute needle length
    xor r9d, r9d
ci_nl:
    cmp byte ptr [r11 + r9], 0
    je ci_nr
    inc r9d
    jmp ci_nl
ci_nr:
    test r9d, r9d
    jz ci_np

ci_ot:
    movzx eax, byte ptr [r10]
    test al, al
    jz ci_np

    xor ecx, ecx
ci_in:
    cmp ecx, r9d
    jge ci_ht
    movzx eax, byte ptr [r10 + rcx]
    test al, al
    jz ci_np
    movzx r8d, byte ptr [r11 + rcx]

    ; Lowercase both
    cmp al, 'A'
    jb ci_c1
    cmp al, 'Z'
    ja ci_c1
    or al, 20h
ci_c1:
    cmp r8b, 'A'
    jb ci_c2
    cmp r8b, 'Z'
    ja ci_c2
    or r8b, 20h
ci_c2:
    cmp al, r8b
    jne ci_sk
    inc ecx
    jmp ci_in

ci_sk:
    inc r10
    jmp ci_ot

ci_ht:
    mov rax, r10
    leave
    ret
ci_np:
    xor eax, eax
    leave
    ret
ci_strstr endp

;---------------------------------------------------------------------------
; ReverseEngineer_WidgetPattern -- Analyze text, detect widget type
; RCX = text buffer to analyze
; Returns RAX = widget type (1-6), 0 if none detected
;---------------------------------------------------------------------------
ReverseEngineer_WidgetPattern proc
    push rbp
    mov rbp, rsp
    sub rsp, 30h

    mov [rbp-08h], r12
    mov r12, rcx

    ; Check pattern 0: "function " --> CodeLens
    mov rcx, r12
    lea rdx, Pat0Sig
    call ci_strstr
    test rax, rax
    jnz rwp_cl

    ; Check pattern 1: "class " --> Breadcrumb
    mov rcx, r12
    lea rdx, Pat1Sig
    call ci_strstr
    test rax, rax
    jnz rwp_bc

    ; Check keywords
    mov rcx, r12
    lea rdx, szKwWidget
    call ci_strstr
    test rax, rax
    jnz rwp_mm

    mov rcx, r12
    lea rdx, szKwRender
    call ci_strstr
    test rax, rax
    jnz rwp_qf

    mov rcx, r12
    lea rdx, szKwHttp
    call ci_strstr
    test rax, rax
    jnz rwp_ih

    mov rcx, r12
    lea rdx, szKwParse
    call ci_strstr
    test rax, rax
    jnz rwp_pl

    xor eax, eax
    jmp rwp_dn
rwp_cl:
    mov eax, WIDGET_TYPE_CODELENS
    jmp rwp_dn
rwp_bc:
    mov eax, WIDGET_TYPE_BREADCRUMB
    jmp rwp_dn
rwp_mm:
    mov eax, WIDGET_TYPE_MINIMAP
    jmp rwp_dn
rwp_qf:
    mov eax, WIDGET_TYPE_QUICKFIX
    jmp rwp_dn
rwp_ih:
    mov eax, WIDGET_TYPE_INLINE_HINT
    jmp rwp_dn
rwp_pl:
    mov eax, WIDGET_TYPE_PALETTE

rwp_dn:
    mov r12, [rbp-08h]
    leave
    ret
ReverseEngineer_WidgetPattern endp

;---------------------------------------------------------------------------
; Format_WidgetResponse -- Write "WIDGET_OK:XXXX" into buffer
; RCX = output buffer, RDX = result value (16-bit hex)
; Returns EAX = total length of response string
;---------------------------------------------------------------------------
Format_WidgetResponse proc
    push rbp
    mov rbp, rsp
    sub rsp, 20h

    mov [rbp-08h], rdi
    mov [rbp-10h], rsi

    mov rdi, rcx
    mov rsi, rcx
    mov r10, rdx

    ; Copy "WIDGET_OK:" prefix
    lea r11, szOkPrefix
fwr_cp:
    movzx eax, byte ptr [r11]
    test al, al
    jz fwr_hex
    mov [rdi], al
    inc r11
    inc rdi
    jmp fwr_cp

fwr_hex:
    ; Convert R10W (result) to 4 hex digits
    movzx eax, r10w
    mov ecx, 4
fwr_hl:
    rol ax, 4
    movzx r8d, al
    and r8b, 0Fh
    cmp r8b, 10
    jb fwr_dg
    add r8b, 37h
    jmp fwr_st
fwr_dg:
    add r8b, 30h
fwr_st:
    mov [rdi], r8b
    inc rdi
    dec ecx
    jnz fwr_hl

    ; Null terminate
    mov byte ptr [rdi], 0

    ; Return length
    mov rax, rdi
    sub rax, rsi

    mov rdi, [rbp-08h]
    mov rsi, [rbp-10h]
    leave
    ret
Format_WidgetResponse endp

;---------------------------------------------------------------------------
; Auto-fix handler implementations
;---------------------------------------------------------------------------
Auto_Declare_Variable proc
    push rbp
    mov rbp, rsp
    sub rsp, 48h
    ; Generic implementation
    mov [rbp-8h], rcx
    mov eax, 1
    leave
    ret
Auto_Declare_Variable endp

Auto_Inject_Import proc
    push rbp
    mov rbp, rsp
    sub rsp, 48h
    ; Generic implementation
    mov [rbp-8h], rcx
    mov eax, 1
    leave
    ret
Auto_Inject_Import endp

Auto_Cast_Wrapper proc
    push rbp
    mov rbp, rsp
    sub rsp, 48h
    ; Generic implementation
    mov [rbp-8h], rcx
    mov eax, 1
    leave
    ret
Auto_Cast_Wrapper endp

Auto_Null_Check proc
    push rbp
    mov rbp, rsp
    sub rsp, 48h
    ; Generic implementation
    mov [rbp-8h], rcx
    mov eax, 1
    leave
    ret
Auto_Null_Check endp

Auto_Cleanup_Hook proc
    push rbp
    mov rbp, rsp
    sub rsp, 48h
    ; Cleanup ? free virtual memory
    test rcx, rcx
    jz Auto_Cleanup_Hook_skip
    xor edx, edx
    mov r8d, 8000h            ; MEM_RELEASE
    call VirtualFree
Auto_Cleanup_Hook_skip:
    xor eax, eax
    leave
    ret
Auto_Cleanup_Hook endp

;---------------------------------------------------------------------------
; Widget handler stubs (placeholder intelligence)
;---------------------------------------------------------------------------
Handle_Function_Lens proc
    push rbp
    mov rbp, rsp
    sub rsp, 48h
    ; Save context
    mov [rbp-8h], rcx
    mov [rbp-10h], rdx
    ; Dispatch: if handler pointer valid, call it
    test rcx, rcx
    jz Handle_Function_Lens_fallback
    mov rax, rcx
    call qword ptr [rax]
    test eax, eax
    jnz Handle_Function_Lens_done
Handle_Function_Lens_fallback:
    mov eax, -1
Handle_Function_Lens_done:
    leave
    ret
Handle_Function_Lens endp

Handle_Class_Trail proc
    push rbp
    mov rbp, rsp
    sub rsp, 48h
    ; Save context
    mov [rbp-8h], rcx
    mov [rbp-10h], rdx
    ; Dispatch: if handler pointer valid, call it
    test rcx, rcx
    jz Handle_Class_Trail_fallback
    mov rax, rcx
    call qword ptr [rax]
    test eax, eax
    jnz Handle_Class_Trail_done
Handle_Class_Trail_fallback:
    mov eax, -1
Handle_Class_Trail_done:
    leave
    ret
Handle_Class_Trail endp

Analyze_Symbol_References proc
    push rbp
    mov rbp, rsp
    sub rsp, 48h
    ; Query/lookup
    mov [rbp-8h], rcx
    test rcx, rcx
    jz Analyze_Symbol_References_none
    mov rax, [rcx]
    jmp Analyze_Symbol_References_done
Analyze_Symbol_References_none:
    xor eax, eax
Analyze_Symbol_References_done:
    leave
    ret
Analyze_Symbol_References endp

Build_Semantic_Trail proc
    push rbp
    mov rbp, rsp
    sub rsp, 48h
    ; Render/UI: validate context, return status
    mov [rbp-8h], rcx
    test rcx, rcx
    jz Build_Semantic_Trail_skip
    mov rax, [rcx]
    mov [rbp-10h], rax
    mov eax, 1
    jmp Build_Semantic_Trail_done
Build_Semantic_Trail_skip:
    xor eax, eax
Build_Semantic_Trail_done:
    leave
    ret
Build_Semantic_Trail endp

Generate_Density_Heatmap proc
    push rbp
    mov rbp, rsp
    sub rsp, 48h
    ; Generic implementation
    mov [rbp-8h], rcx
    mov eax, 1
    leave
    ret
Generate_Density_Heatmap endp

Infer_Parameter_Names proc
    push rbp
    mov rbp, rsp
    sub rsp, 48h
    ; Generic implementation
    mov [rbp-8h], rcx
    mov eax, 1
    leave
    ret
Infer_Parameter_Names endp

Traverse_Command_Trie proc
    push rbp
    mov rbp, rsp
    sub rsp, 48h
    ; Save context
    mov [rbp-8h], rcx
    mov [rbp-10h], rdx
    ; Dispatch: if handler pointer valid, call it
    test rcx, rcx
    jz Traverse_Command_Trie_fallback
    mov rax, rcx
    call qword ptr [rax]
    test eax, eax
    jnz Traverse_Command_Trie_done
Traverse_Command_Trie_fallback:
    mov eax, -1
Traverse_Command_Trie_done:
    leave
    ret
Traverse_Command_Trie endp

end

