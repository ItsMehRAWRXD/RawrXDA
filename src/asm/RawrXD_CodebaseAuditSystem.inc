; =============================================================================
; RawrXD_CodebaseAuditSystem.inc — Structure layouts for codebase audit (x64 MASM)
; Matches codebase_audit_system.hpp (no C++ dependency)
; =============================================================================

; CodeQualityMetrics (simplified for ASM)
CQM_LINES_OF_CODE       EQU 0
CQM_COMMENT_LINES       EQU 4
CQM_BLANK_LINES         EQU 8
CQM_CYCLOMATIC          EQU 12
CQM_FUNCTION_COUNT      EQU 16
CQM_CLASS_COUNT         EQU 20
CQM_COMMENT_RATIO       EQU 24   ; double = 8 bytes
CQM_SIZE                EQU 64

; SecurityAnalysis (simplified)
SA_BUFFER_OVERFLOW      EQU 0
SA_HARDCODED_SECRETS    EQU 4
SA_UNSAFE_CASTS         EQU 8
SA_MEMORY_LEAKS         EQU 12
SA_OVERALL_SCORE        EQU 16   ; double
SA_PASSES               EQU 24   ; bool
SA_SIZE                 EQU 32

; FileAnalysisResult minimal layout (output)
FAR_FILE_PATH           EQU 0    ; char* or offset
FAR_OVERALL_SCORE       EQU 8   ; double
FAR_PRODUCTION_READY    EQU 16  ; bool
FAR_ANALYSIS_OK         EQU 20
FAR_SIZE                EQU 32

; Audit result codes
AUDIT_OK                EQU 1
AUDIT_FAIL              EQU 0

; Max path and buffer
AUDIT_MAX_PATH          EQU 260
AUDIT_READ_BUF          EQU 65536

; ProjectAuditResult minimal layout (output of AuditFullProject)
PAR_TOTAL_FILES         EQU 0
PAR_FILES_PASSED        EQU 4
PAR_FILES_FAILED        EQU 8
PAR_CRITICAL_ISSUES     EQU 12
PAR_SCORE               EQU 16   ; double
PAR_PRODUCTION_READY    EQU 24   ; bool
PAR_PATH_PTR            EQU 32
PAR_NAME_PTR            EQU 40
PAR_SIZE                EQU 48
PAR_MAX_PATHS           EQU 32
