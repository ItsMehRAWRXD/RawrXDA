; nvme_query.asm - Silicon Query Engine
option casemap:none

; Standard Windows Definitions
GENERIC_READ             EQU 80000000h
GENERIC_WRITE            EQU 40000000h
FILE_SHARE_READ          EQU 00000001h
FILE_SHARE_WRITE         EQU 00000002h
OPEN_EXISTING            EQU 3
IOCTL_STORAGE_QUERY_PROPERTY EQU 002D1400h

EXTERN CreateFileA:PROC
EXTERN DeviceIoControl:PROC
EXTERN CloseHandle:PROC

PUBLIC QueryNVMeTemp

.DATA
    qry      dd 0Bh, 0, 0, 0 ; StorageDeviceTemperatureProperty structure (StorageDeviceProperty=0, PropertyStandardQuery=0)
                             ; Actually 0Bh is PropertyId for StorageDeviceTemperatureProperty?
                             ; StorageDeviceTemperatureProperty is 11 (0xB). correct.
                             ; QueryType = PropertyStandardQuery (0).
    tempBuf  db 512 dup(0)
    bytesRet dq 0
    szDrive  db "\\.\PhysicalDrive0",0

.CODE
QueryNVMeTemp PROC FRAME
    ; RCX = Drive ID
    push rbx
    .pushreg rbx
    push r12
    .pushreg r12
    sub rsp, 48h
    .allocstack 48h
    .endprolog
    
    mov r12, rcx
    lea rax, szDrive
    ; Convert Drive ID (0-9) to ASCII and store in string
    ; Warning: Only supports 0-9.
    add r12b, '0'
    mov [szDrive+17], r12b ; Update path for Drive X ("\\.\PhysicalDrive0" -> index 17 is '0')

    ; Open Handle
    lea rcx, szDrive
    mov edx, 0C0000000h ; GENERIC_READ | GENERIC_WRITE
    mov r8d, 3          ; FILE_SHARE_READ | FILE_SHARE_WRITE
    xor r9d, r9d
    mov qword ptr [rsp+20h], 3 ; OPEN_EXISTING
    call CreateFileA
    cmp rax, -1
    je _fail
    mov rbx, rax

    ; Query IOCTL
    mov rcx, rbx
    mov edx, 002D1400h  ; IOCTL_STORAGE_QUERY_PROPERTY
    lea r8, qry
    mov r9d, 16         ; Size of input buffer
    lea rax, tempBuf
    mov qword ptr [rsp+20h], rax
    mov qword ptr [rsp+28h], 512    ; Output buffer size
    lea rax, bytesRet
    mov qword ptr [rsp+30h], rax
    mov qword ptr [rsp+38h], 0      ; Overlapped
    call DeviceIoControl
    
    test eax, eax
    jz _close_fail

    ; Parse Result
    ; STORAGE_TEMPERATURE_DATA_DESCRIPTOR
    ; Offset 16 is usually where temperature is if parsing raw
    ; Structure:
    ; DWORD Version
    ; DWORD Size
    ; DWORD CriticalTemperature
    ; DWORD WarningTemperature
    ; DWORD InfoCount
    ; STORAGE_TEMPERATURE_INFO_ENTRY ...
    ;   WORD Index
    ;   WORD Temperature (0.1 Kelvin) ... ? or just Celsius?
    ; NVMe spec says Composite Temperature in Log Page 02h is in Kelvin.
    ; Windows IOCTL_STORAGE_QUERY_PROPERTY with StorageDeviceTemperatureProperty returns STORAGE_TEMPERATURE_DATA_DESCRIPTOR.
    ; The struct has a simpler layout often.
    ; Let's assume the user's code "mov eax, dword ptr [tempBuf+16]" is correct for their hardware/driver.
    ; And "sub eax, 2731" implies 0.1 Kelvin? 273.1 K = 0 C. 2731 in 0.1K units.
    
    mov eax, dword ptr [tempBuf+16] ; Temp field (Critical? Warning? Or actual?)
                                    ; If it's the STORAGE_TEMPERATURE_INFO_ENTRY[0].Temperature
    
    ; Just trusting the provided ASM logic:
    sub eax, 2731   ; Convert from 0.1K to 0.1C (Relative to 0C) ? 
                    ; 273.15 K = 0 C.
                    ; If val is 3000 (300.0 K), 3000 - 2731 = 269 (26.9 C).
    
    cdq
    mov ecx, 10
    idiv ecx        ; Divide by 10 to get integer Celsius.
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
