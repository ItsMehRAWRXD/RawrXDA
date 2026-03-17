; ============================================================
; FILE: rawr_cpu_features.inc
; PURPOSE: EXTERNDEF declarations for the canonical CPU
;          feature gates in rawr_cpu_features.asm
; ============================================================
IFNDEF RAWR_CPU_FEATURES_INC
RAWR_CPU_FEATURES_INC EQU 1

; ── Runtime CPU gates (return EAX=1 safe / 0 unsafe) ──────
EXTERNDEF rawr_cpu_has_avx512:PROC
EXTERNDEF rawr_cpu_has_avx2:PROC

ENDIF ; RAWR_CPU_FEATURES_INC
