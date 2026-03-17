; ============================================================================
; IDE_05_HTTP.asm - WinHTTP bearer token download
; ============================================================================
include IDE_INC.ASM

; External from winhttp_download.asm
EXTERN WinHTTP_DownloadToFileBearerA:PROC

PUBLIC IDEHTTP_DownloadToFileBearer

.code

IDEHTTP_DownloadToFileBearer PROC pszUrl:DWORD, pszToken:DWORD, pszPath:DWORD
    push pszPath
    push pszToken
    push pszUrl
    call WinHTTP_DownloadToFileBearerA
    ret
IDEHTTP_DownloadToFileBearer ENDP

END
