<#
.SYNOPSIS
    Reverse Engineering Engine - Automated Code Analysis and Reconstruction
.DESCRIPTION
    Advanced reverse engineering system for:
    - Binary analysis and decompilation
    - API extraction and documentation
    - Dependency mapping
    - Security vulnerability detection
    - Performance bottleneck identification
    - Code structure reconstruction
#>

using namespace System.Collections.Generic
using namespace System.IO

param(
    [Parameter(Mandatory=$false)]
    [string]$TargetPath,
    
    [Parameter(Mandatory=$false)]
    [string]$OutputPath = ".\reverse_engineering_output",
    
    [Parameter(Mandatory=$false)]
    [string]$ConfigPath = ".\reverse_engineering_config.json",
    
    [Parameter(Mandatory=$false)]
    [switch]$AnalyzeBinaries,
    
    [Parameter(Mandatory=$false)]
    [switch]$ExtractAPIs,
    
    [Parameter(Mandatory=$false)]
    [switch]$MapDependencies,
    
    [Parameter(Mandatory=$false)]
    [switch]$DetectVulnerabilities,
    
    [Parameter(Mandatory=$false)]
    [switch]$IdentifyBottlenecks,
    
    [Parameter(Mandatory=$false)]
    [switch]$ReconstructStructure,
    
    [Parameter(Mandatory=$false)]
    [switch]$GenerateDocumentation,
    
    [Parameter(Mandatory=$false)]
    [switch]$Interactive
)

# Global configuration
$global:ReverseEngineeringConfig = @{
    Version = "1.0.0"
    EngineName = "ReverseEngineeringEngine"
    AnalysisDepth = "Deep"  # Shallow, Medium, Deep, Full
    EnableParallelProcessing = $true
    MaxThreadCount = 8
    BinaryAnalysis = @{
        Enabled = $true
        ExtractStrings = $true
        IdentifyFunctions = $true
        DetectPackers = $true
        AnalyzeHeaders = $true
    }
    APIExtraction = @{
        Enabled = $true
        ExtractEndpoints = $true
        DocumentParameters = $true
        IdentifyAuthentication = $true
        MapDataFlows = $true
    }
    DependencyMapping = @{
        Enabled = $true
        TrackImports = $true
        IdentifyExternalCalls = $true
        MapLibraryDependencies = $true
        DetectCircularDependencies = $true
    }
    SecurityAnalysis = @{
        Enabled = $true
        ScanForVulnerabilities = $true
        IdentifyHardcodedSecrets = $true
        DetectInjectionPoints = $true
        AnalyzePermissions = $true
    }
    PerformanceAnalysis = @{
        Enabled = $true
        IdentifyBottlenecks = $true
        AnalyzeMemoryUsage = $true
        TrackResourceConsumption = $true
        DetectBlockingOperations = $true
    }
    CodeReconstruction = @{
        Enabled = $true
        ReconstructClasses = $true
        ReconstructFunctions = $true
        ReconstructDataStructures = $true
        GeneratePseudoCode = $true
    }
}

# Analysis results storage
$global:ReverseEngineeringResults = @{
    Binaries = [List[hashtable]]::new()
    APIs = [List[hashtable]]::new()
    Dependencies = [List[hashtable]]::new()
    Vulnerabilities = [List[hashtable]]::new()
    Bottlenecks = [List[hashtable]]::new()
    ReconstructedCode = [List[hashtable]]::new()
    Statistics = @{
        TotalFiles = 0
        TotalFunctions = 0
        TotalClasses = 0
        TotalAPIs = 0
        TotalVulnerabilities = 0
        TotalBottlenecks = 0
        AnalysisDuration = 0
    }
}

# Initialize logging
function Initialize-Logging {
    param($LogPath)
    
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $global:LogFile = Join-Path $LogPath "reverse_engineering_$timestamp.log"
    $global:EmergencyLog = Join-Path $LogPath "emergency_reverse_engineering.log"
    
    # Create log directory
    if (!(Test-Path $LogPath)) {
        New-Item -ItemType Directory -Path $LogPath -Force | Out-Null
    }
    
    Write-Log "INFO" "Reverse Engineering Engine v$($global:ReverseEngineeringConfig.Version) initialized"
    Write-Log "INFO" "Log file: $global:LogFile"
}

# Enhanced logging function
function Write-Log {
    param(
        [string]$Level = "INFO",
        [string]$Message,
        [string]$Component = "Main"
    )
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
    $logEntry = "[$timestamp] [$Level] [$Component] $Message"
    
    # Console output with colors
    $color = switch ($Level) {
        "ERROR" { "Red" }
        "WARNING" { "Yellow" }
        "SUCCESS" { "Green" }
        "INFO" { "Cyan" }
        "DEBUG" { "Gray" }
        default { "White" }
    }
    
    Write-Host $logEntry -ForegroundColor $color
    
    # File logging
    if ($global:LogFile) {
        Add-Content -Path $global:LogFile -Value $logEntry -Encoding UTF8
    }
}

# Load configuration
function Load-Configuration {
    param($ConfigPath)
    
    if (Test-Path $ConfigPath) {
        try {
            $configContent = Get-Content $ConfigPath -Raw | ConvertFrom-Json
            
            # Merge configurations
            foreach ($key in $configContent.PSObject.Properties.Name) {
                if ($global:ReverseEngineeringConfig.ContainsKey($key)) {
                    $value = $configContent.$key
                    if ($value -is [System.Management.Automation.PSCustomObject]) {
                        foreach ($subKey in $value.PSObject.Properties.Name) {
                            $global:ReverseEngineeringConfig[$key][$subKey] = $value.$subKey
                        }
                    }
                    else {
                        $global:ReverseEngineeringConfig[$key] = $value
                    }
                }
            }
            
            Write-Log "INFO" "Configuration loaded from $ConfigPath"
        }
        catch {
            Write-Log "WARNING" "Failed to load configuration: $_"
        }
    }
}

# Save configuration
function Save-Configuration {
    param($ConfigPath)
    
    try {
        $global:ReverseEngineeringConfig | ConvertTo-Json -Depth 10 | Out-File $ConfigPath -Encoding UTF8
        Write-Log "INFO" "Configuration saved to $ConfigPath"
    }
    catch {
        Write-Log "ERROR" "Failed to save configuration: $_"
    }
}

# Main execution function
function Start-ReverseEngineering {
    param(
        [string]$TargetPath,
        [string]$OutputPath
    )
    
    Write-Log "INFO" "Starting reverse engineering process"
    Write-Log "INFO" "Target: $TargetPath"
    Write-Log "INFO" "Output: $OutputPath"
    
    # Validate target path
    if (!(Test-Path $TargetPath)) {
        Write-Log "ERROR" "Target path does not exist: $TargetPath"
        return $false
    }
    
    # Create output directory
    if (!(Test-Path $OutputPath)) {
        New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
        Write-Log "INFO" "Created output directory: $OutputPath"
    }
    
    # Initialize logging
    Initialize-Logging -LogPath (Join-Path $OutputPath "logs")
    
    $startTime = Get-Date
    
    # Phase 1: Target Discovery
    Write-Log "INFO" "Phase 1: Target Discovery"
    $discoveryResults = Start-TargetDiscovery -TargetPath $TargetPath
    
    if ($discoveryResults.TotalFiles -eq 0) {
        Write-Log "ERROR" "No target files found in $TargetPath"
        return $false
    }
    
    Write-Log "SUCCESS" "Discovered $($discoveryResults.TotalFiles) files"
    
    # Phase 2: Binary Analysis (if enabled)
    if ($AnalyzeBinaries -or $global:ReverseEngineeringConfig.BinaryAnalysis.Enabled) {
        Write-Log "INFO" "Phase 2: Binary Analysis"
        $binaryResults = Start-BinaryAnalysis -TargetFiles $discoveryResults.Files
    }
    
    # Phase 3: API Extraction (if enabled)
    if ($ExtractAPIs -or $global:ReverseEngineeringConfig.APIExtraction.Enabled) {
        Write-Log "INFO" "Phase 3: API Extraction"
        $apiResults = Start-APIExtraction -TargetFiles $discoveryResults.Files
    }
    
    # Phase 4: Dependency Mapping (if enabled)
    if ($MapDependencies -or $global:ReverseEngineeringConfig.DependencyMapping.Enabled) {
        Write-Log "INFO" "Phase 4: Dependency Mapping"
        $dependencyResults = Start-DependencyMapping -TargetFiles $discoveryResults.Files
    }
    
    # Phase 5: Vulnerability Detection (if enabled)
    if ($DetectVulnerabilities -or $global:ReverseEngineeringConfig.SecurityAnalysis.Enabled) {
        Write-Log "INFO" "Phase 5: Vulnerability Detection"
        $vulnerabilityResults = Start-VulnerabilityDetection -TargetFiles $discoveryResults.Files
    }
    
    # Phase 6: Performance Analysis (if enabled)
    if ($IdentifyBottlenecks -or $global:ReverseEngineeringConfig.PerformanceAnalysis.Enabled) {
        Write-Log "INFO" "Phase 6: Performance Analysis"
        $performanceResults = Start-PerformanceAnalysis -TargetFiles $discoveryResults.Files
    }
    
    # Phase 7: Code Reconstruction (if enabled)
    if ($ReconstructStructure -or $global:ReverseEngineeringConfig.CodeReconstruction.Enabled) {
        Write-Log "INFO" "Phase 7: Code Reconstruction"
        $reconstructionResults = Start-CodeReconstruction -TargetFiles $discoveryResults.Files
    }
    
    # Phase 8: Documentation Generation (if enabled)
    if ($GenerateDocumentation) {
        Write-Log "INFO" "Phase 8: Documentation Generation"
        $documentationPath = Generate-Documentation -OutputPath $OutputPath
        Write-Log "SUCCESS" "Documentation generated: $documentationPath"
    }
    
    # Update statistics
    $endTime = Get-Date
    $global:ReverseEngineeringResults.Statistics.AnalysisDuration = ($endTime - $startTime).TotalSeconds
    
    # Save results
    Save-ReverseEngineeringResults -OutputPath $OutputPath
    
    Write-Log "SUCCESS" "Reverse engineering completed successfully"
    Write-Log "INFO" "Analysis duration: $($global:ReverseEngineeringResults.Statistics.AnalysisDuration) seconds"
    
    return $true
}

# Target discovery function
function Start-TargetDiscovery {
    param($TargetPath)
    
    Write-Log "INFO" "Discovering target files..."
    
    $results = @{
        Files = [List[hashtable]]::new()
        TotalFiles = 0
        FileTypes = @{}
        TotalSize = 0
    }
    
    # File extensions mapping
    $fileTypeMap = @{
        # Executables
        '.exe' = 'Executable'; '.dll' = 'Library'; '.so' = 'Library'
        '.bin' = 'Binary'; '.o' = 'Object'; '.obj' = 'Object'
        '.a' = 'Archive'; '.lib' = 'Library'
        
        # Scripts
        '.ps1' = 'PowerShell'; '.psm1' = 'PowerShellModule'; '.psd1' = 'PowerShellData'
        '.py' = 'Python'; '.pyw' = 'Python'; '.pyc' = 'PythonCompiled'
        '.js' = 'JavaScript'; '.mjs' = 'JavaScriptModule'
        '.sh' = 'Shell'; '.bash' = 'Bash'; '.zsh' = 'Zsh'
        '.bat' = 'Batch'; '.cmd' = 'BatchCommand'
        
        # Source code
        '.cpp' = 'Cpp'; '.cc' = 'Cpp'; '.cxx' = 'Cpp'; '.c' = 'C'
        '.hpp' = 'CppHeader'; '.h' = 'CHeader'; '.hxx' = 'CppHeader'
        '.cs' = 'CSharp'; '.csx' = 'CSharpScript'
        '.java' = 'Java'; '.class' = 'JavaClass'
        '.ts' = 'TypeScript'; '.tsx' = 'TypeScriptReact'
        '.rs' = 'Rust'; '.go' = 'Go'
        '.asm' = 'Assembly'; '.s' = 'Assembly'
        
        # Configuration
        '.json' = 'JSON'; '.xml' = 'XML'; '.yaml' = 'YAML'; '.yml' = 'YAML'
        '.ini' = 'INI'; '.conf' = 'Config'; '.config' = 'Config'
        '.toml' = 'TOML'
        
        # Documentation
        '.md' = 'Markdown'; '.txt' = 'Text'; '.rst' = 'ReStructuredText'
        '.html' = 'HTML'; '.htm' = 'HTML'; '.css' = 'CSS'
        
        # Archives
        '.zip' = 'ZIP'; '.tar' = 'TAR'; '.gz' = 'GZIP'; '.7z' = '7ZIP'
        '.rar' = 'RAR'
        
        # Database
        '.sql' = 'SQL'; '.db' = 'Database'; '.sqlite' = 'SQLite'
        '.mdb' = 'AccessDB'
        
        # Images
        '.png' = 'PNG'; '.jpg' = 'JPEG'; '.jpeg' = 'JPEG'; '.gif' = 'GIF'
        '.bmp' = 'BMP'; '.ico' = 'Icon'; '.svg' = 'SVG'
        
        # Documents
        '.pdf' = 'PDF'; '.doc' = 'Word'; '.docx' = 'WordX'
        '.xls' = 'Excel'; '.xlsx' = 'ExcelX'
        '.ppt' = 'PowerPoint'; '.pptx' = 'PowerPointX'
    }
    
    # Get all files recursively
    $allFiles = Get-ChildItem -Path $TargetPath -Recurse -File -ErrorAction SilentlyContinue
    
    foreach ($file in $allFiles) {
        $extension = $file.Extension.ToLower()
        $fileType = $fileTypeMap[$extension]
        
        if ($fileType) {
            $fileInfo = @{
                Path = $file.FullName
                Name = $file.Name
                Extension = $extension
                Type = $fileType
                Size = $file.Length
                LastModified = $file.LastWriteTime
                RelativePath = $file.FullName.Replace($TargetPath, "").TrimStart('\', '/')
                Hash = ""
                Metadata = @{}
                AnalysisStatus = "Pending"
            }
            
            # Calculate hash for smaller files
            if ($file.Length -lt 100MB) {
                try {
                    $fileInfo.Hash = (Get-FileHash $file.FullName -Algorithm SHA256).Hash
                }
                catch {
                    $fileInfo.Hash = "N/A"
                }
            }
            
            $results.Files.Add($fileInfo)
            $results.TotalSize += $file.Length
            
            # Track file types
            if (!$results.FileTypes[$fileType]) {
                $results.FileTypes[$fileType] = 0
            }
            $results.FileTypes[$fileType]++
        }
    }
    
    $results.TotalFiles = $results.Files.Count
    
    Write-Log "INFO" "Discovery complete: $($results.TotalFiles) files, $($results.FileTypes.Count) types"
    
    return $results
}

# Binary analysis function
function Start-BinaryAnalysis {
    param($TargetFiles)
    
    Write-Log "INFO" "Starting binary analysis of $($TargetFiles.Count) files"
    
    $binaryFiles = $TargetFiles | Where-Object { $_.Type -in @('Executable', 'Library', 'Object', 'Binary') }
    
    foreach ($file in $binaryFiles) {
        Write-Log "DEBUG" "Analyzing binary: $($file.Name)"
        
        $binaryAnalysis = @{
            File = $file
            Architecture = "Unknown"
            Compiler = "Unknown"
            Packer = "None"
            EntryPoints = [List[string]]::new()
            ImportedFunctions = [List[string]]::new()
            ExportedFunctions = [List[string]]::new()
            Strings = [List[string]]::new()
            Sections = [List[hashtable]]::new()
            SecurityFeatures = [List[string]]::new()
            Vulnerabilities = [List[hashtable]]::new()
        }
        
        try {
            # Analyze file headers
            $headerAnalysis = Analyze-BinaryHeader -FilePath $file.Path
            $binaryAnalysis.Architecture = $headerAnalysis.Architecture
            $binaryAnalysis.Compiler = $headerAnalysis.Compiler
            $binaryAnalysis.Packer = $headerAnalysis.Packer
            $binaryAnalysis.EntryPoints = $headerAnalysis.EntryPoints
            
            # Extract strings
            if ($global:ReverseEngineeringConfig.BinaryAnalysis.ExtractStrings) {
                $binaryAnalysis.Strings = Extract-Strings -FilePath $file.Path
            }
            
            # Identify functions
            if ($global:ReverseEngineeringConfig.BinaryAnalysis.IdentifyFunctions) {
                $functionAnalysis = Identify-BinaryFunctions -FilePath $file.Path
                $binaryAnalysis.ImportedFunctions = $functionAnalysis.Imported
                $binaryAnalysis.ExportedFunctions = $functionAnalysis.Exported
            }
            
            # Detect packers
            if ($global:ReverseEngineeringConfig.BinaryAnalysis.DetectPackers) {
                $packerDetection = Detect-BinaryPackers -FilePath $file.Path
                $binaryAnalysis.Packer = $packerDetection.Packer
                $binaryAnalysis.SecurityFeatures = $packerDetection.SecurityFeatures
            }
            
            # Analyze sections
            $binaryAnalysis.Sections = Analyze-BinarySections -FilePath $file.Path
            
            $file.AnalysisStatus = "Completed"
        }
        catch {
            Write-Log "WARNING" "Failed to analyze binary $($file.Name): $_"
            $file.AnalysisStatus = "Failed"
            $binaryAnalysis.Vulnerabilities.Add(@{
                Type = "AnalysisFailure"
                Description = $_
                Severity = "Low"
            })
        }
        
        $global:ReverseEngineeringResults.Binaries.Add($binaryAnalysis)
        $global:ReverseEngineeringResults.Statistics.TotalFunctions += $binaryAnalysis.ImportedFunctions.Count + $binaryAnalysis.ExportedFunctions.Count
    }
    
    Write-Log "SUCCESS" "Binary analysis completed for $($binaryFiles.Count) files"
    
    return $global:ReverseEngineeringResults.Binaries
}

# Binary header analysis
function Analyze-BinaryHeader {
    param($FilePath)
    
    $analysis = @{
        Architecture = "Unknown"
        Compiler = "Unknown"
        Packer = "None"
        EntryPoints = [List[string]]::new()
        FileFormat = "Unknown"
        Bitness = "Unknown"
    }
    
    try {
        # Read first 1024 bytes for header analysis
        $bytes = [System.IO.File]::ReadAllBytes($FilePath)[0..1023]
        
        # Check for PE (Portable Executable) format
        if ($bytes[0] -eq 0x4D -and $bytes[1] -eq 0x5A) {  # MZ header
            $analysis.FileFormat = "PE"
            
            # Check PE header offset
            $peOffset = [BitConverter]::ToInt32($bytes, 60)
            
            if ($peOffset -lt $bytes.Length - 4) {
                # Check PE signature
                if ($bytes[$peOffset] -eq 0x50 -and $bytes[$peOffset + 1] -eq 0x45) {
                    # Analyze PE header
                    $analysis = Analyze-PEHeader -Bytes $bytes -Offset $peOffset
                }
            }
        }
        # Check for ELF format
        elseif ($bytes[0] -eq 0x7F -and $bytes[1] -eq 0x45 -and $bytes[2] -eq 0x4C -and $bytes[3] -eq 0x46) {
            $analysis.FileFormat = "ELF"
            $analysis = Analyze-ELFHeader -Bytes $bytes
        }
        # Check for Mach-O format
        elseif ($bytes[0] -eq 0xFE -and $bytes[1] -eq 0xED -and $bytes[2] -eq 0xFA -and $bytes[3] -eq 0xCE) {
            $analysis.FileFormat = "Mach-O"
            $analysis = Analyze-MachOHeader -Bytes $bytes
        }
        # Check for shell scripts or text-based executables
        elseif ($bytes[0] -eq 0x23 -and $bytes[1] -eq 0x21) {  # Shebang (#!)
            $analysis.FileFormat = "Script"
            $analysis = Analyze-ScriptHeader -Bytes $bytes
        }
    }
    catch {
        Write-Log "WARNING" "Failed to analyze binary header: $_"
    }
    
    return $analysis
}

# PE header analysis
function Analyze-PEHeader {
    param($Bytes, $Offset)
    
    $analysis = @{
        Architecture = "Unknown"
        Compiler = "Unknown"
        Packer = "None"
        EntryPoints = [List[string]]::new()
        FileFormat = "PE"
        Bitness = "Unknown"
    }
    
    try {
        # Machine type (offset 4 from PE header)
        $machineType = [BitConverter]::ToUInt16($Bytes, $Offset + 4)
        
        switch ($machineType) {
            0x014C { $analysis.Architecture = "x86" }
            0x8664 { $analysis.Architecture = "x64" }
            0x0200 { $analysis.Architecture = "IA64" }
            0x01C0 { $analysis.Architecture = "ARM" }
            0xAA64 { $analysis.Architecture = "ARM64" }
            default { $analysis.Architecture = "Unknown ($machineType)" }
        }
        
        # Determine bitness
        if ($analysis.Architecture -eq "x86") {
            $analysis.Bitness = "32-bit"
        }
        elseif ($analysis.Architecture -eq "x64") {
            $analysis.Bitness = "64-bit"
        }
        
        # Number of sections
        $numSections = [BitConverter]::ToUInt16($Bytes, $Offset + 6)
        
        # Time date stamp
        $timeDateStamp = [BitConverter]::ToUInt32($Bytes, $Offset + 8)
        
        # Entry point
        $entryPoint = [BitConverter]::ToUInt32($Bytes, $Offset + 40)
        $analysis.EntryPoints.Add("0x$($entryPoint.ToString('X8'))")
        
        # Detect compiler and packer signatures
        $compilerDetection = Detect-CompilerAndPacker -Bytes $Bytes -Offset $Offset
        $analysis.Compiler = $compilerDetection.Compiler
        $analysis.Packer = $compilerDetection.Packer
        
    }
    catch {
        Write-Log "WARNING" "Failed to analyze PE header: $_"
    }
    
    return $analysis
}

# Compiler and packer detection
function Detect-CompilerAndPacker {
    param($Bytes, $Offset)
    
    $detection = @{
        Compiler = "Unknown"
        Packer = "None"
        SecurityFeatures = [List[string]]::new()
    }
    
    try {
        # Common compiler signatures
        $compilerSignatures = @{
            "Microsoft Visual C++" = @(
                "Microsoft Visual C++ Runtime",
                "MSVC",
                "Visual Studio"
            )
            "GCC" = @(
                "GCC: (GNU)",
                "MinGW",
                "mingw"
            )
            "Clang" = @(
                "clang version",
                "LLVM"
            )
            "Intel C++" = @(
                "Intel(R) C++",
                "ICC"
            )
        }
        
        # Common packer signatures
        $packerSignatures = @{
            "UPX" = @(
                "UPX!",
                "This file is packed with the UPX executable packer"
            )
            "ASPack" = @(
                "ASPack",
                "aspack"
            )
            "PECompact" = @(
                "PECompact2",
                "PEC2"
            )
            "Themida" = @(
                "Themida",
                "www.oreans.com"
            )
        }
        
        # Convert bytes to string for signature scanning
        $stringContent = [System.Text.Encoding]::ASCII.GetString($Bytes)
        
        # Check for compiler signatures
        foreach ($compiler in $compilerSignatures.Keys) {
            foreach ($signature in $compilerSignatures[$compiler]) {
                if ($stringContent -like "*$signature*") {
                    $detection.Compiler = $compiler
                    break
                }
            }
            if ($detection.Compiler -ne "Unknown") { break }
        }
        
        # Check for packer signatures
        foreach ($packer in $packerSignatures.Keys) {
            foreach ($signature in $packerSignatures[$packer]) {
                if ($stringContent -like "*$signature*") {
                    $detection.Packer = $packer
                    break
                }
            }
            if ($detection.Packer -ne "None") { break }
        }
        
        # Check for security features
        $securityFeatures = @{
            "ASLR" = "ASLR"
            "DEP" = "DEP"
            "SafeSEH" = "SafeSEH"
            "Control Flow Guard" = "CFG"
        }
        
        foreach ($feature in $securityFeatures.Keys) {
            if ($stringContent -like "*$($securityFeatures[$feature])*") {
                $detection.SecurityFeatures.Add($feature)
            }
        }
        
    }
    catch {
        Write-Log "WARNING" "Failed to detect compiler/packer: $_"
    }
    
    return $detection
}

# String extraction
function Extract-Strings {
    param($FilePath)
    
    $strings = [List[string]]::new()
    
    try {
        # Use strings.exe or built-in extraction
        $bytes = [System.IO.File]::ReadAllBytes($FilePath)
        $currentString = ""
        
        foreach ($byte in $bytes) {
            if ($byte -ge 32 -and $byte -le 126) {  # Printable ASCII
                $currentString += [char]$byte
            }
            else {
                if ($currentString.Length -ge 4) {  # Minimum string length
                    $strings.Add($currentString)
                }
                $currentString = ""
            }
        }
        
        # Add last string if any
        if ($currentString.Length -ge 4) {
            $strings.Add($currentString)
        }
        
        # Remove duplicates and sort
        $uniqueStrings = $strings | Sort-Object -Unique
        
        Write-Log "DEBUG" "Extracted $($uniqueStrings.Count) strings from $FilePath"
        
        return $uniqueStrings
    }
    catch {
        Write-Log "WARNING" "Failed to extract strings from $FilePath : $($_.Exception.Message)"
        return $strings
    }
}

# API extraction function
function Start-APIExtraction {
    param($TargetFiles)
    
    Write-Log "INFO" "Starting API extraction from $($TargetFiles.Count) files"
    
    $sourceFiles = $TargetFiles | Where-Object { $_.Type -in @('PowerShell', 'Python', 'JavaScript', 'TypeScript', 'CSharp', 'Cpp', 'C', 'Java') }
    
    foreach ($file in $sourceFiles) {
        Write-Log "DEBUG" "Extracting APIs from: $($file.Name)"
        
        try {
            $content = Get-Content $file.Path -Raw -ErrorAction Stop
            
            $apiExtraction = @{
                File = $file
                Endpoints = [List[hashtable]]::new()
                Functions = [List[hashtable]]::new()
                Classes = [List[hashtable]]::new()
                DataStructures = [List[hashtable]]::new()
                AuthenticationMethods = [List[string]]::new()
                DataFlows = [List[hashtable]]::new()
            }
            
            # Language-specific API extraction
            switch ($file.Type) {
                "PowerShell" { Extract-PowerShellAPIs -Content $content -Extraction $apiExtraction }
                "Python" { Extract-PythonAPIs -Content $content -Extraction $apiExtraction }
                "JavaScript" { Extract-JavaScriptAPIs -Content $content -Extraction $apiExtraction }
                "TypeScript" { Extract-TypeScriptAPIs -Content $content -Extraction $apiExtraction }
                "CSharp" { Extract-CSharpAPIs -Content $content -Extraction $apiExtraction }
                "Cpp" { Extract-CppAPIs -Content $content -Extraction $apiExtraction }
                "C" { Extract-CAPIs -Content $content -Extraction $apiExtraction }
                "Java" { Extract-JavaAPIs -Content $content -Extraction $apiExtraction }
            }
            
            # Extract authentication methods
            $apiExtraction.AuthenticationMethods = Extract-AuthenticationMethods -Content $content
            
            # Map data flows
            $apiExtraction.DataFlows = Map-DataFlows -Content $content -Language $file.Type
            
            $global:ReverseEngineeringResults.APIs.Add($apiExtraction)
            $global:ReverseEngineeringResults.Statistics.TotalAPIs += $apiExtraction.Endpoints.Count
            $global:ReverseEngineeringResults.Statistics.TotalFunctions += $apiExtraction.Functions.Count
            $global:ReverseEngineeringResults.Statistics.TotalClasses += $apiExtraction.Classes.Count
            
            $file.AnalysisStatus = "Completed"
        }
        catch {
            Write-Log "WARNING" "Failed to extract APIs from $($file.Name): $_"
            $file.AnalysisStatus = "Failed"
        }
    }
    
    Write-Log "SUCCESS" "API extraction completed for $($sourceFiles.Count) files"
    
    return $global:ReverseEngineeringResults.APIs
}

# PowerShell API extraction
function Extract-PowerShellAPIs {
    param($Content, $Extraction)
    
    # Extract functions
    $functionMatches = [regex]::Matches($Content, '^\s*function\s+([\w-]+)\s*{')
    foreach ($match in $functionMatches) {
        $Extraction.Functions.Add(@{
            Name = $match.Groups[1].Value
            Type = "Function"
            Line = $match.Index
            Parameters = @()
            ReturnType = "Unknown"
            Visibility = "Public"
        })
    }
    
    # Extract cmdlets (API endpoints)
    $cmdletMatches = [regex]::Matches($Content, '\b([A-Z][a-z]+-[A-Z][a-z]+)\b')
    foreach ($match in $cmdletMatches) {
        $Extraction.Endpoints.Add(@{
            Name = $match.Groups[1].Value
            Type = "Cmdlet"
            Line = $match.Index
            Parameters = @()
            Description = "PowerShell cmdlet"
        })
    }
    
    # Extract classes
    $classMatches = [regex]::Matches($Content, '^\s*class\s+(\w+)')
    foreach ($match in $classMatches) {
        $Extraction.Classes.Add(@{
            Name = $match.Groups[1].Value
            Type = "Class"
            Line = $match.Index
            Methods = @()
            Properties = @()
            BaseClass = "None"
        })
    }
}

# Python API extraction
function Extract-PythonAPIs {
    param($Content, $Extraction)
    
    # Extract functions
    $functionMatches = [regex]::Matches($Content, '^\s*def\s+(\w+)\s*\(')
    foreach ($match in $functionMatches) {
        $Extraction.Functions.Add(@{
            Name = $match.Groups[1].Value
            Type = "Function"
            Line = $match.Index
            Parameters = @()
            ReturnType = "Unknown"
            Decorators = @()
        })
    }
    
    # Extract classes
    $classMatches = [regex]::Matches($Content, '^\s*class\s+(\w+)')
    foreach ($match in $classMatches) {
        $Extraction.Classes.Add(@{
            Name = $match.Groups[1].Value
            Type = "Class"
            Line = $match.Index
            Methods = @()
            Properties = @()
            BaseClasses = @()
        })
    }
    
    # Extract Flask/Django endpoints
    $flaskMatches = [regex]::Matches($Content, "@(\w+)\.route\(([`"`'])([^`"`']+)\2")
    foreach ($match in $flaskMatches) {
        $Extraction.Endpoints.Add(@{
            Method = $match.Groups[1].Value
            Path = $match.Groups[3].Value
            Type = "WebEndpoint"
            Line = $match.Index
            Framework = "Flask"
        })
    }
    
    # Extract FastAPI endpoints
    $fastapiMatches = [regex]::Matches($Content, "@(\w+)\.([a-z]+)\(([`"`'])([^`"`']+)\3")
    foreach ($match in $fastapiMatches) {
        $Extraction.Endpoints.Add(@{
            Decorator = $match.Groups[1].Value
            Method = $match.Groups[2].Value
            Path = $match.Groups[4].Value
            Type = "WebEndpoint"
            Line = $match.Index
            Framework = "FastAPI"
        })
    }
}

# JavaScript API extraction
function Extract-JavaScriptAPIs {
    param($Content, $Extraction)
    
    # Extract functions
    $functionMatches = [regex]::Matches($Content, '(?:function\s+(\w+)|(\w+)\s*=\s*function|(\w+)\s*:\s*function)')
    foreach ($match in $functionMatches) {
        $functionName = $match.Groups[1].Value
        if ([string]::IsNullOrEmpty($functionName)) { $functionName = $match.Groups[2].Value }
        if ([string]::IsNullOrEmpty($functionName)) { $functionName = $match.Groups[3].Value }
        
        if (![string]::IsNullOrEmpty($functionName)) {
            $Extraction.Functions.Add(@{
                Name = $functionName
                Type = "Function"
                Line = $match.Index
                Parameters = @()
                Async = $false
            })
        }
    }
    
    # Extract arrow functions
    $arrowMatches = [regex]::Matches($Content, '(\w+)\s*=\s*\([^)]*\)\s*=>')
    foreach ($match in $arrowMatches) {
        $Extraction.Functions.Add(@{
            Name = $match.Groups[1].Value
            Type = "ArrowFunction"
            Line = $match.Index
            Parameters = @()
            Async = $false
        })
    }
    
    # Extract async functions
    $asyncMatches = [regex]::Matches($Content, 'async\s+(?:function\s+(\w+)|(\w+)\s*=\s*async)')
    foreach ($match in $asyncMatches) {
        $functionName = $match.Groups[1].Value
        if ([string]::IsNullOrEmpty($functionName)) { $functionName = $match.Groups[2].Value }
        
        if (![string]::IsNullOrEmpty($functionName)) {
            $Extraction.Functions.Add(@{
                Name = $functionName
                Type = "AsyncFunction"
                Line = $match.Index
                Parameters = @()
                Async = $true
            })
        }
    }
    
    # Extract Express.js endpoints
    $expressMatches = [regex]::Matches($Content, "(\w+)\.(get|post|put|delete|patch)\(([`"`'])([^`"`']+)\3")
    foreach ($match in $expressMatches) {
        $Extraction.Endpoints.Add(@{
            Router = $match.Groups[1].Value
            Method = $match.Groups[2].Value.ToUpper()
            Path = $match.Groups[4].Value
            Type = "WebEndpoint"
            Line = $match.Index
            Framework = "Express.js"
        })
    }
    
    # Extract classes
    $classMatches = [regex]::Matches($Content, '(?:class\s+(\w+)|(\w+)\s*=\s*class)')
    foreach ($match in $classMatches) {
        $className = $match.Groups[1].Value
        if ([string]::IsNullOrEmpty($className)) { $className = $match.Groups[2].Value }
        
        if (![string]::IsNullOrEmpty($className)) {
            $Extraction.Classes.Add(@{
                Name = $className
                Type = "Class"
                Line = $match.Index
                Methods = @()
                Properties = @()
                Constructor = $null
            })
        }
    }
}

# Authentication method extraction
function Extract-AuthenticationMethods {
    param($Content)
    
    $authMethods = [List[string]]::new()
    
    # Common authentication patterns
    $authPatterns = @(
        "basic auth",
        "bearer token",
        "oauth",
        "jwt",
        "api key",
        "session",
        "cookie",
        "ldap",
        "saml",
        "openid"
    )
    
    $lowerContent = $Content.ToLower()
    
    foreach ($pattern in $authPatterns) {
        if ($lowerContent -like "*$pattern*") {
            $authMethods.Add($pattern.ToUpper())
        }
    }
    
    return $authMethods | Sort-Object -Unique
}

# Data flow mapping
function Map-DataFlows {
    param($Content, $Language)
    
    $dataFlows = [List[hashtable]]::new()
    
    # Common data flow patterns
    $flowPatterns = @{
        PowerShell = @{
            'Read-Host' = "UserInput"
            'Get-Content' = "FileRead"
            'Set-Content' = "FileWrite"
            'Invoke-RestMethod' = "APIRequest"
            'Invoke-WebRequest' = "WebRequest"
        }
        Python = @{
            'input(' = "UserInput"
            'open(' = "FileIO"
            'requests.' = "HTTPRequest"
            'json.load' = "JSONParse"
            'json.dump' = "JSONWrite"
        }
        JavaScript = @{
            'prompt(' = "UserInput"
            'fetch(' = "HTTPRequest"
            'localStorage' = "LocalStorage"
            'sessionStorage' = "SessionStorage"
            'XMLHttpRequest' = "XHRRequest"
        }
    }
    
    $patterns = $flowPatterns[$Language]
    if ($patterns) {
        foreach ($pattern in $patterns.Keys) {
            $matches = [regex]::Matches($Content, [regex]::Escape($pattern))
            foreach ($match in $matches) {
                $dataFlows.Add(@{
                    Pattern = $pattern
                    Type = $patterns[$pattern]
                    Line = $match.Index
                    Direction = if ($patterns[$pattern] -like "*Write*" -or $patterns[$pattern] -like "*Request*") { "Output" } else { "Input" }
                })
            }
        }
    }
    
    return $dataFlows
}

# Dependency mapping function
function Start-DependencyMapping {
    param($TargetFiles)
    
    Write-Log "INFO" "Starting dependency mapping for $($TargetFiles.Count) files"
    
    $dependencyGraph = @{
        Nodes = [List[hashtable]]::new()
        Edges = [List[hashtable]]::new()
        ExternalDependencies = [List[string]]::new()
        CircularDependencies = [List[hashtable]]::new()
    }
    
    foreach ($file in $TargetFiles) {
        $node = @{
            Id = $file.RelativePath
            Name = $file.Name
            Type = $file.Type
            Dependencies = [List[string]]::new()
            Dependents = [List[string]]::new()
            ExternalCalls = [List[string]]::new()
        }
        
        try {
            if ($file.Size -lt 10MB) {
                $content = Get-Content $file.Path -Raw -ErrorAction Stop
                
                # Extract dependencies based on file type
                switch ($file.Type) {
                    "PowerShell" {
                        $dependencies = Extract-PowerShellDependencies -Content $content
                    }
                    "Python" {
                        $dependencies = Extract-PythonDependencies -Content $content
                    }
                    "JavaScript" {
                        $dependencies = Extract-JavaScriptDependencies -Content $content
                    }
                    "CSharp" {
                        $dependencies = Extract-CSharpDependencies -Content $content
                    }
                    "Cpp" {
                        $dependencies = Extract-CppDependencies -Content $content
                    }
                    "C" {
                        $dependencies = Extract-CDependencies -Content $content
                    }
                    default {
                        $dependencies = Extract-GenericDependencies -Content $content
                    }
                }
                
                $node.Dependencies = $dependencies.Internal
                $node.ExternalCalls = $dependencies.External
                
                # Add external dependencies to global list
                foreach ($extDep in $dependencies.External) {
                    if ($extDep -notin $dependencyGraph.ExternalDependencies) {
                        $dependencyGraph.ExternalDependencies.Add($extDep)
                    }
                }
            }
        }
        catch {
            Write-Log "WARNING" "Failed to extract dependencies from $($file.Name): $_"
        }
        
        $dependencyGraph.Nodes.Add($node)
    }
    
    # Build edges between nodes
    foreach ($node in $dependencyGraph.Nodes) {
        foreach ($dependency in $node.Dependencies) {
            # Find the dependency in other nodes
            $dependentNode = $dependencyGraph.Nodes | Where-Object { $_.Name -eq $dependency -or $_.Id -like "*$dependency*" }
            
            if ($dependentNode) {
                $edge = @{
                    From = $node.Id
                    To = $dependentNode.Id
                    Type = "Internal"
                    Strength = "Strong"
                }
                $dependencyGraph.Edges.Add($edge)
                
                # Add to dependent's dependents list
                $dependentNode.Dependents.Add($node.Id)
            }
        }
    }
    
    # Detect circular dependencies
    $dependencyGraph.CircularDependencies = Detect-CircularDependencies -Graph $dependencyGraph
    
    $global:ReverseEngineeringResults.Dependencies = $dependencyGraph.Edges
    
    Write-Log "SUCCESS" "Dependency mapping completed: $($dependencyGraph.Nodes.Count) nodes, $($dependencyGraph.Edges.Count) edges"
    
    if ($dependencyGraph.CircularDependencies.Count -gt 0) {
        Write-Log "WARNING" "Detected $($dependencyGraph.CircularDependencies.Count) circular dependencies"
    }
    
    return $dependencyGraph
}

# PowerShell dependency extraction
function Extract-PowerShellDependencies {
    param($Content)
    
    $dependencies = @{
        Internal = [List[string]]::new()
        External = [List[string]]::new()
    }
    
    # Extract module imports
    $moduleMatches = [regex]::Matches($Content, '^\s*Import-Module\s+([\w.-]+)')
    foreach ($match in $moduleMatches) {
        $dependencies.External.Add($match.Groups[1].Value)
    }
    
    # Extract module imports with paths
    $modulePathMatches = [regex]::Matches($Content, "^\s*Import-Module\s+([`"`'])([^`"`'\s]+)\1")
    foreach ($match in $modulePathMatches) {
        $path = $match.Groups[2].Value
        if ($path -like "*.psm1" -or $path -like "*.ps1") {
            $dependencies.Internal.Add([System.IO.Path]::GetFileNameWithoutExtension($path))
        }
        else {
            $dependencies.External.Add($path)
        }
    }
    
    # Extract dot-sourced files
    $dotSourceMatches = [regex]::Matches($Content, "^\s*\.\s+([`"`'])([^`"`'\s]+)\\1")
    foreach ($match in $dotSourceMatches) {
        $path = $match.Groups[2].Value
        $dependencies.Internal.Add([System.IO.Path]::GetFileNameWithoutExtension($path))
    }
    
    # Extract external commands
    $externalCommandMatches = [regex]::Matches($Content, '^\s*(\w+)\s+[^|]*\|')
    foreach ($match in $externalCommandMatches) {
        $cmd = $match.Groups[1].Value
        if ($cmd -notin @('if', 'else', 'foreach', 'for', 'while', 'do', 'switch', 'try', 'catch', 'finally')) {
            $dependencies.External.Add($cmd)
        }
    }
    
    return $dependencies
}

# Python dependency extraction
function Extract-PythonDependencies {
    param($Content)
    
    $dependencies = @{
        Internal = [List[string]]::new()
        External = [List[string]]::new()
    }
    
    # Extract import statements
    $importMatches = [regex]::Matches($Content, '^\s*import\s+(\w+)')
    foreach ($match in $importMatches) {
        $module = $match.Groups[1].Value
        $dependencies.External.Add($module)
    }
    
    # Extract from imports
    $fromMatches = [regex]::Matches($Content, '^\s*from\s+(\w+)\s+import')
    foreach ($match in $fromMatches) {
        $module = $match.Groups[1].Value
        if ($module -like ".*") {
            $dependencies.Internal.Add($module.TrimStart('.'))
        }
        else {
            $dependencies.External.Add($module)
        }
    }
    
    # Extract relative imports
    $relativeMatches = [regex]::Matches($Content, '^\s*from\s+(\.+)\s+import')
    foreach ($match in $relativeMatches) {
        $relativePath = $match.Groups[1].Value
        $dependencies.Internal.Add($relativePath)
    }
    
    return $dependencies
}

# JavaScript dependency extraction
function Extract-JavaScriptDependencies {
    param($Content)
    
    $dependencies = @{
        Internal = [List[string]]::new()
        External = [List[string]]::new()
    }
    
    # Extract require statements (CommonJS)
    $requireMatches = [regex]::Matches($Content, "require\s*\\(([`"`'])([^`"`']+)\\1\\)")
    foreach ($match in $requireMatches) {
        $module = $match.Groups[2].Value
        if ($module.StartsWith('.') -or $module.StartsWith('/')) {
            $dependencies.Internal.Add($module)
        }
        else {
            $dependencies.External.Add($module)
        }
    }
    
    # Extract import statements (ES6)
    $importMatches = [regex]::Matches($Content, 'import\s+(?:\*\s+as\s+\w+|\w+)\s+from\s+["\']([^"\']+)["\']')
    foreach ($match in $importMatches) {
        $module = $match.Groups[1].Value
        if ($module.StartsWith('.') -or $module.StartsWith('/')) {
            $dependencies.Internal.Add($module)
        }
        else {
            $dependencies.External.Add($module)
        }
    }
    
    # Extract default imports
    $defaultImportMatches = [regex]::Matches($Content, 'import\s+["\']([^"\']+)["\']')
    foreach ($match in $defaultImportMatches) {
        $module = $match.Groups[1].Value
        if ($module.StartsWith('.') -or $module.StartsWith('/')) {
            $dependencies.Internal.Add($module)
        }
        else {
            $dependencies.External.Add($module)
        }
    }
    
    return $dependencies
}

# Circular dependency detection
function Detect-CircularDependencies {
    param($Graph)
    
    $circularDeps = [List[hashtable]]::new()
    $visited = [System.Collections.Generic.HashSet[string]]::new()
    $recursionStack = [System.Collections.Generic.HashSet[string]]::new()
    
    function DFS {
        param($nodeId, $path)
        
        if ($recursionStack.Contains($nodeId)) {
            # Found a circular dependency
            $cycleStart = $path.IndexOf($nodeId)
            $cycle = $path[$cycleStart..($path.Count - 1)]
            
            $circularDeps.Add(@{
                Cycle = $cycle
                Length = $cycle.Count
                Nodes = $cycle
            })
            return
        }
        
        if ($visited.Contains($nodeId)) {
            return
        }
        
        $visited.Add($nodeId)
        $recursionStack.Add($nodeId)
        $path.Add($nodeId)
        
        # Find edges from this node
        $edges = $Graph.Edges | Where-Object { $_.From -eq $nodeId }
        
        foreach ($edge in $edges) {
            DFS -nodeId $edge.To -path $path
        }
        
        $recursionStack.Remove($nodeId)
        $path.RemoveAt($path.Count - 1)
    }
    
    foreach ($node in $Graph.Nodes) {
        if (!$visited.Contains($node.Id)) {
            DFS -nodeId $node.Id -path ([System.Collections.Generic.List[string]]::new())
        }
    }
    
    return $circularDeps
}

# Vulnerability detection function
function Start-VulnerabilityDetection {
    param($TargetFiles)
    
    Write-Log "INFO" "Starting vulnerability detection for $($TargetFiles.Count) files"
    
    $vulnerabilityDatabase = @{
        PowerShell = @{
            "Invoke-Expression" = @{
                Severity = "High"
                Category = "CodeInjection"
                Description = "Potential code injection vulnerability"
                Remediation = "Use safer alternatives or validate input"
            }
            "Start-Process -FilePath" = @{
                Severity = "Medium"
                Category = "CommandInjection"
                Description = "Potential command injection if input is not validated"
                Remediation = "Validate and sanitize all user input"
            }
            "ConvertTo-SecureString -AsPlainText" = @{
                Severity = "High"
                Category = "HardcodedSecrets"
                Description = "Hardcoded password detected"
                Remediation = "Use secure credential storage"
            }
        }
        Python = @{
            "eval(" = @{
                Severity = "High"
                Category = "CodeInjection"
                Description = "Potential code injection vulnerability"
                Remediation = "Use ast.literal_eval() for safe evaluation"
            }
            "exec(" = @{
                Severity = "High"
                Category = "CodeInjection"
                Description = "Potential code injection vulnerability"
                Remediation = "Avoid using exec() with user input"
            }
            "subprocess.call" = @{
                Severity = "Medium"
                Category = "CommandInjection"
                Description = "Potential command injection"
                Remediation = "Use subprocess.run() with shell=False"
            }
            "pickle.loads" = @{
                Severity = "High"
                Category = "Deserialization"
                Description = "Insecure deserialization vulnerability"
                Remediation = "Use safer serialization formats like JSON"
            }
            "yaml.load" = @{
                Severity = "High"
                Category = "Deserialization"
                Description = "Insecure YAML deserialization"
                Remediation = "Use yaml.safe_load() instead"
            }
        }
        JavaScript = @{
            "eval(" = @{
                Severity = "High"
                Category = "CodeInjection"
                Description = "Potential code injection vulnerability"
                Remediation = "Avoid using eval() entirely"
            }
            "innerHTML = " = @{
                Severity = "Medium"
                Category = "XSS"
                Description = "Potential XSS vulnerability"
                Remediation = "Use textContent or createTextNode()"
            }
            "document.write(" = @{
                Severity = "Medium"
                Category = "XSS"
                Description = "Potential XSS vulnerability"
                Remediation = "Use DOM manipulation methods"
            }
        }
        Cpp = @{
            "strcpy(" = @{
                Severity = "High"
                Category = "BufferOverflow"
                Description = "Potential buffer overflow vulnerability"
                Remediation = "Use strncpy() or safer alternatives"
            }
            "sprintf(" = @{
                Severity = "High"
                Category = "BufferOverflow"
                Description = "Potential buffer overflow vulnerability"
                Remediation = "Use snprintf() instead"
            }
            "gets(" = @{
                Severity = "Critical"
                Category = "BufferOverflow"
                Description = "Dangerous function - always causes buffer overflow"
                Remediation = "Use fgets() instead"
            }
        }
    }
    
    foreach ($file in $TargetFiles) {
        if ($file.Size -gt 10MB) { continue }  # Skip large files
        
        try {
            $content = Get-Content $file.Path -Raw -ErrorAction Stop
            
            $fileVulnerabilities = @{
                File = $file
                Issues = [List[hashtable]]::new()
                RiskScore = 0
                CriticalCount = 0
                HighCount = 0
                MediumCount = 0
                LowCount = 0
            }
            
            $languageVulns = $vulnerabilityDatabase[$file.Type]
            if ($languageVulns) {
                foreach ($vulnPattern in $languageVulns.Keys) {
                    $matches = [regex]::Matches($content, [regex]::Escape($vulnPattern))
                    
                    foreach ($match in $matches) {
                        $vulnInfo = $languageVulns[$vulnPattern]
                        
                        $issue = @{
                            Pattern = $vulnPattern
                            Severity = $vulnInfo.Severity
                            Category = $vulnInfo.Category
                            Description = $vulnInfo.Description
                            Remediation = $vulnInfo.Remediation
                            Line = $match.Index
                            RiskScore = Get-RiskScore -Severity $vulnInfo.Severity
                        }
                        
                        $fileVulnerabilities.Issues.Add($issue)
                        $fileVulnerabilities.RiskScore += $issue.RiskScore
                        
                        # Count by severity
                        switch ($vulnInfo.Severity) {
                            "Critical" { $fileVulnerabilities.CriticalCount++ }
                            "High" { $fileVulnerabilities.HighCount++ }
                            "Medium" { $fileVulnerabilities.MediumCount++ }
                            "Low" { $fileVulnerabilities.LowCount++ }
                        }
                    }
                }
            }
            
            if ($fileVulnerabilities.Issues.Count -gt 0) {
                $global:ReverseEngineeringResults.Vulnerabilities.Add($fileVulnerabilities)
                $global:ReverseEngineeringResults.Statistics.TotalVulnerabilities += $fileVulnerabilities.Issues.Count
            }
            
            $file.AnalysisStatus = "Completed"
        }
        catch {
            Write-Log "WARNING" "Failed to scan vulnerabilities in $($file.Name): $_"
            $file.AnalysisStatus = "Failed"
        }
    }
    
    Write-Log "SUCCESS" "Vulnerability detection completed: $($global:ReverseEngineeringResults.Statistics.TotalVulnerabilities) issues found"
    
    return $global:ReverseEngineeringResults.Vulnerabilities
}

# Risk score calculation
function Get-RiskScore {
    param($Severity)
    
    switch ($Severity) {
        "Critical" { return 10 }
        "High" { return 7 }
        "Medium" { return 4 }
        "Low" { return 1 }
        default { return 0 }
    }
}

# Performance analysis function
function Start-PerformanceAnalysis {
    param($TargetFiles)
    
    Write-Log "INFO" "Starting performance analysis for $($TargetFiles.Count) files"
    
    foreach ($file in $TargetFiles) {
        if ($file.Size -gt 10MB) { continue }  # Skip large files
        
        try {
            $content = Get-Content $file.Path -Raw -ErrorAction Stop
            $lines = $content -split "`n"
            
            $performanceAnalysis = @{
                File = $file
                Bottlenecks = [List[hashtable]]::new()
                ComplexityMetrics = @{
                    CyclomaticComplexity = 0
                    CognitiveComplexity = 0
                    LinesOfCode = $lines.Count
                    CodeLines = 0
                    CommentLines = 0
                    BlankLines = 0
                }
                ResourceUsage = @{
                    MemoryOperations = 0
                    FileOperations = 0
                    NetworkOperations = 0
                    DatabaseOperations = 0
                }
                BlockingOperations = [List[hashtable]]::new()
                Recommendations = [List[string]]::new()
            }
            
            # Analyze complexity
            $performanceAnalysis.ComplexityMetrics = Analyze-CodeComplexity -Content $content -Language $file.Type
            
            # Identify resource-intensive operations
            $performanceAnalysis.ResourceUsage = Identify-ResourceOperations -Content $content -Language $file.Type
            
            # Detect blocking operations
            $performanceAnalysis.BlockingOperations = Detect-BlockingOperations -Content $content -Language $file.Type
            
            # Generate recommendations
            $performanceAnalysis.Recommendations = Generate-PerformanceRecommendations -Analysis $performanceAnalysis
            
            if ($performanceAnalysis.Bottlenecks.Count -gt 0 -or $performanceAnalysis.BlockingOperations.Count -gt 0) {
                $global:ReverseEngineeringResults.Bottlenecks.Add($performanceAnalysis)
                $global:ReverseEngineeringResults.Statistics.TotalBottlenecks += $performanceAnalysis.Bottlenecks.Count
            }
            
            $file.AnalysisStatus = "Completed"
        }
        catch {
            Write-Log "WARNING" "Failed to analyze performance of $($file.Name): $_"
            $file.AnalysisStatus = "Failed"
        }
    }
    
    Write-Log "SUCCESS" "Performance analysis completed: $($global:ReverseEngineeringResults.Statistics.TotalBottlenecks) bottlenecks identified"
    
    return $global:ReverseEngineeringResults.Bottlenecks
}

# Code complexity analysis
function Analyze-CodeComplexity {
    param($Content, $Language)
    
    $metrics = @{
        CyclomaticComplexity = 1  # Base complexity
        CognitiveComplexity = 0
        CodeLines = 0
        CommentLines = 0
        BlankLines = 0
    }
    
    $lines = $content -split "`n"
    
    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        
        if ([string]::IsNullOrEmpty($trimmed)) {
            $metrics.BlankLines++
        }
        elseif ($trimmed.StartsWith('#') -or $trimmed.StartsWith('//') -or $trimmed.StartsWith('/*')) {
            $metrics.CommentLines++
        }
        else {
            $metrics.CodeLines++
        }
    }
    
    # Language-specific complexity patterns
    $complexityPatterns = @{
        PowerShell = @{
            'if\s*\(            "\\\\|\\\\|" = 1
            "elseif\s*\(" = 1
            "while\s*\(" = 1
            "for\s*\(" = 1
            "foreach\s*\(" = 1
            "switch\s*\(" = 1
            "\\|\\|" = 1
            "\\&\\&" = 1
        }
            "\bif\s" = 1
            "\belif\s" = 1
            "\bwhile\s" = 1
            "\bfor\s" = 1
            "\bor\b" = 1
            "\band\b" = 1
            "\band\b" = 1
        }
        JavaScript = @{
            "\bif\s*\(" = 1
            "\belse\s*if\s*\(" = 1
            "\bwhile\s*\(" = 1
            "\bfor\s*\(" = 1
            "\bfor\s*\(" = 1
            "\|\|" = 1
            "\u0026\u0026" = 1
            "\?" = 1  # Ternary operator
    }
    
    $patterns = $complexityPatterns[$Language]
    if ($patterns) {
        foreach ($pattern in $patterns.Keys) {
            $matches = [regex]::Matches($Content, $pattern)
            $metrics.CyclomaticComplexity += $matches.Count * $patterns[$pattern]
        }
    }
    
    # Calculate cognitive complexity (simplified)
    $metrics.CognitiveComplexity = $metrics.CyclomaticComplexity + [Math]::Floor($metrics.CodeLines / 50)
    
    return $metrics
}

# Resource operation identification
function Identify-ResourceOperations {
    param($Content, $Language)
    
    $resourceUsage = @{
        MemoryOperations = 0
        FileOperations = 0
        NetworkOperations = 0
        DatabaseOperations = 0
    }
    
    # Language-specific resource patterns
    $resourcePatterns = @{
        PowerShell = @{
            Memory = @('New-Object', 'Clear-Variable', 'Remove-Variable')
            File = @('Get-Content', 'Set-Content', 'Add-Content', 'Import-Csv', 'Export-Csv')
            Network = @('Invoke-RestMethod', 'Invoke-WebRequest', 'Test-Connection')
            Database = @('Invoke-Sqlcmd', 'sqlcmd')
        }
        Python = @{
            Memory = @('list(', 'dict(', 'set(', 'object(')
            File = @('open(', 'read(', 'write(', 'os.listdir', 'os.walk')
            Network = @('requests.', 'urllib.', 'http.client', 'socket.')
            Database = @('sqlite3.', 'psycopg2.', 'mysql.', 'pymongo.')
        }
        JavaScript = @{
            Memory = @('new Array', 'new Object', 'JSON.parse', 'JSON.stringify')
            File = @('fs.readFile', 'fs.writeFile', 'fs.createReadStream')
            Network = @('fetch(', 'XMLHttpRequest', 'http.', 'https.')
            Database = @('mongodb.', 'mysql.', 'pg.', 'sqlite3.')
        }
    }
    
    $patterns = $resourcePatterns[$Language]
    if ($patterns) {
        foreach ($category in $patterns.Keys) {
            foreach ($pattern in $patterns[$category]) {
                $matches = [regex]::Matches($Content, [regex]::Escape($pattern))
                switch ($category) {
                    "Memory" { $resourceUsage.MemoryOperations += $matches.Count }
                    "File" { $resourceUsage.FileOperations += $matches.Count }
                    "Network" { $resourceUsage.NetworkOperations += $matches.Count }
                    "Database" { $resourceUsage.DatabaseOperations += $matches.Count }
                }
            }
        }
    }
    
    return $resourceUsage
}

# Blocking operation detection
function Detect-BlockingOperations {
    param($Content, $Language)
    
    $blockingOps = [List[hashtable]]::new()
    
    # Language-specific blocking patterns
    $blockingPatterns = @{
        PowerShell = @{
            'Start-Sleep' = "Sleep"
            'Wait-Process' = "ProcessWait"
            'Read-Host' = "UserInput"
            'pause' = "Pause"
        }
        Python = @{
            'time.sleep' = "Sleep"
            'input(' = "UserInput"
            'os.system' = "SystemCall"
            'subprocess.call' = "Subprocess"
        }
        JavaScript = @{
            'while\s*\(true\)' = "InfiniteLoop"
            'for\s*\(;;\)' = "InfiniteLoop"
            'prompt(' = "UserInput"
            'alert(' = "BlockingAlert"
        }
    }
    
    $patterns = $blockingPatterns[$Language]
    if ($patterns) {
        foreach ($pattern in $patterns.Keys) {
            $matches = [regex]::Matches($Content, $pattern)
            foreach ($match in $matches) {
                $blockingOps.Add(@{
                    Pattern = $pattern
                    Type = $patterns[$pattern]
                    Line = $match.Index
                    Severity = "Medium"
                    Recommendation = "Consider async alternative or timeout mechanism"
                })
            }
        }
    }
    
    return $blockingOps
}

# Performance recommendation generation
function Generate-PerformanceRecommendations {
    param($Analysis)
    
    $recommendations = [List[string]]::new()
    
    # Complexity recommendations
    if ($Analysis.ComplexityMetrics.CyclomaticComplexity -gt 20) {
        $recommendations.Add("High cyclomatic complexity ($($Analysis.ComplexityMetrics.CyclomaticComplexity)). Consider refactoring into smaller functions.")
    }
    
    if ($Analysis.ComplexityMetrics.CognitiveComplexity -gt 30) {
        $recommendations.Add("High cognitive complexity. Simplify logic and reduce nesting.")
    }
    
    # Resource usage recommendations
    if ($Analysis.ResourceUsage.MemoryOperations -gt 100) {
        $recommendations.Add("High number of memory operations. Consider object pooling or caching.")
    }
    
    if ($Analysis.ResourceUsage.FileOperations -gt 50) {
        $recommendations.Add("Frequent file operations. Consider batching or async I/O.")
    }
    
    if ($Analysis.ResourceUsage.NetworkOperations -gt 20) {
        $recommendations.Add("Multiple network operations. Consider connection pooling or caching.")
    }
    
    if ($Analysis.ResourceUsage.DatabaseOperations -gt 30) {
        $recommendations.Add("Frequent database operations. Consider query optimization or caching.")
    }
    
    # Blocking operation recommendations
    if ($Analysis.BlockingOperations.Count -gt 5) {
        $recommendations.Add("Multiple blocking operations detected. Consider async/await pattern.")
    }
    
    return $recommendations
}

# Save reverse engineering results
function Save-ReverseEngineeringResults {
    param($OutputPath)
    
    $resultsFile = Join-Path $OutputPath "reverse_engineering_results.json"
    
    try {
        $global:ReverseEngineeringResults | ConvertTo-Json -Depth 20 | Out-File $resultsFile -Encoding UTF8
        Write-Log "INFO" "Reverse engineering results saved to $resultsFile"
    }
    catch {
        Write-Log "ERROR" "Failed to save reverse engineering results: $_"
    }
}

# Interactive mode
function Start-InteractiveMode {
    Write-Host "`n=== Reverse Engineering Engine - Interactive Mode ===" -ForegroundColor Cyan
    Write-Host "Version: $($global:ReverseEngineeringConfig.Version)" -ForegroundColor Gray
    
    while ($true) {
        Write-Host "`nOptions:" -ForegroundColor Yellow
        Write-Host "1. Start new reverse engineering analysis"
        Write-Host "2. Load existing results"
        Write-Host "3. Configure analysis settings"
        Write-Host "4. View analysis statistics"
        Write-Host "5. Export detailed report"
        Write-Host "6. Exit"
        
        $choice = Read-Host "`nSelect option (1-6)"
        
        switch ($choice) {
            "1" {
                $target = Read-Host "Enter target path"
                $output = Read-Host "Enter output path (press Enter for default)"
                if ([string]::IsNullOrEmpty($output)) { $output = ".\reverse_engineering_output" }
                
                $analyzeBinaries = (Read-Host "Analyze binaries? (y/n)").ToLower() -eq 'y'
                $extractAPIs = (Read-Host "Extract APIs? (y/n)").ToLower() -eq 'y'
                $mapDependencies = (Read-Host "Map dependencies? (y/n)").ToLower() -eq 'y'
                $detectVulns = (Read-Host "Detect vulnerabilities? (y/n)").ToLower() -eq 'y'
                $analyzePerformance = (Read-Host "Analyze performance? (y/n)").ToLower() -eq 'y'
                
                Start-ReverseEngineering -TargetPath $target -OutputPath $output
            }
            "2" {
                $resultsPath = Read-Host "Enter results file path"
                Load-ReverseEngineeringResults -Path $resultsPath
            }
            "3" {
                Show-ConfigurationMenu
            }
            "4" {
                Show-Statistics
            }
            "5" {
                $format = Read-Host "Report format (HTML/JSON/Markdown)"
                $path = Read-Host "Output path"
                Generate-DetailedReport -OutputPath $path -Format $format
            }
            "6" {
                Write-Host "Exiting..." -ForegroundColor Yellow
                return
            }
            default {
                Write-Host "Invalid option. Please try again." -ForegroundColor Red
            }
        }
    }
}

# Main execution
if ($Interactive) {
    Start-InteractiveMode
}
elseif ($TargetPath) {
    Start-ReverseEngineering -TargetPath $TargetPath -OutputPath $OutputPath
}
else {
    Write-Host "Reverse Engineering Engine v$($global:ReverseEngineeringConfig.Version)" -ForegroundColor Cyan
    Write-Host "Use -Interactive for interactive mode or specify -TargetPath" -ForegroundColor Yellow
    Write-Host "Example: .\ReverseEngineeringEngine.ps1 -TargetPath 'C:\project' -AnalyzeBinaries -ExtractAPIs -DetectVulnerabilities" -ForegroundColor Gray
}
