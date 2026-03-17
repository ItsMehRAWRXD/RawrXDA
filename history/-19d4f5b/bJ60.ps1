param(
    [Parameter(Mandatory=$false)][string]$Exe = "$env:USERPROFILE\OneDrive\Desktop\RawrXD-IDE\bin\Executables\RawrXD-AgenticIDE.exe",
    [Parameter(Mandatory=$false)][string]$WindeployQtPath,
    [Parameter(Mandatory=$false)][switch]$NoTranslations
)

$ErrorActionPreference = 'Stop'

function Find-WindeployQt {
    param([string]$Hint)
    if ($Hint -and (Test-Path $Hint)) { return (Resolve-Path $Hint).Path }

    $candidates = @()
    $candidates += "$env:QTDIR\bin\windeployqt.exe"
    $candidates += "$env:QT_DIR\bin\windeployqt.exe"
    $candidates += "$env:ProgramFiles\Qt\**\bin\windeployqt.exe"
    $candidates += "$env:ProgramFiles(x86)\Qt\**\bin\windeployqt.exe"
    $candidates += "C:\Qt\**\bin\windeployqt.exe"

    foreach ($pattern in $candidates) {
        Get-ChildItem -Path $pattern -ErrorAction SilentlyContinue | ForEach-Object { return $_.FullName }
    }

    return $null
}

if (!(Test-Path $Exe)) {
    Write-Host "[Collect-QtRuntime] Target exe not found yet: $Exe" -ForegroundColor Yellow
    exit 2
}

$wqt = Find-WindeployQt -Hint $WindeployQtPath
if (-not $wqt) {
    Write-Host "[Collect-QtRuntime] windeployqt.exe not found. Skipping Qt runtime collection." -ForegroundColor Yellow
    exit 3
}

Write-Host "[Collect-QtRuntime] Using: $wqt" -ForegroundColor Cyan

$opts = @('--release')
if ($NoTranslations) { $opts += '--no-translations' }
$opts += "`"$Exe`""

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $wqt
$psi.Arguments = ($opts -join ' ')
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true

$proc = New-Object System.Diagnostics.Process
$proc.StartInfo = $psi
[void]$proc.Start()
$stdout = $proc.StandardOutput.ReadToEnd()
$stderr = $proc.StandardError.ReadToEnd()
$proc.WaitForExit()

Write-Host $stdout
if ($proc.ExitCode -ne 0) {
    Write-Host $stderr -ForegroundColor Red
    Write-Host "[Collect-QtRuntime] windeployqt failed with code $($proc.ExitCode)" -ForegroundColor Red
    exit $proc.ExitCode
}

# Brief summary of deployed content
$exeDir = Split-Path -Parent $Exe
$plugins = @('platforms','imageformats','styles','iconengines','networkinformation','sqldrivers','tls')
Write-Host "[Collect-QtRuntime] Deployed to: $exeDir" -ForegroundColor Green
foreach ($p in $plugins) {
    $dir = Join-Path $exeDir $p
    if (Test-Path $dir) {
        $count = (Get-ChildItem $dir -Recurse -File -ErrorAction SilentlyContinue).Count
        Write-Host (" - {0}: {1} files" -f $p, $count)
    }
}

Write-Host "[Collect-QtRuntime] Qt runtime collection complete." -ForegroundColor Green
