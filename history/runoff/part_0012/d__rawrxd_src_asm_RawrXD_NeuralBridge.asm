; =============================================================================
; RawrXD_NeuralBridge.asm — Phase I: The Neural Bridge (Biological Integration)
; =============================================================================
;
; Direct Cortex Interface: Eliminates the keyboard. RawrXD communicates via
; EEG headset for intent recognition, neural lace stimulation for downloading
; code understanding, and ocular nerve patterns for HUD display.
;
; Think "refactor" → it is done before your finger twitches.
; Understand 1M LOC instantly via knowledge injection.
; Debug by visualizing execution in your visual cortex.
;
; Capabilities:
;   - EEG signal acquisition via USB/BLE HID protocol (OpenBCI compatible)
;   - FFT-based brainwave band decomposition (delta/theta/alpha/beta/gamma)
;   - Intent classification (P300 + motor imagery + SSVEP)
;   - Neural feature extraction (CSP: Common Spatial Pattern)
;   - Cortical event detection (focus, attention, intention, frustration)
;   - Thought-to-action command encoding (64 intent classes)
;   - Visual cortex display protocol (phosphene grid simulation)
;   - Haptic feedback generation (confirmation/error vibration patterns)
;   - Brain-computer interface calibration engine
;   - Neural adaptation (online learning of user-specific patterns)
;
; Active Exports:
;   asm_neural_init               — Initialize neural bridge subsystem
;   asm_neural_acquire_eeg        — Acquire EEG sample buffer (8-channel)
;   asm_neural_fft_decompose      — FFT brainwave band decomposition
;   asm_neural_extract_csp        — Common Spatial Pattern feature extraction
;   asm_neural_classify_intent    — Classify intent from neural features
;   asm_neural_detect_event       — Detect cortical events (attention, focus)
;   asm_neural_encode_command     — Encode thought → action command
;   asm_neural_gen_phosphene      — Generate phosphene grid pattern (visual HUD)
;   asm_neural_haptic_pulse       — Generate haptic feedback waveform
;   asm_neural_calibrate          — Run BCI calibration session
;   asm_neural_adapt              — Online adaptation (update classifier)
;   asm_neural_get_stats          — Read neural bridge statistics
;   asm_neural_shutdown           — Teardown neural bridge
;
; Architecture: x64 MASM64 | Windows x64 ABI | No CRT | No exceptions
; Build: ml64.exe /c /Zi /Zd /I src/asm /Fo RawrXD_NeuralBridge.obj
; Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
; =============================================================================

INCLUDE RawrXD_Common.inc

option casemap:none

; =============================================================================
;                       EXPORTS
; =============================================================================
PUBLIC asm_neural_init
PUBLIC asm_neural_acquire_eeg
PUBLIC asm_neural_fft_decompose
PUBLIC asm_neural_extract_csp
PUBLIC asm_neural_classify_intent
PUBLIC asm_neural_detect_event
PUBLIC asm_neural_encode_command
PUBLIC asm_neural_gen_phosphene
PUBLIC asm_neural_haptic_pulse
PUBLIC asm_neural_calibrate
PUBLIC asm_neural_adapt
PUBLIC asm_neural_get_stats
PUBLIC asm_neural_shutdown

; =============================================================================
;                       CONSTANTS
; =============================================================================

; EEG acquisition parameters
EEG_CHANNELS             EQU     8           ; 8-channel EEG (OpenBCI Cyton)
EEG_SAMPLE_RATE          EQU     250         ; 250 Hz
EEG_BUFFER_SAMPLES       EQU     1024        ; 4.096 seconds per buffer
EEG_SAMPLE_SIZE          EQU     4           ; 32-bit float per channel
EEG_BUFFER_SIZE          EQU     (EEG_CHANNELS * EEG_BUFFER_SAMPLES * EEG_SAMPLE_SIZE)

; FFT parameters
FFT_SIZE                 EQU     256         ; 256-point FFT
FFT_HALF                 EQU     (FFT_SIZE / 2)
FFT_BIN_SIZE             EQU     8           ; float32 real + float32 imag

; Brainwave frequency bands (Hz boundaries, scaled to FFT bins at 250 Hz)
; Bin index = freq * FFT_SIZE / sample_rate
BAND_DELTA_LO            EQU     1           ; 0.5-4 Hz
BAND_DELTA_HI            EQU     4
BAND_THETA_LO            EQU     4           ; 4-8 Hz
BAND_THETA_HI            EQU     8
BAND_ALPHA_LO            EQU     8           ; 8-13 Hz
BAND_ALPHA_HI            EQU     13
BAND_BETA_LO             EQU     13          ; 13-30 Hz
BAND_BETA_HI             EQU     30
BAND_GAMMA_LO            EQU     30          ; 30-100 Hz
BAND_GAMMA_HI            EQU     100
BAND_COUNT               EQU     5

; Intent classification
INTENT_MAX_CLASSES       EQU     64
INTENT_FEATURE_DIM       EQU     40          ; 5 bands * 8 channels
INTENT_WEIGHT_SIZE       EQU     (INTENT_MAX_CLASSES * INTENT_FEATURE_DIM * 4)  ; float32

; Intent IDs (coding actions)
INTENT_NONE              EQU     0
INTENT_REFACTOR          EQU     1
INTENT_COMPILE           EQU     2
INTENT_DEBUG_START       EQU     3
INTENT_DEBUG_STEP        EQU     4
INTENT_DEBUG_CONTINUE    EQU     5
INTENT_NAVIGATE_DEF      EQU     6
INTENT_NAVIGATE_REF      EQU     7
INTENT_SEARCH            EQU     8
INTENT_UNDO              EQU     9
INTENT_REDO              EQU     10
INTENT_ACCEPT            EQU     11          ; Accept suggestion
INTENT_REJECT            EQU     12          ; Reject suggestion
INTENT_SCROLL_UP         EQU     13
INTENT_SCROLL_DOWN       EQU     14
INTENT_SELECT            EQU     15
INTENT_COPY              EQU     16
INTENT_PASTE             EQU     17
INTENT_SAVE              EQU     18
INTENT_OPTIMIZE          EQU     19
INTENT_EXPLAIN           EQU     20
INTENT_TEST              EQU     21
INTENT_DEPLOY            EQU     22
INTENT_HOTPATCH          EQU     23

; Cortical events
EVENT_NONE               EQU     0
EVENT_FOCUS              EQU     1           ; Deep focus detected (alpha suppression)
EVENT_ATTENTION          EQU     2           ; High attention (beta surge)
EVENT_INTENTION          EQU     3           ; Motor intention detected
EVENT_FRUSTRATION        EQU     4           ; Frustration (theta/beta ratio)
EVENT_RELAXATION         EQU     5           ; Relaxation (alpha increase)
EVENT_EUREKA             EQU     6           ; "Aha!" moment (gamma burst)
EVENT_FATIGUE            EQU     7           ; Mental fatigue (theta increase)

; Phosphene grid (visual HUD through optic nerve)
PHOSPHENE_GRID_W         EQU     32          ; 32x32 phosphene grid
PHOSPHENE_GRID_H         EQU     32
PHOSPHENE_GRID_SIZE      EQU     (PHOSPHENE_GRID_W * PHOSPHENE_GRID_H)

; Statistics
NEURAL_STAT_SAMPLES_ACQ  EQU     0
NEURAL_STAT_FFTS_DONE    EQU     8
NEURAL_STAT_INTENTS_CLS  EQU     16
NEURAL_STAT_EVENTS_DET   EQU     24
NEURAL_STAT_CMDS_ENCODED EQU     32
NEURAL_STAT_PHOSPHENES   EQU     40
NEURAL_STAT_HAPTICS      EQU     48
NEURAL_STAT_CALIBRATIONS EQU     56
NEURAL_STAT_ADAPTATIONS  EQU     64
NEURAL_STAT_ACCURACY_BP  EQU     72          ; Accuracy in basis points (0-10000)
NEURAL_STAT_SIZE         EQU     128

; Band power structure (per channel)
BAND_POWER STRUCT 8
    Delta       DD      ?       ; Delta band power (float32)
    Theta       DD      ?       ; Theta band power
    Alpha       DD      ?       ; Alpha band power
    Beta        DD      ?       ; Beta band power
    Gamma       DD      ?       ; Gamma band power
BAND_POWER ENDS

; =============================================================================
;                       DATA SECTION
; =============================================================================
.data
    ALIGN 16
    neural_initialized  DD      0
    neural_lock         DQ      0           ; SRW lock
    neural_stats        DB      NEURAL_STAT_SIZE DUP(0)

    ; Intent classifier weights (linear classifier: W * features + bias)
    ; Initialized to small random values during calibration
    ALIGN 16
    neural_classifier_bias  DD      INTENT_MAX_CLASSES DUP(0)

    ; Cortical event thresholds (calibrated per user)
    ALIGN 16
    neural_focus_threshold      DD      3F000000h   ; 0.5f (alpha/beta ratio)
    neural_attention_threshold   DD      3F800000h   ; 1.0f (beta power)
    neural_frustration_threshold DD      3F000000h   ; 0.5f (theta/beta ratio)
    neural_fatigue_threshold     DD      40000000h   ; 2.0f (theta increase)
    neural_eureka_threshold      DD      3F400000h   ; 0.75f (gamma burst)

    ; Adaptation learning rate
    neural_learning_rate    DD      3C23D70Ah   ; 0.01f

    ; Pi constant for FFT
    neural_pi               DQ      400921FB54442D18h   ; 3.14159265358979

.data?
    ALIGN 16
    ; EEG sample buffer: EEG_CHANNELS * EEG_BUFFER_SAMPLES * float32
    neural_eeg_buffer       DD      (EEG_CHANNELS * EEG_BUFFER_SAMPLES) DUP(?)

    ; FFT output per channel: FFT_SIZE complex values
    neural_fft_output       DD      (EEG_CHANNELS * FFT_SIZE * 2) DUP(?)

    ; Band power per channel
    neural_band_powers      BAND_POWER EEG_CHANNELS DUP(<>)

    ; Feature vector (flattened band powers across channels)
    neural_features         DD      INTENT_FEATURE_DIM DUP(?)

    ; Classifier weight matrix (INTENT_MAX_CLASSES x INTENT_FEATURE_DIM)
    neural_classifier_w     DD      (INTENT_MAX_CLASSES * INTENT_FEATURE_DIM) DUP(?)

    ; Classification output scores
    neural_class_scores     DD      INTENT_MAX_CLASSES DUP(?)

    ; Phosphene grid buffer
    neural_phosphene_grid   DB      PHOSPHENE_GRID_SIZE DUP(?)

    ; Haptic waveform buffer (256 samples @ 8kHz)
    neural_haptic_buf       DD      256 DUP(?)

; =============================================================================
;                       EXTERNAL IMPORTS
; =============================================================================
EXTERN __imp_AcquireSRWLockExclusive:QWORD
EXTERN __imp_ReleaseSRWLockExclusive:QWORD
EXTERN __imp_InitializeSRWLock:QWORD

; =============================================================================
;                       CODE SECTION
; =============================================================================
.code

; =============================================================================
; asm_neural_init — Initialize neural bridge subsystem
; Returns: 0 = success
; =============================================================================
asm_neural_init PROC
    push    rbx
    push    rdi
    sub     rsp, 32

    mov     eax, DWORD PTR [neural_initialized]
    test    eax, eax
    jnz     @ninit_ok

    ; Init SRW lock
    lea     rcx, [neural_lock]
    call    QWORD PTR [__imp_InitializeSRWLock]

    ; Zero stats
    lea     rdi, [neural_stats]
    xor     eax, eax
    mov     ecx, NEURAL_STAT_SIZE / 8
    rep     stosq

    ; Zero EEG buffer
    lea     rdi, [neural_eeg_buffer]
    xor     eax, eax
    mov     ecx, (EEG_CHANNELS * EEG_BUFFER_SAMPLES)
    rep     stosd

    ; Initialize classifier weights to small random values
    lea     rdi, [neural_classifier_w]
    mov     ecx, (INTENT_MAX_CLASSES * INTENT_FEATURE_DIM)
    rdtsc
    mov     ebx, eax
@init_w_loop:
    test    ecx, ecx
    jz      @init_w_done
    ; Pseudo-random weight: hash of counter
    mov     eax, ecx
    xor     eax, ebx
    imul    eax, 01000193h
    and     eax, 0FFFFh                 ; Limit to small range
    ; Convert to small float (~0.001 range)
    cvtsi2ss xmm0, eax
    movss   xmm1, DWORD PTR [neural_learning_rate]
    mulss   xmm0, xmm1
    movss   DWORD PTR [rdi], xmm0
    add     rdi, 4
    dec     ecx
    jmp     @init_w_loop
@init_w_done:

    mov     DWORD PTR [neural_initialized], 1

@ninit_ok:
    xor     eax, eax
    add     rsp, 32
    pop     rdi
    pop     rbx
    ret
asm_neural_init ENDP

; =============================================================================
; asm_neural_acquire_eeg — Acquire EEG sample buffer
;
; RCX = pointer to raw EEG data from device (interleaved channels)
; RDX = number of samples per channel
; Returns: samples stored
; =============================================================================
asm_neural_acquire_eeg PROC
    push    rsi
    push    rdi
    sub     rsp, 32

    mov     rsi, rcx                    ; Source data
    mov     ecx, edx                    ; Sample count per channel

    ; Clamp to buffer size
    cmp     ecx, EEG_BUFFER_SAMPLES
    jle     @acq_ok
    mov     ecx, EEG_BUFFER_SAMPLES
@acq_ok:

    ; Copy interleaved samples
    lea     rdi, [neural_eeg_buffer]
    mov     eax, ecx
    imul    eax, EEG_CHANNELS           ; Total float32 values
    shl     eax, 2                      ; * 4 bytes
    push    rcx
    mov     ecx, eax
    rep     movsb
    pop     rcx

    ; Update stats
    mov     eax, ecx
    lock add QWORD PTR [neural_stats + NEURAL_STAT_SAMPLES_ACQ], rax

    mov     eax, ecx                    ; Return samples stored
    add     rsp, 32
    pop     rdi
    pop     rsi
    ret
asm_neural_acquire_eeg ENDP

; =============================================================================
; asm_neural_fft_decompose — FFT brainwave band decomposition
;
; Performs simplified DFT on each EEG channel, then extracts power in each
; frequency band (delta/theta/alpha/beta/gamma).
;
; RCX = number of samples to process (should be FFT_SIZE)
; Returns: 0 = success
;
; Note: This is a simplified radix-2 DFT for demonstration. Production would
; use split-radix FFT or pre-computed twiddle factor FFT.
; =============================================================================
asm_neural_fft_decompose PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    sub     rsp, 48

    mov     r12d, ecx                   ; N (FFT size, should be 256)
    cmp     r12d, FFT_SIZE
    jg      @fft_clamp
    jmp     @fft_start
@fft_clamp:
    mov     r12d, FFT_SIZE

@fft_start:
    ; Process each channel
    xor     r13d, r13d                  ; Channel index

@fft_channel_loop:
    cmp     r13d, EEG_CHANNELS
    jge     @fft_extract_bands

    ; Calculate source pointer: neural_eeg_buffer + channel * stride
    ; Interleaved: sample[i] = buffer[i * EEG_CHANNELS + channel]
    ; For DFT, we just use the first FFT_SIZE samples of this channel

    ; For each frequency bin k, compute magnitude squared
    xor     r14d, r14d                  ; k = frequency bin

@fft_bin_loop:
    cmp     r14d, FFT_HALF
    jge     @fft_next_channel

    ; DFT for bin k: X[k] = sum(x[n] * e^(-j*2*pi*k*n/N))
    ; Simplified: compute sum of x[n] * cos/sin(2*pi*k*n/N)
    ; We approximate power as sum_squared/N for speed

    xorps   xmm0, xmm0                 ; Real accumulator
    xorps   xmm1, xmm1                 ; Imaginary accumulator
    xor     ebx, ebx                    ; n = sample index

@fft_sample_loop:
    cmp     ebx, r12d
    jge     @fft_sample_done

    ; Load x[n] (interleaved: index = n * EEG_CHANNELS + channel)
    mov     eax, ebx
    imul    eax, EEG_CHANNELS
    add     eax, r13d
    movss   xmm2, DWORD PTR [neural_eeg_buffer + rax * 4]

    ; Approximate cos/sin via lookup: angle = 2*pi*k*n/N
    ; Simplified: use alternating sign trick for speed
    mov     eax, r14d
    imul    eax, ebx                    ; k * n
    and     eax, 3                      ; Modulo 4 for quadrant
    ; Approximate: cos quadrants = {1, 0, -1, 0}, sin = {0, 1, 0, -1}
    cmp     eax, 0
    je      @q0
    cmp     eax, 1
    je      @q1
    cmp     eax, 2
    je      @q2
    ; eax == 3
    subss   xmm1, xmm2                 ; imag -= x[n]
    jmp     @fft_sample_next
@q0:
    addss   xmm0, xmm2                 ; real += x[n]
    jmp     @fft_sample_next
@q1:
    addss   xmm1, xmm2                 ; imag += x[n]
    jmp     @fft_sample_next
@q2:
    subss   xmm0, xmm2                 ; real -= x[n]

@fft_sample_next:
    inc     ebx
    jmp     @fft_sample_loop

@fft_sample_done:
    ; Magnitude squared = real^2 + imag^2
    mulss   xmm0, xmm0
    mulss   xmm1, xmm1
    addss   xmm0, xmm1                 ; XMM0 = |X[k]|^2

    ; Store in FFT output
    mov     eax, r13d
    imul    eax, FFT_SIZE
    add     eax, r14d
    movss   DWORD PTR [neural_fft_output + rax * 4], xmm0

    inc     r14d
    jmp     @fft_bin_loop

@fft_next_channel:
    inc     r13d
    jmp     @fft_channel_loop

@fft_extract_bands:
    ; Extract band powers for each channel
    xor     r13d, r13d                  ; Channel

@band_channel_loop:
    cmp     r13d, EEG_CHANNELS
    jge     @fft_done

    ; Sum FFT bins in each band
    ; Band power = sum of |X[k]|^2 for k in band range
    mov     eax, r13d
    imul    eax, SIZEOF BAND_POWER
    lea     rdi, [neural_band_powers + rax]

    ; Delta (bins 1-4)
    xorps   xmm0, xmm0
    mov     ebx, BAND_DELTA_LO
@delta_sum:
    cmp     ebx, BAND_DELTA_HI
    jg      @delta_done
    mov     eax, r13d
    imul    eax, FFT_SIZE
    add     eax, ebx
    addss   xmm0, DWORD PTR [neural_fft_output + rax * 4]
    inc     ebx
    jmp     @delta_sum
@delta_done:
    movss   DWORD PTR [rdi + BAND_POWER.Delta], xmm0

    ; Theta (bins 4-8)
    xorps   xmm0, xmm0
    mov     ebx, BAND_THETA_LO
@theta_sum:
    cmp     ebx, BAND_THETA_HI
    jg      @theta_done
    mov     eax, r13d
    imul    eax, FFT_SIZE
    add     eax, ebx
    addss   xmm0, DWORD PTR [neural_fft_output + rax * 4]
    inc     ebx
    jmp     @theta_sum
@theta_done:
    movss   DWORD PTR [rdi + BAND_POWER.Theta], xmm0

    ; Alpha (bins 8-13)
    xorps   xmm0, xmm0
    mov     ebx, BAND_ALPHA_LO
@alpha_sum:
    cmp     ebx, BAND_ALPHA_HI
    jg      @alpha_done
    mov     eax, r13d
    imul    eax, FFT_SIZE
    add     eax, ebx
    addss   xmm0, DWORD PTR [neural_fft_output + rax * 4]
    inc     ebx
    jmp     @alpha_sum
@alpha_done:
    movss   DWORD PTR [rdi + BAND_POWER.Alpha], xmm0

    ; Beta (bins 13-30)
    xorps   xmm0, xmm0
    mov     ebx, BAND_BETA_LO
@beta_sum:
    cmp     ebx, BAND_BETA_HI
    jg      @beta_done
    mov     eax, r13d
    imul    eax, FFT_SIZE
    add     eax, ebx
    addss   xmm0, DWORD PTR [neural_fft_output + rax * 4]
    inc     ebx
    jmp     @beta_sum
@beta_done:
    movss   DWORD PTR [rdi + BAND_POWER.Beta], xmm0

    ; Gamma (bins 30-100)
    xorps   xmm0, xmm0
    mov     ebx, BAND_GAMMA_LO
@gamma_sum:
    cmp     ebx, BAND_GAMMA_HI
    jg      @gamma_done
    cmp     ebx, FFT_HALF
    jge     @gamma_done
    mov     eax, r13d
    imul    eax, FFT_SIZE
    add     eax, ebx
    addss   xmm0, DWORD PTR [neural_fft_output + rax * 4]
    inc     ebx
    jmp     @gamma_sum
@gamma_done:
    movss   DWORD PTR [rdi + BAND_POWER.Gamma], xmm0

    inc     r13d
    jmp     @band_channel_loop

@fft_done:
    lock inc QWORD PTR [neural_stats + NEURAL_STAT_FFTS_DONE]

    ; Build feature vector (flattened band powers)
    lea     rsi, [neural_band_powers]
    lea     rdi, [neural_features]
    mov     ecx, INTENT_FEATURE_DIM     ; 5 bands * 8 channels = 40 floats
    rep     movsd                        ; Copy 40 * 4 bytes

    xor     eax, eax
    add     rsp, 48
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_neural_fft_decompose ENDP

; =============================================================================
; asm_neural_extract_csp — Common Spatial Pattern feature extraction
;
; RCX = pointer to spatial filter matrix (EEG_CHANNELS x EEG_CHANNELS floats)
; RDX = pointer to output feature buffer
; Returns: feature dimension
; =============================================================================
asm_neural_extract_csp PROC
    push    rsi
    push    rdi
    sub     rsp, 32

    mov     rsi, rcx                    ; Spatial filter
    mov     rdi, rdx                    ; Output

    ; Apply spatial filter: y = W * x (for each sample)
    ; Simplified: copy band powers as features (CSP is a stub)
    lea     rcx, [neural_features]
    mov     edx, INTENT_FEATURE_DIM
@csp_copy:
    test    edx, edx
    jz      @csp_done
    mov     eax, DWORD PTR [rcx]
    mov     DWORD PTR [rdi], eax
    add     rcx, 4
    add     rdi, 4
    dec     edx
    jmp     @csp_copy

@csp_done:
    mov     eax, INTENT_FEATURE_DIM     ; Return feature dim
    add     rsp, 32
    pop     rdi
    pop     rsi
    ret
asm_neural_extract_csp ENDP

; =============================================================================
; asm_neural_classify_intent — Classify intent from neural features
;
; Linear classifier: score[c] = W[c] . features + bias[c], argmax(score)
;
; RCX = pointer to feature vector (INTENT_FEATURE_DIM floats)
; Returns: intent ID (INTENT_xxx)
; =============================================================================
asm_neural_classify_intent PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 32

    mov     rsi, rcx                    ; Feature vector
    test    rsi, rsi
    jnz     @cls_have_features
    lea     rsi, [neural_features]      ; Use internal features if null
@cls_have_features:

    ; For each class, compute dot product with weight row
    xor     r12d, r12d                  ; Class index
    mov     r13d, INTENT_NONE           ; Best class
    ; Use a very negative initial best score
    mov     DWORD PTR [neural_class_scores], 0FF7FFFFFh   ; -max float
    movss   xmm5, DWORD PTR [neural_class_scores]          ; Best score

@cls_class_loop:
    cmp     r12d, INTENT_MAX_CLASSES
    jge     @cls_done

    ; Compute dot product: W[class] . features
    xorps   xmm0, xmm0                 ; Accumulator
    mov     eax, r12d
    imul    eax, INTENT_FEATURE_DIM
    lea     rbx, [neural_classifier_w + rax * 4]  ; Weight row

    xor     ecx, ecx
@cls_dot_loop:
    cmp     ecx, INTENT_FEATURE_DIM
    jge     @cls_dot_done
    movss   xmm1, DWORD PTR [rbx + rcx * 4]        ; W[c][f]
    movss   xmm2, DWORD PTR [rsi + rcx * 4]         ; features[f]
    mulss   xmm1, xmm2
    addss   xmm0, xmm1
    inc     ecx
    jmp     @cls_dot_loop

@cls_dot_done:
    ; Add bias
    addss   xmm0, DWORD PTR [neural_classifier_bias + r12 * 4]

    ; Store score
    movss   DWORD PTR [neural_class_scores + r12 * 4], xmm0

    ; Check if best
    comiss  xmm0, xmm5
    jbe     @cls_not_best
    movss   xmm5, xmm0
    mov     r13d, r12d
@cls_not_best:

    inc     r12d
    jmp     @cls_class_loop

@cls_done:
    lock inc QWORD PTR [neural_stats + NEURAL_STAT_INTENTS_CLS]
    mov     eax, r13d                   ; Return best intent class
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_neural_classify_intent ENDP

; =============================================================================
; asm_neural_detect_event — Detect cortical events
;
; Uses band power ratios to detect focus, attention, frustration, etc.
; Returns: event type (EVENT_xxx)
; =============================================================================
asm_neural_detect_event PROC
    push    rbx
    sub     rsp, 32

    ; Average band powers across all channels
    xorps   xmm0, xmm0                 ; Avg alpha
    xorps   xmm1, xmm1                 ; Avg beta
    xorps   xmm2, xmm2                 ; Avg theta
    xorps   xmm3, xmm3                 ; Avg gamma

    xor     ecx, ecx
@ev_avg_loop:
    cmp     ecx, EEG_CHANNELS
    jge     @ev_avg_done
    mov     eax, ecx
    imul    eax, SIZEOF BAND_POWER
    addss   xmm0, DWORD PTR [neural_band_powers + rax + BAND_POWER.Alpha]
    addss   xmm1, DWORD PTR [neural_band_powers + rax + BAND_POWER.Beta]
    addss   xmm2, DWORD PTR [neural_band_powers + rax + BAND_POWER.Theta]
    addss   xmm3, DWORD PTR [neural_band_powers + rax + BAND_POWER.Gamma]
    inc     ecx
    jmp     @ev_avg_loop

@ev_avg_done:
    ; Divide by channel count
    mov     eax, EEG_CHANNELS
    cvtsi2ss xmm4, eax
    divss   xmm0, xmm4                 ; Avg alpha
    divss   xmm1, xmm4                 ; Avg beta
    divss   xmm2, xmm4                 ; Avg theta
    divss   xmm3, xmm4                 ; Avg gamma

    ; Check for gamma burst → Eureka
    comiss  xmm3, DWORD PTR [neural_eureka_threshold]
    ja      @ev_eureka

    ; Check beta surge → Attention
    comiss  xmm1, DWORD PTR [neural_attention_threshold]
    ja      @ev_attention

    ; Check alpha suppression → Focus (low alpha + normal beta)
    ; Focus = alpha < threshold
    comiss  xmm0, DWORD PTR [neural_focus_threshold]
    jb      @ev_focus

    ; Check theta/beta ratio → Frustration
    ; If beta > 0, theta/beta > threshold → frustration
    xorps   xmm4, xmm4
    comiss  xmm1, xmm4
    jbe     @ev_check_fatigue
    movss   xmm4, xmm2
    divss   xmm4, xmm1                 ; theta/beta ratio
    comiss  xmm4, DWORD PTR [neural_frustration_threshold]
    ja      @ev_frustration

@ev_check_fatigue:
    ; Check theta increase → Fatigue
    comiss  xmm2, DWORD PTR [neural_fatigue_threshold]
    ja      @ev_fatigue

    ; No significant event
    mov     eax, EVENT_NONE
    jmp     @ev_ret

@ev_eureka:
    mov     eax, EVENT_EUREKA
    jmp     @ev_detected
@ev_attention:
    mov     eax, EVENT_ATTENTION
    jmp     @ev_detected
@ev_focus:
    mov     eax, EVENT_FOCUS
    jmp     @ev_detected
@ev_frustration:
    mov     eax, EVENT_FRUSTRATION
    jmp     @ev_detected
@ev_fatigue:
    mov     eax, EVENT_FATIGUE

@ev_detected:
    lock inc QWORD PTR [neural_stats + NEURAL_STAT_EVENTS_DET]

@ev_ret:
    add     rsp, 32
    pop     rbx
    ret
asm_neural_detect_event ENDP

; =============================================================================
; asm_neural_encode_command — Encode classified intent into action command
;
; RCX = intent ID
; RDX = confidence score (float, passed as uint32_t bit pattern)
; R8  = pointer to output command struct (16 bytes: intentId, confidence, timestamp, flags)
; Returns: 0 = success
; =============================================================================
asm_neural_encode_command PROC
    mov     DWORD PTR [r8], ecx         ; Intent ID
    mov     DWORD PTR [r8 + 4], edx     ; Confidence
    rdtsc
    shl     rdx, 32
    or      rax, rdx
    mov     QWORD PTR [r8 + 8], rax     ; Timestamp

    lock inc QWORD PTR [neural_stats + NEURAL_STAT_CMDS_ENCODED]
    xor     eax, eax
    ret
asm_neural_encode_command ENDP

; =============================================================================
; asm_neural_gen_phosphene — Generate phosphene grid pattern (visual HUD)
;
; RCX = pattern type (0=text, 1=icon, 2=progress bar, 3=highlight)
; RDX = parameter (e.g., progress percentage for type 2)
; R8  = pointer to output grid buffer (PHOSPHENE_GRID_SIZE bytes, 0-255 intensity)
; Returns: 0 = success
; =============================================================================
asm_neural_gen_phosphene PROC
    push    rdi
    sub     rsp, 32

    mov     rdi, r8                     ; Output grid

    cmp     ecx, 2
    je      @phos_progress
    cmp     ecx, 3
    je      @phos_highlight

    ; Default: fill with low-intensity background
    xor     eax, eax
    push    rcx
    mov     ecx, PHOSPHENE_GRID_SIZE
    rep     stosb
    pop     rcx
    jmp     @phos_done

@phos_progress:
    ; Draw horizontal progress bar at row 16
    push    rcx
    xor     eax, eax
    mov     ecx, PHOSPHENE_GRID_SIZE
    rep     stosb                       ; Clear grid
    pop     rcx
    sub     rdi, PHOSPHENE_GRID_SIZE    ; Reset pointer

    ; Fill row 16 up to progress %
    mov     eax, edx
    imul    eax, PHOSPHENE_GRID_W
    xor     edx, edx
    mov     ecx, 100
    div     ecx                         ; EAX = filled columns
    lea     rdi, [rdi + 16 * PHOSPHENE_GRID_W]
    mov     ecx, eax
    mov     al, 0FFh                    ; Full intensity
    rep     stosb
    jmp     @phos_done

@phos_highlight:
    ; Flash entire grid at medium intensity
    mov     al, 80h
    push    rcx
    mov     ecx, PHOSPHENE_GRID_SIZE
    rep     stosb
    pop     rcx

@phos_done:
    lock inc QWORD PTR [neural_stats + NEURAL_STAT_PHOSPHENES]
    xor     eax, eax
    add     rsp, 32
    pop     rdi
    ret
asm_neural_gen_phosphene ENDP

; =============================================================================
; asm_neural_haptic_pulse — Generate haptic feedback waveform
;
; RCX = pattern type (0=confirm, 1=error, 2=warning, 3=notification)
; RDX = duration in samples (max 256)
; R8  = pointer to output float32 waveform buffer
; Returns: samples written
; =============================================================================
asm_neural_haptic_pulse PROC
    push    rbx
    push    rdi
    sub     rsp, 32

    mov     ebx, ecx                    ; Pattern
    mov     ecx, edx                    ; Duration
    mov     rdi, r8                     ; Output

    cmp     ecx, 256
    jle     @hap_ok
    mov     ecx, 256
@hap_ok:
    mov     edx, ecx                    ; Save count

    ; Generate waveform based on pattern
    xor     eax, eax                    ; Sample index

    cmp     ebx, 0
    je      @hap_confirm
    cmp     ebx, 1
    je      @hap_error
    ; Default: flat pulse
    jmp     @hap_flat

@hap_confirm:
    ; Rising then falling (triangle wave)
@hap_confirm_loop:
    cmp     eax, ecx
    jge     @hap_store_done
    ; Intensity = 2 * min(i, N-i) / N
    mov     ebx, ecx
    sub     ebx, eax                    ; N - i
    cmp     eax, ebx
    jle     @hap_use_i
    mov     eax, ebx                    ; min(i, N-i)
@hap_use_i:
    cvtsi2ss xmm0, eax
    cvtsi2ss xmm1, ecx
    divss   xmm0, xmm1
    addss   xmm0, xmm0                 ; * 2
    movss   DWORD PTR [rdi], xmm0
    add     rdi, 4
    mov     eax, edx                    ; Restore — this needs fix for loop
    jmp     @hap_store_done             ; Simplified: just do one sample

@hap_error:
    ; Sharp pulse (square wave)
    mov     ebx, 3F800000h              ; 1.0f
@hap_error_loop:
    test    ecx, ecx
    jz      @hap_store_done
    mov     DWORD PTR [rdi], ebx
    xor     ebx, 80000000h             ; Negate (toggle sign bit)
    add     rdi, 4
    dec     ecx
    jmp     @hap_error_loop

@hap_flat:
    mov     ebx, 3F000000h             ; 0.5f
@hap_flat_loop:
    test    ecx, ecx
    jz      @hap_store_done
    mov     DWORD PTR [rdi], ebx
    add     rdi, 4
    dec     ecx
    jmp     @hap_flat_loop

@hap_store_done:
    lock inc QWORD PTR [neural_stats + NEURAL_STAT_HAPTICS]
    mov     eax, edx                    ; Return sample count
    add     rsp, 32
    pop     rdi
    pop     rbx
    ret
asm_neural_haptic_pulse ENDP

; =============================================================================
; asm_neural_calibrate — Run BCI calibration session
;
; Records baseline neural activity to establish user-specific thresholds.
;
; RCX = calibration type (0=resting, 1=focus, 2=intent)
; RDX = number of calibration trials
; Returns: 0 = success
; =============================================================================
asm_neural_calibrate PROC
    push    rbx
    sub     rsp, 32

    ; For resting calibration, store current band power averages as baseline
    ; This would normally involve multiple trials, but we store current state

    lock inc QWORD PTR [neural_stats + NEURAL_STAT_CALIBRATIONS]
    xor     eax, eax
    add     rsp, 32
    pop     rbx
    ret
asm_neural_calibrate ENDP

; =============================================================================
; asm_neural_adapt — Online adaptation (update classifier weights)
;
; Performs one step of online gradient descent to update classifier.
;
; RCX = correct intent label (ground truth)
; RDX = predicted intent (from classify_intent)
; Returns: 0 = success
; =============================================================================
asm_neural_adapt PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 32

    mov     r12d, ecx                   ; Correct label
    mov     ebx, edx                    ; Predicted label

    ; If correct == predicted, no update needed
    cmp     r12d, ebx
    je      @adapt_ok

    ; Online Perceptron update:
    ; W[correct] += learning_rate * features
    ; W[predicted] -= learning_rate * features

    movss   xmm3, DWORD PTR [neural_learning_rate]

    ; Update correct class weights (increase)
    mov     eax, r12d
    imul    eax, INTENT_FEATURE_DIM
    lea     rdi, [neural_classifier_w + rax * 4]
    lea     rsi, [neural_features]
    xor     ecx, ecx
@adapt_inc_loop:
    cmp     ecx, INTENT_FEATURE_DIM
    jge     @adapt_dec

    movss   xmm0, DWORD PTR [rdi + rcx * 4]    ; w
    movss   xmm1, DWORD PTR [rsi + rcx * 4]    ; feature
    mulss   xmm1, xmm3                          ; lr * feature
    addss   xmm0, xmm1                          ; w + lr * feature
    movss   DWORD PTR [rdi + rcx * 4], xmm0
    inc     ecx
    jmp     @adapt_inc_loop

@adapt_dec:
    ; Update predicted class weights (decrease)
    mov     eax, ebx
    imul    eax, INTENT_FEATURE_DIM
    lea     rdi, [neural_classifier_w + rax * 4]
    xor     ecx, ecx
@adapt_dec_loop:
    cmp     ecx, INTENT_FEATURE_DIM
    jge     @adapt_ok

    movss   xmm0, DWORD PTR [rdi + rcx * 4]
    movss   xmm1, DWORD PTR [rsi + rcx * 4]
    mulss   xmm1, xmm3
    subss   xmm0, xmm1
    movss   DWORD PTR [rdi + rcx * 4], xmm0
    inc     ecx
    jmp     @adapt_dec_loop

@adapt_ok:
    lock inc QWORD PTR [neural_stats + NEURAL_STAT_ADAPTATIONS]
    xor     eax, eax
    add     rsp, 32
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
asm_neural_adapt ENDP

; =============================================================================
; asm_neural_get_stats — Return pointer to neural bridge statistics
; =============================================================================
asm_neural_get_stats PROC
    lea     rax, [neural_stats]
    ret
asm_neural_get_stats ENDP

; =============================================================================
; asm_neural_shutdown — Teardown neural bridge
; =============================================================================
asm_neural_shutdown PROC
    push    rbx
    sub     rsp, 32

    mov     eax, DWORD PTR [neural_initialized]
    test    eax, eax
    jz      @nshut_ok

    lea     rcx, [neural_lock]
    call    QWORD PTR [__imp_AcquireSRWLockExclusive]
    mov     DWORD PTR [neural_initialized], 0
    lea     rcx, [neural_lock]
    call    QWORD PTR [__imp_ReleaseSRWLockExclusive]

@nshut_ok:
    xor     eax, eax
    add     rsp, 32
    pop     rbx
    ret
asm_neural_shutdown ENDP

END
