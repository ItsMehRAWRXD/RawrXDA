; voice_processor_masm.asm
; Pure x64 MASM core for decoupled VoiceProcessor behavior.

OPTION CASEMAP:NONE

PUBLIC rawrxd_vp_start_recording
PUBLIC rawrxd_vp_stop_recording
PUBLIC rawrxd_vp_is_recording
PUBLIC rawrxd_vp_transcribe
PUBLIC rawrxd_vp_detect_intent
PUBLIC rawrxd_vp_generate_speech

.const
kTranscriptionText DB "voice command (masm core)",0
kIntentBuild       DB "build",0
kIntentChat        DB "chat",0
kSpeechPrefix      DB "pcm:",0
kConfBuild         DQ 03FECCCCCCCCCCCCDh ; 0.9
kConfChat          DQ 03FE3333333333333h ; 0.6

.data
g_recording DD 0

.code

; int rawrxd_vp_start_recording(void)
rawrxd_vp_start_recording PROC FRAME
    .ENDPROLOG
    mov DWORD PTR [g_recording], 1
    xor eax, eax
    ret
rawrxd_vp_start_recording ENDP

; int rawrxd_vp_stop_recording(void)
rawrxd_vp_stop_recording PROC FRAME
    .ENDPROLOG
    mov DWORD PTR [g_recording], 0
    xor eax, eax
    ret
rawrxd_vp_stop_recording ENDP

; int rawrxd_vp_is_recording(void)
rawrxd_vp_is_recording PROC FRAME
    .ENDPROLOG
    mov eax, DWORD PTR [g_recording]
    ret
rawrxd_vp_is_recording ENDP

; long long rawrxd_vp_transcribe(const unsigned char* data,
;                                unsigned long long size,
;                                char* out,
;                                unsigned long long cap)
; Returns bytes written, -1 on error.
rawrxd_vp_transcribe PROC FRAME
    .ENDPROLOG

    ; out == NULL or cap == 0 => error
    test r8, r8
    jz vp_transcribe_err
    test r9, r9
    jz vp_transcribe_err

    ; data == NULL or size == 0 => emit empty string
    test rcx, rcx
    jz vp_transcribe_empty
    test rdx, rdx
    jz vp_transcribe_empty

    lea r10, kTranscriptionText
    mov r11, r8          ; out ptr
    xor eax, eax         ; written count

vp_transcribe_copy:
    cmp r9, 1
    jbe vp_transcribe_done
    mov dl, BYTE PTR [r10]
    test dl, dl
    jz vp_transcribe_done
    mov BYTE PTR [r11], dl
    inc r10
    inc r11
    inc eax
    dec r9
    jmp vp_transcribe_copy

vp_transcribe_done:
    mov BYTE PTR [r11], 0
    cdqe
    ret

vp_transcribe_empty:
    mov BYTE PTR [r8], 0
    xor eax, eax
    cdqe
    ret

vp_transcribe_err:
    mov rax, -1
    ret
rawrxd_vp_transcribe ENDP

; long long rawrxd_vp_detect_intent(const char* text,
;                                   char* intent_out,
;                                   unsigned long long cap,
;                                   double* confidence_out)
; Returns bytes written, -1 on error.
rawrxd_vp_detect_intent PROC FRAME
    .ENDPROLOG

    test rcx, rcx
    jz vp_detect_err
    test rdx, rdx
    jz vp_detect_err
    test r8, r8
    jz vp_detect_err

    mov al, BYTE PTR [rcx]
    cmp al, 'b'
    je vp_detect_build
    cmp al, 'B'
    je vp_detect_build

    ; chat fallback
    test r9, r9
    jz vp_detect_chat_noconf
    movsd xmm0, QWORD PTR [kConfChat]
    movsd QWORD PTR [r9], xmm0
vp_detect_chat_noconf:
    lea r10, kIntentChat
    jmp vp_detect_copy

vp_detect_build:
    test r9, r9
    jz vp_detect_build_noconf
    movsd xmm0, QWORD PTR [kConfBuild]
    movsd QWORD PTR [r9], xmm0
vp_detect_build_noconf:
    lea r10, kIntentBuild

vp_detect_copy:
    mov r11, rdx
    mov r9, r8
    xor eax, eax

vp_detect_copy_loop:
    cmp r9, 1
    jbe vp_detect_done
    mov dl, BYTE PTR [r10]
    test dl, dl
    jz vp_detect_done
    mov BYTE PTR [r11], dl
    inc r10
    inc r11
    inc eax
    dec r9
    jmp vp_detect_copy_loop

vp_detect_done:
    mov BYTE PTR [r11], 0
    cdqe
    ret

vp_detect_err:
    mov rax, -1
    ret
rawrxd_vp_detect_intent ENDP

; long long rawrxd_vp_generate_speech(const char* text,
;                                     char* out,
;                                     unsigned long long cap)
; Returns bytes written, -1 on error.
rawrxd_vp_generate_speech PROC FRAME
    .ENDPROLOG

    test rcx, rcx
    jz vp_speech_err
    test rdx, rdx
    jz vp_speech_err
    test r8, r8
    jz vp_speech_err

    mov r10, rcx ; input text
    mov r11, rdx ; out ptr
    mov r9,  r8  ; cap
    xor eax, eax ; written

    ; write "pcm:" prefix
    lea rcx, kSpeechPrefix
vp_speech_prefix:
    cmp r9, 1
    jbe vp_speech_done
    mov dl, BYTE PTR [rcx]
    test dl, dl
    jz vp_speech_text
    mov BYTE PTR [r11], dl
    inc rcx
    inc r11
    inc eax
    dec r9
    jmp vp_speech_prefix

vp_speech_text:
    cmp r9, 1
    jbe vp_speech_done
    mov dl, BYTE PTR [r10]
    test dl, dl
    jz vp_speech_done
    mov BYTE PTR [r11], dl
    inc r10
    inc r11
    inc eax
    dec r9
    jmp vp_speech_text

vp_speech_done:
    mov BYTE PTR [r11], 0
    cdqe
    ret

vp_speech_err:
    mov rax, -1
    ret
rawrxd_vp_generate_speech ENDP

END
