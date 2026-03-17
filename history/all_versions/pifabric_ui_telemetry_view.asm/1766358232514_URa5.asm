; pifabric_ui_telemetry_view.asm
.386
.model flat, stdcall
option casemap:none

include windows.inc
include user32.inc

includelib user32.lib

PUBLIC  UiTelemetryView_Init
PUBLIC  UiTelemetryView_Update
PUBLIC  UiTelemetryView_Show
PUBLIC  UiTelemetryView_Hide

telemetryViewHandle dq 0

.code

UiTelemetryView_Init PROC USES esi edi hParent:DWORD
    ; Create a static control for telemetry display
    invoke CreateWindowEx, 0, "STATIC", "Telemetry: CPU: 0% RAM: 0MB Latency: 0ms",
        WS_CHILD or WS_VISIBLE,
        0, 0, 300, 50,
        hParent, 0, NULL, NULL
    mov telemetryViewHandle, eax
    mov eax, 1
    ret
UiTelemetryView_Init ENDP

UiTelemetryView_Update PROC USES esi edi dwCPU:DWORD, dwRAM:DWORD, dwLatency:DWORD
    mov esi, telemetryViewHandle
    test esi, esi
    jz @done
    ; Update telemetry display (stub)
    invoke SetWindowText, esi, "Telemetry: CPU: 0% RAM: 0MB Latency: 0ms"
@done:
    xor eax, eax
    ret
UiTelemetryView_Update ENDP

UiTelemetryView_Show PROC
    mov eax, telemetryViewHandle
    test eax, eax
    jz @done
    invoke ShowWindow, eax, SW_SHOW
@done:
    xor eax, eax
    ret
UiTelemetryView_Show ENDP

UiTelemetryView_Hide PROC
    mov eax, telemetryViewHandle
    test eax, eax
    jz @done
    invoke ShowWindow, eax, SW_HIDE
@done:
    xor eax, eax
    ret
UiTelemetryView_Hide ENDP

END