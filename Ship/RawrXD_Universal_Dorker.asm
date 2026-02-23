; RawrXD_Universal_Dorker.asm — x64 MASM thunks only; all logic is in C++ (RawrXD_Universal_Dorker.cpp). XOR, hotpatch, secure query, universal dorks, result reverser.
; Build: ml64 /c Ship\RawrXD_Universal_Dorker.asm
; Link with RawrXD_Universal_Dorker.obj (from RawrXD_Universal_Dorker.cpp)

OPTION CASE_SENSITIVE
.CODE

EXTERN UniversalDorker_XorObfuscate : PROC
EXTERN UniversalDorker_ApplyHotpatch : PROC
EXTERN UniversalDorker_BuildSecureQuery : PROC
EXTERN UniversalDorker_GenerateUniversalDorks : PROC
EXTERN UniversalDorker_AnalyzeResult : PROC
EXTERN UniversalDorker_IDE_Command_UniversalDorkScan : PROC

; void UniversalDorker_XorObfuscate(const unsigned char* inBuf, int inLen, unsigned char* outBuf, int* outLen, unsigned char keyByte)
; RCX=inBuf, RDX=inLen, R8=outBuf, R9=outLen, keyByte at [rsp+28h] at entry
Masm_XorObfuscate PROC
    sub rsp, 40h
    movzx eax, byte ptr [rsp+40h+28h] ; 5th param keyByte
    mov [rsp+20h], eax
    call UniversalDorker_XorObfuscate
    add rsp, 40h
    ret
Masm_XorObfuscate ENDP

; int UniversalDorker_ApplyHotpatch(const char* urlIn, const char* marker, char* resultBuf, int resultSize)
; RCX=urlIn, RDX=marker, R8=resultBuf, R9=resultSize
Masm_ApplyHotpatch PROC
    sub rsp, 40h
    call UniversalDorker_ApplyHotpatch
    add rsp, 40h
    ret
Masm_ApplyHotpatch ENDP

; int UniversalDorker_BuildSecureQuery(const char* templateSql, const char* tableWhitelist, const char* columnWhitelist, int limit1, char* outSql, int outSize)
; RCX=templateSql, RDX=tableWhitelist, R8=columnWhitelist, R9=limit1; outSql at [rsp+28h], outSize at [rsp+30h]
Masm_BuildSecureQuery PROC
    sub rsp, 40h
    mov rax, [rsp+40h+28h]
    mov [rsp+20h], rax
    mov eax, [rsp+40h+30h]
    mov [rsp+28h], eax
    call UniversalDorker_BuildSecureQuery
    add rsp, 40h
    ret
Masm_BuildSecureQuery ENDP

; int UniversalDorker_GenerateUniversalDorks(int obfuscate, char** outDorks, int maxDorks, int* actualCount)
; RCX=obfuscate, RDX=outDorks, R8=maxDorks, R9=actualCount
Masm_GenerateUniversalDorks PROC
    sub rsp, 40h
    call UniversalDorker_GenerateUniversalDorks
    add rsp, 40h
    ret
Masm_GenerateUniversalDorks ENDP

; int UniversalDorker_AnalyzeResult(const char* url, const char* responseBody, int bodyLen, char* outVerdict, int outSize, int* severity)
; RCX=url, RDX=responseBody, R8=bodyLen, R9=outVerdict; outSize at [rsp+28h], severity at [rsp+30h]
Masm_AnalyzeResult PROC
    sub rsp, 40h
    mov eax, [rsp+40h+28h]
    mov [rsp+20h], eax
    mov rax, [rsp+40h+30h]
    mov [rsp+28h], rax
    call UniversalDorker_AnalyzeResult
    add rsp, 40h
    ret
Masm_AnalyzeResult ENDP

; void UniversalDorker_IDE_Command_UniversalDorkScan(void)
IDE_Command_UniversalDorkScan PROC
    sub rsp, 40h
    call UniversalDorker_IDE_Command_UniversalDorkScan
    add rsp, 40h
    ret
IDE_Command_UniversalDorkScan ENDP

END
