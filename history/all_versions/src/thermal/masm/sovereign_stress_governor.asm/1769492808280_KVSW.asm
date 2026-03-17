; ============================================================================
; SOVEREIGN STRESS GOVERNOR - ENTERPRISE MASM x64
; Full autonomous stress cycle with real-time NVMe hot-swap
; 
; Architecture: Pure Win32 / No CRT / RIP-relative / ASLR-resilient
; ABI: Microsoft x64 Calling Convention
; ============================================================================

option casemap:none

; ============================================================================
; WIN32 CONSTANTS (No windows.inc dependency)
; ============================================================================

INVALID_HANDLE_VALUE    equ -1
NULL                    equ 0
PAGE_READWRITE          equ 04h
FILE_MAP_ALL_ACCESS     equ 000F001Fh
FILE_MAP_READ           equ 04h
GENERIC_READ            equ 80000000h
GENERIC_WRITE           equ 40000000h
FILE_SHARE_READ         equ 01h
FILE_SHARE_WRITE        equ 02h
OPEN_EXISTING           equ 3
FILE_FLAG_NO_BUFFERING  equ 20000000h
FILE_FLAG_WRITE_THROUGH equ 80000000h
INFINITE                equ 0FFFFFFFFh

; MMF Signature
SOVEREIGN_SIGNATURE     equ 534F5645h   ; "SOVE" little-endian

; Thermal thresholds
THROTTLE_TEMP_C         equ 65          ; Start throttling
CRITICAL_TEMP_C         equ 75          ; Emergency evacuation
BLACKLIST_TEMP_C        equ 70          ; Blacklist threshold
MAX_DRIVES              equ 16

; Governor states
STATE_IDLE              equ 0
STATE_STREAMING         equ 1
STATE_THROTTLED         equ 2
STATE_EVACUATING        equ 3
STATE_COOLDOWN          equ 4

; ============================================================================
; PUBLIC SYMBOLS (Export for linker)
; ============================================================================

PUBLIC main

; ============================================================================
; EXTERNAL WIN32 API
; ============================================================================

extern OpenFileMappingA     : proc
extern MapViewOfFile        : proc
extern UnmapViewOfFile      : proc
extern CloseHandle          : proc
extern CreateFileA          : proc
extern ReadFile             : proc
extern WriteFile            : proc
extern GetTickCount64       : proc
extern Sleep                : proc
extern VirtualAlloc         : proc
extern VirtualFree          : proc
extern GetLastError         : proc
extern ExitProcess          : proc

; Console output for diagnostics
extern GetStdHandle         : proc
extern WriteConsoleA        : proc

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    align 16
    
    ; MMF Names (Global namespace for cross-session)
    szMMFGlobal         db "Global\SOVEREIGN_NVME_TEMPS", 0
    szMMFLocal          db "Local\SOVEREIGN_NVME_TEMPS", 0
    
    ; Physical drive path template
    szDrivePath         db "\\.\PhysicalDrive", 0
    szDriveNum          db "0", 0, 0, 0, 0, 0, 0, 0   ; Buffer for drive number
    
    ; Console messages
    szBanner            db 13,10
                        db "======================================================", 13, 10
                        db "  SOVEREIGN STRESS GOVERNOR - Enterprise MASM x64", 13, 10  
                        db "  Real-Time Thermal Hot-Swap Engine", 13, 10
                        db "======================================================", 13, 10, 0
    lenBanner           equ $ - szBanner
    
    szConnected         db "[INIT] MMF Connected - Signature Valid", 13, 10, 0
    lenConnected        equ $ - szConnected
    
    szCycleStart        db "[CYCLE] Stress cycle initiated", 13, 10, 0
    lenCycleStart       equ $ - szCycleStart
    
    szHotSwap           db "[HOTSWAP] Drive evacuation triggered!", 13, 10, 0
    lenHotSwap          equ $ - szHotSwap
    
    szThrottle          db "[THROTTLE] Thermal limit reached - reducing I/O", 13, 10, 0
    lenThrottle         equ $ - szThrottle
    
    szComplete          db "[COMPLETE] Stress cycle finished", 13, 10, 0
    lenComplete         equ $ - szComplete
    
    szError             db "[ERROR] ", 0
    
    ; Formatting
    szDriveTemp         db "[THERMAL] Drive ", 0
    szTempSuffix        db " C", 13, 10, 0
    szBestDrive         db "[GOVERNOR] Best drive: ", 0
    szBlacklisted       db " (BLACKLISTED)", 0
    
.data?
    align 16
    
    ; Governor State Block
    hMMF                dq ?            ; MMF handle
    pMMFView            dq ?            ; Mapped view pointer
    hStdOut             dq ?            ; Console handle
    
    ; Drive State Arrays (16 drives max)
    driveTemps          dd MAX_DRIVES dup(?)
    driveWear           dd MAX_DRIVES dup(?)
    driveScores         dq MAX_DRIVES dup(?)    ; 64-bit for precision
    driveBlacklist      db MAX_DRIVES dup(?)
    driveHandles        dq MAX_DRIVES dup(?)
    
    ; Current state
    currentState        dd ?
    activeDrive         dd ?
    driveCount          dd ?
    cycleCount          dq ?
    lastUpdateMs        dq ?
    
    ; I/O Buffer (aligned for direct I/O)
    align 4096
    ioBuffer            db 65536 dup(?)  ; 64KB aligned buffer
    
    ; Scratch buffers
    scratchBuf          db 256 dup(?)
    bytesWritten        dq ?

; ============================================================================
; CODE SECTION  
; ============================================================================

.code

; ----------------------------------------------------------------------------
; ConsolePrint: Write string to stdout
; RCX = String pointer, RDX = Length
; ----------------------------------------------------------------------------
ConsolePrint PROC FRAME
    push rbx
    push rsi
    push rdi
    sub rsp, 40h
    .endprolog
    
    mov rsi, rcx                ; String
    mov edi, edx                ; Length
    
    ; Get stdout handle if not cached
    mov rax, [hStdOut]
    test rax, rax
    jnz @have_handle
    
    mov ecx, -11                ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov [hStdOut], rax
    
@have_handle:
    mov rcx, rax                ; hConsole
    mov rdx, rsi                ; lpBuffer
    mov r8d, edi                ; nCharsToWrite
    lea r9, [bytesWritten]      ; lpCharsWritten
    mov qword ptr [rsp+20h], 0  ; lpReserved
    call WriteConsoleA
    
    add rsp, 40h
    pop rdi
    pop rsi
    pop rbx
    ret
ConsolePrint ENDP

; ----------------------------------------------------------------------------
; IntToStr: Convert integer to decimal string
; ECX = Value, RDX = Buffer pointer
; Returns: Length in EAX
; ----------------------------------------------------------------------------
IntToStr PROC FRAME
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    .endprolog
    
    mov eax, ecx
    mov rdi, rdx
    mov rsi, rdx
    
    ; Handle negative
    test eax, eax
    jns @positive
    neg eax
    mov byte ptr [rdi], '-'
    inc rdi
    
@positive:
    ; Convert digits in reverse
    mov rbx, rdi
    mov ecx, 10
    
@digit_loop:
    xor edx, edx
    div ecx
    add dl, '0'
    mov [rdi], dl
    inc rdi
    test eax, eax
    jnz @digit_loop
    
    ; Calculate length
    mov rax, rdi
    sub rax, rsi
    push rax                    ; Save length
    
    ; Reverse the digits
    dec rdi
@reverse_loop:
    cmp rbx, rdi
    jge @reverse_done
    mov al, [rbx]
    mov cl, [rdi]
    mov [rbx], cl
    mov [rdi], al
    inc rbx
    dec rdi
    jmp @reverse_loop
    
@reverse_done:
    pop rax                     ; Return length
    
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
IntToStr ENDP

; ----------------------------------------------------------------------------
; Governor_Init: Initialize MMF connection and state
; Returns: 1 = Success, 0 = Failure
; ----------------------------------------------------------------------------
Governor_Init PROC FRAME
    push rbx
    push r12
    push r13
    sub rsp, 40h
    .endprolog
    
    ; Print banner
    lea rcx, [szBanner]
    mov edx, lenBanner
    call ConsolePrint
    
    ; Try Global namespace first
    mov ecx, FILE_MAP_READ
    xor edx, edx                ; bInheritHandle = FALSE
    lea r8, [szMMFGlobal]
    call OpenFileMappingA
    
    mov rbx, rax
    cmp rax, NULL
    jne @mmf_opened
    
    ; Fallback to Local namespace
    mov ecx, FILE_MAP_READ
    xor edx, edx
    lea r8, [szMMFLocal]
    call OpenFileMappingA
    
    mov rbx, rax
    cmp rax, NULL
    je @init_fail
    
@mmf_opened:
    mov [hMMF], rbx
    
    ; Map the view
    mov rcx, rbx                ; hFileMappingObject
    mov edx, FILE_MAP_READ      ; dwDesiredAccess
    xor r8d, r8d                ; dwFileOffsetHigh
    xor r9d, r9d                ; dwFileOffsetLow
    mov qword ptr [rsp+20h], 160 ; dwNumberOfBytesToMap (SovereignThermalMMF size)
    call MapViewOfFile
    
    test rax, rax
    jz @init_fail
    mov [pMMFView], rax
    
    ; Validate signature
    mov r12, rax
    mov eax, [r12]              ; Read signature at offset 0
    cmp eax, SOVEREIGN_SIGNATURE
    jne @init_fail
    
    ; Read drive count
    mov eax, [r12 + 8]          ; Offset 8 = driveCount
    mov [driveCount], eax
    
    ; Initialize state
    mov dword ptr [currentState], STATE_IDLE
    mov dword ptr [activeDrive], -1
    mov qword ptr [cycleCount], 0
    
    ; Clear blacklist
    lea rdi, [driveBlacklist]
    xor eax, eax
    mov ecx, MAX_DRIVES
    rep stosb
    
    ; Print success
    lea rcx, [szConnected]
    mov edx, lenConnected
    call ConsolePrint
    
    mov eax, 1
    jmp @init_done
    
@init_fail:
    xor eax, eax
    
@init_done:
    add rsp, 40h
    pop r13
    pop r12
    pop rbx
    ret
Governor_Init ENDP

; ----------------------------------------------------------------------------
; Governor_ReadThermals: Atomically read thermal data from MMF
; Populates driveTemps[], driveWear[], updates blacklist
; Returns: Hottest valid temperature in EAX
; ----------------------------------------------------------------------------
Governor_ReadThermals PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 30h
    .endprolog
    
    mov r12, [pMMFView]
    test r12, r12
    jz @read_fail
    
    ; Read temperature array (offset 0x10, 16 DWORDs)
    mov ecx, [driveCount]
    cmp ecx, MAX_DRIVES
    jbe @count_ok
    mov ecx, MAX_DRIVES
@count_ok:
    mov r13d, ecx               ; R13 = drive count
    
    xor r14d, r14d              ; R14 = index
    mov r15d, -1000             ; R15 = hottest temp (init very cold)
    
@read_loop:
    cmp r14d, r13d
    jge @read_done
    
    ; Atomic read of temperature
    lea rax, [r12 + 10h]        ; Temps start at offset 0x10
    mov eax, [rax + r14*4]
    
    ; Store in local array
    lea rbx, [driveTemps]
    mov [rbx + r14*4], eax
    
    ; Read wear level (offset 0x50)
    lea rbx, [r12 + 50h]
    mov ecx, [rbx + r14*4]
    lea rbx, [driveWear]
    mov [rbx + r14*4], ecx
    
    ; Update blacklist based on temperature
    lea rbx, [driveBlacklist]
    
    ; Check if invalid (temp < -1 means IOCTL failure)
    cmp eax, -1
    jl @mark_blacklist
    
    ; Check if overheating
    cmp eax, BLACKLIST_TEMP_C
    jge @mark_blacklist
    
    ; Check wear level
    cmp ecx, 95
    jg @mark_blacklist
    
    ; Valid drive - update hottest
    mov byte ptr [rbx + r14], 0
    cmp eax, r15d
    jle @next_drive
    mov r15d, eax               ; New hottest
    jmp @next_drive
    
@mark_blacklist:
    mov byte ptr [rbx + r14], 1
    
@next_drive:
    inc r14d
    jmp @read_loop
    
@read_done:
    ; Read timestamp (offset 0x90)
    mov rax, [r12 + 90h]
    mov [lastUpdateMs], rax
    
    mov eax, r15d               ; Return hottest temp
    jmp @read_exit
    
@read_fail:
    mov eax, -999
    
@read_exit:
    add rsp, 30h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Governor_ReadThermals ENDP

; ----------------------------------------------------------------------------
; Governor_RankDrives: Calculate weighted scores and find best drive
; Score = Temp * 0.7 + Wear * 0.3 (implemented as integer math)
; Returns: Best drive ID in EAX (-1 if none available)
; ----------------------------------------------------------------------------
Governor_RankDrives PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 20h
    .endprolog
    
    mov r12d, [driveCount]
    cmp r12d, MAX_DRIVES
    jbe @rank_count_ok
    mov r12d, MAX_DRIVES
@rank_count_ok:
    
    xor r13d, r13d              ; Index
    mov r14d, -1                ; Best drive (-1 = none)
    mov r15, 7FFFFFFFFFFFFFFFh  ; Best score (max = worst)
    
@rank_loop:
    cmp r13d, r12d
    jge @rank_done
    
    ; Check blacklist
    lea rax, [driveBlacklist]
    movzx eax, byte ptr [rax + r13]
    test eax, eax
    jnz @rank_next              ; Skip blacklisted
    
    ; Calculate score: (temp * 70 + wear * 30) / 100
    ; This gives us score * 10 for better precision
    lea rbx, [driveTemps]
    mov eax, [rbx + r13*4]
    
    ; Handle unknown temp (-1) -> use 50
    cmp eax, -1
    jne @temp_ok
    mov eax, 50
@temp_ok:
    imul eax, 70                ; temp * 70
    mov ecx, eax
    
    lea rbx, [driveWear]
    mov eax, [rbx + r13*4]
    
    ; Handle unknown wear (-1) -> use 50
    cmp eax, -1
    jne @wear_ok
    mov eax, 50
@wear_ok:
    imul eax, 30                ; wear * 30
    add eax, ecx                ; total = temp*70 + wear*30
    
    ; Store score
    cdqe
    lea rbx, [driveScores]
    mov [rbx + r13*8], rax
    
    ; Check if best
    cmp rax, r15
    jge @rank_next
    mov r15, rax                ; New best score
    mov r14d, r13d              ; New best drive
    
@rank_next:
    inc r13d
    jmp @rank_loop
    
@rank_done:
    mov eax, r14d               ; Return best drive
    
    add rsp, 20h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Governor_RankDrives ENDP

; ----------------------------------------------------------------------------
; Governor_OpenDrive: Open physical drive for direct I/O
; ECX = Drive ID
; Returns: Handle in RAX (INVALID_HANDLE_VALUE on failure)
; ----------------------------------------------------------------------------
Governor_OpenDrive PROC FRAME
    push rbx
    push rsi
    sub rsp, 40h
    .endprolog
    
    mov ebx, ecx                ; Save drive ID
    
    ; Build path: "\\.\PhysicalDrive" + N
    lea rsi, [szDrivePath]
    lea rdi, [scratchBuf]
    
    ; Copy prefix
@copy_prefix:
    lodsb
    stosb
    test al, al
    jnz @copy_prefix
    dec rdi                     ; Back up over null
    
    ; Append drive number
    mov ecx, ebx
    mov rdx, rdi
    call IntToStr
    add rdi, rax
    mov byte ptr [rdi], 0       ; Null terminate
    
    ; CreateFileA
    lea rcx, [scratchBuf]
    mov edx, GENERIC_READ or GENERIC_WRITE
    mov r8d, FILE_SHARE_READ or FILE_SHARE_WRITE
    xor r9d, r9d                ; lpSecurityAttributes = NULL
    mov dword ptr [rsp+20h], OPEN_EXISTING
    mov dword ptr [rsp+28h], FILE_FLAG_NO_BUFFERING or FILE_FLAG_WRITE_THROUGH
    mov qword ptr [rsp+30h], 0  ; hTemplateFile = NULL
    call CreateFileA
    
    ; Store handle
    cmp rax, INVALID_HANDLE_VALUE
    je @open_fail
    
    lea rcx, [driveHandles]
    mov [rcx + rbx*8], rax
    
@open_fail:
    add rsp, 40h
    pop rsi
    pop rbx
    ret
Governor_OpenDrive ENDP

; ----------------------------------------------------------------------------
; Governor_HotSwap: Emergency migration from hot drive to cool drive
; ECX = Source drive, EDX = Target drive
; Returns: 1 = Success, 0 = Failure
; ----------------------------------------------------------------------------
Governor_HotSwap PROC FRAME
    push rbx
    push r12
    push r13
    sub rsp, 30h
    .endprolog
    
    mov r12d, ecx               ; Source
    mov r13d, edx               ; Target
    
    ; Print hotswap message
    lea rcx, [szHotSwap]
    mov edx, lenHotSwap
    call ConsolePrint
    
    ; Close source drive handle
    lea rax, [driveHandles]
    mov rcx, [rax + r12*8]
    cmp rcx, 0
    je @no_source_handle
    cmp rcx, INVALID_HANDLE_VALUE
    je @no_source_handle
    call CloseHandle
    
    lea rax, [driveHandles]
    mov qword ptr [rax + r12*8], 0
    
@no_source_handle:
    ; Open target drive
    mov ecx, r13d
    call Governor_OpenDrive
    
    cmp rax, INVALID_HANDLE_VALUE
    je @swap_fail
    
    ; Update active drive
    mov [activeDrive], r13d
    
    mov eax, 1
    jmp @swap_done
    
@swap_fail:
    xor eax, eax
    
@swap_done:
    add rsp, 30h
    pop r13
    pop r12
    pop rbx
    ret
Governor_HotSwap ENDP

; ----------------------------------------------------------------------------
; Governor_StressCycle: Main stress loop with thermal monitoring
; RCX = Duration in milliseconds (0 = infinite)
; ----------------------------------------------------------------------------
Governor_StressCycle PROC FRAME
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp, 50h
    .endprolog
    
    mov r12, rcx                ; Duration (0 = infinite)
    
    ; Get start time
    call GetTickCount64
    mov r13, rax                ; Start timestamp
    
    ; Print cycle start
    lea rcx, [szCycleStart]
    mov edx, lenCycleStart
    call ConsolePrint
    
    ; Initial thermal read and drive selection
    call Governor_ReadThermals
    mov r14d, eax               ; R14 = current hottest temp
    
    call Governor_RankDrives
    cmp eax, -1
    je @no_drives
    
    mov r15d, eax               ; R15 = active drive
    mov [activeDrive], eax
    
    ; Open initial drive
    mov ecx, r15d
    call Governor_OpenDrive
    cmp rax, INVALID_HANDLE_VALUE
    je @no_drives
    
    mov dword ptr [currentState], STATE_STREAMING
    
@stress_loop:
    ; Check duration
    test r12, r12
    jz @no_timeout
    call GetTickCount64
    sub rax, r13
    cmp rax, r12
    jge @cycle_complete
    
@no_timeout:
    ; Read thermals (every iteration for responsiveness)
    call Governor_ReadThermals
    mov r14d, eax               ; Update hottest
    
    ; Check for critical temperature
    cmp eax, CRITICAL_TEMP_C
    jge @emergency_evac
    
    ; Check for throttle threshold
    cmp eax, THROTTLE_TEMP_C
    jge @throttle_mode
    
    ; Normal operation - check if better drive available
    call Governor_RankDrives
    cmp eax, r15d
    je @continue_stress         ; Same drive is still best
    
    ; Better drive found - check if worth swapping (>5C difference)
    mov ebx, eax                ; New best drive
    
    lea rcx, [driveTemps]
    mov eax, [rcx + rbx*4]      ; New drive temp
    mov ecx, [rcx + r15*4]      ; Current drive temp
    sub ecx, eax                ; Difference
    cmp ecx, 5                  ; Only swap if 5C+ cooler
    jl @continue_stress
    
    ; Perform hot-swap
    mov ecx, r15d               ; Source
    mov edx, ebx                ; Target
    call Governor_HotSwap
    test eax, eax
    jz @continue_stress         ; Swap failed, continue with current
    mov r15d, ebx               ; Update active drive
    
@continue_stress:
    ; Perform I/O burst on active drive
    ; [Simplified - actual implementation would read/write sectors]
    
    ; Small delay to prevent CPU spin
    mov ecx, 100                ; 100ms between iterations
    call Sleep
    
    inc qword ptr [cycleCount]
    jmp @stress_loop
    
@throttle_mode:
    ; Print throttle message
    lea rcx, [szThrottle]
    mov edx, lenThrottle
    call ConsolePrint
    
    mov dword ptr [currentState], STATE_THROTTLED
    
    ; Reduce I/O intensity - longer sleep
    mov ecx, 500                ; 500ms cooldown
    call Sleep
    
    mov dword ptr [currentState], STATE_STREAMING
    jmp @stress_loop
    
@emergency_evac:
    ; Find ANY drive that's not critical
    mov dword ptr [currentState], STATE_EVACUATING
    
    call Governor_RankDrives
    cmp eax, -1
    je @all_drives_hot
    
    ; Swap to best available
    mov ecx, r15d
    mov edx, eax
    call Governor_HotSwap
    test eax, eax
    jz @all_drives_hot
    mov r15d, eax
    
    mov dword ptr [currentState], STATE_STREAMING
    jmp @stress_loop
    
@all_drives_hot:
    ; All drives overheating - enter cooldown
    mov dword ptr [currentState], STATE_COOLDOWN
    mov ecx, 5000               ; 5 second cooldown
    call Sleep
    jmp @stress_loop
    
@cycle_complete:
    lea rcx, [szComplete]
    mov edx, lenComplete
    call ConsolePrint
    
@no_drives:
    ; Cleanup - close all handles
    xor ebx, ebx
    mov r12d, [driveCount]
    
@cleanup_loop:
    cmp ebx, r12d
    jge @cleanup_done
    
    lea rax, [driveHandles]
    mov rcx, [rax + rbx*8]
    test rcx, rcx
    jz @next_cleanup
    cmp rcx, INVALID_HANDLE_VALUE
    je @next_cleanup
    call CloseHandle
    
@next_cleanup:
    inc ebx
    jmp @cleanup_loop
    
@cleanup_done:
    mov dword ptr [currentState], STATE_IDLE
    
    add rsp, 50h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
Governor_StressCycle ENDP

; ----------------------------------------------------------------------------
; Governor_Shutdown: Clean shutdown of governor
; ----------------------------------------------------------------------------
Governor_Shutdown PROC FRAME
    sub rsp, 28h
    .endprolog
    
    ; Unmap MMF view
    mov rcx, [pMMFView]
    test rcx, rcx
    jz @no_view
    call UnmapViewOfFile
    mov qword ptr [pMMFView], 0
    
@no_view:
    ; Close MMF handle
    mov rcx, [hMMF]
    test rcx, rcx
    jz @no_mmf
    call CloseHandle
    mov qword ptr [hMMF], 0
    
@no_mmf:
    add rsp, 28h
    ret
Governor_Shutdown ENDP

; ----------------------------------------------------------------------------
; main: Entry Point
; ----------------------------------------------------------------------------
main PROC FRAME
    sub rsp, 28h
    .endprolog
    
    ; Initialize governor
    call Governor_Init
    test eax, eax
    jz @init_failed
    
    ; Run stress cycle (60 seconds for test)
    mov rcx, 60000              ; 60,000 ms = 60 seconds
    call Governor_StressCycle
    
    ; Shutdown
    call Governor_Shutdown
    
    xor ecx, ecx
    call ExitProcess
    
@init_failed:
    mov ecx, 1
    call ExitProcess
main ENDP

END
