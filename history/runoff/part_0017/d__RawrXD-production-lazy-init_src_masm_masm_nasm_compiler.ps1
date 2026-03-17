#!/usr/bin/env pwsh
<#
RawrXD MASM Minimal Compiler - NASM-based PE64 Generator
Converts simple MASM assembly to x64 PE executables
Requires: NASM 2.16+

Usage: .\masm_nasm_compiler.ps1 -Input input.asm -Output output.exe
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$Input,
    
    [Parameter(Mandatory=$true)]
    [string]$Output
)

function Write-Status {
    param([string]$Message)
    Write-Host "⚙️  $Message" -ForegroundColor Cyan
}

function Write-Success {
    param([string]$Message)
    Write-Host "✅ $Message" -ForegroundColor Green
}

function Write-Error {
    param([string]$Message)
    Write-Host "❌ $Message" -ForegroundColor Red
}

# Verify input exists
if (-not (Test-Path $Input)) {
    Write-Error "Input file not found: $Input"
    exit 1
}

Write-Status "Reading source: $Input"
$source = Get-Content $Input -Raw
$sourceSize = (Get-Item $Input).Length
Write-Host "   Size: $sourceSize bytes"

# Create temp NASM-compatible source
$tempNasm = [System.IO.Path]::GetTempFileName() + ".asm"
$tempObj = [System.IO.Path]::GetTempFileName() + ".o"

Write-Status "Converting MASM → NASM syntax..."

# Simple MASM to NASM conversion
$nasmSource = @"
; RawrXD NASM PE64 Output
default rel

BITS 64

section .text
    align 16, db 0x90
"@

# Parse MASM source line by line
$lines = $source -split "`n"
$inCode = $false
$inData = $false

foreach ($line in $lines) {
    $line = $line.Trim()
    
    # Skip empty lines and comments
    if (-not $line -or $line.StartsWith(";")) {
        continue
    }
    
    # Handle directives
    if ($line -match "^\.CODE|^\.code") {
        $inCode = $true
        $nasmSource += "`n"
        continue
    }
    
    if ($line -match "^\.DATA|^\.data") {
        $inData = $true
        $nasmSource += "`nsection .data`n"
        continue
    }
    
    if ($line -match "^\.ENDS|^\.ENDP|^END") {
        continue
    }
    
    # Convert procedure labels
    if ($line -match "^(\w+)\s+PROC") {
        $procName = $matches[1]
        $nasmSource += "`n${procName}:`n"
        continue
    }
    
    # Convert labels (label:)
    if ($line -match "^(\w+)\s*:") {
        $labelName = $matches[1]
        $nasmSource += "`n${labelName}:`n"
        continue
    }
    
    # Pass through instructions (NASM syntax is compatible)
    if ($line -match "^\s*(MOV|XOR|ADD|SUB|RET|PUSH|POP|CALL|JMP|RES|INT3|NOP)" -or 
        $line -match "^\s*(mov|xor|add|sub|ret|push|pop|call|jmp|nop)") {
        # Convert to lowercase for consistency
        $line = $line -replace '([A-Z]+)(?=\s|$)', { $args[0].Value.ToLower() }
        $nasmSource += "    $line`n"
        continue
    }
    
    # Data declarations (db, dw, dd, dq)
    if ($line -match "^\s*(db|dw|dd|dq|TIMES)\s") {
        $nasmSource += "    $line`n"
        continue
    }
}

# Add entry point marker
$nasmSource += @"

; Exit via Windows API (simplified - just return 0)
    align 16

    global main
main:
    xor eax, eax
    ret
"@

# Write NASM source
Set-Content -Path $tempNasm -Value $nasmSource -Encoding ASCII

Write-Status "Assembling with NASM..."
nasm -f win64 -o $tempObj $tempNasm 2>&1 | ForEach-Object { Write-Host "   $_" }

if ($LASTEXITCODE -ne 0) {
    Write-Error "NASM assembly failed"
    Remove-Item $tempNasm, $tempObj -ErrorAction SilentlyContinue
    exit 1
}

Write-Success "Object file generated: $(Get-Item $tempObj | Select-Object -ExpandProperty Length) bytes"

# Read object file
$objData = [System.IO.File]::ReadAllBytes($tempObj)

# Build PE64 header (minimal valid executable)
$dos = New-Object byte[] 64
$dos[0] = 0x4D  # 'M'
$dos[1] = 0x5A  # 'Z'
# PE offset at 0x3C
[BitConverter]::GetBytes(0x40) | ForEach-Object -Begin { $pos = 0x3C } { $dos[$pos++] = $_ }

# PE signature at 0x40
$pe = New-Object System.Collections.ArrayList
$null = $pe.AddRange($dos)
$null = $pe.AddRange([System.Text.Encoding]::ASCII.GetBytes("PE`0`0"))

# COFF Header
$null = $pe.AddRange([BitConverter]::GetBytes(0x8664))     # Machine (x64)
$null = $pe.AddRange([BitConverter]::GetBytes(0x0001))     # Sections: 1
$null = $pe.AddRange([BitConverter]::GetBytes(0x00000000)) # Timestamp
$null = $pe.AddRange([BitConverter]::GetBytes(0x00000000)) # Symbol table
$null = $pe.AddRange([BitConverter]::GetBytes(0x00000000)) # Num symbols
$null = $pe.AddRange([BitConverter]::GetBytes(0x00F0))     # Opt header size (240 bytes)
$null = $pe.AddRange([BitConverter]::GetBytes(0x0022))     # Characteristics

# Optional Header PE32+
$null = $pe.AddRange([BitConverter]::GetBytes(0x020B))     # Magic PE32+
$null = $pe.AddRange([BitConverter]::GetBytes(0x0E00))     # Linker version
$null = $pe.AddRange([BitConverter]::GetBytes($objData.Length)) # Code size
$null = $pe.AddRange([BitConverter]::GetBytes(0x00000000)) # Initialized data
$null = $pe.AddRange([BitConverter]::GetBytes(0x00000000)) # Uninitialized data
$null = $pe.AddRange([BitConverter]::GetBytes(0x00001000)) # Entry point (RVA)
$null = $pe.AddRange([BitConverter]::GetBytes(0x00001000)) # Base of code

# Image base (64-bit)
$null = $pe.AddRange([BitConverter]::GetBytes(0x0000000140000000L)) # 0x140000000

$null = $pe.AddRange([BitConverter]::GetBytes(0x00001000)) # Section alignment
$null = $pe.AddRange([BitConverter]::GetBytes(0x00000200)) # File alignment
$null = $pe.AddRange([BitConverter]::GetBytes(0x00060000)) # OS version
$null = $pe.AddRange([BitConverter]::GetBytes(0x00000000)) # Image version
$null = $pe.AddRange([BitConverter]::GetBytes(0x00060000)) # Subsystem version
$null = $pe.AddRange([BitConverter]::GetBytes(0x00000000)) # Win32 version

# Size of image
$imageSize = 0x2000 + (([Math]::Ceiling($objData.Length / 0x1000)) * 0x1000)
$null = $pe.AddRange([BitConverter]::GetBytes($imageSize))

$null = $pe.AddRange([BitConverter]::GetBytes(0x00000200)) # Size of headers
$null = $pe.AddRange([BitConverter]::GetBytes(0x00000000)) # Checksum
$null = $pe.AddRange([BitConverter]::GetBytes(0x0003))     # Subsystem (console)
$null = $pe.AddRange([BitConverter]::GetBytes(0x0000))     # DLL characteristics

# Stack/Heap
$null = $pe.AddRange([BitConverter]::GetBytes(0x0000000000100000L)) # Stack reserve
$null = $pe.AddRange([BitConverter]::GetBytes(0x0000000000001000L)) # Stack commit
$null = $pe.AddRange([BitConverter]::GetBytes(0x0000000000100000L)) # Heap reserve
$null = $pe.AddRange([BitConverter]::GetBytes(0x0000000000001000L)) # Heap commit

$null = $pe.AddRange([BitConverter]::GetBytes(0x00000000)) # Loader flags
$null = $pe.AddRange([BitConverter]::GetBytes(0x00000010)) # Data directories

# Data directories (16 empty)
1..16 | ForEach-Object {
    $null = $pe.AddRange([BitConverter]::GetBytes(0x00000000))
    $null = $pe.AddRange([BitConverter]::GetBytes(0x00000000))
}

# Section header (.text)
$null = $pe.AddRange([System.Text.Encoding]::ASCII.GetBytes(".text"))
$null = $pe.AddRange([byte[]]@(0, 0, 0))  # Padding

$null = $pe.AddRange([BitConverter]::GetBytes($objData.Length))    # Virtual size
$null = $pe.AddRange([BitConverter]::GetBytes(0x00001000))         # Virtual address
$rawSize = [Math]::Ceiling($objData.Length / 0x200) * 0x200
$null = $pe.AddRange([BitConverter]::GetBytes($rawSize))           # Raw size
$null = $pe.AddRange([BitConverter]::GetBytes(0x00000200))         # Raw address

$null = $pe.AddRange([BitConverter]::GetBytes(0x00000000)) # Relocations
$null = $pe.AddRange([BitConverter]::GetBytes(0x00000000)) # Line numbers
$null = $pe.AddRange([BitConverter]::GetBytes(0x0000))     # # Relocations
$null = $pe.AddRange([BitConverter]::GetBytes(0x0000))     # # Line numbers
$null = $pe.AddRange([BitConverter]::GetBytes(0x60000020)) # Characteristics

# Pad to 0x200
while ($pe.Count -lt 0x200) {
    $null = $pe.Add(0)
}

# Add code
$null = $pe.AddRange($objData)

# Pad to next 0x200 boundary
while ($pe.Count % 0x200 -ne 0) {
    $null = $pe.Add(0)
}

# Write output
$peBytes = [byte[]]$pe
[System.IO.File]::WriteAllBytes($Output, $peBytes)

Write-Success "Generated: $Output ($('{0:N0}' -f $peBytes.Length) bytes)"
Write-Success "Entry point: 0x140001000"

# Cleanup
Remove-Item $tempNasm, $tempObj -ErrorAction SilentlyContinue

exit 0
