; =============================================================================
; RawrXD Static Linker - x64 MASM (Fortress stub)
; Links COFF objects into PE32+ executables. Full RAWRXD-LINK v1.0 can replace this.
; Build: ml64 /c rawrxd_link.asm /Fo rawrxd_link.obj
; Link:  link rawrxd_link.obj /subsystem:console /out:rawrxd_link.exe kernel32.lib
; =============================================================================

option casemap:none

.data
align 8
szUsage   db "RawrXD Fortress Linker - links COFF .obj into PE32+", 13, 10
          db "Usage: rawrxd_link <out.exe> <obj1.obj> [obj2.obj ...]", 13, 10, 0

.code

main PROC FRAME
    .ENDPROLOG
    sub     rsp, 28h
    call    LinkerEntry
    add     rsp, 28h
    ret
main ENDP

LinkerEntry PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 28h
    .allocstack 28h
    .ENDPROLOG

    call    GetCommandLineA
    mov     rsi, rax

@@skip_exe:
    lodsb
    test    al, al
    jz      @@no_args
    cmp     al, ' '
    jne     @@skip_exe

@@parse_args:
    lodsb
    cmp     al, ' '
    je      @@parse_args
    test    al, al
    jz      @@link
    jmp     @@parse_args

@@link:
    call    LayoutSections
    call    ResolveSymbols
    call    WritePEFile

    xor     eax, eax
    add     rsp, 28h
    pop     rdi
    pop     rsi
    pop     rbx
    ret

@@no_args:
    lea     rcx, szUsage
    call    PrintString
    mov     eax, 1
    add     rsp, 28h
    pop     rdi
    pop     rsi
    pop     rbx
    ret
LinkerEntry ENDP

PrintString PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    sub     rsp, 28h
    .allocstack 28h
    .ENDPROLOG
    mov     rsi, rcx
    mov     rbx, rcx
@@len:
    cmp     byte ptr [rbx], 0
    je      @@print
    inc     rbx
    jmp     @@len
@@print:
    sub     rbx, rsi
    mov     ecx, -11
    call    GetStdHandle
    mov     rcx, rax
    mov     rdx, rsi
    mov     r8, rbx
    lea     r9, [rsp+20h]
    xor     eax, eax
    push    rax
    sub     rsp, 20h
    call    WriteConsoleA
    add     rsp, 28h
    add     rsp, 28h
    pop     rsi
    pop     rbx
    ret
PrintString ENDP

LayoutSections PROC FRAME
    .ENDPROLOG
    ret
LayoutSections ENDP

ResolveSymbols PROC FRAME
    .ENDPROLOG
    ret
ResolveSymbols ENDP

WritePEFile PROC FRAME
    .ENDPROLOG
    ret
WritePEFile ENDP

extern GetStdHandle:proc
extern WriteConsoleA:proc
extern GetCommandLineA:proc

public main
public LinkerEntry

end
