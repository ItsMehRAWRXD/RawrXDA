option casemap:none

EXTERN OutputDebugStringA:PROC

PUBLIC RawrXD_Marketplace_DownloadExtension_MASM
PUBLIC RawrXD_Marketplace_ResolveSymbol_MASM

.data
    kMsgDownload db "Marketplace_DownloadExtension MASM bridge invoked",0
    kMsgResolve  db "RawrXD_Marketplace_ResolveSymbol MASM bridge invoked",0

.code
RawrXD_Marketplace_DownloadExtension_MASM PROC
    lea rcx, kMsgDownload
    call OutputDebugStringA
    ret
RawrXD_Marketplace_DownloadExtension_MASM ENDP

RawrXD_Marketplace_ResolveSymbol_MASM PROC
    lea rcx, kMsgResolve
    call OutputDebugStringA
    ret
RawrXD_Marketplace_ResolveSymbol_MASM ENDP

END
