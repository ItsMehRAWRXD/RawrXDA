;==============================================================================
; RawrXD Private Compiler - Phase 5.3: Code Generator
; Simple textual code generator and binary emission stubs (MASM)
; Produces a serialized output buffer (assembly listing / placeholder object)
;==============================================================================

.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

;==============================================================================
; CODEGEN CONSTANTS
;==============================================================================
OUTPUT_BUFFER_SIZE      equ 65536
MAX_LABELS              equ 8192
MAX_PATCHES             equ 8192

; Patch types
PATCH_TYPE_REL32        equ 1
PATCH_TYPE_REL16        equ 2
PATCH_TYPE_ABSOLUTE     equ 3

;==============================================================================
; DATA STRUCTURES
;==============================================================================
LABEL_ENTRY struct
    name            db 128 dup(?)
    position        dd ?
    defined         dd ?
    section         dd ?
LABEL_ENTRY ends

PATCH_ENTRY struct
    offset          dd ?
    labelIndex      dd ?
    patchType       dd ?
    addend          dd ?
PATCH_ENTRY ends

EMIT_STREAM struct
    buffer          dd ?
    pos             dd ?
    capacity        dd ?
EMIT_STREAM ends

;==============================================================================
; GLOBAL DATA
;==============================================================================
.data?
outputBuffer db OUTPUT_BUFFER_SIZE dup(?)
outputPos     dd 0
outputFile    db MAX_PATH dup(?)
labelTable    LABEL_ENTRY MAX_LABELS dup(<?>)
labelCount    dd 0
patchTable    PATCH_ENTRY MAX_PATCHES dup(<?>)
patchCount    dd 0

.code

;==============================================================================
; CodeGen_Init - initialize code generator with optional output filename
;   eax = TRUE on success
;==============================================================================
CodeGen_Init proc outputFilename:DWORD
    ; Clear buffers
    invoke ZeroMemory, addr outputBuffer, OUTPUT_BUFFER_SIZE
    mov dword ptr outputPos, 0
    mov dword ptr labelCount, 0
    mov dword ptr patchCount, 0

    ; Store filename if provided
    .if outputFilename != NULL
        invoke lstrcpyn, addr outputFile, outputFilename, MAX_PATH
    .else
        mov byte ptr outputFile[0], 0
    .endif

    return TRUE
CodeGen_Init endp

;==============================================================================
; CodeGen_Generate - entry point to generate code from AST root
;   astRoot: pointer to AST node (implementation assumes parser provides AST)
;   returns TRUE on success
;==============================================================================
CodeGen_Generate proc astRoot:DWORD
    ; NOTE: This implementation is a structured stub — it traverses the AST
    ; and emits a textual assembly listing into `outputBuffer`. Real binary
    ; encoding and relocations would be implemented here.

    ; If no AST provided, nothing to do
    .if astRoot == NULL
        invoke OutputDebugString, addr CSTR("CodeGen: No AST provided")
        return FALSE
    .endif

    ; Walk AST nodes and emit content
    invoke WalkAST_ForCodeGen, astRoot

    ; Resolve patches (backpatch labels)
    invoke CodeGen_ResolvePatches

    ; Optionally write to disk (if filename specified)
    invoke CodeGen_WriteOutputFile

    return TRUE
CodeGen_Generate endp

;==============================================================================
; WalkAST_ForCodeGen - simple recursive traversal that emits nodes
;==============================================================================
WalkAST_ForCodeGen proc node:DWORD
    local childIndex:DWORD
    local i:DWORD

    .if node == NULL
        return TRUE
    .endif

    ; Emit based on node type
    ; We assume AST_NODE layout is compatible with parser's AST_NODE
    ; node->nodeType is the first dword
    mov eax, [node]
    cmp eax, AST_INSTRUCTION
    je .emit_instr
    cmp eax, AST_DIRECTIVE
    je .emit_directive
    cmp eax, AST_LABEL
    je .emit_label

    ; Default: traverse children
    mov ecx, [node+OFFSET AST_NODE.childCount]
    test ecx, ecx
    jz .done

    mov i, 0
.loop_children:
    mov edx, [node+OFFSET AST_NODE.children + i*4]
    invoke WalkAST_ForCodeGen, edx
    inc i
    cmp i, ecx
    jl .loop_children
    jmp .done

.emit_instr:
    invoke CodeGen_EmitInstruction, node
    jmp .after_emit

.emit_directive:
    invoke CodeGen_EmitDirective, node
    jmp .after_emit

.emit_label:
    invoke CodeGen_RecordLabel, node
    jmp .after_emit

.after_emit:
    ; traverse children after emitting
    mov ecx, [node+OFFSET AST_NODE.childCount]
    mov i, 0
.loop_children2:
    cmp i, ecx
    jge .done
    mov edx, [node+OFFSET AST_NODE.children + i*4]
    invoke WalkAST_ForCodeGen, edx
    inc i
    jmp .loop_children2

.done:
    return TRUE
WalkAST_ForCodeGen endp

;==============================================================================
; CodeGen_EmitInstruction - produce textual representation for instruction node
;==============================================================================
CodeGen_EmitInstruction proc instrNode:DWORD
    local mnemonicPtr:DWORD
    local bufPos:DWORD

    ; Get mnemonic from node attribute 0 (parser SetNodeAttribute stores pointer)
    mov eax, [instrNode+OFFSET AST_NODE.attributes]
    mov mnemonicPtr, eax

    ; Append mnemonic and a space
    invoke CodeGen_AppendFormat, CSTR("\t%s "), mnemonicPtr

    ; Append operands (children)
    mov ecx, [instrNode+OFFSET AST_NODE.childCount]
    cmp ecx, 0
    je .emit_newline

    mov edx, 0
.emit_op_loop:
    mov esi, [instrNode+OFFSET AST_NODE.children + edx*4]
    ; For simplicity, use a textual operand serializer
    invoke CodeGen_EmitOperand, esi
    inc edx
    cmp edx, ecx
    jl .emit_comma
    jmp .emit_newline

.emit_comma:
    invoke CodeGen_AppendString, CSTR(", ")
    jmp .emit_op_loop

.emit_newline:
    invoke CodeGen_AppendString, CSTR("\r\n")
    return TRUE
CodeGen_EmitInstruction endp

;==============================================================================
; CodeGen_EmitOperand - emit a textual representation for an operand node
;==============================================================================
CodeGen_EmitOperand proc operandNode:DWORD
    ; Operand types: register, memory, immediate, identifier, string
    mov eax, [operandNode]
    cmp eax, AST_REGISTER
    je .emit_register
    cmp eax, AST_MEMORY
    je .emit_memory
    cmp eax, AST_IMMEDIATE
    je .emit_immediate
    cmp eax, AST_IDENTIFIER
    je .emit_identifier
    cmp eax, AST_STRING
    je .emit_string

    ; Unknown operand: just append placeholder
    invoke CodeGen_AppendString, CSTR("<op>")
    ret

.emit_register:
    ; attribute[0] holds register name pointer
    mov eax, [operandNode+OFFSET AST_NODE.attributes]
    invoke CodeGen_AppendString, eax
    ret

.emit_memory:
    ; Render memory operand as [base + index*scale + disp]
    ; attribute[0]=base, [1]=index, [2]=scale, [3]=disp
    mov eax, [operandNode+OFFSET AST_NODE.attributes]
    .if eax != 0
        invoke CodeGen_AppendString, eax
    .endif
    ret

.emit_immediate:
    ; attribute[0] contains immediate value pointer
    mov eax, [operandNode+OFFSET AST_NODE.attributes]
    invoke CodeGen_AppendString, eax
    ret

.emit_identifier:
    mov eax, [operandNode+OFFSET AST_NODE.attributes]
    invoke CodeGen_AppendString, eax
    ret

.emit_string:
    mov eax, [operandNode+OFFSET AST_NODE.attributes]
    invoke CodeGen_AppendString, eax
    ret

CodeGen_EmitOperand endp

;==============================================================================
; CodeGen_EmitDirective - emit textual directive
;==============================================================================
CodeGen_EmitDirective proc dirNode:DWORD
    ; attribute[0] holds directive text
    mov eax, [dirNode+OFFSET AST_NODE.attributes]
    invoke CodeGen_AppendFormat, CSTR(".%s\r\n"), eax
    return TRUE
CodeGen_EmitDirective endp

;==============================================================================
; CodeGen_RecordLabel - record label position for later backpatch
;==============================================================================
CodeGen_RecordLabel proc labelNode:DWORD
    local namePtr:DWORD
    local idx:DWORD
    local pos:DWORD

    mov eax, labelNode
    ; attribute[0] assumed to be pointer to label name
    mov eax, [labelNode+OFFSET AST_NODE.attributes]
    mov namePtr, eax

    mov edx, dword ptr outputPos

    mov eax, labelCount
    mov idx, eax
    inc dword ptr labelCount

    ; store label
    mov esi, offset labelTable
    add esi, idx*SIZEOF LABEL_ENTRY
    invoke lstrcpyn, esi, namePtr, 128
    mov [esi+128], edx            ; position (after name bytes)
    mov dword ptr [esi+128+4], 1  ; defined

    return TRUE
CodeGen_RecordLabel endp

;==============================================================================
; CodeGen_AppendString - append a null-terminated string to outputBuffer
;==============================================================================
CodeGen_AppendString proc strPtr:DWORD
    local pSrc:DWORD
    local dst:DWORD

    mov esi, strPtr
    mov edi, offset outputBuffer
    mov eax, dword ptr outputPos
    add edi, eax

.append_loop:
    mov bl, [esi]
    cmp bl, 0
    je .done
    mov [edi], bl
    inc esi
    inc edi
    inc dword ptr outputPos
    jmp .append_loop

.done:
    ret
CodeGen_AppendString endp

;==============================================================================
; CodeGen_AppendFormat - wrapper that uses wsprintf to format into a small
; temporary buffer and appends it (avoids formatting directly into outputBuffer)
;==============================================================================
CodeGen_AppendFormat proc formatStr:DWORD, param1:DWORD
    local tmp[512]:BYTE

    invoke wsprintf, addr tmp, formatStr, param1
    invoke CodeGen_AppendString, addr tmp
    ret
CodeGen_AppendFormat endp

;==============================================================================
; CodeGen_AddPatch - add a relocation/patch entry
;==============================================================================
CodeGen_AddPatch proc offset:DWORD, labelIndex:DWORD, patchType:DWORD, addend:DWORD
    local idx:DWORD

    mov eax, patchCount
    mov idx, eax
    inc dword ptr patchCount

    mov esi, offset patchTable
    add esi, idx*SIZEOF PATCH_ENTRY
    mov [esi], offset
    mov [esi+4], labelIndex
    mov [esi+8], patchType
    mov [esi+12], addend

    return TRUE
CodeGen_AddPatch endp

;==============================================================================
; CodeGen_ResolvePatches - attempt to resolve any recorded patches
;==============================================================================
CodeGen_ResolvePatches proc
    local i:DWORD
    local pcnt:DWORD

    mov pcnt, patchCount
    mov i, 0
    .while i < pcnt
        ; In a full implementation we would read patchTable[i], find label, and
        ; write relocation bytes. For now just log.
        invoke OutputDebugString, addr CSTR("CodeGen: Resolving patch (stub)")
        inc i
    .endw
    return TRUE
CodeGen_ResolvePatches endp

;==============================================================================
; CodeGen_WriteOutputFile - write `outputBuffer` to disk if filename provided
;==============================================================================
CodeGen_WriteOutputFile proc
    local fnamePtr:DWORD

    lea eax, outputFile
    mov fnamePtr, eax
    mov al, [fnamePtr]
    cmp al, 0
    je .no_file

    ; Prepare debug message showing length
    local msg[256]:BYTE
    invoke wsprintf, addr msg, CSTR("CodeGen: Writing output file (%s), %d bytes"), fnamePtr, dword ptr outputPos
    invoke OutputDebugString, addr msg

    ; NOTE: Writing to disk is left as a placeholder to avoid introducing
    ; platform-specific file flags in this stub. Implement CreateFileA/WriteFile
    ; here when ready.

    jmp .done

.no_file:
    invoke OutputDebugString, addr CSTR("CodeGen: No output filename specified; skipping write")

.done:
    return TRUE
CodeGen_WriteOutputFile endp

;==============================================================================
; Utility stubs to satisfy link dependencies (if any)
;==============================================================================
; The real implementation would provide binary encoding, sections, symbol
; emission, relocations, and object file emission. These stubs maintain the
; architecture and placeholders for future expansion.

end
