;==============================================================================
; hotpatch_coordinator.inc - Hotpatch Coordinator Include File
; ==============================================================================
; Provides declarations for hotpatch system.
;==============================================================================

; Hotpatch zones
HOTPATCH_ZONE_VRAM_CHECK    EQU 1
HOTPATCH_ZONE_MEMORY_LIMIT  EQU 2
HOTPATCH_ZONE_TENSOR_LIMIT  EQU 3
HOTPATCH_ZONE_LOADER_CHECK  EQU 4

; External function declarations
EXTERN HotpatchApply:PROC
EXTERN ApplyBypassHotpatches:PROC
EXTERN HotpatchInitialize:PROC

