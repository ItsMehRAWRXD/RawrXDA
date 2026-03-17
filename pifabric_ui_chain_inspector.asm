; pifabric_ui_chain_inspector.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc

includelib user32.lib

PUBLIC  UiChainInspector_Init
PUBLIC  UiChainInspector_Select
PUBLIC  UiChainInspector_Show
PUBLIC  UiChainInspector_Hide

chainInspectorHandle dq 0

.code

UiChainInspector_Init PROC USES esi edi hParent:DWORD
    ; Create a tree view for chain inspection
    invoke CreateWindowEx, 0, "SysTreeView32", NULL,
        WS_CHILD or WS_VISIBLE or TVS_HASLINES,
        0, 0, 250, 200,
        hParent, 0, NULL, NULL
    mov chainInspectorHandle, eax
    mov eax, 1
    ret
UiChainInspector_Init ENDP

UiChainInspector_Select PROC USES esi edi
    ; Stub for chain selection logic
    mov eax, 1
    ret
UiChainInspector_Select ENDP

UiChainInspector_Show PROC
    mov eax, chainInspectorHandle
    test eax, eax
    jz @done
    invoke ShowWindow, eax, SW_SHOW
@done:
    xor eax, eax
    ret
UiChainInspector_Show ENDP

UiChainInspector_Hide PROC
    mov eax, chainInspectorHandle
    test eax, eax
    jz @done
    invoke ShowWindow, eax, SW_HIDE
@done:
    xor eax, eax
    ret
UiChainInspector_Hide ENDP

END