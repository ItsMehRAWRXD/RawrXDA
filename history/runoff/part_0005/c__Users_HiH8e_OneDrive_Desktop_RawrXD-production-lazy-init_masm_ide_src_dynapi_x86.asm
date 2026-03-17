; ============================================================================
; dynapi_x86.asm - PEB-based dynamic API resolver (x86, zero import libs)
; Exports:
;   DYN_FindModuleBaseA(pAsciiName) -> EAX=module base or 0
;   DYN_GetProcAddressA(hModule, pFuncName) -> EAX=function ptr or 0 (handles forwarders)
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc

PUBLIC DYN_FindModuleBaseA
PUBLIC DYN_GetProcAddressA

.data
    szKernel32 db "KERNEL32.DLL",0

.code

; Allow explicit fs:[] access (TEB/PEB) in flat model
assume fs:nothing

; --- helpers ---

DYN_ToUpperA PROC chVal:DWORD
    mov eax, chVal
    and eax, 0FFh
    cmp eax, 'a'
    jb  @done
    cmp eax, 'z'
    ja  @done
    sub eax, 20h
@done:
    ret
DYN_ToUpperA ENDP

; Compare UNICODE_STRING (buffer+length) to ASCII string (case-insensitive)
; int DYN_UnicodeEqAscii(pWBuf, wLenBytes, pAscii)
DYN_UnicodeEqAscii PROC pWBuf:DWORD, wLenBytes:DWORD, pAscii:DWORD
    push esi
    push edi
    push ebx

    mov esi, pWBuf       ; wchar*
    mov ebx, wLenBytes   ; bytes
    mov edi, pAscii      ; char*

    xor ecx, ecx         ; index

@loop:
    mov al, byte ptr [edi]
    cmp al, 0
    je  @ascii_end

    ; if idx*2 >= len => mismatch
    mov edx, ecx
    shl edx, 1
    cmp edx, ebx
    jae @mismatch

    ; load wchar low byte
    movzx eax, word ptr [esi + edx]
    and eax, 0FFh
    push eax
    call DYN_ToUpperA
    mov ah, al

    movzx eax, byte ptr [edi]
    push eax
    call DYN_ToUpperA

    cmp al, ah
    jne @mismatch

    inc edi
    inc ecx
    jmp @loop

@ascii_end:
    ; must consume full unicode length
    mov edx, ecx
    shl edx, 1
    cmp edx, ebx
    jne @mismatch

    mov eax, 1
    jmp @done

@mismatch:
    xor eax, eax

@done:
    pop ebx
    pop edi
    pop esi
    ret
DYN_UnicodeEqAscii ENDP

; EAX = PEB
DYN_GetPEB PROC
    mov eax, fs:[30h]
    ret
DYN_GetPEB ENDP

; DWORD DYN_FindModuleBaseA(char* name)
DYN_FindModuleBaseA PROC pAsciiName:DWORD
    push esi
    push edi
    push ebx

    call DYN_GetPEB
    test eax, eax
    jz  @not_found

    ; PEB->Ldr (offset 0Ch)
    mov eax, [eax+0Ch]
    test eax, eax
    jz  @not_found

    ; Ldr->InMemoryOrderModuleList (offset 14h) LIST_ENTRY
    lea esi, [eax+14h]
    mov edi, [esi]          ; Flink

@walk:
    cmp edi, esi
    je  @not_found

    ; entry = flink - 8 (InMemoryOrderLinks offset)
    mov ebx, edi
    sub ebx, 8

    ; BaseDllName UNICODE_STRING at +2Ch
    movzx ecx, word ptr [ebx+2Ch]  ; Length
    mov edx, [ebx+30h]             ; Buffer

    push pAsciiName
    push ecx
    push edx
    call DYN_UnicodeEqAscii
    test eax, eax
    jnz @found

    mov edi, [edi] ; next Flink
    jmp @walk

@found:
    mov eax, [ebx+18h] ; DllBase
    jmp @done

@not_found:
    xor eax, eax

@done:
    pop ebx
    pop edi
    pop esi
    ret
DYN_FindModuleBaseA ENDP

; Compare ASCII strings (case-sensitive)
DYN_StrEqA PROC pA:DWORD, pB:DWORD
    push esi
    push edi
    mov esi, pA
    mov edi, pB
@cmp:
    mov al, [esi]
    mov dl, [edi]
    cmp al, dl
    jne @no
    cmp al, 0
    je  @yes
    inc esi
    inc edi
    jmp @cmp
@yes:
    mov eax, 1
    jmp @out
@no:
    xor eax, eax
@out:
    pop edi
    pop esi
    ret
DYN_StrEqA ENDP

; Parse PE export directory and locate function address.
; EAX=0 if not found.
DYN_GetProcAddressA PROC hModule:DWORD, pFuncName:DWORD
    push esi
    push edi
    push ebx
    push ebp

    mov ebp, hModule
    test ebp, ebp
    jz  @fail

    ; DOS header
    cmp word ptr [ebp], 'MZ'
    jne @fail

    mov eax, [ebp+3Ch]     ; e_lfanew
    add eax, ebp

    ; NT headers
    cmp dword ptr [eax], 'PE'
    jne @fail

    ; OptionalHeader start = NT + 18h
    lea esi, [eax+18h]

    ; Export DataDirectory is at OptionalHeader + 60h
    mov edx, [esi+60h]     ; Export RVA
    mov ecx, [esi+64h]     ; Export Size
    test edx, edx
    jz  @fail

    mov ebx, edx           ; exportRVA
    mov edi, ecx           ; exportSize

    lea eax, [ebp+ebx]     ; exportDir
    mov esi, eax

    mov ecx, [esi+18h]     ; NumberOfNames
    test ecx, ecx
    jz  @fail

    mov edx, [esi+20h]     ; AddressOfNames RVA
    add edx, ebp

    mov eax, [esi+24h]     ; AddressOfNameOrdinals RVA
    add eax, ebp
    mov ebx, eax

    mov eax, [esi+1Ch]     ; AddressOfFunctions RVA
    add eax, ebp
    mov esi, eax

    xor eax, eax           ; i=0

@name_loop:
    cmp eax, ecx
    jae @fail

    mov edi, [edx + eax*4] ; name RVA
    add edi, ebp           ; name ptr

    push pFuncName
    push edi
    call DYN_StrEqA
    test eax, eax
    jnz @match

    inc eax
    jmp @name_loop

@match:
    ; ordinal = ordinals[i]
    movzx eax, word ptr [ebx + eax*2]

    ; funcRVA = functions[ordinal]
    mov eax, [esi + eax*4]

    ; handle forwarder if funcRVA within export directory range
    ; exportRVA in ??? We saved earlier in ebx but reused.
    ; Recompute exportRVA/exportSize from optional header again
    ; (cheap and keeps registers simple)

    ; re-derive exportRVA/exportSize
    mov edi, hModule
    mov edx, [edi+3Ch]
    add edx, edi
    lea edx, [edx+18h]
    mov ecx, [edx+60h]     ; exportRVA
    mov edx, [edx+64h]     ; exportSize

    cmp eax, ecx
    jb  @return_ptr
    mov ebx, ecx
    add ebx, edx
    cmp eax, ebx
    jae @return_ptr

    ; forwarder string at (module + funcRVA)
    mov ebx, hModule
    add ebx, eax

    ; stack scratch: 256 bytes (mod 64, func 192)
    sub esp, 256
    lea edi, [esp]         ; modbuf
    lea esi, [esp+64]      ; funcbuf

    ; copy module up to '.'
    xor ecx, ecx
@fwd_mod:
    mov al, [ebx]
    cmp al, 0
    je  @fwd_fail
    cmp al, '.'
    je  @fwd_mod_done
    mov [edi+ecx], al
    inc ebx
    inc ecx
    cmp ecx, 60
    jb  @fwd_mod
    jmp @fwd_fail

@fwd_mod_done:
    mov byte ptr [edi+ecx], 0
    inc ebx                ; skip '.'

    ; copy function part
    xor ecx, ecx
@fwd_fun:
    mov al, [ebx]
    mov [esi+ecx], al
    cmp al, 0
    je  @fwd_fun_done
    inc ebx
    inc ecx
    cmp ecx, 190
    jb  @fwd_fun
    jmp @fwd_fail

@fwd_fun_done:
    ; ensure module has .DLL
    ; if contains '.', assume has extension
    push edi
    call DYN_FindDotA
    test eax, eax
    jnz @fwd_has_ext

    ; append .DLL
    push edi
    call DYN_StrLenA
    mov ecx, eax
    mov byte ptr [edi+ecx], '.'
    mov byte ptr [edi+ecx+1], 'D'
    mov byte ptr [edi+ecx+2], 'L'
    mov byte ptr [edi+ecx+3], 'L'
    mov byte ptr [edi+ecx+4], 0

@fwd_has_ext:
    push edi
    call DYN_FindModuleBaseA
    test eax, eax
    jz  @fwd_fail

    push esi
    push eax
    call DYN_GetProcAddressA

    add esp, 256
    jmp @done

@fwd_fail:
    add esp, 256
    xor eax, eax
    jmp @done

@return_ptr:
    add eax, hModule
    jmp @done

@fail:
    xor eax, eax

@done:
    pop ebp
    pop ebx
    pop edi
    pop esi
    ret
DYN_GetProcAddressA ENDP

; Helpers for forwarder parsing
DYN_StrLenA PROC pStr:DWORD
    push edi
    mov edi, pStr
    xor eax, eax
@l:
    cmp byte ptr [edi+eax], 0
    je  @out
    inc eax
    jmp @l
@out:
    pop edi
    ret
DYN_StrLenA ENDP

DYN_FindDotA PROC pStr:DWORD
    push edi
    mov edi, pStr
@d:
    mov al, [edi]
    cmp al, 0
    je  @no
    cmp al, '.'
    je  @yes
    inc edi
    jmp @d
@yes:
    mov eax, 1
    jmp @out
@no:
    xor eax, eax
@out:
    pop edi
    ret
DYN_FindDotA ENDP

END
