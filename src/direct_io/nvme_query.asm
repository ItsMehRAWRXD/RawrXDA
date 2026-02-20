; nvme_query.asm - DLL with temperature query export
option casemap:none

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


; External APIs
EXTERN CreateFileA:PROC
EXTERN DeviceIoControl:PROC
EXTERN CloseHandle:PROC

PUBLIC QueryNVMeTemp
PUBLIC DllMain

.CODE

DllMain PROC FRAME hinst:DWORD, reason:DWORD, reserved:DWORD
    .ENDPROLOG
    mov eax, 1
    ret
DllMain ENDP

QueryNVMeTemp PROC FRAME
    ; RCX = drive ID
    ; Returns EAX = temperature in Celsius, -1 on failure
    
    push rbx
    push r12
    sub rsp, 48h
    .ENDPROLOG
    
    mov r12d, ecx
    
    ; Build path "\\.\PhysicalDriveX"
    lea rax, drivePath
    mov byte ptr [rax+17], '0'
    add byte ptr [rax+17], r12b
    
    ; CreateFileA
    lea rcx, drivePath
    mov edx, 0C0000000h         ; GENERIC_READ | GENERIC_WRITE
    mov r8d, 3                  ; FILE_SHARE_READ | FILE_SHARE_WRITE
    xor r9d, r9d
    mov qword ptr [rsp+20h], 3  ; OPEN_EXISTING
    mov qword ptr [rsp+28h], 0
    mov qword ptr [rsp+30h], 0
    call CreateFileA
    cmp rax, -1
    je _fail
    mov rbx, rax
    
    ; DeviceIoControl for temperature
    mov dword ptr [qry], 0Bh     ; StorageDeviceTemperatureProperty
    mov dword ptr [qry+4], 0     ; PropertyStandardQuery
    
    mov rcx, rbx
    mov edx, 002D1400h          ; IOCTL_STORAGE_QUERY_PROPERTY
    lea r8, qry
    mov r9d, 16
    lea rax, tempBuf
    mov qword ptr [rsp+20h], rax
    mov qword ptr [rsp+28h], 512
    lea rax, bytesRet
    mov qword ptr [rsp+30h], rax
    mov qword ptr [rsp+38h], 0
    call DeviceIoControl
    
    test eax, eax
    jz _close_fail
    
    ; Parse temperature (0.1 Kelvin -> Celsius)
    mov eax, dword ptr [tempBuf+16]  ; Temperature field offset
    sub eax, 2731
    cdq
    mov ecx, 10
    idiv ecx
    mov r12d, eax
    
    mov rcx, rbx
    call CloseHandle
    
    mov eax, r12d
    add rsp, 48h
    pop r12
    pop rbx
    ret

_close_fail:
    mov rcx, rbx
    call CloseHandle
_fail:
    mov eax, -1
    add rsp, 48h
    pop r12
    pop rbx
    ret

QueryNVMeTemp ENDP

.DATA

drivePath   db "\\.\PhysicalDrive0",0
qry         dd 4 dup(0)
tempBuf     db 512 dup(0)
bytesRet    dq ?

END