# ============================================================================
# Pure MASM x64 Converter - Remove ALL SDK Dependencies
# Converts 32-bit MASM with includes to self-contained x64 MASM
# ============================================================================

param(
    [string]$SourceDir = "C:\Users\HiH8e\OneDrive\Desktop\RawrXD-production-lazy-init\masm_ide\src",
    [string]$OutputDir = "D:\temp\RawrXD-agentic-ide-production\src\masm_pure",
    [switch]$Force = $false
)

Write-Host "=== Pure MASM x64 Converter ===" -ForegroundColor Cyan
Write-Host "Source: $SourceDir"
Write-Host "Output: $OutputDir"

# Create output directory
if (!(Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

# Win32 API declarations (inline, no SDK needed)
$Win32APIHeader = @"
; ============================================================================
; WIN32 API DECLARATIONS - Self-Contained (No SDK Required)
; Pure x64 MASM - No TYPEDEF (not supported by ml64)
; ============================================================================

; Constants
NULL                equ 0
TRUE                equ 1
FALSE               equ 0
INVALID_HANDLE_VALUE equ -1

; File Access Modes
GENERIC_READ        equ 80000000h
GENERIC_WRITE       equ 40000000h
FILE_SHARE_READ     equ 00000001h
FILE_SHARE_WRITE    equ 00000002h
OPEN_EXISTING       equ 3
CREATE_ALWAYS       equ 2

; Memory Flags
MEM_COMMIT          equ 00001000h
MEM_RESERVE         equ 00002000h
MEM_RELEASE         equ 00008000h
PAGE_READWRITE      equ 04h

; Message Box
MB_OK               equ 0
MB_ICONERROR        equ 10h
MB_ICONINFORMATION  equ 40h

; Window Styles
WS_OVERLAPPEDWINDOW equ 00CF0000h
WS_VISIBLE          equ 10000000h
CW_USEDEFAULT       equ 80000000h

; Windows Messages
WM_DESTROY          equ 0002h
WM_CLOSE            equ 0010h
WM_COMMAND          equ 0111h

; ============================================================================
; KERNEL32.DLL EXPORTS
; ============================================================================
EXTERN GetModuleHandleA:PROC
EXTERN GetProcAddress:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetLastError:PROC
EXTERN VirtualAlloc:PROC
EXTERN VirtualFree:PROC
EXTERN Sleep:PROC
EXTERN ExitProcess:PROC
EXTERN lstrlenA:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC

; ============================================================================
; USER32.DLL EXPORTS
; ============================================================================
EXTERN MessageBoxA:PROC
EXTERN CreateWindowExA:PROC
EXTERN ShowWindow:PROC
EXTERN UpdateWindow:PROC
EXTERN GetMessageA:PROC
EXTERN TranslateMessage:PROC
EXTERN DispatchMessageA:PROC
EXTERN DefWindowProcA:PROC
EXTERN PostQuitMessage:PROC
EXTERN RegisterClassExA:PROC
EXTERN LoadIconA:PROC
EXTERN LoadCursorA:PROC
EXTERN GetDC:PROC
EXTERN ReleaseDC:PROC

"@

# Core agentic files to convert (priority order)
$CoreFiles = @(
    "ide_master_integration.asm",
    "agentic_ide_full_control.asm",
    "autonomous_browser_agent.asm",
    "model_hotpatch_engine.asm",
    "gguf_loader_unified.asm",
    "gguf_chain_loader_unified.asm",
    "inference_backend_selector.asm",
    "tool_dispatcher_complete.asm",
    "tool_registry_full.asm",
    "agent_system_core.asm",
    "autonomous_agent_system.asm",
    "action_executor_enhanced.asm",
    "error_logging_enhanced.asm",
    "qt_ide_integration.asm",
    "qt_pane_system.asm",
    "ui_gguf_integration.asm",
    "piram_compress.asm",
    "reverse_quant.asm"
)

function Convert-To-PureMASM {
    param([string]$InputFile, [string]$OutputFile)
    
    Write-Host "Converting: $(Split-Path $InputFile -Leaf)" -ForegroundColor Yellow
    
    if (!(Test-Path $InputFile)) {
        Write-Host "  ⚠ Source file not found, skipping" -ForegroundColor Gray
        return
    }
    
    $content = Get-Content $InputFile -Raw
    
    # Step 1: Remove all include directives
    $content = $content -replace '(?m)^include\s+.*$', ''
    $content = $content -replace '(?m)^includelib\s+.*$', ''
    
    # Step 2: Convert .386 → x64
    $content = $content -replace '\.386', '; x64 build'
    $content = $content -replace '\.686', '; x64 build'
    
    # Step 3: Remove .model directive (not needed in x64)
    $content = $content -replace '(?m)^\.model\s+.*$', '; x64 flat memory model'
    
    # Step 4: Preserve option casemap
    # (already correct)
    
    # Step 5: Convert 32-bit registers to 64-bit where appropriate
    # (Manual review needed, don't auto-convert - can break code)
    
    # Step 6: Add API header
    $finalContent = @"
; ============================================================================
; CONVERTED TO PURE X64 MASM - NO SDK DEPENDENCIES
; Original: $(Split-Path $InputFile -Leaf)
; Conversion Date: $(Get-Date -Format "yyyy-MM-dd HH:mm")
; ============================================================================

$Win32APIHeader

; ============================================================================
; ORIGINAL CODE (SDK references removed)
; ============================================================================

$content
"@
    
    # Write output
    $finalContent | Out-File $OutputFile -Encoding ASCII
    Write-Host "  ✅ Converted → $(Split-Path $OutputFile -Leaf)" -ForegroundColor Green
}

# Convert all core files
$convertedCount = 0
foreach ($file in $CoreFiles) {
    $sourcePath = Join-Path $SourceDir $file
    $outputPath = Join-Path $OutputDir $file
    
    if (Test-Path $sourcePath) {
        Convert-To-PureMASM -InputFile $sourcePath -OutputFile $outputPath
        $convertedCount++
    } else {
        Write-Host "⚠ Not found: $file" -ForegroundColor Gray
    }
}

Write-Host ""
Write-Host "=== Conversion Complete ===" -ForegroundColor Green
Write-Host "Converted: $convertedCount files"
Write-Host "Output: $OutputDir"
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "1. Review register usage (eax→rax, esp→rsp where needed)"
Write-Host "2. Update stack alignment (16-byte boundary for x64)"
Write-Host "3. Update calling convention (RCX, RDX, R8, R9 for parameters)"
Write-Host "4. Build with: ml64 /c /nologo /Zi /Fo<output>.obj <input>.asm"
