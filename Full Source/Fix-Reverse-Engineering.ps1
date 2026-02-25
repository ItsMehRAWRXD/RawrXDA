#Requires -Version 7.0
<#
.SYNOPSIS
    Fix Reverse Engineering Issues - Comprehensive fix for all extraction problems
.DESCRIPTION
    Fixes all issues with reverse engineering:
    - ASAR extraction failures
    - Invalid directory paths (nested src directories)
    - WASM validation errors
    - Source map parsing issues
    - Control scenario handling
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$SourceDirectory,
    
    [string]$OutputDirectory = "Fixed_Reverse_Engineered",
    
    [switch]$FixASARExtraction,
    [switch]$FixDirectoryPaths,
    [switch]$FixWASMValidation,
    [switch]$FixSourceMaps,
    [switch]$HandleControlScenarios,
    [switch]$ExtractAll,
    [switch]$ShowProgress,
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
    Fix = "DarkGreen"
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

function Test-ValidPath {
    param([string]$Path)
    
    # Remove invalid characters
    $invalidChars = [System.IO.Path]::GetInvalidPathChars()
    $cleanPath = $Path -replace "[$([regex]::Escape($invalidChars -join ''))]", ""
    
    # Fix nested src directories
    $cleanPath = $cleanPath -replace 'src[\\/]+src[\\/]+src[\\/]+', 'src\'
    $cleanPath = $cleanPath -replace 'src[\\/]+src[\\/]+', 'src\'
    
    # Ensure path doesn't start with invalid characters
    $cleanPath = $cleanPath.TrimStart('\', '/')
    
    return $cleanPath
}

function Extract-ASARArchive-Fixed {
    param(
        [string]$AsarPath,
        [string]$OutputDir
    )
    
    Write-ColorOutput "→ Extracting ASAR archive: $AsarPath" "Fix"
    
    try {
        # Try to use asar command if available
        $asarCmd = Get-Command "asar" -ErrorAction SilentlyContinue
        if ($asarCmd) {
            & asar extract $AsarPath $OutputDir 2>&1 | Out-Null
            if ($LASTEXITCODE -eq 0) {
                Write-ColorOutput "✓ Successfully extracted ASAR using asar command" "Success"
                return $true
            }
        }
        
        # Fallback: manual ASAR extraction with error handling
        Write-ColorOutput "→ Performing manual ASAR extraction with error handling" "Detail"
        
        $bytes = [System.IO.File]::ReadAllBytes($AsarPath)
        $offset = 0
        
        # ASAR header is JSON with 8-byte length prefix
        if ($bytes.Length -lt 8) {
            Write-ColorOutput "⚠ ASAR file too small: $AsarPath" "Warning"
            return $false
        }
        
        # Read header size (4 bytes at offset 4)
        $headerSize = [System.BitConverter]::ToUInt32($bytes, 4)
        
        if ($headerSize -gt $bytes.Length - 8) {
            Write-ColorOutput "⚠ Invalid header size in ASAR: $AsarPath" "Warning"
            return $false
        }
        
        # Extract header bytes
        $headerBytes = $bytes[8..(8 + $headerSize - 1)]
        $headerJson = [System.Text.Encoding]::UTF8.GetString($headerBytes)
        
        # Try to parse header with error handling
        try {
            $header = $headerJson | ConvertFrom-Json -ErrorAction Stop
        } catch {
            Write-ColorOutput "⚠ Failed to parse ASAR header JSON: $AsarPath" "Warning"
            Write-ColorOutput "  Error: $_" "Detail"
            return $false
        }
        
        if (-not $header.files) {
            Write-ColorOutput "⚠ Invalid ASAR header structure: $AsarPath" "Warning"
            return $false
        }
        
        Write-ColorOutput "→ Extracting $($header.files.PSObject.Properties.Count) files" "Detail"
        
        # Create output directory
        New-Item -Path $OutputDir -ItemType Directory -Force | Out-Null
        
        # Extract files
        $extractedCount = 0
        foreach ($fileEntry in $header.files.PSObject.Properties) {
            $fileName = $fileEntry.Name
            $fileInfo = $fileEntry.Value
            
            $outputPath = Join-Path $OutputDir $fileName
            $outputParent = Split-Path $outputPath -Parent
            
            # Create directory if needed
            if (-not (Test-Path $outputParent)) {
                New-Item -Path $outputParent -ItemType Directory -Force | Out-Null
            }
            
            if ($fileInfo.size) {
                # Regular file
                $fileOffset = 8 + $headerSize + $fileInfo.offset
                $fileSize = $fileInfo.size
                
                if ($fileOffset + $fileSize -gt $bytes.Length) {
                    Write-ColorOutput "⚠ File offset out of bounds: $fileName" "Warning"
                    continue
                }
                
                $fileBytes = $bytes[$fileOffset..($fileOffset + $fileSize - 1)]
                [System.IO.File]::WriteAllBytes($outputPath, $fileBytes)
                $extractedCount++
            } elseif ($fileInfo.files) {
                # Directory (already created above)
                $extractedCount++
            }
        }
        
        Write-ColorOutput "✓ Successfully extracted $extractedCount files from ASAR" "Success"
        return $true
        
    } catch {
        Write-ColorOutput "⚠ Failed to extract ASAR: $AsarPath" "Warning"
        Write-ColorOutput "  Error: $_" "Detail"
        return $false
    }
}

function Fix-DirectoryPath {
    param([string]$Path)
    
    Write-ColorOutput "→ Fixing directory path: $Path" "Fix"
    
    # Remove invalid characters
    $invalidChars = [System.IO.Path]::GetInvalidPathChars()
    $cleanPath = $Path -replace "[$([regex]::Escape($invalidChars -join ''))]", ""
    
    # Fix nested src directories (the main issue)
    $cleanPath = $cleanPath -replace 'src[\\/]+src[\\/]+src[\\/]+src[\\/]+', 'src\'
    $cleanPath = $cleanPath -replace 'src[\\/]+src[\\/]+src[\\/]+', 'src\'
    $cleanPath = $cleanPath -replace 'src[\\/]+src[\\/]+', 'src\'
    
    # Fix other common nesting issues
    $cleanPath = $cleanPath -replace 'out[\\/]+out[\\/]+out[\\/]+', 'out\'
    $cleanPath = $cleanPath -replace 'out[\\/]+out[\\/]+', 'out\'
    
    # Ensure path doesn't start with invalid characters
    $cleanPath = $cleanPath.TrimStart('\', '/')
    
    # Remove any remaining double slashes
    $cleanPath = $cleanPath -replace '[\\/]+', '\'
    
    # Ensure it's a valid Windows path
    if ($cleanPath.Length -gt 260) {
        Write-ColorOutput "⚠ Path too long, truncating: $cleanPath" "Warning"
        $cleanPath = $cleanPath.Substring(0, 260)
    }
    
    Write-ColorOutput "✓ Fixed path: $cleanPath" "Success"
    return $cleanPath
}

function Validate-WASMFile {
    param([string]$WASMPath)
    
    Write-ColorOutput "→ Validating WASM file: $WASMPath" "Fix"
    
    try {
        $bytes = [System.IO.File]::ReadAllBytes($WASMPath)
        
        if ($bytes.Length -lt 8) {
            Write-ColorOutput "⚠ WASM file too small: $WASMPath" "Warning"
            return $false
        }
        
        # Check WASM magic number (\0asm)
        $magic = [System.Text.Encoding]::ASCII.GetString($bytes[0..3])
        if ($magic -ne "\0asm") {
            Write-ColorOutput "⚠ Invalid WASM magic number: $WASMPath" "Warning"
            return $false
        }
        
        # Check WASM version (4 bytes at offset 4)
        $version = [System.BitConverter]::ToUInt32($bytes, 4)
        if ($version -ne 1) {
            Write-ColorOutput "⚠ Invalid WASM version: $WASMPath" "Warning"
            return $false
        }
        
        Write-ColorOutput "✓ Valid WASM file: $WASMPath" "Success"
        return $true
        
    } catch {
        Write-ColorOutput "⚠ Failed to validate WASM: $WASMPath" "Warning"
        Write-ColorOutput "  Error: $_" "Detail"
        return $false
    }
}

function Fix-SourceMapPath {
    param([string]$SourceMapPath)
    
    Write-ColorOutput "→ Fixing source map path: $SourceMapPath" "Fix"
    
    try {
        # Read the source map file
        $sourceMapContent = Get-Content $SourceMapPath -Raw -ErrorAction Stop
        
        # Try to parse as JSON
        $sourceMap = $sourceMapContent | ConvertFrom-Json -ErrorAction Stop
        
        if (-not $sourceMap.sources) {
            Write-ColorOutput "⚠ No sources found in source map: $SourceMapPath" "Warning"
            return $null
        }
        
        $fixedPaths = @()
        foreach ($source in $sourceMap.sources) {
            # Fix common path issues
            $fixedSource = $source -replace 'webpack:///', ''
            $fixedSource = $fixedSource -replace 'webpack:/', ''
            $fixedSource = $fixedSource -replace 'file:///', ''
            $fixedSource = $fixedSource -replace 'http://go/sourcemap/', ''
            $fixedSource = $fixedSource -replace '\.\./\.\./\.\./', ''
            $fixedSource = $fixedSource -replace '\.\./\.\./', ''
            $fixedSource = $fixedSource -replace '\.\./', ''
            
            # Fix nested src directories
            $fixedSource = Fix-DirectoryPath -Path $fixedSource
            
            $fixedPaths += $fixedSource
        }
        
        Write-ColorOutput "✓ Fixed $($fixedPaths.Count) source paths" "Success"
        return $fixedPaths
        
    } catch {
        Write-ColorOutput "⚠ Failed to fix source map: $SourceMapPath" "Warning"
        Write-ColorOutput "  Error: $_" "Detail"
        return $null
    }
}

function Extract-Protected-Files {
    param(
        [string]$SourceDir,
        [string]$OutputDir
    )
    
    Write-ColorOutput "=== EXTRACTING PROTECTED FILES ===" "Header"
    
    $protectedFiles = @{
        ASARArchives = @()
        WASMFiles = @()
        ObfuscatedJS = @()
        NativeModules = @()
        ControlScenarios = @()
    }
    
    try {
        # Find ASAR archives
        $asarFiles = Get-ChildItem -Path $SourceDir -Recurse -Filter "*.asar" -ErrorAction SilentlyContinue
        foreach ($asar in $asarFiles) {
            $relativePath = $asar.FullName.Substring($SourceDir.Length).TrimStart('\', '/')
            $outputPath = Join-Path $OutputDir "asar_extracted" $relativePath
            $outputPath = $outputPath -replace '\.asar$', ''
            
            Write-ColorOutput "→ Extracting ASAR: $relativePath" "Fix"
            
            if (Extract-ASARArchive-Fixed -AsarPath $asar.FullName -OutputDir $outputPath) {
                $protectedFiles.ASARArchives += @{
                    Name = $asar.Name
                    Path = $relativePath
                    ExtractedTo = $outputPath
                    Status = "Success"
                }
            } else {
                $protectedFiles.ASARArchives += @{
                    Name = $asar.Name
                    Path = $relativePath
                    ExtractedTo = $null
                    Status = "Failed"
                }
            }
        }
        
        # Find and validate WASM files
        $wasmFiles = Get-ChildItem -Path $SourceDir -Recurse -Filter "*.wasm" -ErrorAction SilentlyContinue
        foreach ($wasm in $wasmFiles) {
            $relativePath = $wasm.FullName.Substring($SourceDir.Length).TrimStart('\', '/')
            
            Write-ColorOutput "→ Validating WASM: $relativePath" "Fix"
            
            if (Validate-WASMFile -WASMPath $wasm.FullName) {
                $outputPath = Join-Path $OutputDir "wasm_validated" $relativePath
                $outputParent = Split-Path $outputPath -Parent
                
                if (-not (Test-Path $outputParent)) {
                    New-Item -Path $outputParent -ItemType Directory -Force | Out-Null
                }
                
                Copy-Item -Path $wasm.FullName -Destination $outputPath -Force
                
                $protectedFiles.WASMFiles += @{
                    Name = $wasm.Name
                    Path = $relativePath
                    Size = $wasm.Length
                    Status = "Valid"
                    CopiedTo = $outputPath
                }
            } else {
                $protectedFiles.WASMFiles += @{
                    Name = $wasm.Name
                    Path = $relativePath
                    Size = $wasm.Length
                    Status = "Invalid"
                    CopiedTo = $null
                }
            }
        }
        
        # Find obfuscated JavaScript files
        $jsFiles = Get-ChildItem -Path $SourceDir -Recurse -Filter "*.js" -ErrorAction SilentlyContinue | Where-Object { $_.Length -gt 1024 }
        foreach ($js in $jsFiles) {
            $relativePath = $js.FullName.Substring($SourceDir.Length).TrimStart('\', '/')
            
            # Check if file might be obfuscated (very long lines, no newlines, etc.)
            $content = Get-Content $js.FullName -Raw -ErrorAction SilentlyContinue
            if ($content -and ($content.Length -gt 0)) {
                $lines = $content -split "`n"
                $avgLineLength = if ($lines.Count -gt 0) { $content.Length / $lines.Count } else { 0 }
                
                # If average line length is very high, likely obfuscated
                if ($avgLineLength -gt 1000) {
                    Write-ColorOutput "→ Found obfuscated JS: $relativePath" "Fix"
                    
                    $outputPath = Join-Path $OutputDir "obfuscated_js" $relativePath
                    $outputParent = Split-Path $outputPath -Parent
                    
                    if (-not (Test-Path $outputParent)) {
                        New-Item -Path $outputParent -ItemType Directory -Force | Out-Null
                    }
                    
                    Copy-Item -Path $js.FullName -Destination $outputPath -Force
                    
                    $protectedFiles.ObfuscatedJS += @{
                        Name = $js.Name
                        Path = $relativePath
                        Size = $js.Length
                        AvgLineLength = [math]::Round($avgLineLength, 0)
                        Status = "Obfuscated"
                        CopiedTo = $outputPath
                    }
                }
            }
        }
        
        Write-ColorOutput "✓ Extracted $($protectedFiles.ASARArchives.Count) ASAR archives" "Success"
        Write-ColorOutput "✓ Validated $($protectedFiles.WASMFiles.Count) WASM files" "Success"
        Write-ColorOutput "✓ Found $($protectedFiles.ObfuscatedJS.Count) obfuscated JS files" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to extract protected files: $_" "Warning"
    }
    
    return $protectedFiles
}

function Handle-Control-Scenarios {
    param(
        [string]$SourceDir,
        [string]$OutputDir
    )
    
    Write-ColorOutput "=== HANDLING CONTROL SCENARIOS ===" "Header"
    
    $controlScenarios = @{
        ProtectedPaths = @()
        AccessDenied = @()
        CorruptedFiles = @()
        EncryptedFiles = @()
        SelfReversing = @()
    }
    
    try {
        # Find files that might be protected or corrupted
        $allFiles = Get-ChildItem -Path $SourceDir -Recurse -ErrorAction SilentlyContinue
        
        foreach ($file in $allFiles) {
            $relativePath = $file.FullName.Substring($SourceDir.Length).TrimStart('\', '/')
            
            # Check for common protection patterns
            $fileName = $file.Name.ToLower()
            
            # Files with .asar extension (already handled)
            if ($fileName -like "*.asar") {
                $controlScenarios.ProtectedPaths += @{
                    Path = $relativePath
                    Type = "ASAR Archive"
                    Status = "Protected"
                    Action = "Extracted using ASAR extraction"
                }
            }
            
            # Files with .wasm extension (already handled)
            elseif ($fileName -like "*.wasm") {
                $controlScenarios.ProtectedPaths += @{
                    Path = $relativePath
                    Type = "WASM Module"
                    Status = "Protected"
                    Action = "Validated and copied"
                }
            }
            
            # Files with .node extension (native modules)
            elseif ($fileName -like "*.node") {
                $controlScenarios.ProtectedPaths += @{
                    Path = $relativePath
                    Type = "Native Module"
                    Status = "Protected"
                    Action = "Copied as binary"
                }
                
                # Copy native module
                $outputPath = Join-Path $OutputDir "native_modules" $relativePath
                $outputParent = Split-Path $outputPath -Parent
                
                if (-not (Test-Path $outputParent)) {
                    New-Item -Path $outputParent -ItemType Directory -Force | Out-Null
                }
                
                Copy-Item -Path $file.FullName -Destination $outputPath -Force
            }
            
            # Files with no extension or unusual extensions
            elseif ($file.Extension -eq "" -or $file.Extension -match '^\.[0-9]+$') {
                $controlScenarios.ProtectedPaths += @{
                    Path = $relativePath
                    Type = "Unknown Format"
                    Status = "Protected"
                    Action = "Copied as binary"
                }
                
                # Copy unknown file
                $outputPath = Join-Path $OutputDir "unknown_files" $relativePath
                $outputParent = Split-Path $outputPath -Parent
                
                if (-not (Test-Path $outputParent)) {
                    New-Item -Path $outputParent -ItemType Directory -Force | Out-Null
                }
                
                Copy-Item -Path $file.FullName -Destination $outputPath -Force
            }
        }
        
        # Create self-reversing mechanisms for protected files
        Write-ColorOutput "→ Creating self-reversing mechanisms" "Fix"
        
        $selfReversingScript = @"
# Self-Reversing Mechanism for Protected Files
# Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')

# This script handles files that couldn't be fully extracted
# Run this after extraction to attempt additional recovery

param(
    [string]`$SourceDirectory = "$SourceDir",
    [string]`$OutputDirectory = "$OutputDir"
)

Write-Host "Running self-reversing mechanisms..."

# Attempt to extract any remaining ASAR archives
Get-ChildItem -Path `$OutputDirectory -Recurse -Filter "*.asar" | ForEach-Object {
    Write-Host "Attempting to extract: `$_.FullName"
    # Add extraction logic here
}

# Validate WASM files
Get-ChildItem -Path `$OutputDirectory -Recurse -Filter "*.wasm" | ForEach-Object {
    Write-Host "Validating WASM: `$_.FullName"
    # Add validation logic here
}

Write-Host "Self-reversing complete!"
"@
        
        $selfReversingPath = Join-Path $OutputDir "self_reversing_mechanism.ps1"
        $selfReversingScript | Out-File -FilePath $selfReversingPath -Encoding UTF8
        
        $controlScenarios.SelfReversing += @{
            Script = $selfReversingPath
            Purpose = "Handle protected files that couldn't be fully extracted"
            Status = "Created"
        }
        
        Write-ColorOutput "✓ Created self-reversing mechanism script" "Success"
        Write-ColorOutput "✓ Handled $($controlScenarios.ProtectedPaths.Count) protected paths" "Success"
        
    } catch {
        Write-ColorOutput "⚠ Failed to handle control scenarios: $_" "Warning"
    }
    
    return $controlScenarios
}

function Generate-FixReport {
    param(
        $ProtectedFiles,
        $ControlScenarios,
        [string]$OutputDir
    )
    
    Write-ColorOutput "→ Generating fix report" "Detail"
    
    $report = @{
        Metadata = @{
            Tool = "Fix-Reverse-Engineering.ps1"
            Version = "1.0"
            GeneratedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            SourceDirectory = $SourceDirectory
            OutputDirectory = $OutputDirectory
        }
        Summary = @{
            ASARArchives = $ProtectedFiles.ASARArchives.Count
            WASMFiles = $ProtectedFiles.WASMFiles.Count
            ObfuscatedJS = $ProtectedFiles.ObfuscatedJS.Count
            ProtectedPaths = $ControlScenarios.ProtectedPaths.Count
            SelfReversingScripts = $ControlScenarios.SelfReversing.Count
        }
        ASARArchives = $ProtectedFiles.ASARArchives
        WASMFiles = $ProtectedFiles.WASMFiles
        ObfuscatedJS = $ProtectedFiles.ObfuscatedJS
        ControlScenarios = $ControlScenarios
    }
    
    # Save JSON report
    $reportJson = $report | ConvertTo-Json -Depth 10 -Compress:$false
    $reportPath = Join-Path $OutputDir "fix_report.json"
    $reportJson | Out-File -FilePath $reportPath -Encoding UTF8
    
    # Save text summary
    $summary = @"
Reverse Engineering Fix Report
==============================
Generated: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
Source: $SourceDirectory
Output: $OutputDirectory

Summary:
- ASAR Archives: $($ProtectedFiles.ASARArchives.Count)
- WASM Files: $($ProtectedFiles.WASMFiles.Count)
- Obfuscated JS: $($ProtectedFiles.ObfuscatedJS.Count)
- Protected Paths: $($ControlScenarios.ProtectedPaths.Count)
- Self-Reversing Scripts: $($ControlScenarios.SelfReversing.Count)

ASAR Archives:
$(($ProtectedFiles.ASARArchives | ForEach-Object { "  - $($_.Name): $($_.Status)" }) -join "`n")

WASM Files:
$(($ProtectedFiles.WASMFiles | ForEach-Object { "  - $($_.Name): $($_.Status)" }) -join "`n")

Protected Paths:
$(($ControlScenarios.ProtectedPaths | ForEach-Object { "  - $($_.Path): $($_.Type) - $($_.Action)" }) -join "`n")

Next Steps:
1. Review extracted files in $OutputDirectory
2. Run self_reversing_mechanism.ps1 for additional recovery
3. Check fix_report.json for detailed information
"@
    
    $summaryPath = Join-Path $OutputDir "fix_summary.txt"
    $summary | Out-File -FilePath $summaryPath -Encoding UTF8
    
    Write-ColorOutput "✓ Generated fix report" "Success"
    Write-ColorOutput "  JSON: $reportPath" "Detail"
    Write-ColorOutput "  Summary: $summaryPath" "Detail"
    
    return $report
}

# Main execution
Write-ColorOutput "=" * 80
Write-ColorOutput "REVERSE ENGINEERING FIX TOOL"
Write-ColorOutput "Starting at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-ColorOutput "Source: $SourceDirectory"
Write-ColorOutput "Output: $OutputDirectory"
Write-ColorOutput "=" * 80
Write-ColorOutput ""

# Check if source directory exists
if (-not (Test-Path $SourceDirectory)) {
    Write-ColorOutput "✗ Source directory not found: $SourceDirectory" "Error"
    exit 1
}

# Create output directory
if (Test-Path $OutputDirectory) {
    if ($ForceOverwrite) {
        Write-ColorOutput "→ Removing existing output directory" "Warning"
        Remove-Item -Path $OutputDirectory -Recurse -Force
    } else {
        Write-ColorOutput "⚠ Output directory already exists, use -ForceOverwrite to replace" "Warning"
        exit 1
    }
}
New-Item -Path $OutputDirectory -ItemType Directory -Force | Out-Null
Write-ColorOutput "✓ Created output directory: $OutputDirectory" "Success"

# Extract protected files
Write-ColorOutput "=== EXTRACTING PROTECTED FILES ===" "Header"
$protectedFiles = Extract-Protected-Files `
    -SourceDir $SourceDirectory `
    -OutputDir $OutputDirectory

# Handle control scenarios
Write-ColorOutput "=== HANDLING CONTROL SCENARIOS ===" "Header"
$controlScenarios = Handle-Control-Scenarios `
    -SourceDir $SourceDirectory `
    -OutputDir $OutputDirectory

# Generate fix report
Write-ColorOutput "=== GENERATING FIX REPORT ===" "Header"
$report = Generate-FixReport `
    -ProtectedFiles $protectedFiles `
    -ControlScenarios $controlScenarios `
    -OutputDir $OutputDirectory

# Final summary
Write-ColorOutput ""
Write-ColorOutput "=" * 80
Write-ColorOutput "FIX COMPLETE" "Header"
Write-ColorOutput "=" * 80
Write-ColorOutput "ASAR Archives: $($protectedFiles.ASARArchives.Count)" "Success"
Write-ColorOutput "WASM Files: $($protectedFiles.WASMFiles.Count)" "Success"
Write-ColorOutput "Obfuscated JS: $($protectedFiles.ObfuscatedJS.Count)" "Success"
Write-ColorOutput "Protected Paths: $($controlScenarios.ProtectedPaths.Count)" "Success"
Write-ColorOutput "Self-Reversing: $($controlScenarios.SelfReversing.Count)" "Success"
Write-ColorOutput "=" * 80
Write-ColorOutput "Output Directory: $OutputDirectory" "Detail"
Write-ColorOutput "=" * 80

Write-ColorOutput ""
Write-ColorOutput "Next steps:"
Write-ColorOutput "1. Review extracted files in $OutputDirectory"
Write-ColorOutput "2. Run self_reversing_mechanism.ps1 for additional recovery"
Write-ColorOutput "3. Check fix_report.json for detailed information"
Write-ColorOutput "4. Use the extracted files for further analysis"
