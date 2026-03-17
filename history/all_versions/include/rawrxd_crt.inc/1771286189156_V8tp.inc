; ═══════════════════════════════════════════════════════════════════════════════
; rawrxd_crt.inc - C Runtime EXTERN Declarations (x64 MSVC CRT)
; ═══════════════════════════════════════════════════════════════════════════════
; Resolves all undefined C runtime call targets.
; Link with: msvcrt.lib or ucrt.lib + vcruntime.lib
;
; Usage:  INCLUDE rawrxd_crt.inc
; ═══════════════════════════════════════════════════════════════════════════════

IFNDEF RAWRXD_CRT_INC
RAWRXD_CRT_INC EQU 1

; ─── Standard I/O ─────────────────────────────────────────────────────────────
EXTERNDEF printf:PROC
EXTERNDEF sprintf:PROC
EXTERNDEF snprintf:PROC
EXTERNDEF fprintf:PROC
EXTERNDEF fopen:PROC
EXTERNDEF fclose:PROC
EXTERNDEF fread:PROC
EXTERNDEF fwrite:PROC
EXTERNDEF fseek:PROC
EXTERNDEF ftell:PROC
EXTERNDEF fflush:PROC
EXTERNDEF puts:PROC
EXTERNDEF fputs:PROC
EXTERNDEF fgets:PROC
EXTERNDEF sscanf:PROC

; ─── Memory ───────────────────────────────────────────────────────────────────
EXTERNDEF malloc:PROC
EXTERNDEF calloc:PROC
EXTERNDEF realloc:PROC
EXTERNDEF free:PROC
EXTERNDEF memcpy:PROC
EXTERNDEF memset:PROC
EXTERNDEF memmove:PROC
EXTERNDEF memcmp:PROC

; ─── String ───────────────────────────────────────────────────────────────────
EXTERNDEF strlen:PROC
EXTERNDEF strcpy:PROC
EXTERNDEF strncpy:PROC
EXTERNDEF strncpy_s:PROC
EXTERNDEF strcat:PROC
EXTERNDEF strncat:PROC
EXTERNDEF strcmp:PROC
EXTERNDEF strncmp:PROC
EXTERNDEF strchr:PROC
EXTERNDEF strrchr:PROC
EXTERNDEF strstr:PROC
EXTERNDEF strtok:PROC
EXTERNDEF stricmp:PROC
EXTERNDEF _stricmp:PROC
EXTERNDEF _strdup:PROC
EXTERNDEF wcstombs:PROC
EXTERNDEF mbstowcs:PROC
EXTERNDEF wcslen:PROC

; ─── Conversion ───────────────────────────────────────────────────────────────
EXTERNDEF atoi:PROC
EXTERNDEF atol:PROC
EXTERNDEF atof:PROC
EXTERNDEF strtol:PROC
EXTERNDEF strtoul:PROC
EXTERNDEF strtod:PROC
EXTERNDEF itoa:PROC
EXTERNDEF _itoa:PROC
EXTERNDEF ltoa:PROC
EXTERNDEF _ltoa:PROC

; ─── Math ─────────────────────────────────────────────────────────────────────
EXTERNDEF exp:PROC
EXTERNDEF log:PROC
EXTERNDEF sqrt:PROC
EXTERNDEF pow:PROC
EXTERNDEF floor:PROC
EXTERNDEF ceil:PROC
EXTERNDEF fabs:PROC
EXTERNDEF sin:PROC
EXTERNDEF cos:PROC

; ─── Process / System ─────────────────────────────────────────────────────────
EXTERNDEF atexit:PROC
EXTERNDEF exit:PROC
EXTERNDEF _exit:PROC
EXTERNDEF abort:PROC
EXTERNDEF system:PROC
EXTERNDEF getenv:PROC
EXTERNDEF _set_invalid_parameter_handler:PROC
EXTERNDEF _set_purecall_handler:PROC

; ─── Time ─────────────────────────────────────────────────────────────────────
EXTERNDEF time:PROC
EXTERNDEF clock:PROC
EXTERNDEF difftime:PROC

; ─── Character Classification ─────────────────────────────────────────────────
EXTERNDEF isalpha:PROC
EXTERNDEF isdigit:PROC
EXTERNDEF isalnum:PROC
EXTERNDEF isspace:PROC
EXTERNDEF toupper:PROC
EXTERNDEF tolower:PROC

ENDIF ; RAWRXD_CRT_INC
