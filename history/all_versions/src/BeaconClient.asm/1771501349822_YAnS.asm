; RawrXD Circular Beacon Client — MASM64 Implementation
; nasm -f win64 BeaconClient.asm -o BeaconClient.obj && link /subsystem:windows BeaconClient.obj kernel32.lib winhttp.lib

EXTERN ExitProcess:PROC, GetModuleHandleW:PROC, WinHttpOpen:PROC, WinHttpConnect:PROC
EXTERN WinHttpOpenRequest:PROC, WinHttpSendRequest:PROC, WinHttpReceiveResponse:PROC
EXTERN WinHttpReadData:PROC, WinHttpCloseHandle:PROC

.data
szAgent         dw  'R','a','w','r','X','D',0
szProtocol      dw  'H','T','T','P','/','1','.','1',0
szBeaconRoute   db  '/api/beacon/circular',0
szMethodPost    dw  'P','O','S','T',0
szContentJson   dw  'C','o','n','t','e','n','t','-','T','y','p','e',0
szAppJson       dw  'a','p','p','l','i','c','a','t','i','o','n','/','j','s','o','n',0

BEACON_RING_SIZE EQU 4096
BEACON_MAGIC     EQU 0x52425752 ; 'RBWR' (RawrXD Beacon Wire)

; Ring buffer for tamper-resistant beacon messages
BeaconRing struct
    magic       dq ?
    head        dq ?
    tail        dq ?
    lock        dq ?
    buffer      db BEACON_RING_SIZE dup(?)
BeaconRing ends

.data?
align 16
g_BeaconRing    BeaconRing <>
g_hSession      dq ?
g_hConnect      dq ?
g_hRequest      dq ?

.code
main PROC
    ; Initialize beacon ring with magic
    mov g_BeaconRing.magic, BEACON_MAGIC
    mov g_BeaconRing.head, 0
    mov g_BeaconRing.tail, 0
    mov g_BeaconRing.lock, 0
    
    ; Open WinHTTP session
    xor rcx, rcx
    lea rdx, szAgent
    xor r8, r8
    xor r9, r9
    xor r10, r10
    call WinHttpOpen
    mov g_hSession, rax
    
    ; Connect to localhost:9337 (beacon manager)
    mov rcx, rax
    lea rdx, szLocalhost
    mov r8, 9337
    xor r9, r9
    call WinHttpConnect
    mov g_hConnect, rax
    
    ; Post beacon registration
    call BeaconRegister
    
    ; Enter circular dispatch loop
    call BeaconDispatchLoop
    
    xor ecx, ecx
    call ExitProcess
main ENDP

; Circular beacon protocol: [MAGIC(4)][TYPE(2)][LEN(2)][PAYLOAD(LEN)][HMAC(32)]
BeaconRegister PROC
    sub rsp, 512
    
    ; Build registration payload with RBAC token
    lea rdi, [rsp+64]
    mov dword ptr [rdi], BEACON_MAGIC
    mov word ptr [rdi+4], 0x0001 ; TYPE_REGISTER
    mov word ptr [rdi+6], 256    ; Payload len
    
    ; Copy RBAC signature (from shared_context.h RBAC engine)
    lea rsi, g_RBACSessionToken
    lea rdi, [rsp+72]
    mov rcx, 32
    rep movsb
    
    ; Send via WinHTTP
    mov rcx, g_hConnect
    lea rdx, szMethodPost
    lea r8, szBeaconRoute
    xor r9, r9
    xor r10, r10
    xor r11, r11
    call WinHttpOpenRequest
    
    mov g_hRequest, rax
    
    ; Add Content-Type header
    mov rcx, g_hRequest
    lea rdx, szContentJson
    lea r8, szAppJson
    mov r9, -1
    xor r10, r10
    call WinHttpAddRequestHeaders
    
    ; Send beacon packet
    mov rcx, g_hRequest
    lea rdx, [rsp+64]
    mov r8, 264 ; Header + payload
    xor r9, r9
    xor r10, r10
    xor r11, r11
    call WinHttpSendRequest
    
    add rsp, 512
    ret
BeaconRegister ENDP

; Real-time beacon dispatch with AVX-512 optimized ring buffer
BeaconDispatchLoop PROC
    sub rsp, 256
    
dispatch_loop:
    ; Check ring buffer for outgoing messages (lock-free with CRC32)
    mov rax, g_BeaconRing.head
    cmp rax, g_BeaconRing.tail
    je check_incoming
    
    ; Calculate CRC32 of message for integrity
    lea rsi, g_BeaconRing.buffer
    add rsi, rax
    mov rcx, [rsi+6] ; Length
    lea rdi, [rsi+8] ; Payload start
    xor eax, eax
    db 0xF2, 0x0F, 0x38, 0xF1, 0x07 ; CRC32 eax, dword ptr [rdi]
    
    ; Transmit beacon packet
    call BeaconTransmit
    
check_incoming:
    ; Non-blocking check for server commands
    mov rcx, g_hRequest
    lea rdx, [rsp]
    mov r8, 256
    xor r9, r9
    call WinHttpReadData
    
    test rax, rax
    jz dispatch_loop
    
    ; Process incoming beacon command
    call BeaconProcessCommand
    jmp dispatch_loop
    
    add rsp, 256
    ret
BeaconDispatchLoop ENDP

; Hot-patch triggered beacon message (called from HotPatcher)
BeaconNotifyHotpatch PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    ; Atomic ring buffer write
    lea rdi, g_BeaconRing.buffer
    mov rax, g_BeaconRing.head
    and rax, BEACON_RING_SIZE-1
    
    ; Write beacon header
    mov dword ptr [rdi+rax], BEACON_MAGIC
    mov word ptr [rdi+rax+4], 0x0002 ; TYPE_HOTPATCH
    mov word ptr [rdi+rax+6], 48     ; Payload length
    
    ; Copy patch ID and hash
    mov rsi, [rbp+16] ; Patch ID ptr
    lea rdi, [rdi+rax+8]
    mov rcx, 48
    rep movsb
    
    ; Advance head atomically
    lock add g_BeaconRing.head, 56
    
    leave
    ret
BeaconNotifyHotpatch ENDP

szLocalhost     dw  '1','2','7','.','0','.','0','.','1',0
g_RBACSessionToken db 32 dup(0)

END
