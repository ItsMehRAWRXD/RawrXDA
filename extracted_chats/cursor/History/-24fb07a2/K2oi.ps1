# hybrid_builder.ps1
# Uses real NASM for complex code, our assembler for simple payloads

param([string]$AsmFile = "complex_payload.asm")

$nasmPath = "nasm"
$gccPath = "gcc"

# Check if we should use real NASM (complex code) or our assembler (simple code)
$content = Get-Content $AsmFile -Raw

if ($content -match "\[.*\]" -or $content -match "GS:" -or $content -match "FS:") {
    Write-Host "[!] Complex code detected - using real NASM" -ForegroundColor Yellow

    # Use real NASM for complex code
    & $nasmPath -f win64 $AsmFile -o temp.obj
    & $gccPath temp.obj -o output.exe -nostartfiles -nostdlib

    Write-Host "[✓] Built with real NASM+GCC" -ForegroundColor Green
} else {
    Write-Host "[!] Simple code - using PowerShell assembler" -ForegroundColor Cyan
    # Use our PowerShell assembler
    .\minimal_pe_assembler_enhanced.ps1 $AsmFile
}
