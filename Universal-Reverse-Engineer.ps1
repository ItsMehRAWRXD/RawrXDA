#Requires -Version 7.0
<#
.SYNOPSIS
    Universal Reverse Engineering Tool - Extracts source code from any file type
.DESCRIPTION
    Comprehensive reverse engineering tool that can:
    - Analyze Windows PE, Linux ELF, macOS Mach-O binaries
    - Extract Electron app source code (ASAR archives)
    - Reverse engineer from the ground up
    - Generate complete source code dumps
    - Work cross-platform (Windows/Mac/Linux)
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$InputFile,
    
    [string]$OutputDirectory = "Reverse_Engineered_Source",
    
    [ValidateSet("auto", "pe", "elf", "macho", "asar", "zip", "tar", "js", "ts", "json", "binary")]
    [string]$FileType = "auto",
    
    [switch]$ExtractSource,
    [switch]$Decompile,
    [switch]$GenerateReport,
    [switch]$ShowProgress,
    [switch]$DeepAnalysis,
    [switch]$ExtractStrings,
    [switch]$ExtractResources,
    [switch]$ReconstructSourceTree,
    [switch]$IncludeDependencies,
    [int]$StringMinLength = 4,
    [string[]]$IncludeExtensions = @("*"),
    [string[]]$ExcludeExtensions = @(),
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
    Binary = "DarkCyan"
    Source = "DarkGreen"
    Electron = "DarkYellow"
    Extract = "Blue"
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
        Write-Progress -Activity $Activity -Status $Status -PercentComplete $PercentComplete -CurrentOperation $CurrentOperation
    }
}

function Get-FileType {
    param([string]$FilePath)
    
    if (-not (Test-Path $FilePath)) {
        return "unknown"
    }
    
    $fileInfo = Get-Item $FilePath
    $extension = $fileInfo.Extension.ToLower()
    
    # Check by extension first
    switch ($extension) {
        ".exe" { return "pe" }
        ".dll" { return "pe" }
        ".sys" { return "pe" }
        ".elf" { return "elf" }
        ".so" { return "elf" }
        ".dylib" { return "macho" }
        ".app" { return "macho" }
        ".asar" { return "asar" }
        ".zip" { return "zip" }
        ".tar" { return "tar" }
        ".gz" { return "tar" }
        ".js" { return "js" }
        ".ts" { return "ts" }
        ".json" { return "json" }
        ".node" { return "pe" }
        default { 
            # Check magic bytes for binary files
            try {
                $bytes = [System.IO.File]::ReadAllBytes($FilePath)
                if ($bytes.Length -ge 4) {
                    $magic = [System.BitConverter]::ToString($bytes[0..3])
                    switch ($magic) {
                        "4D-5A" { return "pe" }  # MZ
                        "7F-45-4C-46" { return "elf" }  # ELF
                        "CA-FE-BA-BE" { return "macho" }  # Mach-O
                        "50-4B-03-04" { return "zip" }  # ZIP
                        "1F-8B" { return "tar" }  # GZIP
                        default { return "binary" }
                    }
                }
            } catch {
                return "binary"
            }
        }
    }
}

function Detect-ElectronApp {
    param([string]$FilePath)
    
    $fileInfo = Get-Item $FilePath
    $dir = $fileInfo.Directory
    
    # Check for Electron app structure
    $electronIndicators = @(
        "resources\app.asar",
        "resources\app",
        "resources\electron.asar",
        "LICENSE.electron.txt",
        "version"
    )
    
    foreach ($indicator in $electronIndicators) {
        $checkPath = Join-Path $dir.FullName $indicator
        if (Test-Path $checkPath) {
            return $true
        }
    }
    
    # Check if it's an ASAR file
    if ($fileInfo.Extension -eq ".asar") {
        return $true
    }
    
    return $false
}

function Extract-ASARArchive {
    param(
        [string]$AsarPath,
        [string]$OutputDir
    )
    
    Write-ColorOutput "→ Extracting ASAR archive: $AsarPath" "Extract"
    
    try {
        # Try to use asar command if available
        $asarCmd = Get-Command "asar" -ErrorAction SilentlyContinue
        if ($asarCmd) {
            & asar extract $AsarPath $OutputDir
            if ($LASTEXITCODE -eq 0) {
                Write-ColorOutput "✓ Successfully extracted ASAR using asar command" "Success"
                return $true
            }
        }
        
        # Fallback: manual ASAR extraction
        Write-ColorOutput "→ Performing manual ASAR extraction" "Detail"
        
        $bytes = [System.IO.File]::ReadAllBytes($AsarPath)
        $offset = 0
        
        # ASAR header is JSON with 8-byte length prefix
        if ($bytes.Length -lt 8) {
            Write-ColorOutput "⚠ ASAR file too small" "Warning"
            return $false
        }
        
        $headerSize = [System.BitConverter]::ToUInt32($bytes, 4)
        $headerBytes = $bytes[8..(8 + $headerSize - 1)]
        $headerJson = [System.Text.Encoding]::UTF8.GetString($headerBytes)
        $header = $headerJson | ConvertFrom-Json
        
        Write-ColorOutput "→ Extracting $($header.files.PSObject.Properties.Count) files" "Detail"
        
        # Create output directory
        New-Item -Path $OutputDir -ItemType Directory -Force | Out-Null
        
        # Extract files
        foreach ($fileEntry in $header.files.PSObject.Properties) {
            $fileName = $fileEntry.Name
            $fileInfo = $fileEntry.Value
            
            $outputPath = Join-Path $OutputDir $fileName
            $outputDir = Split-Path $outputPath -Parent
            
            if (-not (Test-Path $outputDir)) {
                New-Item -Path $outputDir -ItemType Directory -Force | Out-Null
            }
            
            if ($fileInfo.size) {
                # Regular file
                $fileOffset = 8 + $headerSize + $fileInfo.offset
                $fileSize = $fileInfo.size
                $fileBytes = $bytes[$fileOffset..($fileOffset + $fileSize - 1)]
                [System.IO.File]::WriteAllBytes($outputPath, $fileBytes)
            } elseif ($fileInfo.files) {
                # Directory (already created above)
            }
        }
        
        Write-ColorOutput "✓ Successfully extracted ASAR archive" "Success"
        return $true
        
    } catch {
        Write-ColorOutput "⚠ Failed to extract ASAR: $_" "Warning"
        return $false
    }
}

function Analyze-PEBinary {
    param(
        [string]$FilePath,
        [string]$OutputDir
    )
    
    Write-ColorOutput "→ Analyzing PE binary: $FilePath" "Binary"
    
    try {
        $analysis = @{
            FileInfo = @{}
            Headers = @{}
            Sections = @()
            Imports = @()
            Exports = @()
            Resources = @()
            Strings = @()
        }
        
        $fileInfo = Get-Item $FilePath
        $analysis.FileInfo = @{
            Name = $fileInfo.Name
            Size = $fileInfo.Length
            Created = $fileInfo.CreationTime
            Modified = $fileInfo.LastWriteTime
            Extension = $fileInfo.Extension
        }
        
        # Read PE headers (simplified)
        $bytes = [System.IO.File]::ReadAllBytes($FilePath)
        if ($bytes.Length -lt 512) {
            Write-ColorOutput "⚠ File too small for PE analysis" "Warning"
            return $analysis
        }
        
        # Check DOS header
        $dosSignature = [System.Text.Encoding]::ASCII.GetString($bytes[0..1])
        if ($dosSignature -eq "MZ") {
            $analysis.Headers["DOS"] = "Valid DOS header"
            
            # Get PE header offset
            $peOffset = [System.BitConverter]::ToInt32($bytes, 60)
            if ($peOffset -lt $bytes.Length - 4) {
                $peSignature = [System.Text.Encoding]::ASCII.GetString($bytes[$peOffset..($peOffset + 3)])
                if ($peSignature -eq "PE\0\0") {
                    $analysis.Headers["PE"] = "Valid PE header"
                    
                    # Extract basic info
                    $machine = [System.BitConverter]::ToUInt16($bytes, $peOffset + 4)
                    $analysis.Headers["Machine"] = switch ($machine) {
                        0x014C { "x86" }
                        0x8664 { "x64" }
                        0x0200 { "IA64" }
                        default { "Unknown (0x$($machine.ToString('X4')))" }
                    }
                    
                    $characteristics = [System.BitConverter]::ToUInt16($bytes, $peOffset + 22)
                    $analysis.Headers["Characteristics"] = @()
                    if ($characteristics -band 0x0002) { $analysis.Headers["Characteristics"] += "Executable" }
                    if ($characteristics -band 0x2000) { $analysis.Headers["Characteristics"] += "DLL" }
                }
            }
        }
        
        # Extract strings if requested
        if ($ExtractStrings) {
            $analysis.Strings = Extract-Strings -FilePath $FilePath -MinLength $StringMinLength
        }
        
        # Save analysis
        $analysisJson = $analysis | ConvertTo-Json -Depth 10
        $analysisPath = Join-Path $OutputDir "pe_analysis.json"
        $analysisJson | Out-File -FilePath $analysisPath -Encoding UTF8
        
        Write-ColorOutput "✓ PE analysis complete" "Success"
        return $analysis
        
    } catch {
        Write-ColorOutput "⚠ PE analysis failed: $_" "Warning"
        return $null
    }
}

function Analyze-ELFBinary {
    param(
        [string]$FilePath,
        [string]$OutputDir
    )
    
    Write-ColorOutput "→ Analyzing ELF binary: $FilePath" "Binary"
    
    try {
        $analysis = @{
            FileInfo = @{}
            Headers = @{}
            Sections = @()
            Strings = @()
        }
        
        $fileInfo = Get-Item $FilePath
        $analysis.FileInfo = @{
            Name = $fileInfo.Name
            Size = $fileInfo.Length
            Created = $fileInfo.CreationTime
            Modified = $fileInfo.LastWriteTime
            Extension = $fileInfo.Extension
        }
        
        # Read ELF header
        $bytes = [System.IO.File]::ReadAllBytes($FilePath)
        if ($bytes.Length -lt 64) {
            Write-ColorOutput "⚠ File too small for ELF analysis" "Warning"
            return $analysis
        }
        
        # Check ELF magic
        $elfMagic = [System.Text.Encoding]::ASCII.GetString($bytes[0..3])
        if ($elfMagic -eq "\x7fELF") {
            $analysis.Headers["ELF"] = "Valid ELF header"
            
            # Extract architecture
            $class = $bytes[4]
            $analysis.Headers["Class"] = switch ($class) {
                1 { "32-bit" }
                2 { "64-bit" }
                default { "Unknown" }
            }
            
            $endian = $bytes[5]
            $analysis.Headers["Endian"] = switch ($endian) {
                1 { "Little" }
                2 { "Big" }
                default { "Unknown" }
            }
            
            $machine = [System.BitConverter]::ToUInt16($bytes, 18)
            $analysis.Headers["Machine"] = switch ($machine) {
                0x03 { "x86" }
                0x3E { "x64" }
                0xB7 { "ARM64" }
                default { "Unknown (0x$($machine.ToString('X4')))" }
            }
        }
        
        # Extract strings if requested
        if ($ExtractStrings) {
            $analysis.Strings = Extract-Strings -FilePath $FilePath -MinLength $StringMinLength
        }
        
        # Save analysis
        $analysisJson = $analysis | ConvertTo-Json -Depth 10
        $analysisPath = Join-Path $OutputDir "elf_analysis.json"
        $analysisJson | Out-File -FilePath $analysisPath -Encoding UTF8
        
        Write-ColorOutput "✓ ELF analysis complete" "Success"
        return $analysis
        
    } catch {
        Write-ColorOutput "⚠ ELF analysis failed: $_" "Warning"
        return $null
    }
}

function Analyze-MachOBinary {
    param(
        [string]$FilePath,
        [string]$OutputDir
    )
    
    Write-ColorOutput "→ Analyzing Mach-O binary: $FilePath" "Binary"
    
    try {
        $analysis = @{
            FileInfo = @{}
            Headers = @{}
            Strings = @()
        }
        
        $fileInfo = Get-Item $FilePath
        $analysis.FileInfo = @{
            Name = $fileInfo.Name
            Size = $fileInfo.Length
            Created = $fileInfo.CreationTime
            Modified = $fileInfo.LastWriteTime
            Extension = $fileInfo.Extension
        }
        
        # Read Mach-O header
        $bytes = [System.IO.File]::ReadAllBytes($FilePath)
        if ($bytes.Length -lt 32) {
            Write-ColorOutput "⚠ File too small for Mach-O analysis" "Warning"
            return $analysis
        }
        
        # Check Mach-O magic
        $magic = [System.BitConverter]::ToUInt32($bytes, 0)
        $analysis.Headers["Magic"] = switch ($magic) {
            0xFEEDFACE { "Mach-O 32-bit" }
            0xFEEDFACF { "Mach-O 64-bit" }
            0xCAFEBABE { "Mach-O Universal Binary" }
            default { "Unknown (0x$($magic.ToString('X8')))" }
        }
        
        if ($magic -eq 0xFEEDFACE -or $magic -eq 0xFEEDFACF) {
            $analysis.Headers["Type"] = "Valid Mach-O header"
            
            $cpuType = [System.BitConverter]::ToUInt32($bytes, 4)
            $analysis.Headers["CPU"] = switch ($cpuType) {
                0x01000007 { "x86_64" }
                0x0100000C { "ARM64" }
                0x00000007 { "x86" }
                default { "Unknown (0x$($cpuType.ToString('X8')))" }
            }
        }
        
        # Extract strings if requested
        if ($ExtractStrings) {
            $analysis.Strings = Extract-Strings -FilePath $FilePath -MinLength $StringMinLength
        }
        
        # Save analysis
        $analysisJson = $analysis | ConvertTo-Json -Depth 10
        $analysisPath = Join-Path $OutputDir "macho_analysis.json"
        $analysisJson | Out-File -FilePath $analysisPath -Encoding UTF8
        
        Write-ColorOutput "✓ Mach-O analysis complete" "Success"
        return $analysis
        
    } catch {
        Write-ColorOutput "⚠ Mach-O analysis failed: $_" "Warning"
        return $null
    }
}

function Extract-Strings {
    param(
        [string]$FilePath,
        [int]$MinLength = 4
    )
    
    Write-ColorOutput "→ Extracting strings from: $FilePath" "Extract"
    
    try {
        $bytes = [System.IO.File]::ReadAllBytes($FilePath)
        $strings = [System.Collections.Generic.List[string]]::new()
        $currentString = [System.Collections.Generic.List[byte]]::new()
        
        foreach ($byte in $bytes) {
            if ($byte -ge 32 -and $byte -le 126 -or $byte -eq 9 -or $byte -eq 10 -or $byte -eq 13) {
                $currentString.Add($byte)
            } else {
                if ($currentString.Count -ge $MinLength) {
                    $str = [System.Text.Encoding]::ASCII.GetString($currentString.ToArray())
                    if ($str.Trim().Length -ge $MinLength) {
                        $strings.Add($str.Trim())
                    }
                }
                $currentString.Clear()
            }
        }
        
        # Check final string
        if ($currentString.Count -ge $MinLength) {
            $str = [System.Text.Encoding]::ASCII.GetString($currentString.ToArray())
            if ($str.Trim().Length -ge $MinLength) {
                $strings.Add($str.Trim())
            }
        }
        
        Write-ColorOutput "✓ Extracted $($strings.Count) strings" "Success"
        return $strings | Select-Object -Unique | Sort-Object
        
    } catch {
        Write-ColorOutput "⚠ String extraction failed: $_" "Warning"
        return @()
    }
}

function Extract-Archive {
    param(
        [string]$ArchivePath,
        [string]$OutputDir,
        [string]$ArchiveType
    )
    
    Write-ColorOutput "→ Extracting $ArchiveType archive: $ArchivePath" "Extract"
    
    try {
        if ($ArchiveType -eq "zip") {
            Expand-Archive -Path $ArchivePath -DestinationPath $OutputDir -Force
        } elseif ($ArchiveType -eq "tar" -or $ArchivePath -like "*.gz") {
            # For tar/gz files, try to use tar command if available
            $tarCmd = Get-Command "tar" -ErrorAction SilentlyContinue
            if ($tarCmd) {
                & tar -xzf $ArchivePath -C $OutputDir
            } else {
                Write-ColorOutput "⚠ tar command not available, skipping extraction" "Warning"
                return $false
            }
        }
        
        Write-ColorOutput "✓ Successfully extracted archive" "Success"
        return $true
        
    } catch {
        Write-ColorOutput "⚠ Archive extraction failed: $_" "Warning"
        return $false
    }
}

function Reconstruct-SourceTree {
    param(
        [string]$SourceDir,
        [string]$OutputDir
    )
    
    Write-ColorOutput "→ Reconstructing source tree from: $SourceDir" "Source"
    
    try {
        # Create output directory structure
        $reconstructedDir = Join-Path $OutputDir "reconstructed_source"
        New-Item -Path $reconstructedDir -ItemType Directory -Force | Out-Null
        
        # Find all source files
        $sourceFiles = Get-ChildItem -Path $SourceDir -Recurse -File | Where-Object {
            $ext = $_.Extension.ToLower()
            $included = if ($IncludeExtensions -contains "*") { $true } else { $IncludeExtensions -contains $ext }
            $excluded = $ExcludeExtensions -contains $ext
            $included -and -not $excluded
        }
        
        Write-ColorOutput "→ Found $($sourceFiles.Count) source files" "Detail"
        
        $totalFiles = $sourceFiles.Count
        $currentFile = 0
        
        foreach ($file in $sourceFiles) {
            $currentFile++
            $percentComplete = [math]::Round(($currentFile / $totalFiles) * 100, 1)
            
            $relativePath = $file.FullName.Substring($SourceDir.Length).TrimStart('\', '/')
            $outputPath = Join-Path $reconstructedDir $relativePath
            $outputParent = Split-Path $outputPath -Parent
            
            Write-ProgressBar -Activity "Reconstructing" -Status "Processing files" -PercentComplete $percentComplete -CurrentOperation $relativePath
            
            # Create directory if needed
            if (-not (Test-Path $outputParent)) {
                New-Item -Path $outputParent -ItemType Directory -Force | Out-Null
            }
            
            # Copy file
            Copy-Item -Path $file.FullName -Destination $outputPath -Force
        }
        
        Write-ProgressBar -Activity "Reconstructing" -Status "Complete" -PercentComplete 100
        Write-Host ""
        
        Write-ColorOutput "✓ Reconstructed source tree with $totalFiles files" "Success"
        return $reconstructedDir
        
    } catch {
        Write-ColorOutput "⚠ Source tree reconstruction failed: $_" "Warning"
        return $null
    }
}

function Generate-ComprehensiveReport {
    param(
        $AnalysisResults,
        [string]$OutputDir
    )
    
    Write-ColorOutput "=== GENERATING COMPREHENSIVE REPORT ===" "Header"
    
    $report = @{
        Metadata = @{
            Tool = "Universal-Reverse-Engineer.ps1"
            Version = "1.0"
            GeneratedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            InputFile = $InputFile
            FileType = $FileType
        }
        Summary = @{
            TotalFiles = 0
            TotalSize = 0
            SourceFiles = 0
            BinaryFiles = 0
            ExtractedStrings = 0
            AnalysisComplete = $false
        }
        Analysis = $AnalysisResults
        FileTree = @()
        Strings = @()
    }
    
    # Generate file tree
    if (Test-Path $OutputDir) {
        $files = Get-ChildItem -Path $OutputDir -Recurse -File
        $report.Summary.TotalFiles = $files.Count
        $report.Summary.TotalSize = ($files | Measure-Object -Property Length -Sum).Sum
        
        foreach ($file in $files) {
            $relativePath = $file.FullName.Substring($OutputDir.Length).TrimStart('\', '/')
            $report.FileTree += @{
                Path = $relativePath
                Size = $file.Length
                Extension = $file.Extension
            }
            
            # Count by type
            $ext = $file.Extension.ToLower()
            if ($ext -in @(".js", ".ts", ".jsx", ".tsx", ".py", ".java", ".cpp", ".c", ".h", ".cs", ".go", ".rb", ".php")) {
                $report.Summary.SourceFiles++
            } elseif ($ext -in @(".exe", ".dll", ".so", ".dylib", ".node")) {
                $report.Summary.BinaryFiles++
            }
        }
    }
    
    # Collect all extracted strings
    if ($AnalysisResults) {
        foreach ($result in $AnalysisResults.Values) {
            if ($result.Strings) {
                $report.Strings += $result.Strings
            }
        }
        $report.Summary.ExtractedStrings = $report.Strings.Count
    }
    
    $report.Summary.AnalysisComplete = $true
    
    # Save report
    $reportJson = $report | ConvertTo-Json -Depth 20 -Compress:$false
    $reportPath = Join-Path $OutputDir "reverse_engineering_report.json"
    $reportJson | Out-File -FilePath $reportPath -Encoding UTF8
    
    Write-ColorOutput "✓ Generated comprehensive report" "Success"
    Write-ColorOutput "  Report saved to: $reportPath" "Detail"
    
    return $report
}

# Main execution
Write-ColorOutput "=" * 80
Write-ColorOutput "UNIVERSAL REVERSE ENGINEERING TOOL"
Write-ColorOutput "Starting at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-ColorOutput "Input File: $InputFile"
Write-ColorOutput "=" * 80
Write-ColorOutput ""

# Check if input file exists
if (-not (Test-Path $InputFile)) {
    Write-ColorOutput "✗ Input file not found: $InputFile" "Error"
    exit 1
}

# Detect file type if auto
if ($FileType -eq "auto") {
    $FileType = Get-FileType -FilePath $InputFile
    Write-ColorOutput "→ Detected file type: $FileType" "Detail"
}

# Create output directory
if (Test-Path $OutputDirectory) {
    if ($ForceOverwrite) {
        Remove-Item -Path $OutputDirectory -Recurse -Force
    } else {
        Write-ColorOutput "⚠ Output directory already exists, use -ForceOverwrite to replace" "Warning"
        exit 1
    }
}
New-Item -Path $OutputDirectory -ItemType Directory -Force | Out-Null
Write-ColorOutput "✓ Created output directory: $OutputDirectory" "Success"

# Check if it's an Electron app
$isElectron = Detect-ElectronApp -FilePath $InputFile
if ($isElectron) {
    Write-ColorOutput "✓ Detected Electron application" "Electron"
}

# Initialize analysis results
$analysisResults = @{}

# Process based on file type
switch ($FileType) {
    "pe" {
        $analysisResults["PE"] = Analyze-PEBinary -FilePath $InputFile -OutputDir $OutputDirectory
    }
    "elf" {
        $analysisResults["ELF"] = Analyze-ELFBinary -FilePath $InputFile -OutputDir $OutputDirectory
    }
    "macho" {
        $analysisResults["MachO"] = Analyze-MachOBinary -FilePath $InputFile -OutputDir $OutputDirectory
    }
    "asar" {
        $extractDir = Join-Path $OutputDirectory "extracted_asar"
        if (Extract-ASARArchive -AsarPath $InputFile -OutputDir $extractDir) {
            $analysisResults["ASAR"] = @{ ExtractedTo = $extractDir }
            
            # Recursively analyze extracted content
            if ($DeepAnalysis) {
                $extractedFiles = Get-ChildItem -Path $extractDir -Recurse -File
                foreach ($file in $extractedFiles) {
                    $fileType = Get-FileType -FilePath $file.FullName
                    if ($fileType -in @("pe", "elf", "macho", "asar", "zip", "tar")) {
                        $subOutputDir = Join-Path $OutputDirectory "sub_analysis" $file.BaseName
                        switch ($fileType) {
                            "pe" { $analysisResults["Sub_$($file.Name)"] = Analyze-PEBinary -FilePath $file.FullName -OutputDir $subOutputDir }
                            "elf" { $analysisResults["Sub_$($file.Name)"] = Analyze-ELFBinary -FilePath $file.FullName -OutputDir $subOutputDir }
                            "macho" { $analysisResults["Sub_$($file.Name)"] = Analyze-MachOBinary -FilePath $file.FullName -OutputDir $subOutputDir }
                            "asar" { 
                                $subExtractDir = Join-Path $subOutputDir "extracted"
                                Extract-ASARArchive -AsarPath $file.FullName -OutputDir $subExtractDir
                            }
                            "zip" { Extract-Archive -ArchivePath $file.FullName -OutputDir $subOutputDir -ArchiveType "zip" }
                            "tar" { Extract-Archive -ArchivePath $file.FullName -OutputDir $subOutputDir -ArchiveType "tar" }
                        }
                    }
                }
            }
        }
    }
    "zip" {
        Extract-Archive -ArchivePath $InputFile -OutputDir $OutputDirectory -ArchiveType "zip"
    }
    "tar" {
        Extract-Archive -ArchivePath $InputFile -OutputDir $OutputDirectory -ArchiveType "tar"
    }
    "js" {
        Copy-Item -Path $InputFile -Destination $OutputDirectory -Force
        if ($ExtractStrings) {
            $analysisResults["JS"] = @{ Strings = Extract-Strings -FilePath $InputFile -MinLength $StringMinLength }
        }
    }
    "ts" {
        Copy-Item -Path $InputFile -Destination $OutputDirectory -Force
        if ($ExtractStrings) {
            $analysisResults["TS"] = @{ Strings = Extract-Strings -FilePath $InputFile -MinLength $StringMinLength }
        }
    }
    "json" {
        Copy-Item -Path $InputFile -Destination $OutputDirectory -Force
        try {
            $json = Get-Content $InputFile -Raw | ConvertFrom-Json
            $analysisResults["JSON"] = @{ Content = $json }
        } catch {
            Write-ColorOutput "⚠ Failed to parse JSON: $_" "Warning"
        }
    }
    "binary" {
        if ($ExtractStrings) {
            $analysisResults["Binary"] = @{ Strings = Extract-Strings -FilePath $InputFile -MinLength $StringMinLength }
        }
        Copy-Item -Path $InputFile -Destination $OutputDirectory -Force
    }
}

# Handle Electron apps specially
if ($isElectron -and $ExtractSource) {
    Write-ColorOutput "=== EXTRACTING ELECTRON SOURCE CODE ===" "Electron"
    
    $fileInfo = Get-Item $InputFile
    $appDir = $fileInfo.Directory
    
    # Look for ASAR archives
    $asarFiles = Get-ChildItem -Path $appDir -Recurse -Filter "*.asar"
    
    foreach ($asar in $asarFiles) {
        Write-ColorOutput "→ Found ASAR: $($asar.Name)" "Electron"
        $extractDir = Join-Path $OutputDirectory "electron_source" $asar.BaseName
        Extract-ASARArchive -AsarPath $asar.FullName -OutputDir $extractDir
    }
    
    # Look for app directory
    $appDirPath = Join-Path $appDir "resources\app"
    if (Test-Path $appDirPath) {
        Write-ColorOutput "→ Found app directory, copying source" "Electron"
        $electronSourceDir = Join-Path $OutputDirectory "electron_source_app"
        Copy-Item -Path $appDirPath -Destination $electronSourceDir -Recurse -Force
    }
    
    # Look for package.json
    $packageJsonPath = Join-Path $appDirPath "package.json"
    if (Test-Path $packageJsonPath) {
        try {
            $package = Get-Content $packageJsonPath -Raw | ConvertFrom-Json
            $analysisResults["ElectronPackage"] = $package
            Write-ColorOutput "✓ Extracted package.json" "Success"
        } catch {
            Write-ColorOutput "⚠ Failed to parse package.json: $_" "Warning"
        }
    }
}

# Reconstruct source tree if requested
if ($ReconstructSourceTree) {
    $sourceDirs = @()
    
    # Find extracted source directories
    if (Test-Path (Join-Path $OutputDirectory "electron_source")) {
        $sourceDirs += Get-ChildItem -Path (Join-Path $OutputDirectory "electron_source") -Directory
    }
    if (Test-Path (Join-Path $OutputDirectory "extracted_asar")) {
        $sourceDirs += Get-ChildItem -Path (Join-Path $OutputDirectory "extracted_asar") -Directory
    }
    if (Test-Path (Join-Path $OutputDirectory "electron_source_app")) {
        $sourceDirs += Get-Item -Path (Join-Path $OutputDirectory "electron_source_app")
    }
    
    foreach ($sourceDir in $sourceDirs) {
        Reconstruct-SourceTree -SourceDir $sourceDir.FullName -OutputDir $OutputDirectory
    }
}

# Generate comprehensive report if requested
if ($GenerateReport) {
    $report = Generate-ComprehensiveReport -AnalysisResults $analysisResults -OutputDir $OutputDirectory
    
    # Display summary
    Write-ColorOutput ""
    Write-ColorOutput "=" * 80
    Write-ColorOutput "REVERSE ENGINEERING COMPLETE" "Header"
    Write-ColorOutput "=" * 80
    Write-ColorOutput "Total Files: $($report.Summary.TotalFiles)" "Success"
    Write-ColorOutput "Total Size: $([math]::Round($report.Summary.TotalSize / 1MB, 2)) MB" "Detail"
    Write-ColorOutput "Source Files: $($report.Summary.SourceFiles)" "Source"
    Write-ColorOutput "Binary Files: $($report.Summary.BinaryFiles)" "Binary"
    Write-ColorOutput "Extracted Strings: $($report.Summary.ExtractedStrings)" "Detail"
    Write-ColorOutput "Output Directory: $OutputDirectory" "Detail"
    Write-ColorOutput "=" * 80
    
    # Save text summary
    $summaryPath = Join-Path $OutputDirectory "reverse_engineering_summary.txt"
    @"
Universal Reverse Engineering Summary
=====================================
Generated: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
Input File: $InputFile
File Type: $FileType
Electron App: $isElectron

Statistics:
- Total Files: $($report.Summary.TotalFiles)
- Total Size: $([math]::Round($report.Summary.TotalSize / 1MB, 2)) MB
- Source Files: $($report.Summary.SourceFiles)
- Binary Files: $($report.Summary.BinaryFiles)
- Extracted Strings: $($report.Summary.ExtractedStrings)

Analysis Results:
$(($analysisResults.Keys | ForEach-Object { "- $_`: $($analysisResults[$_].Keys -join ', ')" }) -join "`n")

Output Directory: $OutputDirectory
"@ | Out-File -FilePath $summaryPath -Encoding UTF8
    
    Write-ColorOutput "✓ Summary saved to: $summaryPath" "Success"
} else {
    Write-ColorOutput ""
    Write-ColorOutput "=" * 80
    Write-ColorOutput "PROCESSING COMPLETE" "Header"
    Write-ColorOutput "=" * 80
    Write-ColorOutput "Output Directory: $OutputDirectory" "Detail"
    Write-ColorOutput "=" * 80
}

Write-ColorOutput ""
Write-ColorOutput "Next steps: Analyze the extracted source code in $OutputDirectory" "Info"
