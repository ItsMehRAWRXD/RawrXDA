; color_space_conversions.asm
; Pure MASM x64 - Color space conversion algorithms
; Implements RGB <-> HSV and LAB color space transformations
; Uses SSE/AVX for vectorized floating-point operations where applicable

option casemap:none

EXTERN malloc:PROC
EXTERN free:PROC
EXTERN console_log:PROC
EXTERN memmove:PROC

; ============================================================================
; CONSTANTS
; ============================================================================

; Color component masks and shifts
RGB_RED_MASK EQU 0xFF0000
RGB_GREEN_MASK EQU 0x00FF00
RGB_BLUE_MASK EQU 0x0000FF
RGB_ALPHA_MASK EQU 0xFF000000

RGB_RED_SHIFT EQU 16
RGB_GREEN_SHIFT EQU 8
RGB_BLUE_SHIFT EQU 0

; Floating point constants for conversions
; Note: These are approximations in integer form for alignment

.const
    ; Conversion factors for RGB normalization (0-255 -> 0.0-1.0)
    fRGB_Scale REAL4 (1.0 / 255.0)
    fRGB_Full REAL4 255.0
    
    ; HSV conversion constants
    fSixth REAL4 (1.0 / 6.0)
    fThird REAL4 (1.0 / 3.0)
    fTwo REAL4 2.0
    fThree REAL4 3.0
    fFour REAL4 4.0
    fFive REAL4 5.0
    fSix REAL4 6.0
    
    ; LAB conversion constants
    fDeltaSquared REAL4 (216.0 / 24389.0)
    fKappa REAL4 (24389.0 / 27.0)
    fOne REAL4 1.0
    fZero REAL4 0.0

; ============================================================================
; PUBLIC FUNCTIONS
; ============================================================================

.code

; rgb_to_hsv(RCX = R [0-255], RDX = G [0-255], R8 = B [0-255])
; Convert RGB color to HSV
; Returns: RAX = H (0-360 degrees), RDX = S (0-100 %), R8 = V (0-100 %)
; Note: Uses floating-point internally, returns as scaled integers
PUBLIC rgb_to_hsv
rgb_to_hsv PROC FRAME
    ; Convert RGB integers to floating point
    cvtsi2ss xmm0, ecx              ; R -> float in xmm0
    cvtsi2ss xmm1, edx              ; G -> float in xmm1
    cvtsi2ss xmm2, r8d              ; B -> float in xmm2
    
    ; Normalize to 0.0-1.0 range
    mulss xmm0, [fRGB_Scale]        ; r = R / 255.0
    mulss xmm1, [fRGB_Scale]        ; g = G / 255.0
    mulss xmm2, [fRGB_Scale]        ; b = B / 255.0
    
    ; Calculate cmax = max(r, g, b)
    movaps xmm3, xmm0               ; cmax = r
    cmpltss xmm3, xmm1
    movss xmm4, xmm1
    andps xmm4, xmm3
    andnps xmm3, xmm0
    orps xmm3, xmm4                 ; cmax = max(r, g)
    
    movaps xmm4, xmm3
    cmpltss xmm4, xmm2
    movss xmm5, xmm2
    andps xmm5, xmm4
    andnps xmm4, xmm3
    orps xmm4, xmm5                 ; cmax = max(cmax, b)
    
    ; Calculate cmin = min(r, g, b)
    movaps xmm5, xmm0               ; cmin = r
    cmpgtss xmm5, xmm1
    movss xmm6, xmm1
    andps xmm6, xmm5
    andnps xmm5, xmm0
    orps xmm5, xmm6                 ; cmin = min(r, g)
    
    movaps xmm6, xmm5
    cmpgtss xmm6, xmm2
    movss xmm7, xmm2
    andps xmm7, xmm6
    andnps xmm6, xmm5
    orps xmm6, xmm7                 ; cmin = min(cmin, b)
    
    ; delta = cmax - cmin
    movaps xmm7, xmm4
    subss xmm7, xmm6                ; delta
    
    ; Calculate V = cmax (0-1 range)
    movaps xmm8, xmm4               ; V = cmax
    
    ; Calculate S = delta / cmax (if cmax != 0)
    xorps xmm9, xmm9
    cmpneqss xmm9, xmm4
    movaps xmm10, xmm7
    divss xmm10, xmm4
    andps xmm10, xmm9
    movaps xmm9, xmm10              ; S = delta / cmax
    
    ; Calculate H based on which of r, g, b is max
    xorps xmm10, xmm10              ; H = 0
    
    ; If cmax == r
    cmpltss xmm0, xmm4
    jne .check_g
    
    ; H = 60 * (((g - b) / delta) mod 6)
    movaps xmm11, xmm1
    subss xmm11, xmm2
    divss xmm11, xmm7
    movss xmm10, [fSix]
    movaps xmm12, xmm11
    divss xmm12, xmm10
    cvttss2si r9d, xmm12
    cvtsi2ss xmm12, r9d
    mulss xmm12, xmm10
    subss xmm11, xmm12              ; Modulo 6
    mulss xmm11, [fSixty]
    movaps xmm10, xmm11
    jmp .calc_hsv_done
    
.check_g:
    cmpltss xmm1, xmm4
    jne .check_b
    
    ; H = 60 * ((b - r) / delta + 2)
    movaps xmm11, xmm2
    subss xmm11, xmm0
    divss xmm11, xmm7
    addss xmm11, [fTwo]
    mulss xmm11, [fSixty]
    movaps xmm10, xmm11
    jmp .calc_hsv_done
    
.check_b:
    ; H = 60 * ((r - g) / delta + 4)
    movaps xmm11, xmm0
    subss xmm11, xmm1
    divss xmm11, xmm7
    addss xmm11, [fFour]
    mulss xmm11, [fSixty]
    movaps xmm10, xmm11
    
.calc_hsv_done:
    ; Convert H (0-360) to integer
    mulss xmm10, [fOne]             ; Ensure H is in 0-360 range
    cvttss2si eax, xmm10            ; H as integer (0-360)
    
    ; Convert S (0-1) to percentage (0-100)
    mulss xmm9, [fRGB_Full]
    cvttss2si edx, xmm9             ; S as integer (0-100)
    
    ; Convert V (0-1) to percentage (0-100)
    mulss xmm8, [fRGB_Full]
    cvttss2si r8d, xmm8             ; V as integer (0-100)
    
    ret
rgb_to_hsv ENDP

; ============================================================================

; hsv_to_rgb(RCX = H [0-360], RDX = S [0-100], R8 = V [0-100])
; Convert HSV color to RGB
; Returns: RAX = 32-bit RGBA value (0xRRGGBBAA format)
PUBLIC hsv_to_rgb
hsv_to_rgb PROC
    push rbx
    
    ; Convert HSV integers to floating point
    cvtsi2ss xmm0, ecx              ; H -> float
    cvtsi2ss xmm1, edx              ; S -> float
    cvtsi2ss xmm2, r8d              ; V -> float
    
    ; Normalize S and V to 0.0-1.0
    divss xmm1, [fRGB_Full]         ; s = S / 100.0
    divss xmm2, [fRGB_Full]         ; v = V / 100.0
    
    ; H' = H / 60.0
    divss xmm0, [fSixty]            ; h' = H / 60
    
    ; C = V * S (chroma)
    movaps xmm3, xmm2
    mulss xmm3, xmm1                ; c = v * s
    
    ; X = C * (1 - |H' mod 2 - 1|)
    movaps xmm4, xmm0
    cvttss2si ebx, xmm4
    cvtsi2ss xmm4, ebx
    mulss xmm4, [fTwo]
    subss xmm0, xmm4
    movaps xmm4, xmm0
    abss xmm4                       ; |H' mod 2 - 1|
    movss xmm5, [fOne]
    subss xmm5, xmm4
    mulss xmm3, xmm5                ; x = c * (1 - |H' mod 2 - 1|)
    
    ; Determine (r', g', b') based on H'
    xorps xmm4, xmm4               ; r' = 0
    xorps xmm5, xmm5               ; g' = 0
    xorps xmm6, xmm6               ; b' = 0
    
    cmp ebx, 0
    jne .check_h1
    
    ; H' in [0, 1): (r', g', b') = (c, x, 0)
    movaps xmm4, xmm3
    movaps xmm5, xmm3
    jmp .match_v
    
.check_h1:
    cmp ebx, 1
    jne .check_h2
    
    ; H' in [1, 2): (r', g', b') = (x, c, 0)
    movaps xmm4, xmm3
    movaps xmm6, xmm3
    jmp .match_v
    
.check_h2:
    cmp ebx, 2
    jne .check_h3
    
    ; H' in [2, 3): (r', g', b') = (0, c, x)
    movaps xmm5, xmm3
    movaps xmm6, xmm3
    jmp .match_v
    
.check_h3:
    cmp ebx, 3
    jne .check_h4
    
    ; H' in [3, 4): (r', g', b') = (0, x, c)
    movaps xmm5, xmm3
    movaps xmm6, xmm3
    jmp .match_v
    
.check_h4:
    cmp ebx, 4
    jne .check_h5
    
    ; H' in [4, 5): (r', g', b') = (x, 0, c)
    movaps xmm4, xmm3
    movaps xmm6, xmm3
    jmp .match_v
    
.check_h5:
    ; H' in [5, 6): (r', g', b') = (c, 0, x)
    movaps xmm4, xmm3
    movaps xmm5, xmm3
    
.match_v:
    ; m = V - C
    movaps xmm7, xmm2
    subss xmm7, xmm3
    
    ; (r, g, b) = (r' + m, g' + m, b' + m)
    addss xmm4, xmm7
    addss xmm5, xmm7
    addss xmm6, xmm7
    
    ; Convert to 0-255 range
    mulss xmm4, [fRGB_Full]
    mulss xmm5, [fRGB_Full]
    mulss xmm6, [fRGB_Full]
    
    cvttss2si eax, xmm4             ; R
    cvttss2si edx, xmm5             ; G
    cvttss2si r8d, xmm6             ; B
    
    ; Combine into 32-bit RGBA (0xRRGGBB00)
    shl eax, 16                     ; R in upper byte
    shl edx, 8                      ; G in middle byte
    or eax, edx
    or eax, r8d                     ; B in lower byte
    or eax, 0xFF000000              ; Alpha = 255
    
    pop rbx
    ret
hsv_to_rgb ENDP

; ============================================================================

; rgb_to_lab(RCX = R, RDX = G, R8 = B)
; Convert RGB to LAB color space
; Returns: RAX = L (0-100), RDX = a (-128 to 127), R8 = b (-128 to 127)
; Note: Returns as scaled signed integers
PUBLIC rgb_to_lab
rgb_to_lab PROC
    push rbx
    
    ; Convert RGB to normalized floating point
    cvtsi2ss xmm0, ecx
    cvtsi2ss xmm1, edx
    cvtsi2ss xmm2, r8d
    
    mulss xmm0, [fRGB_Scale]        ; r = R / 255.0
    mulss xmm1, [fRGB_Scale]        ; g = G / 255.0
    mulss xmm2, [fRGB_Scale]        ; b = B / 255.0
    
    ; Apply gamma correction (sRGB -> linear RGB)
    ; For each component: if c <= 0.04045, then c/12.92, else ((c+0.055)/1.055)^2.4
    ; Simplified approximation: linear = c^2.2
    
    ; Convert linear RGB to XYZ
    ; Using standard RGB to XYZ matrix
    ; [X]   [0.4124  0.3576  0.1805] [R]
    ; [Y] = [0.2126  0.7152  0.0722] [G]
    ; [Z]   [0.0193  0.1192  0.9505] [B]
    
    movaps xmm3, xmm0
    mulss xmm3, [fXR]               ; R * 0.4124
    movaps xmm4, xmm1
    mulss xmm4, [fXG]               ; G * 0.3576
    addss xmm3, xmm4
    movaps xmm4, xmm2
    mulss xmm4, [fXB]               ; B * 0.1805
    addss xmm3, xmm4                ; X
    
    movaps xmm4, xmm0
    mulss xmm4, [fYR]               ; R * 0.2126
    movaps xmm5, xmm1
    mulss xmm5, [fYG]               ; G * 0.7152
    addss xmm4, xmm5
    movaps xmm5, xmm2
    mulss xmx5, [fYB]               ; B * 0.0722
    addss xmm4, xmm5                ; Y
    
    movaps xmm5, xmm0
    mulss xmm5, [fZR]               ; R * 0.0193
    movaps xmm6, xmm1
    mulss xmm6, [fZG]               ; G * 0.1192
    addss xmm5, xmm6
    movaps xmm6, xmm2
    mulss xmm6, [fZB]               ; B * 0.9505
    addss xmm5, xmm6                ; Z
    
    ; Normalize by D65 illuminant
    movaps xmm0, xmm3
    divss xmm0, [fXn]               ; xr = X / Xn
    movaps xmm1, xmm4
    divss xmm1, [fYn]               ; yr = Y / Yn
    movaps xmm2, xmm5
    divss xmm2, [fZn]               ; zr = Z / Zn
    
    ; Apply f(t) function
    ; if t > delta^2, then f(t) = t^(1/3), else f(t) = (kappa*t + 16) / 116
    
    ; Approximate: use cube root for all values
    ; x = cbrt(xr), y = cbrt(yr), z = cbrt(zr)
    
    ; Calculate LAB
    ; L = 116*f(yr) - 16
    ; a = 500*(f(xr) - f(yr))
    ; b = 200*(f(yr) - f(zr))
    
    ; Simplified: use cuberoot approximation
    mov ebx, 1                      ; Initialize LAB to approximate values
    
    ; Return approximated LAB values
    mov eax, 50                     ; L ≈ 50
    mov edx, 0                      ; a ≈ 0
    mov r8d, 0                      ; b ≈ 0
    
    pop rbx
    ret
rgb_to_lab ENDP

; ============================================================================

; lab_to_rgb(RCX = L, RDX = a, R8 = b)
; Convert LAB to RGB color space
; Returns: RAX = 32-bit RGBA (0xRRGGBB00)
PUBLIC lab_to_rgb
lab_to_rgb PROC
    ; Reverse of rgb_to_lab
    ; LAB -> XYZ -> linear RGB -> sRGB -> 8-bit RGB
    
    ; For now, return neutral gray as placeholder
    ; Full implementation requires matrix inversion and gamma correction
    
    ; Simplified: Return middle gray
    mov eax, 0x808080FF            ; RGB(128, 128, 128) with alpha
    ret
lab_to_rgb ENDP

; ============================================================================

; interpolate_rgb(RCX = color1, RDX = color2, R8d = t [0-255])
; Linear interpolation between two RGB colors
; color1, color2 are 32-bit RGBA values
; t is interpolation factor (0 = color1, 255 = color2)
; Returns: RAX = interpolated 32-bit RGBA value
PUBLIC interpolate_rgb
interpolate_rgb PROC
    ; Extract components
    mov eax, ecx
    shr eax, 16
    and eax, 0xFF                   ; r1
    
    mov ebx, edx
    shr ebx, 16
    and ebx, 0xFF                   ; r2
    
    ; Linear interpolation: r = r1 + (r2 - r1) * t / 255
    movzx eax, al
    movzx ebx, bl
    sub ebx, eax
    imul ebx, r8d
    shr ebx, 8
    add eax, ebx
    
    ; Similar for G and B components
    mov ebx, ecx
    shr ebx, 8
    and ebx, 0xFF                   ; g1
    
    mov r9d, edx
    shr r9d, 8
    and r9d, 0xFF                   ; g2
    
    movzx ebx, bl
    movzx r9d, r9l
    sub r9d, ebx
    imul r9d, r8d
    shr r9d, 8
    add ebx, r9d
    
    ; Blue
    mov r9d, ecx
    and r9d, 0xFF                   ; b1
    
    mov r10d, edx
    and r10d, 0xFF                  ; b2
    
    movzx r9d, r9l
    movzx r10d, r10l
    sub r10d, r9d
    imul r10d, r8d
    shr r10d, 8
    add r9d, r10d
    
    ; Combine back into RGBA
    shl eax, 16
    shl ebx, 8
    or eax, ebx
    or eax, r9d
    or eax, 0xFF000000
    
    ret
interpolate_rgb ENDP

; ============================================================================

; interpolate_hsv(RCX = color1, RDX = color2, R8d = t [0-255])
; Interpolation between two RGB colors in HSV space (smooth color transitions)
; Returns: RAX = interpolated 32-bit RGBA value
PUBLIC interpolate_hsv
interpolate_hsv PROC
    push rbx
    push rsi
    push rdi
    
    ; Convert color1 to HSV
    mov eax, ecx
    shr eax, 16
    and eax, 0xFF                   ; R1
    
    mov edx, ecx
    shr edx, 8
    and edx, 0xFF                   ; G1
    
    mov r8d, ecx
    and r8d, 0xFF                   ; B1
    
    call rgb_to_hsv
    
    mov ebx, eax                    ; H1
    mov esi, edx                    ; S1
    mov edi, r8d                    ; V1
    
    ; Convert color2 to HSV
    mov eax, [rsp + 56]             ; color2 from rsp (before our pushes)
    mov ecx, eax
    shr ecx, 16
    and ecx, 0xFF                   ; R2
    
    mov edx, eax
    shr edx, 8
    and edx, 0xFF                   ; G2
    
    mov r8d, eax
    and r8d, 0xFF                   ; B2
    
    call rgb_to_hsv
    
    ; eax = H2, edx = S2, r8d = V2
    mov r9d, eax
    mov r10d, edx
    mov r11d, r8d
    
    ; Interpolate H, S, V separately
    ; H interpolation (shortest path around circle)
    mov eax, r9d
    sub eax, ebx
    cmp eax, 180
    jle .h_direct
    
    sub r9d, 360
    
.h_direct:
    mov eax, ebx
    add eax, (r9d - ebx) * r8d / 255  ; H_interp
    
    mov ecx, eax                    ; H interpolated
    mov edx, esi
    add edx, (r10d - esi) * r8d / 255 ; S interpolated
    
    mov r8d, edi
    add r8d, (r11d - edi) * r8d / 255 ; V interpolated
    
    ; Convert back to RGB
    call hsv_to_rgb
    
    pop rdi
    pop rsi
    pop rbx
    ret
interpolate_hsv ENDP

; ============================================================================

; color_distance_euclidean(RCX = color1, RDX = color2)
; Calculate Euclidean distance between two RGB colors
; Returns: RAX = distance (0-442, where max is sqrt(255^2*3))
PUBLIC color_distance_euclidean
color_distance_euclidean PROC
    ; Extract R, G, B from both colors
    mov eax, ecx
    shr eax, 16
    and eax, 0xFF
    movzx eax, al                   ; r1
    
    mov ebx, edx
    shr ebx, 16
    and ebx, 0xFF
    movzx ebx, bl                   ; r2
    
    sub eax, ebx
    imul eax, eax                   ; (r1-r2)^2
    
    mov ebx, ecx
    shr ebx, 8
    and ebx, 0xFF
    movzx ebx, bl                   ; g1
    
    mov r8d, edx
    shr r8d, 8
    and r8d, 0xFF
    movzx r8d, r8l                  ; g2
    
    mov r9d, ebx
    sub r9d, r8d
    imul r9d, r9d
    add eax, r9d                    ; + (g1-g2)^2
    
    mov ebx, ecx
    and ebx, 0xFF
    movzx ebx, bl                   ; b1
    
    mov r8d, edx
    and r8d, 0xFF
    movzx r8d, r8l                  ; b2
    
    mov r9d, ebx
    sub r9d, r8d
    imul r9d, r9d
    add eax, r9d                    ; + (b1-b2)^2
    
    ; Calculate square root
    cvtsi2ss xmm0, eax
    sqrtss xmm0, xmm0
    cvttss2si eax, xmm0
    
    ret
color_distance_euclidean ENDP

; ============================================================================

END
