; =============================================================================
; RawrXD_DiskRecoveryAgent.asm
; Enterprise-grade hardware recovery for failed USB bridges
; Targets WD My Book hardware encryption with JMS567/NS1066 bridges
; Pure x64 MASM - Uses RawrXD_Common.inc (zero external SDK dependency)
;
; Build (standalone):
;   ml64.exe /c /Zi /Zd RawrXD_DiskRecoveryAgent.asm
;   link.exe RawrXD_DiskRecoveryAgent.obj /subsystem:console /entry:DiskRecovery_Main
;          kernel32.lib ntdll.lib user32.lib
;
; Build (as library kernel linked into RawrXD-Shell):
;   Included in CMakeLists.txt ASM_KERNEL_SOURCES — exports C-callable procs.
;
; Pattern: PatchResult (RAX=0 success, RAX=NTSTATUS on error, RDX=detail)
; Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
; =============================================================================

option casemap:none

include RawrXD_Common.inc

; =============================================================================
; Additional EXTERN declarations not in RawrXD_Common.inc
; =============================================================================
EXTERNDEF DeviceIoControl:PROC
EXTERNDEF lstrlenA:PROC
EXTERNDEF WriteFile:PROC
EXTERNDEF ReadFile:PROC
EXTERNDEF SetFilePointerEx:PROC
EXTERNDEF ExitProcess:PROC
EXTERNDEF GetStdHandle:PROC
EXTERNDEF wsprintfA:PROC
EXTERNDEF CreateFileA:PROC
EXTERNDEF Sleep:PROC
EXTERNDEF GetLastError:PROC
EXTERNDEF CloseHandle:PROC
EXTERNDEF FlushFileBuffers:PROC

; =============================================================================
; Win32 constants not in RawrXD_Common.inc
; =============================================================================
STD_OUTPUT_HANDLE        equ -11
FILE_CURRENT             equ 1

; =============================================================================
; NT Status Codes (supplement RawrXD_Common.inc)
; =============================================================================
STATUS_TIMEOUT               equ 00000102h
STATUS_DEVICE_DATA_ERROR     equ 0C000009Ch

; =============================================================================
; IOCTLs
; =============================================================================
IOCTL_SCSI_PASS_THROUGH_DIRECT equ 04D014h
IOCTL_STORAGE_QUERY_PROPERTY   equ 02D1400h
FSCTL_SET_SPARSE               equ 000900C4h

; =============================================================================
; SCSI Commands
; =============================================================================
SCSI_READ10              equ 28h
SCSI_READ12              equ 0A8h
SCSI_INQUIRY             equ 12h
SCSI_REQUEST_SENSE       equ 03h
SCSI_MODE_SENSE          equ 1Ah
SCSI_READ_CAPACITY       equ 25h
; WD Vendor Specific
WD_SECURITY_PROTOCOL     equ 0A1h     ; Vendor specific key extraction
WD_JMS567_EEPROM_READ    equ 0E0h     ; JMicron bridge EEPROM dump

; =============================================================================
; SCSI Data Direction
; =============================================================================
SCSI_IOCTL_DATA_OUT      equ 0
SCSI_IOCTL_DATA_IN       equ 1
SCSI_IOCTL_DATA_UNSPEC   equ 2

; =============================================================================
; File I/O constants (supplement RawrXD_Common.inc)
; =============================================================================
FILE_SHARE_WRITE             equ 2
FILE_FLAG_NO_BUFFERING       equ 20000000h
FILE_FLAG_WRITE_THROUGH      equ 80000000h
FILE_FLAG_SEQUENTIAL_SCAN    equ 08000000h
FILE_ATTRIBUTE_NORMAL        equ 80h

; =============================================================================
; Error codes
; =============================================================================
ERROR_SEM_TIMEOUT        equ 121
ERROR_GEN_FAILURE        equ 31

; =============================================================================
; Config Constants
; =============================================================================
MAX_RETRIES              equ 100
TIMEOUT_MS               equ 2000
SECTOR_SIZE              equ 4096     ; USB 3.0 usually 4K
BUFFER_SIZE              equ 1048576  ; 1MB transfer chunks
MAX_BAD_SECTORS          equ 65536    ; Max entries in bad map
CHECKPOINT_INTERVAL      equ 1000     ; Save progress every N sectors

; =============================================================================
; Bridge type enum
; =============================================================================
BRIDGE_UNKNOWN           equ 0
BRIDGE_JMS567            equ 1
BRIDGE_NS1066            equ 2

; =============================================================================
; SCSI_PASS_THROUGH_DIRECT (x64 layout, matches Windows SDK)
; Total size: 56 bytes (without sense data appended)
; NOTE: ml64 struct layout differs from SDK due to packing.
; We use a flat byte buffer and access via offsets for correctness.
; =============================================================================
SPTD_LENGTH              equ 0       ; WORD   (2)
SPTD_SCSISTATUS          equ 2       ; BYTE   (1)
SPTD_PATHID              equ 3       ; BYTE   (1)
SPTD_TARGETID            equ 4       ; BYTE   (1)
SPTD_LUN                 equ 5       ; BYTE   (1)
SPTD_CDBLENGTH           equ 6       ; BYTE   (1)
SPTD_SENSEINFOLENGTH     equ 7       ; BYTE   (1)
SPTD_DATAIN              equ 8       ; BYTE   (1)  0=None,1=In,2=Out
; padding 3 bytes to align DataTransferLength at +12
SPTD_DATATRANSFERLENGTH  equ 12      ; DWORD  (4)
SPTD_TIMEOUTVALUE        equ 16      ; DWORD  (4)
; padding 4 bytes to align DataBuffer at +24
SPTD_DATABUFFER          equ 24      ; QWORD  (8)
SPTD_SENSEINFOOFFSET     equ 32      ; DWORD  (4)
; padding 4 bytes to align Cdb at +40  (NOTE: on x64 it's actually at offset 36+padding)
SPTD_CDB                 equ 36      ; BYTE[16]
SPTD_TOTAL_SIZE          equ 56      ; 56 bytes total on x64

; We append sense buffer right after SPTD
SENSE_OFFSET             equ SPTD_TOTAL_SIZE
SENSE_SIZE               equ 32
SPTD_WITH_SENSE          equ SPTD_TOTAL_SIZE + SENSE_SIZE  ; 88 bytes

; =============================================================================
; DISK_RECOVERY_CONTEXT — Tracks the full recovery session
; =============================================================================
DISK_RECOVERY_CONTEXT STRUCT 8
    hSource              QWORD   ?   ; Source drive handle
    hTarget              QWORD   ?   ; Target image file handle
    hLog                 QWORD   ?   ; Log file handle
    hBadSectorMap        QWORD   ?   ; Bad sector map file handle
    TotalSectors         QWORD   ?   ; Drive capacity in sectors
    CurrentLBA           QWORD   ?   ; Resume position
    GoodSectors          QWORD   ?   ; Successful reads
    BadSectors           QWORD   ?   ; Failed reads
    BridgeType           DWORD   ?   ; BRIDGE_* enum
    _pad0                DWORD   ?   ; Alignment padding
    EncryptionKey        BYTE 32 dup(?) ; Extracted AES-256 key
    AbortSignal          BYTE    ?   ; Thread-safe abort flag
    KeyExtracted         BYTE    ?   ; 1 if key was successfully extracted
    _pad1                BYTE  6 dup(?) ; Pad to 8-byte boundary
DISK_RECOVERY_CONTEXT ENDS

; =============================================================================
; .data — Static strings and global context
; =============================================================================
.data

    ; Device path template (modified at offset 17 for drive number)
    szDevicePathBase     db "\\.\PhysicalDrive", 0, 0, 0, 0
    szTargetImage        db "D:\Recovery\WD_Rescue.bin", 0
    szLogFile            db "D:\Recovery\recovery.log", 0
    szBadMap             db "D:\Recovery\badsectors.map", 0
    szKeyFile            db "D:\Recovery\aes_key.bin", 0

    ; Console output templates
    szBanner             db 13,10
                         db "================================================",13,10
                         db "  RawrXD Disk Recovery Agent v1.0",13,10
                         db "  Hardware-level extraction for dying USB bridges",13,10
                         db "  (c) RawrXD Project - Zero dependency MASM64",13,10
                         db "================================================",13,10,0

    szScanning           db "[*] Scanning PhysicalDrive0-15 for dying WD devices...",13,10,0
    szFoundDrive         db "[+] Found candidate: PhysicalDrive",0
    szDriveNum           db "0",13,10,0    ; Filled dynamically
    szNoDevice           db "[-] No dying WD My Book device found.",13,10,0
    szOpening            db "[*] Opening device with SCSI passthrough...",13,10,0
    szOpenFail           db "[-] Failed to open device handle.",13,10,0
    szInitOk             db "[+] Recovery context initialized.",13,10,0
    szHammerStart        db "[*] Initiating SCSI hammer protocol...",13,10,0
    szKeyExtract         db "[*] Attempting hardware key extraction from bridge EEPROM...",13,10,0
    szKeySuccess         db "[+] AES-256 key extracted and saved!",13,10,0
    szKeyFail            db "[-] Key extraction failed (bridge unresponsive).",13,10,0
    szSectorOk           db "+",0          ; Single char for fast progress
    szSectorBad          db "X",0          ; Single char for bad sector
    szCheckpoint         db 13,10,"[*] Checkpoint: LBA ",0
    szStatHeader         db 13,10,"=== Recovery Statistics ===",13,10,0
    szStatTotal          db "  Processed:  ",0
    szStatGood           db "  Good:       ",0
    szStatBad            db "  Bad:        ",0
    szStatPct            db "  Progress:   ",0
    szPctSign            db "%%",13,10,0
    szNewLine            db 13,10,0
    szComplete           db 13,10,"[+] Recovery session complete.",13,10,0
    szInitError          db "[-] Initialization error.",13,10,0
    szBridgeJMS567       db "[+] Bridge identified: JMicron JMS567",13,10,0
    szBridgeNS1066       db "[+] Bridge identified: Norelsys NS1066",13,10,0
    szBridgeUnknown      db "[?] Bridge type unknown — generic SCSI mode.",13,10,0

    ; Formatting scratch buffer
    align 8
    FmtBuf               db 256 dup(0)

; =============================================================================
; .data? — Uninitialized (BSS) data
; =============================================================================
.data?

    ; Transfer buffer (page-alignment via align 16 — max for .data? segment)
    align 16
    TransferBuffer       db BUFFER_SIZE dup(?)

    ; Global recovery context
    align 8
    g_Context            DISK_RECOVERY_CONTEXT <>

    ; Console handle cache
    g_hStdOut            QWORD ?

; =============================================================================
; .code
; =============================================================================
.code

; =============================================================================
; ConsolePrint — Write a null-terminated string to stdout
; RCX = ptr to string
; Clobbers: RAX, RCX, RDX, R8, R9
; =============================================================================
ConsolePrint PROC
    push rbx
    push rsi
    sub  rsp, 40              ; Shadow + local (2 pushes + 40 = 56 ≡ 8+56=64 → 0 mod 16)

    mov  rsi, rcx             ; Save string ptr

    ; lstrlenA to get length
    ; rcx already has string
    call lstrlenA
    mov  rbx, rax             ; RBX = length

    test rbx, rbx
    jz   cp_done

    ; GetStdHandle(STD_OUTPUT_HANDLE)
    mov  ecx, STD_OUTPUT_HANDLE
    call GetStdHandle
    test rax, rax
    jz   cp_done

    ; WriteFile(hStdOut, pStr, len, &bytesWritten, NULL)
    mov  rcx, rax             ; hFile
    mov  rdx, rsi             ; lpBuffer
    mov  r8d, ebx             ; nNumberOfBytesToWrite
    lea  r9, [rsp+36]         ; lpNumberOfBytesWritten (inside shadow, not overlapping 5th arg)
    mov  qword ptr [rsp+32], 0 ; lpOverlapped = NULL (5th arg at correct position)
    call WriteFile

cp_done:
    add  rsp, 40
    pop  rsi
    pop  rbx
    ret
ConsolePrint ENDP

; =============================================================================
; PrintU64 — Print a QWORD as decimal to console
; RCX = value
; Clobbers: RAX, RCX, RDX, R8, R9
; =============================================================================
PrintU64 PROC
    push rbx
    push rsi
    push rdi
    sub  rsp, 48

    mov  rbx, rcx             ; Value to print

    ; Manual itoa (decimal): fill FmtBuf right-to-left
    lea  rdi, FmtBuf
    add  rdi, 30              ; End of scratch area
    mov  byte ptr [rdi], 0    ; Null terminator
    dec  rdi

    mov  rax, rbx
    test rax, rax
    jnz  pu64_loop
    mov  byte ptr [rdi], '0'
    dec  rdi
    jmp  pu64_print

pu64_loop:
    test rax, rax
    jz   pu64_print
    xor  edx, edx
    mov  rcx, 10
    div  rcx                  ; RAX = quotient, RDX = remainder
    add  dl, '0'
    mov  byte ptr [rdi], dl
    dec  rdi
    jmp  pu64_loop

pu64_print:
    inc  rdi                  ; Point to first digit
    mov  rcx, rdi
    call ConsolePrint

    add  rsp, 48
    pop  rdi
    pop  rsi
    pop  rbx
    ret
PrintU64 ENDP

; =============================================================================
; ScsiInquiryQuick — Fast INQUIRY with custom timeout
; RCX = hDevice, EDX = TimeoutMS
; Returns: RAX = 0 success, nonzero = GetLastError code
; Clobbers: all volatile regs
; =============================================================================
ScsiInquiryQuick PROC
    LOCAL sptd_buf[SPTD_WITH_SENSE]:BYTE
    LOCAL inquiryData[64]:BYTE
    LOCAL bytesRet:DWORD

    push rbx
    push rdi
    push r12                  ; R12 is non-volatile — must save

    mov  rbx, rcx             ; hDevice
    mov  r12d, edx            ; Timeout

    ; Zero SPTD buffer
    lea  rdi, sptd_buf
    mov  ecx, SPTD_WITH_SENSE
    xor  eax, eax
    rep  stosb

    ; Fill SPTD fields
    lea  rdi, sptd_buf
    mov  word ptr  [rdi+SPTD_LENGTH], SPTD_TOTAL_SIZE
    mov  byte ptr  [rdi+SPTD_CDBLENGTH], 6          ; INQUIRY CDB = 6 bytes
    mov  byte ptr  [rdi+SPTD_SENSEINFOLENGTH], SENSE_SIZE
    mov  byte ptr  [rdi+SPTD_DATAIN], SCSI_IOCTL_DATA_IN
    mov  dword ptr [rdi+SPTD_DATATRANSFERLENGTH], 36 ; Standard INQUIRY response
    mov  dword ptr [rdi+SPTD_TIMEOUTVALUE], r12d
    lea  rax, inquiryData
    mov  qword ptr [rdi+SPTD_DATABUFFER], rax
    mov  dword ptr [rdi+SPTD_SENSEINFOOFFSET], SENSE_OFFSET

    ; CDB: INQUIRY (12h, 0, 0, 0, 36, 0)
    mov  byte ptr  [rdi+SPTD_CDB+0], SCSI_INQUIRY
    mov  byte ptr  [rdi+SPTD_CDB+4], 36             ; Allocation length

    ; DeviceIoControl (8 params: 4 reg + 4 stack → need 64 bytes)
    sub  rsp, 64
    mov  rcx, rbx                                    ; hDevice
    mov  edx, IOCTL_SCSI_PASS_THROUGH_DIRECT         ; dwIoControlCode
    lea  r8, sptd_buf                                ; lpInBuffer
    mov  r9d, SPTD_WITH_SENSE                        ; nInBufferSize
    lea  rax, sptd_buf
    mov  qword ptr [rsp+32], rax                     ; lpOutBuffer
    mov  qword ptr [rsp+40], SPTD_WITH_SENSE         ; nOutBufferSize
    lea  rax, bytesRet
    mov  qword ptr [rsp+48], rax                     ; lpBytesReturned
    mov  qword ptr [rsp+56], 0                       ; lpOverlapped
    call DeviceIoControl
    add  rsp, 64

    test eax, eax
    jnz  inq_ok

    call GetLastError
    jmp  inq_exit

inq_ok:
    xor  eax, eax           ; Success

inq_exit:
    pop  r12
    pop  rdi
    pop  rbx
    ret
ScsiInquiryQuick ENDP

; =============================================================================
; WdPingBridge — Identify WD bridge controller type via vendor SCSI commands
; RCX = hDevice
; Returns: EAX = BRIDGE_* type (0=None, 1=JMS567, 2=NS1066)
; =============================================================================
WdPingBridge PROC
    LOCAL sptd_buf[SPTD_WITH_SENSE]:BYTE
    LOCAL resp[64]:BYTE
    LOCAL bytesRet:DWORD

    push rbx
    push rdi

    mov  rbx, rcx

    ; === Try JMS567 vendor command ===
    lea  rdi, sptd_buf
    mov  ecx, SPTD_WITH_SENSE
    xor  eax, eax
    rep  stosb

    lea  rdi, sptd_buf
    mov  word ptr  [rdi+SPTD_LENGTH], SPTD_TOTAL_SIZE
    mov  byte ptr  [rdi+SPTD_CDBLENGTH], 10
    mov  byte ptr  [rdi+SPTD_SENSEINFOLENGTH], SENSE_SIZE
    mov  byte ptr  [rdi+SPTD_DATAIN], SCSI_IOCTL_DATA_IN
    mov  dword ptr [rdi+SPTD_DATATRANSFERLENGTH], 64
    mov  dword ptr [rdi+SPTD_TIMEOUTVALUE], 1000     ; 1s timeout
    lea  rax, resp
    mov  qword ptr [rdi+SPTD_DATABUFFER], rax
    mov  dword ptr [rdi+SPTD_SENSEINFOOFFSET], SENSE_OFFSET

    ; Vendor CDB: E0 'J' 'M' 'S' '5' ...
    mov  byte ptr  [rdi+SPTD_CDB+0], WD_JMS567_EEPROM_READ
    mov  byte ptr  [rdi+SPTD_CDB+1], 'J'
    mov  byte ptr  [rdi+SPTD_CDB+2], 'M'
    mov  byte ptr  [rdi+SPTD_CDB+3], 'S'
    mov  byte ptr  [rdi+SPTD_CDB+4], '5'

    sub  rsp, 64
    mov  rcx, rbx
    mov  edx, IOCTL_SCSI_PASS_THROUGH_DIRECT
    lea  r8, sptd_buf
    mov  r9d, SPTD_WITH_SENSE
    lea  rax, sptd_buf
    mov  qword ptr [rsp+32], rax
    mov  dword ptr [rsp+40], SPTD_WITH_SENSE
    lea  rax, bytesRet
    mov  qword ptr [rsp+48], rax
    mov  qword ptr [rsp+56], 0
    call DeviceIoControl
    add  rsp, 64

    test eax, eax
    jz   try_ns1066

    mov  eax, BRIDGE_JMS567
    jmp  ping_exit

try_ns1066:
    ; === Try NS1066 vendor command ===
    lea  rdi, sptd_buf
    mov  ecx, SPTD_WITH_SENSE
    xor  eax, eax
    rep  stosb

    lea  rdi, sptd_buf
    mov  word ptr  [rdi+SPTD_LENGTH], SPTD_TOTAL_SIZE
    mov  byte ptr  [rdi+SPTD_CDBLENGTH], 10
    mov  byte ptr  [rdi+SPTD_SENSEINFOLENGTH], SENSE_SIZE
    mov  byte ptr  [rdi+SPTD_DATAIN], SCSI_IOCTL_DATA_IN
    mov  dword ptr [rdi+SPTD_DATATRANSFERLENGTH], 64
    mov  dword ptr [rdi+SPTD_TIMEOUTVALUE], 1000
    lea  rax, resp
    mov  qword ptr [rdi+SPTD_DATABUFFER], rax
    mov  dword ptr [rdi+SPTD_SENSEINFOOFFSET], SENSE_OFFSET

    mov  byte ptr  [rdi+SPTD_CDB+0], WD_JMS567_EEPROM_READ
    mov  byte ptr  [rdi+SPTD_CDB+1], 'N'
    mov  byte ptr  [rdi+SPTD_CDB+2], 'S'
    mov  byte ptr  [rdi+SPTD_CDB+3], '1'
    mov  byte ptr  [rdi+SPTD_CDB+4], '0'

    sub  rsp, 64
    mov  rcx, rbx
    mov  edx, IOCTL_SCSI_PASS_THROUGH_DIRECT
    lea  r8, sptd_buf
    mov  r9d, SPTD_WITH_SENSE
    lea  rax, sptd_buf
    mov  qword ptr [rsp+32], rax
    mov  dword ptr [rsp+40], SPTD_WITH_SENSE
    lea  rax, bytesRet
    mov  qword ptr [rsp+48], rax
    mov  qword ptr [rsp+56], 0
    call DeviceIoControl
    add  rsp, 64

    test eax, eax
    jz   no_bridge

    mov  eax, BRIDGE_NS1066
    jmp  ping_exit

no_bridge:
    xor  eax, eax

ping_exit:
    pop  rdi
    pop  rbx
    ret
WdPingBridge ENDP

; =============================================================================
; FindDyingDrive — Scans PhysicalDrive0-15 for WD signature
; Returns: EAX = drive number (0-15), or -1 if not found
; =============================================================================
FindDyingDrive PROC
    LOCAL driveNum:DWORD
    LOCAL hDevice:QWORD
    LOCAL devPath[32]:BYTE

    push rbx
    push rsi
    push rdi
    push r12
    sub  rsp, 64              ; 4 pushes (32) + 64 = 96 → 0 mod 16; room for 7-param CreateFileA

    mov  driveNum, 0

scan_loop:
    cmp  driveNum, 15
    jg   not_found

    ; Build path: \\.\PhysicalDriveN
    lea  rdi, devPath
    lea  rsi, szDevicePathBase
    mov  ecx, 17               ; Length of "\\.\PhysicalDrive"
    rep  movsb
    mov  eax, driveNum
    add  al, '0'
    mov  byte ptr [rdi], al
    mov  byte ptr [rdi+1], 0

    ; CreateFileA — NO_BUFFERING prevents Windows metadata probing
    lea  rcx, devPath
    mov  edx, GENERIC_READ
    mov  r8d, FILE_SHARE_READ or FILE_SHARE_WRITE
    xor  r9d, r9d             ; lpSecurityAttributes = NULL
    mov  qword ptr [rsp+32], OPEN_EXISTING
    mov  qword ptr [rsp+40], FILE_FLAG_NO_BUFFERING or FILE_FLAG_WRITE_THROUGH
    mov  qword ptr [rsp+48], 0
    call CreateFileA

    cmp  rax, INVALID_HANDLE_VALUE
    je   next_drive

    mov  hDevice, rax

    ; Quick SCSI INQUIRY with 500ms timeout
    mov  rcx, rax
    mov  edx, 500
    call ScsiInquiryQuick

    test eax, eax
    jz   inquiry_ok

    ; Inquiry failed — if timeout, might be our dying drive
    cmp  eax, ERROR_SEM_TIMEOUT
    jne  close_next

    ; Timeout → try vendor ping to confirm WD bridge
    mov  rcx, hDevice
    call WdPingBridge
    test eax, eax
    jnz  found_it

    jmp  close_next

inquiry_ok:
    ; Inquiry succeeded — still check if it's a WD bridge
    mov  rcx, hDevice
    call WdPingBridge
    test eax, eax
    jnz  found_it

close_next:
    mov  rcx, hDevice
    call CloseHandle

next_drive:
    inc  driveNum
    jmp  scan_loop

found_it:
    ; EAX already has bridge type — save it
    mov  g_Context.BridgeType, eax

    mov  rcx, hDevice
    call CloseHandle

    mov  eax, driveNum
    jmp  find_exit

not_found:
    mov  eax, -1

find_exit:
    add  rsp, 64
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    ret
FindDyingDrive ENDP

; =============================================================================
; ExtractEncryptionKey — Dump AES-256 key from bridge EEPROM
; RCX = ptr to DISK_RECOVERY_CONTEXT
; Returns: RAX = 1 success (key in context), 0 = fail
; =============================================================================
ExtractEncryptionKey PROC
    LOCAL sptd_buf[SPTD_WITH_SENSE]:BYTE
    LOCAL keyBuf[512]:BYTE
    LOCAL bytesRet:DWORD

    push rbx
    push rsi                  ; RSI used by rep movsb — must save
    push rdi

    mov  rbx, rcx

    ; Only works if bridge type is known
    cmp  (DISK_RECOVERY_CONTEXT ptr [rbx]).BridgeType, BRIDGE_UNKNOWN
    je   ek_fail

    ; Zero SPTD
    lea  rdi, sptd_buf
    mov  ecx, SPTD_WITH_SENSE
    xor  eax, eax
    rep  stosb

    ; Fill SPTD for SECURITY PROTOCOL IN (A1h)
    lea  rdi, sptd_buf
    mov  word ptr  [rdi+SPTD_LENGTH], SPTD_TOTAL_SIZE
    mov  byte ptr  [rdi+SPTD_CDBLENGTH], 12         ; 12-byte CDB
    mov  byte ptr  [rdi+SPTD_SENSEINFOLENGTH], SENSE_SIZE
    mov  byte ptr  [rdi+SPTD_DATAIN], SCSI_IOCTL_DATA_IN
    mov  dword ptr [rdi+SPTD_DATATRANSFERLENGTH], 512
    mov  dword ptr [rdi+SPTD_TIMEOUTVALUE], 5000     ; 5s for crypto op
    lea  rax, keyBuf
    mov  qword ptr [rdi+SPTD_DATABUFFER], rax
    mov  dword ptr [rdi+SPTD_SENSEINFOOFFSET], SENSE_OFFSET

    ; CDB: SECURITY PROTOCOL IN
    ;   [0] = A1h (opcode)
    ;   [1] = 00h (security protocol = vendor)
    ;   [2] = 01h (protocol specific = key extraction)
    ;   [6-9] = allocation length (512 = 0x200)
    mov  byte ptr  [rdi+SPTD_CDB+0], WD_SECURITY_PROTOCOL
    mov  byte ptr  [rdi+SPTD_CDB+1], 0
    mov  byte ptr  [rdi+SPTD_CDB+2], 01h
    mov  byte ptr  [rdi+SPTD_CDB+6], 0
    mov  byte ptr  [rdi+SPTD_CDB+7], 0
    mov  byte ptr  [rdi+SPTD_CDB+8], 02h            ; 0x200 = 512
    mov  byte ptr  [rdi+SPTD_CDB+9], 0

    ; DeviceIoControl
    sub  rsp, 64
    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).hSource
    mov  edx, IOCTL_SCSI_PASS_THROUGH_DIRECT
    lea  r8, sptd_buf
    mov  r9d, SPTD_WITH_SENSE
    lea  rax, sptd_buf
    mov  qword ptr [rsp+32], rax
    mov  qword ptr [rsp+40], SPTD_WITH_SENSE
    lea  rax, bytesRet
    mov  qword ptr [rsp+48], rax
    mov  qword ptr [rsp+56], 0
    call DeviceIoControl
    add  rsp, 64

    test eax, eax
    jz   ek_fail

    ; Copy first 32 bytes of response as AES-256 key
    lea  rsi, keyBuf
    lea  rdi, (DISK_RECOVERY_CONTEXT ptr [rbx]).EncryptionKey
    mov  ecx, 32
    rep  movsb

    mov  (DISK_RECOVERY_CONTEXT ptr [rbx]).KeyExtracted, 1

    ; Save key to separate file
    mov  rcx, rbx
    call SaveEncryptionKey

    mov  eax, 1
    jmp  ek_exit

ek_fail:
    xor  eax, eax

ek_exit:
    pop  rdi
    pop  rsi
    pop  rbx
    ret
ExtractEncryptionKey ENDP

; =============================================================================
; SaveEncryptionKey — Write extracted key to D:\Recovery\aes_key.bin
; RCX = ptr to DISK_RECOVERY_CONTEXT
; =============================================================================
SaveEncryptionKey PROC
    LOCAL bytesWritten:DWORD

    push rbx
    push r12                  ; R12 is non-volatile — must save
    sub  rsp, 56              ; 2 pushes (16) + LOCAL frame + 56 → aligned; room for 7-param CreateFileA

    mov  rbx, rcx

    ; CreateFileA for key file
    lea  rcx, szKeyFile
    mov  edx, GENERIC_WRITE
    mov  r8d, FILE_SHARE_READ
    xor  r9d, r9d
    mov  qword ptr [rsp+32], CREATE_ALWAYS
    mov  qword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov  qword ptr [rsp+48], 0
    call CreateFileA

    cmp  rax, INVALID_HANDLE_VALUE
    je   sk_done

    mov  r12, rax             ; hFile

    ; WriteFile(hFile, key, 32, &bytesWritten, NULL)
    mov  rcx, r12
    lea  rdx, (DISK_RECOVERY_CONTEXT ptr [rbx]).EncryptionKey
    mov  r8d, 32
    lea  r9, bytesWritten
    mov  qword ptr [rsp+32], 0
    call WriteFile

    mov  rcx, r12
    call FlushFileBuffers

    mov  rcx, r12
    call CloseHandle

sk_done:
    add  rsp, 56
    pop  r12
    pop  rbx
    ret
SaveEncryptionKey ENDP

; =============================================================================
; HammerReadSector — High-reliability sector read with retry + sense decoding
; RCX = ptr to Context
; RDX = LBA (QWORD)
; R8  = ptr to output buffer
; Returns: RAX = 1 success, 0 = permanent failure (logged to bad map)
; =============================================================================
HammerReadSector PROC
    LOCAL sptd_buf[SPTD_WITH_SENSE]:BYTE
    LOCAL bytesRet:DWORD
    LOCAL retryCount:DWORD

    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub  rsp, 80              ; 6 pushes (48) + 80 = 128 → 0 mod 16; room for 8-param DeviceIoControl

    mov  rbx, rcx             ; Context ptr
    mov  r12, rdx             ; LBA
    mov  r13, r8              ; Buffer
    mov  retryCount, 0

retry_read:
    mov  eax, retryCount
    cmp  eax, MAX_RETRIES
    jge  sector_dead

    ; Zero SPTD
    lea  rdi, sptd_buf
    mov  ecx, SPTD_WITH_SENSE
    xor  eax, eax
    rep  stosb

    ; Fill SPTD for READ(10)
    lea  rdi, sptd_buf
    mov  word ptr  [rdi+SPTD_LENGTH], SPTD_TOTAL_SIZE
    mov  byte ptr  [rdi+SPTD_CDBLENGTH], 10
    mov  byte ptr  [rdi+SPTD_SENSEINFOLENGTH], SENSE_SIZE
    mov  byte ptr  [rdi+SPTD_DATAIN], SCSI_IOCTL_DATA_IN
    mov  dword ptr [rdi+SPTD_DATATRANSFERLENGTH], SECTOR_SIZE
    mov  dword ptr [rdi+SPTD_TIMEOUTVALUE], TIMEOUT_MS
    mov  qword ptr [rdi+SPTD_DATABUFFER], r13
    mov  dword ptr [rdi+SPTD_SENSEINFOOFFSET], SENSE_OFFSET

    ; CDB: READ(10) = 28h, LBA[4 bytes big-endian], 0, TransferLen[2 bytes BE]
    mov  byte ptr  [rdi+SPTD_CDB+0], SCSI_READ10

    ; Write LBA in big-endian at CDB offsets 2-5
    mov  eax, r12d             ; Lower 32 bits of LBA
    bswap eax
    mov  dword ptr [rdi+SPTD_CDB+2], eax

    ; Transfer length = 1 sector, big-endian (0x0001)
    mov  byte ptr  [rdi+SPTD_CDB+7], 0
    mov  byte ptr  [rdi+SPTD_CDB+8], 1

    ; DeviceIoControl
    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).hSource
    mov  edx, IOCTL_SCSI_PASS_THROUGH_DIRECT
    lea  r8, sptd_buf
    mov  r9d, SPTD_WITH_SENSE
    lea  rax, sptd_buf
    mov  qword ptr [rsp+32], rax
    mov  qword ptr [rsp+40], SPTD_WITH_SENSE
    lea  rax, bytesRet
    mov  qword ptr [rsp+48], rax
    mov  qword ptr [rsp+56], 0
    call DeviceIoControl

    test eax, eax
    jnz  hr_check_scsi_status

    ; Win32 error — check type
    call GetLastError

    cmp  eax, ERROR_SEM_TIMEOUT
    je   hr_retry

    cmp  eax, ERROR_GEN_FAILURE
    je   hr_check_sense

    ; Unknown error — retry anyway
    jmp  hr_retry

hr_check_scsi_status:
    ; DeviceIoControl succeeded — check SCSI status byte
    lea  rdi, sptd_buf
    movzx eax, byte ptr [rdi+SPTD_SCSISTATUS]
    test al, al
    jz   read_success          ; SCSI GOOD status = 0

    cmp  al, 02h               ; CHECK CONDITION
    je   hr_check_sense

    ; Other SCSI status — retry
    jmp  hr_retry

hr_check_sense:
    ; Decode sense data: SenseKey at sense[2] bits 0-3
    lea  rdi, sptd_buf
    add  rdi, SENSE_OFFSET
    movzx eax, byte ptr [rdi+2]
    and  al, 0Fh

    cmp  al, 03h               ; MEDIUM ERROR — permanent
    je   sector_dead

    cmp  al, 05h               ; ILLEGAL REQUEST — permanent
    je   sector_dead

    ; Hardware error (04h) or other — may recover with retry
    jmp  hr_retry

hr_retry:
    inc  retryCount

    ; Exponential backoff: Sleep(10 * retryCount)
    mov  ecx, retryCount      ; Read full DWORD (was word ptr — fragile)
    imul ecx, ecx, 10
    call Sleep

    jmp  retry_read

sector_dead:
    ; Log to bad sector map
    mov  rcx, rbx
    mov  rdx, r12
    call LogBadSector

    xor  eax, eax              ; Return 0 = fail
    jmp  hr_exit

read_success:
    mov  eax, 1                ; Return 1 = success

hr_exit:
    add  rsp, 80
    pop  r14
    pop  r13
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    ret
HammerReadSector ENDP

; =============================================================================
; LogBadSector — Append LBA to bad sector map (binary: 8 bytes per entry)
; RCX = Context ptr, RDX = LBA
; =============================================================================
LogBadSector PROC
    LOCAL bytesWritten:DWORD
    LOCAL lbaValue:QWORD

    push rbx
    sub  rsp, 40              ; 1 push (8) + LOCAL frame + 40 → aligned

    mov  rbx, rcx
    mov  lbaValue, rdx

    ; WriteFile(hBadSectorMap, &lbaValue, 8, &bytesWritten, NULL)
    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).hBadSectorMap
    lea  rdx, lbaValue
    mov  r8d, 8
    lea  r9, bytesWritten
    mov  qword ptr [rsp+32], 0
    call WriteFile

    inc  (DISK_RECOVERY_CONTEXT ptr [rbx]).BadSectors

    add  rsp, 40
    pop  rbx
    ret
LogBadSector ENDP

; =============================================================================
; SaveCheckpoint — Write resume position to log file
; RCX = Context ptr
; =============================================================================
SaveCheckpoint PROC
    LOCAL bytesWritten:DWORD
    LOCAL liZero:QWORD

    push rbx
    sub  rsp, 40              ; 1 push (8) + LOCAL frame + 40 → aligned

    mov  rbx, rcx

    ; Print checkpoint message
    lea  rcx, szCheckpoint
    call ConsolePrint
    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).CurrentLBA
    call PrintU64
    lea  rcx, szNewLine
    call ConsolePrint

    ; Seek log to beginning: SetFilePointerEx(hLog, 0, NULL, FILE_BEGIN)
    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).hLog
    xor  edx, edx             ; liDistanceToMove (low)
    xor  r8d, r8d             ; lpNewFilePointer = NULL
    xor  r9d, r9d             ; FILE_BEGIN = 0
    call SetFilePointerEx

    ; Write LBA as 8-byte binary checkpoint (simple + resumable)
    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).hLog
    lea  rdx, (DISK_RECOVERY_CONTEXT ptr [rbx]).CurrentLBA
    mov  r8d, 8
    lea  r9, bytesWritten
    mov  qword ptr [rsp+32], 0
    call WriteFile

    ; Flush to ensure it survives a crash
    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).hLog
    call FlushFileBuffers

    add  rsp, 40
    pop  rbx
    ret
SaveCheckpoint ENDP

; =============================================================================
; DisplayProgress — Print recovery statistics
; RCX = Context ptr
; =============================================================================
DisplayProgress PROC
    push rbx
    sub  rsp, 32

    mov  rbx, rcx

    lea  rcx, szStatHeader
    call ConsolePrint

    lea  rcx, szStatTotal
    call ConsolePrint
    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).CurrentLBA
    call PrintU64
    lea  rcx, szNewLine
    call ConsolePrint

    lea  rcx, szStatGood
    call ConsolePrint
    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).GoodSectors
    call PrintU64
    lea  rcx, szNewLine
    call ConsolePrint

    lea  rcx, szStatBad
    call ConsolePrint
    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).BadSectors
    call PrintU64
    lea  rcx, szNewLine
    call ConsolePrint

    ; Percentage = (CurrentLBA * 100) / TotalSectors
    lea  rcx, szStatPct
    call ConsolePrint

    mov  rax, (DISK_RECOVERY_CONTEXT ptr [rbx]).CurrentLBA
    mov  rcx, 100
    mul  rcx                   ; RDX:RAX = CurrentLBA * 100
    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).TotalSectors
    test rcx, rcx
    jz   dp_skip_pct
    div  rcx                   ; RAX = percentage
    mov  rcx, rax
    call PrintU64

dp_skip_pct:
    lea  rcx, szPctSign
    call ConsolePrint

    add  rsp, 32
    pop  rbx
    ret
DisplayProgress ENDP

; =============================================================================
; RecoveryWorker — Main imaging loop with resume + checkpoint
; RCX = ptr to DISK_RECOVERY_CONTEXT
; =============================================================================
RecoveryWorker PROC
    push rbx
    push rsi
    push rdi
    sub  rsp, 48

    mov  rbx, rcx
    mov  rsi, (DISK_RECOVERY_CONTEXT ptr [rbx]).CurrentLBA ; Resume LBA

main_loop:
    ; Check abort signal (thread-safe volatile read)
    movzx eax, (DISK_RECOVERY_CONTEXT ptr [rbx]).AbortSignal
    test al, al
    jnz  worker_done

    ; Check if we've exceeded drive capacity
    cmp  rsi, (DISK_RECOVERY_CONTEXT ptr [rbx]).TotalSectors
    jge  worker_done

    ; Hammer read this sector
    mov  rcx, rbx
    mov  rdx, rsi
    lea  r8, TransferBuffer
    call HammerReadSector

    test eax, eax
    jz   rw_bad_sector

    ; Success — write sector to image file
    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).hTarget
    lea  rdx, TransferBuffer
    mov  r8d, SECTOR_SIZE
    lea  r9, [rsp+36]         ; &bytesWritten (inside shadow, not overlapping 5th arg)
    mov  qword ptr [rsp+32], 0 ; lpOverlapped = NULL (5th arg at correct position)
    call WriteFile

    inc  (DISK_RECOVERY_CONTEXT ptr [rbx]).GoodSectors

    ; Print inline progress dot
    lea  rcx, szSectorOk
    call ConsolePrint
    jmp  rw_next

rw_bad_sector:
    ; Seek target forward to leave a hole (sparse-file-friendly)
    ; SetFilePointerEx(hTarget, SECTOR_SIZE, NULL, FILE_CURRENT)
    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).hTarget
    mov  edx, SECTOR_SIZE      ; liDistanceToMove
    xor  r8d, r8d              ; lpNewFilePointer = NULL
    mov  r9d, FILE_CURRENT     ; dwMoveMethod
    call SetFilePointerEx

    ; Print bad sector marker
    lea  rcx, szSectorBad
    call ConsolePrint

rw_next:
    inc  rsi
    mov  (DISK_RECOVERY_CONTEXT ptr [rbx]).CurrentLBA, rsi

    ; Checkpoint every CHECKPOINT_INTERVAL sectors
    mov  rax, rsi
    xor  edx, edx
    mov  ecx, CHECKPOINT_INTERVAL
    div  rcx
    test edx, edx
    jnz  main_loop

    ; Save checkpoint + display stats
    mov  rcx, rbx
    call SaveCheckpoint
    mov  rcx, rbx
    call DisplayProgress

    jmp  main_loop

worker_done:
    ; Final checkpoint
    mov  rcx, rbx
    call SaveCheckpoint

    add  rsp, 48
    pop  rdi
    pop  rsi
    pop  rbx
    ret
RecoveryWorker ENDP

; =============================================================================
; SetSparseFile — Mark a file handle as sparse via FSCTL_SET_SPARSE
; RCX = hFile
; =============================================================================
SetSparseFile PROC
    LOCAL bytesRet:DWORD

    sub  rsp, 64

    ; DeviceIoControl(hFile, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &ret, NULL)
    ; rcx = hFile (already set)
    mov  edx, FSCTL_SET_SPARSE
    xor  r8d, r8d             ; lpInBuffer = NULL
    xor  r9d, r9d             ; nInBufferSize = 0
    mov  qword ptr [rsp+32], 0 ; lpOutBuffer
    mov  dword ptr [rsp+40], 0 ; nOutBufferSize
    lea  rax, bytesRet
    mov  qword ptr [rsp+48], rax
    mov  qword ptr [rsp+56], 0
    call DeviceIoControl

    add  rsp, 64
    ret
SetSparseFile ENDP

; =============================================================================
; InitializeRecoveryContext — Open device, create output files
; ECX = Drive number (0-15)
; Returns: RAX = ptr to DISK_RECOVERY_CONTEXT, or 0 on failure
; =============================================================================
InitializeRecoveryContext PROC
    LOCAL driveNumSave:DWORD
    LOCAL devPath[32]:BYTE
    LOCAL bytesRead:DWORD

    push rbx
    push rsi
    push rdi
    push r12
    sub  rsp, 64

    mov  driveNumSave, ecx

    ; Zero the global context
    lea  rdi, g_Context
    mov  ecx, sizeof DISK_RECOVERY_CONTEXT
    xor  eax, eax
    rep  stosb

    ; Build device path
    lea  rdi, devPath
    lea  rsi, szDevicePathBase
    mov  ecx, 17
    rep  movsb
    mov  eax, driveNumSave
    add  al, '0'
    mov  byte ptr [rdi], al
    mov  byte ptr [rdi+1], 0

    ; Open source drive with extreme flags
    lea  rcx, devPath
    mov  edx, GENERIC_READ or GENERIC_WRITE
    mov  r8d, FILE_SHARE_READ or FILE_SHARE_WRITE
    xor  r9d, r9d
    mov  qword ptr [rsp+32], OPEN_EXISTING
    mov  dword ptr [rsp+40], FILE_FLAG_NO_BUFFERING or FILE_FLAG_WRITE_THROUGH
    mov  qword ptr [rsp+48], 0
    call CreateFileA

    cmp  rax, INVALID_HANDLE_VALUE
    je   ic_fail

    mov  g_Context.hSource, rax

    ; Create target image file
    lea  rcx, szTargetImage
    mov  edx, GENERIC_WRITE
    mov  r8d, FILE_SHARE_READ
    xor  r9d, r9d
    mov  qword ptr [rsp+32], CREATE_ALWAYS
    mov  dword ptr [rsp+40], FILE_FLAG_NO_BUFFERING or FILE_FLAG_WRITE_THROUGH or FILE_FLAG_SEQUENTIAL_SCAN
    mov  qword ptr [rsp+48], 0
    call CreateFileA

    cmp  rax, INVALID_HANDLE_VALUE
    je   ic_fail

    mov  g_Context.hTarget, rax

    ; Mark target as sparse file for bad sector holes
    mov  rcx, rax
    call SetSparseFile

    ; Create log file (resume checkpoint)
    lea  rcx, szLogFile
    mov  edx, GENERIC_READ or GENERIC_WRITE
    mov  r8d, FILE_SHARE_READ
    xor  r9d, r9d
    mov  qword ptr [rsp+32], OPEN_EXISTING         ; Try to open existing (resume)
    mov  dword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov  qword ptr [rsp+48], 0
    call CreateFileA

    cmp  rax, INVALID_HANDLE_VALUE
    je   ic_create_log

    ; Existing log found — try to read resume LBA
    mov  r12, rax
    mov  rcx, rax
    lea  rdx, g_Context.CurrentLBA
    mov  r8d, 8
    lea  r9, bytesRead
    mov  qword ptr [rsp+32], 0
    call ReadFile

    test eax, eax
    jz   ic_reset_log
    cmp  bytesRead, 8
    jne  ic_reset_log

    ; Resume position loaded!
    mov  g_Context.hLog, r12
    jmp  ic_log_done

ic_reset_log:
    mov  rcx, r12
    call CloseHandle

ic_create_log:
    ; Create fresh log
    lea  rcx, szLogFile
    mov  edx, GENERIC_READ or GENERIC_WRITE
    mov  r8d, FILE_SHARE_READ
    xor  r9d, r9d
    mov  qword ptr [rsp+32], CREATE_ALWAYS
    mov  dword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov  qword ptr [rsp+48], 0
    call CreateFileA

    mov  g_Context.hLog, rax
    mov  g_Context.CurrentLBA, 0

ic_log_done:
    ; Create bad sector map
    lea  rcx, szBadMap
    mov  edx, GENERIC_WRITE
    mov  r8d, FILE_SHARE_READ
    xor  r9d, r9d
    mov  qword ptr [rsp+32], CREATE_ALWAYS
    mov  dword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov  qword ptr [rsp+48], 0
    call CreateFileA

    mov  g_Context.hBadSectorMap, rax

    ; Default capacity: 2TB / 4096 = 488,281,250 sectors
    ; The RecoveryWorker will stop at this or at abort signal
    mov  g_Context.TotalSectors, 488281250

    ; Report bridge type
    mov  eax, g_Context.BridgeType
    cmp  eax, BRIDGE_JMS567
    je   ic_report_jms
    cmp  eax, BRIDGE_NS1066
    je   ic_report_ns
    lea  rcx, szBridgeUnknown
    call ConsolePrint
    jmp  ic_report_done
ic_report_jms:
    lea  rcx, szBridgeJMS567
    call ConsolePrint
    jmp  ic_report_done
ic_report_ns:
    lea  rcx, szBridgeNS1066
    call ConsolePrint
ic_report_done:

    lea  rcx, szInitOk
    call ConsolePrint

    lea  rax, g_Context
    jmp  ic_exit

ic_fail:
    lea  rcx, szOpenFail
    call ConsolePrint
    xor  eax, eax

ic_exit:
    add  rsp, 64
    pop  r12
    pop  rdi
    pop  rsi
    pop  rbx
    ret
InitializeRecoveryContext ENDP

; =============================================================================
; CleanupRecovery — Close all handles
; RCX = ptr to Context
; =============================================================================
CleanupRecovery PROC
    push rbx
    sub  rsp, 32

    mov  rbx, rcx

    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).hSource
    test rcx, rcx
    jz   cr_skip_src
    call CloseHandle
cr_skip_src:

    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).hTarget
    test rcx, rcx
    jz   cr_skip_tgt
    call CloseHandle
cr_skip_tgt:

    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).hLog
    test rcx, rcx
    jz   cr_skip_log
    call CloseHandle
cr_skip_log:

    mov  rcx, (DISK_RECOVERY_CONTEXT ptr [rbx]).hBadSectorMap
    test rcx, rcx
    jz   cr_skip_map
    call CloseHandle
cr_skip_map:

    add  rsp, 32
    pop  rbx
    ret
CleanupRecovery ENDP

; =============================================================================
; C-callable exports for C++ wrapper integration
; =============================================================================

; DiskRecovery_FindDrive — returns drive number or -1
; extern "C" int DiskRecovery_FindDrive();
PUBLIC DiskRecovery_FindDrive
DiskRecovery_FindDrive PROC
    sub  rsp, 28h             ; Shadow space + alignment
    call FindDyingDrive
    add  rsp, 28h
    ret
DiskRecovery_FindDrive ENDP

; DiskRecovery_Init — returns context ptr or NULL
; extern "C" void* DiskRecovery_Init(int driveNum);
PUBLIC DiskRecovery_Init
DiskRecovery_Init PROC
    ; ECX = driveNum (already in first arg register)
    sub  rsp, 28h             ; Shadow space + alignment
    call InitializeRecoveryContext
    add  rsp, 28h
    ret
DiskRecovery_Init ENDP

; DiskRecovery_ExtractKey — returns 1/0
; extern "C" int DiskRecovery_ExtractKey(void* ctx);
PUBLIC DiskRecovery_ExtractKey
DiskRecovery_ExtractKey PROC
    sub  rsp, 28h             ; Shadow space + alignment
    call ExtractEncryptionKey
    add  rsp, 28h
    ret
DiskRecovery_ExtractKey ENDP

; DiskRecovery_Run — runs the recovery loop (blocks until complete/abort)
; extern "C" void DiskRecovery_Run(void* ctx);
PUBLIC DiskRecovery_Run
DiskRecovery_Run PROC
    sub  rsp, 28h             ; Shadow space + alignment
    call RecoveryWorker
    add  rsp, 28h
    ret
DiskRecovery_Run ENDP

; DiskRecovery_Abort — thread-safe abort signal
; extern "C" void DiskRecovery_Abort(void* ctx);
PUBLIC DiskRecovery_Abort
DiskRecovery_Abort PROC
    mov  byte ptr (DISK_RECOVERY_CONTEXT ptr [rcx]).AbortSignal, 1
    ret
DiskRecovery_Abort ENDP

; DiskRecovery_Cleanup — close handles
; extern "C" void DiskRecovery_Cleanup(void* ctx);
PUBLIC DiskRecovery_Cleanup
DiskRecovery_Cleanup PROC
    sub  rsp, 28h             ; Shadow space + alignment
    call CleanupRecovery
    add  rsp, 28h
    ret
DiskRecovery_Cleanup ENDP

; DiskRecovery_GetStats — fill caller-provided stat buffer
; extern "C" void DiskRecovery_GetStats(void* ctx, uint64_t* outGood, uint64_t* outBad, uint64_t* outCurrent, uint64_t* outTotal);
PUBLIC DiskRecovery_GetStats
DiskRecovery_GetStats PROC
    ; RCX = ctx, RDX = &good, R8 = &bad, R9 = &current, [rsp+40] = &total
    mov  rax, (DISK_RECOVERY_CONTEXT ptr [rcx]).GoodSectors
    mov  qword ptr [rdx], rax
    mov  rax, (DISK_RECOVERY_CONTEXT ptr [rcx]).BadSectors
    mov  qword ptr [r8], rax
    mov  rax, (DISK_RECOVERY_CONTEXT ptr [rcx]).CurrentLBA
    mov  qword ptr [r9], rax
    mov  rax, qword ptr [rsp+40]  ; &total (5th arg, stack)
    test rax, rax
    jz   gs_skip_total
    mov  r10, (DISK_RECOVERY_CONTEXT ptr [rcx]).TotalSectors
    mov  qword ptr [rax], r10
gs_skip_total:
    ret
DiskRecovery_GetStats ENDP

; =============================================================================
; Standalone entry point (only used when linked as /entry:DiskRecovery_Main)
; =============================================================================
PUBLIC DiskRecovery_Main
DiskRecovery_Main PROC
    ; Save non-volatile registers used below (R12, RBX)
    push rbx
    push r12
    sub  rsp, 48              ; 2 pushes (16) + 48 = 64 → 0 mod 16 for entry point ✓

    ; Banner
    lea  rcx, szBanner
    call ConsolePrint

    ; Scan for drives
    lea  rcx, szScanning
    call ConsolePrint

    call FindDyingDrive
    cmp  eax, -1
    je   dm_no_drive

    ; Report what we found
    mov  r12d, eax             ; Save drive number

    lea  rcx, szFoundDrive
    call ConsolePrint

    ; Print drive number digit
    mov  al, r12b
    add  al, '0'
    mov  szDriveNum, al
    lea  rcx, szDriveNum
    call ConsolePrint

    ; Initialize
    lea  rcx, szOpening
    call ConsolePrint

    mov  ecx, r12d
    call InitializeRecoveryContext
    test rax, rax
    jz   dm_init_err

    mov  rbx, rax              ; Context ptr

    ; Attempt key extraction before the bridge dies
    lea  rcx, szKeyExtract
    call ConsolePrint

    mov  rcx, rbx
    call ExtractEncryptionKey
    test eax, eax
    jz   dm_no_key

    lea  rcx, szKeySuccess
    call ConsolePrint
    jmp  dm_start_recovery

dm_no_key:
    lea  rcx, szKeyFail
    call ConsolePrint

dm_start_recovery:
    lea  rcx, szHammerStart
    call ConsolePrint

    mov  rcx, rbx
    call RecoveryWorker

    ; Final stats
    mov  rcx, rbx
    call DisplayProgress

    ; Cleanup
    mov  rcx, rbx
    call CleanupRecovery

    lea  rcx, szComplete
    call ConsolePrint

    xor  ecx, ecx
    call ExitProcess

dm_no_drive:
    lea  rcx, szNoDevice
    call ConsolePrint
    mov  ecx, 1
    call ExitProcess

dm_init_err:
    lea  rcx, szInitError
    call ConsolePrint
    mov  ecx, 2
    call ExitProcess

DiskRecovery_Main ENDP

END
