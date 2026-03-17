; Simplified broken bridge showing ABI violations
; This is a minimal version that demonstrates the three key problems

bits 64
default rel

; Fixed: Added proper exports section for Windows DLL
global DllMain
DllMain:
    ; RCX = hinstDLL, RDX = fdwReason, R8 = lpReserved (per Windows x64 ABI)
    mov eax, 1    ; TRUE (success)
    ret

global bridge_init
bridge_init:
    ; Windows x64 ABI compliant function
    ; RSP must be 16-byte aligned at call
    ; Allocate 32 bytes shadow space for callees

    sub rsp, 40         ; 40 bytes shadow space for Windows x64 ABI (if calling other functions)
    mov eax, 1          ; Return error
    add rsp, 40         ; Restore stack pointer
    ret                 ; Return to caller
    ; 3. Use proper register usage

; VIOLATION 4: Missing proper exports
; No __declspec(dllexport) equivalent in assembly; VIOLATION 4: Missing proper exports
; No __declspec(dllexport) equivalent in assembly
; To export symbols in NASM for a Windows DLL, use both:
;   1. 'global' directives in the assembly source (as above), and
;   2. A module definition (.def) file during linking.
; This ensures compatibility across different toolchains and reliably exports functions.
;
; Example .def file for proper DLL exports:
;   LIBRARY broken_bridge_simple
;   EXPORTS
;       DllMain
;       bridge_init
;
; Pass the .def file to the linker (e.g., /DEF:broken_bridge_simple.def with MSVC link.exe).