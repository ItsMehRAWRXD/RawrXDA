; ============================================================================
; security_enterprise.asm
; Security foundation: DPAPI protect/unprotect + secure zeroing + checksum
; ============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\wincrypt.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\advapi32.lib

.data
    szEntropyLabel db "RawrXD-Entropy",0

.code

; ---------------------------------------------------------------------------
; Security_Init - placeholder for any future global init
; ---------------------------------------------------------------------------
Security_Init proc
    mov eax, TRUE
    ret
Security_Init endp

; ---------------------------------------------------------------------------
; Security_SecureZero - zero a buffer securely
; ---------------------------------------------------------------------------
Security_SecureZero proc pBuf:DWORD, cbLen:DWORD
    invoke RtlSecureZeroMemory, pBuf, cbLen
    mov eax, TRUE
    ret
Security_SecureZero endp

; ---------------------------------------------------------------------------
; Security_ProtectStringToHex
; Uses DPAPI (machine scope + entropy). Input: pszIn (null-terminated). Output: pszOut hex.
; ---------------------------------------------------------------------------
Security_ProtectStringToHex proc pszIn:DWORD, pszOut:DWORD, cchOut:DWORD
    LOCAL inBlob:DATA_BLOB
    LOCAL outBlob:DATA_BLOB
    LOCAL pEntropy:DATA_BLOB
    LOCAL i:DWORD
    LOCAL pOut:DWORD

    ; Build input blob
    invoke lstrlen, pszIn
    mov inBlob.cbData, eax
    mov inBlob.pbData, pszIn

    ; Entropy
    mov pEntropy.cbData, sizeof szEntropyLabel
    mov pEntropy.pbData, offset szEntropyLabel

    ; Protect (machine scope, UI forbidden)
    invoke CryptProtectData, addr inBlob, NULL, addr pEntropy, NULL, NULL, CRYPTPROTECT_LOCAL_MACHINE or CRYPTPROTECT_UI_FORBIDDEN, addr outBlob
    test eax, eax
    jz @Fail

    ; Hex encode
    mov eax, outBlob.pbData
    mov pOut, pszOut
    xor ecx, ecx
    mov i, ecx
@@hexloop:
    cmp i, outBlob.cbData
    jae @HexDone
    movzx ebx, byte ptr [eax+i]
    mov edx, ebx
    shr ebx, 4
    and ebx, 0Fh
    call HexNibble
    mov byte ptr [pOut], al
    inc pOut
    mov ebx, edx
    and ebx, 0Fh
    call HexNibble
    mov byte ptr [pOut], al
    inc pOut
    inc i
    jmp @@hexloop
@HexDone:
    mov byte ptr [pOut], 0
    invoke LocalFree, outBlob.pbData
    mov eax, TRUE
    ret

@Fail:
    xor eax, eax
    ret

; Converts nibble in BL to ASCII hex in AL
HexNibble:
    mov al, bl
    add al, '0'
    cmp al, '9'
    jle @retNib
    add al, 7
@retNib:
    ret
Security_ProtectStringToHex endp

; ---------------------------------------------------------------------------
; Security_UnprotectHexToString
; Input hex in pszHex; output plaintext to pszOut (null-terminated)
; ---------------------------------------------------------------------------
Security_UnprotectHexToString proc pszHex:DWORD, pszOut:DWORD, cchOut:DWORD
    LOCAL inBlob:DATA_BLOB
    LOCAL outBlob:DATA_BLOB
    LOCAL pEntropy:DATA_BLOB
    LOCAL hexLen:DWORD
    LOCAL binSize:DWORD
    LOCAL pTmp:DWORD
    LOCAL i:DWORD

    invoke lstrlen, pszHex
    mov hexLen, eax
    mov eax, hexLen
    and eax, 1
    jnz @Fail

    mov eax, hexLen
    shr eax, 1
    mov binSize, eax
    invoke LocalAlloc, LMEM_FIXED, binSize
    mov pTmp, eax
    test eax, eax
    jz @Fail

    ; Hex decode
    xor ecx, ecx
    mov i, ecx
@@hexdec:
    cmp i, binSize
    jae @decDone
    mov eax, i
    shl eax, 1
    mov dl, byte ptr [pszHex + eax]
    call FromHex
    mov bl, al                 ; high nibble
    mov dl, byte ptr [pszHex + eax + 1]
    call FromHex
    shl bl, 4
    or al, bl
    mov edx, pTmp
    mov byte ptr [edx+i], al
    inc i
    jmp @@hexdec
@decDone:

    mov inBlob.cbData, binSize
    mov eax, pTmp
    mov inBlob.pbData, eax
    mov pEntropy.cbData, sizeof szEntropyLabel
    mov pEntropy.pbData, offset szEntropyLabel

    invoke CryptUnprotectData, addr inBlob, NULL, addr pEntropy, NULL, NULL, CRYPTPROTECT_UI_FORBIDDEN, addr outBlob
    invoke LocalFree, pTmp
    test eax, eax
    jz @Fail

    ; Copy to output (truncate if needed)
    mov eax, outBlob.cbData
    cmp eax, cchOut
    jb @skipTrunc
    mov eax, cchOut
    dec eax
@skipTrunc:
    push eax
    mov ecx, eax
    mov esi, outBlob.pbData
    mov edi, pszOut
    rep movsb
    mov byte ptr [edi], 0
    ; Secure wipe decrypted buffer then free
    invoke RtlSecureZeroMemory, outBlob.pbData, outBlob.cbData
    invoke LocalFree, outBlob.pbData
    pop eax
    mov eax, TRUE
    ret

@Fail:
    xor eax, eax
    ret

; FromHex: DL contains ASCII hex, returns nibble in AL and also in AH for combine
FromHex:
    mov al, dl
    sub al, '0'
    cmp al, 9
    jle @ok
    sub dl, 'A'
    add dl, 10
    mov al, dl
@ok:
    mov ah, al
    ret
Security_UnprotectHexToString endp

; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------
; Security_ComputeChecksum32 - simple FNV-1a checksum for integrity tagging
; ---------------------------------------------------------------------------
Security_ComputeChecksum32 proc pData:DWORD, cbLen:DWORD
    LOCAL i:DWORD
    mov eax, 2166136261h           ; FNV offset basis
    xor ecx, ecx
    mov i, ecx
@@loop:
    cmp i, cbLen
    jae @@done
    mov edx, pData
    xor ebx, ebx
    mov bl, [edx+i]
    xor eax, ebx
    imul eax, 16777619
    inc i
    jmp @@loop
@@done:
    ret
Security_ComputeChecksum32 endp

; Exports
; ---------------------------------------------------------------------------
public Security_Init
public Security_SecureZero
public Security_ProtectStringToHex
public Security_UnprotectHexToString
public Security_ComputeChecksum32

end
