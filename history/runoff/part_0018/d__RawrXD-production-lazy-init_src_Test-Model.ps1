param(
    [string]$ModelPath,
    [switch]$ForcePath,
    [string]$Prompt = "What is a React server?",
    # Default to a local test_cli.exe next to this script; can be overridden to any drive/USB
    [string]$ExePath = (Join-Path $PSScriptRoot "test_cli.exe")
)

Write-Host "RawrXD Model Resolver (path-agnostic)" -ForegroundColor Cyan

function Resolve-ModelPath {
    param([string]$Path, [switch]$Force)
    # 1) If provided and exists, use it
    if ($Path -and (Test-Path $Path)) { return (Get-Item $Path).FullName }
    # 1b) If provided but missing and Force is set, still return it
    if ($Path -and $Force) { return $Path }
    # 2) If env var set
    if ($env:BIGDADDYG_PATH -and (Test-Path $env:BIGDADDYG_PATH)) { return (Get-Item $env:BIGDADDYG_PATH).FullName }
    # 3) Search all drives (file system only) for *bigdaddy*.gguf (depth-unbounded, may take time)
    Write-Host "Searching for *bigdaddy*.gguf across drives..." -ForegroundColor Yellow
    $drives = Get-PSDrive -PSProvider FileSystem
    foreach ($d in $drives) {
        try {
            $found = Get-ChildItem -Path "$($d.Root)" -Filter "*bigdaddy*.gguf" -File -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($found) { return $found.FullName }
        } catch { continue }
    }
    return $null
}

$resolved = Resolve-ModelPath -Path $ModelPath -Force:$ForcePath
if (-not $resolved) {
    Write-Host "Model not found. Set -ModelPath or BIGDADDYG_PATH." -ForegroundColor Red
    exit 1
}

$env:BIGDADDYG_PATH = $resolved
Write-Host "Model resolved: $resolved" -ForegroundColor Green
if (Test-Path $resolved) {
    Write-Host "Size: $([math]::Round((Get-Item $resolved).Length/1GB,2)) GB" -ForegroundColor Gray
} else {
    Write-Host "Warning: path does not currently exist (forced)." -ForegroundColor Yellow
}
Write-Host "Stored in env: BIGDADDYG_PATH" -ForegroundColor Gray

if (-not (Test-Path $ExePath)) {
    Write-Host "Test executable not found at $ExePath" -ForegroundColor Yellow
    Write-Host "Build or point -ExePath to your binary." -ForegroundColor Yellow
    Write-Host "Example build (PowerShell):" -ForegroundColor DarkGray
    Write-Host "  cl /EHsc /std:c++17 /O2 /Fe:test_cli.exe test_cli.cpp RawrXD_UNIFIED_ENGINE.cpp" -ForegroundColor DarkGray
    exit 0
}

Write-Host "\nRunning inference test..." -ForegroundColor Cyan
$cmd = "`"$ExePath`" `"$resolved`" `"$Prompt`""
Write-Host "Command: $cmd" -ForegroundColor Gray

# Run and stream output
& $ExePath $resolved $Prompt
