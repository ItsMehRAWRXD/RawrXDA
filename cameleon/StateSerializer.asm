; RawrXD_StateSerializer.asm - Persist extension state across migrations
OPTION CASEMAP:NONE

includelib kernel32.lib

.data
szStateFile     DB "RawrXD_Cameleon.state",0

.code
SerializeExtensionState PROC
    ret
SerializeExtensionState ENDP

DeserializeExtensionState PROC
    ret
DeserializeExtensionState ENDP

PUBLIC SerializeExtensionState
PUBLIC DeserializeExtensionState
END
