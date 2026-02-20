; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Agentic_Router.asm
; Semantic routing, tool selection, SIMD keyword matching
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
include RawrXD_Defs.inc

; ═══════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ═══════════════════════════════════════════════════════════════════════════════
KEYWORD_COUNT           EQU 8
MAX_KEYWORD_LEN         EQU 16

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═══════════════════════════════════════════════════════════════════════════════

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.DATA
align 16
; Keywords that trigger "Tool" usage instead of "Inference"
xzString_Find           BYTE "find", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
xzString_Search         BYTE "search", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
xzString_File           BYTE "file", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
xzString_Read           BYTE "read", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
xzString_Write          BYTE "write", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
xzString_Run            BYTE "run", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
xzString_Calc           BYTE "calc", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
xzString_Scan           BYTE "scan", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

g_Keywords              QWORD OFFSET xzString_Find
                        QWORD OFFSET xzString_Search
                        QWORD OFFSET xzString_File
                        QWORD OFFSET xzString_Read
                        QWORD OFFSET xzString_Write
                        QWORD OFFSET xzString_Run
                        QWORD OFFSET xzString_Calc
                        QWORD OFFSET xzString_Scan

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; AgentRouter_Initialize
; ═══════════════════════════════════════════════════════════════════════════════
AgentRouter_Initialize PROC FRAME
    .endprolog
    
    ; Initialize agent router: zero route table, set default capabilities
    push rbx
    push rsi
    sub rsp, 32
    
    ; Allocate route dispatch table: 16 agent slots * 64 bytes = 1KB
    xor ecx, ecx
    mov edx, 1024
    mov r8d, 3000h                   ; MEM_COMMIT | MEM_RESERVE
    mov r9d, 04h                     ; PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz @@ari_fail
    
    mov QWORD PTR [g_agent_route_table], rax
    mov DWORD PTR [g_agent_count], 0
    mov DWORD PTR [g_max_agents], 16
    mov DWORD PTR [g_router_initialized], 1
    
    ; Register default agent handlers from string table
    ; Each entry: agent_id(DWORD), capability_mask(DWORD), handler_ptr(QWORD), name_ptr(QWORD)
    ; Default: inference agent at slot 0
    mov DWORD PTR [rax], 0           ; agent_id = 0
    mov DWORD PTR [rax+4], 7         ; mask: inference|tool|chain
    lea rcx, [AgentRouter_ExecuteTask]
    mov QWORD PTR [rax+8], rcx       ; handler
    mov QWORD PTR [rax+16], 0        ; name
    mov DWORD PTR [g_agent_count], 1
    
    mov rax, 1                       ; success
    add rsp, 32
    pop rsi
    pop rbx
    ret
@@ari_fail:
    xor eax, eax
    add rsp, 32
    pop rsi
    pop rbx
    ret
AgentRouter_Initialize ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; AgentRouter_ExecuteTask
; RCX = Task Ptr (String), RDX = Context Ptr
; Returns: 0 = Inference, 1 = Tool, 2 = Complex Chain
; ═══════════════════════════════════════════════════════════════════════════════
AgentRouter_ExecuteTask PROC FRAME
    push rbx
    push rsi
    push rdi
    sub rsp, 32
    .endprolog
    
    test rcx, rcx
    jz @router_inference    ; Null task -> Inference default

    mov rsi, rcx            ; Task String
    
    ; Naive scan for keywords
    ; In a real implementation, we'd use SIMD string search (pcmpistri)
    
    xor rbx, rbx            ; Keyword Index
    
@keyword_loop:
    cmp rbx, KEYWORD_COUNT
    jge @router_inference
    
    lea rax, g_Keywords
    mov rdi, [rax + rbx*8]  ; Get Keyword Ptr
    
    ; Check if Task String contains Keyword
    ; Push volatile registers if calling unknown external, but here we inline or call local?
    ; Let's inline a simple "strstr" equivalent
    
    push rsi                ; Save start of string
    
    mov r8, rsi             ; r8 = haystack
    
@haystack_loop:
    mov al, [r8]
    test al, al
    jz @next_keyword_pop    ; End of haystack
    
    ; Compare needle (rdi) with haystack at r8
    mov r9, rdi             ; r9 = needle
    mov r10, r8             ; r10 = current haystack pos
    
@cmp_loop:
    mov al, [r9]
    test al, al
    jz @found_match         ; End of needle -> Match found!
    
    mov dl, [r10]
    cmp al, dl
    jne @next_haystack_char
    
    inc r9
    inc r10
    jmp @cmp_loop
    
@next_haystack_char:
    inc r8
    jmp @haystack_loop
    
@found_match:
    pop rsi
    mov rax, 1              ; TOOL FOUND
    jmp @router_exit

@next_keyword_pop:
    pop rsi
@next_keyword:
    inc rbx
    jmp @keyword_loop
    
@router_inference:
    xor rax, rax            ; INFERENCE
    
@router_exit:
    add rsp, 32
    pop rdi
    pop rsi
    pop rbx
    ret
AgentRouter_ExecuteTask ENDP

PUBLIC AgentRouter_Initialize
PUBLIC AgentRouter_ExecuteTask

END
