; =============================================================================
; Qt String Formatter - Include Definitions (MASM x64)
; =============================================================================
; Purpose: Printf-style string formatting in pure MASM
; Platform: Windows x64 (MSVC), Linux x64, macOS x64
; Author: AI Toolkit / GitHub Copilot
; Date: December 29, 2025
; =============================================================================

; =============================================================================
; Format Specifier Constants
; =============================================================================

; Format type indicators
FORMAT_TYPE_INVALID     EQU 0
FORMAT_TYPE_PERCENT     EQU 1       ; %% → %
FORMAT_TYPE_DECIMAL     EQU 2       ; %d, %i
FORMAT_TYPE_UNSIGNED    EQU 3       ; %u
FORMAT_TYPE_HEX_LOWER   EQU 4       ; %x
FORMAT_TYPE_HEX_UPPER   EQU 5       ; %X
FORMAT_TYPE_OCTAL       EQU 6       ; %o
FORMAT_TYPE_BINARY      EQU 7       ; %b
FORMAT_TYPE_FLOAT       EQU 8       ; %f
FORMAT_TYPE_SCIENTIFIC  EQU 9       ; %e, %E
FORMAT_TYPE_SHORT_FLOAT EQU 10      ; %g, %G
FORMAT_TYPE_STRING      EQU 11      ; %s
FORMAT_TYPE_CHAR        EQU 12      ; %c
FORMAT_TYPE_POINTER     EQU 13      ; %p
FORMAT_TYPE_COUNT       EQU 14      ; %n (write count)

; Flags for number formatting
FLAG_ZERO_PAD           EQU 0x01    ; 0 flag
FLAG_LEFT_ALIGN         EQU 0x02    ; - flag
FLAG_SPACE_POS          EQU 0x04    ; space flag (positive space)
FLAG_ALWAYS_SIGN        EQU 0x08    ; + flag
FLAG_ALT_FORM           EQU 0x10    ; # flag

; Float formatting flags
FLAG_UPPERCASE          EQU 0x20    ; Uppercase E, X, etc.
FLAG_TRAILING_ZEROS     EQU 0x40    ; Keep trailing zeros

; =============================================================================
; Format Specification Structure
; =============================================================================
; This structure holds parsed format specifier information

QTSTRING_FORMAT_SPEC struct
    flags               DD  ?       ; Flag bits (zero pad, left align, etc.)
    width               DD  ?       ; Field width (0 = not specified)
    precision           DD  ?       ; Precision (-1 = not specified)
    length_mod          DB  ?       ; Length modifier (h, l, L, etc.)
    conversion_type     DB  ?       ; Conversion type (d, x, f, s, etc.)
    reserved1           DW  ?       ; Alignment padding
QTSTRING_FORMAT_SPEC ends

; =============================================================================
; Number Conversion Bases
; =============================================================================

BASE_BINARY             EQU 2
BASE_OCTAL              EQU 8
BASE_DECIMAL            EQU 10
BASE_HEX                EQU 16

; =============================================================================
; Float Constants
; =============================================================================

; Precision defaults
DEFAULT_FLOAT_PRECISION EQU 6       ; %f default
DEFAULT_EXP_PRECISION   EQU 6       ; %e default
DEFAULT_SHORT_PRECISION EQU 6       ; %g default

; Float special values
FLOAT_POSITIVE_ZERO     EQU 0x0000000000000000
FLOAT_NEGATIVE_ZERO     EQU 0x8000000000000000
FLOAT_POSITIVE_INF      EQU 0x7FF0000000000000
FLOAT_NEGATIVE_INF      EQU 0xFFF0000000000000
FLOAT_NAN               EQU 0x7FF8000000000000

; =============================================================================
; String Formatting Limits
; =============================================================================

MAX_FORMAT_WIDTH        EQU 8192    ; Maximum field width
MAX_FLOAT_PRECISION     EQU 308     ; Maximum float precision digits
MAX_INT_DIGITS          EQU 65      ; Maximum digits for 64-bit int (binary)
MAX_FORMAT_BUFFER       EQU 4096    ; Maximum temporary buffer size

; =============================================================================
; Error Codes
; =============================================================================

FORMAT_ERROR_NONE               EQU 0
FORMAT_ERROR_NULL_FORMAT        EQU 1
FORMAT_ERROR_NULL_OUTPUT        EQU 2
FORMAT_ERROR_INVALID_SPEC       EQU 3
FORMAT_ERROR_BUFFER_OVERFLOW    EQU 4
FORMAT_ERROR_INVALID_PRECISION  EQU 5
FORMAT_ERROR_INVALID_WIDTH      EQU 6
FORMAT_ERROR_INSUFFICIENT_ARGS  EQU 7

; =============================================================================
; Character Class Constants
; =============================================================================

CHAR_PERCENT            EQU '%'
CHAR_DASH               EQU '-'
CHAR_PLUS               EQU '+'
CHAR_SPACE              EQU ' '
CHAR_ZERO               EQU '0'
CHAR_HASH               EQU '#'
CHAR_DOT                EQU '.'
CHAR_STAR               EQU '*'

CHAR_H_LOWER            EQU 'h'
CHAR_L_LOWER            EQU 'l'
CHAR_Z_LOWER            EQU 'z'
CHAR_J_LOWER            EQU 'j'
CHAR_T_LOWER            EQU 't'

; =============================================================================
; Digit Characters
; =============================================================================

DIGIT_CHARS_LOWER       DB  '0123456789abcdefghijklmnopqrstuvwxyz'
DIGIT_CHARS_UPPER       DB  '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'

; =============================================================================
; String Formatting API Functions (extern declarations)
; =============================================================================

; Core formatting function
; int wrapper_format_string(char *output, size_t output_size,
;                          const char *format, va_list args)
extern wrapper_format_string

; Format integer with specifier
; int wrapper_format_integer(char *output, size_t output_size,
;                           int64_t value, QTSTRING_FORMAT_SPEC *spec)
extern wrapper_format_integer

; Format unsigned integer with specifier
; int wrapper_format_unsigned(char *output, size_t output_size,
;                            uint64_t value, QTSTRING_FORMAT_SPEC *spec)
extern wrapper_format_unsigned

; Format float with specifier
; int wrapper_format_float(char *output, size_t output_size,
;                         double value, QTSTRING_FORMAT_SPEC *spec)
extern wrapper_format_float

; Format string with specifier
; int wrapper_format_string_arg(char *output, size_t output_size,
;                              const char *str, QTSTRING_FORMAT_SPEC *spec)
extern wrapper_format_string_arg

; Format character with specifier
; int wrapper_format_char(char *output, size_t output_size,
;                        int ch, QTSTRING_FORMAT_SPEC *spec)
extern wrapper_format_char

; Parse format specifier
; QTSTRING_FORMAT_SPEC *wrapper_parse_format(const char *format,
;                                           size_t *consumed)
extern wrapper_parse_format

; =============================================================================
; Utility Helper Functions
; =============================================================================

; Convert integer to string (internal use)
; Returns: Length of converted string
extern wrapper_int_to_string

; Convert float to string (internal use)
extern wrapper_float_to_string

; Calculate field width with padding
extern wrapper_apply_padding

; Pad buffer with character
extern wrapper_pad_buffer

; =============================================================================
; Format String Constants (for error messages)
; =============================================================================

; Common format strings for reference
FMT_INT_DECIMAL         DB  '%d', 0
FMT_INT_HEX             DB  '%x', 0
FMT_INT_OCTAL           DB  '%o', 0
FMT_UNSIGNED            DB  '%u', 0
FMT_FLOAT               DB  '%f', 0
FMT_STRING              DB  '%s', 0
FMT_CHAR                DB  '%c', 0
FMT_POINTER             DB  '%p', 0

; =============================================================================
; Digit String (for conversion)
; =============================================================================

DIGITS_LOWER    DB  '0123456789abcdefghijklmnopqrstuvwxyz', 0
DIGITS_UPPER    DB  '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ', 0

; =============================================================================
; Special String Constants
; =============================================================================

STR_NULL                DB  '(null)', 0
STR_TRUE                DB  'true', 0
STR_FALSE               DB  'false', 0
STR_NAN                 DB  'nan', 0
STR_INF                 DB  'inf', 0
STR_INFINITY            DB  'infinity', 0

; =============================================================================
; Helper Macros for Format Processing
; =============================================================================

; Test if character is digit
; is_digit(ch) - returns 1 if digit, 0 otherwise
MACRO is_digit ch
    local _is_d
    cmp ch, '0'
    jl _is_d_false
    cmp ch, '9'
    jg _is_d_false
    mov eax, 1
    jmp _is_d_end
_is_d_false:
    xor eax, eax
_is_d_end:
ENDM

; Test if character is format flag
; is_flag(ch) - returns 1 if format flag, 0 otherwise
MACRO is_flag ch
    cmp ch, '-'
    je _is_flag_true
    cmp ch, '+'
    je _is_flag_true
    cmp ch, ' '
    je _is_flag_true
    cmp ch, '0'
    je _is_flag_true
    cmp ch, '#'
    je _is_flag_true
    xor eax, eax
    jmp _is_flag_end
_is_flag_true:
    mov eax, 1
_is_flag_end:
ENDM

; Convert character digit to number
; char_to_digit(ch) - returns digit value 0-9, or -1 if not digit
MACRO char_to_digit ch
    sub ch, '0'
    cmp ch, 9
    jbe _char_digit_ok
    mov ch, -1
_char_digit_ok:
ENDM

; =============================================================================
; Floating Point Constants (IEEE 754)
; =============================================================================

; Powers of 10 (for scaling floats)
POW10_0         DQ  10000000000000000h  ; 1.0
POW10_1         DQ  10240000000000000h  ; 10.0 (approximate in binary)
POW10_NEG1      DQ  0EB851EB851EB85Bh   ; 0.1 (approximate)

; =============================================================================
; End of Include Definitions
; =============================================================================
