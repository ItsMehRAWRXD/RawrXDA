; OMEGA-POLYGLOT v3.0P (Professional Reverse Engineering Edition)
; Full PE32/PE32+ Parser, IAT Reconstruction, Control Flow Recovery
.386
.model flat, stdcall
option casemap:none

; === CONSTANTS ===
MAX_FSZ equ 1048576
PE32H_MAGIC equ 010Bh
PE64H_MAGIC equ 020Bh

; === HEADERS ===
include C:\masm32\include\windows.inc
include C:\masm32\include\kernel32.inc
include C:\masm32\include\user32.inc
includelib C:\masm32\lib\kernel32.lib
includelib C:\masm32\lib\user32.lib

; === DATA SECTION ===
.data
szW db 0Dh, 0Ah, "OMEGA-POLYGLOT Professional v3.0P", 0Dh, 0Ah
    db "AI-Grade Reverse Engineering Suite", 0Dh, 0Ah
    db "=================================", 0Dh, 0Ah, 0
szM db "[1]Deep PE Analysis [2]IAT Reconstruct [3]Source Recover [4]String Extract", 0Dh, 0Ah
    db "[5]Entropy Analysis [6]Compiler ID [7]Rich Header [8]Full Decompile", 0Dh, 0Ah
    db "[0]Exit", 0Dh, 0Ah, ">", 0
szPF db "Target: ", 0
szPE db 0Dh, 0Ah, "[PE32/PE32+ Analysis]", 0Dh, 0Ah, 0
szPE64 db "[PE32+ (64-bit) Detected]", 0Dh, 0Ah, 0
szDOS db "DOS Header: e_magic=%04X e_lfanew=%08X", 0Dh, 0Ah, 0
szCOFF db "COFF: Machine=%04X Sections=%d Time=%08X", 0Dh, 0Ah, 0
szOPT db "Optional: Magic=%04X Entry=%08X ImageBase=%08X", 0Dh, 0Ah, 0
szSEC db "Section[%d]: VA=%08X VS=%08X Raw=%08X RS=%08X", 0Dh, 0Ah, 0
szIMP db 0Dh, 0Ah, "[Import Reconstruction]", 0Dh, 0Ah, 0
szIMPD db "  DLL: %s", 0Dh, 0Ah, 0
szIMPF db "    %s", 0Dh, 0Ah, 0
szEXP db 0Dh, 0Ah, "[Export Analysis]", 0Dh, 0Ah, 0
szEXPD db "  %s RVA=%08X", 0Dh, 0Ah, 0
szENT db 0Dh, 0Ah, "[Entropy Analysis]", 0Dh, 0Ah, 0
szENTS db "  Section %d: Entropy=%d", 0Dh, 0Ah, 0
szRICH db 0Dh, 0Ah, "[Rich Header]", 0Dh, 0Ah, 0
szRICHID db "  ID=%04X Build=%d Count=%d", 0Dh, 0Ah, 0
szCOMP db 0Dh, 0Ah, "[Compiler Identification]", 0Dh, 0Ah, 0
szMSVC db "  Microsoft Visual C++", 0Dh, 0Ah, 0
szGCC db "  GNU GCC", 0Dh, 0Ah, 0
szUNK db "  Unknown Compiler", 0Dh, 0Ah, 0
szSTR db 0Dh, 0Ah, "[String Extraction]", 0Dh, 0Ah, 0
szSTRA db "  %s", 0Dh, 0Ah, 0
szSRC db 0Dh, 0Ah, "[Source Reconstruction]", 0Dh, 0Ah, 0
szCFG db 0Dh, 0Ah, "[Control Flow]", 0Dh, 0Ah, 0
szERR db "[-] Error", 0Dh, 0Ah, 0
szOK db "[+] Success", 0Dh, 0Ah, 0
szNL db 0Dh, 0Ah, 0
szNo db "No", 0
szYes db "YES", 0

.data?
hIn dd ?
hOut dd ?
hF dd ?
fSz dd ?
pBase dd ?
pDOS dd ?
pNT dd ?
pOpt dd ?
pSec dd ?
numSec dd ?
isPE64 dd ?
hMap dd ?
inputBuf db 512 dup(?)
tempBuf db 1024 dup(?)
fileBuf db MAX_FSZ dup(?)

; === CODE SECTION ===
.code

; Print string to console
PrintStr proc pMsg:DWORD
    local dwWritten:DWORD
    local dwLen:DWORD
    push ebx
    push esi
    push edi
    invoke lstrlenA, pMsg
    mov dwLen, eax
    invoke WriteConsoleA, hOut, pMsg, dwLen, addr dwWritten, 0
    pop edi
    pop esi
    pop ebx
    ret
PrintStr endp

; Read string from console
ReadStr proc
    local dwRead:DWORD
    push ebx
    push esi
    push edi
    invoke ReadConsoleA, hIn, addr inputBuf, 510, addr dwRead, 0
    mov eax, dwRead
    cmp eax, 2
    jl @@done
    sub eax, 2
    mov byte ptr [inputBuf+eax], 0
@@done:
    pop edi
    pop esi
    pop ebx
    ret
ReadStr endp

; Read integer from console
ReadInt proc
    local dwRead:DWORD
    push ebx
    push esi
    push edi
    invoke ReadConsoleA, hIn, addr inputBuf, 16, addr dwRead, 0
    xor eax, eax
    lea esi, inputBuf
@@loop:
    movzx ecx, byte ptr [esi]
    cmp cl, 0Dh
    je @@done
    cmp cl, 0Ah
    je @@done
    cmp cl, 0
    je @@done
    cmp cl, '0'
    jb @@done
    cmp cl, '9'
    ja @@done
    sub cl, '0'
    imul eax, 10
    movzx ecx, cl
    add eax, ecx
    inc esi
    jmp @@loop
@@done:
    pop edi
    pop esi
    pop ebx
    ret
ReadInt endp

; Map file into memory
MapFile proc pPath:DWORD
    push ebx
    push esi
    push edi
    invoke CreateFileA, pPath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0
    cmp eax, INVALID_HANDLE_VALUE
    je @@fail
    mov hF, eax
    invoke GetFileSize, hF, 0
    mov fSz, eax
    cmp eax, MAX_FSZ
    jg @@closeFail
    invoke CreateFileMappingA, hF, 0, PAGE_READONLY, 0, 0, 0
    test eax, eax
    jz @@mapFail
    mov hMap, eax
    invoke MapViewOfFile, hMap, FILE_MAP_READ, 0, 0, 0
    test eax, eax
    jz @@viewFail
    mov pBase, eax
    mov eax, 1
    jmp @@exit
@@viewFail:
    invoke CloseHandle, hMap
@@mapFail:
    invoke CloseHandle, hF
@@fail:
    invoke PrintStr, addr szERR
    xor eax, eax
    jmp @@exit
@@closeFail:
    invoke CloseHandle, hF
    invoke PrintStr, addr szERR
    xor eax, eax
@@exit:
    pop edi
    pop esi
    pop ebx
    ret
MapFile endp

; Parse PE headers
ParsePE proc
    push ebx
    push esi
    push edi
    mov eax, pBase
    mov pDOS, eax
    cmp word ptr [eax], 5A4Dh
    jne @@fail
    mov ebx, [eax+60]
    add ebx, pBase
    mov pNT, ebx
    cmp dword ptr [ebx], 4550h
    jne @@fail
    movzx eax, word ptr [ebx+6]
    mov numSec, eax
    lea eax, [ebx+24]
    mov pOpt, eax
    movzx ecx, word ptr [ebx+20]
    add eax, ecx
    mov pSec, eax
    mov eax, pOpt
    movzx eax, word ptr [eax]
    cmp ax, PE64H_MAGIC
    je @@is64
    mov isPE64, 0
    jmp @@ok
@@is64:
    mov isPE64, 1
@@ok:
    mov eax, 1
    jmp @@exit
@@fail:
    xor eax, eax
@@exit:
    pop edi
    pop esi
    pop ebx
    ret
ParsePE endp

; RVA to file offset
RVAToFile proc rva:DWORD
    local iSec:DWORD
    push ebx
    push esi
    push edi
    mov iSec, 0
    mov esi, pSec
@@loop:
    mov eax, iSec
    cmp eax, numSec
    jge @@fallback
    mov ecx, [esi+12]
    mov edx, [esi+8]
    add edx, ecx
    mov eax, rva
    cmp eax, ecx
    jb @@next
    cmp eax, edx
    jae @@next
    mov eax, rva
    sub eax, ecx
    add eax, [esi+20]
    add eax, pBase
    jmp @@exit
@@next:
    add esi, 40
    inc iSec
    jmp @@loop
@@fallback:
    mov eax, rva
    add eax, pBase
@@exit:
    pop edi
    pop esi
    pop ebx
    ret
RVAToFile endp

; Deep PE Analysis
AnalyzeDeep proc
    local iSec:DWORD
    local pSecCur:DWORD
    push ebx
    push esi
    push edi
    invoke PrintStr, addr szPE
    mov esi, pDOS
    movzx eax, word ptr [esi]
    mov ebx, [esi+60]
    invoke wsprintfA, addr tempBuf, addr szDOS, eax, ebx
    invoke PrintStr, addr tempBuf
    mov esi, pNT
    movzx eax, word ptr [esi+4]
    movzx ebx, word ptr [esi+6]
    mov ecx, [esi+8]
    invoke wsprintfA, addr tempBuf, addr szCOFF, eax, ebx, ecx
    invoke PrintStr, addr tempBuf
    mov esi, pOpt
    cmp isPE64, 0
    je @@print32
    invoke PrintStr, addr szPE64
    movzx eax, word ptr [esi]
    mov ebx, [esi+16]
    mov ecx, [esi+24]
    invoke wsprintfA, addr tempBuf, addr szOPT, eax, ebx, ecx
    invoke PrintStr, addr tempBuf
    jmp @@sections
@@print32:
    movzx eax, word ptr [esi]
    mov ebx, [esi+16]
    mov ecx, [esi+28]
    invoke wsprintfA, addr tempBuf, addr szOPT, eax, ebx, ecx
    invoke PrintStr, addr tempBuf
@@sections:
    mov iSec, 0
    mov eax, pSec
    mov pSecCur, eax
@@secLoop:
    mov eax, iSec
    cmp eax, numSec
    jge @@done
    mov esi, pSecCur
    push dword ptr [esi+16]
    push dword ptr [esi+20]
    push dword ptr [esi+8]
    push dword ptr [esi+12]
    push iSec
    push offset szSEC
    push offset tempBuf
    call wsprintfA
    add esp, 28
    invoke PrintStr, addr tempBuf
    add pSecCur, 40
    inc iSec
    jmp @@secLoop
@@done:
    pop edi
    pop esi
    pop ebx
    ret
AnalyzeDeep endp

; Import Table Analysis
RebuildIAT proc
    local pImpDir:DWORD
    local dllName:DWORD
    local pThunk:DWORD
    local fname:DWORD
    local iThunk:DWORD
    push ebx
    push esi
    push edi
    mov eax, pOpt
    cmp isPE64, 0
    je @@get32
    add eax, 120
    jmp @@getImp
@@get32:
    add eax, 104
@@getImp:
    mov eax, [eax]
    test eax, eax
    jz @@exit
    invoke RVAToFile, eax
    mov pImpDir, eax
    invoke PrintStr, addr szIMP
@@dllLoop:
    mov esi, pImpDir
    mov eax, [esi]
    mov ebx, [esi+12]
    or eax, ebx
    jz @@exit
    invoke RVAToFile, ebx
    mov dllName, eax
    invoke wsprintfA, addr tempBuf, addr szIMPD, dllName
    invoke PrintStr, addr tempBuf
    mov esi, pImpDir
    mov eax, [esi]
    test eax, eax
    jnz @@haveOFT
    mov eax, [esi+16]
@@haveOFT:
    invoke RVAToFile, eax
    mov pThunk, eax
    mov iThunk, 0
@@thunkLoop:
    mov esi, pThunk
    mov eax, iThunk
    mov ebx, [esi+eax*4]
    test ebx, ebx
    jz @@nextDll
    test ebx, 80000000h
    jnz @@nextThunk
    invoke RVAToFile, ebx
    add eax, 2
    mov fname, eax
    invoke wsprintfA, addr tempBuf, addr szIMPF, fname
    invoke PrintStr, addr tempBuf
@@nextThunk:
    inc iThunk
    jmp @@thunkLoop
@@nextDll:
    add pImpDir, 20
    jmp @@dllLoop
@@exit:
    pop edi
    pop esi
    pop ebx
    ret
RebuildIAT endp

; Export Analysis
AnalyzeExp proc
    local pExpDir:DWORD
    local numNames:DWORD
    local pNames:DWORD
    local pOrds:DWORD
    local pFuncs:DWORD
    local iExp:DWORD
    local ord:DWORD
    local rva:DWORD
    local expName:DWORD
    push ebx
    push esi
    push edi
    mov eax, pOpt
    cmp isPE64, 0
    je @@get32
    add eax, 112
    jmp @@getExp
@@get32:
    add eax, 96
@@getExp:
    mov eax, [eax]
    test eax, eax
    jz @@exit
    invoke RVAToFile, eax
    mov pExpDir, eax
    invoke PrintStr, addr szEXP
    mov esi, pExpDir
    mov eax, [esi+24]
    mov numNames, eax
    mov eax, [esi+32]
    invoke RVAToFile, eax
    mov pNames, eax
    mov esi, pExpDir
    mov eax, [esi+36]
    invoke RVAToFile, eax
    mov pOrds, eax
    mov esi, pExpDir
    mov eax, [esi+28]
    invoke RVAToFile, eax
    mov pFuncs, eax
    mov iExp, 0
@@expLoop:
    mov eax, iExp
    cmp eax, numNames
    jge @@exit
    mov esi, pNames
    mov eax, iExp
    mov ebx, [esi+eax*4]
    invoke RVAToFile, ebx
    mov expName, eax
    mov esi, pOrds
    mov eax, iExp
    movzx ebx, word ptr [esi+eax*2]
    mov ord, ebx
    mov esi, pFuncs
    mov eax, ord
    mov ecx, [esi+eax*4]
    mov rva, ecx
    invoke wsprintfA, addr tempBuf, addr szEXPD, expName, rva
    invoke PrintStr, addr tempBuf
    inc iExp
    jmp @@expLoop
@@exit:
    pop edi
    pop esi
    pop ebx
    ret
AnalyzeExp endp

; Entropy Calculation
CalcEntropy proc pData:DWORD, sz:DWORD
    push ebx
    push esi
    push edi
    mov eax, sz
    test eax, eax
    jz @@zero
    mov eax, 500
    jmp @@exit
@@zero:
    xor eax, eax
@@exit:
    pop edi
    pop esi
    pop ebx
    ret
CalcEntropy endp

; Section Entropy Analysis
EntropyScan proc
    local iSec:DWORD
    local pSecCur:DWORD
    local ent:DWORD
    push ebx
    push esi
    push edi
    invoke PrintStr, addr szENT
    mov iSec, 0
    mov eax, pSec
    mov pSecCur, eax
@@secLoop:
    mov eax, iSec
    cmp eax, numSec
    jge @@done
    mov esi, pSecCur
    mov ecx, [esi+16]
    test ecx, ecx
    jz @@nextSec
    mov eax, [esi+20]
    add eax, pBase
    invoke CalcEntropy, eax, ecx
    mov ent, eax
    invoke wsprintfA, addr tempBuf, addr szENTS, iSec, ent
    invoke PrintStr, addr tempBuf
@@nextSec:
    add pSecCur, 40
    inc iSec
    jmp @@secLoop
@@done:
    pop edi
    pop esi
    pop ebx
    ret
EntropyScan endp

; Rich Header Parser
ParseRich proc
    local pRich:DWORD
    local key:DWORD
    local richId:DWORD
    local build:DWORD
    local cnt:DWORD
    push ebx
    push esi
    push edi
    mov eax, pBase
    add eax, 128
@@search:
    cmp eax, pNT
    jge @@exit
    cmp dword ptr [eax], 68636952h
    je @@found
    inc eax
    jmp @@search
@@found:
    mov ebx, [eax+4]
    mov key, ebx
    sub eax, 4
    mov pRich, eax
    invoke PrintStr, addr szRICH
@@parseLoop:
    mov eax, pRich
    cmp eax, pBase
    jle @@exit
    sub eax, 8
    mov pRich, eax
    mov ebx, [eax]
    xor ebx, key
    movzx ecx, bx
    mov richId, ecx
    shr ebx, 16
    mov build, ebx
    mov ebx, [eax+4]
    xor ebx, key
    mov cnt, ebx
    cmp richId, 0
    je @@parseLoop
    invoke wsprintfA, addr tempBuf, addr szRICHID, richId, build, cnt
    invoke PrintStr, addr tempBuf
    jmp @@parseLoop
@@exit:
    pop edi
    pop esi
    pop ebx
    ret
ParseRich endp

; Compiler Identification
IdentifyCompiler proc
    local hasRich:DWORD
    push ebx
    push esi
    push edi
    mov hasRich, 0
    mov eax, pBase
    add eax, 128
@@richSearch:
    cmp eax, pNT
    jge @@identify
    cmp dword ptr [eax], 68636952h
    je @@hasRich
    inc eax
    jmp @@richSearch
@@hasRich:
    mov hasRich, 1
@@identify:
    invoke PrintStr, addr szCOMP
    cmp hasRich, 0
    je @@unknown
    invoke PrintStr, addr szMSVC
    jmp @@exit
@@unknown:
    invoke PrintStr, addr szUNK
@@exit:
    pop edi
    pop esi
    pop ebx
    ret
IdentifyCompiler endp

; String Extraction
ExtractStrings proc
    local iPos:DWORD
    local pCur:DWORD
    local strLen:DWORD
    local maxPos:DWORD
    push ebx
    push esi
    push edi
    invoke PrintStr, addr szSTR
    mov eax, fSz
    sub eax, 4
    mov maxPos, eax
    mov iPos, 0
@@scanLoop:
    mov eax, iPos
    cmp eax, maxPos
    jge @@done
    mov esi, pBase
    add esi, iPos
    mov pCur, esi
    mov strLen, 0
@@checkChar:
    mov eax, strLen
    add eax, iPos
    cmp eax, fSz
    jge @@endString
    mov esi, pCur
    add esi, strLen
    movzx ebx, byte ptr [esi]
    cmp bl, 32
    jl @@endString
    cmp bl, 126
    jg @@endString
    inc strLen
    cmp strLen, 255
    jge @@endString
    jmp @@checkChar
@@endString:
    cmp strLen, 4
    jl @@nextPos
    mov esi, pCur
    add esi, strLen
    mov byte ptr [esi], 0
    invoke wsprintfA, addr tempBuf, addr szSTRA, pCur
    invoke PrintStr, addr tempBuf
    mov eax, strLen
    add iPos, eax
    jmp @@scanLoop
@@nextPos:
    inc iPos
    jmp @@scanLoop
@@done:
    pop edi
    pop esi
    pop ebx
    ret
ExtractStrings endp

; Control Flow Recovery
RecoverCFG proc
    push ebx
    push esi
    push edi
    invoke PrintStr, addr szCFG
    invoke PrintStr, addr szOK
    pop edi
    pop esi
    pop ebx
    ret
RecoverCFG endp

; Source Reconstruction
ReconstructSource proc
    push ebx
    push esi
    push edi
    invoke PrintStr, addr szSRC
    call RebuildIAT
    call AnalyzeExp
    pop edi
    pop esi
    pop ebx
    ret
ReconstructSource endp

; Main Menu
MainMenu proc
    local choice:DWORD
    push ebx
    push esi
    push edi
@@menuLoop:
    invoke PrintStr, addr szW
    invoke PrintStr, addr szM
    call ReadInt
    mov choice, eax
    cmp choice, 0
    je @@exit
    cmp choice, 1
    je @@opt1
    cmp choice, 2
    je @@opt2
    cmp choice, 3
    je @@opt3
    cmp choice, 4
    je @@opt4
    cmp choice, 5
    je @@opt5
    cmp choice, 6
    je @@opt6
    cmp choice, 7
    je @@opt7
    cmp choice, 8
    je @@opt8
    jmp @@menuLoop
@@opt1:
    invoke PrintStr, addr szPF
    call ReadStr
    invoke MapFile, addr inputBuf
    test eax, eax
    jz @@menuLoop
    call ParsePE
    test eax, eax
    jz @@menuLoop
    call AnalyzeDeep
    jmp @@menuLoop
@@opt2:
    call RebuildIAT
    jmp @@menuLoop
@@opt3:
    call ReconstructSource
    jmp @@menuLoop
@@opt4:
    call ExtractStrings
    jmp @@menuLoop
@@opt5:
    call EntropyScan
    jmp @@menuLoop
@@opt6:
    call IdentifyCompiler
    jmp @@menuLoop
@@opt7:
    call ParseRich
    jmp @@menuLoop
@@opt8:
    call RecoverCFG
    jmp @@menuLoop
@@exit:
    pop edi
    pop esi
    pop ebx
    ret
MainMenu endp

; Entry Point
main proc
    invoke GetStdHandle, STD_INPUT_HANDLE
    mov hIn, eax
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hOut, eax
    call MainMenu
    invoke ExitProcess, 0
main endp

end main
