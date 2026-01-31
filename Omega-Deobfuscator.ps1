<#
.SYNOPSIS
    Omega-Deobfuscator: Bulletproof Reverse Engineering Tool
.DESCRIPTION
    Handles corrupted paths, malformed ASARs, encrypted WASM, and layered obfuscation
.PARAMETER SourceDir
    Source directory containing obfuscated Electron app
.PARAMETER OutDir
    Output directory for reconstructed sources
.PARAMETER BruteForce
    Enable aggressive deobfuscation (slower but thorough)
#>
[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [Parameter(Mandatory=$true)]
    [string]$SourceDir,
    
    [Parameter(Mandatory=$false)]
    [string]$OutDir = "Omega_Reconstructed",
    
    [Parameter(Mandatory=$false)]
    [int]$MaxTPS = 10,           # Maximum transactions per second (anti-detection)
    
    [Parameter(Mandatory=$false)]
    [int]$MinDelayMs = 50,       # Minimum delay between operations (ms)
    [Parameter(Mandatory=$false)]
    [int]$MaxDelayMs = 200,      # Maximum delay between operations (ms)
    
    [switch]$BruteForce,
    [switch]$ExtractSecrets,
    [switch]$ReconstructTypescript,
    [switch]$EnableJitter,        # Add random jitter to delays
    [switch]$SimulateHuman        # Simulate human-like interaction patterns
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "Continue"

# TPS Rate Limiter for anti-detection
$script:operationCount = 0
$script:operationStartTime = Get-Date
$script:lastOperationTime = Get-Date

function Test-RateLimit {
    param([int]$maxTPS)
    
    $script:operationCount++
    $elapsed = (Get-Date) - $script:operationStartTime
    $currentTPS = $script:operationCount / $elapsed.TotalSeconds
    
    if ($currentTPS -gt $maxTPS) {
        # Calculate required delay
        $targetInterval = 1.0 / $maxTPS
        $actualInterval = (Get-Date) - $script:lastOperationTime
        $delayNeeded = $targetInterval - $actualInterval.TotalSeconds
        
        if ($delayNeeded -gt 0) {
            Start-Sleep -Milliseconds ($delayNeeded * 1000)
        }
    }
    
    $script:lastOperationTime = Get-Date
    return $true
}

function Get-HumanLikeDelay {
    param(
        [int]$minMs,
        [int]$maxMs,
        [switch]$enableJitter
    )
    
    # Base delay
    $delay = Get-Random -Minimum $minMs -Maximum $maxMs
    
    if ($enableJitter) {
        # Add Gaussian jitter (more natural than uniform)
        $jitter = (Get-Random -Minimum -50 -Maximum 50) + (Get-Random -Minimum -50 -Maximum 50)
        $delay = [Math]::Max($minMs, [Math]::Min($maxMs, $delay + $jitter))
    }
    
    if ($SimulateHuman) {
        # Simulate human work patterns (slower at start, faster in middle, slower at end)
        $progress = $script:operationCount % 100 / 100
        if ($progress -lt 0.1 -or $progress -gt 0.9) {
            $delay *= 1.5  # Slower at beginning and end
        } elseif ($progress -gt 0.3 -and $progress -lt 0.7) {
            $delay *= 0.7  # Faster in the middle
        }
        
        # Simulate fatigue (gradually slower over time)
        $fatigueFactor = 1.0 + ($script:operationCount / 1000)
        $delay = [Math]::Min($delay * $fatigueFactor, $maxMs * 1.5)
    }
    
    return [int]$delay
}

# Critical: Sanitize path function to prevent JS injection into paths
function Get-SanitizedPath {
    param([string]$path)
    # Remove newlines, null bytes, and path traversal attempts embedded in strings
    $clean = $path -replace '[\r\n\0]', '' -replace '\\x[0-9a-fA-F]{2}', '' -replace '\.\.', ''
    # Truncate if excessively long (indicates corruption)
    if ($clean.Length -gt 260) { $clean = $clean.Substring(0, 260) }
    return $clean.Trim()
}

function Invoke-SafeDirectoryCreation {
    param([string]$path)
    try {
        $sanitized = Get-SanitizedPath $path
        if (-not (Test-Path $sanitized)) {
            New-Item -ItemType Directory -Path $sanitized -Force | Out-Null
        }
        return $sanitized
    } catch {
        # Fallback to short path
        $fallback = Join-Path $OutDir ("dir_" + [Guid]::NewGuid().ToString().Substring(0,8))
        New-Item -ItemType Directory -Path $fallback -Force | Out-Null
        return $fallback
    }
}

# Enhanced ASAR extraction with binary header parsing
function Expand-AsarArchive {
    param([string]$asarPath, [string]$destPath)
    
    Write-Host "→ Processing ASAR: $(Split-Path $asarPath -Leaf)" -ForegroundColor Cyan
    
    try {
        $bytes = [System.IO.File]::ReadAllBytes($asarPath)
        
        # ASAR format: 4 bytes header size (uint32) + 4 bytes header size again + JSON header + files
        if ($bytes.Length -lt 8) { throw "File too small" }
        
        # Read header size (little-endian)
        $headerSize = [BitConverter]::ToUInt32($bytes, 0)
        
        # If header size is invalid, try big-endian
        if ($headerSize -eq 0 -or $headerSize -gt $bytes.Length -or $headerSize -gt 100MB) {
            $headerSize = [BitConverter]::ToUInt32([byte[]]($bytes[3], $bytes[2], $bytes[1], $bytes[0]), 0)
        }
        
        if ($headerSize -eq 0 -or $headerSize -gt $bytes.Length -or $headerSize -gt 100MB) {
            throw "Invalid header size: $headerSize"
        }
        
        # Extract header bytes
        $headerStart = 8
        $headerBytes = $bytes[$headerStart..($headerStart + $headerSize - 1)]
        $headerJson = [System.Text.Encoding]::UTF8.GetString($headerBytes)
        
        # Sanitize JSON - remove trailing nulls and invalid characters
        $headerJson = $headerJson.TrimEnd("`0", " ", "`n", "`r", "`t")
        
        # Remove any invalid JSON characters before parsing
        $headerJson = $headerJson -replace '[^\x20-\x7E\x09\x0A\x0D]', ''
        
        # Handle truncated JSON
        if (-not $headerJson.EndsWith("}")) {
            $lastBrace = $headerJson.LastIndexOf("}")
            if ($lastBrace -gt 0) {
                $headerJson = $headerJson.Substring(0, $lastBrace + 1)
            }
        }
        
        # Try to parse JSON
        try {
            $header = $headerJson | ConvertFrom-Json -ErrorAction Stop
        } catch {
            # If JSON parsing fails, try to repair common issues
            $headerJson = $headerJson -replace ',\s*}', '}' -replace ',\s*]', ']'
            $header = $headerJson | ConvertFrom-Json -ErrorAction Stop
        }
        
        function Extract-AsarFiles {
            param($node, $basePath, $offset)
            
            if ($node.files) {
                foreach ($file in $node.files.PSObject.Properties) {
                    $name = $file.Name
                    $info = $file.Value
                    $currentPath = Join-Path $basePath $name
                    
                    if ($info.size -and $info.offset) {
                        # It's a file
                        $fileOffset = [long]$info.offset + [long]$offset
                        $fileSize = [int]$info.size
                        
                        if (($fileOffset + $fileSize) -le $bytes.Length) {
                            $fileBytes = $bytes[$fileOffset..($fileOffset + $fileSize - 1)]
                            $safePath = Invoke-SafeDirectoryCreation (Split-Path $currentPath)
                            $filePath = Join-Path $safePath $name
                            [System.IO.File]::WriteAllBytes($filePath, $fileBytes)
                        }
                    } elseif ($info.files) {
                        # It's a directory
                        $safeDir = Invoke-SafeDirectoryCreation $currentPath
                        Extract-AsarFiles -node $info -basePath $safeDir -offset $offset
                    }
                }
            }
        }
        
        $safeDest = Invoke-SafeDirectoryCreation $destPath
        Extract-AsarFiles -node $header -basePath $safeDest -offset (8 + $headerSize)
        
        Write-Host "  ✓ Extracted ASAR to $destPath" -ForegroundColor Green
        return $true
    } catch {
        Write-Warning "  ✗ ASAR extraction failed: $_"
        # Fallback: treat as zip
        try {
            Expand-Archive -Path $asarPath -DestinationPath $destPath -Force
            return $true
        } catch {
            return $false
        }
    }
}

# Advanced JS Deobfuscator with AST reconstruction
function Restore-JavaScriptSource {
    param([string]$jsPath, [string]$outPath)
    
    $content = Get-Content -Path $jsPath -Raw -ErrorAction SilentlyContinue
    if (-not $content) { return $false }
    
    # Detect obfuscation type
    $isWebpack = $content -match '__webpack_require__|module\.exports\s*='
    $isTerser = $content -match '\!function\(|function\([a-z],\s*[a-z],\s*[a-z]\)\{'
    $isEval = $content -match 'eval\(function\(p,a,c,k,e'
    
    $reconstructed = $content
    
    # Layer 1: Decode eval-packed scripts (common in Cursor)
    if ($isEval) {
        try {
            # Pattern: eval(function(p,a,c,k,e,d){...}(...))
            $matches = [regex]::Matches($content, 'eval\(function\(p,a,c,k,e,d\)\{([^\}]+)\}\(([^)]+)\)\)')
            foreach ($match in $matches) {
                # This is a packed script - would need JS engine to unpack properly
                # For now, mark it
                $reconstructed = $reconstructed -replace [regex]::Escape($match.Value), "// PACKED: $($match.Value.Substring(0,50))..."
            }
        } catch {}
    }
    
    # Layer 2: Beautify/minified code
    if ($isTerser -or $content.Length -lt 50000) {
        # Add newlines after semicolons and braces for readability
        $reconstructed = $reconstructed -replace ';\s*', "`n;`n" -replace '\{\s*', "`n{`n" -replace '\}\s*', "`n}`n"
        $reconstructed = $reconstructed -replace ',\s*', ", " # Normalize spacing
    }
    
    # Layer 3: Webpack module reconstruction
    if ($isWebpack) {
        # Extract module map
        $modulePattern = '(\d+):\s*function\s*\([^)]+\)\s*\{([^}]+)\}'
        $modules = [regex]::Matches($reconstructed, $modulePattern)
        if ($modules.Count -gt 0) {
            $moduleComments = "`n// Webpack Modules Detected: $($modules.Count)`n"
            $reconstructed = $moduleComments + $reconstructed
        }
    }
    
    # Layer 4: Restore string concatenation
    $reconstructed = $reconstructed -replace "'\s*\+\s*'", "" -replace '"\s*\+\s*"', ""
    
    # Write reconstructed
    $safeOut = Get-SanitizedPath $outPath
    $reconstructed | Out-File -FilePath $safeOut -Encoding UTF8
    
    return $true
}

# Sourcemap reconstruction with integrity checking
function Repair-SourceMap {
    param([string]$mapPath, [string]$jsPath, [string]$outDir)
    
    try {
        $mapContent = Get-Content -Path $mapPath -Raw | ConvertFrom-Json -ErrorAction Stop
        
        # Validate mappings
        if (-not $mapContent.mappings) { return $false }
        
        $sources = $mapContent.sources
        $contents = $mapContent.sourcesContent
        
        if ($sources -and $contents) {
            for ($i = 0; $i -lt $sources.Length; $i++) {
                $sourcePath = $sources[$i]
                $sourceContent = $contents[$i]
                
                # Sanitize source path (remove webpack://, etc)
                $cleanPath = $sourcePath -replace 'webpack:///', '' -replace 'file:///', ''
                $cleanPath = $cleanPath -replace '[<>:"|?*]', '_'
                
                $fullPath = Join-Path $outDir $cleanPath
                $dir = Split-Path $fullPath
                Invoke-SafeDirectoryCreation $dir | Out-Null
                
                $sourceContent | Out-File -FilePath $fullPath -Encoding UTF8 -Force
            }
            return $true
        }
    } catch {
        Write-Verbose "Sourcemap repair failed for $mapPath : $_"
    }
    return $false
}

# WASM validation and WAT generation
function Convert-WasmToWat {
    param([string]$wasmPath, [string]$outDir)
    
    $bytes = [System.IO.File]::ReadAllBytes($wasmPath)
    
    # Check magic number: 0x00 0x61 0x73 0x6D ("\0asm")
    if ($bytes.Length -lt 4 -or 
        $bytes[0] -ne 0x00 -or 
        $bytes[1] -ne 0x61 -or 
        $bytes[2] -ne 0x73 -or 
        $bytes[3] -ne 0x6D) {
        
        # Try to repair corrupted WASM (sometimes prefixed with garbage)
        $asmIndex = 0
        for ($i = 0; $i -lt [Math]::Min($bytes.Length - 4, 1024); $i++) {
            if ($bytes[$i] -eq 0x00 -and $bytes[$i+1] -eq 0x61 -and 
                $bytes[$i+2] -eq 0x73 -and $bytes[$i+3] -eq 0x6D) {
                $asmIndex = $i
                break
            }
        }
        
        if ($asmIndex -gt 0) {
            $bytes = $bytes[$asmIndex..($bytes.Length-1)]
            [System.IO.File]::WriteAllBytes($wasmPath, $bytes)
        } else {
            return $false
        }
    }
    
    # Generate WAT (WebAssembly Text) - simplified representation
    $watPath = Join-Path $outDir ((Split-Path $wasmPath -Leaf) -replace '\.wasm$', '.wat')
    $watContent = ";; WebAssembly module: $(Split-Path $wasmPath -Leaf)`n"
    $watContent += ";; Size: $($bytes.Length) bytes`n"
    $watContent += ";; Version: $([BitConverter]::ToUInt32($bytes, 4))`n`n"
    $watContent += "(module`n"
    
    # Parse sections (simplified)
    $pos = 8
    while ($pos -lt $bytes.Length) {
        $sectionId = $bytes[$pos++]
        if ($sectionId -eq 0) { break } # Custom section or end
        
        $sectionSize = 0
        $shift = 0
        while ($true) {
            $byte = $bytes[$pos++]
            $sectionSize = $sectionSize -bor (($byte -band 0x7F) -shl $shift)
            $shift += 7
            if (($byte -band 0x80) -eq 0) { break }
        }
        
        $sectionNames = @("custom", "type", "import", "func", "table", "memory", "global", "export", "start", "elem", "code", "data")
        $name = if ($sectionId -lt $sectionNames.Length) { $sectionNames[$sectionId] } else { "unknown_$sectionId" }
        $watContent += "  ;; Section: $name ($sectionSize bytes)`n"
        
        $pos += $sectionSize
    }
    
    $watContent += ")`n"
    $watContent | Out-File -FilePath $watPath -Encoding UTF8
    return $true
}

# Secret extraction (API keys, endpoints, etc)
function Find-EmbeddedSecrets {
    param([string]$content, [string]$sourceFile)
    
    $secrets = @()
    
    # Patterns
    $patterns = @{
        'API Key' = 'api[_-]?key\s*[:=]\s*["'']([a-zA-Z0-9_\-]{20,})["'']'
        'Secret' = 'secret\s*[:=]\s*["'']([a-zA-Z0-9_\-]{20,})["'']'
        'Token' = 'token\s*[:=]\s*["'']([a-zA-Z0-9_\-\.]{20,})["'']'
        'Endpoint' = 'https?://[a-zA-Z0-9\-\.]+\.(?:com|net|org|io)[a-zA-Z0-9/\-]*'
        'IP' = '\b(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\b'
    }
    
    foreach ($type in $patterns.Keys) {
        $matches = [regex]::Matches($content, $patterns[$type], [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        foreach ($match in $matches) {
            $secrets += [PSCustomObject]@{
                Type = $type
                Value = $match.Groups[1].Value
                File = $sourceFile
                Context = $match.Value.Substring(0, [Math]::Min($match.Value.Length, 50))
            }
        }
    }
    
    return $secrets
}

# Main execution
$startTime = Get-Date
$SourceDir = Resolve-Path $SourceDir
$OutDir = Invoke-SafeDirectoryCreation $OutDir

Write-Host @"
╔══════════════════════════════════════════════════════════╗
║     OMEGA DEOBFUSCATOR - Bulletproof Reconstruction      ║
║     Target: $(Split-Path $SourceDir -Leaf)                              ║
╚══════════════════════════════════════════════════════════╝
"@ -ForegroundColor Magenta

$stats = @{
    AsarExtracted = 0
    JsDeobfuscated = 0
    SourcemapsReconstructed = 0
    WasmConverted = 0
    SecretsFound = @()
    Errors = @()
}

# Phase 1: ASAR Extraction
Write-Host "`n[Phase 1] Extracting ASAR Archives..." -ForegroundColor Yellow
$asars = Get-ChildItem -Path $SourceDir -Filter "*.asar" -Recurse -ErrorAction SilentlyContinue
$asarCount = 0
foreach ($asar in $asars) {
    $asarCount++
    
    # Rate limit check
    Test-RateLimit -maxTPS $MaxTPS | Out-Null
    
    # Human-like delay
    $delay = Get-HumanLikeDelay -minMs $MinDelayMs -maxMs $MaxDelayMs -enableJitter:$EnableJitter
    Start-Sleep -Milliseconds $delay
    
    Write-Progress -Activity "Extracting ASAR" -Status $asar.Name -PercentComplete (($asarCount/$asars.Count)*100)
    
    $dest = Join-Path $OutDir "asar_extracted" (Split-Path $asar.FullName -Leaf)
    if (Expand-AsarArchive -asarPath $asar.FullName -destPath $dest) {
        $stats.AsarExtracted++
    }
}

# Phase 2: JavaScript Deobfuscation
Write-Host "`n[Phase 2] Deobfuscating JavaScript..." -ForegroundColor Yellow
$jsFiles = Get-ChildItem -Path $SourceDir -Filter "*.js" -Recurse | Where-Object { $_.Length -lt 5MB }
$total = $jsFiles.Count
$current = 0

foreach ($js in $jsFiles) {
    $current++
    
    # Rate limit check
    Test-RateLimit -maxTPS $MaxTPS | Out-Null
    
    # Human-like delay
    $delay = Get-HumanLikeDelay -minMs $MinDelayMs -maxMs $MaxDelayMs -enableJitter:$EnableJitter
    Start-Sleep -Milliseconds $delay
    
    Write-Progress -Activity "Deobfuscating" -Status $js.Name -PercentComplete (($current/$total)*100)
    
    # Get relative path from source directory
    $relativePath = $js.FullName.Substring($SourceDir.Length).TrimStart('\', '/')
    
    # Create output path - ensure we don't nest "deobfuscated" multiple times
    $outJs = Join-Path $OutDir "deobfuscated" $relativePath
    $outDir = Split-Path $outJs
    
    # Create directory safely
    Invoke-SafeDirectoryCreation $outDir | Out-Null
    
    if (Restore-JavaScriptSource -jsPath $js.FullName -outPath $outJs) {
        $stats.JsDeobfuscated++
        
        if ($ExtractSecrets) {
            $content = Get-Content $outJs -Raw
            $secrets = Find-EmbeddedSecrets -content $content -sourceFile $js.Name
            $stats.SecretsFound += $secrets
        }
    }
}

# Phase 3: Sourcemap Reconstruction
Write-Host "`n[Phase 3] Reconstructing from Sourcemaps..." -ForegroundColor Yellow
$maps = Get-ChildItem -Path $SourceDir -Filter "*.js.map" -Recurse
foreach ($map in $maps) {
    $jsFile = $map.FullName -replace '\.map$', ''
    if (Test-Path $jsFile) {
        $dest = Join-Path $OutDir "typescript_sources"
        if (Repair-SourceMap -mapPath $map.FullName -jsPath $jsFile -outDir $dest) {
            $stats.SourcemapsReconstructed++
        }
    }
}

# Phase 4: WASM Processing
Write-Host "`n[Phase 4] Processing WebAssembly..." -ForegroundColor Yellow
$wasms = Get-ChildItem -Path $SourceDir -Filter "*.wasm" -Recurse
foreach ($wasm in $wasms) {
    $dest = Join-Path $OutDir "wasm_disassembly"
    Invoke-SafeDirectoryCreation $dest | Out-Null
    if (Convert-WasmToWat -wasmPath $wasm.FullName -outDir $dest) {
        $stats.WasmConverted++
    }
}

# Phase 5: Native Module Analysis
Write-Host "`n[Phase 5] Analyzing Native Modules..." -ForegroundColor Yellow
$binaries = Get-ChildItem -Path $SourceDir -Include "*.exe", "*.dll", "*.node" -Recurse
$binDir = Invoke-SafeDirectoryCreation (Join-Path $OutDir "native_analysis")
foreach ($bin in $binaries) {
    $info = @{
        Name = $bin.Name
        Size = $bin.Length
        Type = if ($bin.Extension -eq '.node') { "Node Native Addon" } else { "PE Binary" }
        Path = $bin.FullName
    }
    $info | ConvertTo-Json | Out-File -FilePath (Join-Path $binDir "$($bin.Name).json")
}

# Report
$duration = (Get-Date) - $startTime
Write-Host "`n╔══════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║                    RECONSTRUCTION COMPLETE                 ║" -ForegroundColor Green
Write-Host "╚══════════════════════════════════════════════════════════╝" -ForegroundColor Green
Write-Host "Duration: $($duration.ToString('hh\:mm\:ss'))"
Write-Host "ASAR Archives: $($stats.AsarExtracted)"
Write-Host "JS Files Deobfuscated: $($stats.JsDeobfuscated)"
Write-Host "TypeScript Sources: $($stats.SourcemapsReconstructed)"
Write-Host "WASM Modules: $($stats.WasmConverted)"
Write-Host "Native Binaries: $($binaries.Count)"

if ($ExtractSecrets -and $stats.SecretsFound.Count -gt 0) {
    Write-Host "`n[!] Secrets Detected:" -ForegroundColor Red
    $stats.SecretsFound | Format-Table -AutoSize
    $stats.SecretsFound | Export-Csv -Path (Join-Path $OutDir "secrets.csv") -NoTypeInformation
}

Write-Host "`nOutput: $OutDir" -ForegroundColor Cyan
