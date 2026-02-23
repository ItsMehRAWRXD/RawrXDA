; RawrXD_DorkScanner_MASM.asm — x64 MASM integration for Google Dork Scanner
; Calls C API: DorkScanner_Create, DorkScanner_ScanSingle, etc.
; Build: ml64 /c RawrXD_DorkScanner_MASM.asm
; Link with RawrXD_GoogleDork_Scanner.obj (from RawrXD_GoogleDork_Scanner.cpp)

OPTION CASE_SENSITIVE
.CODE

; C API externs (from RawrXD_GoogleDork_Scanner.cpp)
EXTERN DorkScanner_Create : PROC
EXTERN DorkScanner_Destroy : PROC
EXTERN DorkScanner_Initialize : PROC
EXTERN DorkScanner_SetProgressCallback : PROC
EXTERN DorkScanner_ScanSingle : PROC
EXTERN DorkScanner_ScanFile : PROC
EXTERN DorkScanner_ExportToJson : PROC
EXTERN DorkScanner_GetBuiltinDorkCount : PROC
EXTERN DorkScanner_GetBuiltinDork : PROC
EXTERN DorkScanner_TestBooleanPayloads : PROC

; DorkScannerConfig: threadCount(4), delayMs(4), timeoutMs(4), userAgent(8), proxyUrl(8), maxIterations(4), enableErrorBased(4), enableTimeBased(4), enableBoolean(4)
; RCX = config* (or NULL for defaults)

; void* DorkScanner_Create(const DorkScannerConfig* config)
; RCX = config
DorkScanner_Create_ASM PROC
    sub rsp, 28h
    call DorkScanner_Create
    add rsp, 28h
    ret
DorkScanner_Create_ASM ENDP

; void DorkScanner_Destroy(void* scanner)
; RCX = scanner
DorkScanner_Destroy_ASM PROC
    sub rsp, 28h
    call DorkScanner_Destroy
    add rsp, 28h
    ret
DorkScanner_Destroy_ASM ENDP

; int DorkScanner_Initialize(void* scanner, void* userData)
; RCX = scanner, RDX = userData
DorkScanner_Initialize_ASM PROC
    sub rsp, 28h
    call DorkScanner_Initialize
    add rsp, 28h
    ret
DorkScanner_Initialize_ASM ENDP

; void DorkScanner_SetProgressCallback(void* scanner, DorkProgressFn fn)
; RCX = scanner, RDX = fn
DorkScanner_SetProgressCallback_ASM PROC
    sub rsp, 28h
    call DorkScanner_SetProgressCallback
    add rsp, 28h
    ret
DorkScanner_SetProgressCallback_ASM ENDP

; int DorkScanner_ScanSingle(void* scanner, const char* dork, DorkResult* results, int maxResults)
; RCX = scanner, RDX = dork, R8 = results, R9 = maxResults
DorkScanner_ScanSingle_ASM PROC
    sub rsp, 28h
    call DorkScanner_ScanSingle
    add rsp, 28h
    ret
DorkScanner_ScanSingle_ASM ENDP

; int DorkScanner_ScanFile(void* scanner, const char* dorkFilePath, DorkResult* results, int maxResults)
; RCX = scanner, RDX = dorkFilePath, R8 = results, R9 = maxResults
DorkScanner_ScanFile_ASM PROC
    sub rsp, 28h
    call DorkScanner_ScanFile
    add rsp, 28h
    ret
DorkScanner_ScanFile_ASM ENDP

; int DorkScanner_ExportToJson(void* scanner, const char* filePath)
; RCX = scanner, RDX = filePath
DorkScanner_ExportToJson_ASM PROC
    sub rsp, 28h
    call DorkScanner_ExportToJson
    add rsp, 28h
    ret
DorkScanner_ExportToJson_ASM ENDP

; int DorkScanner_GetBuiltinDorkCount(void* scanner)
; RCX = scanner (can be NULL)
DorkScanner_GetBuiltinDorkCount_ASM PROC
    sub rsp, 28h
    call DorkScanner_GetBuiltinDorkCount
    add rsp, 28h
    ret
DorkScanner_GetBuiltinDorkCount_ASM ENDP

; int DorkScanner_GetBuiltinDork(void* scanner, int index, char* buf, int bufSize)
; RCX = scanner, EDX = index, R8 = buf, R9 = bufSize
DorkScanner_GetBuiltinDork_ASM PROC
    sub rsp, 28h
    call DorkScanner_GetBuiltinDork
    add rsp, 28h
    ret
DorkScanner_GetBuiltinDork_ASM ENDP

; int DorkScanner_TestBooleanPayloads(const char* baseUrl, char* outVerdict, int outSize)
; RCX = baseUrl, RDX = outVerdict, R8 = outSize
DorkScanner_TestBooleanPayloads_ASM PROC
    sub rsp, 28h
    call DorkScanner_TestBooleanPayloads
    add rsp, 28h
    ret
DorkScanner_TestBooleanPayloads_ASM ENDP

END
