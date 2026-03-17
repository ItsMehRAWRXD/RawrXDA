; Stub version of beaconism_dispatcher for initial build
OPTION casemap:none

PUBLIC ResidentBeaconLoop
PUBLIC ProcessSignal  
PUBLIC InitializeAperture
PUBLIC TriggerHotPatch
PUBLIC ManifestVisualIdentity
PUBLIC PulseBeacon

.code

ResidentBeaconLoop PROC
    ; Stub - infinite loop
beacon_wait:
    pause
    jmp beacon_wait
    ret
ResidentBeaconLoop ENDP

ProcessSignal PROC
    mov rax, 1
    ret
ProcessSignal ENDP

InitializeAperture PROC
    mov rax, 1
    ret
InitializeAperture ENDP

TriggerHotPatch PROC
    mov rax, 1
    ret
TriggerHotPatch ENDP

ManifestVisualIdentity PROC
    mov rax, 1
    ret
ManifestVisualIdentity ENDP

PulseBeacon PROC
    ret
PulseBeacon ENDP

END
