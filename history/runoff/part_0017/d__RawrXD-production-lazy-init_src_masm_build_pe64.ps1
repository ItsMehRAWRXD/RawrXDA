# RawrXD PE64 Builder - Wraps NASM object file with PE headers
# Usage: .\build_pe64.ps1 <nasm_object> <output_exe>

param(
    [Parameter(Mandatory=$true)]
    [string]$ObjFile,
    
    [Parameter(Mandatory=$true)]
    [string]$Output
)

function Emit-DWord {
    param([uint32]$Value)
    [BitConverter]::GetBytes($Value)
}

function Emit-QWord {
    param([uint64]$Value)
    [BitConverter]::GetBytes($Value)
}

if (-not (Test-Path $ObjFile)) {
    Write-Error "Object file not found: $ObjFile"
    exit 1
}

Write-Host "📦 Reading object file: $ObjFile" -ForegroundColor Cyan
$objData = [System.IO.File]::ReadAllBytes($ObjFile)
$objSize = $objData.Length

Write-Host "📝 Building PE64 headers..."

$pe = New-Object System.Collections.ArrayList

# DOS Header (64 bytes)
$dosHeader = New-Object byte[] 64
$dosHeader[0] = 0x4D  # 'M'
$dosHeader[1] = 0x5A  # 'Z'
# PE offset at 0x3C (little-endian 0x40)
[BitConverter]::GetBytes(0x40) | ForEach-Object -Begin { $i = 0x3C } { $dosHeader[$i++] = $_ }
$null = $pe.AddRange($dosHeader)

# Padding to offset 0x40 (already at 0x40)

# PE Signature
$null = $pe.AddRange([System.Text.Encoding]::ASCII.GetBytes("PE`0`0"))

# COFF Header (20 bytes)
$null = $pe.AddRange((Emit-DWord 0x8664))      # Machine: x64
$null = $pe.AddRange((Emit-DWord 0x0001))      # Number of sections: 1
$null = $pe.AddRange((Emit-DWord 0x00000000))  # Timestamp
$null = $pe.AddRange((Emit-DWord 0x00000000))  # Symbol table pointer
$null = $pe.AddRange((Emit-DWord 0x00000000))  # Number of symbols
$null = $pe.AddRange((Emit-DWord 0x00F0))      # Size of optional header: 240
$null = $pe.AddRange((Emit-DWord 0x0022))      # Characteristics: EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE

# Optional Header (PE32+, 240 bytes)
$null = $pe.AddRange((Emit-DWord 0x020B))      # Magic: PE32+
$null = $pe.AddRange((Emit-DWord 0x0E00))      # Linker version: 14.0
$null = $pe.AddRange((Emit-DWord $objSize))    # Size of code
$null = $pe.AddRange((Emit-DWord 0x00000000))  # Size of initialized data
$null = $pe.AddRange((Emit-DWord 0x00000000))  # Size of uninitialized data
$null = $pe.AddRange((Emit-DWord 0x00001000)) # Address of entry point (RVA)
$null = $pe.AddRange((Emit-DWord 0x00001000)) # Base of code

# Image base (64-bit)
$null = $pe.AddRange((Emit-QWord 0x0000000140000000L))

# Alignment & sizes
$null = $pe.AddRange((Emit-DWord 0x00001000)) # Section alignment
$null = $pe.AddRange((Emit-DWord 0x00000200)) # File alignment
$null = $pe.AddRange((Emit-DWord 0x00060000)) # OS version
$null = $pe.AddRange((Emit-DWord 0x00000000)) # Image version
$null = $pe.AddRange((Emit-DWord 0x00060000)) # Subsystem version
$null = $pe.AddRange((Emit-DWord 0x00000000)) # Win32 version value

# Image size (page-aligned)
$imageSize = 0x2000 + (([Math]::Ceiling($objSize / 0x1000)) * 0x1000)
$null = $pe.AddRange((Emit-DWord $imageSize))

$null = $pe.AddRange((Emit-DWord 0x00000200)) # Size of headers
$null = $pe.AddRange((Emit-DWord 0x00000000)) # Checksum
$null = $pe.AddRange((Emit-DWord 0x0003))     # Subsystem: CONSOLE
$null = $pe.AddRange((Emit-DWord 0x0000))     # DLL characteristics

# Stack & Heap sizes
$null = $pe.AddRange((Emit-QWord 0x0000000000100000L)) # Stack reserve
$null = $pe.AddRange((Emit-QWord 0x0000000000001000L)) # Stack commit
$null = $pe.AddRange((Emit-QWord 0x0000000000100000L)) # Heap reserve
$null = $pe.AddRange((Emit-QWord 0x0000000000001000L)) # Heap commit

$null = $pe.AddRange((Emit-DWord 0x00000000)) # Loader flags
$null = $pe.AddRange((Emit-DWord 0x00000010)) # Number of RVA and sizes

# Data directories (16 entries, all null)
for ($i = 0; $i -lt 16; $i++) {
    $null = $pe.AddRange((Emit-DWord 0x00000000))
    $null = $pe.AddRange((Emit-DWord 0x00000000))
}

# Section Header (.text - 40 bytes)
$null = $pe.AddRange([System.Text.Encoding]::ASCII.GetBytes(".text`0`0`0"))
$null = $pe.AddRange((Emit-DWord $objSize))         # Virtual size
$null = $pe.AddRange((Emit-DWord 0x00001000))       # Virtual address
$rawSize = [Math]::Ceiling($objSize / 0x200) * 0x200
$null = $pe.AddRange((Emit-DWord $rawSize))         # Size of raw data
$null = $pe.AddRange((Emit-DWord 0x00000200))       # Pointer to raw data
$null = $pe.AddRange((Emit-DWord 0x00000000))       # Pointer to relocations
$null = $pe.AddRange((Emit-DWord 0x00000000))       # Pointer to line numbers
$null = $pe.AddRange((Emit-DWord 0x0000))           # Number of relocations
$null = $pe.AddRange((Emit-DWord 0x0000))           # Number of line numbers
$null = $pe.AddRange((Emit-DWord 0x60000020))       # Characteristics: CODE | EXECUTE | READ

# Pad headers to 0x200 boundary
while ($pe.Count -lt 0x200) {
    $null = $pe.Add(0)
}

# Add code section
$null = $pe.AddRange($objData)

# Pad to next 0x200 boundary
while ($pe.Count % 0x200 -ne 0) {
    $null = $pe.Add(0)
}

# Write output
$outputBytes = [byte[]]$pe
[System.IO.File]::WriteAllBytes($Output, $outputBytes)

Write-Host "✅ Generated: $Output" -ForegroundColor Green
Write-Host "   Size: $('{0:N0}' -f $outputBytes.Length) bytes" -ForegroundColor Green
Write-Host "   Entry: 0x140001000" -ForegroundColor Green

exit 0
