; RawrXD Tokenizer - MASM x64 Assembly Implementation
; Supports BPE, SentencePiece, and Tiktoken algorithms
; Minimal dependencies, efficient data structures

.DATA
; Constants
BOS_ID EQU 1
EOS_ID EQU 2
UNK_ID EQU 0

; Byte encoder/decoder maps (256 entries each)
byte_encoder DB 256 DUP (?)  ; uint8_t -> token_id
byte_decoder DD 256 DUP (?)  ; token_id -> uint8_t

; Vocab hash table (simple array for demo)
MAX_VOCAB EQU 65536
vocab_strings DQ MAX_VOCAB DUP (?)  ; pointers to strings
vocab_ids DD MAX_VOCAB DUP (?)       ; token_ids
vocab_count DD 0

; Merge rules for BPE (array of pairs: left_id, right_id, merged_id)
MAX_MERGES EQU 50000
merge_left DD MAX_MERGES DUP (?)
merge_right DD MAX_MERGES DUP (?)
merge_result DD MAX_MERGES DUP (?)
merge_count DD 0

; Temporary buffers
MAX_TEMP EQU 1024*1024
temp_buffer DB MAX_TEMP DUP (?)  ; 1MB temp buffer
token_list DD MAX_TEMP DUP (?)   ; token IDs list (4MB for 1M tokens)

.CODE

; Initialize tokenizer
; RCX = tokenizer type (0=BPE, 1=SentencePiece, 2=Tiktoken)
InitTokenizer PROC
    PUSH RBX
    
    ; Initialize byte mappings (identity for simplicity)
    MOV RBX, 0
    MOV RCX, 256
init_byte_loop:
    MOV byte_encoder[RBX], BL
    MOV byte_decoder[RBX*4], EBX
    INC RBX
    LOOP init_byte_loop
    
    ; Load vocab and merges based on type
    ; Stub: assume preloaded
    
    POP RBX
    RET
InitTokenizer ENDP

; Load vocabulary
; RCX = path to vocab file
LoadVocab PROC
    ; Stub: In real impl, read file and populate vocab_strings and vocab_ids
    ; For demo, bytes 0-255 mapped to 3-258
    
    MOV vocab_count, 256
    MOV RCX, 256
    MOV RBX, 0
load_vocab_loop:
    MOV vocab_ids[RBX*4], EBX
    ADD vocab_ids[RBX*4], 3  ; offset
    ; vocab_strings[RBX*8] = pointer to single char string (stub)
    INC RBX
    LOOP load_vocab_loop
    
    RET
LoadVocab ENDP

; Load merge rules for BPE
; RCX = path to merges file
LoadMerges PROC
    ; Stub: Read merge pairs into merge_left, merge_right, merge_result arrays
    ; Assume merges are loaded
    
    RET
LoadMerges ENDP

; Tokenize BPE
; RCX = input UTF-8 string pointer
; RDX = output token list pointer
; R8 = max tokens
; Returns: RAX = token count
Tokenize_BPE PROC
    PUSH RBX
    PUSH RSI
    PUSH RDI
    PUSH R12
    
    MOV RSI, RCX  ; input string
    MOV RDI, RDX  ; output list
    MOV R12, R8   ; max tokens
    MOV RBX, 0    ; current token count
    
    ; Initialize with byte tokens
tokenize_init:
    MOV AL, [RSI]
    TEST AL, AL
    JZ tokenize_merge
    
    MOVZX RAX, AL
    ADD EAX, 3  ; byte token id
    MOV [RDI + RBX*4], EAX
    INC RBX
    INC RSI
    CMP RBX, R12
    JL tokenize_init
    
tokenize_merge:
    ; Apply merges (simplified: only a few merges for demo)
    MOV RCX, merge_count
    TEST RCX, RCX
    JZ tokenize_done
    
merge_loop:
    ; For each merge, scan and replace pairs
    PUSH RCX
    MOV RAX, RCX
    DEC RAX  ; 0-based index
    MOV R9D, merge_left[RAX*4]
    MOV R10D, merge_right[RAX*4]
    MOV R11D, merge_result[RAX*4]
    
    ; Scan token list for pair
    MOV RSI, 0  ; position
scan_pair:
    CMP RSI, RBX
    JGE next_merge
    
    MOV EAX, [RDI + RSI*4]
    CMP EAX, R9D
    JNE no_match
    
    CMP RSI+1, RBX
    JGE no_match
    
    MOV EAX, [RDI + (RSI+1)*4]
    CMP EAX, R10D
    JNE no_match
    
    ; Found pair, replace with merged
    MOV [RDI + RSI*4], R11D
    ; Shift rest left
    MOV RCX, RSI
    INC RCX
shift_loop:
    CMP RCX+1, RBX
    JGE shift_done
    MOV EAX, [RDI + (RCX+1)*4]
    MOV [RDI + RCX*4], EAX
    INC RCX
    JMP shift_loop
shift_done:
    DEC RBX  ; one less token
    
no_match:
    INC RSI
    JMP scan_pair
    
next_merge:
    POP RCX
    LOOP merge_loop
    
tokenize_done:
    MOV RAX, RBX
    POP R12
    POP RDI
    POP RSI
    POP RBX
    RET
Tokenize_BPE ENDP

; Detokenize
; RCX = token list pointer
; RDX = token count
; R8 = output string pointer
Detokenize PROC
    PUSH RBX
    PUSH RSI
    PUSH RDI
    PUSH R12
    
    MOV RSI, RCX  ; token list
    MOV RDI, R8   ; output string
    MOV RBX, RDX  ; count
    MOV R12, 0    ; output pos
    
detokenize_loop:
    TEST RBX, RBX
    JZ detokenize_done
    
    MOV EAX, [RSI]
    ADD RSI, 4
    
    ; Skip BOS/EOS
    CMP EAX, BOS_ID
    JE skip_token
    CMP EAX, EOS_ID
    JE skip_token
    
    ; Decode token
    CMP EAX, 258  ; > byte tokens
    JGE decode_merge  ; need to expand merge
    
    ; Byte token
    SUB EAX, 3
    MOV [RDI + R12], AL
    INC R12
    JMP next_token
    
decode_merge:
    ; Stub: recursively decode merges (complex, skip for demo)
    ; Assume it's a byte
    SUB EAX, 3
    MOV [RDI + R12], AL
    INC R12
    
next_token:
    DEC RBX
    JMP detokenize_loop
    
skip_token:
    DEC RBX
    JMP detokenize_loop
    
detokenize_done:
    MOV BYTE PTR [RDI + R12], 0  ; null terminate
    MOV RAX, R12  ; return length
    POP R12
    POP RDI
    POP RSI
    POP RBX
    RET
Detokenize ENDP

; SentencePiece tokenization (subword segmentation)
Tokenize_SentencePiece PROC
    ; Stub: Use unigram or BPE-like
    CALL Tokenize_BPE
    RET
Tokenize_SentencePiece ENDP

; Tiktoken tokenization (regex-based splitting + BPE)
Tokenize_Tiktoken PROC
    ; Stub: First regex split, then BPE
    CALL Tokenize_BPE
    RET
Tokenize_Tiktoken ENDP

END