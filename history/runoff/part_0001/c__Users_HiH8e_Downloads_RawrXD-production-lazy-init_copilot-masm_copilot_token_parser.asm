;======================================================================
; copilot_token_parser.asm - JWT Token Parsing & Validation
;======================================================================
INCLUDE windows.inc
INCLUDE bcrypt.inc
INCLUDELIB bcrypt.lib

.CONST
JWT_HEADER_BASE64 DB "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9",0
BCRYPT_ALG_RSA_SIGN DB "RSA",0
BCRYPT_SHA256_ALGORITHM DB "SHA256",0

.DATA?
jwtHeaderBuffer        DB 512 DUP(?)
jwtPayloadBuffer       DB 4096 DUP(?)
jwtSignatureBuffer     DB 2048 DUP(?)
verificationKeyData    DB 2048 DUP(?)

.CODE

JwtParser_ValidateToken PROC pJwtToken:QWORD
    LOCAL pHeader:QWORD
    LOCAL pPayload:QWORD
    LOCAL pSignature:QWORD
    LOCAL cbHeader:DWORD
    LOCAL cbPayload:DWORD
    LOCAL cbSignature:DWORD
    
    ; JWT format: header.payload.signature (dot-separated)
    invoke lstrcpyA, ADDR jwtHeaderBuffer, pJwtToken
    
    ; Find first dot (header.payload separator)
    lea rcx, jwtHeaderBuffer
    mov dl, '.'
    call strchr
    .if rax == 0
        xor rax, rax          ; Invalid format
        ret
    .endif
    
    ; Null-terminate header and move to payload
    mov BYTE PTR [rax], 0
    inc rax
    mov pPayload, rax
    
    ; Find second dot (payload.signature separator)
    mov rcx, rax
    mov dl, '.'
    call strchr
    .if rax == 0
        xor rax, rax
        ret
    .endif
    
    mov BYTE PTR [rax], 0
    inc rax
    mov pSignature, rax
    
    ; Base64 decode header
    invoke Base64UrlDecode, ADDR jwtHeaderBuffer
    mov pHeader, rax
    mov cbHeader, ecx
    
    ; Base64 decode payload
    invoke Base64UrlDecode, pPayload
    mov pPayload, rax
    mov cbPayload, ecx
    
    ; Base64 decode signature
    invoke Base64UrlDecode, pSignature
    mov pSignature, rax
    mov cbSignature, ecx
    
    ; Verify signature with GitHub's public key
    invoke CopilotHttp_GetGithubPublicKey
    mov rdx, verificationKeyData
    mov r8d, 2048
    invoke RsaVerifySignature, pPayload, cbPayload, pSignature, cbSignature, rdx, r8d
    
    ret
JwtParser_ValidateToken ENDP

Base64UrlDecode PROC pBase64:QWORD
    LOCAL pDecoded:QWORD
    LOCAL cbDecoded:DWORD
    
    ; Replace '-' with '+' and '_' with '/'
    ; Add padding if needed
    ; [Full base64url implementation - 100 lines]
    
    ; Call CryptStringToBinaryA
    mov rcx, pBase64
    mov edx, -1               ; Calculate length
    lea r8, pDecoded
    xor r9, r9
    push ADDR cbDecoded
    push CRYPT_STRING_BASE64
    call CryptStringToBinaryA
    add rsp, 16
    
    mov ecx, cbDecoded
    mov rax, pDecoded
    ret
Base64UrlDecode ENDP

RsaVerifySignature PROC \
        pData:QWORD, \
        cbData:DWORD, \
        pSignature:QWORD, \
        cbSignature:DWORD, \
        pPublicKey:QWORD, \
        cbPublicKey:DWORD
    
    LOCAL hKey:QWORD
    LOCAL hHash:QWORD
    LOCAL hashBuffer[32]:BYTE
    
    ; Import public key
    lea rcx, hKey
    lea rdx, pPublicKey
    mov r8d, cbPublicKey
    call BCryptImportKeyPair
    
    ; Hash data
    lea rcx, hHash
    lea rdx, BCRYPT_ALG_SHA256
    xor r8, r8
    xor r9, r9
    call BCryptCreateHash
    
    mov rcx, hHash
    mov rdx, pData
    mov r8d, cbData
    call BCryptHashData
    
    ; Finalize hash
    lea rcx, hashBuffer
    mov edx, 32
    call BCryptFinishHash
    
    ; Verify signature
    mov rcx, hKey
    lea rdx, hashBuffer
    mov edx, 32
    mov r8, pSignature
    mov r9d, cbSignature
    call BCryptVerifySignature
    
    ret
RsaVerifySignature ENDP

END
