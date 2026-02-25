<#
.SYNOPSIS
PE32+ Generator - Produces minimal Windows executables
.DESCRIPTION
Creates valid PE32+ binaries that load and return immediately
#>

. "$PSScriptRoot\Backend-Interface.ps1"

function Emit-Binary {
    param(
        [string]$SIRPath,
        [string]$OutputPath,
        [string]$TargetProfile
    )
    
    Write-Host "[PE32+] Emitting minimal PE32+ -> $OutputPath"
    
    $fs = [System.IO.File]::Open($OutputPath, 'Create')
    $writer = New-Object System.IO.BinaryWriter($fs)

    # --- DOS header (64 bytes ---
    $writer.Write([byte[]](0x4D,0x5A))           # 'MZ'
    # Fill DOS stub with zeros
    $writer.Write((0..57 | ForEach-Object {0}))
    # e_lfanew at offset 0x3C
    $writer.Seek(0x3C, 'Begin') | Out-Null
    Write-LE32 $writer 0x80

    # Pad to PE header offset (0x80)
    $writer.Seek(0x80, 'Begin') | Out-Null

    # --- NT Headers ---
    # Signature 'PE\0\0'
    $writer.Write([byte[]](0x50,0x45,0x00,0x00))

    # COFF Header (20 bytes)
    Write-LE16 $writer 0x8664  # Machine AMD64
    Write-LE16 $writer 1       # NumberOfSections
    Write-LE32 $writer 0        # TimeDateStamp
    Write-LE32 $writer 0        # PointerToSymbolTable
    Write-LE32 $writer 0        # NumberOfSymbols
    Write-LE16 $writer 0xF0     # SizeOfOptionalHeader
    Write-LE16 $writer 0x0022  # Characteristics

    # Optional Header (PE32+)
    Write-LE16 $writer 0x20B    # Magic PE32+
    $writer.Write([byte]0x0E)   # Linker major
    $writer.Write([byte]0x00)   # Linker minor
    Write-LE32 $writer 1       # SizeOfCode
    Write-LE32 $writer 0        # SizeOfInitializedData
    Write-LE32 $writer 0        # SizeOfUninitializedData
    Write-LE32 $writer 0x1000   # AddressOfEntryPoint
    Write-LE32 $writer 0x1000   # BaseOfCode

    # Windows-specific
    Write-LE64 $writer 0x0000000140000000L  # ImageBase
    Write-LE32 $writer 0x1000   # SectionAlignment
    Write-LE32 $writer 0x200    # FileAlignment
    Write-LE16 $writer 6        # OS major
    Write-LE16 $writer 0        # OS minor
    Write-LE16 $writer 0        # Image major
    Write-LE16 $writer 0        # Image minor
    Write-LE16 $writer 3        # Subsystem: Windows CUI
    Write-LE16 $writer 0        # DllCharacteristics
    Write-LE64 $writer 0x00100000  # SizeOfStackReserve
    Write-LE64 $writer 0x00001000  # SizeOfStackCommit
    Write-LE64 $writer 0x00100000  # SizeOfHeapReserve
    Write-LE64 $writer 0x00001000  # SizeOfHeapCommit
    Write-LE32 $writer 0        # LoaderFlags
    Write-LE32 $writer 16       # NumberOfRvaAndSizes
    # Data directories (16 * 8 = 128 bytes) all zeros
    $writer.Write((0..127 | ForEach-Object {0}))

    # --- Section Header: .text ---
    $name = [byte[]](0x2E,0x74,0x65,0x78,0x74,0,0,0) # ".text\0\0\0"
    $writer.Write($name)
    Write-LE32 $writer 1        # VirtualSize
    Write-LE32 $writer 0x1000   # VirtualAddress
    Write-LE32 $writer 0x200    # SizeOfRawData
    Write-LE32 $writer 0x200    # PointerToRawData
    Write-LE32 $writer 0        # PointerToRelocations
    Write-LE32 $writer 0        # PointerToLinenumbers
    Write-LE16 $writer 0        # NumberOfRelocations
    Write-LE16 $writer 0        # NumberOfLinenumbers
    Write-LE32 $writer 0x60000020  # Characteristics

    # Align headers to FileAlignment
    if ($writer.BaseStream.Position -lt 0x200) {
        $writer.Write((0..(0x200 - $writer.BaseStream.Position - 1) | ForEach-Object {0}))
    }

    # --- .text raw data ---
    $code = [byte[]](0xC3)       # ret
    $writer.Write($code)
    # Pad to FileAlignment
    if ($writer.BaseStream.Position -lt 0x400) {
        $writer.Write((0..(0x400 - $writer.BaseStream.Position - 1) | ForEach-Object {0}))
    }

    $writer.Close()
    Write-Host "[PE32+] Wrote $OutputPath - should load and return immediately"
}

Export-ModuleMember -Function Emit-Binary
