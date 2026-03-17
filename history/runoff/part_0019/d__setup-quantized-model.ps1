# setup-quantized-model.ps1
# Automates the conversion of GGUF models to different quantization types
# Usage: ./setup-quantized-model.ps1 -BlobPath "C:\...model.gguf" -OutputDir "C:\Output" -TargetQuantization "Q4_K_M"

param (
    [Parameter(Mandatory=$true)]
    [string]$BlobPath,

    [Parameter(Mandatory=$true)]
    [string]$OutputDir,

    [Parameter(Mandatory=$true)]
    [string]$TargetQuantization
)

$ErrorActionPreference = "Stop"

function Write-Log {
    param([string]$Message)
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $Message"
}

Write-Log "Starting Model Conversion Process..."
Write-Log "Input: $BlobPath"
Write-Log "Target: $TargetQuantization"

if (-not (Test-Path $BlobPath)) {
    Write-Error "Input model file not found: $BlobPath"
    exit 1
}

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

$ToolsDir = "C:\llama.cpp" # Fixed tool path for stability in this env
if (-not (Test-Path $ToolsDir)) {
    Write-Log "Cloning llama.cpp repository..."
    git clone https://github.com/ggerganov/llama.cpp $ToolsDir
} else {
    Write-Log "llama.cpp repository found."
}

$QuantizeExe = Join-Path $ToolsDir "build\bin\Release\quantize.exe"

# If pre-built binary doesn't exist, try to build or find it
if (-not (Test-Path $QuantizeExe)) {
    Write-Log "Building quantize tool..."
    
    # Check for cmake
    if (Get-Command "cmake" -ErrorAction SilentlyContinue) {
        Push-Location $ToolsDir
        try {
            if (-not (Test-Path "build")) { mkdir build }
            cd build
            cmake .. -DLLAMA_BUILD_SERVER=OFF -DLLAMA_BUILD_TESTS=OFF
            cmake --build . --config Release --target quantize
        } catch {
            Write-Warning "Build failed: $_"
        } finally {
            Pop-Location
        }
    } else {
        Write-Log "CMake not found. Attempting to use pre-downloaded tools if available..."
    }
}

# Fallback check
if (-not (Test-Path $QuantizeExe)) {
    # Check locally in simplified path
    $QuantizeExe = "D:\Tools\quantize.exe"
}

if (-not (Test-Path $QuantizeExe)) {
    Write-Error "Could not find 'quantize.exe'. Please install llama.cpp tools manually or ensure CMake is available."
    exit 1
}

$ModelName = [System.IO.Path]::GetFileNameWithoutExtension($BlobPath)
$OutputName = "${ModelName}-${TargetQuantization}.gguf"
$OutputPath = Join-Path $OutputDir $OutputName

Write-Log "Converting model to $OutputPath..."

# Run conversion
$ProcessInfo = New-Object System.Diagnostics.ProcessStartInfo
$ProcessInfo.FileName = $QuantizeExe
$ProcessInfo.Arguments = "`"$BlobPath`" `"$OutputPath`" $TargetQuantization"
$ProcessInfo.RedirectStandardOutput = $true
$ProcessInfo.RedirectStandardError = $true
$ProcessInfo.UseShellExecute = $false
$ProcessInfo.CreateNoWindow = $true

$Process = New-Object System.Diagnostics.Process
$Process.StartInfo = $ProcessInfo

$Process.Start() | Out-Null
$StdOut = $Process.StandardOutput.ReadToEnd()
$StdErr = $Process.StandardError.ReadToEnd()
$Process.WaitForExit()

if ($Process.ExitCode -ne 0) {
    Write-Log "Conversion Failed!"
    Write-Host $StdErr
    exit $Process.ExitCode
}

Write-Log "Conversion Complete!"
Write-Host $StdOut

# Verification
if (Test-Path $OutputPath) {
    Write-Log "Verified: Output file exists."
    exit 0
} else {
    Write-Error "Output file missing after successful exit code."
    exit 1
}
