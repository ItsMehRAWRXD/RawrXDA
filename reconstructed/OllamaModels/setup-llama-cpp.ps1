$ErrorActionPreference = "Stop"
$LlamaCppDir = "D:\OllamaModels\llama.cpp"

# Download llama.cpp
Write-Host "Fetching latest release info..."
$apiUrl = "https://api.github.com/repos/ggerganov/llama.cpp/releases/latest"
$release = Invoke-RestMethod -Uri $apiUrl -Headers @{"User-Agent"="PowerShell"}
$windowsAsset = $release.assets | Where-Object { 
    $_.name -like "*bin-win-avx2-x64*" 
} | Select-Object -First 1

if (-not $windowsAsset) {
    Write-Host "Could not find AVX2 Windows binary. Trying generic x64..."
    $windowsAsset = $release.assets | Where-Object { 
        $_.name -like "*win*x64*" -and $_.name -notlike "*cuda*"
    } | Select-Object -First 1
}

if (-not $windowsAsset) {
    Write-Error "Could not find suitable Windows binary."
}

$url = $windowsAsset.browser_download_url
Write-Host "Downloading from $url..."
$zipFile = "$env:TEMP\llama.cpp.zip"

Invoke-WebRequest -Uri $url -OutFile $zipFile -UseBasicParsing

# Extract
Write-Host "Extracting..."
if (Test-Path $LlamaCppDir) {
    Remove-Item $LlamaCppDir -Recurse -Force
}
New-Item -ItemType Directory -Path $LlamaCppDir -Force | Out-Null

Expand-Archive -Path $zipFile -DestinationPath $LlamaCppDir -Force
Remove-Item $zipFile -Force

Write-Host "Done. quantize.exe should be in $LlamaCppDir"
