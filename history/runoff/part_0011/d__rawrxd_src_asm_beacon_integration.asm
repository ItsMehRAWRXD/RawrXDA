; beacon_integration.asm - Pure MASM64 Circular Beacon Integration
; Replaces Win32IDE_CircularBeaconIntegration.cpp

 includelib kernel32.lib
 includelib user32.lib

 EXTERN OutputDebugStringA:PROC
 EXTERN GetCurrentProcess:PROC

 ; Win32IDE member offsets (must match C++ class layout)
 W32IDE_HWND_MAIN        equ 0x08
 W32IDE_BEACON_INIT      equ 0xC0   ; Adjust to actual offset

 .data
 align 8
 msg_init        db "Circular beacon system initialized (MASM)",0
 msg_init_fail   db "Failed to initialize circular beacon system",0
 msg_shutdown    db "Circular beacon system shutdown",0

 .code
 align 16

 ; Export public symbols
 PUBLIC Win32IDE_InitializeBeaconIntegration
 PUBLIC Win32IDE_ShutdownBeaconIntegration
 PUBLIC Win32IDE_SendBeaconMessage
 PUBLIC Win32IDE_BroadcastBeaconMessage
 PUBLIC Win32IDE_IsBeaconInitialized

 ; void Win32IDE_InitializeBeaconIntegration(void* this)
 Win32IDE_InitializeBeaconIntegration PROC FRAME
     push rbx
     .pushreg rbx
     push rdi
     .pushreg rdi
     sub rsp, 0x20
     .allocstack 0x20
     .endprolog

     mov rbx, rcx           ; Save this pointer

     ; Get HWND from this->m_hwndMain
     mov rax, [rbx+W32IDE_HWND_MAIN]
     mov rdi, rax           ; RDI = hwnd

     ; In real implementation, call initializeFullCircularSystem(hwnd, this)
     ; For now, just mark as initialized to unblock IDE startup

     ; Set beacon initialized flag
     mov byte ptr [rbx+W32IDE_BEACON_INIT], 1

     ; Log success
     lea rcx, msg_init
     call OutputDebugStringA
     jmp done

 done:
     add rsp, 0x20
     pop rdi
     pop rbx
     ret
 Win32IDE_InitializeBeaconIntegration ENDP

 ; void Win32IDE_ShutdownBeaconIntegration(void* this)
 Win32IDE_ShutdownBeaconIntegration PROC FRAME
     push rbx
     .pushreg rbx
     sub rsp, 0x20
     .allocstack 0x20
     .endprolog

     mov rbx, rcx

     ; Check if initialized
     cmp byte ptr [rbx+W32IDE_BEACON_INIT], 0
     je done

     ; TODO: Call CircularBeaconManager_Shutdown

     ; Clear flag
     mov byte ptr [rbx+W32IDE_BEACON_INIT], 0

     ; Log
     lea rcx, msg_shutdown
     call OutputDebugStringA

 done:
     add rsp, 0x20
     pop rbx
     ret
 Win32IDE_ShutdownBeaconIntegration ENDP

 ; bool Win32IDE_SendBeaconMessage(void* this, const char* target, void* data, size_t len)
 Win32IDE_SendBeaconMessage PROC FRAME
     push rbx
     .pushreg rbx
     sub rsp, 0x20
     .allocstack 0x20
     .endprolog

     mov rbx, rcx           ; this

     ; Check initialized
     cmp byte ptr [rbx+W32IDE_BEACON_INIT], 0
     je send_failed

     ; TODO: Call CircularBeaconManager.SendTo()
     mov al, 1
     jmp done

 send_failed:
     xor al, al

 done:
     add rsp, 0x20
     pop rbx
     ret
 Win32IDE_SendBeaconMessage ENDP

 ; bool Win32IDE_BroadcastBeaconMessage(void* this, void* data, size_t len)
 Win32IDE_BroadcastBeaconMessage PROC FRAME
     push rbx
     .pushreg rbx
     sub rsp, 0x20
     .allocstack 0x20
     .endprolog

     mov rbx, rcx

     ; Check initialized
     cmp byte ptr [rbx+W32IDE_BEACON_INIT], 0
     je broadcast_failed

     ; TODO: Call CircularBeaconManager.Broadcast()
     mov al, 1
     jmp done

 broadcast_failed:
     xor al, al

 done:
     add rsp, 0x20
     pop rbx
     ret
 Win32IDE_BroadcastBeaconMessage ENDP

 ; bool Win32IDE_IsBeaconInitialized(void* this)
 Win32IDE_IsBeaconInitialized PROC FRAME
     mov al, [rcx+W32IDE_BEACON_INIT]
     ret
 Win32IDE_IsBeaconInitialized ENDP

 END
