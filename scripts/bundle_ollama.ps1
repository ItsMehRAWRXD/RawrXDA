# =============================================================================
# Ollama Binary Bundling System for RawrXD IDE
# =============================================================================
# This script handles downloading, verifying, and bundling Ollama binaries
# with the RawrXD IDE distribution for seamless embedded service operation.

param(
    [string]$BuildDir = "d:\rawrxd\build",
    [string]$BundleDir = "$BuildDir\bundle",
    [string]$OllamaVersion = "latest",
    [switch]$Force,
    [switch]$Verify,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# Configuration
$OllamaConfig = @{
    BaseUrl = "https://github.com/ollama/ollama/releases"
    WindowsAsset = "ollama-windows-amd64.exe"
    BundlePath = "$BundleDir\bin\ollama.exe"
    ModelsPath = "$BundleDir\models"
    ConfigPath = "$BundleDir\config\ollama"
    HashAlgorithm = "SHA256"
    MinSizeBytes = 50MB  # Minimum expected file size
}

function Write-BuildLog {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $color = switch ($Level) {
        "ERROR" { "Red" }
        "WARN" { "Yellow" }
        "SUCCESS" { "Green" }
        default { "White" }
    }
    Write-Host "[$timestamp] [$Level] $Message" -ForegroundColor $color
}

function Get-OllamaLatestVersion {
    try {
        Write-BuildLog "Fetching latest Ollama version info..."
        $apiUrl = "https://api.github.com/repos/ollama/ollama/releases/latest"
        $response = Invoke-RestMethod -Uri $apiUrl -UserAgent "RawrXD-Builder/1.0"
        return $response.tag_name
    }
    catch {
        Write-BuildLog "Failed to fetch version info: $($_.Exception.Message)" "ERROR"
        throw "Unable to determine latest Ollama version"
    }
}

function Get-OllamaDownloadUrl {
    param([string]$Version)
    
    if ($Version -eq "latest") {
        $Version = Get-OllamaLatestVersion
    }
    
    $downloadUrl = "$($OllamaConfig.BaseUrl)/download/$Version/$($OllamaConfig.WindowsAsset)"
    Write-BuildLog "Download URL: $downloadUrl"
    return $downloadUrl
}

function Test-OllamaBinaryIntegrity {
    param([string]$BinaryPath)
    
    if (-not (Test-Path $BinaryPath)) {
        return $false
    }
    
    $fileInfo = Get-Item $BinaryPath
    if ($fileInfo.Length -lt $OllamaConfig.MinSizeBytes) {
        Write-BuildLog "Binary file too small: $($fileInfo.Length) bytes" "WARN"
        return $false
    }
    
    # Test if the binary is executable
    try {
        $processInfo = & $BinaryPath --version 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-BuildLog "Binary integrity verified: $processInfo" "SUCCESS"
            return $true
        }
    }
    catch {
        Write-BuildLog "Binary failed execution test: $($_.Exception.Message)" "WARN"
    }
    
    return $false
}

function Download-OllamaBinary {
    param([string]$DownloadUrl, [string]$OutputPath)
    
    $tempPath = "$OutputPath.tmp"
    $parentDir = Split-Path -Path $OutputPath -Parent
    
    # Ensure directory exists
    if (-not (Test-Path $parentDir)) {
        New-Item -Path $parentDir -ItemType Directory -Force | Out-Null
        Write-BuildLog "Created directory: $parentDir"
    }
    
    try {
        Write-BuildLog "Downloading Ollama binary..."
        Write-BuildLog "From: $DownloadUrl"
        Write-BuildLog "To: $OutputPath"
        
        # Use WebClient with progress tracking
        $webClient = New-Object System.Net.WebClient
        $webClient.Headers.Add("User-Agent", "RawrXD-Builder/1.0")
        
        # Progress tracking
        Register-ObjectEvent -InputObject $webClient -EventName DownloadProgressChanged -Action {
            $percent = $Event.SourceEventArgs.ProgressPercentage
            $received = [math]::Round($Event.SourceEventArgs.BytesReceived / 1MB, 2)
            $total = [math]::Round($Event.SourceEventArgs.TotalBytesToReceive / 1MB, 2)
            Write-Progress -Activity "Downloading Ollama" -Status "$percent% Complete" -PercentComplete $percent -CurrentOperation "$received MB / $total MB"
        }
        
        $webClient.DownloadFile($DownloadUrl, $tempPath)
        $webClient.Dispose()
        
        Write-Progress -Activity "Downloading Ollama" -Completed
        Write-BuildLog "Download completed successfully"
        
        # Verify downloaded file
        if ((Get-Item $tempPath).Length -lt $OllamaConfig.MinSizeBytes) {
            throw "Downloaded file is too small (possible download failure)"
        }
        
        # Move to final location
        Move-Item -Path $tempPath -Destination $OutputPath -Force
        Write-BuildLog "Binary saved to: $OutputPath" "SUCCESS"
        
        return $true
    }
    catch {
        Write-BuildLog "Download failed: $($_.Exception.Message)" "ERROR"
        if (Test-Path $tempPath) {
            Remove-Item $tempPath -Force
        }
        return $false
    }
}

function Initialize-OllamaBundleStructure {
    Write-BuildLog "Initializing Ollama bundle directory structure..."
    
    # Create necessary directories
    $directories = @(
        $BundleDir,
        (Split-Path $OllamaConfig.BundlePath -Parent),
        $OllamaConfig.ModelsPath,
        $OllamaConfig.ConfigPath
    )
    
    foreach ($dir in $directories) {
        if (-not (Test-Path $dir)) {
            New-Item -Path $dir -ItemType Directory -Force | Out-Null
            Write-BuildLog "Created directory: $dir"
        }
    }
    
    # Create bundle metadata
    $bundleInfo = @{
        BundledAt = Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ"
        OllamaVersion = $OllamaVersion
        BundledBy = "RawrXD-Builder"
        Architecture = "windows-amd64"
        MinRawrXDVersion = "6.0"
    }
    
    $bundleInfo | ConvertTo-Json -Depth 3 | Out-File -FilePath "$BundleDir\ollama-bundle.json" -Encoding UTF8
    
    # Create default Ollama configuration
    $ollamaConfig = @{
        host = "127.0.0.1"
        port = 11434
        models_path = ".\models"
        keep_alive = "5m"
        log_level = "info"
        origins = @("http://localhost:*", "http://127.0.0.1:*")
    }
    
    $ollamaConfig | ConvertTo-Json -Depth 3 | Out-File -FilePath "$($OllamaConfig.ConfigPath)\config.json" -Encoding UTF8
    Write-BuildLog "Created Ollama configuration file"
}

function Test-ExistingBundle {
    if (-not (Test-Path $OllamaConfig.BundlePath)) {
        Write-BuildLog "No existing Ollama bundle found"
        return $false
    }
    
    if (Test-OllamaBinaryIntegrity -BinaryPath $OllamaConfig.BundlePath) {
        $bundleInfo = Get-Item $OllamaConfig.BundlePath
        Write-BuildLog "Found valid Ollama bundle: $($bundleInfo.Length) bytes, modified $($bundleInfo.LastWriteTime)" "SUCCESS"
        
        if (-not $Force) {
            Write-BuildLog "Use -Force to re-download existing bundle"
            return $true
        }
    }
    
    Write-BuildLog "Existing bundle found but invalid or force specified"
    return $false
}

function Clear-Bundle {
    if (Test-Path $BundleDir) {
        Write-BuildLog "Cleaning existing bundle directory..."
        Remove-Item -Path $BundleDir -Recurse -Force
        Write-BuildLog "Bundle directory cleaned" "SUCCESS"
    }
}

function Get-BundleSize {
    if (Test-Path $BundleDir) {
        $size = (Get-ChildItem -Path $BundleDir -Recurse | Measure-Object -Property Length -Sum).Sum
        return [math]::Round($size / 1MB, 2)
    }
    return 0
}

function Test-Prerequisites {
    Write-BuildLog "Checking prerequisites..."
    
    # Check internet connectivity
    try {
        Test-Connection -ComputerName "github.com" -Count 1 -Quiet
        Write-BuildLog "Internet connectivity: OK" "SUCCESS"
    }
    catch {
        Write-BuildLog "Internet connectivity required for download" "ERROR"
        return $false
    }
    
    # Check disk space (require at least 500MB free)
    $buildDisk = [System.IO.DriveInfo]::GetDrives() | Where-Object { $BuildDir.StartsWith($_.Name) }
    if ($buildDisk -and ($buildDisk.AvailableFreeSpace / 1GB) -lt 0.5) {
        Write-BuildLog "Insufficient disk space (need 500MB, have $([math]::Round($buildDisk.AvailableFreeSpace / 1GB, 2))GB)" "ERROR"
        return $false
    }
    
    Write-BuildLog "Prerequisites check passed" "SUCCESS"
    return $true
}

function Show-BundleInfo {
    Write-BuildLog "Ollama Bundle Information" "SUCCESS"
    Write-BuildLog "========================"
    Write-BuildLog "Bundle Directory: $BundleDir"
    Write-BuildLog "Ollama Binary: $($OllamaConfig.BundlePath)"
    Write-BuildLog "Models Directory: $($OllamaConfig.ModelsPath)"
    Write-BuildLog "Config Directory: $($OllamaConfig.ConfigPath)"
    
    if (Test-Path $OllamaConfig.BundlePath) {
        $binaryInfo = Get-Item $OllamaConfig.BundlePath
        Write-BuildLog "Binary Size: $([math]::Round($binaryInfo.Length / 1MB, 2)) MB"
        Write-BuildLog "Last Modified: $($binaryInfo.LastWriteTime)"
        
        # Get version if possible
        try {
            $version = & $OllamaConfig.BundlePath --version 2>$null
            Write-BuildLog "Version: $version"
        }
        catch {
            Write-BuildLog "Version: Unable to determine"
        }
    }
    
    $bundleSize = Get-BundleSize
    Write-BuildLog "Total Bundle Size: $bundleSize MB"
    
    if (Test-Path "$BundleDir\ollama-bundle.json") {
        $metadata = Get-Content "$BundleDir\ollama-bundle.json" | ConvertFrom-Json
        Write-BuildLog "Bundled At: $($metadata.BundledAt)"
        Write-BuildLog "Target Version: $($metadata.OllamaVersion)"
    }
}

# =============================================================================
# MAIN EXECUTION
# =============================================================================

try {
    Write-BuildLog "Starting Ollama binary bundling process..." "SUCCESS"
    Write-BuildLog "Target version: $OllamaVersion"
    Write-BuildLog "Bundle directory: $BundleDir"
    
    # Handle clean operation
    if ($Clean) {
        Clear-Bundle
        Write-BuildLog "Bundle cleaning completed" "SUCCESS"
        exit 0
    }
    
    # Handle verify operation
    if ($Verify) {
        Show-BundleInfo
        $isValid = Test-OllamaBinaryIntegrity -BinaryPath $OllamaConfig.BundlePath
        Write-BuildLog "Bundle verification: $(if ($isValid) { 'PASSED' } else { 'FAILED' })" $(if ($isValid) { 'SUCCESS' } else { 'ERROR' })
        exit $(if ($isValid) { 0 } else { 1 })
    }
    
    # Check prerequisites
    if (-not (Test-Prerequisites)) {
        Write-BuildLog "Prerequisites check failed" "ERROR"
        exit 1
    }
    
    # Check existing bundle
    if (Test-ExistingBundle) {
        Show-BundleInfo
        Write-BuildLog "Ollama bundle is already up-to-date" "SUCCESS"
        exit 0
    }
    
    # Initialize bundle structure
    Initialize-OllamaBundleStructure
    
    # Download Ollama binary
    $downloadUrl = Get-OllamaDownloadUrl -Version $OllamaVersion
    $downloadSuccess = Download-OllamaBinary -DownloadUrl $downloadUrl -OutputPath $OllamaConfig.BundlePath
    
    if (-not $downloadSuccess) {
        Write-BuildLog "Failed to download Ollama binary" "ERROR"
        exit 1
    }
    
    # Verify downloaded binary
    if (Test-OllamaBinaryIntegrity -BinaryPath $OllamaConfig.BundlePath) {
        Write-BuildLog "Ollama binary bundling completed successfully!" "SUCCESS"
        Show-BundleInfo
        exit 0
    } else {
        Write-BuildLog "Bundle verification failed after download" "ERROR"
        exit 1
    }
    
} catch {
    Write-BuildLog "Bundling process failed: $($_.Exception.Message)" "ERROR"
    Write-BuildLog "Stack trace: $($_.ScriptStackTrace)" "ERROR"
    exit 1
}