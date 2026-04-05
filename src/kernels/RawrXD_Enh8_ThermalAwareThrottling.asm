; RawrXD_Enh8_ThermalAwareThrottling.asm
OPTION CASEMAP:NONE

RAWRXD_TEMP_THRESHOLD EQU 80
RAWRXD_TEMP_CRITICAL EQU 95

.DATA
g_gpu_temp DWORD 70
g_tps_target DWORD 0
g_throttle_percent DWORD 100

.CODE
ThermalAwareThrottling_Initialize PROC
    mov g_tps_target, edx
    mov g_throttle_percent, 100
    xor eax, eax
    ret
ThermalAwareThrottling_Initialize ENDP

ThermalMonitor_Thread PROC
    mov eax, g_gpu_temp
    cmp eax, RAWRXD_TEMP_CRITICAL
    jae critical
    cmp eax, RAWRXD_TEMP_THRESHOLD
    jae warm
    mov g_throttle_percent, 100
    xor eax, eax
    ret
warm:
    mov g_throttle_percent, 75
    xor eax, eax
    ret
critical:
    mov g_throttle_percent, 25
    xor eax, eax
    ret
ThermalMonitor_Thread ENDP

ThermalAwareThrottling_GetCurrentTarget PROC
    mov eax, g_tps_target
    imul eax, g_throttle_percent
    mov ecx, 100
    cdq
    idiv ecx
    ret
ThermalAwareThrottling_GetCurrentTarget ENDP
END
