; ============================================================================
; ICON RESOURCES - Simple placeholder icons for ImageList
; Phase 3: File Explorer Icons
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\gdi32.inc
include \masm32\include\comctl32.inc
includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\gdi32.lib
includelib \masm32\lib\comctl32.lib

; ==================== CONSTANTS ====================
ICON_SIZE              equ 16
ICON_FOLDER            equ 0
ICON_FILE              equ 1
ICON_DRIVE             equ 2

; Colors
CLR_FOLDER_YELLOW      equ 00FFD700h
CLR_FILE_GRAY          equ 00808080h
CLR_DRIVE_BLUE         equ 000080FFh
CLR_BLACK              equ 00000000h

; ==================== CODE ====================

ImageList_Add PROTO :DWORD,:DWORD,:DWORD

.code

; ============================================================================
; CreateFolderIcon - Create 16x16 folder icon bitmap
; Returns: HBITMAP in eax
; ============================================================================
public CreateFolderIcon
CreateFolderIcon proc
    LOCAL hDC:DWORD
    LOCAL hMemDC:DWORD
    LOCAL hBitmap:DWORD
    LOCAL hOldBitmap:DWORD
    LOCAL hBrush:DWORD
    LOCAL hPen:DWORD
    LOCAL hOldBrush:DWORD
    LOCAL hOldPen:DWORD
    
    ; Get screen DC
    invoke GetDC, NULL
    mov hDC, eax
    
    ; Create memory DC
    invoke CreateCompatibleDC, hDC
    mov hMemDC, eax
    
    ; Create bitmap
    invoke CreateCompatibleBitmap, hDC, ICON_SIZE, ICON_SIZE
    mov hBitmap, eax
    
    ; Select bitmap into DC
    invoke SelectObject, hMemDC, hBitmap
    mov hOldBitmap, eax
    
    ; Create folder brush (yellow)
    invoke CreateSolidBrush, CLR_FOLDER_YELLOW
    mov hBrush, eax
    invoke SelectObject, hMemDC, hBrush
    mov hOldBrush, eax
    
    ; Create pen (black outline)
    invoke CreatePen, PS_SOLID, 1, CLR_BLACK
    mov hPen, eax
    invoke SelectObject, hMemDC, hPen
    mov hOldPen, eax
    
    ; Draw folder shape (simple rectangle)
    invoke Rectangle, hMemDC, 2, 4, 14, 13
    
    ; Draw folder tab
    invoke Rectangle, hMemDC, 2, 2, 8, 5
    
    ; Cleanup
    invoke SelectObject, hMemDC, hOldPen
    invoke SelectObject, hMemDC, hOldBrush
    invoke SelectObject, hMemDC, hOldBitmap
    invoke DeleteObject, hPen
    invoke DeleteObject, hBrush
    invoke DeleteDC, hMemDC
    invoke ReleaseDC, NULL, hDC
    
    mov eax, hBitmap
    ret
CreateFolderIcon endp

; ============================================================================
; CreateFileIcon - Create 16x16 file icon bitmap
; Returns: HBITMAP in eax
; ============================================================================
public CreateFileIcon
CreateFileIcon proc
    LOCAL hDC:DWORD
    LOCAL hMemDC:DWORD
    LOCAL hBitmap:DWORD
    LOCAL hOldBitmap:DWORD
    LOCAL hBrush:DWORD
    LOCAL hPen:DWORD
    LOCAL hOldBrush:DWORD
    LOCAL hOldPen:DWORD
    
    invoke GetDC, NULL
    mov hDC, eax
    
    invoke CreateCompatibleDC, hDC
    mov hMemDC, eax
    
    invoke CreateCompatibleBitmap, hDC, ICON_SIZE, ICON_SIZE
    mov hBitmap, eax
    
    invoke SelectObject, hMemDC, hBitmap
    mov hOldBitmap, eax
    
    ; Create file brush (gray)
    invoke CreateSolidBrush, CLR_FILE_GRAY
    mov hBrush, eax
    invoke SelectObject, hMemDC, hBrush
    mov hOldBrush, eax
    
    invoke CreatePen, PS_SOLID, 1, CLR_BLACK
    mov hPen, eax
    invoke SelectObject, hMemDC, hPen
    mov hOldPen, eax
    
    ; Draw file shape (rectangle with folded corner)
    invoke Rectangle, hMemDC, 3, 2, 13, 14
    
    ; Draw folded corner (small triangle)
    invoke MoveToEx, hMemDC, 10, 2, NULL
    invoke LineTo, hMemDC, 13, 2
    invoke LineTo, hMemDC, 13, 5
    invoke LineTo, hMemDC, 10, 2
    
    ; Cleanup
    invoke SelectObject, hMemDC, hOldPen
    invoke SelectObject, hMemDC, hOldBrush
    invoke SelectObject, hMemDC, hOldBitmap
    invoke DeleteObject, hPen
    invoke DeleteObject, hBrush
    invoke DeleteDC, hMemDC
    invoke ReleaseDC, NULL, hDC
    
    mov eax, hBitmap
    ret
CreateFileIcon endp

; ============================================================================
; CreateDriveIcon - Create 16x16 drive icon bitmap
; Returns: HBITMAP in eax
; ============================================================================
public CreateDriveIcon
CreateDriveIcon proc
    LOCAL hDC:DWORD
    LOCAL hMemDC:DWORD
    LOCAL hBitmap:DWORD
    LOCAL hOldBitmap:DWORD
    LOCAL hBrush:DWORD
    LOCAL hPen:DWORD
    LOCAL hOldBrush:DWORD
    LOCAL hOldPen:DWORD
    
    invoke GetDC, NULL
    mov hDC, eax
    
    invoke CreateCompatibleDC, hDC
    mov hMemDC, eax
    
    invoke CreateCompatibleBitmap, hDC, ICON_SIZE, ICON_SIZE
    mov hBitmap, eax
    
    invoke SelectObject, hMemDC, hBitmap
    mov hOldBitmap, eax
    
    ; Create drive brush (blue)
    invoke CreateSolidBrush, CLR_DRIVE_BLUE
    mov hBrush, eax
    invoke SelectObject, hMemDC, hBrush
    mov hOldBrush, eax
    
    invoke CreatePen, PS_SOLID, 1, CLR_BLACK
    mov hPen, eax
    invoke SelectObject, hMemDC, hPen
    mov hOldPen, eax
    
    ; Draw drive shape (cylinder/disk)
    invoke Ellipse, hMemDC, 4, 2, 12, 6
    invoke Rectangle, hMemDC, 4, 4, 12, 12
    invoke Ellipse, hMemDC, 4, 10, 12, 14
    
    ; Cleanup
    invoke SelectObject, hMemDC, hOldPen
    invoke SelectObject, hMemDC, hOldBrush
    invoke SelectObject, hMemDC, hOldBitmap
    invoke DeleteObject, hPen
    invoke DeleteObject, hBrush
    invoke DeleteDC, hMemDC
    invoke ReleaseDC, NULL, hDC
    
    mov eax, hBitmap
    ret
CreateDriveIcon endp

; ============================================================================
; LoadIconsIntoImageList - Load all icons into an ImageList
; Input: hImageList
; Returns: TRUE on success
; ============================================================================
public LoadIconsIntoImageList
LoadIconsIntoImageList proc hImageList:DWORD
    LOCAL hFolderBmp:DWORD
    LOCAL hFileBmp:DWORD
    LOCAL hDriveBmp:DWORD
    
    ; Create folder icon
    call CreateFolderIcon
    mov hFolderBmp, eax
    invoke ImageList_Add, hImageList, hFolderBmp, NULL
    invoke DeleteObject, hFolderBmp
    
    ; Create file icon
    call CreateFileIcon
    mov hFileBmp, eax
    invoke ImageList_Add, hImageList, hFileBmp, NULL
    invoke DeleteObject, hFileBmp
    
    ; Create drive icon
    call CreateDriveIcon
    mov hDriveBmp, eax
    invoke ImageList_Add, hImageList, hDriveBmp, NULL
    invoke DeleteObject, hDriveBmp
    
    mov eax, TRUE
    ret
LoadIconsIntoImageList endp

end
