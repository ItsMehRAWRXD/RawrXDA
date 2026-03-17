; src/direct_io/nvme_thermal_stressor.asm
; ════════════════════════════════════════════════════════════════════════════════
; SovereignControlBlock NVMe Thermal Poller & Unbuffered I/O Stressor
; Pure MASM x64 - Direct Hardware Interface
; ════════════════════════════════════════════════════════════════════════════════
;
; Build: ml64.exe /c nvme_thermal_stressor.asm
; Link:  link.exe /subsystem:console nvme_thermal_stressor.obj kernel32.lib
;
; Features:
;   - Direct NVMe temperature acquisition via IOCTL_STORAGE_QUERY_PROPERTY
;   - Unbuffered sequential I/O stressor (FILE_FLAG_NO_BUFFERING)
;   - Multi-drive polling (IDs 0, 1, 2, 4, 5)
;   - Aligned 4KB sector I/O for direct NAND access
;   - Temperature-based drive rotation logic
;
; Exported Functions:
;   NVMe_GetTemperature(driveId) -> temperature in Celsius (or -1 on error)
;   NVMe_PollAllDrives(outArray, driveIds, count) -> fills array with temps
;   NVMe_StressWrite(driveId, bufferPtr, sizeBytes, offsetLow, offsetHigh) -> bytes written
;   NVMe_StressRead(driveId, bufferPtr, sizeBytes, offsetLow, offsetHigh) -> bytes read
;   NVMe_GetCoolestDrive(driveIds, count) -> driveId with lowest temp
;   NVMe_AllocAlignedBuffer(sizeBytes) -> aligned buffer ptr (or 0)
;   NVMe_FreeAlignedBuffer(ptr)
;
; ════════════════════════════════════════════════════════════════════════════════

; ════════════════════════════════════════════════════════════════════════════════
; Windows API Constants
; ════════════════════════════════════════════════════════════════════════════════
INVALID_HANDLE_VALUE        EQU -1
GENERIC_READ                EQU 80000000h
GENERIC_WRITE               EQU 40000000h
FILE_SHARE_READ             EQU 1
FILE_SHARE_WRITE            EQU 2
OPEN_EXISTING               EQU 3
FILE_FLAG_NO_BUFFERING      EQU 20000000h
FILE_FLAG_WRITE_THROUGH     EQU 80000000h
FILE_FLAG_OVERLAPPED        EQU 40000000h

; IOCTL codes
IOCTL_STORAGE_QUERY_PROPERTY EQU 002D1400h

; StorageDeviceTemperatureProperty = 0x14 (20 decimal)
STORAGE_PROPERTY_TEMPERATURE EQU 14h

; Memory allocation
MEM_COMMIT                  EQU 1000h
MEM_RESERVE                 EQU 2000h
MEM_RELEASE                 EQU 8000h
PAGE_READWRITE              EQU 4

; Alignment for direct I/O (4KB sector)
SECTOR_ALIGN                EQU 4096

; ════════════════════════════════════════════════════════════════════════════════
; External Windows API
; ════════════════════════════════════════════════════════════════════════════════
EXTERN CreateFileA:PROC
EXTERN CloseHandle:PROC
EXTERN DeviceIoControl:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN SetFilePointerEx:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN GetLastError:PROC

; ════════════════════════════════════════════════════════════════════════════════
; Safe global access macros (RIP-relative helpers to avoid ADDR32 relocations)
; Usage:
;   LOAD_GPTR reg, symbol   ; reg = &symbol (RIP-relative)
;   LD32      reg, symbol   ; reg = DWORD symbol
;   ST32      symbol, reg   ; DWORD symbol = reg
; These keep the code /LAA and ASLR safe.
; ════════════════════════════════════════════════════════════════════════════════
LOAD_GPTR MACRO reg, sym
    lea reg, sym
ENDM

LD32 MACRO reg, sym
    LOAD_GPTR reg, sym
    mov reg, DWORD PTR [reg]
ENDM

ST32 MACRO sym, reg
    LOAD_GPTR rax, sym
    mov DWORD PTR [rax], reg
ENDM

; ════════════════════════════════════════════════════════════════════════════════
; Data Section
; ════════════════════════════════════════════════════════════════════════════════
.data
ALIGN 16

; Drive path buffer (PhysicalDrive format)
g_DrivePathTemplate     DB "\\.\PhysicalDrive", 0
                        ALIGN 8
g_DrivePathBuffer       DB 32 DUP(0)

; STORAGE_PROPERTY_QUERY structure (12 bytes, padded to 16)
                        ALIGN 8
g_StorageQuery          DD STORAGE_PROPERTY_TEMPERATURE  ; PropertyId
                        DD 0                              ; PropertyStandardQuery
                        DD 0                              ; AdditionalParameters
                        DD 0                              ; Padding

; Output buffer for temperature query (1024 bytes)
                        ALIGN 16
g_TempResultBuffer      DB 1024 DUP(0)

; Bytes returned from IOCTL
                        ALIGN 8
g_BytesReturned         DQ 0

; Default drive IDs for SovereignControlBlock
g_DefaultDriveIds       DD 0, 1, 2, 4, 5
g_DefaultDriveCount     DD 5

; Temperature cache (indexed by drive ID, max 16 drives)
                        ALIGN 16
g_TempCache             DD 16 DUP(-1)

; Last error code
g_LastError             DD 0

; ════════════════════════════════════════════════════════════════════════════════
; Code Section
; ════════════════════════════════════════════════════════════════════════════════
.code

; ════════════════════════════════════════════════════════════════════════════════
; Internal: BuildDrivePath
; Builds "\\.\PhysicalDriveN" string in g_DrivePathBuffer
; Input:  ECX = drive ID (0-15)
; Output: RAX = pointer to g_DrivePathBuffer
; Clobbers: RDX, R8
; ════════════════════════════════════════════════════════════════════════════════
BuildDrivePath PROC
    push rbx
    push rdi
    push rsi
    
    mov ebx, ecx                    ; Save drive ID
    
    ; Copy template to buffer - use RIP-relative LEA
    lea rsi, g_DrivePathTemplate
    lea rdi, g_DrivePathBuffer
    
_copy_loop:
    lodsb
    stosb
    test al, al
    jnz _copy_loop
    
    ; Back up one byte (overwrite null)
    dec rdi
    
    ; Convert drive ID to ASCII digit(s)
    mov eax, ebx
    cmp eax, 10
    jl _single_digit
    
    ; Two digits (10-15)
    mov ecx, 10
    xor edx, edx
    div ecx                         ; EAX = tens, EDX = ones
    add al, '0'
    stosb
    mov eax, edx
    
_single_digit:
    add al, '0'
    stosb
    
    ; Null terminate
    xor al, al
    stosb
    
    lea rax, g_DrivePathBuffer
    
    pop rsi
    pop rdi
    pop rbx
    ret
BuildDrivePath ENDP

; ════════════════════════════════════════════════════════════════════════════════
; Internal: OpenDriveHandle
; Opens a physical drive with specified access flags
; Input:  ECX = drive ID
;         EDX = access flags (e.g., GENERIC_READ | GENERIC_WRITE)
;         R8D = additional flags (e.g., FILE_FLAG_NO_BUFFERING)
; Output: RAX = handle (or INVALID_HANDLE_VALUE on error)
; ════════════════════════════════════════════════════════════════════════════════
OpenDriveHandle PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64                     ; Shadow space + locals
    
    ; Save parameters
    mov [rbp-8], rdx                ; Access flags
    mov [rbp-16], r8                ; Additional flags
    
    ; Build drive path
    call BuildDrivePath             ; RAX = path string
    
    ; CreateFileA(path, access, share, security, disposition, flags, template)
    mov rcx, rax                    ; lpFileName
    mov rdx, [rbp-8]                ; dwDesiredAccess
    mov r8d, FILE_SHARE_READ OR FILE_SHARE_WRITE  ; dwShareMode
    xor r9, r9                      ; lpSecurityAttributes = NULL
    mov DWORD PTR [rsp+20h], OPEN_EXISTING        ; dwCreationDisposition
    mov rax, [rbp-16]
    mov [rsp+28h], rax              ; dwFlagsAndAttributes
    mov QWORD PTR [rsp+30h], 0      ; hTemplateFile = NULL
    call CreateFileA
    
    ; RAX = handle or INVALID_HANDLE_VALUE
    cmp rax, INVALID_HANDLE_VALUE
    jne _open_success
    
    ; Store last error
    push rax
    call GetLastError
    lea rcx, g_LastError
    mov DWORD PTR [rcx], eax
    pop rax
    
_open_success:
    add rsp, 64
    pop rbp
    ret
OpenDriveHandle ENDP

; ════════════════════════════════════════════════════════════════════════════════
; NVMe_GetTemperature
; Reads temperature from a single NVMe drive
; Input:  ECX = drive ID (0-15)
; Output: EAX = temperature in Celsius, or -1 on error
; ════════════════════════════════════════════════════════════════════════════════
PUBLIC NVMe_GetTemperature
NVMe_GetTemperature PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80
    push rbx
    push r12
    push rdi
    
    mov r12d, ecx                   ; Save drive ID
    
    ; Open drive for IOCTL (no special flags needed)
    mov edx, GENERIC_READ OR GENERIC_WRITE
    xor r8d, r8d                    ; No extra flags
    call OpenDriveHandle
    
    cmp rax, INVALID_HANDLE_VALUE
    je _temp_error
    
    mov rbx, rax                    ; Save handle
    
    ; Zero the result buffer
    lea rdi, g_TempResultBuffer
    mov ecx, 256                    ; 1024 bytes / 4
    xor eax, eax
    rep stosd
    
    ; DeviceIoControl(handle, IOCTL, inBuf, inSize, outBuf, outSize, &returned, NULL)
    mov rcx, rbx                    ; hDevice
    mov edx, IOCTL_STORAGE_QUERY_PROPERTY ; dwIoControlCode
    lea r8, g_StorageQuery          ; lpInBuffer
    mov r9d, 12                     ; nInBufferSize
    lea rax, g_TempResultBuffer
    mov [rsp+20h], rax              ; lpOutBuffer
    mov DWORD PTR [rsp+28h], 1024   ; nOutBufferSize
    lea rax, g_BytesReturned
    mov [rsp+30h], rax              ; lpBytesReturned
    mov QWORD PTR [rsp+38h], 0      ; lpOverlapped
    call DeviceIoControl
    
    test eax, eax
    jz _temp_close_error
    
    ; Parse STORAGE_TEMPERATURE_DATA_DESCRIPTOR
    ; Structure layout (approximate):
    ;   +0x00: Version (DWORD)
    ;   +0x04: Size (DWORD)
    ;   +0x08: CriticalTemperature (SHORT)
    ;   +0x0A: WarningTemperature (SHORT)
    ;   +0x0C: InfoCount (DWORD)
    ;   +0x10: Reserved0 (DWORD[2])
    ;   +0x18: TemperatureInfo[0].Index (WORD)
    ;   +0x1A: TemperatureInfo[0].Temperature (SHORT) <-- Current temp
    ;   +0x1C: TemperatureInfo[0].OverThreshold (WORD)
    ;   +0x1E: TemperatureInfo[0].UnderThreshold (WORD)
    ;   +0x20: TemperatureInfo[0].OverThresholdChangable (BOOLEAN)
    ;   etc.
    
    lea rax, g_TempResultBuffer
    
    ; Check if we got valid data (Size > 0)
    mov ecx, DWORD PTR [rax+4]      ; Size field
    test ecx, ecx
    jz _temp_close_error
    
    ; Get current temperature from first TemperatureInfo entry
    ; Temperature is at offset 0x1A (a signed SHORT in Kelvin tenths or Celsius depending on drive)
    ; Most NVMe drives report in Celsius directly
    movsx eax, WORD PTR [rax+1Ah]
    
    ; Sanity check: temperature should be 0-100°C typically
    cmp eax, -40
    jl _temp_close_error
    cmp eax, 150
    jg _temp_close_error
    
    ; Cache the result
    mov ecx, r12d
    and ecx, 15                     ; Clamp to array bounds
    lea rdx, g_TempCache
    mov DWORD PTR [rdx + rcx*4], eax
    
    ; Close handle
    push rax
    mov rcx, rbx
    call CloseHandle
    pop rax
    
    jmp _temp_done
    
_temp_close_error:
    mov rcx, rbx
    call CloseHandle
    
_temp_error:
    mov eax, -1
    
_temp_done:
    pop rdi
    pop r12
    pop rbx
    add rsp, 80
    pop rbp
    ret
NVMe_GetTemperature ENDP

; ════════════════════════════════════════════════════════════════════════════════
; NVMe_PollAllDrives
; Polls temperature from multiple drives
; Input:  RCX = pointer to output array (DWORD per drive)
;         RDX = pointer to drive ID array (DWORD per drive)
;         R8D = number of drives
; Output: EAX = number of successful reads
; ════════════════════════════════════════════════════════════════════════════════
PUBLIC NVMe_PollAllDrives
NVMe_PollAllDrives PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx                    ; Output array
    mov r13, rdx                    ; Drive ID array
    mov r14d, r8d                   ; Count
    xor r15d, r15d                  ; Success counter
    xor ebx, ebx                    ; Loop index
    
_poll_loop:
    cmp ebx, r14d
    jge _poll_done
    
    ; Get drive ID from array
    mov ecx, DWORD PTR [r13 + rbx*4]
    call NVMe_GetTemperature
    
    ; Store result
    mov DWORD PTR [r12 + rbx*4], eax
    
    ; Count successes
    cmp eax, -1
    je _poll_next
    inc r15d
    
_poll_next:
    inc ebx
    jmp _poll_loop
    
_poll_done:
    mov eax, r15d
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    add rsp, 48
    pop rbp
    ret
NVMe_PollAllDrives ENDP

; ════════════════════════════════════════════════════════════════════════════════
; NVMe_GetCoolestDrive
; Finds the drive with the lowest temperature
; Input:  RCX = pointer to drive ID array (DWORD per drive)
;         EDX = number of drives
; Output: EAX = drive ID with lowest temp, or -1 if all failed
; ════════════════════════════════════════════════════════════════════════════════
PUBLIC NVMe_GetCoolestDrive
NVMe_GetCoolestDrive PROC
    push rbp
    mov rbp, rsp
    sub rsp, 80
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx                    ; Drive ID array
    mov r13d, edx                   ; Count
    
    mov r14d, -1                    ; Best drive ID (none yet)
    mov r15d, 7FFFFFFFh             ; Best temperature (max int)
    xor ebx, ebx                    ; Loop index
    
_cool_loop:
    cmp ebx, r13d
    jge _cool_done
    
    ; Get drive ID
    mov ecx, DWORD PTR [r12 + rbx*4]
    push rcx                        ; Save drive ID
    
    ; Get temperature
    call NVMe_GetTemperature
    
    pop rcx                         ; Restore drive ID
    
    ; Check if valid and cooler
    cmp eax, -1
    je _cool_next
    cmp eax, r15d
    jge _cool_next
    
    ; New best
    mov r15d, eax                   ; Best temp
    mov r14d, ecx                   ; Best drive ID
    
_cool_next:
    inc ebx
    jmp _cool_loop
    
_cool_done:
    mov eax, r14d
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    add rsp, 80
    pop rbp
    ret
NVMe_GetCoolestDrive ENDP

; ════════════════════════════════════════════════════════════════════════════════
; NVMe_AllocAlignedBuffer
; Allocates a 4KB-aligned buffer for direct I/O
; Input:  RCX = size in bytes (will be rounded up to 4KB boundary)
; Output: RAX = buffer pointer, or 0 on failure
; ════════════════════════════════════════════════════════════════════════════════
PUBLIC NVMe_AllocAlignedBuffer
NVMe_AllocAlignedBuffer PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; Round size up to 4KB boundary
    mov rax, rcx
    add rax, SECTOR_ALIGN - 1
    and rax, NOT (SECTOR_ALIGN - 1)
    mov rdx, rax                    ; Save rounded size
    
    ; VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
    xor ecx, ecx                    ; lpAddress = NULL (let system choose)
    ; RDX = rounded size
    mov r8d, MEM_COMMIT OR MEM_RESERVE
    mov r9d, PAGE_READWRITE
    call VirtualAlloc
    
    ; RAX = buffer or NULL
    
    add rsp, 48
    pop rbp
    ret
NVMe_AllocAlignedBuffer ENDP

; ════════════════════════════════════════════════════════════════════════════════
; NVMe_FreeAlignedBuffer
; Frees a buffer allocated by NVMe_AllocAlignedBuffer
; Input:  RCX = buffer pointer
; ════════════════════════════════════════════════════════════════════════════════
PUBLIC NVMe_FreeAlignedBuffer
NVMe_FreeAlignedBuffer PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; VirtualFree(ptr, 0, MEM_RELEASE)
    ; RCX = ptr
    xor edx, edx                    ; dwSize = 0 for MEM_RELEASE
    mov r8d, MEM_RELEASE
    call VirtualFree
    
    add rsp, 32
    pop rbp
    ret
NVMe_FreeAlignedBuffer ENDP

; ════════════════════════════════════════════════════════════════════════════════
; NVMe_StressWrite
; Performs unbuffered sequential write to a physical drive
; Input:  ECX = drive ID
;         RDX = buffer pointer (must be 4KB aligned)
;         R8  = size in bytes (must be multiple of 4KB)
;         R9  = offset low DWORD
;         [rbp+30h] = offset high DWORD
; Output: RAX = bytes written, or 0 on error
; ════════════════════════════════════════════════════════════════════════════════
PUBLIC NVMe_StressWrite
NVMe_StressWrite PROC
    push rbp
    mov rbp, rsp
    sub rsp, 96
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    ; Save parameters
    mov r12d, ecx                   ; Drive ID
    mov r13, rdx                    ; Buffer
    mov r14, r8                     ; Size
    mov r15, r9                     ; Offset low
    mov rax, [rbp+30h]              ; Offset high
    mov [rbp-8], rax
    
    ; Open drive with unbuffered + write-through flags
    mov ecx, r12d
    mov edx, GENERIC_READ OR GENERIC_WRITE
    mov r8d, FILE_FLAG_NO_BUFFERING OR FILE_FLAG_WRITE_THROUGH
    call OpenDriveHandle
    
    cmp rax, INVALID_HANDLE_VALUE
    je _write_error
    
    mov rbx, rax                    ; Save handle
    
    ; SetFilePointerEx(handle, offset, NULL, FILE_BEGIN)
    mov rcx, rbx                    ; Handle
    mov rdx, r15                    ; Low offset
    mov rax, [rbp-8]
    shl rax, 32
    or rdx, rax                     ; Full 64-bit offset in RDX
    xor r8, r8                      ; lpNewFilePointer = NULL
    xor r9d, r9d                    ; dwMoveMethod = FILE_BEGIN
    call SetFilePointerEx
    
    test eax, eax
    jz _write_close_error
    
    ; WriteFile(handle, buffer, size, &written, NULL)
    mov rcx, rbx                    ; hFile
    mov rdx, r13                    ; lpBuffer
    mov r8, r14                     ; nNumberOfBytesToWrite (truncated to DWORD)
    lea r9, g_BytesReturned         ; lpNumberOfBytesWritten
    mov QWORD PTR [rsp+20h], 0      ; lpOverlapped
    call WriteFile
    
    test eax, eax
    jz _write_close_error
    
    ; Get bytes written
    lea rax, g_BytesReturned
    mov rax, QWORD PTR [rax]
    push rax
    
    ; Close handle
    mov rcx, rbx
    call CloseHandle
    
    pop rax
    jmp _write_done
    
_write_close_error:
    mov rcx, rbx
    call CloseHandle
    
_write_error:
    xor eax, eax
    
_write_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    add rsp, 96
    pop rbp
    ret
NVMe_StressWrite ENDP

; ════════════════════════════════════════════════════════════════════════════════
; NVMe_StressRead
; Performs unbuffered sequential read from a physical drive
; Input:  ECX = drive ID
;         RDX = buffer pointer (must be 4KB aligned)
;         R8  = size in bytes (must be multiple of 4KB)
;         R9  = offset low DWORD
;         [rbp+30h] = offset high DWORD
; Output: RAX = bytes read, or 0 on error
; ════════════════════════════════════════════════════════════════════════════════
PUBLIC NVMe_StressRead
NVMe_StressRead PROC
    push rbp
    mov rbp, rsp
    sub rsp, 96
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    ; Save parameters
    mov r12d, ecx                   ; Drive ID
    mov r13, rdx                    ; Buffer
    mov r14, r8                     ; Size
    mov r15, r9                     ; Offset low
    mov rax, [rbp+30h]              ; Offset high
    mov [rbp-8], rax
    
    ; Open drive with unbuffered flag
    mov ecx, r12d
    mov edx, GENERIC_READ
    mov r8d, FILE_FLAG_NO_BUFFERING
    call OpenDriveHandle
    
    cmp rax, INVALID_HANDLE_VALUE
    je _read_error
    
    mov rbx, rax                    ; Save handle
    
    ; SetFilePointerEx(handle, offset, NULL, FILE_BEGIN)
    mov rcx, rbx
    mov rdx, r15
    mov rax, [rbp-8]
    shl rax, 32
    or rdx, rax
    xor r8, r8
    xor r9d, r9d
    call SetFilePointerEx
    
    test eax, eax
    jz _read_close_error
    
    ; ReadFile(handle, buffer, size, &read, NULL)
    mov rcx, rbx
    mov rdx, r13
    mov r8, r14
    lea r9, g_BytesReturned
    mov QWORD PTR [rsp+20h], 0
    call ReadFile
    
    test eax, eax
    jz _read_close_error
    
    lea rax, g_BytesReturned
    mov rax, QWORD PTR [rax]
    push rax
    
    mov rcx, rbx
    call CloseHandle
    
    pop rax
    jmp _read_done
    
_read_close_error:
    mov rcx, rbx
    call CloseHandle
    
_read_error:
    xor eax, eax
    
_read_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    add rsp, 96
    pop rbp
    ret
NVMe_StressRead ENDP

; ════════════════════════════════════════════════════════════════════════════════
; NVMe_StressBurst
; High-throughput burst I/O with temperature monitoring
; Performs multiple sequential reads/writes while sampling temperature
; Input:  ECX = drive ID
;         RDX = buffer pointer (4KB aligned)
;         R8  = total bytes to transfer
;         R9D = 0=read, 1=write
; Output: EAX = final temperature after burst, or -1 on error
; ════════════════════════════════════════════════════════════════════════════════
PUBLIC NVMe_StressBurst
NVMe_StressBurst PROC
    push rbp
    mov rbp, rsp
    sub rsp, 112
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12d, ecx                   ; Drive ID
    mov r13, rdx                    ; Buffer
    mov r14, r8                     ; Total bytes
    mov r15d, r9d                   ; Operation (0=read, 1=write)
    
    ; Get initial temperature
    mov ecx, r12d
    call NVMe_GetTemperature
    mov [rbp-16], eax               ; Store initial temp
    
    ; Calculate number of 1MB chunks
    mov rax, r14
    shr rax, 20                     ; Divide by 1MB
    test rax, rax
    jz _burst_single                ; Less than 1MB, do single op
    
    mov rbx, rax                    ; Chunk count
    xor r14, r14                    ; Current offset
    
_burst_loop:
    test rbx, rbx
    jz _burst_final_temp
    
    ; Perform 1MB I/O
    mov ecx, r12d                   ; Drive ID
    mov rdx, r13                    ; Buffer
    mov r8, 100000h                 ; 1MB
    mov r9, r14                     ; Offset low
    mov QWORD PTR [rsp+20h], 0      ; Offset high
    
    test r15d, r15d
    jz _burst_read
    
    call NVMe_StressWrite
    jmp _burst_check
    
_burst_read:
    call NVMe_StressRead
    
_burst_check:
    test rax, rax
    jz _burst_error
    
    ; Advance offset
    add r14, 100000h
    dec rbx
    jmp _burst_loop
    
_burst_single:
    ; Handle sub-1MB transfer
    mov ecx, r12d
    mov rdx, r13
    mov r8, r14                     ; Actual size (rounded to 4KB)
    add r8, 0FFFh
    and r8, NOT 0FFFh
    xor r9, r9
    mov QWORD PTR [rsp+20h], 0
    
    test r15d, r15d
    jz _burst_single_read
    call NVMe_StressWrite
    jmp _burst_final_temp
    
_burst_single_read:
    call NVMe_StressRead
    
_burst_final_temp:
    ; Get final temperature
    mov ecx, r12d
    call NVMe_GetTemperature
    jmp _burst_done
    
_burst_error:
    mov eax, -1
    
_burst_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    add rsp, 112
    pop rbp
    ret
NVMe_StressBurst ENDP

; ════════════════════════════════════════════════════════════════════════════════
; NVMe_GetLastError
; Returns the last Windows error code from failed operations
; Output: EAX = error code
; ════════════════════════════════════════════════════════════════════════════════
PUBLIC NVMe_GetLastError
NVMe_GetLastError PROC
    lea rax, g_LastError
    mov eax, DWORD PTR [rax]
    ret
NVMe_GetLastError ENDP

; ════════════════════════════════════════════════════════════════════════════════
; NVMe_GetCachedTemp
; Returns cached temperature for a drive (from last poll)
; Input:  ECX = drive ID
; Output: EAX = cached temperature, or -1 if not polled
; ════════════════════════════════════════════════════════════════════════════════
PUBLIC NVMe_GetCachedTemp
NVMe_GetCachedTemp PROC
    and ecx, 15
    lea rax, g_TempCache
    mov eax, DWORD PTR [rax + rcx*4]
    ret
NVMe_GetCachedTemp ENDP

END
