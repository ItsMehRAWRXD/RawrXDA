; ============================================================================
; RawrXD IDE - Project Management System
; Multi-file projects with build configurations
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; ============================================================================
; CONSTANTS
; ============================================================================

MAX_PROJECT_NAME        equ 128
MAX_PROJECT_PATH        equ 260
MAX_PROJECT_FILES       equ 100
MAX_BUILD_CONFIGS       equ 10
MAX_INCLUDE_PATHS       equ 20

; Project file types
FILETYPE_ASM            equ 1
FILETYPE_INC            equ 2
FILETYPE_RC             equ 3
FILETYPE_LIB            equ 4
FILETYPE_OBJ            equ 5

; Build targets
BUILD_TARGET_EXE        equ 1
BUILD_TARGET_DLL        equ 2
BUILD_TARGET_LIB        equ 3

; ============================================================================
; STRUCTURES
; ============================================================================

PROJECT_FILE struct
    filePath            db MAX_PROJECT_PATH dup(?)
    fileName            db 64 dup(?)
    fileType            dd ?
    isActive            dd ?
    compileOrder        dd ?
    lastModified        FILETIME <>
PROJECT_FILE ends

BUILD_CONFIG struct
    configName          db 64 dup(?)
    targetType          dd ?
    outputPath          db MAX_PROJECT_PATH dup(?)
    outputName          db 128 dup(?)
    debugInfo           dd ?
    optimize            dd ?
    defines             db 512 dup(?)
    includePaths        db 1024 dup(?)
    libPaths            db 1024 dup(?)
    additionalOptions   db 256 dup(?)
BUILD_CONFIG ends

PROJECT struct
    projectName         db MAX_PROJECT_NAME dup(?)
    projectPath         db MAX_PROJECT_PATH dup(?)
    projectFile         db MAX_PROJECT_PATH dup(?)
    fileCount           dd ?
    files               PROJECT_FILE MAX_PROJECT_FILES dup(<>)
    configCount         dd ?
    configs             BUILD_CONFIG MAX_BUILD_CONFIGS dup(<>)
    activeConfig        dd ?
    isDirty             dd ?
    lastSaved           FILETIME <>
PROJECT ends

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    ; Current project
    currentProject      PROJECT <>
    projectLoaded       dd 0
    
    ; File extension filters
    asmExtension        db ".asm", 0
    incExtension        db ".inc", 0
    rcExtension         db ".rc", 0
    libExtension        db ".lib", 0
    objExtension        db ".obj", 0
    
    ; Project file extension
    projectExtension    db ".rawproj", 0
    
    ; Default config names
    szDebugConfig       db "Debug", 0
    szReleaseConfig     db "Release", 0
    
    ; Strings
    szBackslash         db "\\", 0
    szNewProjectName    db "NewProject", 0
    szDefaultOutput     db "output.exe", 0
    szProjectCreated    db "Project created successfully", 0
    szProjectSaved      db "Project saved", 0
    szProjectLoaded     db "Project loaded", 0
    szFileAdded         db "File added to project", 0
    szFileRemoved       db "File removed from project", 0
    
    ; Temporary buffer
    tempBuffer          db 1024 dup(0)

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; Forward declarations
InitializeProjectSystem proto
CreateNewProject proto :DWORD, :DWORD
LoadProject proto :DWORD
SaveProject proto
CloseProject proto
AddFileToProject proto :DWORD
RemoveFileFromProject proto :DWORD
GetProjectFile proto :DWORD
CreateBuildConfig proto :DWORD, :DWORD
SetActiveBuildConfig proto :DWORD
GetBuildConfig proto :DWORD
IsProjectLoaded proto
MarkProjectDirty proto

public InitializeProjectSystem
public CreateNewProject
public LoadProject
public SaveProject
public CloseProject
public AddFileToProject
public RemoveFileFromProject
public GetProjectFile
public IsProjectLoaded

; ============================================================================
; InitializeProjectSystem - Setup project management
; ============================================================================
InitializeProjectSystem proc
    ; Clear current project
    invoke RtlZeroMemory, offset currentProject, sizeof PROJECT
    mov projectLoaded, 0
    
    mov eax, 1
    ret
InitializeProjectSystem endp

; ============================================================================
; CreateNewProject - Create a new project
; Input: lpProjectName = pointer to project name
;        lpProjectPath = pointer to project directory
; Returns: EAX = 1 if successful
; ============================================================================
CreateNewProject proc lpProjectName:DWORD, lpProjectPath:DWORD
    LOCAL hFile:HANDLE
    
    ; Clear current project structure
    invoke RtlZeroMemory, offset currentProject, sizeof PROJECT
    
    ; Copy project name
    invoke lstrcpy, offset currentProject.projectName, lpProjectName
    
    ; Copy project path
    invoke lstrcpy, offset currentProject.projectPath, lpProjectPath
    
    ; Build full project file path (path + name + .rawproj)
    invoke lstrcpy, offset currentProject.projectFile, lpProjectPath
    invoke lstrcat, offset currentProject.projectFile, offset szBackslash
    invoke lstrcat, offset currentProject.projectFile, lpProjectName
    invoke lstrcat, offset currentProject.projectFile, offset projectExtension
    
    ; Initialize counts
    mov currentProject.fileCount, 0
    mov currentProject.configCount, 0
    mov currentProject.activeConfig, 0
    mov currentProject.isDirty, 1
    
    ; Create default Debug configuration
    invoke CreateBuildConfig, offset szDebugConfig, BUILD_TARGET_EXE
    
    ; Create default Release configuration
    invoke CreateBuildConfig, offset szReleaseConfig, BUILD_TARGET_EXE
    
    ; Get current time
    invoke GetSystemTimeAsFileTime, addr currentProject.lastSaved
    
    ; Save project file
    invoke SaveProject
    
    ; Mark as loaded
    mov projectLoaded, 1
    
    mov eax, 1
    ret

CreateNewProject endp

; ============================================================================
; LoadProject - Load project from file
; Input: lpProjectFilePath = pointer to .rawproj file path
; Returns: EAX = 1 if successful
; ============================================================================
LoadProject proc lpProjectFilePath:DWORD
    LOCAL hFile:HANDLE
    LOCAL bytesRead:DWORD
    
    ; Open project file
    invoke CreateFile, lpProjectFilePath, GENERIC_READ, FILE_SHARE_READ, \
           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    mov hFile, eax
    
    .if eax == INVALID_HANDLE_VALUE
        xor eax, eax
        ret
    .endif
    
    ; Read project structure
    invoke ReadFile, hFile, offset currentProject, sizeof PROJECT, \
           addr bytesRead, NULL
    
    ; Close file
    invoke CloseHandle, hFile
    
    .if bytesRead == sizeof PROJECT
        mov projectLoaded, 1
        mov currentProject.isDirty, 0
        mov eax, 1
    .else
        xor eax, eax
    .endif
    
    ret
LoadProject endp

; ============================================================================
; SaveProject - Save current project to file
; Returns: EAX = 1 if successful
; ============================================================================
SaveProject proc
    LOCAL hFile:HANDLE
    LOCAL bytesWritten:DWORD
    
    .if projectLoaded == 0
        xor eax, eax
        ret
    .endif
    
    ; Update last saved time
    invoke GetSystemTimeAsFileTime, addr currentProject.lastSaved
    
    ; Create/overwrite project file
    invoke CreateFile, offset currentProject.projectFile, GENERIC_WRITE, \
           0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    mov hFile, eax
    
    .if eax == INVALID_HANDLE_VALUE
        xor eax, eax
        ret
    .endif
    
    ; Write project structure
    invoke WriteFile, hFile, offset currentProject, sizeof PROJECT, \
           addr bytesWritten, NULL
    
    ; Close file
    invoke CloseHandle, hFile
    
    ; Clear dirty flag
    mov currentProject.isDirty, 0
    
    mov eax, 1
    ret
SaveProject endp

; ============================================================================
; CloseProject - Close current project
; Returns: EAX = 1 if successful
; ============================================================================
CloseProject proc
    .if projectLoaded == 0
        mov eax, 1
        ret
    .endif
    
    ; Check if project is dirty
    .if currentProject.isDirty == 1
        ; Prompt to save (simplified - just save)
        invoke SaveProject
    .endif
    
    ; Clear project
    invoke RtlZeroMemory, offset currentProject, sizeof PROJECT
    mov projectLoaded, 0
    
    mov eax, 1
    ret
CloseProject endp

; ============================================================================
; AddFileToProject - Add file to project
; Input: lpFilePath = pointer to file path
; Returns: EAX = file index if successful, -1 if failed
; ============================================================================
AddFileToProject proc lpFilePath:DWORD
    LOCAL fileType:DWORD
    LOCAL pFile:DWORD
    LOCAL fileName[64]:BYTE
    
    .if projectLoaded == 0
        mov eax, -1
        ret
    .endif
    
    ; Check if project is full
    mov eax, currentProject.fileCount
    .if eax >= MAX_PROJECT_FILES
        mov eax, -1
        ret
    .endif
    
    ; Determine file type from extension
    mov fileType, FILETYPE_ASM  ; Default to ASM
    
    ; Get file name from path
    ; Simplified: just use full path
    
    ; Get pointer to next file slot
    mov eax, currentProject.fileCount
    mov ecx, sizeof PROJECT_FILE
    mul ecx
    lea edx, currentProject.files
    add edx, eax
    mov pFile, edx
    
    ; Fill in file info
    mov eax, pFile
    invoke lstrcpy, eax, lpFilePath  ; filePath
    
    add eax, MAX_PROJECT_PATH
    invoke lstrcpy, eax, lpFilePath  ; fileName (simplified)
    
    add eax, 64
    mov ecx, fileType
    mov [eax], ecx  ; fileType
    
    add eax, 4
    mov dword ptr [eax], 1  ; isActive
    
    add eax, 4
    mov ecx, currentProject.fileCount
    mov [eax], ecx  ; compileOrder
    
    ; Increment file count
    inc currentProject.fileCount
    
    ; Mark project as dirty
    call MarkProjectDirty
    
    ; Return file index
    mov eax, currentProject.fileCount
    dec eax
    ret
AddFileToProject endp

; ============================================================================
; RemoveFileFromProject - Remove file from project
; Input: fileIndex = index of file to remove
; Returns: EAX = 1 if successful
; ============================================================================
RemoveFileFromProject proc fileIndex:DWORD
    LOCAL i:DWORD
    LOCAL pSrc:DWORD
    LOCAL pDest:DWORD
    
    .if projectLoaded == 0
        xor eax, eax
        ret
    .endif
    
    ; Validate index
    mov eax, fileIndex
    .if eax >= currentProject.fileCount
        xor eax, eax
        ret
    .endif
    
    ; Shift remaining files down
    mov eax, fileIndex
    inc eax
    mov i, eax
    
    @@ShiftLoop:
        mov eax, i
        .if eax >= currentProject.fileCount
            jmp @ShiftDone
        .endif
        
        ; Calculate source and destination pointers
        mov eax, i
        mov ecx, sizeof PROJECT_FILE
        mul ecx
        lea edx, currentProject.files
        add edx, eax
        mov pSrc, edx
        
        mov eax, i
        dec eax
        mul ecx
        lea edx, currentProject.files
        add edx, eax
        mov pDest, edx
        
        ; Copy file entry
        push sizeof PROJECT_FILE
        push pSrc
        push pDest
        call RtlMoveMemory
        
        inc i
        jmp @@ShiftLoop
    
    @ShiftDone:
    ; Decrement file count
    dec currentProject.fileCount
    
    ; Mark project as dirty
    call MarkProjectDirty
    
    mov eax, 1
    ret
RemoveFileFromProject endp

; ============================================================================
; GetProjectFile - Get project file by index
; Input: fileIndex = index of file
; Returns: EAX = pointer to PROJECT_FILE or 0 if invalid
; ============================================================================
GetProjectFile proc fileIndex:DWORD
    mov eax, fileIndex
    .if eax >= currentProject.fileCount
        xor eax, eax
        ret
    .endif
    
    mov ecx, sizeof PROJECT_FILE
    mul ecx
    lea edx, currentProject.files
    add eax, edx
    
    ret
GetProjectFile endp

; ============================================================================
; CreateBuildConfig - Create new build configuration
; Input: lpConfigName = pointer to config name
;        targetType = BUILD_TARGET_xxx constant
; Returns: EAX = config index if successful
; ============================================================================
CreateBuildConfig proc lpConfigName:DWORD, targetType:DWORD
    LOCAL pConfig:DWORD
    
    ; Check if configs full
    mov eax, currentProject.configCount
    .if eax >= MAX_BUILD_CONFIGS
        mov eax, -1
        ret
    .endif
    
    ; Get pointer to next config slot
    mov ecx, sizeof BUILD_CONFIG
    mul ecx
    lea edx, currentProject.configs
    add edx, eax
    mov pConfig, edx
    
    ; Fill in config
    mov eax, pConfig
    invoke lstrcpy, eax, lpConfigName
    
    add eax, 64
    mov ecx, targetType
    mov [eax], ecx
    
    ; Set defaults
    add eax, 4
    invoke lstrcpy, eax, offset szDefaultOutput
    
    ; Increment config count
    inc currentProject.configCount
    
    ; Return config index
    mov eax, currentProject.configCount
    dec eax
    ret
CreateBuildConfig endp

; ============================================================================
; SetActiveBuildConfig - Set active build configuration
; Input: configIndex = index of configuration
; Returns: EAX = 1 if successful
; ============================================================================
SetActiveBuildConfig proc configIndex:DWORD
    mov eax, configIndex
    .if eax >= currentProject.configCount
        xor eax, eax
        ret
    .endif
    
    mov currentProject.activeConfig, eax
    call MarkProjectDirty
    
    mov eax, 1
    ret
SetActiveBuildConfig endp

; ============================================================================
; GetBuildConfig - Get build configuration by index
; Input: configIndex = index of configuration
; Returns: EAX = pointer to BUILD_CONFIG or 0 if invalid
; ============================================================================
GetBuildConfig proc configIndex:DWORD
    mov eax, configIndex
    .if eax >= currentProject.configCount
        xor eax, eax
        ret
    .endif
    
    mov ecx, sizeof BUILD_CONFIG
    mul ecx
    lea edx, currentProject.configs
    add eax, edx
    
    ret
GetBuildConfig endp

; ============================================================================
; IsProjectLoaded - Check if project is loaded
; Returns: EAX = 1 if project loaded
; ============================================================================
IsProjectLoaded proc
    mov eax, projectLoaded
    ret
IsProjectLoaded endp

; ============================================================================
; MarkProjectDirty - Mark project as modified
; ============================================================================
MarkProjectDirty proc
    mov currentProject.isDirty, 1
    ret
MarkProjectDirty endp

end