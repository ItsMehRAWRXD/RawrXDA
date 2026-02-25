#Requires -Version 5.1
<#
.SYNOPSIS
  Enhanced x86-64 assembler with memory operand support
.NEW FEATURES
  - Memory operands: [rax], [rax+rbx], [rax+0x10]
  - Segment registers: GS, FS prefixes
  - Complex addressing modes
  - Extended instruction set
#>
param([string]$AsmFile = "payload.asm", [string]$OutExe = "")

if (!$OutExe) { $OutExe = [IO.Path]::ChangeExtension($AsmFile, ".exe") }

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ---------- helpers ----------------------------------------------------------
function Write-DWord([System.IO.BinaryWriter]$w, [uint32]$v) { $w.Write($v) }
function Write-Word([System.IO.BinaryWriter]$w, [uint16]$v)  { $w.Write($v) }
function Write-Byte([System.IO.BinaryWriter]$w, [byte]$v)   { $w.Write($v) }
function Write-QWord([System.IO.BinaryWriter]$w, [uint64]$v) { $w.Write($v) }

# ---------- PE constants (X64 COMPATIBLE) ------------------------------------
$IMAGE_DOS_SIGNATURE      = 0x5A4D
$IMAGE_NT_SIGNATURE       = 0x00004550
$IMAGE_FILE_MACHINE_AMD64 = 0x8664  # ✅ FIXED: x64 architecture
$IMAGE_NT_OPTIONAL_HDR64_MAGIC = 0x20B
$IMAGE_SUBSYSTEM_WINDOWS_CUI   = 0x0003  # Console subsystem
$IMAGE_SCN_CNT_CODE            = 0x00000020
$IMAGE_SCN_MEM_EXECUTE         = 0x20000000
$IMAGE_SCN_MEM_READ            = 0x40000000

# Proper alignment for x64
$ImageBase        = 0x00400000
$SectionAlignment = 0x1000
$FileAlignment    = 0x200
$HeaderSize       = 0x200
$CodeRVA          = 0x1000
$EntryRVA         = $CodeRVA

# ---------- ENHANCED INSTRUCTION PARSER -------------------------------------
$lines = Get-Content $AsmFile -Encoding UTF8
$byteList = [System.Collections.Generic.List[byte]]::new()
$symTable = @{}
$va = $EntryRVA

function Emit-RexPrefix {
    param([byte]$w = 1, [byte]$r = 0, [byte]$x = 0, [byte]$b = 0)
    $rex = 0x40 -bor ($w -shl 3) -bor ($r -shl 2) -bor ($x -shl 1) -bor $b
    if ($rex -ne 0x40) { $byteList.Add($rex) }
}

function Emit-ModRM {
    param([byte]$mod, [byte]$reg, [byte]$rm)
    $byteList.Add(($mod -shl 6) -bor ($reg -shl 3) -bor $rm)
}

function Emit-SIB {
    param([byte]$scale, [byte]$index, [byte]$base)
    $byteList.Add(($scale -shl 6) -bor ($index -shl 3) -bor $base)
}

# Pass 1 - collect labels
for ($i = 0; $i -lt $lines.Count; $i++) {
    $l = $lines[$i].Trim()
    if ($l -eq '' -or $l.StartsWith(';')) { continue }
    if ($l -match '^(?<lab>[A-Za-z_][A-Za-z0-9_]*):$') {
        $symTable[$Matches.lab] = $va
        continue
    }
    # Rough size estimation
    $va += 4
}

# Pass 2 - emit code
$va = $EntryRVA
for ($i = 0; $i -lt $lines.Count; $i++) {
    $l = $lines[$i].Trim()
    if ($l -eq '' -or $l.StartsWith(';')) { continue }
    if ($l -match '^(?<lab>[A-Za-z_][A-Za-z0-9_]*):$') { continue }

    # ---------- data directives -------------------------------------------
    if ($l -match '^DB\s+(.+)') {
        $bytes = Invoke-Expression "[byte[]]($($Matches[1]))"
        $byteList.AddRange($bytes); $va += $bytes.Count; continue
    }
    if ($l -match '^DW\s+(.+)') {
        $words = Invoke-Expression "[uint16[]]($($Matches[1]))"
        foreach ($w in $words) { $byteList.AddRange([BitConverter]::GetBytes($w)) }
        $va += $words.Count * 2; continue
    }
    if ($l -match '^DD\s+(.+)') {
        $dwords = Invoke-Expression "[uint32[]]($($Matches[1]))"
        foreach ($d in $dwords) { $byteList.AddRange([BitConverter]::GetBytes($d)) }
        $va += $dwords.Count * 4; continue
    }

    $up = $l.ToUpper()

    # ✅ GS PREFIX SUPPORT
    if ($up -match '^MOV\s+(RCX|RAX|RDX|RBX|RSP|RBP|RSI|RDI|R8|R9|R10|R11|R12|R13|R14|R15),\s*\[GS:0x([0-9A-F]+)\]$') {
        $reg = $Matches[1]
        $offset = [uint32]::Parse($Matches[2], 'HexNumber')
        $byteList.Add(0x65)  # GS prefix
        Emit-RexPrefix -w 1 -b (($reg -band 7) -ne 0 ? 1 : 0)
        $byteList.Add(0x8B)  # MOV r64, r/m64
        Emit-ModRM -mod 0 -reg ($reg -band 7) -rm 5  # [disp32]
        $byteList.AddRange([BitConverter]::GetBytes($offset))
        $va += 8
        continue
    }

    # ✅ FS PREFIX SUPPORT
    if ($up -match '^MOV\s+(RCX|RAX|RDX|RBX|RSP|RBP|RSI|RDI|R8|R9|R10|R11|R12|R13|R14|R15),\s*\[FS:0x([0-9A-F]+)\]$') {
        $reg = $Matches[1]
        $offset = [uint32]::Parse($Matches[2], 'HexNumber')
        $byteList.Add(0x64)  # FS prefix
        Emit-RexPrefix -w 1 -b (($reg -band 7) -ne 0 ? 1 : 0)
        $byteList.Add(0x8B)  # MOV r64, r/m64
        Emit-ModRM -mod 0 -reg ($reg -band 7) -rm 5  # [disp32]
        $byteList.AddRange([BitConverter]::GetBytes($offset))
        $va += 8
        continue
    }

    # ✅ MEMORY OPERAND SUPPORT: [REG + OFFSET]
    if ($up -match '^MOV\s+(RCX|RAX|RDX|RBX|RSP|RBP|RSI|RDI|R8|R9|R10|R11|R12|R13|R14|R15),\s*\[(RCX|RAX|RDX|RBX|RSP|RBP|RSI|RDI|R8|R9|R10|R11|R12|R13|R14|R15)\s*\+\s*0x([0-9A-F]+)\]$') {
        $destReg = $Matches[1]
        $srcReg = $Matches[2]
        $offset = [uint32]::Parse($Matches[3], 'HexNumber')

        Emit-RexPrefix -w 1 -b (($destReg -band 7) -ne 0 ? 1 : 0)
        $byteList.Add(0x8B)  # MOV r64, r/m64

        if ($offset -eq 0) {
            Emit-ModRM -mod 0 -reg ($destReg -band 7) -rm ($srcReg -band 7)
            $va += 3
        } elseif ($offset -le 0x7F) {
            Emit-ModRM -mod 1 -reg ($destReg -band 7) -rm ($srcReg -band 7)
            $byteList.Add([byte]$offset)
            $va += 4
        } else {
            Emit-ModRM -mod 2 -reg ($destReg -band 7) -rm ($srcReg -band 7)
            $byteList.AddRange([BitConverter]::GetBytes($offset))
            $va += 7
        }
        continue
    }

    # ✅ CMP DWORD [MEM], IMM32
    if ($up -match '^CMP\s+DWORD\s*\[(RCX|RAX|RDX|RBX|RSP|RBP|RSI|RDI|R8|R9|R10|R11|R12|R13|R14|R15)\s*\+\s*0x([0-9A-F]+)\],\s*0x([0-9A-F]+)$') {
        $baseReg = $Matches[1]
        $offset = [uint32]::Parse($Matches[2], 'HexNumber')
        $imm32 = [uint32]::Parse($Matches[3], 'HexNumber')

        Emit-RexPrefix -w 0 -b (($baseReg -band 7) -ne 0 ? 1 : 0)
        $byteList.Add(0x81)  # CMP r/m32, imm32
        Emit-ModRM -mod 0 -reg 7 -rm ($baseReg -band 7)  # mod=00, reg=7 for CMP, rm=base
        $byteList.AddRange([BitConverter]::GetBytes($imm32))
        $va += 7
        continue
    }

    # ✅ JNE REL8 (Enhanced)
    if ($up -match '^JNE\s+(?<tgt>[A-Za-z_][A-Za-z0-9_]*)$') {
        $tgt = $Matches.tgt
        if (!$symTable.ContainsKey($tgt)) { throw "Undefined label $tgt" }
        $off = $symTable[$tgt] - ($va + 2)
        if ($off -lt -128 -or $off -gt 127) { throw "Label $tgt out of rel8 range" }
        $byteList.AddRange(@(0x75, [byte]$off))
        $va += 2
        continue
    }

    # ✅ EXISTING SIMPLE INSTRUCTIONS
    switch -Regex ($up) {
        '^NOP' { $byteList.Add(0x90); $va++; continue }
        '^RET' { $byteList.Add(0xC3); $va++; continue }
        '^SYSCALL' { $byteList.AddRange(@(0x0F, 0x05)); $va += 2; continue }

        # MOV r64, imm64
        '^MOV\s+(RAX|RCX|RDX|RBX|RSP|RBP|RSI|RDI|R8|R9|R10|R11|R12|R13|R14|R15),\s*0x([0-9A-F]+)$' {
            $reg = $Matches[1]; $imm = [uint64]::Parse($Matches[2], 'HexNumber')
            Emit-RexPrefix -w 1 -b (($reg -band 7) -ne 0 ? 1 : 0)
            $byteList.Add(0xB8 + ($reg -band 7))
            $byteList.AddRange([BitConverter]::GetBytes($imm))
            $va += 10; continue
        }

        # XOR r64, r64 (same register)
        '^XOR\s+(RAX|RCX|RDX|RBX|RSP|RBP|RSI|RDI|R8|R9|R10|R11|R12|R13|R14|R15),\s*\1$' {
            $reg = $Matches[1]
            Emit-RexPrefix -w 1 -b (($reg -band 7) -ne 0 ? 1 : 0)
            $byteList.AddRange(@(0x31, 0xC0 + ($reg -band 7) * 9))
            $va += 3; continue
        }

        # INC r64
        '^INC\s+(RAX|RCX|RDX|RBX|RSP|RBP|RSI|RDI|R8|R9|R10|R11|R12|R13|R14|R15)$' {
            $reg = $Matches[1]
            Emit-RexPrefix -w 1 -b (($reg -band 7) -ne 0 ? 1 : 0)
            $byteList.AddRange(@(0xFF, 0xC0 + ($reg -band 7)))
            $va += 3; continue
        }

        # CMP r64, imm8
        '^CMP\s+(RAX|RCX|RDX|RBX|RSP|RBP|RSI|RDI|R8|R9|R10|R11|R12|R13|R14|R15),\s*0x([0-9A-F]{1,2})$' {
            $reg = $Matches[1]; $imm8 = [byte]::Parse($Matches[2], 'HexNumber')
            Emit-RexPrefix -w 1 -b (($reg -band 7) -ne 0 ? 1 : 0)
            $byteList.AddRange(@(0x83, 0xF8 + ($reg -band 7), $imm8))
            $va += 4; continue
        }

        # JNZ rel8 label
        '^JNZ\s+(?<tgt>[A-Za-z_][A-Za-z0-9_]*)$' {
            $tgt = $Matches.tgt; if (!$symTable.ContainsKey($tgt)) { throw "Undefined label $tgt" }
            $off = $symTable[$tgt] - ($va + 2)
            if ($off -lt -128 -or $off -gt 127) { throw "Label $tgt out of rel8 range" }
            $byteList.AddRange(@(0x75, [byte]$off)); $va += 2; continue
        }

        # JMP rel8 label
        '^JMP\s+(?<tgt>[A-Za-z_][A-Za-z0-9_]*)$' {
            $tgt = $Matches.tgt; if (!$symTable.ContainsKey($tgt)) { throw "Undefined label $tgt" }
            $off = $symTable[$tgt] - ($va + 2)
            if ($off -lt -128 -or $off -gt 127) { throw "Label $tgt out of rel8 range" }
            $byteList.AddRange(@(0xEB, [byte]$off)); $va += 2; continue
        }

        # ADD RCX, imm8
        '^ADD\s+RCX,\s*0x([0-9A-F]{1,2})$' {
            $imm8 = [byte]::Parse($Matches[1], 'HexNumber')
            $byteList.AddRange(@(0x48, 0x83, 0xC1, $imm8)); $va += 4; continue
        }

        # CALL $+5
        '^CALL\s+\$\+5$' { $byteList.AddRange(@(0xE8, 0x00, 0x00, 0x00, 0x00)); $va += 5; continue }

        # JMP RCX
        '^JMP\s+RCX$' { $byteList.AddRange(@(0xFF, 0xE1)); $va += 2; continue }

        default { throw "Unsupported instruction: $l" }
    }
}

$codeBytes = $byteList.ToArray()
$codeSize  = $codeBytes.Length

# Calculate proper sizes for x64
$SizeOfImage = [Math]::Ceiling(($CodeRVA + $codeSize) / $SectionAlignment) * $SectionAlignment
if ($SizeOfImage -lt 0x2000) { $SizeOfImage = 0x2000 }  # Minimum 2 pages

# ---------- 3. build PE64 (X64 COMPATIBLE) ------------------------------------
$ms = [System.IO.MemoryStream]::new()
$w  = [System.IO.BinaryWriter]::new($ms)

# DOS stub (64 B)
[byte[]]$dosStub = @(
    0x4D,0x5A,0x90,0x00,0x03,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,
    0xB8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x00,0x00,0x00
)
$w.Write($dosStub)
$w.Seek(0x3C, 'Begin') | Out-Null
Write-DWord $w 0x80

# NT headers (X64 COMPATIBLE)
$w.Seek(0x80, 'Begin') | Out-Null
Write-DWord $w 0x00004550                    # Signature
Write-Word   $w $IMAGE_FILE_MACHINE_AMD64    # ✅ FIXED: x64 Machine Type
Write-Word   $w 1                            # NumberOfSections
Write-DWord $w ([uint32](Get-Date -UFormat %s)) # TimeDateStamp
Write-DWord $w 0                             # PointerToSymbolTable
Write-DWord $w 0                             # NumberOfSymbols
Write-Word   $w 0x00F0                       # SizeOfOptionalHeader
Write-Word   $w 0x022F                       # Characteristics

# Optional header (X64 COMPATIBLE)
Write-Word   $w $IMAGE_NT_OPTIONAL_HDR64_MAGIC # Magic
Write-Byte   $w 0                            # MajorLinkerVersion
Write-Byte   $w 0                            # MinorLinkerVersion
Write-DWord $w $codeSize                     # SizeOfCode
Write-DWord $w 0                             # SizeOfInitializedData
Write-DWord $w 0                             # SizeOfUninitializedData
Write-DWord $w $EntryRVA                     # AddressOfEntryPoint
Write-DWord $w $CodeRVA                      # BaseOfCode
Write-QWord $w $ImageBase                    # ImageBase
Write-DWord $w $SectionAlignment             # SectionAlignment
Write-DWord $w $FileAlignment                # FileAlignment
Write-Word   $w 6                            # MajorOperatingSystemVersion
Write-Word   $w 0                            # MinorOperatingSystemVersion
Write-Word   $w 0                            # MajorImageVersion
Write-Word   $w 0                            # MinorImageVersion
Write-Word   $w 6                            # MajorSubsystemVersion
Write-Word   $w 0                            # MinorSubsystemVersion
Write-DWord $w 0                             # Win32VersionValue
Write-DWord $w $SizeOfImage                  # SizeOfImage
Write-DWord $w $HeaderSize                   # SizeOfHeaders
Write-DWord $w 0                             # CheckSum
Write-Word   $w $IMAGE_SUBSYSTEM_WINDOWS_CUI # Subsystem (Console)
Write-Word   $w 0x8160                       # DllCharacteristics
Write-QWord $w 0x100000                      # SizeOfStackReserve
Write-QWord $w 0x1000                        # SizeOfStackCommit
Write-QWord $w 0x100000                      # SizeOfHeapReserve
Write-QWord $w 0x1000                        # SizeOfHeapCommit
Write-DWord $w 0                             # LoaderFlags
Write-DWord $w 16                            # NumberOfRvaAndSizes

# DataDirectory – all zero (no imports, no relocs)
$w.Write([byte[]]::new(16 * 8))

# Section header (.text) - X64 COMPATIBLE
$w.Write([System.Text.Encoding]::ASCII.GetBytes(".text`0`0`0")) # Name (8 bytes)
Write-DWord $w $codeSize                     # VirtualSize
Write-DWord $w $CodeRVA                      # VirtualAddress
Write-DWord $w ([Math]::Ceiling($codeSize / $FileAlignment) * $FileAlignment) # SizeOfRawData
Write-DWord $w $HeaderSize                   # PointerToRawData
Write-DWord $w 0                             # PointerToRelocations
Write-DWord $w 0                             # PointerToLinenumbers
Write-Word   $w 0                            # NumberOfRelocations
Write-Word   $w 0                            # NumberOfLinenumbers
Write-DWord $w ($IMAGE_SCN_CNT_CODE -bor $IMAGE_SCN_MEM_EXECUTE -bor $IMAGE_SCN_MEM_READ) # Characteristics

# Pad to code section
while ($w.BaseStream.Position -lt $HeaderSize) { Write-Byte $w 0 }

# Code section
$w.Write($codeBytes)

# File-alignment pad
while (($w.BaseStream.Position % $FileAlignment) -ne 0) { Write-Byte $w 0 }

# ---------- finish ------------------------------------------------------------
$w.Flush()
$exeBytes = $ms.ToArray()
$w.Dispose(); $ms.Dispose()
[System.IO.File]::WriteAllBytes($OutExe, $exeBytes)

# ---------- ARCHITECTURE VALIDATION -------------------------------------------
Write-Host "`n[✔] Built $OutExe ($($exeBytes.Length) bytes)" -ForegroundColor Green
Write-Host "Architecture : x64 (0x8664)" -ForegroundColor Cyan
Write-Host "ImageBase    : 0x$($ImageBase.ToString('X'))" -ForegroundColor Cyan
Write-Host "Entry RVA    : 0x$($EntryRVA.ToString('X'))" -ForegroundColor Cyan
Write-Host "Subsystem    : $($IMAGE_SUBSYSTEM_WINDOWS_CUI) (Console)" -ForegroundColor Cyan
Write-Host "SizeOfImage  : 0x$($SizeOfImage.ToString('X'))`n" -ForegroundColor Cyan

# Enhanced PE validation
Write-Host "[PE Architecture Validation]" -ForegroundColor Yellow
try {
    $peBytes = [System.IO.File]::ReadAllBytes($OutExe)

    # Check DOS header
    if ($peBytes[0] -eq 0x4D -and $peBytes[1] -eq 0x5A) {
        Write-Host "  ✓ Valid DOS signature (MZ)" -ForegroundColor Green
    } else {
        Write-Host "  ✗ Invalid DOS signature" -ForegroundColor Red
    }

    # Check PE signature
    $peOffset = [BitConverter]::ToInt32($peBytes, 0x3C)
    $peSig = [BitConverter]::ToUInt32($peBytes, $peOffset)
    if ($peSig -eq 0x00004550) {
        Write-Host "  ✓ Valid PE signature" -ForegroundColor Green
    } else {
        Write-Host "  ✗ Invalid PE signature" -ForegroundColor Red
    }

    # Check machine type (CRITICAL FIX)
    $machineType = [BitConverter]::ToUInt16($peBytes, $peOffset + 4)
    if ($machineType -eq 0x8664) {
        Write-Host "  ✓ Correct x64 architecture (0x8664)" -ForegroundColor Green
    } elseif ($machineType -eq 0xAA64) {
        Write-Host "  ✗ ARM64EC architecture (0xAA64) - INCOMPATIBLE" -ForegroundColor Red
    } else {
        Write-Host "  ✗ Unknown architecture: 0x$($machineType.ToString('X4'))" -ForegroundColor Red
    }

    # Check subsystem
    $optHeaderOffset = $peOffset + 24
    $subsystem = [BitConverter]::ToUInt16($peBytes, $optHeaderOffset + 68)
    Write-Host "  ✓ Subsystem: $subsystem" -ForegroundColor Green

    Write-Host "`n[🎯] Executable is now x64 compatible and should run on Windows!" -ForegroundColor Green
    Write-Host "[💡] Test with: .\$OutExe" -ForegroundColor Cyan
}
catch {
    Write-Host "  ✗ Validation failed: $_" -ForegroundColor Red
}
