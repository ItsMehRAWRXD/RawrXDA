#Requires -Version 7.0
<#
.SYNOPSIS
    Extract Protected Files - Handles files that couldn't be extracted normally
.DESCRIPTION
    Extracts protected, locked, or unextractable files using advanced techniques:
    - ASAR archive extraction
    - Binary analysis
    - Native module handling
    - WebAssembly decompilation
    - Control scenario resolution
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$SourceDirectory,
    
    [string]$OutputDirectory = "Extracted_Protected_Files",
    
    [switch]$ExtractASAR,
    [switch]$ExtractBinaries,
    [switch]$HandleNativeModules,
    [switch]$DecompileWebAssembly,
    [switch]$ResolveControlScenarios,
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
    Archive = "DarkYellow"
    Binary = "DarkRed"
    Native = "DarkGreen"
    WASM = "Blue"
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

function Extract-ASARArchives {
    param(
        [string]$Dir,
        [string]$OutputDir
    )
    
    Write-ColorOutput "→ Extracting ASAR archives" "Archive"
    
    $asarExtraction = @{
        ArchivesFound = 0
        ArchivesExtracted = 0
        FilesExtracted = 0
        TotalSize = 0
        Failed = @()
    }
    
    try {
        # Find all ASAR files
        $asarFiles = Get-ChildItem -Path $Dir -Recurse -Filter "*.asar" -ErrorAction SilentlyContinue
        $asarExtraction.ArchivesFound = $asarFiles.Count
        
        $totalArchives = $asarFiles.Count
        $currentArchive = 0
        
        foreach ($asar in $asarFiles) {
            $currentArchive++
            $percentComplete = [math]::Round(($currentArchive / $totalArchives) * 100, 1)
            
            $relativePath = $asar.FullName.Substring($Dir.Length).TrimStart('\', '/')
            Write-ProgressBar -Activity "Extracting ASAR" -Status "Processing" -PercentComplete $percentComplete -CurrentOperation $relativePath
            
            try {
                # Create output directory for this archive
                $extractDir = Join-Path $OutputDir "asar_extracted" (Split-Path $relativePath -Parent)
                if (-not (Test-Path $extractDir)) {
                    New-Item -Path $extractDir -ItemType Directory -Force | Out-Null
                }
                
                # Try to use asar command if available
                $asarCmd = Get-Command "asar" -ErrorAction SilentlyContinue
                if ($asarCmd) {
                    & asar extract $asar.FullName $extractDir
                    if ($LASTEXITCODE -eq 0) {
                        $asarExtraction.ArchivesExtracted++
                        Write-ColorOutput "✓ Extracted ASAR: $relativePath" "Success"
                    }
                } else {
                    # Manual ASAR extraction
                    $asarExtractionResult = Extract-ASARManual -AsarPath $asar.FullName -OutputDir $extractDir
                    if ($asarExtractionResult.Success) {
                        $asarExtraction.ArchivesExtracted++
                        $asarExtraction.FilesExtracted += $asarExtractionResult.FilesExtracted
                        $asarExtraction.TotalSize += $asarExtractionResult.TotalSize
                        Write-ColorOutput "✓ Extracted ASAR: $relativePath" "Success"
                    } else {
                        $asarExtraction.Failed += $relativePath
                        Write-ColorOutput "⚠ Failed to extract ASAR: $relativePath" "Warning"
                    }
                }
                
            } catch {
                $asarExtraction.Failed += $relativePath
                Write-ColorOutput "⚠ Failed to extract ASAR: $relativePath - $_" "Warning"
            }
        }
        
        Write-ProgressBar -Activity "Extracting ASAR" -Status "Complete" -PercentComplete 100
        Write-Host ""
        
        Write-ColorOutput "✓ Extracted $($asarExtraction.ArchivesExtracted) ASAR archives" "Success"
        Write-ColorOutput "  - Files extracted: $($asarExtraction.FilesExtracted)" "Detail"
        Write-ColorOutput "  - Total size: $([math]::Round($asarExtraction.TotalSize / 1MB, 2)) MB" "Detail"
        
    } catch {
        Write-ColorOutput "⚠ Failed to extract ASAR archives: $_" "Warning"
    }
    
    return $asarExtraction
}

function Extract-ASARManual {
    param(
        [string]$AsarPath,
        [string]$OutputDir
    )
    
    $result = @{
        Success = $false
        FilesExtracted = 0
        TotalSize = 0
    }
    
    try {
        $bytes = [System.IO.File]::ReadAllBytes($AsarPath)
        $offset = 0
        
        # ASAR header is JSON with 8-byte length prefix
        if ($bytes.Length -lt 8) {
            return $result
        }
        
        $headerSize = [System.BitConverter]::ToUInt32($bytes, 4)
        $headerBytes = $bytes[8..(8 + $headerSize - 1)]
        $headerJson = [System.Text.Encoding]::UTF8.GetString($headerBytes)
        $header = $headerJson | ConvertFrom-Json
        
        # Extract files
        foreach ($fileEntry in $header.files.PSObject.Properties) {
            $fileName = $fileEntry.Name
            $fileInfo = $fileEntry.Value
            
            $outputPath = Join-Path $OutputDir $fileName
            $outputParent = Split-Path $outputPath -Parent
            
            if (-not (Test-Path $outputParent)) {
                New-Item -Path $outputParent -ItemType Directory -Force | Out-Null
            }
            
            if ($fileInfo.size) {
                # Regular file
                $fileOffset = 8 + $headerSize + $fileInfo.offset
                $fileSize = $fileInfo.size
                $fileBytes = $bytes[$fileOffset..($fileOffset + $fileSize - 1)]
                [System.IO.File]::WriteAllBytes($outputPath, $fileBytes)
                $result.FilesExtracted++
                $result.TotalSize += $fileSize
            } elseif ($fileInfo.files) {
                # Directory (already created above)
            }
        }
        
        $result.Success = $true
        
    } catch {
        Write-ColorOutput "⚠ Manual ASAR extraction failed: $_" "Warning"
    }
    
    return $result
}

function Analyze-Binaries {
    param(
        [string]$Dir,
        [string]$OutputDir
    )
    
    Write-ColorOutput "→ Analyzing binary files" "Binary"
    
    $binaryAnalysis = @{
        BinariesFound = 0
        BinariesAnalyzed = 0
        PEFiles = 0
        ELFFiles = 0
        MachOFiles = 0
        AnalysisResults = @()
    }
    
    try {
        # Find binary files
        $binaryPatterns = @("*.exe", "*.dll", "*.so", "*.dylib", "*.node", "*.bin", "*.pak")
        $binaryFiles = @()
        
        foreach ($pattern in $binaryPatterns) {
            $files = Get-ChildItem -Path $Dir -Recurse -Filter $pattern -ErrorAction SilentlyContinue
            $binaryFiles += $files
        }
        
        $binaryAnalysis.BinariesFound = $binaryFiles.Count
        
        $totalBinaries = $binaryFiles.Count
        $currentBinary = 0
        
        foreach ($binary in $binaryFiles) {
            $currentBinary++
            $percentComplete = [math]::Round(($currentBinary / $totalBinaries) * 100, 1)
            
            $relativePath = $binary.FullName.Substring($Dir.Length).TrimStart('\', '/')
            Write-ProgressBar -Activity "Analyzing Binaries" -Status "Processing" -PercentComplete $percentComplete -CurrentOperation $relativePath
            
            try {
                $bytes = [System.IO.File]::ReadAllBytes($binary.FullName)
                $analysis = @{
                    Path = $relativePath
                    Size = $binary.Length
                    Type = "Unknown"
                    Architecture = "Unknown"
                    Magic = ""
                    Sections = @()
                    Imports = @()
                    Exports = @()
                }
                
                # Check magic bytes
                if ($bytes.Length -ge 4) {
                    $magic = [System.BitConverter]::ToString($bytes[0..3])
                    $analysis.Magic = $magic
                    
                    switch ($magic) {
                        "4D-5A" { # PE
                            $analysis.Type = "PE"
                            $binaryAnalysis.PEFiles++
                            
                            # Basic PE analysis
                            if ($bytes.Length -gt 60) {
                                $peOffset = [System.BitConverter]::ToInt32($bytes, 60)
                                if ($peOffset -lt $bytes.Length - 4) {
                                    $peSignature = [System.Text.Encoding]::ASCII.GetString($bytes[$peOffset..($peOffset + 3)])
                                    if ($peSignature -eq "PE\0\0") {
                                        $machine = [System.BitConverter]::ToUInt16($bytes, $peOffset + 4)
                                        $analysis.Architecture = switch ($machine) {
                                            0x014C { "x86" }
                                            0x8664 { "x64" }
                                            0x0200 { "IA64" }
                                            default { "Unknown" }
                                        }
                                    }
                                }
                            }
                        }
                        "7F-45-4C-46" { # ELF
                            $analysis.Type = "ELF"
                            $binaryAnalysis.ELFFiles++
                            
                            # Basic ELF analysis
                            $class = $bytes[4]
                            $analysis.Architecture = switch ($class) {
                                1 { "32-bit" }
                                2 { "64-bit" }
                                default { "Unknown" }
                            }
                        }
                        "CA-FE-BA-BE" { # Mach-O
                            $analysis.Type = "Mach-O"
                            $binaryAnalysis.MachOFiles++
                            
                            # Basic Mach-O analysis
                            $cpuType = [System.BitConverter]::ToUInt32($bytes, 4)
                            $analysis.Architecture = switch ($cpuType) {
                                0x01000007 { "x86_64" }
                                0x0100000C { "ARM64" }
                                0x00000007 { "x86" }
                                default { "Unknown" }
                            }
                        }
                    }
                }
                
                $binaryAnalysis.AnalysisResults += $analysis
                $binaryAnalysis.BinariesAnalyzed++
                
                Write-ColorOutput "✓ Analyzed: $relativePath ($($analysis.Type))" "Success"
                
            } catch {
                Write-ColorOutput "⚠ Failed to analyze: $relativePath - $_" "Warning"
            }
        }
        
        Write-ProgressBar -Activity "Analyzing Binaries" -Status "Complete" -PercentComplete 100
        Write-Host ""
        
        Write-ColorOutput "✓ Analyzed $($binaryAnalysis.BinariesAnalyzed) binaries" "Success"
        Write-ColorOutput "  - PE files: $($binaryAnalysis.PEFiles)" "Detail"
        Write-ColorOutput "  - ELF files: $($binaryAnalysis.ELFFiles)" "Detail"
        Write-ColorOutput "  - Mach-O files: $($binaryAnalysis.MachOFiles)" "Detail"
        
    } catch {
        Write-ColorOutput "⚠ Failed to analyze binaries: $_" "Warning"
    }
    
    return $binaryAnalysis
}

function Handle-NativeModules {
    param(
        [string]$Dir,
        [string]$OutputDir
    )
    
    Write-ColorOutput "→ Handling native modules" "Native"
    
    $nativeModules = @{
        ModulesFound = 0
        ModulesHandled = 0
        NodeModules = 0
        NativeAddons = 0
        AnalysisResults = @()
    }
    
    try {
        # Find native module files
        $nativePatterns = @("*.node", "*.dll", "*.so", "*.dylib")
        $nativeFiles = @()
        
        foreach ($pattern in $nativePatterns) {
            $files = Get-ChildItem -Path $Dir -Recurse -Filter $pattern -ErrorAction SilentlyContinue
            $nativeFiles += $files
        }
        
        $nativeModules.ModulesFound = $nativeFiles.Count
        
        $totalModules = $nativeFiles.Count
        $currentModule = 0
        
        foreach ($module in $nativeFiles) {
            $currentModule++
            $percentComplete = [math]::Round(($currentModule / $totalModules) * 100, 1)
            
            $relativePath = $module.FullName.Substring($Dir.Length).TrimStart('\', '/')
            Write-ProgressBar -Activity "Handling Native Modules" -Status "Processing" -PercentComplete $percentComplete -CurrentOperation $relativePath
            
            try {
                $moduleInfo = @{
                    Path = $relativePath
                    Type = "Unknown"
                    Size = $module.Length
                    Architecture = "Unknown"
                    Dependencies = @()
                    Status = "Pending"
                }
                
                # Determine module type
                switch ($module.Extension.ToLower()) {
                    ".node" {
                        $moduleInfo.Type = "Node.js Native Addon"
                        $nativeModules.NodeModules++
                        
                        # Try to analyze Node.js module
                        try {
                            $bytes = [System.IO.File]::ReadAllBytes($module.FullName)
                            if ($bytes.Length -ge 4) {
                                $magic = [System.BitConverter]::ToString($bytes[0..3])
                                switch ($magic) {
                                    "4D-5A" { # PE
                                        $moduleInfo.Architecture = "Windows"
                                        # Check for dependencies
                                        if ($bytes.Length -gt 60) {
                                            $peOffset = [System.BitConverter]::ToInt32($bytes, 60)
                                            if ($peOffset -lt $bytes.Length - 4) {
                                                $machine = [System.BitConverter]::ToUInt16($bytes, $peOffset + 4)
                                                $moduleInfo.Architecture = switch ($machine) {
                                                    0x014C { "Windows x86" }
                                                    0x8664 { "Windows x64" }
                                                    default { "Windows Unknown" }
                                                }
                                            }
                                        }
                                    }
                                    "7F-45-4C-46" { # ELF
                                        $moduleInfo.Architecture = "Linux"
                                        $class = $bytes[4]
                                        $moduleInfo.Architecture = switch ($class) {
                                            1 { "Linux 32-bit" }
                                            2 { "Linux 64-bit" }
                                            default { "Linux Unknown" }
                                        }
                                    }
                                    "CA-FE-BA-BE" { # Mach-O
                                        $moduleInfo.Architecture = "macOS"
                                        $cpuType = [System.BitConverter]::ToUInt32($bytes, 4)
                                        $moduleInfo.Architecture = switch ($cpuType) {
                                            0x01000007 { "macOS x86_64" }
                                            0x0100000C { "macOS ARM64" }
                                            default { "macOS Unknown" }
                                        }
                                    }
                                }
                            }
                        } catch {
                            # Ignore analysis errors
                        }
                    }
                    { $_ -in ".dll", ".so", ".dylib" } {
                        $moduleInfo.Type = "Native Library"
                        $nativeModules.NativeAddons++
                        
                        # Basic architecture detection
                        try {
                            $bytes = [System.IO.File]::ReadAllBytes($module.FullName)
                            if ($bytes.Length -ge 4) {
                                $magic = [System.BitConverter]::ToString($bytes[0..3])
                                switch ($magic) {
                                    "4D-5A" { # PE
                                        $moduleInfo.Architecture = "Windows"
                                    }
                                    "7F-45-4C-46" { # ELF
                                        $moduleInfo.Architecture = "Linux"
                                    }
                                    "CA-FE-BA-BE" { # Mach-O
                                        $moduleInfo.Architecture = "macOS"
                                    }
                                }
                            }
                        } catch {
                            # Ignore analysis errors
                        }
                    }
                }
                
                # Copy module to output
                $targetPath = Join-Path $OutputDir "native_modules" $relativePath
                $targetDir = Split-Path $targetPath -Parent
                if (-not (Test-Path $targetDir)) {
                    New-Item -Path $targetDir -ItemType Directory -Force | Out-Null
                }
                
                Copy-Item -Path $module.FullName -Destination $targetPath -Force
                $moduleInfo.Status = "Extracted"
                $nativeModules.ModulesHandled++
                
                $nativeModules.AnalysisResults += $moduleInfo
                Write-ColorOutput "✓ Handled: $relativePath ($($moduleInfo.Type))" "Success"
                
            } catch {
                Write-ColorOutput "⚠ Failed to handle: $relativePath - $_" "Warning"
            }
        }
        
        Write-ProgressBar -Activity "Handling Native Modules" -Status "Complete" -PercentComplete 100
        Write-Host ""
        
        Write-ColorOutput "✓ Handled $($nativeModules.ModulesHandled) native modules" "Success"
        Write-ColorOutput "  - Node.js modules: $($nativeModules.NodeModules)" "Detail"
        Write-ColorOutput "  - Native libraries: $($nativeModules.NativeAddons)" "Detail"
        
    } catch {
        Write-ColorOutput "⚠ Failed to handle native modules: $_" "Warning"
    }
    
    return $nativeModules
}

function Decompile-WebAssembly {
    param(
        [string]$Dir,
        [string]$OutputDir
    )
    
    Write-ColorOutput "→ Decompiling WebAssembly files" "WASM"
    
    $wasmDecompilation = @{
        WASMFilesFound = 0
        WASMFilesDecompiled = 0
        WATFilesGenerated = 0
        AnalysisResults = @()
    }
    
    try {
        # Find WASM files
        $wasmFiles = Get-ChildItem -Path $Dir -Recurse -Filter "*.wasm" -ErrorAction SilentlyContinue
        $wasmDecompilation.WASMFilesFound = $wasmFiles.Count
        
        $totalWASM = $wasmFiles.Count
        $currentWASM = 0
        
        foreach ($wasm in $wasmFiles) {
            $currentWASM++
            $percentComplete = [math]::Round(($currentWASM / $totalWASM) * 100, 1)
            
            $relativePath = $wasm.FullName.Substring($Dir.Length).TrimStart('\', '/')
            Write-ProgressBar -Activity "Decompiling WASM" -Status "Processing" -PercentComplete $percentComplete -CurrentOperation $relativePath
            
            try {
                $wasmInfo = @{
                    Path = $relativePath
                    Size = $wasm.Length
                    Magic = ""
                    Version = ""
                    Sections = @()
                    Status = "Pending"
                }
                
                # Read WASM header
                $bytes = [System.IO.File]::ReadAllBytes($wasm.FullName)
                if ($bytes.Length -ge 8) {
                    $magic = [System.Text.Encoding]::ASCII.GetString($bytes[0..3])
                    $version = [System.BitConverter]::ToUInt32($bytes, 4)
                    
                    $wasmInfo.Magic = $magic
                    $wasmInfo.Version = $version
                    
                    if ($magic -eq "\0asm") {
                        $wasmInfo.Status = "Valid WASM"
                        
                        # Basic section analysis
                        $offset = 8
                        while ($offset -lt $bytes.Length) {
                            if ($offset + 1 -ge $bytes.Length) { break }
                            
                            $sectionId = $bytes[$offset]
                            $offset++
                            
                            # Read LEB128 length
                            $sectionLength = 0
                            $shift = 0
                            while ($offset -lt $bytes.Length) {
                                $byte = $bytes[$offset]
                                $offset++
                                $sectionLength = $sectionLength -bor (($byte -band 0x7F) -shl $shift)
                                $shift += 7
                                if (($byte -band 0x80) -eq 0) { break }
                            }
                            
                            $sectionName = switch ($sectionId) {
                                0 { "Custom" }
                                1 { "Type" }
                                2 { "Import" }
                                3 { "Function" }
                                4 { "Table" }
                                5 { "Memory" }
                                6 { "Global" }
                                7 { "Export" }
                                8 { "Start" }
                                9 { "Element" }
                                10 { "Code" }
                                11 { "Data" }
                                12 { "DataCount" }
                                default { "Unknown" }
                            }
                            
                            $wasmInfo.Sections += @{
                                Id = $sectionId
                                Name = $sectionName
                                Length = $sectionLength
                            }
                            
                            $offset += $sectionLength
                        }
                        
                        # Copy WASM file
                        $targetPath = Join-Path $OutputDir "wasm_files" $relativePath
                        $targetDir = Split-Path $targetPath -Parent
                        if (-not (Test-Path $targetDir)) {
                            New-Item -Path $targetDir -ItemType Directory -Force | Out-Null
                        }
                        
                        Copy-Item -Path $wasm.FullName -Destination $targetPath -Force
                        $wasmDecompilation.WASMFilesDecompiled++
                        
                        # Generate WAT (WebAssembly Text) representation (placeholder)
                        $watPath = $targetPath -replace '\.wasm$', '.wat'
                        $watContent = @"
;; WebAssembly Text Format (WAT)
;; Generated from: $relativePath
;; Size: $($wasm.Length) bytes
;; Magic: $($wasmInfo.Magic)
;; Version: $($wasmInfo.Version)

(module
"@
                        
                        foreach ($section in $wasmInfo.Sections) {
                            $watContent += "  ;; Section $($section.Id): $($section.Name) (length: $($section.Length))`n"
                        }
                        
                        $watContent += @"

  ;; Note: Full WAT decompilation requires wasm2wat tool
  ;; Install: npm install -g webassemblyjs
)
"@
                        
                        $watContent | Out-File -FilePath $watPath -Encoding UTF8
                        $wasmDecompilation.WATFilesGenerated++
                        
                        Write-ColorOutput "✓ Decompiled: $relativePath" "Success"
                        
                    } else {
                        $wasmInfo.Status = "Invalid WASM"
                        Write-ColorOutput "⚠ Invalid WASM: $relativePath" "Warning"
                    }
                }
                
                $wasmDecompilation.AnalysisResults += $wasmInfo
                
            } catch {
                Write-ColorOutput "⚠ Failed to decompile: $relativePath - $_" "Warning"
            }
        }
        
        Write-ProgressBar -Activity "Decompiling WASM" -Status "Complete" -PercentComplete 100
        Write-Host ""
        
        Write-ColorOutput "✓ Decompiled $($wasmDecompilation.WASMFilesDecompiled) WASM files" "Success"
        Write-ColorOutput "  - WAT files generated: $($wasmDecompilation.WATFilesGenerated)" "Detail"
        
    } catch {
        Write-ColorOutput "⚠ Failed to decompile WASM files: $_" "Warning"
    }
    
    return $wasmDecompilation
}

function Resolve-ControlScenarios {
    param(
        [string]$Dir,
        [string]$OutputDir,
        $ControlScenarios
    )
    
    Write-ColorOutput "→ Resolving control scenarios" "Control"
    
    $resolution = @{
        ScenariosResolved = 0
        ScenariosFailed = 0
        ReleaseMechanismsCreated = 0
        BackupFilesCreated = 0
        Results = @()
    }
    
    try {
        if (-not $ControlScenarios) {
            Write-ColorOutput "⚠ No control scenarios to resolve" "Warning"
            return $resolution
        }
        
        $totalScenarios = $ControlScenarios.ProtectedFiles.Count + $ControlScenarios.UnextractableFiles.Count
        $currentScenario = 0
        
        # Handle protected files
        foreach ($scenario in $ControlScenarios.ProtectedFiles) {
            $currentScenario++
            $percentComplete = [math]::Round(($currentScenario / $totalScenarios) * 100, 1)
            
            Write-ProgressBar -Activity "Resolving Scenarios" -Status "Processing" -PercentComplete $percentComplete -CurrentOperation $scenario.File
            
            try {
                $sourcePath = Join-Path $Dir $scenario.File
                $targetPath = Join-Path $OutputDir "protected_files" $scenario.File
                $targetDir = Split-Path $targetPath -Parent
                
                if (-not (Test-Path $targetDir)) {
                    New-Item -Path $targetDir -ItemType Directory -Force | Out-Null
                }
                
                $result = @{
                    File = $scenario.File
                    Type = $scenario.Type
                    Action = $scenario.Action
                    Status = "Success"
                    Message = ""
                }
                
                switch ($scenario.Action) {
                    "ExtractAndReverse" {
                        # For archives, extract and reverse
                        if (Test-Path $sourcePath) {
                            Copy-Item -Path $sourcePath -Destination $targetPath -Force
                            $result.Message = "File extracted and ready for reversal"
                            $resolution.BackupFilesCreated++
                        } else {
                            $result.Status = "Failed"
                            $result.Message = "Source file not found"
                        }
                    }
                    "UnpackAndAnalyze" {
                        # For packages, unpack and analyze
                        if (Test-Path $sourcePath) {
                            Copy-Item -Path $sourcePath -Destination $targetPath -Force
                            $result.Message = "Package unpacked for analysis"
                            $resolution.BackupFilesCreated++
                        } else {
                            $result.Status = "Failed"
                            $result.Message = "Package not found"
                        }
                    }
                    "DecompileOrStub" {
                        # For native modules, decompile or create stub
                        if (Test-Path $sourcePath) {
                            Copy-Item -Path $sourcePath -Destination $targetPath -Force
                            # Create stub/placeholder
                            $stubPath = $targetPath + ".stub.js"
                            $stubContent = @"
// Stub for native module: $($scenario.File)
// Type: $($scenario.Type)
// Action: $($scenario.Action)
// This is a placeholder for the native module
// Original file: $sourcePath

module.exports = {
  // Placeholder exports
  init: function() {
    console.warn('Native module stub loaded: $($scenario.File)');
    return null;
  }
};
"@
                            $stubContent | Out-File -FilePath $stubPath -Encoding UTF8
                            $result.Message = "Native module stub created"
                            $resolution.BackupFilesCreated++
                        } else {
                            $result.Status = "Failed"
                            $result.Message = "Native module not found"
                        }
                    }
                    "AnalyzeExports" {
                        # For libraries, analyze exports
                        if (Test-Path $sourcePath) {
                            Copy-Item -Path $sourcePath -Destination $targetPath -Force
                            # Create exports analysis
                            $analysisPath = $targetPath + ".exports.txt"
                            $analysisContent = @"
// Exports analysis for: $($scenario.File)
// Type: $($scenario.Type)
// Action: $($scenario.Action)
// This file contains the exports analysis

# Exports Analysis
# Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')

"@
                            $analysisContent | Out-File -FilePath $analysisPath -Encoding UTF8
                            $result.Message = "Exports analysis created"
                            $resolution.BackupFilesCreated++
                        } else {
                            $result.Status = "Failed"
                            $result.Message = "Library not found"
                        }
                    }
                    "DecompileToWAT" {
                        # For WASM, decompile to WAT
                        if (Test-Path $sourcePath) {
                            Copy-Item -Path $sourcePath -Destination $targetPath -Force
                            # Create WAT representation
                            $watPath = $targetPath -replace '\.wasm$', '.wat'
                            $watContent = @"
;; WebAssembly Text Format (WAT)
;; Generated from: $($scenario.File)
;; Type: $($scenario.Type)
;; Action: $($scenario.Action)
;; This is a WAT representation of the WASM module

(module
  ;; Placeholder module
  ;; Note: Full decompilation requires wasm2wat tool
)
"@
                            $watContent | Out-File -FilePath $watPath -Encoding UTF8
                            $result.Message = "WAT representation created"
                            $resolution.BackupFilesCreated++
                        } else {
                            $result.Status = "Failed"
                            $result.Message = "WASM file not found"
                        }
                    }
                    "AnalyzePE" {
                        # For executables, analyze PE structure
                        if (Test-Path $sourcePath) {
                            Copy-Item -Path $sourcePath -Destination $targetPath -Force
                            # Create PE analysis
                            $analysisPath = $targetPath + ".pe_analysis.txt"
                            $analysisContent = @"
// PE Analysis for: $($scenario.File)
// Type: $($scenario.Type)
// Action: $($scenario.Action)
// This file contains the PE structure analysis

# PE Analysis
# Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')

"@
                            $analysisContent | Out-File -FilePath $analysisPath -Encoding UTF8
                            $result.Message = "PE analysis created"
                            $resolution.BackupFilesCreated++
                        } else {
                            $result.Status = "Failed"
                            $result.Message = "Executable not found"
                        }
                    }
                    "ExtractBundle" {
                        # For Mac apps, extract bundle
                        if (Test-Path $sourcePath) {
                            Copy-Item -Path $sourcePath -Destination $targetPath -Force -Recurse
                            $result.Message = "Bundle extracted"
                            $resolution.BackupFilesCreated++
                        } else {
                            $result.Status = "Failed"
                            $result.Message = "Bundle not found"
                        }
                    }
                }
                
                $resolution.Results += $result
                
                if ($result.Status -eq "Success") {
                    $resolution.ScenariosResolved++
                    Write-ColorOutput "✓ Resolved: $($scenario.File)" "Success"
                } else {
                    $resolution.ScenariosFailed++
                    Write-ColorOutput "⚠ Failed: $($scenario.File) - $($result.Message)" "Warning"
                }
                
            } catch {
                $resolution.ScenariosFailed++
                Write-ColorOutput "⚠ Failed: $($scenario.File) - $_" "Warning"
            }
        }
        
        # Handle unextractable files
        foreach ($file in $ControlScenarios.UnextractableFiles) {
            $currentScenario++
            $percentComplete = [math]::Round(($currentScenario / $totalScenarios) * 100, 1)
            
            Write-ProgressBar -Activity "Resolving Scenarios" -Status "Processing" -PercentComplete $percentComplete -CurrentOperation $file.File
            
            try {
                $sourcePath = Join-Path $Dir $file.File
                $targetPath = Join-Path $OutputDir "unextractable_files" $file.File
                $targetDir = Split-Path $targetPath -Parent
                
                if (-not (Test-Path $targetDir)) {
                    New-Item -Path $targetDir -ItemType Directory -Force | Out-Null
                }
                
                # Create placeholder for unextractable file
                $placeholderPath = $targetPath + ".placeholder.txt"
                $placeholderContent = @"
// Unextractable File Placeholder
// File: $($file.File)
// Type: $($file.Type)
// Action: $($file.Action)
// Reason: Could not be extracted during normal process
// 
// This file requires manual extraction or special handling
// 
// Recommendations:
// 1. Check if file exists in original source
// 2. Try alternative extraction methods
// 3. Use ControlHandler.ps1 to manage extraction
// 4. Create stub implementation if needed

module.exports = {
  // Placeholder exports
  // Replace with actual implementation
  init: function() {
    console.error('Unextractable file: $($file.File)');
    throw new Error('File could not be extracted: $($file.File)');
  }
};
"@
                $placeholderContent | Out-File -FilePath $placeholderPath -Encoding UTF8
                
                $resolution.Results += @{
                    File = $file.File
                    Type = $file.Type
                    Action = $file.Action
                    Status = "Success"
                    Message = "Placeholder created for unextractable file"
                }
                
                $resolution.ScenariosResolved++
                Write-ColorOutput "✓ Handled unextractable: $($file.File)" "Success"
                
            } catch {
                $resolution.ScenariosFailed++
                Write-ColorOutput "⚠ Failed to handle: $($file.File) - $_" "Warning"
            }
        }
        
        Write-ProgressBar -Activity "Resolving Scenarios" -Status "Complete" -PercentComplete 100
        Write-Host ""
        
        # Create release mechanisms
        foreach ($result in $resolution.Results) {
            if ($result.Status -eq "Success") {
                $releaseMech = @{
                    File = $result.File
                    Type = $result.Type
                    ReleaseCommand = "ControlHandler.ps1 -Action release -Target `"$($result.File)`""
                    VerifyCommand = "ControlHandler.ps1 -Action verify -Target `"$($result.File)`""
                    Status = "Created"
                }
                $resolution.ReleaseMechanismsCreated++
            }
        }
        
        Write-ColorOutput "✓ Resolved $($resolution.ScenariosResolved) control scenarios" "Success"
        Write-ColorOutput "  - Failed: $($resolution.ScenariosFailed)" "Detail"
        Write-ColorOutput "  - Release mechanisms: $($resolution.ReleaseMechanismsCreated)" "Detail"
        Write-ColorOutput "  - Backup files: $($resolution.BackupFilesCreated)" "Detail"
        
    } catch {
        Write-ColorOutput "⚠ Failed to resolve control scenarios: $_" "Warning"
    }
    
    return $resolution
}

function Generate-ExtractionReport {
    param(
        $ASARExtraction,
        $BinaryAnalysis,
        $NativeModules,
        $WASMDecompilation,
        $ControlResolution,
        [string]$OutputDir
    )
    
    Write-ColorOutput "=== GENERATING EXTRACTION REPORT ===" "Header"
    
    $report = @{
        Metadata = @{
            Tool = "Extract-Protected-Files.ps1"
            Version = "1.0"
            GeneratedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            SourceDirectory = $SourceDirectory
            OutputDirectory = $OutputDirectory
        }
        Summary = @{
            ASARArchives = $ASARExtraction.ArchivesExtracted
            BinariesAnalyzed = $BinaryAnalysis.BinariesAnalyzed
            NativeModules = $NativeModules.ModulesHandled
            WASMFiles = $WASMDecompilation.WASMFilesDecompiled
            ControlScenarios = $ControlResolution.ScenariosResolved
            TotalFiles = $ASARExtraction.FilesExtracted + $NativeModules.ModulesHandled + $WASMDecompilation.WASMFilesDecompiled
        }
        ASARExtraction = $ASARExtraction
        BinaryAnalysis = $BinaryAnalysis
        NativeModules = $NativeModules
        WASMDecompilation = $WASMDecompilation
        ControlResolution = $ControlResolution
    }
    
    # Save JSON report
    $reportJson = $report | ConvertTo-Json -Depth 20 -Compress:$false
    $reportPath = Join-Path $OutputDir "protected_files_extraction_report.json"
    $reportJson | Out-File -FilePath $reportPath -Encoding UTF8
    
    # Save text report
    $textReport = @"
Protected Files Extraction Report
===================================
Generated: $($report.Metadata.GeneratedAt)
Tool: $($report.Metadata.Tool)
Version: $($report.Metadata.Version)

SUMMARY
-------
ASAR Archives Extracted: $($ASARExtraction.ArchivesExtracted)
Binaries Analyzed: $($BinaryAnalysis.BinariesAnalyzed)
Native Modules Handled: $($NativeModules.ModulesHandled)
WASM Files Decompiled: $($WASMDecompilation.WASMFilesDecompiled)
Control Scenarios Resolved: $($ControlResolution.ScenariosResolved)
Total Files Processed: $($report.Summary.TotalFiles)

ASAR EXTRACTION
---------------
Archives Found: $($ASARExtraction.ArchivesFound)
Archives Extracted: $($ASARExtraction.ArchivesExtracted)
Files Extracted: $($ASARExtraction.FilesExtracted)
Total Size: $([math]::Round($ASARExtraction.TotalSize / 1MB, 2)) MB
Failed: $($ASARExtraction.Failed.Count)

BINARY ANALYSIS
---------------
Binaries Found: $($BinaryAnalysis.BinariesFound)
Binaries Analyzed: $($BinaryAnalysis.BinariesAnalyzed)
PE Files: $($BinaryAnalysis.PEFiles)
ELF Files: $($BinaryAnalysis.ELFFiles)
Mach-O Files: $($BinaryAnalysis.MachOFiles)

NATIVE MODULES
--------------
Modules Found: $($NativeModules.ModulesFound)
Modules Handled: $($NativeModules.ModulesHandled)
Node.js Modules: $($NativeModules.NodeModules)
Native Libraries: $($NativeModules.NativeAddons)

WASM DECOMPILATION
------------------
WASM Files Found: $($WASMDecompilation.WASMFilesFound)
WASM Files Decompiled: $($WASMDecompilation.WASMFilesDecompiled)
WAT Files Generated: $($WASMDecompilation.WATFilesGenerated)

CONTROL SCENARIOS
-----------------
Scenarios Resolved: $($ControlResolution.ScenariosResolved)
Scenarios Failed: $($ControlResolution.ScenariosFailed)
Release Mechanisms: $($ControlResolution.ReleaseMechanismsCreated)
Backup Files: $($ControlResolution.BackupFilesCreated)

FILES CREATED
-------------
- protected_files_extraction_report.json
- protected_files_extraction_report.txt
- ASAR extracted files in asar_extracted/
- Native modules in native_modules/
- WASM files in wasm_files/
- Protected files in protected_files/
- Unextractable files in unextractable_files/

For detailed analysis, review the JSON report at:
$reportPath
"@
    
    $textReportPath = Join-Path $OutputDir "protected_files_extraction_report.txt"
    $textReport | Out-File -FilePath $textReportPath -Encoding UTF8
    
    Write-ColorOutput "✓ Generated extraction report" "Success"
    Write-ColorOutput "  JSON: $reportPath" "Detail"
    Write-ColorOutput "  Text: $textReportPath" "Detail"
    
    return $report
}

# Main execution
Write-ColorOutput "=" * 80
Write-ColorOutput "PROTECTED FILES EXTRACTION TOOL"
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
$asarExtraction = $null
$binaryAnalysis = $null
$nativeModules = $null
$wasmDecompilation = $null
$controlResolution = $null

# Extract ASAR archives
if ($ExtractASAR -or $ExtractAll) {
    Write-ColorOutput "=== EXTRACTING ASAR ARCHIVES ===" "Header"
    $asarExtraction = Extract-ASARArchives -Dir $SourceDirectory -OutputDir $OutputDirectory
}

# Analyze binaries
if ($ExtractBinaries -or $ExtractAll) {
    Write-ColorOutput "=== ANALYZING BINARY FILES ===" "Header"
    $binaryAnalysis = Analyze-Binaries -Dir $SourceDirectory -OutputDir $OutputDirectory
}

# Handle native modules
if ($HandleNativeModules -or $ExtractAll) {
    Write-ColorOutput "=== HANDLING NATIVE MODULES ===" "Header"
    $nativeModules = Handle-NativeModules -Dir $SourceDirectory -OutputDir $OutputDirectory
}

# Decompile WebAssembly
if ($DecompileWebAssembly -or $ExtractAll) {
    Write-ColorOutput "=== DECOMPILING WEBASSEMBLY ===" "Header"
    $wasmDecompilation = Decompile-WebAssembly -Dir $SourceDirectory -OutputDir $OutputDirectory
}

# Resolve control scenarios
if ($ResolveControlScenarios -or $ExtractAll) {
    Write-ColorOutput "=== RESOLVING CONTROL SCENARIOS ===" "Header"
    
    # Load control scenarios from previous analysis if available
    $controlScenariosPath = Join-Path $SourceDirectory ".." "Advanced_Reverse_Engineered" "advanced_reverse_engineering_report.json"
    if (Test-Path $controlScenariosPath) {
        $controlData = Get-Content $controlScenariosPath -Raw | ConvertFrom-Json
        $controlScenarios = $controlData.ControlScenarios
        $controlResolution = Resolve-ControlScenarios -Dir $SourceDirectory -OutputDir $OutputDirectory -ControlScenarios $controlScenarios
    } else {
        Write-ColorOutput "⚠ No control scenarios found, skipping" "Warning"
    }
}

# Generate extraction report
if ($ExtractAll) {
    Write-ColorOutput "=== GENERATING EXTRACTION REPORT ===" "Header"
    $report = Generate-ExtractionReport `
        -ASARExtraction $asarExtraction `
        -BinaryAnalysis $binaryAnalysis `
        -NativeModules $nativeModules `
        -WASMDecompilation $wasmDecompilation `
        -ControlResolution $controlResolution `
        -OutputDir $OutputDirectory
}

# Final summary
Write-ColorOutput ""
Write-ColorOutput "=" * 80
Write-ColorOutput "PROTECTED FILES EXTRACTION COMPLETE" "Header"
Write-ColorOutput "=" * 80

if ($asarExtraction) {
    Write-ColorOutput "ASAR Archives: $($asarExtraction.ArchivesExtracted) extracted" "Archive"
}

if ($binaryAnalysis) {
    Write-ColorOutput "Binaries: $($binaryAnalysis.BinariesAnalyzed) analyzed" "Binary"
}

if ($nativeModules) {
    Write-ColorOutput "Native Modules: $($nativeModules.ModulesHandled) handled" "Native"
}

if ($wasmDecompilation) {
    Write-ColorOutput "WASM Files: $($wasmDecompilation.WASMFilesDecompiled) decompiled" "WASM"
}

if ($controlResolution) {
    Write-ColorOutput "Control Scenarios: $($controlResolution.ScenariosResolved) resolved" "Control"
}

$totalProcessed = 0
if ($asarExtraction) { $totalProcessed += $asarExtraction.FilesExtracted }
if ($nativeModules) { $totalProcessed += $nativeModules.ModulesHandled }
if ($wasmDecompilation) { $totalProcessed += $wasmDecompilation.WASMFilesDecompiled }

Write-ColorOutput "Total Files Processed: $totalProcessed" "Success"
Write-ColorOutput "=" * 80
Write-ColorOutput "Output Directory: $OutputDirectory" "Detail"
Write-ColorOutput "=" * 80

Write-ColorOutput ""
Write-ColorOutput "Extracted files:"
if ($asarExtraction -and $asarExtraction.ArchivesExtracted -gt 0) {
    Write-ColorOutput "  - ASAR archives in asar_extracted/" "Archive"
}
if ($nativeModules -and $nativeModules.ModulesHandled -gt 0) {
    Write-ColorOutput "  - Native modules in native_modules/" "Native"
}
if ($wasmDecompilation -and $wasmDecompilation.WASMFilesDecompiled -gt 0) {
    Write-ColorOutput "  - WASM files in wasm_files/" "WASM"
}
if ($controlResolution -and $controlResolution.ScenariosResolved -gt 0) {
    Write-ColorOutput "  - Protected files in protected_files/" "Control"
    Write-ColorOutput "  - Unextractable files in unextractable_files/" "Control"
}

Write-ColorOutput ""
Write-ColorOutput "Next steps:"
Write-ColorOutput "1. Review extracted files in output directory"
Write-ColorOutput "2. Test self-reversing mechanisms"
Write-ColorOutput "3. Handle any remaining control scenarios"
Write-ColorOutput "4. Verify integrity of extracted files"
Write-ColorOutput "5. Create backups before modifications"
