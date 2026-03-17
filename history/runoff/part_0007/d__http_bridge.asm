;==============================================================================
; RawrXD HTTP Bridge Server - Pure MASM64
; Parses HTTP requests, dispatches to RunLocalModel in model_bridge.dll,
; and returns JSON with CORS + Content-Length, no keep-alive.
;==============================================================================

OPTION CASEMAP:NONE
; OPTION WIN64:3  <-- Removed

INCLUDE masm64_structs.inc
; INCLUDE \masm64\include64\windows.inc  <-- Removed dependency
; INCLUDE \masm64\include64\kernel32.inc <-- Removed dependency
; INCLUDE \masm64\include64\ws2_32.inc   <-- Removed dependency

; INCLUDELIB kernel32.lib
; INCLUDELIB ws2_32.lib

.DATA
wPort               WORD 8080
szDllName           BYTE "model_bridge.dll",0
szRunFn             BYTE "RunLocalModel",0
szResp200           BYTE "HTTP/1.1 200 OK",13,10,
                        "Content-Type: application/json",13,10,
                        "Access-Control-Allow-Origin: *",13,10,
                        "Connection: close",13,10,
                        "Content-Length: ",0
szResp404           BYTE "HTTP/1.1 404 Not Found",13,10,
                        "Content-Type: application/json",13,10,
                        "Access-Control-Allow-Origin: *",13,10,
                        "Connection: close",13,10,
                        "Content-Length: ",0
szResp500           BYTE "HTTP/1.1 500 Internal Server Error",13,10,
                        "Content-Type: application/json",13,10,
                        "Access-Control-Allow-Origin: *",13,10,
                        "Connection: close",13,10,
                        "Content-Length: ",0
szCRLF              BYTE 13,10,0
szCRLFCRLF          BYTE 13,10,13,10,0
szHealthBody        BYTE '{"status":"ok","service":"RawrXD HTTP bridge"}',0
szErrBody           BYTE '{"error":"bridge failed"}',0
szNotFoundBody      BYTE '{"error":"not found"}',0
szEndpointCompletion BYTE "http://127.0.0.1:8080/completion",0
szEndpointGenerate   BYTE "http://127.0.0.1:8080/generate",0
szEndpointChat       BYTE "http://127.0.0.1:8080/v1/chat/completions",0
szPathCompletion     BYTE "/completion",0
szPathGenerate       BYTE "/generate",0
szPathChat           BYTE "/v1/chat/completions",0
szPathHealth         BYTE "/health",0

.DATA?
requestBuf          BYTE 65536 dup(?)
responseBuf         BYTE 65536 dup(?)
modelOutBuf         BYTE 131072 dup(?)
hDll                QWORD ?
pRunLocalModel      QWORD ?

.CODE

;------------------------------------------------------------------------------
; htons (rcx=val) -> rax=val
;------------------------------------------------------------------------------
htons PROC
    mov eax, ecx
    xchg al, ah
    ret
htons ENDP

;------------------------------------------------------------------------------
; strlen (rcx=str) -> rax=len
;------------------------------------------------------------------------------
strlen_ PROC
    mov rax, rcx
@@sloop:
    mov dl, [rax]
    test dl, dl
    jz @@sdone
    inc rax
    jmp @@sloop
@@sdone:
    sub rax, rcx
    ret
strlen_ ENDP

;------------------------------------------------------------------------------
; memcpy (rcx=dst, rdx=src, r8=len) -> rax=dst
;------------------------------------------------------------------------------
memcpy_ PROC
    push rsi
    push rdi
    mov rdi, rcx
    mov rsi, rdx
    mov rcx, r8
    rep movsb
    mov rax, rdi 
    pop rdi
    pop rsi
    ret
memcpy_ ENDP

;------------------------------------------------------------------------------
; itoa10_fixed (rcx=val, rdx=buf) -> rax=chars_written
;------------------------------------------------------------------------------
itoa10_fixed PROC ; rcx=val, rdx=buf
    mov r10, rdx ; save buf ptr
    mov eax, ecx
    mov rdi, rdx
    mov ecx, 0
    mov ebx, 10
    cmp eax, 0
    jne @@loop
    mov byte ptr [rdi], '0'
    mov byte ptr [rdi+1], 0
    mov rax, 1
    ret
@@loop:
    xor edx, edx
    div ebx
    add dl, '0'
    push rdx
    inc ecx
    test eax, eax
    jnz @@loop
@@emit:
    pop rdx
    mov [rdi], dl
    inc rdi
    dec ecx
    jnz @@emit
    mov byte ptr [rdi], 0
    
    mov rax, rdi
    sub rax, r10 ; len
    ret
itoa10_fixed ENDP

;------------------------------------------------------------------------------
; starts_with (rcx=pStr, rdx=pPat) -> rax=1/0
;------------------------------------------------------------------------------
starts_with PROC
    push rsi
    push rdi
    mov rsi, rcx
    mov rdi, rdx
@@sw_loop:
    mov al, [rdi]
    test al, al
    jz @@sw_yes
    cmp al, [rsi]
    jne @@sw_no
    inc rsi
    inc rdi
    jmp @@sw_loop
@@sw_yes:
    mov eax, 1
    jmp @@sw_exit
@@sw_no:
    xor eax, eax
@@sw_exit:
    pop rdi
    pop rsi
    ret
starts_with ENDP

;------------------------------------------------------------------------------
; find_crlfcrlf (rcx=pBuf, rdx=len) -> rax=offset (or 0)
;------------------------------------------------------------------------------
find_crlfcrlf PROC
    push rsi
    mov rsi, rcx
    mov ecx, edx ; len
    cmp ecx, 4
    jb @@nf
    mov r8, rsi  ; save start
@@search:
    cmp dword ptr [rsi], 0A0D0A0Dh
    je @@found
    inc rsi
    dec ecx
    cmp ecx, 4
    jae @@search
@@nf:
    xor eax, eax
    pop rsi
    ret
@@found:
    lea rax, [rsi+4]
    sub rax, r8
    pop rsi
    ret
find_crlfcrlf ENDP

;------------------------------------------------------------------------------
; parse_content_length (rcx=pBuf, rdx=len) -> rax=val
;------------------------------------------------------------------------------
parse_content_length PROC
    push rsi
    push rdi
    mov rsi, rcx
    mov ecx, edx ; len
@@cl_loop:
    cmp ecx, 15
    jb @@cl_done
    mov eax, dword ptr [rsi]
    cmp eax, 'netC'            ; "Cont"
    jne @@cl_adv
    cmp dword ptr [rsi+4], 'gneL'
    jne @@cl_adv
    cmp dword ptr [rsi+8], 'ht: '
    jne @@cl_adv
    ; digits
    mov rdi, rsi
    add rdi, 12
    xor eax, eax
    xor r8d, r8d ; scratch
@@cl_digits:
    mov dl, [rdi]
    cmp dl, '0'
    jb @@cl_store
    cmp dl, '9'
    ja @@cl_store
    imul eax, eax, 10
    sub dl, '0'
    movzx r8d, dl
    add eax, r8d
    inc rdi
    jmp @@cl_digits
@@cl_store:
    pop rdi
    pop rsi
    ret
@@cl_adv:
    inc rsi
    dec ecx
    jmp @@cl_loop
@@cl_done:
    xor eax, eax
    pop rdi
    pop rsi
    ret
parse_content_length ENDP

;------------------------------------------------------------------------------
; match_path (rcx=pBuf) -> rax=code
;------------------------------------------------------------------------------
match_path PROC
    push rsi
    mov rsi, rcx
@@skip_method:
    mov al, [rsi]
    test al, al
    jz @@no
    cmp al, ' '
    je @@path
    inc rsi
    jmp @@skip_method
@@path:
    inc rsi
    cmp byte ptr [rsi], '/'
    jne @@no
    
    ; preserve rsi? starts_with takes rcx, rdx
    mov rcx, rsi
    lea rdx, szPathCompletion
    call starts_with
    test eax, eax
    jnz @@is_completion
    
    mov rcx, rsi
    lea rdx, szPathGenerate
    call starts_with
    test eax, eax
    jnz @@is_generate
    
    mov rcx, rsi
    lea rdx, szPathChat
    call starts_with
    test eax, eax
    jnz @@is_chat

    mov rcx, rsi
    lea rdx, szPathHealth
    call starts_with
    test eax, eax
    jnz @@is_health
    
    jmp @@no

@@is_completion:
    mov eax, 1
    jmp @@exit
@@is_generate:
    mov eax, 2
    jmp @@exit
@@is_chat:
    mov eax, 3
    jmp @@exit
@@is_health:
    mov eax, 4
    jmp @@exit
@@no:
    xor eax, eax
@@exit:
    pop rsi
    ret
match_path ENDP

;------------------------------------------------------------------------------
; build_response (rcx=pHdr, rdx=pBody, r8=pOutLen) -> fills responseBuf
;------------------------------------------------------------------------------
build_response PROC
    push rsi
    push rdi
    push rbx
    sub rsp, 32 ; shadow space

    mov rsi, rcx ; hdr
    mov rbx, rdx ; body
    mov r12, r8  ; outLen ptr (using r12 as callee saved)

    ; strlen body
    mov rcx, rbx
    call strlen_
    mov r13d, eax ; lenBody, r13 callee saved

    lea rdi, responseBuf
    ; copy hdr
@@cpy_hdr:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz @@cpy_hdr
    dec rdi ; back up over null

    ; write content-length
    mov ecx, r13d
    mov rdx, rdi
    call itoa10_fixed
    add rdi, rax

    ; CRLFCRLF
    mov word ptr [rdi], 0A0Dh
    add rdi, 2
    mov word ptr [rdi], 0A0Dh
    add rdi, 2

    ; body
    mov rsi, rbx
@@cpy_body:
    mov al, [rsi]
    mov [rdi], al
    inc rsi
    inc rdi
    test al, al
    jnz @@cpy_body
    dec rdi

    lea rax, responseBuf
    sub rdi, rax
    mov [r12], edi ; *pOutLen = len

    add rsp, 32
    pop rbx
    pop rdi
    pop rsi
    ret
build_response ENDP

;------------------------------------------------------------------------------
; load_bridge_if_needed
;------------------------------------------------------------------------------
load_bridge_if_needed PROC
    sub rsp, 40 ; shadow
    cmp hDll, 0
    jne @@done
    lea rcx, szDllName
    call LoadLibraryA
    mov hDll, rax
    test rax, rax
    jz @@done
    mov rcx, rax
    lea rdx, szRunFn
    call GetProcAddress
    mov pRunLocalModel, rax
@@done:
    add rsp, 40
    ret
load_bridge_if_needed ENDP

;------------------------------------------------------------------------------
; main
;------------------------------------------------------------------------------
main PROC
    ; Stack frame:
    ; [rbp-400] wsaData (lets say 400 bytes)
    ; [rbp-416] sa (16)
    ; [rbp-424] hListen
    ; [rbp-432] hClient
    ; [rbp-440] recvLen (4) + pad
    ; [rbp-448] bodyOff (8)
    ; [rbp-456] bodyLen (4) + pad
    ; [rbp-464] pathCode (4) + pad
    ; [rbp-472] respLen (4) + pad
    ; Total 480 bytes. Align to 512.
    
    push rbp
    mov rbp, rsp
    sub rsp, 512

    ; Winsock startup
    lea rcx, [rbp-400] ; wsaData
    mov edx, 0202h
    call WSAStartup

    ; socket
    mov ecx, AF_INET
    mov edx, SOCK_STREAM
    mov r8d, IPPROTO_TCP
    call socket
    mov [rbp-424], rax ; hListen

    ; sockaddr setup
    ; sa.sin_family = AF_INET (2)
    mov word ptr [rbp-416], AF_INET
    ; sa.sin_port = htons(8080)
    movzx ecx, wPort
    call htons
    mov word ptr [rbp-414], ax
    ; sa.sin_addr = 0
    mov dword ptr [rbp-412], 0
    ; zero
    mov qword ptr [rbp-408], 0

    ; bind
    mov rcx, [rbp-424]
    lea rdx, [rbp-416]
    mov r8d, 16 ; sizeof sockaddr_in
    call bind

    ; listen
    mov rcx, [rbp-424]
    mov edx, 16
    call listen

@@accept_loop:
    mov rcx, [rbp-424]
    xor rdx, rdx
    xor r8d, r8d
    call accept
    mov [rbp-432], rax ; hClient
    cmp rax, INVALID_SOCKET
    je @@accept_loop

    ; recv
    mov rcx, [rbp-432]
    lea rdx, requestBuf
    mov r8d, 65535 ; sizeof requestBuf-1
    xor r9d, r9d
    call recv
    mov [rbp-440], eax ; recvLen
    cmp eax, 0
    jle @@close_client

    ; null terminate
    lea rcx, requestBuf
    movsxd rax, dword ptr [rbp-440]
    mov byte ptr [rcx+rax], 0

    ; path match
    lea rcx, requestBuf
    call match_path
    mov [rbp-464], eax ; pathCode
    cmp eax, 0
    je @@resp_404

    ; health (4)
    cmp eax, 4
    jne @@cont_parse
    
    lea rcx, szResp200
    lea rdx, szHealthBody
    lea r8, [rbp-472] ; respLen
    call build_response
    
    mov ecx, [rbp-472]
    mov r8d, ecx
    mov rcx, [rbp-432]
    lea rdx, responseBuf
    xor r9d, r9d
    call send
    jmp @@close_client

@@cont_parse:
    ; find body
    lea rcx, requestBuf
    mov edx, [rbp-440]
    call find_crlfcrlf
    mov [rbp-448], rax ; bodyOff
    
    lea rcx, requestBuf
    mov edx, [rbp-440]
    call parse_content_length
    mov [rbp-456], eax ; bodyLen
    
    cmp qword ptr [rbp-448], 0
    je @@resp_404

    ; clamp
    mov eax, [rbp-440] ; recvLen
    sub eax, dword ptr [rbp-448] ; bodyOff cast? bodyOff is QWORD
    ; safely sub
    mov r10, [rbp-448]
    sub eax, r10d
    
    cmp [rbp-456], eax
    jbe @@len_ok
    mov [rbp-456], eax
@@len_ok:

    ; load bridge
    call load_bridge_if_needed
    cmp pRunLocalModel, 0
    je @@resp_500

    ; endpoint
    mov eax, [rbp-464]
    lea r8, szEndpointCompletion
    cmp eax, 1
    je @@ep_set
    lea r8, szEndpointGenerate
    cmp eax, 2
    je @@ep_set
    lea r8, szEndpointChat
@@ep_set:
    ; r8 has endpoint

    ; prompt setup (null term)
    lea rdx, requestBuf
    add rdx, [rbp-448] ; body ptr
    mov ecx, [rbp-456]
    cmp ecx, 131070
    jbe @@oklen
    mov ecx, 131070
@@oklen:
    mov byte ptr [rdx+rcx], 0
    
    ; call RunLocalModel(endpoint, prompt, outBuf, outSize)
    ; rcx=endpoint, rdx=prompt
    mov rcx, r8
    ; rdx is already prompt body ptr
    lea r8, modelOutBuf
    mov r9d, 131072
    
    ; Must use stack shadow for call? 
    ; Yes, we have space in [rbp-something] but we need 32 bytes at RSP for shadow.
    ; Our RSP is [rbp-512]. we need to sub more.
    sub rsp, 32
    call pRunLocalModel
    add rsp, 32
    
    test eax, eax
    jz @@resp_500

    ; success
    lea rcx, szResp200
    lea rdx, modelOutBuf
    lea r8, [rbp-472]
    call build_response
    
    mov ecx, [rbp-472]
    mov r8d, ecx
    mov rcx, [rbp-432]
    lea rdx, responseBuf
    xor r9d, r9d
    call send
    jmp @@close_client

@@resp_404:
    lea rcx, szResp404
    lea rdx, szNotFoundBody
    lea r8, [rbp-472]
    call build_response
    mov ecx, [rbp-472]
    mov r8d, ecx
    mov rcx, [rbp-432]
    lea rdx, responseBuf
    xor r9d, r9d
    call send
    jmp @@close_client

@@resp_500:
    lea rcx, szResp500
    lea rdx, szErrBody
    lea r8, [rbp-472]
    call build_response
    mov ecx, [rbp-472]
    mov r8d, ecx
    mov rcx, [rbp-432]
    lea rdx, responseBuf
    xor r9d, r9d
    call send

@@close_client:
    mov rcx, [rbp-432]
    call closesocket
    jmp @@accept_loop

    leave
    ret
main ENDP

END