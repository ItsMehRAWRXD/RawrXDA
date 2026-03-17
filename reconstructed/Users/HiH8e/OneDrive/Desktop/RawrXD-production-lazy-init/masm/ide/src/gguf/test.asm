;==============================================================================
; GGUF_COMPRESSION_TEST.ASM - Minimal test to verify compilation
;==============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

;==============================================================================
; CONSTANTS
;==============================================================================
STREAMING_BUFFER_SIZE     EQU 65536  ; 64KB chunks

;==============================================================================
; DATA SECTION
;==============================================================================
.DATA
testBuffer   DB STREAMING_BUFFER_SIZE DUP(?)
szTestMsg    DB "Test message: %d", 0

;==============================================================================
; CODE SECTION
;==============================================================================
.CODE

TestFunction PROC
    LOCAL result:DWORD
    
    mov result, 42
    invoke wsprintf, offset testBuffer, offset szTestMsg, result
    
    mov eax, TRUE
    ret
TestFunction ENDP

END