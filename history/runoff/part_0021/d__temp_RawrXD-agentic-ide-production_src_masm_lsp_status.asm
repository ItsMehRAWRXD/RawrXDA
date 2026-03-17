;==============================================================================
; File 20: lsp_status.asm - Language Server Status Indicator
;==============================================================================
include windows.inc

.code
;==============================================================================
; Create LSP Status Widget
;==============================================================================
LspStatus_Create PROC
    invoke CreateWindowEx, 0, OFFSET szStaticClass,
        OFFSET szLSPDisconnected,
        WS_CHILD or WS_VISIBLE or 0x00000010,
        10, 10, 120, 20,
        [hMainWnd], IDC_LSP_STATUS, [hInstance], NULL
    mov [hLspStatus], rax
    
    invoke SetTimer, rax, 100, 5000, NULL
    
    LOG_INFO "LSP status widget created"
    
    ret
LspStatus_Create ENDP

;==============================================================================
; Update LSP Status
;==============================================================================
LspStatus_Update PROC status:DWORD
    LOCAL color:DWORD
    
    mov eax, status
    
    .if eax == 0
        mov [color], 0x000000FFh  ; Red (disconnected)
        invoke SetWindowText, [hLspStatus],
            OFFSET szLSPDisconnected
    .elseif eax == 1
        mov [color], 0x0000FFFFh  ; Yellow (connecting)
        invoke SetWindowText, [hLspStatus],
            OFFSET szLSPConnecting
    .elseif eax == 2
        mov [color], 0x0000FF00h  ; Green (connected)
        invoke SetWindowText, [hLspStatus],
            OFFSET szLSPConnected
    .endif
    
    invoke GetDC, [hLspStatus]
    mov [hdc], rax
    invoke SetTextColor, [hdc], [color]
    invoke ReleaseDC, [hLspStatus], [hdc]
    
    ret
LspStatus_Update ENDP

;==============================================================================
; Data
;==============================================================================
.data
szStaticClass      db 'STATIC',0
hLspStatus         dq ?
hdc                dq ?
IDC_LSP_STATUS     equ 1200

szLSPDisconnected  db 'LSP: Disconnected',0
szLSPConnecting    db 'LSP: Connecting...',0
szLSPConnected     db 'LSP: Connected',0

END
