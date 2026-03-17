# Download llama.cpp quantizer for Windows (standalone binary)
# This script fetches a prebuilt quantize.exe from a llama.cpp release and places it in Powershield\tools.

[CmdletBinding()] param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$toolsDir = Join-Path $PSScriptRoot 'tools'
if (-not (Test-Path $toolsDir)) { New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null }

$quantizeExe = Join-Path $toolsDir 'quantize.exe'

# Check if already downloaded
if (Test-Path $quantizeExe -PathType Leaf) {
    Write-Host "✅ quantize.exe already exists: $quantizeExe" -ForegroundColor Green
    exit 0
}

# We'll fetch a recent llama.cpp Windows release with quantize.exe
# Example: llama.cpp releases often bundle a quantize.exe in win-x64 builds
# You can customize this URL to point to a specific llama.cpp release asset or a trusted mirror.

$releaseUrl = "https://github.com/ggerganov/llama.cpp/releases/latest"
Write-Host "Fetching latest llama.cpp release info..." -ForegroundColor Cyan

try {
    # Get redirect to latest release page
    $response = Invoke-WebRequest -Uri $releaseUrl -MaximumRedirection 0 -ErrorAction SilentlyContinue
    $latestUrl = $response.Headers.Location
    if (-not $latestUrl) { throw "Could not determine latest release URL." }
    
    Write-Host "Latest release: $latestUrl" -ForegroundColor Cyan
    
    # Fetch the release page to find asset download links
    $releasePage = Invoke-WebRequest -Uri $latestUrl -UseBasicParsing
    
    # Look for a Windows binary asset (common patterns: llama-*-win-x64.zip, llama.cpp-*-windows.zip, etc.)
    # Adjust the filter pattern based on actual llama.cpp release naming
    $assetPattern = 'llama.*win.*x64.*\.zip|llama.*windows.*\.zip|llama-cpp.*win.*\.zip'
    $assetLinks = $releasePage.Links | Where-Object { $_.href -match $assetPattern } | Select-Object -First 1
    
    if (-not $assetLinks) {
        throw "No suitable Windows x64 asset found in latest release. Manual download required."
    }
    
    $assetUrl = $assetLinks.href
    if ($assetUrl -notmatch '^https?://') {
        # Relative URL, prepend GitHub domain
        $assetUrl = "https://github.com$assetUrl"
    }
    
    Write-Host "Downloading: $assetUrl" -ForegroundColor Cyan
    $zipPath = Join-Path $toolsDir 'llama-cpp-windows.zip'
    Invoke-WebRequest -Uri $assetUrl -OutFile $zipPath -UseBasicParsing
    
    Write-Host "Extracting..." -ForegroundColor Cyan
    Expand-Archive -Path $zipPath -DestinationPath $toolsDir -Force
    
    # Find quantize.exe in extracted contents
    $extracted = Get-ChildItem -Path $toolsDir -Recurse -Filter 'quantize.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $extracted) {
        throw "quantize.exe not found in downloaded archive. Manual extraction required."
    }
    
    # Move to tools root
    Move-Item -Path $extracted.FullName -Destination $quantizeExe -Force
    Write-Host "✅ quantize.exe installed: $quantizeExe" -ForegroundColor Green
    
    # Cleanup
    Remove-Item -Path $zipPath -Force -ErrorAction SilentlyContinue
}
catch {
    Write-Host "⚠️ Automated download failed: $($_.Exception.Message)" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Manual steps:" -ForegroundColor Cyan
    Write-Host "1. Download llama.cpp Windows binaries from: https://github.com/ggerganov/llama.cpp/releases"
    Write-Host "2. Extract and locate quantize.exe"
    Write-Host "3. Copy it to: $quantizeExe"
    Write-Host "4. Re-run Downsize-Model.ps1 with -QuantizeExe '$quantizeExe'"
    exit 1
}
