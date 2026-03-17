#!/usr/bin/env pwsh
<#
RawrXD Complete MASM Assembler - NASM Backend with Macro Support
Handles: MASM syntax, macros (with %n substitution), x64 instructions, PE64 output

Usage: .\masm_assembler.ps1 -Input input.asm -Output output.exe
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$Input,
    
    [Parameter(Mandatory=$true)]
    [string]$Output
)

$ErrorActionPreference = "Stop"

# ============================================================================
# MACRO SYSTEM
# ============================================================================

class MacroDefinition {
    [string]$Name
    [string[]]$Body
    [int]$ParamCount
    [hashtable]$Defaults
    [bool]$IsVariadic
    [int]$DefinedAtLine
    
    MacroDefinition([string]$name, [string[]]$body, [int]$paramCount, [int]$line) {
        $this.Name = $name
        $this.Body = $body
        $this.ParamCount = $paramCount
        $this.Defaults = @{}
        $this.IsVariadic = $false
        $this.DefinedAtLine = $line
    }
}

$script:MacroTable = @{}
$script:MacroDepth = 0
$script:MaxMacroDepth = 32

# ============================================================================
# MASM → NASM CONVERTER
# ============================================================================

function Convert-MAsmToNasm {
    param([string]$Source)
    
    $lines = @()
    $inMacro = $false
    $macroName = ""
    $macroBody = @()
    $macroParams = 0
    $lineNum = 0
    
    $sourceLines = $Source -split "`n"
    
    foreach ($line in $sourceLines) {
        $lineNum++
        $trimmed = $line.Trim()
        
        # Skip empty lines
        if (-not $trimmed -or $trimmed.StartsWith(";")) {
            $lines += $line
            continue
        }
        
        # Check for MACRO definition
        if ($trimmed -match '^\s*(\w+)\s+MACRO\s*(.*)$') {
            $macroName = $matches[1]
            $params = $matches[2].Trim()
            $macroParams = if ($params) { ($params -split ',').Count } else { 0 }
            $macroBody = @()
            $inMacro = $true
            
            $script:MacroTable[$macroName] = [MacroDefinition]::new($macroName, $null, $macroParams, $lineNum)
            continue
        }
        
        # Check for ENDM
        if ($trimmed -match '^\s*ENDM\s*$') {
            if ($inMacro) {
                $script:MacroTable[$macroName].Body = $macroBody
                $inMacro = $false
                $macroName = ""
                $macroBody = @()
            }
            continue
        }
        
        # Collect macro body
        if ($inMacro) {
            $macroBody += $trimmed
            continue
        }
        
        # Handle MASM directives
        if ($trimmed -match '^\.CODE|^\.code') {
            $lines += "section .text"
            continue
        }
        
        if ($trimmed -match '^\.DATA|^\.data') {
            $lines += "section .data"
            continue
        }
        
        if ($trimmed -match '^\.ENDS|^\.ENDP|^END') {
            continue
        }
        
        # Convert procedure labels
        if ($trimmed -match '^(\w+)\s+PROC') {
            $procName = $matches[1]
            $lines += "${procName}:"
            continue
        }
        
        # Convert labels (colon notation)
        if ($trimmed -match '^(\w+)\s*:') {
            $labelName = $matches[1]
            $lines += "${labelName}:"
            continue
        }
        
        # Check for macro invocation
        $macroInvoke = $false
        foreach ($macroName in $script:MacroTable.Keys) {
            if ($trimmed -match "^\s*$macroName\s*(.*)$") {
                $args = $matches[1].Trim()
                $expanded = Expand-Macro $macroName $args
                $lines += $expanded
                $macroInvoke = $true
                break
            }
        }
        
        if ($macroInvoke) {
            continue
        }
        
        # Convert instruction operands (MOV to mov, etc.)
        if ($trimmed -match '^[A-Z]+') {
            $lower = $trimmed -replace '^([A-Z]+)', { $args[0].Value.ToLower() }
            $lines += "    $lower"
            continue
        }
        
        # Pass through as-is
        $lines += $line
    }
    
    return $lines -join "`n"
}

function Expand-Macro {
    param([string]$Name, [string]$ArgsStr)
    
    if ($script:MacroDepth -ge $script:MaxMacroDepth) {
        throw "Macro recursion depth exceeded: $Name"
    }
    
    $macro = $script:MacroTable[$Name]
    if (-not $macro) {
        throw "Unknown macro: $Name"
    }
    
    # Parse arguments
    $args = @()
    if ($ArgsStr) {
        $args = $ArgsStr -split ',' | ForEach-Object { $_.Trim() }
    }
    
    $script:MacroDepth++
    
    try {
        $expanded = @()
        
        foreach ($bodyLine in $macro.Body) {
            $line = $bodyLine
            
            # Substitute %n with arguments
            for ($i = 0; $i -lt $args.Count; $i++) {
                $paramNum = $i + 1
                $line = $line -replace "%$paramNum\b", $args[$i]
            }
            
            # Substitute %0 with argument count
            $line = $line -replace "%0\b", $args.Count
            
            # Expand nested macros
            foreach ($macroName in $script:MacroTable.Keys) {
                if ($line -match "\b$macroName\b") {
                    $line = Expand-Macro $macroName $line
                }
            }
            
            $expanded += $line
        }
        
        return $expanded
    }
    finally {
        $script:MacroDepth--
    }
}

# ============================================================================
# PE64 BUILDER
# ============================================================================

function Build-PE64 {
    param(
        [byte[]]$Code,
        [string]$OutputFile
    )
    
    $dos = New-Object byte[] 64
    $dos[0] = 0x4D  # 'M'
    $dos[1] = 0x5A  # 'Z'
    [BitConverter]::GetBytes(0x40) | ForEach-Object -Begin { $i = 0x3C } { $dos[$i++] = $_ }
    
    $pe = New-Object System.Collections.ArrayList
    $null = $pe.AddRange($dos)
    $null = $pe.AddRange([System.Text.Encoding]::ASCII.GetBytes("PE`0`0"))
    
    # COFF Header
    $null = $pe.AddRange([BitConverter]::GetBytes([uint16]0x8664))     # Machine: x64
    $null = $pe.AddRange([BitConverter]::GetBytes([uint16]1))          # Sections: 1
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0))          # Timestamp
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0))          # Symbol table
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0))          # Num symbols
    $null = $pe.AddRange([BitConverter]::GetBytes([uint16]240))        # Opt header size
    $null = $pe.AddRange([BitConverter]::GetBytes([uint16]0x22))       # Characteristics
    
    # Optional Header PE32+
    $null = $pe.AddRange([BitConverter]::GetBytes([uint16]0x020B))     # Magic
    $null = $pe.AddRange([BitConverter]::GetBytes([uint16]0x0E00))     # Linker version
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]$Code.Length)) # Code size
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0))          # Init data
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0))          # Uninit data
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0x1000))     # Entry point
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0x1000))     # Base of code
    
    # Image base
    $null = $pe.AddRange([BitConverter]::GetBytes([uint64]0x140000000L))
    
    # Alignment and sizes
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0x1000))     # Section align
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0x200))      # File align
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0x00060000)) # OS version
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0))          # Image version
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0x00060000)) # Subsystem version
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0))          # Win32 version
    
    $imageSize = 0x2000 + (([Math]::Ceiling($Code.Length / 0x1000)) * 0x1000)
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]$imageSize))
    
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0x200))      # Header size
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0))          # Checksum
    $null = $pe.AddRange([BitConverter]::GetBytes([uint16]3))          # Subsystem (console)
    $null = $pe.AddRange([BitConverter]::GetBytes([uint16]0))          # DLL characteristics
    
    # Stack/Heap
    $null = $pe.AddRange([BitConverter]::GetBytes([uint64]0x100000L))  # Stack reserve
    $null = $pe.AddRange([BitConverter]::GetBytes([uint64]0x1000L))    # Stack commit
    $null = $pe.AddRange([BitConverter]::GetBytes([uint64]0x100000L))  # Heap reserve
    $null = $pe.AddRange([BitConverter]::GetBytes([uint64]0x1000L))    # Heap commit
    
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0))          # Loader flags
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]16))         # Data directories
    
    # Data directories (16 null entries)
    for ($i = 0; $i -lt 16; $i++) {
        $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0))
        $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0))
    }
    
    # Section header (.text)
    $null = $pe.AddRange([System.Text.Encoding]::ASCII.GetBytes(".text`0`0`0"))
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]$Code.Length))
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0x1000))
    
    $rawSize = [Math]::Ceiling($Code.Length / 0x200) * 0x200
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]$rawSize))
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0x200))
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0))
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0))
    $null = $pe.AddRange([BitConverter]::GetBytes([uint16]0))
    $null = $pe.AddRange([BitConverter]::GetBytes([uint16]0))
    $null = $pe.AddRange([BitConverter]::GetBytes([uint32]0x60000020))  # Characteristics
    
    # Pad headers to 0x200
    while ($pe.Count -lt 0x200) {
        $null = $pe.Add(0)
    }
    
    # Add code
    $null = $pe.AddRange($Code)
    
    # Pad to next 0x200 boundary
    while ($pe.Count % 0x200 -ne 0) {
        $null = $pe.Add(0)
    }
    
    $outputBytes = [byte[]]$pe
    [System.IO.File]::WriteAllBytes($OutputFile, $outputBytes)
}

# ============================================================================
# MAIN
# ============================================================================

try {
    Write-Host "📄 RawrXD MASM Assembler (NASM Backend)" -ForegroundColor Cyan
    Write-Host "   Input:  $Input" -ForegroundColor Gray
    Write-Host "   Output: $Output" -ForegroundColor Gray
    
    if (-not (Test-Path $Input)) {
        throw "Input file not found: $Input"
    }
    
    # Read source
    $source = Get-Content $Input -Raw
    $sourceSize = (Get-Item $Input).Length
    Write-Host "   Size:   $sourceSize bytes" -ForegroundColor Gray
    Write-Host ""
    
    # Phase 1: Macro Expansion
    Write-Host "📖 Macro Processing..." -ForegroundColor Yellow
    $converted = Convert-MAsmToNasm $source
    Write-Host "   Macros defined: $($script:MacroTable.Count)" -ForegroundColor Green
    
    # Phase 2: NASM Assembly
    Write-Host "🔨 Assembling with NASM..." -ForegroundColor Yellow
    $tempNasm = [System.IO.Path]::GetTempFileName() + ".asm"
    $tempObj = [System.IO.Path]::GetTempFileName() + ".o"
    
    # Prepend NASM header
    $nasmSource = @"
default rel
BITS 64

section .text
    align 16, db 0x90

"@ + $converted
    
    Set-Content -Path $tempNasm -Value $nasmSource -Encoding ASCII
    
    $nasmOutput = nasm -f win64 -o $tempObj $tempNasm 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "NASM assembly failed: $nasmOutput"
    }
    
    Write-Host "   ✅ Assembly successful" -ForegroundColor Green
    
    # Phase 3: Link
    Write-Host "🔗 Linking..." -ForegroundColor Yellow
    $objData = [System.IO.File]::ReadAllBytes($tempObj)
    Write-Host "   Code size: $($objData.Length) bytes" -ForegroundColor Green
    
    # Phase 4: PE Generation
    Write-Host "📝 Generating PE64..." -ForegroundColor Yellow
    Build-PE64 $objData $Output
    
    $outputSize = (Get-Item $Output).Length
    Write-Host "   ✅ Generated: $Output ($outputSize bytes)" -ForegroundColor Green
    Write-Host "   Entry point: 0x140001000" -ForegroundColor Green
    
    # Cleanup
    Remove-Item $tempNasm, $tempObj -ErrorAction SilentlyContinue
    
    Write-Host ""
    Write-Host "✅ Build complete!" -ForegroundColor Green
    exit 0
}
catch {
    Write-Host "❌ Error: $_" -ForegroundColor Red
    exit 1
}
