    mov inner,rax ; 3072
    mov rax,12
    mov heads,rax

    ; layers = budget / (12 * hidden * hidden)
    ; hidden * hidden = 768 * 768 = 589824
    ; 12 * 589824 = 7077888 (approx cost per layer)
    mov rax,hidden
    imul rax,hidden ; rax = 768*768
    imul rax,12      ; rax = denominator
    
    mov r13,rax      ; save denominator
    mov rax,r12      ; rax = budget
    xor rdx,rdx
    div r13          ; rax = layers (budget / cost_per_layer)
    
    ; Clamp layers (max 48)
    cmp rax,48
    jle @NoClamp
    mov rax,48
@NoClamp:
    mov layers,rax

    ; params = layers * 12 * hidden * hidden (total float32 weights)
    mov rax,layers
    imul rax,r13 ; layers * cost_per_layer (r13 = 12 * hidden * hidden)
    mov params,rax
    
    ; ---- print JSON ----
    PRINT fmtJson
    PRINT jsonStart
    ; Print Model Name (keyStr)
    lea rsi,keyStr
    call StrLen
    mov rcx,STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx,rax
    lea rdx,keyStr
    mov r8,rbx
    xor r9,r9
    push 0
    call WriteConsoleA
    
    PRINT jsonMid
    mov rcx,hidden
    call PrintU64
    PRINT jsonMid2
    mov rcx,layers
    call PrintU64
    PRINT jsonMid3
    mov rcx,heads
    call PrintU64
    PRINT jsonMid4
    mov rcx,inner
    call PrintU64
    PRINT jsonEnd
    mov rcx,params
    call PrintU64
    PRINT jsonClose

    ; ---- write .bin ----
    ; Create Filename
    lea rsi,nameStr
    call StrLen
    lea rdi,[rsp+40h]
    mov rcx,rbx
    rep movsb
    
    lea rsi,binExt ; Append .bin
    call StrCat

    ; CreateFileA
    mov rcx,GENERIC_WRITE
    mov rdx,0
    lea r8,[rsp+40h]
    mov r9,0
    mov qword ptr [rsp+28h],CREATE_ALWAYS
    mov qword ptr [rsp+30h],FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+38h],0
    call CreateFileA
    mov hBin,rax

    ; write dead floats - using pure assembly LCG PRNG
    mov r12,params
    imul r12,4 ; total bytes
    lea rdi,[recvBuf] ; Use recvBuf as temporary storage for 64KB of weights
    
        lea  rsi,keyStr
        call StrLen
        xor  rcx,rcx
    @@: lodsb
        test al,al
        jz   @F
        add  ecx,eax
        dec  rbx
        jnz  @B
    @@: add  rax,ecx             ; budget = harvested/7 + keyHash
        mov  r13d,ecx             ; seed PRNG with key hash
    mov r14, 0    ; Bytes written to buffer
    
@FillBufferLoop:
    cmp r14, sizeof recvBuf
    jae @WriteBuffer

    ; Simple 32-bit LCG: next = (seed * 1664525) + 1013904223
    mov eax, r13d
    imul eax, 1664525
    add eax, 1013904223
    mov r13d, eax ; Save new seed
    
    ; Store random 32-bit int (which represents a dead float32)
    mov [rdi+r14], eax
    add r14, 4
    sub r12, 4
    jnz @FillBufferLoop ; continue filling if more bytes needed
    
@WriteBuffer:
    ; WriteFile (rcx=hFile, rdx=lpBuffer, r8=nBytesToWrite, r9=lpNumberOfBytesWritten, [rsp+28h]=lpOverlapped)
    mov rcx,hBin
    lea rdx,[recvBuf]
    mov r8,r14 ; bytes in buffer
    lea r9,bytesWritten
    push 0
    call WriteFile
    
    cmp r12, 0
    jnz @FillBufferLoop ; If total bytes remaining (r12) > 0, reset buffer and fill again

    mov rcx,hBin
    call CloseHandle

    PRINT fmtFiles
    ; Print model name twice for output description
    mov rcx,STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx,rax
    lea rdx,nameStr
    mov r8,rbx ; Use last known length in RBX (from StrLen call above)
    xor r9,r9
    push 0
    call WriteConsoleA
    PRINT fmtJsonFile
    
    mov rcx,STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rcx,rax
    lea rdx,nameStr
    mov r8,rbx
    xor r9,r9
    push 0
    call WriteConsoleA
    PRINT fmtBinFile

    add rsp,88h
    pop r15 r14 r13 r12 rsi rdi rbx
    ret
MakeDeadModel endp

; ---------- helpers ----------
ConInLine proc
    push rbx
    mov rcx,STD_INPUT_HANDLE
    call GetStdHandle
    mov rcx,rax
    lea rdx,bufIn
    mov r8,sizeof bufIn
    lea r9,bytesWritten
    push 0
    call ReadConsoleA
    ; strip CR/LF
    lea rdi,bufIn
    mov rcx,bytesWritten
    call StripCRLF
    pop rbx
    ret
ConInLine endp

StripCRLF proc
    ; Corrected logic to ensure we don't try to access negative index
    cmp rcx,0
    je @F
    lea rdi,bufIn
    add rdi,rcx
    dec rdi ; rdi = last char
    
    ; Check for LF (0x0A)
    cmp byte ptr [rdi],10
    je @RemoveLF
    ret
    
@RemoveLF:
    mov byte ptr [rdi],0
    dec rdi
    cmp rcx,1
    jle @F ; only one char (LF), done
    
    ; Check for CR (0x0D)
    cmp byte ptr [rdi],13
    je @RemoveCR
    ret

@RemoveCR:
    mov byte ptr [rdi],0
@@:
    ret
StripCRLF endp

StrLen proc
    ; returns length in RBX
    mov rbx,rcx
    xor al,al
@@: cmp byte ptr [rbx],al
    je @F
    inc rbx
    jmp @B
@@: sub rbx,rcx
    ret
StrLen endp

StrCpy proc
    ; rsi->src, rdi->dst
@@: lodsb
    stosb
    test al,al
    jnz @B
    ret
StrCpy endp

StrCat proc
    ; rsi->src, appends to rdi
    push rbx
    mov rbx,rdi
    call StrLen
    add rdi,rbx
    pop rbx
    jmp StrCpy
StrCat endp

ParseU64 proc
    ; rsi = string, returns U64 value in RAX
    push rbx rdi
    xor rax,rax
    mov rbx,10
    mov rdi,rsi
@@: movzx ecx,byte ptr [rdi]
    cmp cl,'0'
    jl @F
    cmp cl,'9'
    jg @F
    sub cl,'0'
    imul rax,rbx
    add rax,rcx
    inc rdi
    jmp @B
@@: pop rdi rbx
    ret
ParseU64 endp

U64ToAscii proc
    ; rcx = value, appends to rdi
    push rbx rsi r12 r13
    mov rbx,10
    lea r12,[rsp+20h] ; temp buffer
    mov byte ptr [r12],0
    dec r12
    mov r13,rdi ; Save original RDI (end of main buffer)

@DivideLoop:
    xor rdx,rdx
    mov rax,rcx
    div rbx ; rax=quotient, rdx=remainder
    add dl,'0'
    mov [r12],dl
    dec r12
    mov rcx,rax
    test rcx,rcx
    jnz @DivideLoop
    
    inc r12 ; R12 is now the start of the ASCII string
    mov rsi,r12
    mov rdi,r13
    call StrCat ; Appends the number string to RDI buffer

    pop r13 r12 rsi rbx
    ret
U64ToAscii endp

BuildHTTP proc
    push rbx rsi rdi r12 r13 r14 r15
    sub rsp,88h

    lea  rdi,httpBuf
    lea  rsi,httpTemplate
    call StrCpy
    lea  rsi,hostStr
    call StrCat
    mov  al,':'
    stosb
    lea  rsi,portStr
    call StrCat
    mov  word ptr [rdi],0x0A0D ; append CRLF inside buffer
    add  rdi,2
    lea  rsi,httpTail
    call StrCat
    mov  word ptr [rdi],0x0A0D
    add  rdi,2
    lea  rsi,httpBody1
    call StrCat
    lea  rsi,promptStr
    call StrCat
    lea  rsi,httpBody2
    call StrCat
    add rsp,88h
    pop r15 r14 r13 r12 rsi rdi rbx
    ret
BuildHTTP endp
    sub r8,rdi ; length of string
    xor r9,r9
    push 0
    call WriteConsoleA
    add rsp,32
    pop rdi rsi rbx
    ret
PrintU64 endp

end
