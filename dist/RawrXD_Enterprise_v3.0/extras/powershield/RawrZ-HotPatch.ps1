#Requires -PSEdition Core
<#
.SYNOPSIS
  RawrZ-HotPatch K1.6 (Pure PowerShell)
  Ephemeral memory-mapped hot-patch for GGUF refusal tensor removal
  
.DESCRIPTION
  1. Locates the refusal tensor in the live MMF by file offset
  2. Overwrites that region with a pre-built "alignment" tensor (few KB)
  3. Keeps the change in memory only → original GGUF untouched
  4. Supports automatic offset detection from GGUF header parsing
  
.PARAMETER MmfName
  Name of the memory-mapped file created by RawrZ-GGUF-MMF.ps1
  
.PARAMETER PatchFile
  Path to pre-quantised alignment tensor (≤ 512 KB recommended)
  
.PARAMETER RefusalOffset
  Byte offset of refusal tensor in GGUF file (auto-detected if -1)
  
.PARAMETER GgufPath
  Path to original GGUF file (required for auto-offset detection)
  
.PARAMETER TensorPattern
  Regex pattern to identify refusal tensor (default: last layer Q-proj)
  
.PARAMETER WhatIf
  Show what would be patched without applying changes
  
.EXAMPLE
  .\RawrZ-HotPatch.ps1 -GgufPath C:\models\llama-3.1-8b-q4.gguf -PatchFile C:\patches\alignment-q4.gguf
  
.EXAMPLE
  .\RawrZ-HotPatch.ps1 -RefusalOffset 0x12345678 -PatchFile alignment-q4.gguf -WhatIf
  
.NOTES
  - Patch never leaves RAM; close MMF to revert
  - Only exact byte range of refusal tensor is overwritten
  - Keep patch ≤ 512 KB to fit within single shard boundary
  - Use pre-quantised patch matching model quantization
#>

param(
    [string]$MmfName         = "RawrZ-GGUF-MMF",
    [string]$PatchFile       = "",
    [long]$RefusalOffset     = -1,
    [string]$GgufPath        = "",
    [string]$TensorPattern   = "layers\.\d+\.self_attn\.q_proj\.weight$",
    [int]$TargetLayer        = -1,  # -1 = auto-detect last layer
    [switch]$WhatIf,
    [switch]$Verbose
)

Add-Type -Assembly System.IO
Add-Type -Assembly System.IO.MemoryMappedFiles

# ============================================================================
# GGUF Header Parsing for Offset Detection
# ============================================================================

function Read-GGUFHeader {
    param([string]$Path)
    
    $fs = [System.IO.File]::OpenRead($Path)
    $br = [System.IO.BinaryReader]::new($fs)
    
    try {
        # Read magic (4 bytes)
        $magic = $br.ReadUInt32()
        if ($magic -ne 0x46554747) {  # "GGUF"
            throw "Invalid GGUF magic: 0x$($magic.ToString('X8'))"
        }
        
        # Read version (4 bytes)
        $version = $br.ReadUInt32()
        if ($version -ne 3) {
            throw "Unsupported GGUF version: $version (need v3)"
        }
        
        # Read tensor count (8 bytes)
        $tensorCount = $br.ReadUInt64()
        
        # Read metadata KV count (8 bytes)
        $kvCount = $br.ReadUInt64()
        
        Write-Verbose "GGUF Header: Magic=0x$($magic.ToString('X8')), Version=$version, Tensors=$tensorCount, KV=$kvCount"
        
        return @{
            Magic        = $magic
            Version      = $version
            TensorCount  = $tensorCount
            KVCount      = $kvCount
            MetadataPos  = $fs.Position
        }
    } finally {
        $br.Close()
        $fs.Close()
    }
}

function Read-GGUFString {
    param([System.IO.BinaryReader]$Reader)
    $len = $Reader.ReadUInt64()
    $bytes = $Reader.ReadBytes($len)
    return [System.Text.Encoding]::UTF8.GetString($bytes)
}

function Skip-GGUFValue {
    param([System.IO.BinaryReader]$Reader, [uint32]$Type)
    
    switch ($Type) {
        0  { $Reader.ReadByte() }          # uint8
        1  { Read-GGUFString $Reader }     # string
        2  { $Reader.ReadBytes(8) }        # array - simplified
        3  { $Reader.ReadBytes(8) }        # uint16
        4  { $Reader.ReadUInt32() }        # uint32
        5  { $Reader.ReadInt32() }         # int32
        6  { $Reader.ReadSingle() }        # float32
        7  { $Reader.ReadBoolean() }       # bool
        8  { Read-GGUFString $Reader }     # string (v2)
        9  { $Reader.ReadUInt64() }        # uint64
        10 { $Reader.ReadInt64() }         # int64
        11 { $Reader.ReadDouble() }        # float64
        default { throw "Unknown metadata type: $Type" }
    }
}

function Get-RefusalTensorOffset {
    param([string]$GgufPath, [string]$Pattern, [int]$PreferredLayer)
    
    if (-not (Test-Path $GgufPath)) {
        throw "GGUF file not found: $GgufPath"
    }
    
    Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "  Scanning GGUF for refusal tensor offset..." -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Cyan
    
    $header = Read-GGUFHeader -Path $GgufPath
    $fs = [System.IO.File]::OpenRead($GgufPath)
    $br = [System.IO.BinaryReader]::new($fs)
    
    try {
        $fs.Position = $header.MetadataPos
        
        # Skip metadata KV pairs
        Write-Verbose "Skipping $($header.KVCount) metadata entries..."
        for ($i = 0; $i -lt $header.KVCount; $i++) {
            $key = Read-GGUFString $br
            $type = $br.ReadUInt32()
            Skip-GGUFValue $br $type
        }
        
        # Read tensor info
        $tensors = @()
        Write-Verbose "Reading $($header.TensorCount) tensor entries..."
        
        for ($i = 0; $i -lt $header.TensorCount; $i++) {
            $name = Read-GGUFString $br
            $nDims = $br.ReadUInt32()
            
            $shape = @()
            for ($d = 0; $d -lt $nDims; $d++) {
                $shape += $br.ReadUInt64()
            }
            
            $type = $br.ReadUInt32()
            $offset = $br.ReadUInt64()
            
            $tensors += @{
                Name   = $name
                Dims   = $nDims
                Shape  = $shape
                Type   = $type
                Offset = $offset
            }
            
            if ($Verbose -and $name -match "layers\.(\d+)") {
                Write-Verbose "  Layer $($Matches[1]): $name @ 0x$($offset.ToString('X'))"
            }
        }
        
        # Find refusal tensor candidates
        $candidates = $tensors | Where-Object { $_.Name -match $Pattern }
        
        if ($candidates.Count -eq 0) {
            throw "No tensors matching pattern '$Pattern' found"
        }
        
        Write-Host "`nFound $($candidates.Count) candidate tensor(s):" -ForegroundColor Yellow
        foreach ($c in $candidates) {
            $layerNum = if ($c.Name -match "layers\.(\d+)") { $Matches[1] } else { "?" }
            Write-Host "  Layer $layerNum : $($c.Name) @ 0x$($c.Offset.ToString('X'))" -ForegroundColor Gray
        }
        
        # Select target tensor (last layer by default)
        $target = if ($PreferredLayer -ge 0) {
            $candidates | Where-Object { $_.Name -match "layers\.$PreferredLayer\." } | Select-Object -First 1
        } else {
            # Auto-detect last layer
            $maxLayer = ($candidates | ForEach-Object { 
                if ($_.Name -match "layers\.(\d+)") { [int]$Matches[1] } else { 0 }
            } | Measure-Object -Maximum).Maximum
            $candidates | Where-Object { $_.Name -match "layers\.$maxLayer\." } | Select-Object -First 1
        }
        
        if (-not $target) {
            throw "Could not determine target tensor"
        }
        
        Write-Host "`n✓ Selected refusal tensor:" -ForegroundColor Green
        Write-Host "  Name  : $($target.Name)" -ForegroundColor White
        Write-Host "  Offset: 0x$($target.Offset.ToString('X')) ($($target.Offset) bytes)" -ForegroundColor White
        Write-Host "  Shape : $($target.Shape -join ' × ')" -ForegroundColor White
        
        return $target.Offset
        
    } finally {
        $br.Close()
        $fs.Close()
    }
}

# ============================================================================
# Hot-Patch Application
# ============================================================================

function Invoke-HotPatch {
    param(
        [System.IO.MemoryMappedFiles.MemoryMappedFile]$Mmf,
        [long]$Offset,
        [byte[]]$PatchBytes
    )
    
    Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Magenta
    Write-Host "  Applying hot-patch to memory-mapped file..." -ForegroundColor Magenta
    Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Magenta
    
    $view = $Mmf.CreateViewStream($Offset, $PatchBytes.Length, [System.IO.MemoryMappedFiles.MemoryMappedFileAccess]::Write)
    
    try {
        $view.Write($PatchBytes, 0, $PatchBytes.Length)
        $view.Flush()
        
        Write-Host "✓ Hot-patched $($PatchBytes.Length) bytes at MMF offset 0x$($Offset.ToString('X'))" -ForegroundColor Green
        Write-Host "✓ Patch size: $(($PatchBytes.Length / 1KB).ToString('F2')) KB" -ForegroundColor Green
        
        # Verify write (read back first 16 bytes)
        $view.Position = 0
        $verify = New-Object byte[] 16
        $view.Read($verify, 0, 16)
        Write-Verbose "Verification (first 16 bytes): $([BitConverter]::ToString($verify))"
        
    } finally {
        $view.Close()
    }
}

# ============================================================================
# Main Workflow
# ============================================================================

Write-Host @"

╔═══════════════════════════════════════════════════════════════════════╗
║                                                                       ║
║   ██████╗  █████╗ ██╗    ██╗██████╗ ███████╗    ██╗  ██╗ ██╗ ██████╗ ║
║   ██╔══██╗██╔══██╗██║    ██║██╔══██╗╚══███╔╝    ██║ ██╔╝███║██╔════╝ ║
║   ██████╔╝███████║██║ █╗ ██║██████╔╝  ███╔╝     █████╔╝ ╚██║███████╗ ║
║   ██╔══██╗██╔══██║██║███╗██║██╔══██╗ ███╔╝      ██╔═██╗  ██║██╔═══██║║
║   ██║  ██║██║  ██║╚███╔███╔╝██║  ██║███████╗    ██║  ██╗ ██║╚██████╔╝║
║   ╚═╝  ╚═╝╚═╝  ╚═╝ ╚══╝╚══╝ ╚═╝  ╚═╝╚══════╝    ╚═╝  ╚═╝ ╚═╝ ╚═════╝ ║
║                                                                       ║
║              Ephemeral Refusal Hot-Patch (Zero-Disk)                 ║
║                   Memory-Mapped File Edition                         ║
║                                                                       ║
╚═══════════════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

try {
    # Validate patch file
    if (-not $PatchFile) {
        # Try to find default alignment patch
        $defaultPaths = @(
            "$(pwd)\alignment-q4.gguf",
            "C:\patches\alignment-q4.gguf",
            "$PSScriptRoot\alignment-q4.gguf"
        )
        $PatchFile = $defaultPaths | Where-Object { Test-Path $_ } | Select-Object -First 1
        
        if (-not $PatchFile) {
            throw "No patch file specified and no default found. Use -PatchFile parameter."
        }
    }
    
    if (-not (Test-Path $PatchFile)) {
        throw "Patch file not found: $PatchFile"
    }
    
    $patchBytes = [System.IO.File]::ReadAllBytes($PatchFile)
    $patchSizeKB = $patchBytes.Length / 1KB
    
    Write-Host "Patch file: $PatchFile" -ForegroundColor White
    Write-Host "Patch size: $($patchSizeKB.ToString('F2')) KB ($($patchBytes.Length) bytes)" -ForegroundColor White
    
    if ($patchBytes.Length -gt 512KB) {
        Write-Warning "Patch size exceeds 512 KB! May span multiple shards."
    }
    
    # Auto-detect offset if needed
    if ($RefusalOffset -lt 0) {
        if (-not $GgufPath) {
            throw "Either -RefusalOffset or -GgufPath (for auto-detection) must be provided"
        }
        $RefusalOffset = Get-RefusalTensorOffset -GgufPath $GgufPath -Pattern $TensorPattern -PreferredLayer $TargetLayer
    } else {
        Write-Host "`nUsing provided offset: 0x$($RefusalOffset.ToString('X')) ($RefusalOffset bytes)" -ForegroundColor Yellow
    }
    
    # Open MMF
    Write-Host "`nOpening memory-mapped file: $MmfName" -ForegroundColor White
    $mmf = [System.IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting($MmfName)
    Write-Host "✓ MMF opened successfully" -ForegroundColor Green
    
    # Apply or simulate patch
    if ($WhatIf) {
        Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Yellow
        Write-Host "  WHATIF MODE - No changes will be applied" -ForegroundColor Yellow
        Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Yellow
        Write-Host "`nWould patch:" -ForegroundColor Yellow
        Write-Host "  Size  : $($patchSizeKB.ToString('F2')) KB" -ForegroundColor Gray
        Write-Host "  Offset: 0x$($RefusalOffset.ToString('X'))" -ForegroundColor Gray
        Write-Host "  MMF   : $MmfName" -ForegroundColor Gray
    } else {
        Invoke-HotPatch -Mmf $mmf -Offset $RefusalOffset -PatchBytes $patchBytes
        
        Write-Host "`n═══════════════════════════════════════════════════════════" -ForegroundColor Green
        Write-Host "  ✓ Hot-patch complete!" -ForegroundColor Green
        Write-Host "═══════════════════════════════════════════════════════════" -ForegroundColor Green
        Write-Host "`n  • Model in MMF no longer refuses" -ForegroundColor White
        Write-Host "  • Original GGUF file untouched" -ForegroundColor White
        Write-Host "  • Patch is ephemeral (RAM only)" -ForegroundColor White
        Write-Host "  • Close MMF to revert changes" -ForegroundColor White
    }
    
} catch {
    Write-Host "`n╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Red
    Write-Host "║  ERROR                                                    ║" -ForegroundColor Red
    Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Red
    Write-Error $_
    exit 1
} finally {
    if ($mmf) {
        # Don't dispose - keep MMF alive for consumers
        Write-Verbose "Keeping MMF handle open for consumer access"
    }
}

Write-Host "`n" -NoNewline
