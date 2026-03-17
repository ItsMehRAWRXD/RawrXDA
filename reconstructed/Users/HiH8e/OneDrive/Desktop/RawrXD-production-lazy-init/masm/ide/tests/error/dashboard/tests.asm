; error_dashboard_tests.asm
; Tests for Real-Time Error Dashboard filtering and live feed

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

CreateErrorDashboard PROTO
SetDashboardFilter PROTO :DWORD
GetListBoxCount PROTO
GetListBoxText PROTO :DWORD, :DWORD, :DWORD
CloseErrorDashboard PROTO
UpdateDashboard PROTO

; Log levels (match modules)
LOG_INFO            equ 1
LOG_WARNING         equ 2
LOG_ERROR           equ 3
LOG_FATAL           equ 4

.DATA
szAppName           db "Dashboard Tests", 0
szPassMsg           db "All dashboard tests passed", 0
szFailMsg           db "Some dashboard tests failed", 0

szInfoMsg           db "info msg", 0
szWarnMsg           db "warn msg", 0
szErr1Msg           db "error one", 0
szErr2Msg           db "error two", 0
szFatalMsg          db "fatal msg", 0

passCount           dd 0
failCount           dd 0

; Log path and formatting
szLogPath           db "C:\\RawrXD\\logs\\ide_errors.log", 0
szFmt               db "[%s] [%s] %s", 13, 10, 0
szFixedTime         db "2025-12-20 12:00:00", 0
szInfo              db "INFO", 0
szWarningStr        db "WARNING", 0
szErrorStr          db "ERROR", 0
szFatalStr          db "FATAL", 0
lineBuffer          db 512 dup(0)

.CODE

; Simple assert (increments passCount or failCount)
AssertEqual PROC expected:DWORD, actual:DWORD
    mov eax, expected
    cmp eax, actual
    jne ae_fail
    inc passCount
    ret
ae_fail:
    inc failCount
    ret
AssertEqual ENDP

WinMain PROC
    LOCAL hWnd:DWORD

    ; Create dashboard window
    invoke CreateErrorDashboard
    mov hWnd, eax

    ; Ensure empty and filter All
    invoke SetDashboardFilter, 0
    invoke UpdateDashboard

    ; Write sample log entries AFTER dashboard opens so it can tail
    ; Directly append to log file used by dashboard
    ; Format: [YYYY-MM-DD HH:MM:SS] [LEVEL] message\r\n
    ; Build and write lines
    call WriteInfo
    call WriteWarning
    call WriteError1
    call WriteError2
    call WriteFatal

    ; small delay
    invoke Sleep, 500

    ; Update and count All
    invoke SetDashboardFilter, 0
    invoke UpdateDashboard
    invoke GetListBoxCount
    mov ecx, eax
    invoke AssertEqual, 5, ecx

    ; ERROR filter: expect 2
    invoke SetDashboardFilter, 3
    invoke UpdateDashboard
    invoke GetListBoxCount
    mov ecx, eax
    invoke AssertEqual, 2, ecx

    ; WARNING filter: expect 1
    invoke SetDashboardFilter, 2
    invoke UpdateDashboard
    invoke GetListBoxCount
    mov ecx, eax
    invoke AssertEqual, 1, ecx

    ; FATAL filter: expect 1
    invoke SetDashboardFilter, 4
    invoke UpdateDashboard
    invoke GetListBoxCount
    mov ecx, eax
    invoke AssertEqual, 1, ecx

    ; INFO filter: expect 1
    invoke SetDashboardFilter, 1
    invoke UpdateDashboard
    invoke GetListBoxCount
    push eax
    mov eax, 1
    pop ecx
    push ecx
    push eax
    call AssertEqual
    add esp, 8

    ; Close dashboard
    invoke CloseErrorDashboard

    ; Report
    mov eax, failCount
    .IF eax == 0
        invoke MessageBox, 0, OFFSET szPassMsg, OFFSET szAppName, MB_OK or MB_ICONINFORMATION
        invoke ExitProcess, 0
    .ELSE
        invoke MessageBox, 0, OFFSET szFailMsg, OFFSET szAppName, MB_OK or MB_ICONERROR
        invoke ExitProcess, 1
    .ENDIF
    ret
WinMain ENDP

; -------------------- Helpers to write lines --------------------
WriteLogLine PROC levelPtr:DWORD, msgPtr:DWORD
    LOCAL hFile:DWORD
    LOCAL bytesWritten:DWORD
    LOCAL len:DWORD
    ; Build one line using fixed timestamp
    invoke wsprintf, OFFSET lineBuffer, OFFSET szFmt, OFFSET szFixedTime, levelPtr, msgPtr
    ; Open file for append
    invoke CreateFile, OFFSET szLogPath, GENERIC_WRITE, FILE_SHARE_READ or FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    mov hFile, eax
    .IF hFile != INVALID_HANDLE_VALUE
        invoke SetFilePointer, hFile, 0, NULL, FILE_END
        invoke lstrlen, OFFSET lineBuffer
        mov len, eax
        invoke WriteFile, hFile, OFFSET lineBuffer, len, ADDR bytesWritten, NULL
        invoke CloseHandle, hFile
    .ENDIF
    ret
WriteLogLine ENDP

WriteInfo PROC
    invoke WriteLogLine, OFFSET szInfo, OFFSET szInfoMsg
    ret
WriteInfo ENDP

WriteWarning PROC
    invoke WriteLogLine, OFFSET szWarningStr, OFFSET szWarnMsg
    ret
WriteWarning ENDP

WriteError1 PROC
    invoke WriteLogLine, OFFSET szErrorStr, OFFSET szErr1Msg
    ret
WriteError1 ENDP

WriteError2 PROC
    invoke WriteLogLine, OFFSET szErrorStr, OFFSET szErr2Msg
    ret
WriteError2 ENDP

WriteFatal PROC
    invoke WriteLogLine, OFFSET szFatalStr, OFFSET szFatalMsg
    ret
WriteFatal ENDP

END WinMain
