;==============================================================================
; final_missing_symbols.asm - Final 50 missing symbols
;==============================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib dwrite.lib
includelib d2d1.lib

;==============================================================================
; EXTERNAL DECLARATIONS
;==============================================================================
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN GetProcessHeap:PROC
EXTERN CreateThread:PROC
EXTERN CreatePipe:PROC
EXTERN OutputDebugStringA:PROC
EXTERN lstrcpyA:PROC
EXTERN CloseHandle:PROC

EXTERN asm_log:PROC
EXTERN object_create:PROC
EXTERN object_destroy:PROC

;==============================================================================
; DATA
;==============================================================================
.data

hwnd_editor                  QWORD 0
PUBLIC hwnd_editor

g_rawr1024_active            DWORD 0

szMsg                        BYTE "Function called",0

.code

;==============================================================================
; Windows API Wrappers
;==============================================================================

qt_foundation_init PROC
    sub rsp, 40
    lea rcx, szMsg
    call asm_log
    xor eax, eax
    add rsp, 40
    ret
qt_foundation_init ENDP
PUBLIC qt_foundation_init

CreateMenuA PROC
    sub rsp, 40
    call CreateMenu
    add rsp, 40
    ret
CreateMenuA ENDP
PUBLIC CreateMenuA

CreatePopupMenuA PROC
    sub rsp, 40
    call CreatePopupMenu
    add rsp, 40
    ret
CreatePopupMenuA ENDP
PUBLIC CreatePopupMenuA

CreateThreadEx PROC
    sub rsp, 88
    call CreateThread
    add rsp, 88
    ret
CreateThreadEx ENDP
PUBLIC CreateThreadEx

CreatePipeEx PROC
    sub rsp, 88
    call CreatePipe
    add rsp, 88
    ret
CreatePipeEx ENDP
PUBLIC CreatePipeEx

;==============================================================================
; MainWindow Helper Functions
;==============================================================================

main_window_hide PROC
    sub rsp, 40
    xor eax, eax
    add rsp, 40
    ret
main_window_hide ENDP
PUBLIC main_window_hide

main_window_set_title PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
main_window_set_title ENDP
PUBLIC main_window_set_title

main_window_get_title PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
main_window_get_title ENDP
PUBLIC main_window_get_title

main_window_set_status PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
main_window_set_status ENDP
PUBLIC main_window_set_status

main_window_get_status PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
main_window_get_status ENDP
PUBLIC main_window_get_status

;==============================================================================
; Settings
;==============================================================================

masm_settings_reset_to_defaults PROC
    sub rsp, 40
    xor eax, eax
    add rsp, 40
    ret
masm_settings_reset_to_defaults ENDP
PUBLIC masm_settings_reset_to_defaults

;==============================================================================
; Terminal Process Management
;==============================================================================

masm_terminal_spawn_process PROC
    sub rsp, 88
    xor eax, eax
    add rsp, 88
    ret
masm_terminal_spawn_process ENDP
PUBLIC masm_terminal_spawn_process

masm_terminal_kill_process PROC
    sub rsp, 48
    xor eax, eax
    add rsp, 48
    ret
masm_terminal_kill_process ENDP
PUBLIC masm_terminal_kill_process

masm_terminal_get_status PROC
    sub rsp, 48
    xor eax, eax
    add rsp, 48
    ret
masm_terminal_get_status ENDP
PUBLIC masm_terminal_get_status

masm_terminal_list_processes PROC
    sub rsp, 40
    xor eax, eax
    add rsp, 40
    ret
masm_terminal_list_processes ENDP
PUBLIC masm_terminal_list_processes

masm_terminal_wait_for_process PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_terminal_wait_for_process ENDP
PUBLIC masm_terminal_wait_for_process

masm_terminal_get_process_count PROC
    sub rsp, 40
    xor eax, eax
    add rsp, 40
    ret
masm_terminal_get_process_count ENDP
PUBLIC masm_terminal_get_process_count

;==============================================================================
; Hotpatch Extended
;==============================================================================

masm_hotpatch_add_patch PROC
    sub rsp, 88
    xor eax, eax
    add rsp, 88
    ret
masm_hotpatch_add_patch ENDP
PUBLIC masm_hotpatch_add_patch

masm_hotpatch_remove_patch PROC
    sub rsp, 48
    xor eax, eax
    add rsp, 48
    ret
masm_hotpatch_remove_patch ENDP
PUBLIC masm_hotpatch_remove_patch

masm_hotpatch_rollback_patch PROC
    sub rsp, 48
    xor eax, eax
    add rsp, 48
    ret
masm_hotpatch_rollback_patch ENDP
PUBLIC masm_hotpatch_rollback_patch

masm_hotpatch_list_patches PROC
    sub rsp, 40
    xor eax, eax
    add rsp, 40
    ret
masm_hotpatch_list_patches ENDP
PUBLIC masm_hotpatch_list_patches

masm_hotpatch_get_patch_status PROC
    sub rsp, 48
    xor eax, eax
    add rsp, 48
    ret
masm_hotpatch_get_patch_status ENDP
PUBLIC masm_hotpatch_get_patch_status

;==============================================================================
; MainWindow Extended
;==============================================================================

masm_mainwindow_create PROC
    sub rsp, 88
    xor eax, eax
    add rsp, 88
    ret
masm_mainwindow_create ENDP
PUBLIC masm_mainwindow_create

masm_mainwindow_close PROC
    sub rsp, 40
    xor eax, eax
    add rsp, 40
    ret
masm_mainwindow_close ENDP
PUBLIC masm_mainwindow_close

masm_mainwindow_resize PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_mainwindow_resize ENDP
PUBLIC masm_mainwindow_resize

masm_mainwindow_add_dock PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_mainwindow_add_dock ENDP
PUBLIC masm_mainwindow_add_dock

masm_mainwindow_show_dock PROC
    sub rsp, 48
    xor eax, eax
    add rsp, 48
    ret
masm_mainwindow_show_dock ENDP
PUBLIC masm_mainwindow_show_dock

masm_mainwindow_hide_dock PROC
    sub rsp, 48
    xor eax, eax
    add rsp, 48
    ret
masm_mainwindow_hide_dock ENDP
PUBLIC masm_mainwindow_hide_dock

masm_mainwindow_remove_menu_item PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_mainwindow_remove_menu_item ENDP
PUBLIC masm_mainwindow_remove_menu_item

masm_mainwindow_dispatch_signal PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_mainwindow_dispatch_signal ENDP
PUBLIC masm_mainwindow_dispatch_signal

masm_mainwindow_list_docks PROC
    sub rsp, 40
    xor eax, eax
    add rsp, 40
    ret
masm_mainwindow_list_docks ENDP
PUBLIC masm_mainwindow_list_docks

masm_mainwindow_save_layout PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_mainwindow_save_layout ENDP
PUBLIC masm_mainwindow_save_layout

masm_mainwindow_load_layout PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_mainwindow_load_layout ENDP
PUBLIC masm_mainwindow_load_layout

;==============================================================================
; File Browser Extended
;==============================================================================

masm_file_browser_get_node PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_file_browser_get_node ENDP
PUBLIC masm_file_browser_get_node

masm_file_browser_expand_node PROC
    sub rsp, 48
    xor eax, eax
    add rsp, 48
    ret
masm_file_browser_expand_node ENDP
PUBLIC masm_file_browser_expand_node

masm_file_browser_collapse_node PROC
    sub rsp, 48
    xor eax, eax
    add rsp, 48
    ret
masm_file_browser_collapse_node ENDP
PUBLIC masm_file_browser_collapse_node

masm_file_browser_list_children PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_file_browser_list_children ENDP
PUBLIC masm_file_browser_list_children

masm_file_browser_add_filter PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_file_browser_add_filter ENDP
PUBLIC masm_file_browser_add_filter

masm_file_browser_remove_filter PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_file_browser_remove_filter ENDP
PUBLIC masm_file_browser_remove_filter

masm_file_browser_detect_project PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_file_browser_detect_project ENDP
PUBLIC masm_file_browser_detect_project

masm_file_browser_get_project_type PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_file_browser_get_project_type ENDP
PUBLIC masm_file_browser_get_project_type

masm_file_browser_search_files PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_file_browser_search_files ENDP
PUBLIC masm_file_browser_search_files

masm_file_browser_refresh_tree PROC
    sub rsp, 56
    xor eax, eax
    add rsp, 56
    ret
masm_file_browser_refresh_tree ENDP
PUBLIC masm_file_browser_refresh_tree

;==============================================================================
; Stream Processor
;==============================================================================

stream_processor_init PROC
    sub rsp, 40
    xor eax, eax
    add rsp, 40
    ret
stream_processor_init ENDP
PUBLIC stream_processor_init

stream_processor_shutdown PROC
    sub rsp, 40
    xor eax, eax
    add rsp, 40
    ret
stream_processor_shutdown ENDP
PUBLIC stream_processor_shutdown

stream_stats PROC
    sub rsp, 40
    xor eax, eax
    add rsp, 40
    ret
stream_stats ENDP
PUBLIC stream_stats

;==============================================================================
; RAWR1024 Cleanup
;==============================================================================

rawr1024_cleanup PROC
    sub rsp, 40
    mov g_rawr1024_active, 0
    xor eax, eax
    add rsp, 40
    ret
rawr1024_cleanup ENDP
PUBLIC rawr1024_cleanup

END
