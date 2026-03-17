; ================================================
; RXUC-IntegrationLayer v7.0 — The Orchestration Core
; Reverse-Engineered Complete Build Pipeline
; Zero-State Reset | Temp Patch Application | CLI Driver
; ================================================

option casemap:none
option frame:auto

; ================================================
; External Symbols (Link-Time Resolution)
; ================================================
extrn manifest_agent_limits:proc
extrn agent_controller_execute:proc
extrn patch_pe_headers:proc
extrn lex:proc
extrn parse_function:proc
extrn write_pe_file:proc
extrn cur_tok:proc
extrn advance:proc
extrn emit_prolog:proc
extrn emit_epilog:proc
extrn AgentEngine_Init:proc
extrn AgentEngine_CreateAgent:proc

; WinAPI
externdef GetCommandLineA:qword
externdef GetModuleHandleA:qword
externdef GetProcAddress:qword
externdef LoadLibraryA:qword
externdef CreateFileA:qword
externdef ReadFile:qword
externdef WriteFile:qword
externdef CloseHandle:qword
externdef GetFileSizeEx:qword
externdef VirtualAlloc:qword
externdef VirtualFree:qword
externdef GetStdHandle:qword
externdef WriteConsoleA:qword
externdef ExitProcess:qword
externdef SetConsoleTitleA:qword
externdef GetTickCount64:qword
externdef QueryPerformanceCounter:qword
externdef QueryPerformanceFrequency:qword

; ================================================
; Integration Constants
; ================================================
INTEGRATION_MAGIC equ 0x5241545241434E49  ; 'INTCARTAR' (Integration Marker)
TEMP_PATCH_MAGIC equ 0x5041544348544554   ; 'TEMPTHACT' (Temp Patch Marker)
MAX_COMPILE_UNITS equ 32
MAX_TEMP_PATCHES equ 256

; Compile Unit States
UNIT_IDLE equ 0
UNIT_SCANNING equ 1
UNIT_AGENT_MANIFEST equ 2
UNIT_COMPILING equ 3
UNIT_LINKING equ 4
UNIT_PATCHING equ 5
UNIT_COMPLETE equ 6

; ================================================
; Complex Structures
; ================================================