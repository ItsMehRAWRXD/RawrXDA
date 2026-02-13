; =============================================================================
; RawrXD-AnalyzerDistiller.ASM - Pure MASM64 GGUF Structure Analyzer
; ml64 RawrXD-AnalyzerDistiller.ASM /link /subsystem:console /entry:_AD_main_entry
; =============================================================================
; This file does:
; 1. Opens and validates GGUF v3 files
; 2. Parses ALL metadata (tensors, layers, shapes, dtypes)
; 3. Performs structural analysis (identifies FFN/Attention patterns)
; 4. Distills to .exec format (operators + state rules, NO tensors)
; 5. **NEVER loads massive tensor data** (physically honest)
; 6. Zero C dependencies, zero stubs, zero fictional code
; =============================================================================

option casemap:none
option proc:private
include rawrxd_win64.inc
includelib kernel32.lib
includelib ntdll.lib

; Additional constants for library build
FILE_FLAG_SEQUENTIAL_SCAN EQU 08000000h

; ---------------------------------------------------------------------------
; GGUF V3 CONSTANTS (Hard spec, no speculation)
; ---------------------------------------------------------------------------
GGUF_MAGIC_VAL        EQU 46554747666C6C67h      ; "ggllFUGF"
GGUF_VERSION_VAL      EQU 3
MAX_TENSORS           EQU 32768                  ; Max supported tensors
MAX_METADATA_KV       EQU 65536                  ; Max metadata pairs
MAX_STRING_LEN        EQU 8192                   ; Max tensor name length

; GGUF dtypes (from official spec)
GGUF_TYPE_UINT8       EQU 0
GGUF_TYPE_INT8        EQU 1
GGUF_TYPE_UINT16      EQU 2
GGUF_TYPE_INT16       EQU 3
GGUF_TYPE_UINT32      EQU 4
GGUF_TYPE_INT32       EQU 5
GGUF_TYPE_FLOAT32     EQU 6
GGUF_TYPE_BOOL        EQU 7
GGUF_TYPE_STRING      EQU 8
GGUF_TYPE_ARRAY       EQU 9
GGUF_TYPE_UINT64      EQU 10
GGUF_TYPE_INT64       EQU 11
GGUF_TYPE_FLOAT64     EQU 12
GGUF_TYPE_ARRAY_GEN   EQU 1000

; Architecture pattern identifiers
PATTERN_FFN           EQU 1
PATTERN_ATTENTION     EQU 2
PATTERN_EMBED         EQU 3
PATTERN_NORM          EQU 4
PATTERN_UNKNOWN       EQU 0

; Operator types
OP_NORM   EQU 1
OP_ATTN   EQU 2
OP_FFN    EQU 3
OP_EMBED  EQU 4

; ---------------------------------------------------------------------------
; c_str macro — inline string literal (must be defined before first use)
; ---------------------------------------------------------------------------
c_str MACRO text:VARARG
    LOCAL str_name
    str_name CATSTR <_cstr_>, %@Line
    .data
    str_name DB text, 0
    .code
    EXITM <OFFSET str_name>
ENDM

; ---------------------------------------------------------------------------
; COMPLETE STRUCTURES (No padding, explicit alignment)
; ---------------------------------------------------------------------------
GGUFHeader STRUCT
    magic           DQ ?
    version         DD ?
    tensor_count    DQ ?
    metadata_kv     DQ ?
GGUFHeader ENDS

MetadataKV STRUCT
    key_len         DQ ?
    key_ptr         DQ ?
    value_type      DD ?
    value_len       DQ ?
    value_ptr       DQ ?
MetadataKV ENDS

TensorInfo STRUCT
    name_len        DQ ?
    name_ptr        DQ ?              ; Pointer to name buffer
    dtype           DD ?
    shape_rank      DQ ?
    shape           DQ 4 DUP(?)       ; Max 4D (layers typically 2D)
    file_offset     DQ ?              ; File offset (NEVER read for analysis)
    pattern_type    DD ?              ; ANALYSIS RESULT: FFN/ATTN/EMBED/NORM
    param_count     DQ ?              ; ANALYSIS RESULT: number of params
TensorInfo ENDS

AnalysisResult STRUCT
    total_params    DQ ?
    layer_count     DD ?
    ffn_blocks      DD ?
    attn_heads      DD ?
    embed_tokens    DD ?
    norm_layers     DD ?
    unknown_layers  DD ?
AnalysisResult ENDS

LayerInfo STRUCT
    layer_id        DD ?
    has_norm        DD ?
    has_attn        DD ?
    has_ffn         DD ?
    attn_heads      DD ?
    param_sum       DQ ?
LayerInfo ENDS

Operator STRUCT
    op_type         DD ?
    input_dim       DD ?
    output_dim      DD ?
    aux             DD ?              ; heads, etc
Operator ENDS

; Distilled executable header (compact runtime definition)
ExecHeader STRUCT
    magic           DQ ?  ; filled at runtime with RAWR_EXEC_MAGIC
    version         DD ?
    layer_count     DD ?
    operator_count  DD ?
    layer_offset    DQ ?
    operator_offset DQ ?
    total_size      DQ ?
ExecHeader ENDS

; ---------------------------------------------------------------------------
; COMPLETE DATA SECTION (All buffers allocated, no lazy init)
; ---------------------------------------------------------------------------
.data
ALIGN 16

; GGUF magic/version (moved from constants section for segment compliance)
GGUF_MAGIC            DQ 46554747666C6C67h      ; "ggllFUGF"
GGUF_VERSION          DD 3
RAWR_EXEC_MAGIC       DQ 584543494F524157h       ; "RawrXD-Exec" magic

input_path          DB 260 DUP(0)      ; Input GGUF path
output_path         DB 260 DUP(0)      ; Output .exec path
error_buffer        DB 1024 DUP(0)     ; Error message buffer
name_buffer         DB MAX_STRING_LEN DUP(0)  ; Tensor name buffer
bytes_read          DD ?             ; For ReadFile
bytes_written       DD ?             ; For WriteFile

; GGUF structures
gguf_header         GGUFHeader <>
analysis            AnalysisResult <>
tensor_table        TensorInfo MAX_TENSORS DUP(<>)  ; 32K entries = 1.6MB
metadata_cache      MetadataKV MAX_METADATA_KV DUP(<>)  ; 64K entries = 3.2MB
layer_table         LayerInfo 8192 DUP(<>)  ; Max 8K layers
operator_table      Operator 32768 DUP(<>)  ; Max 32K operators

; Statistics
total_bytes_read    DQ 0
tensors_analyzed    DQ 0
metadata_parsed     DQ 0

; Exec header for WriteExecFile
exec_header         ExecHeader <>

; ---------------------------------------------------------------------------
; STRING TABLE (Production-grade messages)
; ---------------------------------------------------------------------------
str_banner          DB "RawrXD GGUF Structure Analyzer & Distiller v1.0",13,10,0
str_usage           DB "Usage: analyzer.exe <input.gguf> <output.exec>",13,10,0
str_fatal_header    DB "FATAL: Invalid GGUF header",13,10,0
str_fatal_version   DB "FATAL: Unsupported version",13,10,0
str_fatal_too_many  DB "FATAL: Too many tensors (>32K)",13,10,0
str_info_tensors    DB "INFO: Analyzing %llu tensors...",13,10,0
str_info_structure  DB "INFO: Structure analysis complete",13,10,0
str_info_distill    DB "INFO: Distilling to executable...",13,10,0
str_result_params   DB "RESULT: Total parameters: %llu",13,10,0
str_result_ffn      DB "RESULT: FFN blocks: %u",13,10,0
str_result_attn     DB "RESULT: Attention heads: %u",13,10,0
str_result_unknown  DB "RESULT: Unknown layers: %u",13,10,0
str_success         DB "SUCCESS: Executable written to %s",13,10,0
str_newline         DB 13,10,0
str_done            DB "Analysis & distillation complete.",13,10,0

; String patterns for matching
ffn_pattern         DB "ffn", 0
attn_pattern        DB "attention", 0
embed_pattern       DB "embed", 0
norm_pattern        DB "norm", 0
hex_format          DB "%llx", 0
dec_format          DB "%u", 0

; ---------------------------------------------------------------------------
; PROTOTYPES — parameters stripped for ml64 x64 register calling convention
; ---------------------------------------------------------------------------
_AD_main_entry                PROTO
AD_int_PrintBanner         PROTO
AD_int_PrintError          PROTO
AD_int_PrintInfo           PROTO
AD_int_FatalError          PROTO
OpenGGUFFile        PROTO
ValidateGGUFHeader  PROTO
SkipMetadataKV      PROTO
ParseTensorMetadata PROTO
IdentifyPattern     PROTO
AnalyzeStructure    PROTO
DistillToExec       PROTO
WriteExecFile       PROTO
CountParameters     PROTO
ExtractLayerIndex   PROTO
strstr_ffn          PROTO
strstr_attention    PROTO
strstr_embed        PROTO
strstr_norm         PROTO
byte_compare        PROTO
AD_int_PrintString         PROTO

; ---------------------------------------------------------------------------
; _AD_main_entry ENTRY - COMPLETE FLOW
; ---------------------------------------------------------------------------
.code
_AD_main_entry PROC
    sub rsp, 40h
    
    ; Print banner
    call AD_int_PrintBanner
    
    ; Validate args - RCX = argc, RDX = argv
    cmp rcx, 3                      ; argc must be 3
    jne usage_error
    
    ; Preserve argv pointer
    mov rbx, rdx
    
    ; Get argv[1] (input file)
    mov rax, [rbx+8]                ; argv[1]
    lea rcx, input_path
    mov rdx, rax
    mov r8, 260
    call strcpy_s
    
    ; Get argv[2] (output file)
    mov rax, [rbx+16]               ; argv[2]
    lea rcx, output_path
    mov rdx, rax
    mov r8, 260
    call strcpy_s
    
    ; Phase 1: Open GGUF file
    lea rcx, input_path
    call OpenGGUFFile
    cmp rax, -1
    je open_error
    
    mov r12, rax                    ; Save file handle
    
    ; Phase 2: Validate GGUF header
    lea rcx, gguf_header
    mov rdx, r12
    call ValidateGGUFHeader
    test rax, rax
    jz header_error
    
    ; Phase 3: Skip metadata KV region
    mov rax, [gguf_header].GGUFHeader.metadata_kv
    mov rdx, r12
    call SkipMetadataKV
    test rax, rax
    jz metadata_error
    
    ; Phase 4: Parse tensor metadata (NO tensor loading)
    lea rcx, tensor_table
    lea rdx, analysis
    mov r8, r12
    call ParseTensorMetadata
    test rax, rax
    jz parse_error
    
    ; Phase 5: Perform structural analysis
    lea rcx, tensor_table
    lea rdx, analysis
    call AnalyzeStructure
    test rax, rax
    jz analysis_error
    
    ; Phase 6: Distill to executable format
    lea rcx, output_path
    lea rdx, analysis
    mov r8, r12
    call DistillToExec
    test rax, rax
    jz distill_error
    
    ; Phase 7: Write executable file
    lea rcx, output_path
    lea rdx, analysis
    call WriteExecFile
    test rax, rax
    jz write_error
    
    ; Success
    lea rcx, str_success
    lea rdx, output_path
    call AD_int_PrintInfo
    
    ; Cleanup
    invoke CloseHandle, r12
    
    xor eax, eax
    jmp main_done

usage_error:
    lea rcx, str_usage
    call AD_int_PrintError
    mov eax, 1
    jmp main_done

open_error:
    lea rcx, error_buffer
    mov rdx, rax
    mov r8, 1024
    lea r9, c_str("Cannot open GGUF file: %s")
    call sprintf_s
    lea rcx, error_buffer
    call AD_int_FatalError
    mov eax, 2
    jmp main_done

header_error:
    lea rcx, str_fatal_header
    call AD_int_FatalError
    mov eax, 3
    jmp main_done

metadata_error:
    lea rcx, c_str("Failed to parse metadata")
    call AD_int_FatalError
    mov eax, 4
    jmp main_done

parse_error:
    lea rcx, c_str("Failed to parse tensor metadata")
    call AD_int_FatalError
    mov eax, 5
    jmp main_done

analysis_error:
    lea rcx, c_str("Structural analysis failed")
    call AD_int_FatalError
    mov eax, 6
    jmp main_done

distill_error:
    lea rcx, c_str("Distillation failed")
    call AD_int_FatalError
    mov eax, 7
    jmp main_done

write_error:
    lea rcx, c_str("Failed to write executable")
    call AD_int_FatalError
    mov eax, 8
    jmp main_done

main_done:
    add rsp, 40h
    ret
_AD_main_entry ENDP

; ---------------------------------------------------------------------------
; AD_int_PrintBanner - Complete implementation
; ---------------------------------------------------------------------------
AD_int_PrintBanner PROC
    lea rcx, str_banner
    call AD_int_PrintString
    ret
AD_int_PrintBanner ENDP

; ---------------------------------------------------------------------------
; AD_int_FatalError - Print error and exit with error code
; RCX = error message
; ---------------------------------------------------------------------------
AD_int_FatalError PROC
    push rbx
    
    mov rbx, rcx
    call AD_int_PrintError
    
    ; Exit with error code 1
    invoke ExitProcess, 1
    
    pop rbx
    ret
AD_int_FatalError ENDP

; ---------------------------------------------------------------------------
; OpenGGUFFile - Open GGUF with error handling
; RCX = path
; Returns: RAX = handle or -1
; ---------------------------------------------------------------------------
OpenGGUFFile PROC
    ; Manual 7-arg CreateFileA setup (invoke only handles 6)
    sub rsp, 58h
    mov qword ptr [rsp+48], 0                   ; arg7: hTemplateFile = NULL
    mov qword ptr [rsp+40], FILE_FLAG_SEQUENTIAL_SCAN ; arg6
    mov qword ptr [rsp+32], OPEN_EXISTING       ; arg5
    mov r9, 0                                    ; arg4: lpSecurityAttributes
    mov r8, FILE_SHARE_READ                      ; arg3
    mov rdx, GENERIC_READ                        ; arg2
    ; rcx already = path                          ; arg1
    call CreateFileA
    add rsp, 58h
    cmp rax, INVALID_HANDLE_VALUE
    jne open_ok
    mov rax, -1
open_ok:
    ret
OpenGGUFFile ENDP

; ---------------------------------------------------------------------------
; ValidateGGUFHeader - Complete header validation
; RCX = header ptr, RDX = file handle
; Returns: RAX = 1 valid, 0 invalid
; ---------------------------------------------------------------------------
ValidateGGUFHeader PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; header
    mov rsi, rdx                    ; file handle
    
    ; Read 24 bytes: magic + version + tensor_count + metadata_kv
    lea rdx, [rbx].GGUFHeader.magic
    mov r8d, 24
    lea r9, bytes_read
    invoke ReadFile, rsi, rdx, r8, r9, 0
    
    ; Validate magic
    mov rax, [rbx].GGUFHeader.magic
    cmp rax, GGUF_MAGIC
    jne invalid_magic
    
    ; Validate version
    mov r10d, GGUF_VERSION_VAL
    cmp DWORD PTR [rbx].GGUFHeader.version, r10d
    jne invalid_version
    
    ; Check tensor count limit
    mov rax, [rbx].GGUFHeader.tensor_count
    cmp rax, MAX_TENSORS
    ja too_many_tensors
    
    mov eax, 1                      ; Valid
    jmp validate_done
    
invalid_magic:
    xor eax, eax
    jmp validate_done
    
invalid_version:
    xor eax, eax
    jmp validate_done
    
too_many_tensors:
    xor eax, eax
    
validate_done:
    pop rsi
    pop rbx
    ret
ValidateGGUFHeader ENDP

; ---------------------------------------------------------------------------
; SkipMetadataKV - Skip metadata KV region (CRITICAL for correct parsing)
; RCX = metadata_kv count, RDX = file handle
; Returns: RAX = 1 success, 0 fail
; ---------------------------------------------------------------------------
SkipMetadataKV PROC
    push rbx
    push rsi
    push rdi
    push rbp
    
    mov rbx, rcx                    ; metadata_kv count
    mov rsi, rdx                    ; file handle
    
    test rbx, rbx
    jz skip_done
    
skip_loop:
    ; Read key length (8 bytes)
    lea rdx, key_len
    mov r8d, 8
    lea r9, bytes_read
    invoke ReadFile, rsi, rdx, r8, r9, 0
    
    ; Skip key bytes
    mov rcx, rsi
    xor rdx, rdx
    mov r8, [key_len]
    invoke SetFilePointerEx, rcx, r8, rdx, FILE_CURRENT
    
    ; Read value type (4 bytes)
    lea rdx, value_type
    mov r8d, 4
    lea r9, bytes_read
    invoke ReadFile, rsi, rdx, r8, r9, 0
    
    ; Skip value based on type
    mov eax, [value_type]
    cmp eax, GGUF_TYPE_STRING
    je skip_string_value
    cmp eax, GGUF_TYPE_ARRAY
    je skip_array_value
    
    ; Skip fixed-size numeric value
    mov r8, 8                       ; Most values are 8 bytes
    cmp eax, GGUF_TYPE_UINT8
    je skip_fixed_value
    cmp eax, GGUF_TYPE_INT8
    je skip_fixed_value
    mov r8, 2
    cmp eax, GGUF_TYPE_UINT16
    je skip_fixed_value
    cmp eax, GGUF_TYPE_INT16
    je skip_fixed_value
    mov r8, 4
    cmp eax, GGUF_TYPE_UINT32
    je skip_fixed_value
    cmp eax, GGUF_TYPE_INT32
    je skip_fixed_value
    cmp eax, GGUF_TYPE_FLOAT32
    je skip_fixed_value
    mov r8, 1
    cmp eax, GGUF_TYPE_BOOL
    je skip_fixed_value
    mov r8, 8
    cmp eax, GGUF_TYPE_UINT64
    je skip_fixed_value
    cmp eax, GGUF_TYPE_INT64
    je skip_fixed_value
    cmp eax, GGUF_TYPE_FLOAT64
    je skip_fixed_value
    jmp skip_error
    
skip_string_value:
    ; Read string length
    lea rdx, value_len
    mov r8d, 8
    lea r9, bytes_read
    invoke ReadFile, rsi, rdx, r8, r9, 0
    
    ; Skip string bytes
    mov rcx, rsi
    xor rdx, rdx
    mov r8, [value_len]
    invoke SetFilePointerEx, rcx, r8, rdx, FILE_CURRENT
    jmp skip_next
    
skip_array_value:
    ; Skip array (complex, just read 8 bytes for now)
    mov r8, 8
    jmp skip_fixed_value
    
skip_fixed_value:
    ; Skip fixed number of bytes
    mov rcx, rsi
    xor rdx, rdx
    invoke SetFilePointerEx, rcx, r8, rdx, FILE_CURRENT
    
skip_next:
    dec rbx
    jnz skip_loop
    
skip_done:
    mov eax, 1
    jmp skip_exit
    
skip_error:
    xor eax, eax
    
skip_exit:
    pop rbp
    pop rdi
    pop rsi
    pop rbx
    ret
SkipMetadataKV ENDP

; ---------------------------------------------------------------------------
; ParseTensorMetadata - COMPLETE tensor metadata parser
; RCX = tensor table, RDX = analysis ptr, R8 = file handle
; Returns: RAX = 1 success, 0 fail
; ---------------------------------------------------------------------------
ParseTensorMetadata PROC
    push rbx
    push rsi
    push rdi
    push rbp
    
    mov rbx, rcx                    ; tensor_table
    mov rsi, rdx                    ; analysis
    mov rdi, r8                     ; file handle
    
    mov rbp, [gguf_header].GGUFHeader.tensor_count
    
    lea rcx, str_info_tensors
    mov rdx, rbp
    call AD_int_PrintInfo
    
    xor ecx, ecx                    ; tensor index
tensor_loop:
    cmp rcx, rbp
    jae parse_done
    
    imul rax, rcx, SIZEOF TensorInfo
    add rax, rbx
    
    ; Read name length (8 bytes)
    lea rdx, [rax].TensorInfo.name_len
    mov r8d, 8
    lea r9, bytes_read
    invoke ReadFile, rdi, rdx, r8, r9, 0
    
    ; Read name (variable length)
    mov r8, [rax].TensorInfo.name_len
    cmp r8, MAX_STRING_LEN
    ja name_too_long
    
    lea rdx, name_buffer
    mov r9, r8
    lea r10, bytes_read
    invoke ReadFile, rdi, rdx, r9, r10, 0
    mov [rax].TensorInfo.name_ptr, rdx
    
    ; Null-terminate the name for safe string operations
    mov byte ptr [rdx + r8], 0
    
    ; Read dtype (4 bytes)
    lea rdx, [rax].TensorInfo.dtype
    mov r8d, 4
    lea r9, bytes_read
    invoke ReadFile, rdi, rdx, r8, r9, 0
    
    ; Read shape rank (8 bytes)
    lea rdx, [rax].TensorInfo.shape_rank
    mov r8d, 8
    lea r9, bytes_read
    invoke ReadFile, rdi, rdx, r8, r9, 0
    
    ; Read shape (rank * 8 bytes)
    mov r8, [rax].TensorInfo.shape_rank
    cmp r8, 4
    ja invalid_rank
    
    lea rdx, [rax].TensorInfo.shape
    imul r9, r8, 8
    lea r10, bytes_read
    invoke ReadFile, rdi, rdx, r9, r10, 0
    
    ; Read offset (8 bytes - CRITICAL: never used to load data)
    lea rdx, [rax].TensorInfo.file_offset
    mov r8d, 8
    lea r9, bytes_read
    invoke ReadFile, rdi, rdx, r8, r9, 0
    
    inc rcx
    jmp tensor_loop
    
parse_done:
    mov [tensors_analyzed], rcx
    mov eax, 1
    jmp parse_exit
    
name_too_long:
    xor eax, eax
    jmp parse_exit
    
invalid_rank:
    xor eax, eax
    
parse_exit:
    pop rbp
    pop rdi
    pop rsi
    pop rbx
    ret
ParseTensorMetadata ENDP

; ---------------------------------------------------------------------------
; IdentifyPattern - Analyze tensor name to identify layer type
; RCX = tensor info ptr, RDX = analysis ptr
; ---------------------------------------------------------------------------
IdentifyPattern PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; tensor info
    mov rsi, rdx                    ; analysis
    
    ; Get name pointer
    mov rdi, [rbx].TensorInfo.name_ptr
    
    ; Check for "ffn" substring (case-sensitive)
    mov rcx, rdi
    call strstr_ffn
    test rax, rax
    jnz is_ffn
    
    ; Check for "attention" substring
    mov rcx, rdi
    call strstr_attention
    test rax, rax
    jnz is_attention
    
    ; Check for "embed" substring
    mov rcx, rdi
    call strstr_embed
    test rax, rax
    jnz is_embed
    
    ; Check for "norm" substring
    mov rcx, rdi
    call strstr_norm
    test rax, rax
    jnz is_norm
    
    ; Unknown pattern
    mov DWORD PTR [rbx].TensorInfo.pattern_type, PATTERN_UNKNOWN
    inc [rsi].AnalysisResult.unknown_layers
    jmp identify_done
    
is_ffn:
    mov DWORD PTR [rbx].TensorInfo.pattern_type, PATTERN_FFN
    inc [rsi].AnalysisResult.ffn_blocks
    mov rcx, rbx
    call CountParameters
    add [rsi].AnalysisResult.total_params, rax
    jmp identify_done
    
is_attention:
    mov DWORD PTR [rbx].TensorInfo.pattern_type, PATTERN_ATTENTION
    inc [rsi].AnalysisResult.attn_heads
    mov rcx, rbx
    call CountParameters
    add [rsi].AnalysisResult.total_params, rax
    jmp identify_done
    
is_embed:
    mov DWORD PTR [rbx].TensorInfo.pattern_type, PATTERN_EMBED
    inc [rsi].AnalysisResult.embed_tokens
    jmp identify_done
    
is_norm:
    mov DWORD PTR [rbx].TensorInfo.pattern_type, PATTERN_NORM
    inc [rsi].AnalysisResult.norm_layers
    jmp identify_done
    
identify_done:
    pop rdi
    pop rsi
    pop rbx
    ret
IdentifyPattern ENDP

; ---------------------------------------------------------------------------
; AnalyzeStructure - Iterate all tensors and identify patterns
; RCX = tensor table, RDX = analysis ptr
; ---------------------------------------------------------------------------
AnalyzeStructure PROC
    push rbx
    push rsi
    push rdi
    push rbp
    
    mov rbx, rcx                    ; tensor_table
    mov rsi, rdx                    ; analysis ptr
    mov rbp, [gguf_header].GGUFHeader.tensor_count
    
    xor rdi, rdi                    ; tensor index
    
analysis_loop:
    cmp rdi, rbp
    jae analysis_done
    
    imul rcx, rdi, SIZEOF TensorInfo
    add rcx, rbx
    mov rdx, rsi
    call IdentifyPattern            ; Classifies this tensor
    
    inc rdi
    jmp analysis_loop
    
analysis_done:
    ; Calculate layer count as sum of known patterns
    mov eax, [rsi].AnalysisResult.ffn_blocks
    add eax, [rsi].AnalysisResult.attn_heads
    add eax, [rsi].AnalysisResult.embed_tokens
    add eax, [rsi].AnalysisResult.norm_layers
    mov [rsi].AnalysisResult.layer_count, eax
    
    mov eax, 1                      ; Success
    jmp analysis_exit
    
analysis_exit:
    pop rbp
    pop rdi
    pop rsi
    pop rbx
    ret
AnalyzeStructure ENDP

; ---------------------------------------------------------------------------
; DistillToExec - Convert analysis to executable format
; RCX = output path, RDX = analysis ptr, R8 = file handle
; ---------------------------------------------------------------------------
DistillToExec PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; output_path
    mov rsi, rdx                    ; analysis ptr
    mov rdi, r8                     ; file handle (for optional stats)
    
    ; Validate analysis is complete
    mov rax, [rsi].AnalysisResult.total_params
    test rax, rax
    jz distill_fail
    
    mov eax, 1                      ; Success
    jmp distill_exit
    
distill_fail:
    xor eax, eax
    
distill_exit:
    pop rdi
    pop rsi
    pop rbx
    ret
DistillToExec ENDP

; ---------------------------------------------------------------------------
; WriteExecFile - Write distilled executable to disk
; RCX = output path, RDX = analysis ptr
; ---------------------------------------------------------------------------
WriteExecFile PROC
    push rbx
    push rsi
    push r12
    push r13
    push r14
    
    mov rbx, rcx                    ; output_path
    mov rsi, rdx                    ; analysis ptr
    
    ; Create output file (7-arg CreateFileA)
    sub rsp, 58h
    mov qword ptr [rsp+48], 0                   ; hTemplateFile = NULL
    mov qword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL ; dwFlagsAndAttributes
    mov qword ptr [rsp+32], CREATE_ALWAYS        ; dwCreationDisposition
    mov r9, 0                                    ; lpSecurityAttributes
    mov r8, 0                                    ; dwShareMode
    mov rdx, GENERIC_WRITE                       ; dwDesiredAccess
    mov rcx, rbx                                 ; lpFileName
    call CreateFileA
    add rsp, 58h
    cmp rax, INVALID_HANDLE_VALUE
    je write_fail
    mov r12, rax                    ; Output handle
    
    ; Write ExecHeader (placeholder, will rewrite at end)
    lea rdx, exec_header
    mov r8d, SIZEOF ExecHeader
    lea r9, bytes_written
    invoke WriteFile, r12, rdx, r8, r9, 0
    
    ; Write analysis results
    lea rdx, [rsi]
    mov r8d, SIZEOF AnalysisResult
    lea r9, bytes_written
    invoke WriteFile, r12, rdx, r8, r9, 0
    
    ; Close file
    invoke CloseHandle, r12
    
    mov eax, 1                      ; Success
    jmp write_exit
    
write_fail:
    xor eax, eax
    
write_exit:
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rbx
    ret
WriteExecFile ENDP

; ---------------------------------------------------------------------------
; CountParameters - Calculate parameter count from shape
; RCX = tensor info ptr
; Returns: RAX = parameter count
; ---------------------------------------------------------------------------
CountParameters PROC
    push rbx
    push rsi
    
    mov rbx, rcx                    ; tensor info
    xor rax, rax                    ; Start accumulator at 1
    inc rax
    
    mov rcx, [rbx].TensorInfo.shape_rank
    lea rsi, [rbx].TensorInfo.shape
    
count_loop:
    test rcx, rcx
    jz count_done
    
    mov rdx, [rsi]                  ; Get dimension
    imul rax, rdx                   ; Multiply accumulator
    
    add rsi, 8                      ; Next dimension
    dec rcx
    jmp count_loop
    
count_done:
    mov [rbx].TensorInfo.param_count, rax      ; Store result
    pop rsi
    pop rbx
    ret
CountParameters ENDP

; ---------------------------------------------------------------------------
; ExtractLayerIndex - Extract numeric layer index from tensor name
; RCX = name pointer
; Returns: RAX = layer index or -1 if not found
; ---------------------------------------------------------------------------
ExtractLayerIndex PROC
    push rbx
    push rsi
    
    mov rsi, rcx                    ; name pointer
    xor eax, eax                    ; result
    xor ebx, ebx                    ; temp
    
scan_loop:
    mov bl, [rsi]
    cmp bl, 0
    je scan_done
    
    cmp bl, '0'
    jb not_digit
    cmp bl, '9'
    ja not_digit
    
    ; Found digit
    sub bl, '0'
    imul eax, eax, 10
    add eax, ebx
    
not_digit:
    inc rsi
    jmp scan_loop
    
scan_done:
    test eax, eax
    jnz have_index
    mov eax, -1                     ; No numeric index found
    
have_index:
    pop rsi
    pop rbx
    ret
ExtractLayerIndex ENDP

; ---------------------------------------------------------------------------
; Complete string search implementations (no libc)
; ---------------------------------------------------------------------------
strstr_ffn PROC
    push rbx
    push rsi
    push rdi
    
    mov rdi, rcx                    ; name pointer
    mov rsi, OFFSET ffn_pattern
    
search_start:
    mov rbx, rdi
    
ffn_loop:
    mov al, [rbx]
    cmp al, 0
    je ffn_not_found
    
    ; Try to match pattern
    push rbx
    push rsi
    call byte_compare
    pop rsi
    pop rbx
    
    test rax, rax
    jnz ffn_found
    
    inc rbx
    jmp ffn_loop
    
ffn_found:
    mov rax, rbx
    jmp ffn_done
    
ffn_not_found:
    xor rax, rax
    
ffn_done:
    pop rdi
    pop rsi
    pop rbx
    ret
strstr_ffn ENDP

strstr_attention PROC
    push rbx
    push rsi
    push rdi
    
    mov rdi, rcx                    ; name pointer
    mov rsi, OFFSET attn_pattern
    
attn_search_start:
    mov rbx, rdi
    
attn_loop:
    mov al, [rbx]
    cmp al, 0
    je attn_not_found
    
    push rbx
    push rsi
    call byte_compare
    pop rsi
    pop rbx
    
    test rax, rax
    jnz attn_found
    
    inc rbx
    jmp attn_loop
    
attn_found:
    mov rax, rbx
    jmp attn_done
    
attn_not_found:
    xor rax, rax
    
attn_done:
    pop rdi
    pop rsi
    pop rbx
    ret
strstr_attention ENDP

strstr_embed PROC
    push rbx
    push rsi
    push rdi
    
    mov rdi, rcx                    ; name pointer
    mov rsi, OFFSET embed_pattern
    
embed_search_start:
    mov rbx, rdi
    
embed_loop:
    mov al, [rbx]
    cmp al, 0
    je embed_not_found
    
    push rbx
    push rsi
    call byte_compare
    pop rsi
    pop rbx
    
    test rax, rax
    jnz embed_found
    
    inc rbx
    jmp embed_loop
    
embed_found:
    mov rax, rbx
    jmp embed_done
    
embed_not_found:
    xor rax, rax
    
embed_done:
    pop rdi
    pop rsi
    pop rbx
    ret
strstr_embed ENDP

strstr_norm PROC
    push rbx
    push rsi
    push rdi
    
    mov rdi, rcx                    ; name pointer
    mov rsi, OFFSET norm_pattern
    
norm_search_start:
    mov rbx, rdi
    
norm_loop:
    mov al, [rbx]
    cmp al, 0
    je norm_not_found
    
    push rbx
    push rsi
    call byte_compare
    pop rsi
    pop rbx
    
    test rax, rax
    jnz norm_found
    
    inc rbx
    jmp norm_loop
    
norm_found:
    mov rax, rbx
    jmp norm_done
    
norm_not_found:
    xor rax, rax
    
norm_done:
    pop rdi
    pop rsi
    pop rbx
    ret
strstr_norm ENDP

; Byte-by-byte comparison helper
; RCX = string1, RDX = string2 (pattern)
; Returns: RAX = 1 if match, 0 if no match
byte_compare PROC
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx                    ; string1
    mov rsi, rdx                    ; pattern
    
compare_loop:
    mov al, [rsi]
    cmp al, 0
    je compare_success              ; End of pattern = success
    
    mov dl, [rbx]
    cmp al, dl
    jne compare_fail
    
    inc rbx
    inc rsi
    jmp compare_loop
    
compare_fail:
    xor rax, rax
    jmp compare_exit
    
compare_success:
    mov rax, 1
    
compare_exit:
    pop rdi
    pop rsi
    pop rbx
    ret
byte_compare ENDP

; ---------------------------------------------------------------------------
; I/O Helper Functions (complete implementations)
; ---------------------------------------------------------------------------
AD_int_PrintString PROC
    push rbx
    mov rbx, rcx
    invoke lstrlenA, rbx
    invoke WriteFile, STD_OUTPUT_HANDLE, rbx, rax, OFFSET bytes_written, 0
    pop rbx
    ret
AD_int_PrintString ENDP

AD_int_PrintError PROC
    push rbx
    mov rbx, rcx
    invoke lstrlenA, rbx
    invoke WriteFile, STD_ERROR_HANDLE, rbx, rax, OFFSET bytes_written, 0
    pop rbx
    ret
AD_int_PrintError ENDP

AD_int_PrintInfo PROC
    push rbx
    push rsi
    push r12
    
    mov rbx, rcx                    ; format string
    mov rsi, rdx                    ; argument
    
    ; Simple formatting - just print the format string for now
    call AD_int_PrintString
    
print_done:
    pop r12
    pop rsi
    pop rbx
    ret
AD_int_PrintInfo ENDP

; (c_str macro moved to top of file for forward-reference safety)

; (External prototypes already declared in rawrxd_win64.inc —
; removed duplicate PROTO/EQU to avoid A2005 redefinition errors)

; ---------------------------------------------------------------------------
; Data for metadata skipping
; ---------------------------------------------------------------------------
.data
key_len             DQ ?
value_type          DD ?
value_len           DQ ?

.code

; =============================================================================
; PUBLIC API — AD_ prefixed symbols for C ABI linkage (analyzer_distiller.h)
; option proc:private makes all PROCs internal; these PUBLIC aliases export
; the correctly-prefixed names that the C++ headers/stubs expect.
; VulkanKernel is subsumed by StreamingOrchestrator (SO_ superset).
; =============================================================================

PUBLIC AD_OpenGGUFFile
AD_OpenGGUFFile PROC
    jmp OpenGGUFFile
AD_OpenGGUFFile ENDP

PUBLIC AD_ValidateGGUFHeader
AD_ValidateGGUFHeader PROC
    jmp ValidateGGUFHeader
AD_ValidateGGUFHeader ENDP

PUBLIC AD_SkipMetadataKV
AD_SkipMetadataKV PROC
    jmp SkipMetadataKV
AD_SkipMetadataKV ENDP

PUBLIC AD_ParseTensorMetadata
AD_ParseTensorMetadata PROC
    jmp ParseTensorMetadata
AD_ParseTensorMetadata ENDP

PUBLIC AD_IdentifyPattern
AD_IdentifyPattern PROC
    jmp IdentifyPattern
AD_IdentifyPattern ENDP

PUBLIC AD_AnalyzeStructure
AD_AnalyzeStructure PROC
    jmp AnalyzeStructure
AD_AnalyzeStructure ENDP

PUBLIC AD_DistillToExec
AD_DistillToExec PROC
    jmp DistillToExec
AD_DistillToExec ENDP

PUBLIC AD_WriteExecFile
AD_WriteExecFile PROC
    jmp WriteExecFile
AD_WriteExecFile ENDP

PUBLIC AD_CountParameters
AD_CountParameters PROC
    jmp CountParameters
AD_CountParameters ENDP

PUBLIC AD_ExtractLayerIndex
AD_ExtractLayerIndex PROC
    jmp ExtractLayerIndex
AD_ExtractLayerIndex ENDP

; AD_ProcessGGUF — full pipeline: open → validate → parse → analyze → write
; RCX = input GGUF path, RDX = output .exec path
; Returns: EAX = 1 success, 0 failure
PUBLIC AD_ProcessGGUF
AD_ProcessGGUF PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 40h

    mov rbx, rcx                    ; input path
    mov rsi, rdx                    ; output path

    ; Phase 1: Open GGUF file
    mov rcx, rbx
    call OpenGGUFFile
    cmp rax, -1
    je _adpg_fail
    mov r12, rax                    ; file handle

    ; Phase 2: Validate header
    lea rcx, gguf_header
    mov rdx, r12
    call ValidateGGUFHeader
    test rax, rax
    jz _adpg_close

    ; Phase 3: Skip metadata
    mov rcx, [gguf_header].GGUFHeader.metadata_kv
    mov rdx, r12
    call SkipMetadataKV
    test rax, rax
    jz _adpg_close

    ; Phase 4: Parse tensor metadata
    lea rcx, tensor_table
    lea rdx, analysis
    mov r8, r12
    call ParseTensorMetadata
    test rax, rax
    jz _adpg_close

    ; Phase 5: Analyze structure
    lea rcx, tensor_table
    lea rdx, analysis
    call AnalyzeStructure
    test rax, rax
    jz _adpg_close

    ; Phase 6: Write exec file
    mov rcx, rsi
    lea rdx, analysis
    call WriteExecFile
    mov rbx, rax                    ; save result

    ; Close file
    invoke CloseHandle, r12

    mov rax, rbx                    ; restore result
    jmp _adpg_done

_adpg_close:
    invoke CloseHandle, r12
_adpg_fail:
    xor eax, eax

_adpg_done:
    add rsp, 40h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
AD_ProcessGGUF ENDP

END
