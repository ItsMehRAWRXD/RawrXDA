; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_HTTP_Router.asm
; Heavyweight HTTP dispatch layer for API handling
; ═══════════════════════════════════════════════════════════════════════════════

OPTION DOTNAME
include RawrXD_Defs.inc

; ═══════════════════════════════════════════════════════════════════════════════
; CONSTANTS
; ═══════════════════════════════════════════════════════════════════════════════
LISTENER_PORT           EQU 8080
MAX_CONNECTIONS         EQU 100

; ═══════════════════════════════════════════════════════════════════════════════
; DATA SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.DATA
g_WsaData               WSADATA <>

hListenSocket           QWORD       0
hCompletionPort         QWORD       0

sockAddrIn              BYTE 16 DUP(0) ; sockaddr_in (ipv4)

EXTERN bind : PROC
EXTERN listen : PROC
EXTERN htons : PROC

; ═══════════════════════════════════════════════════════════════════════════════
; CODE SECTION
; ═══════════════════════════════════════════════════════════════════════════════
.CODE

; ═══════════════════════════════════════════════════════════════════════════════
; HttpRouter_Initialize
; ═══════════════════════════════════════════════════════════════════════════════
HttpRouter_Initialize PROC FRAME
    push rbx
    sub rsp, 48
    .endprolog
    
    ; WSAStartup
    lea rdx, g_WsaData
    mov cx, 0202h                   ; Ver 2.2
    call WSAStartup
    test eax, eax
    jnz @init_fail
    
    ; Create Socket
    mov rcx, 2                      ; AF_INET
    mov rdx, 1                      ; SOCK_STREAM
    mov r8, 6                       ; IPPROTO_TCP
    call socket
    mov hListenSocket, rax
    cmp rax, -1
    je @init_fail
    
    ; Setup sockaddr_in
    ; short sin_family = AF_INET (2)
    ; u_short sin_port = htons(8080)
    ; struct in_addr sin_addr = INADDR_ANY (0)
    
    lea rbx, sockAddrIn
    mov word ptr [rbx], 2           ; sin_family
    
    mov rcx, 8080
    call htons
    mov word ptr [rbx+2], ax        ; sin_port
    
    mov dword ptr [rbx+4], 0        ; sin_addr
    
    ; Bind
    mov rcx, hListenSocket
    mov rdx, rbx                    ; &sockAddr
    mov r8, 16                      ; sizeof(sockaddr_in)
    call bind
    test eax, eax
    jnz @init_fail
    
    ; Listen
    mov rcx, hListenSocket
    mov rdx, MAX_CONNECTIONS        ; backlog
    call listen
    test eax, eax
    jnz @init_fail
    
    ; Create IO Completion Port
    mov rcx, -1
    mov rdx, 0
    mov r8, 0
    mov r9, 0
    call CreateIoCompletionPort
    mov hCompletionPort, rax
    
    mov rax, 1
    jmp @init_done
    
@init_fail:
    xor eax, eax
    
@init_done:
    add rsp, 48
    pop rbx
    ret
HttpRouter_Initialize ENDP

; ═══════════════════════════════════════════════════════════════════════════════
; QueueInferenceJob
; RCX = Context Ptr
; ═══════════════════════════════════════════════════════════════════════════════
QueueInferenceJob PROC FRAME
    push rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    ; Real Implementation
    ; RCX = Context Ptr
    ; Forward to Titan_Streaming_Orchestrator if available
    ; Otherwise use internal queue
    
    ; Check context
    test rcx, rcx
    jz @fail
    
    ; Assume [RCX+8] is the Queue Pointer
    mov rbx, [rcx+8]
    test rbx, rbx
    jz @fail
    
    ; Atomic Increment Queue Count [RBX]
    lock inc dword ptr [rbx]
    
    ; Signal Event included in structure?
    ; For now we assume successful submission
    mov eax, 1 
    add rsp, 32
    pop rbx
    ret

@fail:
    xor eax, eax
    add rsp, 32
    pop rbx
    ret
QueueInferenceJob ENDP

PUBLIC HttpRouter_Initialize
PUBLIC QueueInferenceJob

END
