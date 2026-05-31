param(
    [string]$BackendExe = "D:\rawrxd\build-win32\bin\rawrxd-tubi-backend.exe",
    [string]$OutDir = "D:\rawrxd\video-studio\smoke-script"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $BackendExe)) {
    throw "Backend exe not found: $BackendExe"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

& $BackendExe `
    --job-id smoke-script `
    --engine tubi `
    --provider local `
    --model procedural `
    --style cinematic-gothic `
    --camera cinematic-pan `
    --negative-prompt "blurry, washed out" `
    --seed 1337 `
    --duration 5s `
    --aspect 16:9 `
    --resolution 320p `
    --prompt "Castlevania castle under moonlight" `
    --storyboard "establishing castle; torch corridor; vampire approach" `
    --out-dir $OutDir

if ($LASTEXITCODE -ne 0) {
    throw "tubi backend smoke failed with exit code $LASTEXITCODE"
}

$required = @(
    "render_backend_manifest.json",
    "render_progress.json",
    "shot_plan.json",
    "contact_sheet.bmp",
    "preview_start.bmp",
    "preview_mid.bmp",
    "preview_end.bmp"
)

foreach ($file in $required) {
    $path = Join-Path $OutDir $file
    if (-not (Test-Path $path)) {
        throw "Missing expected artifact: $path"
    }
}

Write-Host "tubi smoke passed: $OutDir"
