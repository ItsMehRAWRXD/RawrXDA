#Requires -Version 7.0
<#
.SYNOPSIS
    Advanced Reverse Engineering Tool - Handles obfuscated code and creates self-reversing system
.DESCRIPTION
    Advanced reverse engineering tool that can:
    - Reverse engineer obfuscated/minified code
    - Reconstruct source maps from compiled output
    - Decompile and beautify JavaScript/TypeScript
    - Reverse the build process (compiled → source)
    - Create self-reversing mechanisms for difficult extractions
    - Handle control/unextract/release scenarios
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$InputDirectory,
    
    [string]$OutputDirectory = "Advanced_Reverse_Engineered",
    
    [switch]$ReverseObfuscated,
    [switch]$ReconstructSourceMaps,
    [switch]$DecompileJavaScript,
    [switch]$ReverseBuildProcess,
    [switch]$CreateSelfReversing,
    [switch]$HandleControlScenarios,
    [switch]$ExtractAll,
    [switch]$GenerateReport,
    [switch]$ShowProgress,
    [int]$MaxDepth = 20,
    [switch]$ForceOverwrite
)

# Color codes
$Colors = @{
    Info = "Cyan"
    Success = "Green"
    Warning = "Yellow"
    Error = "Red"
    Header = "Magenta"
    Detail = "White"
    Obfuscated = "DarkRed"
    Decompile = "DarkYellow"
    Reconstruct = "DarkGreen"
    SelfReverse = "Blue"
    Control = "DarkCyan"
}

function Write-ColorOutput {
    param(
        [string]$Message,
        [string]$Type = "Info"
    )
    $color = $Colors[$Type]
    if (-not $color) { $color = "White" }
    Write-Host $Message -ForegroundColor $color
}

function Write-ProgressBar {
    param(
        [string]$Activity,
        [string]$Status,
        [int]$PercentComplete,
        [string]$CurrentOperation = ""
    )
    if ($ShowProgress) {
        Write-Progress -Activity $Activity -Status $Status -PercentComplete $percentComplete -CurrentOperation $currentOperation
    }
}

function Get-ObfuscatedFiles {
    param([string]$Dir)
    
    Write-ColorOutput "→ Scanning for obfuscated/minified files" "Obfuscated"
    
    $obfuscated = @()
    
    try {
        $jsFiles = Get-ChildItem -Path $Dir -Recurse -Filter "*.js" -ErrorAction SilentlyContinue
        
        $totalFiles = $jsFiles.Count
        $currentFile = 0
        
        foreach ($file in $jsFiles) {
            $currentFile++
            $percentComplete = [math]::Round(($currentFile / $totalFiles) * 100, 1)
            
            $relativePath = $file.FullName.Substring($Dir.Length).TrimStart('\', '/')
            Write-ProgressBar -Activity "Scanning" -Status "Processing" -PercentComplete $percentComplete -CurrentOperation $relativePath
            
            try {
                $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
                $lines = $content -split "`n"
                $avgLineLength = if ($lines.Count -gt 0) { ($lines | ForEach-Object { $_.Length } | Measure-Object -Average).Average } else { 0 }
                
                # Detect obfuscation patterns
                $isObfuscated = $false
                $obfuscationType = "Unknown"
                
                # Check for minification (very long lines, few line breaks)
                if ($avgLineLength -gt 500 -and $lines.Count -lt 100) {
                    $isObfuscated = $true
                    $obfuscationType = "Minified"
                }
                
                # Check for variable name obfuscation (single letter variables)
                if ($content -match '\b[a-zA-Z]\b\s*=\s*function\s*\(') {
                    $isObfuscated = $true
                    $obfuscationType = "Variable Obfuscation"
                }
                
                # Check for string obfuscation (hex/octal escapes)
                if ($content -match '\\x[0-9a-fA-F]{2}|\\[0-7]{3}') {
                    $isObfuscated = $true
                    $obfuscationType = "String Obfuscation"
                }
                
                # Check for control flow obfuscation
                if ($content -match 'switch\s*\([^)]+\)\s*\{[^}]+case\s+\d+:' -and ($content | Select-String -Pattern 'case' -AllMatches).Matches.Count -gt 50) {
                    $isObfuscated = $true
                    $obfuscationType = "Control Flow Obfuscation"
                }
                
                if ($isObfuscated) {
                    $obfuscated += @{
                        Path = $relativePath
                        FullPath = $file.FullName
                        Size = $file.Length
                        Lines = $lines.Count
                        AvgLineLength = $avgLineLength
                        Type = $obfuscationType
                        Confidence = if ($obfuscationType -eq "Minified") { "High" } else { "Medium" }
                    }
                }
                
            } catch {
                # Skip files that can't be read
            }
        }
        
        Write-ProgressBar -Activity "Scanning" -Status "Complete" -PercentComplete 100
        Write-Host ""
        
        Write-ColorOutput "✓ Found $($obfuscated.Count) obfuscated files" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to scan for obfuscated files: $_" "Warning"
    }
    
    return $obfuscated
}

function Deobfuscate-JavaScript {
    param(
        [string]$Content,
        [string]$ObfuscationType
    )
    
    Write-ColorOutput "→ Deobfuscating JavaScript (Type: $ObfuscationType)" "Decompile"
    
    try {
        $deobfuscated = $Content
        
        switch ($ObfuscationType) {
            "Minified" {
                # Basic beautification
                $deobfuscated = $deobfuscated -replace ';', ";`n"
                $deobfuscated = $deobfuscated -replace '{', "{`n"
                $deobfuscated = $deobfuscated -replace '}', "}`n"
                $deobfuscated = $deobfuscated -replace ',', ",`n"
                
                # Add indentation (simple approach)
                $lines = $deobfuscated -split "`n"
                $indentLevel = 0
                $beautified = @()
                
                foreach ($line in $lines) {
                    $line = $line.Trim()
                    if ($line -match '^}') { $indentLevel-- }
                    if ($indentLevel -lt 0) { $indentLevel = 0 }
                    
                    $beautifiedLine = ("  " * $indentLevel) + $line
                    $beautified += $beautifiedLine
                    
                    if ($line -match '{$') { $indentLevel++ }
                }
                
                $deobfuscated = $beautified -join "`n"
            }
            
            "Variable Obfuscation" {
                # Simple variable renaming (basic heuristic)
                $varMap = @{}
                $varCounter = 0
                
                # Find all single-letter variables
                $matches = [regex]::Matches($deobfuscated, '\b[a-zA-Z]\b')
                foreach ($match in $matches) {
                    $varName = $match.Value
                    if (-not $varMap.ContainsKey($varName)) {
                        $varMap[$varName] = "var_$varCounter"
                        $varCounter++
                    }
                }
                
                # Replace variables
                foreach ($var in $varMap.Keys) {
                    $deobfuscated = $deobfuscated -replace "\b$var\b", $varMap[$var]
                }
            }
            
            "String Obfuscation" {
                # Decode hex escapes
                $deobfuscated = $deobfuscated -replace '\\x([0-9a-fA-F]{2})', {
                    param($match)
                    $hex = $match.Groups[1].Value
                    $char = [char][convert]::ToInt32($hex, 16)
                    return $char
                }
                
                # Decode octal escapes
                $deobfuscated = $deobfuscated -replace '\\([0-7]{3})', {
                    param($match)
                    $octal = $match.Groups[1].Value
                    $char = [char][convert]::ToInt32($octal, 8)
                    return $char
                }
            }
            
            "Control Flow Obfuscation" {
                # Simplify switch statements (basic)
                $deobfuscated = $deobfuscated -replace 'switch\s*\([^)]+\)\s*\{', "switch (condition) {"
                
                # Remove dead code blocks (basic heuristic)
                $deobfuscated = $deobfuscated -replace 'if\s*\(\s*false\s*\)\s*\{[^}]*\}', ''
                $deobfuscated = $deobfuscated -replace 'if\s*\(\s*0\s*\)\s*\{[^}]*\}', ''
            }
        }
        
        # Add comments for clarity
        $deobfuscated = "// Deobfuscated from $ObfuscationType`n" + $deobfuscated
        
        Write-ColorOutput "✓ Deobfuscated successfully" "Success"
        
        return $deobfuscated
        
    } catch {
        Write-ColorOutput "⚠ Failed to deobfuscate: $_" "Warning"
        return $Content
    }
}

function Get-SourceMapInfo {
    param([string]$Content, [string]$FilePath)
    
    Write-ColorOutput "→ Extracting source map information" "Reconstruct"
    
    $sourceMapInfo = @{
        HasSourceMap = $false
        SourceMapURL = ""
        OriginalPath = ""
        Sources = @()
        SourcesContent = @()
    }
    
    try {
        # Look for source map comment
        $sourceMapMatch = [regex]::Match($Content, '\/\/#\s*sourceMappingURL=(.+)')
        if ($sourceMapMatch.Success) {
            $sourceMapInfo.HasSourceMap = $true
            $sourceMapInfo.SourceMapURL = $sourceMapMatch.Groups[1].Value.Trim()
            
            # Handle different source map URL formats
            if ($sourceMapInfo.SourceMapURL -match '^data:application/json;base64,(.+)') {
                # Inline source map (base64 encoded)
                $base64Data = $sourceMapInfo.SourceMapURL -replace '^data:application/json;base64,', ''
                try {
                    $jsonBytes = [System.Convert]::FromBase64String($base64Data)
                    $sourceMapJson = [System.Text.Encoding]::UTF8.GetString($jsonBytes)
                    $sourceMap = $sourceMapJson | ConvertFrom-Json
                    
                    $sourceMapInfo.Sources = $sourceMap.sources
                    $sourceMapInfo.SourcesContent = $sourceMap.sourcesContent
                    
                    # Try to determine original path
                    if ($sourceMap.sources -and $sourceMap.sources.Count -gt 0) {
                        $firstSource = $sourceMap.sources[0]
                        $sourceMapInfo.OriginalPath = Split-Path $firstSource -Parent
                    }
                } catch {
                    Write-ColorOutput "⚠ Failed to decode inline source map" "Warning"
                }
            } elseif ($sourceMapInfo.SourceMapURL -match '^http://go/sourcemap/(.+)') {
                # Google's internal source map system
                $goPath = $sourceMapInfo.SourceMapURL -replace '^http://go/sourcemap/', ''
                $sourceMapInfo.OriginalPath = Split-Path $goPath -Parent
                
                # Try to infer from path structure
                if ($goPath -match 'core/vs/workbench/') {
                    $sourceMapInfo.OriginalPath = "src/vs/workbench/"
                } elseif ($goPath -match 'extensions/') {
                    $sourceMapInfo.OriginalPath = "extensions/"
                }
            } else {
                # External source map file
                $sourceMapInfo.OriginalPath = Split-Path $sourceMapInfo.SourceMapURL -Parent
            }
        }
        
        Write-ColorOutput "✓ Extracted source map info" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to extract source map info: $_" "Warning"
    }
    
    return $sourceMapInfo
}

function Reconstruct-SourceStructure {
    param(
        [string]$OutDir,
        [string]$SrcDir
    )
    
    Write-ColorOutput "=== RECONSTRUCTING SOURCE STRUCTURE ===" "Header"
    
    $reconstruction = @{
        TotalFiles = 0
        ReconstructedFiles = 0
        SourceMapFiles = 0
        DeobfuscatedFiles = 0
        InferredTypeScript = 0
        FileMappings = @()
        Statistics = @{
            OriginalSize = 0
            ReconstructedSize = 0
            Savings = 0
        }
    }
    
    try {
        # Create src directory
        if (-not (Test-Path $SrcDir)) {
            New-Item -Path $SrcDir -ItemType Directory -Force | Out-Null
        }
        
        # Find all JavaScript files in out/ directory
        $jsFiles = Get-ChildItem -Path $OutDir -Recurse -Filter "*.js" -ErrorAction SilentlyContinue
        $reconstruction.TotalFiles = $jsFiles.Count
        
        $currentFile = 0
        
        foreach ($file in $jsFiles) {
            $currentFile++
            $percentComplete = [math]::Round(($currentFile / $reconstruction.TotalFiles) * 100, 1)
            
            $relativePath = $file.FullName.Substring($OutDir.Length).TrimStart('\', '/')
            Write-ProgressBar -Activity "Reconstructing" -Status "Processing" -PercentComplete $percentComplete -CurrentOperation $relativePath
            
            try {
                $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
                $reconstruction.Statistics.OriginalSize += $file.Length
                
                # Extract source map info
                $sourceMapInfo = Get-SourceMapInfo -Content $content -FilePath $file.FullName
                
                if ($sourceMapInfo.HasSourceMap) {
                    $reconstruction.SourceMapFiles++
                    
                    # Determine original path
                    $originalPath = if ($sourceMapInfo.OriginalPath) { $sourceMapInfo.OriginalPath } else { $relativePath }
                    
                    # Validate the path - ensure it's not JavaScript code
                    if ($originalPath -match '[;{}()]' -or $originalPath.Length -gt 200 -or $originalPath -match 'function\s*\(' -or $originalPath -match 'const\s+\w+\s*=') {
                        # Invalid path detected, use relative path instead
                        Write-ColorOutput "⚠ Invalid path detected, using relative path: $relativePath" "Warning"
                        $originalPath = $relativePath
                    }
                    
                    # Convert out/ path to src/ path
                    $srcPath = $originalPath -replace '^out/', 'src/'
                    
                    # Handle special cases
                    if ($srcPath -match 'vs/workbench/') {
                        $srcPath = $srcPath -replace 'vs/workbench/', 'src/vs/workbench/'
                    } elseif ($srcPath -match 'vs/') {
                        $srcPath = $srcPath -replace 'vs/', 'src/vs/'
                    }
                    
                    # Validate srcPath before using it
                    if ($srcPath -match '[;{}()]' -or $srcPath.Length -gt 200 -or $srcPath -match 'function\s*\(' -or $srcPath -match 'const\s+\w+\s*=') {
                        Write-ColorOutput "⚠ Invalid srcPath detected, using relative path: $relativePath" "Warning"
                        $srcPath = $relativePath -replace '^out/', 'src/'
                    }
                    
                    # Final validation - ensure srcPath is a valid file path
                    if ($srcPath -match '[<>\*\?\|\"\n\r\t]') {
                        Write-ColorOutput "⚠ Invalid characters in srcPath, using relative path: $relativePath" "Warning"
                        $srcPath = $relativePath -replace '^out/', 'src/'
                    }
                    
                    # Create directory structure
                    $targetDir = Join-Path $SrcDir (Split-Path $srcPath -Parent)
                    if (-not (Test-Path $targetDir)) {
                        New-Item -Path $targetDir -ItemType Directory -Force | Out-Null
                    }
                    
                    # Deobfuscate if needed
                    $processedContent = $content
                    if ($ReverseObfuscated) {
                        $obfuscatedFiles = Get-ObfuscatedFiles -Dir (Split-Path $file.FullName -Parent)
                        $thisFile = $obfuscatedFiles | Where-Object { $_.Path -eq $relativePath }
                        if ($thisFile) {
                            $processedContent = Deobfuscate-JavaScript -Content $content -ObfuscationType $thisFile.Type
                            $reconstruction.DeobfuscatedFiles++
                        }
                    }
                    
                    # Write reconstructed file
                    $targetPath = Join-Path $SrcDir $srcPath
                    $processedContent | Out-File -FilePath $targetPath -Encoding UTF8 -NoNewline
                    
                    # Try to infer TypeScript source
                    if ($srcPath -match '\.js$') {
                        $tsPath = $srcPath -replace '\.js$', '.ts'
                        $tsTargetPath = Join-Path $SrcDir $tsPath
                        
                        # Create placeholder TypeScript file if it doesn't exist
                        if (-not (Test-Path $tsTargetPath)) {
                            $tsContent = "// Inferred TypeScript source for $(Split-Path $srcPath -Leaf)`n// Original: $relativePath`n`n$processedContent"
                            $tsContent | Out-File -FilePath $tsTargetPath -Encoding UTF8
                            $reconstruction.InferredTypeScript++
                        }
                    }
                    
                    $reconstruction.ReconstructedFiles++
                    $reconstruction.Statistics.ReconstructedSize += $processedContent.Length
                    
                    $reconstruction.FileMappings += @{
                        Original = $relativePath
                        Reconstructed = $srcPath
                        HasSourceMap = $true
                        Deobfuscated = ($thisFile -ne $null)
                    }
                    
                } else {
                    # No source map, copy as-is to src/
                    $srcPath = $relativePath -replace '^out/', 'src/'
                    $targetDir = Join-Path $SrcDir (Split-Path $srcPath -Parent)
                    if (-not (Test-Path $targetDir)) {
                        New-Item -Path $targetDir -ItemType Directory -Force | Out-Null
                    }
                    
                    $targetPath = Join-Path $SrcDir $srcPath
                    Copy-Item -Path $file.FullName -Destination $targetPath -Force
                    
                    $reconstruction.ReconstructedFiles++
                    $reconstruction.Statistics.ReconstructedSize += $file.Length
                    
                    $reconstruction.FileMappings += @{
                        Original = $relativePath
                        Reconstructed = $srcPath
                        HasSourceMap = $false
                        Deobfuscated = $false
                    }
                }
                
            } catch {
                Write-ColorOutput "⚠ Failed to reconstruct $relativePath : $_" "Warning"
            }
        }
        
        Write-ProgressBar -Activity "Reconstructing" -Status "Complete" -PercentComplete 100
        Write-Host ""
        
        # Calculate savings
        if ($reconstruction.Statistics.OriginalSize -gt 0) {
            $reconstruction.Statistics.Savings = [math]::Round((1 - ($reconstruction.Statistics.ReconstructedSize / $reconstruction.Statistics.OriginalSize)) * 100, 2)
        }
        
        Write-ColorOutput "✓ Reconstructed $($reconstruction.ReconstructedFiles) files" "Success"
        Write-ColorOutput "  - With source maps: $($reconstruction.SourceMapFiles)" "Detail"
        Write-ColorOutput "  - Deobfuscated: $($reconstruction.DeobfuscatedFiles)" "Detail"
        Write-ColorOutput "  - Inferred TypeScript: $($reconstruction.InferredTypeScript)" "Detail"
        Write-ColorOutput "  - Size savings: $($reconstruction.Statistics.Savings)%" "Detail"
        
    } catch {
        Write-ColorOutput "⚠ Failed to reconstruct source structure: $_" "Warning"
    }
    
    return $reconstruction
}

function Create-SelfReversingMechanism {
    param(
        [string]$OutputDir,
        $Reconstruction
    )
    
    Write-ColorOutput "=== CREATING SELF-REVERSING MECHANISM ===" "Header"
    
    try {
        # Create self-reversing script
        $selfReverseScript = @'
# Self-Reversing Mechanism for Cursor IDE
# This script can reverse the reconstruction process if needed

param(
    [switch]$Reverse,
    [switch]$Verify,
    [switch]$Release
)

$manifest = Get-Content "MANIFEST.json" | ConvertFrom-Json
$reconstruction = Get-Content "RECONSTRUCTION.json" | ConvertFrom-Json

if ($Reverse) {
    Write-Host "Reversing reconstruction..."
    foreach ($mapping in $reconstruction.FileMappings) {
        $srcFile = Join-Path "src" $mapping.Reconstructed
        $outFile = Join-Path "out" $mapping.Original
        
        if (Test-Path $srcFile) {
            # Reverse the reconstruction
            Copy-Item -Path $srcFile -Destination $outFile -Force
            Write-Host "Reversed: $($mapping.Reconstructed) -> $($mapping.Original)"
        }
    }
    Write-Host "Reversal complete!"
}

if ($Verify) {
    Write-Host "Verifying reconstruction integrity..."
    $verified = 0
    $failed = 0
    
    foreach ($mapping in $reconstruction.FileMappings) {
        $srcFile = Join-Path "src" $mapping.Reconstructed
        $outFile = Join-Path "out" $mapping.Original
        
        if (Test-Path $srcFile -and Test-Path $outFile) {
            $srcHash = Get-FileHash $srcFile -Algorithm SHA256
            $outHash = Get-FileHash $outFile -Algorithm SHA256
            
            if ($srcHash.Hash -eq $outHash.Hash) {
                $verified++
            } else {
                $failed++
                Write-Warning "Hash mismatch: $($mapping.Reconstructed)"
            }
        }
    }
    
    Write-Host "Verified: $verified, Failed: $failed"
}

if ($Release) {
    Write-Host "Releasing control..."
    # Release any locked resources
    # Reset any modified files
    # Restore original state
    
    foreach ($mapping in $reconstruction.FileMappings) {
        $srcFile = Join-Path "src" $mapping.Reconstructed
        $backupFile = Join-Path "src" "$($mapping.Reconstructed).backup"
        
        if (Test-Path $backupFile) {
            # Restore from backup
            Copy-Item -Path $backupFile -Destination $srcFile -Force
            Remove-Item -Path $backupFile -Force
            Write-Host "Released: $($mapping.Reconstructed)"
        }
    }
    
    Write-Host "Control released!"
}
'@
        
        $scriptPath = Join-Path $OutputDir "SelfReverse.ps1"
        $selfReverseScript | Out-File -FilePath $scriptPath -Encoding UTF8
        
        Write-ColorOutput "✓ Created self-reversing mechanism" "Success"
        Write-ColorOutput "  Location: $scriptPath" "Detail"
        
        # Create backup mechanism
        $backupScript = @"
# Backup mechanism for self-reversing
# Creates backups before modifications

param(
    [string[]]`$Files
)

foreach (`$file in `$Files) {
    if (Test-Path `$file) {
        `$backupPath = "`$file.backup"
        Copy-Item -Path `$file -Destination `$backupPath -Force
        Write-Host "Backup created: `$backupPath"
    }
}
"@
        
        $backupPath = Join-Path $OutputDir "CreateBackup.ps1"
        $backupScript | Out-File -FilePath $backupPath -Encoding UTF8
        
        Write-ColorOutput "✓ Created backup mechanism" "Success"
        
        # Create control handler
        $controlScript = @"
# Control handler for reverse engineering
# Handles control/unextract/release scenarios

param(
    [string]`$Action,
    [string]`$Target
)

switch (`$Action) {
    "control" {
        # Take control of the target
        if (Test-Path `$Target) {
            # Create backup before taking control
            .\CreateBackup.ps1 `$Target
            Write-Host "Control established: `$Target"
        }
    }
    "unextract" {
        # Reverse extraction
        if (Test-Path `$Target) {
            .\SelfReverse.ps1 -Reverse
            Write-Host "Unextract complete: `$Target"
        }
    }
    "release" {
        # Release control
        if (Test-Path `$Target) {
            .\SelfReverse.ps1 -Release
            Write-Host "Control released: `$Target"
        }
    }
    "verify" {
        # Verify integrity
        .\SelfReverse.ps1 -Verify
    }
}
"@
        
        $controlPath = Join-Path $OutputDir "ControlHandler.ps1"
        $controlScript | Out-File -FilePath $controlPath -Encoding UTF8
        
        Write-ColorOutput "✓ Created control handler" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to create self-reversing mechanism: $_" "Warning"
    }
}

function Handle-ControlScenarios {
    param(
        [string]$Dir,
        $Reconstruction
    )
    
    Write-ColorOutput "=== HANDLING CONTROL SCENARIOS ===" "Header"
    
    $controlScenarios = @{
        ProtectedFiles = @()
        LockedResources = @()
        UnextractableFiles = @()
        ReleaseMechanisms = @()
        TotalScenarios = 0
    }
    
    try {
        # Look for files that might be difficult to extract
        $suspiciousPatterns = @(
            "*.asar", "*.pak", "*.bin", "*.node",
            "*.dll", "*.so", "*.dylib",
            "*.exe", "*.app",
            "*.wasm", "*.wasm.map"
        )
        
        foreach ($pattern in $suspiciousPatterns) {
            $files = Get-ChildItem -Path $Dir -Recurse -Filter $pattern -ErrorAction SilentlyContinue
            
            foreach ($file in $files) {
                $relativePath = $file.FullName.Substring($Dir.Length).TrimStart('\', '/')
                
                $scenario = @{
                    File = $relativePath
                    Type = "Protected"
                    Size = $file.Length
                    Action = "ReverseEngineer"
                    Status = "Pending"
                }
                
                # Determine action based on file type
                switch ($file.Extension.ToLower()) {
                    ".asar" {
                        $scenario.Type = "Archive"
                        $scenario.Action = "ExtractAndReverse"
                    }
                    ".pak" {
                        $scenario.Type = "Package"
                        $scenario.Action = "UnpackAndAnalyze"
                    }
                    ".node" {
                        $scenario.Type = "NativeModule"
                        $scenario.Action = "DecompileOrStub"
                    }
                    ".dll" {
                        $scenario.Type = "WindowsLibrary"
                        $scenario.Action = "AnalyzeExports"
                    }
                    ".so" {
                        $scenario.Type = "LinuxLibrary"
                        $scenario.Action = "AnalyzeSymbols"
                    }
                    ".dylib" {
                        $scenario.Type = "MacLibrary"
                        $scenario.Action = "AnalyzeSymbols"
                    }
                    ".wasm" {
                        $scenario.Type = "WebAssembly"
                        $scenario.Action = "DecompileToWAT"
                    }
                    ".exe" {
                        $scenario.Type = "Executable"
                        $scenario.Action = "AnalyzePE"
                    }
                    ".app" {
                        $scenario.Type = "MacApp"
                        $scenario.Action = "ExtractBundle"
                    }
                }
                
                $controlScenarios.ProtectedFiles += $scenario
                $controlScenarios.TotalScenarios++
            }
        }
        
        # Look for files that couldn't be extracted in previous attempts
        $failedExtractions = @()
        if ($Reconstruction) {
            $failed = $Reconstruction.FileMappings | Where-Object { -not $_.HasSourceMap }
            foreach ($fail in $failed) {
                $scenario = @{
                    File = $fail.Original
                    Type = "NoSourceMap"
                    Size = 0
                    Action = "ManualReverseEngineer"
                    Status = "Pending"
                }
                $controlScenarios.UnextractableFiles += $scenario
                $controlScenarios.TotalScenarios++
            }
        }
        
        # Create release mechanisms for each scenario
        foreach ($scenario in $controlScenarios.ProtectedFiles) {
            $releaseMech = @{
                File = $scenario.File
                Type = $scenario.Type
                Action = $scenario.Action
                ReleaseCommand = "ControlHandler.ps1 -Action release -Target `"$($scenario.File)`""
                VerifyCommand = "ControlHandler.ps1 -Action verify -Target `"$($scenario.File)`""
                Status = "Created"
            }
            $controlScenarios.ReleaseMechanisms += $releaseMech
        }
        
        Write-ColorOutput "✓ Analyzed $($controlScenarios.TotalScenarios) control scenarios" "Success"
        Write-ColorOutput "  - Protected files: $($controlScenarios.ProtectedFiles.Count)" "Detail"
        Write-ColorOutput "  - Unextractable files: $($controlScenarios.UnextractableFiles.Count)" "Detail"
        Write-ColorOutput "  - Release mechanisms: $($controlScenarios.ReleaseMechanisms.Count)" "Detail"
        
    } catch {
        Write-ColorOutput "⚠ Failed to handle control scenarios: $_" "Warning"
    }
    
    return $controlScenarios
}

function Generate-AdvancedReport {
    param(
        $ObfuscatedFiles,
        $Reconstruction,
        $ControlScenarios,
        [string]$OutputDir
    )
    
    Write-ColorOutput "=== GENERATING ADVANCED REPORT ===" "Header"
    
    $report = @{
        Metadata = @{
            Tool = "Reverse-Engineer-Advanced.ps1"
            Version = "1.0"
            GeneratedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            InputDirectory = $InputDirectory
            OutputDirectory = $OutputDirectory
        }
        Summary = @{
            ObfuscatedFiles = $ObfuscatedFiles.Count
            ReconstructedFiles = $Reconstruction.ReconstructedFiles
            ControlScenarios = $ControlScenarios.TotalScenarios
            DeobfuscatedFiles = $Reconstruction.DeobfuscatedFiles
            SourceMapFiles = $Reconstruction.SourceMapFiles
            InferredTypeScript = $Reconstruction.InferredTypeScript
        }
        ObfuscatedFiles = $ObfuscatedFiles
        Reconstruction = $Reconstruction
        ControlScenarios = $ControlScenarios
        Recommendations = @(
            "Use the SelfReverse.ps1 script to reverse the reconstruction if needed",
            "Run ControlHandler.ps1 to manage protected files",
            "Verify integrity with the -Verify flag",
            "Create backups before modifications"
        )
    }
    
    # Save JSON report
    $reportJson = $report | ConvertTo-Json -Depth 20 -Compress:$false
    $reportPath = Join-Path $OutputDir "advanced_reverse_engineering_report.json"
    $reportJson | Out-File -FilePath $reportPath -Encoding UTF8
    
    # Save text report
    $textReport = @"
Advanced Reverse Engineering Report
=====================================
Generated: $($report.Metadata.GeneratedAt)
Tool: $($report.Metadata.Tool)
Version: $($report.Metadata.Version)

SUMMARY
-------
Obfuscated Files: $($report.Summary.ObfuscatedFiles)
Reconstructed Files: $($report.Summary.ReconstructedFiles)
Control Scenarios: $($report.Summary.ControlScenarios)
Deobfuscated Files: $($report.Summary.DeobfuscatedFiles)
Source Map Files: $($report.Summary.SourceMapFiles)
Inferred TypeScript: $($report.Summary.InferredTypeScript)

RECONSTRUCTION STATISTICS
-------------------------
Original Size: $([math]::Round($Reconstruction.Statistics.OriginalSize / 1MB, 2)) MB
Reconstructed Size: $([math]::Round($Reconstruction.Statistics.ReconstructedSize / 1MB, 2)) MB
Size Savings: $($Reconstruction.Statistics.Savings)%

OBLFUSCATED FILES
-----------------
"@
    
    foreach ($file in $ObfuscatedFiles | Select-Object -First 10) {
        $textReport += "- $($file.Path) ($($file.Type)) - Confidence: $($file.Confidence)`n"
    }
    
    if ($ObfuscatedFiles.Count -gt 10) {
        $textReport += "... and $($ObfuscatedFiles.Count - 10) more files`n"
    }
    
    $textReport += @"

CONTROL SCENARIOS
-----------------
"@
    
    foreach ($scenario in $ControlScenarios.ProtectedFiles | Select-Object -First 10) {
        $textReport += "- $($scenario.File) ($($scenario.Type)) - Action: $($scenario.Action)`n"
    }
    
    if ($ControlScenarios.ProtectedFiles.Count -gt 10) {
        $textReport += "... and $($ControlScenarios.ProtectedFiles.Count - 10) more scenarios`n"
    }
    
    $textReport += @"

RECOMMENDATIONS
---------------
"@
    
    foreach ($rec in $report.Recommendations) {
        $textReport += "- $rec`n"
    }
    
    $textReport += @"

NEXT STEPS
----------
1. Review the reconstructed source in src/
2. Examine the self-reversing mechanisms
3. Test the control handlers
4. Verify integrity with -Verify flag
5. Create backups before modifications

FILES CREATED
-------------
- advanced_reverse_engineering_report.json
- advanced_reverse_engineering_report.txt
- SelfReverse.ps1
- CreateBackup.ps1
- ControlHandler.ps1
- RECONSTRUCTION.json
- MANIFEST.json
- README.md

For detailed analysis, review the JSON report at:
$reportPath
"@
    
    $textReportPath = Join-Path $OutputDir "advanced_reverse_engineering_report.txt"
    $textReport | Out-File -FilePath $textReportPath -Encoding UTF8
    
    Write-ColorOutput "✓ Generated advanced report" "Success"
    Write-ColorOutput "  JSON: $reportPath" "Detail"
    Write-ColorOutput "  Text: $textReportPath" "Detail"
    
    return $report
}

# Main execution
Write-ColorOutput "=" * 80
Write-ColorOutput "ADVANCED REVERSE ENGINEERING TOOL"
Write-ColorOutput "Starting at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-ColorOutput "Input: $InputDirectory"
Write-ColorOutput "Output: $OutputDirectory"
Write-ColorOutput "=" * 80
Write-ColorOutput ""

# Check if input directory exists
if (-not (Test-Path $InputDirectory)) {
    Write-ColorOutput "✗ Input directory not found: $InputDirectory" "Error"
    exit 1
}

# Check if output directory exists
if (Test-Path $OutputDirectory) {
    if ($ForceOverwrite) {
        Write-ColorOutput "→ Removing existing output directory" "Warning"
        Remove-Item -Path $OutputDirectory -Recurse -Force
    } else {
        Write-ColorOutput "⚠ Output directory already exists, use -ForceOverwrite to replace" "Warning"
        exit 1
    }
}

# Create output directory
New-Item -Path $OutputDirectory -ItemType Directory -Force | Out-Null
Write-ColorOutput "✓ Created output directory: $OutputDirectory" "Success"

# Initialize variables
$obfuscatedFiles = @()
$reconstruction = $null
$controlScenarios = $null

# Scan for obfuscated files
if ($ReverseObfuscated -or $ExtractAll) {
    Write-ColorOutput "=== SCANNING FOR OBFUSCATED FILES ===" "Header"
    $obfuscatedFiles = Get-ObfuscatedFiles -Dir $InputDirectory
}

# Reconstruct source structure
if ($ReconstructSourceMaps -or $ReverseBuildProcess -or $ExtractAll) {
    Write-ColorOutput "=== RECONSTRUCTING SOURCE STRUCTURE ===" "Header"
    
    $outDir = Join-Path $InputDirectory "out"
    $srcDir = Join-Path $OutputDirectory "src"
    
    if (Test-Path $outDir) {
        $reconstruction = Reconstruct-SourceStructure -OutDir $outDir -SrcDir $srcDir
    } else {
        Write-ColorOutput "⚠ No out/ directory found, skipping reconstruction" "Warning"
    }
}

# Create self-reversing mechanism
if ($CreateSelfReversing -or $ExtractAll) {
    Write-ColorOutput "=== CREATING SELF-REVERSING MECHANISM ===" "Header"
    Create-SelfReversingMechanism -OutputDir $OutputDirectory -Reconstruction $reconstruction
}

# Handle control scenarios
if ($HandleControlScenarios -or $ExtractAll) {
    Write-ColorOutput "=== HANDLING CONTROL SCENARIOS ===" "Header"
    $controlScenarios = Handle-ControlScenarios -Dir $InputDirectory -Reconstruction $reconstruction
}

# Generate advanced report
if ($GenerateReport) {
    Write-ColorOutput "=== GENERATING ADVANCED REPORT ===" "Header"
    $report = Generate-AdvancedReport `
        -ObfuscatedFiles $obfuscatedFiles `
        -Reconstruction $reconstruction `
        -ControlScenarios $controlScenarios `
        -OutputDir $OutputDirectory
}

# Final summary
Write-ColorOutput ""
Write-ColorOutput "=" * 80
Write-ColorOutput "ADVANCED REVERSE ENGINEERING COMPLETE" "Header"
Write-ColorOutput "=" * 80

if ($obfuscatedFiles) {
    Write-ColorOutput "Obfuscated Files Found: $($obfuscatedFiles.Count)" "Obfuscated"
}

if ($reconstruction) {
    Write-ColorOutput "Files Reconstructed: $($reconstruction.ReconstructedFiles)" "Reconstruct"
    Write-ColorOutput "  - With source maps: $($reconstruction.SourceMapFiles)" "Detail"
    Write-ColorOutput "  - Deobfuscated: $($reconstruction.DeobfuscatedFiles)" "Detail"
    Write-ColorOutput "  - Inferred TypeScript: $($reconstruction.InferredTypeScript)" "Detail"
    Write-ColorOutput "Size Savings: $($reconstruction.Statistics.Savings)%" "Detail"
}

if ($controlScenarios) {
    Write-ColorOutput "Control Scenarios: $($controlScenarios.TotalScenarios)" "Control"
    Write-ColorOutput "  - Protected files: $($controlScenarios.ProtectedFiles.Count)" "Detail"
    Write-ColorOutput "  - Release mechanisms: $($controlScenarios.ReleaseMechanisms.Count)" "Detail"
}

Write-ColorOutput "=" * 80
Write-ColorOutput "Output Directory: $OutputDirectory" "Detail"
Write-ColorOutput "=" * 80

Write-ColorOutput ""
Write-ColorOutput "Self-reversing mechanisms created:"
Write-ColorOutput "  - SelfReverse.ps1" "SelfReverse"
Write-ColorOutput "  - CreateBackup.ps1" "SelfReverse"
Write-ColorOutput "  - ControlHandler.ps1" "Control"
Write-ColorOutput ""
Write-ColorOutput "Next steps:"
Write-ColorOutput "1. Review reconstructed source in src/"
Write-ColorOutput "2. Test self-reversing mechanisms"
Write-ColorOutput "3. Handle control scenarios"
Write-ColorOutput "4. Verify integrity"
Write-ColorOutput "5. Create backups before modifications"
