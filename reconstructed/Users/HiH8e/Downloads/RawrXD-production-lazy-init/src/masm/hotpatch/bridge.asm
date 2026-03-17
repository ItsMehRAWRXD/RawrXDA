; hotpatch_bridge.asm - MASM wrappers around C++ hotpatch bridge exports
; Provides MASM-callable entry points to drive UnifiedHotpatchManager via extern "C" functions.

option casemap:none

EXTERN hotpatch_manager_init:PROC
EXTERN hotpatch_manager_enable_layers:PROC
EXTERN hotpatch_manager_optimize:PROC
EXTERN hotpatch_manager_apply_safety:PROC
EXTERN hotpatch_manager_boost_speed:PROC

.code

PUBLIC hotpatch_init_masm
hotpatch_init_masm PROC
    jmp hotpatch_manager_init
hotpatch_init_masm ENDP

PUBLIC hotpatch_enable_layers_masm
hotpatch_enable_layers_masm PROC
    ; rcx=enableMemory, rdx=enableByte, r8=enableServer
    jmp hotpatch_manager_enable_layers
hotpatch_enable_layers_masm ENDP

PUBLIC hotpatch_optimize_masm
hotpatch_optimize_masm PROC
    jmp hotpatch_manager_optimize
hotpatch_optimize_masm ENDP

PUBLIC hotpatch_apply_safety_masm
hotpatch_apply_safety_masm PROC
    jmp hotpatch_manager_apply_safety
hotpatch_apply_safety_masm ENDP

PUBLIC hotpatch_boost_speed_masm
hotpatch_boost_speed_masm PROC
    jmp hotpatch_manager_boost_speed
hotpatch_boost_speed_masm ENDP

END
