; Simple test file
.386
.model flat, stdcall
option casemap:none

include C:\masm32\include\windows.inc
include C:\masm32\include\kernel32.inc
includelib C:\masm32\lib\kernel32.lib

.data
szMsg db "Hello World", 0

.code
main proc
    invoke ExitProcess, 0
main endp

end main
