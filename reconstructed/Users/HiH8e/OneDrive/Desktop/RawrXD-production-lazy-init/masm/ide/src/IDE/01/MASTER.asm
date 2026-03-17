; ============================================================================
; IDE_01_MASTER.asm - IDEMaster subsystem with thread-safe operations
; ============================================================================
include IDE_INC.ASM

; External from ide_master_integration.asm
EXTERN IDEMaster_Initialize:PROC
EXTERN IDEMaster_LoadModel:PROC
EXTERN IDEMaster_HotSwapModel:PROC
EXTERN IDEMaster_ExecuteAgenticTask:PROC

include IDE_INC.ASM

; External from ide_master_integration.asm
EXTERN IDEMaster_Initialize_Impl:PROC
EXTERN IDEMaster_LoadModel_Impl:PROC
EXTERN IDEMaster_HotSwapModel_Impl:PROC
EXTERN IDEMaster_ExecuteAgenticTask_Impl:PROC

PUBLIC IDEMaster_Initialize
PUBLIC IDEMaster_LoadModel
PUBLIC IDEMaster_HotSwapModel
PUBLIC IDEMaster_ExecuteAgenticTask

.code

IDEMaster_Initialize PROC
    call IDEMaster_Initialize_Impl
    ret
IDEMaster_Initialize ENDP

IDEMaster_LoadModel PROC pPath:DWORD
    push pPath
    call IDEMaster_LoadModel_Impl
    ret
IDEMaster_LoadModel ENDP

IDEMaster_HotSwapModel PROC hOld:DWORD, hNew:DWORD
    push hNew
    push hOld
    call IDEMaster_HotSwapModel_Impl
    ret
IDEMaster_HotSwapModel ENDP

IDEMaster_ExecuteAgenticTask PROC pTask:DWORD
    push pTask
    call IDEMaster_ExecuteAgenticTask_Impl
    ret
IDEMaster_ExecuteAgenticTask ENDP

END
