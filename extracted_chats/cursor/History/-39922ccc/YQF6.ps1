<#
.SYNOPSIS
ELF64 Generator - Produces runnable ELF x86_64 executables
.DESCRIPTION
Creates minimal ELF64 binaries that call exit(0) syscall
#>

. "$PSScriptRoot\Backend-Interface.ps1"

function Emit-Binary {
    param(
        [string]$SIRPath,
        [string]$OutputPath,
        [string]$TargetProfile
    )
    
    Write-Host "[ELF64] Emitting minimal ELF64 -> $OutputPath"
    
    # x86_64 assembly bytes for: mov edi,0; mov eax,60; syscall
    $code = [byte[]] (0xBF,0x00,0x00,0x00,0x00, 0xB8,0x3C,0x00,0x00,0x00, 0x0F,0x05)
    
    # Program layout
    $entryVA = 0x400000 + 0x1000  # entry at start of mapped segment + file offset
    $textFileOffset = 0x1000
    $textVAddr = 0x400000 + $textFileOffset
    $textSize = $code.Length

    $fs = [System.IO.File]::Open($OutputPath, 'Create')
    $writer = New-Object System.IO.BinaryWriter($fs)

    # ELF Header (64 bytes)
    $writer.Write([byte[]](0x7F,0x45,0x4C,0x46)) # 0x7F 'E' 'L' 'F'
    $writer.Write([byte]2)                       # EI_CLASS = ELFCLASS64
    $writer.Write([byte]1)                       # EI_DATA = little endian
    $writer.Write([byte]1)                       # EI_VERSION
    $writer.Write([byte]0)                       # EI_OSABI
    $writer.Write([byte]0)                       # EI_ABIVERSION
    $writer.Write((0..7 | ForEach-Object {0}))   # padding (8 bytes)

    # e_type (ET_EXEC) = 2, e_machine (EM_X86_64)=0x3E, e_version=1
    Write-LE16 $writer 2
    Write-LE16 $writer 0x3E
    Write-LE32 $writer 1

    # e_entry
    Write-LE64 $writer ([uint64]$entryVA)
    # e_phoff (program header offset) = 0x40
    Write-LE64 $writer 0x40
    # e_shoff = 0 (no section headers)
    Write-LE64 $writer 0
    # e_flags
    Write-LE32 $writer 0
    # e_ehsize = 64, e_phentsize = 56, e_phnum = 1
    Write-LE16 $writer 64
    Write-LE16 $writer 56
    Write-LE16 $writer 1
    # e_shentsize, e_shnum, e_shstrndx
    Write-LE16 $writer 0
    Write-LE16 $writer 0
    Write-LE16 $writer 0

    # Program Header (PT_LOAD)
    # p_type = 1 (PT_LOAD)
    Write-LE32 $writer 1
    # p_flags = 5 (R + X)
    Write-LE32 $writer 5
    # p_offset (file offset)
    Write-LE64 $writer $textFileOffset
    # p_vaddr
    Write-LE64 $writer $textVAddr
    # p_paddr
    Write-LE64 $writer $textVAddr
    # p_filesz
    Write-LE64 $writer ([uint64]$textSize)
    # p_memsz
    Write-LE64 $writer ([uint64]$textSize)
    # p_align
    Write-LE64 $writer 0x1000

    # pad up to file offset 0x1000
    $current = $writer.BaseStream.Position
    if ($current -gt $textFileOffset) { throw "Header too large" }
    $pad = $textFileOffset - $current
    if ($pad -gt 0) { $writer.Write((0..($pad-1) | ForEach-Object {0})) }

    # write code bytes
    $writer.Write($code)

    $writer.Flush()
    $writer.Close()
    Write-Host "[ELF64] Wrote $OutputPath ($textSize bytes code)."
    Write-Host "Run with: chmod +x $OutputPath && ./$OutputPath"
}

Export-ModuleMember -Function Emit-Binary
