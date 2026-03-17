;==============================================================================
; file_browser.inc - MASM File Browser Header
; Author: RawrXD CI/CD
; Date: Dec 29, 2025
;==============================================================================

IFNDEF FILE_BROWSER_INC
FILE_BROWSER_INC EQU 1

; File types
FILE_TYPE_UNKNOWN       EQU 0
FILE_TYPE_FILE          EQU 1
FILE_TYPE_DIRECTORY     EQU 2
FILE_TYPE_SYMLINK       EQU 3

; Project types
PROJECT_TYPE_NONE       EQU 0
PROJECT_TYPE_GIT        EQU 1
PROJECT_TYPE_CMAKE      EQU 2
PROJECT_TYPE_PYTHON     EQU 3
PROJECT_TYPE_RUST       EQU 4

; Function prototypes
EXTERN masm_file_browser_init:PROC
EXTERN masm_file_browser_shutdown:PROC
EXTERN masm_file_browser_set_root:PROC
EXTERN masm_file_browser_scan_directory:PROC
EXTERN masm_file_browser_get_node:PROC
EXTERN masm_file_browser_expand_node:PROC
EXTERN masm_file_browser_collapse_node:PROC
EXTERN masm_file_browser_list_children:PROC
EXTERN masm_file_browser_add_filter:PROC
EXTERN masm_file_browser_remove_filter:PROC
EXTERN masm_file_browser_detect_project:PROC
EXTERN masm_file_browser_get_project_type:PROC
EXTERN masm_file_browser_search_files:PROC
EXTERN masm_file_browser_watch_changes:PROC
EXTERN masm_file_browser_refresh_tree:PROC

ENDIF
