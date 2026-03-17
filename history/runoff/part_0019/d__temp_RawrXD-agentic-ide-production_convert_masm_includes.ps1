# ============================================================================
# CONVERT_MASM_INCLUDES.PS1 - Convert MASM32 includes to Windows SDK
# Processes all .asm files in masm_agentic directory
# ============================================================================

$masmDir = "D:\temp\RawrXD-agentic-ide-production\src\masm_agentic"
$asmFiles = Get-ChildItem "$masmDir\*.asm"

Write-Host "Converting $($asmFiles.Count) MASM files to Windows SDK includes..." -ForegroundColor Cyan

foreach ($file in $asmFiles) {
    Write-Host "Processing: $($file.Name)" -ForegroundColor Yellow
    
    $content = Get-Content $file.FullName -Raw
    $original = $content
    
    # Remove 32-bit directives
    $content = $content -replace '\.386\s*[\r\n]+', ''
    $content = $content -replace '\.model\s+flat\s*,\s*stdcall\s*[\r\n]+', ''
    
    # Replace MASM32 includes with our Windows SDK include
    $content = $content -replace 'include\s+\\masm32\\include\\windows\.inc\s*[\r\n]+', ''
    $content = $content -replace 'include\s+\\masm32\\include\\kernel32\.inc\s*[\r\n]+', ''
    $content = $content -replace 'include\s+\\masm32\\include\\user32\.inc\s*[\r\n]+', ''
    $content = $content -replace 'include\s+\\masm32\\include\\wininet\.inc\s*[\r\n]+', ''
    $content = $content -replace 'include\s+\\masm32\\include\\shell32\.inc\s*[\r\n]+', ''
    
    # Replace includelib directives
    $content = $content -replace 'includelib\s+\\masm32\\lib\\kernel32\.lib\s*[\r\n]+', ''
    $content = $content -replace 'includelib\s+\\masm32\\lib\\user32\.lib\s*[\r\n]+', ''
    $content = $content -replace 'includelib\s+\\masm32\\lib\\wininet\.lib\s*[\r\n]+', ''
    $content = $content -replace 'includelib\s+\\masm32\\lib\\shell32\.lib\s*[\r\n]+', ''
    
    # Add our Windows SDK include after option casemap:none
    if ($content -match 'option casemap:none\s*[\r\n]+' -and $content -notmatch 'include windows_sdk\.inc') {
        $content = $content -replace '(option casemap:none\s*[\r\n]+)', "`$1`r`ninclude windows_sdk.inc`r`n"
    }
    
    # Fix INVOKE macros - Windows SDK uses Microsoft x64 calling convention
    # First 4 integer args go in RCX, RDX, R8, R9 (rest on stack)
    # No changes needed - our windows_sdk.inc has a simplified INVOKE
    
    if ($content -ne $original) {
        Set-Content $file.FullName $content -NoNewline
        Write-Host "  ✓ Converted" -ForegroundColor Green
    } else {
        Write-Host "  - No changes needed" -ForegroundColor Gray
    }
}

Write-Host "`nConversion complete! All MASM files now use Windows SDK includes." -ForegroundColor Green
