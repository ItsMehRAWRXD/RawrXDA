;==========================================================================
; file_tree_driver.asm - File Tree with Drive Navigation
;==========================================================================

option casemap:none

include windows.inc
includelib kernel32.lib
includelib user32.lib

; EXTERN GetLogicalDrives:PROC
; EXTERN GetDriveTypeA:PROC
; EXTERN CreateWindowExA:PROC
; EXTERN SendMessageA:PROC

PUBLIC file_tree_init
PUBLIC file_tree_expand_drive
PUBLIC file_tree_refresh

MAX_DRIVES EQU 26

.data?
    DriveList       BYTE MAX_DRIVES * 4 DUP (?)
    DriveCount      DWORD ?

.code

file_tree_init PROC
    ; rcx = hParent, edx = x, r8d = y, r9d = width, [rsp+32] = height
    mov dword ptr [DriveCount], 0
    xor eax, eax
    ret
file_tree_init ENDP

file_tree_expand_drive PROC
    ; ecx = drive_id
    xor eax, eax
    ret
file_tree_expand_drive ENDP

file_tree_refresh PROC
    xor eax, eax
    ret
file_tree_refresh ENDP

END
