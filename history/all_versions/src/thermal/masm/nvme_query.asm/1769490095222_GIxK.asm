; nvme_query.asm - Silicon Query Engine (Fixed Temperature Parsing)
option casemap:none

; Standard Windows Definitions
GENERIC_READ             EQU 80000000h
GENERIC_WRITE            EQU 40000000h
FILE_SHARE_READ          EQU 00000001h
FILE_SHARE_WRITE         EQU 00000002h
OPEN_EXISTING            EQU 3
IOCTL_STORAGE_QUERY_PROPERTY EQU 002D1400h

; STORAGE_TEMPERATURE_DATA_DESCRIPTOR offsets (Windows SDK)
; Offset 0:  DWORD Version
; Offset 4:  DWORD Size  
; Offset 8:  SHORT CriticalTemperature (Celsius, signed)
; Offset 10: SHORT WarningTemperature (Celsius, signed)
; Offset 12: WORD  InfoCount
; Offset 14: BYTE  Reserved0[2]
; Offset 16: STORAGE_TEMPERATURE_INFO[0]
;   Offset 16: WORD  Index
;   Offset 18: SHORT Temperature (Celsius, signed!) 
;   Offset 20: SHORT OverThreshold
;   Offset 22: SHORT UnderThreshold
;   Offset 24: BYTE  OverThresholdChangable
;   Offset 25: BYTE  UnderThresholdChangable
;   Offset 26: BYTE  EventGenerated
;   Offset 27: BYTE  Reserved0
;   Offset 28: DWORD Reserved1

EXTERN CreateFileA:PROC
EXTERN DeviceIoControl:PROC
EXTERN CloseHandle:PROC

PUBLIC QueryNVMeTemp

.DATA
    ALIGN 8
    qry      dd 0Bh     ; PropertyId = StorageDeviceTemperatureProperty (11)
             dd 0       ; QueryType = PropertyStandardQuery (0)
             dd 0, 0    ; Additional padding
    tempBuf  db 512 dup(0)
    bytesRet dq 0
    szDrive  db "\\.\PhysicalDrive0",0

.CODE
QueryNVMeTemp PROC FRAME
    ; RCX = Drive ID (0-9)
    ; Returns: Temperature in Celsius, or -1 on failure
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    push rdi
    .pushreg rdi
    sub rsp, 60h
    .allocstack 60h
    .endprolog
    
    ; Validate drive ID (0-9 only)
    cmp ecx, 9
    ja _fail
    
    ; Build drive path string
    mov r12d, ecx
    add r12b, '0'
    lea rax, szDrive
    mov byte ptr [rax+17], r12b ; "\\.\PhysicalDrive0" -> index 17 is the digit

    ; CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile)
    lea rcx, szDrive
    mov edx, GENERIC_READ OR GENERIC_WRITE
    mov r8d, FILE_SHARE_READ OR FILE_SHARE_WRITE
    xor r9d, r9d                    ; NULL security attributes
    mov qword ptr [rsp+20h], OPEN_EXISTING
    mov qword ptr [rsp+28h], 0      ; dwFlagsAndAttributes
    mov qword ptr [rsp+30h], 0      ; hTemplateFile
    call CreateFileA
    
    cmp rax, -1                     ; INVALID_HANDLE_VALUE
    je _fail
    mov rbx, rax                    ; Save handle

    ; Clear output buffer
    lea rdi, tempBuf
    xor eax, eax
    mov ecx, 128                    ; 512 bytes / 4
    rep stosd

    ; DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned, lpOverlapped)
    mov rcx, rbx                    ; hDevice
    mov edx, IOCTL_STORAGE_QUERY_PROPERTY
    lea r8, qry                     ; lpInBuffer
    mov r9d, 16                     ; nInBufferSize (STORAGE_PROPERTY_QUERY size)
    lea rax, tempBuf
    mov qword ptr [rsp+20h], rax    ; lpOutBuffer
    mov qword ptr [rsp+28h], 512    ; nOutBufferSize
    lea rax, bytesRet
    mov qword ptr [rsp+30h], rax    ; lpBytesReturned
    mov qword ptr [rsp+38h], 0      ; lpOverlapped
    call DeviceIoControl
    
    test eax, eax
    jz _close_fail                  ; DeviceIoControl failed

    ; Check we got enough data
    mov rax, [bytesRet]
    cmp rax, 20                     ; Need at least header + one temp entry
    jb _close_fail

    ; Parse STORAGE_TEMPERATURE_DATA_DESCRIPTOR
    ; Check Version (should be 1)
    lea rdi, tempBuf
    mov eax, dword ptr [rdi]        ; Version
    cmp eax, 1
    jne _close_fail

    ; Check InfoCount > 0
    movzx eax, word ptr [rdi+12]    ; InfoCount
    test eax, eax
    jz _close_fail

    ; Read Temperature from first STORAGE_TEMPERATURE_INFO entry
    ; Offset 18 = Temperature (SHORT, signed, already in Celsius!)
    movsx eax, word ptr [rdi+18]    ; Temperature in Celsius (signed)
    
    ; Validate temperature is reasonable (-50 to 150 C)
    cmp eax, -50
    jl _close_fail
    cmp eax, 150
    jg _close_fail
    
    ; Save valid temperature
    mov r12d, eax

    ; Close handle
    mov rcx, rbx
    call CloseHandle
    
    ; Return temperature
    mov eax, r12d
    jmp _exit

_close_fail:
    mov rcx, rbx
    call CloseHandle
_fail:
    mov eax, -1                     ; Return -1 for failure/invalid
_exit:
    add rsp, 60h
    pop rdi
    pop r12
    pop rbx
    ret
QueryNVMeTemp ENDP
END
    mov r12d, eax   ; Save result

    mov rcx, rbx
    call CloseHandle
    mov eax, r12d
    jmp _exit

_close_fail:
    mov rcx, rbx
    call CloseHandle
_fail:
    mov eax, -1
_exit:
    add rsp, 48h
    pop r12
    pop rbx
    ret
QueryNVMeTemp ENDP
END
