; =============================================================================
; disk_recovery_scsi.asm — SCSI Hammer Kernel for Disk Recovery Agent
; =============================================================================
; Direct SCSI pass-through routines for dying USB bridge recovery.
; Bypasses Windows USBSTOR driver via IOCTL_SCSI_PASS_THROUGH_DIRECT.
;
; Exports:
;   asm_scsi_hammer_read    — Retry-loop sector read with exponential backoff
;   asm_scsi_inquiry_quick  — Fast INQUIRY with custom timeout
;   asm_scsi_read_capacity  — Read drive capacity via READ CAPACITY(10)
;   asm_scsi_request_sense  — Issue REQUEST SENSE after error
;   asm_extract_bridge_key  — Vendor-specific EEPROM key extraction
;
; Architecture: x64 MASM | Windows ABI | No exceptions | No CRT
; Build: ml64.exe /c /Zi /Zd /Fo disk_recovery_scsi.obj disk_recovery_scsi.asm
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

; =============================================================================
;                             EXPORTS
; =============================================================================
PUBLIC asm_scsi_hammer_read
PUBLIC asm_scsi_inquiry_quick
PUBLIC asm_scsi_read_capacity
PUBLIC asm_scsi_request_sense
PUBLIC asm_extract_bridge_key

; =============================================================================
;                          EXTERNAL IMPORTS
; =============================================================================
EXTERN DeviceIoControl: PROC
EXTERN GetLastError: PROC
EXTERN Sleep: PROC

; =============================================================================
;                            CONSTANTS
; =============================================================================

; IOCTLs
IOCTL_SCSI_PASS_THROUGH_DIRECT     EQU 04D004h

; SCSI Opcodes
SCSI_OP_INQUIRY         EQU 12h
SCSI_OP_REQUEST_SENSE   EQU 03h
SCSI_OP_READ_CAPACITY   EQU 25h
SCSI_OP_READ10          EQU 28h
SCSI_OP_READ12          EQU 0A8h
SCSI_OP_SECURITY_IN     EQU 0A1h       ; Vendor key extraction

; SCSI_PASS_THROUGH_DIRECT data direction
SCSI_DATA_NONE          EQU 0
SCSI_DATA_IN            EQU 1
SCSI_DATA_OUT           EQU 2

; SPTD structure size (x64: Length(2) + Status(1) + PathId(1) + TargetId(1) +
;   Lun(1) + CdbLength(1) + SenseLength(1) + DataIn(1) + pad(3) +
;   DataTransferLen(4) + TimeOut(4) + pad(4) + DataBuf(8) +
;   SenseOffset(4) + CDB(16) = ~56 bytes; we allocate 64 for alignment)
SPTD_SIZE               EQU 56
SPTD_ALLOC              EQU 64          ; Padded for stack alignment
SENSE_BUFFER_SIZE       EQU 32          ; Appended after SPTD

; SPTD field offsets (x64 Windows SDK layout)
SPTD_OFF_LENGTH         EQU 0           ; USHORT
SPTD_OFF_SCSISTATUS     EQU 2           ; UCHAR
SPTD_OFF_PATHID         EQU 3           ; UCHAR
SPTD_OFF_TARGETID       EQU 4           ; UCHAR
SPTD_OFF_LUN            EQU 5           ; UCHAR
SPTD_OFF_CDBLENGTH      EQU 6           ; UCHAR
SPTD_OFF_SENSEINFOLEN   EQU 7           ; UCHAR
SPTD_OFF_DATAIN         EQU 8           ; UCHAR  (DataIn)
; 3 bytes padding
SPTD_OFF_DATAXFERLEN    EQU 12          ; ULONG
SPTD_OFF_TIMEOUT        EQU 16          ; ULONG
; 4 bytes padding on x64
SPTD_OFF_DATABUF        EQU 24          ; PVOID  (8 bytes on x64)
SPTD_OFF_SENSEOFFSET    EQU 32          ; ULONG
; CDB starts at offset 36 on x64
SPTD_OFF_CDB            EQU 36

; Error codes matching Windows
ERROR_SEM_TIMEOUT       EQU 79h
ERROR_GEN_FAILURE       EQU 1Fh

; SCSI Sense Keys
SENSE_KEY_NO_SENSE      EQU 00h
SENSE_KEY_RECOVERED     EQU 01h
SENSE_KEY_NOT_READY     EQU 02h
SENSE_KEY_MEDIUM_ERROR  EQU 03h
SENSE_KEY_HARDWARE_ERR  EQU 04h
SENSE_KEY_ILLEGAL_REQ   EQU 05h
SENSE_KEY_UNIT_ATTN     EQU 06h
SENSE_KEY_ABORTED_CMD   EQU 0Bh

; Recovery return codes
RECOVERY_OK             EQU 0
RECOVERY_TIMEOUT        EQU 1
RECOVERY_MEDIUM_ERROR   EQU 2
RECOVERY_HARDWARE_ERROR EQU 3
RECOVERY_BRIDGE_DEAD    EQU 4
RECOVERY_ILLEGAL_REQ    EQU 5

; =============================================================================
;                            CODE
; =============================================================================
.code

; =============================================================================
; asm_scsi_hammer_read
; Retry-loop SCSI READ(10) with exponential backoff for dying drives.
;
; RCX = device HANDLE (from CreateFileA on \\.\PhysicalDriveX)
; RDX = LBA (64-bit, truncated to 32-bit for READ10)
; R8  = destination buffer (must be >= sectorSize bytes)
; R9D = sector size in bytes (typically 512 or 4096)
; [RSP+40] = max retries (DWORD)
; [RSP+48] = timeout per attempt in ms (DWORD)
;
; Returns: RAX = RECOVERY_OK (0) on success, error code on failure
;          RDX = bytes transferred on success, SCSI sense key on failure
; =============================================================================
asm_scsi_hammer_read PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 160                    ; SPTD + sense + shadow + locals
    .allocstack 160
    .endprolog

    ; Save parameters
    mov     r12, rcx                    ; r12 = hDevice
    mov     r13, rdx                    ; r13 = LBA
    mov     r14, r8                     ; r14 = dest buffer
    mov     r15d, r9d                   ; r15d = sector size
    mov     esi, dword ptr [rsp + 160 + 56 + 40]   ; max retries (past pushed regs + alloc + ret)
    mov     edi, dword ptr [rsp + 160 + 56 + 48]   ; timeout ms

    xor     ebx, ebx                    ; ebx = retry counter

@@hammer_loop:
    cmp     ebx, esi
    jge     @@hammer_exhausted

    ; ---- Zero the SPTD + sense region on stack ----
    lea     rcx, [rsp + 32]             ; SPTD starts at rsp+32 (after shadow)
    push    rdi
    mov     rdi, rcx
    mov     ecx, SPTD_ALLOC + SENSE_BUFFER_SIZE
    xor     eax, eax
    rep     stosb
    pop     rdi

    ; ---- Build SCSI_PASS_THROUGH_DIRECT ----
    lea     rax, [rsp + 32]             ; rax -> SPTD base

    mov     word ptr  [rax + SPTD_OFF_LENGTH],      SPTD_SIZE
    mov     byte ptr  [rax + SPTD_OFF_CDBLENGTH],   10      ; READ(10) = 10 byte CDB
    mov     byte ptr  [rax + SPTD_OFF_SENSEINFOLEN], SENSE_BUFFER_SIZE
    mov     byte ptr  [rax + SPTD_OFF_DATAIN],      SCSI_DATA_IN
    mov     dword ptr [rax + SPTD_OFF_DATAXFERLEN], r15d    ; sector size
    mov     dword ptr [rax + SPTD_OFF_TIMEOUT],     edi     ; timeout ms
    mov     qword ptr [rax + SPTD_OFF_DATABUF],     r14     ; dest buffer

    ; SenseInfoOffset = SPTD_SIZE (sense buffer appended right after)
    mov     dword ptr [rax + SPTD_OFF_SENSEOFFSET], SPTD_SIZE

    ; ---- Build READ(10) CDB at SPTD + SPTD_OFF_CDB ----
    lea     rcx, [rax + SPTD_OFF_CDB]
    mov     byte ptr [rcx + 0], SCSI_OP_READ10  ; Opcode

    ; LBA in bytes 2-5 (big-endian)
    mov     eax, r13d                   ; 32-bit LBA
    bswap   eax
    mov     dword ptr [rcx + 2], eax

    ; Transfer length = 1 sector in bytes 7-8 (big-endian)
    mov     word ptr [rcx + 7], 0100h   ; 1 sector, big-endian

    ; ---- DeviceIoControl ----
    lea     rax, [rsp + 32]             ; SPTD base
    mov     rcx, r12                    ; hDevice
    mov     edx, IOCTL_SCSI_PASS_THROUGH_DIRECT
    mov     r8, rax                     ; lpInBuffer = SPTD
    mov     r9d, SPTD_ALLOC + SENSE_BUFFER_SIZE  ; nInBufferSize
    ; Stack args for DeviceIoControl:
    ;   [rsp+32] = lpOutBuffer (same as inbuf for SPTD)
    ;   [rsp+40] = nOutBufferSize
    ;   [rsp+48] = lpBytesReturned
    ;   [rsp+56] = lpOverlapped
    lea     rax, [rsp + 32]
    mov     qword ptr [rsp + 0],  rax                       ; lpOutBuffer
    mov     dword ptr [rsp + 8],  SPTD_ALLOC + SENSE_BUFFER_SIZE  ; nOutBufferSize
    lea     rax, [rsp + 24]                                   ; scratch for bytesReturned
    mov     qword ptr [rsp + 16], rax                        ; lpBytesReturned
    mov     qword ptr [rsp + 24], 0                          ; lpOverlapped = NULL
    call    DeviceIoControl

    test    eax, eax
    jnz     @@hammer_check_scsi_status

    ; ---- DeviceIoControl failed — classify error ----
    call    GetLastError
    cmp     eax, ERROR_SEM_TIMEOUT
    je      @@hammer_retry_backoff

    cmp     eax, ERROR_GEN_FAILURE
    je      @@hammer_check_sense

    ; Unknown error — retry anyway
    jmp     @@hammer_retry_backoff

@@hammer_check_scsi_status:
    ; IOCTL succeeded — check SCSI status byte
    lea     rax, [rsp + 32]
    movzx   ecx, byte ptr [rax + SPTD_OFF_SCSISTATUS]
    test    ecx, ecx
    jz      @@hammer_success            ; SCSI GOOD status

    cmp     cl, 02h                     ; CHECK CONDITION
    je      @@hammer_check_sense

    ; Other SCSI status — retry
    jmp     @@hammer_retry_backoff

@@hammer_check_sense:
    ; Decode sense data
    lea     rax, [rsp + 32 + SPTD_SIZE] ; sense buffer
    movzx   ecx, byte ptr [rax + 2]    ; Sense Key at byte 2
    and     cl, 0Fh

    cmp     cl, SENSE_KEY_MEDIUM_ERROR
    je      @@hammer_medium_error

    cmp     cl, SENSE_KEY_HARDWARE_ERR
    je      @@hammer_hw_error

    cmp     cl, SENSE_KEY_ILLEGAL_REQ
    je      @@hammer_illegal

    cmp     cl, SENSE_KEY_NOT_READY
    je      @@hammer_retry_backoff      ; Drive spinning up — retry

    cmp     cl, SENSE_KEY_UNIT_ATTN
    je      @@hammer_retry_backoff      ; Unit attention — retry

    cmp     cl, SENSE_KEY_RECOVERED
    je      @@hammer_success            ; Recovered error = data OK

    ; Unknown sense key — retry
    jmp     @@hammer_retry_backoff

@@hammer_retry_backoff:
    inc     ebx
    ; Exponential backoff: Sleep(10 * retryCount) — caps at ~1s
    mov     ecx, ebx
    imul    ecx, 10
    cmp     ecx, 1000
    jle     @@do_sleep
    mov     ecx, 1000
@@do_sleep:
    call    Sleep
    jmp     @@hammer_loop

@@hammer_success:
    xor     eax, eax                    ; RECOVERY_OK
    mov     edx, r15d                   ; bytes transferred
    jmp     @@hammer_done

@@hammer_exhausted:
    mov     eax, RECOVERY_TIMEOUT
    xor     edx, edx
    jmp     @@hammer_done

@@hammer_medium_error:
    mov     eax, RECOVERY_MEDIUM_ERROR
    movzx   edx, byte ptr [rsp + 32 + SPTD_SIZE + 2]  ; sense key
    jmp     @@hammer_done

@@hammer_hw_error:
    mov     eax, RECOVERY_HARDWARE_ERROR
    movzx   edx, byte ptr [rsp + 32 + SPTD_SIZE + 2]
    jmp     @@hammer_done

@@hammer_illegal:
    mov     eax, RECOVERY_ILLEGAL_REQ
    movzx   edx, byte ptr [rsp + 32 + SPTD_SIZE + 2]

@@hammer_done:
    add     rsp, 160
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_scsi_hammer_read ENDP


; =============================================================================
; asm_scsi_inquiry_quick
; Fast SCSI INQUIRY with user-specified timeout.
;
; RCX = hDevice
; RDX = buffer for inquiry data (min 36 bytes)
; R8D = buffer size
; R9D = timeout in ms
;
; Returns: RAX = 0 on success, GetLastError on failure
; =============================================================================
asm_scsi_inquiry_quick PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 128                    ; SPTD + sense + shadow
    .allocstack 128
    .endprolog

    mov     rbx, rcx                    ; hDevice
    mov     r12, rdx                    ; buffer
    mov     r13d, r8d                   ; bufsize / timeout packed

    ; Zero SPTD region
    lea     rdi, [rsp + 32]
    mov     ecx, SPTD_ALLOC + SENSE_BUFFER_SIZE
    xor     eax, eax
    push    rdi
    rep     stosb
    pop     rdi

    ; Build SPTD for INQUIRY
    lea     rax, [rsp + 32]
    mov     word ptr  [rax + SPTD_OFF_LENGTH],      SPTD_SIZE
    mov     byte ptr  [rax + SPTD_OFF_CDBLENGTH],   6
    mov     byte ptr  [rax + SPTD_OFF_SENSEINFOLEN], SENSE_BUFFER_SIZE
    mov     byte ptr  [rax + SPTD_OFF_DATAIN],      SCSI_DATA_IN
    mov     dword ptr [rax + SPTD_OFF_DATAXFERLEN], r13d
    mov     dword ptr [rax + SPTD_OFF_TIMEOUT],     r9d
    mov     qword ptr [rax + SPTD_OFF_DATABUF],     r12
    mov     dword ptr [rax + SPTD_OFF_SENSEOFFSET], SPTD_SIZE

    ; CDB: INQUIRY (12h), allocation length in byte 4
    lea     rcx, [rax + SPTD_OFF_CDB]
    mov     byte ptr [rcx + 0], SCSI_OP_INQUIRY
    mov     byte ptr [rcx + 4], 36      ; Standard inquiry length

    ; DeviceIoControl
    mov     rcx, rbx
    mov     edx, IOCTL_SCSI_PASS_THROUGH_DIRECT
    lea     r8, [rsp + 32]
    mov     r9d, SPTD_ALLOC + SENSE_BUFFER_SIZE
    lea     rax, [rsp + 32]
    mov     qword ptr [rsp + 0],  rax
    mov     dword ptr [rsp + 8],  SPTD_ALLOC + SENSE_BUFFER_SIZE
    lea     rax, [rsp + 24]
    mov     qword ptr [rsp + 16], rax
    mov     qword ptr [rsp + 24], 0
    call    DeviceIoControl

    test    eax, eax
    jnz     @@inq_ok

    call    GetLastError
    jmp     @@inq_exit

@@inq_ok:
    xor     eax, eax

@@inq_exit:
    add     rsp, 128
    pop     r13
    pop     r12
    pop     rbx
    ret
asm_scsi_inquiry_quick ENDP


; =============================================================================
; asm_scsi_read_capacity
; Issue READ CAPACITY(10) to get total sectors and sector size.
;
; RCX = hDevice
; RDX = ptr to QWORD for total sectors (output)
; R8  = ptr to DWORD for sector size (output)
;
; Returns: RAX = 0 on success, error code on failure
; =============================================================================
asm_scsi_read_capacity PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 128
    .allocstack 128
    .endprolog

    mov     rbx, rcx                    ; hDevice
    mov     r12, rdx                    ; &totalSectors
    mov     r13, r8                     ; &sectorSize

    ; 8-byte response buffer on stack
    lea     r8, [rsp + 112]             ; capacity response at end of our alloc

    ; Zero SPTD
    lea     rdi, [rsp + 32]
    mov     ecx, SPTD_ALLOC + SENSE_BUFFER_SIZE
    xor     eax, eax
    push    rdi
    rep     stosb
    pop     rdi

    ; Build SPTD for READ CAPACITY(10)
    lea     rax, [rsp + 32]
    mov     word ptr  [rax + SPTD_OFF_LENGTH],      SPTD_SIZE
    mov     byte ptr  [rax + SPTD_OFF_CDBLENGTH],   10
    mov     byte ptr  [rax + SPTD_OFF_SENSEINFOLEN], SENSE_BUFFER_SIZE
    mov     byte ptr  [rax + SPTD_OFF_DATAIN],      SCSI_DATA_IN
    mov     dword ptr [rax + SPTD_OFF_DATAXFERLEN], 8   ; 8-byte response
    mov     dword ptr [rax + SPTD_OFF_TIMEOUT],     5000 ; 5 second timeout
    lea     rcx, [rsp + 112]
    mov     qword ptr [rax + SPTD_OFF_DATABUF],     rcx
    mov     dword ptr [rax + SPTD_OFF_SENSEOFFSET], SPTD_SIZE

    ; CDB: READ CAPACITY(10) = 0x25
    lea     rcx, [rax + SPTD_OFF_CDB]
    mov     byte ptr [rcx], SCSI_OP_READ_CAPACITY

    ; DeviceIoControl
    mov     rcx, rbx
    mov     edx, IOCTL_SCSI_PASS_THROUGH_DIRECT
    lea     r8, [rsp + 32]
    mov     r9d, SPTD_ALLOC + SENSE_BUFFER_SIZE
    lea     rax, [rsp + 32]
    mov     qword ptr [rsp + 0],  rax
    mov     dword ptr [rsp + 8],  SPTD_ALLOC + SENSE_BUFFER_SIZE
    lea     rax, [rsp + 24]
    mov     qword ptr [rsp + 16], rax
    mov     qword ptr [rsp + 24], 0
    call    DeviceIoControl

    test    eax, eax
    jz      @@cap_fail

    ; Parse response: bytes 0-3 = last LBA (big-endian), 4-7 = sector size (big-endian)
    lea     rax, [rsp + 112]
    mov     ecx, dword ptr [rax]        ; Last LBA (BE)
    bswap   ecx
    inc     ecx                         ; Total sectors = lastLBA + 1
    mov     dword ptr [r12], ecx        ; Store total sectors (32-bit)
    mov     dword ptr [r12 + 4], 0      ; Clear high DWORD

    mov     ecx, dword ptr [rax + 4]    ; Sector size (BE)
    bswap   ecx
    mov     dword ptr [r13], ecx

    xor     eax, eax
    jmp     @@cap_exit

@@cap_fail:
    call    GetLastError

@@cap_exit:
    add     rsp, 128
    pop     r13
    pop     r12
    pop     rbx
    ret
asm_scsi_read_capacity ENDP


; =============================================================================
; asm_scsi_request_sense
; Issue REQUEST SENSE command for detailed error information.
;
; RCX = hDevice
; RDX = sense buffer (min 18 bytes)
; R8D = buffer size
;
; Returns: RAX = 0 on success, error code on failure
; =============================================================================
asm_scsi_request_sense PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    sub     rsp, 128
    .allocstack 128
    .endprolog

    mov     rbx, rcx                    ; hDevice
    mov     r12, rdx                    ; sense dest

    ; Zero SPTD
    lea     rdi, [rsp + 32]
    mov     ecx, SPTD_ALLOC + SENSE_BUFFER_SIZE
    xor     eax, eax
    push    rdi
    rep     stosb
    pop     rdi

    lea     rax, [rsp + 32]
    mov     word ptr  [rax + SPTD_OFF_LENGTH],      SPTD_SIZE
    mov     byte ptr  [rax + SPTD_OFF_CDBLENGTH],   6
    mov     byte ptr  [rax + SPTD_OFF_SENSEINFOLEN], SENSE_BUFFER_SIZE
    mov     byte ptr  [rax + SPTD_OFF_DATAIN],      SCSI_DATA_IN
    mov     dword ptr [rax + SPTD_OFF_DATAXFERLEN], r8d
    mov     dword ptr [rax + SPTD_OFF_TIMEOUT],     2000
    mov     qword ptr [rax + SPTD_OFF_DATABUF],     r12
    mov     dword ptr [rax + SPTD_OFF_SENSEOFFSET], SPTD_SIZE

    ; CDB: REQUEST SENSE
    lea     rcx, [rax + SPTD_OFF_CDB]
    mov     byte ptr [rcx + 0], SCSI_OP_REQUEST_SENSE
    mov     byte ptr [rcx + 4], 18      ; Allocation length

    mov     rcx, rbx
    mov     edx, IOCTL_SCSI_PASS_THROUGH_DIRECT
    lea     r8, [rsp + 32]
    mov     r9d, SPTD_ALLOC + SENSE_BUFFER_SIZE
    lea     rax, [rsp + 32]
    mov     qword ptr [rsp + 0],  rax
    mov     dword ptr [rsp + 8],  SPTD_ALLOC + SENSE_BUFFER_SIZE
    lea     rax, [rsp + 24]
    mov     qword ptr [rsp + 16], rax
    mov     qword ptr [rsp + 24], 0
    call    DeviceIoControl

    test    eax, eax
    jnz     @@sense_ok

    call    GetLastError
    jmp     @@sense_exit

@@sense_ok:
    xor     eax, eax

@@sense_exit:
    add     rsp, 128
    pop     r12
    pop     rbx
    ret
asm_scsi_request_sense ENDP


; =============================================================================
; asm_extract_bridge_key
; Vendor-specific SECURITY PROTOCOL IN (0xA1) to extract AES key from
; WD My Book JMS567/NS1066 bridge EEPROM.
;
; RCX = hDevice
; RDX = 32-byte key output buffer
; R8D = bridge type (1=JMS567, 2=NS1066)
;
; Returns: RAX = 0 on success (key populated), -1 on failure
; =============================================================================
asm_extract_bridge_key PROC FRAME
    push    rbx
    .pushreg rbx
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    sub     rsp, 640                    ; SPTD + sense + 512-byte key sector
    .allocstack 640
    .endprolog

    mov     rbx, rcx                    ; hDevice
    mov     r12, rdx                    ; key dest (32 bytes)
    mov     r13d, r8d                   ; bridge type

    ; Zero SPTD region
    lea     rdi, [rsp + 32]
    mov     ecx, SPTD_ALLOC + SENSE_BUFFER_SIZE
    xor     eax, eax
    push    rdi
    rep     stosb
    pop     rdi

    ; Key sector buffer at rsp+128
    lea     rdi, [rsp + 128]
    mov     ecx, 512
    xor     eax, eax
    push    rdi
    rep     stosb
    pop     rdi

    ; Build SPTD for SECURITY PROTOCOL IN
    lea     rax, [rsp + 32]
    mov     word ptr  [rax + SPTD_OFF_LENGTH],      SPTD_SIZE
    mov     byte ptr  [rax + SPTD_OFF_CDBLENGTH],   12  ; 12-byte CDB for SECURITY PROTOCOL
    mov     byte ptr  [rax + SPTD_OFF_SENSEINFOLEN], SENSE_BUFFER_SIZE
    mov     byte ptr  [rax + SPTD_OFF_DATAIN],      SCSI_DATA_IN
    mov     dword ptr [rax + SPTD_OFF_DATAXFERLEN], 512
    mov     dword ptr [rax + SPTD_OFF_TIMEOUT],     5000  ; 5s for crypto op
    lea     rcx, [rsp + 128]
    mov     qword ptr [rax + SPTD_OFF_DATABUF],     rcx
    mov     dword ptr [rax + SPTD_OFF_SENSEOFFSET], SPTD_SIZE

    ; CDB: SECURITY PROTOCOL IN (A1h)
    lea     rcx, [rax + SPTD_OFF_CDB]
    mov     byte ptr [rcx + 0], SCSI_OP_SECURITY_IN
    mov     byte ptr [rcx + 1], 0       ; Security Protocol = 0 (Vendor)

    ; Protocol-specific field based on bridge type
    cmp     r13d, 1
    je      @@jms567_cdb
    cmp     r13d, 2
    je      @@ns1066_cdb
    jmp     @@key_fail                  ; Unknown bridge

@@jms567_cdb:
    ; JMS567: Protocol Specific = 0x0001 (key extraction)
    mov     byte ptr [rcx + 2], 00h     ; MSB
    mov     byte ptr [rcx + 3], 01h     ; LSB
    jmp     @@cdb_done

@@ns1066_cdb:
    ; NS1066: Protocol Specific = 0x00F0 (EEPROM page)
    mov     byte ptr [rcx + 2], 00h
    mov     byte ptr [rcx + 3], 0F0h
    jmp     @@cdb_done

@@cdb_done:
    ; Allocation length = 512 (bytes 6-9, big-endian)
    mov     byte ptr [rcx + 6], 0
    mov     byte ptr [rcx + 7], 0
    mov     byte ptr [rcx + 8], 02h     ; 0x200 = 512
    mov     byte ptr [rcx + 9], 0

    ; DeviceIoControl
    mov     rcx, rbx
    mov     edx, IOCTL_SCSI_PASS_THROUGH_DIRECT
    lea     r8, [rsp + 32]
    mov     r9d, SPTD_ALLOC + SENSE_BUFFER_SIZE
    lea     rax, [rsp + 32]
    mov     qword ptr [rsp + 0],  rax
    mov     dword ptr [rsp + 8],  SPTD_ALLOC + SENSE_BUFFER_SIZE
    lea     rax, [rsp + 24]
    mov     qword ptr [rsp + 16], rax
    mov     qword ptr [rsp + 24], 0
    call    DeviceIoControl

    test    eax, eax
    jz      @@key_fail

    ; Check SCSI status
    lea     rax, [rsp + 32]
    movzx   ecx, byte ptr [rax + SPTD_OFF_SCSISTATUS]
    test    ecx, ecx
    jnz     @@key_fail

    ; Copy first 32 bytes of key sector to output
    lea     rsi, [rsp + 128]
    mov     rdi, r12
    mov     ecx, 32
    rep     movsb

    xor     eax, eax                    ; Success
    jmp     @@key_exit

@@key_fail:
    mov     eax, -1

@@key_exit:
    add     rsp, 640
    pop     r13
    pop     r12
    pop     rbx
    ret
asm_extract_bridge_key ENDP

END
