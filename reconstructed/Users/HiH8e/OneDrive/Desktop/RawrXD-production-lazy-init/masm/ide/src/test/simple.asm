; Simple test file
.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

.code
TestProc proc
    mov eax, 1
    ret
TestProc endp

end